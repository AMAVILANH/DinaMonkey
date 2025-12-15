/*
  ESP32 + ADS1232
  Lectura cruda + TARE corregido + conversión a kiloNewtons (kN)
  Velocidad: 10 SPS (según configuración del ADS)
  Comandos por Serial:
    't' → hacer TARE (lee promedio actual y lo guarda como cero)
    'u' → UN-TARE (borra el tare actual)
    's' → imprimir estado (tare, slope, intercept)
*/

#include <Arduino.h>

// Pines (ajusta si cambias)
#define PIN_DOUT  16
#define PIN_SCLK  17

// --- Parámetros de lectura / filtrado ---
const int NUM_AVG = 10;        // número de muestras para promediar (reduce ruido)
const int READ_TIMEOUT_MS = 200; // timeout esperando DRDY

// --- Constantes de calibración (resultado del experimento) ---
// Counts = slope * N + intercept
const double CAL_SLOPE = 0.8689289669;    // counts por Newton (slope)
const double CAL_INTERCEPT = 30.890357;   // counts (offset)

// --- Variables runtime ---
volatile double tare_counts = 0.0; // valor de tare en "counts" (promedio)
bool have_tare = false;

// --------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  Serial.println();
  Serial.println("=== ADS1232 RAW READ with TARE & kN output (FIXED) ===");
  Serial.println("Comandos: 't' = TARE, 'u' = UN-TARE, 's' = STATUS");
  Serial.println();
}

// --------------------------
void loop() {
  // leer promedio de NUM_AVG lecturas
  long avgRaw;
  bool ok = readADS1232_avg(NUM_AVG, avgRaw);

  if (!ok) {
    Serial.println("TIMEOUT");
    // también permitimos comandos aunque haya timeout
    serialCommands();
    delay(100);
    return;
  }

  // convertir
  double raw = (double) avgRaw;
  double dcounts = raw - (have_tare ? tare_counts : 0.0); // counts relativos al tare (si existe)

  // Convertir a Newtons:
  // - si hay tare, usamos la referencia del tare: N = (raw - tare_counts) / slope
  // - si NO hay tare, usamos la calibracion global con intercept: N = (raw - intercept) / slope
  double N;
  if (have_tare) {
    N = (raw - tare_counts) / CAL_SLOPE;
  } else {
    N = (raw - CAL_INTERCEPT) / CAL_SLOPE;
  }
  double kN = N / 1000.0;                         // kiloNewtons

  // Imprimir: raw | dcounts | kN (2 decimales)
  Serial.print("raw=");
  Serial.print((long)raw);
  Serial.print("\t dcounts=");
  Serial.print(dcounts, 2);
  Serial.print("\t kN=");
  Serial.println(kN, 2);

  // chequear comandos serial
  serialCommands();

  delay(100); // ~10 Hz
}

// --------------------------
// Lee 'n' muestras y promedia. Retorna true si OK, false si timeout.
// outAvg recibe el valor promedio (long)
bool readADS1232_avg(int n, long &outAvg) {
  long sum = 0;
  int valid = 0;
  for (int i = 0; i < n; ++i) {
    long v = readADS1232();
    if (v == LONG_MIN) { // timeout sentinel
      // si timeout en cualquiera, abortar y devolver false
      return false;
    }
    sum += v;
    valid++;
  }
  if (valid == 0) return false;
  outAvg = sum / valid;
  return true;
}

// --------------------------
// Lectura de un valor 24-bit desde ADS1232
// devuelve LONG_MIN en caso de timeout
long readADS1232() {
  unsigned long t0 = millis();

  // Esperar DRDY (DOUT = LOW)
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > READ_TIMEOUT_MS) {
      return LONG_MIN;   // timeout
    }
  }

  unsigned long raw = 0;

  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCLK, HIGH);
    delayMicroseconds(2);

    raw = (raw << 1) | (digitalRead(PIN_DOUT) & 0x1);

    digitalWrite(PIN_SCLK, LOW);
    delayMicroseconds(2);
  }
  // pulso extra recomendado por datasheet
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SCLK, LOW);
  interrupts();

  // Sign extend 24->32
  if (raw & 0x800000) {
    raw |= 0xFF000000;
  }

  return (long) raw;
}

// --------------------------
// Manejo de comandos por Serial: 't' = tare, 'u' = untare, 's' = status
void serialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') {
      // hacer tare: leer promedio (usa NUM_AVG) y guardar
      long avg;
      bool ok = readADS1232_avg(NUM_AVG, avg);
      if (ok) {
        tare_counts = (double)avg;
        have_tare = true;
        Serial.print("TARE set to ");
        Serial.println((long)tare_counts);
      } else {
        Serial.println("TARE failed (timeout).");
      }
    } else if (c == 'u' || c == 'U') {
      have_tare = false;
      tare_counts = 0.0;
      Serial.println("TARE cleared.");
    } else if (c == 's' || c == 'S') {
      Serial.print("STATUS: have_tare=");
      Serial.print(have_tare ? "YES" : "NO");
      Serial.print("  tare_counts=");
      Serial.print(have_tare ? (long)tare_counts : 0);
      Serial.print("  slope=");
      Serial.print(CAL_SLOPE, 9);
      Serial.print("  intercept=");
      Serial.println(CAL_INTERCEPT, 6);
    } else {
      // ignorar otros caracteres; puedes ampliar comandos aquí
    }
  }
}
