#!/usr/bin/env python3
"""Validate every MP3 frame and compare the device decoder with FFmpeg when available."""
import json,subprocess,tempfile,shutil,array
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
pack=(ROOT/'assets/music/walkman/audio.bin').read_bytes()
tracks=json.loads((ROOT/'assets/music/walkman/playlist.json').read_text())
cues=json.loads((ROOT/'assets/music/walkman/captions.json').read_text())
for t in tracks:
    subset=cues[t['cue_start']:t['cue_start']+t['cue_count']]
    assert len(subset)==t['cue_count']
    assert all(0<=c['ms']<t['samples']*1000//22050 and 0<len(c['text'])<=14 for c in subset)
    assert all(a['ms']<b['ms'] for a,b in zip(subset,subset[1:]))
assert 0<len(tracks)<=24 and len(pack)<=6*1024*1024
assert sum(t['bytes'] for t in tracks)==len(pack)
ranges=sorted((t['offset'],t['offset']+t['bytes']) for t in tracks)
assert ranges[0][0]==0 and ranges[-1][1]==len(pack)
assert all(a[1]==b[0] for a,b in zip(ranges,ranges[1:]))
with tempfile.TemporaryDirectory() as temp:
    executable=Path(temp)/'render'
    subprocess.run(['cc','-O2','-DWM_HOST_PACK_POINTER','-Imain','tests/render_walkman.c','main/walkman_decoder.c','main/walkman_tracks.c',*[str(f) for f in (ROOT/'main/vendor/helix').glob('*.c')],'-o',str(executable)],cwd=ROOT,check=True)
    subprocess.run([str(executable),str(ROOT/'assets/music/walkman/audio.bin'),temp],check=True)
    for i,t in enumerate(tracks):
        assert 0<t['samples']<22050*480
        pcm=(Path(temp)/f'track-{i:02d}.raw').read_bytes();assert len(pcm)==t['samples']*2
        if shutil.which('ffmpeg'):
            path=Path(temp)/'track.mp3';path.write_bytes(pack[t['offset']:t['offset']+t['bytes']])
            ref=subprocess.run(['ffmpeg','-v','error','-i',str(path),'-f','s16le','-'],capture_output=True,check=True).stdout
            assert len(ref)==len(pcm)
            a=array.array('h');a.frombytes(pcm);b=array.array('h');b.frombytes(ref)
            maximum=max(abs(x-y) for x,y in zip(a,b))
            assert maximum<=4,(i,maximum)
    print(f'Walkman MP3: {len(tracks)} complete tracks, {len(pack)} bytes, bounds and decoder PASS')
