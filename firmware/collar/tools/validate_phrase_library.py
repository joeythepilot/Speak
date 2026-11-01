#!/usr/bin/env python3
"""Validate the Prototype 001 back-door phrase library."""

from __future__ import annotations

import argparse
import wave
from pathlib import Path

SAMPLE_RATE = 16_000
MAX_LITTLEFS_BYTES = 0x5F0000
RESERVED_HEADROOM_BYTES = 512 * 1024

EXPECTED_FILES = [
    *(f"initial_{i:02d}.wav" for i in range(1, 9)),
    *(f"escalation_1_{i:02d}.wav" for i in range(1, 6)),
    *(f"escalation_2_{i:02d}.wav" for i in range(1, 6)),
    *(f"escalation_3_{i:02d}.wav" for i in range(1, 3)),
]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "library_dir",
        type=Path,
        nargs="?",
        default=Path(__file__).resolve().parents[1] / "data" / "outside",
    )
    args = parser.parse_args()

    errors: list[str] = []
    total_bytes = 0
    total_seconds = 0.0

    expected = set(EXPECTED_FILES)
    actual = {path.name for path in args.library_dir.glob("*.wav")}

    for missing in sorted(expected - actual):
        errors.append(f"missing: {missing}")
    for unexpected in sorted(actual - expected):
        errors.append(f"unexpected: {unexpected}")

    for name in EXPECTED_FILES:
        path = args.library_dir / name
        if not path.exists():
            continue
        try:
            with wave.open(str(path), "rb") as wav:
                if wav.getnchannels() != 1:
                    errors.append(f"{name}: expected mono")
                if wav.getsampwidth() != 2:
                    errors.append(f"{name}: expected 16-bit PCM")
                if wav.getframerate() != SAMPLE_RATE:
                    errors.append(f"{name}: expected {SAMPLE_RATE} Hz")
                if wav.getcomptype() != "NONE":
                    errors.append(f"{name}: expected uncompressed PCM")
                total_seconds += wav.getnframes() / wav.getframerate()
        except (EOFError, wave.Error) as exc:
            errors.append(f"{name}: invalid WAV ({exc})")
        total_bytes += path.stat().st_size

    usable_budget = MAX_LITTLEFS_BYTES - RESERVED_HEADROOM_BYTES
    if total_bytes > usable_budget:
        errors.append(
            f"library uses {total_bytes} bytes; safe budget is {usable_budget} bytes"
        )

    if errors:
        raise SystemExit("Phrase library validation failed:\n  " + "\n  ".join(errors))

    print(
        f"Validated {len(EXPECTED_FILES)} files, {total_seconds:.2f}s, "
        f"{total_bytes / 1024 / 1024:.2f} MiB"
    )
    print(
        f"LittleFS headroom: {(MAX_LITTLEFS_BYTES - total_bytes) / 1024 / 1024:.2f} MiB"
    )


if __name__ == "__main__":
    main()
