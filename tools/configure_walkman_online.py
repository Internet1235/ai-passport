#!/usr/bin/env python3
"""Provision the online player over USB without echoing Wi-Fi or cloud credentials."""
import argparse,json,shlex,time
from pathlib import Path
import serial

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True);p.add_argument('--env',type=Path,required=True)
    choice=p.add_mutually_exclusive_group(required=True)
    choice.add_argument('--setup',action='store_true',help='Save the default key privately and open the Wi-Fi picker');choice.add_argument('--reuse-wifi',action='store_true');choice.add_argument('--wifi-env',action='store_true');choice.add_argument('--wifi-file',type=Path,help='Private JSON containing ssid and password')
    args=p.parse_args();config={}
    for line in args.env.read_text().splitlines():
        if '=' not in line or line.lstrip().startswith('#'):continue
        key,value=line.split('=',1);value=value.strip();tokens=shlex.split(value) if value.startswith(('\"',chr(39))) else [value];config[key.strip()]=tokens[0] if tokens else ''
    if not config.get('DASHSCOPE_API_KEY'):raise SystemExit('DASHSCOPE_API_KEY is missing')
    payload={'api_key':config['DASHSCOPE_API_KEY'],'model':config.get('QWEN_AUDIO_REALTIME_MODEL','qwen-audio-3.0-realtime-plus')}
    if args.setup:payload['setup']=True
    elif args.reuse_wifi:payload['reuse_wifi']=True
    elif args.wifi_env:
        if not config.get('WIFI_SSID'):raise SystemExit('WIFI_SSID is missing from the environment file')
        payload.update(ssid=config['WIFI_SSID'],password=config.get('WIFI_PASSWORD',''))
    else:
        wifi=json.loads(args.wifi_file.read_text());payload.update(ssid=wifi['ssid'],password=wifi.get('password',''))
    raw=b'N'+json.dumps(payload,ensure_ascii=False,separators=(',',':')).encode()+b'\n'
    if len(raw)>1024:raise SystemExit('Configuration exceeds the device limit')
    with serial.Serial(args.port,115200,timeout=.2) as port:
        port.reset_input_buffer();port.write(b'?');deadline=time.monotonic()+5;online=False
        while time.monotonic()<deadline:
            if port.readline().startswith(b'WN_STATE '):online=True;break
        if not online:raise SystemExit('Flash the online firmware first; no credentials were sent')
        port.write(raw);port.flush();deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            line=port.readline()
            if line.startswith(b'WM_CONFIGURED true'):print('Configuration saved privately; device is restarting.');return
            if line.startswith(b'WM_CONFIGURED false'):raise SystemExit('Configuration rejected. Saved Wi-Fi may be unavailable; use --wifi-file or on-device setup.')
    raise SystemExit('No configuration receipt. Check device state before retrying.')
if __name__=='__main__':main()
