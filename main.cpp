#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Servo.h>
#include <math.h>
#include "config.h"
#include "telemetry.h"
#include "web_server.h"

MPU6050 mpu;

Servo servoFL; // Front Left
Servo servoFR; // Front Right
Servo servoRL; // Rear Left
Servo servoRR; // Rear Right

struct SpringServo {
  float x;
  float v;
};

SpringServo sFL, sFR, sRL, sRR;

unsigned long lastUpdate = 0;

void updateSpringServo(SpringServo &s, float target, float dt, float k, float c) {
  float error = s.x - target;
  float force = -k * error - c * s.v;
  float a = force;

  s.v += a * dt;
  s.x += s.v * dt;

  float minPos = offset - suspHalf;
  float maxPos = offset + suspHalf;

  if (s.x < minPos) { s.x = minPos; s.v = 0; }
  if (s.x > maxPos) { s.x = maxPos; s.v = 0; }

  if (s.x < 0)   s.x = 0;
  if (s.x > 180) s.x = 180;
}

void setup() {
  Serial.begin(115200);

  loadConfig();

  Wire.begin(21, 22);
  mpu.initialize();

  servoFL.attach(13);
  servoFR.attach(14);
  servoRL.attach(15);
  servoRR.attach(16);

  sFL.x = sFR.x = sRL.x = sRR.x = offset;
  sFL.v = sFR.v = sRL.v = sRR.v = 0.0f;

  startWeb();

  lastUpdate = millis();
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0f;
  if (dt <= 0)   dt = 0.001f;
  if (dt > 0.05) dt = 0.05f;
  lastUpdate = now;

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  accX = ax / 16384.0f;
  accY = ay / 16384.0f;
  accZ = az / 16384.0f;

  float angleX = atan2(accY, accZ) * 180.0f / PI;
  float angleY = atan2(-accX, sqrt(accY * accY + accZ * accZ)) * 180.0f / PI;

  float roll  = constrain(angleX, -30.0f, 30.0f);
  float pitch = constrain(angleY, -30.0f, 30.0f);

  currentRoll  = roll;
  currentPitch = pitch;

  float normRoll  = constrain(roll  / 30.0f, -1.0f, 1.0f);
  float normPitch = constrain(pitch / 30.0f, -1.0f, 1.0f);

  float pitchFront = normPitch * frontBalance;
  float pitchRear  = normPitch * rearBalance;
  float rollSide   = normRoll;

  float mixFL = constrain(pitchFront - rollSide, -1.0f, 1.0f);
  float mixFR = constrain(pitchFront + rollSide, -1.0f, 1.0f);
  float mixRL = constrain(-pitchRear - rollSide, -1.0f, 1.0f);
  float mixRR = constrain(-pitchRear + rollSide, -1.0f, 1.0f);

  float targetFL = offset + mixFL * suspHalf;
  float targetFR = offset + mixFR * suspHalf;
  float targetRL = offset + mixRL * suspHalf;
  float targetRR = offset + mixRR * suspHalf;

  updateSpringServo(sFL, targetFL, dt, kFront, cFront);
  updateSpringServo(sFR, targetFR, dt, kFront, cFront);
  updateSpringServo(sRL, targetRL, dt, kRear,  cRear);
  updateSpringServo(sRR, targetRR, dt, kRear,  cRear);

  servoFL.write((int)sFL.x);
  servoFR.write((int)sFR.x);
  servoRL.write((int)sRL.x);
  servoRR.write((int)sRR.x);

  handleWeb();
  delay(5);
}
