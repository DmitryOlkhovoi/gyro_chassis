#include <Preferences.h>
#include "config.h"

float offset      = 90.0f;
float share       = 0.25f;
const float totalRange  = 180.0f;
float suspRange   = totalRange * share;
float suspHalf    = suspRange / 2.0f;

float kFront = 5.0f;
float cFront = 1.5f;
float kRear  = 3.0f;
float cRear  = 1.2f;

float frontBalance = 1.0f;
float rearBalance  = 0.8f;

static Preferences prefs;

void updateSuspensionRange() {
  suspRange = totalRange * share;
  suspHalf  = suspRange / 2.0f;
}

void loadConfig() {
  prefs.begin("susp", false);
  offset       = prefs.getFloat("offset", offset);
  share        = prefs.getFloat("share", share);
  kFront       = prefs.getFloat("kFront", kFront);
  cFront       = prefs.getFloat("cFront", cFront);
  kRear        = prefs.getFloat("kRear", kRear);
  cRear        = prefs.getFloat("cRear", cRear);
  frontBalance = prefs.getFloat("frontBal", frontBalance);
  rearBalance  = prefs.getFloat("rearBal", rearBalance);
  prefs.end();
  updateSuspensionRange();
}

void saveConfig() {
  prefs.begin("susp", false);
  prefs.putFloat("offset", offset);
  prefs.putFloat("share", share);
  prefs.putFloat("kFront", kFront);
  prefs.putFloat("cFront", cFront);
  prefs.putFloat("kRear", kRear);
  prefs.putFloat("cRear", cRear);
  prefs.putFloat("frontBal", frontBalance);
  prefs.putFloat("rearBal", rearBalance);
  prefs.end();
}
