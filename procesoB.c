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

bool sonido = false;
int a = 0;
int PID = -1;

// Manejo de señales
void handleMotorSignals(int signal) {

	switch (signal) {
		case SIGUSR1:
			onOffCalefaccion(true);
			break;
		case SIGUSR2:
			onOffCalefaccion(false);
			break;
		default:
			printf("Unsupported signal is received!\n");
			break;
	}
	
}

void onOffCalefaccion(bool onOff) {

	if (onOff) {
		a = 1;
		kill(PID, SIGUSR2);
	
		digitalWrite(MOTOR_PIN, HIGH);
		digitalWrite(MOTOR_PIN2, HIGH);
		system("amixer set Master unmute > /dev/null &");
		
	} else {
		a = 0;
		kill(PID, SIGUSR1);
		digitalWrite(MOTOR_PIN, LOW);
		digitalWrite(MOTOR_PIN2, LOW);

		system("amixer set Master mute > /dev/null &");
	}	

	if (a == 1) {
		printf("Calefacción encendida\n");
	} else {
		printf("Calefacción apagada\n");
	}
}


int main(int argc, char *argv[]) {

	PID = atoi(argv[0]);

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

	system("mpg321 calefaccion.mp3 -l 0 -q > /dev/null &");
	system("amixer set Master mute > /dev/null &");

	signal(SIGUSR1, handleMotorSignals);
	signal(SIGUSR2, handleMotorSignals);

	while (1);

	return 0;
}