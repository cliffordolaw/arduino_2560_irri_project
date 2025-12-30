#include "Irrigation.h"
#include "EepromStore.h"

#define LOG(x) Serial.print(x)
#define LOGln(x) Serial.println(x)

IrrigationManager::IrrigationManager(Sim900Client& modem)
  : sim(modem),
    state(Idle),
    currentCmd(),
    remainingSeconds(0),
    lastTickMs(0),
    lastPersistMs(0) {
}

void IrrigationManager::begin() {
  if (restore()) {
    LOG("Resumed irr. ID="); LOG(currentCmd.id); LOG(" status="); LOG(currentCmd.status);
    LOG(" remaining="); LOG(remainingSeconds); LOGln("s");
    // Re-apply outputs for role
    if (ROLE == ROLE_MASTER && (currentCmd.status == 2)) {
      pumpOn();
      applyZones(currentCmd, true);
      state = Running;
    } else if (ROLE == ROLE_SLAVE && (currentCmd.status == 1 || currentCmd.status == 2)) {
      applyZones(currentCmd, true);
      state = Running;
    } else {
      state = Idle;
    }
  } else {
    // ensure outputs off
    for (uint8_t z = 1; z <= ZONES_MAX; z++) zoneOff(z);
    if (ROLE == ROLE_MASTER) pumpOff();
  }
  lastTickMs = millis();
  lastPersistMs = lastTickMs;
}

void IrrigationManager::tick() {
  if (state != Running) return;
  //if irrigation is running, 
  //check if 1 second has passed since last tick, if so, decrement remaining seconds
  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    if (remainingSeconds > 0) remainingSeconds--;
  }
  //if 15 seconds have passed since last persist, persist the current state
  if (now - lastPersistMs >= 15000) {
    lastPersistMs = now;
    persist(true);
  }
  //if remaining seconds is 0, stop the irrigation
  if (remainingSeconds == 0) {
    // Time's up -> master stops, slave will stop upon S=3
    if (ROLE == ROLE_MASTER) {
      stopMasterComplete(currentCmd);
    }
  }
}

//Returns true if the command has any zones that are controlled by the current role
bool IrrigationManager::roleHasAnyZone(const IrrigationCommand& cmd) const {
  for (uint8_t i = 0; i < cmd.numZones; i++) {
    int p = getZonePin(cmd.zones[i]);
    if (p >= 0) return true;
  }
  return false;
}

//Applies the zones in the command to the current role
void IrrigationManager::applyZones(const IrrigationCommand& cmd, bool on) {
  for (uint8_t i = 0; i < cmd.numZones; i++) {
    uint8_t z = cmd.zones[i];
    int p = getZonePin(z);
    if (p >= 0) {
      if (on) zoneOn(z); 
      else zoneOff(z);
    }
  }
}

void IrrigationManager::persist(bool active) {
  PersistedIrrigation s;
  s.active = active ? 1 : 0;
  s.cmd = currentCmd;
  s.remainingSeconds = remainingSeconds;
  s.role = ROLE;
  EepromStore::save(s);
}

bool IrrigationManager::restore() {
  PersistedIrrigation s;
  if (!EepromStore::load(s)) return false;
  currentCmd = s.cmd;
  currentCmd.valid = true; 
  if (currentCmd.numZones > ZONES_MAX) currentCmd.numZones = ZONES_MAX;
  remainingSeconds = s.remainingSeconds;
  return true;
}

void IrrigationManager::startSlaveOnly(const IrrigationCommand& cmd) {
  if (!roleHasAnyZone(cmd)) return;
  applyZones(cmd, true);
  currentCmd.id = cmd.id;
  currentCmd.status = 1;
  currentCmd.numZones = cmd.numZones;
  for (uint8_t i = 0; i < currentCmd.numZones; i++) currentCmd.zones[i] = cmd.zones[i];

  remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
  state = Running;
  persist(true);
  ParserServer::sendStatusUpdate(currentCmd.id, 1, cmd.remainingMinutes);
}

void IrrigationManager::startMasterBoth(const IrrigationCommand& cmd) {
  if (!roleHasAnyZone(cmd)) return;
  if (ROLE == ROLE_MASTER) pumpOn();

  applyZones(cmd, true);
  currentCmd.id = cmd.id;
  currentCmd.status = 2;
  currentCmd.numZones = cmd.numZones;
  for (uint8_t i = 0; i < currentCmd.numZones; i++) currentCmd.zones[i] = cmd.zones[i];
  remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
  state = Running;
  persist(true);
  ParserServer::sendStatusUpdate(currentCmd.id, 2, cmd.remainingMinutes);
}

void IrrigationManager::stopMasterComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  if (ROLE == ROLE_MASTER) pumpOff();
  currentCmd.status = 3;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 3, 0);
}

void IrrigationManager::stopSlaveComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  currentCmd.status = 4;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 4, 0);
}

void IrrigationManager::stopMasterImmediate(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  if (ROLE == ROLE_MASTER) pumpOff();
  currentCmd.status = 6;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 6, 0);
}

void IrrigationManager::stopSlaveAfterMaster(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  currentCmd.status = 7;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 7, 0);
}

void IrrigationManager::onServerCommand(const IrrigationCommand& cmd) {
  if (!cmd.valid) return;
  
  if (ROLE == ROLE_MASTER) {
    // STOP takes precedence
    if (cmd.status == 5) {
      stopMasterImmediate(cmd);
      return;
    }
    if (cmd.status == 1 && roleHasAnyZone(cmd)) {
      startMasterBoth(cmd); // S=1 -> S=2
      return;
    }
    if (cmd.status == 0) {
      // Do nothing; wait for slave to start if zones involve slave
      return;
    }
  } else { // SLAVE
    if (cmd.status == 0 && roleHasAnyZone(cmd)) {
      startSlaveOnly(cmd); // S=0 -> S=1
      return;
    }
    if (cmd.status == 3 && state == Running) {
      stopSlaveComplete(cmd); // -> S=4
      return;
    }
    if (cmd.status == 6 && state == Running) {
      stopSlaveAfterMaster(cmd); // -> S=7
      return;
    }
  }
}


