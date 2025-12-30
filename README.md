# Arduino Mega 2560 Irrigation (Master/Slave) with SIM900

Two-board irrigation controller (Master/Slave) using Arduino Mega 2560 and SIM900. The design prioritizes:
- Non-blocking scheduling via `millis()` (no `delay()` in app logic)
- EEPROM persistence to resume irrigation after power loss
- Clear master/slave role behavior and server-driven coordination

## 1) Scope and Goals
- Two boards: Master and Slave, remote, each with SIM900 on D10/D11.
- Both periodically poll a URL for the current irrigation command:
  - Master: `http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Mercato`
  - Slave:  `http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=Mercato&c=slave`
- Parse payload like: `ID=199;Z=1,3,10;T=1;M=1;S=1` where:
  - `ID`: irrigation id
  - `Z`: zones (1..10). Master controls zones 1,2,3,4,5,6,10. Slave controls 7,8,9.
  - Note: The implementation uses a fixed-size zones array sized by `ZONES_MAX` (currently 10). To support more zones, increase `ZONES_MAX` in `Pins.h` and adjust parsing limits if needed.

  - `T`: total minutes duration; `M`: remaining minutes
  - `S`: status code (see state machine below)
- Use EEPROM to save in-progress irrigation so it resumes after power loss.
- Avoid blocking delays in app logic; use `millis()` timers. SIM900 interactions will be wrapped to avoid `delay()` where possible.

## 2) Hardware
- Arduino Mega 2560 x2.
- SIM900 wired to Mega via SoftwareSerial:
  - Mega D10 = SIM900 TX (Arduino RX)
  - Mega D11 = SIM900 RX (Arduino TX)
  - APN: `iot.1nce.net` (as provided)
- Pump is switched by the Master only (relay on a digital pin). Zones are each tied to one digital pin.
- Exact zone→pin mapping and pump pin will be defined in `Config.h`.

## 3) Status Codes and Role Behavior
Status meanings (server/board contract):
0 = irrigation pending
1 = started by slave only
2 = started by both (in progress)
3 = completed by master
4 = fully completed (master+slave)
5 = stop immediately (dashboard STOP)
6 = stopped by master
7 = fully stopped (master+slave)

Role rules (simplified):
- Slave:
  - On S=0 and has any zones in Z: turn on its zones, update `S=1` (server).
  - On S=3: turn off its zones, update `S=4`.
  - On S=6 (STOP flow): turn off, update `S=7`.
- Master:
  - If irrigation only affects master zones, server sets `S=1` automatically (no slave action).
  - On S=1 and master has zones in Z: turn on pump + master zones, start timer, update `S=2`.
  - When duration finishes: turn off everything, update `S=3`.
  - On S=5 (STOP): turn off everything immediately, update `S=6`.

Both boards ignore unknown IDs or statuses that do not require action for their role.

## 4) Polling and Timing (non-blocking)
- A scheduler runs via `millis()`:
  - `pollIntervalMs` (e.g., 5–10s): when elapsed, perform HTTP GET via SIM900 wrapper.
  - `irrigationTimer`: tracks remaining time (ms). No `delay()`.
  - Periodically (e.g., every 15–30s) persist progress to EEPROM.
- SIM900 wrapper will avoid `delay()` by:
  - Table-driven AT-command state machine; each state defines entry action, timeout, next-on-timeout, and next-on-complete.
  - `begin()` immediately starts bearer setup; `startGet(url)` only runs the HTTP portion.
  - Error state auto-retries bearer after a short delay.
  - Short waits implemented via `millis()` checks rather than blocking delays.

## 5) EEPROM Persistence
Saved structure:
- Magic/version/checksum for integrity
- `active` flag
- Embedded `IrrigationCommand cmd` snapshot (ID, status, zones list)
- `remainingSeconds`
- `role` (master/slave)

On boot:
1) `EepromStore::load()`; if `active` and checksum ok, `IrrigationManager::restore()` reapplies outputs and resumes.
2) Otherwise start normal polling.

We update EEPROM:
- When irrigation starts (S=1 or S=2 depending on role)
- Periodically during irrigation (remainingSeconds)
- When stopping/completing (clear `active`)

## 6) Networking (SIM900)
- APN: `iot.1nce.net`
- AT sequence (non-blocking):
  - `AT+SAPBR=0,1`
  - `AT+SAPBR=3,1,"Contype","GPRS"`
  - `AT+SAPBR=3,1,"APN","iot.1nce.net"`
  - `AT+CGATT=1`
  - `AT+SAPBR=1,1`
  - `AT+HTTPINIT`
  - `AT+HTTPPARA="CID",1`
  - `AT+HTTPPARA="URL","<url>"`
  - `AT+HTTPACTION=0` (GET), wait for result
  - `AT+HTTPREAD` and parse using the announced length after `+HTTPREAD:<len>`

Status updates:
- `ParserServer::sendStatusUpdate(id, s, m)` queues a status update; the SIM900 client sends it when idle.
- Update endpoint base is configured in `Config.h` (`STATUS_UPDATE_BASE`, `STATUS_UPDATE_PASSWORD`, constant `c=20`).

## 7) File Layout (minimal, modular)
- `arduino_2560_irrigation_proj.ino` — minimal setup/loop calling into the modules
- `Config.h` — role selection, pin map, APN, URLs, timings
- `Pins.h` — zone→pin mapping helpers and pump pin accessors
- `Irrigation.h/.cpp` — role state machines, timers, orchestration; defines `IrrigationCommand`
- `EepromStore.h/.cpp` — persistence of in-progress irrigation
- `Sim900.h/.cpp` — SIM900 driver and HTTP GET (table-driven stepper), parser and status-update helpers under `ParserServer`

Notes:
- Keep modules small and readable. No blocking `delay()` in module logic.
- Master/slave selected at build via `Config.h` macro.

## 8) Configuration Needed at deployment
- Confirm zone→pin mapping for Master and Slave
- Confirm pump relay pin on Master
- Configure status update base URL/password in `Config.h` if using updates
- Preferred poll interval (default 5–10s)
