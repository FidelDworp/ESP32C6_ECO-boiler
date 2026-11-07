# ECO-boiler controller (ESP32-C6)

Stuurt de zonnecollector-pomp van de ECO-boiler (490 L) in Zarlardinge en meet hoeveel warmte er in de boiler zit. Bereikbaar op **http://192.168.0.71** (vast IP, geen mDNS).

Huidige versie: **v2.0 (3 nov 2026) — zonder Matter.** Apple Home loopt via de RPi-bridge (zie §6).

---

## 1. Wat doet hij?

- **Meet** de collector op het dak (PT1000 via MAX31865) en 6 lagen in de boiler (DS18B20).
- **Berekent** de opgeslagen energie boven 35 °C (`EQtot`, kWh) en de winst per 10 min (`dEQ`).
- **Stuurt** de OEG-pomp (230 V-relais + PWM-snelheid) als de collector warmer is dan de boiler.
- **Toont** alles op een eigen webpagina, en geeft `/json` door aan de Dashboard-controller (.60), die logt in Google Sheets.

De boiler krijgt ook warmte van 2 JIDE-haarden (eigen pompen, niet via deze controller).

## 2. Hardware

| Onderdeel | Aansluiting ESP32-C6 |
|---|---|
| 6× DS18B20 (ETopH, ETopL, EMidH, EMidL, EBotH, EBotL) | GPIO 3 (1-Wire, 4,7 kΩ pull-up) |
| Pomprelais 230 V | GPIO 1 |
| Pomp-PWM (OEG, 1 kHz, 8 bit) | GPIO 5 |
| MAX31865 (PT-sensor collector) | CS 20, MOSI 21, MISO 22, SCK 23 |

- PT-sensor instelbaar in `/settings`: **PT1000** (dakcollector) of PT100 (testbord).
- Boilerzones: 110 / 90 / 90 / 90 / 110 L, telkens tussen twee sensoren.
- De pomp start pas vanaf ongeveer PWM 80.

## 3. Pomplogica (elke minuut)

`dT = Tsun − EBotH`

| Voorwaarde | Pomp |
|---|---|
| Buiten 07:00–21:00 | uit |
| Pomp uit en dT < 3 °C, of pomp aan en dT < 2 °C | uit |
| dT > 3 °C maar collector < 22 °C (thermosifon) | uit |
| 3× na elkaar geen energiewinst (`dEQ ≤ 0`) | uit |
| Collector ≥ 90 °C (oververhitting) | **altijd aan**, PWM 255 |
| Anders | aan |

Snelheid: dT 3 → 20 °C geeft PWM 80 → 200. Collector > 75 °C: PWM 180. Alle grenzen staan in `/settings` (bewaard in NVS).

**Handmatig** (webpagina, Apple Home of de URL's hieronder): pomp 60 s geforceerd aan (PWM 200) of uit, daarna terug automatisch.

## 4. Webpagina's en endpoints

| URL | Wat |
|---|---|
| `/` | Status: temperatuurbalken, EQtot, pomp, systeem |
| `/charts` | 3 grafieken (laatste 60 min) |
| `/settings` | WiFi, drempels, volumes, PT-type, sensornamen, logboek, crash-log |
| `/update` | OTA (zie §7) |
| `/json` | Alle waarden (zie §5) |
| `/pump_override_on` · `_off` · `_cancel` | Pomp 60 s aan / 60 s uit / terug automatisch |

Serieel (115200): `status`, `reset-all`, `t` (sensoren), `p` (pomptest), `w` (WiFi), `h` (hulp).

## 5. JSON (`/json`)

| Key | Waarde | Key | Waarde |
|---|---|---|---|
| a | uptime (s) | k | opbrengst vandaag (kWh) |
| b, c | ETopH, ETopL (°C) | l | Tsun collector (°C) |
| d, e | EMidH, EMidL (°C) | m | dT (°C) |
| f, g | EBotH, EBotL (°C) | n | PWM (0-255) |
| h | EAv gemiddeld (°C) | o | pomprelais (0/1) |
| i | EQtot (kWh) | p | RSSI (dBm) |
| j | dEQ (kWh / 10 min) | q, r, s | vrije heap %, grootste blok KB, minimum heap KB |

Gebruikt door: Dashboard (.60 → Google Sheets via `GoogleScript/ECO_GoogleScript.gs`), HVAC (.70) en de RPi-portal.

## 6. Apple Home (via de RPi)

Sinds v2.0 zit er geen Matter meer op de ESP32 (dat kostte ±60 KB heap). Het toestel **"ECO-boiler"** in de woning Zarlardinge komt van de RPi-bridge: `server.js` ≥ v2.11 + plugin `matterbridge-zarlar` ≥ v0.8.

| Onderdeel | Tegel | Bron |
|---|---|---|
| Collector | temperatuur | `l` |
| Boiler boven | temperatuur | `b` |
| Boiler onder | temperatuur | `g` |
| Vulling | vochtigheid % | `i` ÷ 25 kWh |
| Pomp | stopcontact | `o`; aan = 60 s geforceerd, uit = automatisch |

Details: Portal Plan §16.13.

## 7. Compileren en OTA

- Board **ESP32C6 Dev Module**, Flash **16 MB**, Partition **Custom** met `partitions.csv` in de sketchmap (app0 + app1 elk 6 MB).
- Libraries: ESPAsyncWebServer, Adafruit_MAX31865, OneWireNg.
- **OTA:** Arduino IDE → *Sketch → Export Compiled Binary*. In `build/esp32.esp32.esp32c6/` alleen **`ESP32-C6_ECO-boiler.ino.bin`** kiezen (niet bootloader, partitions of merged) → `http://192.168.0.71/update`.
- Eerste flash van v2.0 mag via OTA: de partitietabel blijft dezelfde.

## 8. Mappen

```
README.md                          dit document
ESP32-C6_ECO-boiler/               de sketch: deze map openen in de Arduino IDE
  ESP32-C6_ECO-boiler.ino          sketch v2.0
  partitions.csv                   partitietabel 16 MB (wordt automatisch gebruikt)
bin/
  ESP32-C6_ECO-boiler_v2.0.ino.bin klaar om te uploaden via `/update`
GoogleScript/
  ECO_GoogleScript.gs              Google Sheets-logger (v2)
old/
  S-ECO_SOLAR_Photon.cpp           oude Photon-firmware (tot jan 2026)
```

## 9. Geschiedenis (kort)

| Versie | Datum | Wat |
|---|---|---|
| Photon | 2020–2025 | Eigen controller ter vervanging van de OEG-regelaar; WiFi onstabiel |
| v1.3–v1.7 | jan 2026 | Eerste ESP32-C6: webpagina, grafieken, WiFi altijd wakker (geen power save) |
| v1.13–v1.17 | jan–feb 2026 | Productie, logging, vast IP 192.168.0.71 |
| v1.18 | 1 mrt 2026 | Matter (Apple Home) |
| v1.21 | 11 mrt 2026 | Google Sheets-logging naar de Dashboard verhuisd |
| v1.22 | 13 mrt 2026 | Compacte JSON (a–s), heap-bewaking + crash-log, `/charts`, mDNS weg |
| v1.23 | 22 mrt 2026 | PT100/PT1000 instelbaar |
| **v2.0** | 3 nov 2026 | **Matter weg**; Apple Home via de RPi-bridge |

## 10. Open punten

- **48 u-test v2.0:** heap (`q`, `r`, `s`) en herstarts noteren; verwacht een grootste blok rond 200 KB of meer, zoals bij HVAC v2.0.
- `publishHVAC()` post naar `http://<hvac_ip>/eco_energy`, maar de HVAC heeft dat endpoint niet (die leest zelf `/json`). Opruimen in een volgende versie.
- mDNS kan terug nu Matter weg is.
