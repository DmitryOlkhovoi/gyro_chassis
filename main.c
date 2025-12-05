#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Servo.h>

// ----- MPU6050 -----
// ----- MPU6050 IMU -----
MPU6050 mpu;

// ----- Сервы -----
// ----- Servos -----
Servo servoFL; // Front Left
Servo servoFR; // Front Right
Servo servoRL; // Rear Left
Servo servoRR; // Rear Right

// Структура "серва как пружина"
// Structure "servo as a spring"
struct SpringServo {
  // текущее положение (градусы)
  // current position (degrees)
  float x;
  // "скорость" (для физики)
  // "velocity" (for physics)
  float v;
};

SpringServo sFL, sFR, sRL, sRR;

// ----- Параметры подвески -----
// ----- Suspension parameters -----
// базовая высота (центр хода подвески)
// base height (center of suspension travel)
float offset      = 90.0;
// доля хода сервы под подвеску (1/4)
// fraction of servo travel allocated to suspension (1/4)
float share       = 0.25;
float totalRange  = 180.0;
// общий ход подвески (градусы)
// total suspension travel (degrees)
float suspRange   = totalRange * share;
// половина хода (±)
// half of travel (±)
float suspHalf    = suspRange / 2.0;

// Жёсткость и демпфирование спереди/сзади
// Stiffness and damping front/rear
// жёсткость пружины (перед)
// spring stiffness (front)
float kFront = 5.0;
// демпфирование (перед)
// damping (front)
float cFront = 1.5;
// жёсткость пружины (зад)
// spring stiffness (rear)
float kRear  = 3.0;
// демпфирование (зад)
// damping (rear)
float cRear  = 1.2;

// Баланс перед/зад (как сильно реагируют на pitch)
// Front/rear balance (how strongly they react to pitch)
// перед полностью повторяет крен/клюк
// front fully follows roll/pitch
float frontBalance = 1.0;
// зад чуть мягче по тангажу
// rear is slightly softer on pitch
float rearBalance  = 0.8;

unsigned long lastUpdate = 0;

// Обновление одной сервы как пружины
// Updating a single servo as a spring
void updateSpringServo(SpringServo &s, float target, float dt, float k, float c) {
  float error = s.x - target;
  float force = -k * error - c * s.v;
  // масса = 1
  // mass = 1
  float a = force;

  s.v += a * dt;
  s.x += s.v * dt;

  // Ограничиваем в пределах нашей 1/4 подвески
  // Limit within our 1/4 suspension travel
  float minPos = offset - suspHalf;
  float maxPos = offset + suspHalf;

  if (s.x < minPos) { s.x = minPos; s.v = 0; }
  if (s.x > maxPos) { s.x = maxPos; s.v = 0; }

  // Доп. страховка под серву (на всякий)
  // Extra safety clamp for the servo (just in case)
  if (s.x < 0)   s.x = 0;
  if (s.x > 180) s.x = 180;
}

void setup() {
  Serial.begin(115200);

  // I2C для ESP32: SDA=21, SCL=22
  // I2C for ESP32: SDA=21, SCL=22
  Wire.begin(21, 22);
  mpu.initialize();

  // Можно глянуть, жив ли датчик:
  // You can check whether the sensor is alive:
  // Serial.println(mpu.testConnection() ? "MPU OK" : "MPU FAIL");

  // Пины серво (под ESP32 нормальные GPIO)
  // Servo pins (regular ESP32 GPIOs)
  servoFL.attach(13); // Front Left
  servoFR.attach(14); // Front Right
  servoRL.attach(15); // Rear Left
  servoRR.attach(16); // Rear Right

  // Стартовая позиция всех серв — в центре подвески
  // Initial position of all servos — centered in the suspension
  sFL.x = sFR.x = sRL.x = sRR.x = offset;
  sFL.v = sFR.v = sRL.v = sRR.v = 0.0;

  lastUpdate = millis();
}

void loop() {
  // --- расчёт dt ---
  // --- dt calculation ---
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  if (dt <= 0)   dt = 0.001;
  // защита, чтобы физика не разлетелась
  // safeguard so the physics doesn't blow up
  if (dt > 0.05) dt = 0.05;
  lastUpdate = now;

  // --- читаем акселерометр ---
  // --- read accelerometer ---
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // в g (для MPU6050 при ±2g → делитель 16384)
  // in g-units (for MPU6050 at ±2g → divider 16384)
  float a_x = ax / 16384.0;
  float a_y = ay / 16384.0;
  float a_z = az / 16384.0;

  // углы (крен/тангаж)
  // angles (roll/pitch)
  // roll (left/right)
  float angleX = atan2(a_y, a_z) * 180.0 / PI;                       // roll (лево/право)
  // pitch (forward/back)
  float angleY = atan2(-a_x, sqrt(a_y * a_y + a_z * a_z)) * 180.0 / PI; // pitch (вперёд/назад)

  // ограничим до вменяемых  ±30°
  // clamp to a sane ±30°
  float roll  = constrain(angleX, -30.0, 30.0);
  float pitch = constrain(angleY, -30.0, 30.0);

  // нормируем -30..30 → -1..1
  // normalize -30..30 → -1..1
  float normRoll  = constrain(roll  / 30.0, -1.0, 1.0);
  float normPitch = constrain(pitch / 30.0, -1.0, 1.0);

  // учитываем баланс (перед/зад отдельно)
  // account for balance (front/rear separately)
  float pitchFront = normPitch * frontBalance;
  float pitchRear  = normPitch * rearBalance;
  // левая/правая сторона одинаково
  // left/right sides react the same
  float rollSide   = normRoll;

  // Для каждого колеса получаем "микс" в диапазоне -1..1
  // For each wheel we calculate a "mix" in the -1..1 range
  float mixFL = constrain(pitchFront - rollSide, -1.0, 1.0);
  float mixFR = constrain(pitchFront + rollSide, -1.0, 1.0);
  float mixRL = constrain(-pitchRear - rollSide, -1.0, 1.0);
  float mixRR = constrain(-pitchRear + rollSide, -1.0, 1.0);

  // Превращаем -1..1 → offset ± suspHalf (1/4 хода сервы)
  // Convert -1..1 → offset ± suspHalf (1/4 of servo travel)
  float targetFL = offset + mixFL * suspHalf;
  float targetFR = offset + mixFR * suspHalf;
  float targetRL = offset + mixRL * suspHalf;
  float targetRR = offset + mixRR * suspHalf;

  // --- обновляем физику для каждой сервы ---
  // --- update the physics for each servo ---
  updateSpringServo(sFL, targetFL, dt, kFront, cFront);
  updateSpringServo(sFR, targetFR, dt, kFront, cFront);
  updateSpringServo(sRL, targetRL, dt, kRear,  cRear);
  updateSpringServo(sRR, targetRR, dt, kRear,  cRear);

  // --- пишем в сервы ---
  // --- write to servos ---
  servoFL.write((int)sFL.x);
  servoFR.write((int)sFR.x);
  servoRL.write((int)sRL.x);
  servoRR.write((int)sRR.x);

  // Для отладки можно смотреть углы и позиции:
  // For debugging you can watch angles and positions:
  /*
  Serial.print("roll=");   Serial.print(roll);
  Serial.print(" pitch="); Serial.print(pitch);
  Serial.print(" | FL=");  Serial.print(sFL.x);
  Serial.print(" FR=");    Serial.print(sFR.x);
  Serial.print(" RL=");    Serial.print(sRL.x);
  Serial.print(" RR=");    Serial.println(sRR.x);
  */

  delay(5);
}
