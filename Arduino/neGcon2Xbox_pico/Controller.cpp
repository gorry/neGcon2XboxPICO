#include "Controller.h"
#include "neGcon2Xbox_pico.h"
#include "Controller_FlightStick.h"
#include "Controller_neGcon.h"
#include "Controller_JogCon.h"
#include "Controller_DualShock.h"

/// <summary>
/// 感度カーブを適用してアナログ値を補正
/// </summary>
/// <param name="value">補正前のアナログ値 (0-255)</param>
/// <param name="curveType">カーブの種類 (NEG_ANALOG_CURVE_LINEAR ~ A3)</param>
/// <returns>補正後のアナログ値 (0-255)</returns>
byte Controller::applyAnalogCurve(byte value, byte curveType) {
  // 線形カーブ（補正なし）の場合はそのまま値を返す
  if (curveType == NEG_ANALOG_CURVE_LINEAR) {
    return value;
  }
  
  // 各カーブタイプに応じたべき乗の指数 (p) を設定
  float p = 1.0f;
  if (curveType == NEG_ANALOG_CURVE_A1) p = 2.0f;
  else if (curveType == NEG_ANALOG_CURVE_A2) p = 4.0f;
  else if (curveType == NEG_ANALOG_CURVE_A3) p = 6.0f;
  
  // アナログ値を 0.0〜1.0 に正規化
  float val_norm = (float)value / 255.0f;
  // 指数による感度曲線の計算
  float val_curved = powf(val_norm, p);
  // 再度 0〜255 の範囲にスケーリング
  int result = (int)(val_curved * 255.0f + 0.5f);
  // 境界値のクリッピング
  if (result > 255) result = 255;
  if (result < 0) result = 0;
  
  return (byte)result;
}

/// <summary>
/// アナログ値のセンター(0x80)からの絶対偏差を計算
/// </summary>
/// <param name="lx">対象のアナログ値 (0-255)</param>
/// <returns>0x80からの絶対偏差値</returns>
int Controller::absoluteXY(byte lx) {
  int lx_tmp;
  // センター値 (0x80) からどの程度離れているかを計算
  if (lx < 0x80) lx_tmp = (int)(0xFF - lx);
  else lx_tmp = (int)lx;
  return lx_tmp;
}

/// <summary>
/// short型値の絶対値を計算
/// </summary>
/// <param name="x">入力値</param>
/// <returns>入力値の絶対値</returns>
short Controller::jogcon_abs_val(short x) {
  // 負の値の場合は符号を反転、正の場合はそのまま返す
  return x < 0 ? -x : x;
}

/// <summary>
/// 最大キャリブレーション値に基づいてアナログスティック範囲を補正
/// アナログ値はすべて0x80をセンターとした値
/// </summary>
/// <param name="lx">補正対象のアナログ値</param>
/// <param name="max">最大キャリブレーション値</param>
/// <returns>0-255の範囲にスケーリングされた補正後のアナログ値</returns>
int Controller::adjustXY(byte lx, byte max) {
  int lx_tmp;

  // センター(0x80)より大きいか小さいかで分岐して補正を計算
  if (lx > 0x80) {
    // 最大キャリブレーション値までの傾きを、標準の最大幅(0x7F)に引き伸ばす
    lx_tmp = (int)((lx - 0x80)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x7f) lx_tmp = 0x7f;
    lx_tmp = 0x80 + lx_tmp;
  } else {
    // 同様にマイナス方向の傾きを補正
    lx_tmp = (int)((0x80 - lx)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x80) lx_tmp = 0x80;
    lx_tmp = 0x80 - lx_tmp;
  }
  return lx_tmp;
}

/// <summary>
/// PSXボタン入力をXInputボタン入力データへ変換 (拡張版)
/// </summary>
/// <param name="buttons">PSXコントローラーから読み取ったボタンワード</param>
void Controller::keyConvert_psx2xbox_ex(uint16_t buttons) {
  // D-pad (十字キー) を XInput にマッピング
  if (buttons & PSB_PAD_UP)    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_UP;
  if (buttons & PSB_PAD_DOWN)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_DOWN;
  if (buttons & PSB_PAD_LEFT)  XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_LEFT;
  if (buttons & PSB_PAD_RIGHT) XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_DPAD_RIGHT;

  // SELECTボタン -> BACKボタン
  if (buttons & PSB_SELECT)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_BACK;

  // L3ボタン -> 左スティッククリック
  if (buttons & PSB_L3)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_LEFT_THUMB;

  // R3ボタン -> 右スティッククリック
  if (buttons & PSB_R3)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_RIGHT_THUMB;

  // STARTボタン -> STARTボタン
  if (buttons & PSB_START)
    XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;

  // L2ボタン -> 左トリガー (LT) 全開
  if (buttons & PSB_L2)
    XboxButtonData.lt = 255;

  // R2ボタン -> 右トリガー (RT) 全開
  if (buttons & PSB_R2)
    XboxButtonData.rt = 255;

  // L1ボタン -> 左ショルダー (LB)
  if (buttons & PSB_L1)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_LEFT_SHOULDER;

  // R1ボタン -> 右ショルダー (RB)
  if (buttons & PSB_R1)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

  // Triangle (△) -> Yボタン
  if (buttons & PSB_TRIANGLE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_Y;

  // Circle (○) -> Bボタン
  if (buttons & PSB_CIRCLE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_B;

  // Cross (×) -> Aボタン
  if (buttons & PSB_CROSS)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_A;

  // Square (□) -> Xボタン
  if (buttons & PSB_SQUARE)
    XboxButtonData.digital_buttons_2 |= XINPUT_GAMEPAD_X;
}

/// <summary>
/// 現在取得しているPSXボタン入力をXInputボタン入力データへ変換
/// </summary>
void Controller::keyConvert_psx2xbox() {
  // psxオブジェクトから直接最新のボタン情報を取得し変換を実行
  keyConvert_psx2xbox_ex(psx.getButtonWord());
}

/// <summary>
/// 未対応・デフォルトコントローラーの入力処理
/// </summary>
/// <param name="state">コントローラ状態へのポインタ</param>
void DefaultController::process(ControllerState *state) {
  // デフォルト処理として、単純にボタン入力を変換してXInputに代入
  keyConvert_psx2xbox();
}

/// <summary>
/// プロトコルに応じて適切なコントローラーオブジェクトを返却します。
/// </summary>
/// <param name="protocol">検出されたコントローラープロトコル</param>
/// <returns>対応するControllerインスタンスのポインタ</returns>
Controller* ControllerFactory::getController(PsxControllerProtocol protocol) {
  static DefaultController defaultCtrl;
  static FlightStickController flightStickCtrl;
  static NeGconController neGconCtrl;
  static JogConController jogConCtrl;
  static DualShockController dualShockCtrl;

  switch (protocol) {
    case PSPROTO_FLIGHTSTICK:
      return &flightStickCtrl;
    case PSPROTO_NEGCON:
      return &neGconCtrl;
    case PSPROTO_JOGCON:
      return &jogConCtrl;
    case PSPROTO_DUALSHOCK:
    case PSPROTO_DUALSHOCK2:
      return &dualShockCtrl;
    default:
      return &defaultCtrl;
  }
}
