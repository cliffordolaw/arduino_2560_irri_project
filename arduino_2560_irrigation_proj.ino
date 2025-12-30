// Arduino Mega 2560 Irrigation

#include <SoftwareSerial.h>
#include "Config.h"
#include "Pins.h"
#include "Sim900.h"
#include "Irrigation.h"

SoftwareSerial sim900ss(SIM900_RX_PIN, SIM900_TX_PIN);
Sim900Client sim900Client;
IrrigationManager irrigation(sim900Client);


static void criticalError(const char* msg); 

void setup() {
  Serial.begin(SERIAL_BAUD);
  sim900ss.begin(SIM900_BAUD);
  // Prepare error LED early
  pinMode(ERROR_LED_PIN, OUTPUT);
  digitalWrite(ERROR_LED_PIN, LOW);
  // Wait briefly for Serial to become ready (on boards that support it)
  unsigned long start = millis();
  while (!Serial && (millis() - start) < SERIAL_READY_TIMEOUT_MS) { /* wait */ }
  if (!Serial) {
    criticalError("Serial not ready");
  }

  initPinsForRole(ROLE);
  sim900Client.begin(sim900ss);
  // sim900Client.begin(Serial); //NOTE: This is for testing purposes only, SoftwareSerial is used for the actual hardware.
  irrigation.begin();
  Serial.println("Irrigation program ready [ROLE=" + String(ROLE_NAME) + "]");
}

void loop() {
  sim900Client.loop();
  IrrigationCommand cmd;
  int result = sim900Client.pollAndProcess(cmd);
  if (result == 0) irrigation.onServerCommand(cmd);
  irrigation.tick();
}

static void criticalError(const char* msg) {
  // Turn on error LED and halt
  pinMode(ERROR_LED_PIN, OUTPUT);
  digitalWrite(ERROR_LED_PIN, HIGH);
  // Best-effort log if Serial happens to be available
  if (Serial) {
    Serial.print("CRITICAL: ");
    Serial.println(msg);
  }
  while (true) { /* halt */ }
}
