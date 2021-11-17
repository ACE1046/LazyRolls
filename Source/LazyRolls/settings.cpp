// settings module for esp8266 projects
// stores config in SPIFFS
// (C) 2018 ACE

#include <string.h>
#include "FS.h"

#define INIFILE "/settings.ini"
#define ZEROCRC 0xAC

bool spiffsActive = false;

bool Init()
{
  if (spiffsActive) return true;
  if (SPIFFS.begin()) 
    spiffsActive = true;
  else
    Serial.println("Unable to activate SPIFFS");
  return spiffsActive;
}

bool LoadSettings(void *ini, int len)
{
  int r, i;
  uint8_t crc;
  if (!Init()) return false;
  
  if (!SPIFFS.exists(INIFILE)) 
  {
    Serial.println("No settings file");
    return false;
  }

  File f = SPIFFS.open(INIFILE, "r");
  if (!f)
  {
    Serial.println("Unable to read settings");
    return false;
  } 

  f.read(&crc, sizeof(crc));
  r=f.read((uint8_t*)ini, len);

  for (i=0; i<r; i++) crc ^= ((uint8_t*)ini)[i];
  
  f.close();

  if (crc!=ZEROCRC) Serial.println("Settings CRC error");

  return (crc==ZEROCRC);
}

bool SaveSettings(void *ini, int len)
{
  int i;
  uint8_t crc;
  if (!Init()) return false;
  
  File f = SPIFFS.open(INIFILE, "w");
  if (!f)
  {
    Serial.println("Unable to write settings");
    return false;
  } 

  crc=ZEROCRC;
  for (i=0; i<len; i++) crc ^= ((uint8_t*)ini)[i];

  f.write(&crc, sizeof(crc));
  f.write((uint8_t*)ini, len);

  f.close();
  return true;
}


