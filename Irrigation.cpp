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
  //first turn everything off before restoring from eeprom 
  // ensure outputs off
  for (uint8_t z = 1; z <= ZONES_MAX; z++) zoneOff(z);
  if (ROLE == ROLE_MASTER) pumpOff();
  
  //restore from eeprom
  if (restore()) {
    LOG("Resumed irr. ID="); LOG(currentCmd.id); LOG(" status="); LOG(currentCmd.status);
    LOG(" remaining="); LOG(remainingSeconds); LOGln("s");
    // Re-apply outputs for role
    if (ROLE == ROLE_MASTER && (currentCmd.status == 3)) {
      pumpOn();
      applyZones(currentCmd, true);
      state = Running;
    } else if (ROLE == ROLE_SLAVE1 && (currentCmd.status == 1 || currentCmd.status == 2 || currentCmd.status == 3)) {
      applyZones(currentCmd, true);
      state = Running;
    } else if (ROLE == ROLE_SLAVE2 && (currentCmd.status == 2 || currentCmd.status == 3)) {
      applyZones(currentCmd, true);
      state = Running;
    } else {
      state = Idle;
    }
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
  //Update remaining minutes in real time 
  if (state == Running && (remainingSeconds % 60) == 0) {
    ParserServer::sendStatusUpdate(currentCmd.id, currentCmd.status, (uint8_t) (remainingSeconds/60));
  }
  //if 15 seconds have passed since last persist, persist the current state
  if (now - lastPersistMs >= 120000) {
    lastPersistMs = now;
    persist(true);
  }
  //if remaining seconds is 0, stop the irrigation
  if (remainingSeconds == 0) {
    // Time's up -> owner stops
    if (ROLE == ROLE_MASTER) {
      // Master-owned timing (pump=2 scenario)
      stopMasterComplete(currentCmd);   
    } else if (ROLE == ROLE_SLAVE1) {
      // Slave1-owned timing when its pump zone is active
      stopSlaveComplete(currentCmd);
    } else if (ROLE == ROLE_SLAVE2) {
      stopSlaveComplete(currentCmd);
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
  currentCmd.status = 1; // started by slave1 only
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
  currentCmd.status = 3; // in progress (master + slaves)
  currentCmd.numZones = cmd.numZones;
  for (uint8_t i = 0; i < currentCmd.numZones; i++) currentCmd.zones[i] = cmd.zones[i];
  remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
  state = Running;
  persist(true);
  ParserServer::sendStatusUpdate(currentCmd.id, 3, cmd.remainingMinutes);
}

void IrrigationManager::stopMasterComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  if (ROLE == ROLE_MASTER) pumpOff();
  currentCmd.status = 4; // completed by master
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 4, 0);
}

void IrrigationManager::stopSlaveComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  // If master already completed (4), slave1 completion -> 5, slave2 both -> 6
  if (ROLE == ROLE_SLAVE1) {
    currentCmd.status = 5;
  } else if (ROLE == ROLE_SLAVE2) {
    // can't know if slave1 done; use 6 as terminal if only zones on slave2
    currentCmd.status = 6;
  } else {
    currentCmd.status = 6;
  }
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, currentCmd.status, 0);
}

void IrrigationManager::stopMasterImmediate(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  if (ROLE == ROLE_MASTER) pumpOff();
  currentCmd.status = 8; // stopped by master
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, 8, 0);
}

void IrrigationManager::stopSlaveAfterMaster(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  // after STOP: slave1 -> 9, slave2 -> 10
  if (ROLE == ROLE_SLAVE1) currentCmd.status = 9; else currentCmd.status = 10;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(currentCmd.id, currentCmd.status, 0);
}

void IrrigationManager::onServerCommand(const IrrigationCommand& cmd) {
  if (!cmd.valid) return;
  
  auto hasZone = [&](uint8_t z)->bool{
    for (uint8_t i=0;i<cmd.numZones;i++) if (cmd.zones[i]==z) return true;
    return false;
  };
  bool pumpIsSlave1 = hasZone(PUMP_ZONE_SLAVE1);

  if (ROLE == ROLE_MASTER) {
    // STOP takes precedence
    if (cmd.status == 7) {
      stopMasterImmediate(cmd);
      return;
    }
    // Start pump if MASTER is pump owner (i.e., no zone5 in list)
    if ((cmd.status == 1 || cmd.status == 2) && !pumpIsSlave1) {
      startMasterBoth(cmd); // -> 3
      return;
    }
    if (cmd.status == 0) {
      // Do nothing; wait for slave to start if zones involve slave
      return;
    }
  } else if (ROLE == ROLE_SLAVE1) {
    if (cmd.status == 0 && roleHasAnyZone(cmd)) {
      startSlaveOnly(cmd); // -> 1
      return;
    }
    if (cmd.status == 4 && state == Running) {
      stopSlaveComplete(cmd); // -> 5
      return;
    }
    if (cmd.status == 8 && state == Running) {
      stopSlaveAfterMaster(cmd); // -> 9
      return;
    }
  } else if (ROLE == ROLE_SLAVE2) {
    if (cmd.status == 1 && roleHasAnyZone(cmd)) {
      // join after slave1: -> 2
      applyZones(cmd, true);
      currentCmd = cmd;
      currentCmd.status = 2;
      remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
      state = Running;
      persist(true);
      ParserServer::sendStatusUpdate(currentCmd.id, 2, cmd.remainingMinutes);
      return;
    }
    if (cmd.status == 4 && state == Running) {
      stopSlaveComplete(cmd); // -> 6
      return;
    }
    if (cmd.status == 8 && state == Running) {
      stopSlaveAfterMaster(cmd); // -> 10
      return;
    }
  }
}


