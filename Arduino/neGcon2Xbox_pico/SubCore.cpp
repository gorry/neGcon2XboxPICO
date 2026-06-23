#include "SubCore.h"
#include "neGcon2Xbox_pico.h"
#include "Config.h"
#include "Controller.h"

// LEDまわりの変数の実体定義
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIX, NEO_GRB + NEO_KHZ800);
byte ledLx = 0, ledLy = 0, ledRx = 0, ledRy = 0, ledB1 = 0, ledB2 = 0, ledBL = 0;
volatile int ledFlashCount = 0;

/// <summary>
/// Core 1 (サブコア) の初期化処理
/// </summary>
void setup1() {
  // Core 0 (メインコア) のセットアップが完了するまで待機します
  while (!setup_done) {
    delay(50);
  }
}

/// <summary>
/// Core 1 (サブコア) のメインループ
/// </summary>
void loop1() {
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
        pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // 読み込み：青
      } else {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // 書き込み：赤
      }
      pixels.show();
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
    pixels.setPixelColor(0, pixels.Color(0, brightness, brightness));
    pixels.show();
    delay(20);
    return;
  }

  static byte heartbeat_num = 0;
  static bool heartbeat_flag = true;
  const byte heartbeat_speed = 32;

  // 設定変更時の LED 明滅通知処理
  if (ledFlashCount > 0) {
    byte r_val = (config.negReduceHandlePlay == 32) ? 255 : (config.negReduceHandlePlay * 8);
    byte g_val = ledLx;
    byte b_val = heartbeat_num;

    // 点灯 (0.1秒)
    pixels.setPixelColor(0, pixels.Color(r_val, g_val, b_val));
    pixels.show();
    delay(100);

    // 消灯 (0.1秒)
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(100);

    ledFlashCount--;
    return;
  }

  // スティックモードに応じた LED 色制御
  switch (stickMode) {
    case MODE_STD:
      // 青色ベース（標準モード、無入力時は青）
      pixels.setPixelColor(0, pixels.Color(ledRx / 4, ledLx / 4, 0x80 - ledB1 - ledB2 + ledBL / 2));
      pixels.show();
      break;

    case MODE_SWAPAB:
      // 緑色ベース（A/Bスワップ、無入力時は緑）
      pixels.setPixelColor(0, pixels.Color(ledRx / 4, 0x80 - ledB1 + ledBL / 2, ledLx / 4));
      pixels.show();
      break;

    case MODE_SWAPLTRT:
      // 赤色ベース（LT/RTスワップ、無入力時は赤）
      pixels.setPixelColor(0, pixels.Color(0x80 - ledB2 + ledBL / 2, ledLx / 4, ledLy / 4));
      pixels.show();
      break;

    case MODE_SWAPAB_SWAPLTRT:
      // 紫・マゼンタベース (両方スワップ、無入力時は水色)
      pixels.setPixelColor(0, pixels.Color(0, 0x80 - ledB1 + ledB2, ledLx / 2 + ledBL / 2));
      pixels.show();
      break;

    // フライトコントローラ接続時の点灯パターン
    case MODE_AIRCON22:
      pixels.setPixelColor(0, pixels.Color(ledRy / 4, ledLx / 8, ledLy / 8 + 0x40));
      pixels.show();
      break;

    case MODE_SETTING_NEG:
      {
        // heartbeat_num で B値（青）を明滅、ねじり値でG値（緑）を明滅、config.negReduceHandlePlay に比例した固定輝度で R値（赤）を点灯
        byte r_val = (config.negReduceHandlePlay == 32) ? 255 : (config.negReduceHandlePlay * 8);
        byte g_val = ledLx;
        pixels.setPixelColor(0, pixels.Color(r_val, g_val, heartbeat_num));
        pixels.show();
      }
      break;

    default:
      pixels.clear();
      pixels.show();
      break;
  }

  // LEDの点滅処理数値計算 (delay(50)にあわせてステップ幅を8に調整し、約3秒周期で明滅)
  if (heartbeat_flag) {
    if (heartbeat_num >= 255 - heartbeat_speed) {
      heartbeat_num = 255;
      heartbeat_flag = false;
    } else {
      heartbeat_num += heartbeat_speed;
    }
  } else {
    if (heartbeat_num <= heartbeat_speed) {
      heartbeat_num = 0;
      heartbeat_flag = true;
    } else {
      heartbeat_num -= heartbeat_speed;
    }
  }

  delay(50); // Sub core の無駄な CPU 占有とバス負荷を軽減するためのウェイト
}
