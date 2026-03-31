#include "RFID.h"

#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

#include "CarcarConst.h"

namespace {
MFRC522* g_mfrc522 = nullptr;
}

namespace RFID {
void Init() {
  SPI.begin();
  g_mfrc522 = new MFRC522(CarcarConst::SS_PIN, CarcarConst::RST_PIN);
  g_mfrc522->PCD_Init();
  Serial.println(F("Read UID on a MIFARE PICC:"));
}

void ReadUid() {
  if (!g_mfrc522) {
    return;
  }
  if (!g_mfrc522->PICC_IsNewCardPresent()) {
    return;
  }
  if (!g_mfrc522->PICC_ReadCardSerial()) {
    return;
  }

  String uid = "";

  for (byte i = 0; i < g_mfrc522->uid.size; i++) {
    if (g_mfrc522->uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(g_mfrc522->uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  Serial.print("UID:");
  Serial.println(uid);

  Serial3.print("UID:");
  Serial3.println(uid);

  g_mfrc522->PICC_HaltA();
  g_mfrc522->PCD_StopCrypto1();
}
} // namespace RFID
