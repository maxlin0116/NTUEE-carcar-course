/***************************************************************************/
#pragma once

/***************************************************************************/
// File       [control.h]
// Author     [Max Lin]
// Synopsis   [Code for managing main process]
// Functions  [SetState, Search]
// Modify     [2026/04/04 Max Lin]
/***************************************************************************/

#include "hardware.h"
#include "RFID.h"
#include "node.h"
#include "track.h"
#include <ctype.h>


// shared variables
extern int l2, l1, m0, r1, r2;
extern int _Tp;
extern bool state;
extern char queued_node_cmd;

constexpr unsigned long NODE_SETTLE_DELAY = 0;
constexpr unsigned long NODE_FORWARD_SETTLE_DELAY = 0;

inline bool HandleMotorTestCommand(char cmd);

inline bool HandleTrackingModeCommand(char cmd);

inline char& PendingNodeCommandRef() {
	static char pending_cmd = 0;
	return pending_cmd;
}

inline unsigned long& LastNodeEventMsRef() {
	static unsigned long last_node_event_ms = 0;
	return last_node_event_ms;
}

inline bool& WaitingAtNodeRef() {
	static bool waiting_at_node = false;
	return waiting_at_node;
}

inline bool& NodeEventReportedRef() {
	static bool node_event_reported = false;
	return node_event_reported;
}

inline bool ShouldPrintTrackingDebug() {
	static unsigned long last_debug_ms = 0;
	const unsigned long now = millis();
	if (now - last_debug_ms < 250) {
		return false;
	}
	last_debug_ms = now;
	return true;
}

inline void ResetSearchState() {
	PendingNodeCommandRef() = 0;
	LastNodeEventMsRef() = 0;
	WaitingAtNodeRef() = false;
	NodeEventReportedRef() = false;
	ResetRecoveryTrackingState();
	queued_node_cmd = 0;
}

// check if the command is a node command, if so, execute it and return true. Otherwise return false.
inline bool IsNodeCommand(char cmd) {
	return cmd == 'L' || cmd == 'R' || cmd == 'B' || cmd == 'C' || cmd == 'S' || cmd == 'F';
}

inline void ReportNodeEvent() {
	Serial3.println("EVENT:NODE");
#ifdef DEBUG
	Serial.println("BT event: EVENT:NODE");
#endif
}

inline void MaybeReportNodeEvent() {
	const unsigned long now = millis();
	unsigned long& last_node_event_ms = LastNodeEventMsRef();
	bool& node_event_reported = NodeEventReportedRef();

	if (!node_event_reported || now - last_node_event_ms >= 250) {
		ReportNodeEvent();
		node_event_reported = true;
		last_node_event_ms = now;
	}
}

// execute the node command, return true if the command is valid, false otherwise.
inline bool ExecuteNodeCommand(char cmd) {
	switch (cmd) {
		case 'L':
			node_left_turn();
			return true;
		case 'R':
			node_right_turn();
			return true;
		case 'B':
			node_u_turn();
			return true;
		case 'C':
			node_u_turn_ccw();
			return true;
		case 'S':
			node_stop();
			return true;
		case 'F':
			node_forward();
			return true;
		default:
			return false;
	}
}

// check if the command is a run command, if so, set state to true and return true. Otherwise return false.
inline bool IsRunCommand(char cmd) {
	return cmd == 'G';
}

// check if the command is a halt command, if so, set state to false and return true. Otherwise return false.
inline bool IsHaltCommand(char cmd) {
	return cmd == 'H';
}

// read commands from Serial3 and set state accordingly. Also handle node command queuing and motor test commands.
inline void SetState() {
	while (Serial3.available()) {
		const char cmd = toupper(Serial3.read());
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
			if (!state) {
				ResetSearchState();
			}
			state = true;
	#ifdef DEBUG
				Serial.print("State -> RUN via ");
				Serial.println(cmd);
	#endif
			} else if (IsHaltCommand(cmd)) {
				ResetSearchState();
				state = false;
				MotorWriting(0, 0);
	#ifdef DEBUG
				Serial.print("State -> HALT via ");
				Serial.println(cmd);
	#endif
			} else if (HandleTrackingModeCommand(cmd)) {
				// Mode command handled.
			} else if (IsNodeCommand(cmd)) {
				queued_node_cmd = cmd;
	#ifdef DEBUG
				Serial.print("Queued node cmd: ");
				Serial.println(cmd);
	#endif
			} else if (!HandleMotorTestCommand(cmd)) {
	#ifdef DEBUG
				Serial.print("Ignored cmd: ");
				Serial.println(cmd);
	#endif
		}
	}
}

// main search function, called when state is active. Handle both tracking and node process.
inline void Search() {
	char& pending_cmd = PendingNodeCommandRef();				// pending node command to execute when reaching the node, 0 if no pending command
	bool& waiting_at_node = WaitingAtNodeRef();
	bool& node_event_reported = NodeEventReportedRef();
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

	// if there's a queued node command, override the pending_cmd with it. This ensures that the car will execute the most recent node command when it reaches the node.
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
		if (ShouldPrintTrackingDebug()) {
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
		}
	#endif

	// if we're waiting at the node, we will only execute the pending node command when the car is still at the node. If the car has left the node, we will clear the pending node command and stop waiting.
	if (waiting_at_node) {
		MotorWriting(0, 0);
		// if the car has left the node, stop waiting and clear pending command. Otherwise, keep waiting and do nothing (i.e. keep the car stopped at the node).
		if (!pending_cmd) {
			MaybeReportNodeEvent();
			#ifdef DEBUG
						Serial.println("Waiting at node");
			#endif
			return;
		}

		if (pending_cmd == 'L' || pending_cmd == 'R' || pending_cmd == 'B' || pending_cmd == 'C') {
			if (!ExecuteNodeCommand(pending_cmd)) {
				node_stop();
			}

			pending_cmd = 0;
			waiting_at_node = false;
			node_event_reported = false;
			return;
		}

		while (node_is_active()) {
			MotorWriting(_Tp, _Tp);
			UIDRead();
		}

		MotorWriting(0, 0);
		DelayWithUIDPolling(pending_cmd == 'F' ? NODE_FORWARD_SETTLE_DELAY : NODE_SETTLE_DELAY);
		waiting_at_node = false;
		node_event_reported = false;

		// if there's a pending node command, execute it. Otherwise, just stop at the node.
		if (!ExecuteNodeCommand(pending_cmd)) {
			node_stop();
		}

		pending_cmd = 0;
		return;
	}

	// if we're not at the node, do tracking. If we're at the node, execute the pending node command if there's any, otherwise just stop at the node.
	if (!at_node) {
		node_event_reported = false;
		if (IsRecoveryTrackingMode()) {
			tracking_with_recovery(l2, l1, m0, r1, r2);
		} else {
			tracking(l2, l1, m0, r1, r2);
		}
		return;
	}

	MotorWriting(0, 0);	// stop at the node first
	DelayWithUIDPolling(NODE_SETTLE_DELAY);

	if (!pending_cmd) {
		waiting_at_node = true;
		MaybeReportNodeEvent();
		MotorWriting(0, 0);
		return;
	}

	// Keep auto-drive aligned with manual remote control:
	// directional node commands should always execute from the
	// "stopped on the node, then leave-and-turn" path.
	if (pending_cmd == 'L' || pending_cmd == 'R' || pending_cmd == 'B' || pending_cmd == 'C' || pending_cmd == 'F') {
		waiting_at_node = true;
		return;
	}
	if (!ExecuteNodeCommand(pending_cmd)) {
		node_stop();
	}

	// after executing the node command, clear the pending command and set waiting_at_node to false to prepare for the next node.
	pending_cmd = 0;
	waiting_at_node = false;
	node_event_reported = false;
}


/*=============below are the test code=============*/
inline bool HandleTrackingModeCommand(char cmd) {
	switch (cmd) {
		case 'P':
			SetRecoveryTrackingMode(true);
			Serial.println("Tracking mode: recovery PID");
			Serial3.println("EVENT:TRACKING:RECOVERY");
			return true;
		case 'O':
			SetFastTrackingMode();
			Serial.println("Tracking mode: fast");
			Serial3.println("EVENT:TRACKING:FAST");
			return true;
		case 'M':
			SetSlowTrackingMode();
			Serial.println("Tracking mode: slow");
			Serial3.println("EVENT:TRACKING:SLOW");
			return true;
		default:
			return false;
	}
}

inline bool HandleMotorTestCommand(char cmd) {
	switch (cmd) {
		case '5':
			Serial.println("TEST: MotorWriting(_Tp, _Tp)");
			MotorWriting(_Tp, _Tp);
			DelayWithUIDPolling(500);
			MotorWriting(0, 0);
			return true;
		case '6':
			Serial.println("TEST: MotorWriting(_Tp, 0)");
			MotorWriting(_Tp, 0);
			DelayWithUIDPolling(500);
			MotorWriting(0, 0);
			return true;
		case '7':
			Serial.println("TEST: MotorWriting(0, _Tp)");
			MotorWriting(0, _Tp);
			DelayWithUIDPolling(500);
			MotorWriting(0, 0);
			return true;
		case '8':
			Serial.println("TEST: MotorWriting(_Tp, -_Tp)");
			MotorWriting(_Tp, -_Tp);
			DelayWithUIDPolling(500);
			MotorWriting(0, 0);
			return true;
		case '9':
			Serial.println("TEST: MotorWriting(-_Tp, _Tp)");
			MotorWriting(-_Tp, _Tp);
			DelayWithUIDPolling(500);
			MotorWriting(0, 0);
			return true;
		case '4':
			Serial.println("TEST: node_u_turn_ccw()");
			node_u_turn_ccw();
			return true;
		default:
			return false;
	}
}
