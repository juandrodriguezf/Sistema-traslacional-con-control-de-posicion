# Contexto del Proyecto (`nema_uart.X`)

Este repositorio contiene un proyecto de firmware para un **PIC16F18426 (14 pines)** usando **MPLAB X + XC8** y archivos generados por **MCC (Microchip Code Configurator)**. El objetivo es controlar un **motor a pasos** mediante un driver **A4988**, recibiendo comandos por **UART (puerto serial)** para mover a posiciones deseadas, configurar velocidad y configurar microstepping desde el mismo puerto serial.

## Hardware (suposiciones actuales)

- Microcontrolador: `PIC16F18426`, encapsulado de 14 pines.
- Driver: `A4988`.
- `ENABLE` del A4988: conectado a **GND** (siempre habilitado).
- Señales controladas por el PIC:
  - `STEP`: generado por `NCO1OUT` (salida de NCO).
  - `DIR`: pin digital.
  - `MS1/MS2/MS3`: pines digitales para seleccionar microstepping.
- UART:
  - TX del PIC hacia PC/USB-Serial.
  - RX del PIC desde PC/USB-Serial.

Nota: `ENABLE` fijo a GND simplifica el control, pero impide implementar un comando para desenergizar el motor (ej. `M18/M84`) a menos que se recablee a un pin del PIC.

## Asignación de pines (según `mcc_generated_files/pin_manager.h` y `pin_manager.c`)

Nombres lógicos del proyecto -> puerto -> pin físico (DIP-14):

- `MS3` -> `RA2` -> pin 11
- `MS2` -> `RC0` -> pin 10
- `STEP` (`NCO1OUT`) -> `RC1` -> pin 9
- `DIR` -> `RC2` -> pin 8
- `MS1` -> `RC3` -> pin 7
- `EUSART1 TX` -> `RC4` -> pin 6
- `EUSART1 RX` -> `RC5` -> pin 5

Algunas asignaciones PPS configuradas por MCC:

- `RC1PPS = 0x18` -> `RC1` como `NCO1OUT`
- `RC4PPS = 0x0F` -> `RC4` como `EUSART1 TX`
- `RX1PPS = 0x15` -> `RC5` como `EUSART1 RX`

## Estado actual del firmware

**Último commit:** `d8aceb0` - "Gcode funcionando v1: Gcode se reconocen pero motor tiene problemas con ciertas frecuencias"

### Implementado

Archivo principal: `main.c`.

El firmware incluye:

- **Parser G-code completo** con buffer de 32 caracteres y conversión automática a mayúsculas.
- **NCO1** para generación de pulsos con velocidad configurable (`nco_set_speed()`).
- **Timer 2** configurado para interrupción cada 100ms -> **telemetría a 10Hz**.
- **Contador de posición** en ISR del NCO (`NCO1_ISR` en `nco1.c`), lectura atómica con `position_get_atomic()`.
- **Máquinas de estado** (`IDLE`, `HOMING`, `MOVE`).

### Comandos G-code soportados

| Comando | Función | Ejemplo |
|---|---|---|
| `G0 X<pos>` | Mover a posición (rápido) | `G0 X50.5` |
| `G1 X<pos>` | Mover a posición (trabajo) | `G1 X30.0` |
| `G28` | Homing (velocidad/4) | `G28` |
| `G92 X<pos>` | Establecer posición actual + detiene motor | `G92 X0` |
| `M114` | Reportar posición bajo demanda | `M114` |
| `M203 S<vel>` | Configurar velocidad (1-5000 mm/min) | `M203 S2000` |
| `M350 S<mode>` | Configurar microstepping (1,2,4,8,16) | `M350 S8` |

### Respuestas

- `ok` - comando ejecutado
- `ok F<vel>` - velocidad configurada/consultada
- `ok M:<mode>` - microstepping configurado/consultado
- `ok X:xx.xx` - posición reportada o fijada
- `error: <motivo>` - error en comando
- **Telemetría automática:** `X:xx.xx` cada 100ms

### Configuración por defecto

- Velocidad máxima: `1000` mm/min
- Microstepping: `1/8` (MS1=1, MS2=1, MS3=0)
- STEPS_PER_MM: `320`
- Rango de velocidad configurable: `1-5000` mm/min

## Limitaciones conocidas

- **Homing sin sensor de fin de carrera:** `G28` inicia movimiento pero no detecta automáticamente el fin del recorrido. Se requiere intervención manual o agregar un switch limit.
- **Problemas de frecuencias:** El motor tiene problemas con ciertas frecuencias de NCO. Se requiere revisar el cálculo del incremento del NCO y posiblemente agregar aceleración/desaceleración.
- **Sin aceleración:** El motor arranca y para a velocidad constante, lo que puede causar pérdida de pasos en ciertas velocidades.
- **Sin límite de recorrido:** No hay software limits (`$130/$131` en GRBL). El motor puede intentar mover más allá del rango físico.

## Pendientes

1. **Revisar cálculos eléctricos** del hardware (corriente, disipación, configuración del A4988) y ajustar si aplica para el motor elegido.
2. **Agregar sensor de fin de carrera** para homing automático.
3. **Implementar aceleración/desaceleración** (rampas) para evitar pérdida de pasos.
4. **Límites de recorrido** (software endstops).
5. **Comando de deshabilitar motor** (requiere recablear ENABLE a pin del PIC).

## Microstepping A4988 (referencia)

Tabla típica de A4988 (MS1 MS2 MS3):

- `000`: paso completo
- `100`: 1/2
- `010`: 1/4
- `110`: 1/8
- `111`: 1/16

En este proyecto los MS están conectados a:

- `MS1` -> `RC3`
- `MS2` -> `RC0`
- `MS3` -> `RA2`

## Carpeta y archivos relevantes

- `main.c`: lógica principal (firmware) con parser G-code.
- `GCODE_REFERENCE.md`: manual de referencia de comandos G-code.
- `mcc_generated_files/`: drivers generados (EUSART1, NCO1, TMR2, pin manager, etc.).
- `nema_uart.mc3`: configuración de MCC.
- `nbproject/`: metadata de MPLAB X.
- `.vscode/`: configuración del editor.

## Git (seguridad para cambios)

Este repo se creó para preservar una versión funcional y poder iterar sin perderla:

- Commit inicial: `version-funcional-inicial` (Timer 2 + reporte 100ms).
- Commit actual: `d8aceb0` (G-code v1).

Volver al estado guardado:

- `git switch --detach version-funcional-inicial`
- o `git switch main` si `main` aún apunta a esa versión.
- `git log --oneline` para ver historial completo.
