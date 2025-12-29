#include "Parser.h"

static int indexOfField(const String& s, const String& key) {
  // Return index at the start of the value for key=value
  String needle = key + "=";
  int pos = s.indexOf(needle);
  if (pos < 0) return -1;
  return pos + needle.length();
}

static String readFieldValue(const String& s, const String& key) {
  int start = indexOfField(s, key);
  if (start < 0) return "";
  int end = s.indexOf(';', start);
  if (end < 0) end = s.length();
  return s.substring(start, end);
}

IrrigationCommand parsePayload(const String& payload) {
  IrrigationCommand cmd;
  cmd.valid = false;
  cmd.id = -1;
  cmd.numZones = 0;
  cmd.totalMinutes = 0;
  cmd.remainingMinutes = 0;
  cmd.status = 0xFF;

  if (payload.length() == 0) return cmd;

  String idStr = readFieldValue(payload, "ID");
  String zStr  = readFieldValue(payload, "Z");
  String tStr  = readFieldValue(payload, "T");
  String mStr  = readFieldValue(payload, "M");
  String sStr  = readFieldValue(payload, "S");

  if (idStr.length() == 0 || sStr.length() == 0) {
    return cmd;
  }

  cmd.id = idStr.toInt();
  cmd.totalMinutes = (uint8_t) tStr.toInt();
  cmd.remainingMinutes = (uint8_t) mStr.toInt();
  cmd.status = (uint8_t) sStr.toInt();

  // Parse zones (comma-separated) into a list with capacity ZONES_MAX
  int start = 0;
  while (start < zStr.length()) {
    int comma = zStr.indexOf(',', start);
    String part = (comma >= 0) ? zStr.substring(start, comma) : zStr.substring(start);
    part.trim();
    int z = part.toInt();
    if (z >= 1 && z <= ZONES_MAX) {
      if (cmd.numZones < ZONES_MAX) {
        cmd.zones[cmd.numZones++] = (uint8_t)z;
      } else {
        if (Serial) {
          Serial.print("Parse warning: too many zones, ignoring zone ");
          Serial.println(z);
        }
      }
    } 
    if (comma < 0) break;
    start = comma + 1;
  }
  cmd.valid = true;
  return cmd;
}


