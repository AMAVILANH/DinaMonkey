/*
 * DINAMÓMETRO PRO - CON ZONA MUERTA Y RE-TARA
 */
#include <Arduino.h>

const int PIN_DOUT = 16;
const int PIN_SCLK = 17;

// --- AJUSTES ---
float FACTOR_CALIBRACION = 1.0; 
const float ALPHA = 0.1; 

// ZONA MUERTA: Cualquier valor entre -4000 y +4000 se mostrará como 0
// Auméntalo si sigues viendo "fantasmas" cuando no tocas nada.
long ZONA_MUERTA = 4000; 

long OFFSET_TARA = 0;
float lecturaFiltrada = 0;

// Prototipo
long readADS1232_raw();
void realizarTara();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PIN_DOUT, INPUT);
  pinMode(PIN_SCLK, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);
  
  Serial.println("\n--- SISTEMA LISTO ---");
  Serial.println("Calentando (espera un poco)...");
  delay(2000); // Espera breve
  
  realizarTara(); // Tara inicial
}

void loop() {
  // 1. REVISAR SI EL USUARIO QUIERE RE-TARAR
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') {
      realizarTara();
    }
  }

  // 2. LEER SENSOR
  long raw = readADS1232_raw();

  if (raw != -999) {
    long valorNeto = raw - OFFSET_TARA;

    // 3. FILTRO SUAVIZADO
    lecturaFiltrada = (ALPHA * valorNeto) + ((1.0 - ALPHA) * lecturaFiltrada);

    // 4. APLICAR ZONA MUERTA (El truco para ver 0.0)
    float fuerzaAmostrar = lecturaFiltrada;
    
    // Si el valor está dentro del ruido (-4000 a 4000), forzamos a 0
    if (abs(fuerzaAmostrar) < ZONA_MUERTA) {
      fuerzaAmostrar = 0.0;
    }

    // 5. CONVERTIR A UNIDADES
    float fuerzaFinal = fuerzaAmostrar / FACTOR_CALIBRACION;

    Serial.print("Raw: ");
    Serial.print(valorNeto);
    Serial.print(" \t| FUERZA: ");
    Serial.println(fuerzaFinal, 1); 

  } else {
    Serial.println("Error sensor...");
  }
  
  delay(20);
}

// --- FUNCIÓN DE TARA ---
void realizarTara() {
  Serial.println("\n--- RE-CALIBRANDO CERO (TARA) ---");
  long suma = 0;
  for(int i=0; i<30; i++) {
    long val = readADS1232_raw();
    if(val != -999) suma += val;
    delay(10);
  }
  OFFSET_TARA = suma / 30;
  lecturaFiltrada = 0; // Reset filtro
  Serial.print("Nuevo Cero fijado en: ");
  Serial.println(OFFSET_TARA);
  Serial.println("---------------------------------\n");
}

// --- DRIVER ADS1232 ---
long readADS1232_raw() {
  unsigned long t = millis();
  while (digitalRead(PIN_DOUT) == HIGH) {
    if (millis() - t > 150) return -999;
  }
  long raw = 0;
  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(1);
    raw = (raw << 1) | digitalRead(PIN_DOUT);
    digitalWrite(PIN_SCLK, LOW); delayMicroseconds(1);
  }
  interrupts();
  digitalWrite(PIN_SCLK, HIGH); delayMicroseconds(1);
  digitalWrite(PIN_SCLK, LOW);
  if (raw & 0x800000) raw |= 0xFF000000;
  return raw;
}