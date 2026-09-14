#!/usr/bin/env python3
"""Local USB verification and real display-flush capture for Starbridge.

Requires pyserial. Tests temporarily change this game's progress, checkpoint it
on the device, and restore it in a finally block. A disconnected test can be
recovered with --command e. Screenshots are native 240x320 RGB565 panel frames.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import zlib

import serial


def png(path, rows, width=240, height=320):
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        values = rows[y]
        assert len(values) == width * 4
        for x in range(width):
            pixel = int(values[x * 4:x * 4 + 4], 16)
            r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
            scanlines.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(bytes(scanlines), 9)) + chunk(b'IEND', b'')
    path.write_bytes(data)


class Device:
    def __init__(self, port, output):
        self.port = port
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        self.log = (output / 'device-transcript.log').open('a')
        self.open()

    def open(self):
        self.pending = bytearray()
        self.serial = serial.Serial(baudrate=115200, timeout=0.2)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = self.port
        self.serial.open()
        # macOS may reset USB Serial/JTAG on open. Drain unsolicited boot state
        # before sending a command so an old READY frame cannot be its reply.
        until = time.monotonic() + 2
        while time.monotonic() < until:
            self.line()

    def line(self):
        # USB delivery may pause in the middle of a JSON state or a framebuffer row.
        # A serial timeout is not a newline: retain the fragment for the next read.
        self.pending.extend(self.serial.read_until(b'\n'))
        if not self.pending.endswith(b'\n'): return ''
        raw = self.pending.decode('ascii', errors='replace').strip()
        self.pending.clear()
        # Store only this application's non-private diagnostics, not arbitrary firmware logs.
        if 'SB_' in raw or 'starbridge:' in raw:
            if not raw.startswith('SB_ROW'):
                self.log.write(raw + '\n'); self.log.flush()
        if any(word in raw for word in ('Guru Meditation', 'assert failed', 'Stack canary', 'watchdog got triggered')):
            raise RuntimeError(raw)
        return raw

    def command(self, text, timeout=12):
        self.serial.write((text + '\n').encode())
        retries = 0
        error = None
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            line = self.line()
            if 'SB_TEST_ERROR' in line or 'SB_CAPTURE_ERROR' in line:
                error = line
            if 'SB_STATE ' in line:
                if error: raise RuntimeError(error)
                try:
                    return json.loads(line.split('SB_STATE ', 1)[1])
                except json.JSONDecodeError:
                    # The polling/debug console can drop bytes under backpressure.
                    # Re-query state only; never repeat a possibly executed action.
                    retries += 1
                    if retries >= 3: raise
                    self.serial.write(b'?\n')
        raise TimeoutError(f'No state after {text!r}')

    def capture(self, name):
        for attempt in range(3):
            try:
                return self._capture(name)
            except AssertionError:
                if attempt == 2: raise

    def _capture(self, name):
        self.serial.write(b'c\n')
        rows = {}; started = False; malformed = None; until = time.monotonic() + 40
        while time.monotonic() < until:
            line = self.line()
            if line.startswith('SB_FRAME_BEGIN'):
                fields = line.split(); assert fields[1:3] == ['240', '320']; started = True
                assert int(fields[3]) >= 320, f'Incomplete flush: {line}'
            elif line.startswith('SB_ROW '):
                fields = line.split()
                if len(fields) != 3 or len(fields[2]) != 960:
                    malformed = 'Malformed frame row: ' + repr(line[:100])
                else:
                    _, y, pixels = fields; rows[int(y)] = pixels
            elif line == 'SB_FRAME_END':
                # Consume the state emitted after capture before the next command.
                until_state = time.monotonic() + 5
                while time.monotonic() < until_state:
                    if 'SB_STATE ' in self.line():
                        assert started and len(rows) == 320, malformed or f'Incomplete frame: {len(rows)} rows'
                        png(self.output / name, rows)
                        return
                raise TimeoutError('No post-capture state')
            elif 'SB_STATE ' in line and started:
                raise AssertionError('Frame end was lost; complete state consumed before retry')
            elif 'SB_CAPTURE_ERROR' in line:
                raise RuntimeError(line)
        raise TimeoutError('Incomplete frame capture')

    def reboot(self):
        self.serial.close()
        result = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32c3',
                                 '--port', self.port, '--after', 'hard_reset', 'chip_id'],
                                capture_output=True, text=True, timeout=20)
        if result.returncode: raise RuntimeError('Device reset failed: ' + result.stderr)
        self.open()
        return self.command('?')

    def close(self):
        self.serial.close(); self.log.close()


def test(device):
    checks = []
    original = device.command('?')
    assert original['audio']['ready'] and not original['audio']['failed'], original
    state = device.command('b')
    try:
        assert state['mode'] == 0 and state['level'] == 0 and state['moves'] == 0
        while device.command('?')['sound'] != 3: device.command('m')
        device.capture('home.png')
        assert device.command('o')['mode'] == 4
        device.capture('help.png')
        state = device.command('o'); assert state['mode'] == 1
        device.capture('level-01.png')
        initial = state['tiles']
        assert device.command('u')['cursor'] == 8
        assert device.command('d')['cursor'] == 0
        rotated = device.command('o'); assert rotated['moves'] == 1 and rotated['tiles'] != initial
        undone = device.command('D'); assert undone['tiles'] == initial and undone['moves'] == 1
        checks.append('three-key dispatch, wrap, rotate, undo')
        device.command('d'); state = device.command('o')
        assert device.command('m')['sound'] == 4
        saved = device.command('s'); assert saved['save_ok'] and not saved['dirty']
        restarted = device.reboot()
        for field in ('level', 'moves', 'tiles', 'hints', 'unlocked', 'sound'):
            assert restarted[field] == saved[field], (field, restarted, saved)
        checks.append('NVS save and hardware-reset recovery')
        device.command('m'); time.sleep(0.2)
        muted = device.command('?')
        assert muted['sound'] == 0 and muted['audio']['volume'] == 0 and muted['audio']['peak'] == 0
        device.command('m'); time.sleep(0.2)
        audible = device.command('?')
        assert audible['sound'] == 1 and audible['audio']['peak'] > 0
        device.command('m'); device.command('m')
        checks.append('sound preference survives reboot; mute produces zero PCM; unmute resumes music')
        assert device.command('o')['mode'] == 1
        pause = device.command('O'); assert pause['mode'] == 2
        device.capture('pause.png')
        device.command('u')
        for volume in (4, 0, 1, 2, 3): assert device.command('o')['sound'] == volume
        device.command('d')
        assert device.command('o')['mode'] == 1
        checks.append('pause and resume')
        for level in range(30):
            state = device.command('?'); assert state['level'] == level
            if level == 10: device.capture('level-11.png')
            for attempt in range(17):
                if state['won']: break
                state = device.command('U')
            assert state['won'] and state['mode'] == 5, state
            assert state['powered'] == (9 if level < 10 else 16)
            assert state['unlocked'] == min(level + 1, 29)
            assert state['save_ok'] and not state['dirty']
            assert state['audio']['ready'] and not state['audio']['failed'], state
            if level in (0, 10, 29): device.capture(f'win-{level+1:02d}.png')
            state = device.command('o')
            print(f'Device level {level+1:02d}/30 PASS', flush=True)
        assert state['mode'] == 3 and state['stars'] == 30
        checks.append('30 on-device victories, hints, scoring, unlocks and final level selection')
        device.capture('all-levels.png')
        device.command('o')
        for _ in range(20):
            assert device.command('O')['mode'] == 2
            assert device.command('o')['mode'] == 1
            device.command('d'); device.command('u')
        final = device.command('?')
        assert original['heap'] - final['heap'] < 16000, (original['heap'], final['heap'])
        checks.append('20 pause/resume cycles and bounded heap use')
        assert final['audio']['late_feeds'] == 0 and final['audio']['dropped'] == 0, final['audio']
        assert final['audio']['max_render_us'] < 16000, final['audio']
        checks.append('continuous BGM with UI, captures and NVS saves; no write errors or feed gaps above 60 ms')
        report = {'automated_device_tests': 'PASS', 'checks': checks,
                  'initial_heap': original['heap'], 'final_heap': final['heap'],
                  'minimum_heap_including_capture': final['min_heap'],
                  'audio': final['audio'],
                  'physical_switches': 'Pending separate physical observation',
                  'panel_optics_and_battery_life': 'Not certified by USB automation'}
        (device.output / 'device-test-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2))
        return report
    finally:
        restored = device.command('e')
        for field in ('level', 'moves', 'tiles', 'hints', 'stars', 'unlocked', 'sound'):
            assert restored[field] == original[field], f'Checkpoint restore mismatch: {field}'
        device.capture('ready.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, default=Path('delivery/device'))
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--test', action='store_true')
    action.add_argument('--command')
    action.add_argument('--capture', metavar='NAME.png')
    action.add_argument('--listen', type=int, metavar='SECONDS')
    args = parser.parse_args()
    device = Device(args.port, args.output)
    try:
        if args.test: print(json.dumps(test(device), indent=2))
        elif args.capture: device.capture(args.capture); print(args.output / args.capture)
        elif args.command: print(json.dumps(device.command(args.command), indent=2))
        else:
            until = time.monotonic() + args.listen
            while time.monotonic() < until:
                line = device.line()
                if line and ('SB_STATE' in line or 'PHYSICAL' in line): print(line, flush=True)
    finally: device.close()


if __name__ == '__main__': main()
