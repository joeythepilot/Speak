#pragma once

#include <Arduino.h>

namespace speak::prototype {

// Replace with the Prototype 001 ESP32-C3 Spot's BLE address after we flash
// the Spot firmware and read its address from Serial. Keep lowercase for
// comparison with the ESP32 BLE library's string format.
inline constexpr char TARGET_SPOT_ADDRESS[] = "70:af:09:2a:7d:4e";

// Proximity behavior. Entry is deliberately stronger than reset so the collar
// does not chatter when RSSI wanders around one threshold.
inline constexpr int TRIGGER_RSSI_DBM = -62;
inline constexpr int RESET_RSSI_DBM = -72;
inline constexpr float RSSI_EMA_ALPHA = 0.35F;
inline constexpr uint32_t SPOT_LOST_AFTER_MS = 1800;
inline constexpr uint32_t RESET_CONFIRM_MS = 1000;
inline constexpr uint32_t REENTRY_COOLDOWN_MS = 5000;

// Back-door interaction timeline, measured from confirmed entry into the Spot.
// The minimum post-playback gap prevents a long phrase from running directly
// into the next escalation.
inline constexpr uint32_t INITIAL_DWELL_MS = 2000;
inline constexpr uint32_t ESCALATION_1_DWELL_MS = 12000;
inline constexpr uint32_t ESCALATION_2_DWELL_MS = 25000;
inline constexpr uint32_t ESCALATION_3_DWELL_MS = 40000;
inline constexpr uint32_t MIN_GAP_AFTER_PLAYBACK_MS = 2500;

// Quiet button: one terminal to this GPIO, the other to GND. INPUT_PULLUP.
inline constexpr gpio_num_t QUIET_BUTTON_PIN = GPIO_NUM_4;
inline constexpr uint32_t QUIET_DURATION_MS = 15UL * 60UL * 1000UL;

// NULLLAB NS4168 I2S amplifier.
// Bench wiring: ESP32 3V3 -> amp VCC, GND -> GND.
// These three GPIOs carry digital audio only.
inline constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_12;
inline constexpr gpio_num_t I2S_LRCLK_PIN = GPIO_NUM_11;
inline constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_10;

// Conservative software attenuation for first bench/dog-safe SPL measurements.
// Increase only after measuring at 3, 6 and 9 inches with the actual enclosure.
inline constexpr uint8_t AUDIO_GAIN_PERCENT = 30;

}  // namespace speak::prototype
