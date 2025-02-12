#ifndef GPS_H_
#define GPS_H_

#include <zaf_event_distributor_soc.h>
#include "CC_GeographicLoc.h"
#include <em_i2c.h>

// Size of the I2C buffer to fetch data - larger results in long I2C fetch which can delay other processing
#define I2C_BUF_SIZE 32

// I2C address of the GPS module - the uBlox modules are all 0x42
#define I2C_GPS_ADDR 0x42

// GPS NMEA sentence buffer - must be large enough to hold an entire sentence
#define NMEA_BUF_SIZE 80

// The polling interval should typically be under 1s to avoid overruns which would require even more error handling
#define GPS_POLLING_INTERVAL 933

I2C_TransferReturn_TypeDef Fetch_GPS(void);
void ZCB_I2CTimerCallBack(SSwTimer *pTimer);

#endif
