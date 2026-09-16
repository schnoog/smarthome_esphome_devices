#pragma once

#include "esphome/core/component.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <cstdint>
#include <cstddef>

namespace esphome {
namespace camera_motion {

// Block-diff Bewegungserkennung auf Basis von JPEG-Frames der esp32_camera-Komponente.
// Portierung des Algorithmus aus kakopappa/esp32-cam-motion-detection-with-no-pir-no-microwave
// auf ESPHomes camera::CameraListener-Mechanismus.
//
// WICHTIG: Alle Bildpuffer (RGB565 + zwei Graustufen-Puffer im Ping-Pong-Verfahren)
// werden per heap_caps_malloc() gezielt im PSRAM alloziert, nicht per std::vector
// (DRAM-Heap). Ein 320x240-Frame braucht z.B. ~150 KB fuer RGB565 - das sprengt den
// regulaeren DRAM-Heap neben WLAN-/Kamera-Puffern und fuehrt sonst zu einem abort()
// beim Allozieren (operator new kann unter ESP-IDF nicht werfen).
class CameraMotion : public Component, public esphome::camera::CameraListener {
 public:
  ~CameraMotion();

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_camera(esp32_camera::ESP32Camera *camera) { this->camera_ = camera; }
  void set_binary_sensor(binary_sensor::BinarySensor *sensor) { this->sensor_ = sensor; }
  void set_block_size(uint8_t size) { this->block_size_ = size; }
  void set_block_threshold(uint8_t threshold) { this->block_threshold_ = threshold; }
  void set_motion_threshold(uint16_t threshold) { this->motion_threshold_ = threshold; }
  void set_check_interval(uint32_t interval_ms) { this->check_interval_ms_ = interval_ms; }

  // esphome::camera::CameraListener - wird von ESP32Camera pro Frame aufgerufen.
  void on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) override;

 protected:
  // Berechnet Graustufen aus RGB565 in out_gray, vergleicht danach blockweise
  // mit previous_gray_ (falls Aufloesung uebereinstimmt) und gibt zurueck, ob
  // genug Bloecke sich veraendert haben.
  bool analyze_(const uint8_t *rgb565, uint8_t *out_gray, int width, int height, bool have_reference);

  // Stellt sicher, dass *buf mindestens needed_size Bytes im PSRAM hat.
  // Gibt false zurueck, wenn die Allokation fehlschlaegt (statt abzustuerzen).
  bool ensure_psram_buffer_(uint8_t **buf, size_t *current_size, size_t needed_size);

  esp32_camera::ESP32Camera *camera_{nullptr};
  binary_sensor::BinarySensor *sensor_{nullptr};

  uint8_t block_size_{16};
  uint8_t block_threshold_{30};
  uint16_t motion_threshold_{10};
  uint32_t check_interval_ms_{1000};

  uint32_t last_check_ms_{0};

  uint8_t *rgb_buf_{nullptr};
  size_t rgb_buf_size_{0};

  // Ping-Pong-Graustufenpuffer: nach jedem Vergleich werden previous_/current_
  // per Zeiger-Swap getauscht statt kopiert.
  uint8_t *previous_gray_{nullptr};
  size_t previous_gray_size_{0};
  uint8_t *current_gray_{nullptr};
  size_t current_gray_size_{0};

  int previous_width_{0};
  int previous_height_{0};
  float previous_avg_gray_{0.0f};
};

}  // namespace camera_motion
}  // namespace esphome
