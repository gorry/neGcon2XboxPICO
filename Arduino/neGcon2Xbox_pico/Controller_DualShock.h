#pragma once

#include "Controller.h"

/// <summary>
/// DualShock / DualShock 2 の処理クラス
/// </summary>
class DualShockController : public Controller {
private:
  byte slx, sly, srx, sry;
public:
  DualShockController() : slx(0), sly(0), srx(0), sry(0) {}
  void process(ControllerState *state) override;
};
