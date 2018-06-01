/*
(C) 2018 ACE, a_c_e@mail.ru

16.04.2018 v0.03 beta

*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WiFiUdp.h>
#include "settings.h"

// copy "wifi_settings.example.h" to "wifi_settings.h" and modify it, if you wish
// Or comment next string and change defaults in this file
#include "wifi_settings.h"

#ifndef SSID_AND_PASS
// if "wifi_settings.h" not included
const char* def_ssid = "lazyrolls";
const char* def_password = "";
const char* def_ntpserver = "time.nist.gov";
const char* def_hostname = "lazyroll";
#endif

#define VERSION "0.03 beta"
#define SPIFFS_AUTO_INIT

#ifdef SPIFFS_AUTO_INIT
#include "spiff_files.h"
#endif

//char *temp_host;
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";
uint16_t def_step_delay_mks = 1500;
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
#define ALARMS 10
#define DAY (24*60*60) // day length in seconds

const int Languages=2;
const char *Language[Languages]={"English", "Русский"};
const char *onoff[][Languages]={{"off", "on"}, {"выкл", "вкл"}};

const uint8_t steps[3][4]={
  {PIN_A, PIN_B, PIN_C, PIN_D},
  {PIN_A, PIN_C, PIN_B, PIN_D},
  {PIN_A, PIN_B, PIN_D, PIN_C}
};
const uint8_t microstep[8][4]={
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};
int8_t MStep= 0;
int position; // current motor steps position. 0 - fully open, -1 - unknown.
int roll_to; // position to go to

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

typedef struct {
	uint32_t time;
	uint8_t percent_open; // 0 - open, 100 - close
	uint8_t day_of_week; // LSB - monday
	uint16_t flags; 
	uint32_t reserved;
} alarm_type;
#define ALARM_FLAG_ENABLED 0x0001

struct {
  char hostname[16+1];
  char ssid[32+1];
  char password[32+1];
  char ntpserver[32+1];
  int lang;
  uint8_t pinout; // index in "steps" array, according to motor wiring
  bool reversed; // up-down reverse
  uint16_t step_delay_mks; // delay (mks) for each step of motor
  int timezone; // timezone in minutes relative to UTC
  int full_length; // steps from open to close
	uint32_t spiffs_time; // time of files in spiffs for version checking
	alarm_type alarms[ALARMS];
} ini;

//uint32_t open_time=((10*60)+30)*60;
//uint32_t close_time=((3*60)+00)*60;
//bool close_alarm=false;
//bool open_alarm=false;

// language functions
const char * L(const char *s1, const char *s2)
{
  return (ini.lang==0) ? s1 : s2;
}
String SL(const char *s1, const char *s2)
{
  return (ini.lang==0) ? String(s1) : String(s2);
}

//===================== NTP ============================================

const unsigned long NTP_WAIT=5000;
const unsigned long NTP_SYNC=24*60*60*1000;  // sync clock once a day
unsigned long lastRequest=0; // timestamp of last request
unsigned long lastSync=0; // timestamp of last successful synchronization
IPAddress timeServerIP;
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
const uint32_t seventyYears = 2208988800UL;
uint32_t UNIXTime;
WiFiUDP UDP;

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
    if(!WiFi.hostByName(ini.ntpserver, timeServerIP))
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
  return UNIXTime+ini.timezone*60 + (millis() - lastSync)/1000;
}

String TimeStr()
{
  char buf[9];
  uint32_t t=getTime();
  sprintf(buf, "%02d:%02d:%02d", t/60/60%24, t/60%60, t%60);
  return String(buf);
}

String UptimeStr()
{
  char buf[9];
  uint32_t t=millis()/1000;
  sprintf(buf, "%dd %02d:%02d", t/60/60/24, t/60/60%24, t/60%60);
  return String(buf);
}

String DoWName(int d)
{
	switch (d)
	{
		case 0: return SL("Mo", "Пн"); break;
		case 1: return SL("Tu", "Вт"); break;
		case 2: return SL("We", "Ср"); break;
		case 3: return SL("Th", "Чт"); break;
		case 4: return SL("Fr", "Пт"); break;
		case 5: return SL("Sa", "Сб"); break;
		case 6: return SL("Su", "Вс"); break;
	}
}

int DayOfWeek(uint32_t time)
{
	return ((time / DAY) + 3) % 7;
}

//----------------------- Motor ----------------------------------------

void Rotate(bool dir)
{ // dir: true - up
  for (int i=0; i<4; i++)
    {
    digitalWrite(steps[ini.pinout][i], microstep[MStep][i]);
    }
  if (dir ^ ini.reversed) MStep++;  else MStep--;
  
  if (MStep<0)  MStep+=8;
  if (MStep>=8) MStep-=8;
 
  if (dir) position--; else position++;
  delayMicroseconds(ini.step_delay_mks);
}

void Motor_off()
{
  digitalWrite(PIN_A, LOW);
  digitalWrite(PIN_B, LOW);
  digitalWrite(PIN_C, LOW);
  digitalWrite(PIN_D, LOW);
}

bool IsSwitchPressed()
{
  return digitalRead(PIN_SWITCH);
}

void Open()
{
  roll_to=0-UP_SAFE_LIMIT; // up to 0 and beyond (a little)
}

void Close()
{
  if (position<0) position=0;
  roll_to=ini.full_length;
}

void Motor_Action()
{
  bool dir_up;

  if (position==roll_to) return; // stopped, do nothing

  dir_up=(roll_to < position); // up - true

  for (int i=0; i<50; i++) // Make up to 50 steps before returning control to main loop
  {
    Rotate(dir_up);
    if (IsSwitchPressed() && (dir_up || position>100))
    { // end stop hit. Ignore if going down, at least first 100 positions.
      position=0; // remember zero position
      if (roll_to <= 0)
      { // if opening then finish
				Motor_off();
				roll_to=position;
      }
			break;
    }
    if (position==roll_to)
    {  // finished
      Motor_off();
      break;
    }
    yield();
  }
}

// ==================== initialization ===============================

#ifdef SPIFFS_AUTO_INIT
void CreateFile(const char *filename, const uint8_t *data, int len)
{
	uint8_t buf[256];
	int blk;
	unsigned int bytes;

  // update file if not exist or if version changed
  if ((ini.spiffs_time!=0 && ini.spiffs_time != spiffs_time) ||
	  (!SPIFFS.exists(filename)))
	{
		File f = SPIFFS.open(filename, "w");
		if (!f) {
			Serial.println("file creation failed");
		} else
		{
			bytes=len;
			while (bytes>0)
			{
				blk=min(bytes, sizeof(buf));
				memcpy_P(buf, data, blk);
				data+=blk;
				bytes-=blk;
				f.write(buf, blk);
			}
			f.close();
			Serial.print("file written: ");
			Serial.println(filename);
		}
	}
}
#endif

void init_SPIFFS()
{
#ifdef SPIFFS_AUTO_INIT
	CreateFile(FAV_FILE, fav_icon_data, sizeof(fav_icon_data));
	CreateFile(CLASS_FILE, css_data, sizeof(css_data));
	if (ini.spiffs_time != spiffs_time)
	{
		ini.spiffs_time=spiffs_time;
		SaveSettings(&ini, sizeof(ini));
	}
#endif
}

void setup_Settings(void)
{
  memset(&ini, sizeof(ini), 0);
  strcpy(ini.hostname  , def_hostname);
  strcpy(ini.ssid      , def_ssid);
  strcpy(ini.password  , def_password);
  strcpy(ini.ntpserver , def_ntpserver);
  ini.lang=0;
  ini.pinout=0;
  ini.reversed=false;
  ini.step_delay_mks=def_step_delay_mks;
  ini.timezone=3*60; // Default City time zone by default :)
  ini.full_length=11300;
	ini.spiffs_time=0;
  LoadSettings(&ini, sizeof(ini));
  if (ini.lang<0 || ini.lang>=Languages) ini.lang=0;
  if (ini.step_delay_mks < 10) ini.step_delay_mks=10;
}

void setup_SPIFFS()
{
  if (SPIFFS.begin()) 
	{
    Serial.println("SPIFFS Active");
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
  ArduinoOTA.setHostname(ini.hostname);
  ArduinoOTA.begin();
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");

  setup_SPIFFS();
  setup_Settings(); // setup and load settings.
	init_SPIFFS(); // create static HTML files, if needed

  Serial.print("Hostname: ");
  Serial.println(ini.hostname);

  WiFi.hostname(ini.hostname);
  WiFi.mode(WIFI_STA);

  int attempts=3;
  while (attempts>0)
  {
    WiFi.begin(ini.ssid, ini.password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) break;
    Serial.println("WiFi failed, retrying.");
    attempts--;
    if (attempts>0) delay(1000);
    else
    { // Cannot connect to WiFi. Lets make our own Access Point with blackjack and hookers
      Serial.print("Starting access point. SSID: ");
      Serial.println(ini.hostname);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ini.hostname); // ... but without password
    }
  }

  if (!MDNS.begin(ini.hostname)) Serial.println("Error setting up MDNS responder!");
	else Serial.println("mDNS responder started");

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.on("/",         HTTP_handleRoot);
  httpServer.on("/open",     HTTP_handleOpen);
  httpServer.on("/close",    HTTP_handleClose);
  httpServer.on("/stop",     HTTP_handleStop);
  httpServer.on("/test",     HTTP_handleTest);
  httpServer.on("/settings", HTTP_handleSettings);
  httpServer.on("/alarms", 	 HTTP_handleAlarms);
  httpServer.on("/reboot",   HTTP_handleReboot);
  httpServer.serveStatic(FAV_FILE, SPIFFS, FAV_FILE, "max-age=86400");
  httpServer.serveStatic(CLASS_FILE, SPIFFS, CLASS_FILE, "max-age=86400");
  httpServer.begin();

  Serial.println(WiFi.localIP());

  MDNS.addService("http", "tcp", 80);

  pinMode(PIN_SWITCH, INPUT);
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  pinMode(PIN_D, OUTPUT);
  Motor_off();

  setup_OTA();
  setup_NTP();

  // start position is twice as fully open. So on first close we will go up till home position
  position=ini.full_length*2+UP_SAFE_LIMIT;
  if (IsSwitchPressed()) position=0; // Fully open, if switch is pressed
  roll_to=position;
}

// ============================= HTTP ===================================
String HTML_header()
{
  String ret ="<!doctype html>\n" \
	"<html>\n" \
	"<head>\n" \
	"  <meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\">" \
	"  <meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">\n" \
	"  <meta name = \"viewport\" content = \"width=device-width, initial-scale=1\">\n" \
	"  <title>"+SL("Lazy rolls", "Ленивые шторы")+"</title>\n" \
	"  <link rel=\"stylesheet\" href=\"styles.css\" type=\"text/css\">\n" \
	"</head>\n" \
	"<body>\n" \
	" <div id=\"wrapper2\">\n" \
	"  <div id=\"wrapper\">\n" \
	"    <header>"+SL("Lazy rolls", "Ленивые шторы")+"</header>\n" \
	"    <nav></nav>\n" \
	"    <div id=\"heading\"></div>\n";
  return ret;
}

String HTML_footer()
{
  String ret = "  </div></div>\n" \
  "  <footer></footer>\n" \
  "</body>\n" \
  "</html>";
  return ret;
}

String HTML_openCloseStop()
{
	String out;
  out = "<a href=\"open\">["+SL("Open", "Открыть")+"]</a> \n";
  out += "<a href=\"close\">["+SL("Close", "Закрыть")+"]</a> \n";
  if (position != roll_to)
    out += "<a href=\"stop\">["+SL("Stop", "Стоп")+"]</a> \n";
	return out;
}	

String HTML_tableLine(const char *name, String val, char *id=NULL)
{
  String ret="<tr><td>";
  ret+=name;
  if (id==NULL)
    ret+="</td><td>";
  else
    ret+="</td><td id=\""+String(id)+"\">";
  ret+=val;
  ret+="</td></tr>\n";
  return ret;
}

String HTML_editString(const char *header, const char *id, const char *inistr, int len)
{
  String out;

  out="<tr><td class=\"idname\">";
  out+=header;
  out+="</td><td class=\"val\"><input type=\"text\" name=\"";
  out+=String(id);
  out+="\" id=\"";
  out+=String(id);
  out+="\" value=\"";
  out+=String(inistr);
  out+="\" maxlength=\"";
  out+=String(len-1);
  out+="\"\></td></tr>\n";

  return out;
}

String HTML_status()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String out;

  out = HTML_header();
  out.reserve(4096);

  out += "    <section class=\"info\"><table>\n";
  out += "<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Status", "Статус")+"</td></tr>\n";
  out += HTML_tableLine(L("Version", "Версия"), VERSION);
  out += HTML_tableLine(L("IP", "IP"), WiFi.localIP().toString());
  if (lastSync==0)
    out += HTML_tableLine(L("Time", "Время"), SL("unknown", "хз"));
  else
    out += HTML_tableLine(L("Time", "Время"), TimeStr() + " ["+ DoWName(DayOfWeek(getTime())) +"]");
	
  out += HTML_tableLine(L("Uptime", "Аптайм"), UptimeStr());
  out += HTML_tableLine(L("RSSI", "RSSI"), String(WiFi.RSSI())+SL(" dBm", " дБм"));

  out += "<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Position", "Положение")+"</td></tr>\n";
  out += HTML_tableLine(L("Now", "Сейчас"), String(position), "pos");
  out += HTML_tableLine(L("Roll to", "Цель"), String(roll_to), "dest");
  out += HTML_tableLine(L("Switch", "Концевик"), onoff[ini.lang][IsSwitchPressed()]);
    
  out += "<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Memory", "Память")+"</td></tr>\n";
  out += HTML_tableLine(L("Real id", "ID чипа"), String(ESP.getFlashChipId(), HEX));
  out += HTML_tableLine(L("Real size", "Реально"), String(realSize));
  out += HTML_tableLine(L("IDE size", "Прошивка"), String(ideSize));
  if(ideSize != realSize) {
    out += HTML_tableLine(L("Config", "Конфиг"), L("error!", "ошибка!"));
  } else {
    out += HTML_tableLine(L("Config", "Конфиг"), "OK!");
  }
  out += HTML_tableLine(L("Speed", "Частота"), String(ESP.getFlashChipSpeed()/1000000)+SL("MHz", "МГц"));
  out += HTML_tableLine(L("Mode", "Режим"), (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  //out += HTML_tableLine("Host", String(ini.hostname));
  out +="</table></section>\n";

  return out;
}

String HTML_mainmenu(void)
{
  String out;

  out.reserve(128);

  out += "<section class=\"status\">\n";
	out += HTML_openCloseStop();
  out += "<a href=\"/\">["+SL("Main", "Главная")+"]</a> \n";
  out += "<a href=\"settings\">["+SL("Settings", "Настройки")+"]</a> \n";
  out += "<a href=\"alarms\">["+SL("Schedule", "Расписание")+"]</a> \n";
  out += "</section>\n";
	return out;
}

void HTTP_handleRoot(void)
{
  String out;

  out=HTML_status();
  
	out += HTML_mainmenu();

  out += HTML_footer();
  httpServer.send(200, "text/html", out);
}

void SaveString(const char *id, char *inistr, int len)
{
  if (!httpServer.hasArg(id)) return;

  strncpy(inistr, httpServer.arg(id).c_str(), len-1);
  inistr[len-1]='\0';
}

void HTTP_handleSettings(void)
{
  String out;
	char pass[sizeof(ini.password)];

  if (httpServer.hasArg("save"))
  {
		pass[0]='*';
		pass[1]='\0';
    SaveString("hostname", ini.hostname, sizeof(ini.hostname));
    SaveString("ssid",     ini.ssid,     sizeof(ini.ssid));
    SaveString("password", pass,         sizeof(ini.password));
    SaveString("ntp",      ini.ntpserver,sizeof(ini.ntpserver));
		if (strcmp(pass, "*")!=0) memcpy(ini.password, pass, sizeof(ini.password));

    if (httpServer.hasArg("lang")) ini.lang=atoi(httpServer.arg("lang").c_str());
    if (ini.lang<0 || ini.lang>=Languages) ini.lang=0;
    if (httpServer.hasArg("pinout")) ini.pinout=atoi(httpServer.arg("pinout").c_str());
    if (ini.pinout<0 || ini.pinout>=3) ini.pinout=0;
    if (httpServer.hasArg("reversed")) ini.reversed=atoi(httpServer.arg("reversed").c_str());
    if (httpServer.hasArg("delay")) ini.step_delay_mks=atoi(httpServer.arg("delay").c_str());
    if (ini.step_delay_mks<10 || ini.step_delay_mks>=65000) ini.step_delay_mks=def_step_delay_mks;
    if (httpServer.hasArg("timezone")) ini.timezone=atoi(httpServer.arg("timezone").c_str());
    if (ini.timezone<-11*60 || ini.timezone>=14*60) ini.timezone=0;
    if (httpServer.hasArg("length")) ini.full_length=atoi(httpServer.arg("length").c_str());
    if (ini.full_length<300 || ini.full_length>=99999) ini.full_length=10000;

    SaveSettings(&ini, sizeof(ini));

    HTTP_redirect("/settings?ok=1");
    return;
  }

  out.reserve(10240);
    
  out=HTML_status();

	out += HTML_mainmenu();

  out += "<section class=\"settings\">\n";

  if (httpServer.hasArg("ok"))
    out+=SL("<p>Saved!<br/>Network settings will be applied after reboot.<br/><a href=\"reboot\">[Reboot]</a></p>\n",
      "<p>Сохранено!<br/>Настройки сети будут применены после перезагрузки.<br/><a href=\"reboot\">[Перезагрузить]</a></p>\n");

  out +="<script>\n" \
	"function Test(dir)\n" \
	"{\n" \
	"document.getElementById(\"btn_up\").disabled=true;\n" \
	"document.getElementById(\"btn_dn\").disabled=true;\n" \
	"pinout=document.getElementById(\"pinout\").selectedIndex;\n" \
	"reversed=document.getElementById(\"reversed\").selectedIndex;\n" \
	"delay=document.getElementById(\"delay\").value;\n" \
	"var xhttp = new XMLHttpRequest();\n" \
	"xhttp.onreadystatechange = function() {\n" \
	"if (this.readyState == 4 && this.status == 200) {\n" \
	"document.getElementById(\"btn_up\").disabled=false;\n" \
	"document.getElementById(\"btn_dn\").disabled=false;\n" \
	"document.getElementById(\"pos\").innerHTML=this.responseText;\n" \
	"document.getElementById(\"dest\").innerHTML=this.responseText;\n" \
	"  }};\n" \
	"url=\"test?pinout=\"+pinout+\"&reversed=\"+reversed+\"&delay=\"+delay;\n" \
	"if (dir==1) url=url+\"&up=1\"; else url=url+\"&down=1\";\n" \
	"xhttp.open(\"GET\", url, true);\n" \
	"xhttp.send();\n" \
	"}\n" \
	"function TestUp() { Test(1); }\n" \
	"function TestDown() { Test(0); }\n" \
	"</script>\n" \
	"<form method=\"post\" action=\"/settings\">\n";

  out+="<table>\n";
  out+="<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Network", "Сеть")+"</td></tr>\n";
  out+="<tr><td>"+SL("Language: ", "Язык: ")+"</td><td><select id=\"lang\" name=\"lang\">\n";
  for (int i=0; i<Languages; i++)
  {
    out+="<option value=\""+String(i)+"\"";
    if (i==ini.lang) out+=" selected=\"selected\"";
    out+=+">"+String(Language[i])+"</option>\n";
  }
  out+="</select></td></tr>\n";
  out+=HTML_editString(L("Hostname:", "Имя в сети:"), "hostname", ini.hostname, sizeof(ini.hostname));
  out+=HTML_editString(L("SSID:", "Wi-Fi сеть:"),     "ssid",     ini.ssid,     sizeof(ini.ssid));
  out+=HTML_editString(L("Password:", "Пароль:"),     "password", "*",          sizeof(ini.password));

  out+="<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Time", "Время")+"</td></tr>\n";
  out+=HTML_editString(L("NTP-server:", "NTP-сервер:"),"ntp",     ini.ntpserver,sizeof(ini.ntpserver));
  out+="<tr><td>"+SL("Timezone: ", "Пояс: ")+"</td><td><select id=\"timezone\" name=\"timezone\">\n";
  for (int i=-11*60; i<=14*60; i+=15) // timezones from -11:00 to +14:00 every 15 min
  {
    char b[7];
    sprintf(b, "%+d:%02d", i/60, abs(i%60));
    if (i<0) b[0]='-';
    out+="<option value=\""+String(i)+"\"";
    if (i==ini.timezone) out+=" selected=\"selected\"";
    out+=+">UTC"+String(b)+"</option>\n";
  }
  out+="</select></td></tr>\n";

  out+="<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Motor", "Мотор")+"</td></tr>\n";
	out+="<tr><td>"+SL("Pinout:", "Подключение:")+"</td><td><select id=\"pinout\" name=\"pinout\">\n" \
	"<option value=\"0\""+(ini.pinout==0 ? " selected=\"selected\"" : "")+">A-B-C-D</option>\n" \
	"<option value=\"1\""+(ini.pinout==1 ? " selected=\"selected\"" : "")+">A-C-B-D</option>\n" \
	"<option value=\"2\""+(ini.pinout==2 ? " selected=\"selected\"" : "")+">A-B-D-C</option>\n" \
	"</select></td></tr>\n" \
	"<tr><td>"+SL("Direction:", "Направление:")+"</td><td><select id=\"reversed\" name=\"reversed\">\n" \
	"<option value=\"0\""+(ini.reversed ? "" : " selected=\"selected\"")+">"+SL("Normal", "Прямое")+"</option>\n" \
	"<option value=\"1\""+(ini.reversed ? " selected=\"selected\"" : "")+">"+SL("Reversed", "Обратное")+"</option>\n" \
	"</select></td></tr>\n";
  out+=HTML_editString(L("Step delay:", "Время шага:"),"delay", String(ini.step_delay_mks).c_str(), 6);
	out+="<tr><td colspan=\"2\">"+SL("(microsecs, 10-65000, default 1500)", "(в мкс, 10-65000, обычно 1500)")+"</td></tr>\n" \
	"<tr><td colspan=\"2\">\n" \
	"<input id=\"btn_up\" type=\"button\" name=\"up\" value=\""+SL("Test up", "Тест вверх")+"\" onclick=\"TestUp()\">\n" \
	"<input id=\"btn_dn\" type=\"button\" name=\"down\" value=\""+SL("Test down", "Тест вниз")+"\" onclick=\"TestDown()\">\n" \
	"</td></tr>\n";
  out+="<tr class=\"sect_name\"><td colspan=\"2\">"+SL("Curtain", "Штора")+"</td></tr>\n";
  out+=HTML_editString(L("Length:", "Длина:"), "length", String(ini.full_length).c_str(), 6);
  out+="<tr><td colspan=\"2\">"+SL("(closed position, steps)", "(шагов до полного закрытия)")+"</td></tr>\n";
  
  out+="<tr class=\"sect_name\"><td colspan=\"2\"><input id=\"save\" type=\"submit\" name=\"save\" value=\""+SL("Save", "Сохранить")+"\"></td></tr>\n";
  out+="</table>\n";
  out+="</form>\n";
  out+="</section>\n";

  out += HTML_footer();

  httpServer.send(200, "text/html", out);
}

uint32_t StrToTime(String s)
{
	uint32_t t=0;
	
	if (s.length()<5) return 0;
	if (s[0]>='0' && s[0]<='9') t+=(s[0]-'0')*10*60; else return 0;
	if (s[1]>='0' && s[1]<='9') t+=(s[1]-'0')* 1*60; else return 0;
	if (s[3]>='0' && s[3]<='9') t+=(s[3]-'0')*   10; else return 0;
	if (s[4]>='0' && s[4]<='9') t+=(s[4]-'0')*    1; else return 0;
	return t;
}

String TimeToStr(uint32_t t)
{
	char buf[6];
  sprintf(buf, "%02d:%02d", t/60%24, t%60);
	return String(buf);
}	

void HTTP_handleAlarms(void)
{
  String out;

  if (httpServer.hasArg("save"))
  {
		for (int a=0; a<ALARMS; a++)
		{
		  String n=String(a);
      if (httpServer.hasArg("en"+n)) 
				ini.alarms[a].flags |= ALARM_FLAG_ENABLED;
		  else
				ini.alarms[a].flags &= ~ALARM_FLAG_ENABLED;

      if (httpServer.hasArg("time"+n))
			{
				ini.alarms[a].time=StrToTime(httpServer.arg("time"+n));
			}

      if (httpServer.hasArg("dest"+n))
			{
				ini.alarms[a].percent_open=atoi(httpServer.arg("dest"+n).c_str());
				if (ini.alarms[a].percent_open>100) ini.alarms[a].percent_open=100;
			}

			uint8_t b=0;
			for (int d=6; d>=0; d--)
			{
				b=b<<1;
        if (httpServer.hasArg("d"+n+"_"+String(d))) b|=1;
			}
			ini.alarms[a].day_of_week=b;
		}	

    SaveSettings(&ini, sizeof(ini));
  }

  out.reserve(16384);
    
  out=HTML_status();

	out += HTML_mainmenu();

  out += "<section class=\"alarms\">\n";
	out += "<form method=\"post\" action=\"/alarms\">\n";
	out += "<table width=\"100%\">\n";

	for (int a=0; a<ALARMS; a++)
	{
		String n=String(a);
		out += "<tr><td colspan=\"2\">\n";
		if (a>0) out += "<hr/>\n";
		out += "<label for=\"en"+n+"\">\n";
		out += "<input type=\"checkbox\" id=\"en"+n+"\" name=\"en"+n+"\"" + 
		  ((ini.alarms[a].flags & ALARM_FLAG_ENABLED) ? " checked" : "") + "/>\n";
		out += SL("Enabled", "Включено")+"</label></td></tr>\n";
		
		out += "<tr><td style=\"width:1px\"><label for=\"time"+n+"\">"+
		  SL("Time:", "Время:")+"</label></td><td><input type=\"time\" id=\"time"+n+
			"\" name=\"time"+n+"\" value=\""+TimeToStr(ini.alarms[a].time)+"\" required></td></tr>\n";
		
		out += "<tr><td><label for=\"dest"+n+"\">"+
		  SL("Position:", "Положение:")+"</label></td><td><select id=\"dest"+n+"\" name=\"dest"+n+"\">\n";
		for (int p=0; p<=100; p+=20)
		{
			String s = String(p)+"%";
			if (p==0)   s=SL("Open", "Открыть");
			if (p==100) s=SL("Close", "Закрыть");
			out += "<option value=\""+String(p)+"\""+
			  (ini.alarms[a].percent_open==p ? " selected" : "")+">"+s+"</option>\n";
		}
		out += "</select>\n";
		out += "</td></tr><tr><td>";

		out += SL("Repeat:", "Повтор:")+"</td><td class=\"days\">\n";
		for (int d=0; d<7; d++)
		{
			String id="\"d"+n+"_"+String(d)+"\"";
			out += "<label for="+id+">";
			out += "<input type=\"checkbox\" id="+id+" name="+id+
					((ini.alarms[a].day_of_week & (1 << d)) ? " checked" : "")+">";
			out+=DoWName(d);
			out += "</label> \n";
		}
		out += "</td></tr>\n";
	}

  out+="<tr class=\"sect_name\"><td colspan=\"2\"><input id=\"save\" type=\"submit\" name=\"save\" value=\""+SL("Save", "Сохранить")+"\"></td></tr>\n";
	
	out += "</table>\n";
  out+="</form>\n";
  out+="</section>\n";
  out += HTML_footer();

  httpServer.send(200, "text/html", out);
}

void HTTP_redirect(String link)
{
  httpServer.sendHeader("Location", link, true);
  httpServer.send(302, "text/plain", "");
}

void HTTP_handleReboot(void)
{
  HTTP_redirect(String("/"));
	delay(500);
  ESP.reset();
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

void HTTP_handleStop(void)
{
  roll_to=position;
  Motor_off();
  HTTP_redirect(String("/"));
}

void HTTP_handleTest(void)
{
  bool dir=false;
  uint8_t bak_pinout;
  bool bak_reversed;
  uint16_t bak_step_delay_mks;
  int steps=300;

  bak_pinout=ini.pinout;
  bak_reversed=ini.reversed;
  bak_step_delay_mks=ini.step_delay_mks;

  if (httpServer.hasArg("up")) dir=true;
  if (httpServer.hasArg("reversed")) ini.reversed=atoi(httpServer.arg("reversed").c_str());
  if (httpServer.hasArg("pinout")) ini.pinout=atoi(httpServer.arg("pinout").c_str());
  if (ini.pinout>=3) ini.pinout=0;
  if (httpServer.hasArg("delay")) ini.step_delay_mks=atoi(httpServer.arg("delay").c_str());
  if (ini.step_delay_mks<10) ini.step_delay_mks=10;
  if (ini.step_delay_mks>65000) ini.step_delay_mks=65000;
  if (httpServer.hasArg("steps")) steps=atoi(httpServer.arg("steps").c_str());
  if (steps<0) steps=0;

  for (int i=0; i<steps; i++)
  {
    Rotate(dir);
    if (IsSwitchPressed() && i>50) break; // Ignoring switch at first 50 rotations
    yield();
  }
  Motor_off();
  roll_to=position;

  ini.pinout=bak_pinout;
  ini.reversed=bak_reversed;
  ini.step_delay_mks=bak_step_delay_mks;

  httpServer.send(200, "text/html", String(position));
}

void Scheduler()
{
  uint32_t t;
	static uint32_t last_t;
	int dayofweek;

  t=getTime();
  if (t == 0) return;
	dayofweek = DayOfWeek(t); // 0 - monday
  t=t % DAY; // time from day start
	t=t/60; // in minutes
	
	if (t == last_t) return; // this minute already handled
	last_t=t;

	for (int a=0; a<ALARMS; a++)
	{
		if (!(ini.alarms[a].flags & ALARM_FLAG_ENABLED)) continue;
		if ( (ini.alarms[a].time != t)) continue;
		if (!(ini.alarms[a].day_of_week & (1<<dayofweek)) && (ini.alarms[a].day_of_week != 0)) continue;
		
		if (ini.alarms[a].day_of_week == 0) // if no repeat
		{
			ini.alarms[a].flags &= ~ALARM_FLAG_ENABLED; // disabling
			SaveSettings(&ini, sizeof(ini));
		}

		if (ini.alarms[a].percent_open == 0)
			Open();
		else
		{
			if (position<0) position=0;
			roll_to=ini.full_length * ini.alarms[a].percent_open / 100;
		}
	}
}

unsigned long last_reconnect=0;
void loop(void) 
{
  if(WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP)
  { // in soft AP mode, trying to connect to network
		if (last_reconnect==0) last_reconnect=millis();
    if (millis()-last_reconnect > 60*1000) // every 60 sec
    {
      Serial.println("Trying to reconnect");
      last_reconnect=millis();
      WiFi.begin(ini.ssid, ini.password);
      if (WiFi.waitForConnectResult() == WL_CONNECTED)
      {
        Serial.println("Reconnected to network in STA mode. Closing AP");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
      } else
				WiFi.mode(WIFI_AP);
    }
  }
  
  httpServer.handleClient();
  ArduinoOTA.handle();
  Motor_Action();
  SyncNTPTime();
  Scheduler();
}
