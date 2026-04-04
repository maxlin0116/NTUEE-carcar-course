/***************************************************************************/
#pragma once

#include <Arduino.h>
#include <MFRC522.h>

// Motor pins
constexpr uint8_t MotorR_I1 = 6;
constexpr uint8_t MotorR_I2 = 7;
constexpr uint8_t MotorR_PWMR = 11;
constexpr uint8_t MotorL_I3 = 8;
constexpr uint8_t MotorL_I4 = 9;
constexpr uint8_t MotorL_PWML = 10;

// IR sensor pins
constexpr uint8_t IRpin_LL = A7;
constexpr uint8_t IRpin_L = A6;
constexpr uint8_t IRpin_M = A5;
constexpr uint8_t IRpin_R = A4;
constexpr uint8_t IRpin_RR = A3;

// RFID pins
constexpr uint8_t RST_PIN = 3;
constexpr uint8_t SS_PIN = 2;

extern MFRC522 mfrc522;
