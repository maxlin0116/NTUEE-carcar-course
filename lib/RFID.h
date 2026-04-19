/***************************************************************************/
#pragma once

/***************************************************************************/
// File       [RFID.h]
// Author     [Erik Kuo]
// Synopsis   [Code for getting UID from RFID card]
// Functions  [rfid, UIDRead]
// Modify     [2026/04/19]
/***************************************************************************/

#include <MFRC522.h>
#include <SPI.h>

#include "hardware.h"

/*
 * Arduino Mega 2560 RC522 wiring used by this project:
 * SDA/SS -> D2
 * SCK    -> D52
 * MOSI   -> D51
 * MISO   -> D50
 * RST    -> D3
 * VCC    -> 3.3V
 * GND    -> GND
 */

inline byte* rfid(byte& idSize) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        byte* id = mfrc522.uid.uidByte;
        idSize = mfrc522.uid.size;

#ifdef DEBUG
        Serial.print("UID Size: ");
        Serial.println(idSize);
        for (byte i = 0; i < idSize; i++) {
            Serial.print("id[");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(id[i], HEX);
        }
        Serial.println();
#endif

        mfrc522.PICC_HaltA();
        return id;
    }
    return 0;
}

inline void UIDRead() {
    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }

    uid.toUpperCase();

    Serial.print("UID:");
    Serial.println(uid);
    Serial3.print("UID:");
    Serial3.println(uid);

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}
