# Bresser 5-in-1 LoRa Weather Receiver

*[Deutsche Version](README.md)*

Arduino sketch for a Heltec WiFi LoRa 32 V2 board that receives a Bresser 5-in-1 weather station. It reads the weather data on the European 868 MHz band, shows it on the OLED and in the web interface, and can publish the values over MQTT.

## Features

- Reception of Bresser 5-in-1 weather data through `BresserWeatherSensorReceiver`
- Modern web dashboard with live values (English/German)
- Pages for sensor selection, network, MQTT, console and system
- Selection of a specific sensor ID when several weather stations are active nearby
- Detected sensor IDs shown in a drop-down menu
- MQTT publishing as JSON
- REST API for other devices, also without an MQTT server
- Over-the-air firmware update from the browser, restart button
- Serial console in the web UI showing every received radio packet
- Network settings (WiFi, hostname, static IP) in the web UI
- OLED display directly on the Heltec board
- Dew point calculation
- Rain values for total, 1 hour, 24 hours and 7 days
- Wind direction with a wind rose in the web interface
- Non-blocking reception, so the web interface stays responsive

## Hardware

Tested target platform:

- Heltec WiFi LoRa 32 V2
- Onboard SX1276 LoRa transceiver
- Onboard OLED display

The sketch is intended for the Arduino board profile `esp32:esp32:heltec_wifi_lora_32_V2`.

## Required Arduino libraries

Install these libraries in the Arduino IDE:

- `BresserWeatherSensorReceiver`
- `RadioLib`
- `WiFiManager`
- `PubSubClient`
- `ArduinoJson`
- `ESP8266 and ESP32 OLED driver for SSD1306 displays` / `SSD1306Wire`

The ESP32 board package for Arduino is also required.

## Manual

Detailed manual with screenshots of the web interface as PDF:
[English](docs/Bresser5in1-Manual-en.pdf) | [Deutsch](docs/Bresser5in1-Handbuch-de.pdf)
(sources are in `docs/manual/`, screenshots in `docs/screenshots/`).

## Installation

Ready-made firmware (see [Releases](https://github.com/okoebernik/Bresser5in1/releases)) and a step-by-step guide for the
**first installation** (full flash image, WiFi setup, first steps) are in [INSTALLATION.en.md](INSTALLATION.en.md).
Short version for building from source:

1. Copy the `Bresser5in1_Merged` folder into your Arduino sketchbook folder, or open it directly in the Arduino IDE.
2. Select the board `Heltec WiFi LoRa 32(V2)` in the Arduino IDE.
3. Install the required libraries.
4. Compile the sketch and upload it to the Heltec board.
5. After startup the device connects to the saved WiFi or starts a WiFiManager portal if needed.

## WiFi setup

On first start, or when no WiFi credentials are stored, WiFiManager starts an access point named:

```text
BresserHeltec
```

Use it to set up the WiFi. Afterwards the web interface is reachable at the IP address shown on the OLED and in the serial monitor.

If the boot button is held during startup, the WiFiManager portal is forced as well.

## Web interface

The web interface is a single-page app in the same design as the other devices (status bar, sidebar, switchable
English/German) with the pages Weather, Sensor, Network, MQTT, Console and System (firmware update from the browser,
device restart, OTA password).

- `/` - web UI
- `/data` - current weather data as JSON (internal, used by the web UI)
- `/sensors` - detected sensor IDs as JSON
- `/save` - save the sensor/MQTT settings via POST

Changes to the device itself (network, firmware update, restart, OTA password) are protected with HTTP Basic Auth:
user `admin`, password = OTA password (default `bresser`, please change it).

## REST API

Read-only, no login, sent with `Access-Control-Allow-Origin: *`. Same pattern as the Clinometer
(`GET /api/<device>/values`, flat JSON, unit in the field name, `stale` flag).

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

- `stale` is `true` when no data has been received yet or the last packet is older than 120 s.
- Without received data the response only contains `valid: false` and `stale: true`.
- The rain windows (`rain_1h/24h/7d_mm`) are kept in RAM and start at 0 after a restart.

`GET /api/status` returns the firmware build, uptime, free heap, WiFi/MQTT status, the active sensor ID and
`last_packet_age_s` (-1 = no packet received yet).

Further endpoints: `GET /api/config`, `GET /api/network`, `GET /api/log?since=<pos>` (console, position in the
`X-Log-Next` header).

## Dashboard values

The dashboard shows:

- Sensor ID
- WiFi status/IP address
- MQTT status
- Last update
- Temperature
- Humidity
- Dew point
- Wind speed
- Wind gust
- Wind direction with wind rose
- Rain total
- Rain 1 hour
- Rain 24 hours
- Rain 7 days
- Battery status
- RSSI

## Sensor ID selection

When several weather stations are active nearby, detected sensor IDs can be selected on the *Sensor* page.

Procedure:

1. Disable the filter first.
2. Wait a few reception cycles until sensors have been detected.
3. Pick the desired sensor ID in the drop-down on the *Sensor* page.
4. Enable `Use only the selected sensor ID`.
5. Save.

The dashboard, OLED and rain values always belong to **one** sensor: the saved sensor ID, otherwise the station
received first. Other stations are only published over MQTT (without rain time windows).

Important: while the filter is active, foreign sensors are already blocked at the receiver library level. New or foreign
sensors therefore no longer show up in the list. Disable the filter briefly to search for further sensors.

## MQTT

MQTT can be enabled and configured on the *MQTT* page. The password is never returned to the browser (empty field = unchanged).

Settings:

- MQTT host
- Port
- User name
- Password
- Base topic

Default base topic:

```text
bresser
```

Each sensor is published under:

```text
<baseTopic>/data/<SensorID>
```

Example:

```text
bresser/data/00A1B2C3
```

Payload example:

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

The MQTT buffer is set to `1024` bytes in the sketch. If a publish fails, a diagnostic message is printed on the serial console.

## JSON endpoint `/data`

The `/data` endpoint returns the current dashboard data. Important fields:

- `valid` - true once weather data has been received
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

## JSON endpoint `/sensors`

The `/sensors` endpoint returns the list of recently detected sensors:

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

## Rain calculation

The Bresser station reports a continuous total rain counter. The sketch stores measurement points regularly and derives:

- Rain 1 hour
- Rain 24 hours
- Rain 7 days

With a weak radio signal, individual packets are decoded incorrectly. A jump of the total counter by more than 5 mm per
packet (or a decrease) is therefore only accepted once the next packet confirms the same value. If the station's counter is
reset, rain keeps being counted from the reset. The windows live in RAM and start at 0 after a restart.

## Dew point

The dew point is calculated from temperature and relative humidity. It is shown on the dashboard, on the OLED and in the MQTT payload.

## OLED display

The OLED compactly shows:

- Sensor ID
- Temperature
- Humidity
- Dew point
- Wind
- Rain total
- Rain 1 hour and 24 hours

## Serial diagnostics

The same output is shown live on the *Console* page of the web UI (with an uptime timestamp per line).

Serial interface:

```text
115200 baud
```

Useful messages:

- Startup status
- WiFi IP address
- WeatherSensor init OK/FAIL
- Active sensor filter
- Received sensor data
- MQTT connection status
- MQTT publish errors with payload size and status code

## Troubleshooting

### No weather data

- Check that the correct board profile is used.
- Check the antenna and the receiving position.
- Temporarily disable the sensor filter on the *Sensor* page.
- On the *Console* page check the RSSI and "no packet" messages (worse than about -105 dBm is marginal).
- Check the serial monitor for `WeatherSensor init failed`.

### Wrong station is shown

- Disable the filter and wait until all sensors appear in the drop-down.
- Select the desired ID.
- Enable `Use only the selected sensor ID`.
- Save.

### MQTT connects but no data arrives

- Is MQTT enabled on the *MQTT* page?
- Check host, port, user name and password.
- Check the base topic.
- Check the serial monitor for `MQTT publish failed`.
- Check the broker, for example with `mosquitto_sub`.

Example:

```bash
mosquitto_sub -h <broker-ip> -t 'bresser/#' -v
```

### Web interface is slow

The sketch uses non-blocking polling for weather reception. If it still feels sluggish:

- Check the WiFi signal.
- Reload the browser cache.
- Check the serial console for repeated errors.

## Project files

- `Bresser5in1_Merged/Bresser5in1_Merged.ino` - Arduino sketch
- `Bresser5in1_Merged/web_ui.h` - web UI (HTML/CSS/JS, embedded)
- `README.md` / `README.en.md` - this documentation (German/English)
- `INSTALLATION.md` / `INSTALLATION.en.md` - first installation (German/English)
- `docs/` - manual as PDF (German/English), sources (`manual/`), screenshots and the script `tools/make_screenshots.js`

## Notes

The sensor ID is displayed and stored in hexadecimal. Inputs such as `00A1B2C3` or `0x00A1B2C3` are accepted.

Configuration is stored in ESP32 Preferences memory and survives a restart.
