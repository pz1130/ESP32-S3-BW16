# -*- coding: utf-8 -*-
"""扫描找 iphonejc（隐藏 SSID，5G ch149 或 2.4G）当前 BSSID"""
import urllib.request, urllib.parse, json, time
BASE = 'http://192.168.31.27'
def post(p, b=None, t=20):
    d = None if b is None else urllib.parse.urlencode(b).encode()
    return json.loads(urllib.request.urlopen(urllib.request.Request(BASE+p, data=d, method='POST'), timeout=t).read().decode())
def get(p, t=15):
    return json.loads(urllib.request.urlopen(BASE+p, timeout=t).read().decode())

print('触发扫描...')
try:
    print(post('/api/scan'))
except Exception as e:
    print('scan err:', e)
# 等待扫描完成（10-16秒）
for i in range(30):
    time.sleep(1)
    try:
        res = get('/api/scan-results')
        if isinstance(res, list) and len(res) > 0:
            break
    except Exception:
        pass
if isinstance(res, list):
    print('扫描到 %d 个 AP' % len(res))
    # 找 ch149 及隐藏/无名字条目
    for r in res:
        ch = r.get('channel', 0)
        ssid = r.get('ssid', '')
        bssid = r.get('bssid', '')
        rssi = r.get('rssi', 0)
        if ch >= 100 or not ssid:
            print(' ch=%d rssi=%d bssid=%s ssid=[%s]' % (ch, rssi, bssid, ssid))
else:
    print('扫描结果异常:', res)
