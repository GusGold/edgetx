/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "opentx.h"
#include "spektrum.h"

/*
 * Documentation of the Spektrum protocol is available under
 * https://www.spektrumrc.com/ProdInfo/Files/SPM_Telemetry_Developers_Specs.pdf
 * https://github.com/SpektrumRC/SpektrumDocumentation/blob/master/Telemetry/spektrumTelemetrySensors.h
 *
 * Multi module adds two byte header of 0xAA [RSSI of telemetry packet] [16 byte message]
 */

/*
 * The 16 byte message is formated as follows
 * [ic2 address] [secondary id] [14 byte ic2 address (type) specific data]
 * we translate the secondary to the instance of the sensor
 *
 * OpenTX Mapping
 *
 *  instance = secondary id  (almost always 0)
 *  subid = 0 (always 0)
 *  id = i2c address <<8 | start byte
 */

#define I2C_PSEUDO_TX 0xf0

#define I2C_PSEUDO_TX_RSSI   (I2C_PSEUDO_TX << 8 | 0)
#define I2C_PSEUDO_TX_BIND   (I2C_PSEUDO_TX << 8 | 4)
#define I2C_PSEUDO_TX_FM     (I2C_PSEUDO_TX << 8 | 8)

#define SPEKTRUM_TELEMETRY_LENGTH 18
#define DSM_BIND_PACKET_LENGTH 12

#define I2C_HIGH_CURRENT  0x03
#define I2C_FLITECTRL     0x05
#define I2C_FWD_PGM       0x09
#define I2C_TEXTGEN       0x0c
#define I2C_GPS_LOC       0x16
#define I2C_GPS_STAT      0x17
#define I2C_ESC           0x20
#define I2C_ALPHA6        0x24
#define I2C_CELLS         0x3a

// SMART_BAT is using fake I2C adresses compared to official Spektrum address
// because of subtype used only for this I2C address
#define I2C_SMART_BAT_BASE_ADDRESS    0x42
#define I2C_SMART_BAT_REALTIME        0x42
#define I2C_SMART_BAT_CELLS_1_6       0x43
#define I2C_SMART_BAT_CELLS_7_12      0x44
#define I2C_SMART_BAT_CELLS_13_18     0x45
#define I2C_SMART_BAT_ID              0x4A
#define I2C_SMART_BAT_LIMITS          0x4B

#define I2C_QOS 0x7f

// GPS flags definitions:
// Example:  B9 (1100 1001) = IS_NORTH, !IS_EAST, !GREATER_99, IS_FIX_VALID,
//                            !DATA_RECEIVED, NEGATIVE_ALT
#define GPS_INFO_FLAGS_IS_NORTH               0x01
#define GPS_INFO_FLAGS_IS_EAST                0x02
#define GPS_INFO_FLAGS_LONGITUDE_GREATER_99   0x04
#define GPS_INFO_FLAGS_GPS_FIX_VALID          0x08
#define GPS_INFO_FLAGS_GPS_DATA_RECEIVED      0x10
#define GPS_INFO_FLAGS_3D_FIX                 0x20
// #define	GPS_INFO_FLAGS_BIT6                   0x40 // ??
#define GPS_INFO_FLAGS_NEGATIVE_ALT           0x80

// FLITECTRL FLAGS
#define FLITECTRL_FLAGS_IS_AS3X_STAB      0x01
#define FLITECTRL_FLAGS_IS_ANGLE_DEMAND   0x02 
#define FLITECTRL_FLAGS_IS_AS3X_HEADING   0x04 
#define FLITECTRL_FLAGS_IS_SAFE_ENVELOPE  0x08 

// 1= Test sensor with captured telementry date
// See  replaceForTestingPackage(const uint8_t *packet) at the end of this file
#define TEST_CAPTURED_MESSAGE  0

enum SpektrumDataType : uint8_t {
  int8,
  int16,
  int32,
  uint8,
  uint16,
  uint32,
  uint8bcd,
  uint16bcd,
  uint32bcd,
  uint16le,
  uint32le,
  custom
};

struct SpektrumSensor {
  const uint8_t i2caddress;
  const uint8_t startByte;
  const SpektrumDataType dataType;
  const char *name;
  const TelemetryUnit unit;
  const uint8_t precision;
};

const SpektrumSensor spektrumSensors[] = {
  // High voltage internal sensor
  {0x01,             0,  int16,     STR_SENSOR_A1,                UNIT_VOLTS,                  1},

  // Temperature internal sensor
  {0x02,             0,  int16,     STR_SENSOR_TEMP1,             UNIT_CELSIUS,                1},

  // High current internal sensor (0x03), 300A/2048 resolution
  {I2C_HIGH_CURRENT, 0,  int16,     STR_SENSOR_CURR,              UNIT_AMPS,                   1},

  //Powerbox (also mentioned as 0x7D but that is also transmitter frame data)
  {0x0a,             0,  uint16,    STR_SENSOR_BATT1_VOLTAGE,     UNIT_VOLTS,                  2},
  {0x0a,             2,  uint16,    STR_SENSOR_BATT2_VOLTAGE,     UNIT_VOLTS,                  2},
  {0x0a,             4,  uint16,    STR_SENSOR_BATT1_CONSUMPTION, UNIT_MAH,                    0},
  {0x0a,             6,  uint16,    STR_SENSOR_BATT2_CONSUMPTION, UNIT_MAH,                    0},


  // Lap Timer
  {0x0b,             0, uint8,      STR_SENSOR_LAP_NUMBER,        UNIT_RAW,                    0},
  {0x0b,             0, uint8,      STR_SENSOR_GATE_NUMBER,       UNIT_RAW,                    0},
  {0x0b,             0, uint32,     STR_SENSOR_LAP_TIME,          UNIT_SECONDS,                3},
  {0x0b,             0, uint32,     STR_SENSOR_GATE_TIME,         UNIT_SECONDS,                3},

  // Text Generator
  //{I2C_TEXTGEN,      0, uint32,     STR_SENSOR_FLIGHT_MODE,       UNIT_TEXT,                   0},

  // AirSpeed, also has max (+2, int16)
  {0x11,             0,  int16,     STR_SENSOR_ASPD,              UNIT_KMH,                    0},

  // Altitude, also has max (+2, int16)
  {0x12,             0,  int16,     STR_SENSOR_ALT,               UNIT_METERS,                 1},


  // {0x38, strain}

  // G-Force (+min, max)
  {0x14,             0,  int16,     STR_SENSOR_ACCX,              UNIT_G,                      2},
  {0x14,             2,  int16,     STR_SENSOR_ACCY,              UNIT_G,                      2},
  {0x14,             4,  int16,     STR_SENSOR_ACCZ,              UNIT_G,                      2},


  // 0x15,  JETCAT/TURBINE, BCD Encoded values
  // TODO: Add decoding of status information
  // {0x15,             0,  uint8,     STR_SENSOR_STATUS,            UNIT_BITFIELD,               0},
  {0x15,             1,  uint8bcd,  STR_SENSOR_THROTTLE,          UNIT_PERCENT,                0},
  {0x15,             2,  uint16bcd, STR_SENSOR_A1,                UNIT_VOLTS,                  2},
  {0x15,             4,  uint16bcd, STR_SENSOR_A2,                UNIT_VOLTS,                  2},
  {0x15,             6,  uint32bcd, STR_SENSOR_RPM,               UNIT_RPMS,                   0},
  {0x15,             10, uint16bcd, STR_SENSOR_TEMP1,             UNIT_CELSIUS,                0},
  // {0x15,             0,  uint8,     STR_SENSOR_STATUS,            UNIT_BITFIELD,               0},

  // 0x16  GPS LOG
  // Example 0x16:  97 00 | 54 71 12 28 | 40 80 09 82 | 85 14 | 13 | B9
  //                Alt: 009.7, LAT: 28.127154, LON: -82.098040 Course: 148.5, HDOP 1.3 Flags= B9
  {I2C_GPS_LOC,      0,  uint16bcd,  STR_SENSOR_GPSALT,           UNIT_METERS,                 1}, // Atl-Low BCD 3.1
  {I2C_GPS_LOC,      2,  uint32bcd,  STR_SENSOR_GPS,              UNIT_GPS,                    0}, // LAT and LON
  {I2C_GPS_LOC,      10, uint16bcd,  STR_SENSOR_HDG,              UNIT_DEGREE,                 1}, // Course BCD 3.1
  //{I2C_GPS_LOC,      12, uint8bcd,  STR_SENSOR_HDOP ??,         UNIT_RAW,                    1}, // HDOP BCD 1.1

  // 0x17  GPS STAT
  // Example 0x17:  25 00 | 00 28 17 17 | 06 | 01    
  //                Spd:002.5k, TimeUTC:17:17:28.00, Sats: 06, AltH=01
  {I2C_GPS_STAT,      0,  uint16bcd, STR_SENSOR_GSPD,              UNIT_KTS,                   1}, // BCD 3.1
  {I2C_GPS_STAT,      2,  uint32bcd, STR_SENSOR_GPSDATETIME,       UNIT_DATETIME,              1}, // BCD 6.1 HH:MM:SS.S
  {I2C_GPS_STAT,      6,  uint8bcd,  STR_SENSOR_SATELLITES,        UNIT_RAW,                   0}, // BCD 2.0
 
  // 0x19 Jetcat flow rate
  // {0x19,             0,  uint16bcd, STR_SENSOR_FUEL_CONSUMPTION,  UNIT_MILLILITERS_PER_MINUTE, 1}, missing ml/min
  {0x19,             2,  uint32bcd, STR_SENSOR_FUEL,              UNIT_MILLILITERS,            1},

  // 0x1a Gyro
  {0x1a,             0,  int16,     STR_SENSOR_GYROX,             UNIT_DEGREE,                 1},
  {0x1a,             2,  int16,     STR_SENSOR_GYROY,             UNIT_DEGREE,                 1},
  {0x1a,             4,  int16,     STR_SENSOR_GYROZ,             UNIT_DEGREE,                 1},

  // 0x1b Attitude & Mag Compass
  // mag Units are tbd so probably no sensor in existance, ignore them for now
  {0x1b,             0,  int16,     STR_SENSOR_ROLL,              UNIT_DEGREE,                 1},
  {0x1b,             2,  int16,     STR_SENSOR_PITCH,             UNIT_DEGREE,                 1},
  {0x1b,             4,  int16,     STR_SENSOR_YAW,               UNIT_DEGREE,                 1},

  // {0x20, esc},  Smart ESC telemetry
  {I2C_ESC,          0,  uint16,    STR_ESC_RPM,           UNIT_RPMS,                   0},
  {I2C_ESC,          2,  uint16,    STR_ESC_VIN,           UNIT_VOLTS,                  2},
  {I2C_ESC,          4,  uint16,    STR_ESC_TFET,          UNIT_CELSIUS,                1},
  {I2C_ESC,          6,  uint16,    STR_ESC_CUR,           UNIT_MAH,                    1},
  {I2C_ESC,          8,  uint16,    STR_ESC_TBEC,          UNIT_CELSIUS,                1},
  {I2C_ESC,          10, uint8,     STR_ESC_BCUR,          UNIT_AMPS,                   1},
  {I2C_ESC,          11, uint8,     STR_ESC_VBEC,          UNIT_VOLTS,                  2},
  {I2C_ESC,          12, uint8,     STR_ESC_THR,           UNIT_PERCENT,                1},
  {I2C_ESC,          13, uint8,     STR_ESC_POUT,          UNIT_PERCENT,                1},

  // Dual Cell monitor (0x34)
  {0x34,             0,  int16,     STR_SENSOR_BATT1_CURRENT,     UNIT_AMPS,                   1},
  {0x34,             2,  int16,     STR_SENSOR_BATT1_CONSUMPTION, UNIT_MAH,                    1},
  {0x34,             4,  uint16,    STR_SENSOR_BATT1_TEMP,        UNIT_CELSIUS,                1},
  {0x34,             6,  int16,     STR_SENSOR_BATT2_CURRENT,     UNIT_AMPS,                   1},
  {0x34,             8,  int16,     STR_SENSOR_BATT2_CONSUMPTION, UNIT_MAH,                    1},
  {0x34,             10, uint16,    STR_SENSOR_BATT2_TEMP,        UNIT_CELSIUS,                1},

  // Tank pressure + custom input bits (ignore for now)
  //{0x38,             0,  uint16,    STR_SENSOR_STATUS_BITS,       UNIT_BITFIELD,               0},
  //{0x38,             0,  uint16,    STR_SENSOR_PRESSSURE,         UNIT_PSI,                    1},

  // Cells (0x3a)
  {I2C_CELLS,        0,  uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        2,  uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        4,  uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        6,  uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        8,  uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        10, uint16,    STR_SENSOR_CELLS,             UNIT_VOLTS,                  2},
  {I2C_CELLS,        12, uint16,    STR_SENSOR_TEMP2,             UNIT_CELSIUS,                2},

  // Vario-S
  {0x40,             0,  int16,     STR_SENSOR_ALT,               UNIT_METERS,                 1},
  {0x40,             2,  int16,     STR_SENSOR_VSPD,              UNIT_METERS_PER_SECOND,      1},

  // Smartbat
  //{I2C_SMART_BAT_REALTIME,        1,  int8,      STR_SMART_BAT_BTMP,    UNIT_CELSIUS,             0},  // disabled because sensor is a duplicate of cells sensors ones
  {I2C_SMART_BAT_REALTIME,        2,  uint32le,  STR_SMART_BAT_BCUR,    UNIT_MAH,                 0},
  {I2C_SMART_BAT_REALTIME,        6,  uint16le,  STR_SMART_BAT_BCAP,    UNIT_MAH,                 0},
  {I2C_SMART_BAT_REALTIME,        8,  uint16le,  STR_SMART_BAT_MIN_CEL, UNIT_VOLTS,               2},
  {I2C_SMART_BAT_REALTIME,        10,  uint16le, STR_SMART_BAT_MAX_CEL, UNIT_VOLTS,               2},
  //{I2C_SMART_BAT_REALTIME,          12,  uint16le,  "RFU[2]", UNIT_RAW,                 0},   // disabled to save sensors slots

  {I2C_SMART_BAT_CELLS_1_6,       1,  int8,      STR_SMART_BAT_BTMP,   UNIT_CELSIUS,             0},
  {I2C_SMART_BAT_CELLS_1_6,       2,  uint16le,  STR_CL01,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_1_6,       4,  uint16le,  STR_CL02,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_1_6,       6,  uint16le,  STR_CL03,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_1_6,       8,  uint16le,  STR_CL04,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_1_6,       10,  uint16le,  STR_CL05,            UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_1_6,       12, uint16le,  STR_CL06,             UNIT_VOLTS,               2},

  {I2C_SMART_BAT_CELLS_7_12,      1,  int8,      STR_SMART_BAT_BTMP,   UNIT_CELSIUS,             0},
  {I2C_SMART_BAT_CELLS_7_12,      2,  uint16le,  STR_CL07,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_7_12,      4,  uint16le,  STR_CL08,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_7_12,      6,  uint16le,  STR_CL09,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_7_12,      8,  uint16le,  STR_CL10,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_7_12,      10,  uint16le,  STR_CL11,            UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_7_12,      12, uint16le,  STR_CL12,             UNIT_VOLTS,               2},

  {I2C_SMART_BAT_CELLS_13_18,     1,  int8,      STR_SMART_BAT_BTMP,   UNIT_CELSIUS,             0},
  {I2C_SMART_BAT_CELLS_13_18,     2,  uint16le,  STR_CL13,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_13_18,     4,  uint16le,  STR_CL14,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_13_18,     6,  uint16le,  STR_CL15,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_13_18,     8,  uint16le,  STR_CL16,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_13_18,     10, uint16le,  STR_CL17,             UNIT_VOLTS,               2},
  {I2C_SMART_BAT_CELLS_13_18,     12, uint16le,  STR_CL18,             UNIT_VOLTS,               2},

  //{I2C_SMART_BAT_ID,              1,  uint8,  "chemistery",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_ID,              2,  uint8,  "number of cells",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_ID,              3,  uint8,  "manufacturer code",  UNIT_RAW, 0},   // disabled to save sensors slots
  {I2C_SMART_BAT_ID,              4,  uint16le,  STR_SMART_BAT_CYCLES,  UNIT_RAW,                 0},
  //{I2C_SMART_BAT_ID,              6,  uint8,  "uniqueID[8]",  UNIT_RAW, 0},   // disabled to save sensors slots

  //{I2C_SMART_BAT_LIMITS,          1,  uint8,  "rfu",  UNIT_RAW, 0},   // disabled to save sensors slots
  {I2C_SMART_BAT_LIMITS,          2,  uint16le,  STR_SMART_BAT_CAPACITY,UNIT_MAH,                 0},
  //{I2C_SMART_BAT_LIMITS,          4,  uint16le,  "dischargeCurrentRating",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_LIMITS,          6,  uint16le,  "overDischarge_mV",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_LIMITS,          8,  uint16le,  "zeroCapacity_mV",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_LIMITS,          10,  uint16le,  "fullyCharged_mV",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_LIMITS,          12,  uint8,  "minWorkingTemp",  UNIT_RAW, 0},   // disabled to save sensors slots
  //{I2C_SMART_BAT_LIMITS,          13,  uint8,  "maxWorkingTemp",  UNIT_RAW, 0},   // disabled to save sensors slots

  // 0x50-0x56 custom 3rd party sensors
  //{0x50, 0, int16, STR_SENSOR_}

  // 0x7d are transmitter channels frame data [7], probably only available on the Spektrum
  // telemetry bus on the model itself

  // RPM/Volts/Temperature
  {0x7e,             0,  uint16,    STR_SENSOR_RPM,               UNIT_RPMS,                   0},
  {0x7e,             2,  uint16,    STR_SENSOR_A3,                UNIT_VOLTS,                  2},
  {0x7e,             4,  int16,     STR_SENSOR_TEMP2,             UNIT_FAHRENHEIT,             0},

  // 0x7f, QoS DATA, also called Flight Log,, with A, B, L, R, F, H?
  // A - Antenna Fades on Receiver A
  // B - Antenna Fades on Receiver B
  // L - Antenna Fades on left Receiver
  // R - Antenna Fades on right Receiver
  // F - Frame losses
  {I2C_QOS,          0,  uint16,    STR_SENSOR_QOS_A,             UNIT_RAW,                    0},
  {I2C_QOS,          2,  uint16,    STR_SENSOR_QOS_B,             UNIT_RAW,                    0},
  {I2C_QOS,          4,  uint16,    STR_SENSOR_QOS_L,             UNIT_RAW,                    0},
  {I2C_QOS,          6,  uint16,    STR_SENSOR_QOS_R,             UNIT_RAW,                    0},
  {I2C_QOS,          8,  uint16,    STR_SENSOR_QOS_F,             UNIT_RAW,                    0},
  {I2C_QOS,          10, uint16,    STR_SENSOR_QOS_H,             UNIT_RAW,                    0},
  {I2C_QOS,          12, uint16,    STR_SENSOR_A2,                UNIT_VOLTS,                  2},

  {I2C_PSEUDO_TX,    0,  uint8,     STR_SENSOR_TX_RSSI,           UNIT_RAW,                    0},
  {I2C_PSEUDO_TX,    4,  uint32,    STR_SENSOR_BIND,              UNIT_RAW,                    0},
  {I2C_PSEUDO_TX,    8,  uint32,    STR_SENSOR_FLIGHT_MODE,       UNIT_TEXT,                   0},
  {0,                0,  int16,     NULL,                   UNIT_RAW,                    0} //sentinel
};

// Alt Low and High needs to be combined (in 2 diff packets)
static uint8_t gpsAltHigh = 0;

// Helper function declared later
static void processAS3XPacket(const uint8_t *packet);
static void processAlpha6Packet(const uint8_t *packet);
static void adjustTimeFromUTC(uint8_t hour, uint8_t min, uint8_t sec,
                              struct gtm *tp);

#if TEST_CAPTURED_MESSAGE
static uint8_t replaceForTestingPackage(const uint8_t *packet);
#endif

static int32_t bcdToInt8(uint8_t bcd)
{
  return (bcd & 0xf) + 10 * (bcd >> 4 & 0xf);
}

static int32_t bcdToInt16(uint16_t bcd)
{
  return (bcd & 0xf) + 10 * (bcd >> 4 & 0xf) + 100 * (bcd >> 8 & 0xf) +
         1000 * (bcd >> 12 & 0xf);
}

static int32_t bcdToInt32(uint32_t bcd)
{
  return bcdToInt16(bcd >> 16) + 10000 * bcdToInt16(bcd);
}

// Spektrum uses Big Endian data types
static int32_t spektrumGetValue(const uint8_t *packet, int startByte, SpektrumDataType type)
{
  const uint8_t * data = packet + startByte;
  switch (type) {
    case uint8:
      return *((uint8_t *) (data));
    case int8:
      return *((int8_t *) (data));
    case int16:
      return (int16_t) ((uint16_t) (data[1] + (data[0] << 8)));
    case uint16:
      return ((uint16_t) (data[1] + (data[0] << 8)));
    case int32:
      return ((int32_t) (data[3] + (data[2] << 8) + (data[1] << 16) + (data[0] << 24)));
    case uint32:
      return ((uint32_t) (data[3] + (data[2] << 8) + (data[1] << 16) + (data[0] << 24)));
    case uint16bcd:
      return bcdToInt16(*(uint16_t *)data);
    case uint8bcd:
      return bcdToInt8(*(uint8_t *)data);
    case uint32bcd:
      return bcdToInt32(*(uint32_t *)data);
    case uint16le:
      return (int16_t) ((uint16_t) (data[0] + (data[1] << 8)));
    case uint32le:
      return ((uint32_t) (data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24)));
    default:
      return -1;
  }
}

bool isSpektrumValidValue(int32_t value, const SpektrumDataType type)
{
  switch (type) {
    case uint16:
      return value != 0xffff;
    case int16:
      return value != 0x7fff;
    case int32:
      return value != 0x7fffffff;
    case uint32:
      return ((uint32_t) value) != 0xffffffff;
    default:
      return true;
  }
}

void processSpektrumPacket(const uint8_t *packet)
{
  setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, I2C_PSEUDO_TX_RSSI, 0, 0,
                    packet[1], UNIT_RAW, 0);
  // highest bit indicates that TM1100 is in use, ignore it
  uint8_t i2cAddress = (packet[2] & 0x7f);

#if TEST_CAPTURED_MESSAGE
  // Only for Testing when we don't have the sensor, but have the data
  if (i2cAddress == I2C_FLITECTRL) {
    i2cAddress = replaceForTestingPackage(packet);
  }
#endif

  if (i2cAddress == I2C_FWD_PGM) {
#if defined(LUA) && defined(MULTIMODULE)
    // Forward Programming
    if (Multi_Buffer && memcmp(Multi_Buffer, "DSM", 3) == 0) {
      // Multi_Buffer[0..2]=="DSM" -> Lua script is running
      // Multi_Buffer[3]==0x70 -> TX to RX data ready to be sent
      // Multi_Buffer[4..9]=6 bytes of TX to RX data
      // Multi_Buffer[10..25]=16 bytes of RX to TX data
      Multi_Buffer[10] = i2cAddress;
      memcpy(&Multi_Buffer[11], &packet[3], 15); // Store the received RX answer in the buffer
    }
#endif
    return; // Not a sensor
  }

#if defined(LUA)
    // Generic way for LUA Script to request ANY Specktrum Telemety Raw Data
    // this can be used for TextGen or any other telemetry message

    if (Multi_Buffer && Multi_Buffer[3]==i2cAddress && Multi_Buffer[4]==0 && 
        memcmp(Multi_Buffer, "STR", 3) == 0) {
      // Multi_Buffer[0..2]=="STR" -> Lua script is running
      // Multi_Buffer[3]==i2C Addr -> I2C address of data requested in Lua script
      // Multi_Buffer[4]== Write Semaphore -> 0=Can Write Data. > 0, data not yet consumed
      // Multi_Buffer[5..20]=15 bytes of RX to TX data (any other data after i2c address)

      // Concurrency note: only going to write if Multi_Buffer[4]==0, that means that the
      // Lua script is ready for more data. Whithout this "semaphore", the lua script was getting messages
      // with mix of the old and new data. 

      memcpy(&Multi_Buffer[5], &packet[3], 16); // Store the received RX answer in the buffer
      Multi_Buffer[4]=i2cAddress; // Tell lua script data is ready
    }
#endif

    // SmartBat Hack
    if (i2cAddress == I2C_SMART_BAT_BASE_ADDRESS) {
      // use type to create virtual I2CAddresses
      i2cAddress = i2cAddress + (packet[4] >> 4);
    }

  uint8_t instance = packet[3];

  if (i2cAddress == I2C_TEXTGEN) {
    // TextGen now accessed via the new "Spectrum Telemetry Raw" LUA method

    return;  // Not a sensor
  }

  // AS3X + SAFE information: Flight Mode
  if (i2cAddress == I2C_FLITECTRL) {
    processAS3XPacket(packet);
    // Continue for backward compatibility with scripts using 05XX sensors
  }

  // Alpha6 Flight Controller (Blade Helis): Flight Mode
  if (i2cAddress == I2C_ALPHA6) {
    processAlpha6Packet(packet);
    // Continue for backward compatibility with scripts using 24XX sensors
  }

  bool handled = false;
  for (const SpektrumSensor * sensor = spektrumSensors; sensor->i2caddress; sensor++) {
    uint16_t pseudoId = (sensor->i2caddress << 8 | sensor->startByte);

    if (i2cAddress == sensor->i2caddress) {
      handled = true;

      // Extract value, skip header
      int32_t value =
          spektrumGetValue(packet + 4, sensor->startByte, sensor->dataType);

      if (!isSpektrumValidValue(value, sensor->dataType))
        continue;

      // mV to VOLT PREC2 for Smart Batteries
      if ((i2cAddress >= I2C_SMART_BAT_REALTIME &&
           i2cAddress <= I2C_SMART_BAT_LIMITS) &&
          sensor->unit == UNIT_VOLTS) {
        if (value == -1) {
          continue;  // discard unavailable sensors
        } else {
          value = value / 10;
        }
      }

      // RPM, 10RPM (0-655340 RPM)
      if (i2cAddress == I2C_ESC && sensor->unit == UNIT_RPMS) {
        value = value / 10;
      }

      // Current, 10mA (0-655.34A)
      if (i2cAddress == I2C_ESC && sensor->startByte == 6) {
        value = value / 10;
      }

      // BEC Current, 100mA (0-25.4A)
      if (i2cAddress == I2C_ESC && sensor->startByte == 10) {
        value = value / 10;
      }

      // Throttle 0.5% (0-127%)
      if (i2cAddress == I2C_ESC && sensor->startByte == 12) {
        value = value / 2;
      }

      // Power 0.5% (0-127%)
      if (i2cAddress == I2C_ESC && sensor->startByte == 13) {
        value = value / 2;
      }

      if (i2cAddress == I2C_CELLS && sensor->unit == UNIT_VOLTS) {
        // Map to FrSky style cell values
        int cellIndex = (sensor->startByte / 2) << 16;
        value = value | cellIndex;
      }

      if (sensor->i2caddress == I2C_HIGH_CURRENT && sensor->unit == UNIT_AMPS) {
        // Spektrum's documents talks says: Resolution: 300A/2048 = 0.196791
        // A/tick Note that 300/2048 = 0,1464. DeviationTX also uses the
        // 0.196791 figure
        value = value * 196791 / 100000;
      }

      // Check if this looks like a LemonRX Transceiver, they use QoS Frame loss A as RSSI indicator(0-100)
      if (i2cAddress == I2C_QOS && sensor->startByte == 0) {
        if (spektrumGetValue(packet + 4, 2, uint16) == 0x8000 &&
            spektrumGetValue(packet + 4, 4, uint16) == 0x8000 &&
            spektrumGetValue(packet + 4, 6, uint16) == 0x8000 &&
            spektrumGetValue(packet + 4, 8, uint16) == 0x8000) {
          telemetryData.rssi.set(value);
        }
        else {
          // Otherwise use the received signal strength of the telemetry packet as indicator
          // Range is 0-31, multiply by 3 to get an almost full reading for 0x1f, the maximum the cyrf chip reports
          telemetryData.rssi.set(packet[1] * 3);
        }
        telemetryStreaming = TELEMETRY_TIMEOUT10ms;
      }

      //*********** GPS STAT *********************************
      // Example 0x17:  0  1    2  3  4  5    6    7
      //                25 00 | 00 28 15 17 | 06 | 01    
      //                Spd:002.5k, TimeUTC:17:15:28.00, Sats: 06, AltH=01

      if (sensor->i2caddress == I2C_GPS_STAT && sensor->unit == UNIT_DATETIME) {
        const uint8_t *packetData = packet + 4;  // Skip the header

        uint8_t sec = bcdToInt8(packetData[3]);
        uint8_t min = bcdToInt8(packetData[4]);
        uint8_t hour = bcdToInt8(packetData[5]);

        struct gtm td;
        adjustTimeFromUTC(hour, min, sec, &td);

        // Depending on the last byte it SETS:  DATE: HEX (YYMMDD01)   TIME:
        // HEX(HHMMSS00)
        value = (td.tm_hour << 24) + (td.tm_min << 16) + (td.tm_sec << 8);
        setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance,
                          value, UNIT_DATETIME, 0);

        value = ((td.tm_year + 1900 - 2000) << 24) + ((td.tm_mon + 1) << 16) +
                (td.tm_mday << 8) + 1;
        setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance,
                          value, UNIT_DATETIME, 0);

        // Get Altitude High since we need to combine it with Alt-Low
        // Save the high part for later (0-99)
        gpsAltHigh = bcdToInt8(packetData[7]);

        continue;  // setTelemetryValue handled
      }

      //*********** GPS LOC *********************************
      // Example 0x16:  0  1    2  3  4  5    6  7  8  9    10 11   12   13
      //                97 00 | 54 71 12 28 | 40 80 09 82 | 85 14 | 13 | B9
      //                Alt: 009.7, LAT: 28o 12'7154, LON: -82 09 8040 Course: 148.5, HDOP 1.3 Flags= B9

      // ALTITUDE LOW (METERS)
      if (sensor->i2caddress == I2C_GPS_LOC && sensor->startByte == 0) {
        uint8_t gpsFlags = packet[4 + 13];

        // Format Decimal: HHLLLL, for display will be HHLLL.L
        value = (gpsAltHigh * 1000) + value;

        if (gpsFlags & GPS_INFO_FLAGS_NEGATIVE_ALT) {
          value = -value;
        }
      }

      // Process LAT and LOG together
      if (sensor->i2caddress == I2C_GPS_LOC && sensor->unit == UNIT_GPS) {
        const uint8_t *packetData = packet + 4;  // Skip the header

        uint8_t gpsFlags = packetData[13];

        // LATITUDE
        uint16_t fmin =
            bcdToInt8(packetData[2]) + (bcdToInt8(packetData[3]) * 100);
        uint8_t min = bcdToInt8(packetData[4]);
        uint8_t deg = bcdToInt8(packetData[5]);

        // formula from code in gps.cpp
        value = deg * 1000000UL + (min * 100000UL + fmin * 10UL) / 6;

        if ((gpsFlags & GPS_INFO_FLAGS_IS_NORTH) == 0) {  // SOUTH, negative
          value = -value;
        }
        setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance,
                          value, UNIT_GPS_LATITUDE, 0);

        // LONGITUDE
        fmin = bcdToInt8(packetData[6]) + (bcdToInt8(packetData[7]) * 100);
        min = bcdToInt8(packetData[8]);
        deg = bcdToInt8(packetData[9]);

        // formula from code in gps.cpp
        value = deg * 1000000UL + (min * 100000UL + fmin * 10UL) / 6;

        if ((gpsFlags & GPS_INFO_FLAGS_IS_EAST) == 0) {  // WEST, negative
          value = -value;
        }
        setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance,
                          value, UNIT_GPS_LONGITUDE, 0);

        continue;  // setTelemetryValue handled
      }

      setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance, value, sensor->unit, sensor->precision);
    }
  }
  if (!handled) {
    // If we see a sensor that is not handled at all, add the raw values of this sensor to show its existance to
    // the user and help debugging/implementing these sensors
    for (int startByte=0; startByte<14; startByte+=2) {
      int32_t value = spektrumGetValue(packet + 4, startByte, uint16);
      uint16_t pseudoId = i2cAddress << 8 | startByte;
      setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, pseudoId, 0, instance, value, UNIT_RAW, 0);
    }
  }
}

// Parse the DSM2 bind response, Fields are as per http://www.rcgroups.com/forums/showpost.php?p=35692146&postcount=5191
// "I"  here means the multi module

/*
0-3   4 bytes -> Cyrf ID of the TX xor 0xFF but don't care...
4     1 byte -> RX version but don't care...
5     1 byte -> number of channels, example 0x06=6 channels
6     1 byte -> max DSM type allowed:
        0x01 => 1024 DSM2 1 packet => number of channels is <8 and no telemetry
        0x02 => 1024 DSM2 2 packets => either a number of channel >7 or telemetry enable RX
        0x12 => 2048 DSM2 2 packets => can be any number of channels with/without telemetry -> this mode might be supported following Mike's trials, note the channels should be duplicated between the packets which is not the case today
        0xa2 => 2048 DSMX 1 packet => number of channels is <8 and no telemetry
        0xb2 => 2048 DSMX 2 packets => can be any number of channels with/without telemetry -> this mode is only half supported since the channels should be duplicated between the packets which is not the case but might be supported following Mike's trials
7     0x00: not sure of the use of this byte since I've always seen it at 0...
8-9   2 bytes CRC but don't care...

 Examples:           DSM   #Chan  RXver
 Inductrix           0xa2   07     1
 LemonRX+Sat+tele    0xb2   07     1

 */
void processDSMBindPacket(uint8_t module, const uint8_t *packet)
{
  uint32_t debugval;

  if (g_model.moduleData[module].type == MODULE_TYPE_LEMON_DSMP) {

    // save flags
    g_model.moduleData[module].dsmp.flags = packet[0];

    // save number of channels
    uint8_t channels = packet[2];
    if (channels > 12) { channels = 12; }
    g_model.moduleData[module].channelsCount = channels - 8;

    TRACE("[SPK] DSMP bind packet: 0x%X / %i",
          packet[0] & 0x3F, packet[2]);

    storageDirty(EE_MODEL);
    restartModule(EXTERNAL_MODULE);
    
  } else if (g_model.moduleData[module].type == MODULE_TYPE_MULTIMODULE &&
             g_model.moduleData[module].multi.rfProtocol ==
             MODULE_SUBTYPE_MULTI_DSM2 &&
             g_model.moduleData[module].subType == MM_RF_DSM2_SUBTYPE_AUTO) {

    // Only sets channel etc when in DSM/AUTO mode
    int channels = packet[5];
    if (channels > 12) {
      channels = 12;
    }
    else if (channels < 3) {
      channels = 3;
    }

    switch(packet[6]) {
      case 0xa2:
        g_model.moduleData[module].subType = MM_RF_DSM2_SUBTYPE_DSMX_22;
        break;
      case 0x12:
        g_model.moduleData[module].subType = MM_RF_DSM2_SUBTYPE_DSM2_11;
        if (channels == 7) {
          channels = 12;    // change the number of channels if 7
        }
        break;
      case 0x01:
      case 0x02:
        g_model.moduleData[module].subType = MM_RF_DSM2_SUBTYPE_DSM2_22;
        break;
      default: // 0xb2 or unknown
        g_model.moduleData[module].subType = MM_RF_DSM2_SUBTYPE_DSMX_11;
        if (channels == 7) {
          channels = 12;    // change the number of channels if 7
        }
        break;
    }

    g_model.moduleData[module].channelsCount = channels - 8;
    // clear the 11ms servo refresh rate flag
    g_model.moduleData[module].multi.optionValue &= 0xFD;

    storageDirty(EE_MODEL);
  }

  debugval = packet[7] << 24 | packet[6] << 16 | packet[5] << 8 | packet[4];

  /* log the bind packet as telemetry for quick debugging */
  setTelemetryValue(PROTOCOL_TELEMETRY_SPEKTRUM, I2C_PSEUDO_TX_BIND, 0, 0,
                    debugval, UNIT_RAW, 0);

  /* Finally stop binding as the rx just told us that it is bound */
  if (getModuleMode(module) == MODULE_MODE_BIND) {
    auto module_type = g_model.moduleData[module].type;
#if defined(MULTIMODULE)
    if (module_type == MODULE_TYPE_MULTIMODULE &&
        g_model.moduleData[module].multi.rfProtocol ==
            MODULE_SUBTYPE_MULTI_DSM2) {
      setMultiBindStatus(module, MULTI_BIND_FINISHED);
    } else
#endif
    if (module_type == MODULE_TYPE_LEMON_DSMP) {
      setModuleMode(module, MODULE_MODE_NORMAL);
    }
  }
}
  
void processSpektrumTelemetryData(uint8_t module, uint8_t data,
                                  uint8_t *rxBuffer, uint8_t &rxBufferCount)
{
  if (rxBufferCount == 0 && data != 0xAA) {
    TRACE("[SPK] invalid start byte 0x%02X", data);
    return;
  }

  if (rxBufferCount < TELEMETRY_RX_PACKET_SIZE) {
    rxBuffer[rxBufferCount++] = data;
  }
  else {
    TRACE("[SPK] array size %d error", rxBufferCount);
    rxBufferCount = 0;
  }

  if (rxBuffer[1] == 0x80 && rxBufferCount >= DSM_BIND_PACKET_LENGTH) {
    processDSMBindPacket(module, rxBuffer+2);
    rxBufferCount = 0;
    return;
  }

  if (rxBufferCount >= SPEKTRUM_TELEMETRY_LENGTH) {
    // Debug print content of Telemetry to console
#if 0
    debugPrintf("[SPK] Packet 0x%02X rssi 0x%02X: ic2 0x%02x, %02x: ",
                rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3]);
    for (int i=4; i<SPEKTRUM_TELEMETRY_LENGTH; i+=4) {
      debugPrintf("%02X%02X %02X%02X  ", rxBuffer[i], rxBuffer[i + 1],
                  rxBuffer[i + 2], rxBuffer[i + 3]);
    }
    debugPrintf(CRLF);
#endif
    processSpektrumPacket(rxBuffer);
    rxBufferCount = 0;
  }
}

const SpektrumSensor *getSpektrumSensor(uint16_t pseudoId)
{
  uint8_t startByte = (uint8_t) (pseudoId & 0xff);
  uint8_t i2cadd = (uint8_t) (pseudoId >> 8);
  for (const SpektrumSensor * sensor = spektrumSensors; sensor->i2caddress; sensor++) {
    if (i2cadd == sensor->i2caddress && startByte == sensor->startByte) {
      return sensor;
    }
  }
  return nullptr;
}

void spektrumSetDefault(int index, uint16_t id, uint8_t subId, uint8_t instance)
{
  TelemetrySensor &telemetrySensor = g_model.telemetrySensors[index];
  telemetrySensor.id = id;
  telemetrySensor.subId = subId;
  telemetrySensor.instance = instance;

  const SpektrumSensor *sensor = getSpektrumSensor(id);
  if (sensor) {
    TelemetryUnit unit = sensor->unit;
    uint8_t prec = min<uint8_t>(2, sensor->precision);
    telemetrySensor.init(sensor->name, unit, prec);

    if (unit == UNIT_RPMS) {
      telemetrySensor.custom.ratio = 1;
      telemetrySensor.custom.offset = 1;
    }
    else if (unit == UNIT_FAHRENHEIT) {
      if (!IS_IMPERIAL_ENABLE()) {
        telemetrySensor.unit = UNIT_CELSIUS;
      }

    }
    else if (unit == UNIT_METERS) {
      if (IS_IMPERIAL_ENABLE()) {
        telemetrySensor.unit = UNIT_FEET;
      }
    }
  }
  else {
    telemetrySensor.init(id);
  }

  storageDirty(EE_MODEL);
}

extern int __offtime(const gtime_t *t, long int offset, struct gtm *tp);

static void adjustTimeFromUTC(uint8_t hour, uint8_t min, uint8_t sec,
                              struct gtm *tp)
{
  // Get current UTC date/time
  __offtime(&g_rtcTime, -g_eeGeneral.timezone * 3600, tp);

  tp->tm_hour = hour;
  tp->tm_min = min;
  tp->tm_sec = sec;

  gtime_t newTime = gmktime(tp);
  __offtime(&newTime, +g_eeGeneral.timezone * 3600, tp);
}

// AS3X/SAFE flight Controller
// Contains Flight Mode, and Gyro Settings
static void processAS3XPacket(const uint8_t *packet)
{
    const uint8_t *packetData = packet + 4;  // Skip the header

    // uint8_t  state = packetData[1];
    uint8_t flags = packetData[0];
    // uint8_t  dataPage = packetData[3] & 0x0F;
    //  0=Gains, 1=Headings, 2=Angle Limits
    uint8_t flightMode = packetData[2] & 0x0F;

    char text[50];

    sprintf(text, "%d ", flightMode + 1);

    if (flags & FLITECTRL_FLAGS_IS_AS3X_STAB) {
    strcat(text, "AS3X");
    }

    // only one should show
    if (flags & FLITECTRL_FLAGS_IS_ANGLE_DEMAND) {
    strcat(text, " Level");
    } else if (flags & FLITECTRL_FLAGS_IS_SAFE_ENVELOPE) {
    strcat(text, " Envelope");
    } else if (flags & FLITECTRL_FLAGS_IS_AS3X_HEADING) {
    strcat(text, " Heading");
    }

    setTelemetryText(PROTOCOL_TELEMETRY_SPEKTRUM, I2C_PSEUDO_TX_FM, 0, 0, text);
}

// Alpha6 flight Controller  (Blade Helis based on AR636)
// Contains Flight Mode, and Gyro Settings
static void processAlpha6Packet(const uint8_t *packet)
{
  const uint8_t *packetData = packet + 4;  // Skip the header

  uint8_t status = packetData[2] & 0x0F;
  uint8_t flightMode = packetData[2] >> 4 & 0x0F;

  char text[50];

  sprintf(text, "%d ", flightMode);

  if (flightMode == 0) {
    strcat(text, "NOR");
  } else if (flightMode == 1) {
    strcat(text, "INT");
  } else if (flightMode == 2) {
    strcat(text, "ADV");
  } else if (flightMode == 5) {
    strcat(text, "PANIC");
  }

  if (status == 2) {
    strcat(text, " HOLD");
  }

  setTelemetryText(PROTOCOL_TELEMETRY_SPEKTRUM, I2C_PSEUDO_TX_FM, 0, 0, text);
}

#if TEST_CAPTURED_MESSAGE
// For Testing purposes, replace the package for data captured
bool testflag = false;
static uint8_t replaceForTestingPackage(const uint8_t *packet)
{
  // *********** GPS LOC *********************************
  // Example 0x16:          0  1    2  3  4  5    6  7  8  9    10 11   12   13
  //                16 00 | 97 00 | 54 71 12 28 | 40 80 09 82 | 85 14 | 13 | B9
  //                Alt: 009.7, LAT: 28o 12'7154, LON: -82 09 8040 Course: 148.5, HDOP 1.3 Flags= B9
  const char test16data[] = {0x16, 0x00, 0x97, 0x00, 0x54, 0x71, 0x12, 0x28,
                             0x40, 0x80, 0x09, 0x82, 0x85, 0x14, 0x13, 0xB9};

  // *********** GPS STAT *********************************
  // Example 0x17:          0  1    2  3  4  5    6    7
  //                17 00 | 25 00 | 00 28 18 21 | 06 | 00    
  //                Spd:002.5k, TimeUTC:21:18:28.00, Sats: 06, AltH=00
  const char test17data[] = {0x17, 0x00, 0x25, 0x00, 0x00,
                             0x28, 0x18, 0x21, 0x06, 0x00};

  if (testflag) {
    memcpy((char *)packet + 2, test16data, 16);
  } else {
    memcpy((char *)packet + 2, test17data, 10);
  }

  testflag = !testflag;

  return packet[2] & 0x7ff;
}
#endif
