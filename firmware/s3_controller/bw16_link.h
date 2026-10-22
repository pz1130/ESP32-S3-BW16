// ============ BW16 UART JSON 链路客户端 ============
// 协议见 bw16-firmware/BW16-Tools/serial_api.h
// 用法: JsonDocument rsp; if (bw16Request("status", "", BW16_STATUS_TIMEOUT, rsp)) { rsp["heap"]... }
// extra 参数传形如 ",\"mode\":\"single\"" 的原始字段串（含前导逗号），可为空串

#ifndef BW16_LINK_H
#define BW16_LINK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

static int g_bw16Seq = 0;
// 长任务进行中（异步嗅探等）：期间禁止其它同步请求，避免干扰/吃掉响应
static bool g_bw16Busy = false;

static void bw16Init() {
  Serial1.setPins(BW16_UART_RX, BW16_UART_TX);
  Serial1.begin(BW16_BAUD);
  delay(50);
  // 清空启动噪声
  while (Serial1.available()) Serial1.read();
}

static void bw16DrainInput() {
  while (Serial1.available()) Serial1.read();
}

// 发送命令并等待同 id 的应答。成功返回 true，rsp 为应答 JSON 对象。
// 单次尝试（不含重试）
static bool bw16RequestOnce(const char* cmd, const String& extra,
                        const unsigned long timeoutMs, JsonDocument& rsp) {
  if (g_bw16Busy) return false;   // 长任务（嗅探）进行中，禁止同步命令
  int id = ++g_bw16Seq;
  String req = String("{\"cmd\":\"") + cmd + "\",\"id\":" + String(id) + extra + "}";
  bw16DrainInput();
  Serial1.print(req);
  Serial1.print('\n');
  Serial1.flush();

  unsigned long t0 = millis();
  String line = "";
  while (millis() - t0 < timeoutMs) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (line.length() == 0) continue;
        Serial.print("[UARTDBG] rx: " + line + "\n");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, line);
        line = "";
        if (err) continue;                       // 噪声行，丢弃
        long rid = doc["id"] | -1;
        if (rid != (long)id || !doc["rsp"].is<const char*>()) continue; // 非本请求应答
        rsp = doc;
        return true;
      }
      line += c;
      if (line.length() > 8192) line = "";       // 防爆
    }
    delay(2);
  }
  Serial.print("[UARTDBG] TIMEOUT(" + String(timeoutMs) + "ms): " + req + "\n");
  return false;                                  // 超时
}

// 带一次自动重试：链路偶发接触不良/半行污染会吃掉单条命令，重试即可自愈
static bool bw16Request(const char* cmd, const String& extra,
                             const unsigned long timeoutMs, JsonDocument& rsp) {
  if (bw16RequestOnce(cmd, extra, timeoutMs, rsp)) return true;
  Serial.print("[UARTDBG] retry once: " + String(cmd) + "\n");
  delay(150);
  return bw16RequestOnce(cmd, extra, timeoutMs, rsp);
}

// 便捷封装：带默认超时（走重试版本）
static bool bw16Request(const char* cmd, JsonDocument& rsp) {
  return bw16Request(cmd, "", BW16_TIMEOUT_MS, rsp);
}

// 轻量在线探测（短超时）
static bool bw16Ping() {
  JsonDocument d;
  return bw16Request("ping", "", 1000, d);
}

#endif // BW16_LINK_H
