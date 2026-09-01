#pragma once

#include <Arduino.h>

namespace speak::prototype {

// Replace this with the first test beacon's BLE address after scanning it.
// Keep lowercase for comparison with the ESP32 BLE library's string format.
inline constexpr char TARGET_SPOT_ADDRESS[] = "00:00:00:00:00:00";

// Prototype proximity behavior. These are engineering defaults, not product canon.
inline constexpr int TRIGGER_RSSI_DBM = -62;
inline constexpr int RESET_RSSI_DBM = -72;
inline constexpr uint32_t DWELL_MS = 3000;
inline constexpr uint32_t SPOT_LOST_AFTER_MS = 1500;
inline constexpr uint32_t COOLDOWN_MS = 10000;

// Quiet button: momentary switch from this pin to GND, using INPUT_PULLUP.
inline constexpr gpio_num_t QUIET_BUTTON_PIN = GPIO_NUM_4;
inline constexpr uint32_t QUIET_DURATION_MS = 15UL * 60UL * 1000UL;

// MAX98357A I2S pins. Verify wiring before powering hardware.
inline constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_12;
inline constexpr gpio_num_t I2S_LRCLK_PIN = GPIO_NUM_11;
inline constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_10;

inline constexpr char AUDIO_FILE[] = "/spot.wav";

}  // namespace speak::prototype
