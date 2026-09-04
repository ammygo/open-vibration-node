// Open Vibration Node — bench receiver (Seeed XIAO ESP32-S3 + Wio-SX1262 kit)
//
// Prints every received packet to USB serial as
//   RX: "<payload>"  RSSI=<x> dBm  SNR=<y> dB
// so a collector on the host can parse it. Accepts "DLQ <text>" on serial and transmits
// the text as a downlink right after the next uplink from any node (nodes listen for 1.8 s
// after each transmission). One pending text at a time; the last one wins.
// The collector uses this to send "CFG id=<id> int=<s>" (cadence) and "CAP id=<id>"
// (raw capture) to a node.
#include <RadioLib.h>

#define L_SCK 7
#define L_MISO 8
#define L_MOSI 9
#define L_NSS 41
#define L_DIO1 39
#define L_RST 42
#define L_BUSY 40

SX1262 radio = new Module(L_NSS, L_DIO1, L_RST, L_BUSY);
volatile bool rxFlag = false;
void onRx() { rxFlag = true; }

char pendText[64] = "";
char lineBuf[64];
int lineLen = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== OPEN VIBRATION NODE — bench receiver (XIAO ESP32-S3 + Wio-SX1262) ===");
  SPI.begin(L_SCK, L_MISO, L_MOSI, L_NSS);
  int st = radio.begin(869.525, 125.0, 7, 7, 0x12, 10, 8, 1.8, false);
  Serial.print("radio.begin: "); Serial.println(st);
  if (st != RADIOLIB_ERR_NONE) { Serial.println("RADIO FAIL"); while (true) delay(1000); }
  radio.setDio2AsRfSwitch(true);
  radio.setPacketReceivedAction(onRx);
  radio.startReceive();
  Serial.println("Listening on 869.525 MHz SF7...");
}

void pollSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      lineBuf[lineLen] = 0;
      if (lineLen > 4 && strncmp(lineBuf, "DLQ ", 4) == 0) {
        strncpy(pendText, lineBuf + 4, sizeof(pendText) - 1);
        pendText[sizeof(pendText) - 1] = 0;
        Serial.print("QUEUED DL: "); Serial.println(pendText);
      }
      lineLen = 0;
    } else if (lineLen < (int)sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = ch;
    }
  }
}

void loop() {
  pollSerial();
  if (rxFlag) {
    rxFlag = false;
    String s;
    int st = radio.readData(s);
    float rssi = radio.getRSSI(), snr = radio.getSNR();
    if (st == RADIOLIB_ERR_NONE) {
      Serial.print("RX: \""); Serial.print(s);
      Serial.print("\"  RSSI="); Serial.print(rssi);
      Serial.print(" dBm  SNR="); Serial.print(snr);
      Serial.println(" dB");
      // pending downlink goes out after ANY uplink (the node listens for 1.8 s)
      if (pendText[0]) {
        delay(150);  // give the node time to switch to receive
        int ts = radio.transmit(pendText);
        Serial.print("TXDL "); Serial.print(pendText);
        Serial.print(" -> "); Serial.println(ts == RADIOLIB_ERR_NONE ? "OK" : String(ts));
        if (ts == RADIOLIB_ERR_NONE) pendText[0] = 0;
        rxFlag = false;  // the TX-done interrupt must not look like a reception
      }
    }
    radio.startReceive();
  }
  delay(3);
}
