/***************************************************************************/
// File			  [bluetooth.h]
// Author		  [Erik Kuo]
// Synopsis		[Code for bluetooth communication]
// Functions  [ask_BT, send_msg, send_byte]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 2*/

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

BT_CMD ask_BT() {
    BT_CMD message = NOTHING;
    char cmd;
    if (Serial3.available()) {
        cmd = Serial3.read();
        switch (cmd) {
            case 'G':
            case 'g':
            case 'A':
            case 'a':
            case 'T':
            case 't':
                message = GO;
                break;
            case 'H':
            case 'h':
            case 'P':
            case 'p':
            case 'X':
            case 'x':
                message = HALT;
                break;
            case 'L':
            case 'l':
            case '2':
                message = LEFT;
                break;
            case 'R':
            case 'r':
            case '1':
                message = RIGHT;
                break;
            case 'B':
            case 'b':
            case 'U':
            case 'u':
            case '3':
                message = BACK;
                break;
            case 'S':
            case 's':
            case '0':
                message = STOP;
                break;
            case 'F':
            case 'f':
            case '4':
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
}  // ask_BT

// send msg back through Serial1(bluetooth serial)
// can use send_byte alternatively to send msg back
// (but need to convert to byte type)
void send_msg(const char& msg) {
    Serial3.write(msg);
}  // send_msg

// send UID back through Serial3(bluetooth serial)
void send_byte(byte* id, byte& idSize) {
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
}  // send_byte
