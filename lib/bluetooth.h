/***************************************************************************/
#pragma once

#include <Arduino.h>

// bluetooth.h
/***************************************************************************/
// File       [bluetooth.h]
// Author     [Max Lin]
// Synopsis   [Code for managing Bluetooth communication]
// Functions  [ask_BT, send_msg, send_byte, waitForResponse, sendATCommand]
// Modify     [2026/04/04 Max Lin]
/***************************************************************************/


// Bluetooth command definitions
enum BT_CMD {
    NOTHING,
    GO,
    HALT,
    LEFT,
    RIGHT,
    BACK,
    STOP,
    FORWARD,
};

// ask Bluetooth for command, return NOTHING if no command received
inline BT_CMD ask_BT() {
    BT_CMD message = NOTHING;
    char cmd;
    if (Serial3.available()) {
        cmd = Serial3.read();
        switch (cmd) {
            case 'G':
                message = GO;
                break;
            case 'H':
                message = HALT;
                break;
            case 'L':
                message = LEFT;
                break;
            case 'R':
                message = RIGHT;
                break;
            case 'B':
                message = BACK;
                break;
            case 'S':
                message = STOP;
                break;
            case 'F':
                message = FORWARD;
                break;
            default:
                message = NOTHING;
                break;
        }
#ifdef DEBUG
        Serial.print("cmd : ");
        Serial.println(cmd);
#endif
    }
    return message;
}

// send msg back through Serial3(bluetooth serial)
inline void send_msg(const char& msg) {
    Serial3.write(msg);
}

// send UID back through Serial3(bluetooth serial)
inline void send_byte(byte* id, byte& idSize) {
    for (byte i = 0; i < idSize; i++) {  // Send UID consequently.
        Serial3.print(id[i]);
    }
	#ifdef DEBUG
		Serial.print("Sent id: ");
		for (byte i = 0; i < idSize; i++) {  // Show UID consequently.
			Serial.print(id[i], HEX);
		}
		Serial.println();
	#endif
}

// Wait for a specific response from the Bluetooth module within a timeout period
inline bool waitForResponse(const char* expected, unsigned long timeout) {
    Serial3.setTimeout(timeout);
    String response = Serial3.readString();
    if (response.length() > 0) {
        Serial.print("HM10 Response: ");
        Serial.println(response);
    }
    return response.indexOf(expected) != -1;
}

// Send an AT command to the Bluetooth module and wait for a response
inline void sendATCommand(const char* command) {
    Serial3.print(command);
    waitForResponse("", 1000);
}
