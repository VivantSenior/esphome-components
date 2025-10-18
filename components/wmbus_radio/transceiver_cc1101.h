#pragma once
#include "transceiver.h"

namespace esphome {
namespace wmbus_radio {

// CC1101 state machine
#define MARCSTATE_IDLE 0x01
#define MARCSTATE_RX 0x0D

// RX FIFO thresholds
#define RX_FIFO_START_THRESHOLD 0
#define RX_FIFO_THRESHOLD 10

// Packet length modes
#define FIXED_PACKET_LENGTH 0x00
#define INFINITE_PACKET_LENGTH 0x02

#define MAX_FIXED_LENGTH 256

// WMBus preambles
#define WMBUS_MODE_C_PREAMBLE 0x54
#define WMBUS_BLOCK_A_PREAMBLE 0xCD
#define WMBUS_BLOCK_B_PREAMBLE 0x3D

enum RxLoopState : uint8_t {
  INIT_RX = 0,
  WAIT_FOR_SYNC = 1,
  WAIT_FOR_DATA = 2,
  READ_DATA = 3,
};

enum Cc1101LengthMode : uint8_t {
  INFINITE = 0,
  FIXED = 1,
};

typedef struct RxLoopData {
  uint16_t bytesRx;
  uint8_t lengthField;
  uint16_t length;
  uint16_t bytesLeft;
  uint8_t *pByteIndex;
  bool complete;
  Cc1101LengthMode cc1101Mode;
  RxLoopState state;
} RxLoopData;

class CC1101 : public RadioTransceiver {
public:
  void setup() override;
  optional<uint8_t> read() override;
  void restart_rx() override;
  int8_t get_rssi() override;
  const char *get_name() override;

  void set_gdo0_pin(InternalGPIOPin *gdo0_pin);
  void set_gdo2_pin(InternalGPIOPin *gdo2_pin);
  void set_frequency(float frequency);

protected:
  bool cc1101_init();
  bool start_rx(bool force = true);
  bool rx_task();

  InternalGPIOPin *gdo0_pin_{nullptr};
  InternalGPIOPin *gdo2_pin_{nullptr};
  float frequency_{868.95f};

  RxLoopData rx_loop_;
  uint8_t rx_buffer_[500];
  uint32_t sync_time_{0};
  uint8_t extra_time_{50};
  uint8_t max_wait_time_{50};

  int8_t last_rssi_{0};
};

} // namespace wmbus_radio
} // namespace esphome
