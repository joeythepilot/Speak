#!/usr/bin/env python3
"""Create a complete synthetic phrase library for firmware/CI testing."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 16_000
DURATION_SECONDS = 0.22
AMPLITUDE = 0.12

PHRASE_FILES = [
    *(f"initial_{i:02d}.wav" for i in range(1, 9)),
    *(f"escalation_1_{i:02d}.wav" for i in range(1, 6)),
    *(f"escalation_2_{i:02d}.wav" for i in range(1, 6)),
    *(f"escalation_3_{i:02d}.wav" for i in range(1, 3)),
]


def write_tone(output: Path, frequency_hz: float) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    frames = int(SAMPLE_RATE * DURATION_SECONDS)

    with wave.open(str(output), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)

        for i in range(frames):
            envelope = min(
                1.0,
                i / (SAMPLE_RATE * 0.015),
                (frames - i) / (SAMPLE_RATE * 0.025),
            )
            sample = (
                AMPLITUDE
                * envelope
                * math.sin(2.0 * math.pi * frequency_hz * i / SAMPLE_RATE)
            )
            wav.writeframesraw(struct.pack("<h", int(sample * 32767)))


def main() -> None:
    output_dir = Path(__file__).resolve().parents[1] / "data" / "outside"
    for index, name in enumerate(PHRASE_FILES):
        write_tone(output_dir / name, 440.0 + (index * 17.0))

    print(f"Wrote {len(PHRASE_FILES)} fixture WAVs to {output_dir}")
    print("Format: 16 kHz, mono, 16-bit PCM")


if __name__ == "__main__":
    main()
