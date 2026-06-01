/*
 * Entregable.c
 *
 * Created: 10/4/2026 09:53:16
 * Author : W10
 */ 
#define F_CPU 16000000UL // Defino la frecuencia de oscilador en 8MHz
#include <avr/io.h>
#include <util/delay.h> // Retardos por software




#define NUM_LEDS 8 //numero de leds en la tira inteligente


int buffer_leds[NUM_LEDS][3]; //una fila por cada led y 3 columnas RGB
int leds[3];

// Pinta un LED específico en la tira
void set_pixel(uint8_t indice, uint8_t r, uint8_t g, uint8_t b) {
	if (indice < NUM_LEDS) { // Protección para no escribir fuera de la memoria
		buffer_leds[indice][0] = g; // Verde
		buffer_leds[indice][1] = r; // Rojo
		buffer_leds[indice][2] = b; // Azul
	}
}

// Apaga todos los LEDs en la tira
void limpiar_tira() {
	for(uint8_t i = 0; i < NUM_LEDS; i++) {
		set_pixel(i, 0, 0, 0);
	}
}


void write() {
	// recorre cada LED físico de la tira
	for (uint8_t led = 0; led < NUM_LEDS; led++)
	{
		// recorre los 3 colores (verde, rojo, azul) de ese LED
		for (uint8_t color = 0; color <= 2; color++)
		{
			// enviar 8 bits de ese color de MSB a  LSB
			for (int8_t bit = 7; bit >= 0; bit--)
			{
				if ((buffer_leds[led][color] & (1<<bit)) != 0)
				{
					// 1 LÓGICO 
					PORTB |= (1 << PORTB0);
					asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP"); // calculo: 12 ciclos aprox
					PORTB &= ~(1 << PORTB0);
					asm("NOP");asm("NOP");asm("NOP");
				}
				else
				{
					// 0 LÓGICO 
					PORTB |= (1 << PORTB0);
					asm("NOP");asm("NOP");asm("NOP"); //calculo: 8 ciclos
					PORTB &= ~(1 << PORTB0);
					asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
				}
			}
		}
	}
}

void leer_pulsadores(uint8_t *ptr_modo_diodos, uint8_t *ptr_modo_tira){
	
	// var estatica solo se inicializas la primera vez que entra a la funcion
	static uint8_t estado_anterior_pc0 = 1; 
	static uint8_t estado_anterior_pc1 = 1;
	
	// botón pc0 (para secuencia diodos)
	uint8_t lectura_pc0 = (PINC & (1<<PC0));
	if (lectura_pc0 == 0 && estado_anterior_pc0 != 0) {
		*ptr_modo_diodos = !(*ptr_modo_diodos); // invierte de 0 a 1, o de 1 a 0
	}
	estado_anterior_pc0 = lectura_pc0; // Actualizamos la memoria

	// boton pc1 (para secuencia tira leds)
	uint8_t lectura_pc1 = (PINC & (1<<PC1));
	if (lectura_pc1 == 0 && estado_anterior_pc1 != 0) {
		*ptr_modo_tira = !(*ptr_modo_tira);     // Invierte el modo
	}
	estado_anterior_pc1 = lectura_pc1;
}



int main(void)
{
  /* Setup */
  DDRD = 0xFF; //todos los pines del puerto D se configuran como salida
  DDRC &= ( ~(1<<PORTC0) & ~(1<<PORTC1) ); //configuro a los pines 0 y 1 del puerto C como entrada
  
  PORTC |= (1<<PORTC0);//encender resistencia interna del puerto C0
  PORTC |= (1<<PORTC1);//encender resistencia interna del puerto C1
  
 
  DDRB |= (1 << PORTB0); //pin 0 del puerto B como salida
  
  
  
  //variables necesarias para que cuando se pulse el boton se guarde el estado:
  uint8_t modo_diodos = 0; // 0 = Secuencia A, 1 = Secuencia B
  uint8_t modo_tira = 0;   // 0 = Secuencia C, 1 = Secuencia D
  
  
  //uso de pasos para controlar el encendido de cada led
  int8_t paso_diodo = 0; //esta variable va a ir incrementando hasta llegar a 7 despues se reincia (en secuencia A) o comienza a disminuir "rebota" (en secuencia B)
  int8_t dir_diodo = 1; // 1 sube, -1 baja
  
  int8_t paso_tira = 7; //funciona igual que paso diodo para controlar que led de la tira se esta encendiendo
  int8_t paso_tira_luces = 1; //funciona para controlar los estados de luces rojas y azules
  
   uint8_t vueltas_diodos = 0;
   uint8_t vueltas_tira = 0;

  while(1) {
	  
		  //lectura de pulsadores 
		  leer_pulsadores(&modo_diodos, &modo_tira);  
		
	  
		  //analizo secuencia leds diodos	  
		  // me fijo si ya pasaron 100ms desde el último cambio
		  if (vueltas_diodos >= 10) {
				  vueltas_diodos = 0; // actualizo contador de vueltas
			  
			  
				  if (modo_diodos == 0 ) 
				  {
					   //secuencia A: se enciende un bit a la vez desde el LSB hasta el MSB
					 
					   PORTD = (1 << paso_diodo);
					   paso_diodo++;
					   if (paso_diodo > 7) paso_diodo = 0; // reinicio el paso cuando llega el utlimo bit
				  }
				  else  { 
					   //secuenica b: enciende un LED a la vez empezando desde el MSB y rebotando en los extremos repetitivamente
					   PORTD = (1 << paso_diodo);
					   paso_diodo += dir_diodo;
				   
					   // Si toca un extremo, invierte la dirección de la suma
					   if (paso_diodo >= 7) { paso_diodo = 7; dir_diodo = -1; } //ahora paso diodo va a empezar a diminuir (dir diodo = -1)
					   else if (paso_diodo <= 0) { paso_diodo = 0; dir_diodo = 1; } //paso diodo se va a incrementar (dir diodo = +1)
				   
				  }
			  
			  }
	  
	  
		  
		  // me fijo si ya pasaron 150ms desde el último cambio
		  if (vueltas_tira>= 15) {
				  vueltas_tira = 0; // actualizo vueltas
			  
				  if (modo_tira == 0){ 
				  
					 // secuencia C: secuencia leds pares rojo e impares azul 
					 limpiar_tira();
					 if (paso_tira_luces == 0) {
						 // pares rojos
						 for(uint8_t i = 0; i < NUM_LEDS; i += 2) set_pixel(i, 255, 0, 0); //se envia solo color rojo
						 paso_tira_luces = 1; // preparo para que la proxima encienda azules primero
						 } else {
						 // impares azules
						 for(uint8_t i = 1; i < NUM_LEDS; i += 2) set_pixel(i, 0, 0, 255); //se envia solo color azul
						 paso_tira_luces = 0; //preparo para q la proxima encienda rojo primero
						 }
					 write();
			    
					  }
				    
				  else { 
						// secuencia D: verde moviéndose de derecha a izquierda
						limpiar_tira();
						set_pixel(paso_tira, 0, 255, 0);
						write();
					
						paso_tira--; // mueve el punto verde a la izquierda
						if (paso_tira < 0) paso_tira = NUM_LEDS - 1; // vuelve a empezar a la derecha
				   
				   }
			   
		}
		_delay_ms(10); //comentar
		vueltas_tira++; //incremento vueltas
		vueltas_diodos++; 
   }

	  
	  
	  
  /* Punto de finalizaci?n del programa (NO se debe llegar a este lugar) */
  return 0;
  }
  
  

