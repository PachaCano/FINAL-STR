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

#define TS 100000000 // nanosegundos.   // = 100 ms
#define LIM_MAX_TEMP 30
#define LIM_MIN_TEMP 15

#define PULSER_PIN_PLUS 5 // 18
#define PULSER_PIN_MINUS 2 // 13
#define LED_PIN_RED 1  // 12
#define LED_PIN_BLUE 4  // 16

int fixedTemp = 20; // Temperatura que variará según los pulsadores.

pthread_mutex_t mtx;

int FD, ticks = 0, ticks2 = 0, ticks3 = 0;
bool flag = false;
bool flag2 = false;

void isrTimer (int sig, siginfo_t *si, void *uc) {
    ticks++;
    if (flag) {
        ticks2++;
    }
    if (flag2) {
        ticks3++;
    }
}

// Hilo que lee temperatura
void *thread_body_1(void *arg) {
    // printf("Temperatura  \n");

    while (1) {

        if (ticks == 50) {
            bme280_calib_data cal;
            readCalibrationData(FD, &cal);

            // Setea la frecuencia y modo de SAMPLING
            wiringPiI2CWriteReg8(FD, 0xf2, 0x01); // Registro sensor ctrl_hum
            wiringPiI2CWriteReg8(FD, 0xf4, 0x25); // Registro sensor ctrl_meas

            bme280_raw_data raw;
            getRawData(FD, &raw);

            int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);
            float t = compensateTemperature(t_fine); // C

            printf("************ TEMPERATURA:%.2f \t FIXED_TEMP %d ************\n", t, fixedTemp);
            ticks = 0;
        }
    }

    return NULL;
}

void *thread_body_2(void *arg) {

    bool pulsado = false;
    int pulsador = 0;

    int* ptrFixedTemp = (int*) arg;

    while (1) {
        pulsador = digitalRead(PULSER_PIN_PLUS);

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
                        if (ticks2 == 10) {
                            ticks2 = 0;
                            flag = false;
                            digitalWrite(LED_PIN_RED, LOW);
                            break;
                        }
                    }
                    // delay(3000);       // usar temporizador, no delay xd
                    // digitalWrite(LED_PIN_RED, LOW);
                }
                
                pthread_mutex_unlock(&mtx);
                pulsado = true;
                flag = true;
                while (1) {
                        if (ticks2 == 1) {
                            ticks2 = 0;
                            flag = false;
                            break;
                        }
                    }    // delay(100) de pulsador
            
        } else if (pulsado == true && pulsador == 0) {
                pulsado = false;
        } 

    }

    return NULL;
}

void *thread_body_3(void *arg)
{

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
                    // delay(3000);       // usar temporizador, no delay xd
                    // digitalWrite(LED_PIN_BLUE, LOW);
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
            
        } else if (pulsado == true && pulsador == 0) {
                pulsado = false;
        } 

    }

    return NULL;
}



int main() {

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

    pinMode(PULSER_PIN_MINUS, INPUT);
    pinMode(PULSER_PIN_PLUS, INPUT);
    pinMode(LED_PIN_BLUE, OUTPUT);
    pinMode(LED_PIN_RED, OUTPUT);

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



    int PID = fork();

    if (PID != 0) { // PADRE -> 


        

        // printf("Temperatura  \n");

        pthread_t thread1;
        pthread_t thread2;
        pthread_t thread3;

        int result1 = pthread_create(&thread1, NULL, thread_body_1, NULL);
        int result2 = pthread_create(&thread2, NULL, thread_body_2, &fixedTemp);
        int result3 = pthread_create(&thread3, NULL, thread_body_3, &fixedTemp);

        if (result1 || result2 || result3)
        {
            printf("No se creó.\n");
            exit(2);
        }

        result1 = pthread_join(thread1, NULL);
        result2 = pthread_join(thread2, NULL);
        result3 = pthread_join(thread3, NULL);

        if (result1 || result2 || result3)
        {
            printf("The threads could not be joined.\n");
            exit(2);
        }

    } else { // HIJO

        // Cambiar a proceso B
    }
}






// Funciones para el sensor de temperatura

// La salida del BME280 son valores de un ADC y cada elemento sensor se comporta de manera diferente. 
// Por lo tanto, la temperatura deben calcularse utilizando un conjunto de parámetros de calibración.

// Primero se obtienen los datos crudos de la temperatura del sensor desde los registros 0xFA hasta 0xFC.
// Cada registro tiene una longitud de 8 bits. Menos el registro 0xFA, que tiene una longitud de 4 bits de información útil.

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

// Una vez obtenidos los datos crudos, se realizan rotaciones y corrimientos para obtener 

// Estos parámetros se obtienen de la función readCalibrationData.
// La función getTemperatureCalibration calcula el valor de t_fine para la temperatura.
// La función compensateTemperature calcula la temperatura a partir del valor de t_fine.
// La función getRawData obtiene los valores de temperatura y humedad del sensor.
int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1 << 1))) * ((int32_t)cal->dig_T2)) >> 11;

    int32_t var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) * ((adc_T >> 4) - ((int32_t)cal->dig_T1))) >> 12) * ((int32_t)cal->dig_T3)) >> 14;

    return var1 + var2;
}

void readCalibrationData(int fd, bme280_calib_data *data)
{
    data->dig_T1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T1);
    data->dig_T2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T2);
    data->dig_T3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T3);
}

float compensateTemperature(int32_t t_fine)
{
    float T = (t_fine * 5 + 128) >> 8;
    return T / 100;
}

