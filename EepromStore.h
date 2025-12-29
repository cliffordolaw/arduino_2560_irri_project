#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"

struct PersistedIrrigation {
  uint16_t magic;
  uint8_t version;
  uint8_t active;            // 1 if an irrigation is active
  long irrigationId;
  uint8_t status;
  uint32_t remainingSeconds;
  uint8_t role;              // ROLE_MASTER or ROLE_SLAVE
  uint8_t numZones;
  uint8_t zones[ZONES_MAX];
  uint16_t checksum;
};

class EepromStore {
public:
  static bool load(PersistedIrrigation& out);
  static void save(const PersistedIrrigation& data);
  static void clear();

private:
  static uint16_t computeChecksum(const PersistedIrrigation& data);
  static const int kAddress = 0;
  static const uint16_t kMagic = 0xA51C;
  static const uint8_t kVersion = 1;
};

#endif


