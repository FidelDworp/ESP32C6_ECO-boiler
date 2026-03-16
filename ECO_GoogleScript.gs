// ============================================================
// ECO BOILER DATA LOGGER - Google Apps Script
// Receives JSON from Zarlar Dashboard (192.168.0.60)
// which polls /json from ECO controller (192.168.0.71)
// 
// JSON keys v1.22 (a-s formaat):
//   a = uptime (s)
//   b = ETopH (°C)    c = ETopL (°C)
//   d = EMidH (°C)    e = EMidL (°C)
//   f = EBotH (°C)    g = EBotL (°C)
//   h = EAv (°C)
//   i = EQtot (kWh)
//   j = dEQ (kWh/10min)
//   k = yield_today (kWh)
//   l = Tsun (°C)
//   m = dT (°C)
//   n = pwm_value (0-255)
//   o = pump_relay (0/1)
//   p = wifi_rssi (dBm)
//   q = free_heap (%)
//   r = largest_block (KB)
//   s = min_free_heap (KB)
// ============================================================

function doPost(e) {
  try {
    const data = JSON.parse(e.postData.contents);
    
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
    
    const timestamp = Utilities.formatDate(
      new Date(),
      "Europe/Brussels",
      "yyyy-MM-dd HH:mm:ss"
    );
    
    const row = [
      timestamp,           // A: Timestamp
      data.a || 0,         // B: Uptime (s)
      data.l || 0,         // C: Tsun - collector temp (°C)
      data.m || 0,         // D: dT - temperatuurverschil (°C)
      data.i || 0,         // E: EQtot - totale energie (kWh)
      data.j || 0,         // F: dEQ - energiewijziging per 10min (kWh)
      data.k || 0,         // G: yield_today (kWh)
      data.b || 0,         // H: ETopH (°C)
      data.c || 0,         // I: ETopL (°C)
      data.d || 0,         // J: EMidH (°C)
      data.e || 0,         // K: EMidL (°C)
      data.f || 0,         // L: EBotH (°C)
      data.g || 0,         // M: EBotL (°C)
      data.h || 0,         // N: EAv - gemiddelde boilertemperatuur (°C)
      data.n || 0,         // O: pwm_value (0-255)
      data.o || 0,         // P: pump_relay (0/1)
      data.p || 0,         // Q: wifi_rssi (dBm)
      data.q || 0,         // R: free_heap (%)
      data.r || 0,         // S: largest_block (KB)
      data.s || 0          // T: min_free_heap (KB)
    ];
    
    sheet.appendRow(row);
    
    return ContentService
      .createTextOutput(JSON.stringify({
        'status': 'success',
        'message': 'Data logged',
        'timestamp': timestamp,
        'uptime': data.a
      }))
      .setMimeType(ContentService.MimeType.JSON);
      
  } catch (error) {
    Logger.log('Error: ' + error.toString());
    return ContentService
      .createTextOutput(JSON.stringify({
        'status': 'error',
        'message': error.toString()
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

// ============================================================
// TEST FUNCTION - Run once to verify setup
// Simuleert wat Zarlar Dashboard doorstuurt (v1.22 keys)
// ============================================================
function test() {
  const testData = {
    postData: {
      contents: JSON.stringify({
        "a": 30932,
        "b": 61.0,
        "c": 57.9,
        "d": 51.5,
        "e": 49.3,
        "f": 40.5,
        "g": 36.0,
        "h": 49.5,
        "i": 8.25,
        "j": 0.033,
        "k": 16.4,
        "l": 14.0,
        "m": -26.5,
        "n": 0,
        "o": 0,
        "p": -55,
        "q": 42,
        "r": 38,
        "s": 31
      })
    }
  };
  
  const result = doPost(testData);
  Logger.log(result.getContent());
}

// ============================================================
// DAILY SUMMARY EMAIL
// Set up trigger: Tools → Triggers → Add trigger
// Function: sendDailySummary
// Event: Time-driven → Day timer → 11 PM to midnight
// ============================================================
function sendDailySummary() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const lastRow = sheet.getLastRow();
  
  if (lastRow < 2) {
    Logger.log('No data yet');
    return;
  }
  
  const today = new Date();
  let todayCount = 0;
  let totalYield = 0;
  let maxSolar = 0;
  let avgWiFi = 0;
  let minHeap = 999;
  
  for (let i = lastRow; i > 1; i--) {
    const timestamp = new Date(sheet.getRange(i, 1).getValue());
    if (timestamp.toDateString() === today.toDateString()) {
      todayCount++;
      // Col G (7) = yield_today, Col C (3) = Tsun, Col Q (17) = wifi_rssi, Col S (19) = largest_block
      const yield_val  = sheet.getRange(i, 7).getValue();
      const solar_val  = sheet.getRange(i, 3).getValue();
      const wifi_val   = sheet.getRange(i, 17).getValue();
      const heap_val   = sheet.getRange(i, 19).getValue();
      
      if (yield_val > totalYield) totalYield = yield_val;
      if (solar_val > maxSolar)   maxSolar   = solar_val;
      if (heap_val  < minHeap)    minHeap    = heap_val;
      avgWiFi += wifi_val;
    } else {
      break;
    }
  }
  
  if (todayCount === 0) return;
  
  avgWiFi = avgWiFi / todayCount;
  
  const expected    = 24 * 60 / 5;  // 288 entries/dag bij 5-min interval
  const percentage  = (todayCount / expected * 100).toFixed(1);
  const heapStatus  = minHeap >= 35 ? '✓ OK' : minHeap >= 25 ? '⚠ Laag' : '❌ Kritiek';
  
  MailApp.sendEmail({
    to: 'filip.delannoy@gmail.com',
    subject: '☀️ ECO Boiler Daily Summary - ' + Utilities.formatDate(today, "Europe/Brussels", "dd/MM/yyyy"),
    body:
      '=== ECO BOILER DAILY SUMMARY ===\n\n' +
      '📊 Data collectie:\n' +
      '  Entries: ' + todayCount + '/' + expected + ' (' + percentage + '%)\n' +
      '  Status: ' + (percentage > 95 ? '✓ Excellent' : percentage > 80 ? '⚠ Fair' : '❌ Poor') + '\n\n' +
      '☀️ Zonne-energie:\n' +
      '  Opbrengst vandaag: ' + totalYield.toFixed(2) + ' kWh\n' +
      '  Max collector temp: ' + maxSolar.toFixed(1) + ' °C\n\n' +
      '📡 Systeem:\n' +
      '  Gem. WiFi signaal: ' + avgWiFi.toFixed(0) + ' dBm ' + (avgWiFi > -70 ? '(Goed)' : '(Zwak)') + '\n' +
      '  Min largest heap block: ' + minHeap + ' KB ' + heapStatus + '\n\n' +
      'Bekijk volledige data: ' + sheet.getParent().getUrl() + '\n'
  });
}