#ifndef SUB_CORE_H
#define SUB_CORE_H

#include "Arduino.h"
#include <Adafruit_NeoPixel.h>

// LEDまわりの変数のextern宣言
extern Adafruit_NeoPixel pixels;
extern byte ledLx, ledLy, ledRx, ledRy, ledB1, ledB2, ledBL;
extern volatile int ledFlashCount;

/// <summary>
/// Core 1 (サブコア) の初期化処理を行います。
/// </summary>
void setup1();

/// <summary>
/// Core 1 (サブコア) のメインループ処理を行います。
/// 主に NeoPixel LED の制御と MSC モード中のステータスランプ制御を担当します。
/// </summary>
void loop1();

#endif // SUB_CORE_H
