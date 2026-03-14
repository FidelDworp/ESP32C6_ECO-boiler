/* ESP32-C6_MATTER_ECO-boiler.ino - Solar & Fireplace Energy Controller
   Author: Fidel Dworp

   OPGEPAST: Compileer met "partitions_16mb.csv" in de schetsmap:
   # Name,   Type, SubType, Offset,   Size,    Flags
   nvs,      data, nvs,     0x9000,   0x5000,
   otadata,  data, ota,     0xe000,   0x2000,
   app0,     app,  ota_0,   0x10000,  0x600000,
   app1,     app,  ota_1,   0x610000, 0x600000,
   spiffs,   data, spiffs,  0xC10000, 0x3F0000,

   Board: ESP32C6 Dev Module | Flash: 16MB | Partition: Custom partitions_16mb.csv
   Libraries: ESPAsyncWebServer, Adafruit_MAX31865, OneWireNg, arduino-esp32-Matter

   Version 1.22 (13 mar 2026)
     #define Serial Serial0 (verplicht ESP32-C6 fix)
     MDNS.begin() verwijderd (conflicteert met Matter interne mDNS-stack)
     DS18B20 CONVERT_ALL broadcast (0xCC+0x44): 4500ms -> 750ms blokkering
     Heap-monitoring: largest_block + min_free_heap in /json + UI + serieel
     Crash-log NVS (namespace crash-log): schrijft bij largest_block < 25KB
     Auto-recovery corrupt Matter NVS na Matter.begin()
     JSON: compacte a/b/c keys (a-s, 19 velden), pump_status verwijderd
     UI: chunked streaming (html.reserve(50000) weg), geen Chart.js op hoofdpagina
     UI: temperatuurbalken Tsun + EQtot + 6 boilerlagen (kleurcodering)
     UI: getPumpMessage() verwijderd, SYSTEEM sectie ingekort
     /charts: nieuwe pagina met alle 3 grafieken (Chart.js)
     Matter-sectie toegevoegd aan /settings (geen aparte /matter pagina)
     calculatePWM() return type gecorrigeerd: float -> int
   Version 1.21 (11 mar 2026) Google Sheets logging verwijderd — overgebracht naar Zarlar Dashboard (192.168.0.60). /json endpoint blijft intact.
   Version 1.20 (10 mar 2026) Stuurt nu ook de HVAC JSON data door naar Google, omdat die controller dat niet kan. (memory probleem).
   Version 1.18 (1 mar 2026) MATTER integrated
   Version 1.17 (26 feb 2026) WiFi FIXED IP in telenet router: Config = 192.168.0.71 (Zie tabel)
   Version 1.16 (23 jan 2026) CRITICAL FIX: WiFi power save NA WiFi.begin() (was ervoor!)
   Version 1.15 (22 jan 2026) Improve connection with browser to UI: in setup(): esp_wifi_set_ps(WIFI_PS_NONE);
   Version 1.14 (22 jan 2026) GOOGLE WEBHOOK: LOGGING ON-LINE: Works!
   Version 1.13 (21 jan 2026) PRODUCTION READY = < VOLLEDIG WERKEND! >
     UDP keepalive (98.5% uptime proven)
     Silent logging (log only problems)
     Universal logging framework
     Settings UI: log view/clear/restart

   SIMULATION MODE: Gebruik "HUGE APP 3Mb No OTA" partition!
*/

// VERPLICHT voor ESP32-C6 in Arduino IDE (RISC-V serieel fix)
#define Serial Serial0

// ============== INCLUDES ==============
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include <OneWireNg_CurrentPlatform.h>
#include <WiFiUdp.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <Matter.h>
#include <MatterEndPoints/MatterTemperatureSensor.h>
#include <MatterEndPoints/MatterHumiditySensor.h>
#include <MatterEndPoints/MatterFan.h>

// ============== LOGGING SYSTEM ==============
#define LOG_WARN   "WARN"
#define LOG_ERROR  "ERR"
#define LOG_INFO   ""

extern unsigned long uptime_sec;
bool log_initialized = false;

bool initLogging() {
  if (!SPIFFS.begin(true)) return false;
  if (!SPIFFS.exists("/debug.log")) {
    File f = SPIFFS.open("/debug.log", FILE_WRITE);
    if (f) f.close();
  }
  log_initialized = true;
  return true;
}

void logEvent(const char* level, const char* msg) {
  if (!log_initialized) return;
  File f = SPIFFS.open("/debug.log", FILE_APPEND);
  if (!f) return;
  char ts[16];
  time_t now; time(&now);
  struct tm ti; localtime_r(&now, &ti);
  if (now < 1700000000) {
    snprintf(ts, 16, "%02d:%02d:%02d",
      (int)(uptime_sec/3600), (int)((uptime_sec%3600)/60), (int)(uptime_sec%60));
  } else {
    strftime(ts, 16, "%H:%M:%S", &ti);
  }
  f.printf("[%s] %s %s\n", ts, level, msg);
  f.close();
  File logFile = SPIFFS.open("/debug.log", FILE_READ);
  if (logFile.size() > 819200) {
    logFile.close();
    SPIFFS.remove("/debug.log.old");
    SPIFFS.rename("/debug.log", "/debug.log.old");
  } else { logFile.close(); }
}

void logWarn(const char* msg)  { logEvent(LOG_WARN, msg); }
void logError(const char* msg) { logEvent(LOG_ERROR, msg); }
void logInfo(const char* msg)  { logEvent(LOG_INFO, msg); }

// ============== PIN DEFINITIONS ==============
bool SIMULATION_MODE = false;

#define ONEWIRE_PIN  3
#define RELAY_PIN    1
#define PWM_PIN      5
#define SPI_CS      20
#define SPI_MOSI    21
#define SPI_MISO    22
#define SPI_SCK     23

#define PWM_FREQ       1000
#define PWM_RESOLUTION 8

// ============== CONSTANTS ==============
float DT_START_THRESHOLD = 3.0;
float DT_STOP_THRESHOLD  = 2.0;
float TSUN_MIN_TEMP      = 22.0;
float TSUN_OVERHEAT      = 90.0;
float TSUN_HIGH          = 75.0;
int   MAX_LOSS_STREAK    = 3;
int   PWM_MIN            = 80;
int   PWM_MAX            = 200;
int   PWM_OVERHEAT       = 255;

float ETMIN              = 35.0;
float GLYCOL_PERCENT     = 0.0;
float BOILER_VOLUME_TOTAL= 490.0;
float ZONE_VOLUMES[5]    = {110.0, 90.0, 90.0, 90.0, 110.0};

const unsigned long SENSOR_INTERVAL      = 60000;
const unsigned long PUMP_CHECK_INTERVAL  = 60000;
const unsigned long DEQ_INTERVAL         = 600000;
const unsigned long HVAC_PUBLISH_INTERVAL= 300000;

const float RREF    = 4000.0;
const float RNOMINAL= 1000.0;

int   HOUR_START = 7;
int   HOUR_END   = 21;
float HVAC_TRANSFER_THRESHOLD = 15.0;

// ============== GLOBAL OBJECTS ==============
Preferences preferences;
AsyncWebServer server(80);
DNSServer dnsServer;

Adafruit_MAX31865 pt1000 = Adafruit_MAX31865(SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCK);
OneWireNg_CurrentPlatform ow(ONEWIRE_PIN, false);

// ============== CONFIG STRUCT ==============
struct Config {
  char room_id[32];
  char wifi_ssid[64];
  char wifi_pass[64];
  char static_ip[16];
  char hvac_ip[16];
  char hvac_mdns[32];
  bool hvac_enabled;
  float dt_start, dt_stop, tsun_min, tsun_overheat, tsun_high;
  int max_loss_streak, pwm_min, pwm_max, pwm_overheat;
  float etmin, glycol_percent, boiler_volume, hvac_threshold;
} config;

String mac_address = "";

String sensor_nicknames[6] = {
  "ETopH (Top High)", "ETopL (Top Low)",
  "EMidH (Mid High)", "EMidL (Mid Low)",
  "EBotH (Bottom High)", "EBotL (Bottom Low)"
};

// ============== SENSOR DATA ==============
OneWireNg::Id boilerSensors[6] = {
  {0x28,0xFF,0x0D,0x4C,0x05,0x16,0x03,0xC7}, // ETopH
  {0x28,0xFF,0x25,0x1A,0x01,0x16,0x04,0xCD}, // ETopL
  {0x28,0xFF,0x89,0x19,0x01,0x16,0x04,0x57}, // EMidH
  {0x28,0xFF,0x21,0x9F,0x61,0x15,0x03,0xF9}, // EMidL
  {0x28,0xFF,0x16,0x6B,0x00,0x16,0x03,0x08}, // EBotH
  {0x28,0xFF,0x90,0xA2,0x00,0x16,0x04,0x76}  // EBotL
};

float ETopH=0,ETopL=0,EMidH=0,EMidL=0,EBotH=0,EBotL=0;
float EAv=0, Tsun=0, Tboil=0, dT=0;
float EQtot=0, dEQ=0, prev_EQtot=0;

bool  pump_relay    = false;
int   pwm_value     = 0;
int   consecutive_reductions = 0;

bool pump_override_active = false;
bool pump_override_state  = false;
unsigned long pump_override_start = 0;
const unsigned long PUMP_OVERRIDE_DURATION = 60000UL;

int  pwm_override        = 0;
bool ignore_callbacks    = false;
unsigned long last_matter_update = 0;
float EQ_MAX_KWH = 25.0f;

// ============== MATTER ENDPOINTS ==============
MatterTemperatureSensor matter_tsun;
MatterTemperatureSensor matter_etoph;
MatterTemperatureSensor matter_eboth;
MatterHumiditySensor    matter_eqpct;
MatterFan               matter_pomp;

// ============== TIMING ==============
unsigned long last_sensor_read  = 0;
unsigned long last_pump_check   = 0;
unsigned long last_deq_calc     = 0;
unsigned long last_hvac_publish = 0;
unsigned long uptime_sec        = 0;
unsigned long last_uptime_update= 0;

bool ap_mode   = false;
int  wifi_rssi = 0;

unsigned long last_keepalive = 0;
const unsigned long KEEPALIVE_INTERVAL = 30000UL;

float yield_today       = 0;
int   pump_minutes_today= 0;
int   pump_starts_today = 0;
unsigned long pump_on_start = 0;

unsigned long boot_time_ms = 0;

// ============== GRAPH DATA ==============
#define GRAPH_SAMPLES 60
struct GraphData {
  float tsun[GRAPH_SAMPLES];
  float tboil[GRAPH_SAMPLES];
  float dt[GRAPH_SAMPLES];
  float eqtot[GRAPH_SAMPLES];
  float deq[GRAPH_SAMPLES];
  int   pwm[GRAPH_SAMPLES];
  int   index;
} graph_data;

// ============== NVS KEYS ==============
#define NVS_NAMESPACE    "eco-config"
#define NVS_ROOM_ID      "room_id"
#define NVS_WIFI_SSID    "wifi_ssid"
#define NVS_WIFI_PASS    "wifi_pass"
#define NVS_STATIC_IP    "static_ip"
#define NVS_DT_START     "dt_start"
#define NVS_DT_STOP      "dt_stop"
#define NVS_TSUN_MIN     "tsun_min"
#define NVS_TSUN_OVERHEAT "tsun_overheat"
#define NVS_TSUN_HIGH    "tsun_high"
#define NVS_MAX_LOSS_STREAK "max_loss"
#define NVS_PWM_MIN      "pwm_min"
#define NVS_PWM_MAX      "pwm_max"
#define NVS_PWM_OVERHEAT "pwm_overheat"
#define NVS_ETMIN        "etmin"
#define NVS_GLYCOL_PCT   "glycol_pct"
#define NVS_BOILER_VOL   "boiler_vol"
#define NVS_ZONE_VOL_BASE "zone_vol_"
#define NVS_HOUR_START   "hour_start"
#define NVS_HOUR_END     "hour_end"
#define NVS_HVAC_ENABLED "hvac_enabled"
#define NVS_HVAC_IP      "hvac_ip"
#define NVS_HVAC_MDNS    "hvac_mdns"
#define NVS_HVAC_THRESH  "hvac_thresh"
#define NVS_SENSOR_NICK_BASE "sensor_"
#define NVS_SIMULATION_MODE  "sim_mode"
#define NVS_EQ_MAX_KWH   "eq_max_kwh"

// ============== FUNCTION DECLARATIONS ==============
void loadConfig();
void saveConfig();
void factoryReset();
void setupWiFi();
void setupSensors();
void setupPump();
void setupWebServer();
void readSensors();
void calculateEnergy();
void checkPumpLogic();
void controlPump(bool state, int pwm);
void publishHVAC();
void addGraphSample();
String getGraphDataJSON();
String getFormattedDateTime();
float readPT1000();
int   calculatePWM(float dT, float Tsun);

// ============== HELPER: kleur op basis van boilertemperatuur ==============
const char* tempColor(float t) {
  if (t < 20.0f) return "#0af";
  if (t < 35.0f) return "#48f";
  if (t < 50.0f) return "#0a0";
  if (t < 65.0f) return "#fa0";
  if (t < 80.0f) return "#f60";
  return "#c00";
}

// ============== MATTER HELPERS ==============
uint8_t pwm_to_pct(int pwm) {
  return (uint8_t)constrain((int)round(pwm / 255.0f * 100.0f), 0, 100);
}
int pct_to_pwm(uint8_t pct) {
  return constrain((int)round(pct / 100.0f * 255.0f), 0, 255);
}
uint8_t eq_to_pct(float kWh) {
  if (EQ_MAX_KWH <= 0.0f) return 0;
  return (uint8_t)constrain((int)round(kWh / EQ_MAX_KWH * 100.0f), 0, 100);
}

void check_pump_override() {
  if (pump_override_active &&
      millis() - pump_override_start > PUMP_OVERRIDE_DURATION) {
    pump_override_active = false;
    pump_override_state  = false;
    pwm_override         = 0;
    Serial.println(F("[OVERRIDE] Pomp — vervallen na 60s, terug naar auto"));
  }
}

void update_matter_sensors() {
  matter_tsun.setTemperature(Tsun);
  matter_etoph.setTemperature(ETopH);
  matter_eboth.setTemperature(EBotH);
  matter_eqpct.setHumidity(eq_to_pct(EQtot));
  ignore_callbacks = true;
  int effective_pwm = pump_override_active ? pwm_override : pwm_value;
  matter_pomp.setSpeedPercent(pwm_to_pct(effective_pwm));
  if (!pump_override_active) {
    matter_pomp.setMode(effective_pwm > 0
      ? MatterFan::FAN_MODE_HIGH : MatterFan::FAN_MODE_OFF);
  }
  ignore_callbacks = false;
}

// =========
// SETUP
// =========
void setup() {
  Serial.begin(115200);
  delay(1000);
  boot_time_ms = millis();

  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted OK");
  } else {
    Serial.println("WARN SPIFFS mount failed");
  }

  Serial.println("\n\n=== ESP32 ECO Controller V1.22 ===");

  char boot_msg[100];
  snprintf(boot_msg, sizeof(boot_msg), "heap=%dKB", ESP.getFreeHeap()/1024);
  logEvent("BOOT", boot_msg);

  Serial.println("\nFactory reset? Type 'R' within 3 seconds...");
  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (Serial.available() && Serial.read() == 'R') {
      factoryReset();
      break;
    }
  }

  loadConfig();
  setupPump();
  setupSensors();
  memset(&graph_data, 0, sizeof(graph_data));
  setupWiFi();
  setupWebServer();

  // ── Matter initialisatie ────────────────────────────────────────────────
  if (!ap_mode) {
    Serial.println(F("\n── Matter initialisatie ────────────────────────────────"));
    Serial.printf("EQ_MAX_KWH = %.1f kWh\n", EQ_MAX_KWH);

    matter_tsun.begin();
    matter_etoph.begin();
    matter_eboth.begin();
    matter_eqpct.begin();
    matter_pomp.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_HIGH);

    matter_pomp.onChangeSpeedPercent([](uint8_t new_pct) -> bool {
      if (ignore_callbacks) return true;
      int pwm = pct_to_pwm(new_pct);
      pwm_override = pwm;
      if (pwm == 0) {
        pump_override_active = false;
        pump_override_state  = false;
        Serial.println(F("[HomeKit] Pomp speed = 0% -> terug naar auto"));
      } else {
        pump_override_start  = millis();
        pump_override_active = true;
        pump_override_state  = true;
        Serial.printf("[HomeKit] Pomp speed override -> %d%% = PWM %d\n", new_pct, pwm);
      }
      return true;
    });

    matter_pomp.onChangeMode([](uint8_t new_mode) -> bool {
      if (ignore_callbacks) return true;
      if (new_mode == MatterFan::FAN_MODE_OFF) {
        pump_override_active = false;
        pump_override_state  = false;
        pwm_override         = 0;
        ignore_callbacks = true;
        matter_pomp.setSpeedPercent(0);
        ignore_callbacks = false;
        Serial.println(F("[HomeKit] Pomp mode OFF -> terug naar auto"));
      } else {
        if (!pump_override_active) {
          pwm_override         = PWM_MIN;
          pump_override_start  = millis();
          pump_override_active = true;
          pump_override_state  = true;
          ignore_callbacks = true;
          matter_pomp.setSpeedPercent(pwm_to_pct(PWM_MIN));
          ignore_callbacks = false;
          Serial.printf("[HomeKit] Pomp mode AAN -> override PWM=%d\n", PWM_MIN);
        }
      }
      return true;
    });

    // v1.22: Heap rapport voor Matter
    uint32_t heap_pre = ESP.getFreeHeap();
    uint32_t lb_pre   = ESP.getMaxAllocHeap();
    Serial.printf("[HEAP pre-Matter]  free=%uKB largest=%uKB min_ever=%uKB\n",
      heap_pre/1024, lb_pre/1024, ESP.getMinFreeHeap()/1024);

    Matter.begin();

    // v1.22: Auto-recovery corrupt Matter NVS
    delay(200);
    if (!Matter.isDeviceCommissioned() && Matter.getManualPairingCode().length() < 5) {
      Serial.println("[MATTER] Corrupt NVS — auto-erase + restart");
      nvs_flash_erase();
      nvs_flash_init();
      ESP.restart();
    }

    uint32_t heap_post = ESP.getFreeHeap();
    uint32_t lb_post   = ESP.getMaxAllocHeap();
    Serial.printf("[HEAP post-Matter] free=%uKB largest=%uKB min_ever=%uKB\n",
      heap_post/1024, lb_post/1024, ESP.getMinFreeHeap()/1024);
    Serial.printf("[HEAP Matter kost] free:-%dKB largest:-%dKB\n",
      (int)(heap_pre-heap_post)/1024, (int)(lb_pre-lb_post)/1024);

    Serial.println(F("\n══════════════════════════════════════════"));
    if (!Matter.isDeviceCommissioned()) {
      Serial.println(F("MATTER: Nog niet gepaard."));
      Serial.println(F("Manuele code:"));
      Serial.println("    " + Matter.getManualPairingCode());
      Serial.println(F("Home app -> + -> Accessoire -> Meer opties -> code invoeren"));
      unsigned long t0 = millis();
      while (!Matter.isDeviceCommissioned() && millis() - t0 < 300000UL) {
        delay(500); Serial.print(".");
      }
      if (Matter.isDeviceCommissioned()) Serial.println(F("\nGEPAARD!"));
      else Serial.println(F("\nTimeout — verder zonder pairing."));
    } else {
      Serial.println(F("MATTER: Al gepaard. Typ 'reset-matter' om te wissen."));
    }
    Serial.println(F("══════════════════════════════════════════\n"));
  }
  // ── Einde Matter ─────────────────────────────────────────────────────────

  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0/02,M10.5.0/03", 1);
  tzset();

  if (initLogging()) Serial.println("Logging system initialized");

  // v1.22: Heap-rapport na volledige setup
  Serial.printf("[HEAP setup done] free=%uKB largest=%uKB min_ever=%uKB\n",
    ESP.getFreeHeap()/1024, ESP.getMaxAllocHeap()/1024, ESP.getMinFreeHeap()/1024);

  Serial.println("\n=== Setup Complete ===\nReady!");
}

// ============
// MAIN LOOP
// ============
void loop() {
  if (ap_mode) dnsServer.processNextRequest();

  if (millis() - last_uptime_update >= 1000) {
    uptime_sec++;
    last_uptime_update = millis();
  }

  if (millis() - last_sensor_read >= SENSOR_INTERVAL) {
    readSensors();
    calculateEnergy();
    addGraphSample();
    last_sensor_read = millis();
  }

  if (millis() - last_deq_calc >= DEQ_INTERVAL) {
    dEQ = EQtot - prev_EQtot;
    prev_EQtot = EQtot;
    last_deq_calc = millis();
    if (dEQ > 0) yield_today += dEQ;
    Serial.printf("dEQ: %.3f kWh/10min (Yield today: %.1f kWh)\n", dEQ, yield_today);
  }

  if (millis() - last_pump_check >= PUMP_CHECK_INTERVAL) {
    checkPumpLogic();
    last_pump_check = millis();
  }

  if (config.hvac_enabled && EQtot > config.hvac_threshold) {
    if (millis() - last_hvac_publish >= HVAC_PUBLISH_INTERVAL) {
      publishHVAC();
      last_hvac_publish = millis();
    }
  }

  // Reset dagelijkse stats om middernacht
  time_t now; struct tm timeinfo;
  time(&now); localtime_r(&now, &timeinfo);
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec < 2) {
    yield_today = 0; pump_minutes_today = 0; pump_starts_today = 0;
  }

  // Serial commando's
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("reset-matter")) {
      Serial.println(F("Matter pairing wissen..."));
      nvs_handle_t h;
      const char* chip_ns[] = {"chip-factory","chip-config","chip-counters","chip-kvs"};
      for (int k = 0; k < 4; k++) {
        if (nvs_open(chip_ns[k], NVS_READWRITE, &h) == ESP_OK) {
          nvs_erase_all(h); nvs_commit(h); nvs_close(h);
        }
      }
      delay(300); ESP.restart();
    } else if (cmd.equalsIgnoreCase("reset-all")) {
      preferences.begin(NVS_NAMESPACE, false);
      preferences.clear(); preferences.end();
      nvs_flash_erase(); delay(300); ESP.restart();
    } else if (cmd.equalsIgnoreCase("status")) {
      uint32_t lb = ESP.getMaxAllocHeap();
      Serial.printf("\n=== ECO Status | Uptime: %lu s ===\n", uptime_sec);
      Serial.printf("Tsun=%.1f C  ETopH=%.1f C  EBotH=%.1f C  EQtot=%.2f kWh (%d%%)\n",
        Tsun, ETopH, EBotH, EQtot, eq_to_pct(EQtot));
      Serial.printf("Pomp: %s  PWM: %d  [%s]\n",
        pump_relay ? "AAN" : "UIT", pwm_value,
        pump_override_active ? "OVR" : "AUT");
      Serial.printf("[HEAP] free=%uKB largest=%uKB min_ever=%uKB\n",
        ESP.getFreeHeap()/1024, lb/1024, ESP.getMinFreeHeap()/1024);
    } else if (cmd.length() == 1) {
      switch (cmd[0]) {
        case 't': readSensors(); calculateEnergy(); Serial.println("Sensor test done!"); break;
        case 'p':
          Serial.println("Pump test ON (PWM 150)...");
          controlPump(true, 150); delay(3000);
          controlPump(false, 0); Serial.println("Done!"); break;
        case 'w':
          Serial.printf("WiFi: %s | IP: %s | RSSI: %d dBm\n",
            ap_mode ? "AP" : WiFi.SSID().c_str(),
            ap_mode ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str(),
            WiFi.RSSI()); break;
        case 'h':
          Serial.println("t=sensors  p=pump  w=wifi  h=help");
          Serial.println("reset-matter  reset-all  status"); break;
      }
    }
  }

  // WiFi monitoring
  static bool was_connected = false;
  bool is_connected = (WiFi.status() == WL_CONNECTED);
  if (was_connected && !is_connected && !ap_mode) {
    logError("WIFI disc"); WiFi.reconnect();
  }
  static unsigned long last_signal_warn = 0;
  if (is_connected && wifi_rssi < -75 && millis() - last_signal_warn > 60000) {
    char msg[20]; snprintf(msg, 20, "WIFI weak r=%d", wifi_rssi);
    logWarn(msg); last_signal_warn = millis();
  }
  was_connected = is_connected;

  // UDP keepalive
  if (!ap_mode && is_connected && millis() - last_keepalive >= KEEPALIVE_INTERVAL) {
    WiFiUDP udp;
    IPAddress gateway = WiFi.gatewayIP();
    if (gateway != IPAddress(0,0,0,0)) {
      udp.beginPacket(gateway, 9);
      udp.write((uint8_t*)"ECO", 3);
      bool ok = udp.endPacket();
      wifi_rssi = WiFi.RSSI();
      if (!ok) logError("KA timeout");
    }
    last_keepalive = millis();
  }

  // v1.22: Heap-bewaking + crash-log NVS (elke 60s via slow-tick)
  static unsigned long last_heap_check = 0;
  if (millis() - last_heap_check >= 60000) {
    last_heap_check = millis();
    uint32_t lb = ESP.getMaxAllocHeap();
    if (lb < 25000) {
      Preferences crashPrefs;
      crashPrefs.begin("crash-log", false);
      uint32_t cnt = crashPrefs.getUInt("count", 0) + 1;
      crashPrefs.putUInt("count", cnt);
      char reason[48];
      snprintf(reason, sizeof(reason), "heap %uKB @ %lus", lb/1024, uptime_sec);
      crashPrefs.putString("reason", reason);
      crashPrefs.end();
      Serial.printf("[HEAP] WARN largest block %uKB — crash-log #%u\n", lb/1024, cnt);
    }
  }

  // Matter override timeout + sensor update
  check_pump_override();
  if (!ap_mode && millis() - last_matter_update > 5000) {
    last_matter_update = millis();
    update_matter_sensors();
  }

  yield();
}

// ===============
// CONFIGURATION
// ===============
void loadConfig() {
  preferences.begin(NVS_NAMESPACE, false);
  String temp;
  temp = preferences.getString(NVS_ROOM_ID, "ECO");
  strncpy(config.room_id, temp.c_str(), sizeof(config.room_id)-1);
  temp = preferences.getString(NVS_WIFI_SSID, "");
  strncpy(config.wifi_ssid, temp.c_str(), sizeof(config.wifi_ssid)-1);
  temp = preferences.getString(NVS_WIFI_PASS, "");
  strncpy(config.wifi_pass, temp.c_str(), sizeof(config.wifi_pass)-1);
  temp = preferences.getString(NVS_STATIC_IP, "");
  strncpy(config.static_ip, temp.c_str(), sizeof(config.static_ip)-1);
  temp = preferences.getString(NVS_HVAC_IP, "");
  strncpy(config.hvac_ip, temp.c_str(), sizeof(config.hvac_ip)-1);
  temp = preferences.getString(NVS_HVAC_MDNS, "hvac");
  strncpy(config.hvac_mdns, temp.c_str(), sizeof(config.hvac_mdns)-1);
  config.hvac_enabled   = preferences.getBool(NVS_HVAC_ENABLED, true);
  config.hvac_threshold = preferences.getFloat(NVS_HVAC_THRESH, HVAC_TRANSFER_THRESHOLD);
  DT_START_THRESHOLD  = preferences.getFloat(NVS_DT_START,      DT_START_THRESHOLD);
  DT_STOP_THRESHOLD   = preferences.getFloat(NVS_DT_STOP,       DT_STOP_THRESHOLD);
  TSUN_MIN_TEMP       = preferences.getFloat(NVS_TSUN_MIN,      TSUN_MIN_TEMP);
  TSUN_OVERHEAT       = preferences.getFloat(NVS_TSUN_OVERHEAT, TSUN_OVERHEAT);
  TSUN_HIGH           = preferences.getFloat(NVS_TSUN_HIGH,     TSUN_HIGH);
  MAX_LOSS_STREAK     = preferences.getInt(NVS_MAX_LOSS_STREAK,  MAX_LOSS_STREAK);
  PWM_MIN             = preferences.getInt(NVS_PWM_MIN,          PWM_MIN);
  PWM_MAX             = preferences.getInt(NVS_PWM_MAX,          PWM_MAX);
  PWM_OVERHEAT        = preferences.getInt(NVS_PWM_OVERHEAT,     PWM_OVERHEAT);
  ETMIN               = preferences.getFloat(NVS_ETMIN,          ETMIN);
  GLYCOL_PERCENT      = preferences.getFloat(NVS_GLYCOL_PCT,     GLYCOL_PERCENT);
  BOILER_VOLUME_TOTAL = preferences.getFloat(NVS_BOILER_VOL,     BOILER_VOLUME_TOTAL);
  for (int i = 0; i < 5; i++) {
    char key[20]; snprintf(key, sizeof(key), "%s%d", NVS_ZONE_VOL_BASE, i);
    ZONE_VOLUMES[i] = preferences.getFloat(key, ZONE_VOLUMES[i]);
  }
  HOUR_START = preferences.getInt(NVS_HOUR_START, HOUR_START);
  HOUR_END   = preferences.getInt(NVS_HOUR_END,   HOUR_END);
  for (int i = 0; i < 6; i++) {
    char key[20]; snprintf(key, sizeof(key), "%s%d", NVS_SENSOR_NICK_BASE, i);
    temp = preferences.getString(key, sensor_nicknames[i]);
    sensor_nicknames[i] = temp;
  }
  config.dt_start        = DT_START_THRESHOLD;
  config.dt_stop         = DT_STOP_THRESHOLD;
  config.tsun_min        = TSUN_MIN_TEMP;
  config.tsun_overheat   = TSUN_OVERHEAT;
  config.tsun_high       = TSUN_HIGH;
  config.max_loss_streak = MAX_LOSS_STREAK;
  config.pwm_min         = PWM_MIN;
  config.pwm_max         = PWM_MAX;
  config.pwm_overheat    = PWM_OVERHEAT;
  config.etmin           = ETMIN;
  config.glycol_percent  = GLYCOL_PERCENT;
  config.boiler_volume   = BOILER_VOLUME_TOTAL;
  SIMULATION_MODE        = preferences.getBool(NVS_SIMULATION_MODE, false);
  EQ_MAX_KWH             = preferences.getFloat(NVS_EQ_MAX_KWH, 25.0f);
  preferences.end();
  Serial.printf("Config loaded: room=%s wifi=%s hvac=%s sim=%d\n",
    config.room_id,
    strlen(config.wifi_ssid)>0 ? config.wifi_ssid : "(none)",
    config.hvac_enabled ? "on" : "off",
    SIMULATION_MODE ? 1 : 0);
}

void saveConfig() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putString(NVS_ROOM_ID,   config.room_id);
  preferences.putString(NVS_WIFI_SSID, config.wifi_ssid);
  preferences.putString(NVS_WIFI_PASS, config.wifi_pass);
  preferences.putString(NVS_STATIC_IP, config.static_ip);
  preferences.putString(NVS_HVAC_IP,   config.hvac_ip);
  preferences.putString(NVS_HVAC_MDNS, config.hvac_mdns);
  preferences.putBool(NVS_HVAC_ENABLED, config.hvac_enabled);
  preferences.putFloat(NVS_HVAC_THRESH, HVAC_TRANSFER_THRESHOLD);
  preferences.putFloat(NVS_DT_START,      DT_START_THRESHOLD);
  preferences.putFloat(NVS_DT_STOP,       DT_STOP_THRESHOLD);
  preferences.putFloat(NVS_TSUN_MIN,      TSUN_MIN_TEMP);
  preferences.putFloat(NVS_TSUN_OVERHEAT, TSUN_OVERHEAT);
  preferences.putFloat(NVS_TSUN_HIGH,     TSUN_HIGH);
  preferences.putInt(NVS_MAX_LOSS_STREAK,  MAX_LOSS_STREAK);
  preferences.putInt(NVS_PWM_MIN,          PWM_MIN);
  preferences.putInt(NVS_PWM_MAX,          PWM_MAX);
  preferences.putInt(NVS_PWM_OVERHEAT,     PWM_OVERHEAT);
  preferences.putFloat(NVS_ETMIN,          ETMIN);
  preferences.putFloat(NVS_GLYCOL_PCT,     GLYCOL_PERCENT);
  preferences.putFloat(NVS_BOILER_VOL,     BOILER_VOLUME_TOTAL);
  for (int i = 0; i < 5; i++) {
    char key[20]; snprintf(key, sizeof(key), "%s%d", NVS_ZONE_VOL_BASE, i);
    preferences.putFloat(key, ZONE_VOLUMES[i]);
  }
  preferences.putInt(NVS_HOUR_START, HOUR_START);
  preferences.putInt(NVS_HOUR_END,   HOUR_END);
  for (int i = 0; i < 6; i++) {
    char key[20]; snprintf(key, sizeof(key), "%s%d", NVS_SENSOR_NICK_BASE, i);
    preferences.putString(key, sensor_nicknames[i]);
  }
  preferences.putBool(NVS_SIMULATION_MODE, SIMULATION_MODE);
  preferences.putFloat(NVS_EQ_MAX_KWH, EQ_MAX_KWH);
  preferences.end();
  Serial.println("Config saved to NVS");
}

void factoryReset() {
  Serial.println("\n*** FACTORY RESET ***");
  preferences.begin(NVS_NAMESPACE, false);
  preferences.clear(); preferences.end();
  Serial.println("NVS cleared. Rebooting...");
  delay(1000); ESP.restart();
}

// ===============
// HARDWARE SETUP
// ===============
void setupPump() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pump_relay = false;
  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(PWM_PIN, 0);
  pwm_value = 0;
  Serial.printf("Pump init: relay=%d pwm=%d\n", RELAY_PIN, PWM_PIN);
}

void setupSensors() {
  pt1000.begin(MAX31865_2WIRE);
  Serial.printf("PT1000 init (SPI CS=%d MOSI=%d MISO=%d SCK=%d)\n",
    SPI_CS, SPI_MOSI, SPI_MISO, SPI_SCK);
  Serial.printf("DS18B20 ready (OneWireNg pin=%d, 6 sensoren)\n", ONEWIRE_PIN);
}

// ===================================================
// WIFI SETUP — static IP + power-save fix v1.16
// ===================================================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.room_id);

  if (strlen(config.wifi_ssid) > 0) {
    Serial.printf("Connecting to '%s'...\n", config.wifi_ssid);
    IPAddress local_IP;
    IPAddress subnet(255,255,255,0);
    IPAddress primaryDNS(8,8,8,8);
    bool useStatic = (strlen(config.static_ip)>0) && local_IP.fromString(config.static_ip);
    IPAddress gateway(local_IP[0], local_IP[1], local_IP[2], 1);
    if (useStatic) Serial.printf("Static IP: %s  Gateway: %s\n",
      config.static_ip, gateway.toString().c_str());

    int retry_count = 0;
    bool connected = false;
    while (!connected && retry_count < 5) {
      if (useStatic) {
        if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
          Serial.println("WiFi.config() MISLUKT -> fallback DHCP");
          logError("WiFi.config() failed");
          useStatic = false;
        }
      }
      WiFi.begin(config.wifi_ssid, config.wifi_pass);
      if (retry_count == 0) mac_address = WiFi.macAddress();
      unsigned long t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis()-t0 < 20000) {
        delay(500); Serial.print(".");
      }
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        Serial.println("\nOK WiFi connected!");
        esp_wifi_set_ps(WIFI_PS_NONE);
        esp_pm_config_t pm_config = {.max_freq_mhz=160,.min_freq_mhz=160,.light_sleep_enable=false};
        esp_pm_configure(&pm_config);
        wifi_config_t wifi_cfg;
        esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
        wifi_cfg.sta.listen_interval = 1;
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        Serial.println("Power-save disabled, CPU@160MHz");
        char msg[60];
        snprintf(msg, sizeof(msg), "ip=%s rssi=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        logEvent("WIFI_CONN", msg);
      } else {
        retry_count++;
        Serial.printf("\nX Attempt %d/5 failed\n", retry_count);
        WiFi.disconnect(); delay(2000);
      }
    }
    if (connected) {
      Serial.printf("IP: %s  RSSI: %d dBm  MAC: %s\n",
        WiFi.localIP().toString().c_str(), WiFi.RSSI(), mac_address.c_str());
      // v1.22: MDNS.begin() verwijderd — conflicteert met Matter interne mDNS-stack
      ap_mode = false;
      return;
    }
  }

  Serial.println("X WiFi failed -> AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ECO-Setup");
  IPAddress ap_ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ap_ip);
  Serial.printf("AP SSID: ECO-Setup  IP: %s\n", ap_ip.toString().c_str());
  ap_mode = true;
}

// ===============
// SENSOR READING
// ===============
float readPT1000() {
  uint16_t rtd = pt1000.readRTD();
  if (rtd == 0 || rtd > 32768) return -127.0;
  float ratio = rtd / 32768.0;
  float resistance = ratio * RREF;
  float temperature = (resistance - RNOMINAL) / 3.850;
  if (temperature < -50 || temperature > 200 || isnan(temperature)) return -127.0;
  return temperature;
}

void readSensors() {
  Serial.println("\n=== Reading Sensors ===");

  if (SIMULATION_MODE) {
    Serial.println("WARN SIMULATION MODE - fake data!");
    time_t now; struct tm timeinfo;
    time(&now); localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    if (hour >= 8 && hour <= 17) {
      float sun_factor = sin((hour-8)*3.14159/9.0);
      Tsun = 25.0 + sun_factor*40.0 + random(-20,20)/10.0;
    } else {
      Tsun = 15.0 + random(-30,30)/10.0;
    }
    ETopH = 60.0 + random(-20,20)/10.0;
    ETopL = 58.0 + random(-20,20)/10.0;
    EMidH = 50.0 + random(-20,20)/10.0;
    EMidL = 48.0 + random(-20,20)/10.0;
    EBotH = 40.0 + random(-20,20)/10.0;
    EBotL = 38.0 + random(-20,20)/10.0;
  } else {
    // PT1000 collector
    Tsun = readPT1000();
    Serial.printf("Tsun: %.1f C\n", Tsun);

    // v1.22: CONVERT_ALL broadcast (0xCC + 0x44) — alle 6 sensoren tegelijk
    // Was: 6x delay(750) = 4500ms blokkering. Nu: 1x delay(750) = 750ms.
    ow.reset();
    ow.writeByte(0xCC);  // SKIP ROM — alle sensoren tegelijk
    ow.writeByte(0x44);  // Convert T
    delay(750);          // Eén wachttijd voor alle sensoren

    float temps[6];
    for (int i = 0; i < 6; i++) {
      ow.reset();
      ow.writeByte(0x55);
      for (int j = 0; j < 8; j++) ow.writeByte(boilerSensors[i][j]);
      ow.writeByte(0xBE);
      uint8_t data[9];
      for (int j = 0; j < 9; j++) data[j] = ow.readByte();
      // CRC check
      uint8_t crc = 0;
      for (int j = 0; j < 8; j++) {
        uint8_t inbyte = data[j];
        for (int k = 0; k < 8; k++) {
          uint8_t mix = (crc ^ inbyte) & 0x01;
          crc >>= 1; if (mix) crc ^= 0x8C; inbyte >>= 1;
        }
      }
      if (crc == data[8]) {
        int16_t raw = (data[1]<<8) | data[0];
        temps[i] = raw / 16.0;
      } else {
        temps[i] = -127.0;
        Serial.printf("WARN CRC error sensor %d\n", i);
      }
    }
    ETopH=temps[0]; ETopL=temps[1]; EMidH=temps[2];
    EMidL=temps[3]; EBotH=temps[4]; EBotL=temps[5];
    Serial.printf("Boiler: TopH=%.1f TopL=%.1f MidH=%.1f MidL=%.1f BotH=%.1f BotL=%.1f\n",
      ETopH,ETopL,EMidH,EMidL,EBotH,EBotL);
  }

  float EAv1=(ETopH+ETopL)/2.0, EAv2=(ETopL+EMidH)/2.0, EAv3=(EMidH+EMidL)/2.0;
  float EAv4=(EMidL+EBotH)/2.0, EAv5=(EBotH+EBotL)/2.0;
  EAv = (EAv1+EAv2+EAv3+EAv4+EAv5)/5.0;
  Tboil = EBotH;
  dT = Tsun - Tboil;
  if (!ap_mode) wifi_rssi = WiFi.RSSI();
  Serial.printf("EAv=%.1f dT=%.1f\n", EAv, dT);
}

void calculateEnergy() {
  float specific_heat = 1.163 * (1.0 - GLYCOL_PERCENT/100.0*0.23);
  float EAv1=(ETopH+ETopL)/2.0, EAv2=(ETopL+EMidH)/2.0, EAv3=(EMidH+EMidL)/2.0;
  float EAv4=(EMidL+EBotH)/2.0, EAv5=(EBotH+EBotL)/2.0;
  float EQ1=max(0.0f,(EAv1-ETMIN)*ZONE_VOLUMES[0]*specific_heat/1000.0f);
  float EQ2=max(0.0f,(EAv2-ETMIN)*ZONE_VOLUMES[1]*specific_heat/1000.0f);
  float EQ3=max(0.0f,(EAv3-ETMIN)*ZONE_VOLUMES[2]*specific_heat/1000.0f);
  float EQ4=max(0.0f,(EAv4-ETMIN)*ZONE_VOLUMES[3]*specific_heat/1000.0f);
  float EQ5=max(0.0f,(EAv5-ETMIN)*ZONE_VOLUMES[4]*specific_heat/1000.0f);
  EQtot = EQ1+EQ2+EQ3+EQ4+EQ5;
  Serial.printf("EQtot=%.2f kWh (%.2f+%.2f+%.2f+%.2f+%.2f)\n",EQtot,EQ1,EQ2,EQ3,EQ4,EQ5);
}

// ==================
// PUMP LOGIC
// ==================
void checkPumpLogic() {
  if (pump_override_active) {
    unsigned long elapsed = millis() - pump_override_start;
    if (elapsed < PUMP_OVERRIDE_DURATION) {
      controlPump(pump_override_state, pump_override_state ? PWM_MAX : 0);
      return;
    } else {
      pump_override_active = false;
    }
  }

  time_t now; struct tm timeinfo;
  time(&now); localtime_r(&now, &timeinfo);
  int hour = timeinfo.tm_hour;

  bool should_run = true;
  String reason = "OK";

  if (hour < HOUR_START || hour >= HOUR_END) {
    should_run = false; reason = "Nacht";
  } else if (!pump_relay && dT < DT_START_THRESHOLD) {
    should_run = false; reason = "dT te laag";
  } else if (pump_relay && dT < DT_STOP_THRESHOLD) {
    should_run = false; reason = "dT onder stop";
  } else if (dT > DT_START_THRESHOLD && Tsun < TSUN_MIN_TEMP) {
    should_run = false; reason = "Thermosiphon";
  } else if (should_run && consecutive_reductions >= MAX_LOSS_STREAK) {
    should_run = false; reason = "Loss streak";
  }
  if (Tsun >= TSUN_OVERHEAT) { should_run = true; reason = "OVERHEAT"; }

  if (dEQ > 0) consecutive_reductions = 0;
  else if (dEQ <= 0 && pump_relay) consecutive_reductions++;

  int target_pwm = should_run ? calculatePWM(dT, Tsun) : 0;

  if (should_run != pump_relay) {
    if (should_run) {
      pump_starts_today++;
      pump_on_start = millis();
    } else if (pump_on_start > 0) {
      pump_minutes_today += (millis()-pump_on_start)/60000;
    }
  }
  controlPump(should_run, target_pwm);
  Serial.printf("Pump: %s PWM=%d  Reason: %s  LossStreak: %d/%d\n",
    should_run?"ON":"OFF", target_pwm, reason.c_str(),
    consecutive_reductions, MAX_LOSS_STREAK);
}

// v1.22: return type gecorrigeerd float -> int
int calculatePWM(float dT, float Tsun) {
  if (Tsun >= TSUN_OVERHEAT) return PWM_OVERHEAT;
  if (Tsun > TSUN_HIGH) return 180;
  float delta = constrain(dT - DT_START_THRESHOLD, 0.0, 17.0);
  return (int)(PWM_MIN + (delta * (PWM_MAX - PWM_MIN) / 17.0));
}

void controlPump(bool state, int pwm) {
  pump_relay = state;
  pwm_value  = pwm;
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);
  ledcWrite(PWM_PIN, state ? pwm : 0);
}

// ==================
// HVAC INTEGRATION
// ==================
void publishHVAC() {
  if (!config.hvac_enabled || ap_mode) return;
  String url = "";
  if (strlen(config.hvac_ip) > 0)
    url = "http://" + String(config.hvac_ip) + "/eco_energy";
  else if (strlen(config.hvac_mdns) > 0)
    url = "http://" + String(config.hvac_mdns) + ".local/eco_energy";
  else return;
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String payload = "eqtot=" + String(EQtot,2);
  int httpCode = http.POST(payload);
  Serial.printf("HVAC publish: %d\n", httpCode);
  http.end();
}

// ==================
// GRAPHING
// ==================
void addGraphSample() {
  graph_data.tsun[graph_data.index]  = Tsun;
  graph_data.tboil[graph_data.index] = Tboil;
  graph_data.dt[graph_data.index]    = dT;
  graph_data.eqtot[graph_data.index] = EQtot;
  graph_data.deq[graph_data.index]   = dEQ;
  graph_data.pwm[graph_data.index]   = pwm_value;
  graph_data.index = (graph_data.index+1) % GRAPH_SAMPLES;
}

String getGraphDataJSON() {
  String json = "{";
  auto arr = [&](const char* key, auto* buf, bool isInt=false) {
    json += "\""; json += key; json += "\":[";
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
      int idx = (graph_data.index+i) % GRAPH_SAMPLES;
      if (isInt) json += String((int)buf[idx]);
      else json += String(buf[idx], 1);
      if (i < GRAPH_SAMPLES-1) json += ",";
    }
    json += "],";
  };
  arr("tsun", graph_data.tsun);
  arr("tboil", graph_data.tboil);
  arr("dt", graph_data.dt);
  arr("eqtot", graph_data.eqtot);
  json += "\"pwm\":[";
  for (int i = 0; i < GRAPH_SAMPLES; i++) {
    int idx = (graph_data.index+i) % GRAPH_SAMPLES;
    json += String(graph_data.pwm[idx]);
    if (i < GRAPH_SAMPLES-1) json += ",";
  }
  json += "]}";
  return json;
}

String getFormattedDateTime() {
  time_t now; struct tm timeinfo;
  time(&now); localtime_r(&now, &timeinfo);
  if (now < 1700000000) return "tijd niet gesynchroniseerd";
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

// =========================================
// TREND HELPER
// =========================================
const char* getTrend(float current, float previous, float threshold=0.1) {
  if (abs(current-previous) < threshold) return "&rarr;";
  return (current > previous) ? "&uarr;" : "&darr;";
}

// =========================================
// MAIN PAGE — chunked streaming (v1.22)
// html.reserve(50000) vervangen door AsyncResponseStream
// =========================================
void streamMainPage(AsyncWebServerRequest* request) {
  int prev5 = (graph_data.index - 5 + GRAPH_SAMPLES) % GRAPH_SAMPLES;
  const char* tTsun  = getTrend(Tsun,  graph_data.tsun[prev5],  0.5);
  const char* tDT    = getTrend(dT,    graph_data.dt[prev5],    0.3);
  const char* tEQ    = getTrend(EQtot, graph_data.eqtot[prev5], 0.05);

  uint32_t lb = ESP.getMaxAllocHeap();
  const char* lb_color = lb >= 35000 ? "#0a0" : lb >= 25000 ? "#f80" : "#c00";
  const char* lb_label = lb >= 35000 ? "OK"   : lb >= 25000 ? "LAAG" : "KRITIEK";

  // Tsun balk: bereik -20..120°C (140° span), enkelvoudige kleur blauw->rood
  int tsun_pct = (int)constrain((Tsun + 20.0f) / 140.0f * 100.0f, 0.0f, 100.0f);
  bool tsun_overheat = (Tsun >= TSUN_OVERHEAT);
  // Kleur: lineaire interpolatie #0000cc (koud) -> #cc0000 (heet)
  float tsun_ratio = constrain((Tsun + 20.0f) / 140.0f, 0.0f, 1.0f);
  int tsun_r = (int)(tsun_ratio * 204.0f);
  int tsun_b = (int)((1.0f - tsun_ratio) * 204.0f);
  char tsun_color[8];
  snprintf(tsun_color, sizeof(tsun_color), "#%02x00%02x", tsun_r, tsun_b);

  // EQtot balk
  int eq_pct = (int)constrain(EQtot / EQ_MAX_KWH * 100.0f, 0.0f, 100.0f);

  AsyncResponseStream* p = request->beginResponseStream("text/html; charset=utf-8");

  // Chunk 1 — DOCTYPE, head, CSS
  p->print(F("<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>"));
  p->print(config.room_id);
  p->print(F(" Status</title><style>"
    "body{font-family:Arial,sans-serif;background:#fff;margin:0;padding:0;}"
    ".header{display:flex;background:#ffcc00;color:#000;padding:10px 15px;font-size:18px;font-weight:bold;align-items:center;}"
    ".header-left{flex:1;}.header-right{flex:1;text-align:right;font-size:15px;}"
    ".container{display:flex;min-height:calc(100vh - 60px);}"
    ".sidebar{width:80px;padding:10px 5px;background:#fff;border-right:3px solid #c00;}"
    ".sidebar a{display:block;background:#369;color:#fff;padding:8px;text-decoration:none;font-weight:bold;font-size:12px;border-radius:6px;text-align:center;width:60px;margin:8px auto;}"
    ".sidebar a:hover{background:#036;}.sidebar a.active{background:#c00;}"
    ".main{flex:1;padding:15px;overflow-y:auto;}"
    ".group-title{font-size:15px;font-style:italic;font-weight:bold;color:#fff;background:#336699;padding:6px 12px;margin:14px 0 6px 0;border-radius:4px;}"
    "table{width:100%;border-collapse:collapse;margin-bottom:10px;}"
    "td.label{color:#369;font-size:13px;padding:7px 5px;border-bottom:1px solid #ddd;text-align:left;}"
    "td.value{background:#e6f0ff;font-size:13px;padding:7px 5px;border-bottom:1px solid #ddd;text-align:center;font-weight:bold;}"
    ".cold{background:#e0f4ff!important;}"
    "tr.header-row td,tr.header-row td.label,tr.header-row td.value{background:#336699;color:#fff;font-weight:bold;padding:8px 5px;font-size:12px;}"
    ".bar-wrap{background:#ddd;border-radius:4px;height:28px;width:100%;position:relative;margin:6px 0;overflow:hidden;}"
    ".bar-fill{height:100%;border-radius:4px;transition:width .5s;}"
    ".bar-label{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);font-weight:bold;font-size:14px;color:#000;pointer-events:none;}"
    ".bar-mini{height:7px;border-radius:3px;margin-top:3px;}"
    ".overheat{animation:blink 0.7s infinite;border:2px solid #c00;}"
    "@keyframes blink{0%,100%{opacity:1;}50%{opacity:.4;}}"
    ".pump-badge{display:inline-block;padding:6px 14px;border-radius:6px;font-weight:bold;}"
    ".pump-on{background:#0a0;color:#fff;}.pump-off{background:#999;color:#fff;}"
    ".btn-override{padding:4px 8px;margin:2px;font-size:11px;cursor:pointer;border:none;border-radius:4px;background:#369;color:#fff;}"
    ".btn-override:hover{background:#036;}.btn-override-cancel{background:#c00;}.btn-override-cancel:hover{background:#900;}"
    ".override-badge{background:#c00;color:#fff;padding:4px 8px;border-radius:4px;font-size:11px;font-weight:bold;}"
    ".override-badge-off{background:#666;color:#fff;}"
    "@media(max-width:600px){.container{flex-direction:column;}.sidebar{width:100%;border-right:none;border-bottom:3px solid #c00;display:flex;justify-content:center;}.sidebar a{width:60px;margin:0 3px;}.main{padding:8px;}}"
    "</style></head><body>"));

  // Chunk 2 — header + sidebar
  p->print(F("<div class='header'><div class='header-left'>"));
  p->print(config.room_id);
  p->print(F("</div><div class='header-right'>"));
  p->print(uptime_sec);
  p->print(F("s &nbsp; "));
  p->print(getFormattedDateTime());
  p->print(F("</div></div>"
    "<div class='container'><div class='sidebar'>"
    "<a href='/' class='active'>Status</a>"
    "<a href='/charts'>Charts</a>"
    "<a href='/update'>OTA</a>"
    "<a href='/json'>JSON</a>"
    "<a href='/settings'>Settings</a>"
    "</div><div class='main'>"));

  // Simulatie waarschuwing
  if (SIMULATION_MODE) {
    p->print(F("<div style='background:#c00;color:#fff;padding:10px;margin-bottom:10px;border-radius:6px;text-align:center;font-weight:bold;'>SIMULATION MODE - FAKE DATA!</div>"));
  }

  // Chunk 3 — COLLECTOR met temperatuurbalk
  p->print(F("<div class='group-title'>COLLECTOR (Dak) "));
  p->print(tTsun);
  p->print(F("</div><div class='bar-wrap"));
  if (tsun_overheat) p->print(F(" overheat"));
  p->print(F("'><div class='bar-fill' style='width:"));
  p->print(tsun_pct);
  p->print(F("%;background:"));
  p->print(tsun_color);
  p->print(F(";'></div><div class='bar-label'>"));
  p->print(Tsun, 1);
  p->print(F(" &deg;C</div></div>"));

  // Chunk 4 — dT tabel
  p->print(F("<div class='group-title'>TEMPERATUURVERSCHIL (dT) "));
  p->print(tDT);
  p->print(F("</div><table>"
    "<tr><td class='label'>Collector (Tsun)</td><td class='value'>"));
  p->print(Tsun, 1);
  p->print(F(" &deg;C</td></tr>"
    "<tr><td class='label'>Boiler input (Tboil)</td><td class='value'>"));
  p->print(Tboil, 1);
  p->print(F(" &deg;C</td></tr>"
    "<tr><td class='label'>Verschil dT</td><td class='value' style='font-size:18px;color:"));
  p->print(dT > DT_START_THRESHOLD ? "#0a0" : "#c00");
  p->print(F("'>"));
  p->print(dT, 1);
  p->print(F(" &deg;C</td></tr>"
    "<tr><td class='label'>Drempel start/stop</td><td class='value'>"));
  p->print(DT_START_THRESHOLD, 1);
  p->print(F(" / "));
  p->print(DT_STOP_THRESHOLD, 1);
  p->print(F(" &deg;C</td></tr></table>"));

  // Chunk 5 — POMP STATUS (geen tekststatus)
  p->print(F("<div class='group-title'>POMP</div>"
    "<table><tr><td class='label'>Status</td><td class='value'>"));
  p->print(pump_relay
    ? F("<span class='pump-badge pump-on'>&#x25CF; AAN</span>")
    : F("<span class='pump-badge pump-off'>&#x25CB; UIT</span>"));
  p->print(F("</td></tr><tr><td class='label'>PWM</td><td class='value' style='font-size:18px;'>"));
  p->print(pwm_value);
  p->print(F(" / 255 &nbsp;<small>("));
  p->print(pwm_value * 100 / 255);
  p->print(F("%)</small></td></tr>"
    "<tr><td class='label'>Override</td><td class='value'>"));

  // Override knoppen
  if (pump_override_active) {
    unsigned long elapsed = millis() - pump_override_start;
    if (elapsed < PUMP_OVERRIDE_DURATION) {
      unsigned long rem = (PUMP_OVERRIDE_DURATION - elapsed) / 1000;
      p->print(pump_override_state
        ? F("<span class='override-badge timer' data-remaining='")
        : F("<span class='override-badge override-badge-off timer' data-remaining='"));
      p->print(rem);
      p->print(F("'>"));
      p->print(pump_override_state ? F("ON ") : F("OFF "));
      p->print(rem/60); p->print(F(":")); p->print(rem%60);
      p->print(F("</span> <button class='btn-override btn-override-cancel' onclick='cancelPumpOverride()'>&#215;</button>"));
    } else {
      p->print(F("<button class='btn-override' onclick='setPumpOverride(true)'>ON</button> "
                 "<button class='btn-override' onclick='setPumpOverride(false)'>OFF</button>"));
    }
  } else {
    p->print(F("<button class='btn-override' onclick='setPumpOverride(true)'>ON</button> "
               "<button class='btn-override' onclick='setPumpOverride(false)'>OFF</button>"));
  }

  p->print(F("</td></tr>"
    "<tr><td class='label'>Starts vandaag</td><td class='value'>"));
  p->print(pump_starts_today);
  p->print(F("&times;</td></tr>"
    "<tr><td class='label'>Draaitijd vandaag</td><td class='value'>"));
  p->print(pump_minutes_today);
  p->print(F(" min</td></tr>"
    "<tr><td class='label'>Loss streak</td><td class='value'>"));
  p->print(consecutive_reductions);
  p->print(F(" / "));
  p->print(MAX_LOSS_STREAK);
  p->print(F("</td></tr></table>"));

  // Chunk 6 — BOILER ENERGIE met rode balk
  p->print(F("<div class='group-title'>BOILER ENERGIE "));
  p->print(tEQ);
  p->print(F("</div><div style='text-align:center;font-size:32px;font-weight:bold;color:#c00;margin:6px 0;'>"));
  p->print(EQtot, 2);
  p->print(F(" kWh</div>"
    "<div class='bar-wrap'>"
    "<div class='bar-fill' style='width:"));
  p->print(eq_pct);
  p->print(F("%;background:#c00;'></div>"
    "<div class='bar-label'>"));
  p->print(eq_pct);
  p->print(F("%</div></div><table>"
    "<tr><td class='label'>dEQ (10 min)</td><td class='value' style='color:"));
  p->print(dEQ > 0 ? "#0a0" : "#c00");
  p->print(F("'>"));
  p->print(dEQ, 3);
  p->print(F(" kWh</td></tr>"
    "<tr><td class='label'>Opbrengst vandaag</td><td class='value'>"));
  p->print(yield_today, 1);
  p->print(F(" kWh</td></tr>"
    "<tr><td class='label'>Gemiddelde (EAv)</td><td class='value'>"));
  p->print(EAv, 1);
  p->print(F(" &deg;C</td></tr></table>"));

  // Chunk 7 — BOILER TEMPERATUREN met gekleurde mini-balken
  p->print(F("<div class='group-title'>BOILER TEMPERATUREN</div>"
    "<table><tr class='header-row'><td class='label'>Zone</td>"
    "<td class='value'>Temperatuur</td></tr>"));

  float boilerTemps[6] = {ETopH, ETopL, EMidH, EMidL, EBotH, EBotL};
  for (int i = 0; i < 6; i++) {
    float t = boilerTemps[i];
    int bar_w = (int)constrain(t / 90.0f * 100.0f, 0.0f, 100.0f);
    bool is_cold = (t < ETMIN);  // onder minimale douche temp
    p->print(F("<tr><td class='label'>"));
    p->print(sensor_nicknames[i]);
    p->print(F("</td><td class='value"));
    if (is_cold) p->print(F(" cold"));
    p->print(F("'>"));
    p->print(t, 1);
    p->print(F(" &deg;C"
      "<div class='bar-mini' style='background:"));
    p->print(tempColor(t));
    p->print(F(";width:"));
    p->print(bar_w);
    p->print(F("%;'></div></td></tr>"));
  }
  p->print(F("</table>"));

  // Chunk 8 — SYSTEEM (ingekort: alleen heap info + RSSI)
  p->print(F("<div class='group-title'>SYSTEEM</div><table>"
    "<tr><td class='label'>WiFi RSSI</td><td class='value'>"));
  p->print(wifi_rssi);
  p->print(F(" dBm</td></tr>"
    "<tr><td class='label'>Free heap</td><td class='value'>"));
  p->print((ESP.getFreeHeap()*100)/ESP.getHeapSize());
  p->print(F("% &nbsp; | &nbsp; largest block: <b style='color:"));
  p->print(lb_color);
  p->print(F("'>"));
  p->print(lb/1024);
  p->print(F(" KB ("));
  p->print(lb_label);
  p->print(F(")</b></td></tr>"
    "<tr><td class='label'>Min heap since boot</td><td class='value'>"));
  p->print(ESP.getMinFreeHeap()/1024);
  p->print(F(" KB</td></tr></table>"));

  // Chunk 9 — auto-refresh JS + overrides
  p->print(F("<script>"
    "function setPumpOverride(s){"
    "fetch(s?'/pump_override_on':'/pump_override_off')"
    ".then(()=>setTimeout(()=>location.reload(),500));}"
    "function cancelPumpOverride(){"
    "fetch('/pump_override_cancel').then(()=>setTimeout(()=>location.reload(),500));}"
    "setInterval(()=>{"
    "document.querySelectorAll('.timer').forEach(b=>{"
    "let r=parseInt(b.dataset.remaining);"
    "if(r>0){r--;b.dataset.remaining=r;"
    "const s=b.textContent.split(' ')[0];"
    "b.textContent=s+' '+Math.floor(r/60)+':'+(r%60).toString().padStart(2,'0');}"
    "else if(r===0) setTimeout(()=>location.reload(),1000);"
    "});},1000);"
    "setTimeout(()=>location.reload(),30000);"
    "</script>"
    "</div></div></body></html>"));

  request->send(p);
}

// =========================================
// CHARTS PAGE — Chart.js grafisch
// =========================================
void streamChartsPage(AsyncWebServerRequest* request) {
  AsyncResponseStream* p = request->beginResponseStream("text/html; charset=utf-8");
  p->print(F("<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Charts</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#fff;margin:0;padding:0;}"
    ".header{display:flex;background:#ffcc00;color:#000;padding:10px 15px;font-size:18px;font-weight:bold;}"
    ".header-left{flex:1;}.header-right{flex:1;text-align:right;font-size:15px;}"
    ".container{display:flex;min-height:calc(100vh - 60px);}"
    ".sidebar{width:80px;padding:10px 5px;background:#fff;border-right:3px solid #c00;}"
    ".sidebar a{display:block;background:#369;color:#fff;padding:8px;margin:8px auto;text-decoration:none;font-weight:bold;font-size:12px;border-radius:6px;text-align:center;width:60px;}"
    ".sidebar a:hover{background:#036;}.sidebar a.active{background:#c00;}"
    ".main{flex:1;padding:15px;overflow-y:auto;}"
    ".group-title{font-size:15px;font-style:italic;font-weight:bold;color:#fff;background:#336699;padding:6px 12px;margin:14px 0 6px 0;border-radius:4px;}"
    ".chart-box{background:#fff;padding:8px;border-radius:6px;border:1px solid #ddd;margin-bottom:14px;}"
    "canvas{width:100%!important;height:200px!important;}"
    "@media(max-width:600px){.container{flex-direction:column;}.sidebar{width:100%;border-right:none;border-bottom:3px solid #c00;display:flex;justify-content:center;}.sidebar a{width:60px;margin:0 3px;}.main{padding:8px;}}"
    "</style>"
    "<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script>"
    "</head><body>"
    "<div class='header'><div class='header-left'>"));
  p->print(config.room_id);
  p->print(F("</div><div class='header-right'>Grafieken (60 min)</div></div>"
    "<div class='container'><div class='sidebar'>"
    "<a href='/'>Status</a>"
    "<a href='/charts' class='active'>Charts</a>"
    "<a href='/update'>OTA</a>"
    "<a href='/json'>JSON</a>"
    "<a href='/settings'>Settings</a>"
    "</div><div class='main'>"
    "<div class='group-title'>Temperaturen</div>"
    "<div class='chart-box'><canvas id='cT'></canvas></div>"
    "<div class='group-title'>Energie</div>"
    "<div class='chart-box'><canvas id='cE'></canvas></div>"
    "<div class='group-title'>Pomp PWM</div>"
    "<div class='chart-box'><canvas id='cP'></canvas></div>"
    "<script>"
    "fetch('/graph_data').then(r=>r.json()).then(d=>{"
    "const L=Array.from({length:60},(_,i)=>`-${60-i}m`);"
    "const opt=(yLabel)=>({responsive:true,maintainAspectRatio:false,"
    "plugins:{legend:{position:'top'}},"
    "scales:{y:{title:{display:true,text:yLabel}}}});"
    "new Chart(document.getElementById('cT'),{type:'line',data:{labels:L,datasets:["
    "{label:'Tsun',data:d.tsun,borderColor:'#f90',tension:0.3,pointRadius:0},"
    "{label:'Tboil',data:d.tboil,borderColor:'#36f',tension:0.3,pointRadius:0},"
    "{label:'dT',data:d.dt,borderColor:'#0a0',tension:0.3,pointRadius:0}"
    "]},options:opt('deg C')});"
    "new Chart(document.getElementById('cE'),{type:'line',data:{labels:L,datasets:["
    "{label:'EQtot',data:d.eqtot,borderColor:'#c00',tension:0.3,pointRadius:0,fill:true}"
    "]},options:opt('kWh')});"
    "new Chart(document.getElementById('cP'),{type:'line',data:{labels:L,datasets:["
    "{label:'PWM',data:d.pwm,borderColor:'#369',stepped:true,fill:true}"
    "]},options:{responsive:true,maintainAspectRatio:false,"
    "scales:{y:{min:0,max:255,title:{display:true,text:'PWM'}}}}});"
    "}).catch(e=>console.error('Charts:',e));"
    "</script>"
    "</div></div></body></html>"));
  request->send(p);
}

// =========================================
// SETTINGS PAGE — chunked streaming (v1.22)
// Inclusief Matter-sectie + crash-log
// =========================================
void streamSettingsPage(AsyncWebServerRequest* request) {
  AsyncResponseStream* p = request->beginResponseStream("text/html; charset=utf-8");

  // Chunk 1 — head + CSS
  p->print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Settings</title><style>"
    "body{font-family:Arial,sans-serif;background:#fff;margin:0;padding:0;}"
    ".header{display:flex;background:#ffcc00;color:#000;padding:10px 15px;font-size:18px;font-weight:bold;}"
    ".header-left{flex:1;}.header-right{flex:1;text-align:right;font-size:15px;}"
    ".container{display:flex;min-height:calc(100vh - 60px);}"
    ".sidebar{width:80px;padding:10px 5px;background:#fff;border-right:3px solid #c00;}"
    ".sidebar a{display:block;background:#369;color:#fff;padding:8px;margin:8px auto;text-decoration:none;font-weight:bold;font-size:12px;border-radius:6px;text-align:center;width:60px;}"
    ".sidebar a:hover{background:#036;}.sidebar a.active{background:#c00;}"
    ".main{flex:1;padding:20px;overflow-y:auto;}"
    "table{width:100%;margin:6px 0;}td{padding:7px;}"
    "input,select{padding:6px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;width:100%;}"
    ".btn{background:#369;color:#fff;padding:10px 24px;border:none;border-radius:6px;font-size:15px;cursor:pointer;margin:6px;}"
    ".btn:hover{background:#036;}.btn-red{background:#c00;}.btn-red:hover{background:#900;}"
    ".cgroup{background:#369;color:#fff;padding:3px 8px;border-radius:4px;margin:10px 0 3px 0;font-weight:bold;font-size:13px;}"
    ".warn{background:#ffe0e0;border:2px solid #c00;padding:10px;border-radius:6px;margin:8px 0;color:#900;font-weight:bold;}"
    ".matter-card{background:#e6f0ff;border:2px solid #369;border-radius:8px;padding:15px;margin:8px 0;}"
    ".code{font-family:monospace;font-size:22px;font-weight:bold;color:#003366;background:#fff;padding:8px 16px;border-radius:4px;border:2px solid #369;display:inline-block;letter-spacing:2px;margin:8px 0;}"
    "@media(max-width:600px){.container{flex-direction:column;}.sidebar{width:100%;border-right:none;border-bottom:3px solid #c00;display:flex;justify-content:center;}.sidebar a{width:60px;margin:0 3px;}.main{padding:8px;}}"
    "</style></head><body>"
    "<div class='header'><div class='header-left'>"));
  p->print(config.room_id);
  p->print(F("</div><div class='header-right'>Instellingen</div></div>"
    "<div class='container'><div class='sidebar'>"
    "<a href='/'>Status</a><a href='/charts'>Charts</a>"
    "<a href='/update'>OTA</a><a href='/json'>JSON</a>"
    "<a href='/settings' class='active'>Settings</a>"
    "</div><div class='main'>"));

  // Crash-log sectie
  {
    Preferences crashPrefs;
    crashPrefs.begin("crash-log", true);
    uint32_t  cnt    = crashPrefs.getUInt("count", 0);
    String    reason = crashPrefs.getString("reason", "geen");
    crashPrefs.end();
    p->print(F("<table><tr><td style='width:35%;'>Crashteller</td><td><b style='color:"));
    p->print(cnt > 0 ? F("#c00") : F("#0a0"));
    p->print(F("'>"));
    p->print(cnt);
    p->print(F("</b>"));
    if (cnt > 0) p->print(F(" &nbsp;<a href='/clear_crash_log' style='font-size:12px;color:#369;'"
      " onclick=\"return confirm('Crash-log wissen?');\">Wissen</a>"));
    p->print(F("</td></tr><tr><td>Laatste crash</td><td><code style='font-size:12px;'>"));
    p->print(reason);
    p->print(F("</code></td></tr></table><hr style='border:1px solid #ddd;margin:8px 0;'>"));
  }

  // Heap rapport
  uint32_t lb = ESP.getMaxAllocHeap();
  const char* lb_col = lb>=35000?"#0a0":lb>=25000?"#f80":"#c00";
  p->print(F("<table><tr><td style='width:35%;'>Free heap</td><td><b>"));
  p->print((ESP.getFreeHeap()*100)/ESP.getHeapSize());
  p->print(F("% &nbsp;|&nbsp; largest <span style='color:"));
  p->print(lb_col);
  p->print(F("'>"));
  p->print(lb/1024);
  p->print(F(" KB</span> &nbsp;|&nbsp; min ever "));
  p->print(ESP.getMinFreeHeap()/1024);
  p->print(F(" KB</b></td></tr></table><hr style='border:1px solid #ddd;margin:8px 0;'>"));

  // Form start
  p->print(F("<form action='/save_settings' method='get'>"));

  // WiFi + Systeem
  p->print(F("<div class='cgroup'>WiFi & Systeem</div><table>"
    "<tr><td style='width:35%;'>Room naam</td><td><input type='text' name='room_id' value='"));
  p->print(config.room_id);
  p->print(F("' required></td></tr>"
    "<tr><td>WiFi SSID</td><td><input type='text' name='wifi_ssid' value='"));
  p->print(config.wifi_ssid);
  p->print(F("'></td></tr>"
    "<tr><td>WiFi Password</td><td><input type='password' name='wifi_pass' value='"));
  p->print(config.wifi_pass);
  p->print(F("'></td></tr>"
    "<tr><td>Static IP</td><td><input type='text' name='static_ip' value='"));
  p->print(config.static_ip);
  p->print(F("' placeholder='leeg = DHCP'></td></tr>"
    "<tr><td>MAC adres</td><td><code>"));
  p->print(mac_address);
  p->print(F("</code></td></tr></table>"));

  // Pump thresholds
  p->print(F("<div class='cgroup'>Pomp Drempelwaarden</div><table>"
    "<tr><td style='width:35%;'>dT Start (&deg;C)</td><td><input type='number' step='0.1' name='dt_start' value='"));
  p->print(DT_START_THRESHOLD,1);
  p->print(F("'></td></tr><tr><td>dT Stop (&deg;C)</td><td><input type='number' step='0.1' name='dt_stop' value='"));
  p->print(DT_STOP_THRESHOLD,1);
  p->print(F("'></td></tr><tr><td>Tsun Min (&deg;C)</td><td><input type='number' step='0.1' name='tsun_min' value='"));
  p->print(TSUN_MIN_TEMP,1);
  p->print(F("'></td></tr><tr><td>Tsun Overheat (&deg;C)</td><td><input type='number' step='0.1' name='tsun_overheat' value='"));
  p->print(TSUN_OVERHEAT,1);
  p->print(F("'></td></tr><tr><td>Tsun High (&deg;C)</td><td><input type='number' step='0.1' name='tsun_high' value='"));
  p->print(TSUN_HIGH,1);
  p->print(F("'></td></tr><tr><td>Max Loss Streak</td><td><input type='number' name='max_loss_streak' value='"));
  p->print(MAX_LOSS_STREAK);
  p->print(F("'></td></tr></table>"));

  // PWM
  p->print(F("<div class='cgroup'>PWM Instellingen</div><table>"
    "<tr><td style='width:35%;'>PWM Min</td><td><input type='number' name='pwm_min' value='"));
  p->print(PWM_MIN);
  p->print(F("'></td></tr><tr><td>PWM Max</td><td><input type='number' name='pwm_max' value='"));
  p->print(PWM_MAX);
  p->print(F("'></td></tr><tr><td>PWM Overheat</td><td><input type='number' name='pwm_overheat' value='"));
  p->print(PWM_OVERHEAT);
  p->print(F("'></td></tr></table>"));

  // Energie
  p->print(F("<div class='cgroup'>Energieberekening</div><table>"
    "<tr><td style='width:35%;'>ETmin (&deg;C)</td><td><input type='number' step='0.1' name='etmin' value='"));
  p->print(ETMIN,1);
  p->print(F("'></td></tr><tr><td>Glycol %</td><td><input type='number' step='1' name='glycol_pct' value='"));
  p->print(GLYCOL_PERCENT,0);
  p->print(F("'></td></tr><tr><td>Totaal volume (L)</td><td><input type='number' step='1' name='boiler_volume' value='"));
  p->print(BOILER_VOLUME_TOTAL,0);
  p->print(F("'></td></tr>"
    "<tr><td>Zone 1 (Top)</td><td><input type='number' step='1' name='zone_vol_0' value='"));
  p->print(ZONE_VOLUMES[0],0);
  p->print(F("'></td></tr><tr><td>Zone 2</td><td><input type='number' step='1' name='zone_vol_1' value='"));
  p->print(ZONE_VOLUMES[1],0);
  p->print(F("'></td></tr><tr><td>Zone 3</td><td><input type='number' step='1' name='zone_vol_2' value='"));
  p->print(ZONE_VOLUMES[2],0);
  p->print(F("'></td></tr><tr><td>Zone 4</td><td><input type='number' step='1' name='zone_vol_3' value='"));
  p->print(ZONE_VOLUMES[3],0);
  p->print(F("'></td></tr><tr><td>Zone 5 (Bot)</td><td><input type='number' step='1' name='zone_vol_4' value='"));
  p->print(ZONE_VOLUMES[4],0);
  p->print(F("'></td></tr>"
    "<tr><td>Max boilerenergie (kWh)</td><td><input type='number' step='0.5' name='eq_max_kwh' value='"));
  p->print(EQ_MAX_KWH,1);
  p->print(F("'></td></tr></table>"));

  // HVAC
  p->print(F("<div class='cgroup'>HVAC Integratie</div><table>"
    "<tr><td style='width:35%;'>HVAC Enabled</td><td><input type='checkbox' name='hvac_enabled' value='1' style='width:auto;'"));
  if (config.hvac_enabled) p->print(F(" checked"));
  p->print(F("></td></tr>"
    "<tr><td>HVAC IP</td><td><input type='text' name='hvac_ip' value='"));
  p->print(config.hvac_ip);
  p->print(F("'></td></tr><tr><td>HVAC mDNS</td><td><input type='text' name='hvac_mdns' value='"));
  p->print(config.hvac_mdns);
  p->print(F("' placeholder='ZONDER .local'></td></tr>"
    "<tr><td>Transfer Threshold (kWh)</td><td><input type='number' step='0.1' name='hvac_thresh' value='"));
  p->print(HVAC_TRANSFER_THRESHOLD,1);
  p->print(F("'></td></tr></table>"));

  // Bedrijfsuren
  p->print(F("<div class='cgroup'>Bedrijfsuren</div><table>"
    "<tr><td style='width:35%;'>Start uur</td><td><input type='number' min='0' max='23' name='hour_start' value='"));
  p->print(HOUR_START);
  p->print(F("'></td></tr><tr><td>Stop uur</td><td><input type='number' min='0' max='23' name='hour_end' value='"));
  p->print(HOUR_END);
  p->print(F("'></td></tr></table>"));

  // Sensor nicknames
  p->print(F("<div class='cgroup'>Sensor Namen</div>"));
  for (int i = 0; i < 6; i++) {
    p->print(F("<label style='display:block;margin:4px 0;'>"));
    p->print(sensor_nicknames[i]);
    p->print(F(": <input type='text' name='sensor_nick_"));
    p->print(i);
    p->print(F("' value='"));
    p->print(sensor_nicknames[i]);
    p->print(F("' style='width:220px;'></label>"));
  }

  // Simulatie mode
  p->print(F("<div class='cgroup' style='background:#c00;'>SIMULATION MODE</div>"
    "<div class='warn'>ALLEEN VOOR TESTEN — niet voor productie!</div>"
    "<label><input type='checkbox' name='simulation_mode' value='1' style='width:auto;'"));
  if (SIMULATION_MODE) p->print(F(" checked"));
  p->print(F("> Activeer simulatie mode</label>"));

  // Diagnostics
  p->print(F("<div class='cgroup'>Diagnostics</div>"
    "<div style='margin:8px 0;'>"
    "<a href='/log/view' class='btn' style='display:inline-block;text-decoration:none;'>Log bekijken</a>"
    "<a href='/log' class='btn' style='display:inline-block;text-decoration:none;'>Download log</a>"
    "<a href='/log/clear' class='btn' style='display:inline-block;text-decoration:none;'"
    " onclick=\"return confirm('Log wissen?');\">Log wissen</a>"
    "<a href='/restart' class='btn btn-red' style='display:inline-block;text-decoration:none;'"
    " onclick=\"return confirm('Restart ESP32?');\">Restart</a>"
    "</div>"));

  // Matter sectie
  p->print(F("<div class='cgroup'>Matter / HomeKit</div>"
    "<div class='matter-card'>"));
  if (Matter.isDeviceCommissioned()) {
    p->print(F("<b style='color:#0a0;'>&#x2705; Matter gepaard</b>"
      "<p style='margin:6px 0;font-size:13px;'>Controller is verbonden met Apple Home.</p>"));
  } else {
    p->print(F("<b style='color:#c00;'>Nog niet gepaard</b>"
      "<p style='margin:6px 0;font-size:13px;'>Pairing code:</p>"
      "<div class='code'>"));
    p->print(Matter.getManualPairingCode());
    p->print(F("</div>"
      "<p style='font-size:12px;color:#666;'>Home app &rarr; + &rarr; Accessoire &rarr; Meer opties</p>"));
  }
  p->print(F("<br><button type='button' class='btn btn-red'"
    " onclick=\"if(confirm('Matter pairing wissen?')) location.href='/matter_reset';\">"
    "Matter reset (pairing wissen)</button>"
    "<p style='font-size:12px;color:#666;margin:4px 0;'>Instellingen blijven intact.</p>"
    "</div>"));

  // Opslaan knoppen
  p->print(F("<div style='text-align:center;margin-top:14px;'>"
    "<button type='submit' class='btn'>Opslaan &amp; Reboot</button>"
    "<a href='/' class='btn btn-red' style='display:inline-block;text-decoration:none;'>Annuleren</a>"
    "</div></form>"
    "</div></div></body></html>"));

  request->send(p);
}

// ============================
// WEB SERVER SETUP
// ============================
void setupWebServer() {
  server.onNotFound([](AsyncWebServerRequest *request){
    if (ap_mode) request->redirect("/settings");
    else request->send(404, "text/plain", "Not found");
  });

  // Hoofdpagina
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    streamMainPage(request);
  });

  // Charts pagina
  server.on("/charts", HTTP_GET, [](AsyncWebServerRequest *request){
    streamChartsPage(request);
  });

  // Settings pagina
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    if (ap_mode && request->host() != WiFi.softAPIP().toString())
      request->redirect("http://" + WiFi.softAPIP().toString() + "/settings");
    else
      streamSettingsPage(request);
  });

  // JSON endpoint — v1.22: compacte a/b/c keys, pure snprintf, geen pump_status
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
    char json[320];
    snprintf(json, sizeof(json),
      "{\"a\":%lu,"
      "\"b\":%.1f,\"c\":%.1f,\"d\":%.1f,\"e\":%.1f,\"f\":%.1f,\"g\":%.1f,"
      "\"h\":%.1f,\"i\":%.2f,\"j\":%.3f,\"k\":%.1f,"
      "\"l\":%.1f,\"m\":%.1f,"
      "\"n\":%d,\"o\":%d,"
      "\"p\":%d,\"q\":%d,\"r\":%d,\"s\":%d}",
      uptime_sec,
      ETopH, ETopL, EMidH, EMidL, EBotH, EBotL,
      EAv, EQtot, dEQ, yield_today,
      Tsun, dT,
      pwm_value,
      pump_relay ? 1 : 0,
      wifi_rssi,
      (int)((ESP.getFreeHeap()*100)/ESP.getHeapSize()),
      (int)(ESP.getMaxAllocHeap()/1024),
      (int)(ESP.getMinFreeHeap()/1024)
    );
    request->send(200, "application/json", json);
  });

  // Graph data
  server.on("/graph_data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getGraphDataJSON());
  });

  // Pump overrides
  server.on("/pump_override_on", HTTP_GET, [](AsyncWebServerRequest *request){
    pump_override_start  = millis();
    pump_override_active = true;
    pump_override_state  = true;
    request->send(200, "text/plain", "Pump override ON (60s)");
  });
  server.on("/pump_override_off", HTTP_GET, [](AsyncWebServerRequest *request){
    pump_override_start  = millis();
    pump_override_active = true;
    pump_override_state  = false;
    request->send(200, "text/plain", "Pump override OFF (60s)");
  });
  server.on("/pump_override_cancel", HTTP_GET, [](AsyncWebServerRequest *request){
    pump_override_active = false;
    request->send(200, "text/plain", "Pump override cancelled");
  });

  // Matter reset
  server.on("/matter_reset", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html",
      "<h2 style='text-align:center;padding:40px;color:#c00;'>"
      "Matter reset...<br><small style='font-size:16px;color:#666;'>Rebooting in 1s.</small></h2>");
    Serial.println("[WEB] Matter nuclear reset aangevraagd");
    delay(500);
    nvs_handle_t h;
    const char* ns[] = {"chip-factory","chip-config","chip-counters","chip-kvs"};
    for (int k = 0; k < 4; k++) {
      if (nvs_open(ns[k], NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h); nvs_commit(h); nvs_close(h);
      }
    }
    delay(500); ESP.restart();
  });

  // Crash log endpoints
  server.on("/log/view", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!SPIFFS.begin()) { request->send(500,"text/plain","SPIFFS error"); return; }
    File f = SPIFFS.open("/debug.log","r");
    String content = "<html><head><meta charset='utf-8'><style>"
      "body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:20px;}"
      "pre{background:#2d2d2d;padding:15px;border-radius:6px;font-size:12px;white-space:pre-wrap;}"
      ".ok{background:#1e3a1e;border:2px solid #4caf50;padding:15px;border-radius:6px;color:#4caf50;text-align:center;}"
      "a{color:#4fc3f7;}</style></head><body>"
      "<h2>ECO Log</h2>"
      "<p><a href='/settings'>&larr; Settings</a> | "
      "<a href='/log'>Download</a> | <a href='/log/clear'>Wissen</a></p>";
    if (!f || f.size()==0) {
      content += "<div class='ok'><h3>Log leeg</h3><p>Geen problemen gevonden!</p></div>";
    } else {
      content += "<pre>";
      while (f.available()) content += (char)f.read();
      content += "</pre>";
    }
    if (f) f.close();
    content += "</body></html>";
    request->send(200,"text/html",content);
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!SPIFFS.begin()) { request->send(500,"text/plain","SPIFFS Error"); return; }
    request->send(SPIFFS, "/debug.log", "text/plain");
  });

  server.on("/log/clear", HTTP_GET, [](AsyncWebServerRequest *request){
    if (SPIFFS.begin()) { SPIFFS.remove("/debug.log"); SPIFFS.remove("/debug.log.old"); }
    request->redirect("/log/view");
  });

  server.on("/clear_crash_log", HTTP_GET, [](AsyncWebServerRequest *request){
    Preferences cp; cp.begin("crash-log",false);
    cp.putUInt("count",0); cp.putString("reason","geen"); cp.end();
    request->redirect("/settings");
  });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200,"text/html","<h2>Rebooting...</h2>");
    delay(500); ESP.restart();
  });

  // Reboot alias
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200,"text/plain","Rebooting..."); delay(500); ESP.restart();
  });

  // OTA pagina
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream* p = request->beginResponseStream("text/html; charset=utf-8");
    p->print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<title>OTA</title><style>"
      "body{font-family:Arial,sans-serif;background:#fff;margin:0;padding:0;}"
      ".header{display:flex;background:#ffcc00;color:#000;padding:10px 15px;font-size:18px;font-weight:bold;}"
      ".header-left{flex:1;}.header-right{flex:1;text-align:right;font-size:15px;}"
      ".container{display:flex;min-height:calc(100vh - 60px);}"
      ".sidebar{width:80px;padding:10px 5px;background:#fff;border-right:3px solid #c00;}"
      ".sidebar a{display:block;background:#369;color:#fff;padding:8px;margin:8px auto;text-decoration:none;font-weight:bold;font-size:12px;border-radius:6px;text-align:center;width:60px;}"
      ".sidebar a:hover{background:#036;}.sidebar a.active{background:#c00;}"
      ".main{flex:1;padding:30px;text-align:center;}"
      ".btn{background:#369;color:#fff;padding:12px 24px;border:none;border-radius:8px;cursor:pointer;font-size:16px;margin:10px;}"
      ".btn:hover{background:#036;}.btn-red{background:#c00;}.btn-red:hover{background:#900;}"
      "</style></head><body>"
      "<div class='header'><div class='header-left'>"));
    p->print(config.room_id);
    p->print(F("</div><div class='header-right'>OTA Update</div></div>"
      "<div class='container'><div class='sidebar'>"
      "<a href='/'>Status</a><a href='/charts'>Charts</a>"
      "<a href='/update' class='active'>OTA</a>"
      "<a href='/json'>JSON</a><a href='/settings'>Settings</a>"
      "</div><div class='main'>"
      "<h2 style='color:#369;'>OTA Firmware Update</h2>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update' accept='.bin'><br><br>"
      "<button class='btn' type='submit'>Upload Firmware</button>"
      "</form><br>"
      "<button class='btn btn-red'"
      " onclick=\"if(confirm('Reboot?'))location.href='/reboot'\">Reboot</button>"
      "<br><br><a href='/' style='color:#369;'>&larr; Terug</a>"
      "</div></div></body></html>"));
    request->send(p);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    bool ok = !Update.hasError();
    request->send(200,"text/html",
      ok ? "<h2 style='color:#0a0;text-align:center;padding:40px;'>Update OK! Rebooting...</h2>"
         : "<h2 style='color:#c00;text-align:center;padding:40px;'>Update FAILED!</h2>");
    if (ok) { delay(1000); ESP.restart(); }
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!index) { Serial.println("\n=== OTA UPDATE ==="); Update.begin(UPDATE_SIZE_UNKNOWN); }
    Update.write(data, len);
    if (final && Update.end(true)) Serial.println("OTA OK");
  });

  // Save settings
  server.on("/save_settings", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("room_id"))       strncpy(config.room_id,   request->arg("room_id").c_str(),   sizeof(config.room_id)-1);
    if (request->hasArg("wifi_ssid"))     strncpy(config.wifi_ssid, request->arg("wifi_ssid").c_str(), sizeof(config.wifi_ssid)-1);
    if (request->hasArg("wifi_pass"))     strncpy(config.wifi_pass, request->arg("wifi_pass").c_str(), sizeof(config.wifi_pass)-1);
    if (request->hasArg("static_ip"))     strncpy(config.static_ip, request->arg("static_ip").c_str(), sizeof(config.static_ip)-1);
    if (request->hasArg("hvac_ip"))       strncpy(config.hvac_ip,   request->arg("hvac_ip").c_str(),   sizeof(config.hvac_ip)-1);
    if (request->hasArg("hvac_mdns")) {
      String m = request->arg("hvac_mdns"); m.replace(".local",""); m.trim();
      strncpy(config.hvac_mdns, m.c_str(), sizeof(config.hvac_mdns)-1);
    }
    config.hvac_enabled = request->hasArg("hvac_enabled");
    if (request->hasArg("hvac_thresh"))    HVAC_TRANSFER_THRESHOLD = request->arg("hvac_thresh").toFloat();
    if (request->hasArg("dt_start"))       DT_START_THRESHOLD = request->arg("dt_start").toFloat();
    if (request->hasArg("dt_stop"))        DT_STOP_THRESHOLD  = request->arg("dt_stop").toFloat();
    if (request->hasArg("tsun_min"))       TSUN_MIN_TEMP      = request->arg("tsun_min").toFloat();
    if (request->hasArg("tsun_overheat"))  TSUN_OVERHEAT      = request->arg("tsun_overheat").toFloat();
    if (request->hasArg("tsun_high"))      TSUN_HIGH          = request->arg("tsun_high").toFloat();
    if (request->hasArg("max_loss_streak")) MAX_LOSS_STREAK   = request->arg("max_loss_streak").toInt();
    if (request->hasArg("pwm_min"))        PWM_MIN      = request->arg("pwm_min").toInt();
    if (request->hasArg("pwm_max"))        PWM_MAX      = request->arg("pwm_max").toInt();
    if (request->hasArg("pwm_overheat"))   PWM_OVERHEAT = request->arg("pwm_overheat").toInt();
    if (request->hasArg("etmin"))          ETMIN             = request->arg("etmin").toFloat();
    if (request->hasArg("glycol_pct"))     GLYCOL_PERCENT    = request->arg("glycol_pct").toFloat();
    if (request->hasArg("boiler_volume"))  BOILER_VOLUME_TOTAL = request->arg("boiler_volume").toFloat();
    for (int i = 0; i < 5; i++) {
      char param[12]; snprintf(param, sizeof(param), "zone_vol_%d", i);
      if (request->hasArg(param)) ZONE_VOLUMES[i] = request->arg(param).toFloat();
    }
    if (request->hasArg("eq_max_kwh")) {
      EQ_MAX_KWH = request->arg("eq_max_kwh").toFloat();
      if (EQ_MAX_KWH < 1.0f) EQ_MAX_KWH = 1.0f;
    }
    if (request->hasArg("hour_start")) HOUR_START = request->arg("hour_start").toInt();
    if (request->hasArg("hour_end"))   HOUR_END   = request->arg("hour_end").toInt();
    for (int i = 0; i < 6; i++) {
      char param[16]; snprintf(param, sizeof(param), "sensor_nick_%d", i);
      if (request->hasArg(param)) {
        String n = request->arg(param); n.trim();
        if (n.length() > 0 && n.length() < 50) sensor_nicknames[i] = n;
      }
    }
    SIMULATION_MODE = request->hasArg("simulation_mode");
    saveConfig();
    Serial.println("Settings saved. Rebooting...");
    request->send(200,"text/html",
      "<h2 style='text-align:center;color:#369;padding:40px;'>Opgeslagen! Rebooting in 2s...</h2>");
    delay(2000); ESP.restart();
  });

  server.begin();
  Serial.println("Web server started");
}
