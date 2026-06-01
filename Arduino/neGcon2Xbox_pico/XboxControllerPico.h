#pragma once

#include <stdint.h>
#include "Arduino.h"
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE 1
#endif
#include "tusb.h"

// XInputデジタルボタン定義（Button 1 - Byte 2）
#define XINPUT_GAMEPAD_DPAD_UP          0x01
#define XINPUT_GAMEPAD_DPAD_DOWN        0x02
#define XINPUT_GAMEPAD_DPAD_LEFT        0x04
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x08
#define XINPUT_GAMEPAD_START            0x10
#define XINPUT_GAMEPAD_BACK             0x20
#define XINPUT_GAMEPAD_LEFT_THUMB       0x40
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x80

// XInputデジタルボタン定義（Button 2 - Byte 3）
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x01
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x02
#define XINPUT_GAMEPAD_GUIDE            0x04
#define XINPUT_GAMEPAD_A                0x20
#define XINPUT_GAMEPAD_B                0x10
#define XINPUT_GAMEPAD_X                0x40
#define XINPUT_GAMEPAD_Y                0x80

// XInput 送信データ構造体 (20バイト)
typedef struct {
  uint8_t message_type;      // 常に 0x00
  uint8_t packet_size;       // 常に 0x14 (20)
  uint8_t digital_buttons_1; // DPAD & Start/Back/Thumb clicks
  uint8_t digital_buttons_2; // A/B/X/Y/Shoulders/Guide
  uint8_t lt;                // 左トリガー (0 - 255)
  uint8_t rt;                // 右トリガー (0 - 255)
  int16_t l_x;               // 左アナログ X (-32768 - 32767)
  int16_t l_y;               // 左アナログ Y (-32768 - 32767)
  int16_t r_x;               // 右アナログ X (-32768 - 32767)
  int16_t r_y;               // 右アナログ Y (-32768 - 32767)
  uint8_t reserved[6];       // 常に 0x00
} __attribute__((packed)) ReportDataXinput_t;

// グローバルな送信バッファの extern 宣言
extern ReportDataXinput_t XboxButtonData;

// API関数の宣言
void xboxcontroller_init(void);
void xboxcontroller_reset(void);
bool xboxcontroller_send_report(void);
