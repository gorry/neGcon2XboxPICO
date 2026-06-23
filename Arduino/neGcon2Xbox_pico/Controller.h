#pragma once

#include "Arduino.h"
#include "PsxControllerPICO_HwSpi2.h"
#include "XboxControllerPico.h"
#include "Config.h"

// ピン定義 (constexpr を使用してヘッダー内での定義と定数式としての利用を可能にする)
constexpr unsigned int PIN_PS2_CMD = 3;  // MOSI / CMD (GP3)
constexpr unsigned int PIN_PS2_DAT = 4;  // MISO / DAT (GP4)
constexpr unsigned int PIN_PS2_CLK = 2;  // SCK / CLK (GP2)
constexpr unsigned int PIN_PS2_ATT = 1;  // CS / ATTN (GP1)

constexpr unsigned int PIN_CONNECT = 25;  // BLUE
constexpr unsigned int PIN_ERRORLED = 17;  // RED
constexpr unsigned int PIN_GOODLED = 16;  // GREEN

// コントローラ状態構造体
struct ControllerState {
  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte lx_org, ly_org, b1_org, b2_org, bL_org;
  byte rx_org, ry_org;
  byte r_x, r_y;
  int l_x_tmp, xy_tmp;
  short jogx;
  PsxControllerProtocol psxStickMode;
  bool is_setting;
  unsigned long now;
};

// 状態定義用の列挙型
enum MetaKeyState {
  META_STATE_IDLE = 0,
  META_STATE_START_PRESSED,
  META_STATE_ACTIVE,
  META_STATE_NORMAL_START
};



/// <summary>
/// すべてのコントローラ制御クラスの基底となる抽象クラス
/// </summary>
class Controller {
protected:
  // 共通の補正・変換ヘルパーメソッド
  byte applyAnalogCurve(byte value, byte curveType);
  int absoluteXY(byte lx);
  short jogcon_abs_val(short x);
  int adjustXY(byte lx, byte max);
  void keyConvert_psx2xbox_ex(uint16_t buttons);
  void keyConvert_psx2xbox();

public:
  virtual ~Controller() {}
  
  /// <summary>
  /// コントローラの入力データ処理を実行します。
  /// </summary>
  /// <param name="state">コントローラ状態へのポインタ</param>
  virtual void process(ControllerState* state) = 0;
};

/// <summary>
/// デフォルトコントローラ（デジタルまたはその他の未対応機器）の処理クラス
/// </summary>
class DefaultController : public Controller {
public:
  void process(ControllerState* state) override;
};

/// <summary>
/// プロトコルに応じて適切なコントローラーオブジェクトを返却するファクトリクラス
/// </summary>
class ControllerFactory {
public:
  static Controller* getController(PsxControllerProtocol protocol);
};
