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

  // Читаем ускорения по осям / Read raw acceleration along axes
  int16_t rawAccelerationX, rawAccelerationY, rawAccelerationZ;
  mpuSensor.getAcceleration(&rawAccelerationX, &rawAccelerationY, &rawAccelerationZ);

  accelerationXG = rawAccelerationX / 16384.0f;
  accelerationYG = rawAccelerationY / 16384.0f;
  accelerationZG = rawAccelerationZ / 16384.0f;

  // Вычисляем углы наклона из ускорений / Calculate tilt angles from acceleration
  float rollAngleDegrees  = atan2(accelerationYG, accelerationZG) * 180.0f / PI;
  float pitchAngleDegrees = atan2(-accelerationXG, sqrt(accelerationYG * accelerationYG + accelerationZG * accelerationZG)) * 180.0f / PI;

  // Обрезаем углы для устойчивости / Constrain angles for stability
  float limitedRollDegrees  = constrain(rollAngleDegrees, -30.0f, 30.0f);
  float limitedPitchDegrees = constrain(pitchAngleDegrees, -30.0f, 30.0f);

  currentRollDegrees  = limitedRollDegrees;
  currentPitchDegrees = limitedPitchDegrees;

  // Нормируем углы до диапазона -1..1 / Normalize angles to -1..1
  float normalizedRoll  = constrain(limitedRollDegrees  / 30.0f, -1.0f, 1.0f);
  float normalizedPitch = constrain(limitedPitchDegrees / 30.0f, -1.0f, 1.0f);

  // Фильтруем шум малых ускорений / Filter noise from small accelerations
  float longitudinalAccelerationG = accelerationXG;
  if (fabs(longitudinalAccelerationG) < 0.05f) longitudinalAccelerationG = 0.0f;
  float normalizedLongitudinalAcceleration = constrain(longitudinalAccelerationG / 0.5f, -1.0f, 1.0f);

  float lateralAccelerationG = accelerationYG;
  if (fabs(lateralAccelerationG) < 0.05f) lateralAccelerationG = 0.0f;
  float normalizedLateralAcceleration = constrain(lateralAccelerationG / 0.5f, -1.0f, 1.0f);

  // Весовые коэффициенты смешивания наклона и ускорения / Weights for tilt vs acceleration blending
  float weightPitchFromTilt  = 1.0f;
  float weightPitchFromAccel = 0.7f;

  float weightRollFromTilt   = 1.0f;
  float weightRollFromAccel  = 0.7f;

  // Формируем итоговый сигнал для каждой оси / Compose blended control signals
  float blendedPitch = normalizedPitch * weightPitchFromTilt + normalizedLongitudinalAcceleration * weightPitchFromAccel;
  float blendedRoll  = normalizedRoll  * weightRollFromTilt  + normalizedLateralAcceleration  * weightRollFromAccel;

  blendedPitch = constrain(blendedPitch, -1.0f, 1.0f);
  blendedRoll  = constrain(blendedRoll,  -1.0f, 1.0f);

  // Разносим сигнал по осям шасси / Map blended signals to chassis axes
  float frontPitchComponent = blendedPitch * frontBalanceFactor;
  float rearPitchComponent  = blendedPitch * rearBalanceFactor;
  float rollSideComponent   = blendedRoll;

  // Микшируем команды для каждой сервы / Mix servo commands per corner
  float servoMixFrontLeft  = constrain(frontPitchComponent - rollSideComponent, -1.0f, 1.0f);
  float servoMixFrontRight = constrain(frontPitchComponent + rollSideComponent, -1.0f, 1.0f);
  float servoMixRearLeft   = constrain(-rearPitchComponent - rollSideComponent, -1.0f, 1.0f);
  float servoMixRearRight  = constrain(-rearPitchComponent + rollSideComponent, -1.0f, 1.0f);

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
