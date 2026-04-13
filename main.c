/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC16F18426
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
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
#include <xc.h>
#include "mcc_generated_files/mcc.h"
#include <stdlib.h>
#include <stdio.h>

// ================= VARIABLES =================

typedef enum {
    IDLE,
    HOMING,
    MOVE
} system_state_t;

volatile system_state_t state = IDLE;

volatile long current_position = 0;
volatile long target_position = 0;

volatile uint8_t dir = 0;
volatile uint8_t motor_enable = 0;
volatile uint8_t report_position_flag = 0;

// UART buffer
char rx_buffer[10];
uint8_t rx_index = 0;

// pasos por mm
#define STEPS_PER_MM 320

// ================= UART =================

void UART_SendString(const char *str)
{
    while(*str)
        EUSART1_Write(*str++);
}

static long position_get_atomic(void)
{
    long position;
    uint8_t gie_state = INTCONbits.GIE;

    INTERRUPT_GlobalInterruptDisable();
    position = current_position;
    if(gie_state)
        INTERRUPT_GlobalInterruptEnable();

    return position;
}

static void UART_SendPosition(void)
{
    char tx_buffer[32];
    long position_steps = position_get_atomic();
    long position_centimm = (position_steps * 100L) / STEPS_PER_MM;
    long abs_centimm = labs(position_centimm);

    if(position_centimm < 0)
        sprintf(tx_buffer, "X:-%ld.%02ld\r\n",
                abs_centimm / 100L,
                abs_centimm % 100L);
    else
        sprintf(tx_buffer, "X:%ld.%02ld\r\n",
                abs_centimm / 100L,
                abs_centimm % 100L);

    UART_SendString(tx_buffer);
}

static void TMR2_ReportPositionISR(void)
{
    report_position_flag = 1;
}

// ================= CONTROL NCO =================

void motor_start(void)
{
    NCO1CONbits.EN = 1;
    motor_enable = 1;
}

void motor_stop(void)
{
    NCO1CONbits.EN = 0;
    motor_enable = 0;
}

// ================= MICROSTEPPING =================

void setMicrostep(uint8_t mode)
{
    switch(mode)
    {
        case 8: // 1/8
            LATCbits.LATC3 = 1;
            LATCbits.LATC0 = 1;
            LATAbits.LATA2 = 0;
            break;
    }
}

// ================= MAIN =================

void main(void)
{
    SYSTEM_Initialize();

    // Asegurar que el NCO est� apagado al inicio
    NCO1CONbits.EN = 0;

    TMR2_SetInterruptHandler(TMR2_ReportPositionISR);
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();

    // Configuraci�n inicial
    LATCbits.LATC2 = 0; // DIR
    setMicrostep(8);

    UART_SendString("\r\nSistema listo (NCO)\r\n");

    while (1)
    {
        if(report_position_flag)
        {
            report_position_flag = 0;
            UART_SendPosition();
        }

        // ================= UART =================

        if(EUSART1_is_rx_ready())
        {
            char c = EUSART1_Read();

            // -------- HOMING --------
            if(c == 'H')
            {
                UART_SendString("Homing...\r\n");

                state = HOMING;
                dir = 0;
                LATCbits.LATC2 = 0;

                motor_start();
            }

            // -------- SET ZERO --------
            else if(c == 'Z')
            {
                if(state == HOMING)
                {
                    motor_stop();
                    current_position = 0;
                    state = IDLE;

                    UART_SendString("Cero fijado\r\n");
                }
            }

            // -------- BUFFER NUMERICO --------
            else if(c >= '0' && c <= '9')
            {
                if(rx_index < 9)
                    rx_buffer[rx_index++] = c;
            }

            // -------- FIN DE COMANDO --------
            else if(c == '\r' || c == '\n')
            {
                if(rx_index > 0)
                {
                    rx_buffer[rx_index] = '\0';

                    int mm = atoi(rx_buffer);
                    rx_index = 0;

                    if(mm < 0 || mm > 200)
                    {
                        UART_SendString("Fuera de rango\r\n");
                    }
                    else
                    {
                        target_position = (long)mm * STEPS_PER_MM;

                        if(target_position > current_position)
                        {
                            dir = 1;
                            LATCbits.LATC2 = 1;
                        }
                        else
                        {
                            dir = 0;
                            LATCbits.LATC2 = 0;
                        }

                        state = MOVE;
                        motor_start();

                        UART_SendString("Moviendo...\r\n");
                    }
                }
            }
        }

        // ================= CONTROL DE MOVIMIENTO =================

        if(state == MOVE)
        {
            // margen para evitar errores por pasos perdidos
            if(labs(current_position - target_position) < 2)
            {
                motor_stop();
                state = IDLE;

                UART_SendString("Posicion alcanzada\r\n");
            }
        }
    }
}




