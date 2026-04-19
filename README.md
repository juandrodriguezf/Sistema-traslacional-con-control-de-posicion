# Sistema Traslacional con Control de Posición

Controlador de motor a pasos NEMA con interfaz G-code vía UART. Firmware para PIC16F18426 usando MPLAB X + XC8.

![MCU](https://img.shields.io/badge/MCU-PIC16F18426-blue) ![Driver](https://img.shields.io/badge/Driver-A4988-green) ![Status](https://img.shields.io/badge/Status-En%20Desarrollo-yellow)

---

## Descripción General

Este proyecto implementa un controlador de motor paso a paso (NEMA 17) con control de posición preciso mediante comandos G-code enviados por puerto serial (UART). Incluye aceleración/desaceleración por rampas para evitar pérdida de pasos, microstepping configurable y telemetría de posición.

### Características Principales

- **Parser G-code completo**: Comandos estándar de movimiento y configuración
- **Control de rampas**: Perfiles trapezoidal y triangular para aceleración suave
- **Microstepping configurable**: 1, 1/2, 1/4, 1/8, 1/16 paso (driver A4988)
- **Telemetría automática**: Reporte de posición cada 100ms (activable)
- **Control de posición**: Contador de pasos con lectura atómica
- **Fin de carrera**: Soporte para homing automático con sensor

---

## Hardware

| Componente       | Especificación                     |
| ---------------- | ----------------------------------- |
| Microcontrolador | PIC16F18426 (14 pines)              |
| Driver motor     | A4988                               |
| ENABLE A4988     | GND (siempre habilitado)            |
| Señal STEP      | NCO1OUT (RC1)                       |
| Señal DIR       | RC2 (pin digital)                   |
| Microstepping    | MS1(RC3), MS2(RC0), MS3(RA2)        |
| UART             | EUSART1 (TX:RC4, RX:RC5)            |
| Fin de carrera   | RA4 (con IOC - Interrupt On Change) |

### Asignación de Pines

| Pin Lógico          | Puerto | Pin Físico (DIP-14) | Función                     |
| -------------------- | ------ | -------------------- | ---------------------------- |
| MS3                  | RA2    | 11                   | Microstepping MS3            |
| MS2                  | RC0    | 10                   | Microstepping MS2            |
| STEP (NCO1OUT)       | RC1    | 9                    | Pulsos STEP                  |
| DIR                  | RC2    | 8                    | Dirección                   |
| MS1                  | RC3    | 7                    | Microstepping MS1            |
| EUSART1 TX           | RC4    | 6                    | UART TX                      |
| EUSART1 RX           | RC5    | 5                    | UART RX                      |
| BUTTON (Fin Carrera) | RA4    | 3                    | Fin de carrera / Botón cero |

---

## Comandos G-code Soportados

### Movimiento

| Comando        | Descripción                | Ejemplo      |
| -------------- | --------------------------- | ------------ |
| `G0 X<pos>`  | Mover rápido a posición   | `G0 X50.5` |
| `G1 X<pos>`  | Mover a posición (trabajo) | `G1 X30.0` |
| `G28`        | Homing (velocidad reducida) | `G28`      |
| `G92 X<pos>` | Establecer posición actual | `G92 X0`   |

### Reporte y Telemetría

| Comando  | Descripción                 | Respuesta                   |
| -------- | ---------------------------- | --------------------------- |
| `M114` | Reportar posición actual    | `ok X:12.34`              |
| `M120` | Activar telemetría (100ms)  | `ok REPORT ON`            |
| `M121` | Desactivar telemetría       | `ok REPORT OFF`           |
| `M900` | Estado de pines (DIR, MS1-3) | `DIR:0 MS1:1 MS2:1 MS3:0` |

### Configuración

| Comando          | Descripción               | Rango      |
| ---------------- | -------------------------- | ---------- |
| `M203 S<vel>`  | Velocidad máxima (mm/min) | 1-90000    |
| `M350 S<mode>` | Microstepping (1,2,4,8,16) | 1/1 a 1/16 |

**Valores por defecto:**

- Velocidad: 1000 mm/min
- Microstepping: 1/8 paso
- Pasos por mm (base): 40 steps/mm × modo

---

## Estructura del Proyecto

```
nema_uart.X/
├── main.c                      # Firmware principal (parser G-code + máquinas de estado)
├── mcc_generated_files/        # Drivers MCC generados
│   ├── eusart1.c/h            # Driver UART
│   ├── nco1.c/h               # Numerically Controlled Oscillator (STEP)
│   ├── tmr2.c/h               # Timer 2 (telemetría 10Hz)
│   ├── pin_manager.c/h        # Gestión de pines
│   └── ...
├── diagramas/                  # Diagramas PlantUML (9 archivos)
│   ├── 01_hardware_architecture.puml
│   ├── 02_pin_mapping.puml
│   ├── 03_state_machine.puml
│   ├── 04_gcode_parser_flow.puml
│   ├── 05_timing_signals.puml
│   ├── 06_microstepping_config.puml
│   ├── 07_ramp_profiles.puml
│   ├── 08_uart_protocol.puml
│   └── 09_nco_frequency_calc.puml
├── nema_uart.mc3              # Configuración MCC (Microchip Code Configurator)
├── Makefile                   # Build configuration
├── CONTEXT.md                 # Documentación técnica detallada
├── GCODE_REFERENCE.md         # Manual de referencia G-code
├── RAMP_GUIDE.md              # Guía de implementación de rampas
└── README.md                  # Este archivo
```

---

## Ramas del Repositorio

El repositorio tiene **dos ramas principales**:

### `main` (Estable)

- **Estado**: Versión funcional estable
- **Características**:
  - Full step funcional (S1 y S2)
  - Control básico de velocidad
  - Comandos G-code core implementados
  - Tabla de frecuencias permitidas (Excel)
- **Últimos commits**:
  - `634552c` - Versión más estable de S1 y S2
  - `a891836` - Full step funcional
  - `899b447` - Full step más rápido

### `version-protoboard` (Desarrollo Experimental)

- **Estado**: Rama de desarrollo activo (HEAD actual)
- **Características**:
  - Sistema de rampas (aceleración/desaceleración)
  - Fin de carrera funcionando
  - S4 en progreso
  - Documentación extendida (diagramas, guías)
  - Telemetría automática configurable
- **Últimos commits**:
  - `6e55337` - Update MCC config, manifest, git_repo submodule
  - `0ed265d` - Final de carrera funcionando
  - `d66b50d` - Versión más funcional, S4 en progreso
  - `0ea727e` - Actualizar gitignore y agregar guía de rampas
  - `9ae724f` - Código con rampas funcional S1 y S2

---

## Configuración de Rampas

**Tipos de perfil:**

- **Trapezoidal**: Acelerar → Crucero → Desacelerar (distancias largas)
- **Triangular**: Acelerar hasta pico < max, luego desacelerar (distancias cortas)

---

## Compilación y Flash

### Requisitos

- MPLAB X IDE
- XC8 Compiler
- Microchip Code Configurator (MCC)

### Pasos

1. Abrir proyecto en MPLAB X (`nema_uart.X`)
2. Compilar (Clean and Build)
3. Programar PIC16F18426 con Pickit4 o compatible

---

## Uso Básico (Terminal UART)

```
# Configurar sistema
M203 S1500        # Velocidad 1500 mm/min
M350 S8           # Microstepping 1/8

# Homing y referencia
G28               # Iniciar homing
(detener manualmente o por fin de carrera)
G92 X0            # Fijar posición cero

# Movimiento
G0 X50.5          # Mover a 50.5mm
M114              # Consultar posición

# Telemetría
M120              # Activar reporte automático cada 100ms
M121              # Desactivar reporte automático
```

---

## Documentación Adicional

- [`CONTEXT.md`](CONTEXT.md) - Contexto técnico completo del proyecto
- [`GCODE_REFERENCE.md`](GCODE_REFERENCE.md) - Manual de referencia G-code
- [`RAMP_GUIDE.md`](RAMP_GUIDE.md) - Guía de implementación de rampas
- [`diagramas/`](diagramas/) - Diagramas de arquitectura (PlantUML)

---

## Limitaciones Conocidas

1. **Homing**: Requiere intervención manual o sensor de fin de carrera (en progreso en rama `version-protoboard`)
2. **Aceleración**: Perfiles de rampa en microstepping 1/4, 1/8, 1/16 aún en validación experimental
3. **Sin software limits**: No hay límites de recorrido configurables
4. **ENABLE fijo**: Conectado a GND, no se puede desenergizar motor por software

---

## Autor

**Juan David Rodriguez** - [@juandrodriguezf](https://github.com/juandrodriguezf)

---

## Licencia

Este proyecto usa código generado por Microchip Technology Inc. sujeto a sus términos de licencia. El código propio sigue las mismas convenciones open-source.
