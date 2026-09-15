English | [简体中文](README.zh_CN.md)

# Pocket Sunshine

An offline pocket player for FoloToy AI Passport: nine lively Mandarin pep talks and three original instrumental tracks, a soft cloud-themed screen, and local music import. The public edition uses generic greetings and includes no commercial recordings. Add your own local music and timed captions with the import tools. Speech uses Microsoft Xiaoyi neural synthesis, with extended pauses shortened.

| Input | Action |
| --- | --- |
| UP / DOWN | Previous / next track |
| OK | Play / pause |
| Hold UP / DOWN | Increase / decrease volume |
| Hold OK | Open settings / return to player |

Settings include five volume levels (0–100%), playlist repeat / track repeat / play-through, and a 15/30/60-minute playback timer. Track selection, volume and playback mode persist. Startup is paused, with the selected track at the beginning. The timer pauses audio; it does not power off the device.

Read the [player guide](docs/walkman.md) for importing local MP3/M4A/WAV files, regenerating Chinese fonts, and testing. Use `--append` when importing a song to retain the pep talks. The Chinese UI uses LXGW WenKai Screen under the SIL OFL; asset provenance is in the [asset index](assets/README.md).

```bash
# Activate ESP-IDF 5.5.3 first.
./tools/validate.sh
# Output: build/FoloToy-AI-Passport-full.bin
```

Hardware: ESP32-C3, 8 MB flash, no PSRAM, 240-by-320 display, three physical buttons and ES8311 audio. Streaming MP3 keeps memory use independent of song length. Firmware remains offline and requires no account or microphone.

Development branch: `feature/energy-walkman`. The previous Starbridge game is documented in [its guide](docs/starbridge.md) and preserved on `feature/starbridge`.

See [development guidance](AGENTS.md), [hardware guide](docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md), and [documentation index](docs/README.md). Compilation, host tests, physical sound quality and battery life are separate validation results.
