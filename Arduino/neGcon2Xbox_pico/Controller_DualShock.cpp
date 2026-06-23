#include "Controller_DualShock.h"
#include "neGcon2Xbox_pico.h"
#include "SubCore.h"

/// <summary>
/// DualShock / DualShock 2 の入力処理
/// </summary>
/// <param name="state">コントローラ状態へのポインタ</param>
void DualShockController::process(ControllerState *state) {
  // ファイルスコープに静的な入力キャッシュを保持 (グローバル変数の削減)
  // ボタン情報をXInputにマッピング
  keyConvert_psx2xbox();

  // 左アナログスティック値の取得
  if (psx.getLeftAnalog(state->lx_org, state->ly_org)) {
    state->l_x = state->lx_org;
    state->l_y = state->ly_org;
  } else {
    state->l_x = 0x80;
    state->l_y = 0x80;
  }

  // 右アナログスティック値の取得
  if (psx.getRightAnalog(state->rx_org, state->ry_org)) {
    state->r_x = state->rx_org;
    state->r_y = state->ry_org;
  } else {
    state->r_x = 0x80;
    state->r_y = 0x80;
  }

  // 状態変化時にLED情報を更新
  if (state->l_x != slx || state->l_y != sly || state->r_x != srx || state->r_y != sry) {
    slx = state->l_x;
    sly = state->l_y;
    srx = state->r_x;
    sry = state->r_y;
    SubCoreApp::getInstance().updateLedState(state);
  }

  // 軸の代入 (Xbox仕様にスケーリング)
  XboxButtonData.l_x = (int16_t)(((int32_t)state->l_x - 128) * 32767 / 128);
  XboxButtonData.l_y = (int16_t)(((int32_t)state->l_y - 128) * 32767 / -128); // 上がプラス
  XboxButtonData.r_x = (int16_t)(((int32_t)state->r_x - 128) * 32767 / 128);
  XboxButtonData.r_y = (int16_t)(((int32_t)state->r_y - 128) * 32767 / -128);
}
