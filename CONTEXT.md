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

## Estado actual del firmware (resumen)

Archivo principal: `main.c`.

Actualmente el proyecto:

- Inicializa MCC (`SYSTEM_Initialize()`).
- Usa `NCO1` para habilitar/deshabilitar generación de pulsos (`motor_start()`/`motor_stop()`).
- Usa UART por `EUSART1` para recibir comandos simples:
  - `H`: inicia "homing" (sin sensor de fin de carrera integrado todavía).
  - `Z`: fija cero si está en `HOMING`.
  - Un número ASCII y Enter: interpreta mm, convierte a pasos con `STEPS_PER_MM`, setea `DIR` y comienza movimiento.
- Maneja estados (`IDLE`, `HOMING`, `MOVE`).

Limitación importante detectada:

- Existen `current_position` y `target_position`, pero se requiere un mecanismo confiable para **actualizar `current_position`** en función de los pasos reales (por ISR, contador de NCO, interrupciones, o un timer), especialmente si se necesita reporte 10 Hz y movimientos precisos.

## Requerimientos a implementar

1. **Mover por comandos seriales a posición deseada** (posición en mm o pasos).
2. **Configurar velocidad máxima** por serial.
3. **Configurar resolución de micro-pasos** por serial (A4988: MS1/MS2/MS3).
4. **Revisar cálculos eléctricos** del hardware (corriente, disipación, configuración del A4988) y ajustar si aplica para el motor elegido.
5. **Implementar comandos G-code** mínimos necesarios para cumplir lo anterior.
6. **Enviar la posición en tiempo real a 10 Hz** (10 veces por segundo).

## Propuesta de comandos (subconjunto de G-code)

Se planea implementar un subconjunto simple (1 eje) compatible con herramientas básicas:

- `G0 X<pos>` / `G1 X<pos>`: mover a la posición X (mm).
- `G28`: homing.
- `G92 X<pos>`: establecer posición actual.
- `M114`: reportar posición actual bajo demanda.
- `M203 S<vel>`: configurar velocidad máxima.
- `M350 S<1|2|4|8|16>`: configurar microstepping.

Telemetría periódica (cada 100 ms):

- Enviar una línea con la posición actual (formato por definir, ejemplo `X:<mm>` o estilo `M114` simplificado).

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

- `main.c`: lógica principal (firmware).
- `mcc_generated_files/`: drivers generados (EUSART1, NCO1, TMR2, pin manager, etc.).
- `nema_uart.mc3`: configuración de MCC.
- `nbproject/`: metadata de MPLAB X.
- `.vscode/`: configuración del editor.

## Git (seguridad para cambios)

Este repo se creó para preservar una versión funcional y poder iterar sin perderla:

- Tag de referencia: `version-funcional-inicial` (commit inicial guardado).
- Rama de trabajo: `mejoras-motor-serial`.

Volver al estado guardado:

- `git switch --detach version-funcional-inicial`
- o `git switch main` si `main` aún apunta a esa versión.

