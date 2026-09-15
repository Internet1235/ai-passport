English | [简体中文](README.zh_CN.md)

# Pocket Sunshine Online

A pocket voice companion and offline music player. Speak about your day and hear a warm, playful reply, request a surprise pep talk, or take a break with the bundled local audio. The device connects directly to Qwen over Wi-Fi and works without a running computer.

The independent offline release remains in `feature/energy-walkman`. This version is developed in `feature/energy-walkman-online`.

- Home: Up/Down selects a choice; OK opens it.
- Conversation: OK starts recording, then OK sends it. Recording ends automatically after 20 seconds. During a reply, OK cancels it. Up/Down scrolls the reply.
- Local player: Up/Down changes track; OK plays or pauses.
- Hold Up/Down to adjust volume. Hold OK to return home; holding OK on home opens the local player.

All captions use 14 px type. The title is centered on the full screen, with battery status independently positioned at the top right. Twelve bundled audio tracks remain available offline.

Open **Network settings** to create a temporary device hotspot. Connect a phone, visit `192.168.4.1`, and select a nearby 2.4 GHz Wi-Fi network from the automatically scanned list, then enter its password. Use Rescan to refresh the list; hidden networks have a manual entry option. First-time setup requires your own Qwen API Key; model service charges may apply. When a default Qwen key has been provisioned over USB, leave the key field blank to reuse it or enter a new key to replace it. Configuration is saved in the device's separate `wm_online` namespace. The key is absent from source code and distributable firmware. The microphone opens only after starting a recording; audio is sent directly to the configured Qwen service. Conversation context lasts for the voice session and is cleared when leaving it or when the connection is released after roughly 45 idle seconds. This application does not write transcripts or recordings to local storage.

Alternatively, use the private [USB configuration tool](tools/configure_walkman_online.py). See the [guide](docs/walkman.md) for setup, audio import and validation. Activate ESP-IDF 5.5.3 and run `./tools/validate.sh` to build and verify `build/FoloToy-AI-Passport-full.bin`. Build, host tests and device tests must be reported separately.
