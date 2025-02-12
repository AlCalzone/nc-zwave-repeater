/**
 * @file CC_GeographicLoc.c 
 * @brief Z-Wave Geographic Location V2 Command Class implementation for End Devices
 * See the ReadMe.md for more details.
 * @author Eric Ryherd drzwave@silabs.com
 * @date 2024/2/23
 *
 * Software Architecture:
 * There are two implementations in this code which are chosen based on the GPS_ENABLED #define in the CC_GeographicLoc.h file:
 *  1) GPS_ENABLED defined
 *      - A GPS receiver is attached via UART or I2C and provides live GPS data.
 *      - SET commands are ignored and coordinates are read-only as they come from the GPS receiver.
 *  2) GPS_ENABLED not defined
 *      - GPS coordinates are from a SET command which is typically from a mobile phone or other GPS device.
 *      - The GPS coordinates are stored in NVM.
 * 
 * The code in this file handles the GeoLocCC Set/Get/Report frames in the CC_GeographicLoc_handler.
 * The hardware interface to GPS receviers are in other files in this repo but utilize functions in this file.
 * The NMEA "sentence" from the GPS receiver is buffered and parsed in this file and converted into Long/Lat/Alt values each time the GPS sends data.
 * 
 */

// ISSUES:
// how to update ZW_classcmd.h?
// how to update the XML?
// add code for storing in NVM

#include "CC_GeographicLoc.h"
#ifdef GEOLOCCC_INTERFACE_UART
#include "UART_DRZ.h" // GPS receiver is on EUSART1
#endif
#include <ZW_TransportEndpoint.h>
#include <ZW_application_transport_interface.h>
#include <em_core_generic.h>    // CORE_ATOMIC

// uncomment to enable debugging info
//#define DEBUGPRINT
#include "DebugPrint.h"
#include <string.h>

#include <stdlib.h>

// GPS variables
static int32_t longitude=LON_DEFAULT;
static int32_t latitude=LAT_DEFAULT;
static int32_t altitude=ALT_DEFAULT;
static uint8_t alt_valid=0;
static uint8_t long_valid=0;
static uint8_t lat_valid=0;
static uint8_t gps_quality=0;
//static uint8_t gps_time[]="HH:MM:SS";
//static uint8_t gps_satelites=0;

// Max length is 255
#define SENTENCE_BUF_LENGTH 100
static uint8_t SentenceBufRaw[SENTENCE_BUF_LENGTH]; // holds the GPS sentence for later parsing
static uint8_t NMEA_index; // index pointer into the buffer

//int32_t NMEA_getAltitude(void);

int32_t GetLatitude(void) {
    return(latitude);
}
int32_t GetLongitude(void) {
    return(longitude);
}
int32_t GetAltitude(void) {
    return(altitude);
}
int32_t GetStatus(void) {
    return((gps_quality<<4)|(GEO_READ_ONLY<<3)|(alt_valid<<2)|(lat_valid<<1)|(long_valid));
}

/*
 * @brief handler for Geographic Location Command class when a command is received via Z-Wave
 */
static received_frame_status_t CC_GeographicLoc_handler(
    cc_handler_input_t * input,
    cc_handler_output_t * output)
{
    CORE_DECLARE_IRQ_STATE; // save irqstate when in CORE_ATOMIC

    switch (input->frame->ZW_Common.cmd)
    {
        case GEOGRAPHIC_LOCATION_GET_V2:
            if (true == Check_not_legal_response_job(input->rx_options)) {   // check for multicast etc.
                return RECEIVED_FRAME_STATUS_FAIL;
            }
            // send the report
            output->frame->ZW_GeographicLocationReportV2Frame.cmdClass = COMMAND_CLASS_GEOGRAPHIC_LOCATION_V2;
            output->frame->ZW_GeographicLocationReportV2Frame.cmd      = GEOGRAPHIC_LOCATION_REPORT_V2;
#ifdef CORE_ENTER_ATOMIC
            CORE_ENTER_ATOMIC(); /* prevent values from changing while assembling the frame to avoid corruption */
#endif
            output->frame->ZW_GeographicLocationReportV2Frame.longitude1 = (uint8_t)((longitude>>24)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.longitude2 = (uint8_t)((longitude>>16)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.longitude3 = (uint8_t)((longitude>>8)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.longitude4 = (uint8_t)((longitude>>0)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.latitude1  = (uint8_t)((latitude>>24)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.latitude2  = (uint8_t)((latitude>>16)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.latitude3  = (uint8_t)((latitude>>8)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.latitude4  = (uint8_t)((latitude>>0)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.altitude1  = (uint8_t)((altitude>>16)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.altitude2  = (uint8_t)((altitude>>8)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.altitude3  = (uint8_t)((altitude>>0)&0xFF);
            output->frame->ZW_GeographicLocationReportV2Frame.status    = ((gps_quality<<4)|(GEO_READ_ONLY<<3)|(alt_valid<<2)|(lat_valid<<1)|(long_valid));
            output->length = sizeof(ZW_GEOGRAPHIC_LOCATION_REPORT_V2_FRAME); /* triggers the send */
#ifdef CORE_EXIT_ATOMIC
            CORE_EXIT_ATOMIC();
#endif
            break;
        case GEOGRAPHIC_LOCATION_SET_V2:
            // TODO more to come here - reject it if read only
            break;
        default:
            return RECEIVED_FRAME_STATUS_NO_SUPPORT;
            break;
    }
    return RECEIVED_FRAME_STATUS_SUCCESS;
}

#ifdef GPS_ENABLED
// GPS conversion to GeoLocCC
#else
// Store the GPS values set in NVM
#endif

static uint8_t lifeline_reporting(ccc_pair_t * p_ccc_pair)
{
  p_ccc_pair->cmdClass = COMMAND_CLASS_GEOGRAPHIC_LOCATION;
  p_ccc_pair->cmd      = GEOGRAPHIC_LOCATION_REPORT_V2;
  return 1;
}

// called by ZAF_Init() - Anything that needs initialization on reset - pick up values out of NVM or init the UART
static void init(void)
{
#ifdef GPS_ENABLED
  NMEA_Init(SentenceBufRaw); // initialize the pointer to the NMEA buffer which the GPS interface will fill in
#endif
}

// called when a factory reset is performed
static void reset(void)
{
    // TODO reset the values in the NVM?
}

typedef enum {  // NMEA state machine states
    NMEA_search,
    NMEA_GNGGA,
    NMEA_fetch,
    NMEA_checksum1,
    NMEA_checksum2,
    NMEA_error
} NMEA_state_e;

static uint8_t NMEAState = NMEA_search; // state machine that assembles the Sentance from a UART

/* @brief Add the character C to the "sentence" buffer as each byte arrives via a UART or I2C
 * returns true when a complete buffer has been filled otherwise false
 * Typical NMEA sentence: 
 * $GPGGA,121017.00,4310.24176,N,07052.27544,W,1,08,1.10,00048,M,-032,M,,*52
 *       ,TIME     , Latitude   , Longitude   ,Q,SAT,    , Alt   ,       ,Checksum
 *          1           2      3      4      5 6  7    8    9  10  11 13
 * Time = UTC time HH:MM:SS.ff (hours, minutes, seconds, fractions of a second)
 * Q = GPS Quality - 0=Invalid, 1=GPS fix locked on
 * SAT = Satellites in use (not how many are in view)
 * Alt = Altitude in M above mean sea level (next field is the units for altitude M=meters)
 * Checksum = XOR of all characters between the $ and *
 * When it first power up and before getting satellites it sends:
 * $GNGGA,145358.820,,,,,0,0,,,M,,M,,*
 * When SAT=0, ignore the rest of the message
 * 
 */
bool NMEA_build(char c) {
    bool rtn = false;
//    if (c<0x20 || c>0x7e) DPRINTF("x%02X",c);
//    else DPRINTF("%c",c); // print every character from the GPS module for debugging only
    switch(NMEAState) {
        case NMEA_search: // search for the $
            if ('$'==c) {
                NMEA_index=0;
                SentenceBufRaw[NMEA_index++]=c;
                NMEAState=NMEA_GNGGA;
            }
            break;
        case NMEA_GNGGA: // check for the desired sentence - GPGGA or GNGGA
            if ('G'==c || 'P'==c || 'N'==c || 'A'==c || ','==c) { 
                SentenceBufRaw[NMEA_index++]=c;
                if (','==c) {
                    NMEAState=NMEA_fetch;
                }
            } else { // ignore the other sentences
                NMEAState=NMEA_search;
            }
            break;
        case NMEA_fetch: // collect the sentence to the *
            if (NMEA_index>=SENTENCE_BUF_LENGTH-3) { // don't overrun the buffer
                NMEAState=NMEA_search;
            } else if ('*'==c) { // start of checksum
                NMEAState=NMEA_checksum1;
            } 
            SentenceBufRaw[NMEA_index++]=c;
            break;
        case NMEA_checksum1: // capture checksum 1st digit
            NMEAState=NMEA_checksum2;
            SentenceBufRaw[NMEA_index++]=c;
            break;
        case NMEA_checksum2: // capture checksum 2nd digit
            NMEAState=NMEA_search;
            SentenceBufRaw[NMEA_index++]=c;
        DPRINT("\r\n!");
            rtn=true;
            break;
        default:
            NMEAState=NMEA_search;
            break;
    }
    return(rtn);
}

/* zero the valid bits when the gps is not locked
 */
static void gps_notLocked(void) {
    gps_quality=0;
    alt_valid=0;
    lat_valid=0;
    long_valid=0;
    altitude=ALT_DEFAULT;
    longitude=LON_DEFAULT;
    latitude=LAT_DEFAULT;
}

/* @brief convert hex string of up to 8 chars to an integer. Zero if non-hex chars are found.
 */
static uint32_t hextoi(uint8_t * ptr) {
    uint32_t rtn = 0;
    for (uint8_t i=0; i<8; i++) {
        uint8_t c = ptr[i];
        if ('\0' == c) break; // end of string = done
        if (c >= '0' && c <='9') c = c - '0';
        else if (c >= 'A' && c <= 'F') c = c - 'A' +10;
        else if (c >= 'a' && c <= 'f') c = c - 'a' +10;
        else { // non hex digit - return 0
            c=0;
            rtn=0;
            break;
        }
        rtn = (rtn<<4) | (c &0x0f); // shift in the nibble
    }
    return(rtn);
}

/* @brief return TRUE if the sentence checksum is OK
 */
bool NMEA_checksum(void) {
    uint32_t i;
    uint8_t sum=0;
    for (i=1; ((i<SENTENCE_BUF_LENGTH) && ('*'!=SentenceBufRaw[i])); i++) {
        sum ^= SentenceBufRaw[i];
        DPRINTF("%c",SentenceBufRaw[i]); // print out the NMEA Sentence for debugging purposes
    }
    SentenceBufRaw[i+3]='\0'; // NULL the end of the string
    int check = hextoi(&SentenceBufRaw[i+1]);
    //DPRINTF("\nSum=%x, Check=%x ",sum,check);
    if (check == sum) return(true);
    else return(false);
}

/* @brief parse the NMEA sentence and if checksum is OK, update the GPS coordinates 
 */
void NMEA_parse(void) {
    if (NMEA_checksum()) { // checksum is good so compute the coordinates
        NMEA_getStatus(); // sets the status bits
        DPRINTF("Sats=%d ",gps_quality);
        if (gps_quality >=4) {  // need at least 4 satellites to be locked on
            latitude  = NMEA_getLatitude();
            longitude = NMEA_getLongitude();
            altitude  = NMEA_getAltitude();
        } else {
            gps_notLocked();
        }
    } else {
        gps_notLocked();
        gps_quality = 1; // debugging value indicating the checksum has failed
    }
}

/* @brief search thru the NMEA sentence and return the 32 bit longitude a signed fixed point decimal degress with 23 bits of fraction
 * Returns 0 if errors
 */
int32_t NMEA_getLongitude(void) {
    int32_t i;
    int32_t fieldNum = 0;
    int32_t rtn = 0;
    int32_t k;
    double t;
    char tmp[10];
    for (i=0;i<SENTENCE_BUF_LENGTH;i++) { // search for the longitude field which is the 2nd one
        if (','==SentenceBufRaw[i]) {
            fieldNum++;
            if (fieldNum==4) {
                i++;
                break;
            }
        }
    }
    if (SENTENCE_BUF_LENGTH<=i) return(LON_DEFAULT); // didn't find the field
    tmp[0]=SentenceBufRaw[i++];     // first 3 digits are degrees
    tmp[1]=SentenceBufRaw[i++]; 
    tmp[2]=SentenceBufRaw[i++]; 
    tmp[3]='\0';
    rtn = atoi(tmp); // decimal degrees (0-180)
    for (k=0;((k+i)<SENTENCE_BUF_LENGTH) && (','!=SentenceBufRaw[k+i]);k++) {
        tmp[k]=SentenceBufRaw[k+i];     // copy to tmp buffer
    }
    tmp[k+1] = '\0';
    t = atof(tmp);  // note floating point!
    t = rtn + (t/(float)60.0); // convert from minutes to degrees
    if ('W'==SentenceBufRaw[k+i+1]) t=0-t; // West=negative
    else if ('E'!=SentenceBufRaw[k+i+1]) DPRINTF("Err1=%c", SentenceBufRaw[k+i+1]);
    //DPRINTF("LON=%f ",t);
    rtn = t*(1<<23); // convert to a fixed point integer
    //DPRINTF("hex=%X\r\n",rtn);
    return(rtn);
}

/* @brief search thru the NMEA sentence and return the 32 bit latitude a signed fixed point decimal degress with 23 bits of fraction
 * Returns 0 if errors
 */
int32_t NMEA_getLatitude(void) {
    int32_t i;
    int32_t fieldNum = 0;
    int32_t rtn = 0;
    int32_t k;
    double t;
    char tmp[10];
    for (i=0;i<SENTENCE_BUF_LENGTH;i++) { // search for the latitude field which is the 2nd one
        if (','==SentenceBufRaw[i]) {
            fieldNum++;
            if (fieldNum==2) {
                i++;
                break;
            }
        }
    }
    if (SENTENCE_BUF_LENGTH<=i) return(LAT_DEFAULT); // didn't find the field
    tmp[0]=SentenceBufRaw[i++];     // first 2 digits are degrees
    tmp[1]=SentenceBufRaw[i++]; 
    tmp[2]='\0';
    rtn = atoi(tmp); // decimal degrees (0-90)
    for (k=0;((k+i)<SENTENCE_BUF_LENGTH) && (','!=SentenceBufRaw[k+i]);k++) {
        tmp[k]=SentenceBufRaw[k+i];     // copy to tmp buffer
    }
    tmp[k+1] = '\0';
    t = atof(tmp); // float!
    t = rtn + (t/(float)60.0); // convert from minutes to degrees
    if ('S'==SentenceBufRaw[k+i+1]) t=0-t; // South=negative
    else if ('N'!=SentenceBufRaw[k+i+1]) DPRINTF("Err2=%c", SentenceBufRaw[k+i+1]);
    //DPRINTF("LAT=%f ",t);
    rtn = t*(1<<23); // convert to a fixed point integer
    //DPRINTF("hex=%X ",rtn);
    return(rtn);
}

/* @brief search thru the NMEA sentence and return the signed 32 bit altitude in cm
 * Returns 0 if errors
 */
int32_t NMEA_getAltitude(void) {
    int32_t i;
    int32_t fieldNum = 0;
    int32_t rtn = 0;
    int32_t k;
    float t;
    char tmp[10];
    for (i=0;i<SENTENCE_BUF_LENGTH;i++) { // search for the altitude field which is the 9th one
        if (','==SentenceBufRaw[i]) {
            fieldNum++;
            if (fieldNum==9) { // altitude is the 9th field
                i++;
                break;
            }
        }
    }
    if (SENTENCE_BUF_LENGTH<=i) return(ALT_DEFAULT); // didn't find the Altitude
    for (k=0;((k+i)<SENTENCE_BUF_LENGTH) && (','!=SentenceBufRaw[k+i]);k++) {
        tmp[k]=SentenceBufRaw[k+i];     // copy the altitude to tmp buffer
    }
    tmp[k+1] = '\0';
    t = atof(tmp);
    //DPRINTF("alt=%s = %f \r\n",tmp,t);  // https://community.silabs.com/s/article/floating-point-print-with-gcc?language=en_US required to enable printing of floats (adds 5K of flash!)
    rtn=(int32_t)(t*100);   // convert to centimeters and return an integer
    return(rtn);
} // NMEA_getAltitude

/* @brief search thru the NMEA sentence and return the STATUS byte based on Q and SAT
 * Returns 0 if errors
 */
int32_t NMEA_getStatus(void) {
    int32_t i;
    int32_t fieldNum = 0;
    int32_t rtn = 0;
    int32_t k;
    int32_t t;
    char tmp[10];
    for (i=0;i<SENTENCE_BUF_LENGTH;i++) { // search for the Q field
        if (','==SentenceBufRaw[i]) {
            fieldNum++;
            if (fieldNum==6) {
                i++;
                break;
            }
        }
    }
    if ((i>=SENTENCE_BUF_LENGTH) || ('0'==SentenceBufRaw[i])) { // GPS not locked - values are not valid
        gps_notLocked();
    } else {
        i++; // move to , before the SAT field
        i++; // move to SAT field
        for (k=0;((k+i)<SENTENCE_BUF_LENGTH) && (','!=SentenceBufRaw[k+i]);k++) {
            tmp[k]=SentenceBufRaw[k+i];     // copy SAT to tmp buffer
        }
        tmp[k+1] = '\0';
        t = atoi(tmp);
        if (t>15) t=15; // clip # satellites to the max that fits in the QUAL field
        gps_quality=t;
        //DPRINTF("SAT=%s, %d",&tmp[0],gps_quality);
        if (gps_quality>=4) { // need at least 4 satellites to get accurate readings
            alt_valid=1;
            lat_valid=1;
            long_valid=1;
        } else {
            gps_notLocked();
        }
        rtn=((gps_quality<<4)|(GEO_READ_ONLY<<3)|(alt_valid<<2)|(lat_valid<<1)|(long_valid));
    }
    return(rtn);
} // NMEA_getStatus


// This adds the command class to the NIF and links in the handler
REGISTER_CC_V5(COMMAND_CLASS_GEOGRAPHIC_LOCATION, GEOGRAPHIC_LOCATION_VERSION_V2, CC_GeographicLoc_handler, NULL, NULL, lifeline_reporting, 0, init, reset);
// No BASIC CC mapping so those are NULL. zero is reserved field
