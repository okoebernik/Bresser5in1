# Bresser 5-in-1 LoRa/Wetterempfaenger

Arduino-Sketch fuer ein Heltec WiFi LoRa 32 V2 Board zum Empfangen einer Bresser 5-in-1 Wetterstation. Das Programm liest die Wetterdaten auf der europaeischen Frequenz im 868-MHz-Band ein, zeigt sie auf dem OLED und im Webinterface an und kann die Werte per MQTT veroeffentlichen.

## Funktionen

- Empfang von Bresser 5-in-1 Wetterdaten ueber `BresserWeatherSensorReceiver`
- Modernes Web-Dashboard mit Live-Werten
- Konfigurationsseite fuer MQTT und Sensor-Auswahl
- Auswahl einer bestimmten Sensor-ID, wenn mehrere Wetterstationen in der Umgebung aktiv sind
- Anzeige erkannter Sensor-IDs im Pulldown-Menue
- MQTT-Publishing als JSON
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

## Installation

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

Das Webinterface stellt zwei Hauptseiten bereit:

- `/` - Dashboard mit Live-Wetterdaten
- `/config` - Einstellungen fuer Sensorfilter und MQTT

Weitere Endpunkte:

- `/data` - aktuelle Wetterdaten als JSON
- `/sensors` - erkannte Sensor-IDs als JSON
- `/save` - Speichern der Einstellungen per POST

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

Wenn mehrere Wetterstationen in der Umgebung aktiv sind, koennen im Config-Menue erkannte Sensor-IDs ausgewaehlt werden.

Vorgehen:

1. Filter zunaechst deaktivieren.
2. Einige Empfangszyklen warten, bis Sensoren erkannt wurden.
3. In `/config` die gewuenschte Sensor-ID im Pulldown auswaehlen.
4. `Nur ausgewaehlte Sensor-ID verwenden` aktivieren.
5. Speichern.

Wichtig: Wenn der Filter aktiv ist, werden fremde Sensoren bereits auf Ebene der Empfaenger-Library blockiert. Dadurch erscheinen neue/fremde Sensoren dann nicht mehr in der Liste. Zum Suchen weiterer Sensoren den Filter kurz deaktivieren.

## MQTT

MQTT kann in `/config` aktiviert und konfiguriert werden.

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

Wenn der Regenzaehler der Station zurueckgesetzt wird, behandelt der Sketch den neuen Wert als aktuellen Basiswert.

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
- Sensorfilter in `/config` testweise deaktivieren.
- Seriellen Monitor auf `WeatherSensor init failed` pruefen.

### Falsche Station wird angezeigt

- Filter deaktivieren und warten, bis alle Sensoren im Pulldown erscheinen.
- Gewuenschte ID auswaehlen.
- `Nur ausgewaehlte Sensor-ID verwenden` aktivieren.
- Speichern.

### MQTT verbindet, aber es kommen keine Daten an

- MQTT in `/config` aktiviert?
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

- `Bresser5in1_Merged.ino` - Arduino-Sketch
- `README.md` - diese Dokumentation

## Hinweise

Die Sensor-ID wird hexadezimal angezeigt und gespeichert. Eingaben wie `00A1B2C3` oder `0x00A1B2C3` werden akzeptiert.

Konfigurationen werden im ESP32-Preferences-Speicher abgelegt und bleiben nach Neustart erhalten.
