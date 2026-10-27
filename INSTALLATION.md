# Erstinstallation

Anleitung, um den Bresser-5-in-1-Empfaenger auf ein **neues oder leeres Heltec WiFi LoRa 32 V2** zu bringen.
Wenn auf dem Geraet schon eine Version mit OTA laeuft, reicht ein Update ueber die Web-UI (siehe unten).

## Was du brauchst

- Heltec WiFi LoRa 32 **V2** (SX1276, 868-MHz-Variante) mit **angeschlossener 868-MHz-Antenne**
- USB-Kabel **mit Datenleitung** (Ladekabel ohne Daten werden nicht erkannt)
- Bresser 5-in-1 Wetterstation in Reichweite
- Windows: ggf. den USB-Treiber **CP210x** (Silicon Labs), damit ein COM-Port erscheint

## Welche Dateien gibt es

Beide Dateien liegen im [Release](https://github.com/okoebernik/Bresser5in1/releases):

| Datei | Zweck |
|---|---|
| `Bresser5in1-<version>-erstinstallation.bin` | **Komplettes Flash-Image** (Bootloader, Partitionstabelle, Firmware, 8 MB). Wird bei 0x0 geschrieben. Fuer die Erstinstallation. |
| `Bresser5in1-<version>.bin` | Nur die Firmware. Fuer spaetere **OTA-Updates** ueber die Web-UI. |

> Das Komplett-Image ist fuer Boards mit **8 MB Flash** gebaut (so ist das Arduino-Board-Profil
> `Heltec WiFi LoRa 32(V2)` definiert). Hat dein Board nur 4 MB, bitte Weg C verwenden.

> Das Komplett-Image loescht den Konfigurationsspeicher. WLAN-Zugang, MQTT-Einstellungen und das
> OTA-Passwort sind danach weg und muessen neu eingerichtet werden.

## Weg A: Komplett-Image per Browser (ohne Software-Installation)

1. Geraet per USB anschliessen.
2. Mit Chrome oder Edge die Seite <https://espressif.github.io/esptool-js/> oeffnen.
3. Baudrate `921600`, **Connect** klicken und den COM-Port des Geraets waehlen.
4. Bei *Flash Address* `0x0` eintragen und die Datei `...-erstinstallation.bin` waehlen.
5. **Program** klicken und warten, bis der Vorgang fertig ist. Danach das Geraet mit der **RST**-Taste neu starten.

## Weg B: Komplett-Image per esptool

Voraussetzung: Python mit `pip install esptool`.

```bash
esptool --chip esp32 --port COM5 --baud 921600 write_flash 0x0 Bresser5in1-v1.0.0-erstinstallation.bin
```

`COM5` durch den eigenen Port ersetzen (Linux/macOS: `/dev/ttyUSB0`, `/dev/cu.SLAB_USBtoUART`).
Fuer einen ganz sauberen Start vorher einmal `esptool --chip esp32 --port COM5 erase_flash` ausfuehren.
Aeltere esptool-Versionen heissen `esptool.py` und schreiben `write_flash`/`erase_flash` genauso.

## Weg C: Aus dem Quellcode mit der Arduino IDE

1. **Boardpaket** `esp32` von Espressif installieren (getestet mit 3.3.10).
2. **Libraries** installieren (getestete Versionen in Klammern):
   - `BresserWeatherSensorReceiver` (0.41.1)
   - `RadioLib` (7.7.1)
   - `WiFiManager` von tzapu (2.0.17)
   - `PubSubClient` (2.8)
   - `ArduinoJson` (7.4.3)
   - `ESP8266 and ESP32 OLED driver for SSD1306 displays` (4.6.2)
3. Ordner `Bresser5in1_Merged` oeffnen und als Board **Heltec WiFi LoRa 32(V2)** waehlen. Die
   Standard-Partitionierung (8 MB mit zwei OTA-Slots) passt, nichts umstellen.
4. Den COM-Port waehlen und **Hochladen** klicken.

Mit der Arduino-CLI geht das Ganze so:

```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 Bresser5in1_Merged
arduino-cli upload  -p COM5 --fqbn esp32:esp32:heltec_wifi_lora_32_V2 Bresser5in1_Merged
```

## Erste Inbetriebnahme

1. Nach dem Flashen startet das Geraet neu. Das OLED zeigt kurz `Bresser Receiver`, dann `WS init OK`.
2. Ohne gespeichertes WLAN oeffnet das Geraet ein eigenes Netz **`BresserHeltec`** (ohne Passwort).
3. Mit Handy oder Laptop in dieses Netz wechseln. Das Einrichtungsportal oeffnet sich automatisch,
   sonst im Browser <http://192.168.4.1> aufrufen.
4. **Configure WiFi** waehlen, das eigene WLAN auswaehlen, Passwort eingeben, speichern.
5. Das Geraet verbindet sich und zeigt seine **IP-Adresse auf dem OLED** an (auch im seriellen Monitor, 115200 Baud).
6. Im Browser `http://<IP>` aufrufen. Alternativ `http://BresserHeltec.local`, falls dein System mDNS aufloest.

Das Portal bleibt 5 Minuten offen. Wird in dieser Zeit nichts eingerichtet, laeuft das Geraet ohne WLAN weiter
(Empfang und OLED funktionieren). Zum erneuten Oeffnen des Portals beim Einschalten die **PRG-Taste (GPIO0)** gedrueckt halten.

## Einrichtung in der Web-UI

1. **System: OTA-Passwort aendern.** Das Standardpasswort ist `bresser` (Benutzer `admin`). Es schuetzt
   Firmware-Update, Neustart und Netzwerk-Aenderungen.
2. **Sensor:** Filter zunaechst aus lassen und einige Minuten warten, bis deine Station in der Liste erscheint.
   Dann die Sensor-ID waehlen, den Filter einschalten und speichern. So werden fremde Stationen ausgeblendet.
3. **Konsole:** Pruefen, ob Pakete ankommen und wie stark das Signal ist. Werte schlechter als etwa **-105 dBm**
   sind grenzwertig, dann Antenne und Standort verbessern (naeher an die Station, erhoeht, freie Sicht, Antenne senkrecht).
4. **MQTT** (optional): Host, Port, Benutzer, Passwort und Base Topic eintragen, aktivieren, speichern.
5. **Netzwerk** (optional): feste IP oder anderen Hostnamen einstellen.

Die Messwerte stehen ausserdem ohne MQTT als JSON bereit: `GET /api/weather/values` (siehe README).

## Updates

Neue Versionen werden **ohne Kabel** eingespielt: Web-UI, *System*, die Datei `Bresser5in1-<version>.bin` waehlen,
OTA-Passwort eingeben, **Hochladen**. Das Geraet startet danach selbst neu und behaelt alle Einstellungen.
Das Komplett-Image wird dafuer **nicht** gebraucht.

## Fehlersuche

| Problem | Loesung |
|---|---|
| Kein COM-Port sichtbar | Datenkabel verwenden, CP210x-Treiber installieren, anderen USB-Port probieren |
| Flashen schlaegt fehl / "Connecting..." haengt | **PRG** halten, dabei **RST** kurz druecken, PRG loslassen, dann erneut flashen. Niedrigere Baudrate (`460800` oder `115200`) versuchen |
| OLED bleibt dunkel | Nach dem Flashen **RST** druecken. Das Display wird ueber GPIO21 (Vext) versorgt |
| `WeatherSensor init failed` im seriellen Monitor | Falsches Board gewaehlt oder Hardware-Variante ohne SX1276/868 MHz |
| Keine Wetterdaten | Antenne angeschlossen? Naeher an die Station? In der Web-UI unter *Sensor* den Filter ausschalten |
| Web-UI nicht erreichbar | IP vom OLED/Router ablesen, gleiches Netz pruefen. Zum erneuten Einrichten des WLANs beim Start **PRG** halten (oeffnet das Portal) |
| Alles zurueck auf Werkszustand | `esptool --chip esp32 --port COM5 erase_flash`, dann das Komplett-Image neu schreiben |
