#ifndef INIFILE_TEMPLATE_H
#define INIFILE_TEMPLATE_H

#include <Arduino.h>

// CONFIG.INI の初期テンプレート文字列定数 (PROGMEM)
const char CONFIG_INI_TEMPLATE[] PROGMEM = R"EOF([neGcon]
; neGcon2Xbox_pico CONFIGURATION FILE

; RT/LT analog sensitivity curve (0: LINEAR, 1: A1 (p=2.0), 2: A2 (p=4.0), 3: A3 (p=6.0))
negLtCurve=0
negRtCurve=0

; Twist play reduction (deadzone compensation) value (0 to 32)
negReduceHandlePlay=8

; Twist max angle calibration value (1 to 255)
negTwistMax=100

; Jogcon Dial max position calibration value (8 to 500)
jogconDialMax=100

; Controller operation mode (0: STD, 1: SWAPAB, 2: SWAPLTRT, 3: SWAPAB_SWAPLTRT)
stickMode=0

; Analog stick max calibration value (128 to 255)
analogLxMax=255
)EOF";

#endif // INIFILE_TEMPLATE_H
