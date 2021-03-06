#include <math.h>
#include <ADXL345.h>  // ADXL345 Accelerometer Library
#include <HMC5883L.h> // HMC5883L Magnetometer Library
#include <ITG3200.h>
#include "I2Cdev.h"
#include <Wire.h>
#include <PiD_v1.h>
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);

ADXL345 acc;
HMC5883L compass;
ITG3200 gyro = ITG3200();

float  gx, gy, gz;
float  gx_rate, gy_rate, gz_rate;
int ix, iy, iz;
float Yaw = 0.0, Pitch = 0.0, Roll = 0.0;
int ax, ay, az;
int rawX, rawY, rawZ;
float X, Y, Z;
float rollrad, Pitchrad;
float rolldeg, Pitchdeg;
int error = 0;
float aoffsetX, aoffsetY, aoffsetZ;
float goffsetX = -2.6, goffsetY = -0.6, goffsetZ = 0;
unsigned long time, looptime;
unsigned long interval = 1;
unsigned long previousMillis = 0;

float min_PWM; //default arduino 544
float max_PWM; //default arduino 2400

float H = 25.00; //Aukstis nuo zemes
float h;//tikras aukstis nuo zemes
float L;//kojos ilgis
float l;//kojos ilgis nuo klubo
float K;//Konstrukcija
float Coxa = 32.00;//Klubo ilgis
float Femur = 57.00;//┼álaunikaulio ilgis
float Tibia = 57.00;//Blauzdikaulio ilgis

float alpha_1;
float alpha_2;
float theta_1;
float theta_2;
float theta_3;

float X_1;
float X_2;
float X_3;
float X_4;
float Y_1;
float Y_2;
float Y_3;
float Y_4;
float Z_1;
float Z_2;
float Z_3;
float Z_4;

//PiD vertes
double Yaw_Setpoint, Yaw_Input, Yaw_Output;
double Roll_Setpoint, Roll_Input, Roll_Output;
double Pitch_Setpoint, Pitch_Input, Pitch_Output;

//PiD parametrai
double Yaw_Kp = 2.5, Yaw_Ki = 5, Yaw_Kd = 0;
double Roll_Kp = 2.7, Roll_Ki = 7.6, Roll_Kd = 0;
double Pitch_Kp = 3, Pitch_Ki = 8.7, Pitch_Kd = 0;

PiD Yaw_PiD(&Yaw_Input, &Yaw_Output, &Yaw_Setpoint, Yaw_Kp, Yaw_Ki, Yaw_Kd, DIRECT);
PiD Roll_PiD(&Roll_Input, &Roll_Output, &Roll_Setpoint, Roll_Kp, Roll_Ki, Roll_Kd, DIRECT);
PiD Pitch_PiD(&Pitch_Input, &Pitch_Output, &Pitch_Setpoint, Pitch_Kp, Pitch_Ki, Pitch_Kd, DIRECT);


#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define LED_PiN 13 // 


void setup()
{

  Serial.begin(9600);
  acc.powerOn();
  compass = HMC5883L();
  error = compass.SetScale(1.3); // Set the scale to +/- 1.3 Ga of the compass
  if (error != 0) // If there is an error, print it out.
    Serial.println(compass.GetErrorText(error));

  // Serial.println("Setting measurement mode to continous");
  error = compass.SetMeasurementMode(Measurement_Continuous); // Set the measurement mode to Continuous
  if (error != 0) // If there is an error, print it out.
    Serial.println(compass.GetErrorText(error));
  for (int i = 0; i <= 200; i++) {
    acc.readAccel(&ax, &ay, &az);
    gyro.readGyro(&gx, &gy, &gz);
    if (i == 0) {
      aoffsetX = ax;
      aoffsetY = ay;
      aoffsetZ = az;

    }
    if (i > 1) {
      aoffsetX = (ax + aoffsetX) / 2;
      aoffsetY = (ay + aoffsetY) / 2;
      aoffsetZ = (az + aoffsetZ) / 2;

    }
  }
  delay(500);
  gyro.init(ITG3200_ADDR_AD0_LOW);

  pwm.begin();
  i2cSetup();
  pwm.setOscillatorFrequency(27000000);  // The int.osc. is closer to 27MHz
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates

  VarSet();
  Yaw_Output = 0;
  Roll_Output = 0;
  Pitch_Output = 0;

  //nustatoma pradine roboto koju pozicija
  X_1 = 70.00;
  Y_1 = 0.00;
  Z_1 = 40.00;
  IK_1();
  pwm.setPWM(10, 0, 140 + theta_1 * 2);
  pwm.setPWM(11, 0, 180 + theta_2 * 2);
  pwm.setPWM(12, 0, 340 + theta_3);

  X_2 = 70.00;
  Y_2 = 0.00;
  Z_2 = -40.00;
  IK_2();
  pwm.setPWM(4, 0, 120 + theta_1 * 2);
  pwm.setPWM(5, 0, 400 - theta_2 * 2);
  pwm.setPWM(6, 0, 250 - theta_3);

  X_3 = 70.00;
  Y_3 = 0.00;
  Z_3 = 0.00;
  IK_3();
  pwm.setPWM(7, 0, 440 - theta_1 * 2);
  pwm.setPWM(8, 0, 470 - theta_2 * 2);
  pwm.setPWM(9, 0, 300 - theta_3);

  X_4 = 70.00;
  Y_4 = 0.00;
  Z_4 = 0.00;
  ();
  pwm.setPWM(1, 0, 500 - theta_1 * 2);
  pwm.setPWM(2, 0, 90 + theta_2 * 2);
  pwm.setPWM(3, 0, 300 + theta_3);
}

void loop()
{
  time = millis();
  Accel();
  Gyro();
  Comp();
  PidCalc();
  static long QTimer = millis();
  if ((long)( millis() - QTimer ) >= 100) {
    QTimer = millis();
    Serial.println();
    OR_Control();
    //Walk_OR();
  }
}
void Accel() {
  acc.readAccel(&ax, &ay, &az)
  rawX = ax - aoffsetX;
  rawY = ay - aoffsetY;
  rawZ = az  - (255 - aoffsetZ);
  X = rawX / 256.00;
  Y = rawY / 256.00;
  Z = rawZ / 256.00;
  rolldeg = 180 * (atan(Y / sqrt(X * X + Z * Z))) / Pi;
  pitchdeg = 180 * (atan(X / sqrt(Y * Y + Z * Z))) / Pi;
}
void Gyro() {
  gyro.readGyro(&gx, &gy, &gz);
  looptime = millis() - time;
  gx_rate = (gx - goffsetX) / 25;
  gy_rate = (gy - goffsetY) / 23;
  gz_rate = (gz - goffsetZ) / 15;
  gz_angle = gz_angle + (gz_rate * looptime);
}
void Comp() {
  Roll = 0.98 * (Roll + gx_rate * looptime) + (0.02 * rolldeg);
  Pitch = 0.98 * (Pitch + gy_rate * looptime) + (0.02 * pitchdeg);
  Yaw = gz_angle;
}
void i2cSetup() {
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
  TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif
}

void VarSet() {
  Yaw_PiD.SetMode(AUTOMATIC);
  Yaw_PiD.SetOutputLimits(-150, 150) ;
  Yaw_Setpoint = 0;
  Roll_PiD.SetMode(AUTOMATIC);
  Roll_PiD.SetOutputLimits(-150, 150) ;
  Roll_Setpoint = 0;
  Pitch_PiD.SetMode(AUTOMATIC);
  Pitch_PiD.SetOutputLimits(-150, 150) ;
  Pitch_Setpoint = 0;
}

void PidCalc() {
  Yaw_Input = Yaw;
  Yaw_PiD.Compute();
  Roll_Input = Roll;
  Roll_PiD.Compute();
  Pitch_Input = Pitch;
  Pitch_PiD.Compute();
}
void OR_Control() {
  pwm.setPWM(1, 0, 300 - Yaw_Output);
  pwm.setPWM(4, 0, 300 - Yaw_Output);
  pwm.setPWM(7, 0, 260 - Yaw_Output);
  pwm.setPWM(10, 0, 310 - Yaw_Output);

  pwm.setPWM(2, 0, 260 + Pitch_Output - Roll_Output);
  pwm.setPWM(5, 0, 260 + Pitch_Output + Roll_Output);
  pwm.setPWM(8, 0, 315 - Pitch_Output - Roll_Output);
  pwm.setPWM(12, 0, 390 - Pitch_Output + Roll_Output);

  pwm.setPWM(3, 0, 360 + Pitch_Output - Roll_Output);
  pwm.setPWM(6, 0, 180 + Pitch_Output + Roll_Output);
  pwm.setPWM(9, 0, 210 - Pitch_Output - Roll_Output);
  pwm.setPWM(11, 0, 290 - Pitch_Output + Roll_Output);;
  
  Serial.print(F("\t Roll")); Serial.print(Roll);
  Serial.print(F("\t Roll_Out")); Serial.print(Roll_Output );
  Serial.print(F("\t Pitch ")); Serial.print(Pitch);
  Serial.print(F("\t Pitch_Out ")); Serial.print(Pitch_Output);
}

void IK_1()
{
  if (Z_1 > 0.00)
  {
    L = sqrt(pow(X_1, 2) + pow(Z_1, 2));
    theta_1 = (atan(X_1 / Z_1)) * (180.00 / PI);
    l = L - Coxa;
    h = H - Y_1;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_1 == 0.00)
  {
    L = sqrt(pow(X_1, 2) + pow(Z_1, 2));
    theta_1 = 90.00;
    l = L - Coxa;
    h = H - Y_1;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_1 < 0.00)
  {
    L = sqrt(pow(X_1, 2) + pow(Z_1, 2));
    theta_1 = 90.00 + (90.00 - abs((atan(X_1 / Z_1)) * (180.00 / PI)));
    l = L - Coxa;
    h = H - Y_1;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
}

void IK_2()
{
  if (Z_2 > 0.00)
  {
    L = sqrt(pow(X_2, 2) + pow(Z_2, 2));
    theta_1 = (atan(X_2 / Z_2)) * (180.00 / PI);
    l = L - Coxa;
    h = H - Y_2;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_2 == 0.00)
  {
    L = sqrt(pow(X_2, 2) + pow(Z_2, 2));
    theta_1 = 90.00;
    l = L - Coxa;
    h = H - Y_2;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_2 < 0.00)
  {
    L = sqrt(pow(X_2, 2) + pow(Z_2, 2));
    theta_1 = 90.00 + (90.00 - abs((atan(X_2 / Z_2)) * (180.00 / PI)));
    l = L - Coxa;
    h = H - Y_2;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
}

void IK_3()
{
  if (Z_3 > 0.00)
  {
    L = sqrt(pow(X_3, 2) + pow(Z_3, 2));
    theta_1 = (atan(X_3 / Z_3)) * (180.00 / PI);
    l = L - Coxa;
    h = H - Y_3;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_3 == 0.00)
  {
    L = sqrt(pow(X_3, 2) + pow(Z_3, 2));
    theta_1 = 90.00;
    l = L - Coxa;
    h = H - Y_3;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_3 < 0.00)
  {
    L = sqrt(pow(X_3, 2) + pow(Z_3, 2));
    theta_1 = 90.00 + (90.00 - abs((atan(X_3 / Z_3)) * (180.00 / PI)));
    l = L - Coxa;
    h = H - Y_3;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
}

void IK_4()
{
  if (Z_4 > 0.00)
  {
    L = sqrt(pow(X_4, 2) + pow(Z_4, 2));
    theta_1 = (atan(X_4 / Z_4)) * (180.00 / PI);
    l = L - Coxa;
    h = H - Y_4;
    K = sqrt(pow(l, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_4 == 0.00)
  {
    L = sqrt(pow(X_4, 2) + pow(Z_4, 2));
    theta_1 = 90.00;
    l = L - Coxa;
    h = H - Y_4;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
  else if (Z_4 < 0.00)
  {
    L = sqrt(pow(X_4, 2) + pow(Z_4, 2));
    theta_1 = 90.00 + (90.00 - abs((atan(X_4 / Z_4)) * (180.00 / PI)));
    l = L - Coxa;
    h = H - Y_4;
    K = sqrt(pow(d, 2) + pow(h, 2));
    alpha_1 = (acos(h / K)) * (180.00 / PI);
    alpha_2 = (acos((pow(Femur, 2) + pow(K, 2) - pow(Tibia, 2)) / (2 * Femur * K))) * (180.00 / PI);
    theta_2 = (alpha_1 + alpha_2);
    theta_3 = ((acos((pow(Femur, 2) + pow(Tibia, 2) - pow(K, 2)) / (2 * Femur * Tibia))) * (180.00 / PI));
  }
}

void Walk_OR() {
  if (currentMillis - previousMillis >= interval)
  {
    //STEP 1----------------------------------------------------------------------------------------------------------------------------------
    Y_3 = 0.00;
    do
    {
      X_3 = 70.00;
      Z_3 = 0.00;
      Y_3 += 0.6;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
    }
    while (Y_3 >= 25.00);
    Z_3 = 0.00;
    do
    {
      X_3 = 70.00;
      Z_3 += 0.6;
      Y_3 = 25;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
    }
    while (Z_3 <= 80.00);
    Y_3 = 25.00;
    do
    {
      X_3 = 70.00;
      Z_3 = 80;
      Y_3 -= 0.6;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
    }
    while (Y_3 >= 0.0);
    Z_1 = 40.00;
    Z_2 = -60.00;
    Z_3 = 80.00;
    Z_4 = -20.00;
    do
    {
      X_1 = 70.00;
      Y_1 = 0.00;
      Z_1 -= 0.30;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
      X_2 = 70.00;
      Y_2 = 0.00;
      Z_2 -= 0.30;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
      X_3 = 70.00;
      Y_3 = 0.00;
      Z_3 -= 0.30;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
      X_4 = 70.00;
      Y_4 = 0.00;
      Z_4 -= 0.30;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Z_1 >= -20.00 && Z_2 >= -80.00 && Z_3 >= 60.00 && Z_4 >= -40.00);
    //STEP 2----------------------------------------------------------------------------------------------------------------------------------
    Y_2 = 0;
    do {
      X_2 = 70.00;
      Y_2 += 0.60;
      Z_2 = -80.00;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
    }
    while (Y_2 >= 25.00);
    Z_2 = -80.00;
    do
    {
      X_2 = 70.00;
      Y_2 = 25.00;
      Z_2 += 0.60;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
    }
    while (Z_2 <= 0.00);
    Y_2 = 25.00;
    do
    {
      X_2 = 70.00;
      Y_2 -= 0.60;
      Z_2 = 0.00;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
    }
    while (Y_2 >= 0.00);
    Y_2 = 25.00;
    Z_1 = 20.00;
    Z_2 = 0.00;
    Z_3 = 60.00;
    Z_4 = -40.00;
    do
    {
      X_1 = 70.00;
      Y_1 = 0.00;
      Z_1 -= 0.30;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
      X_2 = 70.00;
      Y_2 = 0.00;
      Z_2 -= 0.30;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
      X_3 = 70.00;
      Y_3 = 0.00;
      Z_3 -= 0.30;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
      X_4 = 70.00;
      Y_4 = 0.00;
      Z_4 -= 0.30;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Z_1 >= 0.00 && Z_2 >= -20.00 && Z_3 >= 40.00 && Z_4 >= -60.00);
    //STEP 3----------------------------------------------------------------------------------------------------------------------------------
    Y_1 = 0.00;
    do
    {
      X_1 = 70.00;
      Y_1 += 0.40;
      Z_1 = 0.0;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
      Serial.print(theta_3);
    }
    while (Y_1 <= 25.00);
    Z_1 = 0.00;
    do
    {
      X_1 = 70.00;
      Y_1 = 25.00;
      Z_1 += 0.40;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
    }
    while (Z_1 <= 80.00);
    Y_1 = 25.00;
    do
    {
      X_1 = 70.00;
      Y_1 -= 0.40;
      Z_1 = 80.0;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
    }
    while (Y_1 >= 0.00);
    Z_1 = 80.00;
    Z_2 = -20.00;
    Z_3 = 40.00;
    Z_4 = -60.00;
    do
    {
      X_1 = 70.00;
      Y_1 = 0.00;
      Z_1 -= 0.30;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
      X_2 = 70.00;
      Y_2 = 0.00;
      Z_2 -= 0.30;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
      X_3 = 70.00;
      Y_3 = 0.00;
      Z_3 -= 0.30;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
      X_4 = 70.00;
      Y_4 = 0.00;
      Z_4 -= 0.30;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Z_1 >= 60.00 && Z_2 >= -40.00 && Z_3 >= 20.00 && Z_4 >= -80.00);
    //STEP 4----------------------------------------------------------------------------------------------------------------------------------
    Y_4 = 0.00;
    do
    {
      X_4 = 70.00;
      Y_4 += 0.40;
      Z_4 = -80.00;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Y_4 <= 25.00);
    Z_4 = -80.00;
    do
    {
      X_4 = 70.00;
      Y_4 = 25.00;
      Z_4 += 0.40;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Z_4 >= 0.00);
    Y_4 = 25.00;
    do
    {
      X_4 = 70.00;
      Y_4 -= 0.40;
      Z_4 = 0.00;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Y_4 >= 0.00);
    Z_1 = 60.00;
    Z_2 = -40.00;
    Z_3 = 20.00;
    Z_4 = 0.00;
    do
    {
      X_1 = 70.00;
      Y_1 = 0.00;
      Z_1 -= 0.30;
      IK_1();
      pwm.setPWM(10, 0, 140 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(11, 0, 130 + theta_2 * 2 - Pitch_Output + Roll_Output);
      pwm.setPWM(12, 0, 410 + theta_3 - Pitch_Output + Roll_Output);
      X_2 = 70.00;
      Y_2 = 0.00;
      Z_2 -= 0.30;
      IK_2();
      pwm.setPWM(4, 0, 120 + theta_1 * 2 - Yaw_Output);
      pwm.setPWM(5, 0, 430 - theta_2 * 2 + Pitch_Output + Roll_Output);
      pwm.setPWM(6, 0, 170 - theta_3 + Pitch_Output + Roll_Output);
      X_3 = 70.00;
      Y_3 = 0.00;
      Z_3 -= 0.30;
      IK_3();
      pwm.setPWM(7, 0, 440 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(8, 0, 470 - theta_2 * 2 - Pitch_Output - Roll_Output);
      pwm.setPWM(9, 0, 220 - theta_3 - Pitch_Output - Roll_Output);
      X_4 = 70.00;
      Y_4 = 0.00;
      Z_4 -= 0.30;
      IK_4();
      pwm.setPWM(1, 0, 500 - theta_1 * 2 - Yaw_Output);
      pwm.setPWM(2, 0, 90 + theta_2 * 2 + Pitch_Output - Roll_Output);
      pwm.setPWM(3, 0, 370 + theta_3 + Pitch_Output - Roll_Output);
    }
    while (Z_1 >= 40.00 && Z_2 >= -60.00 && Z_3 >= 0.00 && Z_4 >= -20.00);
    previousMillis = millis();
  }
}
