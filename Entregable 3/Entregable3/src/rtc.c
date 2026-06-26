#include "rtc.h"
#include <avr/io.h>

#define RTC_ADDR_WRITE 0xD0 // Dirección I2C del DS3231/DS3232 para escribir
#define RTC_ADDR_READ  0xD1 // Dirección I2C del DS3231/DS3232 para leer

// Funciones auxiliares para convertir Decimal a BCD y viceversa (el RTC usa BCD)
static uint8_t decToBcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

static uint8_t bcdToDec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void RTC_Init(void) {
    // Configurar I2C (TWI) a 100kHz con F_CPU = 16MHz
    TWSR = 0x00; // Prescaler = 1
    TWBR = 72;   // Bit Rate = 72
    TWCR = (1 << TWEN); // Habilitar TWI
}

void RTC_SetTime(uint8_t horas, uint8_t minutos, uint8_t segundos) {
    // 1. Iniciar transmisión
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 2. Enviar dirección del RTC en modo escritura
    TWDR = RTC_ADDR_WRITE;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 3. Enviar dirección del registro inicial (0x00 = Segundos)
    TWDR = 0x00;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 4. Escribir Segundos, Minutos y Horas (convertidos a BCD)
    TWDR = decToBcd(segundos);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = decToBcd(minutos);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    TWDR = decToBcd(horas);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 5. Detener transmisión
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void RTC_GetTime(uint8_t* horas, uint8_t* minutos, uint8_t* segundos) {
    // 1. Iniciar transmisión
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 2. Enviar dirección del RTC (Escritura para setear el puntero)
    TWDR = RTC_ADDR_WRITE;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 3. Apuntar al registro 0x00
    TWDR = 0x00;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 4. Reiniciar bus (Repeated Start) para cambiar a modo Lectura
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 5. Enviar dirección del RTC en modo lectura
    TWDR = RTC_ADDR_READ;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    // 6. Leer Segundos (y responder ACK)
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while (!(TWCR & (1 << TWINT)));
    *segundos = bcdToDec(TWDR & 0x7F);

    // 7. Leer Minutos (y responder ACK)
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while (!(TWCR & (1 << TWINT)));
    *minutos = bcdToDec(TWDR);

    // 8. Leer Horas (y responder NACK porque es el último byte)
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    *horas = bcdToDec(TWDR & 0x3F);

    // 9. Detener transmisión
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}