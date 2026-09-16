#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ltr308 {

// Registerwerte gemäß Lite-On LTR-308ALS-01 Datenblatt (ALS_GAIN, Reg. 0x05)
enum Gain {
  GAIN_1X = 0,
  GAIN_3X = 1,
  GAIN_6X = 2,
  GAIN_9X = 3,
  GAIN_18X = 4,
};

// Obere Bits von ALS_MEAS_RATE (Reg. 0x04) -> Auflösung / Integrationszeit
enum Resolution {
  RES_20BIT_400MS = 0,
  RES_19BIT_200MS = 1,
  RES_18BIT_100MS = 2,
  RES_17BIT_50MS = 3,
  RES_16BIT_25MS = 4,
};

class LTR308Sensor : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_resolution(Resolution res) { this->resolution_ = res; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  Gain gain_{GAIN_3X};
  Resolution resolution_{RES_18BIT_100MS};

  float gain_factor_() const;
  float integration_time_factor_() const;
};

}  // namespace ltr308
}  // namespace esphome
