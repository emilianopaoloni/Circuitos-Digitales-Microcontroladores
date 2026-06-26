#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>     // Necesario para usar variables booleanas (true/false)
#include "uart.h"
#include "dht11.h"
#include "rtc.h"

#ifndef F_CPU
#define F_CPU 16000000UL 
#endif

// Variables globales para la sincronización y configuración de tiempos (Background)
volatile uint8_t flag_muestreo = 0;
volatile uint8_t intervalo_reporte = 2;   // Valor interno, pero esperaremos a que el usuario lo confirme
volatile uint8_t contador_segundos = 0;

// Banderas de estado del sistema (Para la configuración inicial)
bool hora_configurada = false;
bool tasa_configurada = false;
bool sistema_activo = false;

// Configuración del Timer1: Genera un tick cada 1 segundo exacto
void Timer1_Init(void) {
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC, Prescaler 1024
    OCR1A = 15624; // 16MHz / 1024 = 15625 Hz -> 1 segundo
    TIMSK1 = (1 << OCIE1A); 
}

// Rutina de interrupción del Timer1 (Background)
ISR(TIMER1_COMPA_vect) {
    contador_segundos++;
    if (contador_segundos >= intervalo_reporte) {
        contador_segundos = 0;
        flag_muestreo = 1;
    }
}

int main(void) {
    // 1. Inicialización de periféricos
    UART_Init(9600);
    DHT11_Init();
    RTC_Init();
    Timer1_Init();
    
    // Buffers y variables de datos
    DHT11_Data datos_dht;
    uint8_t h_actual, m_actual, s_actual;
    char buffer_salida[128];
    char buffer_rx[32];
    uint8_t rx_index = 0;
    
    // 2. Habilitar interrupciones globales
    sei(); 
    
    // Mensaje de bienvenida adaptado al nuevo requerimiento
    UART_SendString("--- MONITOR DE INVERNADERO ---\r\n");
    UART_SendString("Estado: ESPERANDO CONFIGURACION INICIAL\r\n");
    UART_SendString("Por favor ingrese los siguientes parametros para iniciar:\r\n");
    UART_SendString("  1. Hora actual (ej. SET_TIME=14:30:00)\r\n");
    UART_SendString("  2. Tasa de muestreo (ej. SET_TM=05)\r\n\n> ");

    // 3. Bucle Principal (Foreground)
    while (1) {
        
        // --- TAREA 1: PROCESAR COMANDOS DE LA TERMINAL ---
        if (UART_IsDataAvailable()) {
            char c = UART_GetChar();
            UART_SendChar(c); // Eco
            
            if (c == '\r') {
                buffer_rx[rx_index] = '\0';
                
                int h, m, s, tm;
                
                // Opción A: Configurar Tiempo
                if (sscanf(buffer_rx, "SET_TIME=%d:%d:%d", &h, &m, &s) == 3) {
                    if (h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
                        RTC_SetTime((uint8_t)h, (uint8_t)m, (uint8_t)s);
                        hora_configurada = true;
                        UART_SendString("\n[SISTEMA] Hora guardada correctamente.\r\n");
                    } else {
                        UART_SendString("\n[ERROR] Formato de hora invalido.\r\n");
                    }
                }
                // Opción B: Configurar Tasa de Muestreo
                else if (sscanf(buffer_rx, "SET_TM=%d", &tm) == 1) {
                    if (tm >= 2 && tm <= 60) {
                        cli(); 
                        intervalo_reporte = (uint8_t)tm;
                        contador_segundos = 0; 
                        sei();
                        tasa_configurada = true;
                        sprintf(buffer_salida, "\n[SISTEMA] Tasa de muestreo configurada a %d seg.\r\n", tm);
                        UART_SendString(buffer_salida);
                    } else {
                        UART_SendString("\n[ERROR] Intervalo fuera de rango (2-60).\r\n");
                    }
                } 
                else {
                    UART_SendString("\n[ERROR] Comando no reconocido.\r\n");
                }
                
                rx_index = 0; // Limpiamos para el próximo comando
                
                // Verificamos si estamos listos para iniciar por primera vez
                if (!sistema_activo && hora_configurada && tasa_configurada) {
                    sistema_activo = true;
                    UART_SendString("\n==========================================\r\n");
                    UART_SendString(" CONFIGURACION COMPLETA - INICIANDO LECTURAS\r\n");
                    UART_SendString("==========================================\r\n");
                    
                    // Reseteamos el temporizador para que el primer reporte salga limpio
                    cli();
                    contador_segundos = 0;
                    flag_muestreo = 0;
                    sei();
                } else {
                    UART_SendString("> "); // Prompt de consola si seguimos configurando
                }
            } 
            else if (rx_index < 30 && c != '\n') { 
                buffer_rx[rx_index] = c;
                rx_index++;
            }
        }
        
        // --- TAREA 2: MUESTREO (Solo si el sistema está activo) ---
        // Si flag_muestreo es 1, pero sistema_activo es false, simplemente bajamos la bandera y la ignoramos
        if (flag_muestreo) {
            flag_muestreo = 0; 
            
            if (sistema_activo) {
                DHT11_Status estado_dht = DHT11_Read(&datos_dht);
                RTC_GetTime(&h_actual, &m_actual, &s_actual);
                
                const char* estado_invernadero = "ESTABLE";
                
                if (estado_dht == DHT11_OK) {
                    uint8_t es_dia = (h_actual >= 7 && h_actual < 19);
                    
                    if (es_dia) { 
                        if (datos_dht.temperatura < 20 || datos_dht.temperatura > 30 ||
                            datos_dht.humedad < 50 || datos_dht.humedad > 70) {
                            estado_invernadero = "ALARMA! (DIA)";
                        }
                    } else { 
                        if (datos_dht.temperatura < 15 || datos_dht.temperatura > 22 ||
                            datos_dht.humedad < 60 || datos_dht.humedad > 80) {
                            estado_invernadero = "ALARMA! (NOCHE)";
                        }
                    }
                    
                    sprintf(buffer_salida, "\r\n[%02d:%02d:%02d] T: %d C | H: %d %% | Estado: %s\r\n> ", 
                            h_actual, m_actual, s_actual, 
                            datos_dht.temperatura, datos_dht.humedad, estado_invernadero);
                } else {
                    sprintf(buffer_salida, "\r\n[%02d:%02d:%02d] ERROR LECTURA SENSOR DHT11\r\n> ", 
                            h_actual, m_actual, s_actual);
                }
                
                UART_SendString(buffer_salida);
                
                // Reimprimir buffer si el usuario estaba a mitad de escribir un comando
                if (rx_index > 0) {
                    buffer_rx[rx_index] = '\0';
                    UART_SendString(buffer_rx);
                }
            }
        }
    }
    
    return 0;
}