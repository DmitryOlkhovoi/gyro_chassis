#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "telemetry.h"

// Параметры точки доступа, используемой для настройки подвески
// Access point credentials used for suspension configuration
const char *accessPointSsid = "GyroChassis";
const char *accessPointPassword = "12345678";

// HTTP сервер, обслуживающий страницу конфигурации и телеметрию
// HTTP server handling the configuration page and telemetry endpoints
static WebServer configurationWebServer(80);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Настройка подвески</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 16px; background:#0f172a; color:#e2e8f0; }
    h1 { margin-top:0; }
    form { display:grid; gap:12px; max-width:420px; }
    label { display:flex; flex-direction:column; gap:6px; font-weight:600; }
    input { padding:10px; border-radius:8px; border:1px solid #1e293b; background:#1f2937; color:#e2e8f0; }
    button { padding:12px; border:none; border-radius:10px; background:#22c55e; color:#0b1726; font-weight:700; cursor:pointer; }
    button:disabled { opacity:0.6; cursor:not-allowed; }
    .card { background:#111827; border:1px solid #1f2937; border-radius:14px; padding:16px; margin-bottom:18px; box-shadow:0 10px 30px rgba(0,0,0,0.35); }
    #status { min-height:22px; }
    #angles { font-weight:700; }
    canvas { width:100%; max-width:420px; background:#0b1220; border:1px solid #1f2937; border-radius:12px; }
    .actions { display:flex; gap:12px; flex-wrap:wrap; }
    .secondary { background:#0ea5e9; color:#0b1726; }
    .lang-switch { display:flex; gap:8px; margin-bottom:12px; }
    .lang-switch button { background:#1e293b; color:#e2e8f0; border:1px solid #334155; }
    .lang-switch button.active { background:#22c55e; color:#0b1726; border-color:#22c55e; }
  </style>
</head>
<body>
  <div class="lang-switch">
    <button type="button" data-lang="ru" class="active">Русский</button>
    <button type="button" data-lang="en">English</button>
  </div>
  <h1 id="pageTitle">ESP32 Подвеска</h1>
  <div class="card">
    <form id="configForm">
      <label id="labelOffset">Базовая высота <small>(offset, °)</small>
        <input step="0.1" type="number" id="suspensionOffsetDegrees" name="suspensionOffsetDegrees" value="%OFFSET%" required>
      </label>
      <label id="labelShare">Доля хода сервы под подвеску <small>(share 0..1)</small>
        <input step="0.01" min="0.01" max="1" type="number" id="suspensionTravelShare" name="suspensionTravelShare" value="%SHARE%" required>
      </label>
      <label id="labelKFront">Жёсткость перед (kFront)
        <input step="0.1" type="number" id="frontSpringStiffness" name="frontSpringStiffness" value="%KFRONT%" required>
      </label>
      <label id="labelCFront">Демпфирование перед (cFront)
        <input step="0.1" type="number" id="frontDampingCoefficient" name="frontDampingCoefficient" value="%CFRONT%" required>
      </label>
      <label id="labelKRear">Жёсткость зад (kRear)
        <input step="0.1" type="number" id="rearSpringStiffness" name="rearSpringStiffness" value="%KREAR%" required>
      </label>
      <label id="labelCRear">Демпфирование зад (cRear)
        <input step="0.1" type="number" id="rearDampingCoefficient" name="rearDampingCoefficient" value="%CREAR%" required>
      </label>
      <label id="labelFrontBalance">Баланс перед (frontBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="frontBalance" name="frontBalance" value="%FRONTBAL%" required>
      </label>
      <label id="labelRearBalance">Баланс зад (rearBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="rearBalance" name="rearBalance" value="%REARBAL%" required>
      </label>
      <label id="labelDynPitch">Динамика тангажа (kDynPitch)
        <input step="0.05" type="number" id="dynamicPitchInfluence" name="dynamicPitchInfluence" value="%KDYN_PITCH%" required>
      </label>
      <label id="labelDynRoll">Динамика крена (kDynRoll)
        <input step="0.05" type="number" id="dynamicRollInfluence" name="dynamicRollInfluence" value="%KDYN_ROLL%" required>
      </label>
      <label id="labelDynHeave">Вертикальная динамика (kDynHeave)
        <input step="0.05" type="number" id="dynamicHeaveInfluence" name="dynamicHeaveInfluence" value="%KDYN_HEAVE%" required>
      </label>
      <label id="labelAccAlpha">Сглаживание ускорений (alpha 0..1)
        <input step="0.01" min="0" max="1" type="number" id="accelerationFilterAlpha" name="accelerationFilterAlpha" value="%ACC_ALPHA%" required>
      </label>
      <div class="actions">
        <button type="submit" id="saveBtn">Сохранить</button>
        <button type="button" id="resetBtn" class="secondary">Сбросить в дефолт</button>
      </div>
      <div id="status"></div>
    </form>
  </div>
  <div class="card">
    <div id="angles">Roll: 0° | Pitch: 0°</div>
    <canvas id="orientation" width="420" height="320"></canvas>
  </div>
<script>
const statusBox = document.getElementById('status');
const saveBtn = document.getElementById('saveBtn');
const resetBtn = document.getElementById('resetBtn');
const langButtons = document.querySelectorAll('.lang-switch button');
const pageTitle = document.getElementById('pageTitle');

const translations = {
  ru: {
    title: 'ESP32 Подвеска',
    labels: {
      offset: 'Базовая высота <small>(offset, °)</small>',
      share: 'Доля хода сервы под подвеску <small>(share 0..1)</small>',
      kFront: 'Жёсткость перед (kFront)',
      cFront: 'Демпфирование перед (cFront)',
      kRear: 'Жёсткость зад (kRear)',
      cRear: 'Демпфирование зад (cRear)',
      frontBalance: 'Баланс перед (frontBalance 0..1)',
      rearBalance: 'Баланс зад (rearBalance 0..1)',
      dynPitch: 'Динамика тангажа (kDynPitch)',
      dynRoll: 'Динамика крена (kDynRoll)',
      dynHeave: 'Вертикальная динамика (kDynHeave)',
      accAlpha: 'Сглаживание ускорений (alpha 0..1)'
    },
    buttons: { save: 'Сохранить', reset: 'Сбросить в дефолт' },
    status: {
      saving: 'Сохраняю...',
      saved: 'Параметры сохранены',
      reset: 'Возвращаю к базовым значениям...',
      resetDone: 'Дефолтные значения применены',
      errorPrefix: 'Ошибка сохранения: ',
      lostConnection: 'Потеря связи, пробую снова...'
    },
    angles: (roll, pitch, gx, gy, gz, ax, ay, az) => `Roll: ${roll}° | Pitch: ${pitch}° | gyro: x=${gx}°/s y=${gy}°/s z=${gz}°/s | ax=${ax}g ay=${ay}g az=${az}g`
  },
  en: {
    title: 'ESP32 Suspension',
    labels: {
      offset: 'Base angle <small>(offset, °)</small>',
      share: 'Servo travel share <small>(share 0..1)</small>',
      kFront: 'Front stiffness (kFront)',
      cFront: 'Front damping (cFront)',
      kRear: 'Rear stiffness (kRear)',
      cRear: 'Rear damping (cRear)',
      frontBalance: 'Front balance (frontBalance 0..1)',
      rearBalance: 'Rear balance (rearBalance 0..1)',
      dynPitch: 'Pitch dynamics (kDynPitch)',
      dynRoll: 'Roll dynamics (kDynRoll)',
      dynHeave: 'Heave dynamics (kDynHeave)',
      accAlpha: 'Acceleration smoothing (alpha 0..1)'
    },
    buttons: { save: 'Save', reset: 'Reset to defaults' },
    status: {
      saving: 'Saving...',
      saved: 'Settings saved',
      reset: 'Restoring defaults...',
      resetDone: 'Default values applied',
      errorPrefix: 'Save error: ',
      lostConnection: 'Connection lost, retrying...'
    },
    angles: (roll, pitch, gx, gy, gz, ax, ay, az) => `Roll: ${roll}° | Pitch: ${pitch}° | gyro: x=${gx}°/s y=${gy}°/s z=${gz}°/s | ax=${ax}g ay=${ay}g az=${az}g`
  }
};

let currentLanguage = 'ru';

function setLanguage(lang) {
  currentLanguage = lang;
  document.documentElement.lang = lang;
  const t = translations[lang];
  pageTitle.textContent = t.title;
  document.getElementById('labelOffset').innerHTML = t.labels.offset;
  document.getElementById('labelShare').innerHTML = t.labels.share;
  document.getElementById('labelKFront').innerHTML = t.labels.kFront;
  document.getElementById('labelCFront').innerHTML = t.labels.cFront;
  document.getElementById('labelKRear').innerHTML = t.labels.kRear;
  document.getElementById('labelCRear').innerHTML = t.labels.cRear;
  document.getElementById('labelFrontBalance').innerHTML = t.labels.frontBalance;
  document.getElementById('labelRearBalance').innerHTML = t.labels.rearBalance;
  document.getElementById('labelDynPitch').innerHTML = t.labels.dynPitch;
  document.getElementById('labelDynRoll').innerHTML = t.labels.dynRoll;
  document.getElementById('labelDynHeave').innerHTML = t.labels.dynHeave;
  document.getElementById('labelAccAlpha').innerHTML = t.labels.accAlpha;
  saveBtn.textContent = t.buttons.save;
  resetBtn.textContent = t.buttons.reset;
  langButtons.forEach((btn) => {
    btn.classList.toggle('active', btn.dataset.lang === lang);
  });
}

langButtons.forEach((btn) => {
  btn.addEventListener('click', () => setLanguage(btn.dataset.lang));
});

setLanguage(currentLanguage);

function applyConfigValues(values) {
  Object.entries(values).forEach(([id, value]) => {
    const input = document.getElementById(id);
    if (input) {
      input.value = value;
    }
  });
}

document.getElementById('configForm').addEventListener('submit', async (formSubmitEvent) => {
  formSubmitEvent.preventDefault();
  saveBtn.disabled = true;
  statusBox.textContent = translations[currentLanguage].status.saving;
  try {
    const formData = new FormData(formSubmitEvent.target);
    const requestBody = new URLSearchParams(formData);
    const response = await fetch('/save', { method: 'POST', body: requestBody });
    if (!response.ok) {
      throw new Error(await response.text());
    }
    statusBox.textContent = translations[currentLanguage].status.saved;
  } catch (error) {
    statusBox.textContent = translations[currentLanguage].status.errorPrefix + error;
  }
  saveBtn.disabled = false;
});

resetBtn.addEventListener('click', async () => {
  resetBtn.disabled = true;
  saveBtn.disabled = true;
  statusBox.textContent = translations[currentLanguage].status.reset;
  try {
    const response = await fetch('/reset', { method: 'POST' });
    if (!response.ok) {
      throw new Error(await response.text());
    }
    const defaultValues = await response.json();
    applyConfigValues(defaultValues);
    statusBox.textContent = translations[currentLanguage].status.resetDone;
  } catch (error) {
    statusBox.textContent = translations[currentLanguage].status.errorPrefix + error;
  }
  resetBtn.disabled = false;
  saveBtn.disabled = false;
});

const canvas = document.getElementById('orientation');
const canvasContext = canvas.getContext('2d');

function drawCube(rollDegrees, pitchDegrees) {
  const canvasWidthPixels = canvas.width;
  const canvasHeightPixels = canvas.height;
  canvasContext.clearRect(0, 0, canvasWidthPixels, canvasHeightPixels);
  canvasContext.save();
  canvasContext.translate(canvasWidthPixels/2, canvasHeightPixels/2);
  canvasContext.rotate(rollDegrees * Math.PI / 180);
  canvasContext.transform(1, 0, Math.tan(pitchDegrees * Math.PI / 180) * 0.5, 1, 0, 0);
  canvasContext.fillStyle = '#1d4ed8';
  canvasContext.strokeStyle = '#93c5fd';
  canvasContext.lineWidth = 3;
  canvasContext.beginPath();
  canvasContext.rect(-120, -80, 240, 160);
  canvasContext.fill();
  canvasContext.stroke();
  canvasContext.restore();
}

async function poll() {
  try {
    const response = await fetch('/imu');
    const imuTelemetry = await response.json();
    document.getElementById('angles').textContent = translations[currentLanguage].angles(
      imuTelemetry.rollDegrees.toFixed(1),
      imuTelemetry.pitchDegrees.toFixed(1),
      imuTelemetry.gyroXDegreesPerSecond.toFixed(1),
      imuTelemetry.gyroYDegreesPerSecond.toFixed(1),
      imuTelemetry.gyroZDegreesPerSecond.toFixed(1),
      imuTelemetry.accelerationXG.toFixed(2),
      imuTelemetry.accelerationYG.toFixed(2),
      imuTelemetry.accelerationZG.toFixed(2)
    );
    drawCube(imuTelemetry.rollDegrees, imuTelemetry.pitchDegrees);
  } catch (error) {
    statusBox.textContent = translations[currentLanguage].status.lostConnection;
  }
}

setInterval(poll, 200);
poll();
</script>
</body>
</html>
)rawliteral";

// Строим HTML страницу с подстановкой текущих значений
// Build the HTML page while injecting current parameter values
static String buildPage() {
  String page = FPSTR(INDEX_HTML);
  page.replace("%OFFSET%", String(suspensionOffsetDegrees, 2));
  page.replace("%SHARE%", String(suspensionTravelShare, 2));
  page.replace("%KFRONT%", String(frontSpringStiffness, 2));
  page.replace("%CFRONT%", String(frontDampingCoefficient, 2));
  page.replace("%KREAR%", String(rearSpringStiffness, 2));
  page.replace("%CREAR%", String(rearDampingCoefficient, 2));
  page.replace("%FRONTBAL%", String(frontBalanceFactor, 2));
  page.replace("%REARBAL%", String(rearBalanceFactor, 2));
  page.replace("%KDYN_PITCH%", String(dynamicPitchInfluence, 2));
  page.replace("%KDYN_ROLL%", String(dynamicRollInfluence, 2));
  page.replace("%KDYN_HEAVE%", String(dynamicHeaveInfluence, 2));
  page.replace("%ACC_ALPHA%", String(accelerationFilterAlpha, 2));
  return page;
}

// Обработчик главной страницы / Root page handler
static void handleRoot() {
  configurationWebServer.send(200, "text/html", buildPage());
}

// Приём и сохранение новых настроек из формы
// Receive and persist new settings from the form
static void handleSave() {
  if (configurationWebServer.hasArg("suspensionOffsetDegrees")) {
    suspensionOffsetDegrees = configurationWebServer.arg("suspensionOffsetDegrees").toFloat();
  }
  if (configurationWebServer.hasArg("suspensionTravelShare")) {
    suspensionTravelShare = constrain(configurationWebServer.arg("suspensionTravelShare").toFloat(), 0.01f, 1.0f);
  }
  if (configurationWebServer.hasArg("frontSpringStiffness")) {
    frontSpringStiffness = configurationWebServer.arg("frontSpringStiffness").toFloat();
  }
  if (configurationWebServer.hasArg("frontDampingCoefficient")) {
    frontDampingCoefficient = configurationWebServer.arg("frontDampingCoefficient").toFloat();
  }
  if (configurationWebServer.hasArg("rearSpringStiffness"))  {
    rearSpringStiffness  = configurationWebServer.arg("rearSpringStiffness").toFloat();
  }
  if (configurationWebServer.hasArg("rearDampingCoefficient"))  {
    rearDampingCoefficient  = configurationWebServer.arg("rearDampingCoefficient").toFloat();
  }
  if (configurationWebServer.hasArg("frontBalance")) {
    frontBalanceFactor = configurationWebServer.arg("frontBalance").toFloat();
  }
  if (configurationWebServer.hasArg("rearBalance"))  {
    rearBalanceFactor  = configurationWebServer.arg("rearBalance").toFloat();
  }
  if (configurationWebServer.hasArg("dynamicPitchInfluence")) {
    dynamicPitchInfluence = configurationWebServer.arg("dynamicPitchInfluence").toFloat();
  }
  if (configurationWebServer.hasArg("dynamicRollInfluence")) {
    dynamicRollInfluence = configurationWebServer.arg("dynamicRollInfluence").toFloat();
  }
  if (configurationWebServer.hasArg("dynamicHeaveInfluence")) {
    dynamicHeaveInfluence = configurationWebServer.arg("dynamicHeaveInfluence").toFloat();
  }
  if (configurationWebServer.hasArg("accelerationFilterAlpha")) {
    accelerationFilterAlpha = constrain(configurationWebServer.arg("accelerationFilterAlpha").toFloat(), 0.0f, 1.0f);
  }

  updateSuspensionRange();
  saveConfig();
  configurationWebServer.send(200, "text/plain", "ok");
}

static String buildConfigJson() {
  String json = "{";
  json += "\"suspensionOffsetDegrees\":" + String(suspensionOffsetDegrees, 2) + ",";
  json += "\"suspensionTravelShare\":" + String(suspensionTravelShare, 2) + ",";
  json += "\"frontSpringStiffness\":" + String(frontSpringStiffness, 2) + ",";
  json += "\"frontDampingCoefficient\":" + String(frontDampingCoefficient, 2) + ",";
  json += "\"rearSpringStiffness\":" + String(rearSpringStiffness, 2) + ",";
  json += "\"rearDampingCoefficient\":" + String(rearDampingCoefficient, 2) + ",";
  json += "\"frontBalance\":" + String(frontBalanceFactor, 2) + ",";
  json += "\"rearBalance\":" + String(rearBalanceFactor, 2) + ",";
  json += "\"dynamicPitchInfluence\":" + String(dynamicPitchInfluence, 2) + ",";
  json += "\"dynamicRollInfluence\":" + String(dynamicRollInfluence, 2) + ",";
  json += "\"dynamicHeaveInfluence\":" + String(dynamicHeaveInfluence, 2) + ",";
  json += "\"accelerationFilterAlpha\":" + String(accelerationFilterAlpha, 2);
  json += "}";
  return json;
}

static void handleReset() {
  resetConfigToDefaults();
  configurationWebServer.send(200, "application/json", buildConfigJson());
}

static void handleImu() {
  String json = "{";
  json += "\"rollDegrees\":" + String(currentRollDegrees, 3) + ",";
  json += "\"pitchDegrees\":" + String(currentPitchDegrees, 3) + ",";
  json += "\"gyroXDegreesPerSecond\":" + String(gyroXDegreesPerSecond, 3) + ",";
  json += "\"gyroYDegreesPerSecond\":" + String(gyroYDegreesPerSecond, 3) + ",";
  json += "\"gyroZDegreesPerSecond\":" + String(gyroZDegreesPerSecond, 3) + ",";
  json += "\"accelerationXG\":" + String(accelerationXG, 3) + ",";
  json += "\"accelerationYG\":" + String(accelerationYG, 3) + ",";
  json += "\"accelerationZG\":" + String(accelerationZG, 3);
  json += "}";
  configurationWebServer.send(200, "application/json", json);
}

static void setupWifi() {
  // Поднимаем точку доступа для локального доступа
  // Start access point for local configuration access
  WiFi.mode(WIFI_AP);
  WiFi.softAP(accessPointSsid, accessPointPassword);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("WiFi AP: ");
  Serial.print(accessPointSsid);
  Serial.print(" | IP: ");
  Serial.println(ip);
}

static void setupServer() {
  // Регистрируем маршруты веб-сервера
  // Register web server routes
  configurationWebServer.on("/", HTTP_GET, handleRoot);
  configurationWebServer.on("/save", HTTP_POST, handleSave);
  configurationWebServer.on("/reset", HTTP_POST, handleReset);
  configurationWebServer.on("/imu", HTTP_GET, handleImu);
  configurationWebServer.begin();
}

void startWeb() {
  setupWifi();
  setupServer();
}

void handleWeb() {
  configurationWebServer.handleClient();
}
