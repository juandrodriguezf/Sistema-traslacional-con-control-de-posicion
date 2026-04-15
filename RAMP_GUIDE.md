# Guía de Rampas - Sistema de Control de Motor

## Resumen

Este documento explica en detalle cómo funciona el sistema de **aceleración y desaceleración (rampas)** implementado en el firmware del controlador de motor a pasos NEMA con driver A4988.

---

## Tabla de Contenidos

1. [Concepto General](#concepto-general)
2. [Estados de la Rampa](#estados-de-la-rampa)
3. [Parámetros de Configuración](#parámetros-de-configuración)
4. [Perfil Trapezoidal](#perfil-trapezoidal)
5. [Perfil Triangular](#perfil-triangular)
6. [Configuración por Microstepping](#configuración-por-microstepping)
7. [Flujo de Ejecución](#flujo-de-ejecución)
8. [Fórmulas Clave](#fórmulas-clave)
9. [Ejemplo Práctico](#ejemplo-práctico)
10. [Limitaciones y Consideraciones](#limitaciones-y-consideraciones)

---

## Concepto General

El sistema de rampas permite que el motor **arranque y frene suavemente**, evitando la pérdida de pasos que ocurre cuando un motor a pasos intenta arrancar o detenerse instantáneamente a alta velocidad.

### ¿Por qué se necesitan rampas?

Un motor a pasos tiene **inercia mecánica** y **torque limitado**. Si se le pide que arranque a alta frecuencia (ej. 10 kHz) desde cero, el motor no puede seguir los pulsos y **pierde pasos**. La rampa resuelve esto aumentando gradualmente la frecuencia de pulsos STEP.

### Arquitectura del sistema

```
Usuario envía: G0 X50.5
       ↓
execute_gcode() calcula distancia y configura rampa
       ↓
main() loop ejecuta la máquina de estados de rampa
       ↓
nco_set_freq() ajusta frecuencia en cada paso
       ↓
NCO genera pulsos STEP al motor
```

---

## Estados de la Rampa

El sistema usa una máquina de estados con 4 fases:

```
RAMP_ACCEL → RAMP_CRUISE → RAMP_DECEL → RAMP_IDLE
   ↓              ↓             ↓           ↓
 Acelerar    Velocidad      Frenar       Detenido
            constante
```

### `RAMP_IDLE`
- Motor detenido
- Estado inicial y final de cada movimiento

### `RAMP_ACCEL`
- Frecuencia aumenta gradualmente desde `start_freq` hasta `target_freq`
- Fórmula: `current_freq = start_freq + (steps_moved / accel_divisor)`

### `RAMP_CRUISE`
- Motor mantiene velocidad máxima (`target_freq`)
- Solo existe en perfiles trapezoidales (distancias largas)

### `RAMP_DECEL`
- Frecuencia disminuye gradualmente hacia `start_freq`
- Fórmula: `current_freq = start_freq + (steps_remaining / accel_divisor)`

---

## Parámetros de Configuración

### Variables globales de rampa

```c
volatile ramp_state_t ramp_state;           // Estado actual
volatile unsigned long ramp_start_pos;      // Posición donde inicia el movimiento
volatile unsigned long ramp_total_steps;    // Pasos totales a mover
volatile unsigned long ramp_accel_steps;    // Pasos durante aceleración
volatile unsigned long ramp_decel_steps;    // Pasos durante desaceleración

volatile unsigned long current_freq;        // Frecuencia actual (Hz)
volatile unsigned long target_freq;         // Frecuencia objetivo (Hz)
volatile unsigned long start_freq;          // Frecuencia de arranque (Hz)
volatile unsigned long accel_divisor;       // Controla la pendiente de la rampa
```

### Parámetros por microstepping

| Microstep | `start_freq` (Hz) | `accel_divisor` | Pendiente (Hz/paso) |
|-----------|-------------------|-----------------|---------------------|
| 1 (Full)  | 900               | 2               | 0.5                 |
| 2 (1/2)   | 1800              | 4               | 0.25                |
| 4 (1/4)   | 5000              | 8               | 0.125               |
| 8 (1/8)   | 8500              | 16              | 0.0625              |
| 16 (1/16) | 22000             | 32              | 0.03125             |

**Notas:**
- A mayor microstepping, mayor `start_freq` necesaria para vencer la inercia
- El `accel_divisor` determina qué tan suave es la rampa: divisor mayor = rampa más suave
- `target_freq` tiene un tope de **90,000 Hz**

---

## Perfil Trapezoidal

Se usa cuando la distancia es **suficientemente larga** para tener las 3 fases:

```
Condición: distancia_total > (dist_accel + dist_decel)

Frecuencia
    ↑
    |           ┌──────────────┐  ← target_freq (velocidad máxima)
    |          /                \
    |         /                  \
    |        /                    \
    |       /                      \
    |      /                        \
    |     /                          \
    |    /                            \
    └───┴─────────────────────────────┴──→ Posición
        ↑                              ↑
     start_freq                    start_freq
     (inicio)                      (detención)

    |← accel →|← cruise →|← decel →|
```

### Cálculo de pasos de aceleración

```c
ramp_accel_steps = (target_freq - start_freq) * accel_divisor;
ramp_decel_steps = ramp_accel_steps;  // Perfil simétrico
```

**Ejemplo numérico (microstepping 1/2):**
- `target_freq = 5000 Hz`
- `start_freq = 1800 Hz`
- `accel_divisor = 4`
- `ramp_accel_steps = (5000 - 1800) * 4 = 12,800 pasos`
- `ramp_decel_steps = 12,800 pasos`
- **Distancia mínima para trapecio: 25,600 pasos**

---

## Perfil Triangular

Se usa cuando la distancia es **corta** y no hay espacio para las 3 fases:

```
Condición: distancia_total <= (dist_accel + dist_decel)

Frecuencia
    ↑
    |              /\
    |             /  \       ← pico < target_freq
    |            /    \
    |           /      \
    |          /        \
    |         /          \
    |        /            \
    └───────┴──────────────┴──→ Posición
            ↑              ↑
         start_freq    start_freq
         (inicio)      (detención)

    |← accel →|← decel →|
   (sin fase cruise)
```

### Ajuste automático

```c
if (ramp_total_steps <= (ramp_accel_steps + ramp_decel_steps)) {
    ramp_accel_steps = ramp_total_steps / 2;
    ramp_decel_steps = ramp_total_steps - ramp_accel_steps;
}
```

**Ejemplo:**
- Distancia total: 5,000 pasos
- `ramp_accel_steps = 5000 / 2 = 2,500`
- `ramp_decel_steps = 5000 - 2500 = 2,500`
- El motor nunca alcanza `target_freq`, se detiene en un pico intermedio

---

## Configuración por Microstepping

### ¿Por qué cambia `start_freq` con el microstepping?

A mayor microstepping:
1. **Más pasos por mm** → cada paso es más pequeño
2. **Menos torque por paso** → el motor necesita más ayuda para arrancar
3. **Mayor frecuencia base** para lograr la misma velocidad lineal

### Relación con torque

```
Microstepping ↑  →  Torque disponible ↓  →  Rampa más suave necesaria
```

Por eso:
- **Full step (1):** `start_freq = 900 Hz`, rampa agresiva (divisor=2)
- **1/16 step:** `start_freq = 22000 Hz`, rampa muy suave (divisor=32)

### Frecuencia objetivo por velocidad

```c
target_freq = (max_speed_mm_min * steps_per_mm) / 60;
```

Si `max_speed_mm_min = 2000` y `microstepping = 2`:
- `steps_per_mm = 40 * 2 = 80`
- `target_freq = (2000 * 80) / 60 = 2,667 Hz`

---

## Flujo de Ejecución

### 1. Recepción del comando

```c
// execute_gcode() procesa G0 X50.5
target_position = 50.5 * steps_per_mm;
ramp_start_pos = position_get_atomic();
ramp_total_steps = abs(target_position - ramp_start_pos);
```

### 2. Configuración de parámetros

```c
// Calcular frecuencia objetivo
target_freq = (max_speed_mm_min * steps_per_mm) / 60;

// Seleccionar start_freq y accel_divisor según microstepping
if (current_microstepping == 1) {
    start_freq = 900;
    accel_divisor = 2;
} else if (current_microstepping == 2) {
    start_freq = 1800;
    accel_divisor = 4;
}
// ... etc

// Configurar rampa
current_freq = start_freq;
ramp_accel_steps = (target_freq - start_freq) * accel_divisor;
ramp_decel_steps = ramp_accel_steps;

// Verificar si es triangular
if (ramp_total_steps <= (ramp_accel_steps + ramp_decel_steps)) {
    ramp_accel_steps = ramp_total_steps / 2;
    ramp_decel_steps = ramp_total_steps - ramp_accel_steps;
}
```

### 3. Inicio del movimiento

```c
state = MOVE;
ramp_state = RAMP_ACCEL;
nco_set_freq(current_freq);  // Frecuencia inicial
motor_start();               // Habilitar NCO
```

### 4. Loop principal (ejecución en `main()`)

```c
while (1) {
    if (state == MOVE) {
        long current_local_pos = position_get_atomic();
        unsigned long steps_moved = abs(current_local_pos - ramp_start_pos);
        unsigned long steps_remaining = abs(target_position - current_local_pos);

        // ¿Llegó al destino?
        if (steps_remaining < 2) {
            motor_stop();
            state = IDLE;
            ramp_state = RAMP_IDLE;
            UART_SendString("ok\r\n");
        }
        // ¿En aceleración?
        else if (ramp_state == RAMP_ACCEL) {
            if (steps_moved >= ramp_accel_steps) {
                // Fin de aceleración
                current_freq = target_freq;
                nco_set_freq(current_freq);

                // ¿Era triangular?
                if (ramp_total_steps <= (ramp_accel_steps + ramp_decel_steps)) {
                    ramp_state = RAMP_DECEL;
                } else {
                    ramp_state = RAMP_CRUISE;
                }
            } else {
                // Incrementar frecuencia gradualmente
                current_freq = start_freq + (steps_moved / accel_divisor);
                if (current_freq > target_freq)
                    current_freq = target_freq;
                nco_set_freq(current_freq);
            }
        }
        // ¿En velocidad crucero?
        else if (ramp_state == RAMP_CRUISE) {
            // ¿Es hora de desacelerar?
            if (steps_remaining <= ramp_decel_steps) {
                ramp_state = RAMP_DECEL;
            }
        }

        // ¿En desaceleración?
        if (ramp_state == RAMP_DECEL) {
            if (steps_remaining > 0) {
                // Reducir frecuencia gradualmente
                unsigned long tmp_f = start_freq + (steps_remaining / accel_divisor);
                if (tmp_f > target_freq)
                    tmp_f = target_freq;
                current_freq = tmp_f;
                nco_set_freq(current_freq);
            }
        }
    }
}
```

---

## Fórmulas Clave

### 1. Frecuencia del NCO

```c
increment = (freq_hz * 1048576) / 32000000;
```

- `1048576 = 2^20` (resolución del acumulador NCO)
- `32000000` = FOSC (32 MHz, frecuencia del oscilador)

### 2. Conversión de velocidad

```c
target_freq = (velocidad_mm_min * steps_per_mm) / 60;
```

- Convierte mm/min a Hz (pulsos/segundo)

### 3. Pasos de aceleración

```c
ramp_accel_steps = (target_freq - start_freq) * accel_divisor;
```

- Determina cuántos pasos tarda en alcanzar la velocidad máxima

### 4. Frecuencia durante la rampa

**Aceleración:**
```c
current_freq = start_freq + (steps_moved / accel_divisor);
```

**Desaceleración:**
```c
current_freq = start_freq + (steps_remaining / accel_divisor);
```

### 5. Pasos por mm

```c
steps_per_mm = BASE_STEPS_PER_MM * microstepping;
// BASE_STEPS_PER_MM = 40
```

---

## Ejemplo Práctico

### Comando: `G0 X100` (mover 100 mm)

**Configuración:**
- Microstepping: 1/2 (`current_microstepping = 2`)
- Velocidad: 2000 mm/min
- `steps_per_mm = 40 * 2 = 80`

**Paso 1: Calcular destino**
```
target_position = 100 * 80 = 8,000 pasos
```

**Paso 2: Calcular frecuencia objetivo**
```
target_freq = (2000 * 80) / 60 = 2,667 Hz
```

**Paso 3: Configurar rampa (1/2 step)**
```
start_freq = 1800 Hz
accel_divisor = 4
ramp_accel_steps = (2667 - 1800) * 4 = 3,468 pasos
ramp_decel_steps = 3,468 pasos
```

**Paso 4: Determinar perfil**
```
distancia_total = 8,000 pasos
distancia_necesaria = 3468 + 3468 = 6,936 pasos

8000 > 6936 → PERFIL TRAPEZOIDAL

Fases:
- Aceleración: 3,468 pasos (de 1800 Hz a 2667 Hz)
- Crucero: 8000 - 6936 = 1,064 pasos (a 2667 Hz)
- Desaceleración: 3,468 pasos (de 2667 Hz a 1800 Hz)
```

**Paso 5: Ejecución**

| Pasos movidos | Estado | Frecuencia | Notas |
|---------------|--------|------------|-------|
| 0 | RAMP_ACCEL | 1800 Hz | Inicio |
| 500 | RAMP_ACCEL | 1800 + 500/4 = 1925 Hz | Subiendo |
| 1000 | RAMP_ACCEL | 1800 + 1000/4 = 2050 Hz | Subiendo |
| 2000 | RAMP_ACCEL | 1800 + 2000/4 = 2300 Hz | Subiendo |
| 3468 | RAMP_ACCEL→CRUISE | 2667 Hz | Transición |
| 4000 | RAMP_CRUISE | 2667 Hz | Velocidad máxima |
| 4532 | RAMP_CRUISE→DECEL | 2667 Hz | Inicio frenado |
| 5000 | RAMP_DECEL | 1800 + 3000/4 = 2550 Hz | Bajando |
| 6000 | RAMP_DECEL | 1800 + 2000/4 = 2300 Hz | Bajando |
| 7000 | RAMP_DECEL | 1800 + 1000/4 = 2050 Hz | Bajando |
| 7998 | RAMP_DECEL | ~1800 Hz | Casi detenido |
| 8000 | RAMP_IDLE | 0 Hz | Detenido |

---

## Limitaciones y Consideraciones

### 1. Rampa simétrica fija
- `ramp_accel_steps == ramp_decel_steps`
- No considera que la desaceleración podría necesitar diferente pendiente
- El motor podría tener diferente comportamiento al frenar vs acelerar

### 2. Sin perfil S-curve
- La rampa actual es lineal (trapecio/triángulo)
- No hay suavizado en las transiciones (jerk limitado)
- Podría mejorarse con perfil S-curve para movimientos más suaves

### 3. Frecuencia máxima de 90 kHz
- Tope de seguridad: `if (target_freq > 90000) target_freq = 90000;`
- El motor puede no responder bien a frecuencias muy altas
- Algunas frecuencias pueden causar resonancia mecánica

### 4. Cálculo en el loop principal
- La rampa se ejecuta en el `while(1)` de `main()`, no en un ISR
- Si hay otras tareas bloqueantes, la actualización de frecuencia podría retrasarse
- Para mayor precisión, mover a un timer ISR dedicado

### 5. Sin sensor de retroalimentación
- No hay encoder o limit switch para verificar posición real
- Si el motor pierde pasos, el sistema no lo detecta
- Homing (`G28`) no tiene detección automática de fin de carrera

### 6. Parámetros hardcodeados
- `start_freq` y `accel_divisor` están fijos por microstepping
- Idealmente deberían ser configurables por comando G-code (ej. `M204`)

### 7. Posibles mejoras futuras

```
[ ] Comando M204 S<accel> para configurar aceleración
[ ] Perfil S-curve (aceleración jerk-limited)
[ ] Rampas asimétricas (acel != decel)
[ ] Ejecución en ISR para mayor precisión
[ ] Detección de pérdida de pasos
[ ] Límites de software (soft endstops)
[ ] Tabla de frecuencias problemáticas (skip resonancia)
```

---

## Referencia Rápida de Comandos

| Comando | Función | Ejemplo |
|---------|---------|---------|
| `G0 X<pos>` | Mover con rampa | `G0 X50.5` |
| `G1 X<pos>` | Mover con rampa (trabajo) | `G1 X30.0` |
| `M203 S<vel>` | Configurar velocidad | `M203 S2000` |
| `M350 S<mode>` | Configurar microstepping | `M350 S8` |
| `G92 X<pos>` | Establecer posición | `G92 X0` |
| `M114` | Reportar posición | `M114` |

---

**Versión:** 1.0  
**Fecha:** Abril 2026  
**Firmware:** `codigo con rampas funcoinal s1 y s2` (commit `9ae724f`)
