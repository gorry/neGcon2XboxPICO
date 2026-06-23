#pragma once

#include "Controller.h"

/// <summary>
/// Jogcon の処理クラス
/// </summary>
class JogConController : public Controller {
private:
  byte slx, sb1, sb2, sbL;
  byte jogxForcePower;
  byte jogxPosResetEnable;
public:
  JogConController() : slx(0), sb1(0), sb2(0), sbL(0), jogxForcePower(0), jogxPosResetEnable(0) {}
  void process(ControllerState *state) override;
};
