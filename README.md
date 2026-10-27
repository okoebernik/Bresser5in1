# Bresser 5-in-1 LoRa/Wetterempfaenger

*[English version](README.en.md)*

Arduino-Sketch fuer ein Heltec WiFi LoRa 32 V2 Board zum Empfangen einer Bresser 5-in-1 Wetterstation. Das Programm liest die Wetterdaten auf der europaeischen Frequenz im 868-MHz-Band ein, zeigt sie auf dem OLED und im Webinterface an und kann die Werte per MQTT veroeffentlichen.

## Funktionen

- Empfang von Bresser 5-in-1 Wetterdaten ueber `BresserWeatherSensorReceiver`
- Modernes Web-Dashboard mit Live-Werten (Englisch/Deutsch)
- Seiten fuer Sensor-Auswahl, Netzwerk, MQTT, Konsole und System
- Auswahl einer bestimmten Sensor-ID, wenn mehrere Wetterstationen in der Umgebung aktiv sind
- Anzeige erkannter Sensor-IDs im Pulldown-Menue
- MQTT-Publishing als JSON
- REST-API fuer andere Geraete, auch ohne MQTT-Server
- Firmware-Update per Browser (OTA), Neustart-Knopf
- Serielle Konsole in der Web-UI mit allen empfangenen Funkpaketen
- Netzwerk-Einstellungen (WLAN, Hostname, feste IP) in der Web-UI
- OLED-Anzeige direkt am Heltec-Board
- Berechnung des Taupunkts
- Regenwerte fuer Gesamt, 1 Stunde, 24 Stunden und 7 Tage
- Windrichtung mit Windrose im Webinterface
- Nicht-blockierender Empfang, damit das Webinterface fluessiger reagiert

## Hardware

Getestete Zielplattform:

- Heltec WiFi LoRa 32 V2
- Onboard SX1276 LoRa Transceiver
- Onboard OLED Display

Der Sketch ist fuer das Arduino-Boardprofil `esp32:esp32:heltec_wifi_lora_32_V2` vorgesehen.

## Benoetigte Arduino-Libraries

Installiere diese Libraries in der Arduino IDE:

- `BresserWeatherSensorReceiver`
- `RadioLib`
- `WiFiManager`
- `PubSubClient`
- `ArduinoJson`
- `ESP8266 and ESP32 OLED driver for SSD1306 displays` / `SSD1306Wire`

Zusaetzlich wird das ESP32 Boardpaket fuer Arduino benoetigt.

## Handbuch

Ausfuehrliches Handbuch mit Screenshots der Web-Oberflaeche als PDF:
[Deutsch](docs/Bresser5in1-Handbuch-de.pdf) | [English](docs/Bresser5in1-Manual-en.pdf)
(die Quellen liegen in `docs/manual/`, die Screenshots in `docs/screenshots/`).

## Installation

Fertige Firmware (siehe [Releases](https://github.com/okoebernik/Bresser5in1/releases)) und eine Schritt-fuer-Schritt-Anleitung
fuer die **Erstinstallation** (Komplett-Image, WLAN-Einrichtung, erste Schritte) stehen in [INSTALLATION.md](INSTALLATION.md). Die Kurzfassung fuer den Quellcode:

1. Den Ordner `Bresser5in1_Merged` in den Arduino-Sketchbook-Ordner kopieren oder direkt in der Arduino IDE oeffnen.
2. In der Arduino IDE das Board `Heltec WiFi LoRa 32(V2)` auswaehlen.
3. Die benoetigten Libraries installieren.
4. Sketch kompilieren und auf das Heltec-Board hochladen.
5. Nach dem Start verbindet sich das Geraet mit dem gespeicherten WLAN oder startet bei Bedarf ein WiFiManager-Portal.

## WLAN-Einrichtung

Beim ersten Start oder wenn keine WLAN-Daten vorhanden sind, startet WiFiManager einen Access Point mit dem Namen:

```text
BresserHeltec
```

Darueber kann das WLAN eingerichtet werden. Danach ist das Webinterface ueber die IP-Adresse erreichbar, die im seriellen Monitor angezeigt wird.

Wenn beim Start die Boot-Taste gedrueckt wird, wird das WiFiManager-Portal ebenfalls erzwungen.

## Webinterface

Das Webinterface ist eine Single-Page-App im Design der uebrigen Geraete (Statusleiste, Sidebar,
Englisch/Deutsch umschaltbar) mit den Seiten Wetter, Sensor, Netzwerk, MQTT, Konsole und System
(Firmware-Update per Browser, Geraete-Neustart, OTA-Passwort).

- `/` - Web-UI
- `/data` - aktuelle Wetterdaten als JSON (intern fuer die Web-UI)
- `/sensors` - erkannte Sensor-IDs als JSON
- `/save` - Speichern der Sensor-/MQTT-Einstellungen per POST

Aenderungen am Geraet (Netzwerk, Firmware-Update, Neustart, OTA-Passwort) sind per HTTP-Basic-Auth
geschuetzt: Benutzer `admin`, Passwort = OTA-Passwort (Standard `bresser`, bitte aendern).

## REST-API

Nur lesend, ohne Login, mit `Access-Control-Allow-Origin: *`. Gleiches Muster wie beim Clinometer
(`GET /api/<geraet>/values`, flaches JSON, Einheit im Feldnamen, `stale`-Flag).

`GET /api/weather/values`

```json
{
  "valid": true,
  "stale": false,
  "sensor_id": "44125371",
  "age_s": 8,
  "temperature_c": 12.4,
  "humidity_pct": 95,
  "dew_point_c": 11.6,
  "wind_speed_ms": 0.0,
  "wind_gust_ms": 0.0,
  "wind_dir_deg": 324,
  "wind_dir_text": "NNW",
  "rain_total_mm": 113.6,
  "rain_1h_mm": 0.0,
  "rain_24h_mm": 0.0,
  "rain_7d_mm": 0.0,
  "battery_ok": true,
  "rssi_dbm": -106.5
}
```

- `stale` ist `true`, wenn noch keine Daten vorliegen oder das letzte Paket aelter als 120 s ist.
- Ohne empfangene Daten enthaelt die Antwort nur `valid: false` und `stale: true`.
- Die Regenfenster (`rain_1h/24h/7d_mm`) werden im RAM gefuehrt und beginnen nach einem Neustart bei 0.

`GET /api/status` liefert Firmware-Build, Uptime, freien Heap, WLAN/MQTT-Status, aktive Sensor-ID
und `last_packet_age_s` (-1 = noch kein Paket empfangen).

Weitere Endpunkte: `GET /api/config`, `GET /api/network`, `GET /api/log?since=<pos>` (Konsole,
Position im Header `X-Log-Next`).

## Dashboard-Werte

Das Dashboard zeigt:

- Sensor-ID
- WLAN-Status/IP-Adresse
- MQTT-Status
- Letztes Update
- Temperatur
- Luftfeuchte
- Taupunkt
- Windgeschwindigkeit
- Windboe
- Windrichtung mit Windrose
- Regen gesamt
- Regen 1 Stunde
- Regen 24 Stunden
- Regen 7 Tage
- Batterie-Status
- RSSI

## Sensor-ID-Auswahl

Wenn mehrere Wetterstationen in der Umgebung aktiv sind, koennen auf der Seite *Sensor* erkannte Sensor-IDs ausgewaehlt werden.

Vorgehen:

1. Filter zunaechst deaktivieren.
2. Einige Empfangszyklen warten, bis Sensoren erkannt wurden.
3. Auf der Seite *Sensor* die gewuenschte Sensor-ID im Pulldown auswaehlen.
4. `Nur ausgewaehlte Sensor-ID verwenden` aktivieren.
5. Speichern.

Dashboard, OLED und Regenwerte gehoeren immer zu **einem** Sensor: der gespeicherten Sensor-ID, sonst der zuerst
empfangenen Station. Weitere Stationen werden nur per MQTT veroeffentlicht (ohne Regen-Zeitfenster).

Wichtig: Wenn der Filter aktiv ist, werden fremde Sensoren bereits auf Ebene der Empfaenger-Library blockiert. Dadurch erscheinen neue/fremde Sensoren dann nicht mehr in der Liste. Zum Suchen weiterer Sensoren den Filter kurz deaktivieren.

## MQTT

MQTT kann auf der Seite *MQTT* aktiviert und konfiguriert werden. Das Passwort wird nie an den Browser zurueckgegeben (leeres Feld = unveraendert).

Einstellungen:

- MQTT Host
- Port
- Benutzername
- Passwort
- Base Topic

Standard-Base-Topic:

```text
bresser
```

Publiziert wird pro Sensor unter:

```text
<baseTopic>/data/<SensorID>
```

Beispiel:

```text
bresser/data/00A1B2C3
```

Payload-Beispiel:

```json
{
  "id": 10597059,
  "id_hex": "a1b2c3",
  "temp_c": 22.4,
  "humidity": 58,
  "dew_point_c": 13.7,
  "wind_speed_ms": 1.2,
  "wind_gust_ms": 2.8,
  "wind_dir_deg": 225,
  "wind_dir_text": "SW",
  "rain_mm": 12.3,
  "rain_hour_mm": 0.0,
  "rain_day_mm": 1.2,
  "rain_week_mm": 7.8,
  "battery_ok": true,
  "rssi": -78.5,
  "uptime_s": 3600
}
```

Der MQTT-Puffer ist im Sketch auf `1024` Byte gesetzt. Falls ein Publish fehlschlaegt, wird eine Diagnosemeldung auf der seriellen Konsole ausgegeben.

## JSON-Endpunkt `/data`

Der Endpunkt `/data` liefert die aktuellen Dashboard-Daten. Wichtige Felder:

- `valid` - true, wenn bereits Wetterdaten empfangen wurden
- `wifi_connected`
- `ip`
- `mqtt_status`
- `sensor_filter_enabled`
- `selected_sensor_id`
- `selected_sensor_id_hex`
- `id`
- `id_hex`
- `temp_c`
- `humidity`
- `dew_point_c`
- `wind_speed_ms`
- `wind_gust_ms`
- `wind_dir_deg`
- `wind_dir_text`
- `rain_mm`
- `rain_hour_mm`
- `rain_day_mm`
- `rain_week_mm`
- `battery_ok`
- `rssi`
- `age_s`
- `uptime_s`

## JSON-Endpunkt `/sensors`

Der Endpunkt `/sensors` liefert die Liste der zuletzt erkannten Sensoren:

```json
{
  "filter_enabled": false,
  "selected_id": 10597059,
  "selected_id_hex": "00A1B2C3",
  "sensors": [
    {
      "id": 10597059,
      "id_hex": "00A1B2C3",
      "rssi": -78.5,
      "age_s": 12,
      "selected": true
    }
  ]
}
```

## Regenberechnung

Die Bresser-Station liefert einen fortlaufenden Gesamt-Regenzaehler. Der Sketch speichert regelmaessig Messpunkte und berechnet daraus:

- Regen 1 Stunde
- Regen 24 Stunden
- Regen 7 Tage

Bei schwachem Funksignal werden einzelne Pakete fehlerhaft dekodiert. Ein Sprung des Gesamtzaehlers um mehr als 5 mm pro
Paket (oder ein Rueckgang) gilt deshalb erst als echt, wenn das naechste Paket denselben Wert bestaetigt. Wird der Zaehler der
Station zurueckgesetzt, zaehlt der Regen seit dem Reset weiter. Die Fenster liegen im RAM und beginnen nach einem Neustart bei 0.

## Taupunkt

Der Taupunkt wird aus Temperatur und relativer Luftfeuchte berechnet. Die Anzeige erfolgt im Dashboard, im OLED und im MQTT-Payload.

## OLED-Anzeige

Das OLED zeigt kompakt:

- Sensor-ID
- Temperatur
- Luftfeuchte
- Taupunkt
- Wind
- Regen gesamt
- Regen 1 Stunde und 24 Stunden

## Serielle Diagnose

Die gleiche Ausgabe steht live auf der Seite *Konsole* der Web-UI (mit Uptime-Zeitstempel je Zeile).

Serielle Schnittstelle:

```text
115200 Baud
```

Nuetzliche Meldungen:

- Startstatus
- WLAN-IP-Adresse
- WeatherSensor Init OK/FAIL
- aktiver Sensorfilter
- empfangene Sensordaten
- MQTT-Verbindungsstatus
- MQTT-Publish-Fehler mit Payloadgroesse und Statuscode

## Fehlersuche

### Keine Wetterdaten

- Pruefen, ob das richtige Boardprofil verwendet wird.
- Antenne und Empfangsposition pruefen.
- Sensorfilter auf der Seite *Sensor* testweise deaktivieren.
- Auf der Seite *Konsole* RSSI und Meldungen "kein Paket" pruefen (schlechter als etwa -105 dBm ist grenzwertig).
- Seriellen Monitor auf `WeatherSensor init failed` pruefen.

### Falsche Station wird angezeigt

- Filter deaktivieren und warten, bis alle Sensoren im Pulldown erscheinen.
- Gewuenschte ID auswaehlen.
- `Nur ausgewaehlte Sensor-ID verwenden` aktivieren.
- Speichern.

### MQTT verbindet, aber es kommen keine Daten an

- MQTT auf der Seite *MQTT* aktiviert?
- Host, Port, Benutzername und Passwort pruefen.
- Base Topic pruefen.
- Seriellen Monitor auf `MQTT publish failed` pruefen.
- Broker pruefen, z. B. mit `mosquitto_sub`.

Beispiel:

```bash
mosquitto_sub -h <broker-ip> -t 'bresser/#' -v
```

### Webinterface reagiert langsam

Der Sketch nutzt nicht-blockierendes Polling fuer den Wetterempfang. Falls es trotzdem traege wirkt:

- WLAN-Signal pruefen.
- Browser-Cache neu laden.
- Serielle Konsole auf wiederholte Fehler pruefen.

## Projektdateien

- `Bresser5in1_Merged/Bresser5in1_Merged.ino` - Arduino-Sketch
- `Bresser5in1_Merged/web_ui.h` - Web-UI (HTML/CSS/JS, eingebettet)
- `README.md` / `README.en.md` - diese Dokumentation (Deutsch/Englisch)
- `INSTALLATION.md` / `INSTALLATION.en.md` - Erstinstallation (Deutsch/Englisch)
- `docs/` - Handbuch als PDF (Deutsch/Englisch), Quellen (`manual/`), Screenshots und das Skript `tools/make_screenshots.js`

## Hinweise

Die Sensor-ID wird hexadezimal angezeigt und gespeichert. Eingaben wie `00A1B2C3` oder `0x00A1B2C3` werden akzeptiert.

Konfigurationen werden im ESP32-Preferences-Speicher abgelegt und bleiben nach Neustart erhalten.
