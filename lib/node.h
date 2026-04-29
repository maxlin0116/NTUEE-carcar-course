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
inline bool IsSlowTrackingMode();

constexpr unsigned long NODE_TURN_DELAY = 210;
constexpr unsigned long NODE_TURN_ADJUST_DELAY = 200;
constexpr unsigned long NODE_FORWARD_DELAY = 25;
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
    return 	analogRead(IRpin_R) > NODE_IR_CRITICAL_VALUE ||
			analogRead(IRpin_M) > NODE_IR_CRITICAL_VALUE ||
			analogRead(IRpin_L) > NODE_IR_CRITICAL_VALUE;
}

inline bool node_line_seen_u() {
    return analogRead(IRpin_R) > NODE_IR_CRITICAL_VALUE;
	
}

inline bool node_line_seen_uc() {
    return analogRead(IRpin_L) > NODE_IR_CRITICAL_VALUE;
	
}

inline void node_stop() {
    MotorWriting(0, 0);
}

inline bool node_is_active() {
    return analogRead(IRpin_L) > NODE_STOP_IR_CRITICAL_VALUE &&
           analogRead(IRpin_M) > NODE_STOP_IR_CRITICAL_VALUE &&
           analogRead(IRpin_R) > NODE_STOP_IR_CRITICAL_VALUE;
}

inline void node_forward(const unsigned long duration = NODE_FORWARD_DELAY - 20) {
    MotorWriting(NODE_TURN_SPEED, NODE_TURN_SPEED);
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
    }
}

inline void DelayUntilLineOrTimeout_u(const unsigned long duration) {
    const unsigned long start_ms = millis();
    while (millis() - start_ms < duration) {
        UIDRead();
        if (node_line_seen_u()) {
            break;
        }
    }
}

inline void DelayUntilLineOrTimeout_uc(const unsigned long duration) {
    const unsigned long start_ms = millis();
    while (millis() - start_ms < duration) {
        UIDRead();
        if (node_line_seen_uc()) {
            break;
        }
    }
}

inline void node_left_turn(const unsigned long duration = NODE_TURN_DELAY + (IsSlowTrackingMode() ? 65 : 80)) { // 300
    if (IsSlowTrackingMode()) {
        node_forward(40);
    }
    MotorWriting(0, NODE_TURN_SPEED * 0.8);
    DelayWithUIDPolling(duration);

    if (analogRead(IRpin_M) <= NODE_IR_CRITICAL_VALUE) {
        MotorWriting(0, NODE_TURN_SPEED * (IsSlowTrackingMode() ? 0.6 : 0.4));
        DelayUntilLineOrTimeout(NODE_TURN_ADJUST_DELAY + 2000);
    }
    ResetTrackingState();
}

inline void node_right_turn(const unsigned long duration = NODE_TURN_DELAY + (IsSlowTrackingMode() ? 65 : 80)) { // 300 0.6
    if (IsSlowTrackingMode()) {
        node_forward(40);
    }
    MotorWriting(NODE_TURN_SPEED * (IsSlowTrackingMode() ? 0.9 : 0.8), 0);
    DelayWithUIDPolling(duration);

    if (analogRead(IRpin_M) <= NODE_IR_CRITICAL_VALUE) {
		MotorWriting(NODE_TURN_SPEED * (IsSlowTrackingMode() ? 0.6 : 0.4), 0);
        DelayUntilLineOrTimeout(NODE_TURN_ADJUST_DELAY + 2000);
    }
    ResetTrackingState();
}

inline void node_u_turn_ccw(const unsigned long duration = NODE_TURN_DELAY + (IsSlowTrackingMode() ? 110 : 65)) {
	//node_pre_turn_forward();
    MotorWriting(NODE_TURN_SPEED + (IsSlowTrackingMode() ? 80 : 100), -NODE_TURN_SPEED - (IsSlowTrackingMode() ? 0 : 50)); //0.6 + 20, 0.6
    DelayWithUIDPolling(duration); // 250

    if (analogRead(IRpin_M) <= NODE_IR_CRITICAL_VALUE) {
        MotorWriting(NODE_TURN_SPEED * 0.3 + 5, -NODE_TURN_SPEED * 0.3);
        DelayUntilLineOrTimeout_uc(NODE_TURN_ADJUST_DELAY + 2000);
    }
    ResetTrackingState();
}

inline void node_u_turn(const unsigned long duration = NODE_TURN_DELAY + (IsSlowTrackingMode() ? 110 : 80)) {
	//node_pre_turn_forward();
    MotorWriting(-NODE_TURN_SPEED, NODE_TURN_SPEED + (IsSlowTrackingMode() ? 60 : 60)); // 0.6, 0.6 + 20
    DelayWithUIDPolling(duration); // 250

    if (analogRead(IRpin_M) <= NODE_IR_CRITICAL_VALUE) {
        MotorWriting(-NODE_TURN_SPEED * 0.3, NODE_TURN_SPEED * 0.3 + 5);
        DelayUntilLineOrTimeout_u(NODE_TURN_ADJUST_DELAY + 2000);
    }
    ResetTrackingState();
}
