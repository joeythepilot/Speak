#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#include "prototype_config.h"

namespace {

using namespace speak::prototype;

BLEScan* scanner = nullptr;
volatile int latestRssi = -127;
volatile uint32_t lastSeenMs = 0;

bool spotActive = false;
bool triggeredThisVisit = false;
uint32_t dwellStartedMs = 0;
uint32_t lastTriggerMs = 0;
uint32_t quietUntilMs = 0;
bool lastButtonState = HIGH;
uint32_t lastButtonChangeMs = 0;

struct WavInfo {
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
};

uint16_t readU16(File& file) {
  uint8_t bytes[2] = {0, 0};
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) return 0;
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(File& file) {
  uint8_t bytes[4] = {0, 0, 0, 0};
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) return 0;
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

bool readWavHeader(File& file, WavInfo& info) {
  char riff[4];
  char wave[4];
  if (file.read(reinterpret_cast<uint8_t*>(riff), 4) != 4) return false;
  (void)readU32(file);
  if (file.read(reinterpret_cast<uint8_t*>(wave), 4) != 4) return false;
  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) return false;

  bool foundFmt = false;
  bool foundData = false;

  while (file.available()) {
    char chunkId[4];
    if (file.read(reinterpret_cast<uint8_t*>(chunkId), 4) != 4) break;
    const uint32_t chunkSize = readU32(file);
    const uint32_t chunkStart = file.position();

    if (memcmp(chunkId, "fmt ", 4) == 0) {
      const uint16_t audioFormat = readU16(file);
      info.channels = readU16(file);
      info.sampleRate = readU32(file);
      (void)readU32(file);  // byte rate
      (void)readU16(file);  // block align
      info.bitsPerSample = readU16(file);
      if (audioFormat != 1) return false;  // PCM only for Prototype 001.
      foundFmt = true;
    } else if (memcmp(chunkId, "data", 4) == 0) {
      info.dataOffset = file.position();
      info.dataSize = chunkSize;
      foundData = true;
      break;
    }

    uint32_t next = chunkStart + chunkSize;
    if (chunkSize & 1U) ++next;
    file.seek(next);
  }

  return foundFmt && foundData && info.channels == 1 && info.bitsPerSample == 16;
}

bool configureI2S(uint32_t sampleRate) {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = sampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 128,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
      .bck_io_num = I2S_BCLK_PIN,
      .ws_io_num = I2S_LRCLK_PIN,
      .data_out_num = I2S_DOUT_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_uninstall(I2S_NUM_0);
  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  return i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) == ESP_OK;
}

bool playWav(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[audio] missing %s\n", path);
    return false;
  }

  WavInfo info;
  if (!readWavHeader(file, info)) {
    Serial.println("[audio] unsupported WAV; require 16-bit mono PCM");
    file.close();
    return false;
  }

  if (!configureI2S(info.sampleRate)) {
    Serial.println("[audio] I2S setup failed");
    file.close();
    return false;
  }

  file.seek(info.dataOffset);
  uint32_t remaining = info.dataSize;
  uint8_t buffer[512];

  Serial.printf("[audio] play %s @ %lu Hz\n", path, static_cast<unsigned long>(info.sampleRate));

  while (remaining > 0) {
    const size_t requested = min(sizeof(buffer), static_cast<size_t>(remaining));
    const size_t read = file.read(buffer, requested);
    if (read == 0) break;

    size_t written = 0;
    if (i2s_write(I2S_NUM_0, buffer, read, &written, portMAX_DELAY) != ESP_OK) {
      Serial.println("[audio] write failed");
      file.close();
      return false;
    }
    remaining -= read;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  file.close();
  return remaining == 0;
}

class SpotAdvertisementCallbacks final : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    const String address = advertisedDevice.getAddress().toString().c_str();
    if (!address.equalsIgnoreCase(TARGET_SPOT_ADDRESS)) return;

    latestRssi = advertisedDevice.getRSSI();
    lastSeenMs = millis();
  }
};

bool quietNow(uint32_t now) {
  return static_cast<int32_t>(quietUntilMs - now) > 0;
}

void handleQuietButton(uint32_t now) {
  const bool state = digitalRead(QUIET_BUTTON_PIN);

  if (state != lastButtonState && now - lastButtonChangeMs >= 30) {
    lastButtonChangeMs = now;
    lastButtonState = state;

    if (state == LOW) {
      if (quietNow(now)) {
        quietUntilMs = 0;
        Serial.println("[quiet] disabled");
      } else {
        quietUntilMs = now + QUIET_DURATION_MS;
        Serial.printf("[quiet] enabled for %lu minutes\n",
                      static_cast<unsigned long>(QUIET_DURATION_MS / 60000UL));
      }
    }
  }
}

void handleSpotState(uint32_t now) {
  const bool recentlySeen = (now - lastSeenMs) <= SPOT_LOST_AFTER_MS;
  const int rssi = latestRssi;

  if (!spotActive) {
    if (recentlySeen && rssi >= TRIGGER_RSSI_DBM) {
      spotActive = true;
      dwellStartedMs = now;
      Serial.printf("[spot] entered trigger zone, RSSI %d dBm\n", rssi);
    }
    return;
  }

  if (!recentlySeen || rssi <= RESET_RSSI_DBM) {
    spotActive = false;
    triggeredThisVisit = false;
    dwellStartedMs = 0;
    Serial.printf("[spot] exited/reset, RSSI %d dBm\n", rssi);
    return;
  }

  const bool dwellSatisfied = (now - dwellStartedMs) >= DWELL_MS;
  const bool cooledDown = (now - lastTriggerMs) >= COOLDOWN_MS;

  if (dwellSatisfied && !triggeredThisVisit && cooledDown) {
    triggeredThisVisit = true;

    if (quietNow(now)) {
      Serial.println("[spot] qualified but Quiet Mode suppressed playback");
      return;
    }

    Serial.printf("[spot] QUALIFIED after %lu ms at RSSI %d dBm\n",
                  static_cast<unsigned long>(now - dwellStartedMs), rssi);

    if (playWav(AUDIO_FILE)) {
      lastTriggerMs = millis();
      Serial.println("[spot] playback complete");
    } else {
      Serial.println("[spot] playback failed; remaining silent");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSpeak! Prototype 001");

  pinMode(QUIET_BUTTON_PIN, INPUT_PULLUP);

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] LittleFS mount failed; audio unavailable");
  } else {
    Serial.println("[fs] LittleFS ready");
  }

  BLEDevice::init("Speak-Prototype-001");
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new SpotAdvertisementCallbacks(), false);
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(90);
  scanner->start(0, nullptr, false);

  Serial.printf("[ble] scanning continuously for %s\n", TARGET_SPOT_ADDRESS);
  Serial.printf("[spot] enter >= %d dBm, reset <= %d dBm, dwell %lu ms\n",
                TRIGGER_RSSI_DBM, RESET_RSSI_DBM,
                static_cast<unsigned long>(DWELL_MS));
}

void loop() {
  const uint32_t now = millis();
  handleQuietButton(now);
  handleSpotState(now);
  delay(20);
}
