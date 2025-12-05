#pragma once

// Suspension parameters
extern float offset;
extern float share;
extern const float totalRange;
extern float suspRange;
extern float suspHalf;

extern float kFront;
extern float cFront;
extern float kRear;
extern float cRear;

extern float frontBalance;
extern float rearBalance;

void updateSuspensionRange();
void loadConfig();
void saveConfig();
