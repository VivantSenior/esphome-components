#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wmbus {

static const char *TAG = "wmbus";

/**
 * Backward compatibility component for wmbus.
 * 
 * This component exists for backward compatibility with version 4 configurations.
 * In version 5, the wmbus component has been split into:
 * - wmbus_radio: For radio communication (CC1101, SX1276, etc.)
 * - wmbus_meter: For meter definitions and sensors
 * - wmbus_common: For common wmbusmeters code
 * 
 * This stub component allows ESPHome to find the component when it's explicitly
 * listed in external_components configuration, preventing "component not found" errors.
 */
class WMBusComponent : public Component {
 public:
  void setup() override {
    ESP_LOGW(TAG, "The 'wmbus' component is deprecated and exists only for backward compatibility.");
    ESP_LOGW(TAG, "Please update your configuration to use 'wmbus_radio', 'wmbus_meter', and 'wmbus_common' instead.");
    ESP_LOGW(TAG, "See: https://github.com/VivantSenior/esphome-components/blob/main/docs/CC1101_USAGE.md");
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "WMBus (Compatibility Layer)");
    ESP_LOGCONFIG(TAG, "  This is a stub component for backward compatibility.");
    ESP_LOGCONFIG(TAG, "  Please migrate to wmbus_radio, wmbus_meter, and wmbus_common.");
  }

  float get_setup_priority() const override {
    return setup_priority::HARDWARE;
  }
};

}  // namespace wmbus
}  // namespace esphome
