#include "Sensors.h"

#include <Arduino.h>

#include "CarcarConst.h"

namespace {
Sensors::Values g_values = {0, 0, 0, 0, 0};
}

namespace Sensors {
void InitPins() {
  pinMode(CarcarConst::L3_PIN, INPUT);
  pinMode(CarcarConst::L2_PIN, INPUT);
  pinMode(CarcarConst::M_PIN, INPUT);
  pinMode(CarcarConst::R2_PIN, INPUT);
  pinMode(CarcarConst::R3_PIN, INPUT);
}

void Read() {
  g_values.l3 = analogRead(CarcarConst::L3_PIN);
  g_values.l2 = analogRead(CarcarConst::L2_PIN);
  g_values.m = analogRead(CarcarConst::M_PIN);
  g_values.r2 = analogRead(CarcarConst::R2_PIN);
  g_values.r3 = analogRead(CarcarConst::R3_PIN);
}

Values Current() {
  return g_values;
}
} // namespace Sensors
