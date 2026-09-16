import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_MOTION

from . import CameraMotion, camera_motion_ns

CONF_CAMERA_MOTION_ID = "camera_motion_id"

CameraMotionBinarySensor = camera_motion_ns.class_(
    "CameraMotionBinarySensor", binary_sensor.BinarySensor
)

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
