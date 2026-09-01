#!/usr/bin/env python3
"""Create a short mono 16-bit PCM WAV for Prototype 001 audio-path testing."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 16_000
DURATION_SECONDS = 0.75
FREQUENCY_HZ = 660.0
AMPLITUDE = 0.18


def main() -> None:
    output = Path(__file__).resolve().parents[1] / "data" / "spot.wav"
    output.parent.mkdir(parents=True, exist_ok=True)

    frames = int(SAMPLE_RATE * DURATION_SECONDS)
    with wave.open(str(output), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)

        for i in range(frames):
            envelope = min(1.0, i / (SAMPLE_RATE * 0.03), (frames - i) / (SAMPLE_RATE * 0.05))
            sample = AMPLITUDE * envelope * math.sin(2.0 * math.pi * FREQUENCY_HZ * i / SAMPLE_RATE)
            wav.writeframesraw(struct.pack("<h", int(sample * 32767)))

    print(f"Wrote {output}")
    print("Format: 16 kHz, mono, 16-bit PCM")


if __name__ == "__main__":
    main()
