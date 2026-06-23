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
#include "XboxControllerPico.h"
DummySerial Serial_Dummy;
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <FatFS.h>
#include "Config.h"
#include "Controller.h"
#include "Controller_FlightStick.h"
#include "Controller_neGcon.h"
#include "Controller_JogCon.h"
#include "Controller_DualShock.h"
#include "neGcon2Xbox_pico.h"
#include "SubCore.h"

bool fs_ready = false;
bool setup_done = false;

#if !defined(CFG_TUD_MSC) || (CFG_TUD_MSC == 0)
#error "CFG_TUD_MSC is not defined or is 0! TinyUSB MSC stack is disabled!"
#endif

// ConfigEEPROMWrapperの定義 (本物の EEPROM を使用)
static auto& RealEEPROM = ::EEPROM;

void ConfigEEPROMWrapper::begin(size_t size) { RealEEPROM.begin(size); }
uint8_t ConfigEEPROMWrapper::read(int address) { return RealEEPROM.read(address); }
void ConfigEEPROMWrapper::write(int address, uint8_t value) { RealEEPROM.write(address, value); }
ConfigEEPROMWrapper ConfigEEPROM;

// 既存コードの EEPROM 呼び出しをラッパーに置き換える
#define EEPROM ConfigEEPROM

volatile unsigned long last_msc_access_time = 0;
volatile uint8_t msc_access_type = 0; // 0: なし, 1: 読み込み(青), 2: 書き込み(赤)
volatile bool msc_active_connected = false; // ホストとの通信が開始されたか

bool haveController = false;

PsxControllerPICO_HwSpi<PIN_PS2_ATT, PIN_PS2_CLK, PIN_PS2_DAT, PIN_PS2_CMD> psx;

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

// STARTメタキー機能のための状態定義
MetaKeyState metaState = META_STATE_IDLE;
unsigned long menuOnUntil = 0;

byte beforeStickMode;
byte stickMode = DEFAULT_STICK_MODE;

/// --- グローバル変数 (loop内のstatic変数を昇格) ---
static unsigned long lastPsxReadTime = 0;
static unsigned long lastSearchTime = 0; // psx.begin() の間隔制限用
static bool changeNegStickMode;
static bool bootselWaitingForRelease = false; // 設定モード移行時にボタン解放を待つためのフラグ
static PsxControllerType psxContType = PSCTRL_UNKNOWN;
static PsxControllerProtocol OldpsxStickMode = PSPROTO_UNKNOWN;

bool ConfigEEPROMWrapper::commit() {
  if (!fs_ready) {
    uint32_t ints = save_and_disable_interrupts();
    bool res = RealEEPROM.commit();
    restore_interrupts(ints);
    return res;
  }
  // ファイルシステム動作時はEEPROM.commit()を実行せずフリーズを防止
  saveConfig(stickMode);
  return true;
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
  uint32_t fs_start = get_fs_start_address();
  uint32_t fs_end = get_fs_end_address();
  uint32_t fs_size = get_fs_size();
  Serial.printf("[%lu] FS Start: 0x%08X, End: 0x%08X, Size: %u bytes\n", millis(), fs_start, fs_end, fs_size);
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
    stickMode = config.stickMode; // ロードした設定値で状態を同期
    Serial.printf("[%lu] Active Config values:\n", millis());
    Serial.printf("  config.negLtCurve: %d\n", config.negLtCurve);
    Serial.printf("  config.negRtCurve: %d\n", config.negRtCurve);
    Serial.printf("  config.negReduceHandlePlay: %d\n", config.negReduceHandlePlay);
    Serial.printf("  negTwistMax: %d\n", config.lxMax);
    Serial.printf("  config.jogconDialMax: %d\n", config.jogconDialMax);
    Serial.printf("  stickMode: %d\n", stickMode);
    Serial.printf("  config.analogLxMax: %d\n", config.analogLxMax);
  }

  if (!usb_mode_msc) {
    stickMode = MODE_LOST;
  }
  delay(300);

  Serial.printf("[%lu] Ready (Xbox XInput Mode)!\n", millis());

  // 【セットアップ完了】水色を点灯したままにして Core 1 の待機を解除する
  pixels.setPixelColor(0, pixels.Color(0, 255, 255));
  pixels.show();
  setup_done = true;
  // セットアップが無事完了したので、起動試行カウンタをリセット
  RealEEPROM.write(EEPADR_BOOT_ATTEMPT_COUNT, 0);
  RealEEPROM.commit();
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

  // 接続されているPlayStation系コントローラーの有無を確認
  if (!haveController) {
    search_and_connect_controller(now);
  } else {
    // コントローラー接続中の毎フレーム（8msごと）の読込処理
    if (!psx.read()) {
      disconnect_controller();
    } else {
      process_connected_controller(is_setting, now);
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
/// スティックモード（スワップ設定）を次のモードへトグル切り替えし、EEPROMに保存します。
/// </summary>
void toggle_stick_mode() {
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
}

/// <summary>
/// 設定（キャリブレーション・感度カーブ）モードの処理を行います。
/// BOOTSELボタンの短押しにより、接続中のneGcon/Jogconのキー設定・スワップモードの切り替え等を行います。
/// </summary>
void process_setting_mode() {
  static unsigned long bootselPressStart = 0;

  // 設定モード突入直後のボタン離し待ち
  if (bootselWaitingForRelease) {
    if (BOOTSEL) {
      digitalWrite(PIN_ERRORLED, LOW); // 押されている間は赤色LEDを点灯
      return;
    } else {
      digitalWrite(PIN_ERRORLED, HIGH); // 離されたら消灯してフラグをクリア
      bootselWaitingForRelease = false;
      bootselPressStart = 0;
      changeNegStickMode = false;
      return;
    }
  }

  // BOOTSELボタンが押されている場合の処理
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW); // 設定変更操作中であることを示すため、赤色LED（ERROR LED）を点灯（LOWで点灯）
    
    if (bootselPressStart == 0) {
      bootselPressStart = millis();
      changeNegStickMode = false; // ボタン押下開始の直後は、スティックモード変更フラグを初期化
    } else if (millis() - bootselPressStart >= 70) {
      // 短押し閾値（約70ms以上）に達したら変更フラグを有効化
      if (!changeNegStickMode) {
        changeNegStickMode = true;
        Serial.printf("[%lu] Current Stick mode is: %d\n", millis(), stickMode);
      }
    }
  } else {
    // BOOTSELボタンが離された場合の処理
    digitalWrite(PIN_ERRORLED, HIGH); // 赤色LEDを消灯
    
    // ボタンが短押しされたと判定されている場合、スティックモード（スワップ設定）の切り替えを行います
    if (changeNegStickMode) {
      toggle_stick_mode();

      // モード切り替え後、設定モード（MODE_SETTING_NEG）を抜けて切り替えた通常動作モードへ復帰します
      if (stickMode == MODE_SETTING_NEG) stickMode = beforeStickMode;

      Serial.printf("[%lu] Change Stick mode is: %d\n", millis(), stickMode);
    }

    // ボタンの状態変数とカウンタをリセットして次の操作に備えます
    changeNegStickMode = false;
    bootselPressStart = 0;
  }

  // 設定モードとしてコントローラー入力の読み出しとPCへの送信を実行します（is_setting = true）
  process_controller_core(true);
}

/// <summary>
/// 通常コントローラーモードの処理を行います。
/// BOOTSELボタンの長押し（約3秒）により、キャリブレーション等の設定を行う「設定モード」へ移行します。
/// </summary>
void process_controller_mode() {
  static unsigned long bootselPressStart = 0;

  // BOOTSELボタンが押されている場合の処理
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW); // 移行操作中（長押し中）であることを示すため、赤色LEDを点灯
    
    if (bootselPressStart == 0) {
      bootselPressStart = millis();
      changeNegStickMode = false;
    } else {
      unsigned long pressDuration = millis() - bootselPressStart;
      if (pressDuration >= 3000) {
        // 約3秒（3000ms）押し続けられた場合、対応するアナログコントローラーが接続されていれば設定モードへ移行
        if (stickMode != MODE_SETTING_NEG) {
          if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON) || (OldpsxStickMode == PSPROTO_FLIGHTSTICK)) {
            Serial.printf("[%lu] Set Config mode\n", millis());
            changeNegStickMode = false;
            beforeStickMode = stickMode;     // 設定完了後の復帰用に現在の動作モードを退避
            stickMode = MODE_SETTING_NEG;   // 動作状態を設定モード（MODE_SETTING_NEG）に切り替える
            bootselPressStart = 0;           // 移行したのでタイマーリセット
            bootselWaitingForRelease = true; // ボタンがいったん離されるまで設定モード側のトグル判定を抑止
          }
        }
      } else if (pressDuration >= 70) {
        // 短押し閾値（約70ms以上）に達したら変更フラグを有効化
        if (!changeNegStickMode) {
          changeNegStickMode = true;
          Serial.printf("[%lu] Current Stick mode is: %d\n", millis(), stickMode);
        }
      }
    }
  } else {
    // ボタンが押されていない時はカウンタと赤色LEDをクリアします
    digitalWrite(PIN_ERRORLED, HIGH); // 赤色LEDを消灯

    // ボタンが短押しされたと判定されている場合、通常モード時にもスティックモード（スワップ設定）の切り替えを行います
    if (changeNegStickMode) {
      toggle_stick_mode();
      Serial.printf("[%lu] Change Stick mode is: %d\n", millis(), stickMode);
    }

    changeNegStickMode = false;
    bootselPressStart = 0;            // 押下時間タイマーをリセット
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

/// <summary>
/// コントローラー未接続時の接続試行処理
/// </summary>
void search_and_connect_controller(unsigned long now) {
  // 未検出時はUSB列挙などの干渉を防ぐため、1秒間隔で接続を試みます
  if (now - lastSearchTime < 1000) return;
  lastSearchTime = now;
  Serial.printf("[%lu] Searching for controller...\n", now);
  if (!psx.begin()) return;

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
  PsxControllerProtocol proto = psx.getProtocol();
  Serial.printf("[%lu] Controller Protocol is: %s\n", millis(), controllerProtoStrings[proto < PSPROTO_MAX ? static_cast<byte>(proto) : PSPROTO_MAX]);
  haveController = true;
}

/// <summary>
/// コントローラー切断時のリセット処理
/// </summary>
void disconnect_controller() {
  digitalWrite(PIN_GOODLED, HIGH); // 正常接続を示すLEDを消灯
  Serial.printf("[%lu] Controller lost :x\n", millis());
  haveController = false;
  psxContType = PSCTRL_UNKNOWN;
  OldpsxStickMode = PSPROTO_UNKNOWN;
  stickMode = MODE_LOST;

  // ホストPCへのキー出力を無入力状態にリセット
  xboxcontroller_reset();
}

/// <summary>
/// コントローラー接続中の毎フレーム処理（プロトコル判定・メタキー・入力変換）
/// </summary>
void process_connected_controller(bool is_setting, unsigned long now) {
  digitalWrite(PIN_GOODLED, LOW); // 正常読込時はLEDを点灯
  PsxControllerProtocol psxStickMode = psx.getProtocol();

  // デバイスが切り替わった、またはアナログモードボタンが押されるなどしてプロトコルが変わった場合の検知
  if (psxStickMode != OldpsxStickMode) {
    if (psxStickMode == PSPROTO_DIGITAL) stickMode = MODE_STD;
    if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
    if ((psxStickMode == PSPROTO_DUALSHOCK) || (psxStickMode == PSPROTO_DUALSHOCK2)) stickMode = MODE_STD;
    if ((psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) {
      stickMode = config.stickMode; // 内蔵設定を復元
    }

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

  // 設定モード中のSTARTボタン処理 (短押し: MSCモード、3秒長押し: 設定初期化&再起動)
  if (is_setting) {
    static unsigned long startPressTime = 0;
    static bool startProcessed = false;

    uint16_t buttons = psx.getButtonWord();
    bool startPressed = (buttons & PSB_START) != 0;

    if (startPressed) {
      if (startPressTime == 0) {
        startPressTime = now;
        startProcessed = false;
      } else if (!startProcessed && (now - startPressTime >= 3000)) {
        startProcessed = true;
        Serial.println(F("START long pressed! Resetting config to default and rebooting..."));
        resetConfigToDefault();
        xboxcontroller_reconnect(false); // 通常モードで再起動
      }
    } else {
      if (startPressTime > 0) {
        unsigned long pressDuration = now - startPressTime;
        startPressTime = 0;
        if (!startProcessed && (pressDuration >= 50)) { // チャタリング防止 (50ms)
          Serial.println(F("START short pressed! Reconnecting in USB MSC mode..."));
          xboxcontroller_reconnect(true); // MSCモードで再起動
        }
      }
    }
  }

  // コントローラーのプロトコルに応じたボタン・アナログ値の処理
  ControllerState state = {};
  state.is_setting   = is_setting;
  state.now          = now;
  state.psxStickMode = psxStickMode;
  Controller* controller = ControllerFactory::getController(psxStickMode);
  controller->process(&state);

  // XInputデータ送信
  send_xbox_report();
}
