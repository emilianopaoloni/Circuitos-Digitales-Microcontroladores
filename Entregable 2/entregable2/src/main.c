/*
 * practica2.c
 *
 * Created: 20/4/2026 15:20:21
 * Author : W10
 */ 


//#define F_CPU 16000000UL // 16MHz 
#include <avr/io.h>
#include "lcd.h"
#include <util/delay.h>
#include <avr/interrupt.h> // Necesario para la ISR y sei()

//MAQUINA DE ESTADOS:
typedef enum {
    STATE_IDLE,     // Estado 1: Reposo / Ingreso de datos
    STATE_COOKING,  // Estado 2: Cocinando (Cuenta regresiva)
    STATE_PAUSED,   // Estado 3: Pausado
    STATE_DONE      // Estado 4: Finalizado (Blink por 5 seg)
} eSystem_state;

const uint8_t teclado_ascii[4][4] = {
	{'1', '2', '3', 'A'}, // Fila 0
	{'4', '5', '6', 'B'}, // Fila 1
	{'7', '8', '9', 'C'}, // Fila 2
	{'*', '0', '#', 'D'}  // Fila 3
};  


// Variables globales volátiles para comunicar la interrupción con el main
volatile uint8_t Flag_MEF = 0;
volatile uint8_t cont_MEF = 0;
volatile uint8_t tecla_presionada_global = 0xFF; // NUEVA: Tecla detectada por ISR

// Frecuencia a la que se llamará la MEF (cada 100ms mediante el Timer)
#define TICKS_PER_SECOND 10  
#define DONE_BLINK_TIME (5 * TICKS_PER_SECOND) // 5 segundos

// Variables estáticas globales (Memoria de la MEF)
volatile eSystem_state System_state = STATE_IDLE; // Estado actual de la máquina
static uint16_t State_call_count = 0; // Para contar el tiempo (ticks)
static uint16_t Total_seconds = 0;    // Tiempo de cocción restante
static uint8_t Door_is_open = 0;      // 0 = Cerrada, 1 = Abierta

// variables para el ingreso de tiempo (teclado)
volatile uint8_t digitos[4] = {'0', '0', '0', '0'}; //almacena los ASCII de los digitos ingresados (M1,M0,S1,S0). Inicializa todo con los ascii de los 0
static uint8_t cont_ingreso = 0; // Va a ir de 0 a 4

//variable estática para detector de flanco en ISR
static uint8_t ultima_tecla_isr = 0xFF;




void KEYPAD_Init(void) {
	//funcion que se llama una sola vez en el main para configurar el keypad (puertos de entrada y de salida)
	
	// Configuramos las COLUMNAS (PD3, PD5, PD4, PD2) como ENTRADAS (0)
	DDRD &= ~((1<<PD3) | (1<<PD5) | (1<<PD4) | (1<<PD2));
	
	// Encendemos las resistencias PULL-UP de esas columnas (1)
	PORTD |= ((1<<PD3) | (1<<PD5) | (1<<PD4) | (1<<PD2));
	
	// Las FILAS arrancan como entradas en Alta Impedancia (Hi-Z)
	DDRB &= ~((1<<PB4) | (1<<PB3) | (1<<PB0));
	PORTB &= ~((1<<PB4) | (1<<PB3) | (1<<PB0));
	DDRD &= ~(1<<PD7);
	PORTD &= ~(1<<PD7);
}


// Retorna el caracter ASCII presionado o 0xFF si no hay nada presionado.
uint8_t KEYPAD_ReadRaw(void) {
	uint8_t r, c;
	
	for (r = 0; r < 4; r++) { //primero for: recorre todas las filas
		// Pone todas las filas en Alta Impedancia (Hi-Z) apagándolas
		DDRB &= ~((1<<PB4) | (1<<PB3) | (1<<PB0));
		DDRD &= ~(1<<PD7);
		
		// se activa solo la fila actual poniéndola como SALIDA en LOW (0)
		switch(r) {
			case 0: DDRB |= (1<<PB4); break; // Fila A: PB4
			case 1: DDRB |= (1<<PB3); break; // Fila B: PB3
			case 2: DDRB |= (1<<PB0); break; // Fila C: PB0
			case 3: DDRD |= (1<<PD7); break; // Fila D: PD7
		}
		
		
		// retardo para que la señal eléctrica se estabilice
		_delay_us(5);
		
		// se leen todas las columnas. Si alguna es 0, esa tecla se presionó.
		c = 4; // Valor por defecto (ninguna)
		if (!(PIND & (1<<PD3))) c = 0;      // Col 1: PD3
		else if (!(PIND & (1<<PD5))) c = 1; // Col 2: PD5
		else if (!(PIND & (1<<PD4))) c = 2; // Col 3: PD4
		else if (!(PIND & (1<<PD2))) c = 3; // Col 4: PD2
		
		// Si detectamos una columna presionada, devolvemos el ASCII
		if (c < 4) {
			return teclado_ascii[r][c];
		}
	}
	
	return 0xFF; // Terminó el barrido y no hay nada presionado
}

/*
//POR AHORA NO VOY A USAR ESTO
// Recibe un puntero, guarda el ASCII en ese ptr, y devuelve 1 si hay tecla nueva.
uint8_t KEYPAD_Scan(uint8_t *key) {
	// Variables estáticas para hacer memoria entre cada llamada a la función
	static uint8_t Old_key = 0xFF;
	static uint8_t Last_valid_key = 0xFF;
	
	uint8_t Key_Cruda = KEYPAD_ReadRaw();

	// Caso 1: El usuario soltó el teclado
	if (Key_Cruda == 0xFF) {
		Old_key = 0xFF;
		Last_valid_key = 0xFF;
		return 0; // No hay tecla
	}

	// Caso 2: Segunda verificación (Confirmamos que no fue ruido)
	if (Key_Cruda == Old_key) {
		// Evita detección múltiple si el usuario deja el dedo pegado
		if (Key_Cruda != Last_valid_key) {
			*key = Key_Cruda; // <--- Acá guardamos el ASCII en tu puntero
			Last_valid_key = Key_Cruda;
			return 1; // ¡Nueva tecla detectada con éxito!
		}
	}

	// Caso 3: Primera vez que detectamos la tecla (1ra verificación)
	Old_key = Key_Cruda;
	return 0; // Retornamos 0 porque todavía no estamos 100% seguros
} */
  
void DISPLAY_Show_Message(const char* msg) { //muestra un mensaje fijo en el LCD (lo uso para "00:00")
	LCDGotoXY(0,0);
	while (*msg) {
		LCDsendChar(*msg++);
	}
} 

//funcion dedicada a mostrar digitos en el display durante el estado 1 (ingreso de tiempo)
void DISPLAY_Update(){
	
    LCDGotoXY(0, 0); //poner al cursor al inicio de la primera línea
    
    LCDsendChar(digitos[0]); // Imprime el primer minuto
    LCDsendChar(digitos[1]); // Imprime el segundo minuto
    
    LCDsendChar(':'); // Imprimimos los dos puntos fijos en el medio
    
    //Imprimimos los segundos
    LCDsendChar(digitos[2]); 
    LCDsendChar(digitos[3]); 
}

// Función para actualizar el display durante el estado 2 
// Recibe el tiempo total en segundos y lo muestra como MM:SS
void DISPLAY_Update_Time(uint16_t t_sec) {

    //calculo de minutos y segundos a partir del "total segundos"
    uint8_t minutos = t_sec / 60;  // División entera para obtener minutos 
    uint8_t segundos = t_sec % 60; // El resto de la división son los segundos 

    //cursor al inicio
    LCDGotoXY(0, 0); 

    //descomponemos e imprimimos los MINUTOS
    // Convertimos cada dígito a ASCII sumando '0' (48 en decimal) 
    LCDsendChar((minutos / 10) + '0'); // Decena de minuto
    LCDsendChar((minutos % 10) + '0'); // Unidad de minuto

    LCDsendChar(':');

    //descomponemos e imprimimos los SEGUNDOS
    LCDsendChar((segundos / 10) + '0'); // Decena de segundo
    LCDsendChar((segundos % 10) + '0'); // Unidad de segundo
}

void DISPLAY_Toggle_Blink(void) {
    // Variable estática que recuerda el estado entre cada llamada
    // 0 = Pantalla visible, 1 = Pantalla apagada
    static uint8_t is_blank = 0; 

    if (is_blank == 0) {
        LCDblank();       // Apagamos la pantalla
        is_blank = 1;     // Actualizamos el estado
    } else {
        LCDvisible();     // Prendemos la pantalla
        is_blank = 0;     // Actualizamos el estado
    }
}


//funciones para encender/apagar los leds:

// Control de la Luz Interior (Conectada en PC4)
void LED_Magnetron(uint8_t prender) { //solo se enciende cuando el microonda emite radiacion (en coccion)
    if (prender) {
        PORTB |= (1 << PB5); //prender=1 prende el led
    } else {
        PORTB &= ~(1 << PB5);
    }
}


void LED_Violeta(uint8_t prender) {
    if (prender == 1) {
        PORTC |= (1 << PC5);  // prender=1 prende el LED
    } 
    else if (prender == 0) {
        PORTC &= ~(1 << PC5); // prender=0 apaga el LED
    } 
    else if (prender == 2) {
        PORTC ^= (1 << PC5);  // prender=2: Toggle invierte el estado actual
    }
}


void LED_Verde(uint8_t prender) { //esta encendido en todo el proceso, cuando esta la puerta abierta o el proceso en pausa tambien
    if (prender) {
        PORTC |= (1 << PC4); //prender=1 prende el led
    } else {
        PORTC &= ~(1 << PC4);
    }
}
//--------------------------------------------------
//funciones para la maquina de estados: ------------

// INICIALIZACIÓN DEL SISTEMA

void MICROWAVE_Init(void) {
    _delay_ms(50);
    
    System_state = STATE_IDLE;
    Total_seconds = 0;
    Door_is_open = 0;

    //Inicializar periféricos
    LCDinit(); 
    KEYPAD_Init(); 
    
    //configuracion de leds:

    // LED Magnetrón amarillos encedido durante la cocción (en PB5)
    DDRB |= (1 << PC5); //pin configurado como salida
    PORTB &= ~(1 << PC5); //arranca el LED apagado (poniendo un 0 en el PORTB)

    // LED verde encedido durante el proceso(en PC4)
    DDRC |= (1 << PC4); //pin configurado como salida
    PORTC &= ~(1 << PC4); //arranca el LED apagado (poniendo un 0 en el PORTC)

    
    // LED violeta parpadea para indicar fin del proceso(en PC5) 
    DDRC |= (1 << PC5); //pin configurado como salida
    PORTC &= ~(1 << PC5); //arranca el LED apagado (poniendo un 0 en el PORTC)


    //Mostrar estado inicial en pantalla
    DISPLAY_Show_Message("00:00"); 
}

// ACTUALIZACIÓN DE LA MÁQUINA DE ESTADOS (Se llama periódicamente, cada 100ms)
void MICROWAVE_Update(void) {

    State_call_count++; // Incrementamos el contador de tiempo
    
    // --- uso de tecla detectada por el isr:
    uint8_t tecla_presionada = tecla_presionada_global;
    tecla_presionada_global = 0xFF; // Limpiar la tecla para la próxima vez
    

   
   // Lógica global: Tecla 'D' simula apertura/cierre de puerta
   if (tecla_presionada == 'D') {
		Door_is_open = !Door_is_open; // Toggle estado de la puerta
   }
   

    // Evaluación de estados --> maquina de estados
    switch (System_state) {
        

        // ESTADO 1: REPOSO
        // ---------------------------------------------------------
        case STATE_IDLE:

            //Asegurar que los leds estén apagados
            LED_Magnetron(0);
            LED_Verde(0);
            LED_Violeta(0);

            if (tecla_presionada != 0xFF) { // Si se presionó una tecla

                if (tecla_presionada >= '0' && tecla_presionada <= '9') {
					
					if (cont_ingreso < 4) { // Solo permitimos 4 pulsaciones
                        digitos[cont_ingreso] = tecla_presionada; // Guardamos el ascii
                        cont_ingreso++; 
                        
   						DISPLAY_Update(); // actualiza el display con los dígitos ingresados
                    }
                  
                } 
                else if (tecla_presionada == 'B') { // CLEAR
                    Total_seconds = 0;
					cont_ingreso = 0;        //reinicia el contador de digitos ingresado
					digitos[0] = '0';          //limpiamos la memoria visual
					digitos[1] = '0'; 
					digitos[2] = '0'; 
					digitos[3] = '0';
                    DISPLAY_Show_Message("00:00");
                } 
                else if (tecla_presionada == 'C') { // +30 Segundos
                    Total_seconds += 30;

                    //actualizar arreglo digitos:
                    uint8_t minutos = Total_seconds / 60;
                    uint8_t segundos = Total_seconds % 60;
                    digitos[0] = (minutos / 10) + '0';
                    digitos[1] = (minutos % 10) + '0';
                    digitos[2] = (segundos / 10) + '0';
                    digitos[3] = (segundos % 10) + '0';
                    
                    DISPLAY_Update_Time(Total_seconds);
                }
                else if (tecla_presionada == 'A') { // START
					uint16_t minutos = ((digitos[0] - '0') * 10) + (digitos[1] - '0'); // Convertimos los ASCII a números
					uint16_t segundos = ((digitos[2] - '0') * 10) + (digitos[3] - '0');
					
					Total_seconds = (minutos * 60) + segundos;

					if (Total_seconds > 0 && !Door_is_open) {
						System_state = STATE_COOKING;
						State_call_count = 0;
					}
                    
                }
            }
            break;

        
        // ESTADO 2: COCINANDO
        // ---------------------------------------------------------
        case STATE_COOKING:
            // encender leds magnetron y verde
             LED_Magnetron(1);
             LED_Verde(1);
             LED_Violeta(0);

            // Condición de transición de emergencia: Abren la puerta
            if (Door_is_open) {
                System_state = STATE_PAUSED;
                break;
            }

            // Manejo del tiempo: Si pasó 1 segundo real (State_call_count llega a 10
            if (State_call_count >= TICKS_PER_SECOND) { //TICKS_PER_SECOND=10
                State_call_count = 0; //reinicio
                Total_seconds--;
                DISPLAY_Update_Time(Total_seconds);

                if (Total_seconds == 0) { // Terminó el tiempo
                    System_state = STATE_DONE;
                    State_call_count = 0;
                    break;
                }
            }

            // Eventos de teclado durante estado2
            if (tecla_presionada == 'B') { // STOP -> Pausa
                System_state = STATE_PAUSED;
            } 

            else if (tecla_presionada == 'C') { // +30 Segundos 
                Total_seconds += 30;
                //actualizar arreglo digitos:
                uint8_t minutos = Total_seconds / 60;
                uint8_t segundos = Total_seconds % 60;
                digitos[0] = (minutos / 10) + '0';
                digitos[1] = (minutos % 10) + '0';
                digitos[2] = (segundos / 10) + '0';
                digitos[3] = (segundos % 10) + '0';
                DISPLAY_Update_Time(Total_seconds);
            }
            break;

            
        // ESTADO 3: PAUSA
        // ---------------------------------------------------------
        case STATE_PAUSED:
            LED_Magnetron(0); //proceso de coccion pausado
            LED_Verde(1); //el led verde sigue encendido
            
            
            // Eventos de teclado durante pausa
            if (tecla_presionada == 'B') { // STOP/CLEAR (2da vez) -> vuelve a estado1
                Total_seconds = 0;
                cont_ingreso = 0;          //reiniciamos el contador para poder ingresar nuevos digitos
                digitos[0] = '0';          //limpiamos la memoria visual
				digitos[1] = '0'; 
				digitos[2] = '0'; 
				digitos[3] = '0';
                System_state = STATE_IDLE;
                DISPLAY_Show_Message("00:00");
            } 
            else if (tecla_presionada == 'A') { // START -> Reanuda
                if (!Door_is_open) {
                    System_state = STATE_COOKING;
                    State_call_count = 0; // Reiniciar para no comerse una fracción de segundo
                }
            }
            break;

            
        // ESTADO 4: FINALIZADO
        // ---------------------------------------------------------
        case STATE_DONE:
            LED_Magnetron(0); //finaliza el proceso de coccion
            LED_Verde(0); //finaliza el proceso
            
            // Lógica para parpadear usando el contador de ticks
            // Ejemplo: cambiar estado del display/alarma cada medio segundo (5 ticks)
            if (State_call_count % (TICKS_PER_SECOND / 2) == 0) { //para q titile cada medio seg
                DISPLAY_Toggle_Blink();
                LED_Violeta(2); //toggle del led violeta
            }

           // Condición de salida: pasaron 5 segundos
            if (State_call_count >= DONE_BLINK_TIME) {
                LED_Violeta(0);  //APAGAR LED violeta
                
                LCDvisible(); // Forzamos a que quede prendida sí o sí
                
                Total_seconds = 0;
                digitos[0] = '0';          //limpiamos la memoria visual
				digitos[1] = '0'; 
				digitos[2] = '0'; 
				digitos[3] = '0';
                cont_ingreso = 0; //reinicio para que se puedan volver a ingresar valores
                System_state = STATE_IDLE;
                DISPLAY_Show_Message("00:00");
            }
            
            // CONSIDERACION DEL GRUPO: si durante los 5 segundos de titileo se presiona CLEAR o se abre la puerta, se vuelve a estado 1 y finaliza el titileo
            if (tecla_presionada == 'B' || tecla_presionada == 'D') {
                 LED_Violeta(0);;  //APAGAR LED violeta

                 LCDvisible(); // Forzamos a que quede prendida sí o sí

                 Total_seconds = 0;
                 digitos[0] = '0';          //limpiamos la memoria visual
				 digitos[1] = '0'; 
				 digitos[2] = '0'; 
				 digitos[3] = '0';
                 cont_ingreso = 0; //reinicio para que se puedan volver a ingresar valores
                 System_state = STATE_IDLE;
                 DISPLAY_Show_Message("00:00");
            }
            break;
    }
}



// CONFIGURACIÓN DEL TIMER0 (interrumpe cada 10ms)
// ---------------------------------------------------------
void TIMER0_Init(void) {
    // Configurar Timer0 en modo CTC (Clear Timer on Compare Match)
    TCCR0A = (1 << WGM01);
    
    // Configurar el preescalador en 1024
    TCCR0B = (1 << CS02) | (1 << CS00);
    
    // Establecer el tope de conteo para 10ms (16MHz / (1024 * 100Hz) - 1 = 155.3)
	// debemos hacerlo cada 10 ms, porque si lo queremos hacer cada 100 ms, la cuenta nos da (16MHz / (1024 ) - 1 = 1561.5 y el timer es de 8 bits (el registro maximo para contar es de 255)
    OCR0A = 156; //ES 155, solo pruebo con 156
    
    // Habilitar la interrupción por comparación A
    TIMSK0 = (1 << OCIE0A);
}


// RUTINA DE SERVICIO DE INTERRUPCIÓN (se ejecuta por hardware cada 10ms)
// ---------------------------------------------------------
ISR(TIMER0_COMPA_vect) {
    // se lee el keypad cada 10 ns
    uint8_t tecla_cruda = KEYPAD_ReadRaw();
    
    // Detector de flanco: detectar presión de tecla (0xFF -> tecla)
    if (tecla_cruda != 0xFF && ultima_tecla_isr == 0xFF) {
        tecla_presionada_global = tecla_cruda; // Nueva tecla detectada
    }
    ultima_tecla_isr = tecla_cruda; // guardar estado actual
    
    cont_MEF++;
    if (cont_MEF >= 10) {     // Si pasaron 10 * 10ms = 100ms --> pasaron 100ms
        Flag_MEF = 1;         // Levantamos la bandera
        cont_MEF = 0;         // Reiniciamos el contador de la ISR
    }
}



int main(void) {
    
    // Inicializamos hardware, máquina de estados y display
    MICROWAVE_Init();
    
    // Inicializamos el Timer que va a marcar el ritmo
    TIMER0_Init();
    
    // Habilitamos las interrupciones globales
    sei(); 

    while(1) {
        
        //entra al 'if' cuando el Timer0 levantó la bandera (cada 100ms)
        if (Flag_MEF) {
            Flag_MEF = 0; // Bajamos la bandera inmediatamente
            
            // Ejecutamos la lógica del microondas
            MICROWAVE_Update(); 
        }
        
    }
}


