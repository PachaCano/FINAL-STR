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

#define MOTOR_PIN 23 // 33
#define MOTOR_PIN2 24 // 35
#define MC_SIZE sizeof(int)

int memoria_comp = -1;
int* ptrMemComp = -1;

// Manejo de señales
void handleMotorSignals(int signal) {

	switch (signal) {
		case SIGUSR1:
			printf("Señal para encender calefacción!\n");
			onOffCalefaccion(true);
			break;
		case SIGUSR2:
			printf("Señal para apagar calefacción!\n");
			onOffCalefaccion(false);
			break;
		default:
			printf("Unsupported signal is received!\n");
			break;
	}
	
	
}

void onOffCalefaccion(bool onOff) {
	printf("%s calefacción\n", onOff ? "Encendiendo" : "Apagando");

	if (onOff) {
		digitalWrite(MOTOR_PIN, HIGH);
		digitalWrite(MOTOR_PIN2, HIGH);
		printf("Calefacción HIGH\n");
		//system("mpg321 calefaccion.mp3 --quiet &");
	} else {
		digitalWrite(MOTOR_PIN, LOW);
		digitalWrite(MOTOR_PIN2, LOW);
		printf("Calefacción LOW\n");
		//system("mpg321 calefaccion.mp3 --quiet -R STOP");
	}	
	
	// memoriaCompartida();
	// if (*ptrMemComp == 1) {
	// 	printf("Calefacción encendida\n");
	// } else {
	// 	printf("Calefacción apagada\n");
	// }
}

void memoriaCompartida() {

	// Accedemos a la memoria compartida en modo lectura
	memoria_comp = shm_open("/shm0", O_RDWR, 0600);

	if (memoria_comp < 0)
	{
		fprintf(stderr, "ERROR: Falló abrir la memoria compartida: %s\n", strerror(errno));
		return -1;
	}

	void *map = mmap(0, MC_SIZE, PROT_READ, MAP_SHARED, memoria_comp, 0);

	if (map == MAP_FAILED) {
		fprintf(stderr, "ERROR: Mapeo fallido: %s\n", strerror(errno));
		return -1;
	}

	*ptrMemComp = (int*) map;

	if (shm_unlink("/shm0") < 0) {
		fprintf(stderr, "ERROR: Falló el cierre de la memoria compartida: %s\n", strerror(errno));
		exit(1);
	}
	
}

int main() {

	int res = wiringPiSetup();

	if (res < 0) {
		printf("ERROR: Falló la inicialización de wiringPi\n");
		return -1;
	}

	printf("Proceso B iniciado\n");

	// Establecemos modo del pin
	pinMode(MOTOR_PIN, OUTPUT);
	pinMode(MOTOR_PIN2, OUTPUT);

	// Inicializamos el motor
	digitalWrite(MOTOR_PIN2, LOW);

	signal(SIGUSR1, handleMotorSignals);
	signal(SIGUSR2, handleMotorSignals);

	while (1);

	return 0;
}