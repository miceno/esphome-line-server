import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_PORT,
)
from esphome.core import CORE
from esphome.core.entity_helpers import entity_duplicate_validator, setup_entity

CONF_TCP_BUFFER_SIZE = "tcp_buffer_size"
CONF_TCP_TERMINATOR = "tcp_terminator"
CONF_TCP_TIMEOUT = "tcp_timeout"
CONF_TCP_TIMEOUT_LAMBDA = "tcp_timeout_lambda"

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["network"]
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

tcp_server_ns = cg.esphome_ns.namespace("tcp_server")
TCPServerComponent = tcp_server_ns.class_("TCPServerComponent", cg.Component)

def validate_buffer_size(buffer_size):
    if buffer_size & (buffer_size - 1) != 0:
        raise cv.Invalid("Buffer size must be a power of two.")
    return buffer_size

def validate_terminator(value):
    value = cv.string(value)
    if len(value.encode("utf-8")) > 4:
        raise cv.Invalid("Terminator must be <= 4 bytes")
    return value

# Validate ESPHome version
REQUIRES_ESPHOME_VERSION = cv.require_esphome_version(2022, 3, 0)

# Validate component schema
TCP_SERVER_SCHEMA = cv.Schema(
    {
        # The ID of the TCP server component is not needed since this element will not
        # be referenced by other components.

        cv.Optional(CONF_PORT, default=8888): cv.port,
        cv.Optional(CONF_TCP_BUFFER_SIZE, default=256): cv.All(
            cv.positive_int, validate_buffer_size
        ),
        cv.Optional(CONF_TCP_TERMINATOR, default="\r"): validate_terminator,
        cv.Optional(CONF_TCP_TIMEOUT, default="300ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TCP_TIMEOUT_LAMBDA): cv.returning_lambda,
    }
).extend(cv.COMPONENT_SCHEMA
).extend(cv.ENTITY_BASE_SCHEMA)

CONFIG_SCHEMA = cv.All(REQUIRES_ESPHOME_VERSION, TCP_SERVER_SCHEMA)

async def setup_tcp_server(var, config):
    # Only add the ID if it is not present in config
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)

    await setup_entity(var, config, "tcp_server")

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_tcp_buffer_size(config[CONF_TCP_BUFFER_SIZE]))
    cg.add(var.set_tcp_terminator(config[CONF_TCP_TERMINATOR]))
    cg.add(var.set_tcp_flush_timeout(config[CONF_TCP_TIMEOUT]))
    if CONF_TCP_TIMEOUT_LAMBDA in config:
        tcp_lambda_ = await cg.process_lambda(
            config[CONF_TCP_TIMEOUT_LAMBDA],
            [],
            return_type=cg.std_string,
        )
        cg.add(var.set_tcp_timeout_callback(tcp_lambda_))
    await cg.register_component(var, config)
    return var


async def to_code(config):
    cg.add_global(tcp_server_ns.using)
