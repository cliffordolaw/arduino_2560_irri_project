#ifndef PARSER_H
#define PARSER_H

#include <Arduino.h>
#include "Pins.h"
#include "Config.h"
// #include "Sim900.h"

struct IrrigationCommand {
  bool valid;
  long id;
  uint8_t zones[ZONES_MAX]; // list of zone IDs
  uint8_t numZones;         // count of entries in zones[]
  uint8_t totalMinutes; // T
  uint8_t remainingMinutes; // M
  uint8_t status;       // S
};

IrrigationCommand parsePayload(const String& payload);


#endif


