// =====================================================================
// INVERNADERO INTELIGENTE - Programador Novato
//
// Hardware:
//   - ESP32 DevKit V1
//   - Modulo relevador FL-3FF-S-Z (5 V)     IN  -> GPIO 23
//   - Pulsador                              GPIO 4 -> pulsador -> GND
//   - Sensor de nivel XKC-Y25-V             OUT -> divisor -> GPIO 27
//   - Sensor de humedad de tierra YL-69     AO  -> GPIO 34
//
// Pestanas del proyecto:
//   INVERNADERO.ino -> pines, bomba, pulsador, setup() y loop()
//   eventos.ino     -> historial de intentos de riego sin agua y la hora
//   riego.ino       -> riego automatico y boton de la pagina
//   sensores.ino    -> sensor de agua y humedad de la tierra
//   wifi_red.ino    -> WiFi, pagina web e invernadero.local
//
// Modos:
//   1) MODO DIAGNOSTICO: prueba del relevador desde el monitor serie.
//   2) MODO PULSADOR: la bomba enciende mientras se mantiene presionado
//      el pulsador, solo si hay agua en el deposito.
// =====================================================================

// ----------------------- Pines -----------------------
#define PIN_RELEVADOR       23
#define PIN_PULSADOR         4
#define PIN_SENSOR_AGUA     27   // XKC-Y25-V (con divisor de voltaje)
#define PIN_HUMEDAD_TIERRA  34   // YL-69, salida analogica AO (ADC1)

// ----------------------- Configuracion -----------------------

// true  = modo diagnostico (prueba del relevador por monitor serie)
// false = modo normal con pulsador
#define MODO_DIAGNOSTICO false

// Nivel logico que ACTIVA el relevador.
//   true  -> el relevador se activa con LOW  (activacion por nivel bajo)
//   false -> el relevador se activa con HIGH (activacion por nivel alto)
#define RELEVADOR_ACTIVO_EN_BAJO true

// Tiempo maximo que el diagnostico deja activado el relevador.
#define TIEMPO_MAXIMO_PRUEBA_MS 3000

// Tiempo de antirrebote del pulsador
#define TIEMPO_ANTIRREBOTE_MS 30

// ----------------------- Niveles derivados -----------------------
#if RELEVADOR_ACTIVO_EN_BAJO
  #define RELEVADOR_ON  LOW
  #define RELEVADOR_OFF HIGH
#else
  #define RELEVADOR_ON  HIGH
  #define RELEVADOR_OFF LOW
#endif

// Orden que el programa envio al relevador.
// OJO: es el estado LOGICO, no confirma que la bomba gire fisicamente.
bool bombaEncendida = false;

// =====================================================================
// Funciones de la bomba
// =====================================================================

// Activa el relevador. Solo lo impide el boton "Desactivar bomba".
// Devuelve true si se envio la orden de encender.
bool activarRelevador() {
  if (!bombaHabilitada()) {
    return false;
  }
  pinMode(PIN_RELEVADOR, OUTPUT);
  digitalWrite(PIN_RELEVADOR, RELEVADOR_ON);
  bombaEncendida = true;
  return true;
}

// Para el riego automatico y el boton de la pagina:
// nunca enciende la bomba si el deposito no tiene agua.
bool encenderBomba() {
  if (!hayAgua()) {
    return false;
  }
  return activarRelevador();
}

// Para el pulsador fisico: enciende AUNQUE NO HAYA AGUA.
// Es el control manual directo; quien lo presiona decide.
bool encenderBombaPulsador() {
  return activarRelevador();
}

void apagarBomba() {
  // Primero se fija el nivel y despues se configura como salida,
  // asi el pin nunca pasa por el nivel que activa el relevador.
  digitalWrite(PIN_RELEVADOR, RELEVADOR_OFF);
  pinMode(PIN_RELEVADOR, OUTPUT);
  digitalWrite(PIN_RELEVADOR, RELEVADOR_OFF);
  bombaEncendida = false;
}

// =====================================================================
// MODO DIAGNOSTICO
// =====================================================================
// Comandos (escribirlos en el monitor serie y presionar Enter):
//   h -> GPIO 23 en HIGH (3.3 V)
//   l -> GPIO 23 en LOW  (0 V)
//   z -> GPIO 23 en alta impedancia (entrada, el pin queda "suelto")
//   p -> lee el estado del pulsador
//   a -> lee el sensor de agua
//   t -> lee el sensor de humedad de la tierra
//   ? -> muestra la ayuda
//
// IMPORTANTE: h y l regresan solos al reposo despues de
// TIEMPO_MAXIMO_PRUEBA_MS. Estos comandos NO revisan el sensor de agua:
// durante el diagnostico la bomba debe estar desconectada.
// =====================================================================

unsigned long inicioPrueba = 0;
bool pruebaActiva = false;

void mostrarAyuda() {
  Serial.println();
  Serial.println("========== DIAGNOSTICO DEL RELEVADOR ==========");
  Serial.println("Recomendado: desconecta la fuente de la bomba y");
  Serial.println("usa un multimetro en continuidad entre COM y NO.");
  Serial.println();
  Serial.println("  h -> GPIO 23 en HIGH (3.3 V)");
  Serial.println("  l -> GPIO 23 en LOW  (0 V)");
  Serial.println("  z -> GPIO 23 en alta impedancia (pin suelto)");
  Serial.println("  p -> leer el pulsador");
  Serial.println("  a -> leer el sensor de agua");
  Serial.println("  t -> leer la humedad de la tierra");
  Serial.println("  ? -> mostrar esta ayuda");
  Serial.println();
  Serial.println("En cada estado anota: LED del modulo (encendido/apagado),");
  Serial.println("si hubo clic, continuidad COM-NO y voltaje entre IN y GND.");
  Serial.println("===============================================");
}

void estadoReposoDiagnostico() {
  apagarBomba();
  pruebaActiva = false;
  Serial.print(">> Reposo: GPIO 23 en ");
  Serial.println(RELEVADOR_OFF == HIGH ? "HIGH" : "LOW");
}

void setupDiagnostico() {
  estadoReposoDiagnostico();
  mostrarAyuda();
}

void loopDiagnostico() {
  // Regresar a reposo automaticamente tras el tiempo maximo
  if (pruebaActiva && millis() - inicioPrueba >= TIEMPO_MAXIMO_PRUEBA_MS) {
    Serial.println(">> Tiempo maximo alcanzado.");
    estadoReposoDiagnostico();
  }

  if (!Serial.available()) {
    return;
  }

  char comando = Serial.read();

  switch (comando) {
    case 'h':
    case 'H':
      pinMode(PIN_RELEVADOR, OUTPUT);
      digitalWrite(PIN_RELEVADOR, HIGH);
      pruebaActiva = true;
      inicioPrueba = millis();
      Serial.println(">> GPIO 23 = HIGH (3.3 V) durante 3 s. Observa el modulo.");
      break;

    case 'l':
    case 'L':
      pinMode(PIN_RELEVADOR, OUTPUT);
      digitalWrite(PIN_RELEVADOR, LOW);
      pruebaActiva = true;
      inicioPrueba = millis();
      Serial.println(">> GPIO 23 = LOW (0 V) durante 3 s. Observa el modulo.");
      break;

    case 'z':
    case 'Z':
      pinMode(PIN_RELEVADOR, INPUT);
      pruebaActiva = false;
      Serial.println(">> GPIO 23 = ALTA IMPEDANCIA (pin suelto). Observa el modulo.");
      break;

    case 'p':
    case 'P':
      Serial.print(">> Pulsador: ");
      Serial.println(digitalRead(PIN_PULSADOR) == LOW ? "PRESIONADO" : "SUELTO");
      break;

    case 'a':
    case 'A':
      imprimirSensorAgua();
      break;

    case 't':
    case 'T':
      imprimirHumedadTierra();
      break;

    case '?':
      mostrarAyuda();
      break;

    default:
      // Se ignoran saltos de linea y otros caracteres
      break;
  }
}

// =====================================================================
// MODO PULSADOR
// =====================================================================

bool estadoEstable = false;          // estado ya filtrado del pulsador
bool ultimaLectura = false;          // ultima lectura cruda
unsigned long ultimoCambio = 0;      // momento del ultimo cambio crudo

void setupPulsador() {
  apagarBomba();
  Serial.println("Modo pulsador");
  Serial.println("BOMBA APAGADA");
}

void loopPulsador() {
  bool lectura = digitalRead(PIN_PULSADOR) == LOW;  // LOW = presionado

  // Cada vez que la lectura cruda cambia, se reinicia el temporizador
  if (lectura != ultimaLectura) {
    ultimoCambio = millis();
    ultimaLectura = lectura;
  }

  // Solo se acepta el cambio si la lectura se mantuvo estable
  if (millis() - ultimoCambio >= TIEMPO_ANTIRREBOTE_MS && lectura != estadoEstable) {
    estadoEstable = lectura;

    if (estadoEstable) {
      if (encenderBombaPulsador()) {
        Serial.println("BOMBA ENCENDIDA (orden enviada al relevador)");
        if (!hayAgua()) {
          Serial.println("CUIDADO: no hay agua en el deposito, la bomba puede trabajar en seco");
          eventoPulsadorSinAgua();
        }
      } else {
        Serial.println("BOMBA BLOQUEADA: esta desactivada desde la pagina web");
      }
    } else if (bombaEncendida && !riegoEnCurso()) {
      // Al soltar se apaga, salvo que este corriendo el riego automatico
      apagarBomba();
      Serial.println("BOMBA APAGADA (orden enviada al relevador)");
    }
  }

  // Con el pulsador la bomba sigue aunque se acabe el agua.
  // La proteccion por falta de agua del riego automatico esta en riego.ino.
}

// =====================================================================
// setup y loop
// =====================================================================

void setup() {
  // Lo primero: asegurar que el relevador quede en reposo
  apagarBomba();

  pinMode(PIN_PULSADOR, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("Invernadero inteligente iniciado");
  Serial.print("Configuracion: relevador activo en ");
  Serial.println(RELEVADOR_ACTIVO_EN_BAJO ? "LOW" : "HIGH");

  // Sensores (ver pestana sensores.ino)
  iniciarSensores();

  // Historial de intentos de riego sin agua (ver pestana eventos.ino)
  iniciarEventos();

  if (MODO_DIAGNOSTICO) {
    setupDiagnostico();
  } else {
    setupPulsador();
  }

  // Riego automatico (ver pestana riego.ino)
  iniciarRiego();

  // Red WiFi (ver pestana wifi_red.ino)
  iniciarWiFi();

  // Hora desde internet (ver pestana eventos.ino)
  iniciarHora();
}

void loop() {
  // Atender la pagina web y la reconexion WiFi
  manejarWiFi();

  // Leer sensores sin detener el programa
  actualizarSensores();

  if (MODO_DIAGNOSTICO) {
    loopDiagnostico();
  } else {
    manejarRiego();      // revision periodica y riego configurable
    loopPulsador();
  }
}
