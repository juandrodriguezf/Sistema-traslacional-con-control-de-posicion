# Referencia G-Code - NEMA UART Controller

**MCU:** PIC16F18426 | **Driver:** A4988 | **Interfaz:** UART

## Comandos de Movimiento

| Comando | Descripción | Ejemplo | Respuesta |
|---|---|---|---|
| `G0 X<pos>` | Mover a posición (movimiento rápido) | `G0 X50.5` | `ok` |
| `G1 X<pos>` | Mover a posición (movimiento de trabajo) | `G1 X30.0` | `ok` |

**Notas:**
- `<pos>` es en **milímetros (mm)**
- Posición absoluta respecto al cero actual
- El motor se mueve a la velocidad configurada por `M203`
- Al alcanzar la posición envía `ok`

## Homing y Referencia

| Comando | Descripción | Ejemplo | Respuesta |
|---|---|---|---|
| `G28` | Iniciar homing (velocidad reducida) | `G28` | `Homing...` |
| `G92 X<pos>` | Establecer posición actual | `G92 X0` | `ok X:0.00` |
| `G92 X5.5` | Establecer posición actual a 5.5mm | `G92 X5.5` | `ok X:5.50` |

**Notas:**
- `G28` inicia movimiento de homing a **1/4 de la velocidad máxima**
- **Importante:** El homing actual no tiene sensor de fin de carrera. Debes detenerlo manualmente o usar un switch externo.
- `G92 X0` equivale al antiguo comando `Z` (fijar cero)

## Reporte y Telemetría

| Comando | Descripción | Ejemplo | Respuesta |
|---|---|---|---|
| `M114` | Reportar posición actual bajo demanda | `M114` | `ok X:12.34` |
| `M120` | **Activar** telemetría automática cada 100ms | `M120` | `ok REPORT ON` |
| `M121` | **Desactivar** telemetría automática | `M121` | `ok REPORT OFF` |

**Nota:** La telemetría automática está **desactivada por defecto**. Envía `M120` para activarla y `M121` para detenerla.

## Configuración

| Comando | Descripción | Ejemplo | Respuesta |
|---|---|---|---|
| `M203 S<vel>` | Configurar velocidad máxima (mm/min) | `M203 S2000` | `ok F2000` |
| `M350 S<mode>` | Configurar microstepping | `M350 S8` | `ok M:8` |
| `M203` | Consultar velocidad actual | `M203` | `ok F1000` |
| `M350` | Consultar microstepping actual | `M350` | `ok M:8` |

### Velocidad (`M203`)
- Rango: `1` - `5000` mm/min
- Default: `1000` mm/min
- Afecta la velocidad del NCO (frecuencia de pasos)

### Microstepping (`M350`)

| Valor | Configuración A4988 | MS1 (RC3) | MS2 (RC0) | MS3 (RA2) |
|---|---|---|---|---|
| `M350 S1` | Paso completo | 0 | 0 | 0 |
| `M350 S2` | 1/2 paso | 1 | 0 | 0 |
| `M350 S4` | 1/4 paso | 0 | 1 | 0 |
| `M350 S8` | 1/8 paso (default) | 1 | 1 | 0 |
| `M350 S16` | 1/16 paso | 1 | 1 | 1 |

## Formato de Respuestas

### Posición
```
X:12.34
```
- Positivo: dirección positiva
- Negativo: dirección negativa (`X:-5.67`)

### Confirmación
```
ok
ok F2000
ok M:8
ok X:5.50
```

### Errores
```
error: falta X
error: velocidad fuera de rango (1-5000)
error: microstepping invalid (1,2,4,8,16)
error: comando desconocido
```

## Flujo de Uso Típico

```
1. G28            -> Iniciar homing
2. (motor llega al home)
3. G92 X0         -> Fijar cero
4. M203 S1500     -> Configurar velocidad a 1500 mm/min
5. M350 S8        -> Configurar microstepping 1/8
6. M120           -> Activar telemetría automática (opcional)
7. G1 X50.0       -> Mover a 50mm
8. (si M120 activado: telemetría cada 100ms: X:12.34, X:15.67, ...)
9. M121           -> Desactivar telemetría
10. G0 X0         -> Volver a cero
```

## Notas de UART

- Baudrate: según configuración de MCC
- Cada comando termina con `\r` o `\n`
- No distingue mayúsculas/minúsculas (`g0 x50` = `G0 X50`)
- Buffer de recepción: 32 caracteres máximo
