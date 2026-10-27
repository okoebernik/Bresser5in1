# First installation

*[Deutsche Version](INSTALLATION.md)*

Guide to getting the Bresser 5-in-1 receiver onto a **new or blank Heltec WiFi LoRa 32 V2**.
If a version with OTA is already running on the device, an update through the web UI is enough (see below).

## What you need

- Heltec WiFi LoRa 32 **V2** (SX1276, 868 MHz variant) with a **868 MHz antenna attached**
- USB cable **with data lines** (charge-only cables are not detected)
- Bresser 5-in-1 weather station within range
- Windows: if needed the **CP210x** USB driver (Silicon Labs), so that a COM port appears

## Which files are available

Both files are in the [release](https://github.com/okoebernik/Bresser5in1/releases):

| File | Purpose |
|---|---|
| `Bresser5in1-<version>-erstinstallation.bin` | **Full flash image** (bootloader, partition table, firmware, 8 MB). Written at 0x0. For the first installation. |
| `Bresser5in1-<version>.bin` | Firmware only. For later **OTA updates** through the web UI. |

> The full image is built for boards with **8 MB flash** (this is how the Arduino board profile
> `Heltec WiFi LoRa 32(V2)` is defined). If your board only has 4 MB, please use way C.

> The full image erases the configuration storage. WiFi credentials, MQTT settings and the OTA password
> are gone afterwards and have to be set up again.

## Way A: full image from the browser (no software to install)

1. Connect the device over USB.
2. Open <https://espressif.github.io/esptool-js/> with Chrome or Edge.
3. Set the baud rate to `921600`, click **Connect** and pick the device's COM port.
4. Enter `0x0` as *Flash Address* and choose the file `...-erstinstallation.bin`.
5. Click **Program** and wait until it finishes. Then restart the device with the **RST** button.

## Way B: full image with esptool

Requirement: Python with `pip install esptool`.

```bash
esptool --chip esp32 --port COM5 --baud 921600 write_flash 0x0 Bresser5in1-v1.0.0-erstinstallation.bin
```

Replace `COM5` with your own port (Linux/macOS: `/dev/ttyUSB0`, `/dev/cu.SLAB_USBtoUART`).
For a completely clean start run `esptool --chip esp32 --port COM5 erase_flash` once beforehand.
Older esptool versions are called `esptool.py` and use `write_flash`/`erase_flash` the same way.

## Way C: from source with the Arduino IDE

1. Install the Espressif **board package** `esp32` (tested with 3.3.10).
2. Install the **libraries** (tested versions in brackets):
   - `BresserWeatherSensorReceiver` (0.41.1)
   - `RadioLib` (7.7.1)
   - `WiFiManager` by tzapu (2.0.17)
   - `PubSubClient` (2.8)
   - `ArduinoJson` (7.4.3)
   - `ESP8266 and ESP32 OLED driver for SSD1306 displays` (4.6.2)
3. Open the `Bresser5in1_Merged` folder and select the board **Heltec WiFi LoRa 32(V2)**. The default
   partitioning (8 MB with two OTA slots) fits, do not change it.
4. Choose the COM port and click **Upload**.

With the Arduino CLI the same thing looks like this:

```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 Bresser5in1_Merged
arduino-cli upload  -p COM5 --fqbn esp32:esp32:heltec_wifi_lora_32_V2 Bresser5in1_Merged
```

## First start

1. After flashing, the device restarts. The OLED briefly shows `Bresser Receiver`, then `WS init OK`.
2. Without stored WiFi the device opens its own network **`BresserHeltec`** (no password).
3. Switch your phone or laptop to this network. The setup portal opens automatically,
   otherwise open <http://192.168.4.1> in the browser.
4. Choose **Configure WiFi**, select your own WiFi, enter the password and save.
5. The device connects and shows its **IP address on the OLED** (also in the serial monitor, 115200 baud).
6. Open `http://<IP>` in the browser. Alternatively `http://BresserHeltec.local`, if your system resolves mDNS.

The portal stays open for 5 minutes. If nothing is set up in that time, the device keeps running without WiFi
(reception and OLED work). To open the portal again, hold the **PRG button (GPIO0)** while powering on.

## Setup in the web UI

1. **System: change the OTA password.** The default password is `bresser` (user `admin`). It protects
   firmware update, restart and network changes.
2. **Sensor:** leave the filter off at first and wait a few minutes until your station appears in the list.
   Then choose the sensor ID, switch the filter on and save. This hides foreign stations.
3. **Console:** check whether packets arrive and how strong the signal is. Values worse than about **-105 dBm**
   are marginal; improve the antenna and location (closer to the station, elevated, clear line of sight, antenna vertical).
4. **MQTT** (optional): enter host, port, user, password and base topic, enable, save.
5. **Network** (optional): set a static IP or a different hostname.

The measurements are also available without MQTT as JSON: `GET /api/weather/values` (see README).

## Updates

New versions are installed **without a cable**: web UI, *System*, choose the file `Bresser5in1-<version>.bin`,
enter the OTA password, **Upload**. The device restarts by itself and keeps all settings.
The full image is **not** needed for this.

## Troubleshooting

| Problem | Solution |
|---|---|
| No COM port visible | Use a data cable, install the CP210x driver, try another USB port |
| Flashing fails / "Connecting..." hangs | Hold **PRG**, briefly press **RST**, release PRG, then flash again. Try a lower baud rate (`460800` or `115200`) |
| OLED stays dark | Press **RST** after flashing. The display is powered through GPIO21 (Vext) |
| `WeatherSensor init failed` in the serial monitor | Wrong board selected, or a hardware variant without SX1276/868 MHz |
| No weather data | Antenna connected? Closer to the station? Switch the filter off on the *Sensor* page in the web UI |
| Web UI not reachable | Read the IP from the OLED/router, check that you are on the same network. To set up the WiFi again, hold **PRG** at startup (opens the portal) |
| Reset everything to factory state | `esptool --chip esp32 --port COM5 erase_flash`, then write the full image again |
