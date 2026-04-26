import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID

from . import SnapCapComponent, snapcap_ns

CONF_SNAPCAP_ID = "snapcap_id"

SnapCapServoPositionNumber = snapcap_ns.class_(
    "SnapCapServoPositionNumber", number.Number
)

CONFIG_SCHEMA = number.number_schema(
    SnapCapServoPositionNumber,
    min_value=0,
    max_value=270,
    step=1,
).extend(
    {
        cv.GenerateID(CONF_SNAPCAP_ID): cv.use_id(SnapCapComponent),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SNAPCAP_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await number.register_number(var, config, min_value=0, max_value=270, step=1)
    cg.add(parent.set_servo_position_number(var))

