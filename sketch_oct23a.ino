#include <WiFiS3.h>              // Arduino UNO R4 WiFi
#include "DHT.h"                 // DHT sensor library
#include <LiquidCrystal_I2C.h>   // LCD I2C

// --------- WIFI SETTINGS (EDIT THESE) ---------
const char* ssid     = "DESKTOP-G6D8OLO 1420";
const char* password = "1r7580R}";
// ----------------------------------------------

// DHT11
#define DHTPIN  2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// MQ-2 (used as "Ammonia" sensor)
const int MQ2_PIN = A0;    // Analog output from MQ-2

// LEDs & buzzer
const int GREEN_LED_PIN  = 5;  // normal
const int YELLOW_LED_PIN = 6;  // moderate
const int RED_LED_PIN    = 7;  // high
const int BUZZER_PIN     = 9;  // buzzer

// LCD: address 0x27, 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Web server
WiFiServer server(80);

// --------- AMMONIA THRESHOLDS (MQ2 RAW) ---------
const int MQ2_NORMAL_MAX   = 250; // <= this: "Safe"
const int MQ2_MODERATE_MAX = 300; // <= this: "Warning"
// ----------------------------------------------

// --------- TEMP / HUMIDITY RISK THRESHOLDS -----
// Only used when ammonia is still "normal"
const float TEMP_RISK_MOD  = 27.0; // moderate risk from temperature
const float TEMP_RISK_HIGH = 33.0; // high risk from temperature
const float HUM_RISK_MOD   = 75.0; // moderate risk from humidity
const float HUM_RISK_HIGH  = 80.0; // high risk from humidity
// ----------------------------------------------

// Cached sensor values (for web responses)
float lastTemp       = NAN;
float lastHum        = NAN;
int   lastMQ2        = 0;
String lastLevel     = "normal"; // ammonia: normal/moderate/high
String lastRiskLevel = "none";   // temp/humidity risk: none/moderate/high

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 2000;

// ---------- BLINK CONTROL ----------
const unsigned long BLINK_INTERVAL_MS = 500;
bool gBlinkYellow = false;
bool gBlinkRed    = false;
bool gSolidGreen  = false;
bool gSolidYellow = false;
bool gSolidRed    = false;
// ------------------------------------

// ------------- HTML DASHBOARD -----------------
const char dashboardPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Poultry Air Quality Dashboard</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    --bg: #0f172a;
    --card-bg: rgba(15, 23, 42, 0.92);
    --card-soft: rgba(15, 23, 42, 0.85);
    --green: #22c55e;
    --yellow: #facc15;
    --red: #ef4444;
    --text-main: #e5e7eb;
    --text-soft: #9ca3af;
    --border-subtle: rgba(148,163,184,0.25);
  }

  * { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: radial-gradient(circle at top left, #1e293b 0, #020617 40%, #020617 100%);
    min-height: 100vh;
    color: var(--text-main);
    display: flex;
    justify-content: center;
    align-items: stretch;
    padding: 24px;
  }

  .page {
    width: 100%;
    max-width: 1100px;
    display: flex;
    flex-direction: column;
    gap: 20px;
  }

  header {
    display: flex;
    justify-content: space-between;
    gap: 16px;
    flex-wrap: wrap;
    align-items: center;
  }

  .title-block h1 {
    font-size: 1.8rem;
    letter-spacing: 0.03em;
  }

  .title-block p {
    color: var(--text-soft);
    font-size: 0.9rem;
    margin-top: 4px;
  }

  .badge {
    border-radius: 999px;
    border: 1px solid rgba(148,163,184,0.5);
    padding: 6px 14px;
    font-size: 0.8rem;
    display: inline-flex;
    align-items: center;
    gap: 6px;
    color: var(--text-soft);
    background: rgba(15,23,42,0.7);
  }

  .badge span.dot {
    width: 8px;
    height: 8px;
    border-radius: 999px;
    background: var(--green);
    box-shadow: 0 0 8px rgba(34,197,94,0.8);
  }

  main {
    flex: 1;
    display: grid;
    grid-template-columns: minmax(0, 2fr) minmax(0, 1.2fr);
    gap: 20px;
  }

  @media (max-width: 900px) {
    main { grid-template-columns: minmax(0, 1fr); }
  }

  .card {
    background: var(--card-bg);
    border-radius: 22px;
    padding: 22px 24px;
    box-shadow:
      0 18px 45px rgba(15,23,42,0.65),
      0 0 0 1px rgba(148,163,184,0.15);
    border: 1px solid var(--border-subtle);
  }

  .card-soft { background: var(--card-soft); }

  .status-chip {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    border-radius: 999px;
    padding: 6px 12px;
    font-size: 0.8rem;
    font-weight: 600;
  }

  .good { background: rgba(34,197,94,0.15); color: #bbf7d0; }   /* GREEN */
  .warn { background: rgba(250,204,21,0.15); color: #fef3c7; }  /* YELLOW */
  .bad  { background: rgba(239,68,68,0.18); color: #fecaca; }   /* RED */

  .status-chip svg { width: 16px; height: 16px; }

  .status-text {
    margin-top: 14px;
    font-size: 1rem;
    font-weight: 600;
  }

  .recommendation {
    margin-top: 10px;
    font-size: 0.9rem;
    color: var(--text-soft);
    line-height: 1.5;
  }

  #updated {
    margin-top: 16px;
    font-size: 0.8rem;
    color: #6b7280;
  }

  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    margin-top: 16px;
  }

  .tag-pill {
    font-size: 0.75rem;
    padding: 4px 10px;
    border-radius: 999px;
    background: rgba(15,23,42,0.8);
    border: 1px dashed rgba(148,163,184,0.6);
    color: var(--text-soft);
  }

  /* Metrics / cards on right side */
  .metrics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 14px;
  }

  .metric-title {
    font-size: 0.75rem;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-soft);
  }

  .metric-value {
    font-size: 2.4rem;
    font-weight: 700;
    margin-top: 6px;
  }

  .metric-unit {
    font-size: 1rem;
    color: #9ca3af;
  }

  .metric-footnote {
    margin-top: 8px;
    font-size: 0.8rem;
    color: #9ca3af;
  }

  .card-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: 8px;
    margin-bottom: 6px;
  }

  .card-header span.small {
    font-size: 0.8rem;
    color: #6b7280;
  }

  .mini-chip {
    border-radius: 999px;
    padding: 4px 10px;
    font-size: 0.75rem;
    background: rgba(15,23,42,0.9);
    border: 1px solid rgba(148,163,184,0.4);
    color: var(--text-soft);
  }

  footer {
    margin-top: 2px;
    font-size: 0.75rem;
    color: #6b7280;
    text-align: right;
  }

  /* BLINK for cases where LEDs blink (temp/humidity risk) */
  .blink {
    animation: blink 0.9s infinite alternate;
  }

  @keyframes blink {
    from { opacity: 1; }
    to   { opacity: 0.25; }
  }
</style>
</head>
<body>
<div class="page">
  <header>
    <div class="title-block">
      <h1>Poultry Ammonia Monitor</h1>
      <p>Arduino R4 · DHT11 · MQ2 (simulated NH₃)</p>
    </div>
    <div class="badge">
      <span class="dot"></span>
      Live coop conditions · Auto-refresh every 3s
    </div>
  </header>

  <main>
    <!-- LEFT: Main status card -->
    <section class="card">
      <div id="statusChip" class="status-chip good">
        <span id="statusChipText">Safe Environment</span>
      </div>

      <div id="statusText" class="status-text">
        ✅ Safe Environment (GREEN solid)
      </div>

      <div id="recommendation" class="recommendation">
        👌 Ammonia, temperature and humidity all within safe range.
      </div>

      <div class="tags">
        <span id="tagAmmonia" class="tag-pill">Ammonia: normal</span>
        <span id="tagRisk" class="tag-pill">Temp/Humidity risk: none</span>
      </div>

      <div id="updated">Last update: --:--:--</div>
    </section>

    <!-- RIGHT: metric cards -->
    <section class="card card-soft">
      <div class="metrics-grid">
        <!-- Temperature card -->
        <div class="metric-card">
          <div class="card-header">
            <span class="metric-title">Temperature</span>
            <span class="mini-chip" id="tempChip">Comfort range</span>
          </div>
          <div class="metric-value">
            <span id="tempValue">--</span>
            <span class="metric-unit">°C</span>
          </div>
          <div class="metric-footnote">
            Ideal comfort zone for broilers is typically 21–30&nbsp;°C depending on age.
          </div>
        </div>

        <!-- Humidity card -->
        <div class="metric-card">
          <div class="card-header">
            <span class="metric-title">Humidity</span>
            <span class="mini-chip" id="humChip">Stable</span>
          </div>
          <div class="metric-value">
            <span id="humValue">--</span>
            <span class="metric-unit">%</span>
          </div>
          <div class="metric-footnote">
            High humidity with warm litter can quickly increase NH₃ levels.
          </div>
        </div>

        <!-- Ammonia (MQ2) card -->
        <div class="metric-card">
          <div class="card-header">
            <span class="metric-title">Ammonia Index (MQ2)</span>
            <span class="mini-chip" id="nh3Chip">Sensor OK</span>
          </div>
          <div class="metric-value">
            <span id="nh3Value">--</span>
          </div>
          <div class="metric-footnote">
            Lower values indicate safer air. Thresholds are tuned to your coop.
          </div>
        </div>
      </div>
    </section>
  </main>

  <footer>
    Poultry Air Quality Dashboard · Local device: this page works only on your farm network.
  </footer>
</div>

<script>
  function setChipStyles(level, risk) {
    const chip = document.getElementById('statusChip');
    chip.className = 'status-chip'; // reset

    if (level === 'high') {
      chip.classList.add('bad');      // RED
    } else if (level === 'moderate') {
      chip.classList.add('warn');     // YELLOW
    } else {
      // ammonia normal – use risk to decide
      if (risk === 'high') {
        chip.classList.add('bad');    // RED blink case
      } else if (risk === 'moderate') {
        chip.classList.add('warn');   // YELLOW blink case
      } else {
        chip.classList.add('good');   // GREEN
      }
    }
  }

  async function fetchData() {
    try {
      const res = await fetch('/data');
      if (!res.ok) throw new Error('Network response not ok');
      const d = await res.json();

      const t      = d.temperature;
      const h      = d.humidity;
      const nh3    = d.ammonia;
      const level  = d.level;      // normal / moderate / high (ammonia)
      const risk   = d.riskLevel;  // none / moderate / high (temp/hum)

      // Update numeric values
      const tempEl = document.getElementById('tempValue');
      const humEl  = document.getElementById('humValue');
      const nh3El  = document.getElementById('nh3Value');

      if (t !== null && !isNaN(t)) tempEl.textContent = t.toFixed(1);
      else                         tempEl.textContent = '--';

      if (h !== null && !isNaN(h)) humEl.textContent = h.toFixed(1);
      else                         humEl.textContent = '--';

      nh3El.textContent = nh3.toString();

      const statusTextEl = document.getElementById('statusText');
      const recEl        = document.getElementById('recommendation');
      const chipTextEl   = document.getElementById('statusChipText');
      const chip         = document.getElementById('statusChip');

      // reset blink + colour each refresh
      statusTextEl.classList.remove('blink');
      chip.classList.remove('blink');
      statusTextEl.style.color = varTextMain(); // helper below

      // Status & recommendation logic
      if (level === "high") { // RED solid
        chipTextEl.textContent = "HIGH Ammonia · RED solid + buzzer";
        statusTextEl.textContent = "🔥 HIGH Ammonia (RED solid + buzzer)";
        statusTextEl.style.color = '#ef4444';   // RED like LED
        recEl.textContent = "💨 Open vents immediately, add fans and reduce stocking density.";
      } else if (level === "moderate") { // YELLOW solid
        chipTextEl.textContent = "MODERATE Ammonia · YELLOW solid";
        statusTextEl.textContent = "⚠️ MODERATE Ammonia (YELLOW solid)";
        statusTextEl.style.color = '#facc15';   // YELLOW like LED
        recEl.textContent = "🚪 Improve ventilation and check litter moisture.";
      } else {
        // ammonia normal – blinking comes from risk
        if (risk === "high") { // RED blinking LED
          chipTextEl.textContent = "High heat / humidity risk";
          statusTextEl.textContent = "⚠️ HIGH Temp/Humidity Risk (RED blinking)";
          statusTextEl.style.color = '#ef4444'; // RED
          statusTextEl.classList.add('blink');
          chip.classList.add('blink');
          recEl.textContent = "🌡️ Ammonia still safe, but heat/moisture very high. Check ventilation and litter urgently.";
        } else if (risk === "moderate") { // YELLOW blinking LED
          chipTextEl.textContent = "Temp/Humidity rising";
          statusTextEl.textContent = "⚠️ Temp/Humidity Rising (YELLOW blinking)";
          statusTextEl.style.color = '#facc15'; // YELLOW
          statusTextEl.classList.add('blink');
          chip.classList.add('blink');
          recEl.textContent = "🌡️ Early warning: conditions can cause NH₃ to rise. Improve ventilation and monitor birds.";
        } else {
          chipTextEl.textContent = "Safe Environment";
          statusTextEl.textContent = "✅ Safe Environment (GREEN solid)";
          statusTextEl.style.color = '#22c55e'; // GREEN like LED
          recEl.textContent = "👌 Ammonia, temperature and humidity all within safe range.";
        }
      }

      // Tag pills
      document.getElementById('tagAmmonia').textContent = "Ammonia: " + level;
      document.getElementById('tagRisk').textContent    = "Temp/Humidity risk: " + risk;

      // Small chips on metric cards
      const tempChip = document.getElementById('tempChip');
      const humChip  = document.getElementById('humChip');
      const nh3Chip  = document.getElementById('nh3Chip');

      if (t === null || isNaN(t)) {
        tempChip.textContent = "No reading";
      } else if (t >= 33) {
        tempChip.textContent = "Too hot";
      } else if (t >= 30) {
        tempChip.textContent = "Getting warm";
      } else {
        tempChip.textContent = "Comfort range";
      }

      if (h === null || isNaN(h)) {
        humChip.textContent = "No reading";
      } else if (h >= 80) {
        humChip.textContent = "Very humid";
      } else if (h >= 70) {
        humChip.textContent = "Rising moisture";
      } else {
        humChip.textContent = "Stable";
      }

      nh3Chip.textContent = "Level: " + level.toUpperCase();

      // Chip colour based on LED colour
      setChipStyles(level, risk);

      // Last updated time
      const now = new Date();
      document.getElementById('updated').textContent =
        "Last update: " + now.toLocaleTimeString();
    } catch (e) {
      console.error(e);
      const statusTextEl = document.getElementById('statusText');
      const chip         = document.getElementById('statusChip');
      document.getElementById('statusChipText').textContent = "No data";
      chip.className = "status-chip bad";
      statusTextEl.textContent = "Connection error";
      statusTextEl.style.color = '#ef4444';
      document.getElementById('recommendation').textContent =
        "Check that the Arduino board is powered and on the same Wi-Fi network.";
    }
  }

  // helper to get default text colour from CSS variable
  function varTextMain() {
    return getComputedStyle(document.documentElement).getPropertyValue('--text-main') || '#e5e7eb';
  }

  fetchData();
  setInterval(fetchData, 3000);
</script>
</body>
</html>
)rawliteral";

// ------------------------------------------------------

// Decide ammonia level from MQ-2 reading
String getAmmoniaLevel(int mq2Value) {
  if (mq2Value <= MQ2_NORMAL_MAX)   return "normal";
  if (mq2Value <= MQ2_MODERATE_MAX) return "moderate";
  return "high";
}

// Decide temperature/humidity risk level (only when ammonia is normal)
String getRiskLevel(float t, float h, const String &ammoLevel) {
  if (ammoLevel != "normal") return "none";

  bool tempHigh = (!isnan(t) && t >= TEMP_RISK_HIGH);
  bool humHigh  = (!isnan(h) && h >= HUM_RISK_HIGH);
  if (tempHigh || humHigh) return "high";

  bool tempMod = (!isnan(t) && t >= TEMP_RISK_MOD);
  bool humMod  = (!isnan(h) && h >= HUM_RISK_MOD);
  if (tempMod || humMod) return "moderate";

  return "none";
}

// Set LED modes + buzzer based on ammonia level + risk
void setOutputModes(const String &ammoLevel, const String &riskLevel) {
  // reset modes
  gBlinkYellow = gBlinkRed = false;
  gSolidGreen  = gSolidYellow = gSolidRed = false;

  if (ammoLevel == "high") {
    // true ammonia danger → RED solid + buzzer
    gSolidRed = true;
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else if (ammoLevel == "moderate") {
    // ammonia moderate → YELLOW solid
    gSolidYellow = true;
    digitalWrite(BUZZER_PIN, LOW);
  }
  else { // ammonia normal
    digitalWrite(BUZZER_PIN, LOW);

    if (riskLevel == "high") {
      // ammonia safe, BUT strong temp/humidity risk → RED blinking
      gBlinkRed = true;
    } else if (riskLevel == "moderate") {
      // ammonia safe, moderate temp/humidity risk → YELLOW blinking
      gBlinkYellow = true;
    } else {
      // fully safe → GREEN solid
      gSolidGreen = true;
    }
  }
}

// Drive LEDs every loop (non-blocking blink)
void refreshLEDs() {
  static unsigned long lastBlinkToggle = 0;
  static bool blinkPhase = false;

  unsigned long now = millis();
  if (now - lastBlinkToggle >= BLINK_INTERVAL_MS) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  bool greenOn  = gSolidGreen;
  bool yellowOn = gBlinkYellow ? blinkPhase : gSolidYellow;
  bool redOn    = gBlinkRed    ? blinkPhase : gSolidRed;

  digitalWrite(GREEN_LED_PIN,  greenOn  ? HIGH : LOW);
  digitalWrite(YELLOW_LED_PIN, yellowOn ? HIGH : LOW);
  digitalWrite(RED_LED_PIN,    redOn    ? HIGH : LOW);
}

// Update LCD text
void updateLCD(float t, float h, const String &ammoLevel, const String &riskLevel) {
  lcd.clear();

  // Line 1: temperature & humidity
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(t)) lcd.print("--.-");
  else lcd.print(t, 1);
  lcd.print("C H:");
  if (isnan(h)) lcd.print("--");
  else lcd.print(h, 0);
  lcd.print("%");

  // Line 2: status summary
  lcd.setCursor(0, 1);
  if (ammoLevel == "high") {
    lcd.print("NH3 HIGH! VENT  ");
  } else if (ammoLevel == "moderate") {
    lcd.print("NH3 MOD, Vent   ");
  } else { // ammonia normal
    if (riskLevel == "high") {
      lcd.print("Temp/Hum HIGH   ");
    } else if (riskLevel == "moderate") {
      lcd.print("Temp/Hum MOD    ");
    } else {
      lcd.print("Env SAFE        ");
    }
  }
}

// Read sensors + update global state + outputs
void readSensorsAndUpdate() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();   // °C
  int   mq2Value = analogRead(MQ2_PIN);

  String ammoLevel = getAmmoniaLevel(mq2Value);
  String riskLevel = getRiskLevel(t, h, ammoLevel);

  lastTemp       = t;
  lastHum        = h;
  lastMQ2        = mq2Value;
  lastLevel      = ammoLevel;
  lastRiskLevel  = riskLevel;

  setOutputModes(ammoLevel, riskLevel);
  updateLCD(t, h, ammoLevel, riskLevel);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  // WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");

  // Wait until DHCP gives real IP
  IPAddress ip = WiFi.localIP();
  while (ip[0] == 0) {
    Serial.println("Waiting for IP from DHCP...");
    delay(500);
    ip = WiFi.localIP();
  }

  Serial.print("IP address: ");
  Serial.println(ip);

  // ---- FIXED: show IP on LCD as text ----
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");
  lcd.setCursor(0, 1);
  lcd.print(ip[0]);
  lcd.print(".");
  lcd.print(ip[1]);
  lcd.print(".");
  lcd.print(ip[2]);
  lcd.print(".");
  lcd.print(ip[3]);
  delay(10000); // keep IP visible for 5 seconds

  // Now switch to normal sensor display
  readSensorsAndUpdate();
  lastSensorRead = millis();

  server.begin();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    readSensorsAndUpdate();
    lastSensorRead = now;
  }

  // apply blinking behaviour
  refreshLEDs();

  // HTTP server
  WiFiClient client = server.available();
  if (!client) return;

  while (!client.available()) {
    delay(1);
  }

  String req = client.readStringUntil('\r');
  client.readStringUntil('\n');

  bool askData = req.startsWith("GET /data");
  bool askRoot = req.startsWith("GET / ") || req.startsWith("GET /HTTP");

  if (askData) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.print("{\"temperature\":");
    if (isnan(lastTemp)) client.print("null"); else client.print(lastTemp, 1);
    client.print(",\"humidity\":");
    if (isnan(lastHum)) client.print("null"); else client.print(lastHum, 1);
    client.print(",\"ammonia\":");
    client.print(lastMQ2);
    client.print(",\"level\":\"");
    client.print(lastLevel);
    client.print("\",\"riskLevel\":\"");
    client.print(lastRiskLevel);
    client.println("\"}");
  }
  else if (askRoot) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.print(dashboardPage);
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("Not Found");
  }

  delay(1);
  client.stop();
}
