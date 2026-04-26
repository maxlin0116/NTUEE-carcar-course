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
#include "RFID.h"

/*===========================import variable===========================*/
extern int _Tp;

void MotorWriting(double vL, double vR);
inline void ResetTrackingState();

constexpr unsigned long NODE_TURN_DELAY = 200; // 350 orginall
constexpr unsigned long NODE_TURN_ADJUST_DELAY = 200;
constexpr unsigned long NODE_FORWARD_DELAY = 50; // 200
constexpr unsigned long NODE_PRE_TURN_FORWARD_DELAY = 70;
constexpr int NODE_IR_CRITICAL_VALUE = 150;
constexpr int NODE_STOP_IR_CRITICAL_VALUE = 150;
constexpr int NODE_TURN_SPEED = 140;

inline bool node_center_offline() {
    return analogRead(IRpin_L) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) < NODE_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) < NODE_IR_CRITICAL_VALUE;
}

inline bool node_line_seen() {
    return analogRead(IRpin_LL) > NODE_IR_CRITICAL_VALUE ||
           analogRead(IRpin_M) > NODE_IR_CRITICAL_VALUE ||
           analogRead(IRpin_RR) > NODE_IR_CRITICAL_VALUE;
}

inline void node_stop() {
    MotorWriting(0, 0);
}

inline bool node_is_active() {
    return analogRead(IRpin_L) > NODE_STOP_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) > NODE_STOP_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) > NODE_STOP_IR_CRITICAL_VALUE;
}

inline void node_forward(const unsigned long duration = NODE_FORWARD_DELAY) {
    MotorWriting(NODE_TURN_SPEED - 20, NODE_TURN_SPEED);
    DelayWithUIDPolling(duration);
    ResetTrackingState();
}

inline void node_pre_turn_forward() {
    node_forward(NODE_PRE_TURN_FORWARD_DELAY);
}

inline void DelayUntilLineOrTimeout(const unsigned long duration) {
    const unsigned long start_ms = millis();
    while (millis() - start_ms < duration) {
        UIDRead();
        if (node_line_seen()) {
            break;
        }
        delay(5);
    }
}

inline void node_left_turn(const unsigned long duration = NODE_TURN_DELAY + 100) {
    //node_pre_turn_forward();
    MotorWriting(40, NODE_TURN_SPEED);
    DelayWithUIDPolling(duration);

    if (node_center_offline()) {
        MotorWriting(40, NODE_TURN_SPEED);
        DelayUntilLineOrTimeout(NODE_TURN_ADJUST_DELAY + 150);
    }
    ResetTrackingState();
}

inline void node_right_turn(const unsigned long duration = NODE_TURN_DELAY + 75) {
    //node_pre_turn_forward();
    MotorWriting(NODE_TURN_SPEED + 30, 50);
    DelayWithUIDPolling(duration);

    if (node_center_offline()) {
		MotorWriting(NODE_TURN_SPEED, 30);
        DelayUntilLineOrTimeout(NODE_TURN_ADJUST_DELAY + 150);
    }
    ResetTrackingState();
}

inline void node_u_turn(const unsigned long duration = NODE_TURN_DELAY + 85) {
	node_pre_turn_forward();
    MotorWriting(NODE_TURN_SPEED * 0.7, -NODE_TURN_SPEED * 0.7);
    DelayWithUIDPolling(duration);

    if (node_center_offline()) {
        MotorWriting(NODE_TURN_SPEED * 0.7, -NODE_TURN_SPEED * 0.7);
        DelayUntilLineOrTimeout(NODE_TURN_ADJUST_DELAY + 150);
    }
    ResetTrackingState();
}
