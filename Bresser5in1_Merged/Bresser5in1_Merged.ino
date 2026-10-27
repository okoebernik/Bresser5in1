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
#include <Update.h>
#include <ArduinoOTA.h>
#include "web_ui.h"

#ifndef MAX_SENSORS_DEFAULT
#define MAX_SENSORS_DEFAULT 5
#endif
#include <WeatherSensor.h>

// Schreibt auf die serielle Schnittstelle und haelt die letzten Zeilen in einem Ringpuffer,
// den die Web-Konsole (/api/log) ausliest. Jede Zeile bekommt die Uptime als Praefix.
class LogPrint : public Print {
 public:
  static const size_t SIZE = 6144;
  size_t write(uint8_t c) override {
    if (atLineStart_ && c != '\n' && c != '\r') {
      atLineStart_ = false;
      char stamp[16];
      int n = snprintf(stamp, sizeof(stamp), "[%lus] ", (unsigned long)(millis() / 1000));
      for (int i = 0; i < n; i++) put(stamp[i]);
    }
    if (c == '\n') atLineStart_ = true;
    put(c);
    return 1;
  }
  size_t write(const uint8_t *buf, size_t len) override {
    for (size_t i = 0; i < len; i++) write(buf[i]);
    return len;
  }
  uint32_t total() const { return total_; }
  // Kopiert alles ab Position `since` (aelteres wurde ggf. schon ueberschrieben) und setzt `next`.
  String read(uint32_t since, uint32_t &next) const {
    if (since > total_) since = 0;
    if (total_ - since > SIZE) since = total_ - SIZE;
    String out;
    out.reserve(total_ - since);
    for (uint32_t p = since; p < total_; p++) out += buf_[p % SIZE];
    next = total_;
    return out;
  }
 private:
  void put(uint8_t c) {
    Serial.write(c);
    buf_[total_ % SIZE] = (char)c;
    total_++;
  }
  char buf_[SIZE];
  uint32_t total_ = 0;
  bool atLineStart_ = true;
};
LogPrint Log;

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
  char otaPass[32]   = "bresser";
  char hostname[32]  = "BresserHeltec";
  bool staticIp = false;
  char netIp[16]   = "";
  char netGw[16]   = "";
  char netMask[16] = "255.255.255.0";
  char netDns[16]  = "";
} cfg;

// Netzwerk-Aenderungen werden erst nach der HTTP-Antwort angewendet (WLAN-Wechsel
// wuerde sonst die laufende Verbindung zum Browser kappen).
bool netPending = false;
unsigned long netApplyAt = 0;
String netNewSsid;
String netNewPass;

const char *OTA_USER = "admin";
const char *FW_BUILD = __DATE__ " " __TIME__;
bool otaAuthFailed = false;
bool otaFinished = false;

WeatherSensor::Sensor lastData{};
bool hasData = false;
unsigned long lastPacketMs = 0;
uint32_t activeSensorId = 0;
const unsigned long ACTIVE_SENSOR_TIMEOUT_MS = 30UL * 60UL * 1000UL;
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

// Plausibilitaet des Regen-Gesamtzaehlers: Bei schwachem Funksignal werden einzelne Pakete
// fehlerhaft dekodiert. Ein Sprung groesser als RAIN_MAX_STEP_MM pro Paket (oder ein Rueckgang)
// gilt nur dann als echt, wenn ihn das naechste Paket bestaetigt.
const float RAIN_MAX_STEP_MM = 5.0f;
const float RAIN_CONFIRM_TOLERANCE_MM = 0.05f;
float rainAccepted = 0.0f;
bool rainAcceptedValid = false;
float rainCandidate = 0.0f;
bool rainCandidateValid = false;

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

void clearRainHistory() {
  memset(rainHistory, 0, sizeof(rainHistory));
  rainHistoryHead = 0;
  rainHistoryFilled = false;
  lastRainHistoryStore = 0;
  lastRainHistoryValue = NAN;
}

// Prueft den neuen Zaehlerstand. Liefert true, wenn er uebernommen wurde (rainAccepted ist dann aktuell).
bool acceptRain(float v) {
  bool plausible = rainAcceptedValid && v >= rainAccepted && (v - rainAccepted) <= RAIN_MAX_STEP_MM;
  if (plausible) {
    rainAccepted = v;
    rainCandidateValid = false;
    return true;
  }

  bool confirmed = rainCandidateValid && fabsf(v - rainCandidate) <= RAIN_CONFIRM_TOLERANCE_MM;
  if (!confirmed) {
    if (rainAcceptedValid) {
      Log.printf("Regen: unplausibler Zaehlerstand %.1f mm (zuletzt %.1f mm), warte auf Bestaetigung\n", v, rainAccepted);
    }
    rainCandidate = v;
    rainCandidateValid = true;
    return false;
  }

  // Bestaetigt: neuer Zaehlerstand ist echt (Erststart, Station neu gestartet oder Empfangsluecke)
  rainCandidateValid = false;
  if (rainAcceptedValid) {
    Log.printf("Regen: Zaehlerstand %.1f mm bestaetigt (vorher %.1f mm), Regenfenster neu gestartet\n", v, rainAccepted);
  }
  bool counterReset = rainAcceptedValid && v < rainAccepted;
  clearRainHistory();
  if (counterReset) updateRainHistory(0.0f);  // seit dem Reset gefallener Regen zaehlt weiter
  rainAccepted = v;
  rainAcceptedValid = true;
  return true;
}

// Zaehlerstand fuer Anzeige/MQTT: der geprueften Wert fuer den aktiven Sensor, sonst der Rohwert.
float rainTotalMm(const WeatherSensor::Sensor &d) {
  return (rainAcceptedValid && d.sensor_id == activeSensorId) ? rainAccepted : d.w.rain_mm;
}

float rainDeltaForWindow(unsigned long windowMs) {
  if (!hasData || !rainAcceptedValid) return 0.0f;

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

  float delta = rainAccepted - rainHistory[baseSlot].rainMm;
  if (delta < 0.0f) return rainAccepted;
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
  activeSensorId = 0;
  lastData = WeatherSensor::Sensor();
  clearRainHistory();
  rainAcceptedValid = false;
  rainCandidateValid = false;
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
    Log.printf("Sensor-Filter aktiv: %08lX\n", (unsigned long)cfg.selectedSensorId);
  } else {
    ws.setSensorsInc(emptyList, sizeof(emptyList));
    Log.println("Sensor-Filter aus: alle Sensoren erlaubt");
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
  copyToBuffer(cfg.otaPass, sizeof(cfg.otaPass), prefs.getString("otaPass", cfg.otaPass));
  copyToBuffer(cfg.hostname, sizeof(cfg.hostname), prefs.getString("hostname", cfg.hostname));
  cfg.staticIp = prefs.getBool("netStatic", cfg.staticIp);
  copyToBuffer(cfg.netIp, sizeof(cfg.netIp), prefs.getString("netIp", cfg.netIp));
  copyToBuffer(cfg.netGw, sizeof(cfg.netGw), prefs.getString("netGw", cfg.netGw));
  copyToBuffer(cfg.netMask, sizeof(cfg.netMask), prefs.getString("netMask", cfg.netMask));
  copyToBuffer(cfg.netDns, sizeof(cfg.netDns), prefs.getString("netDns", cfg.netDns));
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
  prefs.putString("otaPass", cfg.otaPass);
  prefs.putString("hostname", cfg.hostname);
  prefs.putBool("netStatic", cfg.staticIp);
  prefs.putString("netIp", cfg.netIp);
  prefs.putString("netGw", cfg.netGw);
  prefs.putString("netMask", cfg.netMask);
  prefs.putString("netDns", cfg.netDns);
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
    Log.printf("MQTT buffer resize failed (%u bytes)\n", MQTT_PACKET_BUFFER_SIZE);
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
  Log.printf("MQTT %s (%s:%u)\n", ok ? "connected" : "failed", cfg.mqttHost, cfg.mqttPort);
}

// withRain=false fuer Sensoren ohne eigene Regenhistorie (nicht der aktive Sensor)
void mqttPublishWeather(const WeatherSensor::Sensor &d, bool withRain) {
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
  doc["rain_mm"]       = rainTotalMm(d);
  if (withRain) {
    doc["rain_hour_mm"]  = rainDeltaForWindow(RAIN_HOUR_MS);
    doc["rain_day_mm"]   = rainDeltaForWindow(RAIN_DAY_MS);
    doc["rain_week_mm"]  = rainDeltaForWindow(RAIN_WEEK_MS);
  }
  doc["battery_ok"]    = d.battery_ok;
  doc["rssi"]          = d.rssi;
  doc["uptime_s"]      = millis() / 1000;

  char payload[896];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0 || n >= sizeof(payload)) {
    Log.printf("MQTT publish skipped: payload too large (%u/%u bytes)\n",
                  (unsigned int)n,
                  (unsigned int)sizeof(payload));
    return;
  }

  char topic[128];
  snprintf(topic, sizeof(topic), "%s/data/%08lX", cfg.baseTopic, (unsigned long)d.sensor_id);
  bool ok = mqtt.publish(topic, (const uint8_t *)payload, n);
  if (!ok) {
    Log.printf("MQTT publish failed: topic=%s payload=%u buffer=%u state=%d\n",
                  topic,
                  (unsigned int)n,
                  MQTT_PACKET_BUFFER_SIZE,
                  mqtt.state());
  }
}

void handleIndex() {
  server.send_P(200, "text/html; charset=utf-8", WEB_HTML);
}

void handleStyle() {
  server.send_P(200, "text/css; charset=utf-8", WEB_CSS);
}

void handleScript() {
  server.send_P(200, "application/javascript; charset=utf-8", WEB_JS);
}

// Das MQTT-Passwort wird nie ausgeliefert; leeres Feld beim Speichern = unveraendert.
void handleConfigGet() {
  StaticJsonDocument<384> doc;
  doc["sensor_filter_enabled"] = cfg.sensorFilterEnabled;
  doc["selected_sensor_id_hex"] = cfg.selectedSensorId ? sensorIdHex(cfg.selectedSensorId) : String("");
  doc["mqtt_enabled"] = cfg.mqttEnabled;
  doc["mqtt_host"] = cfg.mqttHost;
  doc["mqtt_port"] = cfg.mqttPort;
  doc["mqtt_user"] = cfg.mqttUser;
  doc["base_topic"] = cfg.baseTopic;
  doc["fw_build"] = FW_BUILD;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleSave() {
  bool oldFilterEnabled = cfg.sensorFilterEnabled;
  uint32_t oldSelectedSensorId = cfg.selectedSensorId;

  long port = server.arg("port").toInt();
  if (server.hasArg("port") && (port < 1 || port > 65535)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid port\"}");
    return;
  }

  cfg.mqttEnabled = server.hasArg("enabled");
  cfg.sensorFilterEnabled = server.hasArg("sensorFilter");
  cfg.selectedSensorId = server.hasArg("sensorId") ? parseSensorId(server.arg("sensorId")) : 0;
  if (server.hasArg("host"))  copyToBuffer(cfg.mqttHost, sizeof(cfg.mqttHost), server.arg("host"));
  if (server.hasArg("port"))  cfg.mqttPort = (uint16_t)port;
  if (server.hasArg("user"))  copyToBuffer(cfg.mqttUser, sizeof(cfg.mqttUser), server.arg("user"));
  if (server.arg("pass").length() > 0) copyToBuffer(cfg.mqttPass, sizeof(cfg.mqttPass), server.arg("pass"));
  if (server.hasArg("topic")) copyToBuffer(cfg.baseTopic, sizeof(cfg.baseTopic), server.arg("topic"));
  if (server.arg("otaPass").length() > 0) {
    copyToBuffer(cfg.otaPass, sizeof(cfg.otaPass), server.arg("otaPass"));
    ArduinoOTA.setPassword(cfg.otaPass);
  }
  savePrefs();
  if (oldFilterEnabled != cfg.sensorFilterEnabled || oldSelectedSensorId != cfg.selectedSensorId) {
    resetSelectedSensorData();
    applySensorFilterToReceiver();
  }
  mqtt.disconnect();
  server.send(200, "application/json", "{\"ok\":true}");
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
    doc["rain_mm"]       = rainTotalMm(lastData);
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

bool validHostname(const String &h) {
  if (h.length() < 1 || h.length() > 31) return false;
  for (size_t i = 0; i < h.length(); i++) {
    char c = h[i];
    if (!(isalnum((unsigned char)c) || c == '-')) return false;
  }
  return h[0] != '-' && h[h.length() - 1] != '-';
}

void handleNetworkGet() {
  StaticJsonDocument<768> doc;
  bool up = WiFi.isConnected();
  doc["connected"] = up;
  doc["ssid"] = up ? WiFi.SSID() : String("");
  doc["rssi"] = up ? WiFi.RSSI() : 0;
  doc["ip"] = up ? WiFi.localIP().toString() : String("");
  doc["gateway"] = up ? WiFi.gatewayIP().toString() : String("");
  doc["subnet"] = up ? WiFi.subnetMask().toString() : String("");
  doc["dns"] = up ? WiFi.dnsIP().toString() : String("");
  doc["mac"] = WiFi.macAddress();
  doc["hostname"] = cfg.hostname;
  doc["dhcp"] = !cfg.staticIp;
  doc["static_ip"] = cfg.netIp;
  doc["static_gateway"] = cfg.netGw;
  doc["static_subnet"] = cfg.netMask;
  doc["static_dns"] = cfg.netDns;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST startet einen asynchronen Scan, GET liefert das Ergebnis (running=true solange er laeuft).
void handleScanStart() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleScanResult() {
  int n = WiFi.scanComplete();
  StaticJsonDocument<1536> doc;
  doc["running"] = (n == WIFI_SCAN_RUNNING);
  JsonArray nets = doc["networks"].to<JsonArray>();
  if (n > 0) {
    int shown = 0;
    for (int i = 0; i < n && shown < 12; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      bool dup = false;
      for (JsonObject o : nets) {
        if (ssid == o["ssid"].as<String>()) { dup = true; break; }
      }
      if (dup) continue;
      JsonObject item = nets.createNestedObject();
      item["ssid"] = ssid;
      item["rssi"] = WiFi.RSSI(i);
      item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      shown++;
    }
    WiFi.scanDelete();
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void sendJsonError(int code, const char *msg) {
  server.send(code, "application/json", String("{\"ok\":false,\"error\":\"") + msg + "\"}");
}

// Aendert WLAN/IP/Hostname. Geschuetzt mit dem Geraete-Passwort (Basic-Auth wie bei OTA),
// weil eine falsche Einstellung das Geraet vom Netz nehmen kann.
void handleNetworkSave() {
  if (!server.authenticate(OTA_USER, cfg.otaPass)) {
    sendJsonError(401, "unauthorized");
    return;
  }

  String hostname = server.arg("hostname");
  hostname.trim();
  if (!validHostname(hostname)) {
    sendJsonError(400, "invalid hostname");
    return;
  }

  bool useStatic = !server.hasArg("dhcp");
  IPAddress ip, gw, mask, dns;
  if (useStatic) {
    if (!ip.fromString(server.arg("ip")) || !gw.fromString(server.arg("gateway")) ||
        !mask.fromString(server.arg("subnet"))) {
      sendJsonError(400, "invalid ip settings");
      return;
    }
    if (server.arg("dns").length() > 0 && !dns.fromString(server.arg("dns"))) {
      sendJsonError(400, "invalid dns");
      return;
    }
  }

  String ssid = server.arg("ssid");
  String pass = server.arg("wifiPass");
  ssid.trim();
  bool credsChanged = ssid.length() > 0 && (ssid != WiFi.SSID() || pass.length() > 0);
  if (ssid.length() > 32 || pass.length() > 63) {
    sendJsonError(400, "ssid/password too long");
    return;
  }

  copyToBuffer(cfg.hostname, sizeof(cfg.hostname), hostname);
  cfg.staticIp = useStatic;
  if (useStatic) {
    copyToBuffer(cfg.netIp, sizeof(cfg.netIp), server.arg("ip"));
    copyToBuffer(cfg.netGw, sizeof(cfg.netGw), server.arg("gateway"));
    copyToBuffer(cfg.netMask, sizeof(cfg.netMask), server.arg("subnet"));
    copyToBuffer(cfg.netDns, sizeof(cfg.netDns), server.arg("dns"));
  }
  savePrefs();

  netNewSsid = credsChanged ? ssid : String("");
  netNewPass = credsChanged ? pass : String("");
  netPending = true;
  netApplyAt = millis() + 800;
  server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
}

// Neue Zugangsdaten werden erst getestet (ohne zu speichern). Nur bei Erfolg werden sie
// dauerhaft uebernommen, sonst bleibt das bisherige WLAN gespeichert. Danach Neustart,
// damit statische IP und Hostname sauber greifen.
void applyPendingNetwork() {
  if (!netPending || (long)(millis() - netApplyAt) < 0) return;
  netPending = false;

  if (netNewSsid.length() > 0) {
    Log.printf("WLAN-Wechsel auf '%s' wird getestet\n", netNewSsid.c_str());
    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.begin(netNewSsid.c_str(), netNewPass.length() ? netNewPass.c_str() : nullptr);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
    WiFi.persistent(true);
    if (WiFi.status() == WL_CONNECTED) {
      Log.println("Neues WLAN ok, wird gespeichert");
      WiFi.begin(netNewSsid.c_str(), netNewPass.length() ? netNewPass.c_str() : nullptr);
      delay(500);
    } else {
      Log.println("Neues WLAN fehlgeschlagen, altes bleibt gespeichert");
    }
  }
  Log.println("Netzwerk-Einstellungen uebernommen, Neustart");
  delay(200);
  ESP.restart();
}

// ---------- REST-API fuer andere Geraete (z.B. Multiinstrument) ----------
// Gleiches Muster wie beim Clinometer: GET /api/<geraet>/values liefert flaches JSON, Einheiten
// stehen im Feldnamen, "stale" ist true, wenn keine frischen Daten vorliegen. Nur lesend, ohne Login.
const unsigned long API_STALE_AFTER_S = 120;

void sendApiJson(const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

void handleApiWeatherValues() {
  StaticJsonDocument<768> doc;
  unsigned long ageS = hasData ? (millis() - lastUpdate) / 1000 : 0;
  bool stale = !hasData || ageS > API_STALE_AFTER_S;
  doc["valid"] = hasData;
  doc["stale"] = stale;

  if (hasData) {
    doc["sensor_id"]       = sensorIdHex(lastData.sensor_id);
    doc["age_s"]           = ageS;
    doc["temperature_c"]   = lastData.w.temp_c;
    doc["humidity_pct"]    = lastData.w.humidity;
    doc["dew_point_c"]     = calculateDewPointC(lastData.w.temp_c, lastData.w.humidity);
    doc["wind_speed_ms"]   = getWindAvgMS(lastData);
    doc["wind_gust_ms"]    = getWindGustMS(lastData);
    doc["wind_dir_deg"]    = getWindDirDeg(lastData);
    doc["wind_dir_text"]   = windDirText(getWindDirDeg(lastData));
    doc["rain_total_mm"]   = rainTotalMm(lastData);
    doc["rain_1h_mm"]      = rainDeltaForWindow(RAIN_HOUR_MS);
    doc["rain_24h_mm"]     = rainDeltaForWindow(RAIN_DAY_MS);
    doc["rain_7d_mm"]      = rainDeltaForWindow(RAIN_WEEK_MS);
    doc["battery_ok"]      = lastData.battery_ok;
    doc["rssi_dbm"]        = lastData.rssi;
  }
  sendApiJson(doc);
}

void handleApiStatus() {
  StaticJsonDocument<512> doc;
  doc["fw_build"] = FW_BUILD;
  doc["uptime_s"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_connected"] = WiFi.isConnected();
  doc["wifi_rssi_dbm"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : String("");
  doc["mqtt_status"] = mqttStatusText();
  doc["active_sensor_id"] = activeSensorId ? sensorIdHex(activeSensorId) : String("");
  doc["last_packet_age_s"] = lastPacketMs ? (long)((millis() - lastPacketMs) / 1000) : -1;
  sendApiJson(doc);
}

unsigned long restartAt = 0;

// Geraete-Neustart per Web-UI (Basic-Auth wie bei OTA); der Neustart erfolgt nach der Antwort.
void handleRestart() {
  if (!server.authenticate(OTA_USER, cfg.otaPass)) {
    sendJsonError(401, "unauthorized");
    return;
  }
  Log.println("Neustart per Web-UI angefordert");
  server.send(200, "application/json", "{\"ok\":true}");
  restartAt = millis() + 800;
}

// Liefert neue Log-Zeilen ab Position `since`; die naechste Position steht im Header X-Log-Next.
void handleLog() {
  uint32_t next = 0;
  String text = Log.read((uint32_t)server.arg("since").toInt(), next);
  server.sendHeader("X-Log-Next", String(next));
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", text);
}

void showOtaProgress(const char *label, unsigned int percent) {
  if (!display) return;
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, label);
  display->drawProgressBar(0, 30, 120, 10, percent);
  display->drawString(0, 46, String(percent) + " %");
  display->display();
}

// Firmware-Upload per Browser (multipart, Feld "firmware"), geschuetzt per Basic-Auth.
void handleOtaFinish() {
  bool authFailed = otaAuthFailed || !server.authenticate(OTA_USER, cfg.otaPass);
  bool finished = otaFinished;
  otaAuthFailed = false;
  otaFinished = false;
  if (authFailed) {
    server.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return;
  }
  bool ok = finished && !Update.hasError();
  server.send(ok ? 200 : 500, "application/json",
              ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"update failed\"}");
  if (ok) {
    delay(500);
    ESP.restart();
  }
}

void handleOtaUpload() {
  // Bei nicht-multipart-Requests ruft WebServer diesen Handler als "raw"-Callback
  // ohne Upload-Objekt auf; server.upload() waere dann ein Null-Zugriff.
  if (server.header("Content-Type").indexOf("multipart/form-data") < 0) return;
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaAuthFailed = !server.authenticate(OTA_USER, cfg.otaPass);
    if (otaAuthFailed) return;
    Log.printf("OTA (Web) Start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Log);
    showOtaProgress("OTA Update (Web)", 0);
  } else if (otaAuthFailed) {
    return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Log);
    // Gesamtgroesse ist beim Multipart-Upload nur ueber Content-Length bekannt (inkl. kleinem Overhead)
    static unsigned int lastPct = 101;
    size_t total = server.clientContentLength();
    unsigned int pct = total ? (unsigned int)min<size_t>(99, (up.totalSize * 100UL) / total) : 0;
    if (pct != lastPct) {
      lastPct = pct;
      showOtaProgress("OTA Update (Web)", pct);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    showOtaProgress("OTA Update (Web)", 100);
    if (Update.end(true)) {
      otaFinished = true;
      Log.printf("OTA (Web) fertig: %u Bytes\n", (unsigned int)up.totalSize);
    } else {
      Update.printError(Log);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void setupOta() {
  ArduinoOTA.setHostname(cfg.hostname);
  ArduinoOTA.setPassword(cfg.otaPass);
  ArduinoOTA.onStart([]() {
    Log.println("OTA Start");
    showOtaProgress("OTA Update", 0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static unsigned int lastPct = 101;
    unsigned int pct = total ? (done * 100U) / total : 0;
    if (pct != lastPct) {
      lastPct = pct;
      showOtaProgress("OTA Update", pct);
    }
  });
  ArduinoOTA.onEnd([]() { Log.println("OTA fertig, Neustart"); });
  ArduinoOTA.onError([](ota_error_t err) { Log.printf("OTA Fehler %u\n", (unsigned int)err); });
  ArduinoOTA.begin();
}

void setupWeb() {
  const char *headerKeys[] = {"Content-Type"};
  server.collectHeaders(headerKeys, 1);
  server.on("/api/system/ota", HTTP_POST, handleOtaFinish, handleOtaUpload);
  server.on("/", handleIndex);
  server.on("/style.css", handleStyle);
  server.on("/app.js", handleScript);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/weather/values", HTTP_GET, handleApiWeatherValues);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/system/restart", HTTP_POST, handleRestart);
  server.on("/api/network", HTTP_GET, handleNetworkGet);
  server.on("/api/network", HTTP_POST, handleNetworkSave);
  server.on("/api/network/scan", HTTP_POST, handleScanStart);
  server.on("/api/network/scan", HTTP_GET, handleScanResult);
  server.on("/config", []() {
    server.sendHeader("Location", "/#mqtt");
    server.send(303);
  });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/data", handleData);
  server.on("/sensors", handleSensors);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  Log.println("HTTP web server started");
}

void setupWiFi() {
  WiFiManager wm;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname);
  wm.setConfigPortalTimeout(300);

  IPAddress ip, gw, mask, dns;
  if (cfg.staticIp && ip.fromString(cfg.netIp) && gw.fromString(cfg.netGw) && mask.fromString(cfg.netMask)) {
    if (!dns.fromString(cfg.netDns)) dns = gw;
    wm.setSTAStaticIPConfig(ip, gw, mask, dns);
    Log.printf("Statische IP: %s\n", cfg.netIp);
  }

  pinMode(0, INPUT_PULLUP);
  bool forceAp = (digitalRead(0) == LOW);
  if (forceAp || !wm.autoConnect("BresserHeltec")) {
    wm.startConfigPortal("BresserHeltec");
  }

  Log.print("Waiting for stable WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Log.print(".");
    retries++;
  }
  Log.println();

  if (WiFi.status() == WL_CONNECTED) {
    Log.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Log.println("WiFi not connected");
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
  display->flipScreenVertically();
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
  display->drawString(0, 44, "R: " + String(rainTotalMm(d), 1) + " mm");
  display->drawString(0, 54, "1h:" + String(rainDeltaForWindow(RAIN_HOUR_MS), 1) + " 24h:" + String(rainDeltaForWindow(RAIN_DAY_MS), 1));
  display->display();
}

void setup() {
  Serial.begin(115200);
  Log.println();
  Log.println("=== Bresser Heltec merged ===");

  loadPrefs();
  oledResetAndInit();
  setupWiFi();
  configureMqttClient();
  setupWeb();
  setupOta();

  int16_t rc = ws.begin();
  if (rc != RADIOLIB_ERR_NONE) {
    Log.printf("WeatherSensor init failed (rc=%d)\n", rc);
    if (display) {
      display->clear();
      display->drawString(0, 12, "WS init FAIL");
      display->display();
    }
  } else {
    Log.println("WeatherSensor init OK");
    applySensorFilterToReceiver();
    if (display) {
      display->clear();
      display->drawString(0, 12, "WS init OK");
      display->drawString(0, 26, WiFi.isConnected() ? "IP " + WiFi.localIP().toString() : String("WiFi offline"));
      display->display();
    }
  }
}

void processWeatherSensor(const WeatherSensor::Sensor &d) {
  lastPacketMs = millis();
  rememberSeenSensor(d);
  if (!sensorAllowed(d.sensor_id)) {
    Log.printf("Sensor %08lX ignoriert (Filter: %08lX)\n",
                  (unsigned long)d.sensor_id,
                  (unsigned long)cfg.selectedSensorId);
    return;
  }

  // Dashboard, OLED und Regenhistorie gehoeren immer zu genau einem Sensor.
  // Vorrang hat die gespeicherte Sensor-ID, sonst der zuerst empfangene Sensor.
  // Verschwindet dieser fuer ACTIVE_SENSOR_TIMEOUT_MS, wird auf den neuen gewechselt.
  if (cfg.selectedSensorId != 0) {
    activeSensorId = cfg.selectedSensorId;
  } else if (activeSensorId == 0) {
    activeSensorId = d.sensor_id;
  } else if (d.sensor_id != activeSensorId && hasData &&
             millis() - lastUpdate >= ACTIVE_SENSOR_TIMEOUT_MS) {
    resetSelectedSensorData();
    activeSensorId = d.sensor_id;
  }

  if (d.sensor_id != activeSensorId) {
    Log.printf("Sensor %08lX (weitere Station) | Temp: %.1f C | Hum: %u%% | RSSI: %.1f dBm\n",
               (unsigned long)d.sensor_id, d.w.temp_c, d.w.humidity, d.rssi);
    mqttPublishWeather(d, false);
    return;
  }

  lastData = d;
  hasData = true;
  lastUpdate = millis();
  if (acceptRain(d.w.rain_mm)) updateRainHistory(rainAccepted);
  mqttPublishWeather(d, true);
  updateDisplay(d);

  Log.printf("Sensor %08lX | Temp: %.1f C | Hum: %u%% | Dew: %.1f C | Wind: %.1f m/s | Rain: %.1f mm | 1h: %.1f | 24h: %.1f | 7d: %.1f | Batt: %s | RSSI: %.1f dBm\n",
                (unsigned long)d.sensor_id,
                d.w.temp_c,
                d.w.humidity,
                calculateDewPointC(d.w.temp_c, d.w.humidity),
                getWindAvgMS(d),
                rainTotalMm(d),
                rainDeltaForWindow(RAIN_HOUR_MS),
                rainDeltaForWindow(RAIN_DAY_MS),
                rainDeltaForWindow(RAIN_WEEK_MS),
                d.battery_ok ? "OK" : "LOW",
                d.rssi);
}

void pollWeatherSensor() {
  unsigned long now = millis();
  if (now - lastWeatherPoll < WEATHER_POLL_INTERVAL_MS) return;
  lastWeatherPoll = now;

  // Lebenszeichen fuer die Web-Konsole, solange nichts empfangen wird
  static unsigned long lastBeat = 0;
  if (now - lastPacketMs >= 30000UL && now - lastBeat >= 30000UL) {
    lastBeat = now;
    Log.printf("Empfang: seit %lu s kein Paket%s\n", (now - lastPacketMs) / 1000UL,
               cfg.sensorFilterEnabled ? " (Sensorfilter aktiv)" : "");
  }

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
  ArduinoOTA.handle();
  server.handleClient();
  applyPendingNetwork();
  if (restartAt != 0 && (long)(millis() - restartAt) >= 0) ESP.restart();

  if (WiFi.isConnected()) {
    mqttReconnect();
    if (cfg.mqttEnabled) mqtt.loop();
  }

  pollWeatherSensor();
  server.handleClient();
  delay(1);
}
