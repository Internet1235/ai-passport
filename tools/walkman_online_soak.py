#!/usr/bin/env python3
"""Exercise six audible quick replies and three synthetic uploads; never opens the microphone."""
import argparse
import json
import time
from pathlib import Path
from walkman_online_device import OnlineDevice

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();d=OnlineDevice(args.port)
    report={'passed':False,'microphone_used':False,'rounds':[],'observations':[]}
    def read():
        s=d.state();report['observations'].append(s)
        assert not s['net']['recording'],'Unexpected real microphone state'
        return s
    def wait(predicate,seconds):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            try:s=read()
            except TimeoutError:continue
            if s['net']['phase']==6:raise RuntimeError('Device failure code '+str(s['net']['error_code']))
            if predicate(s):return s
            time.sleep(.3)
        raise TimeoutError('Online soak deadline exceeded')
    try:
        # Move to home through the regular button path, then choose a quick
        # reply explicitly. Never press OK on the idle conversation screen.
        for attempt in range(3):
            try:s=read();break
            except TimeoutError:
                if attempt==2:raise
        initial=s['net'];baseline=initial['failures']
        for round_index in range(6):
            if s['page']!=3:s=d.key('O')
            target=1+round_index%2
            while s['selected']!=target:s=d.key('d')
            played=s['net']['played'];drops=s['net']['dropped'];starves=s['net']['starves']
            d.serial.write(b'o')
            s=wait(lambda s:s['net']['phase']==2 and s['net']['played']>played,40)
            n=s['net'];assert n['failures']==baseline and n['dropped']==drops and n['starves']==starves
            result={'round':round_index+1,'reply_samples':n['played']-played,'heap':s['heap'],'min_heap':s['min_heap']}
            if round_index%2==1:
                # Recording memory is exercised after a reply on the same
                # connection, using generated silence instead of microphone I/O.
                d.serial.write(b'z')
                wait(lambda s:s['net']['testing'],12)
                s=wait(lambda s:not s['net']['testing'] and s['net']['phase']==2,30)
                n=s['net'];assert n['captured']==320000 and n['uploaded']==320000
                assert n['failures']==baseline and n['dropped']==drops
                result['upload_samples']=n['uploaded'];result['max_upload_ms']=n['upload_ms']
            report['rounds'].append(result);print(json.dumps(result),flush=True)
        if s['page']!=3:s=d.key('O')
        while s['selected']!=0:s=d.key('u')
        report['final']=s;report['passed']=True
        print('Six quick replies and three same-connection uploads: PASS',flush=True)
    finally:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n');d.serial.close()

if __name__=='__main__':main()
