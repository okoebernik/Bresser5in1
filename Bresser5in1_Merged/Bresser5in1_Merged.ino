/*
  Heltec WiFi LoRa 32 (V2) + Bresser 5-in-1 Receiver

  Basis:
  - funktionierender Empfang aus sketch_jun20a.ino
  - modernes Webinterface und JSON-API aus der modularen Version

  Web:
  - /       Live dashboard
  - /config MQTT configuration
  - /save   Save configuration
  - /data   JSON data endpoint
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <SSD1306Wire.h>
#include <math.h>

#ifndef MAX_SENSORS_DEFAULT
#define MAX_SENSORS_DEFAULT 5
#endif
#include <WeatherSensor.h>

Preferences prefs;
WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
WeatherSensor ws;

SSD1306Wire *display = nullptr;
uint8_t oledAddr = 0x3C;

struct AppCfg {
  bool mqttEnabled = false;
  bool sensorFilterEnabled = false;
  uint32_t selectedSensorId = 0;
  char mqttHost[64]  = "mqtt.local";
  uint16_t mqttPort  = 1883;
  char mqttUser[32]  = "";
  char mqttPass[32]  = "";
  char baseTopic[64] = "bresser";
} cfg;

WeatherSensor::Sensor lastData{};
bool hasData = false;
unsigned long lastUpdate = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastWeatherPoll = 0;
const unsigned long WEATHER_POLL_INTERVAL_MS = 100;
const uint16_t MQTT_PACKET_BUFFER_SIZE = 1024;

struct SeenSensor {
  uint32_t id = 0;
  float rssi = 0.0f;
  unsigned long lastSeen = 0;
};

SeenSensor seenSensors[8];

struct RainSample {
  unsigned long timestamp = 0;
  float rainMm = 0.0f;
};

const unsigned long RAIN_HISTORY_INTERVAL_MS = 5UL * 60UL * 1000UL;
const unsigned long RAIN_HOUR_MS = 60UL * 60UL * 1000UL;
const unsigned long RAIN_DAY_MS = 24UL * RAIN_HOUR_MS;
const unsigned long RAIN_WEEK_MS = 7UL * RAIN_DAY_MS;
const uint16_t RAIN_HISTORY_SLOTS = 2100;

RainSample rainHistory[RAIN_HISTORY_SLOTS];
uint16_t rainHistoryHead = 0;
bool rainHistoryFilled = false;
unsigned long lastRainHistoryStore = 0;
float lastRainHistoryValue = NAN;

static inline float getWindAvgMS(const WeatherSensor::Sensor &d) {
#ifdef WIND_DATA_FLOATINGPOINT
  return d.w.wind_avg_meter_sec;
#elif defined(WIND_DATA_FIXEDPOINT)
  return d.w.wind_avg_meter_sec_fp1 / 10.0f;
#else
  return 0.0f;
#endif
}

static inline float getWindGustMS(const WeatherSensor::Sensor &d) {
#ifdef WIND_DATA_FLOATINGPOINT
  return d.w.wind_gust_meter_sec;
#elif defined(WIND_DATA_FIXEDPOINT)
  return d.w.wind_gust_meter_sec_fp1 / 10.0f;
#else
  return 0.0f;
#endif
}

static inline float getWindDirDeg(const WeatherSensor::Sensor &d) {
#ifdef WIND_DATA_FLOATINGPOINT
  return d.w.wind_direction_deg;
#elif defined(WIND_DATA_FIXEDPOINT)
  return d.w.wind_direction_deg_fp1 / 10.0f;
#else
  return 0.0f;
#endif
}

const char *windDirText(float deg) {
  static const char *dirs[] = {"N","NNO","NO","ONO","O","OSO","SO","SSO",
                               "S","SSW","SW","WSW","W","WNW","NW","NNW"};
  int idx = ((int)(deg + 11.25f) / 22) % 16;
  return dirs[idx];
}

float calculateDewPointC(float tempC, uint8_t humidity) {
  float rh = constrain((float)humidity, 1.0f, 100.0f);
  const float a = 17.62f;
  const float b = 243.12f;
  float gamma = log(rh / 100.0f) + (a * tempC) / (b + tempC);
  return (b * gamma) / (a - gamma);
}

void updateRainHistory(float rainMm) {
  unsigned long now = millis();
  bool firstSample = !rainHistoryFilled && rainHistoryHead == 0;
  bool rainChanged = isnan(lastRainHistoryValue) || fabs(rainMm - lastRainHistoryValue) >= 0.05f;
  bool intervalDue = now - lastRainHistoryStore >= RAIN_HISTORY_INTERVAL_MS;

  if (!firstSample && !rainChanged && !intervalDue) return;

  rainHistory[rainHistoryHead].timestamp = now;
  rainHistory[rainHistoryHead].rainMm = rainMm;
  rainHistoryHead = (rainHistoryHead + 1) % RAIN_HISTORY_SLOTS;
  if (rainHistoryHead == 0) rainHistoryFilled = true;
  lastRainHistoryStore = now;
  lastRainHistoryValue = rainMm;
}

float rainDeltaForWindow(unsigned long windowMs) {
  if (!hasData) return 0.0f;

  uint16_t count = rainHistoryFilled ? RAIN_HISTORY_SLOTS : rainHistoryHead;
  if (count == 0) return 0.0f;

  unsigned long now = millis();
  int bestSlot = -1;
  int oldestSlot = -1;
  unsigned long bestAge = ULONG_MAX;
  unsigned long oldestAge = 0;

  for (uint16_t i = 0; i < count; i++) {
    if (rainHistory[i].timestamp == 0) continue;
    unsigned long age = now - rainHistory[i].timestamp;
    if (age >= windowMs && age < bestAge) {
      bestAge = age;
      bestSlot = i;
    }
    if (oldestSlot < 0 || age > oldestAge) {
      oldestAge = age;
      oldestSlot = i;
    }
  }

  int baseSlot = bestSlot >= 0 ? bestSlot : oldestSlot;
  if (baseSlot < 0) return 0.0f;

  float delta = lastData.w.rain_mm - rainHistory[baseSlot].rainMm;
  if (delta < 0.0f) return lastData.w.rain_mm;
  return delta;
}

void copyToBuffer(char *dst, size_t len, const String &src) {
  src.toCharArray(dst, len);
  dst[len - 1] = '\0';
}

uint32_t parseSensorId(String value) {
  value.trim();
  value.replace("0x", "");
  value.replace("0X", "");
  if (value.length() == 0) return 0;
  return strtoul(value.c_str(), nullptr, 16);
}

String sensorIdHex(uint32_t id) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%08lX", (unsigned long)id);
  return String(buf);
}

bool sensorAllowed(uint32_t id) {
  return !cfg.sensorFilterEnabled || cfg.selectedSensorId == 0 || id == cfg.selectedSensorId;
}

void resetSelectedSensorData() {
  hasData = false;
  lastData = WeatherSensor::Sensor();
  memset(rainHistory, 0, sizeof(rainHistory));
  rainHistoryHead = 0;
  rainHistoryFilled = false;
  lastRainHistoryStore = 0;
  lastRainHistoryValue = NAN;
}

void applySensorFilterToReceiver() {
  uint8_t emptyList[] = { 0, 0, 0, 0 };
  ws.setSensorsExc(emptyList, sizeof(emptyList));

  if (cfg.sensorFilterEnabled && cfg.selectedSensorId != 0) {
    uint8_t includeList[] = {
      (uint8_t)(cfg.selectedSensorId >> 24),
      (uint8_t)(cfg.selectedSensorId >> 16),
      (uint8_t)(cfg.selectedSensorId >> 8),
      (uint8_t)(cfg.selectedSensorId)
    };
    ws.setSensorsInc(includeList, sizeof(includeList));
    Serial.printf("Sensor-Filter aktiv: %08lX\n", (unsigned long)cfg.selectedSensorId);
  } else {
    ws.setSensorsInc(emptyList, sizeof(emptyList));
    Serial.println("Sensor-Filter aus: alle Sensoren erlaubt");
  }

  ws.clearSlots();
}

void rememberSeenSensor(const WeatherSensor::Sensor &d) {
  int freeSlot = -1;
  int oldestSlot = 0;
  unsigned long oldestSeen = seenSensors[0].lastSeen;

  for (size_t i = 0; i < 8; i++) {
    if (seenSensors[i].id == d.sensor_id) {
      seenSensors[i].rssi = d.rssi;
      seenSensors[i].lastSeen = millis();
      return;
    }
    if (seenSensors[i].id == 0 && freeSlot < 0) {
      freeSlot = i;
    }
    if (seenSensors[i].lastSeen < oldestSeen) {
      oldestSeen = seenSensors[i].lastSeen;
      oldestSlot = i;
    }
  }

  int slot = freeSlot >= 0 ? freeSlot : oldestSlot;
  seenSensors[slot].id = d.sensor_id;
  seenSensors[slot].rssi = d.rssi;
  seenSensors[slot].lastSeen = millis();
}

void loadPrefs() {
  prefs.begin("bresser", true);
  cfg.mqttEnabled = prefs.getBool("mqttEnabled", cfg.mqttEnabled);
  cfg.sensorFilterEnabled = prefs.getBool("sensorFilter", cfg.sensorFilterEnabled);
  cfg.selectedSensorId = prefs.getUInt("sensorId", cfg.selectedSensorId);
  copyToBuffer(cfg.mqttHost, sizeof(cfg.mqttHost), prefs.getString("host", cfg.mqttHost));
  cfg.mqttPort = prefs.getUShort("port", cfg.mqttPort);
  copyToBuffer(cfg.mqttUser, sizeof(cfg.mqttUser), prefs.getString("user", cfg.mqttUser));
  copyToBuffer(cfg.mqttPass, sizeof(cfg.mqttPass), prefs.getString("pass", cfg.mqttPass));
  copyToBuffer(cfg.baseTopic, sizeof(cfg.baseTopic), prefs.getString("topic", cfg.baseTopic));
  prefs.end();
}

void savePrefs() {
  prefs.begin("bresser", false);
  prefs.putBool("mqttEnabled", cfg.mqttEnabled);
  prefs.putBool("sensorFilter", cfg.sensorFilterEnabled);
  prefs.putUInt("sensorId", cfg.selectedSensorId);
  prefs.putString("host", cfg.mqttHost);
  prefs.putUShort("port", cfg.mqttPort);
  prefs.putString("user", cfg.mqttUser);
  prefs.putString("pass", cfg.mqttPass);
  prefs.putString("topic", cfg.baseTopic);
  prefs.end();
}

String mqttStatusText() {
  if (!cfg.mqttEnabled) return "disabled";
  if (mqtt.connected()) return "connected";
  return String("disconnected (") + mqtt.state() + ")";
}

void configureMqttClient() {
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  if (!mqtt.setBufferSize(MQTT_PACKET_BUFFER_SIZE)) {
    Serial.printf("MQTT buffer resize failed (%u bytes)\n", MQTT_PACKET_BUFFER_SIZE);
  }
}

void mqttReconnect() {
  if (!cfg.mqttEnabled || !WiFi.isConnected() || mqtt.connected()) return;
  if (millis() - lastMqttAttempt < 5000) return;
  lastMqttAttempt = millis();

  configureMqttClient();
  String clientId = String("bresser-heltec-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok = false;
  if (strlen(cfg.mqttUser) > 0) {
    ok = mqtt.connect(clientId.c_str(), cfg.mqttUser, cfg.mqttPass);
  } else {
    ok = mqtt.connect(clientId.c_str());
  }
  Serial.printf("MQTT %s (%s:%u)\n", ok ? "connected" : "failed", cfg.mqttHost, cfg.mqttPort);
}

void mqttPublishWeather(const WeatherSensor::Sensor &d) {
  if (!cfg.mqttEnabled || !mqtt.connected()) return;

  StaticJsonDocument<896> doc;
  doc["id"]            = d.sensor_id;
  doc["id_hex"]        = String(d.sensor_id, HEX);
  doc["temp_c"]        = d.w.temp_c;
  doc["humidity"]      = d.w.humidity;
  doc["dew_point_c"]   = calculateDewPointC(d.w.temp_c, d.w.humidity);
  doc["wind_speed_ms"] = getWindAvgMS(d);
  doc["wind_gust_ms"]  = getWindGustMS(d);
  doc["wind_dir_deg"]  = getWindDirDeg(d);
  doc["wind_dir_text"] = windDirText(getWindDirDeg(d));
  doc["rain_mm"]       = d.w.rain_mm;
  doc["rain_hour_mm"]  = rainDeltaForWindow(RAIN_HOUR_MS);
  doc["rain_day_mm"]   = rainDeltaForWindow(RAIN_DAY_MS);
  doc["rain_week_mm"]  = rainDeltaForWindow(RAIN_WEEK_MS);
  doc["battery_ok"]    = d.battery_ok;
  doc["rssi"]          = d.rssi;
  doc["uptime_s"]      = millis() / 1000;

  char payload[896];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0 || n >= sizeof(payload)) {
    Serial.printf("MQTT publish skipped: payload too large (%u/%u bytes)\n",
                  (unsigned int)n,
                  (unsigned int)sizeof(payload));
    return;
  }

  char topic[128];
  snprintf(topic, sizeof(topic), "%s/data/%08lX", cfg.baseTopic, (unsigned long)d.sensor_id);
  bool ok = mqtt.publish(topic, (const uint8_t *)payload, n);
  if (!ok) {
    Serial.printf("MQTT publish failed: topic=%s payload=%u buffer=%u state=%d\n",
                  topic,
                  (unsigned int)n,
                  MQTT_PACKET_BUFFER_SIZE,
                  mqtt.state());
  }
}

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bresser Receiver</title>
<style>
:root{color-scheme:dark;--bg:#101113;--panel:#191b1f;--panel2:#20242a;--line:#303741;--text:#f2f5f8;--muted:#9aa7b4;--blue:#5aa9ff;--green:#42d392;--amber:#f2b84b;--red:#ff6b6b}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Segoe UI,Arial,sans-serif}
header{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:18px 22px;border-bottom:1px solid var(--line);background:#15171a}
h1{font-size:20px;margin:0;font-weight:700;letter-spacing:0}.nav{display:flex;gap:10px;align-items:center}.nav a{color:var(--text);text-decoration:none;border:1px solid var(--line);padding:8px 12px;border-radius:8px;background:var(--panel2)}
main{max-width:1080px;margin:0 auto;padding:22px}.status{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;margin-bottom:16px}.pill{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:10px 12px;color:var(--muted)}.pill b{display:block;color:var(--text);font-size:14px;margin-top:3px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:16px;min-height:112px}.label{color:var(--muted);font-size:12px;text-transform:uppercase}.value{font-size:34px;line-height:1.1;font-weight:750;margin-top:12px}.unit{font-size:15px;color:var(--muted);font-weight:500}.sub{color:var(--muted);margin-top:8px;font-size:14px}
.temp{border-top:3px solid var(--amber)}.hum{border-top:3px solid var(--blue)}.wind{border-top:3px solid var(--green)}.rain{border-top:3px solid #8ea0ff}.bad{color:var(--red)}.ok{color:var(--green)}
.wide{grid-column:1/-1;display:grid;grid-template-columns:minmax(180px,1fr) 132px;align-items:center;gap:16px}.windrose{width:124px;height:124px;display:block}.rose-ring{fill:#14171b;stroke:var(--line);stroke-width:1.5}.rose-axis{stroke:#45505c;stroke-width:1}.rose-tick{stroke:#66717f;stroke-width:1.4;stroke-linecap:round}.rose-label{fill:var(--muted);font-size:9px;font-weight:800;text-anchor:middle;dominant-baseline:middle}.rose-label.main{fill:var(--text);font-size:11px}.rose-arrow{fill:var(--red);stroke:#ffd0d0;stroke-width:.7}.rose-center{fill:#f2f5f8;stroke:#14171b;stroke-width:1}.footer{margin-top:16px;color:var(--muted);font-size:13px}
@media(max-width:560px){header{align-items:flex-start;flex-direction:column}.wide{grid-template-columns:1fr}.value{font-size:30px}}
</style>
</head>
<body>
<header><h1>Bresser 5-in-1 Receiver</h1><div class="nav"><a href="/">Dashboard</a><a href="/config">Config</a></div></header>
<main>
  <section class="status">
    <div class="pill">Sensor<b id="sensor">--</b></div>
    <div class="pill">WiFi<b id="wifi">--</b></div>
    <div class="pill">MQTT<b id="mqtt">--</b></div>
    <div class="pill">Letztes Update<b id="age">--</b></div>
  </section>
  <section class="grid">
    <div class="card temp"><div class="label">Temperatur</div><div class="value"><span id="temp">--.-</span><span class="unit"> C</span></div></div>
    <div class="card hum"><div class="label">Luftfeuchte</div><div class="value"><span id="hum">--</span><span class="unit"> %</span></div></div>
    <div class="card"><div class="label">Taupunkt</div><div class="value"><span id="dew">--.-</span><span class="unit"> C</span></div></div>
    <div class="card wind"><div class="label">Wind</div><div class="value"><span id="wind">--.-</span><span class="unit"> m/s</span></div><div class="sub" id="windKmh">-- km/h</div></div>
    <div class="card wind"><div class="label">Boe</div><div class="value"><span id="gust">--.-</span><span class="unit"> m/s</span></div></div>
    <div class="card rain"><div class="label">Regen gesamt</div><div class="value"><span id="rain">--.-</span><span class="unit"> mm</span></div></div>
    <div class="card rain"><div class="label">Regen 1h</div><div class="value"><span id="rainHour">--.-</span><span class="unit"> mm</span></div></div>
    <div class="card rain"><div class="label">Regen 24h</div><div class="value"><span id="rainDay">--.-</span><span class="unit"> mm</span></div></div>
    <div class="card rain"><div class="label">Regen 7d</div><div class="value"><span id="rainWeek">--.-</span><span class="unit"> mm</span></div></div>
    <div class="card"><div class="label">Batterie</div><div class="value" id="battery">--</div><div class="sub" id="rssi">RSSI -- dBm</div></div>
    <div class="card wide"><div><div class="label">Windrichtung</div><div class="value"><span id="dirText">---</span> <span class="unit" id="dirDeg">-- deg</span></div></div><svg class="windrose" viewBox="0 0 104 104" aria-label="Windrose"><circle class="rose-ring" cx="52" cy="52" r="50"/><path class="rose-axis" d="M52 8v88M8 52h88M20.9 20.9l62.2 62.2M83.1 20.9L20.9 83.1"/><path class="rose-tick" d="M52 4v8M52 92v8M4 52h8M92 52h8M18.1 18.1l5.7 5.7M80.2 80.2l5.7 5.7M85.9 18.1l-5.7 5.7M23.8 80.2l-5.7 5.7"/><text class="rose-label main" x="52" y="16">N</text><text class="rose-label main" x="88" y="52">O</text><text class="rose-label main" x="52" y="88">S</text><text class="rose-label main" x="16" y="52">W</text><text class="rose-label" x="76" y="28">NO</text><text class="rose-label" x="76" y="76">SO</text><text class="rose-label" x="28" y="76">SW</text><text class="rose-label" x="28" y="28">NW</text><g id="windArrow" class="rose-arrow"><path d="M52 12l7 38h-4v28l-3 7-3-7V50h-4z"/></g><circle class="rose-center" cx="52" cy="52" r="4"/></svg></div>
  </section>
  <div class="footer" id="message">Warte auf Wetterdaten...</div>
</main>
<script>
const f=(n,d=1)=>Number(n||0).toFixed(d);
async function update(){
  try{
    const r=await fetch('/data',{cache:'no-store'});
    const d=await r.json();
    document.getElementById('wifi').textContent=d.wifi_connected?d.ip:'offline';
    document.getElementById('mqtt').textContent=d.mqtt_status;
    document.getElementById('sensor').textContent=d.valid?d.id_hex.toUpperCase():'--';
    document.getElementById('age').textContent=d.valid?d.age_s+' s':'--';
    document.getElementById('message').textContent=d.valid?'Live-Daten aktiv':'Noch keine Daten empfangen';
    document.getElementById('temp').textContent=d.valid?f(d.temp_c,1):'--.-';
    document.getElementById('hum').textContent=d.valid?d.humidity:'--';
    document.getElementById('dew').textContent=d.valid?f(d.dew_point_c,1):'--.-';
    document.getElementById('wind').textContent=d.valid?f(d.wind_speed_ms,1):'--.-';
    document.getElementById('gust').textContent=d.valid?f(d.wind_gust_ms,1):'--.-';
    document.getElementById('rain').textContent=d.valid?f(d.rain_mm,1):'--.-';
    document.getElementById('rainHour').textContent=d.valid?f(d.rain_hour_mm,1):'--.-';
    document.getElementById('rainDay').textContent=d.valid?f(d.rain_day_mm,1):'--.-';
    document.getElementById('rainWeek').textContent=d.valid?f(d.rain_week_mm,1):'--.-';
    document.getElementById('windKmh').textContent=d.valid?f(d.wind_speed_ms*3.6,1)+' km/h':'-- km/h';
    document.getElementById('dirText').textContent=d.valid?d.wind_dir_text:'---';
    document.getElementById('dirDeg').textContent=d.valid?f(d.wind_dir_deg,0)+' deg':'-- deg';
    document.getElementById('battery').innerHTML=d.valid?(d.battery_ok?'<span class=ok>OK</span>':'<span class=bad>LOW</span>'):'--';
    document.getElementById('rssi').textContent=d.valid?'RSSI '+f(d.rssi,1)+' dBm':'RSSI -- dBm';
    document.getElementById('windArrow').setAttribute('transform','rotate('+Number(d.wind_dir_deg||0)+' 52 52)');
  }catch(e){document.getElementById('message').textContent='Verbindung zum Receiver unterbrochen';}
}
setInterval(update,3000);update();
</script>
</body>
</html>
)HTML";

String checked(bool value) {
  return value ? " checked" : "";
}

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleConfig() {
  String html;
  html.reserve(5200);
  html += F("<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Bresser Config</title><style>");
  html += F("body{margin:0;background:#101113;color:#f2f5f8;font-family:Segoe UI,Arial,sans-serif}");
  html += F("header{padding:18px 22px;border-bottom:1px solid #303741;background:#15171a;display:flex;justify-content:space-between;gap:12px}");
  html += F("a{color:#f2f5f8;text-decoration:none}.wrap{max-width:680px;margin:0 auto;padding:22px}");
  html += F(".panel{background:#191b1f;border:1px solid #303741;border-radius:8px;padding:18px}");
  html += F("label{display:block;margin:14px 0 6px;color:#9aa7b4;font-size:13px}");
  html += F("input,select{width:100%;padding:10px;border-radius:8px;border:1px solid #303741;background:#20242a;color:#f2f5f8;font-size:15px}");
  html += F(".row{display:flex;gap:10px;align-items:center;margin-top:14px}.row input{width:auto}");
  html += F(".hint{color:#9aa7b4;font-size:13px;line-height:1.4;margin-top:8px}");
  html += F("button{margin-top:18px;padding:10px 14px;border-radius:8px;border:0;background:#5aa9ff;color:#071018;font-weight:700;cursor:pointer}");
  html += F("</style></head><body><header><b>Bresser Config</b><a href='/'>Dashboard</a></header><div class='wrap'><form class='panel' method='post' action='/save'>");
  html += F("<h3>Sensor-Auswahl</h3><div class='row'><input id='sensorFilter' type='checkbox' name='sensorFilter' value='1'");
  html += checked(cfg.sensorFilterEnabled);
  html += F("><label for='sensorFilter' style='margin:0'>Nur ausgewaehlte Sensor-ID verwenden</label></div>");
  html += F("<label>Sensor-ID</label><select id='sensorId' name='sensorId'><option value=''>Alle Sensoren / keine Auswahl</option>");
  if (cfg.selectedSensorId) {
    html += F("<option selected value='");
    html += sensorIdHex(cfg.selectedSensorId);
    html += F("'>");
    html += sensorIdHex(cfg.selectedSensorId);
    html += F(" (gespeichert)</option>");
  }
  html += F("</select><div class='hint' id='seenHint'>Erkannte Sensoren werden hier angezeigt, sobald Pakete empfangen wurden.</div>");
  html += F("<h3>MQTT</h3><div class='row'><input id='enabled' type='checkbox' name='enabled' value='1'");
  html += checked(cfg.mqttEnabled);
  html += F("><label for='enabled' style='margin:0'>MQTT aktivieren</label></div>");
  html += F("<label>MQTT Host</label><input name='host' value='");
  html += cfg.mqttHost;
  html += F("'><label>Port</label><input name='port' type='number' min='1' max='65535' value='");
  html += String(cfg.mqttPort);
  html += F("'><label>User</label><input name='user' value='");
  html += cfg.mqttUser;
  html += F("'><label>Passwort</label><input name='pass' type='password' value='");
  html += cfg.mqttPass;
  html += F("'><label>Base Topic</label><input name='topic' value='");
  html += cfg.baseTopic;
  html += F("'><button type='submit'>Speichern</button></form></div>");
  html += F("<script>async function loadSensors(){try{const r=await fetch('/sensors',{cache:'no-store'});const d=await r.json();");
  html += F("const sel=document.getElementById('sensorId');const hint=document.getElementById('seenHint');const current=sel.value||d.selected_id_hex||'';sel.innerHTML='';");
  html += F("const all=document.createElement('option');all.value='';all.textContent='Alle Sensoren / keine Auswahl';sel.appendChild(all);let found=!current;");
  html += F("d.sensors.forEach(s=>{const o=document.createElement('option');o.value=s.id_hex;o.textContent=s.id_hex+'  RSSI '+Number(s.rssi).toFixed(1)+' dBm, vor '+s.age_s+'s';if(s.id_hex==current)found=true;sel.appendChild(o);});");
  html += F("if(current&&!found){const o=document.createElement('option');o.value=current;o.textContent=current+' (gespeichert, noch nicht wieder gesehen)';sel.appendChild(o);}");
  html += F("sel.value=current;if(!d.sensors.length){hint.textContent='Noch keine Sensoren erkannt. Empfang abwarten oder Filter kurz deaktivieren, um weitere Stationen zu finden.';return;}");
  html += F("hint.textContent='Erkannt: '+d.sensors.map(s=>s.id_hex+' ('+s.age_s+'s, '+Number(s.rssi).toFixed(1)+' dBm)').join(', ');");
  html += F("}catch(e){}}loadSensors();setInterval(loadSensors,5000);</script></body></html>");
  server.send(200, "text/html", html);
}

void handleSave() {
  bool oldFilterEnabled = cfg.sensorFilterEnabled;
  uint32_t oldSelectedSensorId = cfg.selectedSensorId;

  cfg.mqttEnabled = server.hasArg("enabled");
  cfg.sensorFilterEnabled = server.hasArg("sensorFilter");
  cfg.selectedSensorId = server.hasArg("sensorId") ? parseSensorId(server.arg("sensorId")) : 0;
  if (server.hasArg("host"))  copyToBuffer(cfg.mqttHost, sizeof(cfg.mqttHost), server.arg("host"));
  if (server.hasArg("port"))  cfg.mqttPort = (uint16_t)server.arg("port").toInt();
  if (server.hasArg("user"))  copyToBuffer(cfg.mqttUser, sizeof(cfg.mqttUser), server.arg("user"));
  if (server.hasArg("pass"))  copyToBuffer(cfg.mqttPass, sizeof(cfg.mqttPass), server.arg("pass"));
  if (server.hasArg("topic")) copyToBuffer(cfg.baseTopic, sizeof(cfg.baseTopic), server.arg("topic"));
  savePrefs();
  if (oldFilterEnabled != cfg.sensorFilterEnabled || oldSelectedSensorId != cfg.selectedSensorId) {
    resetSelectedSensorData();
    applySensorFilterToReceiver();
  }
  mqtt.disconnect();
  server.sendHeader("Location", "/config");
  server.send(303);
}

void handleData() {
  StaticJsonDocument<896> doc;
  doc["valid"] = hasData;
  doc["wifi_connected"] = WiFi.isConnected();
  doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : String("");
  doc["mqtt_status"] = mqttStatusText();
  doc["sensor_filter_enabled"] = cfg.sensorFilterEnabled;
  doc["selected_sensor_id"] = cfg.selectedSensorId;
  doc["selected_sensor_id_hex"] = cfg.selectedSensorId ? sensorIdHex(cfg.selectedSensorId) : String("");
  doc["uptime_s"] = millis() / 1000;

  if (hasData) {
    doc["id"]            = lastData.sensor_id;
    doc["id_hex"]        = String(lastData.sensor_id, HEX);
    doc["temp_c"]        = lastData.w.temp_c;
    doc["humidity"]      = lastData.w.humidity;
    doc["dew_point_c"]   = calculateDewPointC(lastData.w.temp_c, lastData.w.humidity);
    doc["wind_speed_ms"] = getWindAvgMS(lastData);
    doc["wind_gust_ms"]  = getWindGustMS(lastData);
    doc["wind_dir_deg"]  = getWindDirDeg(lastData);
    doc["wind_dir_text"] = windDirText(getWindDirDeg(lastData));
    doc["rain_mm"]       = lastData.w.rain_mm;
    doc["rain_hour_mm"]  = rainDeltaForWindow(RAIN_HOUR_MS);
    doc["rain_day_mm"]   = rainDeltaForWindow(RAIN_DAY_MS);
    doc["rain_week_mm"]  = rainDeltaForWindow(RAIN_WEEK_MS);
    doc["battery_ok"]    = lastData.battery_ok;
    doc["rssi"]          = lastData.rssi;
    doc["age_s"]         = (millis() - lastUpdate) / 1000;
  } else {
    doc["id_hex"] = "";
    doc["temp_c"] = 0;
    doc["humidity"] = 0;
    doc["dew_point_c"] = 0;
    doc["wind_speed_ms"] = 0;
    doc["wind_gust_ms"] = 0;
    doc["wind_dir_deg"] = 0;
    doc["wind_dir_text"] = "---";
    doc["rain_mm"] = 0;
    doc["rain_hour_mm"] = 0;
    doc["rain_day_mm"] = 0;
    doc["rain_week_mm"] = 0;
    doc["battery_ok"] = true;
    doc["rssi"] = 0;
    doc["age_s"] = 0;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleSensors() {
  StaticJsonDocument<768> doc;
  doc["filter_enabled"] = cfg.sensorFilterEnabled;
  doc["selected_id"] = cfg.selectedSensorId;
  doc["selected_id_hex"] = cfg.selectedSensorId ? sensorIdHex(cfg.selectedSensorId) : String("");

  JsonArray sensors = doc["sensors"].to<JsonArray>();
  unsigned long now = millis();
  for (size_t i = 0; i < 8; i++) {
    if (seenSensors[i].id == 0) continue;
    JsonObject item = sensors.createNestedObject();
    item["id"] = seenSensors[i].id;
    item["id_hex"] = sensorIdHex(seenSensors[i].id);
    item["rssi"] = seenSensors[i].rssi;
    item["age_s"] = (now - seenSensors[i].lastSeen) / 1000;
    item["selected"] = cfg.selectedSensorId != 0 && seenSensors[i].id == cfg.selectedSensorId;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void setupWeb() {
  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/data", handleData);
  server.on("/sensors", handleSensors);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("HTTP web server started");
}

void setupWiFi() {
  WiFiManager wm;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("BresserHeltec");
  wm.setConfigPortalTimeout(300);

  pinMode(0, INPUT_PULLUP);
  bool forceAp = (digitalRead(0) == LOW);
  if (forceAp || !wm.autoConnect("BresserHeltec")) {
    wm.startConfigPortal("BresserHeltec");
  }

  Serial.print("Waiting for stable WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected");
  }
}

void oledResetAndInit() {
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
  delay(50);

  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delay(20);
  digitalWrite(16, HIGH);
  delay(20);

  Wire.begin(4, 15);
  Wire.setClock(100000);
  delay(10);

  bool found3C = false;
  bool found3D = false;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      if (a == 0x3C) found3C = true;
      if (a == 0x3D) found3D = true;
    }
  }
  if (found3C) oledAddr = 0x3C;
  else if (found3D) oledAddr = 0x3D;

  display = new SSD1306Wire(oledAddr, 4, 15);
  display->init();
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, "Bresser Receiver");
  display->drawString(0, 12, String("OLED @0x") + String(oledAddr, HEX));
  display->display();
}

void updateDisplay(const WeatherSensor::Sensor &d) {
  if (!display) return;
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0,  0, "ID: " + String(d.sensor_id, HEX));
  display->drawString(0, 10, "T: " + String(d.w.temp_c, 1) + " C  H: " + String(d.w.humidity) + "%");
  display->drawString(0, 20, "Tau: " + String(calculateDewPointC(d.w.temp_c, d.w.humidity), 1) + " C");
  display->drawString(0, 32, "W: " + String(getWindAvgMS(d), 1) + " m/s");
  display->drawString(0, 44, "R: " + String(d.w.rain_mm, 1) + " mm");
  display->drawString(0, 54, "1h:" + String(rainDeltaForWindow(RAIN_HOUR_MS), 1) + " 24h:" + String(rainDeltaForWindow(RAIN_DAY_MS), 1));
  display->display();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Bresser Heltec merged ===");

  loadPrefs();
  oledResetAndInit();
  setupWiFi();
  configureMqttClient();
  setupWeb();

  int16_t rc = ws.begin();
  if (rc != RADIOLIB_ERR_NONE) {
    Serial.printf("WeatherSensor init failed (rc=%d)\n", rc);
    if (display) {
      display->clear();
      display->drawString(0, 12, "WS init FAIL");
      display->display();
    }
  } else {
    Serial.println("WeatherSensor init OK");
    applySensorFilterToReceiver();
    if (display) {
      display->clear();
      display->drawString(0, 12, "WS init OK");
      display->display();
    }
  }
}

void processWeatherSensor(const WeatherSensor::Sensor &d) {
  rememberSeenSensor(d);
  if (!sensorAllowed(d.sensor_id)) {
    Serial.printf("Sensor %08lX ignoriert (Filter: %08lX)\n",
                  (unsigned long)d.sensor_id,
                  (unsigned long)cfg.selectedSensorId);
    return;
  }

  lastData = d;
  hasData = true;
  lastUpdate = millis();
  updateRainHistory(d.w.rain_mm);
  mqttPublishWeather(d);
  updateDisplay(d);

  Serial.printf("Sensor %08lX | Temp: %.1f C | Hum: %u%% | Dew: %.1f C | Wind: %.1f m/s | Rain: %.1f mm | 1h: %.1f | 24h: %.1f | 7d: %.1f | Batt: %s\n",
                (unsigned long)d.sensor_id,
                d.w.temp_c,
                d.w.humidity,
                calculateDewPointC(d.w.temp_c, d.w.humidity),
                getWindAvgMS(d),
                d.w.rain_mm,
                rainDeltaForWindow(RAIN_HOUR_MS),
                rainDeltaForWindow(RAIN_DAY_MS),
                rainDeltaForWindow(RAIN_WEEK_MS),
                d.battery_ok ? "OK" : "LOW");
}

void pollWeatherSensor() {
  unsigned long now = millis();
  if (now - lastWeatherPoll < WEATHER_POLL_INTERVAL_MS) return;
  lastWeatherPoll = now;

  if (!cfg.sensorFilterEnabled) {
    ws.clearSlots();
  }
  DecodeStatus status = ws.getMessage();
  if (status != DECODE_OK) return;

  for (auto &d : ws.sensor) {
    if (!d.valid) continue;
    processWeatherSensor(d);
  }
}

void loop() {
  server.handleClient();

  if (WiFi.isConnected()) {
    mqttReconnect();
    if (cfg.mqttEnabled) mqtt.loop();
  }

  pollWeatherSensor();
  server.handleClient();
  delay(1);
}
