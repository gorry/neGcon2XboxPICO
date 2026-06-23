#include "Controller_FlightStick.h"
#include "neGcon2Xbox_pico.h"

/// <summary>
/// FLIGHTSTICK (SPCH-1110) の入力処理
/// </summary>
/// <param name="state">コントローラ状態へのポインタ</param>
void process_flightstick(ControllerState *state) {
  // ファイルローカルに静的な入力キャッシュを保持 (不要なグローバル変数の削減)
  static byte slx = 0;
  static byte sly = 0;
  static byte srx = 0;
  static byte sry = 0;

  // 右スティック（アナログ値）の取得
  if (psx.getRightAnalog(state->lx_org, state->ly_org)) {
    state->l_x = state->lx_org;
    state->l_y = state->ly_org;
  } else {
    state->l_x = 0x80;
    state->l_y = 0x80;
  }

  // 左スティック（アナログ値）の取得
  if (psx.getLeftAnalog(state->rx_org, state->ry_org)) {
    state->r_x = state->rx_org;
    state->r_y = state->ry_org;
  } else {
    state->r_x = 0x80;
    state->r_y = 0x80;
  }

  // 値が変化した際、最大角（アナログキャリブレーション値）に基づき調整・スケーリング
  if (state->l_x != slx || state->l_y != sly || state->r_x != srx || state->r_y != sry) {
    slx = state->l_x;
    sly = state->l_y;
    srx = state->r_x;
    sry = state->r_y;

    state->l_x = (byte)adjustXY(state->l_x, config.analogLxMax);
    state->l_y = (byte)adjustXY(state->l_y, config.analogLxMax);
    state->r_x = (byte)adjustXY(state->r_x, config.analogLxMax);
    state->r_y = (byte)adjustXY(state->r_y, config.analogLxMax);

    // デバッグ表示用LED状態に反映
    ledLx = state->l_x;
    ledLy = state->l_y;
    ledRx = state->r_x;
    ledRy = state->r_y;
  }

  // Xboxコントローラー形式（±32767）にスケーリングして軸データへ代入
  XboxButtonData.l_x = (int16_t)(((int32_t)state->l_x - 128) * 32767 / 128);
  XboxButtonData.l_y = (int16_t)(((int32_t)state->l_y - 128) * 32767 / -128); // 上方向がプラス
  XboxButtonData.r_x = (int16_t)(((int32_t)state->r_x - 128) * 32767 / 128);
  XboxButtonData.r_y = (int16_t)(((int32_t)state->r_y - 128) * 32767 / -128);

  // キャリブレーション設定モード中の処理
  if (state->is_setting) {
    // STARTボタン押下でUSB MSC（マスストレージ）へ再接続移行
    if (psx.getButtonWord() & PSB_START) {
      xboxcontroller_reconnect(true);
    }
    // CIRCLE（赤）ボタンで現在のスティック最大傾きを計測し、最大角としてEEPROMに保存します
    if (psx.getButtonWord() & PSB_CIRCLE) {
      Serial.printf("[%lu] Analog config.lxMax before: %d\n", millis(), config.analogLxMax);
      state->l_x_tmp = absoluteXY(state->lx_org);

      state->xy_tmp = absoluteXY(state->ly_org);
      if (state->xy_tmp > state->l_x_tmp) state->l_x_tmp = state->xy_tmp;

      state->xy_tmp = absoluteXY(state->rx_org);
      if (state->xy_tmp > state->l_x_tmp) state->l_x_tmp = state->xy_tmp;

      state->xy_tmp = absoluteXY(state->ry_org);
      if (state->xy_tmp > state->l_x_tmp) state->l_x_tmp = state->xy_tmp;

      // 傾きが小さすぎる（センター近辺）の場合は異常値とみなし、初期値(255)へ強制リセットします
      if (state->l_x_tmp < (0x80 + 10)) state->l_x_tmp = 255;

      saveAnalogLxMax((byte)state->l_x_tmp, stickMode);

      Serial.printf("[%lu] Analog config.lxMax after: %d\n", millis(), config.analogLxMax);
      stickMode = beforeStickMode; // 設定完了したため通常モードへ戻る
    }
  }

  // ボタン情報をXInputにマッピング
  keyConvert_psx2xbox();
}
