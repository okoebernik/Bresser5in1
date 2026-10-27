// Erzeugt die Screenshots fuer das Handbuch (docs/screenshots/<sprache>-<seite>.png).
//
// Die Web-UI (HTML/CSS/JS) wird direkt aus Bresser5in1_Merged/web_ui.h gelesen und von einem kleinen
// lokalen Server mit festen Beispieldaten ausgeliefert. So entstehen die Bilder ohne Geraet und ohne
// echte IP-/WLAN-Daten. Aufruf: node docs/tools/make_screenshots.js [pfad/zu/msedge.exe]
"use strict";
const fs = require("fs");
const http = require("http");
const path = require("path");
const { spawn } = require("child_process");

const root = path.resolve(__dirname, "..", "..");
const outDir = path.join(root, "docs", "screenshots");
const edge = process.argv[2] || "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
const PORT = 8765;

const header = fs.readFileSync(path.join(root, "Bresser5in1_Merged", "web_ui.h"), "utf8");
const grab = name => {
  const m = header.match(new RegExp("R\"" + name + "\\(([\\s\\S]*?)\\)" + name + "\""));
  if (!m) throw new Error("Block " + name + " nicht gefunden");
  return m[1];
};
const assets = { html: grab("HTML"), css: grab("CSS"), js: grab("JS") };

const weather = {
  valid: true, wifi_connected: true, ip: "192.168.1.50", mqtt_status: "connected", sensor_filter_enabled: true,
  selected_sensor_id: 0x44125371, selected_sensor_id_hex: "44125371", uptime_s: 86400, id: 0x44125371, id_hex: "44125371",
  temp_c: 18.4, humidity: 62, dew_point_c: 11.1, wind_speed_ms: 3.2, wind_gust_ms: 5.8, wind_dir_deg: 225, wind_dir_text: "SW",
  rain_mm: 113.6, rain_hour_mm: 0.4, rain_day_mm: 2.1, rain_week_mm: 9.8, battery_ok: true, rssi: -92.0, age_s: 7
};
const logText = [
  "[0s] === Bresser Heltec merged ===",
  "[3s] WiFi connected: 192.168.1.50",
  "[3s] HTTP web server started",
  "[3s] WeatherSensor init OK",
  "[3s] Sensor-Filter aktiv: 44125371",
  "[5s] MQTT connected (192.168.1.10:1883)",
  "[11s] Sensor 44125371 | Temp: 18.4 C | Hum: 62% | Dew: 11.1 C | Wind: 3.2 m/s | Rain: 113.6 mm | 1h: 0.4 | 24h: 2.1 | 7d: 9.8 | Batt: OK | RSSI: -92.0 dBm",
  "[23s] Sensor 44125371 | Temp: 18.4 C | Hum: 62% | Dew: 11.1 C | Wind: 3.5 m/s | Rain: 113.6 mm | 1h: 0.4 | 24h: 2.1 | 7d: 9.8 | Batt: OK | RSSI: -91.5 dBm",
  "[27s] Sensor 0A4B21C7 (weitere Station) | Temp: 21.0 C | Hum: 48% | RSSI: -108.5 dBm",
  "[35s] Sensor 44125371 | Temp: 18.5 C | Hum: 62% | Dew: 11.2 C | Wind: 2.9 m/s | Rain: 113.6 mm | 1h: 0.4 | 24h: 2.1 | 7d: 9.8 | Batt: OK | RSSI: -93.0 dBm",
  "[47s] Sensor 44125371 | Temp: 18.5 C | Hum: 61% | Dew: 11.0 C | Wind: 3.8 m/s | Rain: 113.6 mm | 1h: 0.4 | 24h: 2.1 | 7d: 9.8 | Batt: OK | RSSI: -92.5 dBm",
  "[89s] Empfang: seit 30 s kein Paket (Sensorfilter aktiv)"
].join("\n") + "\n";

const api = {
  "/data": () => weather,
  "/sensors": () => ({
    filter_enabled: true, selected_id: 0x44125371, selected_id_hex: "44125371",
    sensors: [
      { id: 0x44125371, id_hex: "44125371", rssi: -92.0, age_s: 7, selected: true },
      { id: 0x0A4B21C7, id_hex: "0A4B21C7", rssi: -108.5, age_s: 19, selected: false }
    ]
  }),
  "/api/config": () => ({
    sensor_filter_enabled: true, selected_sensor_id_hex: "44125371", mqtt_enabled: true, mqtt_host: "192.168.1.10",
    mqtt_port: 1883, mqtt_user: "wetter", base_topic: "bresser", fw_build: "Oct 27 2026 09:44:42"
  }),
  "/api/network": () => ({
    connected: true, ssid: "HomeWiFi", rssi: -58, ip: "192.168.1.50", gateway: "192.168.1.1", subnet: "255.255.255.0",
    dns: "192.168.1.1", mac: "10:52:1C:00:00:00", hostname: "BresserHeltec", dhcp: true,
    static_ip: "", static_gateway: "", static_subnet: "255.255.255.0", static_dns: ""
  })
};

const server = http.createServer((req, res) => {
  const url = new URL(req.url, "http://localhost");
  if (url.pathname === "/") {
    const lang = url.searchParams.get("lang") === "de" ? "de" : "en";
    const inject = "<script>try{localStorage.setItem('bresser.lang','" + lang + "')}catch(e){}</script>\n";
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
    return res.end(assets.html.replace('<script src="/app.js">', inject + '<script src="/app.js">'));
  }
  if (url.pathname === "/style.css") { res.writeHead(200, { "Content-Type": "text/css; charset=utf-8" }); return res.end(assets.css); }
  if (url.pathname === "/app.js") { res.writeHead(200, { "Content-Type": "application/javascript; charset=utf-8" }); return res.end(assets.js); }
  if (url.pathname === "/api/log") {
    const since = Number(url.searchParams.get("since") || 0);
    res.writeHead(200, { "Content-Type": "text/plain; charset=utf-8", "X-Log-Next": String(logText.length) });
    return res.end(since >= logText.length ? "" : logText);
  }
  if (api[url.pathname]) { res.writeHead(200, { "Content-Type": "application/json" }); return res.end(JSON.stringify(api[url.pathname]())); }
  res.writeHead(404); res.end("not found");
});

const pages = ["dashboard", "sensor", "network", "mqtt", "console", "system"];
const shots = [];
for (const lang of ["de", "en"]) {
  for (const page of pages) shots.push({ lang, page, name: lang + "-" + page, size: "1280,900" });
}

// Edge muss asynchron laufen, sonst blockiert der Event-Loop und der lokale Server kann nicht antworten.
function runEdge(args) {
  return new Promise(resolve => {
    const child = spawn(edge, args, { stdio: "ignore" });
    const timer = setTimeout(() => child.kill(), 60000);
    child.on("exit", () => { clearTimeout(timer); resolve(); });
  });
}

server.listen(PORT, "127.0.0.1", async () => {
  fs.mkdirSync(outDir, { recursive: true });
  let failed = 0;
  for (const s of shots) {
    const file = path.join(outDir, s.name + ".png");
    const profile = path.join(require("os").tmpdir(), "bresser-shots-" + s.name);
    await runEdge([
      "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run", "--user-data-dir=" + profile,
      "--window-size=" + s.size, "--virtual-time-budget=8000", "--screenshot=" + file,
      "http://127.0.0.1:" + PORT + "/?lang=" + s.lang + "#" + s.page
    ]);
    const ok = fs.existsSync(file);
    if (!ok) failed++;
    console.log((ok ? "ok     " : "FEHLER ") + s.name);
  }
  server.close();
  process.exit(failed ? 1 : 0);
});
