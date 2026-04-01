#include <Arduino.h>

/***************************************************************************/
// File       [final_project.ino]
// Author     [Erik Kuo]
// Synopsis   [Code for managing main process]
// Functions  [setup, loop, Search_Mode, Hault_Mode, SetState]
// Modify     [2020/03/27 Erik Kuo]
/***************************************************************************/

#define DEBUG

// for RFID
#include <MFRC522.h>
#include <SPI.h>

/*===========================define pin & create module object================================*/
// BlueTooth
// BT connect to Serial1 (Hardware Serial)
// Mega               HC05
// Pin  (Function)    Pin
// 18    TX       ->  RX
// 19    RX       <-  TX
#define MotorR_I1 6
#define MotorR_I2 7
#define MotorR_PWMR 11
#define MotorL_I3 8
#define MotorL_I4 9
#define MotorL_PWML 10

#define IRpin_LL A7
#define IRpin_L A6
#define IRpin_M A5
#define IRpin_R A4
#define IRpin_RR A3

#define RST_PIN 3
#define SS_PIN 2
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define CUSTOM_NAME "HM10_G6"
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

/*===========================define pin & create module object===========================*/

bool waitForResponse(const char* expected, unsigned long timeout);
void sendATCommand(const char* command);
void UIDRead();

/*============setup============*/
void setup() {
    Serial3.begin(9600);
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("Initializing HM-10...");

    for (int i = 0; i < 9; i++) {
        Serial.print("Testing baud rate: ");
        Serial.println(baudRates[i]);

        Serial3.begin(baudRates[i]);
        Serial3.setTimeout(100);
        delay(100);
        Serial3.print("AT");

        if (waitForResponse("OK", 800)) {
            Serial.println("HM-10 detected and ready.");
            moduleReady = true;
            break;
        } else {
            Serial3.end();
            delay(100);
        }
    }

    if (!moduleReady) {
        Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
        return;
    }

    Serial.println("Restoring factory defaults...");
    sendATCommand("AT+RENEW");
    delay(500);

    Serial.print("Setting name to: ");
    Serial.println(CUSTOM_NAME);
    String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
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

    pinMode(MotorR_I1, OUTPUT);
    pinMode(MotorR_I2, OUTPUT);
    pinMode(MotorL_I3, OUTPUT);
    pinMode(MotorL_I4, OUTPUT);
    pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_PWMR, OUTPUT);

    pinMode(IRpin_LL, INPUT);
    pinMode(IRpin_L, INPUT);
    pinMode(IRpin_M, INPUT);
    pinMode(IRpin_R, INPUT);
    pinMode(IRpin_RR, INPUT);

    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println(F("Read UID on a MIFARE PICC:"));
#ifdef DEBUG
    Serial.println("Start!");
#endif
}
/*============setup============*/

void MotorWriting(double vL, double vR);

/*=====Import header files=====*/
#include "../lib/RFID.h"
#include "../lib/bluetooth.h"
#include "../lib/node.h"
#include "../lib/track.h"
/*=====Import header files=====*/

/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;
int _Tp = 200;
constexpr int IR_critical_value = 150;
bool state = false;
BT_CMD _cmd = NOTHING;
char queued_node_cmd = 0;
/*===========================initialize variables===========================*/

/*===========================declare function prototypes===========================*/
void Search();
void SetState();
bool IsRunCommand(char cmd);
bool IsHaltCommand(char cmd);
bool HandleMotorTestCommand(char cmd);
/*===========================declare function prototypes===========================*/

/*===========================define function===========================*/
void loop() {
    UIDRead();
    SetState();

    if (!state) {
        MotorWriting(0, 0);
    } else {
        Search();
    }
}

void SetState() {
    while (Serial3.available()) {
        const char cmd = Serial3.read();
#ifdef DEBUG
        Serial.print("BT state cmd: ");
        if (cmd == '\r') {
            Serial.println("\\r");
        } else if (cmd == '\n') {
            Serial.println("\\n");
        } else {
            Serial.println(cmd);
        }
#endif
        if (IsRunCommand(cmd)) {
            state = true;
#ifdef DEBUG
            Serial.print("State -> RUN via ");
            Serial.println(cmd);
#endif
        } else if (IsHaltCommand(cmd)) {
            state = false;
#ifdef DEBUG
            Serial.print("State -> HALT via ");
            Serial.println(cmd);
#endif
        } else {
            if (!HandleMotorTestCommand(cmd)) {
                queued_node_cmd = cmd;
#ifdef DEBUG
                Serial.print("Queued node cmd: ");
                Serial.println(cmd);
#endif
            }
        }
    }
}

void Search() {
    static char pending_cmd = 0;
    static bool waiting_at_node = false;
    const int raw_l2 = analogRead(IRpin_LL);
    const int raw_l1 = analogRead(IRpin_L);
    const int raw_m0 = analogRead(IRpin_M);
    const int raw_r1 = analogRead(IRpin_R);
    const int raw_r2 = analogRead(IRpin_RR);

    l2 = raw_l2;
    l1 = raw_l1;
    m0 = raw_m0;
    r1 = raw_r1;
    r2 = raw_r2;

    if (queued_node_cmd) {
        pending_cmd = queued_node_cmd;
        queued_node_cmd = 0;
#ifdef DEBUG
        Serial.print("Search cmd: ");
        Serial.println(pending_cmd);
#endif
    }

    const bool at_node = node_is_active();
#ifdef DEBUG
    Serial.print("RAW ");
    Serial.print(raw_l2);
    Serial.print(" ");
    Serial.print(raw_l1);
    Serial.print(" ");
    Serial.print(raw_m0);
    Serial.print(" ");
    Serial.print(raw_r1);
    Serial.print(" ");
    Serial.println(raw_r2);
    Serial.print("IR ");
    Serial.print(l2);
    Serial.print(" ");
    Serial.print(l1);
    Serial.print(" ");
    Serial.print(m0);
    Serial.print(" ");
    Serial.print(r1);
    Serial.print(" ");
    Serial.print(r2);
    Serial.print(" | at_node=");
    Serial.println(at_node);
#endif

    if (waiting_at_node) {
        MotorWriting(0, 0);
        if (!pending_cmd) {
#ifdef DEBUG
            Serial.println("Waiting at node");
#endif
            return;
        }

        while (node_is_active()) {
            MotorWriting(_Tp, _Tp);
        }
        MotorWriting(0, 0);
        delay(60);
        waiting_at_node = false;

        switch (pending_cmd) {
            case 'L':
            case 'l':
            case '2':
                node_left_turn();
                break;
            case 'R':
            case 'r':
            case '1':
                node_right_turn();
                break;
            case 'B':
            case 'b':
            case 'U':
            case 'u':
            case '3':
                node_u_turn();
                break;
            case 'S':
            case 's':
            case '0':
                node_stop();
                break;
            case 'F':
            case 'f':
            case '4':
            default:
                node_forward();
                break;
        }

        pending_cmd = 0;
        return;
    }

    if (!at_node) {
        tracking(l2, l1, m0, r1, r2);
        return;
    }

    MotorWriting(0, 0);

    if (!pending_cmd) {
        waiting_at_node = true;
        MotorWriting(0, 0);
        return;
    }

    switch (pending_cmd) {
        case 'L':
        case 'l':
        case '2':
            node_left_turn();
            break;
        case 'R':
        case 'r':
        case '1':
            node_right_turn();
            break;
        case 'B':
        case 'b':
        case '3':
            node_u_turn();
            break;
        case 'S':
        case 's':
        case '0':
            node_stop();
            break;
        case 'F':
        case 'f':
        case '4':
        default:
            node_forward();
            break;
    }

    pending_cmd = 0;
}

bool IsRunCommand(char cmd) {
    return cmd == 'G' || cmd == 'g' || cmd == 'A' || cmd == 'a' || cmd == 'T' || cmd == 't';
}

bool IsHaltCommand(char cmd) {
    return cmd == 'H' || cmd == 'h' || cmd == 'P' || cmd == 'p' || cmd == 'X' || cmd == 'x';
}

bool HandleMotorTestCommand(char cmd) {
    switch (cmd) {
        case '5':
            Serial.println("TEST: MotorWriting(_Tp, _Tp)");
            MotorWriting(_Tp, _Tp);
            delay(500);
            MotorWriting(0, 0);
            return true;
        case '6':
            Serial.println("TEST: MotorWriting(_Tp, 0)");
            MotorWriting(_Tp, 0);
            delay(500);
            MotorWriting(0, 0);
            return true;
        case '7':
            Serial.println("TEST: MotorWriting(0, _Tp)");
            MotorWriting(0, _Tp);
            delay(500);
            MotorWriting(0, 0);
            return true;
        case '8':
            Serial.println("TEST: MotorWriting(_Tp, -_Tp)");
            MotorWriting(_Tp, -_Tp);
            delay(500);
            MotorWriting(0, 0);
            return true;
        case '9':
            Serial.println("TEST: MotorWriting(-_Tp, _Tp)");
            MotorWriting(-_Tp, _Tp);
            delay(500);
            MotorWriting(0, 0);
            return true;
        default:
            return false;
    }
}

void UIDRead() {
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
    return response.indexOf(expected) != -1;
}
/*===========================define function===========================*/

#if 0
Original `src/main.cpp` backup:

#include <Arduino.h>

//bluetooth setup
#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN 3
#define SS_PIN 2
MFRC522 *mfrc522;
#define CUSTOM_NAME "HM10_G6"
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

bool waitForResponse(const char* expected, unsigned long timeout);
void sendATCommand(const char* command);
void UIDRead();

//defining pin for IR sensor
#define L3 A7
#define L2 A6
#define M A5
#define R2 A4
#define R3 A3

//define the pin_name for motor
int MotorL_PWML = 10;
int MotorL_I1 = 8;
int MotorL_I2 = 9;
int MotorR_I3 = 6;
int MotorR_I4 = 7;
int MotorR_PWMR = 11;


//define the used variable            `
int l2, l3, m, r2, r3;                      // the sensor value form the IR sensor or from the left to the right
double vL, vR;                              // the power given to the left and right motor
int w2 = 5;                                 // the weighted value of L2 and R2 IR sensor
int w3 = 25;                                // the weighted value of L3 and R3 IR sensor
double error;                               // the value that tells how left or right the car relative to the black line
int powerCorrection;                        // the value to modify the power of the left and right motor (proportional to error)
double Kp = 1;                              // proportion constant between error and powerCorrection
int Tp = 200;                               // the base power given to the left and right motor
int operation[10] = {1,3,4,3,1,3,1,3,1,3};  // the list to store the move of the operation
//int operation[6] = {3,3,3,3,3,3};
int current_order = 0;                      // the current operating operation index
int turn_time = 500;                        // the main turn-time
int turn_modify_time = 200;                 // the addition time to turn if the main turn is not enough
int IR_critical_value = 150;                // the IR sensor critical value


//set up the pin mode and remote connection with HM-10
void setup() {
  Serial3.begin(9600);  // to HM-10 (Bluetooth Module)
  Serial.begin(115200); // to USB (Serial Monitor)
  while (!Serial);
  Serial.println("Initializing HM-10...");

  // 1. Automatic Baud Rate Detection
  for (int i = 0; i < 9; i++) {
    Serial.print("Testing baud rate: ");
    Serial.println(baudRates[i]);

    Serial3.begin(baudRates[i]);
    Serial3.setTimeout(100);
    delay(100);

    // 2. Force Disconnection
    // Sending "AT" while connected forces the module to disconnect [2].
    Serial3.print("AT");

    if (waitForResponse("OK", 800)) {
      Serial.println("HM-10 detected and ready.");
      moduleReady = true;
      break;
    } else {
      Serial3.end();
      delay(100);
    }
  }

  if (!moduleReady) {
    Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
    return;
  }

  // 3. Restore Factory Defaults
  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW"); // Restores all setup values
  delay(500);

  // 4. Set Custom Name via Macro
  Serial.print("Setting name to: ");
  Serial.println(CUSTOM_NAME);
  String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
  sendATCommand(nameCmd.c_str()); // Max length is 12

  // 5. Enable Connection Notifications
  Serial.println("Enabling notifications...");
  sendATCommand("AT+NOTI1"); // Notify when link is established/lost

  // 6. Get the Bluetooth MAC Address
  Serial.println("Querying Bluetooth Address");
  sendATCommand("AT+ADDR?");

  // 7. Restart the module to apply changes
  Serial.println("Restarting module...");
  sendATCommand("AT+RESET"); // Restart the module
  delay(1000);
  Serial3.begin(9600); // Now the module would use baudrate 9600

  Serial.println("Initialization Complete.");

  pinMode(MotorL_PWML, OUTPUT);
  pinMode(MotorR_PWMR, OUTPUT);
  pinMode(MotorL_I1, OUTPUT);
  pinMode(MotorL_I2, OUTPUT);
  pinMode(MotorR_I3, OUTPUT);
  pinMode(MotorR_I4, OUTPUT);

  pinMode(L3, INPUT);
  pinMode(L2, INPUT);
  pinMode(M, INPUT);
  pinMode(R2, INPUT);
  pinMode(R3, INPUT);

  SPI.begin();
  mfrc522 = new MFRC522(SS_PIN, RST_PIN);
  mfrc522->PCD_Init();
  Serial.println(F("Read UID on a MIFARE PICC:"));
}

//function used to control the left and right motor
void MotorWriting(double vL, double vR) {
  if (vL >= 0) {
    digitalWrite(MotorL_I1, LOW);
    digitalWrite(MotorL_I2, HIGH);
  }
  else {
    digitalWrite(MotorL_I1, HIGH);
    digitalWrite(MotorL_I2, LOW);
    vL = -vL;
  }

  if (vR >= 0) {
    digitalWrite(MotorR_I3, HIGH);
    digitalWrite(MotorR_I4, LOW);
  }
  else {
    digitalWrite(MotorR_I3, LOW);
    digitalWrite(MotorR_I4, HIGH);
    vR = -vR;
  }

  analogWrite(MotorL_PWML, vL);
  analogWrite(MotorR_PWMR, vR);
}

// return the sensor value of the IR sensor
void sensor() {
  l3 = analogRead(L3);
  l2 = analogRead(L2);
  m = analogRead(M);
  r2 = analogRead(R2);
  r3 = analogRead(R3);
}

void turn_right()
{
  MotorWriting(Tp, 0);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(Tp, 0);
    delay(turn_modify_time);
  }
}

void turn_left()
{
  MotorWriting(0, Tp);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(0, Tp);
    delay(turn_modify_time);
  }
}

void U_turn()
{
  MotorWriting(Tp+30, -Tp-30);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(Tp, -Tp);
    delay(turn_modify_time);
  }
}

void no_turn()
{
  MotorWriting(Tp, Tp);
  delay(turn_time);
  sensor();
  if (l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)
  {
    MotorWriting(Tp, Tp);
    delay(turn_modify_time);
  }
}

void stop()
{
  MotorWriting(0, 0);
  delay(turn_time);
}

void action(int order)
{
  if(operation[order] == 0) stop();
  if(operation[order] == 1) turn_right();
  if(operation[order] == 2) turn_left();
  if(operation[order] == 3) U_turn();
  if(operation[order] == 4) no_turn();
}

void Tracking() {
  sensor();

  if (l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)
  // detect the black block
  {
    while(l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)
    {
      MotorWriting(Tp, Tp);
      sensor();
    }
    action(current_order);
    current_order++;
  }
  else  // not detect the black block
  {
    sensor();
    error = (l3*(-w3) + l2*(-w2) + r2*w2 + r3*w3)/(l3 + l2 + m + w2 + w3);
    powerCorrection = Kp * error;
    vR = Tp - powerCorrection;
    vL = Tp + 5*powerCorrection;
    if(vR>255) vR = 255;
    if(vL>255) vL = 255;
    if(vR<-255) vR = -255;
    if(vL<-255) vL = -255;
    MotorWriting(vL, vR);
  }
}

void UIDRead() {
  if (!mfrc522->PICC_IsNewCardPresent()) return;
  if (!mfrc522->PICC_ReadCardSerial()) return;

  String uid = "";

  for (byte i = 0; i < mfrc522->uid.size; i++) {
    if (mfrc522->uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522->uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  // to USB (Serial Monitor)
  Serial.print("UID:");
  Serial.println(uid);

  // to HM-10 (Bluetooth Module)
  Serial3.print("UID:");
  Serial3.println(uid);

  mfrc522->PICC_HaltA();
  mfrc522->PCD_StopCrypto1();
}

void remote_control() {
  if (Serial3.available()) {
    char c = Serial3.read();
    Serial.print("BT received: ");
    Serial.println(c);

    if (c == '0') stop();
    else if (c == '1') turn_right();
    else if (c == '2') turn_left();
    else if (c == '3') U_turn();
    else if (c == '4') no_turn();
  }

  if (Serial.available()) {
    char c = Serial.read();
    Serial.print("USB received: ");
    Serial.println(c);

    if (c == '0') stop();
    else if (c == '1') turn_right();
    else if (c == '2') turn_left();
    else if (c == '3') U_turn();
    else if (c == '4') no_turn();
  }
}

void loop(){
  //Tracking();
  UIDRead();
  remote_control();

  // --------- below are the test code ---------
  // MotorWriting(150, 150);
  // Serial.println("error");
  // Serial.println(powerCorrection);
  // sensor();
  // Serial.println("l3");
  // Serial.println(l3);
  // Serial.println("l2");
  // Serial.println(l2);
  // Serial.println("m");
  // Serial.println(m);
  // Serial.println("r2");
  // Serial.println(r2);
  // Serial.println("r3");
  // Serial.println(r3);
  // delay(300);
}

// Helper to send AT commands (Uppercase, no \r or \n) [6]
void sendATCommand(const char* command) {
  Serial3.print(command);
  waitForResponse("", 1000);
}

// Helper to check response for specific substrings
bool waitForResponse(const char* expected, unsigned long timeout) {
  Serial3.setTimeout(timeout);
  String response = Serial3.readString();
  if (response.length() > 0) {
    Serial.print("HM10 Response: ");
    Serial.println(response);
  }
  return (response.indexOf(expected) != -1);
}
#endif
