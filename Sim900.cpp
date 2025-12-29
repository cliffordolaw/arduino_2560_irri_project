#include "Sim900.h"
#include "Parser.h"
#include "Pins.h"

Sim900Client::Sim900Client()
  : sim(NULL),
    state(Idle),
    stateSince(0),
    stateTimeout(0),
    newResponse(false),
    nextPollAt(0) {
}

void Sim900Client::begin(Stream& serialRef) {
  sim = &serialRef;
  state = Idle;
  stateSince = millis();
  // Kick off bearer setup immediately on init
  changeState(StartBearer0, "init");
}

bool Sim900Client::isIdle() const {
  return state == Idle;
}

void Sim900Client::changeState(State s, const char* reason) {
  State prev = state;
  state = s;
  stateSince = millis();
  stateTimeout = defFor(s).timeoutMs;
  clearBuffer();

  // Log the state change
  // if (Serial) {
    Serial.print("State: ");
    Serial.print(defFor(prev).name);
    Serial.print(" -> ");
    Serial.print(defFor(s).name);
    Serial.print(" (");
    if (reason) Serial.print(reason);
    Serial.println(")");
  // }

  // Call entry action
  void (Sim900Client::*enter)() = defFor(s).onEnter;
  if (enter) {
    (this->*enter)();
  }
}

void Sim900Client::sendCmd(const String& cmd) {
  if (!sim) return;
  sim->println(cmd);
}

void Sim900Client::readIntoBuffer() {
  if (!sim) return;
  while (sim->available()) {
    char c = (char)sim->read();
    buffer += c;
  }
}

void Sim900Client::clearBuffer() {
  buffer = "";
}

bool Sim900Client::startGet(const char* url) {
  if (!isIdle()) return false;
  currentUrl = url;
  newResponse = false;
  lastBody = "";
  // Jump directly into HTTP operation sequence
  changeState(HttpInit, "startGet");
  return true;
}

bool Sim900Client::hasNewResponse() const {
  return newResponse;
}

String Sim900Client::takeResponse() {
  newResponse = false;
  return lastBody;
}

void Sim900Client::loop() {
  readIntoBuffer();
  
  const StateDef& def = defFor(state);

  // Completion detection
  if (def.expectedToken && buffer.indexOf(def.expectedToken) >= 0) {
    // Special handling for HttpRead: parse "+HTTPREAD:<len>" and wait for full body
    if (state == HttpRead) {
      int idx = buffer.indexOf("+HTTPREAD:");
      if (idx >= 0) {
        int nl = buffer.indexOf('\n', idx);
        if (nl > 0) {
          int colon = buffer.indexOf(':', idx);
          if (colon > 0 && colon < nl) {
            String lenStr = buffer.substring(colon + 1, nl);
            lenStr.trim();
            long bodyLen = lenStr.toInt();
            int bodyStart = nl + 1;
            if (bodyLen >= 0 && (int)buffer.length() >= bodyStart + (int)bodyLen) {
              lastBody = buffer.substring(bodyStart, bodyStart + bodyLen);
              newResponse = true;
              changeState(def.onComplete, "complete");
              return;
            }
          }
        }
      }
      // Not enough data yet; keep waiting
    } else {
      changeState(def.onComplete, "complete");
      return;
    }
  }

  // Timeout handling
  if (stateTimeout > 0 && (millis() - stateSince) > stateTimeout) {
    // Serial.print("Changing to timeout state: ");
    // Serial.println(def.onTimeout);
    // Serial.println("FROM state: ");
    // Serial.println(state);
    // delay(1000);
    changeState(def.onTimeout, "timeout");
    return;
  }
}

int Sim900Client::pollAndProcess(IrrigationCommand& cmd) {
  unsigned long now = millis();
  if (now >= nextPollAt) {
    Serial.print("["); Serial.print(ROLE_NAME); Serial.print("] Polling: "); Serial.println(ROLE_URL);
    if (isIdle() && !hasNewResponse()){
      startGet(ROLE_URL);
    } else {
      Serial.println("--> skipped: modem busy/error state.");
    }
    nextPollAt = now + POLL_INTERVAL_MS;
  }

  if (hasNewResponse()) {
    String body = takeResponse();
    Serial.print("[");
    Serial.print(ROLE_NAME);
    Serial.println("] HTTP body:");
    Serial.println(body);

    cmd = parsePayload(body);
    if (!cmd.valid) {
      Serial.println("Parse failed or empty command.");
      return -1;
    }

    Serial.print("ID="); Serial.print(cmd.id);
    Serial.print(" T="); Serial.print(cmd.totalMinutes);
    Serial.print(" M="); Serial.print(cmd.remainingMinutes);
    Serial.print(" S="); Serial.println(cmd.status);

    Serial.print(" Zones["); Serial.print(cmd.numZones); Serial.println("]:");
    for (uint8_t i = 0; i < cmd.numZones; i++) {
      uint8_t z = cmd.zones[i];
      int pin = getZonePin(z);
      Serial.print("  - Zone "); Serial.print(z);
      Serial.print(" -> pin "); Serial.println(pin);
    }

    return 0; 
  }
  return -2;
}

// --- State table and entry actions ---
void Sim900Client::enter_NoOp() {/* nothing */}
void Sim900Client::enter_StartBearer0() { sendCmd("AT+SAPBR=0,1"); }
void Sim900Client::enter_SetContype() { sendCmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\""); }
void Sim900Client::enter_SetApn() { sendCmd(String("AT+SAPBR=3,1,\"APN\",\"") + APN + "\""); }
void Sim900Client::enter_Attach() { sendCmd("AT+CGATT=1"); }
void Sim900Client::enter_StartBearer1() { sendCmd("AT+SAPBR=1,1"); }
void Sim900Client::enter_HttpInit() { sendCmd("AT+HTTPINIT");}
void Sim900Client::enter_HttpCid() { sendCmd("AT+HTTPPARA=\"CID\",1"); }
void Sim900Client::enter_HttpUrl() { sendCmd(String("AT+HTTPPARA=\"URL\",\"") + currentUrl + "\""); }
void Sim900Client::enter_HttpAction() { sendCmd("AT+HTTPACTION=0"); }
void Sim900Client::enter_HttpRead() { sendCmd("AT+HTTPREAD"); }

const Sim900Client::StateDef& Sim900Client::defFor(State s) const {
  return STATE_TABLE[(int)s];
}

// name, onEnter, timeoutMs, onTimeout, onComplete, expectedToken
const Sim900Client::StateDef Sim900Client::STATE_TABLE[] = {
  { "Idle",         &Sim900Client::enter_NoOp,      0,        Error,      Idle,        NULL },
  { "StartBearer0", &Sim900Client::enter_StartBearer0, 5000,  Error,      SetContype,  "OK" },
  { "SetContype",   &Sim900Client::enter_SetContype,   3000,  Error,      SetApn,      "OK" },
  { "SetApn",       &Sim900Client::enter_SetApn,       5000,  Error,      Attach,      "OK" },
  { "Attach",       &Sim900Client::enter_Attach,       10000, Error,      StartBearer1,"OK" },
  { "StartBearer1", &Sim900Client::enter_StartBearer1, 10000, Error,      HttpInit,    "OK" },
  { "HttpInit",     &Sim900Client::enter_HttpInit,     3000,  Error,      HttpCid,     "OK" },
  { "HttpCid",      &Sim900Client::enter_HttpCid,      3000,  Error,      HttpUrl,     "OK" },
  { "HttpUrl",      &Sim900Client::enter_HttpUrl,      3000,  Error,      HttpAction,  "OK" },
  { "HttpAction",   &Sim900Client::enter_HttpAction,   3000,  Error,      HttpRead,    "OK" },
  { "HttpRead",     &Sim900Client::enter_HttpRead,     10000,  Error,      Idle,        "+HTTPREAD:" },
  { "Error",        &Sim900Client::enter_NoOp,         5000,  StartBearer0, Error,     NULL }
  //When error state times out, it will transition to StartBearer0 to retry the connection
};


