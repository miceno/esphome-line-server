import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_STEP
from esphome.core import CORE

from . import SnapCapComponent, snapcap_ns, CONF_MAX_DEGREES

CONF_SNAPCAP_ID = "snapcap_id"

SnapCapServoPositionNumber = snapcap_ns.class_(
    "SnapCapServoPositionNumber", number.Number
)

# number_schema() does not accept min_value/max_value/step — those are
# purely runtime parameters for register_number(). We expose them as
# explicit YAML keys so the user (and to_code) has them in config.
# step is Required; max_value auto-syncs with snapcap's max_degrees.
CONFIG_SCHEMA = number.number_schema(SnapCapServoPositionNumber).extend(
    {
        cv.GenerateID(CONF_SNAPCAP_ID): cv.use_id(SnapCapComponent),
        cv.Required(CONF_STEP): cv.positive_float,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SNAPCAP_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)

    # Try to get max_degrees from the parent snapcap component's config
    parent_id = config[CONF_SNAPCAP_ID]
    parent_config = None

    # Search the CORE config for the snapcap component with this ID
    if "snapcap" in CORE.config:
        snapcap_cfgs = CORE.config.get("snapcap", [])
        if isinstance(snapcap_cfgs, dict):
            snapcap_cfgs = [snapcap_cfgs]
        for snapcap_cfg in snapcap_cfgs:
            if snapcap_cfg.get(CONF_ID) == parent_id:
                parent_config = snapcap_cfg
                break

    # Use max_degrees from parent config, or default to 270
    if parent_config and CONF_MAX_DEGREES in parent_config:
        max_val = parent_config[CONF_MAX_DEGREES]
    else:
        max_val = 270

    min_val = 0
    step_val = config[CONF_STEP]

    await number.register_number(
        var,
        config,
        min_value=min_val,
        max_value=max_val,
        step=step_val,
    )
    cg.add(parent.set_servo_position_number(var))

