# CC1101 Support in ESPHome WMBus Components

## Overview

CC1101 support has been successfully integrated into the main branch of this repository, providing backward compatibility with version_4 while maintaining the modern architecture of version_5.

## Hardware Requirements

- ESP32 board (ESP8266 not supported in version 5)
- CC1101 module
- Appropriate connections between ESP32 and CC1101

## Pin Connections

| CC1101 Pin | ESP32 Pin | Description |
|------------|-----------|-------------|
| SCK        | GPIO14    | SPI Clock (configurable) |
| MOSI       | GPIO13    | SPI MOSI (configurable) |
| MISO       | GPIO12    | SPI MISO (configurable) |
| CSN        | GPIO2     | Chip Select (configurable) |
| GDO0       | GPIO5     | FIFO threshold interrupt (configurable) |
| GDO2       | GPIO4     | Sync detection (configurable) |
| VCC        | 3.3V      | Power supply |
| GND        | GND       | Ground |

**Important Notes:**
- CC1101 operates at 3.3V - do NOT connect to 5V
- Use short wires for SPI connections to reduce noise
- GDO0 and GDO2 are essential for proper operation

## Configuration Example

### Basic Configuration

```yaml
esphome:
  name: wmbus-reader
  friendly_name: WMBus Reader
  platformio_options:
    upload_speed: 921600

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
  level: DEBUG
  baud_rate: 115200

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:

time:
  - platform: homeassistant

api:

ota:
  - platform: esphome

web_server:
  version: 3

# SPI Bus Configuration
spi:
  clk_pin: GPIO14
  mosi_pin: GPIO13
  miso_pin: GPIO12

# CC1101 WMBus Radio
wmbus_radio:
  radio_type: CC1101
  cs_pin: GPIO2
  reset_pin: GPIO2      # Usually same as CS pin
  irq_pin: GPIO5        # GDO0
  gdo0_pin: GPIO5       # GDO0 - for FIFO threshold
  gdo2_pin: GPIO4       # GDO2 - for sync detection
  frequency: 868.95     # MHz - optional, default is 868.95
  on_frame:
    - then:
        - logger.log:
            format: "RSSI: %ddBm T: %s (%d) %s"
            args: [ frame->rssi(), frame->as_hex().c_str(), frame->data().size(), toString(frame->link_mode()) ]
```

### Complete Example with Meter

```yaml
wmbus_radio:
  radio_type: CC1101
  cs_pin: GPIO2
  reset_pin: GPIO2
  irq_pin: GPIO5
  gdo0_pin: GPIO5
  gdo2_pin: GPIO4
  frequency: 868.95
  on_frame:
    - then:
        - logger.log:
            format: "Frame: RSSI=%ddBm Size=%d"
            args: [ frame->rssi(), frame->data().size() ]

# Define your meter
wmbus_meter:
  - id: water_meter
    meter_id: 0x12345678  # Your meter ID
    type: apator162       # Your meter type
    key: "00000000000000000000000000000000"  # Encryption key if needed
    mode: 
      - T1
      - C1

# Create sensors from meter data
sensor:
  - platform: wmbus_meter
    parent_id: water_meter
    field: total_m3
    name: "Water Consumption"
    accuracy_decimals: 3
    unit_of_measurement: "m³"
    device_class: water
    state_class: total_increasing
    icon: "mdi:water"

  - platform: wmbus_meter
    parent_id: water_meter
    field: rssi_dbm
    name: "Water Meter RSSI"
    accuracy_decimals: 0
    unit_of_measurement: "dBm"
    device_class: signal_strength
    state_class: measurement
    entity_category: diagnostic

text_sensor:
  - platform: wmbus_meter
    parent_id: water_meter
    field: timestamp
    name: "Water Meter Last Update"
```

## Configuration Options

### Radio Configuration

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `radio_type` | string | Yes | - | Must be "CC1101" |
| `cs_pin` | pin | Yes | - | SPI chip select pin |
| `reset_pin` | pin | Yes | - | Reset pin (usually same as cs_pin) |
| `irq_pin` | pin | Yes | - | Interrupt pin (GDO0) |
| `gdo0_pin` | pin | No | - | GDO0 pin (usually same as irq_pin) |
| `gdo2_pin` | pin | No | - | GDO2 pin for sync detection |
| `frequency` | float | No | 868.95 | Frequency in MHz (300-928) |

### Frequency Settings by Region

- **Europe (EU)**: 868.95 MHz (default)
- **North America (US)**: 915.0 MHz
- **Custom**: Any value between 300-928 MHz

## Supported Link Modes

Currently optimized for:
- **C1 Mode** (Mode C, Block A and B) - Most common in Europe
- **T1 Mode** - Basic support (full support requires decode3of6 integration)

## Troubleshooting

### No frames received

1. **Check wiring**: Verify all SPI and GDO connections
2. **Check frequency**: Ensure it matches your region (868.95 MHz for EU)
3. **Check power**: CC1101 must have stable 3.3V supply
4. **Check logs**: Look for "CC1101 initialization failed" messages
5. **Try increasing log level**: Set logger level to VERBOSE

### Low RSSI or missed frames

1. **Antenna**: Ensure CC1101 has proper antenna (17.3cm wire for 868 MHz)
2. **Placement**: Keep CC1101 away from metal objects and other electronics
3. **Power supply**: Ensure stable power supply to CC1101
4. **Distance**: Check if meter is within range (typically 10-30m indoors)

### CC1101 not detected

```
[E][CC1101:xxx]: CC1101 not found or invalid version: 0
```

**Solutions:**
1. Check SPI wiring (CLK, MOSI, MISO, CS)
2. Verify CC1101 has power (3.3V)
3. Check if CS pin is correct
4. Try different CS pin if current one is strapping pin

### Buffer overflow warnings

```
[E][CC1101:xxx]: Buffer overflow prevented
```

This indicates a corrupted or malformed frame. The system will automatically recover. If this happens frequently:
1. Check for electromagnetic interference
2. Verify antenna is properly connected
3. Try moving CC1101 away from noise sources

## Performance Tips

1. **Use short SPI wires** (< 10cm) to reduce noise
2. **Add 100nF capacitor** near CC1101 VCC pin
3. **Use proper antenna** (17.3cm wire for 868 MHz)
4. **Keep CC1101 away** from ESP32's WiFi antenna
5. **Use shielded enclosure** if operating in noisy environment

## Differences from Version 4

| Feature | Version 4 | Version 5 (Current) |
|---------|-----------|---------------------|
| Architecture | Monolithic | Modular (transceiver pattern) |
| External Library | ELECHOUSE_CC1101 | Native SPI implementation |
| Configuration | Simple pins | Advanced configuration |
| Security | Basic | Buffer overflow protection |
| Radio Support | CC1101 only | CC1101, SX1276, extensible |

## Migration from Version 4

If you're migrating from version_4:

**Old (version_4):**
```yaml
wmbus:
  mosi_pin: GPIO13
  miso_pin: GPIO5
  clk_pin: GPIO2
  cs_pin: GPIO14
  gdo0_pin: GPIO15
  gdo2_pin: GPIO16
  frequency: 868.950
```

**New (version_5):**
```yaml
spi:
  clk_pin: GPIO2
  mosi_pin: GPIO13
  miso_pin: GPIO5

wmbus_radio:
  radio_type: CC1101
  cs_pin: GPIO14
  reset_pin: GPIO14
  irq_pin: GPIO15    # gdo0_pin
  gdo0_pin: GPIO15
  gdo2_pin: GPIO16
  frequency: 868.95
```

## Technical Details

### State Machine

CC1101 implementation uses a state machine for reliable packet reception:

1. **INIT_RX**: Initialize receiver
2. **WAIT_FOR_SYNC**: Wait for sync word detection (GDO2 goes high)
3. **WAIT_FOR_DATA**: Wait for FIFO threshold (GDO0 goes high)
4. **READ_DATA**: Read data from FIFO until packet complete

### Buffer Management

- Buffer size: 500 bytes (sufficient for WMBus packets)
- Overflow protection: Multiple safety checks
- Automatic recovery: System restarts RX on errors

### SPI Communication

- Clock speed: 2 MHz (from SPIDevice base class)
- Command strobes: Direct SPI transfers
- Burst reads: Delegate-based for efficiency
- Status registers: With C0 prefix for read access

## Support

For issues or questions:
1. Check logs with DEBUG or VERBOSE level
2. Review this documentation
3. Open an issue on GitHub
4. Include your configuration and logs

## Credits

- Original CC1101 implementation: SzczepanLeon (version_4)
- Architecture refactoring: VivantSenior (version_5)
- CC1101 RF settings: Based on TI application note SWRA234A

## License

GPL-compatible, see repository LICENSE file for details.
