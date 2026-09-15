#!/usr/bin/env python3
"""Regenerate the three original instrumental compositions without lossy intermediate files."""
import array,math,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RATE=22050
seqs=[[0,4,7,12,9,7,4,2,0,7,9,12,14,12,9,7],[7,9,12,16,14,12,9,7,4,7,9,12,9,7,4,2],[0,7,12,7,4,9,12,9,2,7,14,12,9,7,4,0]]
for which in range(3):
    seconds=64;beat=[.5,.4,.8][which];melody=seqs[which];out=array.array('h')
    for i in range(seconds*RATE):
        t=i/RATE;k=int(t/beat);age=t-k*beat;note=60+melody[k%len(melody)];f=440*2**((note-69)/12)
        env=min(1,age/.025)*math.exp(-age*4)
        lead=(math.sin(2*math.pi*f*age)+.22*math.sin(4*math.pi*f*age))*env
        bassnote=[48,45,53,55][int(t/(beat*8))%4];bf=440*2**((bassnote-69)/12)
        bass=.32*math.sin(2*math.pi*bf*t)*math.sin(math.pi*(t%(beat*8))/(beat*8))**2
        pad=.13*(math.sin(2*math.pi*bf*2*t)+math.sin(2*math.pi*bf*3*t))
        fade=min(1,t/1.5,(seconds-t)/2)
        out.append(int(11000*(lead+bass+pad)*max(0,fade)))
    path=ROOT/f'assets/music/walkman-source/music-{which}.wav'
    with wave.open(str(path),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(out.tobytes())
    print(path.name,flush=True)
