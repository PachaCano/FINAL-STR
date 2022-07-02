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

#define MOTOR_PIN 7 // 7

void encenderCalefaccion() {

    digitalWrite(MOTOR_PIN, HIGH);
    system("mpg321 calefaccion.mp3 --quiet &");

    //Escribir en memoria compartida un 1
}

void apagarCalefaccion() {

    digitalWrite(MOTOR_PIN, LOW);
    system("mpg321 calefaccion.mp3 --quiet -R STOP");
    //Escribir en memoria compartida un 0

}

//Manejar señales para encender y apagar

int main() {

    //Establecemos modo del pin
    pinMode(MOTOR_PIN, OUTPUT);

    //Inicializamos el motor
    digitalWrite(MOTOR_PIN, LOW);

}