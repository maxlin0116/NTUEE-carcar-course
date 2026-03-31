#include "Controllers.h"

#include <Arduino.h>

#include "CarcarConst.h"
#include "Motors.h"
#include "RFID.h"
#include "Sensors.h"

namespace {
int g_current_order = 0;

double clampPower(double v) {
  if (v > 255) {
    return 255;
  }
  if (v < -255) {
    return -255;
  }
  return v;
}

void action(int order) {
  int op = CarcarConst::OPERATION[order];
  if (op == 0) {
    Controllers::Stop();
  } else if (op == 1) {
    Controllers::TurnRight();
  } else if (op == 2) {
    Controllers::TurnLeft();
  } else if (op == 3) {
    Controllers::UTurn();
  } else if (op == 4) {
    Controllers::NoTurn();
  }
}
} // namespace

namespace Controllers {
void TurnRight() {
  Motors::Write(CarcarConst::TP, 0);
  delay(CarcarConst::TURN_TIME_MS);
  Sensors::Read();

  auto values = Sensors::Current();
  if (values.l2 < CarcarConst::IR_CRITICAL_VALUE && values.m < CarcarConst::IR_CRITICAL_VALUE &&
      values.r2 < CarcarConst::IR_CRITICAL_VALUE) {
    Motors::Write(CarcarConst::TP, 0);
    delay(CarcarConst::TURN_MODIFY_TIME_MS);
  }
}

void TurnLeft() {
  Motors::Write(0, CarcarConst::TP);
  delay(CarcarConst::TURN_TIME_MS);
  Sensors::Read();

  auto values = Sensors::Current();
  if (values.l2 < CarcarConst::IR_CRITICAL_VALUE && values.m < CarcarConst::IR_CRITICAL_VALUE &&
      values.r2 < CarcarConst::IR_CRITICAL_VALUE) {
    Motors::Write(0, CarcarConst::TP);
    delay(CarcarConst::TURN_MODIFY_TIME_MS);
  }
}

void UTurn() {
  RFID::ReadUid();
  Motors::Write(CarcarConst::TP + 30, -CarcarConst::TP - 30);
  delay(CarcarConst::TURN_TIME_MS);
  Sensors::Read();

  auto values = Sensors::Current();
  if (values.l2 < CarcarConst::IR_CRITICAL_VALUE && values.m < CarcarConst::IR_CRITICAL_VALUE &&
      values.r2 < CarcarConst::IR_CRITICAL_VALUE) {
    Motors::Write(CarcarConst::TP, -CarcarConst::TP);
    delay(CarcarConst::TURN_MODIFY_TIME_MS);
  }
}

void NoTurn() {
  Motors::Write(CarcarConst::TP, CarcarConst::TP);
  delay(CarcarConst::TURN_TIME_MS);
  Sensors::Read();

  auto values = Sensors::Current();
  if (values.l2 > CarcarConst::IR_CRITICAL_VALUE && values.m > CarcarConst::IR_CRITICAL_VALUE &&
      values.r2 > CarcarConst::IR_CRITICAL_VALUE) {
    Motors::Write(CarcarConst::TP, CarcarConst::TP);
    delay(CarcarConst::TURN_MODIFY_TIME_MS);
  }
}

void Stop() {
  Motors::Write(0, 0);
  delay(CarcarConst::TURN_TIME_MS);
}

void Tracking() {
  Sensors::Read();
  auto values = Sensors::Current();

  if (values.l2 > CarcarConst::IR_CRITICAL_VALUE && values.m > CarcarConst::IR_CRITICAL_VALUE &&
      values.r2 > CarcarConst::IR_CRITICAL_VALUE) {
    while (values.l2 > CarcarConst::IR_CRITICAL_VALUE && values.m > CarcarConst::IR_CRITICAL_VALUE &&
           values.r2 > CarcarConst::IR_CRITICAL_VALUE) {
      Motors::Write(CarcarConst::TP, CarcarConst::TP);
      Sensors::Read();
      values = Sensors::Current();
    }

    action(g_current_order);
    g_current_order++;
  } else {
    double error = (values.l3 * (-CarcarConst::W3) + values.l2 * (-CarcarConst::W2) +
                    values.r2 * CarcarConst::W2 + values.r3 * CarcarConst::W3) /
                   (values.l3 + values.l2 + values.m + CarcarConst::W2 + CarcarConst::W3);

    double powerCorrection = CarcarConst::KP * error;
    double vR = CarcarConst::TP - powerCorrection;
    double vL = CarcarConst::TP + 7 * powerCorrection;

    Motors::Write(clampPower(vL), clampPower(vR));
  }
}
} // namespace Controllers
