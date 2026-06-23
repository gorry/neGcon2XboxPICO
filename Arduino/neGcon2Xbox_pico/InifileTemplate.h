#ifndef INIFILE_TEMPLATE_H
#define INIFILE_TEMPLATE_H

#include <Arduino.h>

struct ConfigParams {
  byte negLtCurve;
  byte negRtCurve;
  byte negReduceHandlePlay;
  byte lxMax;
  short jogconDialMax;
  byte stickMode;
  byte analogLxMax;
};

/// <summary>
/// 設定パラメータを使って CONFIG.INI 形式のデータを指定した出力先に書き出します。
/// </summary>
/// <param name="out">出力先（FileなどのPrintインターフェース）</param>
/// <param name="config">設定パラメータ構造体</param>
inline void writeConfigIni(Print& out, const ConfigParams& config) {
  out.println(F("[neGcon]"));
  out.println(F("; neGcon2Xbox_pico CONFIGURATION FILE"));
  out.println();
  out.println(F("; RT/LT analog sensitivity curve (0: LINEAR, 1: A1 (p=2.0), 2: A2 (p=4.0), 3: A3 (p=6.0))"));
  out.print(F("negLtCurve=")); out.println(config.negLtCurve);
  out.print(F("negRtCurve=")); out.println(config.negRtCurve);
  out.println();
  out.println(F("; Twist play reduction (deadzone compensation) value (0 to 32)"));
  out.print(F("negReduceHandlePlay=")); out.println(config.negReduceHandlePlay);
  out.println();
  out.println(F("; Twist max angle calibration value (1 to 255)"));
  out.print(F("negTwistMax=")); out.println(config.lxMax);
  out.println();
  out.println(F("; Jogcon Dial max position calibration value (8 to 500)"));
  out.print(F("jogconDialMax=")); out.println(config.jogconDialMax);
  out.println();
  out.println(F("; Controller operation mode (0: STD, 1: SWAPAB, 2: SWAPLTRT, 3: SWAPAB_SWAPLTRT)"));
  out.print(F("stickMode=")); out.println(config.stickMode);
  out.println();
  out.println(F("; Analog stick max calibration value (128 to 255)"));
  out.print(F("analogLxMax=")); out.println(config.analogLxMax);
}

#endif // INIFILE_TEMPLATE_H
