//defining pin for IR sensor
#define L3 A7
#define L2 A6
#define M A5
#define R2 A4
#define R3 A3

#include <SPI.h>
#include <MFRC522.h>
// 引入 SPI 程式庫 與 MFRC522 程式庫
#define RST_PIN 3
#define SS_PIN 2
// 設定重設腳位 與 SPI 介面裝置選擇腳位
MFRC522 *mfrc522;
// 宣告MFRC522指標
#define CUSTOM_NAME "HM10_G6" // Max length is 12 characters [1]
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

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
int Tp = 200;                               // the base power given to the left and right motor
int operation[10] = {1,3,4,3,1,3,1,3,1,3};  // the list to store the move of the operation
//int operation[6] = {3,3,3,3,3,3};
int current_order = 0;                      // the current operating operation index
int turn_time = 500;                        // the main turn-time
int turn_modify_time = 200;                 // the addition time to turn if the main turn is not enough 
int IR_critical_value = 150;                // the IR sensor critical value


//set up the pin mode
void setup() {
  Serial3.begin(9600);
  Serial.begin(115200); // Debug Monitor (USB)
  while (!Serial);
  Serial.println("Initializing HM-10...");

  // 1. Automatic Baud Rate Detection
  for (int i = 0; i < 9; i++) {
    Serial.print("Testing baud rate: ");
    Serial.println(baudRates[i]);
    
    Serial3.begin(baudRates[i]);
    Serial3.setTimeout(100);
    delay(100);

    // 2. Force Disconnection
    // Sending "AT" while connected forces the module to disconnect [2].
    Serial3.print("AT"); 
    
    if (waitForResponse("OK", 800)) {
      Serial.println("HM-10 detected and ready.");
      moduleReady = true;
      break; 
    } else {
      Serial3.end();
      delay(100);
    }
  }

  if (!moduleReady) {
    Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
    return;
  }

  // 3. Restore Factory Defaults
  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW"); // Restores all setup values
  delay(500);

  // 4. Set Custom Name via Macro
  Serial.print("Setting name to: ");
  Serial.println(CUSTOM_NAME);
  String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
  sendATCommand(nameCmd.c_str()); // Max length is 12
  
  // 5. Enable Connection Notifications
  Serial.println("Enabling notifications...");
  sendATCommand("AT+NOTI1"); // Notify when link is established/lost

  // 6. Get the Bluetooth MAC Address
  Serial.println("Querying Bluetooth Address");
  sendATCommand("AT+ADDR?");

  // 7. Restart the module to apply changes
  Serial.println("Restarting module...");
  sendATCommand("AT+RESET"); // Restart the module
  delay(1000);
  Serial3.begin(9600); // Now the module would use baudrate 9600
  
  Serial.println("Initialization Complete.");

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

  SPI.begin();
  mfrc522 = new MFRC522(SS_PIN, RST_PIN);
  mfrc522->PCD_Init();
  Serial.println(F("Read UID on a MIFARE PICC:")); 
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
  UIDRead();
  MotorWriting(Tp+30, -Tp-30);
  delay(turn_time);
  sensor();

  // If turn not enough
  if(l2 < IR_critical_value && m < IR_critical_value && r2 < IR_critical_value)
  {
    MotorWriting(Tp, -Tp);
    delay(turn_modify_time);
  }
}

void no_turn()
{
  MotorWriting(Tp, Tp);
  delay(200);
  sensor();
  if (l2 > IR_critical_value && m > IR_critical_value && r2 > IR_critical_value)
  {
    MotorWriting(Tp, Tp);
    delay(100);
  }
}

void action(int order)
{
  if(operation[order] == 1) turn_right();
  if(operation[order] == 2) turn_left();
  if(operation[order] == 3) U_turn();
  if(operation[order] == 4) no_turn();
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
    vL = Tp + 7*powerCorrection;
    if(vR>255) vR = 255;
    if(vL>255) vL = 255;
    if(vR<-255) vR = -255;
    if(vL<-255) vL = -255;
    MotorWriting(vL, vR);
  }
}

void UIDRead() {
  if (!mfrc522->PICC_IsNewCardPresent()) return;
  if (!mfrc522->PICC_ReadCardSerial()) return;

  String uid = "";

  for (byte i = 0; i < mfrc522->uid.size; i++) {
    if (mfrc522->uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522->uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  // 傳到 USB 連線的電腦
  Serial.print("UID:");
  Serial.println(uid);

  // 傳到藍牙(HM-10)連到電腦 / Python
  Serial3.print("UID:");
  Serial3.println(uid);

  mfrc522->PICC_HaltA();
  mfrc522->PCD_StopCrypto1();
}

void looping() {
  if (Serial3.available()) {
    char c = Serial3.read();
    Serial.print("BT received: ");
    Serial.println(c);

    if (c == '1') turn_right();
    else if (c == '2') turn_left();
    else if (c == '3') U_turn();
    else if (c == '0') MotorWriting(0, 0);
  }

  if (Serial.available()) {
    char c = Serial.read();
    Serial.print("USB received: ");
    Serial.println(c);

    if (c == '1') turn_right();
    else if (c == '2') turn_left();
    else if (c == '3') U_turn();
    else if (c == '0') MotorWriting(0, 0);
  }
}

void loop(){
  //Tracking();
  UIDRead();
  looping();
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

/**
 * Helper to send AT commands (Uppercase, no \r or \n) [6]
 */
void sendATCommand(const char* command) {
  Serial3.print(command);
  waitForResponse("", 1000); 
}

/**
 * Helper to check response for specific substrings
 */
bool waitForResponse(const char* expected, unsigned long timeout) {
  unsigned long start = millis();
  Serial3.setTimeout(timeout);
  String response = Serial3.readString();
  if (response.length() > 0) {
    Serial.print("HM10 Response: ");
    Serial.println(response);
  }
  return (response.indexOf(expected) != -1);
}