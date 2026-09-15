#!/usr/bin/env python3
"""Read paired player/network diagnostics without printing credentials or transcripts."""
import json,time
from walkman_device import Device
class OnlineDevice(Device):
    def state(self):
        self.serial.write(b'?');deadline=time.monotonic()+5;player=None
        while True:
            line=self.line(deadline)
            if line.startswith(('WM_STATE ','WN_STATE ')):
                try:payload=json.loads(line[9:])
                except json.JSONDecodeError:
                    # Driver logs may interleave with a diagnostic printf.
                    # Request a fresh pair instead of accepting a partial state.
                    player=None;self.serial.write(b'?');continue
                if line.startswith('WM_STATE '):player=payload
                elif player is not None:player['net']=payload;return player
            elif any(x in line for x in ['Guru Meditation','assert failed','Task watchdog']):raise RuntimeError('Device crashed or watchdog fired')
    def await_phase(self,phases,seconds=35):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            s=self.state()
            if s['net']['phase'] in phases:return s
            time.sleep(.2)
        raise TimeoutError('Network phase deadline exceeded')
