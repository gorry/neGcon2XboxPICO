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

#include "hardware/watchdog.h"

#include <SPI.h>
#include "PsxControllerPICO_HwSpi2.h"
#include "XboxControllerPico.h"
DummySerial Serial_Dummy;
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <FatFS.h>
#include "InifileTemplate.h"

extern uint8_t _FS_start;
extern uint8_t _FS_end;
bool fs_ready = false;
volatile bool flash_busy = false;
volatile bool setup_done = false;

#if !defined(CFG_TUD_MSC) || (CFG_TUD_MSC == 0)
#error "CFG_TUD_MSC is not defined or is 0! TinyUSB MSC stack is disabled!"
#endif

// 前方宣言
void saveConfig();
void loadConfig();
void migrateOrInitConfig();

// ConfigEEPROMWrapperの定義 (本物の EEPROM を使用)
static auto& RealEEPROM = ::EEPROM;

class ConfigEEPROMWrapper {
public:
  void begin(size_t size) { RealEEPROM.begin(size); }
  uint8_t read(int address) { return RealEEPROM.read(address); }
  void write(int address, uint8_t value) { RealEEPROM.write(address, value); }
  bool commit();
};
static ConfigEEPROMWrapper ConfigEEPROM;

// 既存コードの EEPROM 呼び出しをラッパーに置き換える
#define EEPROM ConfigEEPROM

// EEPROMの設定

// EEPROMの設定
#define EEPROMSIZE 256
// EEPROMのアドレス配置
#define EEPADR_NEGMODE 3
#define EEPADR_NEG_NEGMAX 4
#define EEPADR_ANALOG_STICKMAX 5
#define EEPADR_JOG_MAX_U 6
#define EEPADR_JOG_MAX_L 7
#define EEPADR_NEG_REDUCE_HANDLE_PLAY 8
#define EEPADR_NEG_ANALOG_LT_CURVE 9
#define EEPADR_NEG_ANALOG_RT_CURVE 10
#define EEPADR_BOOT_ATTEMPT_COUNT 15

// アナログカーブの定義
#define NEG_ANALOG_CURVE_LINEAR 0
#define NEG_ANALOG_CURVE_A1     1
#define NEG_ANALOG_CURVE_A2     2
#define NEG_ANALOG_CURVE_A3     3
#define NEG_ANALOG_CURVE_MAX    4

// neGcon ハンドルアナログ値の補正 これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB 1.0

// neGcon ボタン アナログ値の補正 これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB_B 1.0

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

#define MODE_USB_MSC 97
#define MODE_SETTING_NEG 98
#define MODE_LOST 99

#define MAX_NEG_REDUCE_HANDLE_PLAY 32

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

volatile unsigned long last_msc_access_time = 0;
volatile uint8_t msc_access_type = 0; // 0: なし, 1: 読み込み(青), 2: 書き込み(赤)
volatile bool msc_active_connected = false; // ホストとの通信が開始されたか

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

// デフォルト設定値の定義
const byte DEFAULT_NEG_LT_CURVE = NEG_ANALOG_CURVE_LINEAR; // 0
const byte DEFAULT_NEG_RT_CURVE = NEG_ANALOG_CURVE_LINEAR; // 0
const byte DEFAULT_NEG_REDUCE_HANDLE_PLAY = 16;            // EEPROMゲタなし16 (EEPROM書き込み時は+1して17)
const byte DEFAULT_NEG_TWIST_MAX = 242;                    // 255 / ((NEG_CALIB - 1) / 2 + 1)
const short DEFAULT_JOGCON_DIAL_MAX = 100;
const byte DEFAULT_STICK_MODE = MODE_STD;                  // 0
const byte DEFAULT_ANALOG_LX_MAX = 255;

// STARTメタキー機能のための状態定義
enum MetaKeyState {
  META_STATE_IDLE = 0,
  META_STATE_START_PRESSED,
  META_STATE_ACTIVE,
  META_STATE_NORMAL_START
};
static MetaKeyState metaState = META_STATE_IDLE;
static unsigned long menuOnUntil = 0;

/// --- グローバル変数 (loop内のstatic変数を昇格) ---
static unsigned long lastPsxReadTime = 0;
static unsigned long lastSearchTime = 0; // psx.begin() の間隔制限用
static int sBootSel = 0;
static byte beforeStickMode;
static bool changeNegStickMode;
static byte slx = 0, sly = 0, sb1 = 0, sb2 = 0, sbL = 0;
static byte srx = 0, sry = 0;
static short sjogx = 0;
static byte jogxForcePower = 0;
static byte jogxPosResetEnable = 0;
static PsxControllerType psxContType = PSCTRL_UNKNOWN;
static PsxControllerProtocol OldpsxStickMode = PSPROTO_UNKNOWN;
static byte ledLx, ledLy, ledRx, ledRy, ledB1, ledB2, ledBL;
static byte stickMode = DEFAULT_STICK_MODE;
static byte lxMax = DEFAULT_NEG_TWIST_MAX;
static byte analogLxMax = DEFAULT_ANALOG_LX_MAX;
static short jogconDialMax = DEFAULT_JOGCON_DIAL_MAX;
static byte negReduceHandlePlay = DEFAULT_NEG_REDUCE_HANDLE_PLAY;
static byte negLtCurve = DEFAULT_NEG_LT_CURVE;
static byte negRtCurve = DEFAULT_NEG_RT_CURVE;
static volatile int ledFlashCount = 0;

/// <summary>
/// EEPROMの初期設定と全領域のクリア
/// </summary>
void eepromFormat() {
  Serial.printf("[%lu] EEP Write!\n", millis());
  for (int i = 0; i < EEPROMSIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.write(0, 'c');
  EEPROM.write(1, 'f');
  EEPROM.write(2, '1');
  EEPROM.write(EEPADR_NEGMODE, DEFAULT_STICK_MODE);
  EEPROM.write(EEPADR_NEG_NEGMAX, DEFAULT_NEG_TWIST_MAX);
  EEPROM.write(EEPADR_ANALOG_STICKMAX, DEFAULT_ANALOG_LX_MAX);
  EEPROM.write(EEPADR_JOG_MAX_U, (byte)(DEFAULT_JOGCON_DIAL_MAX >> 8));
  EEPROM.write(EEPADR_JOG_MAX_L, (byte)(DEFAULT_JOGCON_DIAL_MAX & 0xff));
  EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, DEFAULT_NEG_REDUCE_HANDLE_PLAY + 1); // ゲタ（+1）をはかせて保存
  EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, DEFAULT_NEG_LT_CURVE);
  EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, DEFAULT_NEG_RT_CURVE);
  EEPROM.commit();
}

/// <summary>
/// neGconのアナログLT感度カーブの設定値を復元
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegLtCurve() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_ANALOG_LT_CURVE);
  if (tmp >= NEG_ANALOG_CURVE_MAX) {
    tmp = DEFAULT_NEG_LT_CURVE;
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, tmp);
    EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// neGconのアナログRT感度カーブの設定値を復元
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegRtCurve() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_ANALOG_RT_CURVE);
  if (tmp >= NEG_ANALOG_CURVE_MAX) {
    tmp = DEFAULT_NEG_RT_CURVE;
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, tmp);
    EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// 感度カーブを適用してアナログ値を補正
/// </summary>
/// <param name="value">補正前のアナログ値 (0-255)</param>
/// <param name="curveType">カーブの種類 (NEG_ANALOG_CURVE_LINEAR ~ A3)</param>
/// <returns>補正後のアナログ値 (0-255)</returns>
byte applyAnalogCurve(byte value, byte curveType) {
  if (curveType == NEG_ANALOG_CURVE_LINEAR) {
    return value;
  }
  
  float p = 1.0f;
  if (curveType == NEG_ANALOG_CURVE_A1) p = 2.0f;
  else if (curveType == NEG_ANALOG_CURVE_A2) p = 4.0f;
  else if (curveType == NEG_ANALOG_CURVE_A3) p = 6.0f;
  
  float val_norm = (float)value / 255.0f;
  float val_curved = powf(val_norm, p);
  int result = (int)(val_curved * 255.0f + 0.5f);
  if (result > 255) result = 255;
  if (result < 0) result = 0;
  
  return (byte)result;
}

/// <summary>
/// EEPROMのフォーマット状態を確認
/// </summary>
/// <returns>有効な設定データが存在すれば true、存在しなければ false</returns>
bool eepromCheck() {
  if (EEPROM.read(0) != 'c') return false;
  if (EEPROM.read(1) != 'f') return false;
  if (EEPROM.read(2) != '1') return false;
  return true;
}

/// <summary>
/// neGconひねり量の最大キャリブレーション値を復元
/// </summary>
/// <returns>復元された最大キャリブレーション値 (0x80-255)</returns>
byte restoreNegDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_NEGMAX);

  if (tmp < 0x80) {
    tmp = DEFAULT_NEG_TWIST_MAX;
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.write(EEPADR_NEG_NEGMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

/// <summary>
/// Jogconダイヤルの最大回転キャリブレーション値を復元
/// </summary>
/// <returns>復元された最大キャリブレーション値 (8-500)</returns>
short restorejogMax() {
  short tmp;
  tmp = (short)EEPROM.read(EEPADR_JOG_MAX_U);
  tmp = (tmp << 8) | (short)EEPROM.read(EEPADR_JOG_MAX_L);

  if (tmp < 0x10) {
    tmp = DEFAULT_JOGCON_DIAL_MAX;
    EEPROM.write(EEPADR_JOG_MAX_U, (byte)(tmp >> 8));
    EEPROM.write(EEPADR_JOG_MAX_L, (byte)(tmp & 0xff));
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.commit();
  }

  return tmp;
}

/// <summary>
/// 通常アナログスティックの最大キャリブレーション値を復元
/// </summary>
/// <returns>復元された最大キャリブレーション値 (128-255)</returns>
byte restoreAnaDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_ANALOG_STICKMAX);

  if (tmp < 0x80) {
    tmp = DEFAULT_ANALOG_LX_MAX;
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.write(EEPADR_ANALOG_STICKMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

/// <summary>
/// コントローラーのボタン配置モード設定を復元
/// </summary>
/// <returns>復元されたモードインデックス (MODE_STD ~ AIRCON22)</returns>
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
      tmp = DEFAULT_STICK_MODE;
      Serial.printf("[%lu] EEP Write!\n", millis());
      EEPROM.write(EEPADR_NEGMODE, tmp);
      EEPROM.commit();
      break;
  }

  return tmp;
}

/// <summary>
/// neGconハンドル（ひねり）の遊び削減値を復元
/// </summary>
/// <returns>復元された遊び削減値 (0-32)</returns>
byte restoreNegReduceHandlePlay() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_REDUCE_HANDLE_PLAY);

  // ゲタ（+1）を考慮し、未設定（0）または範囲外（MAX_NEG_REDUCE_HANDLE_PLAY+1より大きい）なら初期値DEFAULT_NEG_REDUCE_HANDLE_PLAY（ゲタありで+1）にする
  if (tmp == 0 || tmp > MAX_NEG_REDUCE_HANDLE_PLAY+1) {
    tmp = DEFAULT_NEG_REDUCE_HANDLE_PLAY + 1;
    Serial.printf("[%lu] EEP Write!\n", millis());
    EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, tmp);
    EEPROM.commit();
  }

  return tmp - 1; // ゲタ（-1）をはずして 0-32 の実際の値を返す
}

/// <summary>
/// XInputレポートをPCへ送信
/// </summary>
void send_xbox_report() {
  if (stickMode == MODE_SETTING_NEG) {
    xboxcontroller_reset();
  }
  xboxcontroller_send_report();
}

/// <summary>
/// アナログ値のセンター(0x80)からの絶対偏差を計算
/// </summary>
/// <param name="lx">対象のアナログ値 (0-255)</param>
/// <returns>0x80からの絶対偏差値</returns>
int absoluteXY(byte lx) {
  int lx_tmp;
  if (lx < 0x80) lx_tmp = (int)(0xFF - lx);
  else lx_tmp = (int)lx;
  return lx_tmp;
}

/// <summary>
/// short型値の絶対値を計算
/// </summary>
/// <param name="x">入力値</param>
/// <returns>入力値の絶対値</returns>
short jogcon_abs_val(short x) {
  return x < 0 ? -x : x;
}

/// <summary>
/// 最大キャリブレーション値に基づいてアナログスティック範囲を補正
/// アナログ値はすべて0x80をセンターとした値
/// </summary>
/// <param name="lx">補正対象のアナログ値</param>
/// <param name="max">最大キャリブレーション値</param>
/// <returns>0-255の範囲にスケーリングされた補正後のアナログ値</returns>
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
/// <summary>
/// PSXボタン入力をXInputボタン入力データへ変換 (拡張版)
/// </summary>
/// <param name="buttons">PSXコントローラーから読み取ったボタンワード</param>
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

/// <summary>
/// 現在取得しているPSXボタン入力をXInputボタン入力データへ変換
/// </summary>
void keyConvert_psx2xbox() {
  keyConvert_psx2xbox_ex(psx.getButtonWord());
}

/// <summary>
/// Core 1 (サブコア) の初期化処理
/// </summary>
void setup1() {  // core 0
  while (!setup_done) {
    delay(50);
  }
}

/// <summary>
/// Core 1 (サブコア) のメインループ
/// </summary>
void loop1() {  // core 0
  if (flash_busy) {
    delay(100);
    return;
  }

  if (usb_mode_msc) {
    // アクセスランプ処理（最後の読み書きから100ms未満は優先表示）
    if (msc_access_type != 0 && (millis() - last_msc_access_time < 100)) {
      if (msc_access_type == 1) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // 読み込み：青
      } else {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // 書き込み：赤
      }
      pixels.show();
      delay(10);
      return;
    }

    msc_access_type = 0; // 100ms以上アクセスが途切れたら状態をクリア
    unsigned long period = msc_active_connected ? 2000 : 250;
    unsigned long half_period = period / 2;
    unsigned long t = millis() % period;
    int raw_brightness;
    if (t < half_period) {
      raw_brightness = (t * 127) / half_period;
    } else {
      raw_brightness = ((period - t) * 127) / half_period;
    }
    byte brightness = raw_brightness; // 0〜127の範囲でなだらかに明滅
    pixels.setPixelColor(0, pixels.Color(0, brightness, brightness));
    pixels.show();
    delay(20);
    return;
  }
  static byte heartbeat_num = 0;
  static bool heartbeat_flag = true;
  const byte heartbeat_speed = 32;

  if (ledFlashCount > 0) {
    byte r_val = (negReduceHandlePlay == 32) ? 255 : (negReduceHandlePlay * 8);
    byte g_val = ledLx;
    byte b_val = heartbeat_num;

    // 点灯 (0.1秒)
    pixels.setPixelColor(0, pixels.Color(r_val, g_val, b_val));
    pixels.show();
    delay(100);

    // 消灯 (0.1秒)
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(100);

    ledFlashCount--;
    return;
  }

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
      {
        // heartbeat_num で B値（青）を明滅、ねじり値でG値（緑）を明滅、negReduceHandlePlay に比例した固定輝度で R値（赤）を点灯
        // 32 * 8 = 256 となり byte (0-255) からオーバーフローして 0 に戻るのを防ぐため、32の時は255にする
        byte r_val = (negReduceHandlePlay == 32) ? 255 : (negReduceHandlePlay * 8);
        byte g_val = ledLx;
        pixels.setPixelColor(0, pixels.Color(r_val, g_val, heartbeat_num));
        pixels.show();
      }
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

// =========================================================================
// CONFIG.INI ファイル処理の実装
// =========================================================================

bool ConfigEEPROMWrapper::commit() {
  if (!fs_ready) {
    uint32_t ints = save_and_disable_interrupts();
    bool res = RealEEPROM.commit();
    restore_interrupts(ints);
    return res;
  }
  // ファイルシステム動作時はEEPROM.commit()を実行せずフリーズを防止
  saveConfig();
  return true;
}

/// <summary>
/// 設定パラメータを /CONFIG.INI ファイルへ書き出し保存
/// </summary>
void saveConfig() {
  if (!fs_ready) return;
  flash_busy = true; // Core 1 の動作を一時停止
  delay(10);
  config_file_writing = true;
  File file = FatFS.open("/CONFIG.INI", "w");
  if (!file) {
    config_file_writing = false;
    flash_busy = false;
    return;
  }

  // テンプレート関数を使って設定値を CONFIG.INI に書き出し
  writeConfigIni(file, negLtCurve, negRtCurve, negReduceHandlePlay, lxMax, jogconDialMax, stickMode, analogLxMax);

  file.close();
  config_file_writing = false;
  flash_busy = false; // Core 1 の動作を再開
}

/// <summary>
/// /CONFIG.INI ファイルまたは EEPROM から設定パラメータをロード
/// </summary>
void loadConfig() {

  if (!fs_ready) {
    // FATFS が使えない場合は、本物の EEPROM からロードする
    if (eepromCheck() != true) {
      eepromFormat();
    }
    negLtCurve = restoreNegLtCurve();
    negRtCurve = restoreNegRtCurve();
    negReduceHandlePlay = restoreNegReduceHandlePlay();
    lxMax = restoreNegDegMax();
    analogLxMax = restoreAnaDegMax();
    jogconDialMax = restorejogMax();
    stickMode = restoreNegStickMode();
    return;
  }

  // ファイルが存在しない場合は、まず作成する (無限再帰を防ぐ)
  if (!FatFS.exists("/CONFIG.INI")) {
    migrateOrInitConfig(); // ここで初期ファイルが作成される
  }

  File file = FatFS.open("/CONFIG.INI", "r");
  if (!file) {
    // それでもファイルが開けない場合は、本物の EEPROM からロードして諦める (再帰はしない)
    if (eepromCheck() != true) {
      eepromFormat();
    }
    negLtCurve = restoreNegLtCurve();
    negRtCurve = restoreNegRtCurve();
    negReduceHandlePlay = restoreNegReduceHandlePlay();
    lxMax = restoreNegDegMax();
    analogLxMax = restoreAnaDegMax();
    jogconDialMax = restorejogMax();
    stickMode = restoreNegStickMode();
    return;
  }

  // 安全弁：破損などによりファイルサイズが異常に大きい、または小さすぎる場合は削除してフォールバック
  if (file.size() < 50 || file.size() > 4096) {
    file.close();
    FatFS.remove("/CONFIG.INI");
    if (eepromCheck() != true) {
      eepromFormat();
    }
    negLtCurve = restoreNegLtCurve();
    negRtCurve = restoreNegRtCurve();
    negReduceHandlePlay = restoreNegReduceHandlePlay();
    lxMax = restoreNegDegMax();
    analogLxMax = restoreAnaDegMax();
    jogconDialMax = restorejogMax();
    stickMode = restoreNegStickMode();
    return;
  }

  bool updated = false;
  int loopCount = 0;
  while (file.available()) {
    loopCount++;
    String line = file.readStringUntil('\n');

    line.trim();
    if (line.startsWith(";") || line.startsWith("[") || line.length() == 0) {
      continue;
    }
    int eqIdx = line.indexOf('=');
    if (eqIdx > 0) {
      String key = line.substring(0, eqIdx);
      String val = line.substring(eqIdx + 1);
      key.trim();
      val.trim();

      int intVal = val.toInt();
      if (key == "negLtCurve" && negLtCurve != (byte)intVal) {
        negLtCurve = (byte)intVal;
        updated = true;
      } else if (key == "negRtCurve" && negRtCurve != (byte)intVal) {
        negRtCurve = (byte)intVal;
        updated = true;
      } else if (key == "negReduceHandlePlay" && negReduceHandlePlay != (byte)intVal) {
        negReduceHandlePlay = (byte)intVal;
        updated = true;
      } else if (key == "negTwistMax" && lxMax != (byte)intVal) {
        lxMax = (byte)intVal;
        updated = true;
      } else if (key == "jogconDialMax" && jogconDialMax != (short)intVal) {
        jogconDialMax = (short)intVal;
        updated = true;
      } else if (key == "stickMode" && stickMode != (byte)intVal) {
        stickMode = (byte)intVal;
        updated = true;
      } else if (key == "analogLxMax" && analogLxMax != (byte)intVal) {
        analogLxMax = (byte)intVal;
        updated = true;
      }
    }

    // 無限ループ対策
    if (loopCount > 100) {
      break;
    }
  }
  file.close();

  // ファイルシステム読み込み完了後、既存コード内のEEPROM.read()との互換性のためRAMバッファにミラー同期しておく
  if (fs_ready) {
    ::EEPROM.write(EEPADR_NEGMODE, stickMode);
    ::EEPROM.write(EEPADR_NEG_NEGMAX, lxMax);
    ::EEPROM.write(EEPADR_ANALOG_STICKMAX, analogLxMax);
    ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(jogconDialMax >> 8));
    ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(jogconDialMax & 0x00ff));
    ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, negReduceHandlePlay + 1); // ゲタ（+1）
    ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, negLtCurve);
    ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, negRtCurve);
  }

  // フェールセーフとして、PCから書き換えられた値をEEPROMバックアップ側にも同期保存する
  // (ただし、ファイルシステムが無効なフォールバック時のみ実行し、ファイルシステム動作時はフリーズ防止のため実行しない)
  if (updated && !fs_ready) {
    uint32_t ints = save_and_disable_interrupts();
    ::EEPROM.write(0, 'c');
    ::EEPROM.write(1, 'f');
    ::EEPROM.write(2, '1');
    ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, negLtCurve);
    ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, negRtCurve);
    ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, negReduceHandlePlay + 1); // ゲタ（+1）
    ::EEPROM.write(EEPADR_NEG_NEGMAX, lxMax);
    ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(jogconDialMax >> 8));
    ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(jogconDialMax & 0x00ff));
    ::EEPROM.commit();
    restore_interrupts(ints);
  }
}

/// <summary>
/// EEPROM設定を CONFIG.INI へ移行、またはテンプレートから初期生成
/// </summary>
void migrateOrInitConfig() {
  if (!fs_ready) return;

  if (eepromCheck() == true) {
    negLtCurve = restoreNegLtCurve();
    negRtCurve = restoreNegRtCurve();
    negReduceHandlePlay = restoreNegReduceHandlePlay();
    lxMax = restoreNegDegMax();
    analogLxMax = restoreAnaDegMax();
    jogconDialMax = restorejogMax();
    stickMode = restoreNegStickMode();
    saveConfig();
  } else {
    // 無ければ CONFIG.INI をテンプレートからデフォルト値で生成
    File file = FatFS.open("/CONFIG.INI", "w");
    if (file) {
      // デフォルトの設定値を渡して初期ファイルを生成
      writeConfigIni(file, 
                     DEFAULT_NEG_LT_CURVE, 
                     DEFAULT_NEG_RT_CURVE, 
                     DEFAULT_NEG_REDUCE_HANDLE_PLAY, 
                     DEFAULT_NEG_TWIST_MAX, 
                     DEFAULT_JOGCON_DIAL_MAX, 
                     DEFAULT_STICK_MODE, 
                     DEFAULT_ANALOG_LX_MAX);
      file.close();
    }
  }
}

/// <summary>
/// Core 0 (メインコア) の初期化処理
/// </summary>
void setup() {
  // 最初に NeoPixel を起動し、Core 0 側で初期化する
  pinMode(NEO_PWR, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // 紫：ブート初期化中
  pixels.show();

  Serial.begin(115200);
  delay(500); // シリアルが安定するのを少し待つ
  Serial.printf("\n[%lu] --- setup start ---\n", millis());

  // (セットアップ中の他コア一時停止は個別関数内のみに縮小)

  // スクラッチレジスタによる起動モード判定
  uint32_t boot_magic = watchdog_hw->scratch[0];
  watchdog_hw->scratch[0] = 0; // 読み取ったら即座にクリアしてループを防ぐ
  Serial.printf("[%lu] boot_magic = 0x%08X\n", millis(), boot_magic);
  if (boot_magic == 0xAB0057C0) {
    usb_mode_msc = true;
    msc_active_connected = false;
    stickMode = MODE_USB_MSC;
    Serial.printf("[%lu] Mode: USB MSC (Mass Storage Class)\n", millis());
  } else {
    usb_mode_msc = false;
    Serial.printf("[%lu] Mode: Normal Controller (XInput)\n", millis());
  }

  // 安全に USB XInput を初期化（No USBモードの TinyUSB 起動）
  xboxcontroller_init();
  xboxcontroller_reset();

  // 本物の EEPROM の初期化（移行チェック用）
  RealEEPROM.begin(EEPROMSIZE);

  // 起動試行カウンタによる安全フォーマット機能
  byte attempts = RealEEPROM.read(EEPADR_BOOT_ATTEMPT_COUNT);
  if (attempts == 255) attempts = 0; // 初期値（未初期化）なら0にする
  attempts++;
  RealEEPROM.write(EEPADR_BOOT_ATTEMPT_COUNT, attempts);
  RealEEPROM.commit();
  Serial.printf("[%lu] Setup attempt count: %u\n", millis(), attempts);

  if (attempts >= 5) {
    Serial.printf("[%lu] Too many setup failures! Formatting Flash FS...\n", millis());
    for (int i = 0; i < 10; i++) {
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); pixels.show(); delay(100);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0)); pixels.show(); delay(100);
    }
    FatFS.format();
    RealEEPROM.write(EEPADR_BOOT_ATTEMPT_COUNT, 0);
    RealEEPROM.commit();
    Serial.printf("[%lu] Format done. Reconnect...\n", millis());
    xboxcontroller_reconnect(false);
  }

  // FATファイルシステムのマウント (FSサイズが0の場合はスキップしてハングアップを回避)
  fs_ready = false;
  uint32_t fs_size = (uint32_t)&_FS_end - (uint32_t)&_FS_start;
  Serial.printf("[%lu] FS Start: 0x%08X, End: 0x%08X, Size: %u bytes\n", millis(), (uint32_t)&_FS_start, (uint32_t)&_FS_end, fs_size);
  if (fs_size >= 65536) {
    Serial.printf("[%lu] Calling FatFS.begin()...\n", millis());
    if (FatFS.begin()) {
      Serial.printf("[%lu] FatFS.begin() SUCCESS.\n", millis());
      fs_ready = true;
    } else {
      Serial.printf("[%lu] FatFS.begin() FAILED. Formatting...\n", millis());
      if (FatFS.format()) {
        Serial.printf("[%lu] FatFS.format() SUCCESS. Retrying begin()...\n", millis());
        if (FatFS.begin()) {
          Serial.printf("[%lu] FatFS.begin() SUCCESS after format.\n", millis());
          fs_ready = true;
        } else {
          Serial.printf("[%lu] FatFS.begin() FAILED after format.\n", millis());
        }
      } else {
        Serial.printf("[%lu] FatFS.format() FAILED.\n", millis());
      }
    }
  } else {
    Serial.printf("[%lu] FS size is too small, skipping FatFS.begin().\n", millis());
  }

  // 設定ファイルのロード (通常コントローラーモード時のみロードし、MSCモード時はスキップしてフリーズを防ぐ)
  if (!usb_mode_msc) {
    loadConfig();
  }

  // (他コア再開処理を削除)

  if (!usb_mode_msc) {
    stickMode = MODE_LOST;
  }
  delay(300);

  Serial.printf("[%lu] Ready (Xbox XInput Mode)!\n", millis());

  // 【セットアップ完了】水色を点灯したままにして Core 1 の待機を解除する
  // (消灯せず水色で静止させることで、セットアップが無事終わったことを確認可能にします)
  pixels.setPixelColor(0, pixels.Color(0, 255, 255));
  pixels.show();
  setup_done = true;
  // セットアップが無事完了したので、起動試行カウンタをリセット
  RealEEPROM.write(EEPADR_BOOT_ATTEMPT_COUNT, 0);
  RealEEPROM.commit();
}

/// <summary>
/// コントローラー入力を読み取り、XInputレポートを送信するコア処理
/// </summary>
/// <param name="is_setting">設定モード中かどうかのフラグ</param>
void process_controller_core(bool is_setting) {
  // TinyUSBのメイン処理および遅延物理フラッシュ同期タスクを実行し、ホストとの通信を維持します
  tud_task();
  tud_msc_sync_task();

  unsigned long now = millis();

  // コントローラー処理とPCへのレポート送信は約125Hz (8ms間隔) に制限し、
  // USBスタックの処理優先度を確保して高い応答性を維持します。
  if (now - lastPsxReadTime < 8) {
    send_xbox_report();
    return;
  }
  lastPsxReadTime = now;

  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte lx_org, ly_org, b1_org, b2_org, bL_org;
  byte rx_org, ry_org;
  byte r_x, r_y;
  int l_x_tmp, xy_tmp;
  short jogx;

  PsxControllerProtocol psxStickMode;

  // 接続されているPlayStation系コントローラーの有無を確認
  if (!haveController) {
    // 未検出時はUSB列挙などの干渉を防ぐため、1秒間隔で接続を試みます
    if (now - lastSearchTime >= 1000) {
      lastSearchTime = now;
      Serial.printf("[%lu] Searching for controller...\n", now);
      if (psx.begin()) {
        Serial.printf("[%lu] Controller found!\n", millis());
        delay(300); // 接続直後の過渡状態が安定するのを待つ
        
        // 設定モードへ入り、コントローラーがスティックやアナログボタンを扱えるよう構成します
        if (!psx.enterConfigMode()) {
          Serial.printf("[%lu] Cannot enter config mode\n", millis());
          psxContType = psx.getControllerType();
          Serial.printf("[%lu] Controller Type is: %s\n", millis(), controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);
          psx.read();
        } else {
          psxContType = psx.getControllerType();
          Serial.printf("[%lu] Controller Type is: %s\n", millis(), controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);

          // アナログスティック出力を有効化
          if (!psx.enableAnalogSticks()) {
            Serial.printf("[%lu] Cannot enable analog sticks\n", millis());
          }
          // アナログボタンの出力を有効化（neGconやDualShock 2などの各キーの感圧データ）
          if (!psx.enableAnalogButtons()) {
            Serial.printf("[%lu] Cannot enable analog buttons\n", millis());
          }
          // 初期化が完了したため、通常動作状態へ移行
          if (!psx.exitConfigMode()) {
            Serial.printf("[%lu] Cannot exit config mode\n", millis());
          }
        }

        // 初回読込を行いプロトコル（デジタル/アナログなど）を取得・登録して検出を完了
        psx.read();
        psxStickMode = psx.getProtocol();
        Serial.printf("[%lu] Controller Protocol is: %s\n", millis(), controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
        haveController = true;
      }
    }
  } else {
    // コントローラー接続中の毎フレーム（8msごと）の読込処理
    if (!psx.read()) {
      // 読込失敗時はコントローラーが切断されたとみなし、状態をリセットして待機モードに移行
      digitalWrite(PIN_GOODLED, HIGH); // 正常接続を示すLEDを消灯
      Serial.printf("[%lu] Controller lost :x\n", millis());
      haveController = false;
      psxContType = PSCTRL_UNKNOWN;
      psxStickMode = PSPROTO_UNKNOWN;
      OldpsxStickMode = PSPROTO_UNKNOWN;
      stickMode = MODE_LOST;

      // ホストPCへのキー出力を無入力状態にリセット
      xboxcontroller_reset();

    } else {
      digitalWrite(PIN_GOODLED, LOW); // 正常読込時はLEDを点灯
      psxStickMode = psx.getProtocol();

      // デバイスが切り替わった、またはアナログモードボタンが押されるなどしてプロトコルが変わった場合の検知
      if (psxStickMode != OldpsxStickMode) {
        if (psxStickMode == PSPROTO_DIGITAL) stickMode = MODE_STD;
        if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
        if ((psxStickMode == PSPROTO_DUALSHOCK) || (psxStickMode == PSPROTO_DUALSHOCK2)) stickMode = MODE_STD;
        if ((psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) stickMode = restoreNegStickMode(); // 内蔵設定を復元
        
        // Jogconが接続された場合のみ、追加でRumble（反力用）のセットアップを行います
        if (psxStickMode == PSPROTO_JOGCON) {
          delay(300);
          if (psx.enterConfigMode()) {
            Serial.printf("[%lu] Set Jogcon Rumble\n", millis());
            if (!psx.enableJogconRumble()) {
              Serial.printf("[%lu] Cannot enable Jogcon Rumble\n", millis());
            }
            if (!psx.exitConfigMode()) {
              Serial.printf("[%lu] Cannot exit config mode\n", millis());
            }
          }
        }
        Serial.printf("[%lu] Controller Protocol is: %s\n", millis(), controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
      }
      OldpsxStickMode = psxStickMode;

      // Xbox用レポートバッファのデジタルボタン状態のみ毎フレーム初期化（アナログ値は変化がない限り値を保持）
      XboxButtonData.digital_buttons_1 = 0;
      XboxButtonData.digital_buttons_2 = 0;

      // メタSTARTボタン等の「100ms単発疑似クリック」用タイマー処理
      if (menuOnUntil > 0) {
        if (now < menuOnUntil) {
          XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;
        } else {
          menuOnUntil = 0;
        }
      }

      // neGcon/Jogcon での START メタキー判定（STARTキーを長押し＋別キーでガイドボタンやL/R3ボタン等を疑似入力するための状態遷移）
      if (psxStickMode == PSPROTO_NEGCON || psxStickMode == PSPROTO_JOGCON) {
        uint16_t buttons = psx.getButtonWord();
        bool startPressed = (buttons & PSB_START) != 0;
        const uint16_t DIGITAL_BUTTONS_MASK = (PSB_PAD_UP | PSB_PAD_DOWN | PSB_PAD_LEFT | PSB_PAD_RIGHT | PSB_TRIANGLE | PSB_CIRCLE | PSB_R1);
        bool anyDigitalPressed = (buttons & DIGITAL_BUTTONS_MASK) != 0;

        switch (metaState) {
          case META_STATE_IDLE:
            // STARTボタンが押された時、他のデジタルキーが同時に押されていれば通常START入力、そうでなければメタ待機へ
            if (startPressed) {
              if (anyDigitalPressed) {
                metaState = META_STATE_NORMAL_START;
              } else {
                metaState = META_STATE_START_PRESSED;
              }
            }
            break;

          case META_STATE_NORMAL_START:
            // 通常START入力状態で、ボタンが離されたらIDLEへ戻る
            if (!startPressed) {
              metaState = META_STATE_IDLE;
            }
            break;

          case META_STATE_START_PRESSED:
            // START単体の押下中に、他のキーが押されずにボタンが離されたら、通常STARTとして100ms単発クリックを発行
            if (!startPressed) {
              menuOnUntil = millis() + 100;
              metaState = META_STATE_IDLE;
            } else if (anyDigitalPressed) {
              // 他のキーが押されたらメタモードをアクティブ（長押し操作）にします
              metaState = META_STATE_ACTIVE;
            }
            break;

          case META_STATE_ACTIVE:
            // すべてのキーが離されたらIDLEへ戻ります
            if (!startPressed && !anyDigitalPressed) {
              metaState = META_STATE_IDLE;
            }
            break;
        }
      } else {
        // それ以外のデバイスではメタ操作を行わない
        metaState = META_STATE_IDLE;
      }

      // コントローラーのプロトコルに応じたボタン・アナログ値の処理
      switch (psxStickMode) {
        // ==========================================
        // SPCH-1110 FLIGHTSTICK モード（アナログスティック2本仕様）
        // ==========================================
        case PSPROTO_FLIGHTSTICK:
          // 右スティック（アナログ値）の取得
          if (psx.getRightAnalog(lx_org, ly_org)) {
            l_x = lx_org;
            l_y = ly_org;
          } else {
            l_x = 0x80;
            l_y = 0x80;
          }

          // 左スティック（アナログ値）の取得
          if (psx.getLeftAnalog(rx_org, ry_org)) {
            r_x = rx_org;
            r_y = ry_org;
          } else {
            r_x = 0x80;
            r_y = 0x80;
          }

          // 値が変化した際、最大角（アナログキャリブレーション値）に基づき調整・スケーリング
          if (l_x != slx || l_y != sly || r_x != srx || r_y != sry) {
            slx = l_x;
            sly = l_y;
            srx = r_x;
            sry = r_y;

            l_x = (byte)adjustXY(l_x, analogLxMax);
            l_y = (byte)adjustXY(l_y, analogLxMax);
            r_x = (byte)adjustXY(r_x, analogLxMax);
            r_y = (byte)adjustXY(r_y, analogLxMax);

            // デバッグ表示用LED状態に反映
            ledLx = l_x;
            ledLy = l_y;
            ledRx = r_x;
            ledRy = r_y;
          }

          // Xboxコントローラー形式（±32767）にスケーリングして軸データへ代入
          XboxButtonData.l_x = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
          XboxButtonData.l_y = (int16_t)(((int32_t)l_y - 128) * 32767 / -128); // 上方向がプラス
          XboxButtonData.r_x = (int16_t)(((int32_t)r_x - 128) * 32767 / 128);
          XboxButtonData.r_y = (int16_t)(((int32_t)r_y - 128) * 32767 / -128);

          // キャリブレーション設定モード中の処理
          if (is_setting) {
            // STARTボタン押下でUSB MSC（マスストレージ）へ再接続移行
            if (psx.getButtonWord() & PSB_START) {
              xboxcontroller_reconnect(true);
            }
            // CIRCLE（赤）ボタンで現在のスティック最大傾きを計測し、最大角としてEEPROMに保存します
            if (psx.getButtonWord() & PSB_CIRCLE) {
              Serial.printf("[%lu] Analog lxMax before: %d\n", millis(), analogLxMax);
              l_x_tmp = absoluteXY(lx_org);

              xy_tmp = absoluteXY(ly_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(rx_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(ry_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              // 傾きが小さすぎる（センター近辺）の場合は異常値とみなし、初期値(255)へ強制リセットします
              if (l_x_tmp < (0x80 + 10)) l_x_tmp = 255;

              analogLxMax = (byte)l_x_tmp;
              Serial.printf("[%lu] EEP Write!\n", millis());
              EEPROM.write(EEPADR_ANALOG_STICKMAX, analogLxMax);
              EEPROM.commit();

              Serial.printf("[%lu] Analog lxMax after: %d\n", millis(), analogLxMax);
              stickMode = beforeStickMode; // 設定完了したため通常モードへ戻る
            }
          }

          keyConvert_psx2xbox();
          break;

        // ==========================================
        // neGcon モード（ハンドルねじり・I/IIアナログボタン仕様）
        // ==========================================
        case PSPROTO_NEGCON:
          // スワップ（STD, SWAPAB, SWAPLTRT, SWAPAB_SWAPLTRT）を考慮したデジタルマッピング
          if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
            uint16_t buttons = psx.getButtonWord();

            // START長押し＋各キーのメタモードがアクティブである場合のショートカットキー割り当て
            if (metaState == META_STATE_ACTIVE) {
              if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;
              if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_GUIDE; // ガイドボタン
              if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;
              if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
              if (buttons & PSB_TRIANGLE)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;
              if (buttons & PSB_CIRCLE)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;
              if (buttons & PSB_R1)        { XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;
                                             XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER; }
              if (buttons & PSB_SELECT)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;
            } else {
              // メタ長押しではない通常状態のキーマッピング
              if (metaState != META_STATE_START_PRESSED) {
                if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_UP;
                if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_DOWN;
                if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_LEFT;
                if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_RIGHT;
              }

              if (metaState == META_STATE_NORMAL_START)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;

              if (metaState != META_STATE_START_PRESSED) {
                if (buttons & PSB_R1)
                  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;
              }

              // Iボタン（Cross）= XInputのXボタン
              if (buttons & PSB_CROSS)
                XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_X;

              if (buttons & PSB_SELECT)
                XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;

              // A（Triangle）/ B（Circle）ボタンのマッピング。ABスワップ設定が有効なら入れ替えます
              if (metaState != META_STATE_START_PRESSED) {
                if (stickMode == MODE_SWAPAB || stickMode == MODE_SWAPAB_SWAPLTRT) {
                  if (buttons & PSB_TRIANGLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;
                  if (buttons & PSB_CIRCLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;
                } else {
                  if (buttons & PSB_TRIANGLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;
                  if (buttons & PSB_CIRCLE)
                    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;
                }
              }
            }
          } else {
            keyConvert_psx2xbox();
          }

          // ねじりおよび感圧アナログ値（I、II、Lボタン）の読み込み
          if (psx.getLeftAnalog(lx_org, ly_org)) {
            b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
            b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
            bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

            l_x = lx_org;
            l_x = (byte)adjustXY(l_x, lxMax); // ねじりのキャリブレーション補正
            byte lx_scaled_no_play = l_x;

            // 設定された遊びの削減値（デッドゾーン相殺）をねじり値に適用します
            int lx_tmp = l_x;
            int play = negReduceHandlePlay*2;
            if (lx_tmp > 128) {
              lx_tmp += play;
              if (lx_tmp > 255) lx_tmp = 255;
            } else if (lx_tmp < 128) {
              lx_tmp -= play;
              if (lx_tmp < 0) lx_tmp = 0;
            }
            l_x = (byte)lx_tmp;

            // I/II/Lボタンのキャリブレーション補正（最大感圧に到達しやすくスケーリング）
            l_b1 = b1_org / 2;
            l_b1 = l_b1 * NEG_CALIB_B;
            if (l_b1 > 0x80) l_b1 = 0x80;

            l_b2 = b2_org / 2;
            l_b2 = l_b2 * NEG_CALIB_B;
            if (l_b2 > 0x7f) l_b2 = 0x7f;

            l_bL = bL_org / 2;
            l_bL = l_bL * 1;
            if (l_bL > 0x7f) l_bL = 0x7f;

            // START長押し中の誤作動防止とメタモード有効化（ねじり一定以上、またはI/II/Lが押されたらメタをACTIVE化）
            if (metaState == META_STATE_START_PRESSED) {
              if (abs((int)lx_scaled_no_play - 128) > 32 || l_b1 > 20 || l_b2 > 20 || l_bL > 20) {
                metaState = META_STATE_ACTIVE;
              }
            }

            // アナログトリガー(LT/RT)・右スティック軸へのマッピング代入
            if (metaState == META_STATE_ACTIVE) {
              digitalWrite(PIN_CONNECT, LOW);
              XboxButtonData.lt = 0;
              XboxButtonData.rt = 0;

              // メタモード中：II/Lアナログボタンによる左スティックY軸の操作（スワップおよび上昇・下降向き）
              int16_t stick_val = 0;
              bool swapMode = (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT);

              if (swapMode) {
                if (l_b2 > 10) {
                  stick_val = (int16_t)(((int32_t)l_b2) * 32767 / 127);
                } else if (l_bL > 10) {
                  stick_val = (int16_t)(((int32_t)l_bL) * -32768 / 127);
                }
              } else {
                if (l_bL > 10) {
                  stick_val = (int16_t)(((int32_t)l_bL) * 32767 / 127);
                } else if (l_b2 > 10) {
                  stick_val = (int16_t)(((int32_t)l_b2) * -32768 / 127);
                }
              }

              XboxButtonData.l_y = stick_val;
              XboxButtonData.l_x = 0;
            } else {
              // 通常モード時：アナログ値をLT/RTトリガーに割り当て（LTRTスワップ設定を考慮）
              if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
                digitalWrite(PIN_CONNECT, LOW);
                
                if (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
                  uint16_t raw_rt = (uint16_t)l_b2 * 2;
                  byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
                  XboxButtonData.rt = applyAnalogCurve(rt_val, negLtCurve); // RTにIIボタン(カーブ適用)を代入

                  uint16_t raw_lt = (uint16_t)l_bL * 2;
                  byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
                  XboxButtonData.lt = applyAnalogCurve(lt_val, negRtCurve); // LTにLボタン(カーブ適用)を代入
                } else {
                  uint16_t raw_lt = (uint16_t)l_b2 * 2;
                  byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
                  XboxButtonData.lt = applyAnalogCurve(lt_val, negLtCurve); // LTにIIボタン(カーブ適用)を代入

                  uint16_t raw_rt = (uint16_t)l_bL * 2;
                  byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
                  XboxButtonData.rt = applyAnalogCurve(rt_val, negRtCurve); // RTにLボタン(カーブ適用)を代入
                }
              }
              XboxButtonData.r_x = 0;
              XboxButtonData.r_y = 0;
            }

            // 状態変化時にLED情報を更新
            if (l_x != slx || l_b1 != sb1 || l_b2 != sb2 || l_bL != sbL) {
              slx = l_x;
              sb1 = l_b1;
              sb2 = l_b2;
              sbL = l_bL;

              ledLx = l_x;
              ledB1 = b1_org;
              ledB2 = b2_org;
              ledBL = bL_org;
            }

            // ねじりハンドル角度をXInputのアナログ軸に割り当て
            if (metaState == META_STATE_ACTIVE) {
              // メタモード中：ねじりを右アナログスティック（XまたはY方向。Iボタン押下でY方向、未押下でX方向）に割り当て
              XboxButtonData.l_x = 0;

              bool shift_Y = (l_b1 > 10);
              int16_t twist_val = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
              if (shift_Y) {
                XboxButtonData.r_y = twist_val;
                XboxButtonData.r_x = 0;
              } else {
                XboxButtonData.r_x = twist_val;
                XboxButtonData.r_y = 0;
              }
            } else {
              // 通常時：ねじりを左アナログスティックのX軸（ステアリング操作）に割り当て
              XboxButtonData.l_x = (int16_t)(((int32_t)l_x - 128) * 32767 / 128);
              XboxButtonData.l_y = 0;
            }

            // 設定モード時のパラメータ変更処理
            if (is_setting) {
              uint16_t buttons = psx.getButtonWord();
              static uint16_t lastButtons = 0;

              // L1ボタン（Lボタン）押下でRTアナログ感度カーブ（0:線形〜3:二次関数曲線）をローテーション変更しEEPROMに保存
              if ((buttons & PSB_L1) && !(lastButtons & PSB_L1)) {
                negRtCurve = (negRtCurve + 1) % 4;
                EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, negRtCurve);
                EEPROM.commit();
                Serial.printf("[%lu] NEG_ANALOG_RT_CURVE: %d\n", millis(), negRtCurve);
                ledFlashCount = negRtCurve + 1; // 変更後のカーブ種類をLED明滅回数で通知
              }

              // SQUAREボタン（IIボタン）押下でLTアナログ感度カーブをローテーション変更しEEPROMに保存
              if ((buttons & PSB_SQUARE) && !(lastButtons & PSB_SQUARE)) {
                negLtCurve = (negLtCurve + 1) % 4;
                EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, negLtCurve);
                EEPROM.commit();
                Serial.printf("[%lu] NEG_ANALOG_LT_CURVE: %d\n", millis(), negLtCurve);
                ledFlashCount = negLtCurve + 1; // 変更後のカーブ種類をLED明滅回数で通知
              }

              // 十字キー上ボタンでハンドルの遊び削減値（デッドゾーンの詰め量）を増加（感度を高める）しEEPROM保存
              if ((buttons & PSB_PAD_UP) && !(lastButtons & PSB_PAD_UP)) {
                if (negReduceHandlePlay < 32) {
                  negReduceHandlePlay++;
                  EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, negReduceHandlePlay + 1);
                  EEPROM.commit();
                  Serial.printf("[%lu] NEG_REDUCE_HANDLE_PLAY: %d\n", millis(), negReduceHandlePlay);
                }
              }
              // 十字キー下ボタンでハンドルの遊び削減値（デッドゾーンの詰め量）を減少（感度を下げる）しEEPROM保存
              if ((buttons & PSB_PAD_DOWN) && !(lastButtons & PSB_PAD_DOWN)) {
                if (negReduceHandlePlay > 0) {
                  negReduceHandlePlay--;
                  EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, negReduceHandlePlay + 1);
                  EEPROM.commit();
                  Serial.printf("[%lu] NEG_REDUCE_HANDLE_PLAY: %d\n", millis(), negReduceHandlePlay);
                }
              }
              // STARTボタン押下でUSB MSC（マスストレージ）接続モードへリブート
              if ((buttons & PSB_START) && !(lastButtons & PSB_START)) {
                xboxcontroller_reconnect(true);
              }
              // CIRCLE（赤）ボタン押下で、現在のハンドルねじり量を「最大ねじり角」としてキャリブレーション保存
              if ((buttons & PSB_CIRCLE) && !(lastButtons & PSB_CIRCLE)) {
                Serial.printf("[%lu] neG lxMax before: %d\n", millis(), lxMax);

                l_x_tmp = absoluteXY(lx_org);
                // センター近辺の無効値の場合は強制初期化
                if (l_x_tmp < (0x80 + 10)) l_x_tmp = 0xff / ((NEG_CALIB - 1) / 2 + 1);

                lxMax = (byte)l_x_tmp;
                Serial.printf("[%lu] EEP Write!\n", millis());
                EEPROM.write(EEPADR_NEG_NEGMAX, lxMax);
                EEPROM.commit();

                Serial.printf("[%lu] neG lxMax after: %d\n", millis(), lxMax);
                stickMode = beforeStickMode; // 設定完了したため通常モードへ戻る
              }

              lastButtons = buttons;
            }
          }
          break;

        // ==========================================
        // Jogcon モード（反力モーター・高分解能ダイヤルホイール・I/IIボタン感圧仕様）
        // ==========================================
        case PSPROTO_JOGCON:
          {
            // 標準の操作感に合わせるため、Iボタン（CROSS）とIIボタン（SQUARE）のデジタル入力を入れ替えます
            uint16_t buttons = psx.getButtonWord();
            uint16_t swapped_buttons = buttons & ~(PSB_CROSS | PSB_SQUARE);
            if (buttons & PSB_CROSS) swapped_buttons |= PSB_SQUARE;
            if (buttons & PSB_SQUARE) swapped_buttons |= PSB_CROSS;
            buttons = swapped_buttons;

            const uint16_t DIGITAL_BUTTONS_MASK = (PSB_PAD_UP | PSB_PAD_DOWN | PSB_PAD_LEFT | PSB_PAD_RIGHT | PSB_TRIANGLE | PSB_CIRCLE | PSB_R1);

            // START長押し＋各キーのメタモードがアクティブである場合のショートカットキー割り当て
            if (metaState == META_STATE_ACTIVE) {
              if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;
              if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_GUIDE;
              if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;
              if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
              if (buttons & PSB_TRIANGLE)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;
              if (buttons & PSB_CIRCLE)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;
              if (buttons & PSB_R1)        { XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START ;
                                             XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER; }

              uint16_t other_buttons = buttons & ~(DIGITAL_BUTTONS_MASK | PSB_START);
              keyConvert_psx2xbox_ex(other_buttons);
            } else {
              // メタ長押しではない通常状態のキーマッピング
              uint16_t masked_buttons = buttons;

              // START_PRESSED（メタ移行前の長押し待機中）は、デジタルボタン誤出力を防止するためマスクします
              if (metaState == META_STATE_START_PRESSED) {
                masked_buttons &= ~DIGITAL_BUTTONS_MASK;
              }

              // STARTボタン単体の処理
              if (metaState == META_STATE_NORMAL_START) {
                masked_buttons |= PSB_START;
              } else {
                masked_buttons &= ~PSB_START;
              }

              keyConvert_psx2xbox_ex(masked_buttons);

              // ABスワップ設定が有効なら入れ替えます
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

          // ジョグダイヤルのアナログ値（回転位置）の読み込み
          if (psx.getJogConAnalog(jogx)) {
            // 設定された最大回転角（jogconDialMax）を超えて回された場合、反力モータを回してダイヤルを押し戻す反力（Forceback）を設定します
            jogxForcePower = 0;
            if (!is_setting) {
              if (jogcon_abs_val(jogx) > jogconDialMax + JOG_MAX_AJST) {
                jogxForcePower = (jogcon_abs_val(jogx) - (jogconDialMax + JOG_MAX_AJST)) / 2;
                if (jogxForcePower > 0x0f) jogxForcePower = 0x0f;
              }
              if (jogxForcePower == 0) {
                psx.setJogCommand(0x00); // 反力オフ
              } else {
                // コントローラー側に送信するパケットデータ（0x10〜:左方向反力, 0x20〜:右方向反力）を組み立てます
                if (jogx < 0)
                  psx.setJogCommand(0x10 | jogxForcePower & 0x0f);
                else
                  psx.setJogCommand(0x20 | (jogxForcePower & 0x0f));
              }
            }

            // ダイヤルの回転量をXbox 360の左アナログスティックX軸の範囲（±32767）にマッピング
            int32_t calc_lx = (int32_t)32767 * jogx / jogconDialMax;
            if (calc_lx > 32767) calc_lx = 32767;
            if (calc_lx < -32768) calc_lx = -32768;
            l_x = 0x80 + (byte)(0x80 * jogx / jogconDialMax);
            if (jogcon_abs_val(jogx) >= jogconDialMax) l_x = (jogx < 0) ? 0x00 : 0xff;

            b1_org = 0x00;
            b2_org = 0x00;

            // I/II感圧ボタンの読み込み
            if (psx.getButtonWord() & PSB_CROSS) b1_org = 0xff;
            if (psx.getButtonWord() & PSB_SQUARE) b2_org = 0xff;

            l_b1 = b1_org / 2;
            l_b2 = b2_org / 2;

            // 状態変化時にLED情報を更新
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
                byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
                byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
                XboxButtonData.rt = applyAnalogCurve(rt_val, negLtCurve); // 物理Iボタンの補正を適用
                XboxButtonData.lt = applyAnalogCurve(lt_val, negRtCurve); // 物理IIボタンの補正を適用
              } else {
                // 標準アサイン (I = LT, II = RT)
                uint16_t raw_lt = (uint16_t)l_b1 * 2;
                uint16_t raw_rt = (uint16_t)l_b2 * 2;
                byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
                byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
                XboxButtonData.lt = applyAnalogCurve(lt_val, negLtCurve);
                XboxButtonData.rt = applyAnalogCurve(rt_val, negRtCurve);
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
                  xboxcontroller_reconnect(true); // MSCモードへ移行（リセット）
                }
                if (psx.getButtonWord() & PSB_CIRCLE) {
                  Serial.printf("[%lu] Jogcon jogconDialMax before: %d\n", millis(), jogconDialMax);
                  //センターからの相対値に変更
                  jogconDialMax = jogcon_abs_val(jogx);
                  //センター値近辺の場合は初期値に設定
                  if (jogconDialMax <= 8) jogconDialMax = 100;

                  Serial.printf("[%lu] EEP Write!\n", millis());
                  EEPROM.write(EEPADR_JOG_MAX_U, (byte)(jogconDialMax >> 8));
                  EEPROM.write(EEPADR_JOG_MAX_L, (byte)(jogconDialMax & 0x00ff));
                  EEPROM.commit();

                  Serial.printf("[%lu] Jogcon jogconDialMax after: %d\n", millis(), jogconDialMax);
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
      send_xbox_report();
    }
  }

  send_xbox_report(); // 毎ループ確実に最新データをPCへ送信（認識の常時維持）
  tud_task(); // TinyUSBデバイススタックのタスク処理を巡回させる
  delay(1);
}

/// <summary>
/// USB MSC (Mass Storage Class) モードの処理を行います。
/// </summary>
void process_msc_mode() {
  // TinyUSBのメイン処理と遅延同期（Idle-Timeout Delayed Sync）を毎ループ巡回して通信を維持します
  tud_task();
  tud_msc_sync_task();

  // MSCモード中にBOOTSELボタンが押された場合、リブートして通常コントローラーモードへと戻ります
  if (BOOTSEL) {
    xboxcontroller_reconnect(false); // 通常コントローラーモードへ復帰（再接続・リセット処理を実行）
  }
  delay(1);
}

/// <summary>
/// 設定（キャリブレーション・感度カーブ）モードの処理を行います。
/// BOOTSELボタンの短押しにより、接続中のneGcon/Jogconのキー設定・スワップモードの切り替え等を行います。
/// </summary>
void process_setting_mode() {
  // BOOTSELボタンが押されている場合の処理
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW); // 設定変更操作中であることを示すため、赤色LED（ERROR LED）を点灯（LOWで点灯）
    
    // チャタリング防止および短押し時間の計測のため、ループ回数をカウントします
    if (sBootSel < 375) {
      if (sBootSel == 0) {
        changeNegStickMode = false; // ボタン押下開始の直後は、スティックモード変更フラグを初期化
      }
      if (sBootSel == 9) {
        // 短押し閾値（約70〜80ms）に達したら変更フラグを有効化し、現在の設定モード値をログ出力します
        changeNegStickMode = true;
        Serial.printf("[%lu] Current Stick mode is: %d\n", millis(), stickMode);
      }
      sBootSel++;
    }
  } else {
    // BOOTSELボタンが離された場合の処理
    digitalWrite(PIN_ERRORLED, HIGH); // 赤色LEDを消灯
    
    // ボタンが短押しされたと判定されている場合、スティックモード（スワップ設定）の切り替えを行います
    if (changeNegStickMode) {
      // コントローラーが neGcon または Jogcon の場合のみスワップ切り替えをローテーションします
      if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON)) {
        // STD（標準）-> SWAPAB（ABスワップ）-> SWAPLTRT（LTRTスワップ）-> SWAPAB_SWAPLTRT（両方スワップ）
        if (stickMode == MODE_STD) stickMode = MODE_SWAPAB;
        else if (stickMode == MODE_SWAPAB) stickMode = MODE_SWAPLTRT;
        else if (stickMode == MODE_SWAPLTRT) stickMode = MODE_SWAPAB_SWAPLTRT;
        else if (stickMode == MODE_SWAPAB_SWAPLTRT) stickMode = MODE_STD;
        else stickMode = MODE_STD;

        // 新しい設定モードを内蔵EEPROMに書き込み、コミットして確定させます
        Serial.printf("[%lu] EEP Write!\n", millis());
        EEPROM.write(EEPADR_NEGMODE, stickMode);
        EEPROM.commit();
      }

      // モード切り替え後、設定モード（MODE_SETTING_NEG）を抜けて切り替えた通常動作モードへ復帰します
      if (stickMode == MODE_SETTING_NEG) stickMode = beforeStickMode;

      Serial.printf("[%lu] Change Stick mode is: %d\n", millis(), stickMode);
    }

    // ボタンの状態変数とカウンタをリセットして次の操作に備えます
    changeNegStickMode = false;
    sBootSel = 0;
  }

  // 設定モードとしてコントローラー入力の読み出しとPCへの送信を実行します（is_setting = true）
  process_controller_core(true);
}

/// <summary>
/// 通常コントローラーモードの処理を行います。
/// BOOTSELボタンの長押し（約3秒）により、キャリブレーション等の設定を行う「設定モード」へ移行します。
/// </summary>
void process_controller_mode() {
  // BOOTSELボタンが押されている場合の処理
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW); // 移行操作中（長押し中）であることを示すため、赤色LEDを点灯
    
    // 長押し時間のカウント
    if (sBootSel < 375) {
      if (sBootSel == 374) {
        // 約3秒（375ループ）押し続けられた場合、対応するアナログコントローラーが接続されていれば設定モードへ移行
        if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON) || (OldpsxStickMode == PSPROTO_FLIGHTSTICK)) {
          Serial.printf("[%lu] Set Config mode\n", millis());
          changeNegStickMode = false;
          beforeStickMode = stickMode;     // 設定完了後の復帰用に現在の動作モードを退避
          stickMode = MODE_SETTING_NEG;   // 動作状態を設定モード（MODE_SETTING_NEG）に切り替える
        }
      }
      sBootSel++;
    }
  } else {
    // ボタンが押されていない時はカウンタと赤色LEDをクリアします
    digitalWrite(PIN_ERRORLED, HIGH); // 赤色LEDを消灯
    sBootSel = 0;                     // 押下時間カウンタをリセット
  }

  // 通常モードとしてコントローラー入力の読み出しとPCへの送信を実行します（is_setting = false）
  process_controller_core(false);
}

/// <summary>
/// Core 0 (メインコア) のメインループ
/// </summary>
void loop() {
  if (usb_mode_msc) {
    process_msc_mode();
  } else if (stickMode == MODE_SETTING_NEG) {
    process_setting_mode();
  } else {
    process_controller_mode();
  }
}

