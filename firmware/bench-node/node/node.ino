// Open Vibration Node — bench node firmware v6 (RAK3112: ESP32-S3 + SX1262 + IIS3DWB)
//
// Interim point-to-point LoRa firmware used for bench characterisation. The LoRaWAN node
// firmware (proposal milestone) will reuse the acquisition and processing core below and
// replace the radio layer. See README.md in this folder.
//
// Every cycle:
//  - reads a gap-free 8192-sample window (307 ms) from the IIS3DWB hardware FIFO. Polling
//    the data-ready flag over SPI sustains only ~15 kS/s and silently drops ~40 % of the
//    26.667 kHz samples (docs/measurements.md, Errata 2), so nothing time-domain uses it.
//  - computes per axis: broadband acceleration RMS and peak (mg), and velocity RMS in the
//    ISO 10816 band 10–1000 Hz (mm/s); crest factor from the three axes.
//  - transmits a text summary at 869.525 MHz / SF7, then listens 1.8 s for a downlink.
//  - self-monitoring: WHO_AM_I mismatch or FIFO timeout set the e=1 health flag and trigger
//    a sensor re-init; 90 s task watchdog; radio re-init after 5 consecutive TX errors,
//    restart after 20.
//  - cadence is enforced >= 3 s: an 85-byte packet is 226 ms on air, i.e. 7.5 % of the
//    10 % duty-cycle budget of the 869.4–869.65 MHz sub-band (2 s would be 11 %).
//
// Raw capture on demand: downlink "CAP id=<id>" -> 32768 samples per axis (1.23 s) into
// PSRAM -> WiFi -> HTTP POST to the collector (/api/raw?node=<id>&n=32768&sr=26667,
// body = int16 little-endian X block || Y block || Z block).
// USB serial (115200): WIFI <ssid> <password> | INT <s> | CAP | URL <http://host:port/api/raw>
//                      | URL - (back to automatic) | STAT
// Downlinks: "CFG id=<id> int=<s>" (cadence, stored in NVS), "CAP id=<id>" (raw capture).
#include <RadioLib.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

// SX1262 inside the RAK3112 module
#define R_SCK 5
#define R_MISO 3
#define R_MOSI 6
#define R_NSS 7
#define R_RST 8
#define R_DIO1 47
#define R_BUSY 48
#define R_ANTSW 4

// IIS3DWB sensor board (HSPI)
#define S_SCK 13
#define S_MISO 10
#define S_MOSI 11
#define S_CS 12
#define REG_WHO_AM_I 0x0F
#define REG_CTRL1_XL 0x10
#define REG_CTRL3_C 0x12
#define REG_STATUS 0x1E
#define REG_OUT_TEMP_L 0x20
#define REG_OUTX_L_A 0x28
#define REG_FIFO_CTRL1 0x07
#define REG_FIFO_CTRL2 0x08
#define REG_FIFO_CTRL3 0x09
#define REG_FIFO_CTRL4 0x0A
#define REG_FIFO_STATUS1 0x3A
#define REG_FIFO_TAG 0x78
const float MG_PER_LSB = 0.244f;  // ±8 g range
const double FS = 26667.0;
const double DT = 1.0 / FS;
const int N = 8192;               // metrics window: 307 ms (3.3 Hz resolution)
const int WARM = 2730;            // filter settling (~100 ms) excluded from the RMS
const double HP_FC = 8.0;         // two cascaded HP sections -> combined -3 dB at 10 Hz (ISO 10816 lower edge)
const double LP_FC = 1000.0;      // ISO 10816 upper edge
const int CAPN = 32768;           // raw capture window (1.23 s)
const uint32_t MIN_INT = 3;       // s; 2 s would exceed the 10 % duty cycle (226 ms on air)
const uint32_t WDT_S = 90;        // task watchdog
// Collector address: 1) URL stored via the USB "URL" command, 2) mDNS <DASH_HOST>.local,
// 3) this placeholder (RFC 5737 TEST-NET address — replace with your collector's IP).
const char *DASH_URL_DEFAULT = "http://192.0.2.10:8080/api/raw";
const char *DASH_HOST = "vib-collector";

SPIClass sensorSPI(HSPI);
SPISettings senCfg(8000000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(R_NSS, R_DIO1, R_RST, R_BUSY);
Preferences prefs;

// Biquad (Butterworth, Q = 0.707) in double precision: at fc/fs ≈ 3e-4 a float32 biquad
// loses its poles to rounding. Defined before the first function because the Arduino
// prototype generator inserts prototypes ahead of it.
struct Biquad {
  double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
  inline double run(double x) {
    double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

int16_t bx[N], by[N], bz[N];
int16_t *capX, *capY, *capZ;      // PSRAM
char nodeId[5];
uint32_t intervalS = 5;
bool doCapture = false, sensorOk = false;
uint8_t whoAmI = 0;
String wifiSsid, wifiPass, dashUrl;   // empty dashUrl = automatic (mDNS -> default address)
int txErr = 0;
volatile bool rxFlag = false;
void onRx() { rxFlag = true; }

// ---------------------------------------------------------------- sensor
uint8_t senRead(uint8_t reg) {
  sensorSPI.beginTransaction(senCfg); digitalWrite(S_CS, LOW);
  sensorSPI.transfer(reg | 0x80); uint8_t v = sensorSPI.transfer(0);
  digitalWrite(S_CS, HIGH); sensorSPI.endTransaction(); return v;
}
void senWrite(uint8_t reg, uint8_t val) {
  sensorSPI.beginTransaction(senCfg); digitalWrite(S_CS, LOW);
  sensorSPI.transfer(reg & 0x7F); sensorSPI.transfer(val);
  digitalWrite(S_CS, HIGH); sensorSPI.endTransaction();
}
// One sample via the data-ready flag (fallback path only). false = no sample within 2 ms.
bool readSample(int16_t *x, int16_t *y, int16_t *z) {
  uint8_t raw[6];
  uint32_t t0 = micros();
  while ((senRead(REG_STATUS) & 1) == 0) { if (micros() - t0 > 2000) return false; }
  sensorSPI.beginTransaction(senCfg); digitalWrite(S_CS, LOW);
  sensorSPI.transfer(REG_OUTX_L_A | 0x80);
  for (int b = 0; b < 6; b++) raw[b] = sensorSPI.transfer(0);
  digitalWrite(S_CS, HIGH); sensorSPI.endTransaction();
  *x = (int16_t)(raw[0] | (raw[1] << 8));
  *y = (int16_t)(raw[2] | (raw[3] << 8));
  *z = (int16_t)(raw[4] | (raw[5] << 8));
  return true;
}
bool sensorInit() {
  senWrite(REG_CTRL3_C, 0x01); delay(20);     // software reset
  senWrite(REG_CTRL3_C, 0x44);                // BDU + IF_INC
  senWrite(REG_CTRL1_XL, 0xAC);               // 26.667 kHz, ±8 g — the only valid full-rate value (measurements.md, Errata 1)
  delay(50);
  whoAmI = senRead(REG_WHO_AM_I);
  sensorOk = (whoAmI == 0x7B);
  return sensorOk;
}
// FIFO read: the sensor batches samples into its 3 KB queue and we drain it in bursts of up
// to 64 words — no sample is lost. Returns the number of samples collected (< n means the
// sensor stopped delivering within maxMs -> sensor fault).
int fifoRead(int16_t *X, int16_t *Y, int16_t *Z, int n, uint32_t maxMs, int *badTagOut) {
  senWrite(REG_FIFO_CTRL4, 0x00);           // bypass — clears the queue
  delayMicroseconds(200);
  senWrite(REG_FIFO_CTRL1, 0x00);
  senWrite(REG_FIFO_CTRL2, 0x00);
  senWrite(REG_FIFO_CTRL3, 0x0A);           // BDR_XL = 26.667 kHz batching
  senWrite(REG_FIFO_CTRL4, 0x06);           // continuous mode
  uint32_t t0 = millis();
  int got = 0, badTag = 0;
  uint8_t w[7];
  while (got < n) {
    uint8_t s1 = senRead(REG_FIFO_STATUS1);
    uint8_t s2 = senRead(REG_FIFO_STATUS1 + 1);
    int words = ((s2 & 0x03) << 8) | s1;   // 10-bit word count
    if (words == 0) {
      if (millis() - t0 > maxMs) break;     // hang protection
      continue;
    }
    if (words > 64) words = 64;
    for (int k = 0; k < words && got < n; k++) {
      sensorSPI.beginTransaction(senCfg); digitalWrite(S_CS, LOW);
      sensorSPI.transfer(REG_FIFO_TAG | 0x80);
      for (int b = 0; b < 7; b++) w[b] = sensorSPI.transfer(0);
      digitalWrite(S_CS, HIGH); sensorSPI.endTransaction();
      uint8_t tag = w[0] >> 3;
      if (tag == 0x02) {                    // accelerometer data word
        X[got] = (int16_t)(w[1] | (w[2] << 8));
        Y[got] = (int16_t)(w[3] | (w[4] << 8));
        Z[got] = (int16_t)(w[5] | (w[6] << 8));
        got++;
      } else badTag++;
    }
  }
  senWrite(REG_FIFO_CTRL4, 0x00);           // back to bypass
  if (badTagOut) *badTagOut = badTag;
  return got;
}
// Metrics window from the FIFO. false = incomplete (packet flag e=1).
bool acquire() {
  int got = fifoRead(bx, by, bz, N, 1500, NULL);
  for (int i = got; i < N; i++) bx[i] = by[i] = bz[i] = 0;
  return got == N;
}
float dieTemp() {
  sensorSPI.beginTransaction(senCfg); digitalWrite(S_CS, LOW);
  sensorSPI.transfer(REG_OUT_TEMP_L | 0x80);
  uint8_t lo = sensorSPI.transfer(0), hi = sensorSPI.transfer(0);
  digitalWrite(S_CS, HIGH); sensorSPI.endTransaction();
  return 25.0f + (int16_t)(lo | (hi << 8)) / 256.0f;
}

// ---------------------------------------------------------------- metrics (ISO 10816)
static void bqLowpass(Biquad &f, double fc) {
  double w = 2 * M_PI * fc / FS, c = cos(w), s = sin(w), al = s / (2 * 0.70710678), a0 = 1 + al;
  f.b0 = (1 - c) / 2 / a0; f.b1 = (1 - c) / a0; f.b2 = f.b0;
  f.a1 = -2 * c / a0; f.a2 = (1 - al) / a0; f.z1 = f.z2 = 0;
}
static void bqHighpass(Biquad &f, double fc) {
  double w = 2 * M_PI * fc / FS, c = cos(w), s = sin(w), al = s / (2 * 0.70710678), a0 = 1 + al;
  f.b0 = (1 + c) / 2 / a0; f.b1 = -(1 + c) / a0; f.b2 = f.b0;
  f.a1 = -2 * c / a0; f.a2 = (1 - al) / a0; f.z1 = f.z2 = 0;
}
// aRms/aPeak: broadband acceleration RMS and peak (mg, for the crest factor);
// vRms: velocity RMS in the 10–1000 Hz band (mm/s) per ISO 10816:
//   acceleration -> HP 8 Hz -> trapezoidal integration -> HP 8 Hz -> LP 1000 Hz -> RMS (after settling)
void axisMetrics(int16_t *b, float *aRms, float *aPeak, float *vRms) {
  double mean = 0;
  for (int i = 0; i < N; i++) mean += b[i];
  mean /= N;
  Biquad hpA, hpV, lp;
  bqHighpass(hpA, HP_FC); bqHighpass(hpV, HP_FC); bqLowpass(lp, LP_FC);
  double q = 0, peak = 0, v = 0, prev = 0, vq = 0;
  int cnt = 0;
  for (int i = 0; i < N; i++) {
    double a = (b[i] - mean) * MG_PER_LSB;        // mg
    q += a * a;
    if (fabs(a) > peak) peak = fabs(a);
    double x = hpA.run(a * 0.00980665);           // m/s^2
    v += 0.5 * (x + prev) * DT; prev = x;         // m/s
    double y = lp.run(hpV.run(v));
    if (i >= WARM) { vq += y * y; cnt++; }
  }
  *aRms = sqrt(q / N);
  *aPeak = peak;
  *vRms = sqrt(vq / cnt) * 1000.0;
}

// ---------------------------------------------------------------- USB commands
void printStatus() {
  Serial.printf("STAT id=%s int=%lu s wifi=%s url=%s sensor=%s(0x%02X) txErr=%d uptime=%lu s reset=%d\n",
                nodeId, (unsigned long)intervalS, wifiSsid.length() ? wifiSsid.c_str() : "(none)",
                dashUrl.length() ? dashUrl.c_str() : "auto(mDNS->default)", sensorOk ? "OK" : "FAULT",
                whoAmI, txErr, (unsigned long)(millis() / 1000), (int)esp_reset_reason());
}
void pollUsbSerial() {
  static char lb[96]; static int ll = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      lb[ll] = 0;
      if (strncmp(lb, "WIFI ", 5) == 0) {
        char ss[40], pw[40];
        if (sscanf(lb + 5, "%39s %39s", ss, pw) == 2) {
          wifiSsid = ss; wifiPass = pw;
          prefs.putString("ssid", wifiSsid);
          prefs.putString("pass", wifiPass);
          Serial.print("WIFI stored: "); Serial.println(wifiSsid);
        }
      } else if (strncmp(lb, "INT ", 4) == 0) {
        long v = atol(lb + 4);
        if (v >= (long)MIN_INT && v <= 86400) { intervalS = v; prefs.putUInt("int", intervalS);
          Serial.print("INT="); Serial.println(intervalS); }
        else { Serial.print("INT rejected: allowed range "); Serial.print(MIN_INT); Serial.println("..86400 s"); }
      } else if (strcmp(lb, "CAP") == 0) {
        doCapture = true; Serial.println("CAP requested (USB)");
      } else if (strncmp(lb, "URL ", 4) == 0) {
        String u = String(lb + 4); u.trim();
        if (u == "-") { dashUrl = ""; prefs.remove("url"); Serial.println("URL: automatic (mDNS -> default address)"); }
        else if (u.startsWith("http://")) { dashUrl = u; prefs.putString("url", dashUrl);
          Serial.print("URL stored: "); Serial.println(dashUrl); }
        else Serial.println("URL must start with http://");
      } else if (strcmp(lb, "STAT") == 0) {
        printStatus();
      }
      ll = 0;
    } else if (ll < (int)sizeof(lb) - 1) lb[ll++] = ch;
  }
}

// ---------------------------------------------------------------- raw capture
int fifoCapture() {
  int bad = 0;
  uint32_t t0 = millis();
  int got = fifoRead(capX, capY, capZ, CAPN, 4000, &bad);
  Serial.printf("CAP FIFO: %d samples in %lu ms (target 1229 ms), foreign tags: %d\n",
                got, (unsigned long)(millis() - t0), bad);
  return got;
}
void captureAndUpload() {
  doCapture = false;
  if (!capX) { Serial.println("CAP: no PSRAM buffers"); return; }
  Serial.println("CAP: recording 32768 samples via FIFO...");
  esp_task_wdt_reset();
  int got = fifoCapture();
  if (got < CAPN) {
    Serial.println("CAP: FIFO incomplete, falling back to polling");
    for (int i = got; i < CAPN; i++)
      if (!readSample(&capX[i], &capY[i], &capZ[i])) { capX[i] = capY[i] = capZ[i] = 0; }
  }
  esp_task_wdt_reset();
  Serial.println("CAP: recorded, connecting to WiFi...");
  if (wifiSsid.length() == 0) { Serial.println("CAP: WiFi NOT CONFIGURED (USB: WIFI <ssid> <password>)"); return; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(200); esp_task_wdt_reset(); }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("CAP: WiFi failed");
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
    return;
  }
  Serial.print("CAP: WiFi OK, IP "); Serial.println(WiFi.localIP());
  // Collector address: 1) URL from NVS  2) mDNS <DASH_HOST>.local  3) default placeholder
  String base = dashUrl;
  if (base.length() == 0) {
    base = DASH_URL_DEFAULT;
    if (MDNS.begin("vibnode")) {
      char host[16]; strncpy(host, DASH_HOST, sizeof(host) - 1); host[sizeof(host) - 1] = 0;
      IPAddress ip = MDNS.queryHost(host, 2000);
      MDNS.end();
      if ((uint32_t)ip != 0) {
        base = "http://" + ip.toString() + ":8080/api/raw";
        Serial.print("CAP: mDNS "); Serial.print(DASH_HOST); Serial.print(".local -> "); Serial.println(ip);
      } else Serial.println("CAP: mDNS lookup failed, using the default address");
    }
  }
  esp_task_wdt_reset();
  HTTPClient http;
  String url = base + "?node=" + nodeId + "&n=" + String(CAPN) + "&sr=26667";
  http.begin(url);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/octet-stream");
  // one POST: X block + Y block + Z block (int16 little-endian)
  static uint8_t *all = NULL;
  if (!all) all = (uint8_t *)ps_malloc(CAPN * 6);
  memcpy(all, capX, CAPN * 2);
  memcpy(all + CAPN * 2, capY, CAPN * 2);
  memcpy(all + CAPN * 4, capZ, CAPN * 2);
  int code = http.POST(all, CAPN * 6);
  Serial.print("CAP: POST "); Serial.print(url); Serial.print(" -> "); Serial.println(code);
  http.end();
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  esp_task_wdt_reset();
}

// ---------------------------------------------------------------- radio
bool radioInit() {
  int st = radio.begin(869.525, 125.0, 7, 7, 0x12, 10, 8, 1.8, false);
  if (st != RADIOLIB_ERR_NONE) {
    st = radio.begin(869.525, 125.0, 7, 7, 0x12, 10, 8, 0, false);
    Serial.print("radio retry: "); Serial.println(st);
  }
  Serial.print("radio.begin: "); Serial.println(st);
  if (st != RADIOLIB_ERR_NONE) return false;
  radio.setDio2AsRfSwitch(true);
  radio.setPacketReceivedAction(onRx);
  return true;
}
void listenDownlink(uint32_t ms) {
  rxFlag = false;
  radio.startReceive();
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (rxFlag) {
      rxFlag = false;
      String s;
      if (radio.readData(s) == RADIOLIB_ERR_NONE) {
        Serial.print("DL: "); Serial.println(s);
        char wantCfg[24], wantCap[16];
        snprintf(wantCfg, sizeof(wantCfg), "CFG id=%s int=", nodeId);
        snprintf(wantCap, sizeof(wantCap), "CAP id=%s", nodeId);
        int p = s.indexOf(wantCfg);
        if (p >= 0) {
          long v = s.substring(p + strlen(wantCfg)).toInt();
          if (v >= (long)MIN_INT && v <= 86400) {
            intervalS = v;
            prefs.putUInt("int", intervalS);
            Serial.print("New interval: "); Serial.println(intervalS);
          } else { Serial.print("Interval rejected (minimum "); Serial.print(MIN_INT); Serial.println(" s)"); }
        } else if (s.indexOf(wantCap) >= 0) {
          doCapture = true;
          Serial.println("CAP requested (downlink)");
        }
      }
      radio.startReceive();
    }
    delay(5);
  }
  radio.standby();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== OPEN VIBRATION NODE — bench node v6 (FIFO metrics, ISO 10-1000 Hz, WDT, mDNS) ===");
  Serial.print("Reset reason: "); Serial.println((int)esp_reset_reason());

  uint64_t mac = ESP.getEfuseMac();
  snprintf(nodeId, sizeof(nodeId), "%02x%02x", (uint8_t)(mac >> 8), (uint8_t)mac);
  prefs.begin("vib", false);
  intervalS = prefs.getUInt("int", 5);
  if (intervalS < MIN_INT) intervalS = MIN_INT;
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  dashUrl = prefs.getString("url", "");
  Serial.print("Node ID: "); Serial.print(nodeId);
  Serial.print("  int="); Serial.print(intervalS);
  Serial.print("  WiFi="); Serial.print(wifiSsid.length() ? wifiSsid : "(none)");
  Serial.print("  URL="); Serial.println(dashUrl.length() ? dashUrl : "auto(mDNS->default)");

  capX = (int16_t *)ps_malloc(CAPN * 2);
  capY = (int16_t *)ps_malloc(CAPN * 2);
  capZ = (int16_t *)ps_malloc(CAPN * 2);
  Serial.print("PSRAM buffers: "); Serial.println(capX && capY && capZ ? "OK" : "FAILED (compile with PSRAM=opi)");

  pinMode(S_CS, OUTPUT); digitalWrite(S_CS, HIGH);
  sensorSPI.begin(S_SCK, S_MISO, S_MOSI, S_CS);
  sensorInit();
  Serial.print("WHO_AM_I: 0x"); Serial.print(whoAmI, HEX);
  Serial.println(sensorOk ? " OK" : " FAULT (expected 0x7B) — packets will carry e=1");

  pinMode(R_ANTSW, OUTPUT); digitalWrite(R_ANTSW, HIGH);
  SPI.begin(R_SCK, R_MISO, R_MOSI, R_NSS);
  if (!radioInit()) { Serial.println("RADIO FAIL — restarting in 10 s"); delay(10000); ESP.restart(); }

  esp_task_wdt_init(WDT_S, true);
  esp_task_wdt_add(NULL);
  Serial.printf("WDT %lu s enabled. Minimum cadence %lu s.\n", (unsigned long)WDT_S, (unsigned long)MIN_INT);
}

void loop() {
  static uint32_t n = 0;
  uint32_t cycleStart = millis();
  esp_task_wdt_reset();
  pollUsbSerial();
  if (!sensorOk) { Serial.println("Sensor: attempting re-init..."); sensorInit(); }

  uint32_t tA = millis();
  bool acqOk = acquire();
  uint32_t acqMs = millis() - tA;
  if (!acqOk) { sensorOk = false; Serial.println("Sensor: FIFO incomplete — e=1"); }

  uint32_t tP = millis();
  float ax, px, vx, ay, py, vy, az, pz, vz;
  axisMetrics(bx, &ax, &px, &vx);
  axisMetrics(by, &ay, &py, &vy);
  axisMetrics(bz, &az, &pz, &vz);
  uint32_t procMs = millis() - tP;
  float aTot = sqrt(ax * ax + ay * ay + az * az);
  float pTot = max(px, max(py, pz));
  float crest = aTot > 0.01f ? pTot / aTot : 0;
  unsigned err = sensorOk ? 0 : 1;

  char msg[120];
  snprintf(msg, sizeof(msg),
           "VIB3 id=%s n=%lu i=%lu a=%.1f,%.1f,%.1f v=%.2f,%.2f,%.2f c=%.1f t=%.1f e=%u",
           nodeId, (unsigned long)++n, (unsigned long)intervalS,
           ax, ay, az, vx, vy, vz, crest, dieTemp(), err);
  int st = radio.transmit(msg);
  if (st == RADIOLIB_ERR_NONE) txErr = 0;
  else if (++txErr == 5) { Serial.println("Radio: 5 consecutive errors — re-init"); radioInit(); }
  else if (txErr >= 20) { Serial.println("Radio: 20 errors — restarting"); delay(100); ESP.restart(); }
  Serial.print(msg);
  Serial.print(" -> "); Serial.print(st == RADIOLIB_ERR_NONE ? "OK" : String(st));
  Serial.printf("  (acq %lu ms, proc %lu ms)\n", (unsigned long)acqMs, (unsigned long)procMs);

  esp_task_wdt_reset();
  listenDownlink(1800);
  if (doCapture) captureAndUpload();
  // remainder of the cadence: USB commands and CAP served every 20 ms, watchdog fed
  uint32_t period = intervalS * 1000UL;
  while (millis() - cycleStart < period) {
    pollUsbSerial();
    esp_task_wdt_reset();
    if (doCapture) captureAndUpload();
    delay(20);
  }
}
