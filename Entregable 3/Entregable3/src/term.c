#include <avr/io.h>
#include <avr/interrupt.h>
#include "term.h"

#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz de Arduino Uno
#endif

volatile char BufferRX[RX_BUFFER_SIZE];
volatile uint8_t IndexRX = 0;
volatile uint8_t flag_comando_listo = 0;
volatile char RX_Data = 0; //comunicacion isr



void TERMINAL_Init(uint32_t baudrate) {
    // Configurar el Baud Rate (9600 bps @ 16MHz):
    //UBRR0L = 103;
    uint16_t ubrr = (F_CPU / 16 / baudrate) - 1;

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;

    // Configurar formato: 8 bits de datos, Sin paridad (N), 1 bit de parada (8N1)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);//8N1

    // Activar Transmisor, Receptor y la Interrupción local de Recepción (RXCIE0)
    UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);
    
}

void TERMINAL_SendString(const char* str) {
    uint8_t i = 0;
    
    // Recorremos el string hasta encontrar el caracter nulo '\0'
    while (str[i] != '\0') { 
        // Esperamos a que el registro de transmisión esté vacío (UDRE0 = 1)
        while (!(UCSR0A & (1 << UDRE0))); 
        
        // Enviamos el caracter actual y avanzamos al siguiente
        UDR0 = str[i];
        i++;
    }
}

uint8_t TERMINAL_HayComandoNuevo(void) {
    // El main usa esto para saber si la interrupción ya armó una oración
    return flag_comando_listo;
}

char* TERMINAL_ObtenerComando(void) {
    // Le devolvemos al main un puntero a nuestro buffer privado
    return (char*)BufferRX;
}

void TERMINAL_LimpiarComando(void) {
    // El main llama a esta función cuando ya terminó de procesar el texto
    flag_comando_listo = 0;
    BufferRX[0] = '\0'; //pongo caracter nulo a la posicion cero para borrar visualmente al comando viejo
}


ISR(USART_RX_vect) {
    RX_Data = UDR0; // Leemos el dato inmediatamente

    //manejo del borrado --> que lo borre del buffer
    if (RX_Data == '\b' || RX_Data == 0x7F) {
        if (IndexRX > 0) {
            IndexRX--; // Retrocedemos un paso en nuestro arreglo de memoria
            
            // Magia visual: Borramos la letra en la pantallita de Proteus
            // (Enviamos un retroceso, un espacio en blanco para tapar la letra, y otro retroceso)
            while (!(UCSR0A & (1 << UDRE0)));
            UDR0 = '\b';
            while (!(UCSR0A & (1 << UDRE0)));
            UDR0 = ' ';
            while (!(UCSR0A & (1 << UDRE0)));
            UDR0 = '\b';
        }
        return; // Terminamos la interrupción acá, no guardamos nada
    }

    // Si llega un Enter (ya sea \r o \n)
    if (RX_Data == '\r' || RX_Data == '\n') {
        
        // Solo levantamos la bandera si el usuario escribió al menos una letra
        // Esto evita que un \r\n seguido levante la bandera dos veces seguidas
        if (IndexRX > 0) {
            BufferRX[IndexRX] = '\0'; // Cerramos el string
            IndexRX = 0;              // Reseteamos el índice
            flag_comando_listo = 1;   
        }
        
    }
    else { 
        // ECO POR HARDWARE 
        while (! (UCSR0A & (1<<UDRE0))); //esperar a que UDR0 este vacio
        UDR0 = RX_Data;                  //retransmitir
        
        // GUARDAR EN MEMORIA 
        // Solo guardamos si hay espacio en el arreglo 
        if (IndexRX < (RX_BUFFER_SIZE - 1)) {
            BufferRX[IndexRX++] = RX_Data;
            BufferRX[IndexRX] = '\0';
        }
    }
}
