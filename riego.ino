// =====================================================================
// RIEGO AUTOMATICO
//
// Cada cierto tiempo (configurable desde la pagina, 1 hora por defecto)
// se lee el sensor de humedad de la tierra:
//   - Si la tierra esta humeda, no se hace nada.
//   - Si esta seca y HAY agua en el deposito, la bomba riega los
//     segundos configurados desde la pagina web (10 s por defecto).
//   - Si esta seca y NO hay agua, la bomba no se enciende y se guarda
//     una advertencia que aparece en la pagina web.
//
// Si el agua se acaba durante el riego, la bomba se apaga en ese
// momento y tambien se muestra la advertencia.
//
// Solo funciona con MODO_DIAGNOSTICO en false.
// =====================================================================

#include <Preferences.h>

// ----------------------- Configuracion -----------------------
// Cada cuanto se revisa la humedad: se cambia desde la pagina (horas y
// minutos) y se guarda en la flash. Se maneja en minutos.
#define INTERVALO_DEFECTO_MIN            60     // 1 hora
#define INTERVALO_MIN_MIN                 1     // 1 minuto
#define INTERVALO_MAX_MIN              4320     // 72 horas

// Duracion del riego: se cambia desde la pagina y se guarda en la flash
#define DURACION_RIEGO_DEFECTO_SEG       10
#define DURACION_RIEGO_MIN_SEG            1
#define DURACION_RIEGO_MAX_SEG          120     // limite para no vaciar el deposito
#define HUMEDAD_MINIMA_PORC              40     // menos de esto = tierra seca

// La primera revision se hace poco despues de encender el ESP32,
// asi no hay que esperar todo el intervalo para comprobar que funciona.
#define ESPERA_PRIMERA_REVISION_MS    10000UL   // 10 segundos

// ----------------------- Variables -----------------------
// false = bomba desactivada: no se enciende por ningun medio
// (riego automatico, boton de la pagina ni pulsador). Se guarda en la flash.
bool bombaActivada = true;

int duracionRiegoSeg = DURACION_RIEGO_DEFECTO_SEG;
int intervaloRevisionMin = INTERVALO_DEFECTO_MIN;

bool riegoActivo = false;
unsigned long inicioRiego = 0;
unsigned long ultimaRevision = 0;

bool huboRiego = false;
unsigned long momentoUltimoRiego = 0;

// Advertencia para la pagina web ("" = sin advertencia)
String avisoRiego = "";
unsigned long momentoAviso = 0;

// =====================================================================

// ----------------------- Duracion del riego -----------------------

unsigned long duracionRiegoMs() {
  return (unsigned long)duracionRiegoSeg * 1000UL;
}

// Lee de la flash la duracion guardada (o usa la de por defecto)
void cargarDuracionRiego() {
  Preferences memoria;
  memoria.begin("riego", true);            // true = solo lectura
  int seg = memoria.getInt("duracion", DURACION_RIEGO_DEFECTO_SEG);
  memoria.end();

  if (seg < DURACION_RIEGO_MIN_SEG || seg > DURACION_RIEGO_MAX_SEG) {
    seg = DURACION_RIEGO_DEFECTO_SEG;
  }
  duracionRiegoSeg = seg;
}

// Cambia la duracion y la guarda. Devuelve false si esta fuera de rango.
bool cambiarDuracionRiego(int seg) {
  if (seg < DURACION_RIEGO_MIN_SEG || seg > DURACION_RIEGO_MAX_SEG) {
    return false;
  }
  duracionRiegoSeg = seg;

  Preferences memoria;
  memoria.begin("riego", false);
  memoria.putInt("duracion", seg);
  memoria.end();

  Serial.print("RIEGO: nueva duracion = ");
  Serial.print(seg);
  Serial.println(" s");
  return true;
}

// ----------------------- Activar / desactivar la bomba -----------------------

bool bombaHabilitada() {
  return bombaActivada;
}

void cargarBombaActivada() {
  Preferences memoria;
  memoria.begin("riego", true);
  bombaActivada = memoria.getBool("activada", true);
  memoria.end();
}

void cambiarBombaActivada(bool activar) {
  bombaActivada = activar;

  Preferences memoria;
  memoria.begin("riego", false);
  memoria.putBool("activada", activar);
  memoria.end();

  if (!activar) {
    // Si estaba regando, se detiene en este momento
    riegoActivo = false;
    if (bombaEncendida) {
      apagarBomba();
      Serial.println("BOMBA APAGADA (orden enviada): se desactivo desde la pagina");
    }
    Serial.println("RIEGO: bomba DESACTIVADA");
  } else {
    // La cuenta regresiva empieza de nuevo desde ahora
    ultimaRevision = millis();
    Serial.println("RIEGO: bomba ACTIVADA");
  }
}

// ----------------------- Intervalo de revision -----------------------

unsigned long intervaloRevisionMs() {
  return (unsigned long)intervaloRevisionMin * 60000UL;
}

// Texto como "1 h 30 min", "45 min" o "2 h"
String textoIntervalo() {
  int horas = intervaloRevisionMin / 60;
  int minutos = intervaloRevisionMin % 60;
  String texto = "";
  if (horas > 0) texto += String(horas) + " h";
  if (minutos > 0) {
    if (horas > 0) texto += " ";
    texto += String(minutos) + " min";
  }
  return texto;
}

// Lee de la flash el intervalo guardado (o usa el de por defecto)
void cargarIntervaloRevision() {
  Preferences memoria;
  memoria.begin("riego", true);
  int minutos = memoria.getInt("intervalo", INTERVALO_DEFECTO_MIN);
  memoria.end();

  if (minutos < INTERVALO_MIN_MIN || minutos > INTERVALO_MAX_MIN) {
    minutos = INTERVALO_DEFECTO_MIN;
  }
  intervaloRevisionMin = minutos;
}

// Cambia el intervalo, lo guarda y reinicia la cuenta regresiva.
// Devuelve false si esta fuera de rango.
bool cambiarIntervaloRevision(int horas, int minutos) {
  if (horas < 0 || minutos < 0 || minutos > 59) {
    return false;
  }
  int total = horas * 60 + minutos;
  if (total < INTERVALO_MIN_MIN || total > INTERVALO_MAX_MIN) {
    return false;
  }
  intervaloRevisionMin = total;

  Preferences memoria;
  memoria.begin("riego", false);
  memoria.putInt("intervalo", total);
  memoria.end();

  // La siguiente revision sera dentro del nuevo intervalo, contado desde ahora
  ultimaRevision = millis();

  Serial.print("RIEGO: nuevo intervalo de revision = ");
  Serial.println(textoIntervalo());
  return true;
}

// ----------------------- Estado del riego -----------------------

bool riegoEnCurso() {
  return riegoActivo;
}

void guardarAviso(const String &texto) {
  avisoRiego = texto;
  momentoAviso = millis();
  Serial.print("ADVERTENCIA: ");
  Serial.println(texto);
}

void borrarAviso() {
  avisoRiego = "";
}

// Lee la humedad y decide si hay que regar
void revisarHumedad() {
  if (!bombaActivada) {
    return;                              // desactivada: no se revisa ni se riega
  }
  leerHumedadTierra();
  int humedad = obtenerHumedadPorcentaje();

  Serial.print("RIEGO: revision de humedad = ");
  Serial.print(humedad);
  Serial.println(" %");

  if (humedad >= HUMEDAD_MINIMA_PORC) {
    Serial.println("RIEGO: la tierra esta humeda, no se riega");
    return;
  }

  if (bombaEncendida) {
    Serial.println("RIEGO: la bomba ya esta encendida con el pulsador");
    return;
  }

  if (!hayAgua()) {
    registrarEvento(EV_AUTO_SIN_AGUA);
    guardarAviso("La tierra esta seca (" + String(humedad) +
                 " %) pero no hay agua en el deposito. "
                 "El riego automatico NO se realizo. Rellena el deposito.");
    return;
  }

  if (encenderBomba()) {
    riegoActivo = true;
    inicioRiego = millis();
    Serial.print("RIEGO: tierra seca, BOMBA ENCENDIDA por ");
    Serial.print(duracionRiegoSeg);
    Serial.println(" s (orden enviada)");
  }
}

void iniciarRiego() {
  cargarBombaActivada();
  cargarDuracionRiego();
  cargarIntervaloRevision();

  if (!bombaActivada) {
    Serial.println("RIEGO: la bomba esta DESACTIVADA (guardado en memoria)");
  }

  // Truco para que la primera revision ocurra a los 10 s y no al final
  // del intervalo
  ultimaRevision = millis() - intervaloRevisionMs() + ESPERA_PRIMERA_REVISION_MS;

  Serial.print("RIEGO: revision cada ");
  Serial.print(textoIntervalo());
  Serial.print(", riego de ");
  Serial.print(duracionRiegoSeg);
  Serial.print(" s si la humedad es menor a ");
  Serial.print(HUMEDAD_MINIMA_PORC);
  Serial.println(" %");
}

void manejarRiego() {
  // ---- Bomba desactivada: no hay revisiones ni riego ----
  if (!bombaActivada) {
    return;
  }

  // ---- Revision cada intervalo configurado ----
  if (!riegoActivo && millis() - ultimaRevision >= intervaloRevisionMs()) {
    ultimaRevision = millis();
    revisarHumedad();
  }

  if (!riegoActivo) {
    return;
  }

  // ---- Riego en curso ----
  if (!hayAgua()) {
    // Si alguien mantiene presionado el pulsador, manda el pulsador
    if (!estadoEstable) {
      apagarBomba();
    }
    riegoActivo = false;
    registrarEvento(EV_SE_ACABO_AGUA);
    guardarAviso("El riego automatico se detuvo porque se acabo el agua "
                 "del deposito. Rellena el deposito.");
    return;
  }

  if (millis() - inicioRiego >= duracionRiegoMs()) {
    riegoActivo = false;
    huboRiego = true;
    momentoUltimoRiego = millis();
    borrarAviso();                       // el riego funciono

    // Si alguien mantiene presionado el pulsador, la bomba sigue
    if (!estadoEstable) {
      apagarBomba();
      Serial.println("RIEGO: terminado, BOMBA APAGADA (orden enviada)");
    } else {
      Serial.println("RIEGO: terminado, la bomba sigue por el pulsador");
    }
  }
}

// ----------------------- Datos para la pagina -----------------------

long minutosDesde(unsigned long momento) {
  return (millis() - momento) / 60000UL;
}

long segundosParaRevision() {
  unsigned long transcurrido = millis() - ultimaRevision;
  if (transcurrido >= intervaloRevisionMs()) return 0;
  return (intervaloRevisionMs() - transcurrido + 999UL) / 1000UL;
}

// ----------------------- Riego desde la pagina web -----------------------
// Riega los segundos configurados sin importar la humedad, pero con las mismas protecciones.
// Devuelve un mensaje para mostrar en la pagina.
String regarAhora() {
  if (MODO_DIAGNOSTICO) {
    return "El riego esta desactivado mientras se usa el modo diagnostico.";
  }
  if (!bombaActivada) {
    return "La bomba esta DESACTIVADA. Activala para poder regar.";
  }
  if (riegoActivo) {
    return "Ya se esta regando.";
  }
  if (bombaEncendida) {
    return "La bomba ya esta encendida con el pulsador.";
  }
  if (!hayAgua()) {
    registrarEvento(EV_BOTON_SIN_AGUA);
    guardarAviso("Se pidio regar desde la pagina web pero no hay agua en el "
                 "deposito. La bomba NO se encendio. Rellena el deposito.");
    return "No hay agua en el deposito. La bomba no se encendio.";
  }
  if (encenderBomba()) {
    riegoActivo = true;
    inicioRiego = millis();
    Serial.print("RIEGO: pedido desde la pagina web, BOMBA ENCENDIDA por ");
    Serial.print(duracionRiegoSeg);
    Serial.println(" s (orden enviada)");
    return "Riego iniciado: " + String(duracionRiegoSeg) + " segundos.";
  }
  return "No se pudo encender la bomba.";
}

long segundosRestantesRiego() {
  if (!riegoActivo) return 0;
  unsigned long transcurrido = millis() - inicioRiego;
  if (transcurrido >= duracionRiegoMs()) return 0;
  return (duracionRiegoMs() - transcurrido + 999UL) / 1000UL;
}
