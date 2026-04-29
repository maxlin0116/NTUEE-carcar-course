#include <Arduino.h>
#include <SPI.h>

#define DEBUG

#include "../lib/hardware.h"

/*===========================hardware objects================================*/
MFRC522 mfrc522(SS_PIN, RST_PIN);

/*===========================bluetooth config================================*/
#define CUSTOM_NAME "HM10_G10"
constexpr uint8_t HM10_RX_PIN = 15;  // Mega RX3. Pull up when HM-10 is unplugged to avoid floating input noise.
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

/*===========================modules================================*/
#include "../lib/RFID.h"
#include "../lib/bluetooth.h"
#include "../lib/node.h"
#include "../lib/track.h"
#include "../lib/control.h"

/*===========================state================================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;  // IR sensor readings
int _Tp = TRACKING_FAST_TP;                   // Base motor power
constexpr int IR_critical_value = 150;
bool state = false;                           // false = halt, true = active
BT_CMD _cmd = NOTHING;
char queued_node_cmd = 0;

/*===========================initialization helpers================================*/
void setupSerial() {
    pinMode(HM10_RX_PIN, INPUT_PULLUP);
    Serial3.begin(9600);
    Serial3.setTimeout(100);
    Serial.begin(115200);
    while (!Serial) {}
}

void setupBluetooth() {
    Serial.println("Initializing HM-10...");

    for (int i = 0; i < 9; i++) {
        Serial.print("Testing baud rate: ");
        Serial.println(baudRates[i]);

        Serial3.begin(baudRates[i]);
        Serial3.setTimeout(100);
        delay(100);

        if (detectHM10(baudRates[i])) {
            Serial.println("HM-10 detected and ready.");
            moduleReady = true;
            break;
        } else {
            Serial3.end();
            delay(100);
        }
    }

    if (!moduleReady) {
        Serial.println("Failed to detect HM-10. Continuing without Bluetooth.");
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
}

void setupMotorPins() {
    pinMode(MotorR_I1, OUTPUT);
    pinMode(MotorR_I2, OUTPUT);
    pinMode(MotorL_I3, OUTPUT);
    pinMode(MotorL_I4, OUTPUT);
    pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_PWMR, OUTPUT);
}

void setupIrPins() {
    pinMode(IRpin_LL, INPUT);
    pinMode(IRpin_L, INPUT);
    pinMode(IRpin_M, INPUT);
    pinMode(IRpin_R, INPUT);
    pinMode(IRpin_RR, INPUT);
}

void setupRfid() {
    pinMode(53, OUTPUT);  // Mega hardware SS must stay output to keep SPI in master mode.
    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println(F("Read UID on a MIFARE PICC:"));
}

/*===========================arduino entry points================================*/
void setup() {
    setupSerial();
    setupBluetooth();
    setupMotorPins();
    setupIrPins();
    setupRfid();

#ifdef DEBUG
    Serial.println("Start!");
#endif
}

void loop() {
    UIDRead();
    SetState();

    if (!state) {
        MotorWriting(0, 0);
    } else {
        Search();
    }
}
