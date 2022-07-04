// Proceso encargado de leer y modificar la temperatura

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <stdbool.h>
#include <pthread.h>
#include "bme280.h"
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define TS 100000000 // nanosegundos.   // = 100 ms
#define LIM_MAX_TEMP 30
#define LIM_MIN_TEMP 15

#define PULSER_PIN_PLUS 5 // 18
#define PULSER_PIN_MINUS 2 // 13
#define LED_PIN_RED 1  // 12
#define LED_PIN_BLUE 4  // 16

int fixedTemp = 20; // Temperatura que variará según los pulsadores.
float tempActual = 0; // Temperatura actual.

pthread_mutex_t mtx;
int PID = -1;
int PID_PADRE = -1;

int FD, ticks = 0, ticks2 = 0, ticks3 = 0;
bool flag = false;
bool flag2 = false;
bool onOff = false;

// Controlamos que los ticks 2 y 3 sólo aumenten cuando los pulsadores estén pulsados
void isrTimer (int sig, siginfo_t *si, void *uc) {
    ticks++;
    if (flag) 
        ticks2++;
    if (flag2) 
        ticks3++;
}

void handleSignals(int signal) {

	switch (signal) {
		case SIGUSR1:
            onOff = false;
			break;
		case SIGUSR2:
            onOff = true;
			break;
		default:
			printf("Unsupported signal is received!\n");
			break;
	}
	
}

// Hilo que lee temperatura
void threadTemp() {

    bme280_calib_data cal;
    bme280_raw_data raw;

    while (1) {
        if (ticks == 50) {  // Cada 5 segundos
            
            // Calibración de la temperatura
            readCalibrationData(FD, &cal);

            // Setea la frecuencia y modo de SAMPLING
            wiringPiI2CWriteReg8(FD, 0xf2, 0x01); // Registro sensor ctrl_hum
            wiringPiI2CWriteReg8(FD, 0xf4, 0x25); // Registro sensor ctrl_meas

            // Obtención de los datos crudos
            getRawData(FD, &raw);

            // Calculo de la temperatura EN CELSIUS
            int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);
            tempActual = compensateTemperature(t_fine); 

            // Dependiendo de la temperatura actual, se mandará una señal al proceso B
            powerOnOffMotor();

            printf("************ TEMPERATURA:%.2f \t FIXED_TEMP %d ************\n", tempActual, fixedTemp);
            ticks = 0;
        }
    }

}

void threadPlusTemp(void *arg) {

    bool pulsado = false;
    int pulsador = 0;
    int* ptrFixedTemp = (int*) arg;

    while (1) {
        pulsador = digitalRead(PULSER_PIN_PLUS);

        // Si no estaba pulsado (antes), pero ahora sí
        if (pulsado == false && pulsador == 1) {

                pthread_mutex_lock(&mtx);

                if ((*ptrFixedTemp) < LIM_MAX_TEMP) {
                    (*ptrFixedTemp)++;
                    printf("temperatura: %d\n", *ptrFixedTemp);
                } else {
                    printf("SE LLEGO AL LIMITE SUPERIOR\n");
                    digitalWrite(LED_PIN_RED, HIGH);
                    flag = true;
                    while (1) {
                        //Delay de 1000 ms para hacer parpadear el led
                        if (ticks2 == 10) {
                            ticks2 = 0;
                            flag = false;
                            digitalWrite(LED_PIN_RED, LOW);
                            break;
                        }
                    }
                }
                
                pthread_mutex_unlock(&mtx);
                pulsado = true;
                flag = true;

                //Delay de 100 ms para que, si se pulsa 2 veces en este intervalo, sólo tome uno
                while (1) {
                        if (ticks2 == 1) {
                            ticks2 = 0;
                            flag = false;
                            break;
                        }
                    } 

        // Si estaba pulsado (antes), y ya no
        } else if (pulsado == true && pulsador == 0) 
            pulsado = false;
        
    }
}

void threadMinusTemp(void *arg) {

    bool pulsado = false;
    int pulsador = 0;
    int* ptrFixedTemp = (int*) arg;

    while (1) {
        pulsador = digitalRead(PULSER_PIN_MINUS);

        if (pulsado == false && pulsador == 1) {

                pthread_mutex_lock(&mtx);
                if ((*ptrFixedTemp) > LIM_MIN_TEMP) {
                    (*ptrFixedTemp)--;
                    printf("temperatura: %d\n", *ptrFixedTemp);
                } else {
                    printf("LIMITE INFERIOR\n");
                    digitalWrite(LED_PIN_BLUE, HIGH);
                    flag2 = true;
                    while (1) {
                        if (ticks3 == 10) {
                            ticks3 = 0;
                            flag2 = false;
                            digitalWrite(LED_PIN_BLUE, LOW);
                            break;
                        }
                    }
                }
                
                pthread_mutex_unlock(&mtx);
                pulsado = true;
                flag2 = true;
                while (1) {
                        if (ticks3 == 1) {
                            ticks3 = 0;
                            flag2 = false;
                            break;
                        }
                    }
            
        } else if (pulsado == true && pulsador == 0) 
            pulsado = false;
        
    }
}

void powerOnOffMotor() {
    if (tempActual > fixedTemp) {   // Si la temperatura es menor a la fijada, se enciende el motor, es decir, se enciende la calefacción.
        if (onOff == true) 
            kill(PID, SIGUSR2);     // Señal para apagar la calefacción.
    } else {                        // Si la temperatura es mayor a la fijada, se apaga el motor, es decir, se apaga la calefacción.
        if (onOff == false)
            kill(PID, SIGUSR1);     // Señal para encender la calefacción.
    }
}

int main() {

    PID_PADRE = getpid();

    FD = wiringPiI2CSetup(BME280_ADDRESS);

    if (FD < 0) {
        printf("Error al iniciar la interfaz I2C.\n");
        return -1;
    }

    int res = wiringPiSetup();

    if (res) {
        printf("No se pudo inicializar la raspi");
        return 1;
    }

    // Seteamos el modo de los pines
    pinMode(PULSER_PIN_MINUS, INPUT);
    pinMode(PULSER_PIN_PLUS, INPUT);
    pinMode(LED_PIN_BLUE, OUTPUT);
    pinMode(LED_PIN_RED, OUTPUT);

    // Inicializamos los leds 
    digitalWrite(LED_PIN_BLUE, LOW);
    digitalWrite(LED_PIN_RED, LOW);

    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = isrTimer;

    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        printf("error sigaction");

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
        printf("error timer_create");

    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = TS;

    if (timer_settime(timerid, 0, &its, NULL) == -1)
        printf("error timer_settime");

    PID = fork();

    signal(SIGUSR1, handleSignals);
    signal(SIGUSR2, handleSignals);

    if (PID != 0) { // PADRE -> 

        pthread_t thread1;
        pthread_t thread2;
        pthread_t thread3;

        pthread_mutex_init(&mtx, NULL);

        int result1 = pthread_create(&thread1, NULL, threadTemp, NULL);
        int result2 = pthread_create(&thread2, NULL, threadPlusTemp, &fixedTemp);
        int result3 = pthread_create(&thread3, NULL, threadMinusTemp, &fixedTemp);

        if (result1 || result2 || result3) {
            printf("No se creó.\n");
            exit(2);
        }

        result1 = pthread_join(thread1, NULL);
        result2 = pthread_join(thread2, NULL);
        result3 = pthread_join(thread3, NULL);

        if (result1 || result2 || result3) {
            printf("The threads could not be joined.\n");
            exit(2);
        }

    } else {
        char str[5];
        sprintf(str, "%d", PID_PADRE);
        execl("./procesoB", str, 0);
    }
}

// Funciones para el sensor de temperatura

// La salida del BME280 son valores de un ADC y cada elemento sensor se comporta de manera diferente. 
// Por lo tanto, la temperatura deben calcularse utilizando un conjunto de parámetros de calibración.

// Primero se obtienen los datos de calibración de la temperatura del sensor desde los registros 0xFA hasta 0xFC.
// Cada registro tiene una longitud de 8 bits. Menos el registro 0xFA, que tiene una longitud de 4 bits de información útil.

void readCalibrationData(int fd, bme280_calib_data *data) {
    data->dig_T1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T1);
    data->dig_T2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T2);
    data->dig_T3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T3);
}

// Una vez calibrada la temperatura, se obtienen los datos crudos del sensor, del registro BME280_REGISTER_TEMPDATA.

void getRawData(int fd, bme280_raw_data *raw) {
    wiringPiI2CWrite(fd, BME280_REGISTER_TEMPDATA);

    raw->tmsb = wiringPiI2CRead(fd);
    raw->tlsb = wiringPiI2CRead(fd);
    raw->txsb = wiringPiI2CRead(fd);

    raw->temperature = 0;
    raw->temperature = (raw->temperature | raw->tmsb) << 8;
    raw->temperature = (raw->temperature | raw->tlsb) << 8;
    raw->temperature = (raw->temperature | raw->txsb) >> 4;
}

// Una vez obtenidos los datos crudos, se realizan rotaciones y corrimientos para obtener la temperatura calibrada

int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T) {
    
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1 << 1))) * ((int32_t)cal->dig_T2)) >> 11;

    int32_t var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) * ((adc_T >> 4) - ((int32_t)cal->dig_T1))) >> 12) * ((int32_t)cal->dig_T3)) >> 14;

    return var1 + var2;
}

// Por último, se compensa la temperatura calibrada para ser mostrada en grados Celsius

float compensateTemperature(int32_t t_fine) {
    float T = (t_fine * 5 + 128) >> 8;
    return T / 100;
}

