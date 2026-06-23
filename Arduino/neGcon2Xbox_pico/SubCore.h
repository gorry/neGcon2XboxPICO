#ifndef SUB_CORE_H
#define SUB_CORE_H

#include "Arduino.h"
#include <Adafruit_NeoPixel.h>

struct ControllerState;

/// <summary>
/// Core 1 (サブコア) での動作とLED、アクセスランプなどの出力を管理するクラス
/// </summary>
class SubCoreApp {
private:
  Adafruit_NeoPixel _pixels;
  byte _ledLx, _ledLy, _ledRx, _ledRy, _ledB1, _ledB2, _ledBL;
  volatile int _ledFlashCount;

  // アニメーション用内部状態
  byte _heartbeatNum;
  bool _heartbeatFlag;
  const byte _heartbeatSpeed;

  SubCoreApp();

public:
  /// <summary>
  /// シングルトンインスタンスの取得
  /// </summary>
  static SubCoreApp& getInstance() {
    static SubCoreApp instance;
    return instance;
  }

  /// <summary>
  /// Core 1 側の初期化処理を行います。
  /// </summary>
  void begin();

  /// <summary>
  /// Core 1 側のメインループ処理を行います。
  /// </summary>
  void update();

  /// <summary>
  /// コントローラ状態からLEDの描画用パラメータを同期します。
  /// </summary>
  /// <param name="state">コントローラ状態へのポインタ</param>
  void updateLedState(const ControllerState* state);

  /// <summary>
  /// 指定回数LEDを点滅させます（設定変更の通知用）。
  /// </summary>
  /// <param name="count">点滅回数</param>
  void flashLed(int count);

  // 既存コードとの互換性のためのゲッター
  Adafruit_NeoPixel& getPixels() { return _pixels; }
};

// 既存コードとの互換性確保のためのマクロ定義
#define pixels (SubCoreApp::getInstance().getPixels())

#endif // SUB_CORE_H
