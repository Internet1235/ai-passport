<p align="right"><a href="walkman.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Pocket Sunshine

An offline audio player for AI Passport, with nine lively Mandarin pep talks and three original instrumental tracks. Speech uses Microsoft Xiaoyi neural synthesis with extended pauses shortened. Public copy and speech use generic greetings; personal recordings are kept outside the repository. Firmware uses a dedicated `walkman` NVS namespace, preserving the Starbridge namespace during segmented flashing.

## Controls

| Input | Player | Settings |
| --- | --- | --- |
| UP / DOWN | Previous / next track | Select an item |
| OK | Play / pause | Change the selected setting |
| Hold UP / DOWN | Raise / lower volume | Raise / lower volume |
| Hold OK | Open settings | Return to player |

Volume has five levels from mute to 100%. Playback supports playlist repeat, track repeat, and play-through. The 15/30/60 minute timer measures actual playback time and pauses when playback pauses; reaching its deadline pauses audio rather than powering down the device. Track selection, volume and repeat mode are saved after 1.5 seconds of input inactivity. Boot starts paused, with the selected track at its beginning; timer and play position are not restored.

## Import local music

Create a local JSON array of objects with `file`, `title`, and optional `caption`. Paths are relative to the JSON file. MP3, M4A, WAV and other FFmpeg-readable files are supported. Use `--append` to keep the existing pep talks and add songs; without it, import replaces the playlist. Source audio remains outside generated firmware metadata; do not add private file paths to source control.

```json
[
  {"file": "music.wav", "title": "My favorite song", "caption": "A little sunshine"}
]
```

```bash
python3 tools/generate_walkman_assets.py --append --playlist /absolute/path/playlist.json
python3 tools/generate_walkman_fonts.py --font /absolute/path/LXGWWenKaiScreen.ttf
./tools/validate.sh
```

Use the pinned LXGW WenKai Screen v1.522 font described in [assets](../assets/README.md). Regenerate font subsets whenever titles or captions change. Import normalizes loudness, adds short edge fades and encodes 96 kbps, 22,050 Hz mono MP3. Maximum: 24 tracks, each shorter than eight minutes, combined encoded pack at most 6 MiB. Firmware layout verification is the final capacity check. Unsupported/oversized files fail without truncating the playlist. Playback streams 256-sample chunks from flash and never loads whole songs into RAM.

Run `python3 tools/generate_walkman_assets.py` to repack the source playlist with FFmpeg. To regenerate speech, install `edge-tts` and run `tools/generate_walkman_voices.py`; it sends the stored pep-talk text to Microsoft for synthesis. The on-device player is offline. Regenerate original instrumentals with `tools/compose_walkman_music.py`. See [asset index](../assets/README.md) for provenance. This application is developed separately in branch `feature/energy-walkman`.

## Validation

`tests/test_walkman.c` covers decoder saturation, navigation, repeat/end behavior, pause, mute, timer expiry, chunk invariance, persistence corruption and repeated operation. `tools/check_walkman_assets.py` checks pack bounds, decodes every complete track, and compares the MP3 decoder with FFmpeg when available. The complete gate builds and verifies the merged image.

USB diagnostic commands: `?` state, `u/d/o` short buttons, `U/D/O` long buttons, `c` frame capture, `s` save. Physical callbacks and diagnostic commands enter the same input queue. Audio errors remain visible in the player instead of reporting successful playback. Host tests and successful compilation do not establish physical sound quality or battery life.

The fixed-point MP3 decoder is vendored from [Helix](https://github.com/pschatzmann/codec-helix), with original notices and RPSL/RCSL license files in `main/vendor/helix`. Exact upstream blob revisions are recorded in `provenance.json`; the local memory adapter uses the internal heap. Playback uses one decoder and a 6 KiB worker stack; display capture uses 20-line stripes to avoid a full-frame allocation competing with audio memory.

Voice captions use the synthesis service's word timing. Extended silence cuts are mapped onto that timing, and MP3 encoder delay is compensated. The active phrase appears between the previous and next phrases; pausing or changing tracks also pauses or resets captions. Music tracks without a caption file display their title and progress.

Caption reflow keeps short endings with their sentence and retains the original phrase start time. The headphone-cloud illustration has a fixed size for speech and music alike. Caption rows have bounded heights and do not resize the artwork.

All subtitle rows use a fixed 14 px font. Color alone identifies the active line; long lines never shrink the text or resize the illustration. Local song captions are transcribed from the audio, checked against its timing, and stored as millisecond cues. Cue files are JSON arrays of `ms` and `text`, referenced by the playlist entry’s `cues` field. Keep each line within 14 Chinese characters.

Convert a local LRC file with `python3 tools/import_walkman_lrc.py song.lrc song.cues.json`, then add `"cues": "song.cues.json"` to its local playlist entry. LRC offsets and repeated timestamps for repeated lines are supported. Lines exceeding 14 characters fail with an explanation; split them into separately timed phrases. Import regenerates the pack; regenerate fonts and run the complete gate before flashing. Community builds contain the 12 bundled tracks only.
