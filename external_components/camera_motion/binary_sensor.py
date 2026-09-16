import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_MOTION

from . import CameraMotion

CONF_CAMERA_MOTION_ID = "camera_motion_id"

# Kein eigener C++-Subtyp noetig - CameraMotion spricht den Sensor nur ueber
# publish_state() an, dafuer reicht die normale binary_sensor::BinarySensor-Basis.
CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_MOTION,
).extend(
    {
        cv.Required(CONF_CAMERA_MOTION_ID): cv.use_id(CameraMotion),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config[CONF_CAMERA_MOTION_ID])
    cg.add(parent.set_binary_sensor(var))
