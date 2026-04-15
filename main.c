/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules
  selected in the GUI. Generation Information : Product Revision  :  PIC10 /
  PIC12 / PIC16 / PIC18 MCUs - 1.81.8 Device            :  PIC16F18426 Driver
  Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip software
   and any derivatives exclusively with Microchip products. It is your
   responsibility to comply with third party license terms applicable to your
   use of third party software (including open source software) that may
   accompany Microchip software.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
    FOR A PARTICULAR PURPOSE.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
    SOFTWARE.
*/
#include "mcc_generated_files/mcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>

// ================= VARIABLES =================

typedef enum { IDLE, HOMING, MOVE } system_state_t;

volatile system_state_t state = IDLE;

volatile long current_position = 0;
volatile long target_position = 0;

volatile uint8_t dir = 0;
volatile uint8_t motor_enable = 0;
volatile uint8_t report_position_flag = 0;
volatile uint8_t periodic_report_enabled = 0; // 0 = apagado por defecto

// UART buffer para G-code
char rx_buffer[32];
uint8_t rx_index = 0;

// pasos por mm base (Full Step)
#define BASE_STEPS_PER_MM 40

// velocidad maxima en mm/min (default 1000 mm/min)
static long max_speed_mm_min = 1000;

// microstepping actual
static uint8_t current_microstepping =
    1; // Empieza en Full Step segun conversacion

static uint16_t get_steps_per_mm(void) {
  return BASE_STEPS_PER_MM * current_microstepping;
}

// ================= VARIABLES DE RAMPA =================
typedef enum { RAMP_IDLE, RAMP_ACCEL, RAMP_CRUISE, RAMP_DECEL } ramp_state_t;

volatile ramp_state_t ramp_state = RAMP_IDLE;
volatile unsigned long ramp_start_pos = 0;
volatile unsigned long ramp_total_steps = 0;
volatile unsigned long ramp_accel_steps = 0;
volatile unsigned long ramp_decel_steps = 0;

volatile unsigned long current_freq = 400;
volatile unsigned long target_freq = 1500;
volatile unsigned long start_freq = 400;
volatile unsigned long accel_divisor = 2; // global para que main() tenga acceso

// ================= UART =================

void UART_SendString(const char *str) {
  while (*str)
    EUSART1_Write(*str++);
}

static void UART_SendChar(char c) { EUSART1_Write(c); }

// ================= POSICION =================

static long position_get_atomic(void) {
  long position;
  uint8_t gie_state = INTCONbits.GIE;

  INTERRUPT_GlobalInterruptDisable();
  position = current_position;
  if (gie_state)
    INTERRUPT_GlobalInterruptEnable();

  return position;
}

static void UART_SendPosition(void) {
  char tx_buffer[32];
  long position_steps = position_get_atomic();
  long position_centimm = (position_steps * 100L) / get_steps_per_mm();
  long abs_centimm = labs(position_centimm);

  if (position_centimm < 0)
    sprintf(tx_buffer, "X:-%ld.%02ld\r\n", abs_centimm / 100L,
            abs_centimm % 100L);
  else
    sprintf(tx_buffer, "X:%ld.%02ld\r\n", abs_centimm / 100L,
            abs_centimm % 100L);

  UART_SendString(tx_buffer);
}

static void TMR2_ReportPositionISR(void) { report_position_flag = 1; }

// ================= CONTROL NCO =================

static void nco_set_freq(unsigned long freq_hz) {
  unsigned long increment = (freq_hz * 1048576UL) / 32000000UL;

  if (increment == 0)
    increment = 1;

  NCO1INCL = (uint8_t)(increment & 0xFF);
  NCO1INCH = (uint8_t)((increment >> 8) & 0xFF);
  NCO1INCU = (uint8_t)((increment >> 16) & 0xFF);
}

void motor_start(void) {
  NCO1CONbits.EN = 1;
  motor_enable = 1;
}

void motor_stop(void) {
  NCO1CONbits.EN = 0;
  motor_enable = 0;
}

// ================= MICROSTEPPING =================

static void setMicrostep(uint8_t mode) {
  current_microstepping = mode;

  switch (mode) {
  case 1: // full step
    LATCbits.LATC3 = 0;
    LATCbits.LATC0 = 0;
    LATAbits.LATA2 = 0;
    break;
  case 2: // 1/2
    LATCbits.LATC3 = 1;
    LATCbits.LATC0 = 0;
    LATAbits.LATA2 = 0;
    break;
  case 4: // 1/4
    LATCbits.LATC3 = 0;
    LATCbits.LATC0 = 1;
    LATAbits.LATA2 = 0;
    break;
  case 8: // 1/8
    LATCbits.LATC3 = 1;
    LATCbits.LATC0 = 1;
    LATAbits.LATA2 = 0;
    break;
  case 16: // 1/16
    LATCbits.LATC3 = 1;
    LATCbits.LATC0 = 1;
    LATAbits.LATA2 = 1;
    break;
  default:
    break;
  }
}

// ================= G-CODE PARSER =================

static void execute_gcode(const char *cmd) {
  char tx_buffer[64];

  // Ignorar lineas vacias
  if (cmd[0] == '\0')
    return;

  // ================= G0 / G1 =================
  if (cmd[0] == 'G' && (cmd[1] == '0' || cmd[1] == '1')) {
    // Buscar X
    const char *x_ptr = 0;
    const char *p = cmd + 2;
    while (*p) {
      if (*p == 'X' || *p == 'x') {
        x_ptr = p + 1;
        break;
      }
      p++;
    }

    if (x_ptr) {
      float mm_val = (float)atof(x_ptr);
      long steps = (long)(mm_val * get_steps_per_mm());

      target_position = steps;
      ramp_start_pos = position_get_atomic();
      ramp_total_steps = labs(target_position - ramp_start_pos);

      if (target_position > ramp_start_pos) {
        dir = 1;
        LATCbits.LATC2 = 1;
      } else {
        dir = 0;
        LATCbits.LATC2 = 0;
      }

      // --- Configuracion de Rampa (Full Step) ---
      target_freq =
          ((unsigned long)max_speed_mm_min * get_steps_per_mm()) / 60UL;
      if (target_freq > 20000)
        target_freq = 20000; // Nuevo tope solicitado (20k Hz)

      if (current_microstepping == 1) {
        start_freq = 900;
        accel_divisor = 2; // df = 0.5 Hz / paso
      } else if (current_microstepping == 2) {
        // En 1/2 step la arrancada (en Hz) suele ser mayor para romper la
        // inercia pero la rampa debe ser mas suave por la perdida de torque.
        start_freq = 2000;
        accel_divisor = 4; // df = 0.25 Hz / paso
      } else {
        start_freq = 200 * current_microstepping;
        accel_divisor = 2 * current_microstepping;
      }

      current_freq = start_freq;

      // Constante aceleracion calculada dinamicamente
      ramp_accel_steps = (target_freq - start_freq) * accel_divisor;
      ramp_decel_steps = ramp_accel_steps; // Perfil trapecio simetrico

      // Ajuste para perfil triangular si el tramo no da para acelerar a fondo
      if (ramp_total_steps <= (ramp_accel_steps + ramp_decel_steps)) {
        ramp_accel_steps = ramp_total_steps / 2;
        ramp_decel_steps =
            ramp_total_steps - ramp_accel_steps; // el resto frena
      }

      state = MOVE;
      ramp_state = RAMP_ACCEL;
      nco_set_freq(current_freq);
      motor_start();

      UART_SendString("ok\r\n");
    } else {
      UART_SendString("error: falta X\r\n");
    }
  }

  // ================= G28 =================
  else if (cmd[0] == 'G' && cmd[1] == '2' && cmd[2] == '8') {
    UART_SendString("Homing...\r\n");

    state = HOMING;
    dir = 0;
    LATCbits.LATC2 = 0;

    nco_set_freq(400); // homing lento fijo y seguro
    motor_start();
  }

  // ================= G92 =================
  else if (cmd[0] == 'G' && cmd[1] == '9' && cmd[2] == '2') {
    const char *x_ptr = 0;
    const char *p = cmd + 3;
    while (*p) {
      if (*p == 'X' || *p == 'x') {
        x_ptr = p + 1;
        break;
      }
      p++;
    }

    if (x_ptr) {
      float mm_val = (float)atof(x_ptr);
      long new_steps = (long)(mm_val * get_steps_per_mm());

      // Ajustar offset: current_position pasa a ser new_steps
      current_position = new_steps;

      // Si estaba en HOMING o MOVE, detener y pasar a IDLE
      if (state == HOMING || state == MOVE) {
        motor_stop();
        state = IDLE;
      }

      sprintf(tx_buffer, "ok X:%.2f\r\n", mm_val);
      UART_SendString(tx_buffer);
    } else {
      // G92 sin argumentos -> poner cero
      current_position = 0;

      // Si estaba en HOMING o MOVE, detener y pasar a IDLE
      if (state == HOMING || state == MOVE) {
        motor_stop();
        state = IDLE;
      }

      UART_SendString("ok X:0.00\r\n");
    }
  }

  // ================= M114 =================
  else if (cmd[0] == 'M' && cmd[1] == '1' && cmd[2] == '1' && cmd[3] == '4') {
    UART_SendString("ok ");
    UART_SendPosition();
  }

  // ================= M120 - Enable periodic report =================
  else if (cmd[0] == 'M' && cmd[1] == '1' && cmd[2] == '2' && cmd[3] == '0') {
    periodic_report_enabled = 1;
    UART_SendString("ok REPORT ON\r\n");
  }

  // ================= M121 - Disable periodic report =================
  else if (cmd[0] == 'M' && cmd[1] == '1' && cmd[2] == '2' && cmd[3] == '1') {
    periodic_report_enabled = 0;
    UART_SendString("ok REPORT OFF\r\n");
  }

  // ================= M203 =================
  else if (cmd[0] == 'M' && cmd[1] == '2' && cmd[2] == '0' && cmd[3] == '3') {
    const char *s_ptr = 0;
    const char *p = cmd + 4;
    while (*p) {
      if (*p == 'S' || *p == 's') {
        s_ptr = p + 1;
        break;
      }
      p++;
    }

    if (s_ptr) {
      long speed = atol(s_ptr);
      if (speed > 0 && speed <= 20000) {
        max_speed_mm_min = speed;
        sprintf(tx_buffer, "ok F%ld\r\n", speed);
        UART_SendString(tx_buffer);
      } else {
        UART_SendString("error: velocidad fuera de rango (1-20000)\r\n");
      }
    } else {
      sprintf(tx_buffer, "ok F%ld\r\n", max_speed_mm_min);
      UART_SendString(tx_buffer);
    }
  }

  // ================= M350 =================
  else if (cmd[0] == 'M' && cmd[1] == '3' && cmd[2] == '5' && cmd[3] == '0') {
    const char *s_ptr = 0;
    const char *p = cmd + 4;
    while (*p) {
      if (*p == 'S' || *p == 's') {
        s_ptr = p + 1;
        break;
      }
      p++;
    }

    if (s_ptr) {
      uint8_t mode = (uint8_t)atoi(s_ptr);
      if (mode == 1 || mode == 2 || mode == 4 || mode == 8 || mode == 16) {
        setMicrostep(mode);
        sprintf(tx_buffer, "ok M:%d\r\n", mode);
        UART_SendString(tx_buffer);
      } else {
        UART_SendString("error: microstepping invalid (1,2,4,8,16)\r\n");
      }
    } else {
      sprintf(tx_buffer, "ok M:%d\r\n", current_microstepping);
      UART_SendString(tx_buffer);
    }
  }

  // ================= COMANDO DESCONOCIDO =================
  else {
    UART_SendString("error: comando desconocido\r\n");
  }
}

// ================= MAIN =================

void main(void) {
  SYSTEM_Initialize();

  // Asegurar que el NCO esta apagado al inicio
  NCO1CONbits.EN = 0;

  TMR2_SetInterruptHandler(TMR2_ReportPositionISR);
  INTERRUPT_GlobalInterruptEnable();
  INTERRUPT_PeripheralInterruptEnable();

  // Configuracion inicial
  LATCbits.LATC2 = 0;       // DIR
  setMicrostep(2);          // Inicia en modo 1/2 step
  nco_set_freq(start_freq); // Inicializa a la frecuencia de arranque

  UART_SendString("Sistema listo (G-code Rampas 1/2 Step)\r\n");

  while (1) {
    if (report_position_flag && periodic_report_enabled) {
      report_position_flag = 0;
      UART_SendPosition();
    }

    // ================= UART =================

    if (EUSART1_is_rx_ready()) {
      char c = EUSART1_Read();

      // -------- FIN DE COMANDO --------
      if (c == '\r' || c == '\n') {
        if (rx_index > 0) {
          rx_buffer[rx_index] = '\0';
          rx_index = 0;

          // Convertir a mayusculas para parseo
          for (uint8_t i = 0; i < strlen(rx_buffer); i++) {
            if (rx_buffer[i] >= 'a' && rx_buffer[i] <= 'z')
              rx_buffer[i] = rx_buffer[i] - 'a' + 'A';
          }

          execute_gcode(rx_buffer);
        }
      }

      // -------- BUFFER DE COMANDO --------
      else if (rx_index < 31) {
        rx_buffer[rx_index++] = c;
      }
    }

    // ================= CONTROL DE MOVIMIENTO =================

    if (state == MOVE) {
      long current_local_pos = position_get_atomic();
      unsigned long steps_moved = labs(current_local_pos - ramp_start_pos);
      unsigned long steps_remaining = labs(target_position - current_local_pos);

      // maquina de estados de rampa
      if (steps_remaining < 2) {
        motor_stop();
        state = IDLE;
        ramp_state = RAMP_IDLE;
        UART_SendString("ok\r\n");
      } else {
        if (ramp_state == RAMP_ACCEL) {
          if (steps_moved >= ramp_accel_steps) {
            current_freq = target_freq;
            nco_set_freq(current_freq);
            if (ramp_total_steps <= (ramp_accel_steps + ramp_decel_steps)) {
              // Era triangular
              ramp_state = RAMP_DECEL;
            } else {
              ramp_state = RAMP_CRUISE;
            }
          } else {
            // df adaptativo segun modo
            current_freq = start_freq + (steps_moved / accel_divisor);
            if (current_freq > target_freq)
              current_freq = target_freq;
            nco_set_freq(current_freq);
          }
        } else if (ramp_state == RAMP_CRUISE) {
          if (steps_remaining <= ramp_decel_steps) {
            ramp_state = RAMP_DECEL;
          }
        }

        if (ramp_state == RAMP_DECEL) {
          if (steps_remaining > 0) {
            // desaceleracion adaptativa segun modo
            unsigned long tmp_f =
                start_freq + (steps_remaining / accel_divisor);
            if (tmp_f > target_freq)
              tmp_f = target_freq; // seguridad
            current_freq = tmp_f;
            nco_set_freq(current_freq);
          }
        }
      }
    }
  }
}
