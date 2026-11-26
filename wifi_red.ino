// =====================================================================
// RED WIFI DEL INVERNADERO
//
// El ESP32 trabaja en dos modos al mismo tiempo (WIFI_AP_STA):
//
//   1) Punto de acceso propio (AP):
//        Red:        invernadero-esp32
//        Contrasena: 12345678
//        Pagina:     http://192.168.4.1
//      Siempre esta disponible, asi se puede cambiar la red WiFi
//      aunque el ESP32 no logre conectarse.
//
//   2) Cliente de la red WiFi de la casa (STA):
//      Desde la pagina se elige la red y se escribe su contrasena.
//      Los datos se guardan en la memoria flash (Preferences) y se
//      usan automaticamente cada vez que el ESP32 arranca.
//      Ya conectado, la pagina se abre en http://invernadero.local
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ----------------------- Configuracion -----------------------
#define AP_NOMBRE   "invernadero-esp32"
#define AP_CLAVE    "12345678"
#define NOMBRE_MDNS "invernadero"           // -> invernadero.local

// Cada cuanto se reintenta la conexion si se pierde el WiFi
#define TIEMPO_REINTENTO_WIFI_MS 30000

// Si hay alguien conectado a la red propia (192.168.4.1) se reintenta
// con menos frecuencia: cada intento hace que la antena recorra los
// canales y la pagina en 192.168.4.1 deja de responder unos segundos.
#define TIEMPO_REINTENTO_CON_CLIENTES_MS 300000

// IP fija de la red propia
IPAddress ipAP(192, 168, 4, 1);
IPAddress mascaraAP(255, 255, 255, 0);

// ----------------------- Variables -----------------------
WebServer servidor(80);
Preferences preferencias;

String ssidGuardado  = "";
String claveGuardada = "";

bool estabaConectado = false;
unsigned long ultimoIntentoWiFi = 0;

// =====================================================================
// Memoria: guardar y leer la red WiFi
// =====================================================================

void leerCredenciales() {
  preferencias.begin("wifi", true);            // true = solo lectura
  ssidGuardado  = preferencias.getString("ssid", "");
  claveGuardada = preferencias.getString("clave", "");
  preferencias.end();
}

void guardarCredenciales(const String &ssid, const String &clave) {
  preferencias.begin("wifi", false);
  preferencias.putString("ssid", ssid);
  preferencias.putString("clave", clave);
  preferencias.end();
  ssidGuardado  = ssid;
  claveGuardada = clave;
}

void borrarCredenciales() {
  preferencias.begin("wifi", false);
  preferencias.clear();
  preferencias.end();
  ssidGuardado  = "";
  claveGuardada = "";
}

// =====================================================================
// Conexion a la red de la casa
// =====================================================================

void conectarWiFiCasa() {
  if (ssidGuardado.length() == 0) {
    return;
  }
  Serial.print("WiFi: conectando a ");
  Serial.println(ssidGuardado);
  WiFi.begin(ssidGuardado.c_str(), claveGuardada.c_str());
  ultimoIntentoWiFi = millis();
}

// Inicia (o reinicia) el nombre invernadero.local
void iniciarMDNS() {
  MDNS.end();
  if (MDNS.begin(NOMBRE_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://" NOMBRE_MDNS ".local");
  } else {
    Serial.println("mDNS: error al iniciar");
  }
}

// =====================================================================
// Utilidades para la pagina
// =====================================================================

// Evita que un nombre de red con < > & " rompa el HTML
String escaparHTML(const String &texto) {
  String r;
  r.reserve(texto.length());
  for (unsigned int i = 0; i < texto.length(); i++) {
    char c = texto[i];
    if (c == '<')       r += "&lt;";
    else if (c == '>')  r += "&gt;";
    else if (c == '&')  r += "&amp;";
    else if (c == '"')  r += "&quot;";
    else if (c == '\'') r += "&#39;";
    else                r += c;
  }
  return r;
}

String inicioPagina(const String &titulo, const String &extraHead = "") {
  String html;
  html.reserve(3000);
  html += F("<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += extraHead;
  html += "<title>" + titulo + "</title>";
  // Tema oscuro
  html += F("<meta name='color-scheme' content='dark'>"
            "<style>"
            "body{font-family:sans-serif;background:#0f1411;margin:0;padding:16px;color:#e0ebe2}"
            ".caja{max-width:440px;margin:0 auto 16px;background:#1a221c;border-radius:12px;"
            "padding:16px;border:1px solid #2c3a2f}"
            "h1{font-size:1.4em;text-align:center;color:#66bb6a}"
            "h2{font-size:1.1em;margin-top:0;color:#a5d6a7}"
            "a{color:#81c784}"
            "label{display:block;margin:12px 0 4px;color:#b8c7ba}"
            "select,input{width:100%;padding:10px;font-size:1em;box-sizing:border-box;"
            "background:#0f1411;color:#e0ebe2;border:1px solid #3b4f3f;border-radius:8px}"
            "select:focus,input:focus{outline:2px solid #66bb6a;border-color:#66bb6a}"
            "button,.boton{display:block;width:100%;margin-top:14px;padding:12px;font-size:1em;"
            "border:0;border-radius:8px;background:#2e7d32;color:#fff;text-align:center;"
            "text-decoration:none;box-sizing:border-box;cursor:pointer}"
            ".secundario{background:#37474f}.peligro{background:#b71c1c}"
            ".ok{color:#81c784;font-weight:bold}.mal{color:#ef5350;font-weight:bold}"
            ".aviso{background:#3b1414;border:2px solid #ef5350}.aviso h2{color:#ef9a9a}"
            ".reloj{font-family:monospace;font-size:2.6em;text-align:center;color:#66bb6a;"
            "background:#0f1411;border-radius:8px;padding:8px;margin:6px 0}"
            ".centro{text-align:center}"
            ".eventos{padding-left:18px;margin:0}.eventos li{margin:8px 0;color:#e0ebe2}"
            ".eventos b{color:#ef9a9a;display:block}"
            ".fila{display:flex;gap:10px}.fila div{flex:1}"
            "small{display:block;text-align:center;color:#b8c7ba;margin-top:4px}"
            "button:disabled{opacity:.45;cursor:not-allowed}"
            "</style></head><body><h1>Invernadero inteligente</h1>");
  return html;
}

String textoEstadoWiFi() {
  switch (WiFi.status()) {
    case WL_CONNECTED:      return "<span class='ok'>Conectado</span>";
    case WL_NO_SSID_AVAIL:  return "<span class='mal'>No se encuentra la red</span>";
    case WL_CONNECT_FAILED: return "<span class='mal'>Contrasena incorrecta o error</span>";
    case WL_IDLE_STATUS:    return "Conectando...";
    default:                return "<span class='mal'>Desconectado</span>";
  }
}

// =====================================================================
// Paginas web
// =====================================================================

// Pagina principal: estado y formulario para elegir la red
void paginaInicio() {
  int numeroRedes = WiFi.scanComplete();   // -1 buscando, -2 sin busqueda

  // Mientras se buscan redes, la pagina se recarga sola cada 3 s
  String head = (numeroRedes == WIFI_SCAN_RUNNING)
                ? "<meta http-equiv='refresh' content='3'>" : "";
  String html = inicioPagina("Invernadero - WiFi", head);

  // ---- Advertencia del riego (visible en cuanto se abre la pagina) ----
  html += "<div class='caja aviso' id='aviso'";
  if (avisoRiego.length() == 0) html += " style='display:none'";
  html += "><h2>&#9888; Advertencia</h2><p id='avisoTexto'>" + escaparHTML(avisoRiego) +
          "</p><p id='avisoHora'>Hace " + String(minutosDesde(momentoAviso)) + " min</p>"
          "<form method='POST' action='/borrar-aviso'>"
          "<button class='peligro' type='submit'>Entendido</button></form></div>";

  // ---- Estado de la conexion ----
  // Se arma aparte porque cambia de lugar:
  //   - Sin conexion a la red de la casa: arriba (es lo importante).
  //   - Ya conectado (invernadero.local funciona): hasta abajo.
  bool conectadoCasa = WiFi.status() == WL_CONNECTED;

  String estado = F("<div class='caja'><h2>Estado de la conexion</h2>");
  if (ssidGuardado.length() > 0) {
    estado += "<p>Red guardada: <b>" + escaparHTML(ssidGuardado) + "</b></p>";
    estado += "<p>Estado: " + textoEstadoWiFi() + "</p>";
    if (conectadoCasa) {
      String ip = WiFi.localIP().toString();
      estado += "<p>IP en la red: <a href='http://" + ip + "'>" + ip + "</a></p>";
      estado += F("<p>Direccion: <a href='http://" NOMBRE_MDNS ".local'>http://"
                  NOMBRE_MDNS ".local</a></p>");
    }
  } else {
    estado += F("<p class='mal'>No hay red WiFi configurada.</p>");
  }
  estado += "<p>Red propia: <b>" AP_NOMBRE "</b> &rarr; <a href='http://" +
            WiFi.softAPIP().toString() + "'>http://" + WiFi.softAPIP().toString() +
            "</a></p>";
  estado += "<p>Dispositivos en la red propia: " + String(WiFi.softAPgetStationNum()) + "</p>";
  estado += F("</div>");

  if (!conectadoCasa) {
    html += estado;
  }

  // ---- Sensores (se actualizan solos cada 2 s) ----
  html += F("<div class='caja'><h2>Sensores</h2>"
            "<p>Agua en el deposito: <span id='agua'>...</span></p>"
            "<p>Humedad de la tierra: <span id='humedad'>...</span></p>"
            "<p>Orden a la bomba: <span id='bomba'>...</span></p></div>");

  // ---- Riego automatico ----
  html += "<div class='caja'><h2>Riego automatico</h2>"
          // ---- Activar / desactivar la bomba ----
          "<p>Bomba: <span id='activada'>...</span></p>"
          "<button id='btnActivar' onclick='activar()'>...</button>"
          "<p>Riega <b class='dur'>" + String(duracionRiegoSeg) +
          "</b> s cada <b class='int'>" + textoIntervalo() +
          "</b> si la humedad es menor a " +
          String(HUMEDAD_MINIMA_PORC) + " %</p>"
          "<p>Proxima revision de humedad:</p>"
          "<div class='reloj' id='reloj'>--:--</div>"
          "<p class='centro'>a las <b id='horaRev'>--:--</b></p>"
          "<p>Estado: <span id='riego'>...</span></p>"
          "<p>Ultimo riego: <span id='ultimo'>...</span></p>"
          "<button id='btnRegar' onclick='regar()'>Regar ahora (<span class='dur'>" +
          String(duracionRiegoSeg) + "</span> s)</button>"
          "<p class='centro' id='msgRiego'></p>"
          // ---- Configurar la duracion del riego ----
          "<label for='segundos'>Segundos que se activa la bomba (" +
          String(DURACION_RIEGO_MIN_SEG) + " a " + String(DURACION_RIEGO_MAX_SEG) + ")</label>"
          "<input id='segundos' type='number' inputmode='numeric' min='" +
          String(DURACION_RIEGO_MIN_SEG) + "' max='" + String(DURACION_RIEGO_MAX_SEG) +
          "' value='" + String(duracionRiegoSeg) + "'>"
          "<button class='secundario' onclick='guardarDuracion()'>Guardar duracion</button>"
          "<p class='centro' id='msgDur'></p>"
          // ---- Configurar cada cuanto se revisa la humedad ----
          "<label>Revisar la humedad cada (minimo 1 min, maximo 72 h)</label>"
          "<div class='fila'>"
          "<div><input id='horas' type='number' inputmode='numeric' min='0' max='72' value='" +
          String(intervaloRevisionMin / 60) + "'><small>horas</small></div>"
          "<div><input id='minutos' type='number' inputmode='numeric' min='0' max='59' value='" +
          String(intervaloRevisionMin % 60) + "'><small>minutos</small></div></div>"
          "<button class='secundario' onclick='guardarIntervalo()'>Guardar intervalo</button>"
          "<p class='centro' id='msgInt'></p></div>";

  html += F("<script>"
            "function $(i){return document.getElementById(i);}"
            // ---- Reloj: cuenta regresiva hasta la proxima revision ----
            // El ESP32 envia los segundos que faltan; el navegador calcula
            // la hora exacta con su propio reloj y descuenta cada segundo.
            "let objetivo=0;"
            "function dos(n){return String(n).padStart(2,'0');}"
            "function tic(){if(!objetivo)return;"
            "let s=Math.max(0,Math.round((objetivo-Date.now())/1000));"
            "let h=Math.floor(s/3600),m=Math.floor(s%3600/60),r=s%60;"
            "$('reloj').textContent=(h?dos(h)+':':'')+dos(m)+':'+dos(r);}"
            // ---- Boton de riego ----
            "function regar(){$('btnRegar').disabled=true;$('msgRiego').textContent='Enviando...';"
            "fetch('/regar',{method:'POST'}).then(r=>r.text()).then(t=>{"
            "$('msgRiego').textContent=t;actualizar();})"
            ".catch(()=>{$('msgRiego').textContent='Sin conexion con el ESP32';"
            "$('btnRegar').disabled=false;});}"
            // ---- Activar / desactivar la bomba ----
            "let bombaActiva=true;"
            "function activar(){$('btnActivar').disabled=true;"
            "fetch('/activar',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'valor='+(bombaActiva?'0':'1')}).then(()=>actualizar())"
            ".catch(()=>{}).finally(()=>{$('btnActivar').disabled=false;});}"
            // ---- Guardar cada cuanto se revisa la humedad ----
            "function guardarIntervalo(){$('msgInt').textContent='Guardando...';"
            "fetch('/intervalo',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'horas='+encodeURIComponent($('horas').value)+'&minutos='+encodeURIComponent($('minutos').value)})"
            ".then(r=>r.text().then(t=>{"
            "$('msgInt').className=r.ok?'centro ok':'centro mal';$('msgInt').textContent=t;"
            "actualizar();})).catch(()=>{$('msgInt').textContent='Sin conexion con el ESP32';});}"
            // ---- Guardar la duracion del riego ----
            "function guardarDuracion(){let s=$('segundos').value;$('msgDur').textContent='Guardando...';"
            "fetch('/duracion',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'segundos='+encodeURIComponent(s)}).then(r=>r.text().then(t=>{"
            "$('msgDur').className=r.ok?'centro ok':'centro mal';$('msgDur').textContent=t;"
            "actualizar();})).catch(()=>{$('msgDur').textContent='Sin conexion con el ESP32';});}"
            // ---- Datos de los sensores cada 2 s ----
            "function actualizar(){fetch('/estado').then(r=>r.json()).then(d=>{"
            "if(d.auto&&d.activada&&!d.regando){objetivo=Date.now()+d.proximaSeg*1000;"
            "$('horaRev').textContent=new Date(objetivo).toLocaleTimeString('es-MX',"
            "{hour:'2-digit',minute:'2-digit'});tic();}"
            "else{objetivo=0;$('reloj').textContent=d.regando?'REGANDO':"
            "!d.activada?'DESACTIVADA':'--:--';"
            "$('horaRev').textContent='--:--';}"
            "$('btnRegar').disabled=!d.auto||!d.activada||d.regando;"
            "bombaActiva=d.activada;"
            "$('activada').innerHTML=d.activada?\"<span class='ok'>ACTIVADA</span>\":"
            "\"<span class='mal'>DESACTIVADA (no se enciende por ningun medio)</span>\";"
            "$('btnActivar').textContent=d.activada?'Desactivar bomba':'Activar bomba';"
            "$('btnActivar').className=d.activada?'peligro':'';"
            "document.querySelectorAll('.dur').forEach(e=>e.textContent=d.duracion);"
            "document.querySelectorAll('.int').forEach(e=>e.textContent=d.intervalo);"
            "$('agua').innerHTML=d.agua?"
            "\"<span class='ok'>Hay agua</span>\":"
            "\"<span class='mal'>Sin agua (riego automatico y boton bloqueados)</span>\";"
            "$('humedad').textContent=d.humedad+' % (ADC '+d.adc+')';"
            "$('bomba').textContent=d.bomba?'Encendida':'Apagada';"
            "$('riego').textContent=!d.auto?'Desactivado (modo diagnostico)':!d.activada?'Bomba desactivada':"
            "d.regando?'Regando, quedan '+d.quedan+' s':'En espera';"
            "$('ultimo').textContent=d.ultimo<0?'Todavia no':'Hace '+d.ultimo+' min';"
            "$('aviso').style.display=d.aviso?'block':'none';"
            "$('avisoTexto').textContent=d.aviso;"
            "$('avisoHora').textContent='Hace '+d.avisoMin+' min';"
            "}).catch(()=>{});}"
            "actualizar();setInterval(actualizar,2000);setInterval(tic,1000);"
            "</script>");

  // ---- Formulario ----
  html += F("<div class='caja'><h2>Conectar a una red WiFi</h2>");

  if (numeroRedes == WIFI_SCAN_RUNNING) {
    html += F("<p>Buscando redes...</p>");
  } else if (numeroRedes <= 0) {
    html += F("<p>No hay redes en la lista. Presiona <b>Buscar redes</b>.</p>");
  } else {
    html += F("<form method='POST' action='/guardar'>"
              "<label for='ssid'>Red WiFi</label><select id='ssid' name='ssid'>");
    for (int i = 0; i < numeroRedes; i++) {
      String nombre = WiFi.SSID(i);
      if (nombre.length() == 0) continue;           // redes ocultas

      // Omitir nombres repetidos (varios repetidores con la misma red)
      bool repetida = false;
      for (int j = 0; j < i; j++) {
        if (WiFi.SSID(j) == nombre) { repetida = true; break; }
      }
      if (repetida) continue;

      String seguro = escaparHTML(nombre);
      bool abierta = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
      html += "<option value=\"" + seguro + "\"";
      if (nombre == ssidGuardado) html += " selected";
      html += ">" + seguro + " (" + String(WiFi.RSSI(i)) + " dBm" +
              (abierta ? ", abierta" : "") + ")</option>";
    }
    html += F("</select>"
              "<label for='clave'>Contrasena</label>"
              "<input id='clave' name='clave' type='password' maxlength='63' "
              "placeholder='Dejar vacio si la red es abierta'>"
              "<button type='submit'>Guardar y conectar</button></form>");
  }

  html += F("<a class='boton secundario' href='/buscar'>Buscar redes</a>");
  if (ssidGuardado.length() > 0) {
    html += F("<form method='POST' action='/olvidar'>"
              "<button class='peligro' type='submit'>Olvidar red guardada</button></form>");
  }
  html += F("</div>");

  // Ya conectado: el estado de la conexion va hasta abajo
  if (conectadoCasa) {
    html += estado;
  }
  // ---- Historial de intentos de riego sin agua (siempre hasta abajo) ----
  html += F("<div class='caja'><h2>Intentos de riego sin agua</h2>"
            "<ul class='eventos' id='eventos'><li>Cargando...</li></ul>"
            "<button class='secundario' onclick='borrarEventos()'>Borrar historial</button></div>"
            "<script>"
            // Enviar la hora del celular/PC al ESP32 (por si no hay internet)
            "fetch('/hora',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'epoch='+Math.floor(Date.now()/1000)}).catch(()=>{});"
            "let nEventos=-1;"
            "function cargarEventos(){fetch('/eventos').then(r=>r.json()).then(l=>{"
            "let ul=$('eventos');ul.innerHTML='';"
            "if(!l.length){let li=document.createElement('li');li.textContent='Sin eventos.';ul.appendChild(li);}"
            "l.forEach(e=>{"
            // Fecha: la del ESP32 si la conoce; si no, se calcula con el tiempo transcurrido
            "let f=e.t>0?new Date(e.t*1000):(e.h>=0?new Date(Date.now()-e.h*1000):null);"
            "let li=document.createElement('li');let b=document.createElement('b');"
            "b.textContent=f?f.toLocaleString('es-MX',{dateStyle:'short',timeStyle:'short'}):'Hora desconocida';"
            "li.appendChild(b);li.appendChild(document.createTextNode(' '+e.d));ul.appendChild(li);});"
            "}).catch(()=>{});}"
            "function borrarEventos(){fetch('/borrar-eventos',{method:'POST'}).then(()=>cargarEventos());}"
            // Recargar la lista solo cuando cambia la cantidad de eventos
            "setInterval(()=>{fetch('/estado').then(r=>r.json()).then(d=>{"
            "if(d.nEventos!==nEventos){nEventos=d.nEventos;cargarEventos();}}).catch(()=>{});},5000);"
            "cargarEventos();"
            "</script>");
  html += F("</body></html>");

  servidor.send(200, "text/html", html);
}

// Inicia una busqueda de redes sin bloquear el programa
void paginaBuscar() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);                 // true = busqueda asincrona
  servidor.sendHeader("Location", "/");
  servidor.send(303);
}

// Recibe la red elegida y la contrasena
void paginaGuardar() {
  String ssid  = servidor.arg("ssid");
  String clave = servidor.arg("clave");

  // Validaciones basicas del estandar WiFi
  if (ssid.length() == 0 || ssid.length() > 32) {
    servidor.send(400, "text/plain", "Nombre de red no valido");
    return;
  }
  if (clave.length() > 0 && (clave.length() < 8 || clave.length() > 63)) {
    servidor.send(400, "text/plain", "La contrasena debe tener entre 8 y 63 caracteres");
    return;
  }

  guardarCredenciales(ssid, clave);

  String html = inicioPagina("Invernadero - Conectando");
  html += "<div class='caja'><h2>Conectando a " + escaparHTML(ssid) + "</h2>";
  html += F("<p>Espera unos segundos.</p>"
            "<p>Si la conexion funciona, abre "
            "<a href='http://" NOMBRE_MDNS ".local'>http://" NOMBRE_MDNS ".local</a> "
            "desde un dispositivo conectado a esa misma red.</p>"
            "<p>Tambien puedes volver a <a href='http://192.168.4.1'>http://192.168.4.1</a> "
            "desde la red <b>" AP_NOMBRE "</b> para revisar el estado.</p>"
            "<a class='boton' href='/'>Ver estado</a></div></body></html>");
  servidor.send(200, "text/html", html);

  delay(300);                              // dar tiempo a que salga la respuesta
  WiFi.disconnect(false);
  conectarWiFiCasa();
}

// Datos de los sensores en formato JSON para la pagina
void paginaEstado() {
  String json = "{\"agua\":";
  json += hayAgua() ? "true" : "false";
  json += ",\"humedad\":" + String(humedadPorcentaje);
  json += ",\"adc\":" + String(humedadCruda);
  json += ",\"bomba\":";
  json += bombaEncendida ? "true" : "false";
  json += ",\"auto\":";
  json += MODO_DIAGNOSTICO ? "false" : "true";
  json += ",\"nEventos\":" + String(cantidadEventos());
  json += ",\"activada\":";
  json += bombaHabilitada() ? "true" : "false";
  json += ",\"regando\":";
  json += riegoEnCurso() ? "true" : "false";
  json += ",\"quedan\":" + String(segundosRestantesRiego());
  json += ",\"duracion\":" + String(duracionRiegoSeg);
  json += ",\"intervalo\":\"" + textoIntervalo() + "\"";
  json += ",\"proximaSeg\":" + String(segundosParaRevision());
  json += ",\"ultimo\":" + String(huboRiego ? minutosDesde(momentoUltimoRiego) : -1L);
  // El texto del aviso no lleva comillas ni diagonales, se envia tal cual
  json += ",\"aviso\":\"" + avisoRiego + "\"";
  json += ",\"avisoMin\":" + String(minutosDesde(momentoAviso));
  json += "}";
  servidor.send(200, "application/json", json);
}

// Borra la red guardada
void paginaOlvidar() {
  borrarCredenciales();
  WiFi.disconnect(false);
  Serial.println("WiFi: red guardada borrada");
  servidor.sendHeader("Location", "/");
  servidor.send(303);
}

// =====================================================================
// Funciones que se llaman desde setup() y loop()
// =====================================================================

void iniciarWiFi() {
  WiFi.persistent(false);                  // las claves las guarda Preferences
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(NOMBRE_MDNS);

  // Sin reconexion automatica del sistema: la controlamos nosotros en
  // manejarWiFi() para no estorbar a quien use 192.168.4.1
  WiFi.setAutoReconnect(false);

  // Punto de acceso propio, siempre en 192.168.4.1
  WiFi.softAP(AP_NOMBRE, AP_CLAVE);
  delay(100);
  WiFi.softAPConfig(ipAP, ipAP, mascaraAP);
  Serial.print("AP: red '" AP_NOMBRE "' en http://");
  Serial.println(WiFi.softAPIP());

  // Paginas
  servidor.on("/", HTTP_GET, paginaInicio);
  servidor.on("/buscar", HTTP_GET, paginaBuscar);
  servidor.on("/guardar", HTTP_POST, paginaGuardar);
  servidor.on("/olvidar", HTTP_POST, paginaOlvidar);
  servidor.on("/estado", HTTP_GET, paginaEstado);
  servidor.on("/eventos", HTTP_GET, []() {
    servidor.send(200, "application/json", eventosJSON());
  });
  servidor.on("/borrar-eventos", HTTP_POST, []() {
    borrarEventos();
    servidor.send(200, "text/plain", "Historial borrado");
  });
  servidor.on("/hora", HTTP_POST, []() {
    ajustarHoraDesdeNavegador((uint32_t)strtoul(servidor.arg("epoch").c_str(), nullptr, 10));
    servidor.send(200, "text/plain", "ok");
  });
  servidor.on("/activar", HTTP_POST, []() {
    cambiarBombaActivada(servidor.arg("valor") == "1");
    servidor.send(200, "text/plain", bombaHabilitada() ? "Bomba activada" : "Bomba desactivada");
  });
  servidor.on("/intervalo", HTTP_POST, []() {
    int horas = servidor.arg("horas").toInt();
    int minutos = servidor.arg("minutos").toInt();
    if (cambiarIntervaloRevision(horas, minutos)) {
      servidor.send(200, "text/plain", "Guardado: revisar cada " + textoIntervalo() +
                    ". La cuenta empieza de nuevo.");
    } else {
      servidor.send(400, "text/plain",
                    "Intervalo no valido. Debe ser de 1 minuto a 72 horas "
                    "(minutos de 0 a 59).");
    }
  });
  servidor.on("/duracion", HTTP_POST, []() {
    int seg = servidor.arg("segundos").toInt();
    if (cambiarDuracionRiego(seg)) {
      servidor.send(200, "text/plain", "Guardado: " + String(seg) + " segundos.");
    } else {
      servidor.send(400, "text/plain", "Escribe un numero entre " +
                    String(DURACION_RIEGO_MIN_SEG) + " y " +
                    String(DURACION_RIEGO_MAX_SEG) + ".");
    }
  });
  servidor.on("/regar", HTTP_POST, []() {
    servidor.send(200, "text/plain", regarAhora());
  });
  servidor.on("/borrar-aviso", HTTP_POST, []() {
    borrarAviso();
    servidor.sendHeader("Location", "/");
    servidor.send(303);
  });
  servidor.onNotFound([]() {
    servidor.sendHeader("Location", "/");
    servidor.send(303);
  });
  servidor.begin();

  iniciarMDNS();

  // Conectarse a la red guardada o, si no hay, buscar redes
  leerCredenciales();
  if (ssidGuardado.length() > 0) {
    conectarWiFiCasa();
  } else {
    Serial.println("WiFi: no hay red guardada. Configurala desde " AP_NOMBRE);
    WiFi.scanNetworks(true);
  }
}

void manejarWiFi() {
  servidor.handleClient();

  bool conectado = WiFi.status() == WL_CONNECTED;

  // Avisar cuando cambia el estado de la conexion
  if (conectado && !estabaConectado) {
    Serial.print("WiFi: conectado. IP ");
    Serial.println(WiFi.localIP());
    iniciarMDNS();                         // registrar invernadero.local en la red
  } else if (!conectado && estabaConectado) {
    Serial.println("WiFi: conexion perdida");
  }
  estabaConectado = conectado;

  // Reintentar periodicamente si hay red guardada y no hay conexion.
  // No se reintenta mientras se buscan redes para no interrumpir la busqueda.
  unsigned long espera = (WiFi.softAPgetStationNum() > 0)
                         ? TIEMPO_REINTENTO_CON_CLIENTES_MS
                         : TIEMPO_REINTENTO_WIFI_MS;

  if (!conectado && ssidGuardado.length() > 0 &&
      WiFi.scanComplete() != WIFI_SCAN_RUNNING &&
      millis() - ultimoIntentoWiFi >= espera) {
    WiFi.disconnect(false);
    conectarWiFiCasa();
  }
}
