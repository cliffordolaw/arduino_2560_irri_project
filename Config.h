#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Role selection: set ROLE to ROLE_MASTER or ROLE_SLAVE
#define ROLE_MASTER 1
#define ROLE_SLAVE 2

#ifndef ROLE
#define ROLE ROLE_MASTER
#endif

// SIM900 serial pins (SoftwareSerial)
// SIM900 TX -> Arduino RX (pin 10), SIM900 RX -> Arduino TX (pin 11)
#define SIM900_RX_PIN 10
#define SIM900_TX_PIN 11

// Network/APN
static const char APN[] = "iot.1nce.net";

// URLs
static const char MASTER_URL[] = "http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Mercato";
static const char SLAVE_URL[]  = "http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Mercato&c=slave";

// Status update endpoint (set to a valid base URL to enable updates)
// SIM900 supports HTTP (not HTTPS) reliably, so use http://
// Endpoint accepts: id, password, m (remaining minutes), s (status), c (constant=20)
static const char STATUS_UPDATE_BASE[] = "http://guasertemp.online/irrigazione/irrigazione.php";
static const char STATUS_UPDATE_PASSWORD[] = "segreta";
static const int STATUS_UPDATE_CONST_C = 20;

// Pin mapping
// Index 0 is unused so zones map naturally 1..10.
static const uint8_t ZONES_MAX = 10;
static const int8_t zoneToPinMaster[ZONES_MAX + 1] = {
  -1, 22, 23, 24, 25, 26, 27, -1, -1, -1, 28
};
static const int8_t zoneToPinSlave[ZONES_MAX + 1] = {
  -1, -1, -1, -1, -1, -1, -1, 29, 30, 31, -1
};
// Pump pin (master only; unused on slave)
static const int PUMP_PIN = 32;

// Role name
#if ROLE == ROLE_MASTER
#define ROLE_NAME "MASTER"
#define ROLE_URL MASTER_URL
static const int8_t* const ACTIVE_ZONE_TO_PIN = zoneToPinMaster;
#elif ROLE == ROLE_SLAVE
static const int8_t* const ACTIVE_ZONE_TO_PIN = zoneToPinSlave;
#define ROLE_NAME "SLAVE"
#define ROLE_URL SLAVE_URL
#else
#define ROLE_NAME "UNKNOWN"
#error "ROLE must be defined as ROLE_MASTER or ROLE_SLAVE"
#endif

// Timings
static const unsigned long SERIAL_BAUD = 9600;
static const unsigned long SIM900_BAUD = 9600;
static const unsigned long POLL_INTERVAL_MS = 8000;

// Error handling
// Error LED pin (default to onboard LED). Changeable here.
static const uint8_t ERROR_LED_PIN = LED_BUILTIN;
// Timeout waiting for Serial to become ready (some boards support !Serial)
static const unsigned long SERIAL_READY_TIMEOUT_MS = 2000;

#endif


