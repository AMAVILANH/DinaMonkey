/*
  Dinamómetro ESP32 + ADS1232
  - DOUT  -> PIN_DOUT (GPIO12 por defecto)
  - SCLK  -> PIN_SCLK (GPIO14 por defecto)
  Comandos por Serial:
   - 't' + ENTER : Tara (calcula offset)
   - 'c' + ENTER : entra en modo calibración (pausa medición). Luego escribe el peso en kg (ej: 1.5) + ENTER
   - 's' + ENTER : muestra FACTOR_CALIBRACION actual
*/

#include <Arduino.h>

// Pines (ajusta si cambias)
const int PIN_DOUT  = 16;
const int PIN_SCLK  = 17;

// Parámetros ADS / puente (valores según discusión)
const double VEXC = 5.0;         // excitación puente (V)
const double GF = 2.0;          // factor de galga
const double AREA = 240e-6;     // m^2 (240 mm^2)
const double E = 71e9;          // Pa (módulo Young)
const double VFS_PRE = 0.0195;  // ±19.5 mV pre-PGA (AVDD=5V, PGA=128) (datasheet)
const double ADC_SCALE = 8388608.0; // 2^23
const double G = 9.80665;       // gravedad

// Filtro y zona muerta
const double ALPHA = 0.08;
const long ZONA_MUERTA = 4000;

// Variables runtime
volatile long OFFSET_TARA = 0;
double lecturaFiltrada = 0.0;
bool modoCalibracion = false;

// FACTOR: counts por kg (se utiliza: kg = counts / FACTOR_CALIBRACION)
double FACTOR_CALIBRACION = 0.0;   // se calcula teóricamente en setup y puede recalibrarse

// prototipos
long readADS_raw();
long readADS_avg(int N);
void realizarTara();
void iniciarCalibracion();
void mostrarTeorico();

// --- setup ---
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);

  // Calcula factor teórico (counts por kg) para referencia
  // Vout_pre por Newton:
  double eps_per_N = 1.0 / (AREA * E);               // ε por N
  double Vout_per_N = (VEXC / 2.0) * GF * eps_per_N; // V pre-PGA por N

  double counts_per_N_teor = (ADC_SCALE / VFS_PRE) * Vout_per_N;
  double counts_per_kg_teor = counts_per_N_teor * G;

  FACTOR_CALIBRACION = counts_per_kg_teor; // inicializa con valor teórico
  lecturaFiltrada = 0.0;

  Serial.println();
  Serial.println("=== Dinamometro - Inicio ===");
  Serial.print("Pines DOUT,SCLK: "); Serial.print(PIN_DOUT); Serial.print(", "); Serial.println(PIN_SCLK);
  Serial.print("Factor teorico (counts/kg): "); Serial.println(FACTOR_CALIBRACION, 3);
  Serial.print("Factor teorico (counts/N): "); Serial.println(counts_per_N_teor, 3);
  Serial.println("Comandos: 't'=tara, 'c'=calibrar, 's'=mostrar factor");
  Serial.println("----------------------------------");
  delay(400);

  // Tara inicial
  realizarTara();
  delay(200);
}

// --- loop ---
void loop() {
  // Revisa comandos serial (no bloqueante)
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();
    if (linea.length() == 0) {
      // nada
    } else if (linea.equalsIgnoreCase("t")) {
      realizarTara();
    } else if (linea.equalsIgnoreCase("c")) {
      iniciarCalibracion();
    } else if (linea.equalsIgnoreCase("s")) {
      Serial.print("FACTOR_CALIBRACION = "); Serial.println(FACTOR_CALIBRACION, 6);
      Serial.print("Counts por N (aprox) = ");
      double counts_per_N = FACTOR_CALIBRACION / G;
      Serial.println(counts_per_N, 6);
    } else {
      // si estamos en modo calibración, este texto puede ser el peso; pero iniciarCalibracion() ya maneja lectura bloqueante
      // Aquí, ignorar cualquier otro comando.
    }
  }

  // Si estamos en modo calibración, no procesamos la medición normal
  if (modoCalibracion) {
    delay(50);
    return;
  }

  // Lectura normal
  long raw = readADS_raw();
  if (raw == -999) {
    Serial.println("ERROR: Timeout ADS");
    delay(200);
    return;
  }

  long net = raw - OFFSET_TARA;
  // filtro IIR
  lecturaFiltrada = ALPHA * (double)net + (1.0 - ALPHA) * lecturaFiltrada;

  double displayed = lecturaFiltrada;
  if (fabs(displayed) < ZONA_MUERTA) displayed = 0.0;

  // conversiones
  double V_pre = (displayed / ADC_SCALE) * VFS_PRE; // voltios pre-PGA
  double uV_pre = V_pre * 1e6;
  double mV_pre = V_pre * 1e3;

  double kg_est = 0.0;
  double N_est = 0.0;
  if (FACTOR_CALIBRACION > 0.0) {
    kg_est = displayed / FACTOR_CALIBRACION;
    N_est = kg_est * G;
  }

  // imprimir
  Serial.print("Raw: "); Serial.print(net);
  Serial.print("\tVpre: "); Serial.print(uV_pre, 3); Serial.print(" uV");
  Serial.print("\tKg: "); Serial.print(kg_est, 4);
  Serial.print("\tN: "); Serial.println(N_est, 3);

  delay(100); // ≈ 10 Hz
}

// --- funciones --- //

long readADS_raw() {
  unsigned long t0 = millis();
  // espera DRDY (DOUT LOW)
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > 150) return -999;
    delayMicroseconds(50);
  }
  unsigned long raw = 0;
  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(2);
    raw = (raw << 1) | (digitalRead(PIN_DOUT) & 0x1);
    digitalWrite(PIN_SCLK, LOW); delayMicroseconds(2);
  }
  interrupts();

  // extra pulse recommended
  digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(2);
  digitalWrite(PIN_SCLK, LOW); delayMicroseconds(2);

  // sign-extend 24->32
  if (raw & 0x800000UL) raw |= 0xFF000000UL;
  return (long)raw;
}

// read average N samples (espera DRDY cada muestra)
long readADS_avg(int N) {
  long sum = 0;
  int valid = 0;
  for (int i = 0; i < N; i++) {
    long v = readADS_raw();
    if (v != -999) {
      sum += v;
      valid++;
    } else {
      // si timeout, intenta de nuevo
      i--;
      delay(10);
    }
    delay(10);
  }
  if (valid == 0) return -999;
  return sum / valid;
}

void realizarTara() {
  Serial.println("\n--- Realizando TARA (cero) ---");
  long avg = readADS_avg(30);
  if (avg == -999) {
    Serial.println("Error leyendo ADS para tara");
    return;
  }
  OFFSET_TARA = avg;
  lecturaFiltrada = 0.0;
  Serial.print("OFFSET_TARA fijado = "); Serial.println(OFFSET_TARA);
  Serial.println("-------------------------------\n");
}

void iniciarCalibracion() {
  // Modo calibración (bloqueante): pausa medición; pide peso y calcula factor
  modoCalibracion = true;
  Serial.println("\n=== MODO CALIBRACION ===");
  Serial.println("Coloque la masa conocida y escriba su valor en kg (ej: 1.5) y ENTER");
  Serial.println("O escriba 'cancel' para salir.");

  // Espera la entrada (sin timeout)
  while (true) {
    if (Serial.available()) {
      String linea = Serial.readStringUntil('\n');
      linea.trim();
      if (linea.length() == 0) continue;
      if (linea.equalsIgnoreCase("cancel")) {
        Serial.println("Calibracion cancelada.");
        modoCalibracion = false;
        return;
      }
      // parseFloat requiere punto decimal. Force dot: replace comma with dot just in case
      linea.replace(',', '.');
      float kg = linea.toFloat();
      if (kg <= 0.0f) {
        Serial.println("Valor invalido. Escribe un numero > 0 (ej: 1.5) o 'cancel'");
        continue;
      }

      // pide que se estabilice y toma N muestras promedio
      Serial.print("Leyendo medias... esperando estabilizacion 2 s\n");
      delay(2000);
      const int N = 40;
      long avgRaw = readADS_avg(N);
      if (avgRaw == -999) {
        Serial.println("Error leyendo ADS durante calibracion.");
        modoCalibracion = false;
        return;
      }
      long net = avgRaw - OFFSET_TARA;
      if (net <= 0) {
        Serial.println("Lectura neta <= 0. Revisa la tara o la masa.");
        modoCalibracion = false;
        return;
      }

      // calcula factor counts/kg y counts/N
      double counts_per_kg = (double)net / (double)kg;
      double counts_per_N = counts_per_kg / G;

      // actualiza FACTOR
      FACTOR_CALIBRACION = counts_per_kg;

      Serial.println("\n--- CALIBRACION COMPLETADA ---");
      Serial.print("Peso (kg): "); Serial.println(kg, 4);
      Serial.print("Raw promedio: "); Serial.println(avgRaw);
      Serial.print("Net counts: "); Serial.println(net);
      Serial.print("Counts/kg = "); Serial.println(counts_per_kg, 6);
      Serial.print("Counts/N = "); Serial.println(counts_per_N, 6);
      Serial.println("-------------------------------\n");

      modoCalibracion = false;
      return;
    }
    delay(50);
  }
}
