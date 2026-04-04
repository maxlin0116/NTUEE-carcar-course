/***************************************************************************/
#pragma once

/***************************************************************************/
// File			  [node.h]
// Author		  [Max Lin]
// Synopsis		[Code for managing node behavior]
// Functions  [node_center_offline, node_stop, node_is_active, node_forward, node_left_turn, node_right_turn, node_u_turn]
// Modify		  [2026/04/04 Max Lin]
/***************************************************************************/

#include "hardware.h"

/*===========================import variable===========================*/
extern int _Tp;

void MotorWriting(double vL, double vR);

constexpr unsigned long NODE_TURN_DELAY = 500;
constexpr unsigned long NODE_TURN_ADJUST_DELAY = 200;
constexpr unsigned long NODE_FORWARD_DELAY = 120;
constexpr int NODE_IR_CRITICAL_VALUE = 150;

inline bool node_center_offline() {
    return analogRead(IRpin_L) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) < NODE_IR_CRITICAL_VALUE;
}

inline void node_stop() {
    MotorWriting(0, 0);
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
