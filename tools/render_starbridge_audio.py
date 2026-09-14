#!/usr/bin/env python3
"""Render the exact firmware synthesizer to a local WAV for listening (requires cc)."""
import argparse
import ctypes
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
parser.add_argument('--effects', action='store_true', help='Add each UI chime to the preview')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='starbridge-audio-') as temp:
    temp = Path(temp)
    bridge = temp / 'bridge.c'
    bridge.write_text('''#include "starbridge_sound.h"
static sb_synth synth;
void start(void) { sb_synth_init(&synth); sb_synth_volume(&synth, 3, true); }
void render(int16_t *pcm, unsigned samples) { sb_synth_render(&synth, pcm, samples); }
void effect(unsigned kind) { sb_synth_effect(&synth, (sb_sound)kind); }
''')
    library = temp / ('sound.dylib' if sys.platform == 'darwin' else 'sound.so')
    subprocess.run(['cc', '-std=c11', '-O2', '-shared', '-fPIC', '-I' + str(ROOT / 'main'),
                    str(bridge), str(ROOT / 'main/starbridge_sound.c'), '-o', str(library)], check=True)
    synth = ctypes.CDLL(str(library))
    synth.render.argtypes = [ctypes.POINTER(ctypes.c_int16), ctypes.c_uint]
    synth.start()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(args.output), 'wb') as out:
        out.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
        block = (ctypes.c_int16 * 1600)()
        for index in range(200):
            if args.effects and index in (20, 60, 100): synth.effect({20: 2, 60: 3, 100: 7}[index])
            synth.render(block, len(block))
            out.writeframes(bytes(block))
print(args.output)
