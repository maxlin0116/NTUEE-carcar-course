/***************************************************************************/
#pragma once

/***************************************************************************/
// File			  [node.h]
// Author		  [Erik Kuo, Joshua Lin]
// Synopsis		[Code for managing car movement when encounter a node]
// Functions  [/* add on your own! */]
// Modify		  [2020/03/027 Erik Kuo]
/***************************************************************************/

/*===========================import variable===========================*/
extern int _Tp;
/*===========================import variable===========================*/

void MotorWriting(double vL, double vR);

constexpr unsigned long NODE_ADVANCE_DELAY = 80;
constexpr unsigned long NODE_TURN_DELAY = 500;
constexpr unsigned long NODE_U_TURN_DELAY = 1000;
constexpr unsigned long NODE_TURN_ADJUST_DELAY = 200;
constexpr unsigned long NODE_FORWARD_DELAY = 120;
constexpr int NODE_IR_CRITICAL_VALUE = 150;

inline bool node_center_on_line() {
    return analogRead(IRpin_L) > NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) > NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) > NODE_IR_CRITICAL_VALUE;
}

inline bool node_center_offline() {
    return analogRead(IRpin_L) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) < NODE_IR_CRITICAL_VALUE;
}

inline void node_stop() {
    MotorWriting(0, 0);
}

inline void node_approach() {
    MotorWriting(_Tp, _Tp);
    delay(NODE_ADVANCE_DELAY);
}

inline bool node_is_active() {
    return analogRead(IRpin_L) > NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) > NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) > NODE_IR_CRITICAL_VALUE;
}

inline void node_forward(const unsigned long duration = NODE_FORWARD_DELAY) {
    MotorWriting(_Tp, _Tp);
    delay(duration);
}

inline void node_left_turn(const unsigned long duration = NODE_TURN_DELAY) {
    MotorWriting(0, _Tp);
    delay(duration);

    if (node_center_offline()) {
        MotorWriting(0, _Tp);
        delay(NODE_TURN_ADJUST_DELAY);
    }
}

inline void node_right_turn(const unsigned long duration = NODE_TURN_DELAY) {
    MotorWriting(_Tp, 0);
    delay(duration);

    if (node_center_offline()) {
        MotorWriting(_Tp, 0);
        delay(NODE_TURN_ADJUST_DELAY);
    }
}

inline void node_u_turn(const unsigned long duration = NODE_TURN_DELAY) {
    MotorWriting(_Tp, -_Tp);
    delay(duration);

    if (node_center_offline()) {
        MotorWriting(_Tp, -_Tp);
        delay(NODE_TURN_ADJUST_DELAY);
    }
}
