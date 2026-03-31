#include "Bluetooth.h"

#include <Arduino.h>

#include "CarcarConst.h"
#include "Controllers.h"

namespace {
bool g_module_ready = false;

void sendATCommand(const char* command);
bool waitForResponse(const char* expected, unsigned long timeout);
}

namespace Bluetooth {
void Init() {
  Serial3.begin(9600);
  Serial.begin(115200);
  while (!Serial) {
  }
  Serial.println("Initializing HM-10...");

  for (size_t i = 0; i < CarcarConst::BAUD_RATES_COUNT; i++) {
    Serial.print("Testing baud rate: ");
    Serial.println(CarcarConst::BAUD_RATES[i]);

    Serial3.begin(CarcarConst::BAUD_RATES[i]);
    Serial3.setTimeout(100);
    delay(100);

    Serial3.print("AT");

    if (waitForResponse("OK", 800)) {
      Serial.println("HM-10 detected and ready.");
      g_module_ready = true;
      break;
    } else {
      Serial3.end();
      delay(100);
    }
  }

  if (!g_module_ready) {
    Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
    return;
  }

  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW");
  delay(500);

  Serial.print("Setting name to: ");
  Serial.println(CarcarConst::CUSTOM_NAME);
  String nameCmd = "AT+NAME" + String(CarcarConst::CUSTOM_NAME);
  sendATCommand(nameCmd.c_str());

  Serial.println("Enabling notifications...");
  sendATCommand("AT+NOTI1");

  Serial.println("Querying Bluetooth Address");
  sendATCommand("AT+ADDR?");

  Serial.println("Restarting module...");
  sendATCommand("AT+RESET");
  delay(1000);
  Serial3.begin(9600);

  Serial.println("Initialization Complete.");
}

void RemoteControl() {
  if (Serial3.available()) {
    char c = static_cast<char>(Serial3.read());
    Serial.print("BT received: ");
    Serial.println(c);

    if (c == '0') {
      Controllers::Stop();
    } else if (c == '1') {
      Controllers::TurnRight();
    } else if (c == '2') {
      Controllers::TurnLeft();
    } else if (c == '3') {
      Controllers::UTurn();
    } else if (c == '4') {
      Controllers::NoTurn();
    }
  }

  if (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    Serial.print("USB received: ");
    Serial.println(c);

    if (c == '0') {
      Controllers::Stop();
    } else if (c == '1') {
      Controllers::TurnRight();
    } else if (c == '2') {
      Controllers::TurnLeft();
    } else if (c == '3') {
      Controllers::UTurn();
    } else if (c == '4') {
      Controllers::NoTurn();
    }
  }
}
} // namespace Bluetooth

namespace {
void sendATCommand(const char* command) {
  Serial3.print(command);
  waitForResponse("", 1000);
}

bool waitForResponse(const char* expected, unsigned long timeout) {
  Serial3.setTimeout(timeout);
  String response = Serial3.readString();
  if (response.length() > 0) {
    Serial.print("HM10 Response: ");
    Serial.println(response);
  }
  return (response.indexOf(expected) != -1);
}
} // namespace
