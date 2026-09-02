#!/usr/bin/env python3
"""Prepare ElevenLabs MP3 phrases for the Speak! Prototype 001 LittleFS image.

Outputs 16 kHz, mono, 16-bit PCM WAV files with conservative loudness
normalization and only leading/trailing silence removed. Internal pauses remain.
Requires ffmpeg and ffprobe on PATH.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import wave
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SAMPLE_RATE = 16_000
TARGET_LUFS = -21.0
TARGET_LRA = 7.0
TARGET_TRUE_PEAK = -3.0
SILENCE_THRESHOLD_DB = -45.0
SILENCE_MIN_SECONDS = 0.20
EDGE_PADDING_SECONDS = 0.08

FILE_RE = re.compile(
    r"^Outside_(?:(Initial)|Escalation_([123]))_(\d{2})(?:\(\d+\))?\.mp3$",
    re.IGNORECASE,
)

EXPECTED_COUNTS = {
    "initial": 8,
    "escalation_1": 5,
    "escalation_2": 5,
    "escalation_3": 2,
}


@dataclass(frozen=True)
class PhraseSource:
    source: Path
    tier: str
    index: int

    @property
    def output_name(self) -> str:
        return f"{self.tier}_{self.index:02d}.wav"


def run(command: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=check)


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise SystemExit(f"Required tool not found on PATH: {name}")


def probe_duration(path: Path) -> float:
    result = run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=noprint_wrappers=1:nokey=1",
            str(path),
        ]
    )
    return float(result.stdout.strip())


def detect_outer_trim(path: Path, duration: float) -> tuple[float, float]:
    result = run(
        [
            "ffmpeg",
            "-hide_banner",
            "-nostats",
            "-i",
            str(path),
            "-af",
            f"silencedetect=noise={SILENCE_THRESHOLD_DB}dB:d={SILENCE_MIN_SECONDS}",
            "-f",
            "null",
            "-",
        ],
        check=False,
    )

    starts = [float(value) for value in re.findall(r"silence_start:\s*([0-9.]+)", result.stderr)]
    ends = [float(value) for value in re.findall(r"silence_end:\s*([0-9.]+)", result.stderr)]

    intervals: list[tuple[float, float]] = []
    end_iter = iter(ends)
    current_end = next(end_iter, None)
    for start in starts:
        while current_end is not None and current_end < start:
            current_end = next(end_iter, None)
        if current_end is not None:
            intervals.append((start, current_end))
            current_end = next(end_iter, None)

    trim_start = 0.0
    trim_end = duration

    if intervals and intervals[0][0] <= 0.05:
        trim_start = max(0.0, intervals[0][1] - EDGE_PADDING_SECONDS)

    if intervals and intervals[-1][1] >= duration - 0.08:
        trim_end = min(duration, intervals[-1][0] + EDGE_PADDING_SECONDS)

    if trim_end - trim_start < 0.25:
        return 0.0, duration
    return trim_start, trim_end


def extract_loudnorm_stats(stderr: str) -> dict[str, str]:
    matches = re.findall(r"\{\s*\"input_i\".*?\}", stderr, flags=re.DOTALL)
    if not matches:
        raise RuntimeError("ffmpeg did not return loudnorm JSON")
    return json.loads(matches[-1])


def normalize_phrase(source: Path, output: Path, start: float, end: float) -> None:
    common_input = ["-ss", f"{start:.6f}", "-to", f"{end:.6f}", "-i", str(source)]
    first_filter = (
        f"loudnorm=I={TARGET_LUFS}:LRA={TARGET_LRA}:TP={TARGET_TRUE_PEAK}:"
        "print_format=json"
    )
    first = run(
        [
            "ffmpeg",
            "-hide_banner",
            "-nostats",
            *common_input,
            "-af",
            first_filter,
            "-f",
            "null",
            "-",
        ],
        check=False,
    )
    stats = extract_loudnorm_stats(first.stderr)

    second_filter = (
        f"loudnorm=I={TARGET_LUFS}:LRA={TARGET_LRA}:TP={TARGET_TRUE_PEAK}:"
        f"measured_I={stats['input_i']}:measured_LRA={stats['input_lra']}:"
        f"measured_TP={stats['input_tp']}:measured_thresh={stats['input_thresh']}:"
        f"offset={stats['target_offset']}:linear=true:print_format=summary"
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-nostats",
            *common_input,
            "-af",
            second_filter,
            "-ar",
            str(SAMPLE_RATE),
            "-ac",
            "1",
            "-c:a",
            "pcm_s16le",
            "-map_metadata",
            "-1",
            str(output),
        ]
    )


def discover_sources(source_dir: Path) -> list[PhraseSource]:
    phrases: list[PhraseSource] = []
    for path in sorted(source_dir.glob("*.mp3")):
        match = FILE_RE.match(path.name)
        if not match:
            continue
        initial, escalation, index_text = match.groups()
        tier = "initial" if initial else f"escalation_{escalation}"
        phrases.append(PhraseSource(path, tier, int(index_text)))

    seen: set[tuple[str, int]] = set()
    for phrase in phrases:
        key = (phrase.tier, phrase.index)
        if key in seen:
            raise SystemExit(f"Duplicate phrase slot: {phrase.tier} {phrase.index:02d}")
        seen.add(key)

    return phrases


def validate_inventory(phrases: list[PhraseSource]) -> None:
    counts = {tier: 0 for tier in EXPECTED_COUNTS}
    for phrase in phrases:
        counts[phrase.tier] += 1

    errors = [
        f"{tier}: expected {expected}, found {counts[tier]}"
        for tier, expected in EXPECTED_COUNTS.items()
        if counts[tier] != expected
    ]
    if errors:
        raise SystemExit("Phrase inventory mismatch:\n  " + "\n  ".join(errors))


def wav_metadata(path: Path) -> dict[str, Any]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getframerate() != SAMPLE_RATE:
            raise RuntimeError(f"Unexpected WAV format: {path}")
        frames = wav.getnframes()
        duration = frames / wav.getframerate()

    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        "duration_seconds": round(duration, 3),
        "frames": frames,
        "bytes": path.stat().st_size,
        "sha256": digest,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path, help="Directory containing ElevenLabs MP3 files")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "data" / "outside",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "data" / "outside_manifest.json",
    )
    args = parser.parse_args()

    require_tool("ffmpeg")
    require_tool("ffprobe")

    phrases = discover_sources(args.source_dir)
    validate_inventory(phrases)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    manifest_items: list[dict[str, Any]] = []
    for phrase in phrases:
        source_duration = probe_duration(phrase.source)
        start, end = detect_outer_trim(phrase.source, source_duration)
        output = args.output_dir / phrase.output_name
        normalize_phrase(phrase.source, output, start, end)
        metadata = wav_metadata(output)
        manifest_items.append(
            {
                "tier": phrase.tier,
                "index": phrase.index,
                "source_file": phrase.source.name,
                "source_sha256": hashlib.sha256(phrase.source.read_bytes()).hexdigest(),
                "output_file": f"outside/{phrase.output_name}",
                "source_duration_seconds": round(source_duration, 3),
                "trim_start_seconds": round(start, 3),
                "trim_end_seconds": round(end, 3),
                **metadata,
            }
        )
        print(f"Prepared {phrase.source.name} -> {output.name} ({metadata['duration_seconds']:.2f}s)")

    manifest = {
        "format": {
            "sample_rate_hz": SAMPLE_RATE,
            "channels": 1,
            "bits_per_sample": 16,
            "codec": "PCM",
            "target_lufs": TARGET_LUFS,
            "target_true_peak_dbfs": TARGET_TRUE_PEAK,
        },
        "expected_counts": EXPECTED_COUNTS,
        "total_files": len(manifest_items),
        "total_bytes": sum(item["bytes"] for item in manifest_items),
        "total_duration_seconds": round(sum(item["duration_seconds"] for item in manifest_items), 3),
        "phrases": sorted(manifest_items, key=lambda item: (item["tier"], item["index"])),
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote manifest: {args.manifest}")
    print(f"Total audio: {manifest['total_duration_seconds']:.2f}s, {manifest['total_bytes'] / 1024 / 1024:.2f} MiB")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr or str(exc))
        raise SystemExit(exc.returncode) from exc
