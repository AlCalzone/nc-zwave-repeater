/** 
 * @file SAM-M8Q.c
 * @brief Support for the SparkFun SAM-M8Q GPS QWIIC receiver in support of Geographic Command Class
 * 
 * This code will extract the GPS NMEA Sentence from a SparkFun GPS receiver board.
 * The Sparkfun board is easy to use since it has a QWIIC connector so it can be easily added to any Silabs Devkit with a QWIIC connector (DK2603)
 * The tricky part is that the GPS has to be polled every 900ms to pull the GPS data via I2C.
 * The data comes across in pieces which must then be reassembled into a single string.
 * Then the Geographic Location Command Class code can then extract the coordinates from the NMEA string and report them via Z-Wave.
 *
 * Setup to add support to the Z-Wave SwitchOnOff sample app GSDK 4.4.1
 * Add the following to the beginning of ApplicationTask after app_hw_init().
m_AppTaskHandle = xTaskGetCurrentTaskHandle();
AppTimerSetReceiverTask(m_AppTaskHandle);
AppTimerRegister(&I2CTimer, false, ZCB_I2CTimerCallBack);
TimerStart( &I2CTimer, GPS_POLLING_INTERVAL);

 * add the following lines near the top of app.c 
#include <AppTimer.h>            // GeoLocCC
#include "SAM-M8Q.h"
static SSwTimer I2CTimer;
static TaskHandle_t m_AppTaskHandle;

Add the following line to ApplicationInit just before the user task creation
  AppTimerInit(EAPPLICATIONEVENT_TIMER,NULL); // GPS support

add the following lines to the EVENT_APP_SWITCH_ON_OFF enum in events.h
  EVENT_APP_I2CTIMER_TIMEOUT,
  EVENT_APP_NMEA_READY,

If the GPS module is connected and debugprint is enabled there should be NMEA sentences printed out the debug port.
Note that it may take a minute or two for the GPS to lock onto satelites or move to a more open location.

 Typical GPS NMEA Sentence:
$GNGGA,192925.000,4310.250847,N,07052.274013,W,1,14,0.72,42.543,M,-32.898,M,,*7C
         UTC TIme, LATitude    , Longitude    ,Q,Sa,Pre , Alt    ,          , checksum

On power up the M8Q sends:
$GNGGA,120903.00,,,,,0,08,2.33,,,,,,Sats=0
$GNGGA,120904.00,,,,,0,08,2.33,,,,,,Sats=0
$GNGGA,120905.00,,,,,0,08,2.33,,,,,,Sats=0
$GNGGA,120906.00,4310.23746,N,07052.28309,W,1,08,2.33,29.1,M,-32.5,M,,Sats=8
$GNGGA,120907.00,4310.23833,N,07052.28243,W,1,08,2.33,31.1,M,-32.5,M,,Sats=8
$GNGGA,120908.00,4310.23951,N,07052.28237,W,1,08,2.33,33.5,M,-32.5,M,,Sats=8
 */


#include <sl_i2cspm.h> // I2C stuff
#include <sl_i2cspm_gps_config.h> // "gps" or whatever you named the I2C component
#include <AppTimer.h>
#include "SAM-M8Q.h"
#include "events.h"
#define DEBUGPRINT
#ifdef DEBUGPRINT
#include <DebugPrint.h>
#endif

static uint8_t * NMEA_sentence; // Build the GPS NMEA sentence with the string needed - "$G.GGA..."
static bool NMEA_valid = false;

void NMEA_Init(uint8_t * ptr) { // Initialize the pointer to the NMEA buffer
    NMEA_sentence=ptr;
}
bool is_NMEA_Valid(void) { // Is the NEMA sentence in the buffer valid?
    return(NMEA_valid);
}

I2C_TransferReturn_TypeDef Fetch_GPS(void) { // fetch the GPS NMEA sentence from the XA1110 GPS module over I2C and store it in the NMEA_sentence buffer
    static I2C_TransferSeq_TypeDef i2c_dat;
    I2C_TransferReturn_TypeDef i2c_rtn;
    static uint8_t i2c_txBuf[I2C_BUF_SIZE];
    static uint8_t i2c_rxBuf[I2C_BUF_SIZE];
    int i2c_read=0; // index into the Rx buffer
    bool i2c_done;
    int blankcount=0;

    // Setup the struct for I2CSPM to read data out of XA1110
    i2c_dat.addr = I2C_GPS_ADDR<<1; // I2C address is 7-bits - the LSB is the READ/WRITE bit
    i2c_dat.flags = I2C_FLAG_READ;
    i2c_dat.buf[0].data= &i2c_rxBuf[0];
    i2c_dat.buf[0].len= sizeof(i2c_rxBuf);
    i2c_txBuf[0]=0;
    i2c_dat.buf[1].data= &i2c_txBuf[0];
    i2c_dat.buf[1].len= 0;
    i2c_done=false;
    while (!i2c_done) {
        i2c_rtn=I2CSPM_Transfer(SL_I2CSPM_GPS_PERIPHERAL, &i2c_dat);
        if (i2cTransferDone!=i2c_rtn) {
            if (i2cTransferNack==i2c_rtn) DPRINT("I2C NACK ");
            else DPRINTF("I2C ERR=%x\n\r",i2c_rtn);
            return(i2c_rtn); // failed
        }
        for (i2c_read=0; (i2c_read<I2C_BUF_SIZE) && !i2c_done; i2c_read++) {
            if (0xFF==i2c_rxBuf[i2c_read]) {
                blankcount++;
                if (blankcount>=I2C_BUF_SIZE) {
                    i2c_done=true; // full buffer of 0x0a indicates there is no valid data - wait for the next sample
                } 
            } else {
                blankcount=0;
                if (NMEA_build(i2c_rxBuf[i2c_read])) { // Add each character to the Sentence, once the desired sentence is found, return TRUE
                    NMEA_parse();   // parse the NMEA sentence and update coordinates
                    i2c_done=true;
                } 
            }
        }
    }
    return(i2cTransferDone);
}

// This callback fetches the GPS coordinates every XA1110_POLLING_INTERVAL milliseconds
void ZCB_I2CTimerCallBack(SSwTimer *pTimer) {
  static uint8_t FailCount;
  zaf_event_distributor_enqueue_app_event(EVENT_APP_I2CTIMER_TIMEOUT); // TODO don't need this...
//  DPRINT("\n+");
  if (i2cTransferDone==Fetch_GPS()) {// fetch the GPS NMEA Sentence from the XA1110
    // sometimes it fails to fetch the sentence in which case we just wait for the next interval
      FailCount=0;
  } else {
      FailCount++;
  }
  if (FailCount<10) TimerRestart(pTimer); // If failed 10 times then the GPS is probably dead so give up
}
