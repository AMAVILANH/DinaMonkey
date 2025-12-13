/*
 * DINAMÓMETRO ESP32 + ADS1232
 * UNIDAD FINAL: kN
 */

#include <Arduino.h>

const int PIN_DOUT = 16;
const int PIN_SCLK = 17;

// ====== CALIBRACIÓN REAL ======
const float OFFSET_RAW = 82000.0;        // raw a 0 kg
const float COUNTS_PER_N = 102.0;         // sensibilidad real

// ====== FILTRO ======
const float ALPHA = 0.1;

// ====== VARIABLES ======
float fuerza_filtrada_N = 0.0;
float tara_N = 0.0;
bool modoCSV = false;

// ====== PROTOTIPOS ======
long readADS1232_raw();

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  Serial.println("\n--- DINAMÓMETRO (kN) ---");
  Serial.println("Comandos:");
  Serial.println(" t  -> Tara (0 kN)");
  Serial.println(" g  -> Toggle CSV");
  Serial.println("-----------------------");
}

void loop() {

  // ---- COMANDOS ----
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') {
      tara_N = fuerza_filtrada_N;
      Serial.println("[TARA] OK");
    }
    if (c == 'g' || c == 'G') {
      modoCSV = !modoCSV;
    }
  }

  long raw = readADS1232_raw();
  if (raw == -999) return;

  // ---- FUERZA SIN FILTRAR ----
  float fuerza_N = (raw - OFFSET_RAW) / COUNTS_PER_N;
  fuerza_N -= tara_N;

  // ---- FILTRO EN FUERZA ----
  fuerza_filtrada_N += ALPHA * (fuerza_N - fuerza_filtrada_N);

  float fuerza_kN = fuerza_filtrada_N / 1000.0;

  if (modoCSV) {
    Serial.print(millis()); Serial.print(",");
    Serial.print(raw); Serial.print(",");
    Serial.println(fuerza_kN, 3);
  } else {
    Serial.print("RAW: ");
    Serial.print(raw);
    Serial.print(" | F: ");
    Serial.print(fuerza_kN, 2);
    Serial.println(" kN");
  }

  delay(50);
}

// ====== DRIVER ADS1232 ======
long readADS1232_raw() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > 150) return -999;
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
