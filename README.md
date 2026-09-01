# Speak!

**The talking dog collar.**

Speak! is a privacy-first wearable communication system that lets a dog trigger locally stored speech by intentionally interacting with small wireless **Spots** placed near meaningful people, places, and objects.

## Prototype 001 goal

Prove one thing reliably:

> **One Spot → proximity + dwell → one local audio file plays from the collar.**

Prototype 001 intentionally does **not** include the mobile app, AI voice generation, Combos, Responses, Community Spots, or cloud services. Those belong after the basic interaction is trustworthy.

## Hardware target

The first firmware scaffold targets an **Adafruit ESP32-S3 Feather** (or compatible ESP32-S3 board) with:

- MAX98357A I2S audio amplifier
- 4 Ω / 3 W speaker
- BLE beacon used as the first Spot
- momentary pushbutton for Quiet Mode testing
- USB power initially; battery testing follows after the interaction works

All GPIO assignments and trigger values live in `include/prototype_config.h` so the board can be rewired without rewriting the interaction logic.

## Repository layout

```text
firmware/collar/          ESP32-S3 collar firmware
  include/                prototype configuration
  src/                    firmware source
  data/                   LittleFS audio assets
  tools/                  local development helpers
docs/                     build notes and test plans
```

## First build

1. Install VS Code + PlatformIO.
2. Open `firmware/collar` as the PlatformIO project.
3. Edit `include/prototype_config.h` and set `TARGET_SPOT_ADDRESS` to the BLE MAC address of the test beacon.
4. Run `python tools/make_test_wav.py` to create `data/spot.wav`.
5. Upload the LittleFS filesystem image.
6. Build and upload the firmware.
7. Open Serial Monitor at 115200 baud.
8. Bring the configured beacon near the board and hold it there through the dwell interval.

The generated `spot.wav` is only an audio-path test tone. Replace it with a **16-bit mono PCM WAV** once we have the first real Speak! phrase ready.

## Product guardrails already reflected in the prototype

- Playback is local.
- The collar does not need internet access.
- No microphone, GPS, camera, or cellular radio is part of the collar architecture.
- Triggering requires both proximity and dwell.
- A physical Quiet control exists from the first prototype.
- Failure should prefer silence over uncontrolled playback.

See `docs/PROTOTYPE_001.md` for the acceptance test.
