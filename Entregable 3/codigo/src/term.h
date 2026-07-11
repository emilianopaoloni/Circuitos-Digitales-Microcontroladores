#ifndef UART_H_
#define UART_H_

#include <stdint.h>

// Definiciones útiles
#define RX_BUFFER_SIZE 32  // Tamaño máximo del comando esperado

// Prototipos de funciones públicas (Lo que el main puede usar)
void TERMINAL_Init(uint32_t baudrate);
void TERMINAL_SendString(const char* str);
uint8_t TERMINAL_HayComandoNuevo(void);
char* TERMINAL_ObtenerComando(void);
void TERMINAL_LimpiarComando(void);

#endif /* UART_H_ */