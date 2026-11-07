# Overdracht — ECO-boiler-controller (ESP32C6_ECO-boiler)

*Stand 7 november 2026 — Filip Delannoy (FiDel), Zarlardinge (BE)*

Dit document start een nieuw gesprek met Claude over de ECO-boiler-controller: zonnecollector-pomp en warmte-inhoud van de 490 L-boiler. Alle details: [`README.md`](../README.md).

**Werkwijze en afspraken:** zie de [overkoepelende overdracht](https://github.com/FidelDworp/Zarlardinge-Home-Automation/blob/main/docs/ZARLAR_OVERDRACHT.md). Kort: Nederlands · nooit een sketch of script zonder GO · eerst een plan, één stap per versie (versiekop + changelog) en daarna een 48 u-test · alles via OTA · `/json`-keys en bedieningsadressen niet wijzigen zonder akkoord · README en deze overdracht mee bijwerken · na GO compileert Claude en zet de `.bin` in `bin/`.

---

## 1. Wat en waar

| | |
|---|---|
| Adres | `http://192.168.0.71` (vast IP, nog geen mDNS) |
| Sketch | `ESP32-C6_ECO-boiler/ESP32-C6_ECO-boiler.ino` **v2.0** (3/11/2026), zonder Matter |
| Klaar om te uploaden | `bin/ESP32-C6_ECO-boiler_v2.0.ino.bin` |
| Hardware | 6× DS18B20 (GPIO 3), pomprelais (GPIO 1), pomp-PWM (GPIO 5), MAX31865 PT1000 collector (SPI 20–23) |
| Bibliotheken | OneWireNg, Adafruit MAX31865, ESPAsyncWebServer |
| Sheet | S-ECO `1nn_GBmLhS9zZhp5On_3r-LRLonetSjnHCuHZiNOc4Q0` (via het Dashboard) |
| Systeemdocument | Master-document §5 (`Zarlardinge-Home-Automation/docs/`) |

## 2. Stand (7/11)

- v2.0 in productie. 48 u-test liep af op 5/11 22:15 (op 5/11 20:00: 45,8 u zonder herstart, grootste blok 251 KB): resultaat nog in de README noteren.
- Compileert in de sessie met core 3.3.12 (1,36 MB).

## 3. Open punten

1. Resultaat 48 u-test v2.0 in de README zetten.
2. **v2.1:** dode `publishHVAC()` weg (post naar `/eco_energy`, dat de HVAC niet heeft) en mDNS (`eco.local`).

## 4. Praktisch

- Compileren: ESP32C6 Dev Module, Flash 16 MB, Partition Custom, USB CDC On Boot Enabled.
- OTA: enkel `ESP32-C6_ECO-boiler.ino.bin` via `/update`.
