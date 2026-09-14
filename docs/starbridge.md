English | [简体中文](starbridge.zh_CN.md)

# Starbridge: Light the Network

Rotate tracks to power every star from the gold source. Thirty deterministic,
solvable puzzles progress from 3-by-3 (levels 1–10) to 4-by-4 (11–30).
The application is entirely offline, with a gentle original music-box loop and
short chimes for selection, rotation, connection, hints, undo, confirmation and victory.

| Input | Action |
| --- | --- |
| UP / DOWN click | Previous / next cell, wrapping in row-major order |
| OK click | Rotate clockwise |
| UP hold | Align the first incorrect cell; hints limit this run to one star |
| DOWN hold | Undo the last rotation, up to 128 rotations in RAM |
| OK hold | Pause: continue, restart, level selection, instructions, sound |

Gold outlines mark selection, and green tracks have power. All cells must
connect to the source. A run earns three stars without hints within the target
rotation count, two without hints above it, and one with hints. Best stars never
decrease. Undo does not refund moves. The target is the cost of restoring the
known generated solution, not a claimed globally optimal solution.

Rotations save after one idle second; pause and victory save immediately. Wait
until the saving message disappears before powering off. Failures display a
warning and retry. Level, tracks, moves, hints, stars and unlocks persist;
cursor-only moves do not write Flash. Undo history does not survive reboot.

## Sound

The pause menu cycles through 0%, 25%, 50%, 75% (default) and 100%. This preference
survives reboot. A 20-second original melody loops at 96 BPM, with arpeggios and
soft bass. Music lowers in menus and under effects; gains fade to prevent clicks.
The firmware synthesizes 16 kHz, 16-bit mono PCM in 256-sample chunks using six
integer oscillators. A dedicated priority-6 worker feeds the official ES8311 BSP;
LVGL uses priority 4. I2S IRAM-safe interrupts remain active during NVS writes.
USB diagnostics use an 8 KB transmit ring and yield between screenshot rows;
the host retains partial lines across read timeouts.
No third-party recording, streamed music, microphone capture or large audio
buffer is used. Source composition and synthesis are in `main/starbridge_sound.c`.

Preview the same synthesizer with:

```bash
python3 tools/render_starbridge_audio.py delivery/audio/preview.wav --effects
```

USB state reports initialization, write failures, PCM block counts, maximum feed
gap, render duration and chime counts. These diagnostics do not certify speaker
tone or absence of audible glitches; listen while navigating and saving.

## Build

Activate ESP-IDF 5.5.3 and run `./tools/validate.sh`. The verified output is
`build/FoloToy-AI-Passport-full.bin`, intended for offset zero on ESP32-C3 with
8 MB Flash. The app uses the official display/button/battery BSP, a 48 KB LVGL
pool and an 8 KB main stack. Regenerate the OFL WenKai-derived Starbridge bitmap subsets after UI
changes; see [font provenance and commands](../assets/README.md#fonts).
The 12/16 px sizes are retained with 4-bit antialiasing.

The default factory layout is retained. Full merged files pad the NVS gap;
development flashing can preserve NVS by writing only bootloader, partition
table and application segments. Device backups and identities must never enter
public artifacts. The app writes only the `starbridge` namespace and does not
erase NVS on initialization errors.

## Verification

Host tests cover all 30 solutions, rotations, cursor wrap, scoring, hints,
bounded undo, round-trip saves and corruption at every byte. Audio tests cover
two minutes of mixed music/effects, headroom, DC offset, mute and chunk invariance. USB tests exercise
the actual firmware through the same dispatch as physical keys. They do not
certify physical switch feel, panel optics or battery life.

At 115200 baud, send a character followed by newline. `?` reports JSON state;
`u/d/o` inject short UP/DOWN/OK; `U/D/O` inject holds; `s` saves; `c` captures
RGB565 pixels from real LVGL display flushes; `m` cycles sound levels. `b` persistently checkpoints the
player's progress and opens a fresh test journey; `e` restores it, even after
reboot. An existing checkpoint prevents overwriting and a failed restore keeps
the checkpoint. Physical events are recorded separately. Captures are software
frames sent to the panel, not camera photographs. See
`tools/starbridge_device.py --help` for automation.

## Community comparison

On 2026-09-14 the community API returned 335 plays. No dedicated rotating-track
network puzzle was found in titles or descriptions. Minesweeper, Sokoban,
Lights Out and Hanoi already occur inside Pocket Arcade. This only establishes
absence from published descriptions, not knowledge of unpublished work.
Community submission awaits the creator's final explicit approval.
