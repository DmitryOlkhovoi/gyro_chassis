#pragma once

// Suspension parameters (fully expanded names for clarity)
extern float suspensionOffsetDegrees;
extern float suspensionTravelShare;
extern const float totalServoRangeDegrees;
extern float suspensionRangeDegrees;
extern float suspensionHalfRangeDegrees;

extern float frontSpringStiffness;
extern float frontDampingCoefficient;
extern float rearSpringStiffness;
extern float rearDampingCoefficient;

extern float frontBalanceFactor;
extern float rearBalanceFactor;

void updateSuspensionRange();
void loadConfig();
void saveConfig();
