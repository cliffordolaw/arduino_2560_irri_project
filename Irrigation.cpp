#include "Irrigation.h"

IrrigationManager::IrrigationManager(Sim900Client& modem)
  : sim(modem),
    state(Idle),
    currentId(-1),
    currentStatus(0xFF),
    zonesCount(0),
    remainingSeconds(0),
    lastTickMs(0),
    lastPersistMs(0) {
}

void IrrigationManager::begin() {
  PersistedIrrigation s;
  if (EepromStore::load(s)) {
    restore(s);
    Serial.print("Resumed irrigation ID=");
    Serial.print(currentId);
    Serial.print(" status=");
    Serial.print(currentStatus);
    Serial.print(" remaining=");
    Serial.print(remainingSeconds);
    Serial.println("s");
    // Re-apply outputs for role
    IrrigationCommand cmd;
    cmd.valid = true;
    cmd.id = currentId;
    cmd.numZones = zonesCount;
    for (uint8_t i = 0; i < zonesCount; i++) cmd.zones[i] = zones[i];
    cmd.totalMinutes = 0;
    cmd.remainingMinutes = 0;
    cmd.status = currentStatus;
    if (ROLE == ROLE_MASTER && (currentStatus == 2)) {
      pumpOn();
      applyZones(cmd, true);
      state = Running;
    } else if (ROLE == ROLE_SLAVE && (currentStatus == 1 || currentStatus == 2)) {
      applyZones(cmd, true);
      state = Running;
    } else {
      state = Idle;
    }
  } else {
    // ensure outputs off
    for (uint8_t z = 1; z <= ZONES_MAX; z++) zoneOff(z);
    #if ROLE == ROLE_MASTER
    pumpOff();
    #endif
  }
  lastTickMs = millis();
  lastPersistMs = lastTickMs;
}

void IrrigationManager::tick() {
  if (state != Running) return;
  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    if (remainingSeconds > 0) remainingSeconds--;
  }
  if (now - lastPersistMs >= 15000) {
    lastPersistMs = now;
    persist(true);
  }
  if (remainingSeconds == 0) {
    // Time's up -> master stops, slave will stop upon S=3
    if (ROLE == ROLE_MASTER) {
      IrrigationCommand cmd;
      cmd.valid = true;
      cmd.id = currentId;
      cmd.numZones = zonesCount;
      for (uint8_t i = 0; i < zonesCount; i++) cmd.zones[i] = zones[i];
      stopMasterComplete(cmd);
    }
  }
}

bool IrrigationManager::roleHasAnyZone(const IrrigationCommand& cmd) const {
  for (uint8_t i = 0; i < cmd.numZones; i++) {
    int p = getZonePin(cmd.zones[i]);
    if (p >= 0) return true;
  }
  return false;
}

void IrrigationManager::applyZones(const IrrigationCommand& cmd, bool on) {
  for (uint8_t i = 0; i < cmd.numZones; i++) {
    uint8_t z = cmd.zones[i];
    int p = getZonePin(z);
    if (p >= 0) {
      if (on) zoneOn(z); else zoneOff(z);
    }
  }
}

void IrrigationManager::persist(bool active) {
  PersistedIrrigation s;
  s.active = active ? 1 : 0;
  s.irrigationId = currentId;
  s.status = currentStatus;
  s.remainingSeconds = remainingSeconds;
  s.role = ROLE;
  s.numZones = zonesCount;
  for (uint8_t i = 0; i < zonesCount; i++) s.zones[i] = zones[i];
  EepromStore::save(s);
}

void IrrigationManager::restore(const PersistedIrrigation& s) {
  currentId = s.irrigationId;
  currentStatus = s.status;
  remainingSeconds = s.remainingSeconds;
  zonesCount = s.numZones;
  for (uint8_t i = 0; i < zonesCount; i++) zones[i] = s.zones[i];
}

void IrrigationManager::startSlaveOnly(const IrrigationCommand& cmd) {
  if (!roleHasAnyZone(cmd)) return;
  applyZones(cmd, true);
  currentId = cmd.id;
  currentStatus = 1;
  zonesCount = cmd.numZones;
  for (uint8_t i = 0; i < zonesCount; i++) zones[i] = cmd.zones[i];
  remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
  state = Running;
  persist(true);
  ParserServer::sendStatusUpdate(sim, cmd.id, 1, cmd.remainingMinutes);
}

void IrrigationManager::startMasterBoth(const IrrigationCommand& cmd) {
  if (!roleHasAnyZone(cmd)) return;
  #if ROLE == ROLE_MASTER
  pumpOn();
  #endif
  applyZones(cmd, true);
  currentId = cmd.id;
  currentStatus = 2;
  zonesCount = cmd.numZones;
  for (uint8_t i = 0; i < zonesCount; i++) zones[i] = cmd.zones[i];
  remainingSeconds = (uint32_t)cmd.remainingMinutes * 60UL;
  state = Running;
  persist(true);
  ParserServer::sendStatusUpdate(sim, cmd.id, 2, cmd.remainingMinutes);
}

void IrrigationManager::stopMasterComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  #if ROLE == ROLE_MASTER
  pumpOff();
  #endif
  currentStatus = 3;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(sim, cmd.id, 3, 0);
}

void IrrigationManager::stopSlaveComplete(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  currentStatus = 4;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(sim, cmd.id, 4, 0);
}

void IrrigationManager::stopMasterImmediate(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  #if ROLE == ROLE_MASTER
  pumpOff();
  #endif
  currentStatus = 6;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(sim, cmd.id, 6, 0);
}

void IrrigationManager::stopSlaveAfterMaster(const IrrigationCommand& cmd) {
  applyZones(cmd, false);
  currentStatus = 7;
  state = Idle;
  persist(false);
  ParserServer::sendStatusUpdate(sim, cmd.id, 7, 0);
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


