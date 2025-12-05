#include "telemetry.h"

// Текущие данные инерциального датчика, доступные для веб-интерфейса
// Current IMU telemetry exposed to the web interface
float currentRollDegrees  = 0.0f;
float currentPitchDegrees = 0.0f;
float accelerationXG = 0.0f;
float accelerationYG = 0.0f;
float accelerationZG = 0.0f;
