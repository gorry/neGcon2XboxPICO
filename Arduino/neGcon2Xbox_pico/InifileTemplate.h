#ifndef INIFILE_TEMPLATE_H
#define INIFILE_TEMPLATE_H

#include <Arduino.h>

/// <summary>
/// 設定パラメータを使って CONFIG.INI 形式のデータを指定した出力先に書き出します。
/// </summary>
/// <param name="out">出力先（FileなどのPrintインターフェース）</param>
/// <param name="negLtCurve">LTアナログ感度カーブ</param>
/// <param name="negRtCurve">RTアナログ感度カーブ</param>
/// <param name="negReduceHandlePlay">ハンドル遊び削減値</param>
/// <param name="lxMax">ねじり最大値</param>
/// <param name="jogconDialMax">Jogconダイヤル最大値</param>
/// <param name="stickMode">動作モード</param>
/// <param name="analogLxMax">左スティック最大値</param>
inline void writeConfigIni(Print& out, int negLtCurve, int negRtCurve, int negReduceHandlePlay, int lxMax, int jogconDialMax, int stickMode, int analogLxMax) {
  out.println(F("[neGcon]"));
  out.println(F("; neGcon2Xbox_pico CONFIGURATION FILE"));
  out.println();
  out.println(F("; RT/LT analog sensitivity curve (0: LINEAR, 1: A1 (p=2.0), 2: A2 (p=4.0), 3: A3 (p=6.0))"));
  out.print(F("negLtCurve=")); out.println(negLtCurve);
  out.print(F("negRtCurve=")); out.println(negRtCurve);
  out.println();
  out.println(F("; Twist play reduction (deadzone compensation) value (0 to 32)"));
  out.print(F("negReduceHandlePlay=")); out.println(negReduceHandlePlay);
  out.println();
  out.println(F("; Twist max angle calibration value (1 to 255)"));
  out.print(F("negTwistMax=")); out.println(lxMax);
  out.println();
  out.println(F("; Jogcon Dial max position calibration value (8 to 500)"));
  out.print(F("jogconDialMax=")); out.println(jogconDialMax);
  out.println();
  out.println(F("; Controller operation mode (0: STD, 1: SWAPAB, 2: SWAPLTRT, 3: SWAPAB_SWAPLTRT)"));
  out.print(F("stickMode=")); out.println(stickMode);
  out.println();
  out.println(F("; Analog stick max calibration value (128 to 255)"));
  out.print(F("analogLxMax=")); out.println(analogLxMax);
}

#endif // INIFILE_TEMPLATE_H
