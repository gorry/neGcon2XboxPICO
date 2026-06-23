#pragma once

#include "Controller.h"

/// <summary>
/// neGcon (ねじコン) の処理クラス
/// </summary>
class NeGconController : public Controller {
private:
  byte slx, sb1, sb2, sbL;
  uint16_t lastButtons;
public:
  NeGconController() : slx(0), sb1(0), sb2(0), sbL(0), lastButtons(0) {}
  void process(ControllerState *state) override;
};
