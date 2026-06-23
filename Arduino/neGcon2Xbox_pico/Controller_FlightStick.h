#pragma once

#include "Controller.h"

/// <summary>
/// FlightStick (アナログジョイスティックなど) の処理クラス
/// </summary>
class FlightStickController : public Controller {
private:
  byte slx, sly, srx, sry;
public:
  FlightStickController() : slx(0), sly(0), srx(0), sry(0) {}
  void process(ControllerState *state) override;
};
