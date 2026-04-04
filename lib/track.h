/***************************************************************************/
#pragma once

/***************************************************************************/
// File			  [track.h]
// Author		  [Erik Kuo]
// Synopsis		[Code used for tracking]
// Functions  [MotorWriting, MotorInverter, tracking]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 1 & 6*/
/*feel free to add your own function for convenience*/

/*===========================import variable===========================*/
extern int _Tp;
/*===========================import variable===========================*/

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
    vL = constrain(_Tp + 5 * powerCorrection, -255, 255);
    MotorWriting(adj_L * vL, adj_R * vR);
}  // tracking
