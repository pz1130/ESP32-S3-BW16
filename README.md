# BW16-WiFi-Tools（ESP32-S3 控制中心 + BW16 攻击引擎）

双开发板 WiFi 安全测试工具：
- **ESP32-S3（控制中心）**：0.96" OLED + 4 按键 + Web 管理界面，连家里 WiFi，浏览器操作
- **BW16-Kit / RTL8720DN（攻击引擎）**：不开热点，射频全部用于扫描 / 嗅探 / 攻击，由 S3 通过 UART 指挥

> 仅用于**合规前提下的安全研究与教育**，请勿滥用。

---

## 一、本仓库内容

```
BW16-WiFi-Tools/
├── firmware/
│   ├── s3_controller/      ← ESP32-S3 固件（Web + OLED + 按键 + UART 桥）
│   └── BW16-Tools/         ← BW16 固件（Web UI Edition + serial_api 串口命令层）
├── docs/
│   ├── 面包板接线图v3.html ← 可视化接线图（浏览器打开）
│   ├── wiring.md           ← 文字版接线表 + 调试指南
│   ├── 刷机避坑指南.md      ← 烧录全流程 + 踩坑记录
│   └── S3控制中心方案.md     ← 早期设计方案
├── tools/                  ← 调试/验证脚本（Python）
│   ├── serial_listen.py    ← 串口日志监听（COM口 <秒> <日志文件>）
│   ├── verify_v25k_scan.py ← 扫描验收脚本
│   ├── test_jam_manual.py  ← 手动 BSSID 压制测试（改 BSSID/信道后使用）
│   └── rescan_hotspot.py   ← 扫描确认隐藏热点 BSSID
└── snapshots/              ← 固件还原点（3 个）
    ├── v25k/               ← 扫描修复验收版（最后稳定验收版）
    ├── v25p/               ← 手动 BSSID 压制版（S3 /api/jam + 前端卡片）
    └── v25u/               ← promisc 模式压制最终版（主线同款）
```

## 二、硬件接线（要点）

| S3 (ESP32-S3 DevKitC-1) | BW16-Kit | 说明 |
|---|---|---|
| GPIO17 (TX) | PA26 (D8 RX) | S3 发 → BW16 收（**要交叉**） |
| GPIO18 (RX) | PA25 (D7 TX) | BW16 发 → S3 收 |
| GND | GND | **必须共地** |

- OLED（S3）：3V3 / GND / SDA=GPIO8 / SCL=GPIO9
- 按键（S3）：GPIO10 上 / 11 下 / 12 确认 / 13 返回，另一端共 GND
- BW16 的 LP_UART 默认引脚在板上连着 USB 芯片，固件已在运行时重映射到 PA25/PA26（`serialApiRemapPins()`）
- 详细接线见 `docs/面包板接线图v3.html` 和 `docs/wiring.md`；**信号线不需要串电阻**（双 3.3V 推挽 115200），OLED/按键也无需

## 三、烧录

见 `docs/刷机避坑指南.md`（完整流程 + 踩坑记录）。要点：

1. 环境：Arduino CLI（BW16 必须 realtek:AmebaD **3.1.7**；S3 esp32:esp32:esp32s3）
2. 库：ArduinoJson、U8g2；BW16 另需 `firmware/BW16-Tools/face`、`SDK`、`WebPages` 三个内嵌目录（已随仓库携带）
3. 烧录 BW16：按住 BOOT → 按一下 RST → 松开 BOOT → upload（COM5 示例）
4. **BW16 upload 完成后必须手动按一次 RST** 启动固件（不会自动重启）
5. 烧录 S3：upload 后自动 Hard reset
6. 两块板 USB 全拔重插可解决 S3 掉线/Wi-Fi 失效问题

## 四、Web API（S3，http://<S3-IP>/）

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/status` | 状态：bw16_online / ap_running / deauth_running / deauth_packets / adv_attack / jamming_running / scan_count / target_count |
| POST | `/api/scan` | 触发扫描（约 10–16 秒） |
| GET | `/api/scan-results` | 扫描结果：id/ssid/bssid/channel/rssi/security |
| POST | `/api/targets/select` | 目标勾选：`id=N&selected=1` / `clear=1` / `all=1` |
| POST | `/api/deauth/start` | 定向解除认证：`mode&packets&enhanced&client_mac` |
| POST | `/api/ci/start` | 高级干扰（单目标 = 高频压制） |
| POST | `/api/bbh/start` | 广播黑洞 |
| POST | `/api/jam` | **手动 BSSID 压制**：`bssid=XX:..&ch=N`（隐藏 AP 专用，v25n+） |
| POST | `/api/clients/scan` | 嗅探客户端：`sec=&induce=`（立即返回，2s 轮询） |
| GET | `/api/clients` | 嗅探结果：busy/progress/result{count, clients[{mac,rssi,frames}]} |
| POST | `/api/stop` | 停止一切攻击 |

串口协议（S3↔BW16，115200）：BW16 侧见 `firmware/BW16-Tools/serial_api.h` 的 JSON 命令层。

## 五、对话总结 / 版本演进记录

本项目从"两块开发板装面包板的接线图"起步，最终打磨成一个可用的双板 WiFi 测试工具。关键演进：

| 阶段 | 版本 | 内容 | 结果 |
|---|---|---|---|
| 早期 | — | 面包板接线图 v1→v3、串口链路（S3 GPIO17/18 ↔ BW16 PA26/25）、固件骨架 | 交付接线图 + 双板 UART 通信稳定 |
| 攻击修复 | v25c–v25e | 定向单播 deauth（client_mac 定向踢 MAC）| **NETGEAR 广播 deauth 手机掉、电脑不掉（Windows 忽略广播）→ 定向踢人有效** |
| 多目标修复 | v25f | AP 建到目标信道 + 信道漂移识别 | 多目标误打错信道根因修复 |
| 嗅探 | v25g–v25j | 控制帧抓取、诱导嗅探、70s 超时、web 全异步化 | 客户端识别从 4→9→12 个 |
| 扫描修复 | **v25k** | BW16 扫描超时 2.5s→8s（5G 高信道漏扫）、S3 自动重扫、前端重试 | **验收通过**：3 连扫 43/47/47，5G ch36/40/44/149 全覆盖 |
| 手动压制 | v25n/v25p | S3 `/api/jam` + 前端「手动 BSSID 压制」卡片；BW16 jamming 强化（AP 重建到目标信道 + 短帧洪泛） | 功能完成 |
| 5G 信道实锤 | v25t | `wifi_get_channel` 验证 | **AP 模式 wext_set_channel(149) 驱动返回 -107，5G 压制全灭根因** |
| promisc 压制 | v25u/v25v | 攻击改用 promisc 模式切信道 + 静默回调 | 帧能发到 ch149（rc=0），但驱动发送速率 ~3 帧/秒，压制占空比不足 |

**当前主线**：S3 = v25n（Web/OLED/UART 最终版），BW16 = v25u（promisc 压制，无调试残留）。

## 六、已实测验证的能力

- ✅ 全频段扫描稳定（2.4G + 5G 高信道，隐藏 SSID 也能扫到）
- ✅ 嗅探自动识别客户端（诱导嗅探：先广播 deauth 逼重连再抓）
- ✅ **2.4G 无 PMF 路由器定向 deauth 踢人**（NETGEAR/Redmi 实测有效；电脑/Wi-Fi7 网卡会忽略广播 deauth，定向单播可踢）
- ✅ 洪水攻击阻止**新设备**连接（认证/关联洪泛对准备连接的设备有效）
- ✅ 攻击停止稳定（先停调度再等发送结束，避免撞驱动崩溃）

## 七、已知限制（诚实记录）

1. **iPhone 个人热点（WPA3/PMF）踢不掉已连接设备**——三层实锤：
   - 协议层：WPA3 强制 PMF，伪造 deauth/disassoc 被丢弃
   - 信道层：AP 模式 `wext_set_channel` 对 5G 高信道无效（驱动返回 -107）
   - 发送层：promisc 模式能切到 5G，但 `wifi_tx_raw_frame` 驱动发送速率仅 ~3 帧/秒，压制/CSA/NAV 占空比不足
   - 结论：这是 RTL8720DN 硬件 + 驱动极限，非固件 bug；要踢 WPA3/PMF 设备需换 monitor+injection 网卡（如 RTL8812AU + mdk4）或 SDR
2. 洪水攻击对**已连接**设备无效（仅阻止新连接）
3. 多目标同时攻击不可靠（AP 只能建在一个信道），建议单目标操作
4. 攻击时 BW16 板载 RGB 闪烁功能部分版本未达预期（软件确认在闪，物理位置未确认）

## 八、来源鸣谢（上游）

- [5gwifi-bw16](https://github.com/shiyi226/5gwifi-bw16) —— BW16 5G 能力基础
- [RTL8720dn-WiFi-Packet-Injection](https://github.com/tesa-klebeband/RTL8720dn-WiFi-Packet-Injection) —— 原始帧收发
- [BW16-Tools](https://github.com/FlyingIceyyds/BW16-Tools) —— Web UI Edition 固件基础

## 九、许可证

沿用上游 **GPL-3.0**：禁止修改后闭源售卖；禁止应用于闭源项目/产品；衍生需保留版权并无条件提供源代码。
