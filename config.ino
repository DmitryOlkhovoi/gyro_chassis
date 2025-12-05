#include <Preferences.h>
#include "config.h"

// Базовые параметры подвески с подробными именами для наглядности
// Base suspension parameters with explicit names for clarity
static const float defaultSuspensionOffsetDegrees      = 90.0f;   // Центральный угол / Center angle
static const float defaultSuspensionTravelShare        = 0.25f;   // Доля хода для подвески / Share of servo travel used for suspension
static const float defaultFrontSpringStiffness         = 5.0f;
static const float defaultFrontDampingCoefficient      = 1.5f;
static const float defaultRearSpringStiffness          = 3.0f;
static const float defaultRearDampingCoefficient       = 1.2f;
static const float defaultFrontBalanceFactor           = 1.0f;
static const float defaultRearBalanceFactor            = 0.8f;
static const float defaultDynamicPitchInfluence        = 0.5f; // kDynPitch
static const float defaultDynamicRollInfluence         = 0.5f; // kDynRoll
static const float defaultDynamicHeaveInfluence        = 0.3f; // kDynHeave
static const float defaultAccelerationFilterAlpha      = 0.1f;

float suspensionOffsetDegrees       = defaultSuspensionOffsetDegrees;
float suspensionTravelShare         = defaultSuspensionTravelShare;
const float totalServoRangeDegrees  = 180.0f;  // Полный ход сервы / Full servo range
float suspensionRangeDegrees        = totalServoRangeDegrees * suspensionTravelShare;
float suspensionHalfRangeDegrees    = suspensionRangeDegrees / 2.0f;

// Коэффициенты для модели пружина-демпфер / Spring-damper model coefficients
float frontSpringStiffness      = defaultFrontSpringStiffness;
float frontDampingCoefficient   = defaultFrontDampingCoefficient;
float rearSpringStiffness       = defaultRearSpringStiffness;
float rearDampingCoefficient    = defaultRearDampingCoefficient;

// Баланс сил по осям / Balance factors for front vs rear influence
float frontBalanceFactor = defaultFrontBalanceFactor;
float rearBalanceFactor  = defaultRearBalanceFactor;

// Динамические коэффициенты и фильтрация ускорений / Dynamic coefficients and acceleration filtering
float dynamicPitchInfluence  = defaultDynamicPitchInfluence; // kDynPitch
float dynamicRollInfluence   = defaultDynamicRollInfluence; // kDynRoll
float dynamicHeaveInfluence  = defaultDynamicHeaveInfluence; // kDynHeave
float accelerationFilterAlpha = defaultAccelerationFilterAlpha;

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

void resetConfigToDefaults() {
  suspensionOffsetDegrees      = defaultSuspensionOffsetDegrees;
  suspensionTravelShare        = defaultSuspensionTravelShare;
  frontSpringStiffness         = defaultFrontSpringStiffness;
  frontDampingCoefficient      = defaultFrontDampingCoefficient;
  rearSpringStiffness          = defaultRearSpringStiffness;
  rearDampingCoefficient       = defaultRearDampingCoefficient;
  frontBalanceFactor           = defaultFrontBalanceFactor;
  rearBalanceFactor            = defaultRearBalanceFactor;
  dynamicPitchInfluence        = defaultDynamicPitchInfluence;
  dynamicRollInfluence         = defaultDynamicRollInfluence;
  dynamicHeaveInfluence        = defaultDynamicHeaveInfluence;
  accelerationFilterAlpha      = defaultAccelerationFilterAlpha;
  updateSuspensionRange();
  saveConfig();
}
