/*
 * DINAMÓMETRO ESP32 + ADS1232
 * SALIDA: kN (estable)
 */

#include <Arduino.h>

const int PIN_DOUT = 16;
const int PIN_SCLK = 17;

// ====== CALIBRACIÓN ======
float offset_raw = 82000.0;
const float COUNTS_PER_N = 102.0;

// ====== FILTRO ======
const float ALPHA = 0.02;

// ====== VARIABLES ======
float fuerza_filtrada_N = 0.0;
float tara_N = 0.0;

// ====== PROTOTIPOS ======
long readADS1232_raw();
long readADS1232_avg(uint8_t n);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  Serial.println("\n--- DINAMÓMETRO kN ---");
  Serial.println(" t -> Tara");
  Serial.println("----------------------");
}

void loop() {

// ---- COMANDOS ----
if (Serial.available()) {
  char c = Serial.read();
  if (c == 't' || c == 'T') {
    tara_N += fuerza_filtrada_N;
    fuerza_filtrada_N = 0.0;
    Serial.println("[TARA] OK");
  }
}


  // ---- LECTURA ESTABLE ----
  long raw = readADS1232_avg(8);   // PROMEDIO
  if (raw == -999) return;

  // ---- FUERZA ----
  float fuerza_N = (raw - offset_raw) / COUNTS_PER_N;
  fuerza_N -= tara_N;

  // ---- FILTRO ----
  fuerza_filtrada_N += ALPHA * (fuerza_N - fuerza_filtrada_N);

  float fuerza_kN = fuerza_filtrada_N / 1000.0;

  Serial.print("RAW: ");
  Serial.print(raw);
  Serial.print(" | F: ");
  Serial.print(fuerza_kN, 2);
  Serial.println(" kN");
}

// ====== PROMEDIO ======
long readADS1232_avg(uint8_t n) {
  long suma = 0;
  for (uint8_t i = 0; i < n; i++) {
    long v = readADS1232_raw();
    if (v == -999) return -999;
    suma += v;
  }
  return suma / n;
}

// ====== DRIVER ADS1232 ======
long readADS1232_raw() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > 200) return -999;
  }

  long raw = 0;
  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCLK, HIGH);
    delayMicroseconds(1);
    raw = (raw << 1) | digitalRead(PIN_DOUT);
    digitalWrite(PIN_SCLK, LOW);
    delayMicroseconds(1);
  }
  interrupts();

  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_SCLK, LOW);

  if (raw & 0x800000) raw |= 0xFF000000;
  return raw;
}
