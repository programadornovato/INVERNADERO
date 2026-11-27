# Invernadero inteligente con ESP32

Proyecto del canal **Programador Novato**: un sistema de riego automático con ESP32 que revisa la humedad de la tierra, enciende una bomba de agua solo si hay agua en el depósito y se controla desde una página web.

## Funciones

- **Riego automático**: cada cierto tiempo (configurable en horas y minutos) lee la humedad de la tierra. Si está seca y hay agua en el depósito, enciende la bomba los segundos configurados.
- **Protección sin agua**: si el sensor XKC-Y25-V no detecta agua, la bomba no se enciende y aparece una advertencia en la página web.
- **Pulsador físico**: enciende la bomba mientras se mantiene presionado (aunque no haya agua; queda registrado como evento).
- **Página web** (tema oscuro):
  - Lectura de sensores en tiempo real.
  - Reloj con la cuenta regresiva a la próxima revisión.
  - Botón **Regar ahora**.
  - Configuración de la duración del riego y del intervalo de revisión.
  - Botón para **activar / desactivar** la bomba (se guarda aunque se reinicie).
  - Historial de **intentos de riego sin agua** con fecha y hora.
- **WiFi**:
  - Red propia `invernadero-esp32` (clave `12345678`) en `http://192.168.4.1`.
  - Desde la página se elige la red de la casa y su contraseña.
  - Ya conectado, se accede con `http://invernadero.local`.
- La configuración y los eventos se guardan en la memoria flash.

## Material

| Componente | Uso |
|---|---|
| ESP32 DevKit V1 | Controlador |
| Módulo relevador 5 V (FL-3FF-S-Z) | Enciende la bomba |
| Bomba de agua sumergible | Riego (con fuente separada) |
| Sensor XKC-Y25-V | Detecta si hay agua en el depósito |
| Sensor de humedad YL-69 | Humedad de la tierra |
| Pulsador | Encendido manual de la bomba |
| Resistencias 10 kΩ y 20 kΩ | Divisor de voltaje del XKC-Y25-V |
| Diodo y capacitor | Protección de la bomba |

## Conexiones

| Componente | Pin del componente | ESP32 |
|---|---|---|
| Relevador | IN | GPIO 23 |
| Relevador | VCC / GND | VIN / GND |
| Pulsador | Un lado / otro lado | GPIO 4 / GND |
| XKC-Y25-V | Café (VCC) | VIN (5 V) |
| XKC-Y25-V | Azul (GND) | GND |
| XKC-Y25-V | Amarillo (OUT) | 10 kΩ → GPIO 27, con 20 kΩ de GPIO 27 a GND |
| XKC-Y25-V | Negro (MODE) | Al café (HIGH = hay agua) |
| YL-69 | AO | GPIO 34 |
| YL-69 | VCC / GND | 3V3 / GND |

> **Importante:** la salida del XKC-Y25-V es de 5 V y el ESP32 solo aguanta 3.3 V. El divisor 10 kΩ / 20 kΩ baja la señal a ≈ 3.3 V. Si no tienes una de 20 kΩ, usa dos de 10 kΩ en serie.

> La bomba usa su propia fuente; su GND debe unirse al GND del ESP32.

El archivo `diagram.json` tiene el diagrama para el simulador [Wokwi](https://wokwi.com).

## Archivos

| Archivo | Contenido |
|---|---|
| `INVERNADERO.ino` | Pines, bomba, pulsador, modo diagnóstico, `setup()` y `loop()` |
| `eventos.ino` | Historial de intentos de riego sin agua y la hora (NTP / navegador) |
| `riego.ino` | Riego automático, intervalo, duración y activar / desactivar |
| `sensores.ino` | Sensor de agua y humedad de la tierra |
| `wifi_red.ino` | WiFi, página web e `invernadero.local` |
| `diagram.json` | Diagrama de Wokwi |
| `crear_bin_completo.bat` | Une todos los binarios en un solo `.bin` |

## Cómo compilar

1. Instala el **Arduino IDE** y el núcleo **ESP32** (probado con la versión 1.0.6).
2. Abre `INVERNADERO.ino` y selecciona la placa **DOIT ESP32 DEVKIT V1**.
3. Compila y sube el programa.

### Crear un solo .bin

1. En Arduino IDE usa **Sketch > Exportar binario compilado**.
2. Ejecuta `crear_bin_completo.bat`.
3. Se crea `build\esp32.esp32.esp32\INVERNADERO_completo.bin`, que se graba en la dirección **0x0**:

```
esptool.exe --chip esp32 --port COM3 --baud 921600 write_flash 0x0 INVERNADERO_completo.bin
```

## Ajustes en el código

| Constante | Archivo | Para qué sirve |
|---|---|---|
| `MODO_DIAGNOSTICO` | `INVERNADERO.ino` | `true` para probar el relevador desde el monitor serie |
| `RELEVADOR_ACTIVO_EN_BAJO` | `INVERNADERO.ino` | `true` si el relevador se activa con LOW |
| `AGUA_PRESENTE_EN_ALTO` | `sensores.ino` | Lógica del XKC-Y25-V |
| `TIERRA_SECA_ADC` / `TIERRA_MOJADA_ADC` | `sensores.ino` | Calibración del YL-69 (lectura en aire y en agua) |
| `HUMEDAD_MINIMA_PORC` | `riego.ino` | Por debajo de este porcentaje la tierra se considera seca |

## Primer uso

1. Conéctate a la red `invernadero-esp32` (clave `12345678`).
2. Abre `http://192.168.4.1`.
3. Elige tu red WiFi, escribe la contraseña y guarda.
4. Desde la misma red de tu casa entra a `http://invernadero.local`.
