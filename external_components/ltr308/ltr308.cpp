#include "ltr308.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ltr308 {

static const char *const TAG = "ltr308";

// Registeradressen laut Lite-On LTR-308ALS-01 Datenblatt
static const uint8_t REG_MAIN_CTRL = 0x00;
static const uint8_t REG_ALS_MEAS_RATE = 0x04;
static const uint8_t REG_ALS_GAIN = 0x05;
static const uint8_t REG_PART_ID = 0x06;
static const uint8_t REG_MAIN_STATUS = 0x07;
static const uint8_t REG_ALS_DATA_0 = 0x0D;  // 0x0D..0x0F, LSB zuerst

void LTR308Sensor::setup() {
  ESP_LOGCONFIG(TAG, "LTR308 wird eingerichtet...");

  uint8_t part_id;
  if (!this->read_byte(REG_PART_ID, &part_id)) {
    ESP_LOGE(TAG, "Konnte PART_ID nicht lesen - Sensor nicht erreichbar");
    this->mark_failed();
    return;
  }
  if ((part_id & 0xF0) != 0xB0) {
    ESP_LOGW(TAG, "Unerwartete PART_ID 0x%02X (erwartet: 0xBx)", part_id);
  }

  // ALS_MEAS_RATE: Bits 5:4 = Auflösung, Bits 2:0 = Messrate (hier fix 100 ms)
  uint8_t meas_rate = (static_cast<uint8_t>(this->resolution_) << 4) | 0x02;
  this->write_byte(REG_ALS_MEAS_RATE, meas_rate);

  this->write_byte(REG_ALS_GAIN, static_cast<uint8_t>(this->gain_));

  // MAIN_CTRL: Bit1 = ALS aktiv schalten
  this->write_byte(REG_MAIN_CTRL, 0x02);
}

void LTR308Sensor::update() {
  uint8_t status;
  if (!this->read_byte(REG_MAIN_STATUS, &status)) {
    ESP_LOGW(TAG, "Statusregister konnte nicht gelesen werden");
    return;
  }
  // Bit3 = neue ALS-Daten verfügbar
  if ((status & 0x08) == 0) {
    ESP_LOGD(TAG, "Noch keine neuen Daten, überspringe diesen Zyklus");
    return;
  }

  uint8_t data[3];
  if (!this->read_bytes(REG_ALS_DATA_0, data, 3)) {
    ESP_LOGW(TAG, "ALS-Daten konnten nicht gelesen werden");
    return;
  }
  uint32_t raw = (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16);

  // Lux = 0.6 * ALS_DATA / (Gain * Integrationszeit[100ms-Einheiten])
  float lux = (0.6f * raw) / (this->gain_factor_() * this->integration_time_factor_());
  ESP_LOGD(TAG, "Rohwert=%u -> %.1f lx", raw, lux);
  this->publish_state(lux);
}

float LTR308Sensor::gain_factor_() const {
  switch (this->gain_) {
    case GAIN_1X:
      return 1.0f;
    case GAIN_3X:
      return 3.0f;
    case GAIN_6X:
      return 6.0f;
    case GAIN_9X:
      return 9.0f;
    case GAIN_18X:
      return 18.0f;
  }
  return 3.0f;
}

float LTR308Sensor::integration_time_factor_() const {
  switch (this->resolution_) {
    case RES_20BIT_400MS:
      return 4.0f;
    case RES_19BIT_200MS:
      return 2.0f;
    case RES_18BIT_100MS:
      return 1.0f;
    case RES_17BIT_50MS:
      return 0.5f;
    case RES_16BIT_25MS:
      return 0.25f;
  }
  return 1.0f;
}

void LTR308Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "LTR308:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Ambient Light", this);
}

}  // namespace ltr308
}  // namespace esphome
