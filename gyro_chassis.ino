#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Servo.h>
#include <math.h>
#include "config.h"
#include "telemetry.h"
#include "web_server.h"

MPU6050 mpuSensor;

Servo servoFrontLeft;  // Front Left servo (левый передний привод)
Servo servoFrontRight; // Front Right servo (правый передний привод)
Servo servoRearLeft;   // Rear Left servo (левый задний привод)
Servo servoRearRight;  // Rear Right servo (правый задний привод)

// Фильтры для ускорений / Acceleration smoothing filters
float filteredLongitudinalG = 0.0f;
float filteredLateralG      = 0.0f;
float filteredVerticalG     = 0.0f;

// Комплементарный фильтр для углов крена и тангажа
float fusedRollDegrees  = 0.0f;
float fusedPitchDegrees = 0.0f;

// Servo state that models the suspension with a simple spring-damper model
// Состояние сервы, имитирующее подвеску через простую модель пружины и демпфера
struct SpringServo {
  float positionDegrees;           // Текущий угол сервы в градусах / Current servo angle in degrees
  float velocityDegreesPerSecond;  // Угловая скорость в град/с / Angular velocity in deg/s
};

SpringServo springServoFrontLeft, springServoFrontRight, springServoRearLeft, springServoRearRight;

unsigned long lastUpdateMilliseconds = 0; // Отметка времени последнего цикла / Timestamp of last loop

// Step physics simulation for one servo
// Выполняем шаг физической модели для одной сервы
void updateSpringServo(
  SpringServo &springServo,
  float targetPositionDegrees,
  float deltaTimeSeconds,
  float stiffnessCoefficient,
  float dampingCoefficient
) {
  // Оцениваем отклонение от цели и силу пружины с демпфированием
  // Compute deviation from the target and resulting spring-damper force
  float positionErrorDegrees = springServo.positionDegrees - targetPositionDegrees;
  float springDamperForce = -stiffnessCoefficient * positionErrorDegrees - dampingCoefficient * springServo.velocityDegreesPerSecond;

  // В простейшей модели ускорение совпадает с силой (масса = 1)
  // In this simplified model acceleration equals force (mass = 1)
  float angularAcceleration = springDamperForce;

  springServo.velocityDegreesPerSecond += angularAcceleration * deltaTimeSeconds;
  springServo.positionDegrees += springServo.velocityDegreesPerSecond * deltaTimeSeconds;

  // Ограничиваем ход подвески вокруг базового офсета
  // Clamp suspension travel around the base offset
  float minAllowedPosition = suspensionOffsetDegrees - suspensionHalfRangeDegrees;
  float maxAllowedPosition = suspensionOffsetDegrees + suspensionHalfRangeDegrees;

  if (springServo.positionDegrees < minAllowedPosition) {
    springServo.positionDegrees = minAllowedPosition;
    springServo.velocityDegreesPerSecond = 0;
  }
  if (springServo.positionDegrees > maxAllowedPosition) {
    springServo.positionDegrees = maxAllowedPosition;
    springServo.velocityDegreesPerSecond = 0;
  }

  // Гарантируем допустимые углы сервы 0..180 градусов
  // Guarantee servo angles stay within the 0..180 degree hardware limits
  if (springServo.positionDegrees < 0)   springServo.positionDegrees = 0;
  if (springServo.positionDegrees > 180) springServo.positionDegrees = 180;
}

void setup() {
  Serial.begin(115200);

  loadConfig();

  // Настраиваем шину I2C и инерциальный модуль / Set up I2C bus and IMU
  Wire.begin(21, 22);
  mpuSensor.initialize();

  // Одноразово читаем акселерометр и задаём стартовые углы для комплементарного фильтра
  int16_t ax0, ay0, az0;
  mpuSensor.getAcceleration(&ax0, &ay0, &az0);
  float accX0 = ax0 / 16384.0f;
  float accY0 = ay0 / 16384.0f;
  float accZ0 = az0 / 16384.0f;
  fusedRollDegrees  = atan2(accY0, accZ0) * 180.0f / PI;
  fusedPitchDegrees = atan2(-accX0, sqrt(accY0 * accY0 + accZ0 * accZ0)) * 180.0f / PI;

  // Привязываем сервы к соответствующим выводам / Attach servos to pins
  servoFrontLeft.attach(33);
  servoFrontRight.attach(32);
  servoRearLeft.attach(27);
  servoRearRight.attach(26);

  // Инициализируем состояние подвески в базовой позиции / Initialize suspension at base position
  springServoFrontLeft.positionDegrees = springServoFrontRight.positionDegrees = suspensionOffsetDegrees;
  springServoRearLeft.positionDegrees  = springServoRearRight.positionDegrees  = suspensionOffsetDegrees;
  springServoFrontLeft.velocityDegreesPerSecond = springServoFrontRight.velocityDegreesPerSecond = 0.0f;
  springServoRearLeft.velocityDegreesPerSecond  = springServoRearRight.velocityDegreesPerSecond  = 0.0f;

  startWeb();

  lastUpdateMilliseconds = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  float deltaTimeSeconds = (currentMillis - lastUpdateMilliseconds) / 1000.0f;
  if (deltaTimeSeconds <= 0)   deltaTimeSeconds = 0.001f; // Защита от нуля / Guard against zero
  if (deltaTimeSeconds > 0.05) deltaTimeSeconds = 0.05f;  // Ограничиваем шаг интеграции / Cap integration step
  lastUpdateMilliseconds = currentMillis;

  // Читаем ускорения и гироскоп / Read raw acceleration and gyro
  int16_t rawAX, rawAY, rawAZ, rawGX, rawGY, rawGZ;
  mpuSensor.getMotion6(&rawAX, &rawAY, &rawAZ, &rawGX, &rawGY, &rawGZ);

  // Переводим в G / Convert to G
  accelerationXG = rawAX / 16384.0f;
  accelerationYG = rawAY / 16384.0f;
  accelerationZG = rawAZ / 16384.0f;

  // Гироскоп в град/с (для диапазона ±250°/s) / Gyro in deg/s
  gyroXDegreesPerSecond = rawGX / 131.0f;
  gyroYDegreesPerSecond = rawGY / 131.0f;
  gyroZDegreesPerSecond = rawGZ / 131.0f;

  // Углы по акселерометру / Tilt angles from accelerometer
  float accRollDegrees  = atan2(accelerationYG, accelerationZG) * 180.0f / PI;
  float accPitchDegrees = atan2(-accelerationXG,
                                sqrt(accelerationYG * accelerationYG + accelerationZG * accelerationZG))
                          * 180.0f / PI;

  // Комплементарный фильтр: объединяем гироскоп и акселерометр
  const float complementaryAlpha = 0.98f; // доверяем гироскопу на 98%
  fusedRollDegrees  = complementaryAlpha * (fusedRollDegrees  + gyroXDegreesPerSecond * deltaTimeSeconds)
                    + (1.0f - complementaryAlpha) * accRollDegrees;
  fusedPitchDegrees = complementaryAlpha * (fusedPitchDegrees + gyroYDegreesPerSecond * deltaTimeSeconds)
                    + (1.0f - complementaryAlpha) * accPitchDegrees;

  // Обрезаем углы для устойчивости / Constrain angles for stability
  float limitedRollDegrees  = constrain(fusedRollDegrees,  -30.0f, 30.0f);
  float limitedPitchDegrees = constrain(fusedPitchDegrees, -30.0f, 30.0f);

  currentRollDegrees  = limitedRollDegrees;
  currentPitchDegrees = limitedPitchDegrees;

  // Нормируем углы до диапазона -1..1 / Normalize angles to -1..1
  float normalizedRoll  = constrain(limitedRollDegrees  / 30.0f, -1.0f, 1.0f);
  float normalizedPitch = constrain(limitedPitchDegrees / 30.0f, -1.0f, 1.0f);

  // Динамика по ускорениям с экспоненциальным сглаживанием / Dynamic response using filtered accelerations
  float longitudinalAccelerationG = constrain(accelerationXG, -1.5f, 1.5f);
  float lateralAccelerationG      = constrain(accelerationYG, -1.5f, 1.5f);
  float verticalAccelerationG     = constrain(accelerationZG - 1.0f, -1.5f, 1.5f);

  float filterAlpha = constrain(accelerationFilterAlpha, 0.0f, 1.0f);
  filteredLongitudinalG = filteredLongitudinalG * (1.0f - filterAlpha) + longitudinalAccelerationG * filterAlpha;
  filteredLateralG      = filteredLateralG * (1.0f - filterAlpha) + lateralAccelerationG * filterAlpha;
  filteredVerticalG     = filteredVerticalG * (1.0f - filterAlpha) + verticalAccelerationG * filterAlpha;

  float dynamicPitch = filteredLongitudinalG * dynamicPitchInfluence;
  float dynamicRoll  = filteredLateralG * dynamicRollInfluence;
  float dynamicHeave = filteredVerticalG * dynamicHeaveInfluence;

  // Разносим сигнал по осям шасси / Map blended signals to chassis axes
  float frontPitchComponent = normalizedPitch * frontBalanceFactor;
  float rearPitchComponent  = normalizedPitch * rearBalanceFactor;
  float rollSideComponent   = normalizedRoll;

  // Микшируем команды для каждой сервы / Mix servo commands per corner
  float servoMixFrontLeft  = constrain(frontPitchComponent + dynamicHeave - rollSideComponent + dynamicPitch, -1.0f, 1.0f);
  float servoMixFrontRight = constrain(frontPitchComponent + dynamicHeave + rollSideComponent + dynamicPitch, -1.0f, 1.0f);
  float servoMixRearLeft   = constrain(-rearPitchComponent + dynamicHeave - rollSideComponent - dynamicPitch, -1.0f, 1.0f);
  float servoMixRearRight  = constrain(-rearPitchComponent + dynamicHeave + rollSideComponent - dynamicPitch, -1.0f, 1.0f);

  // Целевые углы с учётом базового офсета и допустимого хода / Target angles with base offset and travel limits
  float targetFrontLeftDegrees  = suspensionOffsetDegrees + servoMixFrontLeft  * suspensionHalfRangeDegrees;
  float targetFrontRightDegrees = suspensionOffsetDegrees + servoMixFrontRight * suspensionHalfRangeDegrees;
  float targetRearLeftDegrees   = suspensionOffsetDegrees + servoMixRearLeft   * suspensionHalfRangeDegrees;
  float targetRearRightDegrees  = suspensionOffsetDegrees + servoMixRearRight  * suspensionHalfRangeDegrees;

  updateSpringServo(springServoFrontLeft,  targetFrontLeftDegrees,  deltaTimeSeconds, frontSpringStiffness, frontDampingCoefficient);
  updateSpringServo(springServoFrontRight, targetFrontRightDegrees, deltaTimeSeconds, frontSpringStiffness, frontDampingCoefficient);
  updateSpringServo(springServoRearLeft,   targetRearLeftDegrees,   deltaTimeSeconds, rearSpringStiffness,  rearDampingCoefficient);
  updateSpringServo(springServoRearRight,  targetRearRightDegrees,  deltaTimeSeconds, rearSpringStiffness,  rearDampingCoefficient);

  // Отправляем итоговые углы на сервы / Send final angles to physical servos
  servoFrontLeft.write((int)springServoFrontLeft.positionDegrees);
  servoFrontRight.write((int)springServoFrontRight.positionDegrees);
  servoRearLeft.write((int)springServoRearLeft.positionDegrees);
  servoRearRight.write((int)springServoRearRight.positionDegrees);

  handleWeb();
  delay(5); // Небольшая пауза для стабильности / Small delay for stability
}
