// ============ OLED 菜单界面（U8g2） ============
// 页面：状态 / 扫描列表 / 攻击 / 钓鱼 / 抓包 / 系统
// 依赖主程序中先定义好的共享状态（s_bw 等）与函数（selectTarget/attackAction 等）

#ifndef OLED_UI_H
#define OLED_UI_H

#include <U8g2lib.h>
#include <Wire.h>

#if OLED_USE_SH1106
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#endif

enum UiPage : uint8_t { UI_STATUS = 0, UI_SCAN, UI_ATTACK, UI_PHISH, UI_CAPTURE, UI_SYSTEM };
static UiPage s_page = UI_STATUS;
static int s_menuIdx = 0;
static int s_scanSel = 0;      // 扫描列表高亮行
static int s_scanScroll = 0;   // 滚动偏移
static unsigned long s_statusDrawTs = 0;

static const char* const ATTACK_ITEMS[] = {
  "Deauth(选中)", "Deauth(全部)", "Beacon随机", "Beacon克隆",
  "信道干扰", "广播黑洞", "AP洪水", "停止全部" };
static const int ATTACK_ITEM_N = 8;
static const char* const PHISH_ITEMS[] = { "启动钓鱼(模板1)", "启动钓鱼(模板2)", "停止钓鱼", "清空密码" };
static const int PHISH_ITEM_N = 4;
static const char* const CAP_ITEMS[] = { "开始抓包(选中,主动)", "开始抓包(被动)", "停止抓包", "清空抓包" };
static const int CAP_ITEM_N = 4;
static const char* const SYS_ITEMS[] = { "重启BW16", "重启S3", "关于" };
static const int SYS_ITEM_N = 3;

static void oledInit() {
  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);   // 中文字体
}

static void oledHeader(const char* title) {
  u8g2.drawRFrame(0, 0, 128, 13, 2);
  u8g2.setCursor(4, 10);
  u8g2.print(title);
  u8g2.drawHLine(0, 13, 128);
}

static void oledDrawMenu(const char* title, const char* const* items, int n, int idx) {
  u8g2.clearBuffer();
  oledHeader(title);
  const int rows = 4;
  int scroll = (idx / rows) * rows;
  for (int r = 0; r < rows; r++) {
    int i = scroll + r;
    if (i >= n) break;
    int y = 24 + r * 13;
    u8g2.setCursor(6, y);
    u8g2.print(i == idx ? ">" : " ");
    u8g2.setCursor(16, y);
    u8g2.print(items[i]);
  }
  u8g2.sendBuffer();
}

static void oledDrawStatus() {
  u8g2.clearBuffer();
  oledHeader("BW16 状态");
  u8g2.setCursor(2, 24);
  u8g2.print(s_bw.online ? "在线" : "离线(检查接线)");
  u8g2.setCursor(70, 24);
  u8g2.print("内存:");
  u8g2.print(s_bw.online ? (long)s_bw.heap : 0);
  u8g2.setCursor(2, 38);
  u8g2.print("Deauth:");
  u8g2.print(s_bw.deauthRunning ? "运行" : "停止");
  u8g2.print(" 包:");
  u8g2.print((long)s_bw.deauthPackets);
  u8g2.setCursor(2, 52);
  u8g2.print("目标:");
  u8g2.print(s_bw.targetCount);
  u8g2.print(" 扫描:");
  u8g2.print(s_bw.scanCount);
  u8g2.setCursor(2, 63);
  u8g2.print("干扰:");
  u8g2.print(s_bw.jamming ? "开" : "关");
  u8g2.print(" BBH:");
  u8g2.print(s_bw.bbh ? "开" : "关");
  u8g2.print(" DoS:");
  u8g2.print(s_bw.dos ? "开" : "关");
  u8g2.sendBuffer();
}

static void oledDrawScan() {
  u8g2.clearBuffer();
  oledHeader("扫描列表(OK选择)");
  int n = (int)s_scanList.size();
  if (n == 0) {
    u8g2.setCursor(4, 34);
    u8g2.print("空 - 按<确认>扫描");
    u8g2.sendBuffer();
    return;
  }
  const int rows = 4;
  s_scanScroll = constrain(s_scanSel, s_scanScroll, s_scanSel);
  s_scanScroll = constrain(s_scanScroll, 0, (n > rows ? n - rows : 0));
  for (int r = 0; r < rows; r++) {
    int i = s_scanScroll + r;
    if (i >= n) break;
    const ScanItem& it = s_scanList[i];
    int y = 24 + r * 13;
    u8g2.setCursor(2, y);
    u8g2.print(i == s_scanSel ? ">" : " ");
    u8g2.setCursor(12, y);
    u8g2.print(it.selected ? "[*]" : "[ ]");
    u8g2.setCursor(34, y);
    String nm = it.ssid.length() ? it.ssid : (it.bssid.substring(0, 8) + "..");
    if (nm.length() > 11) nm = nm.substring(0, 11);
    u8g2.print(nm);
    u8g2.setCursor(100, y);
    u8g2.print("ch");
    u8g2.print(it.channel);
  }
  u8g2.sendBuffer();
}

static void oledLoop() {
  // 状态页低频刷新（Web 层的 refreshStatus 提供数据）
  if (s_page == UI_STATUS && millis() - s_statusDrawTs > 1000) {
    s_statusDrawTs = millis();
    oledDrawStatus();
  }
}

static void oledEnterPage(UiPage p) {
  s_page = p;
  s_menuIdx = 0;
  if (p == UI_STATUS) oledDrawStatus();
  else if (p == UI_SCAN) oledDrawScan();
  else if (p == UI_ATTACK) oledDrawMenu("攻击", ATTACK_ITEMS, ATTACK_ITEM_N, s_menuIdx);
  else if (p == UI_PHISH) oledDrawMenu("钓鱼", PHISH_ITEMS, PHISH_ITEM_N, s_menuIdx);
  else if (p == UI_CAPTURE) oledDrawMenu("抓包", CAP_ITEMS, CAP_ITEM_N, s_menuIdx);
  else oledDrawMenu("系统", SYS_ITEMS, SYS_ITEM_N, s_menuIdx);
}

// 扫描列表页的确认动作
static void oledScanOk() {
  int n = (int)s_scanList.size();
  if (n == 0) {
    // 触发扫描（阻塞几秒）
    oledDrawMenu("扫描中..请稍候", (const char* const*)nullptr, 0, 0);
    u8g2.clearBuffer(); oledHeader("扫描中.."); u8g2.setCursor(4, 34);
    u8g2.print("请等待 3-5 秒"); u8g2.sendBuffer();
    triggerScan();                     // 主程序实现
    loadScanList();                    // 主程序实现：刷新 s_scanList
    s_scanSel = 0; s_scanScroll = 0;
    oledDrawScan();
    return;
  }
  bool nowSel = !s_scanList[s_scanSel].selected;
  selectTarget(s_scanList[s_scanSel].id, nowSel);
  s_scanList[s_scanSel].selected = nowSel;
  oledDrawScan();
}

// 各子菜单的确认动作，返回 true 表示留在本页
static bool oledMenuOk(UiPage p, int idx) {
  if (p == UI_ATTACK) {
    switch (idx) {
      case 0: attackDeauthSelected(); break;
      case 1: attackDeauthAll(); break;
      case 2: attackBeaconRandom(); break;
      case 3: attackBeaconClone(); break;
      case 4: attackCi(); break;
      case 5: attackBbh(); break;
      case 6: attackDos(); break;
      case 7: attackStopAll(); break;
    }
  } else if (p == UI_PHISH) {
    switch (idx) {
      case 0: phishStart(1); break;
      case 1: phishStart(2); break;
      case 2: phishStop(); break;
      case 3: phishClear(); break;
    }
  } else if (p == UI_CAPTURE) {
    switch (idx) {
      case 0: captureStart("active"); break;
      case 1: captureStart("passive"); break;
      case 2: captureStop(); break;
      case 3: captureClear(); break;
    }
  } else if (p == UI_SYSTEM) {
    switch (idx) {
      case 0: bw16Request("reboot", "", BW16_TIMEOUT_MS, g_uiDoc); break;
      case 1: ESP.restart(); break;
      case 2: break;
    }
  }
  return true;
}

static void oledHandleButton(BtnEvent ev) {
  if (ev == BTN_EV_NONE) return;
  if (s_page == UI_STATUS) {
    if (ev == BTN_EV_OK || ev == BTN_EV_DOWN) oledEnterPage(UI_ATTACK);
    else if (ev == BTN_EV_UP) oledEnterPage(UI_SCAN);
    else if (ev == BTN_EV_BACK) { bw16RefreshStatus(); oledDrawStatus(); }
    return;
  }
  if (s_page == UI_SCAN) {
    int n = (int)s_scanList.size();
    if (ev == BTN_EV_UP) { if (s_scanSel > 0) s_scanSel--; oledDrawScan(); }
    else if (ev == BTN_EV_DOWN) { if (s_scanSel < n - 1) s_scanSel++; oledDrawScan(); }
    else if (ev == BTN_EV_OK) oledScanOk();
    else if (ev == BTN_EV_BACK) oledEnterPage(UI_STATUS);
    return;
  }
  // 通用菜单页
  int n = (s_page == UI_ATTACK) ? ATTACK_ITEM_N : (s_page == UI_PHISH) ? PHISH_ITEM_N
        : (s_page == UI_CAPTURE) ? CAP_ITEM_N : SYS_ITEM_N;
  const char* const* items = (s_page == UI_ATTACK) ? ATTACK_ITEMS : (s_page == UI_PHISH) ? PHISH_ITEMS
        : (s_page == UI_CAPTURE) ? CAP_ITEMS : SYS_ITEMS;
  const char* title = (s_page == UI_ATTACK) ? "攻击" : (s_page == UI_PHISH) ? "钓鱼"
        : (s_page == UI_CAPTURE) ? "抓包" : "系统";
  if (ev == BTN_EV_UP) { s_menuIdx = (s_menuIdx + n - 1) % n; oledDrawMenu(title, items, n, s_menuIdx); }
  else if (ev == BTN_EV_DOWN) { s_menuIdx = (s_menuIdx + 1) % n; oledDrawMenu(title, items, n, s_menuIdx); }
  else if (ev == BTN_EV_OK) {
    if (oledMenuOk(s_page, s_menuIdx)) oledDrawMenu(title, items, n, s_menuIdx);
  }
  else if (ev == BTN_EV_BACK) oledEnterPage(UI_STATUS);
}

#endif // OLED_UI_H
