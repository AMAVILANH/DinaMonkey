/*
  ESP32 + ADS1232
  Lectura cruda SIN procesar
  Velocidad: 10 SPS
*/

#include <Arduino.h>

#define PIN_DOUT  16
#define PIN_SCLK  17

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  Serial.println();
  Serial.println("=== ADS1232 RAW READ ===");
  Serial.println("Esperando datos...");
}

void loop() {
  long raw = readADS1232();

  if (raw != -1) {
    Serial.println(raw);
  } else {
    Serial.println("TIMEOUT");
  }

  delay(100);   // ~10 Hz para no saturar el serial
}

long readADS1232() {
  unsigned long t0 = millis();

  // Esperar DRDY (DOUT = LOW)
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > 200) {
      return -1;   // timeout
    }
  }

  long value = 0;

  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCLK, HIGH);
    delayMicroseconds(2);

    value = (value << 1) | digitalRead(PIN_DOUT);

    digitalWrite(PIN_SCLK, LOW);
    delayMicroseconds(2);
  }
  interrupts();

  // Pulso extra recomendado por datasheet
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SCLK, LOW);

  // Sign extend (24 → 32 bits)
  if (value & 0x800000) {
    value |= 0xFF000000;
  }

  return value;
}
