/*
  ESP32 + ADS1232 + BLYNK
  Lectura cruda + TARE corregido + conversión a kiloNewtons (kN)
  Velocidad: 10 SPS (según configuración del ADS)
  Comandos por Serial: 't' (tare), 'u' (un-tare), 's' (status)
  
  BLYNK:
  - V0: RAW Data (long)
  - V1: Fuerza en kN (double)
  - V2: Botón de TARA (opcional)
*/

// --- 1. CREDENCIALES BLYNK (REEMPLAZA SI ES NECESARIO) ---
#define BLYNK_TEMPLATE_ID   "TMPL2Vs-zDEfC"
#define BLYNK_TEMPLATE_NAME "Dinamomo"
#define BLYNK_AUTH_TOKEN    "YfU_ahdDLPjNchPf-0xrvNuT4C9HvEg9"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "esp32-hal-cpu.h" // Para modo ahorro si lo necesitas

// --- 2. CREDENCIALES WIFI ---
char ssid[] = "KJCM";
char pass[] = "mdk12345";

// Pines (ajusta si cambias)
#define PIN_DOUT  16
#define PIN_SCLK  17

BlynkTimer timer;

// --- Parámetros de lectura / filtrado ---
const int NUM_AVG = 5;         // Bajé a 5 para no bloquear tanto tiempo el loop de Blynk
const int READ_TIMEOUT_MS = 200; // timeout esperando DRDY

// --- Constantes de calibración (TUS VALORES ORIGINALES) ---
// Counts = slope * N + intercept
const double CAL_SLOPE = 0.8689289669;    // counts por Newton (slope)
const double CAL_INTERCEPT = 30.890357;   // counts (offset)

// --- Variables runtime ---
volatile double tare_counts = 0.0; // valor de tare en "counts" (promedio)
bool have_tare = false;

// --------------------------
// PROTOTIPOS
long readADS1232();
bool readADS1232_avg(int n, long &outAvg);
void serialCommands();

// --------------------------
// FUNCIÓN PRINCIPAL DE PROCESAMIENTO
void processAndSend() {
  // leer promedio
  long avgRaw;
  bool ok = readADS1232_avg(NUM_AVG, avgRaw);

  if (!ok) {
    // Si falla el sensor, no enviamos basura
    return;
  }

  // convertir (TU LÓGICA EXACTA)
  double raw = (double) avgRaw;
  double dcounts = raw - (have_tare ? tare_counts : 0.0); 

  double N;
  if (have_tare) {
    N = (raw - tare_counts) / CAL_SLOPE;
  } else {
    N = (raw - CAL_INTERCEPT) / CAL_SLOPE;
  }
  double kN = N / 1000.0; // kiloNewtons

  // Imprimir Serial (Como tu código original)
  Serial.print("raw=");
  Serial.print((long)raw);
  Serial.print("\t dcounts=");
  Serial.print(dcounts, 2);
  Serial.print("\t kN=");
  Serial.println(kN, 3);

  // ENVIAR A BLYNK
  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, (long)raw); // Enviar Raw a V0
    Blynk.virtualWrite(V1, kN);        // Enviar kN a V1
  }
}

// --------------------------
// COMANDO DE TARA DESDE BLYNK (V2) - Opcional
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
     long avg;
     if (readADS1232_avg(NUM_AVG, avg)) {
        tare_counts = (double)avg;
        have_tare = true;
        Serial.println(">>> TARE FROM BLYNK EXECUTED <<<");
     }
  }
}

// --------------------------
void setup() {
  setCpuFrequencyMhz(80); // Mantiene estabilidad wifi
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  Serial.println();
  Serial.println("=== ADS1232 RAW + kN + BLYNK ===");
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  // Conexión no bloqueante
  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Ejecutar lógica cada 200ms
  timer.setInterval(200L, processAndSend);
  // Reintentar conexión si cae
  timer.setInterval(5000L, [](){ if(!Blynk.connected()) Blynk.connect(); });
}

void loop() {
  if (Blynk.connected()) Blynk.run();
  timer.run();
  serialCommands(); // Mantengo tus comandos seriales 't', 'u', 's'
}

// --------------------------
// TUS FUNCIONES AUXILIARES (INTACTAS)
// --------------------------

bool readADS1232_avg(int n, long &outAvg) {
  long sum = 0;
  int valid = 0;
  for (int i = 0; i < n; ++i) {
    long v = readADS1232();
    if (v == LONG_MIN) return false;
    sum += v;
    valid++;
  }
  if (valid == 0) return false;
  outAvg = sum / valid;
  return true;
}

long readADS1232() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > READ_TIMEOUT_MS) return LONG_MIN;
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
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SCLK, LOW);
  interrupts();
  if (raw & 0x800000) raw |= 0xFF000000;
  return (long) raw;
}

void serialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') {
      long avg;
      if (readADS1232_avg(NUM_AVG, avg)) {
        tare_counts = (double)avg;
        have_tare = true;
        Serial.print("TARE set to "); Serial.println((long)tare_counts);
      } else {
        Serial.println("TARE failed (timeout).");
      }
    } else if (c == 'u' || c == 'U') {
      have_tare = false;
      tare_counts = 0.0;
      Serial.println("TARE cleared.");
    } else if (c == 's' || c == 'S') {
      Serial.print("STATUS: have_tare="); Serial.print(have_tare ? "YES" : "NO");
      Serial.print("  tare_counts="); Serial.println(have_tare ? (long)tare_counts : 0);
    }
  }
}


