#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Role selection: set ROLE to one of:
#define ROLE_MASTER  1   // Arduino 2 (pump-only master/coordinator)
#define ROLE_SLAVE1  2   // Arduino 1 (zones + its own pump as zone 5)
#define ROLE_SLAVE2  3   // Arduino 3 (zones-only)

#ifndef ROLE
#define ROLE ROLE_SLAVE1
#endif

// SIM900 serial pins (SoftwareSerial)
// SIM900 TX -> Arduino RX (pin 11), SIM900 RX -> Arduino TX (pin 10)
#define SIM900_RX_PIN 11
#define SIM900_TX_PIN 10

// Network/APN
static const char APN[] = "iot.1nce.net";

// URLs (read and update use the same endpoint, differentiated by role param)
static const char MASTER_URL[]  = "http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Isola&c=master";
static const char SLAVE1_URL[]  = "http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Isola&c=slave1";
static const char SLAVE2_URL[]  = "http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Isola&c=slave2";

// Status update: same base as ROLE_URL; parameters appended: id, s (status), m (remaining)

// Pin mapping
// Index 0 is unused so zones map naturally 1..10.
static const uint8_t ZONES_MAX = 10;
// Master (pump-only): no zones mapped by default (-1)
static const int8_t zoneToPinMaster[ZONES_MAX + 1] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
// Slave1 (zones + pump as zone 5): map zone 5 to pump pin
static const int8_t zoneToPinSlave1[ZONES_MAX + 1] = {
  -1, 31, 29, 27, 26, /*z5*/ 32, 24, -1, -1, -1, 28
};
// Slave2 (zones-only; example mapping 7,8,9)
static const int8_t zoneToPinSlave2[ZONES_MAX + 1] = {
  -1, -1, -1, -1, -1, -1, -1, 29, 30, 31, -1
};
// Pump pin (physical relay)
static const int PUMP_PIN = 32;
// Treat Slave1 pump as zone number:
static const uint8_t PUMP_ZONE_SLAVE1 = 5;

// Role name
#if ROLE == ROLE_MASTER
#define ROLE_NAME "MASTER"
#define ROLE_URL MASTER_URL
static const int8_t* const ACTIVE_ZONE_TO_PIN = zoneToPinMaster;
#elif ROLE == ROLE_SLAVE1
#define ROLE_NAME "SLAVE1"
#define ROLE_URL SLAVE1_URL
static const int8_t* const ACTIVE_ZONE_TO_PIN = zoneToPinSlave1;
#elif ROLE == ROLE_SLAVE2
#define ROLE_NAME "SLAVE2"
#define ROLE_URL SLAVE2_URL
static const int8_t* const ACTIVE_ZONE_TO_PIN = zoneToPinSlave2;
#else
#define ROLE_NAME "UNKNOWN"
#error "ROLE must be defined as ROLE_MASTER or ROLE_SLAVE1 or ROLE_SLAVE2"
#endif

// Timings
static const unsigned long SERIAL_BAUD = 9600;
static const unsigned long SIM900_BAUD = 9600;
static const unsigned long POLL_INTERVAL_MS = 60000;

// Error handling
// Error LED pin (default to onboard LED). Changeable here.
static const uint8_t ERROR_LED_PIN = LED_BUILTIN;
// Timeout waiting for Serial to become ready (some boards support !Serial)
static const unsigned long SERIAL_READY_TIMEOUT_MS = 2000;

#endif


