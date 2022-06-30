//Proceso encargado de leer y modificar la temperatura

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

//Hilo que lee temperatura
void* thread_body_1(void* arg) {
	// Obtain a pointer to the shared variable
	//int* shared_var_ptr = (int*)arg;
	// Increment the shared variable by 1 by writing
	// directly to its memory address
	//(*shared_var_ptr)++;
	//printf("%d\n", *shared_var_ptr);

    while(1) {

        int fd = wiringPiI2CSetup(BME280_ADDRESS);

            if (fd < 0) {
                printf("Device not found");
                return -1;
                }

        bme280_calib_data cal;
        readCalibrationData(fd, &cal);

        wiringPiI2CWriteReg8(fd, 0xf2, 0x01);
        wiringPiI2CWriteReg8(fd, 0xf4, 0x25);

        bme280_raw_data raw;
        getRawData(fd, &raw);

        int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);
        float t = compensateTemperature(t_fine); // C

        printf("************ TEMPERATURA:%.2f ************\n", t); 

        delay(10000);
    }

	return NULL;
}

void* thread_body_2(void* arg) {
	
	return NULL;
}

void* thread_body_3(void* arg) {
	
	return NULL;
}

int main() {

    int res = wiringPiSetup();

    if (res) {
        printf("No se pudo inicializar la raspi");
        return 1;
    }

    int PID = fork();
    if (PID) {
        //Cambiar a proceso B
    } else {
        printf("Proceso A en ejecución... \n");
        
        pthread_t thread1;
	    pthread_t thread2;
        pthread_t thread3;

        int result1 = pthread_create(&thread1, NULL, thread_body_1, NULL);
	    int result2 = pthread_create(&thread2, NULL, thread_body_2, NULL);
        int result3 = pthread_create(&thread3, NULL, thread_body_3, NULL);

    	if (result1 || result2 || result3) {
		printf("No se creó.\n");
		exit(2);
	}

        result1 = pthread_join(thread1, NULL);
        result2 = pthread_join(thread2, NULL);
    	result3 = pthread_join(thread3, NULL);

        if (result1 || result2 ||result3) {
		printf("The threads could not be joined.\n");
		exit(2);
	}
    }

}

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

void getRawData(int fd, bme280_raw_data *raw)
{
    wiringPiI2CWrite(fd, BME280_REGISTER_TEMPDATA);

    raw->tmsb = wiringPiI2CRead(fd);
    raw->tlsb = wiringPiI2CRead(fd);
    raw->txsb = wiringPiI2CRead(fd);

    raw->temperature = 0;
    raw->temperature = (raw->temperature | raw->tmsb) << 8;
    raw->temperature = (raw->temperature | raw->tlsb) << 8;
    raw->temperature = (raw->temperature | raw->txsb) >> 4;

}