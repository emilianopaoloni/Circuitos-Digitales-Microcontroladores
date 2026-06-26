#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz de Arduino Uno
#endif

// Tamaño de los buffers (debe ser potencia de 2 preferentemente)
#define UART_RX_BUFFER_SIZE 64
#define UART_TX_BUFFER_SIZE 64

// Buffers circulares
static volatile char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

static volatile char tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;

void UART_Init(uint32_t baudrate) {
    uint16_t ubrr = (F_CPU / 16 / baudrate) - 1;
    
    // Configurar el Baud Rate
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    // Habilitar transmisor, receptor y la interrupción de recepción (RX)
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    
    // Formato de trama: 8 bits de datos, 1 bit de parada, sin paridad
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_SendChar(char c) {
    uint8_t next_head = (tx_head + 1) % UART_TX_BUFFER_SIZE;
    
    // Si el buffer está lleno, se queda esperando (bloqueante momentáneo)
    while (next_head == tx_tail); 
    
    tx_buffer[tx_head] = c;
    tx_head = next_head;
    
    // Habilitar la interrupción de buffer de datos de transmisión vacío (UDRE)
    UCSR0B |= (1 << UDRIE0);
}

void UART_SendString(const char* str) {
    while (*str) {
        UART_SendChar(*str++);
    }
}

bool UART_IsDataAvailable(void) {
    return (rx_head != rx_tail);
}

char UART_GetChar(void) {
    if (rx_head == rx_tail) return 0; // Buffer vacío
    
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;
    return c;
}

// ISR (Rutina de Servicio de Interrupción) de Recepción
ISR(USART_RX_vect) {
    char c = UDR0; // Leer hardware inmediatamente
    uint8_t next_head = (rx_head + 1) % UART_RX_BUFFER_SIZE;
    
    if (next_head != rx_tail) { // Si hay espacio
        rx_buffer[rx_head] = c;
        rx_head = next_head;
    }
}

// ISR de Transmisión (Data Register Empty)
ISR(USART_UDRE_vect) {
    if (tx_head != tx_tail) { // Si hay datos para enviar
        UDR0 = tx_buffer[tx_tail];
        tx_tail = (tx_tail + 1) % UART_TX_BUFFER_SIZE;
    } else {
        // Si ya enviamos todo, apagamos la interrupción
        UCSR0B &= ~(1 << UDRIE0);
    }
}