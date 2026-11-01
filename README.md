# Speak!

**The talking dog collar.**

Speak! is a privacy-first wearable communication system that lets a dog trigger locally stored speech by intentionally interacting with small wireless **Spots** placed near meaningful people, places, and objects.

## Prototype 001 goal

Prove the first complete interaction at Asher's back door:

> **One BLE Spot + intentional dwell -> a randomized local phrase -> escalating follow-ups while Asher remains at the door.**

Prototype 001 intentionally does **not** include the mobile app, cloud services, AI conversation, Community Spots, or passive object tags. Those follow after the physical interaction is trustworthy.

## Hardware target

The firmware targets an **Adafruit ESP32-S3 Feather 8 MB** with:

- MAX98357A I2S audio amplifier
- 4 ohm / 3 W speaker
- Blue Charm BC011 beacon used as the back-door Spot
- momentary pushbutton for Quiet Mode testing
- USB power initially; battery testing follows after the interaction works

All GPIO assignments, proximity thresholds, escalation times, and the conservative software volume cap live in `include/prototype_config.h`.

## What the firmware now does

- Continuously scans for one configured BLE address.
- Smooths RSSI with an exponential moving average.
- Uses different entry and reset thresholds plus reset confirmation.
- Requires a two-second initial dwell.
- Randomly chooses from eight initial phrases.
- Advances through three timed escalation pools while the dog remains present.
- Suppresses the last several choices so phrases do not immediately repeat.
- Resets only after confirmed exit, then enforces a short reentry cooldown.
- Logs every state transition and phrase choice over Serial.
- Applies conservative software attenuation before I2S output.
- Keeps the physical Quiet control active from the first prototype.

## Repository layout

```text
firmware/collar/          ESP32-S3 collar firmware
  include/                prototype configuration
  src/                    BLE, state-machine, randomization and audio code
  data/                   LittleFS phrase library location
  tools/                  audio preparation and validation helpers
docs/                     build notes and acceptance tests
```

## Prepare the real phrase library

The approved ElevenLabs source set contains 20 MP3 files: eight initial phrases, five Escalation 1 phrases, five Escalation 2 phrases, and two Escalation 3 phrases.

With `ffmpeg` and `ffprobe` installed:

```bash
cd firmware/collar
python tools/prepare_phrase_library.py /path/to/the/source-mp3-files
python tools/validate_phrase_library.py
```

The output is 16 kHz, mono, 16-bit PCM under `data/outside/`. At the current phrase lengths, the real library is about 3.37 MiB. `partitions.csv` reserves roughly 5.94 MiB for LittleFS and intentionally disables OTA partitioning for this USB-flashed prototype.

## First hardware build

1. Install VS Code and PlatformIO.
2. Open `firmware/collar` as the PlatformIO project.
3. Set the BC011 BLE address in `include/prototype_config.h`.
4. Place the prepared WAV library in `data/outside/`.
5. Run **Upload Filesystem Image**.
6. Build and upload the firmware over USB.
7. Open Serial Monitor at 115200 baud.
8. Place the beacon at the back door and tune RSSI using the documented test matrix.
9. Measure playback SPL at 3, 6 and 9 inches before collar testing.

## Product guardrails reflected in the prototype

- Playback is local.
- The collar does not need internet access.
- No microphone, GPS, camera, or cellular radio is present.
- Triggering requires both proximity and dwell.
- A physical Quiet control exists.
- Missing or corrupt audio fails silent with diagnostics.
- Initial playback gain is deliberately conservative until measured.

See `docs/PROTOTYPE_001.md` for the acceptance test.
