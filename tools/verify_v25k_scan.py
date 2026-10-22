# -*- coding: utf-8 -*-
"""v25k 验证：全频段扫描稳定 + 5G 覆盖"""
import urllib.request, urllib.parse, json, time, sys

BASE = "http://192.168.31.27"

def post(path, body=None, timeout=30):
    data = None if body is None else urllib.parse.urlencode(body).encode()
    req = urllib.request.Request(BASE + path, data=data, method="POST")
    return json.loads(urllib.request.urlopen(req, timeout=timeout).read().decode())

def get(path, timeout=12):
    return json.loads(urllib.request.urlopen(BASE + path, timeout=timeout).read().decode())

for i in range(12):
    try:
        st = get("/api/status")
        if st.get("bw16_online"): break
    except Exception:
        pass
    time.sleep(3)
if not st or not st.get("bw16_online"):
    print("!! 离线"); sys.exit(1)
print("在线")

# 连扫 3 次，验证稳定性
for i in range(3):
    t0 = time.time()
    try:
        r = post("/api/scan", {}, 30)
        dt = time.time() - t0
        print("scan%d: success=%s count=%s 耗时=%.1fs" % (i, r.get("success"), r.get("count"), dt))
    except Exception as e:
        print("scan%d 异常: %s" % (i, e))
    time.sleep(2)

# 看列表（信道分布 + 5G 覆盖）
res = get("/api/scan-results")
nets = res if isinstance(res, list) else res.get("networks", [])
print("列表条数:", len(nets))
chans = {}
for n in nets:
    ch = n.get("channel", 0)
    chans[ch] = chans.get(ch, 0) + 1
ch5 = sorted(c for c in chans if c >= 36)
print("2.4G 信道分布:", {c: chans[c] for c in sorted(chans) if c < 36})
print("5G 信道:", ch5, "（共 %d 个5G网络）" % sum(chans[c] for c in ch5))
