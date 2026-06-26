#include "dht11.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// Definiciones del pin según tu esquema (PC0)
#define DHT11_PORT PORTC
#define DHT11_DDR  DDRC
#define DHT11_PIN  PINC
#define DHT11_BIT  PC0

void DHT11_Init(void) {
    // Configurar como entrada con pull-up interno por si falla la resistencia externa
    DHT11_DDR &= ~(1 << DHT11_BIT);
    DHT11_PORT |= (1 << DHT11_BIT);
}

DHT11_Status DHT11_Read(DHT11_Data* data) {
    uint8_t bits[5] = {0, 0, 0, 0, 0};
    uint8_t i, j;
    uint16_t timeout_counter = 0;

    // 1. MCU envía señal de inicio (Start Signal)
    DHT11_DDR |= (1 << DHT11_BIT);   // Pin como salida
    DHT11_PORT &= ~(1 << DHT11_BIT); // Pin a LOW
    _delay_ms(18);                   // Esperar al menos 18ms
    
    DHT11_PORT |= (1 << DHT11_BIT);  // Pin a HIGH
    DHT11_DDR &= ~(1 << DHT11_BIT);  // Pin como entrada
    _delay_us(40);                   // Esperar respuesta del sensor

    // 2. Esperar respuesta del DHT11 (LOW por 80us)
    timeout_counter = 0;
    while ((DHT11_PIN & (1 << DHT11_BIT))) {
        _delay_us(1);
        if (++timeout_counter > 100) return DHT11_ERROR_TIMEOUT; // Falla: No está presente
    }

    // 3. Esperar a que el sensor suba la línea a HIGH (80us)
    timeout_counter = 0;
    while (!(DHT11_PIN & (1 << DHT11_BIT))) {
        _delay_us(1);
        if (++timeout_counter > 100) return DHT11_ERROR_TIMEOUT;
    }

    // 4. Esperar a que el sensor baje la línea de nuevo para empezar a transmitir
    timeout_counter = 0;
    while ((DHT11_PIN & (1 << DHT11_BIT))) {
        _delay_us(1);
        if (++timeout_counter > 100) return DHT11_ERROR_TIMEOUT;
    }

    // --- SECCIÓN CRÍTICA DE TIEMPO ---
    cli(); // Deshabilitar interrupciones para no perder bits por culpa de la UART
    
    // 5. Leer los 40 bits (5 bytes)
    for (j = 0; j < 5; j++) {
        for (i = 0; i < 8; i++) {
            // Esperar a que el pin pase a HIGH
            while (!(DHT11_PIN & (1 << DHT11_BIT)));
            
            _delay_us(30); // Esperar 30us. Si sigue HIGH, es un '1'. Si bajó a LOW, es un '0'.
            
            if (DHT11_PIN & (1 << DHT11_BIT)) {
                bits[j] |= (1 << (7 - i)); // Escribir un '1'
                // Esperar a que vuelva a bajar para el próximo bit
                while ((DHT11_PIN & (1 << DHT11_BIT)));
            }
        }
    }
    
    sei(); // Volver a habilitar interrupciones
    // --- FIN SECCIÓN CRÍTICA ---

    // 6. Verificar el Checksum
    if (bits[4] != ((bits[0] + bits[1] + bits[2] + bits[3]) & 0xFF)) {
        return DHT11_ERROR_CHECKSUM;
    }

    // 7. Guardar los datos útiles (solo parte entera para el DHT11 estándar)
    data->humedad = bits[0];
    data->temperatura = bits[2];

    return DHT11_OK;
}