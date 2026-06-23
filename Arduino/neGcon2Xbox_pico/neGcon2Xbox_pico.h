#ifndef NEGCON2XBOX_PICO_H
#define NEGCON2XBOX_PICO_H

#include "Arduino.h"
#include "Config.h"
#include "Controller.h"

// NeoPixelの設定
#define NUMPIXELS 1
#define NEO_PWR 11  // GPIO11
#define NEOPIX 12  // GPIO12

// MSC（マスストレージクラス）アクセス関連の変数
extern volatile unsigned long last_msc_access_time;
extern volatile uint8_t msc_access_type;
extern volatile bool msc_active_connected;

// コントローラー接続状態
extern bool haveController;
extern bool fs_ready;
extern volatile bool flash_busy;
extern bool setup_done;

// コントローラーの種類とプロトコルの文字列
extern const char ctrlTypeUnknown[] PROGMEM;
extern const char ctrlTypeDualShock[] PROGMEM;
extern const char ctrlTypeDsWireless[] PROGMEM;
extern const char ctrlTypeGuitHero[] PROGMEM;
extern const char ctrlTypeOutOfBounds[] PROGMEM;
extern const char* const controllerTypeStrings[PSCTRL_MAX + 1] PROGMEM;

extern const char ctrlProtoUnknown[] PROGMEM;
extern const char ctrlProtoDigital[] PROGMEM;
extern const char ctrlProtoDualShock[] PROGMEM;
extern const char ctrlProtoDualShock2[] PROGMEM;
extern const char ctrlProtoFlightstick[] PROGMEM;
extern const char ctrlProtoNegcon[] PROGMEM;
extern const char ctrlProtoJogcon[] PROGMEM;
extern const char ctrlProtoOutOfBounds[] PROGMEM;
extern const char* const controllerProtoStrings[PSPROTO_MAX + 1] PROGMEM;

// neGcon2Xbox_pico.ino 内で定義されているグローバル変数の宣言
extern PsxControllerPICO_HwSpi<PIN_PS2_ATT, PIN_PS2_CLK, PIN_PS2_DAT, PIN_PS2_CMD> psx;
extern byte stickMode;
extern byte beforeStickMode;
extern MetaKeyState metaState;
extern unsigned long menuOnUntil;

// ConfigEEPROMWrapperの定義
class ConfigEEPROMWrapper {
public:
  void begin(size_t size);
  uint8_t read(int address);
  void write(int address, uint8_t value);
  bool commit();
};
extern ConfigEEPROMWrapper ConfigEEPROM;

// 前方宣言・関数プロトタイプ
/// <summary>
/// コントローラーを探索し、接続処理を行います。
/// </summary>
/// <param name="now">現在のミリ秒時間</param>
void search_and_connect_controller(unsigned long now);

/// <summary>
/// コントローラーの接続解除処理を行います。
/// </summary>
void disconnect_controller();

/// <summary>
/// スティックの割り当てモードをトグル切り替えします。
/// </summary>
void toggle_stick_mode();

/// <summary>
/// 接続されたコントローラーの入力処理を実行します。
/// </summary>
/// <param name="is_setting">設定モード中かどうかのフラグ</param>
/// <param name="now">現在のミリ秒時間</param>
void process_connected_controller(bool is_setting, unsigned long now);

/// <summary>
/// XInputレポートをPCへ送信します。
/// </summary>
void send_xbox_report();

/// <summary>
/// コントローラー入力を読み取り、XInputレポートを送信するコア処理
/// </summary>
/// <param name="is_setting">設定モード中かどうかのフラグ</param>
void process_controller_core(bool is_setting);

/// <summary>
/// USB MSC (Mass Storage Class) モードの処理を行います。
/// </summary>
void process_msc_mode();

#endif // NEGCON2XBOX_PICO_H
