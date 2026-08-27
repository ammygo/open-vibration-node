// ============================================================
// IIS3DWB PILNAS TESTAS - vibracija + temperatura
// Plokste: RAK3112 (ESP32-S3) virsutine + IIS3DWB apatine (V5)
// Kojos tos pacios kaip WHO_AM_I teste: SCK=13 MISO=10 MOSI=11 CS=12
// Serial Monitor: 115200 baud
// ============================================================
#include <SPI.h>

const int SCK_PIN  = 13;
const int MISO_PIN = 10;
const int MOSI_PIN = 11;
const int CS_PIN   = 12;

// IIS3DWB registrai (DS12569)
#define REG_WHO_AM_I   0x0F   // turi grazinti 0x7B
#define REG_CTRL1_XL   0x10   // ijungimas + skale
#define REG_CTRL3_C    0x12   // BDU, IF_INC, SW_RESET
#define REG_STATUS     0x1E   // bit0 = nauji XYZ duomenys
#define REG_OUT_TEMP_L 0x20   // temperatura 16 bit
#define REG_OUTX_L_A   0x28   // X,Y,Z po 16 bit (6 baitai)

const float MG_PER_LSB = 0.244f;   // +-8g skale: 0.244 mg/LSB
const int   N_SAMPLES  = 256;      // kiek meginiu vienam matavimui

SPISettings spiCfg(2000000, MSBFIRST, SPI_MODE0);  // IIS3DWB = tik Mode 0

// ---------- zemo lygio SPI ----------
uint8_t readReg(uint8_t reg) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg | 0x80);          // MSB=1 -> skaitymas
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
  return v;
}

void writeReg(uint8_t reg, uint8_t val) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg & 0x7F);          // MSB=0 -> rasymas
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

// ---------- paleidimas ----------
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("=== IIS3DWB PILNAS TESTAS ===");

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  // 1) WHO_AM_I patikra (5 bandymai)
  uint8_t id = 0;
  for (int i = 0; i < 5; i++) {
    id = readReg(REG_WHO_AM_I);
    if (id == 0x7B) break;
    delay(100);
  }
  Serial.print("WHO_AM_I: 0x");
  Serial.println(id, HEX);
  if (id != 0x7B) {
    while (true) {
      Serial.println("KLAIDA: jutiklis neatsako (turi buti 0x7B). Patikrink laidus.");
      delay(2000);
    }
  }
  Serial.println("Jutiklis rastas: IIS3DWB (0x7B) - OK");

  // 2) Programinis resetas ir konfiguracija
  writeReg(REG_CTRL3_C, 0x01);       // SW_RESET
  delay(20);
  writeReg(REG_CTRL3_C, 0x44);       // BDU=1 (nesumaisyti baitu), IF_INC=1
  writeReg(REG_CTRL1_XL, 0x2C);      // XL_EN=1 (26.7 kHz), skale +-8g
  delay(10);
  Serial.println("Jutiklis ijungtas: 26.7 kHz, +-8g");
  Serial.println();
  Serial.println("Gulint ramiai Z turi rodyti apie +-1000 mg (gravitacija).");
  Serial.println("Pabelsk pirstu i stala - RMS ir juosta soktels.");
  Serial.println("------------------------------------------------------------");
}

// ---------- pagrindinis ciklas ----------
void loop() {
  // Surenkam N meginiu kuo greiciau (jutiklis duoda nauja megini kas 37.5 us)
  double sx = 0, sy = 0, sz = 0;         // sumos vidurkiui
  double qx = 0, qy = 0, qz = 0;         // kvadratu sumos RMS skaiciavimui
  uint8_t raw[6];

  for (int i = 0; i < N_SAMPLES; i++) {
    while ((readReg(REG_STATUS) & 0x01) == 0) { }   // laukiam naujo meginio
    readBytes(REG_OUTX_L_A, raw, 6);
    int16_t x = (int16_t)(raw[0] | (raw[1] << 8));
    int16_t y = (int16_t)(raw[2] | (raw[3] << 8));
    int16_t z = (int16_t)(raw[4] | (raw[5] << 8));
    sx += x;  sy += y;  sz += z;
    qx += (double)x * x;  qy += (double)y * y;  qz += (double)z * z;
  }

  // Vidurkiai (nuolatine dedamoji = pasvirimas/gravitacija), mg
  float mx = (sx / N_SAMPLES) * MG_PER_LSB;
  float my = (sy / N_SAMPLES) * MG_PER_LSB;
  float mz = (sz / N_SAMPLES) * MG_PER_LSB;

  // RMS aplink vidurki (kintama dedamoji = vibracija), mg
  float rx = sqrt(qx / N_SAMPLES - (sx / N_SAMPLES) * (sx / N_SAMPLES)) * MG_PER_LSB;
  float ry = sqrt(qy / N_SAMPLES - (sy / N_SAMPLES) * (sy / N_SAMPLES)) * MG_PER_LSB;
  float rz = sqrt(qz / N_SAMPLES - (sz / N_SAMPLES) * (sz / N_SAMPLES)) * MG_PER_LSB;
  float vib = sqrt(rx * rx + ry * ry + rz * rz);   // bendras vibracijos lygis

  // Temperatura: 25 C + raw/256
  uint8_t t[2];
  readBytes(REG_OUT_TEMP_L, t, 2);
  int16_t traw = (int16_t)(t[0] | (t[1] << 8));
  float tempC = 25.0f + traw / 256.0f;

  // Spausdinimas
  char line[160];
  snprintf(line, sizeof(line),
           "X:%+8.1f mg  Y:%+8.1f mg  Z:%+8.1f mg | vib RMS:%7.1f mg | T:%5.1f C | ",
           mx, my, mz, vib, tempC);
  Serial.print(line);

  // Vibracijos juosta: 1 bruksnys = 10 mg (max 50)
  int bars = (int)(vib / 10.0f);
  if (bars > 50) bars = 50;
  for (int i = 0; i < bars; i++) Serial.print('#');
  Serial.println();

  delay(250);
}
