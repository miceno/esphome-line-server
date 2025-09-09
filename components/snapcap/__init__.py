import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_BRIGHTNESS, CONF_POSITION, CONF_DEVICE_ID
from esphome.components import servo
import esphome.components.tcp_server as tcp_server

snapcap_ns = cg.esphome_ns.namespace("snapcap")
SnapCapComponent = snapcap_ns.class_("SnapCapComponent", tcp_server.TCPServerComponent)

CONF_SERVO_ID = "servo_id"

AUTO_LOAD = ["tcp_server"]
DEPENDENCIES = ["tcp_server"]
MULTI_CONF = True

# DeviceType enum values and labels from snapcap.h
DEVICE_TYPE_ENUM = {
    "FLAT_MAN_L": 10,
    "FLAT_MAN_XL": 15,
    "FLAT_MAN": 19,
    "FLIP_DUST": 98,
    "FLIP_FLAT": 99
}

def validate_device_type(value):
    if isinstance(value, str):
        value_upper = value.upper()
        if value_upper in DEVICE_TYPE_ENUM:
            return DEVICE_TYPE_ENUM[value_upper]
        raise cv.Invalid(f"device_id must be one of: {', '.join(DEVICE_TYPE_ENUM.keys())}")
    if value in DEVICE_TYPE_ENUM.values():
        return value
    raise cv.Invalid(f"device_id must be one of: {', '.join(DEVICE_TYPE_ENUM.keys())} or their numeric values")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SnapCapComponent),
    cv.Optional(CONF_DEVICE_ID, default=DEVICE_TYPE_ENUM["FLIP_FLAT"]): validate_device_type,
    cv.Optional(CONF_BRIGHTNESS, default=128): cv.int_range(min=0, max=255),
    cv.Optional(CONF_POSITION, default=0): cv.int_range(min=0, max=999),

    cv.Required(CONF_SERVO_ID): cv.use_id(servo.Servo),

}).extend(tcp_server.TCP_SERVER_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await tcp_server.setup_tcp_server(var, config)

    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_servo_position(config[CONF_POSITION]))

    servo = await cg.get_variable(config[CONF_SERVO_ID])
    cg.add(var.set_servo(servo))
    await cg.register_component(servo, config)

    await cg.register_component(var, config)
