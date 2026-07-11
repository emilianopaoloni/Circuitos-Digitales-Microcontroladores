//GRUPO 17: EMILIANO PAOLONI Y BERNARDO ETCHETO

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h> // Para atoi() (ASCII to Integer)
#include <stdio.h>
#include <stdbool.h>  
#include "term.h"
#include "dht11.h"
#include "rtc.h"

#define CMD_INVALIDO 0
#define CMD_SET_TIME 1
#define CMD_SET_TM   2

// Variables globales para la sincronización y configuración de tiempos (Background)
volatile uint8_t flag_muestreo = 0;
volatile uint8_t intervalo_reporte = 2;   // Valor interno, pero esperaremos a que el usuario lo confirme
volatile uint8_t contador_segundos = 0;

// Banderas de estado del sistema (Para la configuración inicial)
bool hora_configurada = false;
bool tasa_configurada = false;
bool sistema_activo = false;

// Función centralizada de validación
uint8_t ValidarComando(const char* comando, uint8_t* out_h, uint8_t* out_m, uint8_t* out_s, uint8_t* out_tm) {
    
    // ==============================================================
    // VALIDACIÓN PARA: SET_TIME=HH:MM:SS
    // ==============================================================
    if (strncmp(comando, "SET_TIME=", 9) == 0) {
        const char* tiempo = comando + 9; // Apuntamos a la hora
        
        // Verificamos longitud estricta (8 caracteres)
        if (strlen(tiempo) != 8) return CMD_INVALIDO;
        
        // Verificamos los dos puntos ':'
        if (tiempo[2] != ':' || tiempo[5] != ':') return CMD_INVALIDO;
        
        // Verificamos que todo el resto sean números
        if (tiempo[0] < '0' || tiempo[0] > '9') return CMD_INVALIDO;
        if (tiempo[1] < '0' || tiempo[1] > '9') return CMD_INVALIDO;
        if (tiempo[3] < '0' || tiempo[3] > '9') return CMD_INVALIDO;
        if (tiempo[4] < '0' || tiempo[4] > '9') return CMD_INVALIDO;
        if (tiempo[6] < '0' || tiempo[6] > '9') return CMD_INVALIDO;
        if (tiempo[7] < '0' || tiempo[7] > '9') return CMD_INVALIDO;
        
        // Validación lógica (que la hora exista)
        uint8_t horas = (tiempo[0] - '0') * 10 + (tiempo[1] - '0');
        uint8_t minutos = (tiempo[3] - '0') * 10 + (tiempo[4] - '0');
        uint8_t segundos = (tiempo[6] - '0') * 10 + (tiempo[7] - '0');
        
        if (horas > 23 || minutos > 59 || segundos > 59) return CMD_INVALIDO;
        
        //paso toods los filtros, guardo los valores
        *out_h = (tiempo[0] - '0') * 10 + (tiempo[1] - '0');
        *out_m = (tiempo[3] - '0') * 10 + (tiempo[4] - '0');
        *out_s = (tiempo[6] - '0') * 10 + (tiempo[7] - '0');

        return CMD_SET_TIME; // Pasó todos los filtros
    }
    
    // ==============================================================
    // VALIDACIÓN PARA: SET_TM=SS
    // ==============================================================
    else if (strncmp(comando, "SET_TM=", 7) == 0) {
        const char* tasa = comando + 7; // Apuntamos al número
        uint8_t len = strlen(tasa);
        
        // Puede tener 1 dígito (ej: "5") o 2 dígitos (ej: "60")
        if (len < 1 || len > 2) return CMD_INVALIDO;
        
        // Verificamos que sean exclusivamente números y no letras
        for (uint8_t i = 0; i < len; i++) {
            if (tasa[i] < '0' || tasa[i] > '9') return CMD_INVALIDO;
        }
        
        // Calculamos el valor matemático
        uint8_t valor = 0;
        if (len == 1) {
            valor = tasa[0] - '0';
        } else {
            valor = (tasa[0] - '0') * 10 + (tasa[1] - '0');
        }
        
        // Validación lógica (El TP exige que sea entre 2 y 60)
        if (valor < 2 || valor > 60) return CMD_INVALIDO;

        //paso todos los filtros, guardo el valor
        *out_tm = valor;
        
        return CMD_SET_TM; // Pasó todos los filtros, ¡es un SET_TM válido!
    }
    
    //si no coincide con ningun comando conocido
    return CMD_INVALIDO; 
}

// Configuración del Timer1: Genera un tick cada 1 segundo 
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

    TERMINAL_Init(9600); 
    DHT11_Init();
    RTC_Init();
    Timer1_Init();
    
    // Buffers y variables de datos
    DHT11_Data datos_dht;
    char buffer_salida[128];
    //char buffer_rx[32];
    //uint8_t rx_index = 0;
    uint8_t h_actual, m_actual, s_actual;
    uint8_t h = 0;
    uint8_t m = 0;
    uint8_t s = 0;
    uint8_t tm = 0;
    
    // habilitar interrupciones globales
    sei(); 
    
    // Mensaje de bienvenida adaptado al nuevo requerimiento
    TERMINAL_SendString("--- MONITOR DE INVERNADERO ---\r\n");
    TERMINAL_SendString("Por favor ingrese los siguientes parametros para iniciar:\r\n");
    TERMINAL_SendString(" Hora actual: SET_TIME=HH:MM:SS\r\n");
    TERMINAL_SendString(" Tasa de muestreo del clima: SET_TM=SS\r\n\n> ");

    while (1) {
        if (TERMINAL_HayComandoNuevo()) {
            
            char comando_local[32];
            
            cli();
            strcpy(comando_local, TERMINAL_ObtenerComando());
            TERMINAL_LimpiarComando();
            sei(); 
            
            //analizo si es un comando valido:
            uint8_t tipo_comando = ValidarComando(comando_local, &h, &m, &s, &tm);

           if (tipo_comando == CMD_SET_TIME) {
                
                char* tiempo_ingresado = comando_local + 9; 
                
                RTC_SetTime(h, m, s); //actualizo hora
                hora_configurada = true;

                TERMINAL_SendString("-> Reloj actualizado a ");
                TERMINAL_SendString(tiempo_ingresado);
                TERMINAL_SendString("\r\n");
                
                // RTC_SetTime(tiempo_ingresado);
                
            } 
            else if (tipo_comando == CMD_SET_TM) {
                
                char* tasa_ingresada = comando_local + 7;

                cli();
                intervalo_reporte = tm;
                contador_segundos = 0; 
                sei();

                tasa_configurada = true; // levanto bandera

                TERMINAL_SendString("-> Tasa de muestreo actualizada a ");
                TERMINAL_SendString(tasa_ingresada);
                TERMINAL_SendString(" seg.\r\n");
                
                
            } 
            else {
                // informo error: errores de tipeo, formatos inválidos o números fuera de rango 
                TERMINAL_SendString("-> ERROR: Comando no reconocido o valor fuera de rango.\r\n");
            }

            // VERIFICACIÓN DE INICIO: Si ambos parámetros están listos, arranca
            if (!sistema_activo && hora_configurada && tasa_configurada) {
                sistema_activo = true;
                TERMINAL_SendString("\r\n==========================================\r\n");
                TERMINAL_SendString(" CONFIGURACION COMPLETA - INICIANDO LECTURAS\r\n");
                TERMINAL_SendString("==========================================\r\n");
                
                cli();
                contador_segundos = 0;
                flag_muestreo = 0;
                sei();
            } else if (!sistema_activo) {
                TERMINAL_SendString("> "); // Prompt si falta configurar algo
            }
        }
        
        // ==============================================================
        // --- MUESTREO ---
        // ==============================================================
        if (flag_muestreo) {
            flag_muestreo = 0; // Bajamos la bandera inmediatamente
            
            if (sistema_activo) {
                DHT11_Status estado_dht = DHT11_Read(&datos_dht);
                RTC_GetTime(&h_actual, &m_actual, &s_actual);
                
                const char* estado_invernadero = "NORMAL";
                bool hay_alerta = false;
                static uint8_t contador_tramas_alerta = 0;
                char buffer_emergencia[128] = "";
                
                if (estado_dht == DHT11_OK) {
                    
                    uint8_t es_dia = (h_actual >= 7 && h_actual < 19);
                    
                    if (es_dia) { 

                        if (datos_dht.temperatura < 20 || datos_dht.temperatura > 30) {
                            hay_alerta = true;
                            estado_invernadero = "ALERTA";
                            sprintf(buffer_emergencia, "[ALERTA] [%02d:%02d:%02d] Temperatura fuera de rango diurno! Valor: %d C\r\n", 
                                    h_actual, m_actual, s_actual, datos_dht.temperatura);
                        } 
                        else if (datos_dht.humedad < 50 || datos_dht.humedad > 70) {
                            hay_alerta = true;
                            estado_invernadero = "ALERTA";
                            sprintf(buffer_emergencia, "[ALERTA] [%02d:%02d:%02d] Humedad fuera de rango diurno! Valor: %d %%\r\n", 
                                    h_actual, m_actual, s_actual, datos_dht.humedad);
                        }
                    } else { //es de noche

                        if (datos_dht.temperatura < 15 || datos_dht.temperatura > 22) {
                            hay_alerta = true;
                            estado_invernadero = "ALERTA";
                            sprintf(buffer_emergencia, "[ALERTA] [%02d:%02d:%02d] Temperatura fuera de rango nocturno! Valor: %d C\r\n", 
                                    h_actual, m_actual, s_actual, datos_dht.temperatura);
                        } 
                        else if (datos_dht.humedad < 60 || datos_dht.humedad > 80) {
                            hay_alerta = true;
                            estado_invernadero = "ALERTA";
                            sprintf(buffer_emergencia, "[ALERTA] [%02d:%02d:%02d] Humedad fuera de rango nocturno! Valor: %d %%\r\n", 
                                    h_actual, m_actual, s_actual, datos_dht.humedad);
                        }
                    }

                    
                    
                    sprintf(buffer_salida, "\r\n[%02d:%02d:%02d] T: %d C | H: %d %% | Estado: %s\r\n> ", 
                            h_actual, m_actual, s_actual, 
                            datos_dht.temperatura, datos_dht.humedad, estado_invernadero);
                } else {
                    sprintf(buffer_salida, "\r\n[%02d:%02d:%02d] ERROR LECTURA SENSOR DHT11\r\n> ", 
                            h_actual, m_actual, s_actual);
                }
                
                TERMINAL_SendString(buffer_salida);

                // si hay que imprimir la emergencia (cada 2 tramas)
                if (hay_alerta) {
                    contador_tramas_alerta++;
                    if (contador_tramas_alerta >= 2) {
                        TERMINAL_SendString(buffer_emergencia);
                        contador_tramas_alerta = 0; // Reiniciamos el contador
                    }
                } else {
                    // Si el clima volvió a la normalidad, reseteamos el contador de alertas
                    contador_tramas_alerta = 0; 
                }

                TERMINAL_SendString("> "); //imprimimos el prompt para el usuario

                //control para imprimir el comando del usuario a medio hacer (para no eliminarlo)
                char texto_en_progreso[32];
                
                // Copiamos rápido el comando a medio hacer
                cli();
                strcpy(texto_en_progreso, TERMINAL_ObtenerComando());
                sei();
                
                // Si el usuario ya había escrito al menos 1 letra, la reescribimos
                if (strlen(texto_en_progreso) > 0) {
                    TERMINAL_SendString(texto_en_progreso);
                }
            }
        }
    }

    return 0;
}