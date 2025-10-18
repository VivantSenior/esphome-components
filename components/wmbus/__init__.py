"""
Backward compatibility component for wmbus.

This component exists for backward compatibility with version 4 configurations.
In version 5, the wmbus component has been split into:
- wmbus_radio: For radio communication (CC1101, SX1276, etc.)
- wmbus_meter: For meter definitions and sensors
- wmbus_common: For common wmbusmeters code

Please update your configuration to use the new components.
See: https://github.com/VivantSenior/esphome-components/blob/main/docs/CC1101_USAGE.md
"""

import esphome.config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID

CODEOWNERS = ["@SzczepanLeon", "@kubasaw", "@VivantSenior"]

# This is a stub component for backward compatibility
# It doesn't generate any code, it's just here so ESPHome can find it
# when users explicitly list "wmbus" in their external_components configuration

wmbus_ns = cg.esphome_ns.namespace("wmbus")
WMBusComponent = wmbus_ns.class_("WMBusComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # This component doesn't actually do anything
    # It's just here for backward compatibility
    # The actual functionality is in wmbus_radio, wmbus_meter, and wmbus_common
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
