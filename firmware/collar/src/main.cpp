#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <esp_system.h>

#include <cstring>

#include "prototype_config.h"

namespace {

using namespace speak::prototype;

constexpr size_t MAX_POOL_FILES = 8;
constexpr size_t MAX_RECENT_HISTORY = 3;

const char* const OUTSIDE_INITIAL_FILES[] = {
    "/outside/initial_01.wav", "/outside/initial_02.wav",
    "/outside/initial_03.wav", "/outside/initial_04.wav",
    "/outside/initial_05.wav", "/outside/initial_06.wav",
    "/outside/initial_07.wav", "/outside/initial_08.wav",
};

const char* const OUTSIDE_ESCALATION_1_FILES[] = {
    "/outside/escalation_1_01.wav", "/outside/escalation_1_02.wav",
    "/outside/escalation_1_03.wav", "/outside/escalation_1_04.wav",
    "/outside/escalation_1_05.wav",
};

const char* const OUTSIDE_ESCALATION_2_FILES[] = {
    "/outside/escalation_2_01.wav", "/outside/escalation_2_02.wav",
    "/outside/escalation_2_03.wav", "/outside/escalation_2_04.wav",
    "/outside/escalation_2_05.wav",
};

const char* const OUTSIDE_ESCALATION_3_FILES[] = {
    "/outside/escalation_3_01.wav", "/outside/escalation_3_02.wav",
};

struct PhrasePool {
  const char* label;
  const char* const* files;
  size_t count;
  uint8_t historyLimit;
  uint8_t recent[MAX_RECENT_HISTORY];
  uint8_t recentCount;
};

PhrasePool outsideInitial = {
    "initial", OUTSIDE_INITIAL_FILES,
    sizeof(OUTSIDE_INITIAL_FILES) / sizeof(OUTSIDE_INITIAL_FILES[0]),
    3, {0, 0, 0}, 0};
PhrasePool outsideEscalation1 = {
    "escalation 1", OUTSIDE_ESCALATION_1_FILES,
    sizeof(OUTSIDE_ESCALATION_1_FILES) /
        sizeof(OUTSIDE_ESCALATION_1_FILES[0]),
    2, {0, 0, 0}, 0};
PhrasePool outsideEscalation2 = {
    "escalation 2", OUTSIDE_ESCALATION_2_FILES,
    sizeof(OUTSIDE_ESCALATION_2_FILES) /
        sizeof(OUTSIDE_ESCALATION_2_FILES[0]),
    2, {0, 0, 0}, 0};
PhrasePool outsideEscalation3 = {
    "escalation 3", OUTSIDE_ESCALATION_3_FILES,
    sizeof(OUTSIDE_ESCALATION_3_FILES) /
        sizeof(OUTSIDE_ESCALATION_3_FILES[0]),
    1, {0, 0, 0}, 0};

static_assert(sizeof(OUTSIDE_INITIAL_FILES) / sizeof(OUTSIDE_INITIAL_FILES[0]) <=
                  MAX_POOL_FILES,
              "Phrase pool exceeds selector capacity");

BLEScan* scanner = nullptr;
portMUX_TYPE advertisementMux = portMUX_INITIALIZER_UNLOCKED;
volatile int advertisedRssi = -127;
volatile uint32_t advertisedSeenMs = 0;
volatile uint32_t advertisementSequence = 0;

bool haveSmoothedRssi = false;
float smoothedRssi = -127.0F;
uint32_t consumedAdvertisementSequence = 0;
uint32_t latestSeenMs = 0;
int latestRawRssi = -127;

bool spotActive = false;
bool resetCandidate = false;
bool completedSessionBefore = false;
uint32_t resetCandidateStartedMs = 0;
uint32_t sessionStartedMs = 0;
uint32_t lastPlaybackFinishedMs = 0;
uint32_t lastSessionEndedMs = 0;
uint32_t quietUntilMs = 0;
bool lastButtonState = HIGH;
uint32_t lastButtonChangeMs = 0;

enum class VisitStage : uint8_t {
  Initial,
  Escalation1,
  Escalation2,
  Escalation3,
  Complete,
};

VisitStage nextStage = VisitStage::Initial;

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
  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0)
    return false;

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

  return foundFmt && foundData && info.channels == 1 &&
         info.bitsPerSample == 16;
}

bool configureI2S(uint32_t sampleRate) {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = sampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
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
  return i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT,
                     I2S_CHANNEL_STEREO) == ESP_OK;
}

int16_t scaledSample(const uint8_t* bytes) {
  const int16_t sample = static_cast<int16_t>(
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8));
  const int32_t scaled =
      (static_cast<int32_t>(sample) * AUDIO_GAIN_PERCENT) / 100;
  return static_cast<int16_t>(scaled);
}

bool playWav(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[audio] missing %s\\n", path);
    return false;
  }

  WavInfo info;
  if (!readWavHeader(file, info)) {
    Serial.printf("[audio] unsupported WAV %s; require 16-bit mono PCM\\n",
                  path);
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

  // Read mono PCM and duplicate each sample into both LR slots. This matches
  // NULLLAB's NS4168 reference framing: 16-bit MSB-aligned data in both slots.
  uint8_t monoBuffer[256];
  int16_t stereoBuffer[256];  // 128 mono samples -> 256 stereo samples.

  Serial.printf("[audio] play %s @ %lu Hz, gain %u%%\\n", path,
                static_cast<unsigned long>(info.sampleRate),
                static_cast<unsigned>(AUDIO_GAIN_PERCENT));

  while (remaining > 0) {
    size_t requested =
        remaining < sizeof(monoBuffer) ? static_cast<size_t>(remaining)
                                       : sizeof(monoBuffer);
    // 16-bit PCM must stay sample-aligned.
    requested &= ~static_cast<size_t>(1);
    if (requested == 0) break;

    const size_t read = file.read(monoBuffer, requested);
    if (read == 0) break;

    const size_t monoSamples = read / 2;
    for (size_t i = 0; i < monoSamples; ++i) {
      const int16_t sample = scaledSample(&monoBuffer[i * 2]);
      stereoBuffer[i * 2] = sample;
      stereoBuffer[i * 2 + 1] = sample;
    }

    const size_t stereoBytes = monoSamples * 2 * sizeof(int16_t);
    size_t written = 0;
    if (i2s_write(I2S_NUM_0, stereoBuffer, stereoBytes, &written,
                  portMAX_DELAY) != ESP_OK) {
      Serial.println("[audio] write failed");
      file.close();
      return false;
    }
    if (written != stereoBytes) {
      Serial.printf("[audio] short write: %u/%u bytes\\n",
                    static_cast<unsigned>(written),
                    static_cast<unsigned>(stereoBytes));
      file.close();
      return false;
    }

    remaining -= read;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  file.close();
  return remaining == 0;
}

bool isRecent(const PhrasePool& pool, uint8_t index) {
  for (uint8_t i = 0; i < pool.recentCount; ++i) {
    if (pool.recent[i] == index) return true;
  }
  return false;
}

void rememberPhrase(PhrasePool& pool, uint8_t index) {
  if (pool.count <= 1 || pool.historyLimit == 0) return;

  uint8_t limit = pool.historyLimit;
  if (limit > MAX_RECENT_HISTORY) limit = MAX_RECENT_HISTORY;
  if (limit >= pool.count) limit = static_cast<uint8_t>(pool.count - 1);

  if (pool.recentCount < limit) {
    pool.recent[pool.recentCount++] = index;
    return;
  }

  for (uint8_t i = 1; i < limit; ++i) {
    pool.recent[i - 1] = pool.recent[i];
  }
  pool.recent[limit - 1] = index;
}

int selectPhraseIndex(const PhrasePool& pool, uint32_t excludedMask,
                      bool ignoreHistory) {
  uint8_t candidates[MAX_POOL_FILES];
  size_t candidateCount = 0;

  for (size_t i = 0; i < pool.count && i < MAX_POOL_FILES; ++i) {
    if ((excludedMask & (1UL << i)) != 0) continue;
    if (!ignoreHistory && isRecent(pool, static_cast<uint8_t>(i))) continue;
    candidates[candidateCount++] = static_cast<uint8_t>(i);
  }

  if (candidateCount == 0) return -1;
  return candidates[esp_random() % candidateCount];
}

bool playRandomPhrase(PhrasePool& pool) {
  uint32_t attemptedMask = 0;

  for (size_t attempt = 0; attempt < pool.count; ++attempt) {
    int selected = selectPhraseIndex(pool, attemptedMask, false);
    if (selected < 0) selected = selectPhraseIndex(pool, attemptedMask, true);
    if (selected < 0) break;

    attemptedMask |= (1UL << selected);
    const char* path = pool.files[selected];
    Serial.printf("[phrase] %s selected %u/%u: %s\n", pool.label,
                  static_cast<unsigned>(selected + 1),
                  static_cast<unsigned>(pool.count), path);

    if (playWav(path)) {
      rememberPhrase(pool, static_cast<uint8_t>(selected));
      return true;
    }
  }

  Serial.printf("[phrase] no playable files in %s pool\n", pool.label);
  return false;
}

class SpotAdvertisementCallbacks final : public BLEAdvertisedDeviceCallbacks {
 public:
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    const String address = advertisedDevice.getAddress().toString().c_str();
    if (!address.equalsIgnoreCase(TARGET_SPOT_ADDRESS)) return;

    portENTER_CRITICAL(&advertisementMux);
    advertisedRssi = advertisedDevice.getRSSI();
    advertisedSeenMs = millis();
    ++advertisementSequence;
    portEXIT_CRITICAL(&advertisementMux);
  }
};

void updateSmoothedRssi() {
  int raw = -127;
  uint32_t seen = 0;
  uint32_t sequence = 0;

  portENTER_CRITICAL(&advertisementMux);
  raw = advertisedRssi;
  seen = advertisedSeenMs;
  sequence = advertisementSequence;
  portEXIT_CRITICAL(&advertisementMux);

  latestSeenMs = seen;
  latestRawRssi = raw;
  if (sequence == consumedAdvertisementSequence) return;

  consumedAdvertisementSequence = sequence;
  if (!haveSmoothedRssi) {
    smoothedRssi = static_cast<float>(raw);
    haveSmoothedRssi = true;
  } else {
    smoothedRssi = RSSI_EMA_ALPHA * static_cast<float>(raw) +
                   (1.0F - RSSI_EMA_ALPHA) * smoothedRssi;
  }
}

int roundedSmoothedRssi() {
  return static_cast<int>(smoothedRssi >= 0.0F ? smoothedRssi + 0.5F
                                               : smoothedRssi - 0.5F);
}

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

void startSession(uint32_t now) {
  spotActive = true;
  resetCandidate = false;
  resetCandidateStartedMs = 0;
  sessionStartedMs = now;
  lastPlaybackFinishedMs = 0;
  nextStage = VisitStage::Initial;
  Serial.printf("[spot] back-door session started, raw %d dBm, smoothed %d dBm\n",
                latestRawRssi, roundedSmoothedRssi());
}

void endSession(uint32_t now, const char* reason) {
  const uint32_t elapsed = spotActive ? now - sessionStartedMs : 0;
  spotActive = false;
  resetCandidate = false;
  resetCandidateStartedMs = 0;
  sessionStartedMs = 0;
  lastPlaybackFinishedMs = 0;
  nextStage = VisitStage::Initial;
  lastSessionEndedMs = now;
  completedSessionBefore = true;
  Serial.printf("[spot] session reset after %lu ms: %s\n",
                static_cast<unsigned long>(elapsed), reason);
}

uint32_t stageDwellMs(VisitStage stage) {
  switch (stage) {
    case VisitStage::Initial:
      return INITIAL_DWELL_MS;
    case VisitStage::Escalation1:
      return ESCALATION_1_DWELL_MS;
    case VisitStage::Escalation2:
      return ESCALATION_2_DWELL_MS;
    case VisitStage::Escalation3:
      return ESCALATION_3_DWELL_MS;
    case VisitStage::Complete:
      return UINT32_MAX;
  }
  return UINT32_MAX;
}

PhrasePool* stagePool(VisitStage stage) {
  switch (stage) {
    case VisitStage::Initial:
      return &outsideInitial;
    case VisitStage::Escalation1:
      return &outsideEscalation1;
    case VisitStage::Escalation2:
      return &outsideEscalation2;
    case VisitStage::Escalation3:
      return &outsideEscalation3;
    case VisitStage::Complete:
      return nullptr;
  }
  return nullptr;
}

VisitStage followingStage(VisitStage stage) {
  switch (stage) {
    case VisitStage::Initial:
      return VisitStage::Escalation1;
    case VisitStage::Escalation1:
      return VisitStage::Escalation2;
    case VisitStage::Escalation2:
      return VisitStage::Escalation3;
    case VisitStage::Escalation3:
    case VisitStage::Complete:
      return VisitStage::Complete;
  }
  return VisitStage::Complete;
}

bool playbackGapSatisfied(uint32_t now) {
  return lastPlaybackFinishedMs == 0 ||
         now - lastPlaybackFinishedMs >= MIN_GAP_AFTER_PLAYBACK_MS;
}

void handleDueStage(uint32_t now) {
  if (nextStage == VisitStage::Complete) return;

  const uint32_t elapsed = now - sessionStartedMs;
  const uint32_t dueAt = stageDwellMs(nextStage);
  if (elapsed < dueAt || !playbackGapSatisfied(now)) return;

  PhrasePool* pool = stagePool(nextStage);
  if (pool == nullptr) {
    nextStage = VisitStage::Complete;
    return;
  }

  Serial.printf("[stage] %s due at %lu ms (elapsed %lu ms)\n", pool->label,
                static_cast<unsigned long>(dueAt),
                static_cast<unsigned long>(elapsed));

  if (quietNow(now)) {
    Serial.printf("[stage] Quiet Mode suppressed %s\n", pool->label);
    lastPlaybackFinishedMs = now;
  } else {
    const bool played = playRandomPhrase(*pool);
    lastPlaybackFinishedMs = millis();
    Serial.printf("[stage] %s %s\n", pool->label,
                  played ? "playback complete" : "playback unavailable");
  }

  nextStage = followingStage(nextStage);
  if (nextStage == VisitStage::Complete) {
    Serial.println("[stage] final escalation complete; waiting for Spot exit");
  }
}

void handleSpotState(uint32_t now) {
  updateSmoothedRssi();

  const bool recentlySeen =
      latestSeenMs != 0 && (now - latestSeenMs) <= SPOT_LOST_AFTER_MS;
  const int rssi = roundedSmoothedRssi();

  if (!spotActive) {
    const bool cooledDown = !completedSessionBefore ||
                            (now - lastSessionEndedMs) >= REENTRY_COOLDOWN_MS;
    if (cooledDown && recentlySeen && haveSmoothedRssi &&
        rssi >= TRIGGER_RSSI_DBM) {
      startSession(now);
    }
    return;
  }

  if (!recentlySeen) {
    endSession(now, "beacon no longer seen");
    return;
  }

  if (rssi <= RESET_RSSI_DBM) {
    if (!resetCandidate) {
      resetCandidate = true;
      resetCandidateStartedMs = now;
      Serial.printf("[spot] reset candidate, smoothed RSSI %d dBm\n", rssi);
    } else if (now - resetCandidateStartedMs >= RESET_CONFIRM_MS) {
      endSession(now, "sustained exit RSSI");
      return;
    }
    return;  // Do not escalate while the dog appears to be leaving the Spot.
  } else if (resetCandidate) {
    resetCandidate = false;
    resetCandidateStartedMs = 0;
    Serial.printf("[spot] reset cancelled, smoothed RSSI recovered to %d dBm\n",
                  rssi);
  }

  handleDueStage(now);
}

void logAudioInventory() {
  const PhrasePool* pools[] = {&outsideInitial, &outsideEscalation1,
                               &outsideEscalation2, &outsideEscalation3};
  size_t present = 0;
  size_t expected = 0;

  for (const PhrasePool* pool : pools) {
    expected += pool->count;
    for (size_t i = 0; i < pool->count; ++i) {
      if (LittleFS.exists(pool->files[i])) {
        ++present;
      } else {
        Serial.printf("[fs] missing %s\n", pool->files[i]);
      }
    }
  }
  Serial.printf("[fs] phrase inventory %u/%u files present\n",
                static_cast<unsigned>(present),
                static_cast<unsigned>(expected));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSpeak! Prototype 001 - Back Door Potty Test");

  pinMode(QUIET_BUTTON_PIN, INPUT_PULLUP);

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] LittleFS mount failed; audio unavailable");
  } else {
    Serial.println("[fs] LittleFS ready");
    logAudioInventory();

    // Bench bring-up helper: hold the Quiet button LOW during boot/reset to
    // prove LittleFS -> I2S -> amplifier -> speaker before BLE is involved.
    if (digitalRead(QUIET_BUTTON_PIN) == LOW) {
      Serial.println("[self-test] Quiet held at boot; playing initial_01");
      playWav(OUTSIDE_INITIAL_FILES[0]);
      Serial.println("[self-test] complete; release Quiet to continue");
      while (digitalRead(QUIET_BUTTON_PIN) == LOW) {
        delay(20);
      }
      delay(100);
    }
  }

  BLEDevice::init("Speak-Prototype-001");
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new SpotAdvertisementCallbacks(), false);
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(90);
  scanner->start(0, nullptr, false);

  Serial.printf("[ble] scanning continuously for %s\n", TARGET_SPOT_ADDRESS);
  Serial.printf("[spot] enter >= %d dBm, reset <= %d dBm, EMA %.2f\n",
                TRIGGER_RSSI_DBM, RESET_RSSI_DBM, RSSI_EMA_ALPHA);
  Serial.printf("[timeline] initial %lu, E1 %lu, E2 %lu, E3 %lu ms\n",
                static_cast<unsigned long>(INITIAL_DWELL_MS),
                static_cast<unsigned long>(ESCALATION_1_DWELL_MS),
                static_cast<unsigned long>(ESCALATION_2_DWELL_MS),
                static_cast<unsigned long>(ESCALATION_3_DWELL_MS));
}

void loop() {
  const uint32_t now = millis();
  handleQuietButton(now);
  handleSpotState(now);
  delay(20);
}
