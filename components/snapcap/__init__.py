import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.components.tcp_server as tcp_server

snapcap_ns = cg.esphome_ns.namespace("snapcap")
SnapCapComponent = snapcap_ns.class_("SnapCapComponent", tcp_server.TCPServerComponent)

CONF_DEVICE_ID = "device_id"
CONF_BRIGHTNESS = "brightness"
CONF_SERVO_POSITION = "servo_position"

AUTO_LOAD = ["tcp_server"]
DEPENDENCIES = ["tcp_server"]
MULTI_CONF = True

# Allowed device types from DeviceType enum with labels
ALLOWED_DEVICE_TYPES = {
    "FLAT_MAN_L": 10,
    "FLAT_MAN_XL": 15,
    "FLAT_MAN": 19,
    "FLIP_DUST": 98,
    "FLIP_FLAT": 99
}

def validate_device_type(value):
    if isinstance(value, str):
        value_upper = value.upper()
        if value_upper in ALLOWED_DEVICE_TYPES:
            return ALLOWED_DEVICE_TYPES[value_upper]
        raise cv.Invalid(f"device_id must be one of: {', '.join(ALLOWED_DEVICE_TYPES.keys())}")
    if value in ALLOWED_DEVICE_TYPES.values():
        return value
    raise cv.Invalid(f"device_id must be one of: {', '.join(ALLOWED_DEVICE_TYPES.keys())} or their numeric values")
    cv.Optional(CONF_DEVICE_ID, default=1): cv.int_range(min=0, max=99),
    cv.Optional(CONF_BRIGHTNESS, default=128): cv.int_range(min=0, max=255),
    cv.Optional(CONF_SERVO_POSITION, default=0): cv.int_range(min=0, max=999),
    cv.Optional(CONF_DEVICE_ID, default=99): validate_device_type,

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await tcp_server.setup_tcp_server(var, config)
    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_servo_position(config[CONF_SERVO_POSITION]))
    await cg.register_component(var, config)
