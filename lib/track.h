/***************************************************************************/
#pragma once

/***************************************************************************/
// File			  [track.h]
// Author		  [Max Lin]
// Synopsis		[Code for tracking]
// Functions  [tracking]
// Modify		  [2026/04/04 Max Lin]
/***************************************************************************/

#include "hardware.h"

/*===========================import variable===========================*/
extern int _Tp;

inline bool& RecoveryTrackingModeRef() {
    static bool enabled = false;
    return enabled;
}

inline double& RecoveryLastErrorRef() {
    static double last_error = 0;
    return last_error;
}

inline double& RecoveryIntegralRef() {
    static double integral = 0;
    return integral;
}

inline int& RecoveryLastSideRef() {
    // 1: right side, -1: left side, 0: center/unknown.
    static int last_side = 0;
    return last_side;
}

inline bool& IsRecoveringRef() {
    static bool is_recovering = false;
    return is_recovering;
}

inline void ResetRecoveryTrackingState() {
    RecoveryLastErrorRef() = 0;
    RecoveryIntegralRef() = 0;
    RecoveryLastSideRef() = 0;
    IsRecoveringRef() = false;
}

inline void SetRecoveryTrackingMode(bool enabled) {
    RecoveryTrackingModeRef() = enabled;
    ResetRecoveryTrackingState();
}

inline bool IsRecoveryTrackingMode() {
    return RecoveryTrackingModeRef();
}

// Write the voltage to motor.
inline void MotorWriting(double vL, double vR) {
    if (vL >= 0) {
        digitalWrite(MotorL_I3, LOW);
        digitalWrite(MotorL_I4, HIGH);
    } else {
        digitalWrite(MotorL_I3, HIGH);
        digitalWrite(MotorL_I4, LOW);
        vL = -vL;
    }

    if (vR >= 0) {
        digitalWrite(MotorR_I1, HIGH);
        digitalWrite(MotorR_I2, LOW);
    } else {
        digitalWrite(MotorR_I1, LOW);
        digitalWrite(MotorR_I2, HIGH);
        vR = -vR;
    }

    analogWrite(MotorL_PWML, constrain(vL, 0, 255));
    analogWrite(MotorR_PWMR, constrain(vR, 0, 255));
}  // MotorWriting

// P/PID control Tracking
inline void tracking(int l2, int l1, int m0, int r1, int r2) {
    const double _w1 = 5.0;
    const double _w2 = 25.0;
    const double _Kp = 1.0;  // p term parameter
    double error = (l2 * (-_w2) + l1 * (-_w1) + r1 * _w1 + r2 * _w2) / (l2 + l1 + m0 + _w1 + _w2);
    double vR, vL;  // PID control output, between -255 to 255.
    double adj_R = 1, adj_L = 1;  // Motor speed correction factors.

    const double powerCorrection = _Kp * error;
    vR = constrain(_Tp - powerCorrection, -255, 255);
    vL = constrain(_Tp - 50 + powerCorrection, -255, 255);
    MotorWriting(adj_L * vL, adj_R * vR);
}

// Optional tracking mode with PID and automatic backward recovery.
// Original tracking() is kept unchanged; this function is selected only
// when SetRecoveryTrackingMode(true) has been called.
inline void tracking_with_recovery(int l2, int l1, int m0, int r1, int r2) {
    const double Kp = 1.8;
    const double Ki = 0.001;
    const double Kd = 2.5;
    const int threshold = 400;

    double& last_error = RecoveryLastErrorRef();
    double& integral = RecoveryIntegralRef();
    int& last_side = RecoveryLastSideRef();
    bool& is_recovering = IsRecoveringRef();

    const bool on_line = (
        l2 > threshold ||
        l1 > threshold ||
        m0 > threshold ||
        r1 > threshold ||
        r2 > threshold
    );

    if (!on_line) {
        is_recovering = true;

        if (last_side == -1) {
            MotorWriting(-160, -60);
        } else if (last_side == 1) {
            MotorWriting(-60, -160);
        } else {
            MotorWriting(-100, -100);
        }

        integral = 0;
        return;
    }

    if (is_recovering) {
        is_recovering = false;
        last_error = 0;
    }

    if (l2 > threshold) {
        last_side = -1;
    } else if (r2 > threshold) {
        last_side = 1;
    } else if (m0 > threshold && l1 < threshold && r1 < threshold) {
        last_side = 0;
    }

    double sum = l2 + l1 + m0 + r1 + r2;
    if (sum == 0) {
        sum = 1;
    }

    const double error = (
        l2 * -35.0 +
        l1 * -10.0 +
        r1 * 10.0 +
        r2 * 35.0
    ) / sum;

    integral = constrain(integral + error, -80, 80);
    const double derivative = error - last_error;
    const double correction = (Kp * error) + (Ki * integral) + (Kd * derivative);
    last_error = error;

    MotorWriting(_Tp + correction, _Tp - correction);
}
