# ECO-Boiler Controller

**Intelligente zonne- en haardenergie controller voor optimale benutting van gratis warmte**

---

Historiek:

Onze zonneboiler op het dak oogst zonne energie tijdens zonnige dagen en haard-energie in de winterse periode. (Zowel onze zoon als wij hebben een zelfde type haard van JIDE, die een deel van de warmte naar de boiler sturen met hun eigen pomp en sensoren.)

De zonneboiler, heeft enkel deze elementen:
- Een PT1000 sensor (in de collector) om de temperatuur aan de "warme" zijde te meten.
- Een 230V pomprelais en een 5V PWM signaal (0-255) om de snelheid van de pomp te varieren.
- Een OEG pomp, PWM gestuurd.
- Een controller (van het duitse OEG). Die was echter niet intuitief in gebruik en hij werkte in sommige gevallen niet efficient noch betrouwbaar! 

Ik besloot deze controller dan maar door een eigen ontwerp te vervangen.
Voor onze verlichting en HVAC gebruikten we toen al Particle Photons (gemonteerd op mijn eigen ontwerp van shield) met succes.

Dus maakte ik een box met een Photon shield en deze peripherals:
- De bestaande PT1000 sensor (in de collector) om de temperatuur aan de "warme" zijde te meten.
- De bestaande OEG pomp, PWM gestuurd.
- Een bordje om de PT1000 sensor op het dak te lezen en aan de Photon door te geven.
- Een 5V pomprelais, aangesloten op de Photon.
- Een PWM signaal van de Photon, aangesloten op de OEG pomp.
- 6 temperatuur sensors in de ECO boiler om de warmte inhoud en verdeling op te volgen.

Deze controller heeft tot nu toe reeds 5 jaar goeie dienst bewezen, maar een groot nadeel is de wifi stabiliteit van de Photon. De controller verliest te dikwijls "de pedalen" zodat de zonneboiler soms niet meer doorpompt wordt...

Dit deed me besluiten om ook dit systeem om te vormen tot een ESP32C6 controller, die een véél modernere WiFi module heeft, en heel wat krachtiger tegelijk.

FiDel, Recht 11jan26

----------------------------
## 📋 Inhoudsopgave

1. [High-Level Overzicht](#high-level-overzicht) *(Voor nieuwkomers)*
2. [Huidige Systeem (Photon)](#huidige-systeem-photon) *(Technische documentatie bestaand systeem)*
3. [Toekomstig Systeem (ESP32)](#toekomstig-systeem-esp32) *(Geplande migratie - TBD)*

---

# High-Level Overzicht
*Voor nieuwkomers zonder technische achtergrond*

## Wat doet dit systeem?

Stel je voor: je hebt op je dak een **zonneboiler** die warmte verzamelt van de zon. In de winter help je die boiler ook een handje met warmte van je **haard**. Deze gratis energie wil je natuurlijk niet verspillen!

Dit controller systeem zorgt ervoor dat:
1. 🌞 **Zonnewarmte** automatisch wordt opgepompt naar je boiler
2. 🔥 **Haardwarmte** (van 2 JIDE haarden) efficiënt wordt benut  
3. ⚡ **Overtollige energie** wordt doorgegeven aan je verwarmingssysteem
4. 🎯 **Alles automatisch** gebeurt zonder dat je er naar om hoeft te kijken

## Hoe werkt het?

### 🌡️ Slim Meten
Het systeem meet twee belangrijke temperaturen:
- **Dak**: Hoe warm is de zonnecollector? (PT1000 sensor)
- **Boiler**: Hoe warm is het water in de boiler? (6 sensoren)

Door deze te vergelijken weet het wanneer pompen zinvol is!

### 🧠 Slimme Logica
De pomp start **ALLEEN** als:
- ✅ Het verschil > 3°C is (genoeg warmte beschikbaar)
- ✅ De collector warm genoeg is (> 22°C, anders verlies je energie!)
- ✅ Het overdag is (07:00 - 21:00, 's nachts geen zin)
- ✅ De boiler energie **bijwint** (meet elke 10 minuten)

### 💨 Slimme Snelheid
De pomp draait **variabel**:
- Klein verschil (3°C) → Langzaam (PWM 80/255)
- Groot verschil (20°C) → Snel (PWM 200/255)
- Te heet! (90°C+) → Maximum snelheid (PWM 180/255)

Dit spaart elektriciteit EN beschermt de collector!

### 🔋 Energie Delen
Als de boiler vol zit (> 15 kWh), geeft het systeem een seintje aan je **HVAC controller**: 

> *"Hé, hier is 15 kWh gratis warmte! Kom maar halen!"*

De HVAC start dan automatisch een **transfer pomp** die warmte overpompt naar je verwarmingsboilers. Zo voorkom je verspilling!

### 📱 Altijd Bereikbaar
Je kunt via je smartphone of computer zien:
- Hoe warm is de collector? (Tsun)
- Hoeveel energie zit er in de boiler? (EQtot)
- Draait de pomp? Hoe snel? (PWM waarde)
- Hoeveel energie wint/verliest de boiler per 10 minuten? (dEQ)

## Waarom is dit handy?

1. ⚡ **Gratis energie benutten**: Zon + haard kosten niks!
2. 🏠 **Lagere warmterekening**: Tot 700 kWh/jaar besparing
3. 🌍 **Duurzaam**: ~300 kg CO₂ besparing per jaar
4. 🎯 **Automatisch**: Werkt dag en nacht zonder jouw hulp
5. 📊 **Inzicht**: Zie live wat je boiler doet
6. 🛡️ **Veilig**: Beschermt tegen bevriezing én oververhitting

## De Upgrade

Het systeem werkt al **5 jaar goed** (sinds 2020), maar heeft één probleem: de **WiFi verbinding** valt regelmatig weg. Als dat gebeurt:
- ❌ Pomp blijft soms uit (verspilde zonne-energie!)
- ❌ Geen data naar HVAC (transfer werkt niet)
- ❌ Handmatige reset nodig

Daarom krijgt het een upgrade naar **ESP32-C6** die veel betrouwbaarder is!

---

# Huidige Systeem (Photon)
*Technische documentatie van het bestaande Particle Photon systeem*

## Systeem Overzicht

### Hardware Componenten

**Zonnecollector Systeem** (op dak):
- **PT1000 temperatuursensor**: Gemonteerd IN collector vloeistof
- **Range**: -50°C tot +200°C (collector kan tot 150°C stagneren!)
- **Nauwkeurigheid**: ±0.3°C
- **Verbinding**: 2-draads kabel naar controller

**Circulatiepomp** (OEG Solar pump):
- **Type**: OEG pomp met PWM-sturing
- **Voeding**: 230V AC (~50W)
- **Besturing**: 5V PWM signaal (0-255)
- **Flow**: Variabel, afhankelijk van PWM
- **Medium**: Water/glycol mix (vorstbescherming)

**ECO Boiler** (500 liter):
- **Type**: Gestratificeerde warmwaterboiler
- **Sensors**: 6× DS18B20 (digitaal, 1-Wire)
- **Zones**: 3 lagen (Top/Mid/Bot), elk met High/Low sensor
- **Volumes**: 110L, 90L, 90L, 90L, 110L (totaal 490L effectief)
- **Inputs**: 
  - Zonnecollector (via OEG pomp)
  - JIDE haard 1 (eigen pomp/controller)
  - JIDE haard 2 (eigen pomp/controller)

**Controller Box** (custom):
- **Particle Photon**: WiFi microcontroller
- **PhotoniX Shield v4.0**: Custom PCB
- **MAX31865 module**: PT1000 ADC converter (SPI)
- **Relay module**: 230V pomp schakelaar
- **PWM amplifier**: 3.3V → 5V voor OEG pomp

### Origineel OEG Controller - Waarom Vervangen?

De Duitse OEG fabriekscontroller had meerdere problemen:
1. **Niet intuïtief**: Moeilijke configuratie, onduidelijke menu's
2. **Inefficiënt**: Suboptimale dT thresholds en pompregeling
3. **Geen monitoring**: Geen data uitgang, geen remote toegang
4. **Onbetrouwbaar**: Regelmatig valse starts/stops

**Oplossing**: Eigen controller met:
- Flexibele dT logica (aanpasbaar)
- WiFi monitoring & remote control
- Integratie met HVAC systeem
- Data logging voor analyse

## Software Architectuur (Photon)

### Core Control Algorithm: dT-Based Pumping

**Differential Temperature Control**:
```cpp
// Meet beide temperaturen
Tsun = readPT1000();      // Collector temp (dak)
Tboil = EBotH;            // Boiler bottom temp (waar warm water inkomt)
dT = Tsun - Tboil;        // Temperatuurverschil

// Beslissing: Moet pomp aan?
bool shouldPumpOn = (dT > 3.0);
```

**Waarom dT = 3°C threshold?**
- < 3°C: Te weinig warmte → Energieverlies door pomp + leiding
- \> 3°C: Genoeg warmte → Efficiënte transfer
- Hysteresis: Voorkomt flikker tussen ON/OFF

### Geavanceerde Pomp Logica (solarPump Function)

De `solarPump()` functie draait **elke minuut** en check 6 condities:

**1. Nachtblokkering**:
```cpp
if (Hour < 7 || Hour >= 21) {
  // Stop pump: Geen zon 's nachts!
  pumpOFF();
  return;
}
```

**2. Thermosifon Blokkering**:
```cpp
if (dT > 3.0 && Tsun < 22.0) {
  // Collector nog te koud ondanks dT
  // Risico: Afkoeling boiler door circulatie!
  pumpOFF();
}
```

**3. Energie-Verlies Detectie** (nieuw sinds nov 2025!):
```cpp
if (dEQ <= 0.0) {
  consecutiveReductions++;  // Tel verliesrondes
  if (consecutiveReductions >= 3) {
    // 3× achter elkaar verlies → Stop!
    pumpOFF();
  }
} else {
  consecutiveReductions = 0; // Reset bij winst
}
```
**dEQ**: Energie change over 10 minuten. Als negatief = boiler verliest!

**4. Overheat Protection**:
```cpp
if (Tsun >= 90.0) {
  // KRITIEK! Collector te heet!
  pumpON_MAX_SPEED();  // PWM = 255 voor koeling
}
```

**5. PWM Speed Calculation** (als pomp AN moet):
```cpp
if (Tsun > 75.0) {
  pwmValue = 180;  // Hoge temp → Vaste snelheid
} else {
  // Lineaire mapping: dT 3-20°C → PWM 80-200
  float delta = constrain(dT - 3.0, 0.0, 17.0);
  pwmValue = 80 + (delta * 120.0 / 17.0);
}
```

**6. Relay & PWM Output**:
```cpp
if (shouldPumpOn) {
  digitalWrite(relayPin, LOW);   // Relay ON (active LOW)
  analogWrite(pwmPin, pwmValue); // PWM output
} else {
  digitalWrite(relayPin, HIGH);  // Relay OFF
  analogWrite(pwmPin, 0);        // PWM = 0
}
```

### Energieberekening (Qtot)

**Boiler Configuratie**:
```
Zone 1 (Top):    110L  (Sensors: ETopH, ETopL)
Zone 2 (Top-Mid): 90L  (Sensors: ETopL, EMidH)
Zone 3 (Mid):     90L  (Sensors: EMidH, EMidL)
Zone 4 (Mid-Bot): 90L  (Sensors: EMidL, EBotH)
Zone 5 (Bottom): 110L  (Sensors: EBotH, EBotL)
```

**Qtot Formule**:
```cpp
// Per zone:
// 1. Bereken gemiddelde temp van zone
EAv1 = (ETopH + ETopL) / 2;
// ... (herhaal voor zones 2-5)

// 2. Bereken energie boven minimum (35°C)
ETmin = 35; // Hot water minimum (douche bescherming!)
EQ1 = (EAv1 - ETmin) * 110 * 1.163 / 1000; // kWh
// ... (herhaal voor zones 2-5)

// 3. Totaal optellen
EQtot = EQ1 + EQ2 + EQ3 + EQ4 + EQ5; // Totale beschikbare energie
```

**Specifieke Warmte Constante**:
- Water: 4.186 kJ/kg·K = 1.163 Wh/L·K

**dEQ Berekening** (energie change rate):
```cpp
// Elke 10 minuten:
dEQ = EQtot - prev_EQtot;
prev_EQtot = EQtot;

// Interpretatie:
// dEQ > 0 → Boiler wint energie (zon of haard actief)
// dEQ ≈ 0 → Stabiel (noch winst noch verlies)
// dEQ < 0 → Boiler verliest energie (verbruik of afkoeling)
```

### PT1000 Temperature Reading

**Hardware**: MAX31865 SPI module

**Probleem** (opgelost dec 2025!):
```cpp
// OUDE code (library functie):
float temp = sensor.temperature(RNOMINAL, RREF);
// Bug: Foute berekening bij vriestemp! (Tsun = -30°C ipv +5°C)
```

**Oplossing** (handmatige berekening):
```cpp
float readSolarTemp() {
  uint16_t rtd = sensor.readRTD();           // Lees raw ADC
  if (rtd == 0 || rtd > 32768) return -127.0; // Error check
  
  float ratio = rtd / 32768.0;               // Normalize
  float resistance = ratio * 4000.0;         // R = ratio × Rref
  float temperature = (resistance - 1000.0) / 3.850; // Linear approx
  
  if (temperature < -50 || temperature > 200) return -127.0;
  return temperature;
}
```

**Rref Correctie**:
```cpp
// FOUT: const float RREF = 4300.0; 
// CORRECT: const float RREF = 4000.0;
// Chinese MAX31865 modules hebben 4000Ω, niet 4300Ω!
```

### DS18B20 Boiler Sensors

**Hardcoded Adressen** (6 sensors):
```cpp
byte addrs0[6][8] = {
  {0x28,0xFF,0x0D,0x4C,0x05,0x16,0x03,0xC7}, // ETopH
  {0x28,0xFF,0x25,0x1A,0x01,0x16,0x04,0xCD}, // ETopL
  {0x28,0xFF,0x89,0x19,0x01,0x16,0x04,0x57}, // EMidH
  {0x28,0xFF,0x21,0x9F,0x61,0x15,0x03,0xF9}, // EMidL
  {0x28,0xFF,0x16,0x6B,0x00,0x16,0x03,0x08}, // EBotH
  {0x28,0xFF,0x90,0xA2,0x00,0x16,0x04,0x76}  // EBotL
};
```

**Verbeterde Read Routine** (nov 2025 - Grok assisted):
```cpp
// PROBLEEM: delay(1000) in oude code → WiFi drops!
// OPLOSSING: Non-blocking two-step read

void getTemperatures(int select) {
  static unsigned long conversionStart = 0;
  static bool conversionRequested = false;
  
  // Step 1: Start conversie (éénmalig per interval)
  if (!conversionRequested && timeout) {
    ds.reset();
    ds.skip();
    ds.write(0x44, 0);  // Start ALL conversions
    conversionStart = millis();
    conversionRequested = true;
    return; // Exit! Geen blocking delay!
  }
  
  // Step 2: Read na 1s (zonder main loop te blocken)
  if (conversionRequested && (millis() - conversionStart >= 1000)) {
    // Read scratchpad van alle 6 sensors
    // ...
    conversionRequested = false;
  }
}
```

**CRC Error Handling**:
```cpp
byte currentCRC = OneWire::crc8(scratchpadData, 8);
if (currentCRC != scratchpadData[8]) {
  crcErrorCount[i]++;
  
  if (Time.now() - tmStamp[i] > 3600UL) {
    // Sensor > 1 uur offline → TIMEOUT alert!
    Particle.publish("Alerts", "Sensor Timeout: " + String(i));
  }
  continue; // Skip deze sensor, gebruik last valid value
}
```

### HVAC Integration

**Data Publishing** (naar HVAC controller):
```cpp
// Elke 5 minuten, als EQtot > 15 kWh:
if (EQtot > 15 && millis() - lastEvacuate >= 300000) {
  sprintf(str, "ECO: %.2f kWh", EQtot);
  Particle.publish("Status-HEAT:HVAC", str, PRIVATE);
  lastEvacuate = millis();
}
```

**HVAC Reactie** (zie HVAC_Photon.cpp):
```cpp
// HVAC Subscribe event handler:
void eventDecoder(const char *event, const char *data) {
  if (strncmp(Subject, "ECO", 3) == 0) {
    ECOQtot = atof(strtok(NULL, ",")); // Parse "ECO: 15.23 kWh"
    ECOtransfer(); // Check transfer naar SCH/WON boilers
  }
}

// Transfer logica:
void ECOtransfer() {
  // Start pump als:
  if (ECOQtot > 15 && heating_demand > 0) {
    startTransferPump_SCH(); // 30 min cycle
  }
  
  // Stop pump als:
  if (ECOQtot < 12) {
    stopTransferPump_SCH(); // Douche bescherming!
  }
}
```

**Energy Flow**:
```
ECO Boiler (15 kWh) 
    → Publish "ECO: 15.00 kWh"
         → HVAC receives
              → Start transfer pump
                   → Pump 3-5 kWh naar SCH boiler
                        → ECO Boiler (10 kWh)
```

### JSON Status Output

**Complete Status String** (elke minuut):
```json
{
  "ETopH": 72.3,
  "ETopL": 68.1,
  "EMidH": 55.2,
  "EMidL": 51.8,
  "EBotH": 42.5,
  "EBotL": 39.7,
  "EAv": 54.9,
  "EQtot": 14.23,
  "Solar": 78.5,
  "dT": 36.0,
  "dEQ": 0.127,
  "pwmVal": 180,
  "Relay": 1,
  "WiFiSig": -52,
  "Mem": 68
}
```

**Field Explanations**:
| Field | Unit | Description |
|-------|------|-------------|
| `ETopH` | °C | Boiler top sensor (high) |
| `ETopL` | °C | Boiler top sensor (low) |
| `EMidH` | °C | Boiler mid sensor (high) |
| `EMidL` | °C | Boiler mid sensor (low) |
| `EBotH` | °C | Boiler bottom sensor (high) - PUMP INPUT |
| `EBotL` | °C | Boiler bottom sensor (low) |
| `EAv` | °C | Average boiler temperature |
| `EQtot` | kWh | Total available energy (> 35°C) |
| `Solar` | °C | Collector temperature (PT1000) |
| `dT` | °C | Temperature differential (Tsun - Tboil) |
| `dEQ` | kWh | Energy change over last 10 minutes |
| `pwmVal` | 0-255 | Pump PWM speed (0=off, 255=max) |
| `Relay` | 0/1 | Pump relay state (0=off, 1=on) |
| `WiFiSig` | dBm | WiFi signal strength (-30 to -90) |
| `Mem` | % | Free memory percentage |

### WiFi Stability Improvements

**Probleem** (historisch):
- Photon WiFi stack crasht regelmatig
- AUTO antenna switching veroorzaakt disconnects
- Cloud reconnect soms falen

**Oplossingen** (geïmplementeerd):

**1. Externe Antenne** (stabielste):
```cpp
STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
// NIET: ANT_AUTO (veroorzaakt disconnects!)
// NIET: ANT_INTERNAL (te zwak signaal)
```

**2. Smart Reconnect Logic** (nov 2025):
```cpp
// Cooldown tussen reconnect pogingen
static unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30s

if (!Particle.connected()) {
  if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    Particle.connect();
    lastReconnectAttempt = millis();
  }
}
```

**3. Publish Guards** (voorkomt queue overflow):
```cpp
if (Particle.connected()) {
  Particle.publish("Solar", str, PRIVATE);
}
// NIET publishen als offline → Queue overflow → Crash!
```

**4. Non-Blocking Sensor Reads** (nov 2025):
```cpp
// FOUT: delay(1000) → WiFi timeout
// JUIST: Two-step non-blocking read
```

**5. Memory Monitoring**:
```cpp
int freemem = System.freeMemory();
int memPERCENT = (freemem * 100) / 82944; // 80 KB RAM

// Alert als < 30%:
if (memPERCENT < 30) {
  Particle.publish("Alerts", "Low memory!");
}
```

**Resultaat**: Uptime verbeterd van 2-3 dagen → 1-2 weken

## Hardware Specificaties (Photon Systeem)

### Particle Photon
- **MCU**: STM32F205 (ARM Cortex-M3, 120MHz)
- **RAM**: 128KB SRAM
- **Flash**: 1MB
- **WiFi**: Broadcom BCM43362 (2.4GHz 802.11b/g/n)
- **Antenna**: External (Taoglas FXU.07.A.04)

### PhotoniX Shield v4.0 Pin Mapping

```
D0 - I2C-SDA          [Unused]
D1 - I2C-SCL          [Unused]
D2 - RELAY_PUMP       → 230V relay (active LOW)
D3 - T-BUS            → 6× DS18B20 (1-Wire)
D4 - PIXELS           [Unused]
D5 - PIR              [Unused]
D6 - TEMP/HUM         [Unused]
D7 - GAS-DIG          [Unused]
A0 - TOUCH-1          [Unused]
A1 - TOUCH-2          [Unused]
A2 - SPI_SS           → MAX31865 CS (PT1000)
A3 - SPI_SCK          → MAX31865 SCK
A4 - SPI_MISO         → MAX31865 MISO
A5 - SPI_MOSI         → MAX31865 MOSI
A6 - OP1              [Unused]
A7 - PWM_PUMP         → OEG pump PWM (0-5V via OpAmp)
```

### MAX31865 PT1000 Module

**SPI Configuration**:
```cpp
SPI.begin();
sensor.begin(MAX31865_2WIRE); // 2-wire PT1000
```

**Critical Parameters**:
```cpp
RREF = 4000.0;  // Reference resistor (NOT 4300!)
RNOMINAL = 1000.0; // PT1000 @ 0°C
```

**Temperature Range**:
- Nominal: -50°C to +200°C
- Praktijk: -10°C to +150°C (collector stagnatie)

**Resolution**: ~0.03°C

### 230V Relay Module

**Type**: SSR (Solid State Relay) of electromechanical
**Coil**: 5V DC (via Photon D2)
**Contacts**: 10A @ 230VAC
**Logic**: Active LOW
```cpp
digitalWrite(relayPin, LOW);  // Pump ON
digitalWrite(relayPin, HIGH); // Pump OFF
```

### OEG Solar Pump

**Model**: OEG pump (specifiek model onbekend)
**Voeding**: 230V AC
**Vermogen**: ~50W @ full speed
**PWM Input**: 0-5V DC (via OpAmp versterker)
**Mapping**:
```
0V   → 0% speed   (pump off)
2.5V → 50% speed  (~25W)
5V   → 100% speed (~50W)
```

**PWM Curve** (geobserveerd):
```
PWM 0-80:   Pump draait niet (minimum threshold)
PWM 80:     Pump start (32% duty → ~1.6V)
PWM 180:    Normal high speed (71% duty → ~3.5V)
PWM 200:    Max regular speed (78% duty → ~3.9V)
PWM 255:    Absolute max (100% duty → 5V)
```

### 1-Wire DS18B20 Sensors

**Configuration**:
- **Count**: 6 sensors
- **Bus**: D3 (shared bus, different addresses)
- **Pull-up**: 4.7kΩ to 3.3V
- **Resolution**: 12-bit (0.0625°C)
- **Conversion Time**: 750ms

**Physical Layout** (in 490L boiler):
```
Top Zone (110L):
  ├─ ETopH @ 480L mark  (hoogste sensor)
  └─ ETopL @ 420L mark

Mid-Top Zone (90L):
  ├─ EMidH @ 370L mark  
  └─ EMidL @ 320L mark

Mid-Bottom Zone (90L):
  ├─ (EMidL @ 320L)
  └─ EBotH @ 270L mark

Bottom Zone (110L):
  ├─ (EBotH @ 270L)
  └─ EBotL @ 50L mark   (laagste sensor)
```

**Addressing**: By hardcoded ROM address (zie Software Architectuur)

## State Machine & Decision Logic

### Pump State Machine

```
         ┌────────────┐
    ┌───►│    IDLE    │◄────┐
    │    │ (Pump OFF) │     │
    │    └──────┬─────┘     │
    │           │            │
    │    Conditions:         │
    │    • Hour 7-21         │
    │    • dT > 3°C          │
    │    • Tsun > 22°C       │
    │    • No loss-streak    │
    │           │            │
    │           ▼            │
    │    ┌────────────┐     │
    │    │  STARTING  │     │
    │    │ (Ramp PWM) │     │
    │    └──────┬─────┘     │
    │           │            │
    │           ▼            │
    │    ┌────────────┐     │
    │    │  RUNNING   │     │
    │    │ (Variable  │     │
    │    │   Speed)   │     │
    │    └──────┬─────┘     │
    │           │            │
    │    Stop Conditions:    │
    │    • dT < 3°C          │
    │    • Tsun < 22°C       │
    │    • Hour < 7 or ≥ 21  │
    │    • 3× loss streak    │
    │           │            │
    │           ▼            │
    │    ┌────────────┐     │
    └────┤  STOPPING  │─────┘
         │ (Ramp PWM) │
         │  to zero   │
         └────────────┘
```

### Decision Tree

```
START
  │
  ├─ Hour < 7 or >= 21? → YES → PUMP OFF (Night)
  │                     → NO ↓
  │
  ├─ dT < 3°C? → YES → PUMP OFF (Insufficient differential)
  │           → NO ↓
  │
  ├─ Tsun < 22°C? → YES → PUMP OFF (Thermosiphon risk)
  │              → NO ↓
  │
  ├─ consecutiveReductions >= 3? → YES → PUMP OFF (Loss streak)
  │                              → NO ↓
  │
  ├─ Tsun >= 90°C? → YES → PUMP MAX (Overheat protection)
  │               → NO ↓
  │
  └─ Calculate PWM based on dT or Tsun → PUMP ON
       │
       ├─ Tsun > 75°C? → YES → PWM = 180 (Fixed high)
       │              → NO ↓
       │
       └─ PWM = 80 + (dT-3)*120/17  (Linear: 80-200 voor dT 3-20°C)
```

## Performance Characteristics (Photon)

### Typical Daily Cycle (Zonnige Dag - Zomer)

```
Time  | Tsun | Tboil | dT   | Pump | PWM | Action
------|------|-------|------|------|-----|-------------------
06:00 | 15°C | 45°C  | -30° | OFF  | 0   | Night block
07:00 | 18°C | 45°C  | -27° | OFF  | 0   | Too cold (dT < 3)
08:00 | 35°C | 45°C  | -10° | OFF  | 0   | Still negative dT
09:00 | 55°C | 45°C  | +10° | ON   | 105 | Pump starts!
10:00 | 68°C | 48°C  | +20° | ON   | 200 | Max regular speed
12:00 | 85°C | 62°C  | +23° | ON   | 200 | Peak performance
14:00 | 78°C | 68°C  | +10° | ON   | 105 | Slowing down
16:00 | 62°C | 70°C  | -8°  | OFF  | 0   | dT negative
18:00 | 45°C | 68°C  | -23° | OFF  | 0   | Cooling down
21:00 | 25°C | 65°C  | -40° | OFF  | 0   | Night block active
```

**Yield**: 15-25 kWh (EQtot: 8 kWh → 25 kWh → 18 kWh na transfer)

### Energy Metrics

**Typical Performance**:

**Zonnige Dag (zomer)**:
- Start: 08:00 (EQtot = 8 kWh)
- Peak: 14:00 (EQtot = 28 kWh)
- Transfer: 15:00 (28 → 23 kWh, 5 kWh naar HVAC)
- Eind: 20:00 (EQtot = 18 kWh na verbruik)
- **Net yield**: +10 kWh

**Bewolkte Dag**:
- Intermittent pumping (on/off cycles)
- **Yield**: 3-8 kWh
- Geen transfer (blijft onder 15 kWh)

**Winterdag met Haarden**:
- Zon: 1-2 kWh (middag)
- Haard 1: +5 kWh (avond, 18:00-22:00)
- Haard 2: +3 kWh (avond, 19:00-23:00)
- **Total**: 9-10 kWh/dag
- Transfer: Mogelijk 1× per week

**Jaarlijkse Statistieken** (geschat):
```
Zon:    500-700 kWh/jaar
Haard:  300-400 kWh/jaar
Total:  800-1100 kWh/jaar verzameld
Transfer naar HVAC: ~300-400 kWh/jaar
Eigen verbruik (douches): ~400-500 kWh/jaar
Verlies (afkoeling): ~100-200 kWh/jaar
```

### Pump Cycle Statistics

**Run Time Distribution** (typisch):
```
Winter (nov-feb):  1-2 uur/dag  (pomp draait weinig)
Lente (mrt-mei):   3-5 uur/dag  (opbouw seizoen)
Zomer (jun-aug):   5-8 uur/dag  (peak performance)
Herfst (sep-nov):  2-4 uur/dag  (afbouw)
```

**PWM Distribution** (when running):
```
PWM 80-100:    15% (start/stop, lage dT)
PWM 100-150:   35% (moderate dT 5-12°C)
PWM 150-200:   45% (goede dT 12-20°C)
PWM 200-255:    5% (overheat protection)
```

### WiFi & Uptime

**Current Status** (na nov 2025 fixes):
- **Typical Uptime**: 1-2 weken
- **Best Uptime**: 4 weken (stabiel weer, weinig disconnects)
- **Worst Uptime**: 2-3 dagen (WiFi interference, crashes)

**Disconnect Triggers** (geobserveerd):
- Zwak signaal (RSSI < -80 dBm)
- Router reboots
- Overload van Particle Cloud
- Memory leaks (nu opgelost)

**Recovery Time**:
- Auto-reconnect: 30-60s (als succesvol)
- Manual reset: Onmiddellijk (via Particle App of fysiek)

## Known Issues & Limitations (Photon)

### 1. WiFi Instability ⚠️ **CRITICAL**

**Impact**: HIGH  
**Symptomen**:
- Photon disconnect (cloud LED blauw flashing)
- Pomp blijft in laatste staat (ON of OFF)
- Geen data updates naar HVAC
- Vereist handmatige reset

**Frequency**: 
- Gemiddeld: 1× per 1-2 weken
- Worst case: Dagelijks (bij zwak signaal)

**Workarounds Geïmplementeerd**:
- External antenna (stabielst)
- 30s reconnect cooldown
- Publish guards (voorkomt queue overflow)
- Non-blocking reads (voorkomt WiFi timeout)

**Blijvend Probleem**: 
Photon WiFi stack is inherent instabiel. Enige echte oplossing = ESP32 upgrade.

---

### 2. PT1000 Calibration Drift

**Impact**: LOW  
**Symptoom**: Mogelijk 1-2°C offset na 5 jaar gebruik

**Oorzaak**:
- Sensor aging
- Moisture penetration
- Thermal cycling stress

**Detectie**: 
Vergelijk met referentie thermometer op zonnige dag (Tsun vs werkelijk)

**Fix**: 
```cpp
// Software calibration offset
float readSolarTemp() {
  float temp = calculateTemp();
  return temp - CALIBRATION_OFFSET; // bijv. -1.5°C
}
```

---

### 3. No Flow Sensor

**Impact**: MEDIUM  
**Probleem**: Geen feedback of pomp echt draait

**Risico's**:
- Relay ON maar pomp kapot → Geen flow → Collector overheat
- Luchtsluis in systeem → Geen circulatie → Collector overheat

**Detection**: Indirect via dEQ
```
Als pump ON && dEQ blijft negatief → Flow probleem!
```

**Oplossing ESP32**: Add flow sensor (Hall effect, pulse counter)

---

### 4. Boiler Sensor Gaps (CRC Errors)

**Impact**: MEDIUM  
**Frequency**: 1-5 errors per dag (acceptabel)

**Oorzaak**:
- 1-Wire bus interference (lange kabel)
- EMI van relay / pomp
- Loose connections

**Handling**:
```cpp
if (CRC_ERROR) {
  crcErrorCount[i]++;
  useLastValidValue();
  if (timeout > 1hour) sendAlert();
}
```

**Future**: Shielded cable, shorter runs

---

### 5. No Glycol Compensation

**Impact**: LOW  
**Probleem**: Qtot berekening gaat uit van puur water

**Reality**: 
- Systeem bevat water/glycol mix (~30-40% glycol)
- Glycol heeft lagere warmtecapaciteit (0.9 vs 1.163 Wh/L·K)

**Error**: ~10-15% overschatting van Qtot

**Fix**: Adjust formula
```cpp
float SPECIFIC_HEAT = 1.0; // Water/glycol mix (was 1.163)
EQtot = ... * SPECIFIC_HEAT / 1000;
```

---

### 6. Limited Historical Logging

**Impact**: MEDIUM  
**Probleem**: Geen lokale data opslag

**Current**:
- Particle Cloud: 7 dagen data retention (gratis account)
- Manual logging: Google Sheets via IFTTT (manual setup)

**Gemis**:
- Geen long-term trends
- Geen offline logging
- Data verloren bij WiFi outage

**ESP32 Solution**: SD card lokaal logging

---

### 7. No OTA Updates

**Impact**: LOW  
**Probleem**: Firmware update = fysiek naar boiler kamer

**Current Process**:
1. Laptop meenemen
2. USB kabel
3. Upload via Particle Workbench
4. Test
5. Hope it works! 🤞

**ESP32 Solution**: OTA via WiFi (like HVAC V53.4)

---

### 8. Thermosiphon Risk Window

**Impact**: LOW  
**Probleem**: 's Ochtends vroeg (07:00-08:00) mogelijk thermosiphon

**Scenario**:
```
07:00: Collector opwarmt (20°C → 30°C)
Boiler nog warm van gisteren (55°C)
dT = -25°C (nog negatief)
Pomp = OFF (correct)

MAAR: Natuurlijke convectie kan al starten
→ Koud water van collector stijgt naar boiler
→ Boiler koelt af zonder warmtewinst
```

**Mitigatie**: Thermosiphon blokkering (Tsun < 22°C)

**Toekomstig**: Check valve in systeem (mechanisch)

---

## Migratie naar ESP32-C6

### Redenen voor Upgrade

**1. WiFi Reliability** 🔴 **HOOGSTE PRIORITEIT**
```
Photon: 1-2 weken uptime, regelmatige crashes
ESP32:  Maanden uptime, stabiele reconnect
```

**2. Processing Power**
```
Photon: 120MHz ARM Cortex-M3, 128KB RAM
ESP32:  160MHz RISC-V, 512KB RAM
→ 4× meer geheugen voor logging & filtering
```

**3. Better I/O**
```
Photon: 1× SPI (MAX31865), 1× 1-Wire, 1× PWM
ESP32:  Native ADC (PT1000 direct?), multi-PWM, meer GPIO
```

**4. Local Storage**
```
Photon: Geen lokale opslag
ESP32:  SD card logging (CSV files, 32GB+)
```

**5. OTA Updates**
```
Photon: USB required, manual process
ESP32:  WiFi OTA (like HVAC), remote update
```

**6. Web Interface**
```
Photon: Particle Cloud dashboard (basic)
ESP32:  Custom web UI (HVAC V53.4 style)
         → Real-time graphs
         → Manual pump control
         → Live sensor data
         → Historical statistics
```

**7. Consistency**
```
HVAC:  ESP32-C6 ✓
ROOMS: ESP32-C6 ✓
ECO:   Photon ✗ → ESP32-C6 migration
```

### Behouden Features (Hardware Reuse)

✅ **Sensors**:
- PT1000 + MAX31865 module (keep)
- 6× DS18B20 sensors (keep)
- Bekabeling (keep)

✅ **Actuators**:
- OEG pomp (keep)
- 230V relay module (keep)
- PWM amplifier (keep of replace met direct 5V PWM)

✅ **Mounting**:
- Controller box (keep)
- Sensor plaatsing in boiler (keep)

### Nieuwe Hardware (ESP32 System)

🆕 **Core**:
- ESP32-C6 DevKit (replace Photon)
- Power supply 5V/2A (reuse of upgrade)

🆕 **Optional Additions**:
- SD card module (SPI)
- Flow sensor (Hall effect, pulse counter)
- Pressure sensor (optional diagnostics)
- OLED display (local status, optional)

### Software Migration Plan

**Phase 1: Core Functionality** (Week 1-2)
- [ ] Port sensor reading (PT1000 + DS18B20)
- [ ] Port dT calculation
- [ ] Port pump control logic
- [ ] Port Qtot calculation
- [ ] Basic Serial output (debugging)

**Phase 2: WiFi & Integration** (Week 3)
- [ ] WiFi connection (NVS config)
- [ ] mDNS (eco.local)
- [ ] JSON endpoint (/status.json)
- [ ] HVAC integration (publish ECO energy)

**Phase 3: Web Interface** (Week 4)
- [ ] Main page (status dashboard)
- [ ] Settings page (WiFi, thresholds)
- [ ] Manual controls (pump ON/OFF, PWM override)
- [ ] Real-time data (1s updates)

**Phase 4: Advanced Features** (Week 5-6)
- [ ] SD card logging (CSV, daily files)
- [ ] Historical graphs (last 7 days)
- [ ] Statistics page (daily/monthly yield)
- [ ] OTA updates

**Phase 5: Testing & Deployment** (Week 7-8)
- [ ] Parallel testing (Photon + ESP32)
- [ ] Data validation (compare outputs)
- [ ] Reliability testing (7+ day uptime)
- [ ] Final switchover

### Feature Comparison

| Feature | Photon | ESP32 (Planned) |
|---------|--------|-----------------|
| **Core Control** | | |
| dT-based pumping | ✅ | ✅ |
| PWM speed control | ✅ | ✅ |
| Night blocking | ✅ | ✅ |
| Thermosiphon blocking | ✅ | ✅ |
| Loss-streak detection | ✅ | ✅ |
| Overheat protection | ✅ | ✅ |
| **Sensors** | | |
| PT1000 (SPI) | ✅ | ✅ (of direct ADC) |
| 6× DS18B20 (1-Wire) | ✅ | ✅ |
| Flow sensor | ❌ | 🆕 Optional |
| **Connectivity** | | |
| WiFi | ✅ (unstable) | ✅ (stable WiFi 6) |
| mDNS | ❌ | 🆕 Yes |
| JSON endpoint | ❌ | 🆕 /status.json |
| Web interface | ❌ | 🆕 Full UI |
| **Data Logging** | | |
| Cloud publish | ✅ | ✅ |
| Local SD storage | ❌ | 🆕 CSV files |
| Real-time graphs | ❌ | 🆕 Web charts |
| **Maintenance** | | |
| OTA updates | ❌ | 🆕 Via WiFi |
| Remote config | ❌ | 🆕 Web settings |
| Manual override | ✅ (Particle function) | 🆕 Web buttons |
| **Integration** | | |
| HVAC energy publish | ✅ | ✅ Enhanced |
| Status panel data | ✅ JSON | ✅ JSON + Web |

---

# Toekomstig Systeem (ESP32)
*Geplande migratie - Volledige documentatie volgt na implementatie*

**Status**: 🔨 **Planning Phase**

**Target**: Q2 2026 (na HVAC stabilisatie)

**Repository**: [ESP32C6_ECO-boiler](https://github.com/FidelDworp/ESP32C6_ECO-boiler)

---

## Contact & Repository

**Repository**: https://github.com/FidelDworp/ESP32C6_ECO-boiler  
**Issues**: https://github.com/FidelDworp/ESP32C6_ECO-boiler/issues

**Related Projects**:
- ESP32C6_HVAC: https://github.com/FidelDworp/ESP32C6_HVAC (central heating)
- ESP32C6_ROOMS: https://github.com/FidelDworp/ESP32C6_ROOMS (room controllers)

---

**Author**: Fidel Dworp  
**Location**: Zarlardinge, België  
**Last Updated**: 11 januari 2026  
**Current System**: Particle Photon (5 jaar in productie sinds 2020)  
**Planned Upgrade**: ESP32-C6 (Q2 2026)  
**Status**: 🟡 Photon actief, ESP32 in planning
