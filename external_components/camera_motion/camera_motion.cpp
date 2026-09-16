#include "camera_motion.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

#include "esp_heap_caps.h"

// Aus der esp32-camera-Bibliothek (bereits Abhaengigkeit von esp32_camera):
// dekodiert einen JPEG-Buffer nach RGB565.
#include "img_converters.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace esphome {
namespace camera_motion {

static const char *const TAG = "camera_motion";

CameraMotion::~CameraMotion() {
  if (this->rgb_buf_ != nullptr)
    heap_caps_free(this->rgb_buf_);
  if (this->previous_gray_ != nullptr)
    heap_caps_free(this->previous_gray_);
  if (this->current_gray_ != nullptr)
    heap_caps_free(this->current_gray_);
}

void CameraMotion::setup() {
  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "Keine Kamera zugewiesen!");
    this->mark_failed();
    return;
  }

  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
    ESP_LOGW(TAG, "Kein PSRAM gefunden - camera_motion braucht PSRAM, sonst stuerzt das Board ab!");
  }

  // Registriert diese Komponente als Listener bei der ESP32Camera.
  this->camera_->add_listener(this);
}

void CameraMotion::dump_config() {
  ESP_LOGCONFIG(TAG, "Camera Motion:");
  ESP_LOGCONFIG(TAG, "  Block Size: %u", this->block_size_);
  ESP_LOGCONFIG(TAG, "  Block Threshold: %u", this->block_threshold_);
  ESP_LOGCONFIG(TAG, "  Motion Threshold (Bloecke): %u", this->motion_threshold_);
  ESP_LOGCONFIG(TAG, "  Analyse-Intervall: %u ms", (unsigned) this->check_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Freier PSRAM: %u Bytes", (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

bool CameraMotion::ensure_psram_buffer_(uint8_t **buf, size_t *current_size, size_t needed_size) {
  if (*buf != nullptr && *current_size == needed_size)
    return true;

  if (*buf != nullptr) {
    heap_caps_free(*buf);
    *buf = nullptr;
    *current_size = 0;
  }

  uint8_t *new_buf = static_cast<uint8_t *>(heap_caps_malloc(needed_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (new_buf == nullptr) {
    ESP_LOGW(TAG, "PSRAM-Allokation von %u Bytes fehlgeschlagen (frei: %u)", (unsigned) needed_size,
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return false;
  }

  *buf = new_buf;
  *current_size = needed_size;
  return true;
}

void CameraMotion::on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  // Nicht jedes Frame analysieren, sondern nur wenn genug Zeit seit der letzten
  // Analyse vergangen ist - robust gegenueber stark schwankender Framerate
  // (1fps im Leerlauf vs. bis zu 60fps waehrend eines aktiven Streams).
  const uint32_t now = millis();
  if (now - this->last_check_ms_ < this->check_interval_ms_)
    return;
  this->last_check_ms_ = now;

  // Hinweis: frueher wurde hier nach was_requested_by(camera::IDLE) gefiltert.
  // Das verwirft aber JEDES Frame, sobald Bilder ausschliesslich ueber einen
  // aktiven Stream (z.B. esp32_camera_web_server im stream-Modus) oder eine
  // API-Anfrage ankommen und idle_framerate: 0fps gesetzt ist - dann werden
  // nie IDLE-Frames erzeugt und die Erkennung laeuft komplett leer.
  // Wir analysieren daher jedes ankommende Frame, unabhaengig vom Requester.
  // Die ESP32-spezifischen Rohdaten (inkl. Breite/Hoehe) stecken im camera_fb_t,
  // das nur die konkrete ESP32CameraImage-Klasse herausgibt - daher der Cast.
  auto esp_image = std::static_pointer_cast<esp32_camera::ESP32CameraImage>(image);
  camera_fb_t *fb = esp_image->get_raw_buffer();
  if (fb == nullptr || fb->format != PIXFORMAT_JPEG || fb->width == 0 || fb->height == 0)
    return;

  const int width = fb->width;
  const int height = fb->height;
  const size_t rgb_len = static_cast<size_t>(width) * height * 2;  // RGB565 = 2 Byte/Pixel
  const size_t gray_len = static_cast<size_t>(width) * height;

  if (!this->ensure_psram_buffer_(&this->rgb_buf_, &this->rgb_buf_size_, rgb_len))
    return;  // kein Speicher frei -> dieses Frame ueberspringen statt abzustuerzen

  // Watchdog explizit fuettern, bevor die potenziell langsame JPEG-Dekodierung startet.
  App.feed_wdt();

  const uint32_t decode_start_us = micros();
  if (!jpg2rgb565(fb->buf, fb->len, this->rgb_buf_, JPG_SCALE_NONE)) {
    ESP_LOGW(TAG, "JPEG-Dekodierung fuer Bewegungsanalyse fehlgeschlagen");
    return;
  }
  const uint32_t decode_us = micros() - decode_start_us;
  if (decode_us > 200000) {
    ESP_LOGW(TAG, "JPEG-Dekodierung dauerte %u ms (%dx%d) - Aufloesung/Framerate reduzieren!",
             (unsigned) (decode_us / 1000), width, height);
  } else {
    ESP_LOGV(TAG, "JPEG-Dekodierung: %u us", (unsigned) decode_us);
  }
  App.feed_wdt();

  bool resolution_changed = (this->previous_width_ != width || this->previous_height_ != height);

  if (!this->ensure_psram_buffer_(&this->current_gray_, &this->current_gray_size_, gray_len))
    return;
  if (!this->ensure_psram_buffer_(&this->previous_gray_, &this->previous_gray_size_, gray_len))
    return;

  bool have_reference = !resolution_changed;
  bool motion = this->analyze_(this->rgb_buf_, this->current_gray_, width, height, have_reference);

  // Ping-Pong: aktuelles Graustufenbild wird zum Referenzbild fuer den naechsten Durchlauf.
  std::swap(this->previous_gray_, this->current_gray_);
  std::swap(this->previous_gray_size_, this->current_gray_size_);
  this->previous_width_ = width;
  this->previous_height_ = height;

  if (this->sensor_ != nullptr)
    this->sensor_->publish_state(motion);
}

bool CameraMotion::analyze_(const uint8_t *rgb565, uint8_t *out_gray, int width, int height, bool have_reference) {
  const uint32_t analyze_start_us = micros();
  const size_t gray_len = static_cast<size_t>(width) * height;
  uint64_t gray_sum = 0;

  // RGB565 -> grobe Graustufen-Naeherung, ohne teure Farbraum-Umrechnung.
  for (size_t i = 0; i < gray_len; i++) {
    uint16_t pixel = (static_cast<uint16_t>(rgb565[i * 2]) << 8) | rgb565[i * 2 + 1];
    uint8_t r = (pixel >> 11) & 0x1F;
    uint8_t g = (pixel >> 5) & 0x3F;
    uint8_t b = pixel & 0x1F;
    out_gray[i] = static_cast<uint8_t>((r * 8 + g * 4 + b * 8) / 3);
    gray_sum += out_gray[i];
  }

  const float current_avg_gray = static_cast<float>(gray_sum) / static_cast<float>(gray_len);

  if (!have_reference) {
    this->previous_avg_gray_ = current_avg_gray;
    return false;  // kein Vorgaenger-Frame (erster Durchlauf / Aufloesung gewechselt)
  }

  // Globale Helligkeitsverschiebung (AEC/AGC/Weissabgleich) zwischen den beiden
  // Frames herausrechnen, damit sie nicht als "Bewegung" gewertet wird.
  const int brightness_delta = static_cast<int>(std::lround(current_avg_gray - this->previous_avg_gray_));
  this->previous_avg_gray_ = current_avg_gray;

  const int blocks_x = width / this->block_size_;
  const int blocks_y = height / this->block_size_;
  uint16_t changed_blocks = 0;

  for (int by = 0; by < blocks_y; by++) {
    // Watchdog einmal pro Blockzeile fuettern, damit lange Analysen (grosse
    // Aufloesung / kleine Blockgroesse) nicht den Task-WDT ausloesen.
    App.feed_wdt();
    for (int bx = 0; bx < blocks_x; bx++) {
      uint32_t diff_sum = 0;
      for (int y = 0; y < this->block_size_; y++) {
        const int py = by * this->block_size_ + y;
        const int row_offset = py * width;
        for (int x = 0; x < this->block_size_; x++) {
          const int px = bx * this->block_size_ + x;
          const int idx = row_offset + px;
          diff_sum += std::abs((static_cast<int>(out_gray[idx]) - static_cast<int>(this->previous_gray_[idx])) -
                                brightness_delta);
        }
      }
      const uint8_t avg_diff = diff_sum / (this->block_size_ * this->block_size_);
      if (avg_diff > this->block_threshold_)
        changed_blocks++;
    }
  }

  const uint32_t analyze_us = micros() - analyze_start_us;
  if (analyze_us > 200000) {
    ESP_LOGW(TAG, "Bewegungsanalyse dauerte %u ms - Aufloesung/Blockgroesse anpassen!",
             (unsigned) (analyze_us / 1000));
  } else {
    ESP_LOGV(TAG, "Bewegungsanalyse: %u us, %u veraenderte Bloecke", (unsigned) analyze_us, changed_blocks);
  }

  if (changed_blocks >= this->motion_threshold_) {
    ESP_LOGD(TAG, "Bewegung erkannt: %u veraenderte Bloecke", changed_blocks);
    return true;
  }

  return false;
}

}  // namespace camera_motion
}  // namespace esphome
