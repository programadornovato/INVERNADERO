// =====================================================================
// SENSORES DEL INVERNADERO
//
// 1) Sensor de nivel XKC-Y25-V (GPIO 27)
//    Sensor capacitivo sin contacto que se pega por FUERA del deposito.
//    Detecta si hay agua a la altura donde esta colocado.
//    Se alimenta con 5 V y su salida tambien llega a 5 V, por eso
//    pasa por un divisor de voltaje (10k + 20k) antes del ESP32.
//
// 2) Sensor de humedad de tierra YL-69 con modulo comparador (GPIO 34)
//    Se usa la salida analogica AO. Se alimenta con 3.3 V para que
//    AO nunca pase de 3.3 V. Valor alto = tierra seca.
// =====================================================================

// ----------------------- Configuracion -----------------------

// Nivel que entrega el XKC-Y25-V cuando SI detecta agua.
// Depende del cable MODE del sensor; se confirma con el comando 'a'.
#define AGUA_PRESENTE_EN_ALTO true

// El nivel de agua debe mantenerse estable este tiempo para aceptarse
// (evita parpadeos cuando el agua esta justo en el limite o se mueve).
#define TIEMPO_FILTRO_AGUA_MS 500

// Calibracion del YL-69 (valores del ADC de 0 a 4095).
// Medir con el comando 't': sensor al aire = seco, en un vaso con agua = mojado.
#define TIERRA_SECA_ADC    4095
#define TIERRA_MOJADA_ADC  1500

// Cuantas lecturas se promedian y cada cuanto se mide la tierra
#define MUESTRAS_HUMEDAD      10
#define INTERVALO_HUMEDAD_MS  1000

// ----------------------- Variables -----------------------
bool aguaDisponible = false;            // estado filtrado del deposito
bool lecturaAguaPendiente = false;      // ultima lectura cruda
unsigned long cambioAgua = 0;

int humedadCruda = 0;                   // promedio del ADC (0 a 4095)
int humedadPorcentaje = 0;              // 0 % seco, 100 % mojado
unsigned long ultimaLecturaHumedad = 0;

// =====================================================================
// Sensor de agua
// =====================================================================

bool leerAguaCruda() {
  bool nivelAlto = digitalRead(PIN_SENSOR_AGUA) == HIGH;
  return AGUA_PRESENTE_EN_ALTO ? nivelAlto : !nivelAlto;
}

bool hayAgua() {
  return aguaDisponible;
}

void imprimirSensorAgua() {
  Serial.print(">> Sensor de agua: GPIO 27 = ");
  Serial.print(digitalRead(PIN_SENSOR_AGUA) == HIGH ? "HIGH" : "LOW");
  Serial.print(" -> ");
  Serial.println(aguaDisponible ? "HAY AGUA" : "SIN AGUA");
}

// =====================================================================
// Sensor de humedad de la tierra
// =====================================================================

void leerHumedadTierra() {
  long suma = 0;
  for (int i = 0; i < MUESTRAS_HUMEDAD; i++) {
    suma += analogRead(PIN_HUMEDAD_TIERRA);
  }
  humedadCruda = suma / MUESTRAS_HUMEDAD;

  // Convertir a porcentaje: seco = 0 %, mojado = 100 %
  long porcentaje = map(humedadCruda, TIERRA_SECA_ADC, TIERRA_MOJADA_ADC, 0, 100);
  humedadPorcentaje = constrain(porcentaje, 0, 100);
}

int obtenerHumedadPorcentaje() {
  return humedadPorcentaje;
}

void imprimirHumedadTierra() {
  leerHumedadTierra();
  Serial.print(">> Humedad de la tierra: ");
  Serial.print(humedadPorcentaje);
  Serial.print(" %  (ADC = ");
  Serial.print(humedadCruda);
  Serial.println(")");
}

// =====================================================================
// Funciones que se llaman desde setup() y loop()
// =====================================================================

void iniciarSensores() {
  // GPIO 27 sin resistencia interna: el divisor ya lo mantiene en LOW
  pinMode(PIN_SENSOR_AGUA, INPUT);

  // ADC de 12 bits con rango hasta ~3.3 V
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_HUMEDAD_TIERRA, ADC_11db);

  aguaDisponible = leerAguaCruda();
  lecturaAguaPendiente = aguaDisponible;
  leerHumedadTierra();

  imprimirSensorAgua();
  imprimirHumedadTierra();
}

void actualizarSensores() {
  // ---- Agua: aceptar el cambio solo si se mantiene estable ----
  bool lectura = leerAguaCruda();
  if (lectura != lecturaAguaPendiente) {
    lecturaAguaPendiente = lectura;
    cambioAgua = millis();
  }
  if (lectura != aguaDisponible && millis() - cambioAgua >= TIEMPO_FILTRO_AGUA_MS) {
    aguaDisponible = lectura;
    Serial.println(aguaDisponible ? "DEPOSITO: HAY AGUA" : "DEPOSITO: SIN AGUA");
  }

  // ---- Humedad de la tierra: una lectura por segundo ----
  if (millis() - ultimaLecturaHumedad >= INTERVALO_HUMEDAD_MS) {
    ultimaLecturaHumedad = millis();
    leerHumedadTierra();
  }
}
