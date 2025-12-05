#include <Preferences.h>
#include "config.h"

// Базовые параметры подвески с подробными именами для наглядности
// Base suspension parameters with explicit names for clarity
float suspensionOffsetDegrees       = 90.0f;   // Центральный угол / Center angle
float suspensionTravelShare         = 0.25f;   // Доля хода для подвески / Share of servo travel used for suspension
const float totalServoRangeDegrees  = 180.0f;  // Полный ход сервы / Full servo range
float suspensionRangeDegrees        = totalServoRangeDegrees * suspensionTravelShare;
float suspensionHalfRangeDegrees    = suspensionRangeDegrees / 2.0f;

// Коэффициенты для модели пружина-демпфер / Spring-damper model coefficients
float frontSpringStiffness      = 5.0f;
float frontDampingCoefficient   = 1.5f;
float rearSpringStiffness       = 3.0f;
float rearDampingCoefficient    = 1.2f;

// Баланс сил по осям / Balance factors for front vs rear influence
float frontBalanceFactor = 1.0f;
float rearBalanceFactor  = 0.8f;

// Динамические коэффициенты и фильтрация ускорений / Dynamic coefficients and acceleration filtering
float dynamicPitchInfluence  = 0.5f; // kDynPitch
float dynamicRollInfluence   = 0.5f; // kDynRoll
float dynamicHeaveInfluence  = 0.3f; // kDynHeave
float accelerationFilterAlpha = 0.1f;

static Preferences prefs;

void updateSuspensionRange() {
  // Пересчитываем рабочий диапазон после изменения доли хода
  // Recalculate working range after travel share changes
  suspensionRangeDegrees     = totalServoRangeDegrees * suspensionTravelShare;
  suspensionHalfRangeDegrees = suspensionRangeDegrees / 2.0f;
}

void loadConfig() {
  // Читаем сохранённые значения, если они есть / Load persisted values when available
  prefs.begin("susp", false);
  suspensionOffsetDegrees       = prefs.getFloat("offset", suspensionOffsetDegrees);
  suspensionTravelShare        = prefs.getFloat("share", suspensionTravelShare);
  frontSpringStiffness       = prefs.getFloat("kFront", frontSpringStiffness);
  frontDampingCoefficient       = prefs.getFloat("cFront", frontDampingCoefficient);
  rearSpringStiffness        = prefs.getFloat("kRear", rearSpringStiffness);
  rearDampingCoefficient        = prefs.getFloat("cRear", rearDampingCoefficient);
  frontBalanceFactor = prefs.getFloat("frontBal", frontBalanceFactor);
  rearBalanceFactor  = prefs.getFloat("rearBal", rearBalanceFactor);
  dynamicPitchInfluence  = prefs.getFloat("dynPitch", dynamicPitchInfluence);
  dynamicRollInfluence   = prefs.getFloat("dynRoll", dynamicRollInfluence);
  dynamicHeaveInfluence  = prefs.getFloat("dynHeave", dynamicHeaveInfluence);
  accelerationFilterAlpha = prefs.getFloat("accAlpha", accelerationFilterAlpha);
  prefs.end();
  updateSuspensionRange();
}

void saveConfig() {
  // Сохраняем актуальные настройки во flash / Persist current settings to flash
  prefs.begin("susp", false);
  prefs.putFloat("offset", suspensionOffsetDegrees);
  prefs.putFloat("share", suspensionTravelShare);
  prefs.putFloat("kFront", frontSpringStiffness);
  prefs.putFloat("cFront", frontDampingCoefficient);
  prefs.putFloat("kRear", rearSpringStiffness);
  prefs.putFloat("cRear", rearDampingCoefficient);
  prefs.putFloat("frontBal", frontBalanceFactor);
  prefs.putFloat("rearBal", rearBalanceFactor);
  prefs.putFloat("dynPitch", dynamicPitchInfluence);
  prefs.putFloat("dynRoll", dynamicRollInfluence);
  prefs.putFloat("dynHeave", dynamicHeaveInfluence);
  prefs.putFloat("accAlpha", accelerationFilterAlpha);
  prefs.end();
}
