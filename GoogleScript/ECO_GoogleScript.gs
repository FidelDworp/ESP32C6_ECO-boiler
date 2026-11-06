// ============================================================
// ECO BOILER DATA LOGGER - Google Apps Script v2
// Receives JSON from Zarlar Dashboard (192.168.0.60)
// which polls /json from ECO controller (192.168.0.71)
//
// v2 (18mar26):
//   - 2 bevroren rijen: rij 1 = titel+URL, rij 2 = kolomtitels
//   - MAX_ROWS limiet toegevoegd (default 1000)
//   - HEADER_ROWS = 2 constante voor consistentie
//   - doPost() verwijdert oudste rij op rij 3 (niet rij 2)
//   - setupHeaders() aangemaakt (ontbrak in v1)
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

// ============================================================
// CONFIGURATIE
// ============================================================
const MAX_ROWS    = 1000;
const HEADER_ROWS = 2;
// ============================================================


function doPost(e) {
  try {
    const data = JSON.parse(e.postData.contents);
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

    const timestamp = Utilities.formatDate(
      new Date(), "Europe/Brussels", "yyyy-MM-dd HH:mm:ss"
    );

    const row = [
      timestamp,   data.a || 0,  data.l || 0,  data.m || 0,
      data.i || 0, data.j || 0,  data.k || 0,  data.b || 0,
      data.c || 0, data.d || 0,  data.e || 0,  data.f || 0,
      data.g || 0, data.h || 0,  data.n || 0,  data.o || 0,
      data.p || 0, data.q || 0,  data.r || 0,  data.s || 0
    ];

    sheet.appendRow(row);

    const dataRows = sheet.getLastRow() - HEADER_ROWS;
    if (dataRows > MAX_ROWS) {
      sheet.deleteRow(HEADER_ROWS + 1);
      Logger.log("MAX_ROWS (" + MAX_ROWS + ") bereikt — oudste rij verwijderd");
    }

    return ContentService
      .createTextOutput(JSON.stringify({
        status: "success", message: "Data gelogd",
        timestamp: timestamp, uptime: data.a
      }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (error) {
    Logger.log("Error: " + error.toString());
    return ContentService
      .createTextOutput(JSON.stringify({ status: "error", message: error.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}


function setupHeaders() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const ss    = SpreadsheetApp.getActiveSpreadsheet();

  if (sheet.getLastRow() >= 2) {
    if (sheet.getRange(2, 1).getValue() === "Tijdstempel") {
      sheet.deleteRow(2);
      Logger.log("Bestaande kolomtitelrij (rij 2) verwijderd.");
    }
  }
  if (sheet.getLastRow() >= 1) {
    const r1 = sheet.getRange(1, 1).getValue();
    if (typeof r1 === "string" && r1.startsWith("ECO")) {
      sheet.deleteRow(1);
      Logger.log("Bestaande titelrij (rij 1) verwijderd.");
    }
  }

  sheet.insertRowBefore(1);
  const titleCell = sheet.getRange(1, 1);
  titleCell.setValue("ECO BOILER DATA LOGGER v2  |  " + ss.getUrl());
  titleCell.setFontSize(9);
  titleCell.setFontWeight("normal");
  titleCell.setFontStyle("italic");
  titleCell.setFontColor("#cccccc");
  titleCell.setBackground("#222222");
  titleCell.setHorizontalAlignment("left");
  titleCell.setVerticalAlignment("middle");
  sheet.setRowHeight(1, 24);

  const headers = [
    "Tijdstempel",       "Uptime (s)",        "Tsun (°C)",
    "dT (°C)",           "EQtot (kWh)",        "dEQ (kWh)",
    "Yield today (kWh)", "ETopH (°C)",         "ETopL (°C)",
    "EMidH (°C)",        "EMidL (°C)",         "EBotH (°C)",
    "EBotL (°C)",        "EAv (°C)",           "PWM (0-255)",
    "Pomp",              "RSSI (dBm)",          "Free heap (%)",
    "Heap block (KB)",   "Heap min (KB)",
  ];

  sheet.insertRowBefore(2);
  const headerRange = sheet.getRange(2, 1, 1, headers.length);
  headerRange.setValues([headers]);
  headerRange.setFontSize(10);
  headerRange.setFontWeight("normal");
  headerRange.setFontStyle("normal");
  headerRange.setFontColor("#ffffff");
  headerRange.setBackground("#000000");
  headerRange.setHorizontalAlignment("center");
  headerRange.setVerticalAlignment("middle");
  headerRange.setWrap(true);
  sheet.setRowHeight(2, 40);

  sheet.setColumnWidth(1, 130);
  for (let i = 2; i <= headers.length; i++) sheet.setColumnWidth(i, 80);

  sheet.setFrozenRows(2);
  sheet.setFrozenColumns(1);

  Logger.log("Headers aangemaakt! " + headers.length + " kolommen (A t/m T)");
  Logger.log("Rij 1 = titelrij | Rij 2 = kolomtitels | Data vanaf rij 3");
  Logger.log("MAX_ROWS instelling: " + MAX_ROWS);
}


function test() {
  const testData = {
    postData: {
      contents: JSON.stringify({
        "a": 30932,
        "b": 61.0, "c": 57.9, "d": 51.5, "e": 49.3,
        "f": 40.5, "g": 36.0, "h": 49.5,
        "i": 8.25, "j": 0.033, "k": 16.4,
        "l": 14.0, "m": -26.5,
        "n": 0, "o": 0,
        "p": -55, "q": 42, "r": 38, "s": 31
      })
    }
  };
  const result = doPost(testData);
  Logger.log(result.getContent());
}


function sendDailySummary() {
  const sheet   = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const lastRow = sheet.getLastRow();
  if (lastRow < 3) { Logger.log("Nog geen data"); return; }

  const today = new Date();
  let todayCount = 0, totalYield = 0, maxSolar = 0, avgWiFi = 0, minHeap = 999;

  for (let i = lastRow; i > HEADER_ROWS; i--) {
    const ts = new Date(sheet.getRange(i, 1).getValue());
    if (ts.toDateString() !== today.toDateString()) break;
    todayCount++;
    const yield_val = sheet.getRange(i, 7).getValue();
    const solar_val = sheet.getRange(i, 3).getValue();
    const wifi_val  = sheet.getRange(i, 17).getValue();
    const heap_val  = sheet.getRange(i, 19).getValue();
    if (yield_val > totalYield) totalYield = yield_val;
    if (solar_val > maxSolar)   maxSolar   = solar_val;
    if (heap_val  < minHeap)    minHeap    = heap_val;
    avgWiFi += wifi_val;
  }

  if (todayCount === 0) return;
  avgWiFi = avgWiFi / todayCount;

  const expected   = 24 * 60 / 5;
  const percentage = (todayCount / expected * 100).toFixed(1);
  const heapStatus = minHeap >= 35 ? "✓ OK" : minHeap >= 25 ? "⚠ Laag" : "❌ Kritiek";

  MailApp.sendEmail({
    to: "filip.delannoy@gmail.com",
    subject: "☀️ ECO Boiler Dagelijkse Samenvatting - " +
      Utilities.formatDate(today, "Europe/Brussels", "dd/MM/yyyy"),
    body:
      "=== ECO BOILER DAGELIJKSE SAMENVATTING ===\n\n" +
      "📊 Data collectie:\n" +
      "  Entries: " + todayCount + "/" + expected + " (" + percentage + "%)\n" +
      "  Status: " + (percentage > 95 ? "✓ Uitstekend" : percentage > 80 ? "⚠ Matig" : "❌ Slecht") + "\n\n" +
      "☀️ Zonne-energie:\n" +
      "  Opbrengst vandaag: " + totalYield.toFixed(2) + " kWh\n" +
      "  Max collector temp: " + maxSolar.toFixed(1) + " °C\n\n" +
      "📡 Systeem:\n" +
      "  Gem. WiFi signaal: " + avgWiFi.toFixed(0) + " dBm " +
        (avgWiFi > -70 ? "(Goed)" : "(Zwak)") + "\n" +
      "  Min largest heap block: " + minHeap + " KB " + heapStatus + "\n\n" +
      "Bekijk volledige data: " + SpreadsheetApp.getActiveSpreadsheet().getUrl() + "\n"
  });

  Logger.log("Dagelijkse samenvatting verstuurd.");
}
