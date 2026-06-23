#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FatFS.h>

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

// Stick Mode
#define MODE_STD 0
#define MODE_SWAPAB 1
#define MODE_SWAPLTRT 2
#define MODE_SWAPAB_SWAPLTRT 3
#define MODE_AIRCON22 10
#define MODE_USB_MSC 97
#define MODE_SETTING_NEG 98
#define MODE_LOST 99

#define MAX_NEG_REDUCE_HANDLE_PLAY 32

// デフォルト設定値の定義
#define DEFAULT_NEG_LT_CURVE           NEG_ANALOG_CURVE_LINEAR // 0
#define DEFAULT_NEG_RT_CURVE           NEG_ANALOG_CURVE_LINEAR // 0
#define DEFAULT_NEG_REDUCE_HANDLE_PLAY 16
#define DEFAULT_NEG_TWIST_MAX          242
#define DEFAULT_JOGCON_DIAL_MAX        100
#define DEFAULT_STICK_MODE             MODE_STD
#define DEFAULT_ANALOG_LX_MAX          255

struct ConfigParams {
  byte negLtCurve;
  byte negRtCurve;
  byte negReduceHandlePlay;
  byte lxMax;
  short jogconDialMax;
  byte stickMode;
  byte analogLxMax;
};

// グローバル設定パラメータの実体参照
extern ConfigParams config;

/// <summary>
/// 設定パラメータを使って CONFIG.INI 形式のデータを指定した出力先に書き出します。
/// </summary>
/// <param name="out">出力先（FileなどのPrintインターフェース）</param>
/// <param name="config">設定パラメータ構造体</param>
void writeConfigIni(Print& out, const ConfigParams& config);

/// <summary>
/// CONFIG.INI ファイルまたは EEPROM から設定パラメータをロードします。
/// </summary>
void loadConfig();

/// <summary>
/// 現在の設定パラメータを /CONFIG.INI ファイルへ書き出し保存します。
/// </summary>
void saveConfig(byte currentStickMode);

/// <summary>
/// EEPROMの設定値を CONFIG.INI へ移行するか、またはテンプレートから初期生成します。
/// </summary>
void migrateOrInitConfig();

/// <summary>
/// EEPROMが有効にフォーマットされているか確認します。
/// </summary>
/// <returns>有効な設定データが存在すれば true、存在しなければ false</returns>
bool eepromCheck();

/// <summary>
/// EEPROMの初期設定と全領域のクリアを行います。
/// </summary>
void eepromFormat();

/// <summary>
/// neGconのアナログLT感度カーブの設定値を復元します。
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegLtCurve();

/// <summary>
/// neGconのアナログRT感度カーブの設定値を復元します。
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegRtCurve();

/// <summary>
/// neGconハンドル（ひねり）の最大キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (0x80-255)</returns>
byte restoreNegDegMax();

/// <summary>
/// Jogconダイヤルの最大回転キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (8-500)</returns>
short restorejogMax();

/// <summary>
/// 通常アナログスティックの最大キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (128-255)</returns>
byte restoreAnaDegMax();

/// <summary>
/// コントローラーのボタン配置モード設定を復元します。
/// </summary>
/// <returns>復元されたモードインデックス (MODE_STD ~ AIRCON22)</returns>
byte restoreNegStickMode();

/// <summary>
/// neGconハンドル（ひねり）の遊び削減値を復元します。
/// </summary>
/// <returns>復元された遊び削減値 (0-32)</returns>
byte restoreNegReduceHandlePlay();

/// <summary>
/// アナログスティックの最大キャリブレーション値を保存します。
/// </summary>
/// <param name="maxVal">設定する最大キャリブレーション値 (128-255)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveAnalogLxMax(byte maxVal, byte currentStickMode);

/// <summary>
/// Jogconダイヤルの最大回転位置キャリブレーション値を保存します。
/// </summary>
/// <param name="maxVal">設定する最大キャリブレーション値 (8-500)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveJogconDialMax(short maxVal, byte currentStickMode);

/// <summary>
/// neGconのアナログRT感度カーブを保存します。
/// </summary>
/// <param name="curveVal">設定するカーブ種類 (0-3)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveNegRtCurve(byte curveVal, byte currentStickMode);

/// <summary>
/// neGconのアナログLT感度カーブを保存します。
/// </summary>
/// <param name="curveVal">設定するカーブ種類 (0-3)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveNegLtCurve(byte curveVal, byte currentStickMode);

/// <summary>
/// neGconの遊び削減値を保存します。
/// </summary>
/// <param name="playVal">設定する遊び削減値 (0-32)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveNegReduceHandlePlay(byte playVal, byte currentStickMode);

/// <summary>
/// neGconの最大ねじり角キャリブレーション値を保存します。
/// </summary>
/// <param name="maxVal">設定する最大ねじり角キャリブレーション値 (1-255)</param>
/// <param name="currentStickMode">現在のスティックモード</param>
void saveNegTwistMax(byte maxVal, byte currentStickMode);

#endif // CONFIG_H
