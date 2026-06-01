////////////////////////////////////////////////////////////////////////
//
// PSX NeG Controller to Xbox (XInput) Joystick Sample
// Raspberry Pi Pico version
// Copyright 2026 @V9938 / Antigravity Migration
//
//  Based on original work by @V9938 (neGcon2Switch_pico)
//
////////////////////////////////////////////////////////////////////////

#include "Arduino.h"

  // XInput モードでは CDC (Serial) インターフェースが消滅するため、Serial への書き込みによるフリーズを防ぐために強制ダミー化します
#define Serial Serial_Dummy
class DummySerial {
public:
  void begin(unsigned long baud) {}
  void print(const __FlashStringHelper *ifsh) {}
  void print(const String &s) {}
  void print(const char str[]) {}
  void print(char c) {}
  void print(unsigned char b, int base = 10) {}
  void print(int n, int base = 10) {}
  void print(unsigned int n, int base = 10) {}
  void print(long n, int base = 10) {}
  void print(unsigned long n, int base = 10) {}
  void print(double n, int digits = 2) {}
  void println(const __FlashStringHelper *ifsh) {}
  void println(const String &s) {}
  void println(const char str[]) {}
  void println(char c) {}
  void println(unsigned char b, int base = 10) {}
  void println(int n, int base = 10) {}
  void println(unsigned int n, int base = 10) {}
  void println(long n, int base = 10) {}
  void println(unsigned long n, int base = 10) {}
  void println(double n, int digits = 2) {}
  void println(void) {}
  operator bool() { return true; }
};
static DummySerial Serial_Dummy;

#include <SPI.h>
#include "PsxControllerPICO_HwSpi2.h"
#include "XboxControllerPico.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

// EEPROMの設定

// EEPROMの設定
#define EEPROMSIZE 256
// EEPROMのアドレス配置
#define EEPADR_NEGMODE 3
#define EEPADR_NEG_NEGMAX 4
#define EEPADR_ANALOG_STICKMAX 5
#define EEPADR_JOG_MAX_U 6
#define EEPADR_JOG_MAX_L 7

// neGcon ハンドルアナログ値の補正 これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB 1.2

// neGcon ボタン アナログ値の補正 これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB_B 1.1

// jogconの境界線補正値 これを超えるとForcebackが入る、ただし強さが2になるまで人間では分からん
#define JOG_MAX_AJST -4

// NeoPixelの設定
#define NUMPIXELS 1
#define NEO_PWR 11  // GPIO11
#define NEOPIX 12  // GPIO12

//Stick Mode
#define MODE_STD 0
#define MODE_SWAPAB 1
#define MODE_SWAPLTRT 2
#define MODE_SWAPAB_SWAPLTRT 3

#define MODE_AIRCON22 10

#define MODE_SETTING_NEG 98
#define MODE_LOST 99


/** \brief Pin used for Controller Attention (ATTN)
 */
const unsigned int PIN_PS2_CMD = 3;  // MOSI / CMD (GP3)
const unsigned int PIN_PS2_DAT = 4;  // MISO / DAT (GP4)
const unsigned int PIN_PS2_CLK = 2;  // SCK / CLK (GP2)
const unsigned int PIN_PS2_ATT = 1;  // CS / ATTN (GP1)

// Set up the speed, data order and data mode
// static SPISettings spiSettings(250000, LSBFIRST, SPI_MODE3);

/** \brief Pin for Button Press Led
 */
const unsigned int PIN_CONNECT = 25;  // BLUE
const unsigned int PIN_ERRORLED = 17;  // RED
const unsigned int PIN_GOODLED = 16;  // GREEN

bool haveController = false;
const byte ANALOG_DEAD_ZONE = 50U;

PsxControllerPICO_HwSpi<PIN_PS2_ATT, PIN_PS2_CLK, PIN_PS2_DAT, PIN_PS2_CMD> psx;
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIX, NEO_GRB + NEO_KHZ800);

// Controller Type
const char ctrlTypeUnknown[] PROGMEM = "Unknown";
const char ctrlTypeDualShock[] PROGMEM = "Dual Shock";
const char ctrlTypeDsWireless[] PROGMEM = "Dual Shock Wireless";
const char ctrlTypeGuitHero[] PROGMEM = "3rd Party Controller";
const char ctrlTypeOutOfBounds[] PROGMEM = "(Out of bounds)";

const char* const controllerTypeStrings[PSCTRL_MAX + 1] PROGMEM = {
  ctrlTypeUnknown,
  ctrlTypeDualShock,
  ctrlTypeDsWireless,
  ctrlTypeGuitHero,
  ctrlTypeOutOfBounds
};


// Controller Protocol
const char ctrlProtoUnknown[] PROGMEM = "Unknown";
const char ctrlProtoDigital[] PROGMEM = "Digital";
const char ctrlProtoDualShock[] PROGMEM = "Dual Shock";
const char ctrlProtoDualShock2[] PROGMEM = "Dual Shock 2";
const char ctrlProtoFlightstick[] PROGMEM = "FlightstickCPH-1110)";
const char ctrlProtoNegcon[] PROGMEM = "namco neGcon";
const char ctrlProtoJogcon[] PROGMEM = "namco Jogcon";
const char ctrlProtoOutOfBounds[] PROGMEM = "(Out of bounds)";

const char* const controllerProtoStrings[PSPROTO_MAX + 1] PROGMEM = {
  ctrlProtoUnknown,
  ctrlProtoDigital,
  ctrlProtoDualShock,
  ctrlProtoDualShock2,
  ctrlProtoFlightstick,
  ctrlProtoNegcon,
  ctrlProtoJogcon,
  ctrlTypeOutOfBounds
};

byte ledLx, ledLy, ledRx, ledRy, ledB1, ledB2, ledBL;
byte stickMode;
byte lxMax;
byte analogLxMax;
short jogconDialMax;

// STARTメタキー機能のための状態定義
enum MetaKeyState {
  META_STATE_IDLE = 0,
  META_STATE_START_PRESSED,
  META_STATE_ACTIVE,
  META_STATE_NORMAL_START
};
static MetaKeyState metaState = META_STATE_IDLE;
static unsigned long menuOnUntil = 0;

// EEPROMのClear関数
void eepromFormat() {
  Serial.println(F("EEP Write!"));
  for (int i = 0; i < EEPROMSIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.write(0, 'c');
  EEPROM.write(1, 'f');
  EEPROM.write(2, '1');
  EEPROM.write(EEPADR_NEGMODE, MODE_STD);
  EEPROM.write(EEPADR_NEG_NEGMAX, 255 / ((NEG_CALIB - 1) / 2 + 1));
  EEPROM.write(EEPADR_ANALOG_STICKMAX, 255);
  EEPROM.write(EEPADR_JOG_MAX_U, 0);
  EEPROM.write(EEPADR_JOG_MAX_L, 100);
  EEPROM.commit();
}

// EEPROM領域の確認
bool eepromCheck() {
  if (EEPROM.read(0) != 'c') return false;
  if (EEPROM.read(1) != 'f') return false;
  if (EEPROM.read(2) != '1') return false;
  return true;
}

// neGconモードひねり値の最大値の復帰
byte restoreNegDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_NEGMAX);

  if (tmp < 0x80) {
    tmp = 255 / ((NEG_CALIB - 1) / 2 + 1);
    Serial.println(F("EEP Write!"));
    EEPROM.write(EEPADR_NEG_NEGMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

// JOGCONひねり値の最大値の復帰
short restorejogMax() {
  short tmp;
  tmp = (short)EEPROM.read(EEPADR_JOG_MAX_U);
  tmp = (tmp << 8) | (short)EEPROM.read(EEPADR_JOG_MAX_L);

  if (tmp < 0x10) {
    tmp = 100;
    EEPROM.write(EEPADR_JOG_MAX_U, 0);
    EEPROM.write(EEPADR_JOG_MAX_L, 100);
    Serial.println(F("EEP Write!"));
    EEPROM.write(EEPADR_NEG_NEGMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

// アナログスティックモードひねり値の最大値の復帰
byte restoreAnaDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_ANALOG_STICKMAX);

  if (tmp < 0x80) {
    tmp = 255;
    Serial.println(F("EEP Write!"));
    EEPROM.write(EEPADR_ANALOG_STICKMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

// neGconモード設定の復帰
byte restoreNegStickMode() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEGMODE);

  switch (tmp) {
    case MODE_STD:
    case MODE_SWAPAB:
    case MODE_SWAPLTRT:
    case MODE_SWAPAB_SWAPLTRT:
    case MODE_AIRCON22:
      break;

    default:
      tmp = MODE_STD;
      Serial.println(F("EEP Write!"));
      EEPROM.write(EEPADR_NEGMODE, tmp);
      EEPROM.commit();
      break;
  }

  return tmp;
}

// 絶対値 計算関数
int absoluteXY(byte lx) {
  int lx_tmp;
  if (lx < 0x80) lx_tmp = (int)(0xFF - lx);
  else lx_tmp = (int)lx;
  return lx_tmp;
}

// short型絶対値計算関数
short jogcon_abs_val(short x) {
  return x < 0 ? -x : x;
}

// 補正値計算
int adjustXY(byte lx, byte max) {
  int lx_tmp;

  if (lx > 0x80) {
    lx_tmp = (int)((lx - 0x80)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x7f) lx_tmp = 0x7f;
    lx_tmp = 0x80 + lx_tmp;
  } else {
    lx_tmp = (int)((0x80 - lx)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x80) lx_tmp = 0x80;
    lx_tmp = 0x80 - lx_tmp;
  }
  return lx_tmp;
}

// プレステコントローラ向けの標準キー配置のXboxへの変換処理
void keyConvert_psx2xbox_ex(uint16_t buttons) {
  // D-pad (十字キー)
  if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_UP;
  if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_DOWN;
  if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_LEFT;
  if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_RIGHT;

  // SELECT = BACK
  if (buttons & PSB_SELECT)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;

  // L3 = LEFT_THUMB
  if (buttons & PSB_L3)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;

  // R3 = RIGHT_THUMB
  if (buttons & PSB_R3)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;

  // START = START
  if (buttons & PSB_START)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;

  // L2 = LT (左トリガー全開)
  if (buttons & PSB_L2)
    XboxButtonData.lt = 255;

  // R2 = RT (右トリガー全開)
  if (buttons & PSB_R2)
    XboxButtonData.rt = 255;

  // L1 = LB (Left Shoulder)
  if (buttons & PSB_L1)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;

  // R1 = RB (Right Shoulder)
  if (buttons & PSB_R1)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

  // Triangle = Y
  if (buttons & PSB_TRIANGLE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;

  // Circle = B
  if (buttons & PSB_CIRCLE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;

  // Cross = A
  if (buttons & PSB_CROSS)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;

  // Square = X
  if (buttons & PSB_SQUARE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_X;
}

void keyConvert_psx2xbox() {
  keyConvert_psx2xbox_ex(psx.getButtonWord());
}


// CPU1では、NeoPixel LEDの点灯処理をさせる
void setup1() {  // core 0
  pixels.begin();
  pinMode(NEO_PWR, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  delay(1000);
}

void loop1() {  // core 0
  static byte heartbeat_num = 0;
  static bool heartbeat_flag = true;
  const byte heartbeat_speed = 32;

  switch (stickMode) {
    case MODE_STD:
      // 青色ベース（標準モード、無入力時は青）
      // pixels.setPixelColor(0, pixels.Color(ledB1 / 2 + ledBL / 2, ledB2 / 2 + ledBL / 2, 0x80 + ledLx / 4));
      pixels.setPixelColor(0, pixels.Color(ledB1 / 2 + ledBL / 2, ledB2 / 2 + ledBL / 2, ledLx));
      pixels.show();
      break;

    case MODE_SWAPAB:
      // オレンジ・赤ベース (A/Bスワップ、無入力時は紫)
      // pixels.setPixelColor(0, pixels.Color(0x80 + ledLx / 4, 0x20 + ledB1 / 2, 0));
      pixels.setPixelColor(0, pixels.Color(0x80 - ledB1 + ledB2, 0, ledLx));
      pixels.show();
      break;

    case MODE_SWAPLTRT:
      // 緑色ベース (LT/RTスワップ、無入力時は緑)
      // pixels.setPixelColor(0, pixels.Color(0, 0x80 + ledLx / 4, ledB2 / 2));
      pixels.setPixelColor(0, pixels.Color(ledB2 / 2 + ledBL / 2, ledLx, ledB1 / 2 + ledBL / 2));
      pixels.show();
      break;

    case MODE_SWAPAB_SWAPLTRT:
      // 紫・マゼンタベース (両方スワップ、無入力時は水色)
      // pixels.setPixelColor(0, pixels.Color(0x80 + ledLx / 4, 0, 0x80 + ledB2 / 2 + ledBL / 2));
      pixels.setPixelColor(0, pixels.Color(0, 0x80 - ledB1 + ledB2, ledLx / 2 + ledBL / 2));
      pixels.show();
      break;

      // フライトコントローラ接続時の点灯パターン
    case MODE_AIRCON22:
      pixels.setPixelColor(0, pixels.Color(ledRy / 4, ledLx / 8, ledLy / 8 + 0x40));
      pixels.show();
      break;

    case MODE_SETTING_NEG:
      pixels.setPixelColor(0, pixels.Color(0x0, 0x0, heartbeat_num));
      pixels.show();
      break;

    default:
      pixels.clear();
      pixels.show();
      break;
  }

  //LEDの点滅処理数値計算 (delay(50)にあわせてステップ幅を8に調整し、約3秒周期で明滅)
  if (heartbeat_flag) {
    if (heartbeat_num >= 255 - heartbeat_speed) {
      heartbeat_num = 255;
      heartbeat_flag = false;
    } else {
      heartbeat_num += heartbeat_speed;
    }
  } else {
    if (heartbeat_num <= heartbeat_speed) {
      heartbeat_num = 0;
      heartbeat_flag = true;
    } else {
      heartbeat_num -= heartbeat_speed;
    }
  }

  delay(50); // Sub core の無駄な CPU 占有とバス負荷を軽減するためのウェイト
}

void setup() {
  // 安全に USB XInput を初期化（No USBモードの TinyUSB 起動）
  xboxcontroller_init();
  xboxcontroller_reset();

  // 基板のステータスLEDの初期化
  pinMode(PIN_CONNECT, OUTPUT);
  pinMode(PIN_ERRORLED, OUTPUT);
  pinMode(PIN_GOODLED, OUTPUT);

  // 初期状態: LED消灯 (Active Low)
  digitalWrite(PIN_CONNECT, HIGH);
  digitalWrite(PIN_ERRORLED, HIGH);
  digitalWrite(PIN_GOODLED, HIGH);

  // EEPROMの初期化と復元（ハードウェアSPIは完全に無効化し、ソフトウェアBit-Bangを使用）
  EEPROM.begin(EEPROMSIZE);
  if (eepromCheck() != true) eepromFormat();

  lxMax = restoreNegDegMax();
  analogLxMax = restoreAnaDegMax();
  jogconDialMax = restorejogMax();
  
  stickMode = MODE_LOST;
  delay(300);

  Serial.begin(115200);
  Serial.println(F("Ready (Xbox XInput Mode)!"));
}

void loop() {
  // TinyUSBデバイススタックのタスク処理を最優先で巡回させる
  tud_task();

  static unsigned long lastPsxReadTime = 0;
  static unsigned long lastSearchTime = 0; // psx.begin() の間隔制限用
  unsigned long now = millis();

  // 8ms (約125Hz) インターバルでコントローラー処理を実行し、USB応答性を最大化する
  if (now - lastPsxReadTime < 8) {
    xboxcontroller_send_report();
    return;
  }
  lastPsxReadTime = now;

  static int sBootSel = 0;
  static byte beforeStickMode;
  static bool changeNegStickMode;
  static byte slx, sly, sb1, sb2, sbL;
  static byte srx, sry;
  static short sjogx;
  static byte jogxForcePower = 0;
  static byte jogxPosResetEnable;

  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte lx_org, ly_org, b1_org, b2_org, bL_org;
  byte rx_org, ry_org;
  byte r_x, r_y;
  int l_x_tmp, xy_tmp;
  short jogx;

  static PsxControllerType psxContType;
  PsxControllerProtocol psxStickMode;
  static PsxControllerProtocol OldpsxStickMode;

  // BOOTスイッチの処理 現状MODE選択に割り当てている
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW);
    if (sBootSel < 375) {
      if (sBootSel == 0) changeNegStickMode = false;
      if (sBootSel == 9) {
        changeNegStickMode = true;
        Serial.print(F("Current Stick mode is: "));
        Serial.println(stickMode);
      }
      if (sBootSel == 374) {
        if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON) || (OldpsxStickMode == PSPROTO_FLIGHTSTICK)) {
          Serial.println(F("Set Config mode"));
          changeNegStickMode = false;
          beforeStickMode = stickMode;
          stickMode = MODE_SETTING_NEG;
        }
      }
      sBootSel++;
    }
  } else {
    digitalWrite(PIN_ERRORLED, HIGH);
    if (changeNegStickMode) {
      if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON)) {
        if (stickMode == MODE_STD) stickMode = MODE_SWAPAB;
        else if (stickMode == MODE_SWAPAB) stickMode = MODE_SWAPLTRT;
        else if (stickMode == MODE_SWAPLTRT) stickMode = MODE_SWAPAB_SWAPLTRT;
        else if (stickMode == MODE_SWAPAB_SWAPLTRT) stickMode = MODE_STD;
        else stickMode = MODE_STD;

        Serial.println(F("EEP Write!"));
        EEPROM.write(EEPADR_NEGMODE, stickMode);
        EEPROM.commit();
      }

      if (stickMode == MODE_SETTING_NEG) stickMode = beforeStickMode;

      Serial.print(F("Change Stick mode is: "));
      Serial.println(stickMode);
    }

    changeNegStickMode = false;
    sBootSel = 0;
  }


  if (!haveController) {
    // コントローラー未検出時は1秒間隔で探索（USB列挙処理への干渉を防ぐ）
    if (now - lastSearchTime >= 1000) {
      lastSearchTime = now;
      if (psx.begin()) {
        Serial.println(F("Controller found!"));
        delay(300);
        if (!psx.enterConfigMode()) {
          Serial.println(F("Cannot enter config mode"));
          psxContType = psx.getControllerType();
          Serial.print(F("Controller Type is: "));
          Serial.println(controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);
          psx.read();
        } else {
          psxContType = psx.getControllerType();
          Serial.print(F("Controller Type is: "));
          Serial.println(controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);

          if (!psx.enableAnalogSticks()) {
            Serial.println(F("Cannot enable analog sticks"));
          }
          if (!psx.enableAnalogButtons()) {
            Serial.println(F("Cannot enable analog buttons"));
          }
          if (!psx.exitConfigMode()) {
            Serial.println(F("Cannot exit config mode"));
          }
        }

        psx.read();
        psxStickMode = psx.getProtocol();
        Serial.print(F("Controller Protocol is: "));
        Serial.println(controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
        haveController = true;
      }
    }
  } else {
    if (!psx.read()) {
      digitalWrite(PIN_GOODLED, HIGH);
      Serial.println(F("Controller lost :x"));
      haveController = false;
      psxContType = PSCTRL_UNKNOWN;
      psxStickMode = PSPROTO_UNKNOWN;
      OldpsxStickMode = PSPROTO_UNKNOWN;
      stickMode = MODE_LOST;

      // 送信データのリセット
      xboxcontroller_reset();

    } else {
      digitalWrite(PIN_GOODLED, LOW);
      psxStickMode = psx.getProtocol();

      if (psxStickMode != OldpsxStickMode) {
        if (psxStickMode == PSPROTO_DIGITAL) stickMode = MODE_STD;
        if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
        if ((psxStickMode == PSPROTO_DUALSHOCK) || (psxStickMode == PSPROTO_DUALSHOCK2)) stickMode = MODE_STD;
        if ((psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) stickMode = restoreNegStickMode();
        if (psxStickMode == PSPROTO_JOGCON) {
          delay(300);
          if (psx.enterConfigMode()) {
            Serial.println(F("Set Jogcon Rumble"));
            if (!psx.enableJogconRumble()) {
              Serial.println(F("Cannot enable Jogcon Rumble"));
            }
            if (!psx.exitConfigMode()) {
              Serial.println(F("Cannot exit config mode"));
            }
          }
        }
        Serial.print(F("Controller Protocol is: "));
        Serial.println(controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
      }
      OldpsxStickMode = psxStickMode;

      // XInputデジタルボタン情報のみ毎フレームリセット（アナログ値は前回の値を保持）
      XboxButtonData.digital_buttons_1 = 0;
      XboxButtonData.digital_buttons_2 = 0;

      // 100ms単発クリックタイマーの処理
      if (menuOnUntil > 0) {
        if (now < menuOnUntil) {
          XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;
        } else {
          menuOnUntil = 0;
        }
      }

      // STARTメタキー判定（neGcon/Jogcon用）
      if (psxStickMode == PSPROTO_NEGCON || psxStickMode == PSPROTO_JOGCON) {
        uint16_t buttons = psx.getButtonWord();
        bool startPressed = (buttons & PSB_START) != 0;
        const uint16_t DIGITAL_BUTTONS_MASK = (PSB_PAD_UP | PSB_PAD_DOWN | PSB_PAD_LEFT | PSB_PAD_RIGHT | PSB_TRIANGLE | PSB_CIRCLE | PSB_R1);
        bool anyDigitalPressed = (buttons & DIGITAL_BUTTONS_MASK) != 0;

        switch (metaState) {
          case META_STATE_IDLE:
            if (startPressed) {
              if (anyDigitalPressed) {
                metaState = META_STATE_NORMAL_START;
              } else {
                metaState = META_STATE_START_PRESSED;
              }
            }
            break;

          case META_STATE_NORMAL_START:
            if (!startPressed) {
              metaState = META_STATE_IDLE;
            }
            break;

          case META_STATE_START_PRESSED:
            if (!startPressed) {
              menuOnUntil = millis() + 100;
              metaState = META_STATE_IDLE;
            } else if (anyDigitalPressed) {
              metaState = META_STATE_ACTIVE;
            }
            break;

          case META_STATE_ACTIVE:
            if (!startPressed && !anyDigitalPressed) {
              metaState = META_STATE_IDLE;
            }
            break;
        }
      } else {
        // neGcon/Jogcon以外のプロトコル時はメタ状態をIDLEにする
        metaState = META_STATE_IDLE;
      }

      switch (psxStickMode) {

        // SPCH-1110 FLIGHTSTICK mode (AirCombat22 mode) ボタン設定はdefaultのモノに最適化してあります
        case PSPROTO_FLIGHTSTICK:
          if (psx.getRightAnalog(lx_org, ly_org)) {
            l_x = lx_org;
            l_y = ly_org;
          } else {
            l_x = 0x80;
            l_y = 0x80;
          }

          if (psx.getLeftAnalog(rx_org, ry_org)) {
            r_x = rx_org;
            r_y = ry_org;
          } else {
            r_x = 0x80;
            r_y = 0x80;
          }

          if (l_x != slx || l_y != sly || r_x != srx || r_y != sry) {
            slx = l_x;
            sly = l_y;
            srx = r_x;
            sry = r_y;

            l_x = (byte)adjustXY(l_x, analogLxMax);
            l_y = (byte)adjustXY(l_y, analogLxMax);
            r_x = (byte)adjustXY(r_x, analogLxMax);
            r_y = (byte)adjustXY(r_y, analogLxMax);

            ledLx = l_x;
            ledLy = l_y;
            ledRx = r_x;
            ledRy = r_y;
          }

          // 軸の代入 (Xbox仕様にスケーリング)
          XboxButtonData.l_x = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
          XboxButtonData.l_y = (int16_t)(((int32_t)l_y - 128) * 32767 / -128); // 上がプラス
          XboxButtonData.r_x = (int16_t)(((int32_t)r_x - 128) * 32767 / 128);
          XboxButtonData.r_y = (int16_t)(((int32_t)r_y - 128) * 32767 / -128);

          // 最大角、設定モード
          if (stickMode == MODE_SETTING_NEG) {
            if (psx.getButtonWord() & PSB_START) {
              Serial.print(F("Analog lxMax before: "));
              Serial.println(analogLxMax);
              l_x_tmp = absoluteXY(lx_org);

              xy_tmp = absoluteXY(ly_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(rx_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(ry_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              if (l_x_tmp < (0x80 + 10)) l_x_tmp = 255;

              analogLxMax = (byte)l_x_tmp;
              Serial.println(F("EEP Write!"));
              EEPROM.write(EEPADR_ANALOG_STICKMAX, analogLxMax);
              EEPROM.commit();

              Serial.print(F("Analog lxMax after: "));
              Serial.println(analogLxMax);
              stickMode = beforeStickMode;
            }
          }

          // キー変換
          keyConvert_psx2xbox();
          break;


        // NeGconのボタン配置処理(リッジレーサモード)
        case PSPROTO_NEGCON:
          if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
            uint16_t buttons = psx.getButtonWord();

            if (metaState == META_STATE_ACTIVE) {
              // ---------------- メタ状態 ----------------
              if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;
              if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_GUIDE;
              if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;
              if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
              if (buttons & PSB_TRIANGLE)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;
              if (buttons & PSB_CIRCLE)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;
              if (buttons & PSB_R1)        { XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;
                                             XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER; }

              // I (Cross) = X
              if (buttons & PSB_CROSS)
                XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_X;

              // SELECT = BACK
              if (buttons & PSB_SELECT)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;
            } else {
              // ---------------- 通常状態 / NORMAL_START ----------------
              // ハットスイッチ (十字キー)
              if (metaState != META_STATE_START_PRESSED) { // START長押し中の誤出力を防ぐ
                if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_UP;
                if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_RIGHT;
              }

              // START = メニュー (START)
              if (metaState == META_STATE_NORMAL_START)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;

              // R (R1) = シェア (BACK)
              if (metaState != META_STATE_START_PRESSED) {
                if (buttons & PSB_R1)
                  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;
              }

              // I (Cross) = X
              if (buttons & PSB_CROSS)
                XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_X;

              // SELECT = BACK
              if (buttons & PSB_SELECT)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;

              // A (Triangle) / B (Circle) のマッピング (A/Bスワップ考慮)
              if (metaState != META_STATE_START_PRESSED) {
                if (stickMode == MODE_SWAPAB || stickMode == MODE_SWAPAB_SWAPLTRT) {
                  // AボタンとBボタンを入れ替える
                  if (buttons & PSB_TRIANGLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;
                  if (buttons & PSB_CIRCLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;
                } else {
                  // 標準アサイン
                  if (buttons & PSB_TRIANGLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;
                  if (buttons & PSB_CIRCLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;
                }
              }
            }
          } else {
            // 基本キー変換（その他の設定モード等用）
            keyConvert_psx2xbox();
          }

          if (psx.getLeftAnalog(lx_org, ly_org)) {
            # // I/II button NORMAL mode (スワップ処理はトリガーマッピング側で実施)
            b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
            b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);

            bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

            l_x = lx_org;
            l_y = ly_org;

            l_b1 = b1_org / 2;
            l_b2 = b2_org / 2;
            l_bL = bL_org / 2;

            if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
              digitalWrite(PIN_CONNECT, LOW);
              
              if (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
                // LTボタンとRTボタンを入れ替える (I = RT, L = LT)
                uint16_t raw_rt = (uint16_t)l_b1 * 2;
                XboxButtonData.rt = (raw_rt > 255) ? 255 : raw_rt;

                uint16_t raw_lt = (uint16_t)l_bL * 2;
                XboxButtonData.lt = (raw_lt > 255) ? 255 : raw_lt;
              } else {
                // 標準アサイン (I = LT, L = RT)
                uint16_t raw_lt = (uint16_t)l_b1 * 2;
                XboxButtonData.lt = (raw_lt > 255) ? 255 : raw_lt;

                uint16_t raw_rt = (uint16_t)l_bL * 2;
                XboxButtonData.rt = (raw_rt > 255) ? 255 : raw_rt;
              }
            }

            if (l_x != slx || l_b1 != sb1 || l_b2 != sb2 || l_bL != sbL) {
              slx = l_x;
              sb1 = l_b1;
              sb2 = l_b2;
              sbL = l_bL;

              l_x = (byte)adjustXY(l_x, lxMax);

              l_b1 = l_b1 * NEG_CALIB_B;
              if (l_b1 > 0x80) l_b1 = 0x80;
              l_b2 = l_b2 * NEG_CALIB_B;
              if (l_b2 > 0x7f) l_b2 = 0x7f;
              l_bL = l_bL * 1;
              if (l_bL > 0x7f) l_bL = 0x7f;

              ledLx = l_x;
              ledB1 = b1_org;
              ledB2 = b2_org;
              ledBL = bL_org;
            }

            // 最終的な値をセット (ハンドル)
            XboxButtonData.l_x = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
            XboxButtonData.l_y = 0;

            // 最大角、設定モード
            if (stickMode == MODE_SETTING_NEG) {
              if (psx.getButtonWord() & PSB_START) {
                Serial.print(F("neG lxMax before: "));
                Serial.println(lxMax);
                l_x_tmp = absoluteXY(lx_org);

                if (l_x_tmp < (0x80 + 10)) l_x_tmp = 0xff / ((NEG_CALIB - 1) / 2 + 1);

                lxMax = (byte)l_x_tmp;
                Serial.println(F("EEP Write!"));
                EEPROM.write(EEPADR_NEG_NEGMAX, lxMax);
                EEPROM.commit();

                Serial.print(F("neG lxMax after: "));
                Serial.println(lxMax);
                stickMode = beforeStickMode;
              }
            }
          }
          break;


        // Jogcon
        case PSPROTO_JOGCON:
          {
            uint16_t buttons = psx.getButtonWord();
            // I (CROSS) と II (SQUARE) のデジタル入力をスワップ
            uint16_t swapped_buttons = buttons & ~(PSB_CROSS | PSB_SQUARE);
            if (buttons & PSB_CROSS) swapped_buttons |= PSB_SQUARE;
            if (buttons & PSB_SQUARE) swapped_buttons |= PSB_CROSS;
            buttons = swapped_buttons;

            const uint16_t DIGITAL_BUTTONS_MASK = (PSB_PAD_UP | PSB_PAD_DOWN | PSB_PAD_LEFT | PSB_PAD_RIGHT | PSB_TRIANGLE | PSB_CIRCLE | PSB_R1);

            if (metaState == META_STATE_ACTIVE) {
              // ---------------- メタ状態 ----------------
              if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;
              if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_GUIDE;
              if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;
              if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
              if (buttons & PSB_TRIANGLE)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;
              if (buttons & PSB_CIRCLE)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;
              if (buttons & PSB_R1)        { XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START ;
                                             XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER; }

              // メタ状態に関与しない他のボタンは、メタ対象ボタンおよびSTARTを除外したボタンワードで処理する
              uint16_t other_buttons = buttons & ~(DIGITAL_BUTTONS_MASK | PSB_START);
              keyConvert_psx2xbox_ex(other_buttons);
            } else {
              // ---------------- 通常状態 / NORMAL_START / START_PRESSED ----------------
              uint16_t masked_buttons = buttons;

              // START_PRESSED時は、十字キー、A/B、R1の入力を抑止する
              if (metaState == META_STATE_START_PRESSED) {
                masked_buttons &= ~DIGITAL_BUTTONS_MASK;
              }

              // STARTボタンの反映制御
              if (metaState == META_STATE_NORMAL_START) {
                masked_buttons |= PSB_START;
              } else {
                masked_buttons &= ~PSB_START;
              }

              keyConvert_psx2xbox_ex(masked_buttons);

              // A/Bスワップ処理の適用
              if (stickMode == MODE_SWAPAB || stickMode == MODE_SWAPAB_SWAPLTRT) {
                if (metaState != META_STATE_START_PRESSED) {
                  uint8_t a_pressed = XboxButtonData.digital_buttons_2 & XINPUT_GAMEPAD_A;
                  uint8_t b_pressed = XboxButtonData.digital_buttons_2 & XINPUT_GAMEPAD_B;
                  XboxButtonData.digital_buttons_2 &= ~(XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B);
                  if (a_pressed) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;
                  if (b_pressed) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;
                }
              }
            }
          }

          if (psx.getJogConAnalog(jogx)) {
            // MAX値を超えた場合のForceback処理
            jogxForcePower = 0;
            if (stickMode != MODE_SETTING_NEG) {
              if (jogcon_abs_val(jogx) > jogconDialMax + JOG_MAX_AJST) {
                jogxForcePower = (jogcon_abs_val(jogx) - (jogconDialMax + JOG_MAX_AJST)) / 2;
                if (jogxForcePower > 0x0f) jogxForcePower = 0x0f;
              }
              if (jogxForcePower == 0) {
                psx.setJogCommand(0x00);
              } else {
                if (jogx < 0)
                  psx.setJogCommand(0x10 | jogxForcePower & 0x0f);
                else
                  psx.setJogCommand(0x20 | (jogxForcePower & 0x0f));
              }
            }

            // ダイヤル回転を LスティックX軸 にマッピング
            int32_t calc_lx = (int32_t)32767 * jogx / jogconDialMax;
            if (calc_lx > 32767) calc_lx = 32767;
            if (calc_lx < -32768) calc_lx = -32768;
            l_x = 0x80 + (byte)(0x80 * jogx / jogconDialMax);
            if (jogcon_abs_val(jogx) >= jogconDialMax) l_x = (jogx < 0) ? 0x00 : 0xff;

            b1_org = 0x00;
            b2_org = 0x00;

            if (psx.getButtonWord() & PSB_SQUARE) b1_org = 0xff;
            if (psx.getButtonWord() & PSB_CROSS) b2_org = 0xff;

            l_b1 = b1_org / 2;
            l_b2 = b2_org / 2;

            if (l_x != slx || l_b1 != sb1 || l_b2 != sb2 || l_bL != sbL) {
              slx = l_x;
              sb1 = l_b1;
              sb2 = l_b2;
              sbL = l_bL;

              ledLx = l_x;
              ledB1 = 0x00;
              ledB2 = b2_org;
              ledBL = bL_org;
            }

            // 最終的な値をセット (ハンドル)
            XboxButtonData.l_x = (int16_t)calc_lx;
            XboxButtonData.l_y = 0;

            // ゲームモードが有効な場合アナログ値でアクセル・ブレーキが設定可能 (スワップ考慮)
            if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
              if (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
                // LT/RT スワップ (I = RT, II = LT)
                uint16_t raw_rt = (uint16_t)l_b1 * 2;
                uint16_t raw_lt = (uint16_t)l_b2 * 2;
                XboxButtonData.rt = (raw_rt > 255) ? 255 : raw_rt;
                XboxButtonData.lt = (raw_lt > 255) ? 255 : raw_lt;
              } else {
                // 標準アサイン (I = LT, II = RT)
                uint16_t raw_lt = (uint16_t)l_b1 * 2;
                uint16_t raw_rt = (uint16_t)l_b2 * 2;
                XboxButtonData.lt = (raw_lt > 255) ? 255 : raw_lt;
                XboxButtonData.rt = (raw_rt > 255) ? 255 : raw_rt;
              }
            }

            // 最大角、設定モード
            if (stickMode == MODE_SETTING_NEG) {
              if (jogxPosResetEnable < 100) {
                if (jogcon_abs_val(jogx) > 4) {
                  jogxPosResetEnable = 0;
                  jogxForcePower = (jogcon_abs_val(jogx) / 2) + 1;
                  if (jogxForcePower > 0x0f) jogxForcePower = 0x0f;

                  if (jogx < 0) {
                    psx.setJogCommand(0x10 | jogxForcePower);
                  } else {
                    psx.setJogCommand(0x20 | jogxForcePower);
                  }
                } else {
                  jogxPosResetEnable++;
                  psx.setJogCommand(0x00);
                }
              } else {
                if (psx.getButtonWord() & PSB_START) {
                  Serial.print(F("Jogcon jogconDialMax before: "));
                  Serial.println(jogconDialMax);
                  jogconDialMax = jogcon_abs_val(jogx);
                  if (jogconDialMax <= 8) jogconDialMax = 100;

                  Serial.println(F("EEP Write!"));
                  EEPROM.write(EEPADR_JOG_MAX_U, (byte)(jogconDialMax >> 8));
                  EEPROM.write(EEPADR_JOG_MAX_L, (byte)(jogconDialMax & 0x00ff));
                  EEPROM.commit();

                  Serial.print(F("Jogcon jogconDialMax after: "));
                  Serial.println(jogconDialMax);
                  stickMode = beforeStickMode;
                }
              }
            } else {
              jogxPosResetEnable = 0;
            }
          }
          break;


        // Dual Shock/2のボタン配置
        case PSPROTO_DUALSHOCK2:
        case PSPROTO_DUALSHOCK:
          keyConvert_psx2xbox();

          if (psx.getLeftAnalog(lx_org, ly_org)) {
            l_x = lx_org;
            l_y = ly_org;
          } else {
            l_x = 0x80;
            l_y = 0x80;
          }

          if (psx.getRightAnalog(rx_org, ry_org)) {
            r_x = rx_org;
            r_y = ry_org;
          } else {
            r_x = 0x80;
            r_y = 0x80;
          }

          if (l_x != slx || l_y != sly || r_x != srx || r_y != sry) {
            slx = l_x;
            sly = l_y;
            srx = r_x;
            sry = r_y;
            ledLx = l_x;
            ledLy = l_y;
            ledRx = r_x;
            ledRy = r_y;
          }

          // 軸の代入 (Xbox仕様にスケーリング)
          XboxButtonData.l_x = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
          XboxButtonData.l_y = (int16_t)(((int32_t)l_y - 128) * 32767 / -128); // 上がプラス
          XboxButtonData.r_x = (int16_t)(((int32_t)r_x - 128) * 32767 / 128);
          XboxButtonData.r_y = (int16_t)(((int32_t)r_y - 128) * 32767 / -128);
          break;


        default:
          keyConvert_psx2xbox();
          break;
      }

      // XInputデータ送信
      xboxcontroller_send_report();
    }
  }

  xboxcontroller_send_report(); // 毎ループ確実に最新データをPCへ送信（認識の常時維持）
  tud_task(); // TinyUSBデバイススタックのタスク処理を巡回させる
  delay(1);
}
