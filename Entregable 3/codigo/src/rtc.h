#ifndef RTC_H_
#define RTC_H_

#include <stdint.h>

// Inicializa el bus I2C para hablar con el RTC
void RTC_Init(void);

// Setea la hora en el RTC (formato 24hs)
void RTC_SetTime(uint8_t horas, uint8_t minutos, uint8_t segundos);

// Lee la hora actual del RTC
void RTC_GetTime(uint8_t* horas, uint8_t* minutos, uint8_t* segundos);

#endif /* RTC_H_ */