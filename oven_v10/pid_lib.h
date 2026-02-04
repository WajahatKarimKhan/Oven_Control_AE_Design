#ifndef PID_LIB_H
#define PID_LIB_H

#include <Arduino.h>

class QuickPID {
  public:
    QuickPID(double* input, double* output, double* setpoint, double kp, double ki, double kd, int controllerDirection);
    
    // Configuration
    void SetMode(int mode); // AUTOMATIC = 1, MANUAL = 0
    void SetOutputLimits(double min, double max);
    void SetTunings(double kp, double ki, double kd);
    void SetPOn(int pOn);
    
    // Calculation (call this frequently)
    bool Compute();

    // Constants
    static const int AUTOMATIC = 1;
    static const int MANUAL    = 0;
    static const int DIRECT    = 0;
    static const int REVERSE   = 1;

    static const int P_ON_E    = 1; // Proportional on Error (Default)
    static const int P_ON_M    = 0; // Proportional on Measurement

  private:
    double dispKp, dispKi, dispKd;
    double kp, ki, kd;
    int controllerDirection;
    
    double *myInput;
    double *myOutput;
    double *mySetpoint;
    
    double iTerm, lastInput;
    
    unsigned long lastTime;
    double outMin, outMax;
    bool inAuto;
    // Tracker for the mode
    bool pOnE;
};

#endif