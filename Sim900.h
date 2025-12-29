#ifndef SIM900_H
#define SIM900_H
#include "Parser.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "Config.h"

class Sim900Client {
public:
  Sim900Client();
  void begin(Stream& serialRef);
  void loop();

  bool isIdle() const;
  bool startGet(const char* url);

  bool hasNewResponse() const;
  String takeResponse();

  // Test helper: only handles polling/HTTP GET scheduling and poll logs
  int pollAndProcess(IrrigationCommand& cmd);

private:
  struct StateDef;

  enum State {
    Idle,
    StartBearer0,
    SetContype,
    SetApn,
    Attach,
    StartBearer1,
    HttpInit,
    HttpCid,
    HttpUrl,
    HttpAction,
    HttpRead,
    Error
  };

  void changeState(State s, const char* reason);
  void sendCmd(const String& cmd);
  void readIntoBuffer();
  void clearBuffer();
  const StateDef& defFor(State s) const;

  // per-state entry handlers
  void enter_StartBearer0();
  void enter_SetContype();
  void enter_SetApn();
  void enter_Attach();
  void enter_StartBearer1();
  void enter_HttpInit();
  void enter_HttpCid();
  void enter_HttpUrl();
  void enter_HttpAction();
  void enter_HttpRead();
  void enter_NoOp(); // for Idle/Error

  Stream* sim;
  State state;
  unsigned long stateSince;
  unsigned long stateTimeout;
  String buffer;
  String currentUrl;
  String lastBody;
  bool newResponse;
  unsigned long nextPollAt;

  // State table definition
  struct StateDef {
    const char* name;
    void (Sim900Client::*onEnter)();
    unsigned long timeoutMs;
    State onTimeout;
    State onComplete;
    const char* expectedToken; // token that marks completion in buffer
  };
  static const StateDef STATE_TABLE[];
};



namespace ParserServer {
  inline String buildStatusUrl(long id, uint8_t status, uint8_t remainingMinutes) {
    if (strlen(STATUS_UPDATE_BASE) == 0) return String();
    String url = STATUS_UPDATE_BASE;
    url += "?id="; url += id;
    url += "&password="; url += STATUS_UPDATE_PASSWORD;
    url += "&m="; url += remainingMinutes;
    url += "&s="; url += status;
    url += "&c="; url += STATUS_UPDATE_CONST_C;
    return url;
  }

  inline void sendStatusUpdate(Sim900Client& client, long id, uint8_t status, uint8_t remainingMinutes) {
    String url = buildStatusUrl(id, status, remainingMinutes);
    if (url.length() == 0) {
      Serial.println("Status update skipped: STATUS_UPDATE_BASE not configured.");
      return;
    }
    if (client.isIdle()) {
      client.startGet(url.c_str());
      Serial.print("Status update requested: "); Serial.println(url);
    } else {
      Serial.println("Status update skipped: modem busy.");
    }
  }
}

#endif


