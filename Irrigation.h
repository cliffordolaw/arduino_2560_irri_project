#ifndef IRRIGATION_H
#define IRRIGATION_H

#include <Arduino.h>
#include "Config.h"
#include "Pins.h"
#include "Sim900.h"
// forward declare to avoid circular include with EepromStore
struct PersistedIrrigation;

struct IrrigationCommand {
  bool valid;
  long id;
  uint8_t zones[ZONES_MAX];
  uint8_t numZones;
  uint8_t totalMinutes;     // T
  uint8_t remainingMinutes; // M
  uint8_t status;           // S

  IrrigationCommand(): valid(false), id(-1), numZones(0),
      totalMinutes(0), remainingMinutes(0), status(0xFF) 
  {
    for (uint8_t i = 0; i < ZONES_MAX; i++) zones[i] = 0;
  }
};

class IrrigationManager {
public:
  explicit IrrigationManager(Sim900Client& modem);

  void begin(); // resume from EEPROM if available
  void tick();  // timers, periodic persistence, etc.
  void onServerCommand(const IrrigationCommand& cmd); // handle new command

private:
  enum RunState {
    Idle,
    Running
  };

  void startSlaveOnly(const IrrigationCommand& cmd);  // S=0 -> S=1
  void startMasterBoth(const IrrigationCommand& cmd); // S=1 -> S=2
  void stopMasterComplete(const IrrigationCommand& cmd); // -> S=3
  void stopSlaveComplete(const IrrigationCommand& cmd);  // -> S=4
  void stopMasterImmediate(const IrrigationCommand& cmd); // S=5 -> S=6
  void stopSlaveAfterMaster(const IrrigationCommand& cmd); // S=6 -> S=7

  void applyZones(const IrrigationCommand& cmd, bool on);
  bool roleHasAnyZone(const IrrigationCommand& cmd) const;
  void persist(bool active);
  bool restore();

  Sim900Client& sim;
  RunState state;
  IrrigationCommand currentCmd;
  uint32_t remainingSeconds;
  uint32_t lastTickMs;
  uint32_t lastPersistMs;
};

#endif

