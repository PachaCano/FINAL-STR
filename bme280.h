
#ifndef __BME280_H__
#define __BME280_H__

#define BME280_ADDRESS                0x76

#define BME280_REGISTER_DIG_T1        0xFA
#define BME280_REGISTER_DIG_T2        0xFB
#define BME280_REGISTER_DIG_T3        0xFC
#define BME280_REGISTER_TEMPDATA      0xFA

/*
* Immutable calibration data read from bme280
*/
typedef struct
{
  uint8_t dig_T1;
  int8_t  dig_T2;
  int8_t  dig_T3;

} bme280_calib_data;

/*
* Raw sensor measurement data from bme280
*/
typedef struct 
{
  uint8_t tmsb;
  uint8_t tlsb;
  uint8_t txsb;

  uint32_t temperature;
  
} bme280_raw_data;


void readCalibrationData(int fd, bme280_calib_data *cal);
int32_t getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T);
float compensateTemperature(int32_t t_fine);
void getRawData(int fd, bme280_raw_data *raw);

#endif
