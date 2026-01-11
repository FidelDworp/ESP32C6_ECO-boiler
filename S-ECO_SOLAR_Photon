/* S-ECO_SOLAR.ino = Energy_Monitor + SOLAR Pump controller for the "ECO-Boiler" Photon in the boiler room.

Versions:
- 26dec25: Correctie in de logica (Grok) + 2e correctie! (pump function)
- 3dec25: Correctie in de logica (Grok)
- 26nov25: Correctie in de initialisatie van enkele variabelen (Grok)
- 25nov25: Correctie in de solarPump() functie!
- 24nov25: Corrigeerde met Grok: 1) Fout voor PT1000 temperature MAX31865 printje (in geval het vriest). 2) Nieuwe logica in solarPump() functie
- 18nov25: Pomp kan 's nachts NIET draaien (Startte als het vriest 's nachts (door fout in PT1000 temperature MAX31865 printje).
- 5nov25: Alles ivm Sunlevel verwijderd. Nieuwe berekening van dEQ over 10 min.
- 4nov25: Wifi% naar RSSI (dB) gewijzigd. sunlevel niet meer gebruikt (EventDecoder voor "SUN" weggelaten)
- 3nov25: Analysis by Grok: Photon valt regelmatig uit! => 1) Memory leak in EventDecoder()! 2) New functie getTemperatures() zonder delay(1000). Verfijning van de pomplogica. 2 extra diagnose velden voor google sheet.
- 21oct23: Reduce unnecessary particle.publish commands
- 4mei23: Added a message (ex: ECO: 9.63 kWh) published for the HVAC system to activate the ECOtransfer() pumps to store excess energy in the cellar boilers (SCH+WON)
- 25apr23: Installed & tested to replace OEG controller: Made adjustments in the pwm calculation logic...
- 21apr23: Modified pump ON/OFF logic to more complex logic: 4 consecutive red. then <2000 lux
- 15apr23: Added the new pump control logic (assisted by ChatGPT3.5) Made also a "simulator" sketch to test this logic.
-  4apr23: Added Sunlight data (like in roomsketches) to find an alternative SOLAR ON/OFF method.
- 27mar23: Integrated SOLAR pump controller with PT1000 sensor in collector (on the roof)
- 24dec22: Added Homebridge reporting => Sending the total boiler energy (EQTOT) as a temperature to Homebridge (visualize it on iPhone)
- 25sep21: Reduced messaging intervals
- 14oct19: Created JSON publish string with all temperatures.

Features:
- Reads the six "hardcoded" DS18x20 sensors of the ECO boiler by their HEX sensor code and use them in the loop() by their own names.
Note: The sketch was updated by @ric using "doubles" (Particle.variables can't use floating variables). The library was integrated in this sketch so that it only needs the "OneWire" library. @Ric's function was modified by @BulldogLowell to include CRC checking:
- Calculating hourly heat energy added or consumed + accumulating total energy added or consumed for a full season.
- 1-wire bus scanner: Identify the addresses of the (DS18B20) temperature sensors. This is called manually from the function "Manual"
- Variable(s) for this monitoring are retained in ROM memory so that the accumulated energy demand can be kept for a full year... Make sure Vbat remains powered by a 3V Lithium button cell!
- 1-wire address scanner: List all DS18B20 sensors on the T-BUS

NEW: This sketch checks the difference between the Solar and Boiler temperature and controls the Pumprelay based on the "difference"delta T".
A PWM signal between 0 and 100% is generated to control the Solar pump speed. When Delta T is 10°C, the PWM = 100%, when it's 0°C, PWM = 0%.
To digitize PT1000 temperature, an "Adafruit_MAX31865" module is used: Uses hardware SPI1 module: CS = pin A2 (A2=SS,A3=SCK,A4=MISO,A5=MOSI)

---------------------------------------
PhotoniX shield v.4.0	I/O connections: (* = used)

 D0 - I2C-SDA
 D1 - I2C-SCL
*D2 - TOUCH-COM (= relayPin)
*D3 - RoomSense T-BUS (=Multiple TEMP sensors)
 D4 - PIXEL-line
 D5 - RoomSense PIR
 D6 - RoomSense TEMP/HUM
 D7 - RoomSense GAS-DIG
 A0 - TOUCH-1
 A1 - TOUCH-2
*A2 - RoomSense GAS-ANA (= SPI SS pin)
*A3 - RoomSense LIGHT (= SPI CSK pin)
*A4 - RF out (= SPI MISO pin)
*A5 - LCD-Reset (= SPI MOSI pin)
 A6 - OP1
*A7 - OP2 (= pwmPin)
 TX/RX - Serial comms
---------------------------------------
-
*/

// GENERAL
STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));  // INT of EXT = Stabiel. Géén AUTO gebruiken: Creert disconnects!
STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

// WiFi RECONNECT VARIABLES
static unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000;  // 30 sec
bool cloudReady = false;

double Hour; // Used to switch solar pump ON/OFF

// WIFI monitoring
int wifiRSSI = WiFi.RSSI();  // ← int

// Memory monitoring
int freemem = System.freeMemory();
int memPERCENT = (freemem * 100) / 82944;  // 80 KB RAM

// Strings for publishing
char str[255]; // Temporary string for all messages published
char data[255]; // Temporary string for all data published
char JSON_temperat[622]; // Publish all temperature variables in one string (Max 622)

// *A2: SPI bus
#include <Adafruit_MAX31865.h>
Adafruit_MAX31865 sensor = Adafruit_MAX31865(A2); // Using hardware SPI1 module: CS = pin A2 (A2=SS,A3=SCK,A4=MISO,A5=MOSI)
//#define RREF 4300.0 // Rref: PT100 = 430.0; PT1000 = 4300.0 => Vervangen door nieuwe regel om vriesprobleem op te lossen!
const float RREF = 4000.0;   // ← 4000.0 (niet 4300!) => PT1000 met standaard Adafruit/Chinese MAX31865 breakout → Rref = 4000 Ω

// *D3 - T-BUS
#include <OneWire.h> //Initialize "OneWire" library
const int oneWirePin = D3; // Compatible to PhotoniX shield...
OneWire ds = OneWire(oneWirePin);

// Store addresses of DS18B20 sensors (Starting with 0x28,) and activate "getTemperatures(0);" in loop() function:
byte addrs0[6][8] = {{0x28,0xFF,0x0D,0x4C,0x05,0x16,0x03,0xC7}, {0x28,0xFF,0x25,0x1A,0x01,0x16,0x04,0xCD}, {0x28,0xFF,0x89,0x19,0x01,0x16,0x04,0x57}, {0x28,0xFF,0x21,0x9F,0x61,0x15,0x03,0xF9}, {0x28,0xFF,0x16,0x6B,0x00,0x16,0x03,0x08}, {0x28,0xFF,0x90,0xA2,0x00,0x16,0x04,0x76}}; // = For 6 "DS18B20" sensors in ECO buffer

// Initialize the names of the sensors as double variables: (=> can be published as "Particle.variables")
double ETopH, ETopL, EMidH, EMidL, EBotH, EBotL; // = 6 sensors (012345) in the ECO boiler
double* temps[] = {&ETopH, &ETopL, &EMidH, &EMidL, &EBotH, &EBotL}; // Group1: 6 sensors (012345) in the ECO boiler

// Initialize the globals for time stamp (Faulty sensor reporting via CRC checking):
char crcErrorJSON[128];
int crcErrorCount[sizeof(temps)/sizeof(temps[0])];
uint32_t tmStamp[sizeof(temps)/sizeof(temps[0])];

// Global variables for energy calculations:
int getTemperaturesInterval = 1 * 60 * 1000; // Sample rate for temperatures
int getTemperaturesLastTime = millis() - getTemperaturesInterval;  // Reset so that it samples immediately at start-up!
double celsius;
double ETmin = 35; // Minimum ECO boiler temperature to calculate "spare" energy (Securing Hot water supply)
double EAv1, EAv2, EAv3, EAv4, EAv5, EAv; // Average temperatures
double EQ1, EQ2, EQ3, EQ4, EQ5, EQtot, prev_EQtot, dEQ; // Boiler energy
const unsigned long DEQ_INTERVAL = 10 * 60 * 1000; // Elke 10' een dEQ voor JSON STRING
static unsigned long lastDEQcalc = 0;

// Define variables for SOLAR controller: *A7: PWM, *D2: relay
int relayPin = D2;
bool relayState = false; // Initial state of relayPin
double relay = 0; // Initial state of relay = 0 (OFF)
int pwmPin = A7; // On PhotoniX shield, connect these pins (A7, 5V, Gnd) and increase PWM voltage from 3.3v to 5V with OpAmp!

double Tboil = 0; // Boiler temperature where SOLAR liquid enters: EBotH
double Tsun = 0;
double dT = Tsun - Tboil;
double Hysteresis = 1;
double pwmValue = 0; // 0-255 (Use double to calculate)
int consecutiveReductions = 0;  // Initialize counter for consecutive energy reductions (dEQ<=0)

// Define timer to call solarPump function
const unsigned long pumpInterval = 1 * 60 * 1000;
unsigned long pumpTimer = millis() - pumpInterval;





// Handmatige berekening van PT1000 temperatuur (omzeilt bibliotheekbug: Foute berekening van Tsol als het vriest!)
float readSolarTemp() {
  uint16_t rtd = sensor.readRTD();
  if (rtd == 0 || rtd > 32768) {
    return -127.0;
  }
  float ratio = rtd / 32768.0;
  float resistance = ratio * 4000.0;
  float temperature = (resistance - 1000.0) / 3.850;
  if (temperature < -50 || temperature > 200 || isnan(temperature)) {
    return -127.0;
  }
  return temperature;
}




void setup()
{
  SYSTEM_THREAD(ENABLED);
  WiFi.listen(false);// schakelt "WiFi scanning mode" uit: verstoort connectie bij zwak signaal!
  Particle.connect();
  waitUntil(Particle.connected);  // WACHT TOT WIFI + CLOUD KLAAR IS!

  Time.zone(+2); // Set clock to Belgium time (+1 in winter, +2 in summer)

  // *D3 - T-BUS
  for (int i = 0; i < sizeof(temps)/sizeof(temps[0]); i++) // @BulldogLowell: Initialize the timestamp array: prevent wrong messages if you get a bad CRC error on the first reading after startup...
  {
    tmStamp[i] = Time.now();
  }

  // Initialize pin mode for SOLAR controller
  pinMode(relayPin, OUTPUT);
  pinMode(pwmPin, OUTPUT);

  // Make sure the pump is OFF when starting:
  digitalWrite(relayPin, HIGH);
  relayState = false; relay = 0;

  // Start SPI bus for MAX31865 module:
  SPI.begin();
  sensor.begin(MAX31865_2WIRE); // set to 2WIRE connection

  consecutiveReductions = 0;

  // GENERAL Particle functions:
  Particle.function("Manual", manual);

  // Initialize Particle variables:
  Particle.variable("JSON_temper", JSON_temperat, STRING); // Publishes all system temperatures

  // Report the CRC errors with sensor ID:
  Particle.variable("CRC_Errors", crcErrorJSON, STRING); // It creates an array of errorcounts of all active sensors. Example: {"errorCount":[17,4,4,14,8,3]} => 17 = sensor 0, 4 = sensor 1, etc...
}




void loop()
{
  Hour = Time.hour();

  // Memory monitoring
    freemem = System.freeMemory();
    memPERCENT = (freemem * 100) / 82944;  // 80 KB RAM

  // === STABIELE RECONNECTIE MET COOLDOWN ===
  if (!Particle.connected())
  {
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL)
    {
      Particle.connect();
      lastReconnectAttempt = millis();
    }
  }
  else if (!cloudReady)
  {
    cloudReady = true;
  }

  // === TEMPERATUREN & ENERGIE (elke minuut) ===
  if ((millis() - getTemperaturesLastTime) > getTemperaturesInterval)
  {
    getTemperatures(0);
    getTemperaturesLastTime = millis();

    // --- ENERGIEBEREKENINGEN ---
    EAv1 = (ETopH + ETopL)/2;
    EAv2 = (ETopL + EMidH)/2;
    EAv3 = (EMidH + EMidL)/2;
    EAv4 = (EMidL + EBotH)/2;
    EAv5 = (EBotH + EBotL)/2;
    EAv = (EAv1+EAv2+EAv3+EAv4+EAv5)/5;

    EQ1 = (EAv1-ETmin)*110*1.163/1000;
    EQ2 = (EAv2-ETmin)*90*1.163/1000;
    EQ3 = (EAv3-ETmin)*90*1.163/1000;
    EQ4 = (EAv4-ETmin)*90*1.163/1000;
    EQ5 = (EAv5-ETmin)*110*1.163/1000;
    EQtot = EQ1+EQ2+EQ3+EQ4+EQ5;

    if (millis() - lastDEQcalc >= DEQ_INTERVAL)
    {
      dEQ = (EQtot - prev_EQtot);
      prev_EQtot = EQtot;
      lastDEQcalc = millis();
    }


    Tsun = readSolarTemp();
    Tboil = EBotH;
    dT = Tsun - Tboil;


    // --- LIVE WIFI & MEM ---
    wifiRSSI = WiFi.RSSI();  // ← int

    // --- JSON ---
    snprintf(JSON_temperat, sizeof(JSON_temperat), "{"
      "\"ETopH\":%.1f,\"ETopL\":%.1f,\"EMidH\":%.1f,\"EMidL\":%.1f,"
     "\"EBotH\":%.1f,\"EBotL\":%.1f,\"EAv\":%.1f,\"EQtot\":%.2f,"
     "\"Solar\":%.1f,\"dT\":%.1f,\"dEQ\":%.3f,\"pwmVal\":%.0f,"
      "\"Relay\":%.0f,\"WiFiSig\":%d,\"Mem\":%d"
    "}",
      ETopH, ETopL, EMidH, EMidL, EBotH, EBotL, EAv, EQtot,
      Tsun, dT, dEQ, pwmValue, relay,
      wifiRSSI, memPERCENT
    );

    // --- EVACUATE (HVAC) ---
    static unsigned long lastEvacuate = 0;
    if (EQtot > 15 && millis() - lastEvacuate >= 300000)
    {
      sprintf(str, "ECO: %.2f kWh", EQtot);
      if (Particle.connected())
      {
        Particle.publish("Status-HEAT:HVAC", str, PRIVATE);
      }
      lastEvacuate = millis();
    }

    // --- PUBLISH STATUS (alleen elke 5 min) ---
    static unsigned long lastSolarPublish = 0;
    if (millis() - lastSolarPublish >= 300000)
    {
      if (Particle.connected())
      {
        Particle.publish("Solar", str, PRIVATE);
      }
      lastSolarPublish = millis();
    }
  }

  // === POMP (elke minuut) ===
  if (millis() - pumpTimer >= pumpInterval)
  {
    solarPump();
    pumpTimer = millis();
  }
}




// Functions

// Define function to map a value from one range to another in solarPump() function
float mapRange(float value, float inputMin, float inputMax, float outputMin, float outputMax)
{
  return (value - inputMin) * (outputMax - outputMin) / (inputMax - inputMin) + outputMin;
}






// Control solar pump speed (PWM value) and ON/OFF state – DEFINITIEF 4-12-2025
void solarPump() {
  // Nachtblokkering
  if (Hour < 7 || Hour >= 21) {
    digitalWrite(relayPin, HIGH);
    relayState = false; relay = 0; pwmValue = 0;
    sprintf(str, "Pump OFF - Nachtblokkering");
    analogWrite(pwmPin, 0);
    consecutiveReductions = 0;
    return;
  }

  bool shouldBeOn = (dT > 3.0);

  // Thermosifon blokkeren
  if (dT > 3.0 && Tsun < 22.0) {
    shouldBeOn = false;
    sprintf(str, "Pump OFF - Thermosifon (Tsun=%.1fC)", Tsun);
  }

  // Verlies-streak alleen als niet geblokkeerd door thermosifon
  if (shouldBeOn) {
    if (dEQ > 0.0) {
      consecutiveReductions = 0;
    } else if (dEQ <= 0.0) {
      consecutiveReductions++;
      if (consecutiveReductions >= 3) {
        shouldBeOn = false;
      }
    }
  }

  // Oververhitting
  if (Tsun >= 90.0) {
    shouldBeOn = true;
    pwmValue = 255;
    sprintf(str, "Pump MAX - Collector >= 90C");
  }

  if (shouldBeOn) {
    if (!relayState) {
      sprintf(str, "Pump STARTED - dT=%.1fC", dT);
      consecutiveReductions = 0;
    }
    digitalWrite(relayPin, LOW);
    relayState = true; relay = 1;

    if (Tsun < 90.0) {
      if (Tsun > 75.0) pwmValue = 180;
      else {
        float delta = constrain(dT - 3.0, 0.0, 17.0);
        pwmValue = 80 + (uint16_t)(delta * 120.0 / 17.0 + 0.5);
      }
    }
    sprintf(str, "Pump ON - dT=%.1f Tsun=%.1f PWM=%d", dT, Tsun, (int)pwmValue);
  } else {
    if (relayState) sprintf(str, "Pump OFF - geen zon of verlies-streak");
    digitalWrite(relayPin, HIGH);
    relayState = false; relay = 0; pwmValue = 0;
  }

  analogWrite(pwmPin, pwmValue);
}







// *D3 - T-BUS
void discoverOneWireDevices(void) // T-BUS Scanner function  => List addresses of all 1-wire devices on the T-BUS (D3) and publish to Particle cloud. For example T-BUS DS18B20 sensors => discoverOneWireDevices();
{
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  int sensorCount = 0;

  Particle.publish("OneWire", "Looking for 1-wire addresses:", 60, PRIVATE); delay(500);

  while(ds.search(addr) and sensorCount < 20)
  {
    sensorCount++;
    char newAddress[48] = ""; // Make space for the 48 characters of our sensor addresses.
    snprintf(newAddress, sizeof(newAddress), "0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
    Particle.publish("OneWire", newAddress, 60, PRIVATE); delay(500);

    if ( OneWire::crc8( addr, 7) != addr[7])
    {
      Particle.publish("OneWire", "CRC is not valid!", 60, PRIVATE); delay(500);
      return;
    }
  }

  ds.reset_search();
  Particle.publish("OneWire", "No more addresses!", 60, PRIVATE); delay(500);
  return;
}




// T-BUS collecting temperatures function => Reviewed by Grok!
void getTemperatures(int select)
{
  static unsigned long conversionStart = 0;
  static bool conversionRequested = false;

  // Stap 1: Start conversie (alleen 1x per interval)
  if (!conversionRequested && (millis() - getTemperaturesLastTime > getTemperaturesInterval)) {
    ds.reset();
    ds.skip();
    ds.write(0x44, 0);  // Start temperatuurconversie
    conversionStart = millis();
    conversionRequested = true;
    return;
  }

  // Stap 2: Wacht 1 sec, dan lezen
  if (conversionRequested && (millis() - conversionStart >= 1000)) {
    ds.reset();
    conversionRequested = false;
    getTemperaturesLastTime = millis();

    for (int i = 0; i < sizeof(temps)/sizeof(temps[0]); i++) {
      ds.select(addrs0[i]);
      ds.write(0xBE, 0);
      byte scratchpadData[9];
      for (int j = 0; j < 9; j++) {
        scratchpadData[j] = ds.read();
      }
      byte currentCRC = OneWire::crc8(scratchpadData, 8);
      ds.reset();

      if (currentCRC != scratchpadData[8]) {
        char msg[64];
        if (Time.now() - tmStamp[i] > 3600UL) {
          snprintf(msg, sizeof(msg), "Sensor Timeout on sensor: %d", i);
        } else {
          snprintf(msg, sizeof(msg), "Bad reading on Sensor: %d", i);
        }

        if (Particle.connected()) { // Voorkomt queue-opbouw bij WiFi-drops
          Particle.publish("Alerts", msg, 60, PRIVATE);
        }

        crcErrorCount[i]++;
        continue;
      }

      tmStamp[i] = Time.now();
      int16_t raw = (scratchpadData[1] << 8) | scratchpadData[0];
      celsius = (double)raw * 0.0625;
      *temps[i] = celsius;
    }

    // Bouw CRC JSON
    strcpy(crcErrorJSON, "{\"errorCount\":[");
    for (int i = 0; i < 6; i++) {
      char buf[8];
      itoa(crcErrorCount[i], buf, 10);
      strcat(crcErrorJSON, buf);
      if (i < 5) strcat(crcErrorJSON, ",");
    }
    strcat(crcErrorJSON, "]}");
  }
}


// Particle.function to remote control manually. Can also be called from the loop(): ex = manual("Report");
int manual(String command)
{
  if(command == "report")
  {
    // WiFi reception
    wifiRSSI = WiFi.RSSI();
    sprintf(str, "wifiRSSI: %2.0f",wifiRSSI);
    Particle.publish("Status-ROOM", str,60,PRIVATE); delay(500);

    // Memory monitoring
    memPERCENT = (freemem/51944)*100;
    sprintf(str, "Freememory + MemPct: %2.0f + %2.0f",freemem,memPERCENT);
    Particle.publish("Status-ROOM", str,60,PRIVATE); delay(500);

    // Boiler energy & temps
    Particle.publish("Solar", JSON_temperat,60,PRIVATE); delay(500);
    // Sensor IDs
    discoverOneWireDevices();

    return 1000;
  }

  if(command == "on")
  {
    digitalWrite(relayPin, LOW);
    relayState = true; relay = 1;
    sprintf(str, "Pump relay ON (Manual)");
    Particle.publish("Solar", str);
    return 255;
  }

  if(command == "off")
  {
    digitalWrite(relayPin, LOW);
    relayState = false; relay = 0;
    sprintf(str, "Pump relay OFF (Manual)");
    Particle.publish("Solar", str);
    return 0;
  }

  if(command == "reset") // You can remotely RESET the photon with this command...
  {
    System.reset();
    return -10000;
  }

  Particle.publish("Solar", command,60,PRIVATE);// If it does not match one of the above, publish the received string to see what it's "payload" was...
  return -1;// If none above
}
