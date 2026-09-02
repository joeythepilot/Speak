# Prototype 001 - Back Door Potty Test

## Purpose

Prototype 001 answers the first product-shaped engineering question:

> Can a collar-sized ESP32-S3 reliably recognize one back-door BLE Spot, distinguish a deliberate wait from a walk-by, play varied speech, and escalate naturally while the dog remains there?

The prototype is successful when the interaction is repeatable enough to tune around a real dog, a real doorway, and normal household movement.

## Hardware and wiring

### MAX98357A

- BCLK -> GPIO 12
- LRC / WS -> GPIO 11
- DIN -> GPIO 10
- VIN / GND according to the amplifier breakout's documented supply requirements
- 4 ohm / 3 W speaker -> amplifier output

### Quiet button

- One terminal -> GPIO 4
- Other terminal -> GND
- Firmware uses `INPUT_PULLUP`

### Spot

- Blue Charm BC011 BLE beacon
- Mount temporarily near the back door, out of Asher's reach
- Start at low transmitter power and tune from Serial evidence

Verify the breakout-board labels and supply voltage before applying power.

## Audio library

The device-ready library contains:

| Tier | Files | Purpose |
| --- | ---: | --- |
| Initial | 8 | Cheerful first request after deliberate dwell |
| Escalation 1 | 5 | Friendly reminder |
| Escalation 2 | 5 | Clear urgency |
| Escalation 3 | 2 | Final golden-retriever exasperation |

Required format:

- RIFF/WAVE
- PCM
- 16 kHz
- mono
- 16-bit

Prepare and validate the approved source MP3s:

```bash
cd firmware/collar
python tools/prepare_phrase_library.py /path/to/source-mp3-directory
python tools/validate_phrase_library.py
```

Then upload the LittleFS filesystem image before uploading firmware.

## Default interaction timeline

All times are measured from confirmed entry into the trigger zone:

| Stage | Default threshold |
| --- | ---: |
| Initial phrase | 2 seconds |
| Escalation 1 | 12 seconds |
| Escalation 2 | 25 seconds |
| Escalation 3 | 40 seconds |

A stage also requires at least 2.5 seconds after the previous clip finishes. This prevents a long line from colliding with the next escalation.

Each tier remembers recent choices. Initial phrases suppress the previous three selections, Escalations 1 and 2 suppress the previous two, and Escalation 3 suppresses the previous one.

## Proximity behavior

1. Only the configured beacon address is considered.
2. Incoming RSSI is smoothed with an exponential moving average.
3. Smoothed RSSI must cross the stronger entry threshold to start a session.
4. Leaving before the two-second dwell produces no speech.
5. Reset uses a weaker threshold than entry, preventing threshold chatter.
6. A weak RSSI must persist for the reset confirmation interval before ending the session.
7. Complete beacon loss resets the session.
8. Reentry is ignored briefly after exit to prevent doorway bounce.

All values remain easy to tune in `include/prototype_config.h`.

## Safety gate before dog use

The firmware begins at 30 percent digital amplitude. That is an engineering precaution, not proof of safe acoustic output.

Before attaching the prototype to Asher:

1. Test on the bench using the final speaker and temporary enclosure.
2. Measure A-weighted SPL at 3, 6 and 9 inches.
3. Confirm there is no startup pop, clipping, or unexpected maximum-volume output.
4. Increase gain only from measured evidence.
5. Begin dog testing under direct supervision.

## Acceptance criteria

Prototype 001 passes when all of the following are demonstrated:

1. Unknown beacons do nothing.
2. The configured Spot appears in Serial diagnostics.
3. A quick walk past the door produces no playback.
4. Remaining in range through initial dwell produces exactly one initial phrase.
5. Repeated visits vary the selected initial phrase.
6. The same initial phrase is not repeated within the recent-history window.
7. Remaining at the door reaches each escalation at the configured stage.
8. Leaving before a later escalation prevents that escalation.
9. A brief RSSI dip does not reset the session.
10. A confirmed exit resets the session and permits a later new visit.
11. Quiet Mode suppresses any due stage without producing a burst when Quiet ends.
12. Missing or corrupt assets produce silence and useful diagnostics.
13. The real 20-file library fits the LittleFS partition with at least 512 KiB reserved.
14. Measured playback stays within the owner-approved SPL envelope.

## Test matrix

Record at least ten attempts for each ordinary condition:

| Test | Expected |
| --- | --- |
| Walk past Spot quickly | 0/10 initial playbacks |
| Enter trigger distance for less than 2 seconds | 0/10 |
| Wait at the door through initial dwell | 10/10 initial playbacks |
| Leave immediately after initial phrase | no escalation |
| Remain through 12 seconds | one Escalation 1 phrase |
| Remain through 25 seconds | one Escalation 2 phrase |
| Remain through 40 seconds | one Escalation 3 phrase |
| Hover around entry threshold | no rapid session flapping |
| Leave reset zone, return after cooldown | one new session |
| Quiet Mode enabled | 0/10 audible stages |

Also log false triggers during ordinary kitchen traffic for at least one hour.

## What comes next

After Prototype 001 passes:

1. Tune beacon power and thresholds from recorded RSSI evidence.
2. Measure current draw during scanning, dwell, playback and quiet periods.
3. Test supervised use on Asher at the back door.
4. Add a second Spot profile without changing the session engine.
5. Prototype passive object recognition for toys and other deliberate interactions.
6. Preserve ordered interaction events so Responses and Combos do not require a data-model rewrite.

The mobile app, Live Speak, Community Spots and autonomous AI response remain outside Prototype 001.
