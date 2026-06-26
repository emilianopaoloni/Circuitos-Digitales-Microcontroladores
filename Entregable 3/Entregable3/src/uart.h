#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include <stdbool.h>

// Inicializa la UART a la velocidad deseada (ej. 9600)
void UART_Init(uint32_t baudrate);

// Envía un solo carácter a través del buffer
void UART_SendChar(char c);

// Envía una cadena de texto (string) completa
void UART_SendString(const char* str);

// Verifica si hay algún carácter esperando a ser leído en el buffer
bool UART_IsDataAvailable(void);

// Extrae y devuelve un carácter del buffer de recepción
char UART_GetChar(void);

#endif /* UART_H_ */