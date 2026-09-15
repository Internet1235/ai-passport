#!/usr/bin/env python3
"""Exercise the connected Walkman firmware through the same queue as hardware keys."""
import argparse, json, time
from pathlib import Path
import serial
class Device:
    def __init__(self,port):
        self.serial=serial.Serial(port,115200,timeout=.2); self.buffer=b''
    def line(self,deadline):
        while time.monotonic()<deadline:
            if b'\n' in self.buffer:
                raw,self.buffer=self.buffer.split(b'\n',1); return raw.decode(errors='replace').strip()
            self.buffer+=self.serial.read(4096)
        raise TimeoutError('No complete response')
    def state(self):
        self.serial.write(b'?'); deadline=time.monotonic()+5
        while True:
            line=self.line(deadline)
            if line.startswith('WM_STATE '): return json.loads(line[9:])
            if any(x in line for x in ['Guru Meditation','assert failed','Task watchdog']): raise RuntimeError(line)
    def key(self,key):
        self.serial.write(key.encode()); time.sleep(.3); return self.state()
    def capture(self,path):
        for attempt in range(3):
            try: return self._capture_once(path)
            except (AssertionError,ValueError,TimeoutError):
                if attempt==2: raise
                self.buffer=b'';self.serial.reset_input_buffer();self.state()
    def _capture_once(self,path):
        self.serial.write(b'c'); rows={}; deadline=time.monotonic()+15
        while True:
            line=self.line(deadline)
            if line.startswith('WM_ROW '):
                parts=line.split()
                if len(parts)==3 and parts[1].isdigit() and len(parts[2])==960:
                    rows[int(parts[1])]=parts[2]
            if 'WM_FRAME_END' in line: break
            if line.startswith('WM_CAPTURE_ERROR'): raise RuntimeError(line)
        assert len(rows)==320 and all(len(row)==960 for row in rows.values())
        from PIL import Image
        pixels=[]
        for y in range(320):
            for x in range(240):
                p=int(rows[y][x*4:x*4+4],16)
                pixels.append((((p>>11)&31)*255//31,((p>>5)&63)*255//63,(p&31)*255//31))
        im=Image.new('RGB',(240,320)); im.putdata(pixels); im.save(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--port',required=True); parser.add_argument('--output',type=Path,default=Path('delivery/device')); args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    d=Device(args.port); time.sleep(1); s=d.state(); assert s['ready'] and not s['failed']; initial=s.copy(); observations=[s]
    try:
        if s['page']: s=d.key('O')
        if s['playing']: s=d.key('o')
        for _ in range(s['track']): s=d.key('u')
        assert s['track']==0 and not s['playing']
        d.capture(args.output/'01-player.png')
        s=d.key('o'); assert s['playing'];time.sleep(1)
        s=d.state(); assert s['position']>0
        pos=s['position'];s=d.key('o');time.sleep(.2);paused=d.state(); assert not paused['playing'] and paused['position']>=pos
        time.sleep(.3);assert d.state()['position']==paused['position']
        s=d.key('u');assert s['track']==s['count']-1
        s=d.key('d');assert s['track']==0
        s=d.key('o')
        for _ in range(s['count']):
            s=d.key('d');time.sleep(.25);s=d.state();assert s['playing'] and s['ready'] and not s['failed'];observations.append(s)
        assert s['track']==0
        for _ in range(4):s=d.key('D')
        time.sleep(.2);s=d.state();assert s['volume']==0 and s['peak']==0
        for v in range(1,5):s=d.key('U');assert s['volume']==v
        assert d.key('U')['volume']==4
        s=d.key('O');assert s['page']==1
        d.capture(args.output/'02-settings.png')
        s=d.key('d');assert s['selected']==1
        for _ in range(3):
            before=s['repeat'];s=d.key('o');assert s['repeat']==(before+1)%3
        s=d.key('d');assert s['selected']==2
        for expected in [15,30,60,0]:s=d.key('o');assert s['timer']==expected
        s=d.key('d');s=d.key('o');assert s['page']==2
        d.capture(args.output/'03-help.png')
        s=d.key('O');assert s['page']==0
        for _ in range(20): d.key('O');s=d.key('O')
        assert s['ready'] and not s['failed'] and s['dropped']==0
        if s['playing']:s=d.key('o')
        while s['volume']>3:s=d.key('D')
        while s['volume']<3:s=d.key('U')
        while s['track']!=0:s=d.key('u')
        time.sleep(1.7);d.serial.write(b's');time.sleep(.2);s=d.state();assert s['save_ok']
        observations.append(s)
        (args.output/'smoke.json').write_text(json.dumps({'passed':True,'initial':initial,'final':s,'observations':observations},indent=2)+'\n')
        print(json.dumps({'passed':True,'final':s}))
    finally: d.serial.close()
if __name__=='__main__': main()
