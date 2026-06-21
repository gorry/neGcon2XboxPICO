#pragma once

#include <stdint.h>
#include "Arduino.h"
#include <stdarg.h>

class DummySerial {
public:
  void begin(unsigned long baud) {
    Serial1.begin(baud);
  }
  void print(const __FlashStringHelper *ifsh) { Serial1.print(ifsh); }
  void print(const String &s) { Serial1.print(s); }
  void print(const char str[]) { Serial1.print(str); }
  void print(char c) { Serial1.print(c); }
  void print(unsigned char b, int base = 10) { Serial1.print(b, base); }
  void print(int n, int base = 10) { Serial1.print(n, base); }
  void print(unsigned int n, int base = 10) { Serial1.print(n, base); }
  void print(long n, int base = 10) { Serial1.print(n, base); }
  void print(unsigned long n, int base = 10) { Serial1.print(n, base); }
  void print(double n, int digits = 2) { Serial1.print(n, digits); }
  
  void println(const __FlashStringHelper *ifsh) { Serial1.println(ifsh); }
  void println(const String &s) { Serial1.println(s); }
  void println(const char str[]) { Serial1.println(str); }
  void println(char c) { Serial1.println(c); }
  void println(unsigned char b, int base = 10) { Serial1.println(b, base); }
  void println(int n, int base = 10) { Serial1.println(n, base); }
  void println(unsigned int n, int base = 10) { Serial1.println(n, base); }
  void println(long n, int base = 10) { Serial1.println(n, base); }
  void println(unsigned long n, int base = 10) { Serial1.println(n, base); }
  void println(double n, int digits = 2) { Serial1.println(n, digits); }
  void println(void) { Serial1.println(); }
  
  void printf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial1.print(buf);
  }
  
  operator bool() { return true; }
};

extern DummySerial Serial_Dummy;
#define Serial Serial_Dummy

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

extern bool usb_mode_msc;
extern bool config_file_writing;
void xboxcontroller_reconnect(bool as_msc);
