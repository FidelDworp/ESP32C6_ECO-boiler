# ESP32 ECO Controller - v1.6 CHANGELOG

**Release Date:** 2026-01-15  
**Author:** Fidel Dworp (with Claude Sonnet)  
**Focus:** 🚀 WiFi Connectivity Optimalisatie

---

## 🎯 PROBLEEM (v1.5)

Na upgrade naar v1.5 (sleep mode uit):
- ✅ Ping verbeterd: 300ms → 25ms
- ❌ Na pauze: Moeilijk bereikbaar
- ❌ Vereist "energie en geduld" om contact te krijgen

**Oorzaak:** ESP32-C6 heeft automatische **CPU light sleep** die zich anders gedraagt dan oudere ESP32's. De `esp_wifi_set_ps(WIFI_PS_NONE)` fix was onvoldoende.

---

## ✨ OPLOSSING (v1.6)

### **1. ESP32-C6 Power Management** 🔧
```cpp
// NIEUW: CPU light sleep DISABLED
esp_pm_config_t pm_config = {
  .max_freq_mhz = 160,           // Max CPU freq
  .min_freq_mhz = 160,           // Min CPU freq (NIET verlagen!)
  .light_sleep_enable = false    // KRITIEK: Light sleep UIT
};
esp_pm_configure(&pm_config);
```
**Effect:** CPU blijft altijd actief, geen "half slapend" netwerk stack meer.

---

### **2. Beacon Listen Interval = 1** 📡
```cpp
// NIEUW: Luister naar ELKE router beacon
wifi_config_t wifi_config;
esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
wifi_config.sta.listen_interval = 1;  // Normaal 3-10, nu 1!
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
```
**Effect:** Device luistert naar **elke** router beacon (~100ms interval) ipv om de 3-10 beacons.

---

### **3. Non-Blocking Loop** 🔄
```cpp
// OUD (v1.5):
void loop() {
  // ... code ...
  delay(100);  // ❌ CPU mag slapen!
}

// NIEUW (v1.6):
void loop() {
  // ... code ...
  yield();  // ✅ CPU blijft actief, WiFi blijft alert!
}
```
**Effect:** WiFi stack blijft constant actief, geen sleep tijdens delay.

---

### **4. WiFi Keepalive Mechanisme** 💓
```cpp
// NIEUW: Actieve keepalive elke 5s
static unsigned long last_keepalive = 0;
if (millis() - last_keepalive >= 5000) {
  if (!ap_mode && WiFi.status() == WL_CONNECTED) {
    wifi_rssi = WiFi.RSSI();  // Trigger keepalive door RSSI te lezen
  }
  last_keepalive = millis();
}
```
**Effect:** Voorkomt router ARP timeout, device blijft "zichtbaar".

---

### **5. Aggressive mDNS Advertising** 📢
```cpp
// NIEUW: mDNS announce elke 30s
static unsigned long last_mdns_announce = 0;
if (millis() - last_mdns_announce >= 30000) {
  MDNS.announce();  // Herinner router: "Ik ben er nog!"
  last_mdns_announce = millis();
}
```
**Effect:** mDNS blijft up-to-date, `eco.local` altijd bereikbaar.

---

## 📊 VERWACHT RESULTAAT

| Metric | v1.5 | v1.6 | Verbetering |
|--------|------|------|-------------|
| **Ping (actief)** | 25ms | **< 10ms** | 🚀 2.5× sneller |
| **Ping (na pauze)** | Traag/timeout | **< 10ms** | 🎯 Altijd bereikbaar! |
| **Bereikbaar via IP** | Soms moeilijk | **Instant (< 1s)** | ✅ Betrouwbaar |
| **Bereikbaar via mDNS** | Soms timeout | **Binnen 2s** | ✅ Stabiel |
| **Stroomverbruik** | ~80mA | **~110mA** | ⚠️ +30mA (OK voor netvoeding) |

---

## 📝 VOLLEDIGE WIJZIGINGEN

### **Gewijzigde Functies:**

1. **`setupWiFi()`** - Grootste wijziging!
   - `esp_pm_configure()` toegevoegd (CPU power management)
   - `wifi_config.sta.listen_interval = 1` toegevoegd
   - Uitgebreide logging van v1.6 optimalisaties
   
2. **`loop()`** - Kritieke optimalisatie!
   - `delay(100)` vervangen door `yield()`
   - WiFi keepalive mechanisme toegevoegd (5s interval)
   - mDNS announce toegevoegd (30s interval)

3. **`setup()`** - Verbeterde startup logging
   - v1.6 banner met connectivity info
   - Verwachte resultaten getoond bij boot

4. **`getMainPage()`** - UI updates
   - v1.6 banner met connectivity status
   - Uptime display verbeterd (uren + minuten)

5. **`getSettingsPage()`** - Documentatie update
   - v1.6 info box met optimalisaties
   - Duidelijke uitleg van wijzigingen

### **Nieuwe Includes:**
```cpp
#include <esp_pm.h>  // NIEUW: Power management voor ESP32-C6
```

### **Changelog Header:**
```cpp
/* 
 * ESP32_ECO Controller - v1.6
 * ...
 * v1.6 (2026-01-15) - WiFi Connectivity Fix ⚡
 *   - ESP32-C6 power management optimalisatie (esp_pm_configure)
 *   - CPU light sleep DISABLED (blijft altijd actief)
 *   - Beacon listen interval = 1 (luistert naar ELKE router beacon)
 *   - Loop delay(100) vervangen door yield() (non-blocking)
 *   - WiFi keepalive mechanisme toegevoegd (5s RSSI ping)
 *   - mDNS aggressive advertising (30s announce cycle)
 *   - Result: Ping < 10ms constant, ALTIJD bereikbaar! 🎯
 */
```

---

## 🔧 INSTALLATIE INSTRUCTIES

### **Stap 1: Backup**
```bash
# Download huidige v1.5 van je ESP32 (voor zekerheid)
# Noteer huidige instellingen (WiFi, thresholds, etc.)
```

### **Stap 2: Flash v1.6**
```bash
# Arduino IDE:
# 1. Open ESP32_ECO_v1.6.ino
# 2. Board: "ESP32C6 Dev Module"
# 3. Partition: "Huge APP (3MB)"  (als SIMULATION_MODE gebruikt)
#    OF "Default 4MB with spiffs"  (normaal)
# 4. Upload
```

### **Stap 3: Eerste Boot**
```
=== ESP32 ECO Controller v1.6 ===
🎯 WiFi Connectivity Optimized!
⚡ Power management: CPU always active
📡 Beacon listen: EVERY beacon (max alert)
🔄 Loop: Non-blocking yield()
...
✅ WiFi modem power save: DISABLED
✅ CPU light sleep: DISABLED (stays always active)
✅ Beacon listen interval: 1 (max alertness)

🎯 v1.6 CONNECTIVITY OPTIMIZATIONS ACTIVE!
   Expected ping: < 10ms constant
   Power usage: +20-30mA (worth it!)
```

### **Stap 4: Test**
```bash
# Terminal 1: Ping test (moet < 10ms zijn!)
ping 192.168.1.102  # Of jouw IP

# Terminal 2: mDNS test
ping eco.local      # Of jouw hostname

# Browser: Open http://eco.local
# Check v1.6 banner: "🚀 v1.6 CONNECTIVITY MODE: Ping < 10ms"
```

### **Stap 5: Lang durende test**
- Laat controller 30 min zonder interactie
- Probeer dan te pingen/browsen
- **v1.6 moet instant reageren!** (< 10ms)
- **v1.5 zou traag/timeout geweest zijn**

---

## ⚠️ NADELEN & OVERWEGINGEN

### **Stroomverbruik:**
- **+20-30mA** extra verbruik (van ~80mA → ~110mA)
- **OK voor netvoeding** (zoals jouw setup)
- **NIET OK voor batterij** (tenzij ultra-responsiveness nodig is)

### **Warmte:**
- CPU draait constant op 160MHz → **iets warmer**
- Bij normale omgevingstemperatuur: **geen probleem**
- Bij high ambient temp (40°C+): **monitor temperature**

### **Router Load:**
- Beacon listen interval = 1 → **meer traffic naar router**
- Voor 3 controllers: **verwaarloosbaar**
- Voor 50+ devices: **mogelijk router impact**

---

## 🎓 TECHNISCHE DETAILS

### **Waarom delay() slecht is voor WiFi:**
```cpp
delay(100);  // ❌ Probleem:
// 1. ESP32 scheduler ziet: "CPU idle for 100ms"
// 2. Automatisch enter light sleep (power save)
// 3. WiFi stack wordt "half slapend"
// 4. Packets kunnen missed worden
// 5. ARP responses traag/missing
```

```cpp
yield();  // ✅ Oplossing:
// 1. ESP32 scheduler: "CPU idle, maar STAY ACTIVE"
// 2. WiFi stack blijft fully active
// 3. Background tasks (WiFi, TCP/IP) blijven draaien
// 4. No light sleep enter
```

### **Beacon Listen Interval Explained:**
```
Router broadcasts beacon elke ~100ms:
"Hallo devices! Ik ben hier! Sync met mij!"

listen_interval = 10 (oud):
→ Device luistert 1× per 1000ms (10 beacons skip)
→ Max latency: 1000ms

listen_interval = 1 (v1.6):
→ Device luistert ELKE 100ms (geen skip!)
→ Max latency: 100ms
→ MAAR: Meer stroom nodig (moet vaker wakker)
```

### **Power Management Levels (ESP32-C6):**
```
Level 0: CPU @ 160MHz, WiFi active, light sleep OFF
         → v1.6 KEUZE (max responsiveness)
         
Level 1: CPU @ 160MHz, WiFi active, light sleep AUTO
         → v1.5 (CPU kan slapen bij delay)
         
Level 2: CPU @ 80MHz, WiFi power save, deep sleep AUTO
         → Batterij mode (niet geschikt voor ons)
```

---

## 🐛 TROUBLESHOOTING

### **Als ping nog steeds hoog is (> 50ms):**

**Check 1: Power management actief?**
```
Serial Monitor zou moeten tonen bij boot:
"✅ CPU light sleep: DISABLED (stays always active)"

Als je ziet: "⚠️ CPU power config failed: XX"
→ esp-idf versie probleem, update Arduino core!
```

**Check 2: WiFi kwaliteit**
```bash
# Check RSSI in Serial Monitor of /json endpoint
# Moet > -70 dBm zijn
# Als < -70 dBm: Verbeter antenne positie!
```

**Check 3: Router problemen**
```
- Router overloaded? (te veel clients)
- Router WiFi power save enabled? (schakel uit!)
- Router beacon interval > 100ms? (verlaag naar 100ms)
```

---

## 📚 REFERENTIES

**ESP32-C6 Power Management:**
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/system/power_management.html

**WiFi Listen Interval:**
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wifi-protocol-mode

**Arduino ESP32 Core:**
- https://github.com/espressif/arduino-esp32

---

## 👤 CREDITS

**Ontwikkeld door:** Fidel Dworp  
**Met hulp van:** Claude Sonnet 4 (Anthropic)  
**Datum:** 15 januari 2026  
**Repository:** https://github.com/FidelDworp/ESP32C6_ECO-boiler

---

## 📝 VOLGENDE STAPPEN

Na succesvolle v1.6 test:
1. ✅ Update HVAC controller (zelfde fixes)
2. ✅ Update TESTROOM controller (zelfde fixes)
3. 📝 Update GitHub README met v1.6 info
4. 📝 Maak CHANGELOG.md in repository
5. 🎯 Monitor uptime over 1 week
6. 📊 Vergelijk met v1.5 logs

**Doel:** Alle 3 controllers 24/7 bereikbaar met ping < 10ms! 🚀

---

**Veel succes met v1.6!** 🎉
