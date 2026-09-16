import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]

ltr308_ns = cg.esphome_ns.namespace("ltr308")
LTR308Sensor = ltr308_ns.class_(
    "LTR308Sensor", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

Gain = ltr308_ns.enum("Gain")
GAIN_OPTIONS = {
    "1x": Gain.GAIN_1X,
    "3x": Gain.GAIN_3X,
    "6x": Gain.GAIN_6X,
    "9x": Gain.GAIN_9X,
    "18x": Gain.GAIN_18X,
}

Resolution = ltr308_ns.enum("Resolution")
RESOLUTION_OPTIONS = {
    "400ms": Resolution.RES_20BIT_400MS,
    "200ms": Resolution.RES_19BIT_200MS,
    "100ms": Resolution.RES_18BIT_100MS,
    "50ms": Resolution.RES_17BIT_50MS,
    "25ms": Resolution.RES_16BIT_25MS,
}

CONF_GAIN = "gain"
CONF_RESOLUTION = "resolution"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        LTR308Sensor,
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_GAIN, default="3x"): cv.enum(GAIN_OPTIONS, lower=True),
            cv.Optional(CONF_RESOLUTION, default="100ms"): cv.enum(
                RESOLUTION_OPTIONS, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x53))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
