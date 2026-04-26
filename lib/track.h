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

inline double& TrackingLastErrorRef() {
    static double last_error = 0;
    return last_error;
}

inline double& TrackingIntegralRef() {
    static double integral = 0;
    return integral;
}

inline double& TrackingLastDerivativeRef() {
    static double last_derivative = 0;
    return last_derivative;
}

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

inline void ResetTrackingState() {
    TrackingLastErrorRef() = 0;
    TrackingIntegralRef() = 0;
    TrackingLastDerivativeRef() = 0;
    ResetRecoveryTrackingState();
}

inline void SetRecoveryTrackingMode(bool enabled) {
    RecoveryTrackingModeRef() = enabled;
    ResetTrackingState();
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

// PID control Tracking
inline void tracking(int l2, int l1, int m0, int r1, int r2) {
    const double Kp = 0.6;
    const double Ki = 0.0;
    const double Kd = 0.5;
    const int threshold = 150;
    const int left_motor_offset = 6.0;
    const double error_deadband = 2.0;
    const double max_correction = 35.0;

    double& last_error = TrackingLastErrorRef();
    double& integral = TrackingIntegralRef();
    double& last_derivative = TrackingLastDerivativeRef();

    const bool on_line = (
        l2 > threshold ||
        l1 > threshold ||
        m0 > threshold ||
        r1 > threshold ||
        r2 > threshold
    );

    if (!on_line) {
        integral = 0;
        last_derivative = 0;
        const double search_power = 40;
        if (last_error > 0) {
            MotorWriting(_Tp + left_motor_offset + search_power, _Tp - search_power);
        } else if (last_error < 0) {
            MotorWriting(_Tp + left_motor_offset - search_power, _Tp + search_power);
        } else {
            MotorWriting(_Tp + left_motor_offset, _Tp);
        }
        return;
    }

    double sum = l2 + l1 + m0 + r1 + r2;
    if (sum == 0) {
        sum = 1;
    }

    double error = (
        l2 * -25.0 +
        l1 * -8.0 +
        r1 * 8.0 +
        r2 * 25.0
    ) / sum;

    if (abs(error) < error_deadband) {
        error = 0;
    }

    integral = constrain(integral + error, -80, 80);
    const double raw_derivative = error - last_error;
    const double derivative = (0.7 * last_derivative) + (0.3 * raw_derivative);
    last_derivative = derivative;
    const double correction = constrain(
        (Kp * error) + (Ki * integral) + (Kd * derivative),
        -max_correction,
        max_correction
    );
    last_error = error;

    const double vL = constrain(_Tp + left_motor_offset + 1.5 * correction, -255, 255);
    const double vR = constrain(_Tp - 4 * correction, -255, 255);
    MotorWriting(vL, vR);
}

// Optional tracking mode with PID and automatic backward recovery.
// Original tracking() is kept unchanged; this function is selected only
// when SetRecoveryTrackingMode(true) has been called.
inline void tracking_with_recovery(int l2, int l1, int m0, int r1, int r2) {
    const double Kp = 1.8;
    const double Ki = 0.001;
    const double Kd = 2.5;
    const int threshold = 150;

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
