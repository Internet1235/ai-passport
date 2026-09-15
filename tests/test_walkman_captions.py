#!/usr/bin/env python3
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from generate_walkman_assets import reflow_captions
cues=[dict(ms=100,text='要继续元气满满地加油'),dict(ms=1200,text='哦'),dict(ms=1500,text='你真的已经做得超级超级棒'),dict(ms=2400,text='啦')]
result=reflow_captions(cues)
assert len(result)==2 and result[0]['text'].endswith('哦') and result[1]['text'].endswith('啦')
assert [x['ms'] for x in result]==[100,1500]
assert cues[0]['text']=='要继续元气满满地加油'
assert all(len(c['text'])<=16 for c in result)
assert reflow_captions([dict(ms=0,text='哇'),dict(ms=200,text='你也太棒了吧')])==[dict(ms=0,text='哇你也太棒了吧')]
assert reflow_captions([])==[]
print('Captions: orphan endings merged, timing and input preserved PASS')

from import_walkman_lrc import parse_lrc
assert parse_lrc('[offset:-100]\n[00:01.20][00:02.200]你好')==[dict(ms=0,text='前奏 · 轻轻听'),dict(ms=1100,text='你好'),dict(ms=2100,text='你好')]
assert parse_lrc('[00:00.000]')[0]['text']=='间奏 · 静静听'
for invalid in ('[00:60.0]你好','[00:01.0]甲\n[00:01.0]乙','[00:00]'+('长'*15),'[ar:Artist]'):
    try:parse_lrc(invalid)
    except ValueError:pass
    else:raise AssertionError('Invalid LRC accepted')
print('LRC: timestamps, offsets, repeated lines, intro and invalid input PASS')
