#ifndef DHT11_H_
#define DHT11_H_

#include <stdint.h>

// Códigos de estado para cumplir con el manejo de errores
typedef enum {
    DHT11_OK = 0,
    DHT11_ERROR_TIMEOUT,  // El sensor no responde o está desconectado
    DHT11_ERROR_CHECKSUM  // Los datos se leyeron, pero están corruptos
} DHT11_Status;

// Estructura para almacenar los datos limpios
typedef struct {
    uint8_t temperatura;
    uint8_t humedad;
} DHT11_Data;

// Inicializa el pin del sensor
void DHT11_Init(void);

// Intenta leer el sensor y guarda los datos en la estructura pasada por referencia
DHT11_Status DHT11_Read(DHT11_Data* data);

#endif /* DHT11_H_ */