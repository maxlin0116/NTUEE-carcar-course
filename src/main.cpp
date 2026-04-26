#include <Arduino.h>

#define DEBUG

// for RFID
#include <SPI.h>
#include "../lib/hardware.h"

/*===========================define pin & create module object================================*/
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Bluetooth pins
#define CUSTOM_NAME "HM10_G10"
constexpr uint8_t HM10_RX_PIN = 15;  // Mega RX3. Pull up when HM-10 is unplugged to avoid floating input noise.
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

#include "../lib/RFID.h"		// RFID reading functions
#include "../lib/bluetooth.h"	// Bluetooth communication functions
#include "../lib/node.h"		// Node command definitions and queue management
#include "../lib/track.h"		// IR sensor reading and line tracking functions
#include "../lib/control.h"		// Motor control functions

/*===========================setup================================*/
void setup() {
    pinMode(HM10_RX_PIN, INPUT_PULLUP);
    Serial3.begin(9600);									// Start with a common baud rate for HM-10
	Serial3.setTimeout(100);								// Set a short timeout for AT command responses
    Serial.begin(115200);									// Start serial for debugging
    while (!Serial) {}										// Wait for serial to be ready

	// Initialize Bluetooth module
    Serial.println("Initializing HM-10...");

	// Try different baud rates to find the HM-10
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

	// If we couldn't detect the module, continue without Bluetooth so
	// RFID and local serial debugging still work.
    if (!moduleReady) {
        Serial.println("Failed to detect HM-10. Continuing without Bluetooth.");
    } else {
	    // Configure HM-10 settings
        Serial.println("Restoring factory defaults...");
        sendATCommand("AT+RENEW");
        delay(500);

	    // Set the device name
        Serial.print("Setting name to: ");
        Serial.println(CUSTOM_NAME);
        String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
        sendATCommand(nameCmd.c_str());

	    // Enable notifications for the HM-10
        Serial.println("Enabling notifications...");
        sendATCommand("AT+NOTI1");

	    // Query the Bluetooth address for reference
        Serial.println("Querying Bluetooth Address");
        sendATCommand("AT+ADDR?");

	    // Reset the module to apply settings
        Serial.println("Restarting module...");
        sendATCommand("AT+RESET");
        delay(1000);
        Serial3.begin(9600);

	    // Final confirmation
        Serial.println("Initialization Complete.");
    }

	// Set motor control pins as outputs
    pinMode(MotorR_I1, OUTPUT);
    pinMode(MotorR_I2, OUTPUT);
    pinMode(MotorL_I3, OUTPUT);
    pinMode(MotorL_I4, OUTPUT);
    pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_PWMR, OUTPUT);

	// Set IR sensor pins as inputs
    pinMode(IRpin_LL, INPUT);
    pinMode(IRpin_L, INPUT);
    pinMode(IRpin_M, INPUT);
    pinMode(IRpin_R, INPUT);
    pinMode(IRpin_RR, INPUT);

	// Initialize SPI and RFID reader
    pinMode(53, OUTPUT);  // Mega hardware SS must stay output to keep SPI in master mode.
    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println(F("Read UID on a MIFARE PICC:"));

	// Final debug message
	#ifdef DEBUG
		Serial.println("Start!");
	#endif
}

/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;	// IR sensor readings	
int _Tp = 180;								// Base motor power
constexpr int IR_critical_value = 150;		// Threshold for determining if the sensor is over a line (black) or not (white)
bool state = false;							// State of the car: false = halt, true = active
BT_CMD _cmd = NOTHING;						// Enum for Bluetooth commands, defined in bluetooth.h
char queued_node_cmd = 0;					// Queue for node commands received via Bluetooth when at a node

/*===========================main function===========================*/
void loop() {
    UIDRead();
    SetState();

    if (!state) {
        MotorWriting(0, 0);
    } else {
        Search();
    }
}
