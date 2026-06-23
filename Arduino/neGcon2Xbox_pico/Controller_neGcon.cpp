#include "Controller_neGcon.h"
#include "neGcon2Xbox_pico.h"

// neGcon アナログ値補正用定数
#define NEG_CALIB 1.0
#define NEG_CALIB_B 1.0

/// <summary>
/// neGcon の入力処理
/// </summary>
/// <param name="state">コントローラ状態へのポインタ</param>
void process_negcon(ControllerState *state) {
  // ファイルスコープに静的な入力キャッシュを保持 (グローバル変数の削減)
  static byte slx = 0;
  static byte sb1 = 0;
  static byte sb2 = 0;
  static byte sbL = 0;

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
  if (psx.getLeftAnalog(state->lx_org, state->ly_org)) {
    state->b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
    state->b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
    state->bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

    state->l_x = state->lx_org;
    state->l_x = (byte)adjustXY(state->l_x, config.lxMax); // ねじりのキャリブレーション補正
    byte lx_scaled_no_play = state->l_x;

    // 設定された遊びの削減値（デッドゾーン相殺）をねじり値に適用します
    int lx_tmp = state->l_x;
    int play = config.negReduceHandlePlay*2;
    if (lx_tmp > 128) {
      lx_tmp += play;
      if (lx_tmp > 255) lx_tmp = 255;
    } else if (lx_tmp < 128) {
      lx_tmp -= play;
      if (lx_tmp < 0) lx_tmp = 0;
    }
    state->l_x = (byte)lx_tmp;

    // I/II/Lボタンのキャリブレーション補正（最大感圧に到達しやすくスケーリング）
    state->l_b1 = state->b1_org / 2;
    state->l_b1 = state->l_b1 * NEG_CALIB_B;
    if (state->l_b1 > 0x80) state->l_b1 = 0x80;

    state->l_b2 = state->b2_org / 2;
    state->l_b2 = state->l_b2 * NEG_CALIB_B;
    if (state->l_b2 > 0x7f) state->l_b2 = 0x7f;

    state->l_bL = state->bL_org / 2;
    state->l_bL = state->l_bL * 1;
    if (state->l_bL > 0x7f) state->l_bL = 0x7f;

    // START長押し中の誤作動防止とメタモード有効化（ねじり一定以上、またはI/II/Lが押されたらメタをACTIVE化）
    if (metaState == META_STATE_START_PRESSED) {
      if (abs((int)lx_scaled_no_play - 128) > 32 || state->l_b1 > 20 || state->l_b2 > 20 || state->l_bL > 20) {
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
        if (state->l_b2 > 10) {
          stick_val = (int16_t)(((int32_t)state->l_b2) * 32767 / 127);
        } else if (state->l_bL > 10) {
          stick_val = (int16_t)(((int32_t)state->l_bL) * -32768 / 127);
        }
      } else {
        if (state->l_bL > 10) {
          stick_val = (int16_t)(((int32_t)state->l_bL) * 32767 / 127);
        } else if (state->l_b2 > 10) {
          stick_val = (int16_t)(((int32_t)state->l_b2) * -32768 / 127);
        }
      }

      XboxButtonData.l_y = stick_val;
      XboxButtonData.l_x = 0;
    } else {
      // 通常モード時：アナログ値をLT/RTトリガーに割り当て（LTRTスワップ設定を考慮）
      if (stickMode == MODE_STD || stickMode == MODE_SWAPAB || stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
        digitalWrite(PIN_CONNECT, LOW);
        
        if (stickMode == MODE_SWAPLTRT || stickMode == MODE_SWAPAB_SWAPLTRT) {
          uint16_t raw_rt = (uint16_t)state->l_b2 * 2;
          byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
          XboxButtonData.rt = applyAnalogCurve(rt_val, config.negLtCurve); // RTにIIボタン(カーブ適用)を代入

          uint16_t raw_lt = (uint16_t)state->l_bL * 2;
          byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
          XboxButtonData.lt = applyAnalogCurve(lt_val, config.negRtCurve); // LTにLボタン(カーブ適用)を代入
        } else {
          uint16_t raw_lt = (uint16_t)state->l_b2 * 2;
          byte lt_val = (raw_lt > 255) ? 255 : raw_lt;
          XboxButtonData.lt = applyAnalogCurve(lt_val, config.negLtCurve); // LTにIIボタン(カーブ適用)を代入

          uint16_t raw_rt = (uint16_t)state->l_bL * 2;
          byte rt_val = (raw_rt > 255) ? 255 : raw_rt;
          XboxButtonData.rt = applyAnalogCurve(rt_val, config.negRtCurve); // RTにLボタン(カーブ適用)を代入
        }
      }
      XboxButtonData.r_x = 0;
      XboxButtonData.r_y = 0;
    }

    // 状態変化時にLED情報を更新
    if (state->l_x != slx || state->l_b1 != sb1 || state->l_b2 != sb2 || state->l_bL != sbL) {
      slx = state->l_x;
      sb1 = state->l_b1;
      sb2 = state->l_b2;
      sbL = state->l_bL;

      ledLx = state->l_x;
      ledB1 = state->b1_org;
      ledB2 = state->b2_org;
      ledBL = state->bL_org;
    }

    // ねじりハンドル角度をXInputのアナログ軸に割り当て
    if (metaState == META_STATE_ACTIVE) {
      // メタモード中：ねじりを右アナログスティック（XまたはY方向。Iボタン押下でY方向、未押下でX方向）に割り当て
      XboxButtonData.l_x = 0;

      bool shift_Y = (state->l_b1 > 10);
      int16_t twist_val = (int16_t)(((int32_t)state->l_x - 128) * 32767 / 128);
      if (shift_Y) {
        XboxButtonData.r_y = twist_val;
        XboxButtonData.r_x = 0;
      } else {
        XboxButtonData.r_x = twist_val;
        XboxButtonData.r_y = 0;
      }
    } else {
      // 通常時：ねじりを左アナログスティックのX軸（ステアリング操作）に割り当て
      XboxButtonData.l_x = (int16_t)(((int32_t)state->l_x - 128) * 32767 / 128);
      XboxButtonData.l_y = 0;
    }

    // 設定モード時のパラメータ変更処理
    if (state->is_setting) {
      uint16_t buttons = psx.getButtonWord();
      static uint16_t lastButtons = 0;

      // L1ボタン（Lボタン）押下でRTアナログ感度カーブ（0:線形〜3:二次関数曲線）をローテーション変更しEEPROMに保存
      if ((buttons & PSB_L1) && !(lastButtons & PSB_L1)) {
        saveNegRtCurve((config.negRtCurve + 1) % 4, stickMode);
        Serial.printf("[%lu] NEG_ANALOG_RT_CURVE: %d\n", millis(), config.negRtCurve);
        ledFlashCount = config.negRtCurve + 1; // 変更後のカーブ種類をLED明滅回数で通知
      }

      // SQUAREボタン（IIボタン）押下でLTアナログ感度カーブをローテーション変更しEEPROMに保存
      if ((buttons & PSB_SQUARE) && !(lastButtons & PSB_SQUARE)) {
        saveNegLtCurve((config.negLtCurve + 1) % 4, stickMode);
        Serial.printf("[%lu] NEG_ANALOG_LT_CURVE: %d\n", millis(), config.negLtCurve);
        ledFlashCount = config.negLtCurve + 1; // 変更後のカーブ種類をLED明滅回数で通知
      }

      // 十字キー上ボタンでハンドルの遊び削減値（デッドゾーンの詰め量）を増加（感度を高める）しEEPROM保存
      if ((buttons & PSB_PAD_UP) && !(lastButtons & PSB_PAD_UP)) {
        if (config.negReduceHandlePlay < 32) {
          saveNegReduceHandlePlay(config.negReduceHandlePlay + 1, stickMode);
          Serial.printf("[%lu] NEG_REDUCE_HANDLE_PLAY: %d\n", millis(), config.negReduceHandlePlay);
        }
      }
      // 十字キー下ボタンでハンドルの遊び削減値（デッドゾーンの詰め量）を減少（感度を下げる）しEEPROM保存
      if ((buttons & PSB_PAD_DOWN) && !(lastButtons & PSB_PAD_DOWN)) {
        if (config.negReduceHandlePlay > 0) {
          saveNegReduceHandlePlay(config.negReduceHandlePlay - 1, stickMode);
          Serial.printf("[%lu] NEG_REDUCE_HANDLE_PLAY: %d\n", millis(), config.negReduceHandlePlay);
        }
      }
      // STARTボタン押下でUSB MSC（マスストレージ）接続モードへリブート
      if ((buttons & PSB_START) && !(lastButtons & PSB_START)) {
        xboxcontroller_reconnect(true);
      }
      // CIRCLE（赤）ボタン押下で、現在のハンドルねじり量を「最大ねじり角」としてキャリブレーション保存
      if ((buttons & PSB_CIRCLE) && !(lastButtons & PSB_CIRCLE)) {
        Serial.printf("[%lu] neG config.lxMax before: %d\n", millis(), config.lxMax);

        state->l_x_tmp = absoluteXY(state->lx_org);
        // センター近辺の無効値の場合は強制初期化
        if (state->l_x_tmp < (0x80 + 10)) state->l_x_tmp = 0xff / ((NEG_CALIB - 1) / 2 + 1);

        saveNegTwistMax((byte)state->l_x_tmp, stickMode);

        Serial.printf("[%lu] neG config.lxMax after: %d\n", millis(), config.lxMax);
        stickMode = beforeStickMode; // 設定完了したため通常モードへ戻る
      }

      lastButtons = buttons;
    }
  }
}
