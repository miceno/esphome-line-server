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

# Allowed device types from DeviceType enum
ALLOWED_DEVICE_TYPES = [10, 15, 19, 98, 99]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SnapCapComponent),
    cv.Optional(CONF_DEVICE_ID, default=99): cv.one_of(*ALLOWED_DEVICE_TYPES, int=True),
    cv.Optional(CONF_BRIGHTNESS, default=128): cv.int_range(min=0, max=255),
    cv.Optional(CONF_SERVO_POSITION, default=0): cv.int_range(min=0, max=999),
}).extend(tcp_server.TCP_SERVER_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await tcp_server.setup_tcp_server(var, config)
    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_servo_position(config[CONF_SERVO_POSITION]))
    await cg.register_component(var, config)
