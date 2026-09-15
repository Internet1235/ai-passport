#!/usr/bin/env python3
"""Neural pep talks with word-timed captions and explicit silence/time mapping."""
import argparse,asyncio,json,re,subprocess,wave
from pathlib import Path
import edge_tts
ROOT=Path(__file__).resolve().parents[1]
RATE=22050
async def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--voice',default='zh-CN-XiaoyiNeural');p.add_argument('--index',type=int,help='Regenerate one zero-based voice index');args=p.parse_args()
    folder=ROOT/'assets/music/walkman-source';scripts=json.loads((folder/'voice-scripts.json').read_text())
    if args.index is not None and not 0<=args.index<len(scripts):p.error("Invalid voice index")
    for i,entry in enumerate(scripts):
        if args.index is not None and i!=args.index:continue
        text=entry['text'].replace('💗','').replace('～','！').strip();words=[]
        raw=folder/f'voice-{i:02d}-raw.mp3';target=folder/f'voice-{i:02d}.wav'
        with raw.open('wb') as out:
            async for msg in edge_tts.Communicate(text,voice=args.voice,rate='+7%',pitch='+4Hz',boundary='WordBoundary').stream():
                if msg['type']=='audio':out.write(msg['data'])
                elif msg['type']=='WordBoundary':words.append(dict(start=msg['offset']/1e7,end=(msg['offset']+msg['duration'])/1e7,text=msg['text']))
        audio=subprocess.run(['ffmpeg','-v','error','-i',str(raw),'-ar',str(RATE),'-ac','1','-f','s16le','-'],check=True,capture_output=True).stdout
        duration=len(audio)/(2*RATE)
        detection=subprocess.run(['ffmpeg','-hide_banner','-i',str(raw),'-af','silencedetect=noise=-45dB:d=0.35','-f','null','-'],check=True,capture_output=True).stderr.decode()
        start=None;cuts=[]
        for line in detection.splitlines():
            m=re.search(r'silence_start: ([0-9.]+)',line)
            if m:start=float(m[1])
            m=re.search(r'silence_end: ([0-9.]+)',line)
            if m and start is not None:
                end=float(m[1]); a=start+(.04 if start<.02 else .08);b=end-.08
                if b>a:cuts.append((a,b))
                start=None
        if start is not None and duration-start>.35:cuts.append((start+.12,duration))
        def mapped(t):return max(0,t-sum(max(0,min(t,b)-a) for a,b in cuts if t>a))
        pieces=[];cursor=0
        for a,b in cuts:
            left=int(a*RATE)*2;right=int(b*RATE)*2;pieces.append(audio[cursor:left]);cursor=right
        pieces.append(audio[cursor:]);joined=b''.join(pieces)
        with wave.open(str(target),'wb') as w:
            w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE);w.writeframes(joined)
        cues=[];phrase='';cue_start=0;search_at=0
        for word in words:
            if not phrase:cue_start=mapped(word['start'])
            token=word['text'];phrase+=token
            pos=text.find(token,search_at)
            if pos>=0:search_at=pos+len(token)
            following=text[search_at:search_at+1]
            if len(phrase)>=10 or following in '，。！？！\n' or word is words[-1]:
                cues.append(dict(ms=round(cue_start*1000),text=phrase));phrase=''
        if not cues:raise ValueError('No speech timing metadata received')
        (folder/f'voice-{i:02d}.cues.json').write_text(json.dumps(cues,ensure_ascii=False,indent=2)+'\n')
        print(f'Voice {i+1}/{len(scripts)}: {len(cues)} timed lines, {duration-len(joined)/(2*RATE):.2f}s excessive silence removed',flush=True)
if __name__=='__main__':asyncio.run(main())
