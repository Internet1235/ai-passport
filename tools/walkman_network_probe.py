#!/usr/bin/env python3
"""Run a 20-second synthetic PCM upload without reading the microphone or requesting a reply."""
import argparse,json,time
from pathlib import Path
from walkman_online_device import OnlineDevice

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();d=OnlineDevice(args.port)
    try:
        for attempt in range(3):
            try:s=d.state();break
            except TimeoutError:
                if attempt==2:raise
        if 'testing' not in s['net']:raise SystemExit('Flash the firmware with the synthetic diagnostic first')
        baseline_drops=s['net']['dropped'];baseline_failures=s['net'].get('failures',0)
        d.serial.write(b'z');deadline=time.monotonic()+45;observations=[];started=False
        while time.monotonic()<deadline:
            try:s=d.state()
            except TimeoutError:continue
            observations.append(s);n=s['net'];started=started or n['testing'];print(json.dumps(n),flush=True)
            if n['phase']==6 and (started or n.get('failures',0)>baseline_failures):break
            if started and not n['testing'] and n['phase']==2:break
            time.sleep(.5)
        n=s['net'];passed=started and n['phase']==2 and n['captured']==320000 and n['uploaded']==320000 and n['dropped']==baseline_drops and not n['recording']
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps({'passed':passed,'final':s,'observations':observations},indent=2)+'\n')
        if not passed:raise SystemExit('Synthetic upload failed; inspect sanitized diagnostics')
        print('Synthetic upload: PASS; microphone remained unused')
    finally:d.serial.close()
if __name__=='__main__':main()
