// ==========================================================
// AIR QUALITY SENSOR LOGGER
// XIAO ESP32C6 + BME688 + LittleFS + Web Page + Deep Sleep
//
// FUNCTION:
// 1. On first real boot, sets a MANUAL initial date/time
// 2. Creates its own Wi-Fi network (Access Point)
// 3. While awake for 5 minutes, takes 10 samples every 30 seconds
// 4. Saves EVERY RAW SAMPLE into CSV
// 5. Shows current sample and running averages in a web page
// 6. Allows CSV download and CSV erase from the web page
// 7. Shows file size and number of records
// 8. Goes to deep sleep for 5 minutes
//
// IMPORTANT:
// - The web page only exists during the 5-minute awake window
// - The internal clock continues during deep sleep
// - If the board loses full power, time resets to the manual value
// ==========================================================


// ======================= LIBRARIES =======================
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <sys/time.h>


// ======================= OBJECTS =======================
Adafruit_BME680 bme;
WebServer server(80);


// ======================= ACCESS POINT CONFIG =======================
const char* ap_ssid = "AirSensor_BME688";
const char* ap_password = "12345678";


// ======================= TIMING CONFIG =======================

// Total number of samples during the awake window
const int totalSamples = 10;

// Time between samples
const unsigned long sampleInterval = 30000UL;   // 30 seconds

// Deep sleep duration
const uint64_t sleepTimeSeconds = 300;          // 5 minutes


// ======================= IAQ CONFIG =======================

// Simple baseline for educational IAQ calculation
float gas_baseline = 40.0;


// ======================= LIVE SAMPLE VARIABLES =======================
// These are the last raw sample values shown live on the web page
float currentTemp = 0.0;
float currentHum = 0.0;
float currentPres = 0.0;
float currentGas = 0.0;
float currentIAQ = 0.0;


// ======================= AVERAGE VARIABLES =======================
// These store the running averages during the current awake cycle
float avgTemp = 0.0;
float avgHum = 0.0;
float avgPres = 0.0;
float avgGas = 0.0;
float avgIAQ = 0.0;


// ======================= SAMPLE CONTROL =======================
int samplesTaken = 0;

float sumTemp = 0.0;
float sumHum = 0.0;
float sumPres = 0.0;
float sumGas = 0.0;
float sumIAQ = 0.0;

unsigned long lastSampleMillis = 0;
bool cycleFinished = false;


// ======================= FUNCTION: SET MANUAL DATE/TIME =======================
// This is the initial manual date and time.
// It is only applied on a real boot, not on wake from deep sleep.
void setManualDateTime() {
  struct tm tm;

  // Year = actual year - 1900
  tm.tm_year = 2026 - 1900;

  // Month is zero-based:
  // January = 0, February = 1, March = 2, April = 3...
  tm.tm_mon  = 3;   // April

  tm.tm_mday = 13;
  tm.tm_hour = 12;
  tm.tm_min  = 0;
  tm.tm_sec  = 0;

  // Daylight saving flag
  tm.tm_isdst = -1;

  time_t t = mktime(&tm);
  struct timeval now = { t, 0 };
  settimeofday(&now, NULL);
}


// ======================= FUNCTION: GET CURRENT TIME STRING =======================
String getTimeString() {
  time_t now;
  time(&now);

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

  return String(buffer);
}


// ======================= FUNCTION: IAQ STATUS =======================
String getIAQStatus(float iaqValue) {
  if (iaqValue < 90) {
    return "Very Good";
  } else if (iaqValue < 110) {
    return "Good";
  } else if (iaqValue < 130) {
    return "Moderate";
  } else {
    return "Poor";
  }
}


// ======================= FUNCTION: COUNT CSV RECORDS =======================
// Counts how many data rows are stored in the CSV file.
// The header line is not counted as a data record.
int countCSVRecords() {
  File file = LittleFS.open("/data.csv", "r");

  if (!file) {
    return 0;
  }

  int lineCount = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      lineCount++;
    }
  }

  file.close();

  // If file has at least one line, subtract header
  if (lineCount > 0) {
    return lineCount - 1;
  }

  return 0;
}


// ======================= FUNCTION: GET CSV FILE SIZE =======================
String getCSVFileSizeString() {
  File file = LittleFS.open("/data.csv", "r");

  if (!file) {
    return "0 B";
  }

  size_t size = file.size();
  file.close();

  if (size < 1024) {
    return String(size) + " B";
  } else if (size < 1024 * 1024) {
    return String(size / 1024.0, 2) + " KB";
  } else {
    return String(size / 1024.0 / 1024.0, 2) + " MB";
  }
}


// ======================= FUNCTION: ENSURE CSV EXISTS =======================
void ensureCSVExists() {
  if (!LittleFS.exists("/data.csv")) {
    File file = LittleFS.open("/data.csv", "w");

    if (file) {
      file.println("timestamp,temp_C,humidity_percent,pressure_hPa,gas_kOhms,iaq_simple");
      file.close();
      Serial.println("CSV file created with header.");
    } else {
      Serial.println("Error creating /data.csv");
    }
  }
}


// ======================= FUNCTION: SAVE ONE RAW SAMPLE TO CSV =======================
void saveSampleToCSV(float temp, float hum, float pres, float gas, float iaq) {
  String dataLine = getTimeString() + "," +
                    String(temp, 2) + "," +
                    String(hum, 2) + "," +
                    String(pres, 2) + "," +
                    String(gas, 2) + "," +
                    String(iaq, 2);

  File file = LittleFS.open("/data.csv", "a");

  if (file) {
    file.println(dataLine);
    file.close();
    Serial.println("Saved RAW sample: " + dataLine);
  } else {
    Serial.println("Error opening /data.csv");
  }
}


// ======================= FUNCTION: TAKE ONE SAMPLE =======================
// Takes one sensor reading, updates live values, updates running averages,
// and saves the RAW sample to CSV immediately.
void takeOneSample() {
  if (!bme.performReading()) {
    Serial.println("Error reading BME688");
    return;
  }

  // Store current raw sample
  currentTemp = bme.temperature;
  currentHum  = bme.humidity;
  currentPres = bme.pressure / 100.0;
  currentGas  = bme.gas_resistance / 1000.0;
  currentIAQ  = 100.0 * (gas_baseline / currentGas);

  // Add to sums for running averages
  sumTemp += currentTemp;
  sumHum  += currentHum;
  sumPres += currentPres;
  sumGas  += currentGas;
  sumIAQ  += currentIAQ;

  // Increase valid sample counter
  samplesTaken++;

  // Update averages
  avgTemp = sumTemp / samplesTaken;
  avgHum  = sumHum  / samplesTaken;
  avgPres = sumPres / samplesTaken;
  avgGas  = sumGas  / samplesTaken;
  avgIAQ  = sumIAQ  / samplesTaken;

  // Save raw sample to CSV
  saveSampleToCSV(currentTemp, currentHum, currentPres, currentGas, currentIAQ);

  // Serial debug output
  Serial.println("----- Sample " + String(samplesTaken) + " -----");
  Serial.print("Time: ");
  Serial.println(getTimeString());

  Serial.print("Temp: ");
  Serial.println(currentTemp, 2);

  Serial.print("Humidity: ");
  Serial.println(currentHum, 2);

  Serial.print("Pressure: ");
  Serial.println(currentPres, 2);

  Serial.print("Gas: ");
  Serial.println(currentGas, 2);

  Serial.print("Simple IAQ: ");
  Serial.println(currentIAQ, 2);

  Serial.println("----- Running Average -----");
  Serial.print("Avg Temp: ");
  Serial.println(avgTemp, 2);
  Serial.print("Avg Humidity: ");
  Serial.println(avgHum, 2);
  Serial.print("Avg Pressure: ");
  Serial.println(avgPres, 2);
  Serial.print("Avg Gas: ");
  Serial.println(avgGas, 2);
  Serial.print("Avg IAQ: ");
  Serial.println(avgIAQ, 2);
}


// ======================= FUNCTION: BUILD HTML PAGE =======================
// Builds the embedded HTML page shown in the phone browser.
String htmlPage() {
  String page = "";

  int totalRecords = countCSVRecords();
  String fileSize = getCSVFileSizeString();

  page += "<!DOCTYPE html>";
  page += "<html lang='en'>";
  page += "<head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<meta http-equiv='refresh' content='5'>";
  page += "<title>Air Quality Sensor</title>";
  page += "<style>";
  page += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;color:#222;}";
  page += "h1{font-size:24px;}";
  page += ".card{background:white;padding:16px;border-radius:12px;margin-bottom:12px;box-shadow:0 2px 8px rgba(0,0,0,0.08);}";
  page += ".value{font-size:22px;font-weight:bold;}";
  page += ".label{font-size:14px;color:#666;}";
  page += "a.button, button{display:inline-block;padding:12px 16px;background:#222;color:white;text-decoration:none;border:none;border-radius:8px;font-size:16px;cursor:pointer;margin-right:8px;}";
  page += ".danger{background:#b00020;}";
  page += ".row{display:flex;flex-wrap:wrap;gap:10px;}";
  page += "</style>";
  page += "</head>";
  page += "<body>";

  page += "<h1>Air Quality Sensor - BME688</h1>";

  page += "<div class='card'><div class='label'>Current Time</div><div class='value'>" + getTimeString() + "</div></div>";

  page += "<div class='row'>";
  page += "<div class='card' style='flex:1;min-width:220px;'><div class='label'>Samples Taken in Current Cycle</div><div class='value'>" + String(samplesTaken) + " / " + String(totalSamples) + "</div></div>";
  page += "<div class='card' style='flex:1;min-width:220px;'><div class='label'>CSV Records</div><div class='value'>" + String(totalRecords) + "</div></div>";
  page += "<div class='card' style='flex:1;min-width:220px;'><div class='label'>CSV File Size</div><div class='value'>" + fileSize + "</div></div>";
  page += "</div>";

  page += "<div class='card'><div class='label'>Last Temperature Sample</div><div class='value'>" + String(currentTemp, 2) + " &deg;C</div></div>";
  page += "<div class='card'><div class='label'>Last Humidity Sample</div><div class='value'>" + String(currentHum, 2) + " %</div></div>";
  page += "<div class='card'><div class='label'>Last Pressure Sample</div><div class='value'>" + String(currentPres, 2) + " hPa</div></div>";
  page += "<div class='card'><div class='label'>Last Gas Sample</div><div class='value'>" + String(currentGas, 2) + " kOhms</div></div>";
  page += "<div class='card'><div class='label'>Last Simple IAQ Sample</div><div class='value'>" + String(currentIAQ, 2) + "</div><div class='label'>Status: " + getIAQStatus(currentIAQ) + "</div></div>";

  page += "<div class='card'><div class='label'>Average Temperature (Current Cycle)</div><div class='value'>" + String(avgTemp, 2) + " &deg;C</div></div>";
  page += "<div class='card'><div class='label'>Average Humidity (Current Cycle)</div><div class='value'>" + String(avgHum, 2) + " %</div></div>";
  page += "<div class='card'><div class='label'>Average Pressure (Current Cycle)</div><div class='value'>" + String(avgPres, 2) + " hPa</div></div>";
  page += "<div class='card'><div class='label'>Average Gas (Current Cycle)</div><div class='value'>" + String(avgGas, 2) + " kOhms</div></div>";
  page += "<div class='card'><div class='label'>Average IAQ (Current Cycle)</div><div class='value'>" + String(avgIAQ, 2) + "</div><div class='label'>Status: " + getIAQStatus(avgIAQ) + "</div></div>";

  page += "<div class='card'>";
  page += "<a class='button' href='/download'>Download CSV</a>";
  page += "<a class='button danger' href='/clear' onclick=\"return confirm('Delete all CSV data?');\">Clear CSV</a>";
  page += "</div>";

  page += "</body>";
  page += "</html>";

  return page;
}


// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting system...");

  // Check wakeup reason
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

  // Only set manual date/time on a real boot
  if (wakeupReason != ESP_SLEEP_WAKEUP_TIMER) {
    setManualDateTime();
    Serial.print("Manual initial time set to: ");
    Serial.println(getTimeString());
  } else {
    Serial.print("Woke up from deep sleep. Current time: ");
    Serial.println(getTimeString());
  }

  // Start I2C
  Wire.begin();

  // Start BME688
  if (!bme.begin(0x77)) {
    Serial.println("BME688 not found at 0x77. Check wiring.");
    while (1);
  }

  Serial.println("BME688 found.");

  // Sensor configuration
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setGasHeater(320, 150);

  // Mount LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS");
    while (1);
  }

  Serial.println("LittleFS mounted.");

  // Make sure CSV exists
  ensureCSVExists();

  // Start Access Point
  WiFi.mode(WIFI_AP);
  bool apOK = WiFi.softAP(ap_ssid, ap_password);

  if (!apOK) {
    Serial.println("Failed to create Access Point.");
  } else {
    Serial.println("Access Point created.");
  }

  delay(1000);

  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.print("Open this IP in your phone browser: ");
  Serial.println(WiFi.softAPIP());

  // Route: main page
  server.on("/", []() {
    server.send(200, "text/html", htmlPage());
  });

  // Route: download CSV
  server.on("/download", []() {
    File file = LittleFS.open("/data.csv", "r");

    if (!file) {
      server.send(500, "text/plain", "Error opening CSV file");
      return;
    }

    server.sendHeader("Content-Type", "text/csv");
    server.sendHeader("Content-Disposition", "attachment; filename=data.csv");
    server.streamFile(file, "text/csv");
    file.close();
  });

  // Route: clear CSV and recreate header
  server.on("/clear", []() {
    LittleFS.remove("/data.csv");
    ensureCSVExists();
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("CSV file cleared from web request.");
  });

  // Start web server
  server.begin();
  Serial.println("Web server started.");

  // Take first sample immediately on wakeup
  takeOneSample();
  lastSampleMillis = millis();
}


// ======================= LOOP =======================
void loop() {
  // Keep web page responsive while awake
  server.handleClient();

  // Take the next sample every 30 seconds until 10 samples are collected
  if (!cycleFinished && samplesTaken < totalSamples && millis() - lastSampleMillis >= sampleInterval) {
    lastSampleMillis = millis();
    takeOneSample();
  }

  // Once 10 samples are done, finish cycle and sleep
  if (!cycleFinished && samplesTaken >= totalSamples) {
    cycleFinished = true;

    Serial.println("===== FINAL AVERAGE OF CURRENT CYCLE =====");
    Serial.print("Average Temp: ");
    Serial.println(avgTemp, 2);
    Serial.print("Average Humidity: ");
    Serial.println(avgHum, 2);
    Serial.print("Average Pressure: ");
    Serial.println(avgPres, 2);
    Serial.print("Average Gas: ");
    Serial.println(avgGas, 2);
    Serial.print("Average IAQ: ");
    Serial.println(avgIAQ, 2);
    Serial.print("Status: ");
    Serial.println(getIAQStatus(avgIAQ));
    Serial.println("==========================================");

    // Small pause so the last web refresh has a chance to happen
    delay(500);

    // Turn Wi-Fi off before sleeping
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    Serial.println("Going to deep sleep for 5 minutes...");
    delay(100);

    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);
    esp_deep_sleep_start();
  }
}