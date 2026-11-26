// =====================================================================
// EVENTOS: INTENTOS DE RIEGO SIN AGUA
//
// Cada vez que se intenta usar la bomba y el XKC-Y25-V no detecta agua,
// se guarda un evento con la fecha y hora. Los eventos se muestran
// hasta abajo en la pagina web y se guardan en la flash (no se pierden
// al reiniciar). Se guardan los ultimos MAX_EVENTOS.
//
// La hora:
//   El ESP32 no tiene reloj con pila, asi que la hora se obtiene de:
//     1) Internet (NTP), cuando esta conectado a la red de la casa.
//     2) El celular o la PC, cuando se abre la pagina web
//        (sirve aunque solo se use la red invernadero-esp32).
//   Si un evento ocurre antes de conocer la hora, se corrige en cuanto
//   la hora se conoce (mientras el ESP32 no se reinicie).
// =====================================================================

#include <Preferences.h>
#include <time.h>
#include <sys/time.h>

// ----------------------- Configuracion -----------------------
#define MAX_EVENTOS          20
#define HORA_MINIMA_VALIDA   1700000000UL   // nov 2023: antes de esto la hora no es real

// Tipos de evento
#define EV_AUTO_SIN_AGUA      0   // revision automatica: tierra seca y sin agua
#define EV_BOTON_SIN_AGUA     1   // boton "Regar ahora" sin agua
#define EV_PULSADOR_SIN_AGUA  2   // pulsador: la bomba se encendio sin agua
#define EV_SE_ACABO_AGUA      3   // el agua se acabo durante el riego automatico

#define MS_DESCONOCIDO 0xFFFFFFFFUL   // evento de antes del ultimo reinicio

// ----------------------- Variables -----------------------
struct Evento {
  uint32_t epoch;   // fecha y hora (segundos desde 1970, UTC); 0 = desconocida
  uint32_t ms;      // millis() del momento, para corregir la hora despues
  uint8_t  tipo;
};

Evento eventos[MAX_EVENTOS];       // el mas reciente queda al final
int totalEventos = 0;

// =====================================================================
// Hora
// =====================================================================

bool horaValida() {
  return (uint32_t)time(nullptr) > HORA_MINIMA_VALIDA;
}

void iniciarHora() {
  // Hora UTC desde internet; la pagina la muestra en la hora local
  configTime(0, 0, "pool.ntp.org", "time.google.com");
}

// La pagina envia la hora del celular/PC. Solo se usa si todavia
// no se tiene una hora real (internet tiene prioridad).
void ajustarHoraDesdeNavegador(uint32_t epoch) {
  if (horaValida() || epoch < HORA_MINIMA_VALIDA) {
    return;
  }
  struct timeval ahora = { (time_t)epoch, 0 };
  settimeofday(&ahora, nullptr);
  Serial.println("HORA: ajustada con la hora del navegador");
  corregirEventos();
}

// =====================================================================
// Memoria flash
// =====================================================================

void guardarEventos() {
  Preferences memoria;
  memoria.begin("eventos", false);
  memoria.putInt("total", totalEventos);
  if (totalEventos > 0) {
    memoria.putBytes("lista", eventos, sizeof(Evento) * totalEventos);
  }
  memoria.end();
}

void iniciarEventos() {
  Preferences memoria;
  memoria.begin("eventos", true);
  totalEventos = memoria.getInt("total", 0);
  if (totalEventos < 0 || totalEventos > MAX_EVENTOS) {
    totalEventos = 0;
  }
  if (totalEventos > 0) {
    size_t esperado = sizeof(Evento) * totalEventos;
    if (memoria.getBytes("lista", eventos, esperado) != esperado) {
      totalEventos = 0;
    }
  }
  memoria.end();

  // Los millis() guardados eran del arranque anterior: ya no sirven
  for (int i = 0; i < totalEventos; i++) {
    eventos[i].ms = MS_DESCONOCIDO;
  }

  Serial.print("EVENTOS: ");
  Serial.print(totalEventos);
  Serial.println(" guardados en memoria");
}

// =====================================================================
// Registrar y consultar eventos
// =====================================================================

String textoTipoEvento(uint8_t tipo) {
  switch (tipo) {
    case EV_AUTO_SIN_AGUA:     return "Riego automatico: tierra seca pero no habia agua. No se rego.";
    case EV_BOTON_SIN_AGUA:    return "Boton Regar ahora: no habia agua. No se rego.";
    case EV_PULSADOR_SIN_AGUA: return "Pulsador: la bomba se encendio SIN agua.";
    case EV_SE_ACABO_AGUA:     return "Riego automatico: se acabo el agua durante el riego.";
    default:                   return "Evento desconocido";
  }
}

void registrarEvento(uint8_t tipo) {
  // Si ya hay MAX_EVENTOS, se borra el mas antiguo
  if (totalEventos == MAX_EVENTOS) {
    for (int i = 1; i < MAX_EVENTOS; i++) {
      eventos[i - 1] = eventos[i];
    }
    totalEventos--;
  }

  Evento &nuevo = eventos[totalEventos];
  nuevo.epoch = horaValida() ? (uint32_t)time(nullptr) : 0;
  nuevo.ms = millis();
  nuevo.tipo = tipo;
  totalEventos++;

  guardarEventos();

  Serial.print("EVENTO: ");
  Serial.println(textoTipoEvento(tipo));
}

// Pone la hora a los eventos que ocurrieron antes de conocerla
void corregirEventos() {
  if (!horaValida()) {
    return;
  }
  bool cambio = false;
  uint32_t ahora = (uint32_t)time(nullptr);
  for (int i = 0; i < totalEventos; i++) {
    if (eventos[i].epoch == 0 && eventos[i].ms != MS_DESCONOCIDO) {
      eventos[i].epoch = ahora - (millis() - eventos[i].ms) / 1000UL;
      cambio = true;
    }
  }
  if (cambio) {
    guardarEventos();
  }
}

void borrarEventos() {
  totalEventos = 0;
  guardarEventos();
  Serial.println("EVENTOS: historial borrado");
}

int cantidadEventos() {
  return totalEventos;
}

// Lista para la pagina, del mas reciente al mas antiguo:
//   t = fecha y hora (0 = desconocida)
//   h = segundos que han pasado (-1 = desconocido)
//   d = descripcion
String eventosJSON() {
  corregirEventos();
  String json = "[";
  for (int i = totalEventos - 1; i >= 0; i--) {
    long hace = (eventos[i].ms == MS_DESCONOCIDO)
                ? -1L : (long)((millis() - eventos[i].ms) / 1000UL);
    json += "{\"t\":" + String(eventos[i].epoch) +
            ",\"h\":" + String(hace) +
            ",\"d\":\"" + textoTipoEvento(eventos[i].tipo) + "\"}";
    if (i > 0) json += ",";
  }
  json += "]";
  return json;
}

// Para llamar desde la pestana principal (pulsador)
void eventoPulsadorSinAgua() {
  registrarEvento(EV_PULSADOR_SIN_AGUA);
}
