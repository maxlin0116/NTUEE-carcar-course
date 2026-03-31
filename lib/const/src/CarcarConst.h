#ifndef CARCAR_CONST_H
#define CARCAR_CONST_H

#include <Arduino.h>

namespace CarcarConst {
constexpr uint8_t RST_PIN = 3;
constexpr uint8_t SS_PIN = 2;

constexpr char CUSTOM_NAME[] = "HM10_G6";

constexpr uint8_t L3_PIN = A7;
constexpr uint8_t L2_PIN = A6;
constexpr uint8_t M_PIN = A5;
constexpr uint8_t R2_PIN = A4;
constexpr uint8_t R3_PIN = A3;

constexpr uint8_t MOTORL_PWML_PIN = 10;
constexpr uint8_t MOTORL_I1_PIN = 8;
constexpr uint8_t MOTORL_I2_PIN = 9;
constexpr uint8_t MOTORR_I3_PIN = 6;
constexpr uint8_t MOTORR_I4_PIN = 7;
constexpr uint8_t MOTORR_PWMR_PIN = 11;

constexpr int W2 = 5;
constexpr int W3 = 25;
constexpr int IR_CRITICAL_VALUE = 150;
constexpr int TP = 200;
constexpr double KP = 1.0;

constexpr int TURN_TIME_MS = 500;
constexpr int TURN_MODIFY_TIME_MS = 200;

constexpr int OPERATION_LEN = 10;
static const int OPERATION[OPERATION_LEN] = {1, 3, 4, 3, 1, 3, 1, 3, 1, 3};

static const long BAUD_RATES[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
constexpr size_t BAUD_RATES_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);
} // namespace CarcarConst

#endif
