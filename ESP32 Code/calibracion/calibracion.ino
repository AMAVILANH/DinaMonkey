/* Dinamometro - lectura en counts, V, uV y calibración por peso */
#include <Arduino.h>

const int PIN_DOUT = 16;
const int PIN_SCLK = 17;

const double VFS_PRE = 0.0195;    // ±19.5 mV pre-PGA (AVDD=5V, PGA=128)
const double ADC_SCALE = 8388608.0; // 2^23
const double VEXC = 5.0;          // excitación puente
const double GF = 2.0;
const double AREA = 240e-6;       // m^2 (240 mm^2)
const double E = 71e9;            // Pa
const double ALPHA = 0.08;
const long ZONA_MUERTA = 4000;

long OFFSET_TARA = 0;
double lecturaFiltrada = 0.0;

// calibration: counts por Newton (se calcula o se calibra)
double counts_per_N = 126.0; // valor inicial teórico aproximado

// prototipos
long readADS();
void doTara();
void doCalibracion();

void setup() {
  Serial.begin(115200);
  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);
  delay(500);
  Serial.println("Sistema listo. 't'=tara, 'c'=calibrar con peso conocido (kg).");
  doTara();
}

void loop() {
  // comandos por serial
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') doTara();
    if (c == 'c' || c == 'C') doCalibracion();
  }

  long raw = readADS();
  if (raw == -999) {
    Serial.println("Timeout ADS");
    delay(200);
    return;
  }

  long net = raw - OFFSET_TARA;
  lecturaFiltrada = ALPHA * net + (1.0 - ALPHA) * lecturaFiltrada;

  double displayed = lecturaFiltrada;
  if (fabs(displayed) < ZONA_MUERTA) displayed = 0.0;

  // conversiones:
  double V_pre = (displayed / ADC_SCALE) * VFS_PRE;   // voltios (pre-PGA)
  double uV_pre = V_pre * 1e6;
  double mV_pre = V_pre * 1e3;

  // Newton (usando counts_per_N calibrado)
  double fuerza_N = displayed / counts_per_N;

  Serial.print("Raw:");
  Serial.print(net);
  Serial.print("\tVpre:");
  Serial.print(uV_pre, 3);
  Serial.print(" uV");
  Serial.print("\tF:");
  Serial.print(fuerza_N, 3);
  Serial.println(" N");

  delay(100); // ~10 Hz
}

/* --- funciones --- */

long readADS() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t0 > 150) return -999;
  }
  unsigned long raw = 0;
  noInterrupts();
  for (int i=0;i<24;i++) {
    digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(1);
    raw = (raw << 1) | (digitalRead(PIN_DOUT) & 0x1);
    digitalWrite(PIN_SCLK, LOW); delayMicroseconds(1);
  }
  interrupts();
  // one extra pulse as datasheet suggests to set next cycle (safe)
  digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(1);
  digitalWrite(PIN_SCLK, LOW); delayMicroseconds(1);

  // sign extend 24->32
  if (raw & 0x800000UL) raw |= 0xFF000000UL;
  return (long)raw;
}

void doTara() {
  Serial.println("Realizando tara...");
  long sum = 0;
  int N = 30;
  for (int i=0;i<N;i++) {
    long v = readADS();
    if (v != -999) sum += v;
    delay(15);
  }
  OFFSET_TARA = sum / N;
  lecturaFiltrada = 0.0;
  Serial.print("OFFSET_TARA = "); Serial.println(OFFSET_TARA);
}

// calibración por peso: pregunta por el valor (kg)
void doCalibracion() {
  Serial.println("CALIBRACION: coloque la masa conocida y escriba su valor en kg seguido de ENTER");
  Serial.println("Ej: 1.0 <ENTER>");
  while (Serial.available() == 0) { delay(10); } // espera input
  float kg = Serial.parseFloat();
  if (kg <= 0.0) { Serial.println("Valor invalido"); return; }
  Serial.print("Leyendo con masa "); Serial.print(kg); Serial.println(" kg ...");
  long sum = 0;
  int N = 30;
  for (int i=0;i<N;i++) {
    long v = readADS();
    if (v != -999) sum += v;
    delay(15);
  }
  long raw_mass = sum / N;
  long net = raw_mass - OFFSET_TARA;
  double fuerzaN = (double)kg * 9.80665; // kg -> N
  if (fuerzaN <= 0.0) { Serial.println("Masa invalida"); return; }
  counts_per_N = (double)net / fuerzaN;
  Serial.print("Raw con masa: "); Serial.println(raw_mass);
  Serial.print("Net counts: "); Serial.println(net);
  Serial.print("Nueva const counts_per_N = "); Serial.println(counts_per_N, 6);
  Serial.println("Calibracion guardada (temporal). Re-haz tare si es necesario.");
}
