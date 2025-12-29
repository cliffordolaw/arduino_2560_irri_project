#include "EepromStore.h"

uint16_t EepromStore::computeChecksum(const PersistedIrrigation& data) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&data);
  size_t len = sizeof(PersistedIrrigation) - sizeof(uint16_t);
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum = (sum << 1) ^ p[i];
  }
  return sum;
}

bool EepromStore::load(PersistedIrrigation& out) {
  EEPROM.get(kAddress, out);
  if (out.magic != kMagic || out.version != kVersion) return false;
  uint16_t expect = computeChecksum(out);
  if (out.checksum != expect) return false;
  return out.active == 1;
}

void EepromStore::save(const PersistedIrrigation& data) {
  PersistedIrrigation temp = data;
  temp.magic = kMagic;
  temp.version = kVersion;
  temp.checksum = computeChecksum(temp);
  EEPROM.put(kAddress, temp);
}

void EepromStore::clear() {
  PersistedIrrigation empty = {};
  empty.magic = kMagic;
  empty.version = kVersion;
  empty.active = 0;
  empty.checksum = computeChecksum(empty);
  EEPROM.put(kAddress, empty);
}


