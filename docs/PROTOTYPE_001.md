# Prototype 001 — Acceptance Test

## Purpose

Prototype 001 answers the first engineering question only:

> Can a collar-sized ESP32-S3 platform reliably recognize one BLE Spot, require intentional dwell, and play one local audio asset through an I2S amplifier?

The prototype is successful when the interaction is repeatable enough that we can begin tuning real-world proximity instead of debugging the basic chain.

## Wiring target

### MAX98357A

- BCLK → GPIO 12
- LRC / WS → GPIO 11
- DIN → GPIO 10
- VIN / GND according to the amplifier breakout's documented supply requirements
- Speaker → amplifier output

### Quiet button

- One terminal → GPIO 4
- Other terminal → GND
- Firmware uses `INPUT_PULLUP`

**Verify the exact breakout-board pin labels and supply voltage before applying power.** The repository pin map is a prototype default, not a substitute for the module datasheet.

## Audio asset

Prototype firmware accepts:

- RIFF/WAVE
- PCM format
- 16-bit
- mono

Run:

```bash
cd firmware/collar
python tools/make_test_wav.py
```

Then upload the LittleFS image with PlatformIO's **Upload Filesystem Image** task.

The generated tone proves the audio path only. Replace `data/spot.wav` with an approved spoken phrase when ready.

## Beacon setup

1. Power the BLE beacon.
2. Determine its BLE MAC address using a phone scanner or temporary ESP32 scan sketch.
3. Set that value in `include/prototype_config.h` as `TARGET_SPOT_ADDRESS`.
4. Start with the default RSSI thresholds; they are deliberately easy to edit.

## Acceptance criteria

Prototype 001 passes when all of the following are demonstrated:

1. **Unknown beacons do nothing.**
2. The configured Spot can be seen in Serial diagnostics.
3. Crossing the entry threshold starts dwell but does not immediately speak.
4. Leaving before dwell completes produces no playback.
5. Remaining in range for the dwell interval produces exactly one playback.
6. Remaining near the Spot does not repeatedly retrigger.
7. Leaving past the reset threshold and returning allows a new interaction.
8. Entry and reset use different RSSI thresholds, preventing rapid threshold flapping.
9. Pressing Quiet suppresses an otherwise qualified interaction.
10. A missing/corrupt audio asset results in silence plus diagnostics, not uncontrolled output.

## Test matrix

Record at least ten attempts for each condition:

| Test | Expected |
| --- | --- |
| Walk past Spot quickly | 0/10 playbacks |
| Hold Spot just inside trigger distance for less than dwell | 0/10 |
| Hold Spot inside trigger distance through dwell | 10/10 |
| Stay beside Spot for 30 seconds | exactly one playback |
| Leave reset zone, return, dwell | one new playback |
| Quiet Mode enabled | 0/10 audible playbacks |

Do not optimize distance until this basic behavior is stable.

## What comes next

After Prototype 001 passes:

1. Measure RSSI around a real dog and real household objects.
2. Add RSSI smoothing rather than relying on the latest advertisement alone.
3. Move from one hard-coded Spot to a stored Spot profile.
4. Add a real spoken phrase and enforce measured safe output.
5. Measure current draw in active scan, idle/motionless, dwell, and playback states.
6. Add ordered interaction events so later Responses and Combos do not require a data-model rewrite.

The mobile app, Voice Studio, Live Speak, Community Spots and advanced Flows stay out until the physical interaction is trustworthy.
