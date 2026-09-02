# Prototype 001 audio library

The firmware expects these device-ready files under `data/outside/`:

- `initial_01.wav` through `initial_08.wav`
- `escalation_1_01.wav` through `escalation_1_05.wav`
- `escalation_2_01.wav` through `escalation_2_05.wav`
- `escalation_3_01.wav` through `escalation_3_02.wav`

All files must be uncompressed RIFF/WAVE, 16 kHz, mono, 16-bit PCM.

To prepare the approved ElevenLabs MP3 source files:

```bash
python tools/prepare_phrase_library.py /path/to/source-mp3-directory
python tools/validate_phrase_library.py
```

The preparation script removes only leading and trailing silence, preserves pauses inside each performance, and normalizes the clips to a conservative common loudness. The generated WAVs and manifest are intentionally ignored by Git because they are build assets rather than source code.

For CI or an audio-path bench test without the real voice assets:

```bash
python tools/make_test_wav.py
```
