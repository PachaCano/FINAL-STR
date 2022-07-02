#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>

#define MOTOR_PIN 7 // 7
#define MC_SIZE sizeof(int)

int memoria_comp = -1;

//Manejo de señales
void handle_user_signals(int signal) {
  switch (signal) {
    case SIGUSR1:
      printf("Señal para en,cender calefacción!\n");
      encenderCalefaccion();
      break;
    case SIGUSR2:
      printf("Señal para apagar calefacción!\n");
      apagarCalefaccion();
      break;
    default:
      printf("Unsupported signal is received!\n");
  }
}

void encenderCalefaccion() {

    digitalWrite(MOTOR_PIN, HIGH);
    system("mpg321 calefaccion.mp3 --quiet &");

    //Escribir en memoria compartida un 1
    *mem = memoria_comp();
    *mem = 1;

}

void apagarCalefaccion() {

    digitalWrite(MOTOR_PIN, LOW);
    system("mpg321 calefaccion.mp3 --quiet -R STOP");

    //Escribir en memoria compartida un 0
    *mem = memoria_comp();
    *mem = 0;
}

*int memoriaCompartida() {

    //Creamos la memoria compartida
    memoria_comp = shm_open("/shm0", O_CREAT | O_RDWR, 0600);

    if (memoria_comp < 0) {
		fprintf(stderr, "ERROR: Falló crear la memoria compartida: %s\n", strerror(errno));
		return -1;
	}

    fprintf(stdout, "La memoria compartida se creó con el FD: %d\n", memoria_comp);

    if (ftruncate(memoria_comp, MC_SIZE) < 0) {   // Acá no se si iría el mc_size, no haría falta
		fprintf(stderr, "ERROR: Truncation failed: %s\n", strerror(errno));
		return -1;
	}

    void* map = mmap(0, MC_SIZE, PROT_WRITE, MAP_SHARED, memoria_comp, 0);
	
	if (map == MAP_FAILED) {
		fprintf(stderr, "ERROR: Mapeo fallido: %s\n", strerror(errno));
		return -1;
	}

    int* ptr = (int*) map;
	*ptr = -1; //esto para qué se hace?

    return *ptr;
}

//Manejar señales para encender y apagar

int main() {

    //Establecemos modo del pin
    pinMode(MOTOR_PIN, OUTPUT);

    //Inicializamos el motor
    digitalWrite(MOTOR_PIN, LOW);

    signal(SIGUSR1, handle_user_signals);
    signal(SIGUSR2, handle_user_signals);

    //Para enviar las señales, el proceso A debe ejecutar: kill -SIGUSR1 ‘PID’ o kill -SIGUSR2 ‘PID’

}