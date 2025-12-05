#pragma once

// Публичные данные IMU, обновляемые основным циклом и читаемые веб-сервером
// Public IMU data updated by the main loop and read by the web server
extern float currentRollDegrees;
extern float currentPitchDegrees;
extern float accelerationXG;
extern float accelerationYG;
extern float accelerationZG;
extern float gyroXDegreesPerSecond;
extern float gyroYDegreesPerSecond;
extern float gyroZDegreesPerSecond;
