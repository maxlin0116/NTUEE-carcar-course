/***************************************************************************/
// File       [final_project.ino]
// Author     [Erik Kuo]
// Synopsis   [Code for managing main process]
// Functions  [setup, loop, Search_Mode, Hault_Mode, SetState]
// Modify     [2020/03/27 Erik Kuo]
/***************************************************************************/

#define DEBUG  // debug flag

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
// TB6612, ?ｇ????荔?０?????鈭?????(???輸????堊奕?????蟡?????
#define MotorR_I1 6     // ?堊垓??A1 ?鈭伐???踹??
#define MotorR_I2 7     // ?堊垓??A2 ?鈭伐???踹??
#define MotorR_PWMR 11  // ?堊垓??ENA (PWM?方撓?? ?鈭伐
#define MotorL_I3 8     // ?堊垓??B1 ?鈭伐??璊啣??
#define MotorL_I4 9     // ?堊垓??B2 ?鈭伐??璊啣??
#define MotorL_PWML 10  // ?堊垓??ENB (PWM?方撓?? ?鈭伐
// ?箄????, ?ｇ????荔?０?????鈭?????
#define IRpin_LL A7
#define IRpin_L A6
#define IRpin_M A5
#define IRpin_R A4
#define IRpin_RR A3
// RFID, ?ｇ????荔?０?????鈭?????
#define RST_PIN 3                 // ????????菜?賡?
#define SS_PIN 2                  // ????鞊????
MFRC522 mfrc522(SS_PIN, RST_PIN);  // ?梁??MFRC522??麾
// BT
#define CUSTOM_NAME "HM10_Mega" // Max length is 12 characters [1]

/*===========================define pin & create module object===========================*/

/*============setup============*/
void setup() {
    // bluetooth initialization
    Serial3.begin(9600);  
    // Serial window
    Serial.begin(9600);
    // RFID initial
    SPI.begin();
    mfrc522.PCD_Init();
    // TB6612 pin
    pinMode(MotorR_I1, OUTPUT);
    pinMode(MotorR_I2, OUTPUT);
    pinMode(MotorL_I3, OUTPUT);
    pinMode(MotorL_I4, OUTPUT);
    pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_PWMR, OUTPUT);
    // tracking pin
    pinMode(IRpin_LL, INPUT);
    pinMode(IRpin_L, INPUT);
    pinMode(IRpin_M, INPUT);
    pinMode(IRpin_R, INPUT);
    pinMode(IRpin_RR, INPUT);
#ifdef DEBUG
    Serial.println("Start!");
#endif
}
/*============setup============*/

/*=====Import header files=====*/
#include "RFID.h"
#include "bluetooth.h"
#include "node.h"
#include "track.h"
/*=====Import header files=====*/

/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;  // ???綜垮??????0->white,1->black)
int _Tp = 90;                                // set your own value for motor power
constexpr int IR_critical_value = 150;
bool state = false;     // set state to false to halt the car, set state to true to activate the car
BT_CMD _cmd = NOTHING;  // enum for bluetooth message, reference in bluetooth.h line 2
/*===========================initialize variables===========================*/

/*===========================declare function prototypes===========================*/
void Search();    // search graph
void SetState();  // switch the state
/*===========================declare function prototypes===========================*/

/*===========================define function===========================*/
void loop() {
    if (!state)
        MotorWriting(0, 0);
    else
        Search();
    SetState();
}

void SetState() {
    if (!Serial3.available()) return;

    const char cmd = Serial3.peek();
    if (cmd == 'G' || cmd == 'g' || cmd == 'A' || cmd == 'a' || cmd == 'T' || cmd == 't') {
        Serial3.read();
        state = true;
    } else if (cmd == 'H' || cmd == 'h' || cmd == 'P' || cmd == 'p' || cmd == 'X' || cmd == 'x') {
        Serial3.read();
        state = false;
    }
}

void Search() {
    static char pending_cmd = 0;

    l2 = analogRead(IRpin_LL) > IR_critical_value;
    l1 = analogRead(IRpin_L) > IR_critical_value;
    m0 = analogRead(IRpin_M) > IR_critical_value;
    r1 = analogRead(IRpin_R) > IR_critical_value;
    r2 = analogRead(IRpin_RR) > IR_critical_value;

    if (Serial3.available()) {
        pending_cmd = Serial3.read();
#ifdef DEBUG
        Serial.print("Search cmd: ");
        Serial.println(pending_cmd);
#endif
    }

    const bool at_node = l1 && m0 && r1;
    if (!at_node) {
        tracking(l2, l1, m0, r1, r2);
        return;
    }

    node_approach();

    if (!pending_cmd) {
        node_stop();
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
/*===========================define function===========================*/
