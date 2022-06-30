#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <mqueue.h>
#include "bme280.h"

#define MOTOR_PIN 7 // 7

int main()
{

    int fd = wiringPiI2CSetup(BME280_ADDRESS);

    if (fd < 0)
    {
        printf("Device not found");
        return -1;
    }

    int res = wiringPiSetup();

    if (res) {
        printf("No se pudo inicializar la raspi");
        return 1;
    }

    pinMode(MOTOR_PIN, OUTPUT);

    digitalWrite(MOTOR_PIN, LOW);

    bme280_calib_data cal;
    readCalibrationData(fd, &cal);

    wiringPiI2CWriteReg8(fd, 0xf2, 0x01); // humidity oversampling x 1
    wiringPiI2CWriteReg8(fd, 0xf4, 0x25); // pressure and temperature oversampling x 1, mode normal

    bme280_raw_data raw;
    getRawData(fd, &raw);

    int32_t t_fine = getTemperatureCalibration(&cal, raw.temperature);
    float t = compensateTemperature(t_fine); // C

    printf("{\"sensor\":\"bme280\", \"temperature\":%.2f, \"timestamp\":%d}\n", t, (int)time(NULL));

    digitalWrite(MOTOR_PIN, HIGH);
    delay(10000);
    digitalWrite(MOTOR_PIN, LOW);
    
    int pid = fork();

    if (pid == 0) {
        system("mpg321 calefaccion.mp3 --quiet &");
    }
    

    return 0;

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
