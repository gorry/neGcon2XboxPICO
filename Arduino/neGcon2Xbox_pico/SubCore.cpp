#include "SubCore.h"
#include "neGcon2Xbox_pico.h"
#include "Config.h"
#include "Controller.h"

SubCoreApp::SubCoreApp()
  : _pixels(NUMPIXELS, NEOPIX, NEO_GRB + NEO_KHZ800),
    _ledLx(0), _ledLy(0), _ledRx(0), _ledRy(0), _ledB1(0), _ledB2(0), _ledBL(0),
    _ledFlashCount(0), _heartbeatNum(0), _heartbeatFlag(true), _heartbeatSpeed(32) {}

void SubCoreApp::updateLedState(const ControllerState* state) {
  _ledLx = state->l_x;
  _ledLy = state->l_y;
  _ledRx = state->r_x;
  _ledRy = state->r_y;
  _ledB1 = state->l_b1;
  _ledB2 = state->l_b2;
  _ledBL = state->l_bL;
}

void SubCoreApp::flashLed(int count) {
  _ledFlashCount = count;
}


// LEDまわりの変数の実体定義


/// <summary>
/// Core 1 (サブコア) の初期化処理
/// </summary>
void SubCoreApp::begin() {
  // Core 0 (メインコア) のセットアップが完了するまで待機します
  while (!setup_done) {
    delay(50);
  }
}

/// <summary>
/// Core 1 (サブコア) のメインループ
/// </summary>
void SubCoreApp::update() {
  // Flashへの書き込み中は、Core 1 の動作（NeoPixelアクセスなど）によるバス競合を防止するため一時停止します
  if (flash_busy) {
    delay(100);
    return;
  }

  // USB MSC モード動作時の LED 制御
  if (usb_mode_msc) {
    // アクセスランプ処理（最後の読み書きから100ms未満は優先表示）
    if (msc_access_type != 0 && (millis() - last_msc_access_time < 100)) {
      if (msc_access_type == 1) {
        _pixels.setPixelColor(0, _pixels.Color(0, 0, 255)); // 読み込み：青
      } else {
        _pixels.setPixelColor(0, _pixels.Color(255, 0, 0)); // 書き込み：赤
      }
      _pixels.show();
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
    _pixels.setPixelColor(0, _pixels.Color(0, brightness, brightness));
    _pixels.show();
    delay(20);
    return;
  }

  

  // 設定変更時の LED 明滅通知処理
  if (_ledFlashCount > 0) {
    byte r_val = (config.negReduceHandlePlay == 32) ? 255 : (config.negReduceHandlePlay * 8);
    byte g_val = _ledLx;
    byte b_val = _heartbeatNum;

    // 点灯 (0.1秒)
    _pixels.setPixelColor(0, _pixels.Color(r_val, g_val, b_val));
    _pixels.show();
    delay(100);

    // 消灯 (0.1秒)
    _pixels.setPixelColor(0, _pixels.Color(0, 0, 0));
    _pixels.show();
    delay(100);

    _ledFlashCount--;
    return;
  }

  // スティックモードに応じた LED 色制御
  switch (stickMode) {
    case MODE_STD:
      // 青色ベース（標準モード、無入力時は青）
      _pixels.setPixelColor(0, _pixels.Color(_ledRx / 4, _ledLx / 4, 0x80 - _ledB1 - _ledB2 + _ledBL / 2));
      _pixels.show();
      break;

    case MODE_SWAPAB:
      // 緑色ベース（A/Bスワップ、無入力時は緑）
      _pixels.setPixelColor(0, _pixels.Color(_ledRx / 4, 0x80 - _ledB1 + _ledBL / 2, _ledLx / 4));
      _pixels.show();
      break;

    case MODE_SWAPLTRT:
      // 赤色ベース（LT/RTスワップ、無入力時は赤）
      _pixels.setPixelColor(0, _pixels.Color(0x80 - _ledB2 + _ledBL / 2, _ledLx / 4, _ledLy / 4));
      _pixels.show();
      break;

    case MODE_SWAPAB_SWAPLTRT:
      // 紫・マゼンタベース (両方スワップ、無入力時は水色)
      _pixels.setPixelColor(0, _pixels.Color(0, 0x80 - _ledB1 + _ledB2, _ledLx / 2 + _ledBL / 2));
      _pixels.show();
      break;

    // フライトコントローラ接続時の点灯パターン
    case MODE_AIRCON22:
      _pixels.setPixelColor(0, _pixels.Color(_ledRy / 4, _ledLx / 8, _ledLy / 8 + 0x40));
      _pixels.show();
      break;

    case MODE_SETTING_NEG:
      {
        // _heartbeatNum で B値（青）を明滅、ねじり値でG値（緑）を明滅、config.negReduceHandlePlay に比例した固定輝度で R値（赤）を点灯
        byte r_val = (config.negReduceHandlePlay == 32) ? 255 : (config.negReduceHandlePlay * 8);
        byte g_val = _ledLx;
        _pixels.setPixelColor(0, _pixels.Color(r_val, g_val, _heartbeatNum));
        _pixels.show();
      }
      break;

    default:
      _pixels.clear();
      _pixels.show();
      break;
  }

  // LEDの点滅処理数値計算 (delay(50)にあわせてステップ幅を8に調整し、約3秒周期で明滅)
  if (_heartbeatFlag) {
    if (_heartbeatNum >= 255 - _heartbeatSpeed) {
      _heartbeatNum = 255;
      _heartbeatFlag = false;
    } else {
      _heartbeatNum += _heartbeatSpeed;
    }
  } else {
    if (_heartbeatNum <= _heartbeatSpeed) {
      _heartbeatNum = 0;
      _heartbeatFlag = true;
    } else {
      _heartbeatNum -= _heartbeatSpeed;
    }
  }

  delay(50); // Sub core の無駄な CPU 占有とバス負荷を軽減するためのウェイト
}

/// <summary>
/// Arduinoコアから暗黙的に呼び出される Core 1 の初期化エントリポイント
/// </summary>
void setup1() {
  SubCoreApp::getInstance().begin();
}

/// <summary>
/// Arduinoコアから暗黙的に呼び出される Core 1 のメインループエントリポイント
/// </summary>
void loop1() {
  SubCoreApp::getInstance().update();
}
