/**
 * This is a temporary file that is #included at line 23498 in ZW_classcmd.h
 * once the CC V2 is approved it will be built-in
 * this file has the CC structures
 */

/************************************************************/
/* Geographic Location Get command class structs */
/************************************************************/
typedef struct _ZW_GEOGRAPHIC_LOCATION_GET_V2_FRAME_
{
    uint8_t   cmdClass;                     /* The command class */
    uint8_t   cmd;                          /* The command */
} ZW_GEOGRAPHIC_LOCATION_GET_V2_FRAME;

/************************************************************/
/* Geographic Location Report command class structs */
/************************************************************/
typedef struct _ZW_GEOGRAPHIC_LOCATION_REPORT_V2_FRAME_
{
    uint8_t   cmdClass;                     /* The command class */
    uint8_t   cmd;                          /* The command */
    uint8_t   longitude1; /* MSB Signed 32-bit integer - 1 sign bit, 8 integer bits, 23 fractional bits of degree */
    uint8_t   longitude2;
    uint8_t   longitude3;
    uint8_t   longitude4; /* LSB */
    uint8_t   latitude1; /* MSB */
    uint8_t   latitude2;
    uint8_t   latitude3;
    uint8_t   latitude4; /* LSB */
    uint8_t   altitude1; /* MSB signed 24-bit integer of altitude in centimeters */
    uint8_t   altitude2;
    uint8_t   altitude3; /* LSB */
    uint8_t   status;    /* bit field status */
} ZW_GEOGRAPHIC_LOCATION_REPORT_V2_FRAME;

/************************************************************/
/* Geographic Location Set command class structs */
/************************************************************/
typedef struct _ZW_GEOGRAPHIC_LOCATION_SET_V2_FRAME_
{
    uint8_t   cmdClass;                     /* The command class */
    uint8_t   cmd;                          /* The command */
    uint8_t   longitude1; /* MSB Signed 32-bit integer - 1 sign bit, 8 integer bits, 23 fractional bits of degree */
    uint8_t   longitude2;
    uint8_t   longitude3;
    uint8_t   longitude4; /* LSB */
    uint8_t   latitude1; /* MSB */
    uint8_t   latitude2;
    uint8_t   latitude3;
    uint8_t   latitude4; /* LSB */
    uint8_t   altitude1; /* MSB signed 24-bit integer of altitude in centimeters */
    uint8_t   altitude2;
    uint8_t   altitude3; /* LSB */
} ZW_GEOGRAPHIC_LOCATION_SET_V2_FRAME;
