#ifndef PINS_H
#define PINS_H

#include <Arduino.h>
#include "Config.h"

static inline int getZonePin(uint8_t zone) {
  if (zone == 0 || zone > ZONES_MAX) return false;
  return ACTIVE_ZONE_TO_PIN[zone];
}

static inline void initPinsForRole(uint8_t role) {
  // Initialize only the active role's zone pins as OUTPUT LOW 
  for (uint8_t z = 1; z <= ZONES_MAX; z++) {
    int p = ACTIVE_ZONE_TO_PIN[z];
    if (p >= 0) {
      pinMode(p, OUTPUT);
      digitalWrite(p, LOW);
    }
  }
  // Pump (master)
  #if ROLE == ROLE_MASTER
  {
    pinMode(PUMP_PIN, OUTPUT); 
    digitalWrite(PUMP_PIN, LOW);
  }
  #endif
}

static inline void zoneOn(uint8_t zone) {
  int p = getZonePin(zone);
  if (p >= 0) digitalWrite(p, HIGH);
}

static inline void zoneOff(uint8_t zone) {
  int p = getZonePin(zone);
  if (p >= 0) digitalWrite(p, LOW);
}

static inline void pumpOn() {
  digitalWrite(PUMP_PIN, HIGH);
}

static inline void pumpOff() {
  digitalWrite(PUMP_PIN, LOW);
}

#endif


