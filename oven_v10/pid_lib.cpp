#include "pid_lib.h"
#include "config.h"
//Library reference for study: http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/
QuickPID::QuickPID(double* input, double* output, double* setpoint, double kp, double ki, double kd, int controllerDirection) {
  myOutput = output;
  myInput = input;
  mySetpoint = setpoint;
  inAuto = false;
  
  // Default to Standard PID
  pOnE = true;

  QuickPID::SetOutputLimits(0, 255); // Default PWM limits
  controllerDirection = controllerDirection;
  QuickPID::SetTunings(kp, ki, kd);
  
  lastTime = millis() - PID_COMPUTE_FREQ;
}

bool QuickPID::Compute() {
  if (!inAuto) return false;
  
  unsigned long now = millis();
  unsigned long timeChange = (now - lastTime);
  
  if (timeChange >= PID_COMPUTE_FREQ) { // Compute every x ms
    // Inputs
    double input = *myInput;
    double error = *mySetpoint - input;
    // Derivative DonM
    double dInput = (input - lastInput);
    // --- INTEGRAL & PROPORTIONAL CALCULATION ---
    if (pOnE) {
        // Standard PID: iTerm only holds Integral
        iTerm += (ki * error);
    } else {
        // PonM: iTerm holds Integral MINUS Proportional change
        // This effectively moves the P term into the storage
        iTerm += (ki * error - kp * dInput);
    }
    if (iTerm > outMax) iTerm = outMax;
    else if (iTerm < outMin) iTerm = outMin;
    
    // --- FINAL OUTPUT CALCULATION ---
    double output;
    if (pOnE) {
        // Standard: P + I - D
        output = kp * error + iTerm - kd * dInput;
    } else {
        // PonM: (I - P_accumulated) - D
        // Since P is already inside iTerm, we just subtract D
        output = iTerm - kd * dInput;
    }
    
    // Clamp Output
    if (output > outMax) output = outMax;
    else if (output < outMin) output = outMin;
    
    *myOutput = output;
    
    // Save history
    lastInput = input;
    lastTime = now;
    return true;
  }
  return false;
}

void QuickPID::SetTunings(double Kp, double Ki, double Kd) {
  if (Kp < 0 || Ki < 0 || Kd < 0) return;
  
  dispKp = Kp; dispKi = Ki; dispKd = Kd;
  
  double SampleTimeInSec = (double)PID_COMPUTE_FREQ / 1000.0;
  kp = Kp;
  ki = Ki * SampleTimeInSec;
  kd = Kd / SampleTimeInSec;
  
  if (controllerDirection == REVERSE) {
    kp = (0 - kp);
    ki = (0 - ki);
    kd = (0 - kd);
  }
}
// output clamping
void QuickPID::SetOutputLimits(double Min, double Max) {
  if (Min >= Max) return;
  outMin = Min;
  outMax = Max;
  
  if (inAuto) {
    if (*myOutput > outMax) *myOutput = outMax;
    else if (*myOutput < outMin) *myOutput = outMin;
    
    if (iTerm > outMax) iTerm = outMax;
    else if (iTerm < outMin) iTerm = outMin;
  }
}

void QuickPID::SetPOn(int pOn) {
   pOnE = (pOn == P_ON_E);
}

void QuickPID::SetMode(int Mode) {
  bool newAuto = (Mode == AUTOMATIC);
  if (newAuto && !inAuto) {
    // Initialize
    iTerm = *myOutput;
    lastInput = *myInput;
    if (iTerm > outMax) iTerm = outMax;
    else if (iTerm < outMin) iTerm = outMin;
  }
  inAuto = newAuto;
}