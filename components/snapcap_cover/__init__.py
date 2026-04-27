import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import servo, binary_sensor, text_sensor, cover
from esphome.const import CONF_ID

import esphome.components.tcp_server as tcp_server

snapcap_cover_ns = cg.esphome_ns.namespace("snapcap_cover")
SnapCapCoverComponent = snapcap_cover_ns.class_(
    "SnapCapCoverComponent", tcp_server.TCPServerComponent, cover.Cover
)

CONF_SERVO_ID = "servo_id"
CONF_PROTOCOL_DEVICE_ID = "protocol_device_id"
CONF_OPEN_LEVEL = "open_level"
CONF_CLOSED_LEVEL = "closed_level"
CONF_MOVE_DURATION = "move_duration"
CONF_INITIAL_STATE = "initial_state"
CONF_ASSUMED_OPEN = "assumed_open"
CONF_STATE_TEXT = "state_text"

AUTO_LOAD = ["tcp_server", "binary_sensor", "text_sensor", "cover"]
DEPENDENCIES = ["tcp_server"]
MULTI_CONF = True

DEVICE_TYPE_ENUM = {
    "FLAT_MAN_L": 10,
    "FLAT_MAN_XL": 15,
    "FLAT_MAN": 19,
    "FLIP_DUST": 98,
    "FLIP_FLAT": 99,
}


def validate_device_type(value):
    if isinstance(value, str):
        value_upper = value.upper()
        if value_upper in DEVICE_TYPE_ENUM:
            return DEVICE_TYPE_ENUM[value_upper]
        raise cv.Invalid(
            f"device_id must be one of: {', '.join(DEVICE_TYPE_ENUM.keys())}"
        )
    if value in DEVICE_TYPE_ENUM.values():
        return value
    raise cv.Invalid(
        "device_id must be one of: "
        f"{', '.join(DEVICE_TYPE_ENUM.keys())} or their numeric values"
    )


def migrate_legacy_device_id(config):
    legacy_key = "device_id"
    if legacy_key not in config or CONF_PROTOCOL_DEVICE_ID in config:
        return config

    try:
        mapped = validate_device_type(config[legacy_key])
    except cv.Invalid:
        return config

    updated = dict(config)
    updated[CONF_PROTOCOL_DEVICE_ID] = mapped
    del updated[legacy_key]
    return updated


def validate_levels(config):
    if config[CONF_OPEN_LEVEL] == config[CONF_CLOSED_LEVEL]:
        raise cv.Invalid("open_level and closed_level must be different")
    return config


CONFIG_SCHEMA = cv.All(
    migrate_legacy_device_id,
    cover.cover_schema(SnapCapCoverComponent)
    .extend(
        {
            cv.Required(CONF_SERVO_ID): cv.use_id(servo.Servo),
            cv.Optional(
                CONF_DEVICE_ID, default=DEVICE_TYPE_ENUM["FLIP_FLAT"]
            ): validate_device_type,
            cv.Optional(CONF_OPEN_LEVEL, default=1.0): cv.float_range(min=-1.0, max=1.0),
            cv.Optional(CONF_CLOSED_LEVEL, default=-1.0): cv.float_range(
                min=-1.0, max=1.0
            ),
            cv.Optional(
                CONF_MOVE_DURATION, default="1200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_INITIAL_STATE, default="CLOSED"): cv.one_of(
                "OPEN", "CLOSED", upper=True
            ),
            cv.Optional(CONF_ASSUMED_OPEN): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_STATE_TEXT): text_sensor.text_sensor_schema(),
        }
    )
    .extend(tcp_server.TCP_SERVER_SCHEMA),
    validate_levels,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await tcp_server.setup_tcp_server(var, config)

    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    cg.add(var.set_open_level(config[CONF_OPEN_LEVEL]))
    cg.add(var.set_closed_level(config[CONF_CLOSED_LEVEL]))
    cg.add(var.set_move_duration(config[CONF_MOVE_DURATION]))
    cg.add(var.set_initial_opened(config[CONF_INITIAL_STATE] == "OPEN"))

    servo_ref = await cg.get_variable(config[CONF_SERVO_ID])
    cg.add(var.set_servo(servo_ref))

    if CONF_ASSUMED_OPEN in config:
        assumed_open = await binary_sensor.new_binary_sensor(config[CONF_ASSUMED_OPEN])
        cg.add(var.set_assumed_open_sensor(assumed_open))

    if CONF_STATE_TEXT in config:
        state_text = await text_sensor.new_text_sensor(config[CONF_STATE_TEXT])
        cg.add(var.set_state_text_sensor(state_text))

    await cover.register_cover(var, config)
    await cg.register_component(var, config)

