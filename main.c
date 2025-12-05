#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Servo.h>

// ----- MPU6050 -----
MPU6050 mpu;

// ----- Сервы -----
Servo servoFL; // Front Left
Servo servoFR; // Front Right
Servo servoRL; // Rear Left
Servo servoRR; // Rear Right

// Структура "серва как пружина"
struct SpringServo {
  float x; // текущее положение (градусы)
  float v; // "скорость" (для физики)
};

SpringServo sFL, sFR, sRL, sRR;

// ----- Параметры подвески -----
float offset      = 90.0;   // базовая высота (центр хода подвески)
float share       = 0.25;   // доля хода сервы под подвеску (1/4)
float totalRange  = 180.0;  
float suspRange   = totalRange * share;      // общий ход подвески (градусы)
float suspHalf    = suspRange / 2.0;         // половина хода (±)

// Жёсткость и демпфирование спереди/сзади
float kFront = 5.0;   // жёсткость пружины (перед)
float cFront = 1.5;   // демпфирование (перед)
float kRear  = 3.0;   // жёсткость пружины (зад)
float cRear  = 1.2;   // демпфирование (зад)

// Баланс перед/зад (как сильно реагируют на pitch)
float frontBalance = 1.0;  // перед полностью повторяет крен/клюк
float rearBalance  = 0.8;  // зад чуть мягче по тангажу

unsigned long lastUpdate = 0;

// Обновление одной сервы как пружины
void updateSpringServo(SpringServo &s, float target, float dt, float k, float c) {
  float error = s.x - target;
  float force = -k * error - c * s.v;
  float a = force; // масса = 1

  s.v += a * dt;
  s.x += s.v * dt;

  // Ограничиваем в пределах нашей 1/4 подвески
  float minPos = offset - suspHalf;
  float maxPos = offset + suspHalf;

  if (s.x < minPos) { s.x = minPos; s.v = 0; }
  if (s.x > maxPos) { s.x = maxPos; s.v = 0; }

  // Доп. страховка под серву (на всякий)
  if (s.x < 0)   s.x = 0;
  if (s.x > 180) s.x = 180;
}

void setup() {
  Serial.begin(115200);

  // I2C для ESP32: SDA=21, SCL=22
  Wire.begin(21, 22);
  mpu.initialize();

  // Можно глянуть, жив ли датчик:
  // Serial.println(mpu.testConnection() ? "MPU OK" : "MPU FAIL");

  // Пины серво (под ESP32 нормальные GPIO)
  servoFL.attach(13); // Front Left
  servoFR.attach(14); // Front Right
  servoRL.attach(15); // Rear Left
  servoRR.attach(16); // Rear Right

  // Стартовая позиция всех серв — в центре подвески
  sFL.x = sFR.x = sRL.x = sRR.x = offset;
  sFL.v = sFR.v = sRL.v = sRR.v = 0.0;

  lastUpdate = millis();
}

void loop() {
  // --- расчёт dt ---
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  if (dt <= 0)   dt = 0.001;
  if (dt > 0.05) dt = 0.05; // защита, чтобы физика не разлетелась
  lastUpdate = now;

  // --- читаем акселерометр ---
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // в g (для MPU6050 при ±2g → делитель 16384)
  float a_x = ax / 16384.0;
  float a_y = ay / 16384.0;
  float a_z = az / 16384.0;

  // углы (крен/тангаж)
  float angleX = atan2(a_y, a_z) * 180.0 / PI;                       // roll (лево/право)
  float angleY = atan2(-a_x, sqrt(a_y * a_y + a_z * a_z)) * 180.0 / PI; // pitch (вперёд/назад)

  // ограничим до вменяемых  ±30°
  float roll  = constrain(angleX, -30.0, 30.0);
  float pitch = constrain(angleY, -30.0, 30.0);

  // нормируем -30..30 → -1..1
  float normRoll  = constrain(roll  / 30.0, -1.0, 1.0);
  float normPitch = constrain(pitch / 30.0, -1.0, 1.0);

  // учитываем баланс (перед/зад отдельно)
  float pitchFront = normPitch * frontBalance;
  float pitchRear  = normPitch * rearBalance;
  float rollSide   = normRoll; // левая/правая сторона одинаково

  // Для каждого колеса получаем "микс" в диапазоне -1..1
  float mixFL = constrain(pitchFront - rollSide, -1.0, 1.0);
  float mixFR = constrain(pitchFront + rollSide, -1.0, 1.0);
  float mixRL = constrain(-pitchRear - rollSide, -1.0, 1.0);
  float mixRR = constrain(-pitchRear + rollSide, -1.0, 1.0);

  // Превращаем -1..1 → offset ± suspHalf (1/4 хода сервы)
  float targetFL = offset + mixFL * suspHalf;
  float targetFR = offset + mixFR * suspHalf;
  float targetRL = offset + mixRL * suspHalf;
  float targetRR = offset + mixRR * suspHalf;

  // --- обновляем физику для каждой сервы ---
  updateSpringServo(sFL, targetFL, dt, kFront, cFront);
  updateSpringServo(sFR, targetFR, dt, kFront, cFront);
  updateSpringServo(sRL, targetRL, dt, kRear,  cRear);
  updateSpringServo(sRR, targetRR, dt, kRear,  cRear);

  // --- пишем в сервы ---
  servoFL.write((int)sFL.x);
  servoFR.write((int)sFR.x);
  servoRL.write((int)sRL.x);
  servoRR.write((int)sRR.x);

  // Для отладки можно смотреть углы и позиции:
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
