#!/usr/bin/env python3
"""Convert a local, line-timed LRC file to Walkman caption JSON."""
import argparse
import json
import re
from pathlib import Path


def parse_lrc(text):
    offset = re.findall(r'\[offset:([+-]?\d+)\]', text, re.I)
    offset = int(offset[-1]) if offset else 0
    stamps = re.compile(r'\[(\d+):(\d{2})(?:[.:](\d{1,3}))?\]')
    cues = []
    for line in text.splitlines():
        matches = list(stamps.finditer(line))
        if not matches:
            continue
        words = stamps.sub('', line).strip() or '间奏 · 静静听'
        if len(words) > 50:
            raise ValueError('Each timed line must fit 50 characters; split long lines with their own timestamps')
        for match in matches:
            minute, second, fraction = match.groups()
            if int(second) >= 60:
                raise ValueError('LRC seconds must be below 60')
            ms = (int(minute) * 60 + int(second)) * 1000 + int((fraction or '').ljust(3, '0')) + offset
            cues.append(dict(ms=max(0, ms), text=words))
    cues.sort(key=lambda c: c['ms'])
    if not cues or any(a['ms'] == b['ms'] for a, b in zip(cues, cues[1:])):
        raise ValueError('Need at least one cue and distinct timestamps')
    if cues[0]['ms'] > 0:
        cues.insert(0, dict(ms=0, text='前奏 · 轻轻听'))
    return cues


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(parse_lrc(args.input.read_text(encoding='utf-8-sig')), ensure_ascii=False, indent=2) + '\n')
