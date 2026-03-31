#include "Motors.h"

#include <Arduino.h>

#include "CarcarConst.h"

namespace Motors {
void InitPins() {
  pinMode(CarcarConst::MOTORL_PWML_PIN, OUTPUT);
  pinMode(CarcarConst::MOTORR_PWMR_PIN, OUTPUT);
  pinMode(CarcarConst::MOTORL_I1_PIN, OUTPUT);
  pinMode(CarcarConst::MOTORL_I2_PIN, OUTPUT);
  pinMode(CarcarConst::MOTORR_I3_PIN, OUTPUT);
  pinMode(CarcarConst::MOTORR_I4_PIN, OUTPUT);
}

void Write(double vL, double vR) {
  if (vL >= 0) {
    digitalWrite(CarcarConst::MOTORL_I1_PIN, LOW);
    digitalWrite(CarcarConst::MOTORL_I2_PIN, HIGH);
  } else {
    digitalWrite(CarcarConst::MOTORL_I1_PIN, HIGH);
    digitalWrite(CarcarConst::MOTORL_I2_PIN, LOW);
    vL = -vL;
  }

  if (vR >= 0) {
    digitalWrite(CarcarConst::MOTORR_I3_PIN, HIGH);
    digitalWrite(CarcarConst::MOTORR_I4_PIN, LOW);
  } else {
    digitalWrite(CarcarConst::MOTORR_I3_PIN, LOW);
    digitalWrite(CarcarConst::MOTORR_I4_PIN, HIGH);
    vR = -vR;
  }

  analogWrite(CarcarConst::MOTORL_PWML_PIN, vL);
  analogWrite(CarcarConst::MOTORR_PWMR_PIN, vR);
}
} // namespace Motors
