#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "telemetry.h"

const char *apSsid = "GyroChassis";
const char *apPass = "12345678";

static WebServer server(80);

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
        <input step="0.1" type="number" id="offset" name="offset" value="%OFFSET%" required>
      </label>
      <label>Доля хода сервы под подвеску <small>(share 0..1)</small>
        <input step="0.01" min="0.01" max="1" type="number" id="share" name="share" value="%SHARE%" required>
      </label>
      <label>Жёсткость перед (kFront)
        <input step="0.1" type="number" id="kFront" name="kFront" value="%KFRONT%" required>
      </label>
      <label>Демпфирование перед (cFront)
        <input step="0.1" type="number" id="cFront" name="cFront" value="%CFRONT%" required>
      </label>
      <label>Жёсткость зад (kRear)
        <input step="0.1" type="number" id="kRear" name="kRear" value="%KREAR%" required>
      </label>
      <label>Демпфирование зад (cRear)
        <input step="0.1" type="number" id="cRear" name="cRear" value="%CREAR%" required>
      </label>
      <label>Баланс перед (frontBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="frontBalance" name="frontBalance" value="%FRONTBAL%" required>
      </label>
      <label>Баланс зад (rearBalance 0..1)
        <input step="0.1" min="0" max="1.5" type="number" id="rearBalance" name="rearBalance" value="%REARBAL%" required>
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

document.getElementById('configForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  saveBtn.disabled = true;
  statusBox.textContent = 'Сохраняю...';
  try {
    const data = new FormData(e.target);
    const body = new URLSearchParams(data);
    const resp = await fetch('/save', { method: 'POST', body });
    statusBox.textContent = await resp.text();
  } catch (err) {
    statusBox.textContent = 'Ошибка сохранения: ' + err;
  }
  saveBtn.disabled = false;
});

const canvas = document.getElementById('orientation');
const ctx = canvas.getContext('2d');

function drawCube(roll, pitch) {
  const w = canvas.width;
  const h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.save();
  ctx.translate(w/2, h/2);
  ctx.rotate(roll * Math.PI / 180);
  ctx.transform(1, 0, Math.tan(pitch * Math.PI / 180) * 0.5, 1, 0, 0);
  ctx.fillStyle = '#1d4ed8';
  ctx.strokeStyle = '#93c5fd';
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.rect(-120, -80, 240, 160);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

async function poll() {
  try {
    const res = await fetch('/imu');
    const j = await res.json();
    document.getElementById('angles').textContent = `Roll: ${j.roll.toFixed(1)}° | Pitch: ${j.pitch.toFixed(1)}° | ax=${j.ax.toFixed(2)}g ay=${j.ay.toFixed(2)}g az=${j.az.toFixed(2)}g`;
    drawCube(j.roll, j.pitch);
  } catch (e) {
    statusBox.textContent = 'Потеря связи, пробую снова...';
  }
}

setInterval(poll, 200);
poll();
</script>
</body>
</html>
)rawliteral";

static String buildPage() {
  String page = FPSTR(INDEX_HTML);
  page.replace("%OFFSET%", String(offset, 2));
  page.replace("%SHARE%", String(share, 2));
  page.replace("%KFRONT%", String(kFront, 2));
  page.replace("%CFRONT%", String(cFront, 2));
  page.replace("%KREAR%", String(kRear, 2));
  page.replace("%CREAR%", String(cRear, 2));
  page.replace("%FRONTBAL%", String(frontBalance, 2));
  page.replace("%REARBAL%", String(rearBalance, 2));
  return page;
}

static void handleRoot() {
  server.send(200, "text/html", buildPage());
}

static void handleSave() {
  if (server.hasArg("offset")) offset = server.arg("offset").toFloat();
  if (server.hasArg("share")) share = constrain(server.arg("share").toFloat(), 0.01f, 1.0f);
  if (server.hasArg("kFront")) kFront = server.arg("kFront").toFloat();
  if (server.hasArg("cFront")) cFront = server.arg("cFront").toFloat();
  if (server.hasArg("kRear"))  kRear  = server.arg("kRear").toFloat();
  if (server.hasArg("cRear"))  cRear  = server.arg("cRear").toFloat();
  if (server.hasArg("frontBalance")) frontBalance = server.arg("frontBalance").toFloat();
  if (server.hasArg("rearBalance"))  rearBalance  = server.arg("rearBalance").toFloat();

  updateSuspensionRange();
  saveConfig();
  server.send(200, "text/plain", "Параметры сохранены");
}

static void handleImu() {
  String json = "{";
  json += "\"roll\":" + String(currentRoll, 3) + ",";
  json += "\"pitch\":" + String(currentPitch, 3) + ",";
  json += "\"ax\":" + String(accX, 3) + ",";
  json += "\"ay\":" + String(accY, 3) + ",";
  json += "\"az\":" + String(accZ, 3);
  json += "}";
  server.send(200, "application/json", json);
}

static void setupWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPass);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("WiFi AP: ");
  Serial.print(apSsid);
  Serial.print(" | IP: ");
  Serial.println(ip);
}

static void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/imu", HTTP_GET, handleImu);
  server.begin();
}

void startWeb() {
  setupWifi();
  setupServer();
}

void handleWeb() {
  server.handleClient();
}
