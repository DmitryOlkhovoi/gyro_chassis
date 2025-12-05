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
  </style>
</head>
<body>
  <h1>ESP32 Подвеска</h1>
  <div class="card">
    <form id="configForm">
      <label>Базовая высота <small>(offset, °)</small>
        <input step="0.1" type="number" id="suspensionOffsetDegrees" name="suspensionOffsetDegrees" value="%OFFSET%" required>
      </label>
      <label>Доля хода сервы под подвеску <small>(share 0..1)</small>
        <input step="0.01" min="0.01" max="1" type="number" id="suspensionTravelShare" name="suspensionTravelShare" value="%SHARE%" required>
      </label>
      <label>Жёсткость перед (kFront)
        <input step="0.1" type="number" id="frontSpringStiffness" name="frontSpringStiffness" value="%KFRONT%" required>
      </label>
      <label>Демпфирование перед (cFront)
        <input step="0.1" type="number" id="frontDampingCoefficient" name="frontDampingCoefficient" value="%CFRONT%" required>
      </label>
      <label>Жёсткость зад (kRear)
        <input step="0.1" type="number" id="rearSpringStiffness" name="rearSpringStiffness" value="%KREAR%" required>
      </label>
      <label>Демпфирование зад (cRear)
        <input step="0.1" type="number" id="rearDampingCoefficient" name="rearDampingCoefficient" value="%CREAR%" required>
      </label>
      <label>Баланс перед (frontBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="frontBalance" name="frontBalance" value="%FRONTBAL%" required>
      </label>
      <label>Баланс зад (rearBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="rearBalance" name="rearBalance" value="%REARBAL%" required>
      </label>
      <label>Динамика тангажа (kDynPitch)
        <input step="0.05" type="number" id="dynamicPitchInfluence" name="dynamicPitchInfluence" value="%KDYN_PITCH%" required>
      </label>
      <label>Динамика крена (kDynRoll)
        <input step="0.05" type="number" id="dynamicRollInfluence" name="dynamicRollInfluence" value="%KDYN_ROLL%" required>
      </label>
      <label>Вертикальная динамика (kDynHeave)
        <input step="0.05" type="number" id="dynamicHeaveInfluence" name="dynamicHeaveInfluence" value="%KDYN_HEAVE%" required>
      </label>
      <label>Сглаживание ускорений (alpha 0..1)
        <input step="0.01" min="0" max="1" type="number" id="accelerationFilterAlpha" name="accelerationFilterAlpha" value="%ACC_ALPHA%" required>
      </label>
      <button type="submit" id="saveBtn">Сохранить</button>
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

document.getElementById('configForm').addEventListener('submit', async (formSubmitEvent) => {
  formSubmitEvent.preventDefault();
  saveBtn.disabled = true;
  statusBox.textContent = 'Сохраняю...';
  try {
    const formData = new FormData(formSubmitEvent.target);
    const requestBody = new URLSearchParams(formData);
    const response = await fetch('/save', { method: 'POST', body: requestBody });
    statusBox.textContent = await response.text();
  } catch (error) {
    statusBox.textContent = 'Ошибка сохранения: ' + error;
  }
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
    document.getElementById('angles').textContent = `Roll: ${imuTelemetry.rollDegrees.toFixed(1)}° | Pitch: ${imuTelemetry.pitchDegrees.toFixed(1)}° | ax=${imuTelemetry.accelerationXG.toFixed(2)}g ay=${imuTelemetry.accelerationYG.toFixed(2)}g az=${imuTelemetry.accelerationZG.toFixed(2)}g`;
    drawCube(imuTelemetry.rollDegrees, imuTelemetry.pitchDegrees);
  } catch (error) {
    statusBox.textContent = 'Потеря связи, пробую снова...';
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
  configurationWebServer.send(200, "text/plain", "Параметры сохранены");
}

static void handleImu() {
  String json = "{";
  json += "\"rollDegrees\":" + String(currentRollDegrees, 3) + ",";
  json += "\"pitchDegrees\":" + String(currentPitchDegrees, 3) + ",";
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
