/***************************************************************************/
// File       [control.h]
// Synopsis   [State handling and node-search control logic]
/***************************************************************************/

extern int l2, l1, m0, r1, r2;
extern int _Tp;
extern bool state;
extern char queued_node_cmd;

inline bool IsRunCommand(char cmd) {
	return cmd == 'G' || cmd == 'g' || cmd == 'A' || cmd == 'a' || cmd == 'T' || cmd == 't';
}

inline bool IsHaltCommand(char cmd) {
	return cmd == 'H' || cmd == 'h' || cmd == 'P' || cmd == 'p' || cmd == 'X' || cmd == 'x';
}

inline bool HandleMotorTestCommand(char cmd) {
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

inline void SetState() {
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
		} else if (!HandleMotorTestCommand(cmd)) {
			queued_node_cmd = cmd;
#ifdef DEBUG
			Serial.print("Queued node cmd: ");
			Serial.println(cmd);
#endif
		}
	}
}

inline void Search() {
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
