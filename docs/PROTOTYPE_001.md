# Prototype 001 - Back Door Potty Test

## Purpose

Prototype 001 answers the first product-shaped engineering question:

> Can a wearable ESP32-S3 reliably recognize one back-door BLE Spot, distinguish a deliberate wait from a walk-by, play varied speech, and escalate naturally while the dog remains there?

The first build stays on the bench until power, audio level, BLE behavior and thermal behavior are proven.

## Current hardware

### Collar controller

- DORHEA ESP32-S3 DevKit C N16R8
- 16 MB flash / 8 MB PSRAM
- Use the USB-to-UART Type-C port for initial programming
- Prototype firmware target: `esp32-s3-n16r8`

### Audio

NULLLAB NS4168 I2S amplifier + matched 4 ohm / 3 W speaker.

Bench wiring:

| Source | NULLLAB amp |
| --- | --- |
| Battery module 3.3V | VCC |
| Common GND | GND |
| ESP32 GPIO12 | BCLK |
| ESP32 GPIO11 | LRCLK / WS |
| ESP32 GPIO10 | DIN |

Do **not** power the amplifier from the ESP32 board's 3V3 pin. The amplifier should use the battery module's separate regulated 3.3V rail so audio current does not load the ESP32 regulator.

The speaker plugs only into the amplifier's speaker output. Neither speaker lead is connected to ESP32 ground.

### Quiet button

- One lead -> GPIO4
- Other lead -> GND
- Firmware uses `INPUT_PULLUP`
- Holding Quiet during boot/reset runs the audio self-test and plays `initial_01.wav`

The Affection button is intentionally left out of the first electrical bring-up.

### Back Door Spot

Prototype Spots use an ESP32-C3 Super Mini rather than a proprietary beacon.

- Firmware project: `firmware/spot`
- Advertised name: `SPEAK-OUTSIDE-001`
- The Spot prints its BLE address at boot
- Copy that address into `TARGET_SPOT_ADDRESS` in `firmware/collar/include/prototype_config.h`

### Portable power

NULLLAB PM11 integrated power module:

- 1200 mAh LiPo
- 5 V regulated output up to 1.2 A
- 3.3 V regulated output up to 800 mA
- USB-C charging
- Physical master switch

Do not use the PM11 during initial USB bench bring-up. Later, with USB disconnected, PM11 5V feeds the ESP32 5V pin and PM11 GND feeds ESP32 GND.

## Build order

### Checkpoint 1 - board only

1. Solder the supplied headers to one N16R8 board if they are not already fitted.
2. Put the board on the breadboard.
3. Leave PM11, amplifier, buttons and Spot disconnected.
4. Connect a known-good USB data cable to the board's USB-to-UART Type-C port.
5. Build and upload `firmware/collar`.
6. Open Serial Monitor at 115200 baud.

Do not proceed until the controller boots reliably.

### Checkpoint 2 - audio path

1. Disconnect USB power.
2. Wire the NULLLAB amplifier using the table above.
3. Plug the matched speaker into the amplifier.
4. Wire the Quiet button from GPIO4 to GND.
5. Put the approved WAV library under `firmware/collar/data/outside/`.
6. Upload the LittleFS filesystem image.
7. Reconnect USB.
8. Hold Quiet and press Reset.
9. The collar should play `initial_01.wav` at the firmware's conservative 30% digital gain.

Do not attach the speaker near Asher yet.

### Checkpoint 3 - acoustic test

Using the final speaker orientation:

1. Measure A-weighted SPL at approximate dog-ear positions: 3, 6 and 9 inches.
2. Measure intelligibility at 3, 10, 15 and 20 feet in the human-facing direction.
3. Repeat with normal TV/room noise.
4. Check for startup pops, clipping and enclosure/amplifier heating.

Do not raise gain from the default until the measured results justify it.

### Checkpoint 4 - first Spot

1. Flash `firmware/spot` to one ESP32-C3 Super Mini.
2. Open its Serial Monitor at 115200 baud.
3. Record the printed BLE address.
4. Paste it into `TARGET_SPOT_ADDRESS` in the collar config.
5. Rebuild and reflash the collar.
6. Power the C3 Spot near the test back door.
7. Observe RSSI and session diagnostics over Serial.

### Checkpoint 5 - interaction test

Verify:

- quick walk-by does not speak
- intentional dwell does speak
- initial phrase randomizes
- escalation 1 / 2 / 3 occur only while presence continues
- brief RSSI dips do not reset the session
- confirmed exit rearms a later visit
- completed session remains silent while the dog simply occupies the area

### Checkpoint 6 - PM11 portable-power test

Only after the USB-powered system works:

1. Disconnect USB from the ESP32.
2. PM11 5V -> ESP32 5V.
3. PM11 GND -> ESP32 GND.
4. Switch PM11 on.
5. Verify identical boot, BLE and audio behavior.
6. Measure current draw and runtime.

Do not power the ESP32 from PM11 and its programming USB connection at the same time during Prototype 001 bench testing.

### Checkpoint 7 - enclosure / supervised wear

Only after electrical and acoustic acceptance:

- mount battery/power module rigidly
- provide strain relief
- orient speaker outward, away from Asher's ears
- eliminate sharp edges and exposed conductors
- verify temperature after extended operation
- start with short, directly supervised harness/chest tests before collar wear

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

## Default interaction timeline

All times are measured from confirmed entry:

| Stage | Default threshold |
| --- | ---: |
| Initial phrase | 2 seconds |
| Escalation 1 | 12 seconds |
| Escalation 2 | 25 seconds |
| Escalation 3 | 40 seconds |

The system also enforces a minimum gap after each clip and recent-phrase suppression.

## Current safety gate

Prototype 001 begins at 30% digital audio gain. This is an engineering precaution, not a certified safe acoustic level. The wearable is not cleared for Asher until SPL, thermal behavior, power behavior and mechanical containment are measured on the actual assembled prototype.
