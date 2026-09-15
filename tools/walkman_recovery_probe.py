#!/usr/bin/env python3
"""Inject a socket failure during synthetic upload and verify one-request recovery; no microphone or reply."""
import argparse
import json
import time
from pathlib import Path
from walkman_online_device import OnlineDevice

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    device = OnlineDevice(args.port)
    states = []
    report = {'passed': False, 'microphone_used': False, 'observations': states}

    def wait_for(predicate, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                state = device.state()
            except TimeoutError:
                continue
            states.append(state)
            if state['net']['recording']:
                raise RuntimeError('Unexpected real microphone state; stop the probe')
            if predicate(state):
                print(json.dumps(state['net']), flush=True)
                return state
            time.sleep(.3)
        raise TimeoutError('Recovery probe deadline exceeded')

    try:
        initial = wait_for(lambda s: True, 15)
        assert 'error_code' in initial['net'], 'Flash recovery diagnostics first'
        failures = initial['net']['failures']
        device.serial.write(b'z')
        wait_for(lambda s: s['net']['testing'] and s['net']['uploaded'] >= 32000, 20)
        device.serial.write(b'x')
        report['fault_injected'] = True
        broken = wait_for(lambda s: s['net']['phase'] == 6 and not s['net']['connected'] and s['heap'] > 60000, 12)
        # A blocked send can fill the capture queue before the socket timeout
        # arrives; the device deliberately retains that first failure cause.
        assert broken['net']['error_code'] in (4, 5, 8)
        assert broken['net']['failures'] == failures + 1
        # One request must replace the stale socket, with no 30-second dead wait.
        started = time.monotonic()
        device.serial.write(b'z')
        wait_for(lambda s: s['net']['testing'] and s['net']['uploaded'] >= 1600, 12)
        report['recovery_seconds'] = round(time.monotonic() - started, 2)
        final = wait_for(lambda s: not s['net']['testing'] and s['net']['phase'] in (2, 6), 30)
        n = final['net']
        assert n['phase'] == 2 and n['error_code'] == 0
        assert n['captured'] == 320000 and n['uploaded'] == 320000
        assert n['dropped'] == broken['net']['dropped'], 'Recovery introduced another dropped packet'
        assert n['failures'] == failures + 1, 'Intentional close was misreported as an error'
        report['passed'] = True
        print('Socket failure, resource cleanup, one-request recovery, normal close: PASS', flush=True)
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        device.serial.close()

if __name__ == '__main__':
    main()
