#!/usr/bin/env python3
"""Build a bounded MP3 playlist from local audio; decode with FFmpeg to measure sample counts."""
import argparse, json, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RATE=22050

def reflow_captions(cues):
    """Keep brief sentence endings on their phrase without changing audio timing."""
    result=[]
    for cue in cues:
        cue=dict(cue)
        if result and len(cue['text'])<=2 and cue['text'] not in ('哇','嘿','叮咚') and len(result[-1]['text'])+len(cue['text'])<=14:
            result[-1]['text']+=cue['text']
        else: result.append(cue)
    i=0
    while i+1<len(result):
        if len(result[i]['text'])<=2 and len(result[i]['text'])+len(result[i+1]['text'])<=14:
            result[i]['text']+=result[i+1]['text'];result.pop(i+1)
        else:i+=1
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--playlist',type=Path,default=ROOT/'assets/music/walkman-source/playlist.json')
    p.add_argument('--append',action='store_true'); args=p.parse_args()
    target=ROOT/'assets/music/walkman'; target.mkdir(parents=True,exist_ok=True)
    tracks=[];pack=bytearray();cues=[]
    if args.append:
        tracks=json.loads((target/'playlist.json').read_text());pack=bytearray((target/'audio.bin').read_bytes())
        cues=json.loads((target/'captions.json').read_text())
        if any('bytes' not in t for t in tracks): raise ValueError('Cannot append MP3 to an older ADPCM pack')
    entries=json.loads(args.playlist.read_text())
    if not isinstance(entries,list) or not entries: raise ValueError('Playlist must be a nonempty array')
    with tempfile.TemporaryDirectory() as tmp:
        for e in entries:
            title=e['title'];caption=e.get('caption','我的音乐')
            if not title or len(title)>20 or len(caption)>160: raise ValueError('Title limit 20; caption limit 160 characters')
            if len(tracks)>=24: raise ValueError('Maximum 24 tracks')
            src=(args.playlist.parent/e['file']).resolve();out=Path(tmp)/'track.mp3'
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(src),'-ar', '22050','-c:a', 'libmp3lame','-b:a', '32k','-map_metadata','-1',str(out)],check=True)
            data=out.read_bytes()
            decoded=subprocess.run(['ffmpeg','-v','error','-i',str(out),'-f','s16le','-acodec','pcm_s16le','-'],capture_output=True,check=True).stdout
            samples=len(decoded)//2
            if samples<=0 or samples>=480*RATE: raise ValueError('Track must be nonempty and shorter than eight minutes')
            new_cues=reflow_captions(json.loads((args.playlist.parent/e['cues']).read_text())) if e.get('cues') else []
            start=len(cues)
            for cue in new_cues:
                if not cue['text'] or len(cue['text'])>100: raise ValueError('Subtitle line too long')
                cues.append(dict(ms=cue['ms']+50,text=cue['text']))
            tracks.append(dict(cue_start=start,cue_count=len(new_cues),title=title,caption=caption,offset=len(pack),samples=samples,bytes=len(data),source=e.get('source','User supplied local audio; see local playlist'),codec='MP3 96 kbps mono 22050 Hz'))
            pack.extend(data)
            if len(pack)>6*1024*1024: raise ValueError('Playlist exceeds 6 MiB; use fewer or shorter tracks')
    lines=['#include "walkman.h"','const wm_cue wm_cues[] = {']
    for cue in cues: lines.append('    {%du, %s},'%(cue['ms'],json.dumps(cue['text'],ensure_ascii=False)))
    if not cues: lines.append('    {0u, ""},')
    lines+=['};','const wm_track wm_tracks[] = {']
    for t in tracks:
        lines.append('    {%s, %s, %du, %du, %du, %du, %du},'%(json.dumps(t['title'],ensure_ascii=False),json.dumps(t['caption'],ensure_ascii=False),t['offset'],t['samples'],t['bytes'],t['cue_start'],t['cue_count']))
    lines+=['};','const unsigned wm_track_count = sizeof(wm_tracks)/sizeof(wm_tracks[0]);']
    (target/'playlist.json').write_text(json.dumps(tracks,ensure_ascii=False,indent=2)+'\n')
    (target/'captions.json').write_text(json.dumps(cues,ensure_ascii=False,indent=2)+'\n')
    (target/'audio.bin').write_bytes(pack)
    (ROOT/'main/walkman_tracks.c').write_text('\n'.join(lines)+'\n')
    print(f'{len(tracks)} MP3 tracks, {len(pack)} bytes, {sum(t["samples"] for t in tracks)/RATE:.1f} seconds')
if __name__=='__main__':main()
