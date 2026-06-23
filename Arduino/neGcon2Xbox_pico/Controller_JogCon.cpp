#include "Controller_JogCon.h"
#include "neGcon2Xbox_pico.h"
#include "SubCore.h"

// Jogcon 固有の定数と状態変数 (モジュール内に隠蔽)
#define JOG_MAX_AJST -4

/// <summary>
/// Jogcon の入力処理
/// </summary>
/// <param name="state">コントローラ状態へのポインタ</param>
void JogConController::process(ControllerState *state) {
  // ファイルスコープに静的な入力キャッシュを保持 (グローバル変数の削減)
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
    if (buttons & PSB_R1)        { XboxButtonData.digital_buttons_1 |= XINPUT_GAMEPAD_START;
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

  // ジョグダイヤルのアナログ値（回転位置）の読み込み
  if (psx.getJogConAnalog(state->jogx)) {
    // 設定された最大回転角（config.jogconDialMax）を超えて回された場合、反力モータを回してダイヤルを押し戻す反力（Forceback）を設定します
    jogxForcePower = 0;
    if (!state->is_setting) {
      if (jogcon_abs_val(state->jogx) > config.jogconDialMax + JOG_MAX_AJST) {
        jogxForcePower = (jogcon_abs_val(state->jogx) - (config.jogconDialMax + JOG_MAX_AJST)) / 2;
        if (jogxForcePower > 0x0f) jogxForcePower = 0x0f;
      }
      if (jogxForcePower == 0) {
        psx.setJogCommand(0x00); // 反力オフ
      } else {
        // コントローラー側に送信するパケットデータ（0x10〜:左方向反力, 0x20〜:右方向反力）を組み立てます
        if (state->jogx < 0)
          psx.setJogCommand(0x10 | (jogxForcePower & 0x0f));
        else
          psx.setJogCommand(0x20 | (jogxForcePower & 0x0f));
      }
    }

    // ダイヤルの回転量をXbox 360の左アナログスティックX軸の範囲（±32767）にマッピング
    int32_t calc_lx = (int32_t)32767 * state->jogx / config.jogconDialMax;
    if (calc_lx > 32767) calc_lx = 32767;
    if (calc_lx < -32768) calc_lx = -32768;
    state->l_x = 0x80 + (byte)(0x80 * state->jogx / config.jogconDialMax);
    if (jogcon_abs_val(state->jogx) >= config.jogconDialMax) state->l_x = (state->jogx < 0) ? 0x00 : 0xff;

    state->b1_org = 0x00;
    state->b2_org = 0x00;

    // I/II感圧ボタンの読み込み
    if (psx.getButtonWord() & PSB_CROSS) state->b1_org = 0xff;
    if (psx.getButtonWord() & PSB_SQUARE) state->b2_org = 0xff;

    state->l_b1 = state->b1_org / 2;
    state->l_b2 = state->b2_org / 2;

    // 状態変化時にLED情報を更新
    if (state->l_x != slx || state->l_b1 != sb1 || state->l_b2 != sb2 || state->l_bL != sbL) {
      slx = state->l_x;
      sb1 = state->l_b1;
      sb2 = state->l_b2;
      sbL = state->l_bL;
      SubCoreApp::getInstance().updateLedState(state);
    }

    // 最終的な値をセット (ハンドル)
    XboxButtonData.l_x = (int16_t)calc_lx;
    XboxButtonData.l_y = 0;

    // ゲームモードが有効な場合アナログ値でアクセル・ブレーキが設定可能 (スワップ考慮)
    if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
      if (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
        // LT/RT スワップ (I = RT, II = LT)
        uint16_t raw_rt = (uint16_t)state->l_b1 * 2;
        uint16_t raw_lt = (uint16_t)state->l_b2 * 2;
        byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
        byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
        XboxButtonData.rt = applyAnalogCurve(rt_val, config.negLtCurve); // 物理Iボタンの補正を適用
        XboxButtonData.lt = applyAnalogCurve(lt_val, config.negRtCurve); // 物理IIボタンの補正を適用
      } else {
        // 標準アサイン (I = LT, II = RT)
        uint16_t raw_lt = (uint16_t)state->l_b1 * 2;
        uint16_t raw_rt = (uint16_t)state->l_b2 * 2;
        byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
        byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
        XboxButtonData.lt = applyAnalogCurve(lt_val, config.negLtCurve);
        XboxButtonData.rt = applyAnalogCurve(rt_val, config.negRtCurve);
      }
    }

    // 最大角、設定モード
    if (stickMode == MODE_SETTING_NEG) {
      if (jogxPosResetEnable < 100) {
        if (jogcon_abs_val(state->jogx) > 4) {
          jogxPosResetEnable = 0;
          jogxForcePower = (jogcon_abs_val(state->jogx) / 2) + 1;
          if (jogxForcePower > 0x0f) jogxForcePower = 0x0f;

          if (state->jogx < 0) {
            psx.setJogCommand(0x10 | jogxForcePower);
          } else {
            psx.setJogCommand(0x20 | jogxForcePower);
          }
        } else {
          jogxPosResetEnable++;
          psx.setJogCommand(0x00);
        }
      } else {
        if (psx.getButtonWord() & PSB_CIRCLE) {
          Serial.printf("[%lu] Jogcon config.jogconDialMax before: %d\n", millis(), config.jogconDialMax);
          // センターからの相対値に変更
          short maxVal = jogcon_abs_val(state->jogx);
          // センター値近辺の場合は初期値に設定
          if (maxVal <= 8) maxVal = 100;

          saveJogconDialMax(maxVal, stickMode);

          Serial.printf("[%lu] Jogcon config.jogconDialMax after: %d\n", millis(), config.jogconDialMax);
          stickMode = beforeStickMode;
        }
      }
    } else {
      jogxPosResetEnable = 0;
    }
  }
}
