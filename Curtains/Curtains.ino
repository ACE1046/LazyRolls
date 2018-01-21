/*
  To upload through terminal you can use: curl -u admin:admin -F "image=@firmware.bin" esp8266-webupdate.local/firmware
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WiFiUdp.h>

#include "wifi_settings.h"

const char* host = "esp8266-curtains";
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";
#ifndef SSID_AND_PASS
const char* ssid = "ssid";
const char* password = "password";
#endif
#define FAV_FILE "/favicon.ico"
#define CLASS_FILE "/styles.css"
#define PIN_SWITCH 14
#define PIN_A 4
#define PIN_B 5
#define PIN_C 13
#define PIN_D 12
#define DIR_UP (-1)
#define DIR_DN (1)
#define UP_SAFE_LIMIT 300 // make extra rotations up if not hit switch

struct {
  char[16] hostname;
  char[32] ssid;
  char[32] password;
} ini;

uint32_t open_time=((10*60)+30)*60;
uint32_t close_time=((3*60)+19)*60;
bool close_alarm=false;
bool open_alarm=false;

//===================== NTP ============================================

const unsigned long NTP_WAIT=5000;
const unsigned long NTP_SYNC=24*60*60*1000;
unsigned long lastRequest=0;
unsigned long lastSync=0;
IPAddress timeServerIP;
//const char* NTPServerName = "time.nist.gov";
const char* NTPServerName = "10.0.2.235";
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
const uint32_t seventyYears = 2208988800UL;
uint32_t UNIXTime;
WiFiUDP UDP;
int Timezone=3*60*60; // timezone in seconds;

void setup_NTP()
{
  UDP.begin(123); // Start listening for UDP messages on port 123
}

void SyncNTPTime()
{
  if (UDP.parsePacket() == 0)
  { // If there's no response (yet)
    if (lastSync!=0 && (millis()-lastSync < NTP_SYNC)) return; // time ok
    if (millis()-lastRequest < NTP_WAIT) return; // waiting for answer
    lastRequest=millis();
    memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
    // Initialize values needed to form NTP request
    NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
    if(!WiFi.hostByName(NTPServerName, timeServerIP))
    { // Get the IP address of the NTP server failed
      return;
    }
    // send a packet requesting a timestamp:
    UDP.beginPacket(timeServerIP, 123); // NTP requests are to port 123
    UDP.write(NTPBuffer, NTP_PACKET_SIZE);
    UDP.endPacket();
    return;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp: subtract seventy years:
  UNIXTime = NTPTime - seventyYears;
  lastSync=millis();
}

uint32_t getTime()
{
  if (lastSync == 0) return 0;
  return UNIXTime+Timezone + (millis() - lastSync)/1000;
}

String TimeStr()
{
  char buf[9];
  uint32_t t=getTime();
  sprintf(buf, "%02d:%02d:%02d", t/60/60%24, t/60%60, t%60);
  return String(buf);
}

//----------------------- Settings -------------------------------------

void setup_Settings(void)
{

}

//----------------------------------------------------------------------

const uint8_t steps[3][4]={
  {PIN_A, PIN_B, PIN_C, PIN_D},
  {PIN_A, PIN_C, PIN_B, PIN_D},
  {PIN_A, PIN_B, PIN_D, PIN_C}
};
uint8_t pinout=2; // index in "steps" array, according to motor wiring
bool reversed=false; // up-down reverse
uint16_t step_delay_mks=1500;
int position; // current motor steps position. 0 - fully open, -1 - unknown.
int roll_to; // position to go to
int full_length; // steps from open to close

void Rotate(bool dir)
{ // dir: true - up
  if (dir ^ reversed)
  {
    for (int i=0; i<4; i++)
    {
      digitalWrite(steps[pinout][(i+1)%4], HIGH);
      delayMicroseconds(step_delay_mks);
      digitalWrite(steps[pinout][i], LOW);
      delayMicroseconds(step_delay_mks);
    }
  } else {
    for (int i=4; i>0; i--)
    {
      digitalWrite(steps[pinout][i-1], HIGH);
      delayMicroseconds(step_delay_mks);
      digitalWrite(steps[pinout][i%4], LOW);
      delayMicroseconds(step_delay_mks);
    }
  }
  if (dir) position--; else position++;
}

void Motor_off()
{
  digitalWrite(PIN_A, LOW);
  digitalWrite(PIN_B, LOW);
  digitalWrite(PIN_C, LOW);
  digitalWrite(PIN_D, LOW);
}

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void setup_SPIFFS()
{
  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
//      spiffsActive = true;
  } else {
      Serial.println("Unable to activate SPIFFS");
  }
}

void setup_OTA()
{
  ArduinoOTA.onStart([]() {
    Serial.println("Updating");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    Serial.print(".");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(host);
  ArduinoOTA.begin();
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");
  //sprintf(host, "ESP_%06X", ESP.getChipId());
  Serial.print("Hostname: ");
  Serial.println(host);

  WiFi.hostname(host);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.on ("/", HTTP_handleRoot);
  httpServer.on ("/open", HTTP_handleOpen);
  httpServer.on ("/close", HTTP_handleClose);
  httpServer.on ("/test", HTTP_handleTest);
  httpServer.serveStatic(FAV_FILE, SPIFFS, FAV_FILE, "max-age=86400");
  httpServer.serveStatic(CLASS_FILE, SPIFFS, CLASS_FILE, "max-age=86400");
  httpServer.begin();

  Serial.printf("HTTPUpdateServer6 ready!\nOpen http://%s.local%s in your browser and login with username '%s' and password '%s'\n", host, update_path, update_username, update_password);
  Serial.println(WiFi.localIP());

  MDNS.addService("http", "tcp", 80);
  //MDNS.addService("esp", "tcp", 8080); // Announce esp tcp service on port 8080

  pinMode(PIN_SWITCH, INPUT);
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  pinMode(PIN_D, OUTPUT);
  Motor_off();


  setup_OTA();
  setup_SPIFFS();
  setup_NTP();
//  ArduinoOTA.begin();

  full_length=11300;
  // start position is twice as fully open. So on first close we will go up till home position
  position=full_length*2+UP_SAFE_LIMIT;
  if (digitalRead(PIN_SWITCH)) position=0; // Fully open, if switch is pressed
  roll_to=position;
}

String HTTP_header()
{
  String ret ="<!doctype html>\n" \
"<html>\n" \
"<head>\n" \
"  <meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\">" \
"  <meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">\n" \
"  <meta name = \"viewport\" content = \"width=device-width, initial-scale=1\">\n" \
"  <!--meta name=\"viewport\" content=\"width=600, user-scalable=yes\"-->\n" \
"  <title>Oko monitoring</title>\n" \
"  <link rel=\"stylesheet\" href=\"styles.css\" type=\"text/css\">\n" \
"</head>\n" \
"<body>\n" \
"  <div id=\"wrapper\">\n" \
"    <header>Шторы</header>\n" \
"    <nav></nav>\n" \
"    <div id=\"heading\"></div>\n";
  return ret;
}

String HTTP_footer()
{
  String ret = "  </div>\n" \
  "  <footer></footer>\n" \
  "</body>\n" \
  "</html>";
  return ret;
}

void info() {


}

String table_line(const char *name, String val)
{
  String ret="<tr><td>";
  ret+=name;
  ret+="</td><td>";
  ret+=val;
  ret+="</td></tr>\n";
  return ret;
}

bool IsSwitchPressed()
{
  return digitalRead(PIN_SWITCH);
}

const char *onoff[2]={"off", "on"};
void HTTP_handleRoot(void)
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String out;

long m1,m2,m3;
//m1=micros();

  out = HTTP_header();
  out.reserve(4096);
//m2=micros();
  out += "    <section class=\"info\"><table>\n";
  out += "<tr class=\"sect_name\"><td colspan=\"2\">Status</td></tr>\n";
  out += table_line("IP", WiFi.localIP().toString());
  out += table_line("Switch", onoff[IsSwitchPressed()]);
  out += table_line("Position", String(position));
  out += table_line("Roll to", String(roll_to));
  if (lastSync==0)
    out += table_line("Time", "unknown");
  else
    out += table_line("Time", TimeStr());
  out += "<tr class=\"sect_name\"><td colspan=\"2\">Flash</td></tr>\n";
  out += table_line("Real id", String(ESP.getFlashChipId(), HEX));
  out += table_line("Real size", String(realSize));
  out += table_line("IDE size", String(ideSize));
  if(ideSize != realSize) {
    out += table_line("Config", "wrong!");
  } else {
    out += table_line("Config", "OK!");
  }
  out += table_line("Speed", String(ESP.getFlashChipSpeed()/1000000)+"MHz");
  out += table_line("Mode", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  out +="</table></section>\n";
  out += "<section class=\"status\">\n";
  out += "<a href=\"open\">[Открыть]</a> \n";
  out += "<a href=\"close\">[Закрыть]</a> \n";
  out += "</section>\n";


  out += "<section class=\"status\">\n" \
"<br/>\n" \
"<script>\n" \
"function Test(dir)\n" \
"{\n" \
"document.getElementById(\"btn_up\").disabled=true;\n" \
"document.getElementById(\"btn_dn\").disabled=true;\n" \
"pinout=document.getElementById(\"pinout\").selectedIndex;\n" \
"reversed=document.getElementById(\"reversed\").selectedIndex;\n" \
"var xhttp = new XMLHttpRequest();\n" \
"xhttp.onreadystatechange = function() {\n" \
"if (this.readyState == 4 && this.status == 200) {\n" \
"document.getElementById(\"btn_up\").disabled=false;\n" \
"document.getElementById(\"btn_dn\").disabled=false;\n" \
"  }};\n" \
"url=\"test?pinout=\"+pinout+\"&reversed=\"+reversed;\n" \
"if (dir==1) url=url+\"&up=1\"; else url=url+\"&down=1\";\n" \
"xhttp.open(\"GET\", url, true);\n" \
"xhttp.send();\n" \
"}\n" \
"function TestUp() { Test(1); }\n" \
"function TestDown() { Test(0); }\n" \
"</script>\n" \
"<form method=\"get\" action=\"/test\">\n" \
"<select id=\"pinout\" name=\"pinout\">\n" \
"<option value=\"0\">A-B-C-D</option>\n" \
"<option value=\"1\">A-C-B-D</option>\n" \
"<option value=\"2\">A-B-D-C</option>\n" \
"</select>\n" \
"<select id=\"reversed\" name=\"reversed\">\n" \
"<option value=\"0\">Normal</option>\n" \
"<option value=\"1\">Reversed</option>\n" \
"</select>\n" \
"<input id=\"btn_up\" type=\"button\" name=\"up\" value=\"Тест вверх\" onclick=\"TestUp()\">\n" \
"<input id=\"btn_dn\" type=\"button\" name=\"down\" value=\"Тест вниз\" onclick=\"TestDown()\">\n" \
"</form>\n" \
"</section>\n";

//m3=micros();
  out += HTTP_footer();
//out += String(m1) +" "+ String(m2) +" "+ String(m3)+"<br>";
//out += String(m2-m1) +" "+ String(m3-m2);
  httpServer.send(200, "text/html", out);
}

void Open()
{
  roll_to=0-UP_SAFE_LIMIT;
}

void Close()
{
  if (position<0) position=0;
  roll_to=full_length;
}

void HTTP_redirect(String link)
{
  httpServer.sendHeader("Location", link, true);
  httpServer.send(302, "text/plain", "");
}

void HTTP_handleOpen(void)
{
  Open();
  HTTP_redirect(String("/"));
}

void HTTP_handleClose(void)
{
  Close();
  HTTP_redirect(String("/"));
}

void HTTP_handleTest(void)
{
  bool dir=false;
  uint8_t bak_pinout;
  bool bak_reversed;

  bak_pinout=pinout;
  bak_reversed=reversed;

  if (httpServer.hasArg("up")) dir=true;
  if (httpServer.hasArg("reversed")) reversed=atoi(httpServer.arg("reversed").c_str());
  if (httpServer.hasArg("pinout")) pinout=atoi(httpServer.arg("pinout").c_str());
  if (pinout>=3) pinout=0;

  for (int i=0; i<300; i++)
  {
    Rotate(dir);
    if (IsSwitchPressed()) break;
    yield();
  }
  Motor_off();
  roll_to=position;

  pinout=bak_pinout;
  reversed=bak_reversed;

  httpServer.send(200, "text/html", "test ok");
}

void Motor_Action()
{
  bool dir_up;

  if (position==roll_to) return; // stopped, do nothing

  dir_up=(roll_to < position);

  for (int i=0; i<100; i++)
  {
    Rotate(dir_up);
    if (IsSwitchPressed() && (dir_up || position>100))
    { // end stop hit. Ignore if going down, at least first 100 positions.
      position=0; // remember zero position
      if (roll_to <= 0)
      { // if opening then finish
        roll_to=0;
        Motor_off();
        break;
      }
    }
    if (position==roll_to)
    {  // finished
      Motor_off();
      break;
    }
    yield();
  }
}

#define DAY (24*60*60)
void Scheduler()
{
  uint32_t t;

  t=getTime();
  if (t == 0) return;
  t=t % DAY; // time from day start

  if (((t-open_time+DAY) % DAY) < 60)
  {
    if (!open_alarm)
    { // alarm triggered;
      open_alarm=true;
      Open();
    }
  }
  else
    open_alarm=false;

  if (((t-close_time+DAY) % DAY) < 60)
  {
    if (!close_alarm)
    { // alarm triggered;
      close_alarm=true;
      Close();
    }
  }
  else
    open_alarm=false;
}
void loop(void) {
  httpServer.handleClient();
  ArduinoOTA.handle();
  Motor_Action();
  SyncNTPTime();
  Scheduler();
}
