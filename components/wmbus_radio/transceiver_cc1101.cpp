#include "transceiver_cc1101.h"
#include "cc1101_rf_settings.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace wmbus_radio {
static const char *TAG = "CC1101";

void CC1101::setup() {
  this->common_setup();

  ESP_LOGV(TAG, "Setup CC1101");

  if (this->gdo0_pin_) {
    this->gdo0_pin_->setup();
    this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  if (this->gdo2_pin_) {
    this->gdo2_pin_->setup();
    this->gdo2_pin_->pin_mode(gpio::FLAG_INPUT);
  }

  if (!this->cc1101_init()) {
    ESP_LOGE(TAG, "CC1101 initialization failed");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "CC1101 setup complete");
}

bool CC1101::cc1101_init() {
  ESP_LOGV(TAG, "Initializing CC1101");

  // Reset the chip
  this->reset();
  delay(10);

  // Write RF configuration
  for (uint8_t i = 0; i < TMODE_RF_SETTINGS_LEN; i++) {
    this->spi_write(TMODE_RF_SETTINGS_BYTES[i << 1],
                    TMODE_RF_SETTINGS_BYTES[(i << 1) + 1]);
  }

  // Set frequency
  uint32_t freq_reg = uint32_t(this->frequency_ * 65536 / 26);
  uint8_t freq2 = (freq_reg >> 16) & 0xFF;
  uint8_t freq1 = (freq_reg >> 8) & 0xFF;
  uint8_t freq0 = freq_reg & 0xFF;

  ESP_LOGD(TAG, "Set CC1101 frequency to %3.3fMHz [%02X %02X %02X]",
           this->frequency_, freq2, freq1, freq0);

  this->spi_write(CC1101_FREQ2, freq2);
  this->spi_write(CC1101_FREQ1, freq1);
  this->spi_write(CC1101_FREQ0, freq0);

  // Calibrate - command strobe
  this->delegate_->begin_transaction();
  this->delegate_->transfer(CC1101_SCAL);
  this->delegate_->end_transaction();

  // Check version - status register read (bit 6 and 7 set)
  uint8_t version = this->spi_read(CC1101_VERSION | 0xC0);

  if (version == 0 || version == 255) {
    ESP_LOGE(TAG, "CC1101 not found or invalid version: %d", version);
    return false;
  }

  ESP_LOGD(TAG, "CC1101 version: %d", version);

  // Enter RX mode
  this->restart_rx();

  delay(4);

  return true;
}

void CC1101::set_gdo0_pin(InternalGPIOPin *gdo0_pin) {
  this->gdo0_pin_ = gdo0_pin;
}

void CC1101::set_gdo2_pin(InternalGPIOPin *gdo2_pin) {
  this->gdo2_pin_ = gdo2_pin;
}

void CC1101::set_frequency(float frequency) { this->frequency_ = frequency; }

optional<uint8_t> CC1101::read() {
  // Check if we need to restart RX
  bool reinit_needed =
      (millis() - this->sync_time_) > this->max_wait_time_ ? true : false;

  if (reinit_needed) {
    uint8_t marc_state = this->spi_read(CC1101_MARCSTATE | 0xC0);
    if (marc_state != MARCSTATE_RX) {
      this->start_rx(true);
      return {};
    }
  }

  // Process RX state machine
  switch (this->rx_loop_.state) {
  case INIT_RX:
    this->start_rx();
    return {};

  case WAIT_FOR_SYNC:
    if (this->gdo2_pin_ && this->gdo2_pin_->digital_read()) {
      this->rx_loop_.state = WAIT_FOR_DATA;
      this->sync_time_ = millis();
    }
    break;

  case WAIT_FOR_DATA:
    if (this->gdo0_pin_ && this->gdo0_pin_->digital_read()) {
      uint8_t preamble[2];
      // Read the first 3 bytes
      uint8_t temp[3];
      this->delegate_->begin_transaction();
      this->delegate_->transfer(CC1101_RXFIFO | 0xC0); // Burst read
      for (int i = 0; i < 3; i++) {
        temp[i] = this->delegate_->transfer(0x00);
      }
      this->delegate_->end_transaction();

      this->rx_loop_.bytesRx = 3;
      const uint8_t *currentByte = temp;

      // Mode C
      if (*currentByte == WMBUS_MODE_C_PREAMBLE) {
        currentByte++;
        // Block A
        if (*currentByte == WMBUS_BLOCK_A_PREAMBLE) {
          currentByte++;
          this->rx_loop_.lengthField = *currentByte;
          // Calculate packet size for mode C Block A
          uint8_t L = this->rx_loop_.lengthField;
          uint8_t nrBlocks = L < 26 ? 2 : ((L - 26) / 16 + 3);
          this->rx_loop_.length = 2 + (L + 1 + 2 * nrBlocks);
        }
        // Block B
        else if (*currentByte == WMBUS_BLOCK_B_PREAMBLE) {
          currentByte++;
          this->rx_loop_.lengthField = *currentByte;
          this->rx_loop_.length = 2 + 1 + this->rx_loop_.lengthField;
        }
        // Unknown type, reinit loop
        else {
          this->rx_loop_.state = INIT_RX;
          return {};
        }

        // Copy first 3 bytes to buffer
        this->rx_buffer_[0] = temp[0];
        this->rx_buffer_[1] = temp[1];
        this->rx_buffer_[2] = temp[2];
        this->rx_loop_.pByteIndex = this->rx_buffer_ + 3;
      }
      // Mode T is not fully supported in this simplified version
      // For full T mode support, decode3of6 implementation is needed
      else {
        this->rx_loop_.state = INIT_RX;
        return {};
      }

      this->rx_loop_.bytesLeft = this->rx_loop_.length - 3;

      if (this->rx_loop_.length < MAX_FIXED_LENGTH) {
        this->spi_write(CC1101_PKTLEN, (uint8_t)this->rx_loop_.length);
        this->spi_write(CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
        this->rx_loop_.cc1101Mode = FIXED;
      } else {
        this->spi_write(CC1101_PKTLEN,
                        (uint8_t)(this->rx_loop_.length % MAX_FIXED_LENGTH));
      }

      this->rx_loop_.state = READ_DATA;
      this->max_wait_time_ += this->extra_time_;
      this->spi_write(CC1101_FIFOTHR, RX_FIFO_THRESHOLD);
    }
    break;

  case READ_DATA:
    if (this->gdo0_pin_ && this->gdo0_pin_->digital_read()) {
      if ((this->rx_loop_.bytesLeft < MAX_FIXED_LENGTH) &&
          (this->rx_loop_.cc1101Mode == INFINITE)) {
        this->spi_write(CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
        this->rx_loop_.cc1101Mode = FIXED;
      }

      uint8_t bytesInFIFO = this->spi_read(CC1101_RXBYTES | 0xC0) & 0x7F;
      if (bytesInFIFO > 1) {
        // Read data from FIFO
        this->delegate_->begin_transaction();
        this->delegate_->transfer(CC1101_RXFIFO | 0xC0); // Burst read
        for (int i = 0; i < bytesInFIFO - 1; i++) {
          *(this->rx_loop_.pByteIndex++) = this->delegate_->transfer(0x00);
        }
        this->delegate_->end_transaction();

        this->rx_loop_.bytesLeft -= (bytesInFIFO - 1);
        this->rx_loop_.bytesRx += (bytesInFIFO - 1);
        this->max_wait_time_ += this->extra_time_;
      }
    }
    break;
  }

  // Check for end of packet
  uint8_t rxbytes = this->spi_read(CC1101_RXBYTES | 0xC0);
  bool overfl = rxbytes & 0x80;
  bool sync_lost = this->gdo2_pin_ ? !this->gdo2_pin_->digital_read() : false;

  if (!overfl && sync_lost && (this->rx_loop_.state > WAIT_FOR_DATA)) {
    // Read remaining bytes
    if (this->rx_loop_.bytesLeft > 0) {
      this->delegate_->begin_transaction();
      this->delegate_->transfer(CC1101_RXFIFO | 0xC0); // Burst read
      for (int i = 0; i < this->rx_loop_.bytesLeft; i++) {
        *(this->rx_loop_.pByteIndex++) = this->delegate_->transfer(0x00);
      }
      this->delegate_->end_transaction();
      this->rx_loop_.bytesRx += this->rx_loop_.bytesLeft;
    }

    // Get RSSI and LQI
    this->last_rssi_ = this->get_rssi();

    ESP_LOGV(TAG, "Received %d bytes from CC1101 Rx, RSSI: %d dBm",
             this->rx_loop_.bytesRx, this->last_rssi_);

    if (this->rx_loop_.length != this->rx_loop_.bytesRx) {
      ESP_LOGW(TAG, "Length mismatch: expected %d, received %d",
               this->rx_loop_.length, this->rx_loop_.bytesRx);
    }

    this->rx_loop_.complete = true;
    this->rx_loop_.state = INIT_RX;

    // Return first byte to signal data available
    return this->rx_buffer_[0];
  }

  return {};
}

void CC1101::restart_rx() { this->start_rx(true); }

bool CC1101::start_rx(bool force) {
  bool reinit_needed =
      (millis() - this->sync_time_) > this->max_wait_time_ ? true : false;

  if (!force && !reinit_needed) {
    uint8_t marc_state = this->spi_read(CC1101_MARCSTATE | 0xC0);
    if (marc_state == MARCSTATE_RX) {
      return false;
    }
  }

  // Init RX
  this->rx_loop_.state = INIT_RX;
  this->sync_time_ = millis();
  this->max_wait_time_ = this->extra_time_;

  // Enter IDLE mode - command strobe
  this->delegate_->begin_transaction();
  this->delegate_->transfer(CC1101_SIDLE);
  this->delegate_->end_transaction();
  delayMicroseconds(100);

  // Flush TX and RX FIFOs - command strobes
  this->delegate_->begin_transaction();
  this->delegate_->transfer(CC1101_SFTX);
  this->delegate_->end_transaction();
  this->delegate_->begin_transaction();
  this->delegate_->transfer(CC1101_SFRX);
  this->delegate_->end_transaction();

  // Initialize RX info
  this->rx_loop_.lengthField = 0;
  this->rx_loop_.length = 0;
  this->rx_loop_.bytesLeft = 0;
  this->rx_loop_.bytesRx = 0;
  this->rx_loop_.pByteIndex = this->rx_buffer_;
  this->rx_loop_.complete = false;
  this->rx_loop_.cc1101Mode = INFINITE;

  std::fill(std::begin(this->rx_buffer_), std::end(this->rx_buffer_), 0);

  // Set RX FIFO threshold
  this->spi_write(CC1101_FIFOTHR, RX_FIFO_START_THRESHOLD);
  // Set infinite length mode
  this->spi_write(CC1101_PKTCTRL0, INFINITE_PACKET_LENGTH);

  // Enter RX mode - command strobe
  this->delegate_->begin_transaction();
  this->delegate_->transfer(CC1101_SRX);
  this->delegate_->end_transaction();
  delayMicroseconds(100);

  this->rx_loop_.state = WAIT_FOR_SYNC;

  return true;
}

int8_t CC1101::get_rssi() {
  uint8_t rssi_raw = this->spi_read(CC1101_RSSI | 0xC0);
  int16_t rssi_dbm;

  if (rssi_raw >= 128) {
    rssi_dbm = (int16_t)((int16_t)(rssi_raw - 256) / 2) - 74;
  } else {
    rssi_dbm = (rssi_raw / 2) - 74;
  }

  return (int8_t)rssi_dbm;
}

const char *CC1101::get_name() { return TAG; }

} // namespace wmbus_radio
} // namespace esphome
