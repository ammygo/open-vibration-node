#include <SPI.h>

const int SCK_PIN  = 13;
const int MISO_PIN = 10;
const int MOSI_PIN = 11;
const int CS_PIN   = 12;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting SPI Test...");

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
}

void loop() {
  Serial.println("--- Reading WHO_AM_I ---");

  // Try Mode 3
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(CS_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x8F); // Read WHO_AM_I
  byte id3 = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();

  Serial.print("WHO_AM_I (Mode 3): 0x");
  Serial.println(id3, HEX);

  // Try Mode 0
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_PIN, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x8F); // Read WHO_AM_I
  byte id0 = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();

  Serial.print("WHO_AM_I (Mode 0): 0x");
  Serial.println(id0, HEX);

  // Check if MISO is floating (read raw state)
  pinMode(MISO_PIN, INPUT_PULLUP);
  int pullup_val = digitalRead(MISO_PIN);
  pinMode(MISO_PIN, INPUT_PULLDOWN);
  int pulldown_val = digitalRead(MISO_PIN);

  Serial.print("MISO floating check -> PullUp: ");
  Serial.print(pullup_val);
  Serial.print(" PullDown: ");
  Serial.println(pulldown_val);

  // Re-init SPI for next loop
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  delay(3000);
}
