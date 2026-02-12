# Endpoint testing cheat sheet

Quick curl commands to exercise the server as MASTER, SLAVE1, and SLAVE2, and to post status updates. Replace placeholders as needed.

## Setup

```bash
# Adjust as needed
export LUOGO="Petriglieri-Virduzzo"
export BASE_READ="http://guasertemp.online/irrigazione/leggiirrigazione.php?luogo=${LUOGO}"

# Update (status posting)
export UPDATE_BASE="http://guasertemp.online/irrigazione/irrigazione.php"
export UPDATE_PASSWORD="segreta"
export UPDATE_CONST_C="20"
```

## Read commands (polling)

```bash
# MASTER poll
curl -s "${BASE_READ}&c=master"

# SLAVE1 poll (Arduino 1: zones + pump-as-zone5)
curl -s "${BASE_READ}&c=slave1"

# SLAVE2 poll (Arduino 3: zones-only)
curl -s "${BASE_READ}&c=slave2"
```

- Expected response format (example):
  - `ID=199;Z=1,3,10;T=1;M=1;S=1`
  - Meaning:
    - `ID`: irrigation id
    - `Z`: comma-separated zones for this role (on SLAVE1, zone 5 implies pump=1)
    - `T`: total minutes
    - `M`: remaining minutes
    - `S`: status (see map below)

export ID=285
export M=1
export S=5

## Post status updates (same for all roles)

```bash
# Template:
#   id=<id>    s=<status>   m=<remaining_minutes>   c=20   password=segreta
curl -s "${UPDATE_BASE}?id={$ID}&password=${UPDATE_PASSWORD}&m={$M}&s={$S}&c=${UPDATE_CONST_C}"

# Examples:
# 1) SLAVE1 started (S=1), remaining 10 min
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=10&s=1&c=${UPDATE_CONST_C}"

# 2) SLAVE2 joins (S=2), remaining 9 min
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=9&s=2&c=${UPDATE_CONST_C}"

# 3) IN PROGRESS (MASTER + slaves) (S=3), remaining 8 min
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=8&s=3&c=${UPDATE_CONST_C}"

# 4) MASTER completed (S=4)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=4&c=${UPDATE_CONST_C}"

# 5) SLAVE1 completed (S=5)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=5&c=${UPDATE_CONST_C}"

# 6) Fully completed (MASTER+SLAVE1+SLAVE2) (S=6)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=6&c=${UPDATE_CONST_C}"

# 7) STOP immediately (dashboard) (S=7)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=7&c=${UPDATE_CONST_C}"

# 8/9/10) Stopped states (master/slave1/slave2)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=8&c=${UPDATE_CONST_C}"   # stopped by master
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=9&c=${UPDATE_CONST_C}"   # stopped (master+slave1)
curl -s "${UPDATE_BASE}?id=123&password=${UPDATE_PASSWORD}&m=0&s=10&c=${UPDATE_CONST_C}"  # fully stopped (master+slave1+slave2)
```

## Status map (S)

- 0: pending
- 1: started by slave1 only
- 2: started by slave1 and slave2
- 3: in progress (master + slave1/2)
- 4: completed by master
- 5: completed by master and slave1
- 6: fully completed (master + slave1 + slave2)
- 7: stop immediately
- 8: stopped by master
- 9: stopped (master + slave1)
- 10: fully stopped (master + slave1 + slave2)

## Notes

- SLAVE1 pump is treated as zone 5: when `Z` contains 5, SLAVE1 turns its pump ON/OFF with that zone and owns timing for that run.
- Reads use per-role URLs; updates always go to `irrigazione.php` with `id/s/m/password/c=20`.
- Use small `m` values during testing to observe state transitions quickly.




Server: 
ID=286;Z=1,2,3,4,5,6,7,8,9;T=2;M=2;S=0

device(slave1): 
S=1

Server: 
ID=286;Z=1,2,3,4,5,6,7,8,9;T=2;M=2;S=1


