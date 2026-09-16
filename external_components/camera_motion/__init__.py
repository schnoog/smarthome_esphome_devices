import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_camera
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32_camera"]
AUTO_LOAD = ["binary_sensor"]

camera_motion_ns = cg.esphome_ns.namespace("camera_motion")
CameraMotion = camera_motion_ns.class_("CameraMotion", cg.Component)

CONF_CAMERA_ID = "camera_id"
CONF_BLOCK_SIZE = "block_size"
CONF_BLOCK_THRESHOLD = "block_threshold"
CONF_MOTION_THRESHOLD = "motion_threshold"
CONF_CHECK_INTERVAL = "check_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CameraMotion),
        cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
        cv.Optional(CONF_BLOCK_SIZE, default=16): cv.int_range(min=4, max=64),
        cv.Optional(CONF_BLOCK_THRESHOLD, default=30): cv.int_range(min=1, max=255),
        cv.Optional(CONF_MOTION_THRESHOLD, default=10): cv.int_range(
            min=1, max=10000
        ),
        # Mindestabstand zwischen zwei analysierten Frames (statt einer festen
        # "jeden n-ten Frame"-Zaehlung). Frames, die in kuerzerem Abstand
        # ankommen, werden einfach uebersprungen.
        cv.Optional(CONF_CHECK_INTERVAL, default="1s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
    cg.add(var.set_block_size(config[CONF_BLOCK_SIZE]))
    cg.add(var.set_block_threshold(config[CONF_BLOCK_THRESHOLD]))
    cg.add(var.set_motion_threshold(config[CONF_MOTION_THRESHOLD]))
    cg.add(var.set_check_interval(config[CONF_CHECK_INTERVAL]))
