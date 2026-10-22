// ============ 4 按键模块（独立 GPIO，另一端共 GND） ============
#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include "config.h"

enum BtnEvent : uint8_t { BTN_EV_NONE = 0, BTN_EV_UP, BTN_EV_DOWN, BTN_EV_OK, BTN_EV_BACK };

static uint8_t s_btnPins[4] = { BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK };
static bool s_btnStable[4] = { true, true, true, true };
static bool s_btnLast[4] = { true, true, true, true };
static unsigned long s_btnLastChange[4] = { 0, 0, 0, 0 };
static const unsigned long BTN_DEBOUNCE_MS = 30;

static void buttonsInit() {
  for (int i = 0; i < 4; i++) {
    pinMode(s_btnPins[i], INPUT_PULLUP);
    s_btnLast[i] = s_btnStable[i] = digitalRead(s_btnPins[i]) == LOW ? false : true;
  }
}

// 返回一次按键事件（松开时触发，即“按下并释放”计一次）
static BtnEvent buttonsLoop() {
  BtnEvent ev = BTN_EV_NONE;
  for (int i = 0; i < 4 && ev == BTN_EV_NONE; i++) {
    bool raw = digitalRead(s_btnPins[i]) == LOW;   // LOW = 按下
    unsigned long now = millis();
    if (raw != s_btnLast[i]) {
      s_btnLast[i] = raw;
      s_btnLastChange[i] = now;
    }
    if (now - s_btnLastChange[i] >= BTN_DEBOUNCE_MS && raw != s_btnStable[i]) {
      s_btnStable[i] = raw;
      if (!raw) {                                   // 释放瞬间
        if (i == 0) ev = BTN_EV_UP;
        else if (i == 1) ev = BTN_EV_DOWN;
        else if (i == 2) ev = BTN_EV_OK;
        else ev = BTN_EV_BACK;
      }
    }
  }
  return ev;
}

#endif // BUTTONS_H
