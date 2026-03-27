//defining pin for IR sensor
#define L3 A7
#define L2 A6
#define M A5
#define R2 A4
#define R3 A3

//define the pin_name for motor
int MotorL_PWML = 10;
int MotorL_I1 = 8;
int MotorL_I2 = 9;
int MotorR_I3 = 6;
int MotorR_I4 = 7;
int MotorR_PWMR = 11;

//define the used variable                       
int l2, l3, m, r2, r3;                      // the sensor value form the IR sensor from the left to the right
double vL, vR;                              // the power given to the left and right motor
int w2 = 5;                                 // the weighted value of L2 and R2 IR sensor
int w3 = 25;                                // the weighted value of L3 and R3 IR sensor 
double error;                               // the value that tells how left or right the car relative to the black line
int powerCorrection;                        // the value to modify the power of the left and right motor (proportional to error)
double Kp = 1;                              // proportion constant between error and powerCorrection
int Tp = 150;                               // the base power given to the left and right motor
int operation[10] = {1,3,1,3,1,3,1,3,1,3};  // the list to store the move of the operation
//int operation[6] = {3,3,3,3,3,3};
int current_order = 0;                      // the current operating operation index
int turn_time = 700;                        // the main turn-time
int turn_modify_time = 200;                 // the addition time to turn if the main turn is not enough 
int IR_critical_value = 150;                // the IR sensor critical value

//set up the pin mode
void setup() {
  Serial.begin(9600);
  pinMode(MotorL_PWML, OUTPUT);
  pinMode(MotorR_PWMR, OUTPUT);
  pinMode(MotorL_I1, OUTPUT);
  pinMode(MotorL_I2, OUTPUT);
  pinMode(MotorR_I3, OUTPUT);
  pinMode(MotorR_I4, OUTPUT);

  pinMode(L3, INPUT);
  pinMode(L2, INPUT);
  pinMode(M, INPUT);
  pinMode(R2, INPUT);
  pinMode(R3, INPUT);
}

//function used to control the left and right motor
void MotorWriting(double vL, double vR) {
  if (vL >= 0) {
    digitalWrite(MotorL_I1, LOW);
    digitalWrite(MotorL_I2, HIGH);
  } 
  else {
    digitalWrite(MotorL_I1, HIGH);
    digitalWrite(MotorL_I2, LOW);
    vL = -vL;
  }
  
  if (vR >= 0) {
    digitalWrite(MotorR_I3, HIGH);
    digitalWrite(MotorR_I4, LOW);
  } 
  else {
    digitalWrite(MotorR_I3, LOW);
    digitalWrite(MotorR_I4, HIGH);
    vR = -vR;
  }

  analogWrite(MotorL_PWML, vL);
  analogWrite(MotorR_PWMR, vR);
}

// return the sensor value of the IR sensor
void sensor() {
  l3 = analogRead(L3);
  l2 = analogRead(L2);
  m = analogRead(M);
  r2 = analogRead(R2);
  r3 = analogRead(R3);
}

void turn_right()
{
  MotorWriting(Tp, 0);
  delay(turn_time);
  sensor();
  
  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(Tp, 0);
    delay(turn_modify_time);
  }
}

void turn_left()
{
  MotorWriting(0, Tp);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(0, Tp);
    delay(turn_modify_time);
  }
}

void U_turn()
{
  MotorWriting(Tp+20, -Tp-20);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(Tp, -Tp);
    delay(turn_modify_time);
  }
}

void action(int order)
{
  if(operation[order] == 1) turn_right();
  if(operation[order] == 2) turn_left();
  if(operation[order] == 3) U_turn();
}

void Tracking() {
  sensor();

  if (l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)  // detect the black block
  {
    while(l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)
    {
      MotorWriting(Tp, Tp);
      sensor();
    }
    action(current_order);
    current_order++;
  }
  else  // not detect the black block
  {
    sensor();
    error = (l3*(-w3) + l2*(-w2) + r2*w2 + r3*w3)/(l3 + l2 + m + w2 + w3);
    powerCorrection = Kp * error;
    vR = Tp - powerCorrection;
    vL = Tp + 5*powerCorrection;
    if(vR>255) vR = 255;
    if(vL>255) vL = 255;
    if(vR<-255) vR = -255;
    if(vL<-255) vL = -255;
    MotorWriting(vL, vR);
  }
}

void loop(){
  Tracking();

  // --------- below are the test code ---------
  // MotorWriting(150, 150);
  // Serial.println("error");
  // Serial.println(powerCorrection);
  // sensor();
  // Serial.println("l3");
  // Serial.println(l3);
  // Serial.println("l2");
  // Serial.println(l2);
  // Serial.println("m");
  // Serial.println(m);
  // Serial.println("r2");
  // Serial.println(r2);
  // Serial.println("r3");
  // Serial.println(r3);
  // delay(300);
}