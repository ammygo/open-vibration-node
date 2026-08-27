// ============================================================================
//  OPEN VIBRATION NODE  -  bench acquisition demo
//
//  Streams live acceleration statistics from an IIS3DWB running at its full
//  26.667 kHz output data rate. Prints a self-check on start-up, then one row
//  per measurement block: per-axis mean (tilt / gravity), broadband RMS
//  (vibration), peak hold and die temperature.
//
//  Hardware : IIS3DWB on the sensor board, RAK3112 (ESP32-S3) as host MCU
//  Wiring   : SCK=13  MISO=10  MOSI=11  CS=12   (SPI mode 0)
//  Serial   : 115200 baud
// ============================================================================
#include <SPI.h>

const int SCK_PIN  = 13;
const int MISO_PIN = 10;
const int MOSI_PIN = 11;
const int CS_PIN   = 12;

// IIS3DWB registers (datasheet DS12569)
#define REG_WHO_AM_I   0x0F
#define REG_CTRL1_XL   0x10
#define REG_CTRL3_C    0x12
#define REG_STATUS     0x1E
#define REG_OUT_TEMP_L 0x20
#define REG_OUTX_L_A   0x28

#define WHO_AM_I_EXPECTED 0x7B

const float MG_PER_LSB = 0.244f;   // +/-8 g full scale
const int   N_SAMPLES  = 256;      // samples per reported block
const int   PEAK_HOLD_BLOCKS = 20; // peak resets roughly every 5 s

SPISettings spiCfg(8000000, MSBFIRST, SPI_MODE0);

uint8_t readReg(uint8_t reg) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg | 0x80);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
  return v;
}

void writeReg(uint8_t reg, uint8_t val) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg & 0x7F);
  SPI.transfer(val);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void readBytes(uint8_t reg, uint8_t *buf, int n) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg | 0x80);
  for (int i = 0; i < n; i++) buf[i] = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

// Collect one block; returns per-axis mean and RMS about the mean, in mg.
void acquireBlock(float *mean, float *rms) {
  double s[3] = {0, 0, 0}, q[3] = {0, 0, 0};
  uint8_t raw[6];
  for (int i = 0; i < N_SAMPLES; i++) {
    while ((readReg(REG_STATUS) & 0x01) == 0) { }
    readBytes(REG_OUTX_L_A, raw, 6);
    for (int a = 0; a < 3; a++) {
      int16_t v = (int16_t)(raw[a * 2] | (raw[a * 2 + 1] << 8));
      s[a] += v;
      q[a] += (double)v * v;
    }
  }
  for (int a = 0; a < 3; a++) {
    double m = s[a] / N_SAMPLES;
    double var = q[a] / N_SAMPLES - m * m;
    mean[a] = m * MG_PER_LSB;
    rms[a]  = sqrt(var > 0 ? var : 0) * MG_PER_LSB;
  }
}

float readTemperature() {
  uint8_t t[2];
  readBytes(REG_OUT_TEMP_L, t, 2);
  int16_t raw = (int16_t)(t[0] | (t[1] << 8));
  return 25.0f + raw / 256.0f;
}

void setup() {
  Serial.begin(115200);
  delay(2500);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  Serial.println();
  Serial.println("================================================================");
  Serial.println("  OPEN VIBRATION NODE   -   bench acquisition demo");
  Serial.println("  github.com/ammygo/open-vibration-node");
  Serial.println("================================================================");

  // --- sensor identification ---
  uint8_t id = 0;
  for (int i = 0; i < 5 && id != WHO_AM_I_EXPECTED; i++) {
    id = readReg(REG_WHO_AM_I);
    if (id != WHO_AM_I_EXPECTED) delay(100);
  }
  Serial.print("  Sensor ID (WHO_AM_I) : 0x");
  Serial.print(id, HEX);
  Serial.println(id == WHO_AM_I_EXPECTED ? "   [ OK ]   IIS3DWB detected"
                                         : "   [FAIL]   expected 0x7B");
  if (id != WHO_AM_I_EXPECTED) {
    while (true) {
      Serial.println("  Sensor not responding - check wiring and power.");
      delay(3000);
    }
  }

  // --- configure ---
  writeReg(REG_CTRL3_C, 0x01);          // software reset
  delay(20);
  writeReg(REG_CTRL3_C, 0x44);          // block data update + auto increment
  // XL_EN[2:0] = 101 (bits 7:5) is the ONLY valid ODR: 26.667 kHz.
  // FS_XL[1:0] = 11 (bits 3:2) selects +/-8 g.  -> 0xAC
  writeReg(REG_CTRL1_XL, 0xAC);
  delay(50);

  uint8_t ctrl1 = readReg(REG_CTRL1_XL);
  Serial.print("  CTRL1_XL read-back   : 0x");
  Serial.print(ctrl1, HEX);
  Serial.println(ctrl1 == 0xAC ? "   [ OK ]   26.667 kHz ODR, +/-8 g"
                               : "   [FAIL]   configuration not accepted");
  Serial.println("  Full scale           : +/-8 g   (0.244 mg/LSB)");
  Serial.println("  SPI                  : mode 0, 8 MHz");
  Serial.print  ("  Block size           : ");
  Serial.print(N_SAMPLES);
  Serial.println(" samples");

  // --- 1 g reference check (also times the polled readout rate) ---
  float mean[3], rms[3];
  uint32_t t0 = micros();
  acquireBlock(mean, rms);
  uint32_t dt = micros() - t0;
  Serial.print("  Host readout rate    : ");
  Serial.print(N_SAMPLES * 1e6f / dt / 1000.0f, 1);
  Serial.println(" kS/s polled (sensor ODR 26.667 kHz)");
  float total = sqrt(mean[0] * mean[0] + mean[1] * mean[1] + mean[2] * mean[2]);
  Serial.print("  Gravity vector       : ");
  Serial.print(total, 0);
  Serial.print(" mg");
  Serial.println((total > 900 && total < 1100) ? "   [ OK ]   within 10% of 1 g"
                                               : "   [ ?? ]   board in motion?");
  float noise = sqrt(rms[0] * rms[0] + rms[1] * rms[1] + rms[2] * rms[2]);
  Serial.print("  Noise floor at rest  : ");
  Serial.print(noise, 1);
  Serial.println(" mg RMS");

  Serial.println("----------------------------------------------------------------");
  Serial.println("  Tap the surface to see the vibration level respond.");
  Serial.println("----------------------------------------------------------------");
  Serial.println("   time     X mg      Y mg      Z mg    RMS mg   peak    T C");
  Serial.println("----------------------------------------------------------------");
}

void loop() {
  static float peak = 0;
  static int   blocks = 0;

  float mean[3], rms[3];
  acquireBlock(mean, rms);
  float vib = sqrt(rms[0] * rms[0] + rms[1] * rms[1] + rms[2] * rms[2]);

  if (++blocks >= PEAK_HOLD_BLOCKS) { peak = 0; blocks = 0; }
  if (vib > peak) peak = vib;

  char line[160];
  snprintf(line, sizeof(line),
           "  %5.1fs  %+8.1f  %+8.1f  %+8.1f  %8.1f %6.1f  %5.1f  ",
           millis() / 1000.0f, mean[0], mean[1], mean[2],
           vib, peak, readTemperature());
  Serial.print(line);

  int bars = (int)(vib / 20.0f);        // one mark per 20 mg
  if (bars > 40) bars = 40;
  for (int i = 0; i < bars; i++) Serial.print('#');
  Serial.println();

  delay(200);
}
