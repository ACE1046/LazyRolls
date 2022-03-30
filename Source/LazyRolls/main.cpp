/*
LazyRolls
(C) 2019-2021 ACE, a_c_e@mail.ru
http://imlazy.ru/rolls/

21.12.2018 v0.04 beta
21.02.2019 v0.05 beta
16.03.2019 v0.06
10.04.2019 v0.07
02.06.2020 v0.08
29.01.2021 v0.09
30.03.2021 v0.10
05.08.2021 v0.11
27.01.2022 v0.12

*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#define FS_NO_GLOBALS 1
#include <FS.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include "settings.h"

#define VERSION "0.12.1 +rf test"
#define MQTT 1 // MQTT & HA functionality
#define ARDUINO_OTA 0 // Firmware update from Arduino IDE
#define DAYLIGHT 0 // this is just a test, not working yet
#define RF 1
#define MDNSC 1 // mDNS responder. Required for ArduinoIDE web port discovery
#define SPIFFS_AUTO_INIT

#include "spiff_files.h"

#if MDNSC
	#include <ESP8266mDNS.h>
#endif

#if MQTT
	// For MQTT support: Sketch - Include Library - Manage Libraries - PubSubClient - Install
	#include <PubSubClient.h>
#endif

#if ARDUINO_OTA
	#include <ArduinoOTA.h>
#endif

#if RF
	// For MQTT support: Sketch - Include Library - Manage Libraries - rc-switch by Suat Ozgur - Install
	#include "RCSwitch.h"
	RCSwitch mySwitch = RCSwitch();
#endif

#if DAYLIGHT
// this is just a test, not working yet
#include <ESP8266HTTPClient.h>
String payload;
void TestDaylight() 
{

WiFiClient client;
HTTPClient http;
	http.begin(client, "http://api.sunrise-sunset.org/json?lat=55.76501600&lng=37.61&formatted=0");

/*
GET /json?lat=55.76501600&lng=37.61&formatted=0 HTTP/1.1
Host: api.sunrise-sunset.org

*/
      int httpResponseCode = http.GET();
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
}
#else
void TestDaylight() {}
#endif

// copy "wifi_settings.example.h" to "wifi_settings.h" and modify it, if you wish
// Or comment next string and change defaults in this file
//#include "wifi_settings.h"

#ifndef SSID_AND_PASS
// if "wifi_settings.h" not included
const char def_ssid[] PROGMEM = ""; // Empty ssid to create SoftAP at start
const char def_password[] PROGMEM = "";
const char def_ntpserver[] PROGMEM = "ru.pool.ntp.org";
const char def_hostname[] PROGMEM = "lazyroll-%06X";
const char def_mqtt_server[] PROGMEM = "mqtt.lan";
const char def_mqtt_login[] PROGMEM = "";
const char def_mqtt_password[] PROGMEM = "";
const uint16_t def_mqtt_port=1883;
const char def_mqtt_topic_state[] PROGMEM = "lazyroll/%HOSTNAME%/state";
const char def_mqtt_topic_command[] PROGMEM = "lazyroll/%HOSTNAME%/command";
const char def_mqtt_topic_alive[] PROGMEM = "lazyroll/%HOSTNAME%/alive";
const char def_mqtt_topic_aux[] PROGMEM = "lazyroll/%HOSTNAME%/aux";
const char def_mqtt_topic_info[] PROGMEM = "lazyroll/%HOSTNAME%/info";
#endif

//char *temp_host;
const char* update_path = "/update2";
const char* update_username = "admin";
const char* update_password = "admin";
uint16_t def_step_delay_mks = 1500;
#define FAV_FILE "/favicon.ico"
#define CLASS_FILE "/styles.css"
#define JS_FILE "/scripts.js"
#define INI_FILE "/settings.ini"
#define PIN_SWITCH 14
#define PIN_A 5
#define PIN_B 4
#define PIN_C 13
#define PIN_D 12
#define PIN_EN 4
#define PIN_ST 13
#define PIN_DR 12
#define PIN_LED 2
#define DIR_UP (-1)
#define DIR_DN (1)
#define DEFAULT_UP_SAFE_LIMIT 300 // make extra rotations up if not hit switch
#define DEFAULT_SWITCH_IGNORE_STEPS 100 // ignore endstop for first step on moving down
#define MIN_STEP_DELAY 50 // minimal motor step time in mks
#define UART_PING_INTERVAL_MS (30*1000) // Master ping slaves every 30 seconds
#define SLAVE_MAX_NO_PING_MS (70*1000) // Enable WiFi in slave mode if no ping from master
#define SLAVE_SLEEP_TIMEOUT_MS (180*1000) // Disable WiFi in slave mode after network idle
#define ALARMS 10
#define DAY (24*60*60) // day length in seconds
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define MASTER (ini.slave == 255)
#define SLAVE (ini.slave != 255 && ini.slave != 0)
#define ADDR_MASTER 0
#define ADDR_SELF_ONLY 0
#define ADDR_ALL 255
#define MQTT_INFO_SECONDS 5*60 // Send mqtt info (rssi, uptime, etc) every N seconds

const int Languages=2;
const char ru_ru[] PROGMEM = "Русский";
const char en_en[] PROGMEM = "English";
const __FlashStringHelper *Language[Languages]={ (__FlashStringHelper*)en_en, (__FlashStringHelper*)ru_ru };
const PROGMEM char *onoff[][Languages]={{"off", "on"}, {"выкл", "вкл"}};

const uint8_t steps[3][4]={
	{PIN_A, PIN_C, PIN_B, PIN_D},
	{PIN_A, PIN_B, PIN_D, PIN_C},
	{PIN_A, PIN_B, PIN_C, PIN_D}
};
volatile int position; // current motor steps position. 0 - at endstop
volatile int roll_to; // position to go to
uint16_t step_c[8], step_s[8]; // bitmasks for every step
volatile uint8_t endstop_hit = 0; // endstop hit flag. Set at ISR, reset at user level
volatile int endstop_hit_pos = 0; // position of endtop hit
bool voltage_available=0;
uint32_t last_network_time=0; // last network activity time
bool WiFi_active = false;
bool WiFi_connected = false;
bool WiFi_AP_disabled = false; // AP mode disabled after successfull connection to home network or after first ping msg from master
int WiFi_attempts = 0;
unsigned long last_reconnect = 0;
uint32_t uart_crc_errors = 0;
#define MAX_RECONNECT_ATTEMPS 2 // reconnect attemps before creating Access Point
bool rf_page_open = 0; // true while RF settings open

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

typedef struct {
	uint32_t time;
	uint8_t percent_open; // 0 - open, 100 - close, 101-105 - presets
	uint8_t day_of_week; // LSB - monday
	uint16_t flags;
	uint32_t reserved;
} alarm_type;
#define ALARM_FLAG_ENABLED 0x0001

typedef struct {
	uint32_t code;
	uint8_t action;
	uint8_t flags; // reserved
	uint16_t reserved;
} rf_cmd_type;
#define RF_FLAG_STOP2ND 0x01

#define MAX_PRESETS 5
#define MAX_RF_CMDS 10
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
	uint8_t switch_reversed; // reverse switch logic. 0 - NC, 1 - NO
	bool mqtt_enabled;
	char mqtt_server[64+1]; // MQTT server, ip or hostname
	uint32_t mqtt_port;
	char mqtt_login[64+1]; // MQTT server login
	char mqtt_password[64+1]; // MQTT server password
	uint16_t mqtt_ping_interval; // I'm alive ping interval, seconds
	char mqtt_topic_state[127+1]; // publish current state topic
	char mqtt_topic_command[127+1]; // subscribe to commands topic
	uint8_t mqtt_state_type; // percents, ON/OFF, etc
	uint16_t switch_ignore_steps; // ignore endstop for first step on moving down
	uint8_t led_mode; // Default blue LED mode
	uint8_t led_level; // Default blue LED brightness
	uint16_t preset[MAX_PRESETS]; // Position (steps) for preset 1-5
	bool mqtt_discovery; // Home Assistant MQTT Discovery enabled
	uint8_t sw_at_bottom; // 0 - switch at opened position, 1 - switch at closed position
	uint8_t mqtt_invert; // mqtt percents inverted, 0% = closed
	char mqtt_topic_alive[127+1]; // publish availability topic (Birth & LWT)
	int up_safe_limit; // make extra rotations up if not hit switch
	uint8_t btn_pin; // hardware button pin selection
	uint8_t btn_mode; // auto/mqtt report
	uint8_t slave; // master/slave mode
	uint8_t aux_pin; // auxiliary pin selection
	char mqtt_topic_aux[127+1]; // auxiliary input topic
	char mqtt_topic_info[127+1]; // information topic (IP, RSSI, etc)
	char name[64+1]; // Name
	ip4_addr ip, mask, gw, dns; // Network config
	rf_cmd_type rf_cmd[MAX_RF_CMDS]; // RF433 commands
	uint8_t rf_pin; // RF remote input pin
} ini;

// language functions
const char * L(const char *s1, const char *s2)
{
	return (ini.lang==0) ? s1 : s2;
}
const __FlashStringHelper * FL(const __FlashStringHelper *s1, const __FlashStringHelper *s2)
{
	return (ini.lang==0) ? s1 : s2;
}
#define FLF(a, b) FL(F(a), F(b))
String SL(const char *s1, const char *s2)
{
	return (ini.lang==0) ? String(s1) : String(s2);
}
String SL(String s1, String s2)
{
	return (ini.lang==0) ? s1 : s2;
}

void NetworkActivity(void)
{
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	last_network_time=millis();
}

void ToPercent(uint8_t pos, uint8_t address);
void ToPosition(int pos, uint8_t address);
void ToPreset(uint8_t preset, uint8_t address);
void Open(uint8_t address);
void Close(uint8_t address);
void Stop(uint8_t address);
void ButtonClick(uint8_t address);
void ButtonLongClick(uint8_t address);
void WiFi_On();
void WiFi_Off();
uint32_t getTime();
uint32_t getTimeUTC();
String MakeNode(const __FlashStringHelper *name, String val);

//===================== Event Logger ===================================

#define MAX_LOG_ENTRIES 32

const char ET_Err1[] PROGMEM = "Error 01";
const char ET_NTP_Sync[] PROGMEM = "NTP time syncronized";
const char ET_Settings_Loaded[] PROGMEM = "Settings loaded";
const char ET_Settings_Saved[] PROGMEM = "Settings saved";
const char ET_Settings_Not_Loaded[] PROGMEM = "Settings set to default";
const char ET_Cmd_Stop[] PROGMEM = "Stop";
const char ET_Cmd_Open[] PROGMEM = "Open";
const char ET_Cmd_Close[] PROGMEM = "Close";
const char ET_Cmd_Percent[] PROGMEM = "Go to percent";
const char ET_Cmd_Steps[] PROGMEM = "Go to steps";
const char ET_Cmd_Preset[] PROGMEM = "Go to preset";
const char ET_Cmd_Click[] PROGMEM = "Click";
const char ET_Cmd_LClick[] PROGMEM = "Long click";
const char ET_Slave_No_Ping[] PROGMEM = "No ping from master, enabling WiFi";
const char ET_Wifi_Close_AP[] PROGMEM = "Reconnected to network in STA mode. Closing AP";
const char ET_Wifi_Reconnect[] PROGMEM = "Trying to reconnect";
const char ET_Wifi_Got_IP[] PROGMEM = "IP address";
const char ET_Wifi_Start_AP[] PROGMEM = "Starting soft AP";
const char ET_Wifi_Disconnect[] PROGMEM = "WiFi disconnected";
const char ET_Endstop_Hit[] PROGMEM = "Stopped at endstop";
const char ET_Endstop_Hit_Error[] PROGMEM = "Stopped at endstop on going down";
const char ET_MQTT_Connect[] PROGMEM = "MQTT connected";
const char ET_MQTT_Connecting[] PROGMEM = "Connecting to MQTT... ";
const char ET_Started[] PROGMEM = "Started";

const char EQ_HTTP[] PROGMEM = "Src: HTTP";
const char EQ_MQTT[] PROGMEM = "Src: MQTT";
const char EQ_MASTER[] PROGMEM = "Src: Master";
const char EQ_SCHEDULE[] PROGMEM = "Src: Scheduler";
const char EQ_BUTTON[] PROGMEM = "Src: Button";
const char EQ_RF[] PROGMEM = "Src: RF";

enum EVENT_LEVEL { EL_NONE = 0, EL_DEBUG, EL_INFO, EL_WARN, EL_ERROR };
enum EVENT_ID                           { EI_Err1, EI_NTP_Sync, EI_Settings_Loaded, EI_Settings_Saved, EI_Settings_Not_Loaded, EI_Cmd_Stop, EI_Cmd_Open, EI_Cmd_Close, 
	EI_Cmd_Percent, EI_Cmd_Steps, EI_Cmd_Preset, EI_Cmd_Click, EI_Cmd_LClick, EI_Slave_No_Ping, EI_Wifi_Close_AP, EI_Wifi_Reconnect, EI_Wifi_Got_IP, EI_Wifi_Start_AP,
	EI_Endstop_Hit, EI_Endstop_Hit_Error, EI_MQTT_Connect, EI_MQTT_Connecting, EI_Started, EI_Wifi_Disconnect };
const char* const event_txt[] PROGMEM = { ET_Err1, ET_NTP_Sync, ET_Settings_Loaded, ET_Settings_Saved, ET_Settings_Not_Loaded, ET_Cmd_Stop, ET_Cmd_Open, ET_Cmd_Close,
	ET_Cmd_Percent, ET_Cmd_Steps, ET_Cmd_Preset, ET_Cmd_Click, ET_Cmd_LClick, ET_Slave_No_Ping, ET_Wifi_Close_AP, ET_Wifi_Reconnect, ET_Wifi_Got_IP, ET_Wifi_Start_AP,
	ET_Endstop_Hit, ET_Endstop_Hit_Error, ET_MQTT_Connect, ET_MQTT_Connecting, ET_Started, ET_Wifi_Disconnect };

enum EVENT_SRC                              { ES_HTTP, ES_MQTT, ES_MASTER, ES_SCHEDULE, ES_BUTTON, ES_RF };
const char* const event_src_txt[] PROGMEM = { EQ_HTTP, EQ_MQTT, EQ_MASTER, EQ_SCHEDULE, EQ_BUTTON, EQ_RF };

typedef struct {
	uint32_t time; // timecode
	uint32_t val; // value
	uint8_t event; // event code
	uint8_t level; // severity level
	uint16_t flags; // reserved
} LogEntry;

#if MAX_LOG_ENTRIES < 255
	typedef uint8_t idx;
#else
	typedef uint16_t idx;
#endif

class Log
{
private:
	idx head, count;
	LogEntry log[MAX_LOG_ENTRIES]; // 12*64 = 768 bytes of RAM
public:
	Log();
	~Log();
	void Add(uint8_t event, uint8_t level, uint32_t val);
	const LogEntry* Get(idx offset);
	idx Count();
};

Log::Log()
{
	head = count = 0;
}

Log::~Log()
{
}

void Log::Add(uint8_t event, uint8_t level, uint32_t val)
{
	uint32_t time;
	time = getTimeUTC();
	if (!time) time = millis() / 1000;
	log[head].event = event;
	log[head].level = level;
	log[head].time = time;
	log[head].val = val;
	head++;
	if (head >= MAX_LOG_ENTRIES) head = 0;
	if (count < MAX_LOG_ENTRIES) count++;
}

const LogEntry* Log::Get(idx offset)
{ // offset from head, 0 - last entry
	idx i;
	if (offset >= count) return 0;
	if (head > offset)
		i = head - offset - 1;
	else
		i = MAX_LOG_ENTRIES - offset + head - 1;
	return &log[i];
}

idx Log::Count()
{
	return count;
}

Log elog;

//===================== LED ============================================

typedef enum { LED_OFF = 0, LED_ON, LED_MQTT, LED_HTTP, LED_MQTT_HTTP, LED_ALIVE, LED_BUTTON, LED_MODE_MAX } led_modes;
typedef enum { LED_LOW = 0, LED_MED, LED_HIGH, LED_LEVEL_MAX } led_levels;
uint8_t led_mode;
uint8_t led_level;

const __FlashStringHelper* LEDModeString(uint8_t mode = 0xFF)
{
	if (mode == 0xFF) mode = led_mode;
	if (mode == LED_OFF) return FLF("Off", "Выключен");
	if (mode == LED_ON) return FLF("On", "Включен");
	if (mode == LED_MQTT) return FLF("On MQTT commands", "При MQTT командах");
	if (mode == LED_HTTP) return FLF("On HTTP requests", "При HTTP запросах");
	if (mode == LED_MQTT_HTTP) return FLF("On MQTT and HTTP", "При MQTT и HTTP");
	if (mode == LED_ALIVE) return FLF("Blink alive", "Мигать периодически");
	if (mode == LED_BUTTON) return FLF("On button press", "По нажатию кнопки");
	return F("");
}

const __FlashStringHelper* LEDLevelString(uint8_t level = 0xFF)
{
	if (level == 0xFF) level = led_level;
	if (level == LED_LOW) return FLF("Low", "Низкая");
	if (level == LED_MED) return FLF("Medium", "Средняя");
	if (level == LED_HIGH) return FLF("High", "Высокая");
	return F("");
}

void LED_On()
{
	digitalWrite(PIN_LED, LOW);
}

void LED_Off()
{
	digitalWrite(PIN_LED, HIGH);
}

void LED_Blink(uint8_t level = 0xFF)
{
	int delay_ms=0;
	if (level == 0xFF) level=led_level;
	if (level == LED_LOW) delay_ms=1; else
	if (level == LED_MED) delay_ms=20; else
	if (level == LED_HIGH) delay_ms=300;
	LED_On();
	delay(delay_ms);
	LED_Off();
}

bool LED_Command(const char *cmd)
{
	if      (strcmp(cmd, "on") == 0) led_mode=LED_ON;
	else if (strcmp(cmd, "off") == 0) led_mode=LED_OFF;
	else if (strcmp(cmd, "mqtt") == 0) led_mode=LED_MQTT;
	else if (strcmp(cmd, "http") == 0) led_mode=LED_HTTP;
	else if (strcmp(cmd, "mqtt_http") == 0) led_mode=LED_MQTT_HTTP;
	else if (strcmp(cmd, "alive") == 0) led_mode=LED_ALIVE;
	else if (strcmp(cmd, "button") == 0) led_mode=LED_BUTTON;

	else if (strcmp(cmd, "low") == 0) led_level=LED_LOW;
	else if (strcmp(cmd, "med") == 0) led_level=LED_MED;
	else if (strcmp(cmd, "high") == 0) led_level=LED_HIGH;

	else if (strcmp(cmd, "blink") == 0) LED_Blink();
	else if (strcmp(cmd, "blink_low") == 0) LED_Blink(LED_LOW);
	else if (strcmp(cmd, "blink_med") == 0) LED_Blink(LED_MED);
	else if (strcmp(cmd, "blink_high") == 0) LED_Blink(LED_HIGH);

	else return 0;
	return 1;
}

void ProcessLED()
{
	static uint32_t last_blink=0;

	if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP) return;

	if (led_mode == LED_ON)
	{
		if (led_level == LED_LOW) analogWrite(PIN_LED, 980); else
		if (led_level == LED_MED) analogWrite(PIN_LED, 800); else
			LED_On();
	}
	else if (led_mode == LED_ALIVE)
	{
		if (millis()-last_blink > 10000)
		{
			last_blink=millis();
			LED_Blink();
		}
	}
	else LED_Off();
}

//===================== Captive Portal =================================
DNSServer dnsServer;
bool cp_active=false;
const uint16_t DNS_PORT = 53;

void CP_create()
{
	dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
	dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
	cp_active=true;
}

void CP_delete()
{
	cp_active=false;
	dnsServer.stop();
}

void CP_process()
{
	if (!cp_active) return;
	dnsServer.processNextRequest();
}

//===================== NTP ============================================

const unsigned long NTP_WAIT=5000;
const unsigned long NTP_SYNC=24*60*60*1000; // sync clock once a day
unsigned long lastRequest=0; // timestamp of last request
unsigned long lastSync=0; // timestamp of last successful synchronization
IPAddress timeServerIP;
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
const uint32_t seventyYears = 2208988800UL;
uint32_t UNIXTime;
WiFiUDP UDP;

void setup_NTP()
{
//	Serial.println("udp open");
	UDP.begin(123); // Start listening for UDP messages on port 123
}

void SyncNTPTime()
{
	int packetSize = UDP.parsePacket();
//	if (packetSize)
//		Serial.printf("Received %d bytes from %s, port %d\n", packetSize, UDP.remoteIP().toString().c_str(), UDP.remotePort());
	if (packetSize == 0)
	{ // If there's no response (yet)
		if (lastSync!=0 && (millis()-lastSync < NTP_SYNC)) return; // time ok
		if (millis()-lastRequest < NTP_WAIT) return; // waiting for answer
		lastRequest=millis();
		memset(NTPBuffer, 0, NTP_PACKET_SIZE); // set all bytes in the buffer to 0
		// Initialize values needed to form NTP request
		NTPBuffer[0] = 0b11100011; // LI, Version, Mode
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
//	Serial.println(F("NTP packet received"));
	if (UDP.remoteIP() == timeServerIP && UDP.read(NTPBuffer, NTP_PACKET_SIZE) == NTP_PACKET_SIZE) // read the packet into the buffer
	{
		// Combine the 4 timestamp bytes into one 32-bit number
		uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
		// Convert NTP time to a UNIX timestamp: subtract seventy years:
		NTPTime -= seventyYears;
		elog.Add(EI_NTP_Sync, EL_INFO, NTPTime);
		UNIXTime = NTPTime;
		lastSync=millis();
	}
	UDP.flush();
}

uint32_t getTime()
{
	if (lastSync == 0) return 0;
	return UNIXTime+ini.timezone*60 + (millis() - lastSync)/1000;
}

uint32_t getTimeUTC()
{
	if (lastSync == 0) return 0;
	return UNIXTime + (millis() - lastSync)/1000;
}

String TimeStr()
{
	char buf[9];
	uint32_t t=getTime();
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), t/60/60%24, t/60%60, t%60);
	return String(buf);
}

String UptimeStr()
{
	char buf[9];
	uint32_t t=millis()/1000;
	sprintf_P(buf, PSTR("%dd %02d:%02d"), t/60/60/24, t/60/60%24, t/60%60);
	return String(buf);
}

const __FlashStringHelper* DoWName(int d)
{
	switch (d)
	{
		case 0: return FLF("Mo", "Пн"); break;
		case 1: return FLF("Tu", "Вт"); break;
		case 2: return FLF("We", "Ср"); break;
		case 3: return FLF("Th", "Чт"); break;
		case 4: return FLF("Fr", "Пт"); break;
		case 5: return FLF("Sa", "Сб"); break;
		case 6: return FLF("Su", "Вс"); break;
	}
	return F("");
}

int DayOfWeek(uint32_t time)
{
	return ((time / DAY) + 3) % 7;
}

// ===================== UART master/slave =============================

#define UART_CMD_PERCENT '%'
#define UART_CMD_POSITION '='
#define UART_CMD_PRESET '@'
#define UART_CMD_OPEN 'o'
#define UART_CMD_CLOSE 'c'
#define UART_CMD_STOP 's'
#define UART_CMD_PING 'p'
#define UART_CMD_WAKE 'w' // enable wifi
#define UART_CMD_BLINK 'b' // blink led
#define UART_CMD_CLICK 'k' // button klick
#define UART_CMD_LONGCLICK 'K' // button long press
#define CRC_INIT 0xEA // just a random nonzero number for checksum

uint32_t lastUARTping = 0;
char const *int2hex="0123456789ABCDEF";
void SendUART(uint8_t cmd, uint8_t address, uint32_t val=0)
{
	uint8_t buf[11], crc;

	if (!MASTER) return;
	buf[0]='~';
	if (address == ADDR_ALL) buf[1]='*'; else buf[1]='0'+address;
	buf[2]=cmd;
	buf[3]='0' + ((val / 10000) % 10);
	buf[4]='0' + ((val / 1000) % 10);
	buf[5]='0' + ((val / 100) % 10);
	buf[6]='0' + ((val / 10) % 10);
	buf[7]='0' + ((val    ) % 10);
	crc=CRC_INIT;
	for (int i=0; i<8; i++) crc += buf[i];
	buf[8] = int2hex[crc>>4];
	buf[9] = int2hex[crc & 0x0F];
	buf[10] = 0;

	Serial.print((char *)&buf);
}

void SendUARTPing()
{
	static uint32_t lastping;
	uint32_t time;
	uint8_t buf[11], crc;

	if (millis() - lastping < UART_PING_INTERVAL_MS) return;
	lastping = millis();
	if (lastSync == 0) time = 0;
	else time = UNIXTime + (millis() - lastSync)/1000;

	if (!MASTER) return;
	buf[0]='~';
	buf[1]='*';
	buf[2]=UART_CMD_PING;
	buf[3]=(time >> 24);
	buf[4]=(time >> 16) & 0xFF;
	buf[5]=(time >>  8) & 0xFF;
	buf[6]=(time      ) & 0xFF;
	buf[7]='0';
	crc=CRC_INIT;
	for (int i=0; i<8; i++) crc += buf[i];
	buf[8] = int2hex[crc>>4];
	buf[9] = int2hex[crc & 0x0F];
	buf[10] = 0;

	Serial.write(buf, 10);
}

void UARTPingReceived(uint32_t time)
{
	WiFi_AP_disabled = true; // Disabling AP mode if master alive
	lastUARTping = lastSync = millis();
	UNIXTime = time;
	//	Serial.println(TimeStr() + " ["+ DoWName(DayOfWeek(getTime())) +"]");
}

void ProcessUART()
{
	static uint8_t buf[11];
	static uint8_t inbuf = 0;
	uint8_t crc;
	uint32_t val;

	// Enable WiFi in slave mode if no ping from master for SLAVE_MAX_NO_PING_MS
	if (SLAVE && !WiFi_AP_disabled && !WiFi_active && (millis() - lastUARTping > SLAVE_MAX_NO_PING_MS))
	{
		Serial.println(F("No ping from master, enabling WiFi"));
		elog.Add(EI_Slave_No_Ping, EL_ERROR, 0);
		WiFi_On();
	}

	while (Serial.available())
	{
		buf[inbuf++] = Serial.read();

		if (inbuf == 10)
		{
			crc=CRC_INIT;
			for (int i=0; i<8; i++) crc += buf[i];
			if (buf[0] == '~' && buf[8] == int2hex[crc>>4] && buf[9] == int2hex[crc & 0x0F])
			{ // valid command
				if (buf[1] == '*' || buf[1]-'0' == ini.slave)
				{ // our address
					if (buf[2] == UART_CMD_PING)
						val = ((uint32_t)buf[3] << 24) + ((uint32_t)buf[4] << 16) + ((uint32_t)buf[5] << 8) + buf[6];
					else
						val = (buf[3]-'0')*10000 + (buf[4]-'0')*1000 + (buf[5]-'0')*100 + (buf[6]-'0')*10 + (buf[7]-'0');
					switch (buf[2])
					{
						case UART_CMD_OPEN: Open(ADDR_SELF_ONLY); break;
						case UART_CMD_CLOSE: Close(ADDR_SELF_ONLY); break;
						case UART_CMD_STOP: Stop(ADDR_SELF_ONLY); break;
						case UART_CMD_PERCENT: ToPercent(val, ADDR_SELF_ONLY); break;
						case UART_CMD_POSITION: ToPosition(val, ADDR_SELF_ONLY); break;
						case UART_CMD_PRESET: ToPreset(val, ADDR_SELF_ONLY); break;
						case UART_CMD_WAKE: WiFi_On(); break;
						case UART_CMD_BLINK: LED_Blink(LED_HIGH); break;
						case UART_CMD_PING: UARTPingReceived(val); break;
						case UART_CMD_CLICK: ButtonClick(ADDR_SELF_ONLY); break;
						case UART_CMD_LONGCLICK: ButtonLongClick(ADDR_SELF_ONLY); break;
					}
				}
				inbuf=0;
			} else
			{ // invalid command
				if (buf[0] == '~')
				{
					Serial.println(F("Invalid CRC in uart packet"));
					uart_crc_errors++;
				}
				for (int i=0; i<10-1; i++) buf[i] = buf[i+1]; // shifting buffer, removing 1st byte
				inbuf--;
			}
		}
	}
}

//----------------------- Motor ----------------------------------------
#define PINOUT_SD 3
const uint8_t microstep[8][4]={
	{1, 0, 0, 0},
	{1, 1, 0, 0},
	{0, 1, 0, 0},
	{0, 1, 1, 0},
	{0, 0, 1, 0},
	{0, 0, 1, 1},
	{0, 0, 0, 1},
	{1, 0, 0, 1}};
void FillStepsTable()
{
	uint8_t i, j, n;

	if (ini.pinout >= PINOUT_SD) return; // not needed for step/dir

	for (i=0; i<8; i++)
	{
		n=i;
		if (ini.reversed) n=7-i;
		step_s[i]=0;
		step_c[i]=0;
		for (j=0; j<4; j++)
		{
			if (microstep[n][j])
				step_s[i] |= (1 << steps[ini.pinout][j]);
			else
				step_c[i] |= (1 << steps[ini.pinout][j]);
		}
	}
}

void inline ICACHE_RAM_ATTR MotorOff()
{
	if (ini.pinout != PINOUT_SD)
		// digitalWrite(PIN_A, LOW);
		// digitalWrite(PIN_B, LOW);
		// digitalWrite(PIN_C, LOW);
		// digitalWrite(PIN_D, LOW);
		GPOC = (1 << PIN_A) | (1 << PIN_B) | (1 << PIN_C) | (1 << PIN_D);
	else
		GPOS = (1 << PIN_EN) | (1 << PIN_ST); // Disable, step end
}

bool ICACHE_RAM_ATTR IsSwitchPressed()
{
	// return GPIP(PIN_SWITCH) ^^ ini.switch_reversed;
	return (((GPI >> ((PIN_SWITCH) & 0xF)) & 1) != ini.switch_reversed);
}

int Position2Percents(int pos)
{
	int percent;
	if (pos<=0) percent=0;
	else if (pos>=ini.full_length) percent=100;
	else percent=(pos*100 + (ini.full_length/2)) / ini.full_length;
	if (ini.sw_at_bottom) percent=100-percent;
	return percent;
}

void ToPercent(uint8_t pos, uint8_t address=ADDR_ALL)
{
	if ((pos<0) || (pos>100)) return;

	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_PERCENT, address, pos);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (ini.sw_at_bottom) pos=100-pos;

		if (position<0) position=0;
		if (pos == 0)
			roll_to=0-ini.up_safe_limit; // up to 0 and beyond (a little)
		else
			roll_to=ini.full_length * pos / 100;
	}
}

void ToPosition(int pos, uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_POSITION, address, pos);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (pos<0 || pos>ini.full_length) return;
		if (position<0) position=0;
		roll_to=pos;
	}
}

void ToPreset(uint8_t preset, uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_PRESET, address, preset);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (preset==0 || preset > MAX_PRESETS) return;
		if (position<0) position=0;
		roll_to=ini.preset[preset-1];
	}
}

void Open(uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_OPEN, address);
	if (address == ADDR_MASTER || address == ADDR_ALL) ToPercent(0, ADDR_MASTER);
}

void Close(uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_CLOSE, address);
	if (address == ADDR_MASTER || address == ADDR_ALL) ToPercent(100, ADDR_MASTER);
}

void Stop(uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER)) SendUART(UART_CMD_STOP, address);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		roll_to=position;
		MotorOff();
	}
}

uint32_t GetVoltage()
{
	int v, i, min;

	// We need to get a minimal value from a few samples with small delay for better accuracy
	// otherwise ESP gives a little bit higher values.
	min=analogRead(A0);
	for (i=0; i<3; i++)
	{
		delay(1);
		v=analogRead(A0);
		if (v<min) min=v;
	}
	return min*121/8; // *1000*16/1024 mV, 150K:10K divider gives *125/8, but can be adjusted a little
}

const char * GetVoltageStr()
{
	static char buf[5];
	uint32_t v;
	uint8_t n=0;

	v=GetVoltage();
	if (v>10000) buf[n++]=v/10000%10+'0';
	buf[n++]=v/1000%10+'0';
	buf[n++]='.';
	buf[n++]=v/100%10+'0';
	buf[n++]=0;
	return buf;
}

//===================== MQTT ===========================================

#if MQTT

uint8_t pin_aux, aux_state;
uint32_t last_mqtt=0, last_mqtt_info=0;
String mqtt_topic_sub, mqtt_topic_pub, mqtt_topic_lwt, mqtt_topic_aux, mqtt_topic_inf;

WiFiClient espClient;
PubSubClient *mqtt = NULL;
enum AUX_STATES { AUX_NONE, AUX_ON, AUX_OFF };

void UpdateMQTTInfo()
{
	last_mqtt_info=0;	
}

void ICACHE_RAM_ATTR auxISR()
{
	static uint8_t lastState=255;

	uint8_t state = !digitalRead(pin_aux); // active low
	if (state == lastState) return; // ignore duplicate readings

	if (state) aux_state = AUX_ON; else aux_state = AUX_OFF;

	lastState = state;
	UpdateMQTTInfo();
}

const char *aux_state_str()
{
	if (aux_state == AUX_ON) return "ON";
	if (aux_state == AUX_OFF) return "OFF";
	return "";
}

void MQTT_discover();

void mqtt_callback(char* topic, byte* payload, unsigned int len)
{
	int x, p;
	uint8_t address;
	char *str=(char*)payload;
	if (len==0) return;

	address=ADDR_ALL;
	if (len>2 && str[0]=='$') // master/slave selected
	{
		if (str[1] >= '0' && str[1] <='9') address = str[1]-'0';
		str+=2;
		len-=2;
	}

	if (led_mode == LED_MQTT || led_mode == LED_MQTT_HTTP) LED_Blink();

	NetworkActivity();

	for(unsigned int i = 0; i<len; i++) str[i] = tolower(str[i]); // make it lowercase
	str[len]=0;

	x=strtol(str, NULL, 10);
	if ((x>0 && x<=100) || strcmp(str, "0") == 0)
	{
		if (ini.mqtt_invert)
			ToPercent(100-x, address);
		else
			ToPercent(x, address);
	}
	else if (strcmp(str, "on") == 0) { Open(address); elog.Add(EI_Cmd_Open, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "off") == 0) { Close(address); elog.Add(EI_Cmd_Close, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "open") == 0) { Open(address); elog.Add(EI_Cmd_Open, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "close") == 0) { Close(address); elog.Add(EI_Cmd_Close, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "click") == 0) { ButtonClick(address); elog.Add(EI_Cmd_Click, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "longclick") == 0) { ButtonLongClick(address); elog.Add(EI_Cmd_LClick, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "stop") == 0) { Stop(address); elog.Add(EI_Cmd_Stop, EL_INFO, ES_MQTT); last_mqtt=0; } // report current position after stop command
	else if (strncmp(str, "led_", 4) == 0) LED_Command(str+4); // starts with "led_"
	else if (strncmp(str, "=", 1) == 0) { p=strtol(str+1, NULL, 10); ToPosition(p, address); elog.Add(EI_Cmd_Steps, EL_INFO, ES_MQTT + (p<<8)); }
	else if (strncmp(str, "@", 1) == 0) { p=strtol(str+1, NULL, 10); ToPreset(p, address); elog.Add(EI_Cmd_Preset, EL_INFO, ES_MQTT + (p<<8)); }
}

String ReplaceHostname(const char *topic)
{
	String s;
	s=String(topic);
	s.replace("%HOSTNAME%", String(ini.hostname));
	return s;
}

void setup_MQTT()
{
	mqtt_topic_sub = ReplaceHostname(ini.mqtt_topic_command);
	mqtt_topic_pub = ReplaceHostname(ini.mqtt_topic_state);
	mqtt_topic_lwt = ReplaceHostname(ini.mqtt_topic_alive);
	mqtt_topic_aux = ReplaceHostname(ini.mqtt_topic_aux);
	mqtt_topic_inf = ReplaceHostname(ini.mqtt_topic_info);
	if (mqtt)
	{
		if (mqtt_topic_lwt != "-") mqtt->publish(mqtt_topic_lwt.c_str(), "offline", true);
		mqtt->disconnect();
	} else
		mqtt = new PubSubClient(espClient);
	mqtt->setServer(ini.mqtt_server, ini.mqtt_port);
	mqtt->setKeepAlive(ini.mqtt_ping_interval);
	mqtt->setBufferSize(1024);
	if (mqtt_topic_sub != "-")
		mqtt->setCallback(mqtt_callback);
}

void MQTT_connect()
{
	static uint32_t last_reconnect=0;

	if (!ini.mqtt_enabled) return;

	// Stop if already connected.
	if (mqtt->connected()) return;

	if (WiFi.status() != WL_CONNECTED) return;

	if ((last_reconnect != 0) && (millis() - last_reconnect < 10000)) return;

	Serial.print(F("Connecting to MQTT... "));
	elog.Add(EI_MQTT_Connecting, EL_INFO, 0);

	bool res;
	if (mqtt_topic_lwt != "-")
		res=mqtt->connect(ini.hostname, ini.mqtt_login, ini.mqtt_password, mqtt_topic_lwt.c_str(), 1, true, "offline");
	else
		res=mqtt->connect(ini.hostname, ini.mqtt_login, ini.mqtt_password);
	if (res == false)
	{ // connect will return 0 for connected
		Serial.println(mqtt->state());
		Serial.println(F("Retrying MQTT connection in 10 seconds..."));
		mqtt->disconnect();
		last_reconnect=millis();
	} else
	{
		Serial.println(F("MQTT Connected!"));
		elog.Add(EI_MQTT_Connect, EL_INFO, 0);
		last_reconnect=0;
		last_mqtt=last_mqtt_info=0;
		if (mqtt_topic_sub != "-")
			mqtt->subscribe(mqtt_topic_sub.c_str());
		if (ini.mqtt_discovery) MQTT_discover();
		if (mqtt_topic_lwt != "-") mqtt->publish(mqtt_topic_lwt.c_str(), "online", true);
	}
}

void MQTT_discover_delete_sensor(const __FlashStringHelper* sensor_id, bool binary = false)
{
	String mqtt_topic;
	if (binary)
		mqtt_topic = F("homeassistant/binary_sensor/");
	else
		mqtt_topic = F("homeassistant/sensor/");
	mqtt_topic += ini.hostname;
	mqtt_topic += F("/");
	mqtt_topic += sensor_id;
	mqtt_topic += F("/config");
	mqtt->publish(mqtt_topic.c_str(), "", false);
}

void MQTT_Delete_HA_Sensors()
{
	MQTT_discover_delete_sensor(F("ip"));
	MQTT_discover_delete_sensor(F("rssi"));
	MQTT_discover_delete_sensor(F("uptime"));
	MQTT_discover_delete_sensor(F("voltage"));
	MQTT_discover_delete_sensor(F("aux"), true);
}

void MQTT_discover_add_sensor(const char * device_id, 
	const __FlashStringHelper* name, 
	const __FlashStringHelper* sensor_id, 
	const __FlashStringHelper* dev_class,
	const __FlashStringHelper* icon, 
	const __FlashStringHelper* unit, 
	bool binary = false)
{
	String mqtt_topic, mqtt_data;
	
#define qpv(a, b) { mqtt_data += F("\"" a "\":\""); mqtt_data += b; } // "a":"b (b - string)
#define qpc(a, b) { mqtt_data += F("\"" a "\":\"" b); } // "a":"b (b - const)
#define cqpv(a, b) { mqtt_data += F("\",\"" a "\":\""); mqtt_data += b; } // ", "a":"b (b - string)
#define cqpc(a, b) { mqtt_data += F("\",\"" a "\":\"" b); } // ", "a":"b (b - const)
#define sqpv(a, b) { mqtt_data = F("{\"" a "\":\""); mqtt_data += b;} // {"a":"b
	if (binary)
		mqtt_topic = F("homeassistant/binary_sensor/");
	else
		mqtt_topic = F("homeassistant/sensor/");
	mqtt_topic += String(ini.hostname)+"/"+sensor_id+"/config";
	sqpv("name", ini.hostname);
	mqtt_data += " ";
	mqtt_data += name;
	cqpv("stat_t", mqtt_topic_inf);
	cqpc("entity_category", "diagnostic");
	if (dev_class) cqpv("dev_cla", dev_class);
	mqtt_data += F("\",\"dev\":{\"ids\":[\"");
	mqtt_data += device_id;
	mqtt_data += "\"]},";
	qpv("unique_id", device_id);
	mqtt_data += "_";
	mqtt_data += sensor_id;
	if (icon) cqpv("ic", icon);
	if (mqtt_topic_lwt != "-")
		cqpv("avty_t", mqtt_topic_lwt);
	if (unit) cqpv("unit_of_meas", unit);
	mqtt_data += F("\",\"val_tpl\":\"{{value_json.");
	mqtt_data += sensor_id;
	mqtt_data += F("}}\"}");

	mqtt->publish(mqtt_topic.c_str(), mqtt_data.c_str(), true);
}

void MQTT_discover()
{
	String mqtt_topic, mqtt_data;
	char id[17];

	if (!ini.mqtt_enabled) return;
	if (!mqtt->connected()) return;

	snprintf_P(id, 17, PSTR("lazyroll%08X"), ESP.getChipId());
	mqtt_topic = F("homeassistant/cover/");
	mqtt_topic += ini.hostname;
	mqtt_topic += F("/config");
	sqpv("name", ini.hostname);
	cqpv("unique_id", id);
	mqtt_data += F("_blind");
	cqpv("~", mqtt_topic_sub);
	cqpc("set_pos_t", "~");
	cqpv("pos_t", mqtt_topic_pub);
	cqpc("cmd_t", "~");
	mqtt_data += F("\",\"dev\":{\"ids\":[\"");
	mqtt_data += id;
	mqtt_data += "\"],";
	qpv("name", ini.hostname);
	cqpc("mdl", "LazyRoll [");
	mqtt_data += WiFi.localIP().toString();
	mqtt_data += F("]");
	cqpc("mf", "imlazy.ru");
	cqpv("cu", "http://" + WiFi.localIP().toString() + F("/settings"));
	cqpc("sw", VERSION "\"},");
	qpc("dev_cla", "blind");
	if (mqtt_topic_lwt != "-")
			cqpv("avty_t", mqtt_topic_lwt);
	if (ini.mqtt_invert)
		mqtt_data+=F("\",\"pos_clsd\":0,\"pos_open\":100}");
	else
		mqtt_data+=F("\",\"pos_clsd\":100,\"pos_open\":0}");

	mqtt->publish(mqtt_topic.c_str(), mqtt_data.c_str(), true);

	// Additional HA sensors
	if (mqtt_topic_inf != "-")
	{
		MQTT_discover_add_sensor(id, F("IP"), F("ip"), NULL, F("mdi:ip-network-outline"), NULL);
		MQTT_discover_add_sensor(id, F("RSSI"), F("rssi"), F("signal_strength"), NULL, F("dBm"));
		MQTT_discover_add_sensor(id, F("uptime"), F("uptime"), NULL, F("mdi:clock-time-five-outline"), NULL);
		MQTT_discover_add_sensor(id, F("voltage"), F("voltage"), F("voltage"), NULL, NULL);
		if (ini.aux_pin !=0)
		{
			MQTT_discover_add_sensor(id, F("aux"), F("aux"), F("window"), NULL, NULL, true);
		} else
		{
			mqtt_topic = F("homeassistant/binary_sensor/");
			mqtt_topic += ini.hostname;
			mqtt_topic += F("/aux/config");
			mqtt->publish(mqtt_topic.c_str(), "", true);
		}
		last_mqtt_info=0;
	}
	else
		MQTT_Delete_HA_Sensors();
}

void ProcessMQTT()
{
	static int last_val;

	if (!ini.mqtt_enabled) return;

	// Ensure the connection to the MQTT server is alive (this will make the first
	// connection and automatically reconnect when disconnected).
	MQTT_connect();

	if (!mqtt->connected()) return;
	mqtt->loop();

	// Publishing
	if (mqtt_topic_pub != "-")
	{
		int val, val2;
		switch (ini.mqtt_state_type)
		{
			case 1:
			case 2:
				val=Position2Percents(position) < 50;
				break;
			default:
				val=Position2Percents(position);
				if (ini.mqtt_invert) val=100-val;
			break;
		}
		if (val != last_val || last_mqtt==0 || millis()-last_mqtt > 60*60*1000)
		{
			char buf[32];
			last_val=val;
			last_mqtt=millis();
			switch (ini.mqtt_state_type)
			{
				case 0:
					sprintf_P(buf, PSTR("%i"), val);
					break;
				case 1:
					if (val == 0) strcpy(buf, "OFF"); else strcpy(buf, "ON");
					break;
				case 2:
					if (val == 0) strcpy(buf, "0"); else strcpy(buf, "1");
					break;
				case 3:
					val2=Position2Percents(roll_to);
					if (ini.mqtt_invert) val2=100-val2;
					sprintf_P(buf, PSTR("{\"state\":\"%s\", \"position\":\"%d\", \"destination\":\"%d\"}"), (val == 0 ? "OFF" : "ON"), val, val2);
					break;
				default:
					buf[0]=0;
					break;
			}
			mqtt->publish(mqtt_topic_pub.c_str(), buf);
		}
	}
	// Information
	if (mqtt_topic_inf != "-")
	{
		if (last_mqtt_info==0 || millis()-last_mqtt_info > MQTT_INFO_SECONDS*1000)
		{
			char buf[128];
			IPAddress ip=WiFi.localIP();
			last_mqtt_info=millis();
			sprintf_P(buf, PSTR("{\"ip\":\"%d.%d.%d.%d\",\"rssi\":\"%d\",\"uptime\":\"%s\",\"voltage\":\"%s\",\"aux\":\"%s\"}"),
				ip[0], ip[1], ip[2], ip[3], WiFi.RSSI(), UptimeStr().c_str(), GetVoltageStr(), aux_state_str());
			mqtt->publish(mqtt_topic_inf.c_str(), buf);
		}
	}
}

const __FlashStringHelper* MQTTstatus()
{
	if (ini.mqtt_enabled)
	{
		if (mqtt->connected())
			return FL(F("Connected"), F("Подключен"));
		else
			return FL(F("Disconnected"), F("Отключен"));
	}
	else
		return FL(F("Disabled"), F("Выключен"));
}

void MQTT_ReportAux(bool on)
{
	if (mqtt_topic_aux != "-")
	{
		char buf[4];
		if (on) strcpy(buf, "ON"); else strcpy(buf, "OFF");
		mqtt->publish(mqtt_topic_aux.c_str(), buf);
	}
}

#else
void setup_MQTT() {}
void ProcessMQTT() {}
const __FlashStringHelper* MQTTstatus() { return F("Off"); }
void MQTT_ReportAux(bool on) {}
void UpdateMQTTInfo() {}
#endif

// ==================== timer & interrupt ===============================

volatile uint16_t switch_ignore_steps;

#define MAX_SPEED 500
void ICACHE_RAM_ATTR timer1Isr()
{
	static uint8_t step=0;
	static uint16_t speed=0;
	static uint8_t delay=0;

	bool dir_up;

	if (position==roll_to)
	{
		MotorOff();
		speed=0;
		return; // stopped, do nothing
	}

	if (ini.pinout == PINOUT_SD) GPOC = 1 << PIN_ST; // Finish previous step

	if (delay>0)
	{
		delay--;
		return;
	}

	if (speed < MAX_SPEED)
	{
		delay=4+ ((MAX_SPEED-speed) / 100);
		speed++;
	} else delay=3;

	dir_up=(roll_to < position); // up - true

	if (position==0 && !dir_up) switch_ignore_steps=ini.switch_ignore_steps;
	if (dir_up) switch_ignore_steps=0;

	if (IsSwitchPressed())
	{
		if (switch_ignore_steps == 0)
		{
			endstop_hit_pos = position;
			endstop_hit = EL_INFO; // Set flag, will add event in Log, but not in ISR
			if (roll_to <= 0)
			{ // full open, done
				roll_to=0;
				position=0; // zero point found
				MotorOff();
				return;
			} else
			{ // destination - [partialy] closed
				if (dir_up)
				{
					// Zero found, now can go down, ignoring switch for some steps
					position=0; // zero point found
					switch_ignore_steps=ini.switch_ignore_steps;
				} else
				{
					// endswitch hit on going down. Something wrong
					endstop_hit_pos = position;
					endstop_hit = EL_ERROR;  // Set flag, will add event in Log, but not in ISR
					roll_to=position;
					MotorOff();
					return;
				}
			}
			return;
		}
	}

	if (dir_up)
	{
		if (step > 0) step--;
		else
		{
			step=7;
			position--;
		}
	} else
	{
		step++;
		if (step==8)
		{
			step=0;
			position++;
			if (switch_ignore_steps>0) switch_ignore_steps--;
		}
	}

	if (ini.pinout != PINOUT_SD)
	{
		GPOS = step_s[step % 8];
		GPOC = step_c[step % 8];
	} else
	{
		GPOC = 1 << PIN_EN; // active - low
		if (dir_up ^ ini.reversed) GPOS = 1 << PIN_DR; else GPOC = 1 << PIN_DR;
		GPOS = 1 << PIN_ST;
	}
}

void AdjustTimerInterval()
{
	timer1_write((uint32_t)ini.step_delay_mks*ESP.getCpuFreqMHz()/4/16);
}

void SetupTimer()
{
	timer1_isr_init();
	timer1_attachInterrupt(timer1Isr);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	AdjustTimerInterval();
}

void StopTimer()
{
	timer1_disable();
}

// ==================== button =======================================

#define LONG_PRESS_MS 1000
#define SHORT_PRESS_MS 50
#define DOUBLE_CLICK_MS 3000
#define AUX_DEBOUNCE_MS 50

uint8_t pin_btn, button;
enum BTN_STATES { NO_PRESS, SHORT_PRESS, LONG_PRESS };
void ICACHE_RAM_ATTR btnISR()
{
	static uint32_t lastChange=0;
	static uint8_t lastState=0;

	uint32_t now;
	uint8_t state = !digitalRead(pin_btn); // active low
	if (state == lastState) return; // ignore duplicate readings

	now=millis();
	if (!state)
	{
		if (now - lastChange >= LONG_PRESS_MS) button=LONG_PRESS;
		else if (now - lastChange >= SHORT_PRESS_MS) button=SHORT_PRESS;
	}
	lastChange = now;
	lastState = state;
}

int pin2hw_pin(int pin)
{
	switch (pin)
	{
		case 1: return 0; break;
		case 2: return 2; break;
		case 3: return 3; break;
		case 4: return 15; break;
		default: return 0;
	}
}

void setup_Button()
{
	detachInterrupt(0);
	detachInterrupt(2);
	detachInterrupt(3);
	detachInterrupt(15);

	if (ini.btn_pin)
	{
		pin_btn = pin2hw_pin(ini.btn_pin);
		pinMode(pin_btn, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(pin_btn), btnISR, CHANGE);
	}

	aux_state = AUX_NONE;
#if MQTT
	if (ini.aux_pin)
	{
		pin_aux = pin2hw_pin(ini.aux_pin);
		pinMode(pin_aux, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(pin_aux), auxISR, CHANGE);
		auxISR();
	}
#endif

#if RF
  if (ini.rf_pin)
  {
    int pin_rf = pin2hw_pin(ini.rf_pin);
    pinMode(pin_rf, INPUT_PULLUP);
    mySwitch.enableReceive(pin_rf); //запускаем RC приемник на gpio XX
  }
#endif
}

void ButtonClick(uint8_t address=ADDR_ALL)
{
	static uint32_t lastClick;
	static uint8_t lastCommand;
	uint8_t open;

	if (MASTER && (address != ADDR_MASTER) && (address != ADDR_ALL))
	{
		SendUART(UART_CMD_CLICK, address);
		return;
	}

	if (roll_to != position)
	{ // in motion
		Stop(address);
		return;
	}
	if (millis() - lastClick < DOUBLE_CLICK_MS)
		open = !lastCommand; // invert direction on double click
	else
		open = position > ini.full_length/2;

	if (open) Open(address); else Close(address);

	lastCommand = open;
	lastClick=millis();
}

void ButtonLongClick(uint8_t address=ADDR_ALL)
{
	if (MASTER && (address != ADDR_MASTER) && (address != ADDR_ALL))
	{
		SendUART(UART_CMD_LONGCLICK, address);
		return;
	}

	if (roll_to != position)
	{ // in motion
		Stop(address);
		return;
	}
	if (position == ini.preset[0]) ToPreset(2, address); else ToPreset(1, address);
}

void process_Button()
{
	if (button == NO_PRESS) return;

	if (button == SHORT_PRESS) { ButtonClick(); elog.Add(EI_Cmd_Click, EL_INFO, ES_BUTTON); }
	if (button == LONG_PRESS) { ButtonLongClick(); elog.Add(EI_Cmd_LClick, EL_INFO, ES_BUTTON); }
	button=NO_PRESS;
	if (led_mode == LED_BUTTON) LED_Blink();
}

void process_Aux()
{
	bool on;
	if (aux_state == AUX_NONE) return;
	on = (aux_state == AUX_ON);
	aux_state = AUX_NONE;
	MQTT_ReportAux(on);
}

// ===================== RF remote =============================

#if RF
unsigned long last_rf_code = 0;
bool rf_repeat = 0;

// 0, 'None', 101, 'Open', 20, '20%', 40, '40%', 60, '60%', 80, '80%', 100, 'Close', 111, 'Preset 1', 112, 'Preset 2', 113, 'Preset 3', 114, 
// 'Preset 4', 115, 'Preset 5',102, 'Open/Close', 103, 'Stop', 104, 'Blink'
void RF_Action(uint8_t action, uint8_t flags)
{
	if (!action) return;
	if (roll_to != position && rf_repeat && (flags & RF_FLAG_STOP2ND))
	{ // stop on second click
		Stop();
		elog.Add(EI_Cmd_Stop, EL_INFO, ES_RF);
		return;
	}
	if (action == 101) { Open(ADDR_ALL); elog.Add(EI_Cmd_Open, EL_INFO, ES_RF); } else
	if (action == 100) { Close(ADDR_ALL); elog.Add(EI_Cmd_Close, EL_INFO, ES_RF); } else
	if (action == 102) { ButtonClick(ADDR_ALL); elog.Add(EI_Cmd_Click, EL_INFO, ES_RF); } else
	if (action == 103) { Stop(ADDR_ALL); elog.Add(EI_Cmd_Stop, EL_INFO, ES_RF); } else
	if (action == 104) LED_Blink(); else
	if (action > 0 && action < 100) { ToPercent(action, ADDR_ALL); elog.Add(EI_Cmd_Percent, EL_INFO, ES_RF); } else
	if (action > 110 && action <= 110+MAX_PRESETS) { ToPreset(action-110, ADDR_ALL); elog.Add(EI_Cmd_Preset, EL_INFO, ES_RF); }
}

void RF_Keypress(uint32_t code)
{
	if (rf_page_open) return; // RF command disabled, while RF settings open
	for (int i=0; i < MAX_RF_CMDS; i++)
		if (ini.rf_cmd[i].code == code)
			RF_Action(ini.rf_cmd[i].action, ini.rf_cmd[i].flags);
}

#define RF_DELAY 250
void ProcessRF()
{
	static unsigned long last_rf = 0;
	
	if (mySwitch.available())
	{
		unsigned long code = mySwitch.getReceivedValue();

		if (code == 0)
		{
			Serial.printf("Unknown encoding");
		} else {
			if (code != last_rf_code || millis()-last_rf > RF_DELAY)
			{
				last_rf = millis();
				rf_repeat = (last_rf_code == code);
				last_rf_code = code;
				Serial.printf("Received %ld ", code);
				Serial.printf(" bit %d ", mySwitch.getReceivedBitlength());
				Serial.printf(" Protocol: %d \n\r", mySwitch.getReceivedProtocol());
				RF_Keypress(code);
			} else {
				last_rf = millis();
			}
		}
		mySwitch.resetAvailable();
	}
}

#define RF_TIMEOUT 5000 // rf-command wait time, in settings page, ms
void RF_handleXML()
{
	String XML;

	if (httpServer.hasArg("get"))  // waiting for rf keypress up to RF_TIMEOUT ms, returning after press
	{
		last_rf_code = 0;
		unsigned long start = millis();
		while (!last_rf_code && (millis() - start < RF_TIMEOUT))
		{
			ProcessRF();
			delay(10);
			yield();
		}
	}
	XML.reserve(1024);
	XML = F("<?xml version='1.0'?>");
	XML += F("<Curtain>");
	XML += F("<RF>");
	XML += MakeNode(F("LastCode"), String(last_rf_code));
	XML += MakeNode(F("Hex"), String(last_rf_code, HEX));
	// XML += F("<Commands>");
	// for (int i=0; i<MAX_RF_CMDS; i++)
	// {
	// 	XML += F("<Command") + String(i) + F(">");
	// 	XML += MakeNode(F("Code"), String(ini.rf_cmd[i].code));
	// 	XML += MakeNode(F("Action"), String(ini.rf_cmd[i].action));
	// 	XML += F("</Command") + String(i) + F(">");
	// }
	// XML += F("</Commands>");
	XML += F("</RF></Curtain>");

	httpServer.sendHeader(F("Access-Control-Allow-Origin"), "*"); // Allowing Cross-Origin Resource Sharing
	httpServer.send(200, "text/XML", XML);
}

void RF_saveSettings()
{
	for (int c=0; c < MAX_RF_CMDS; c++)
	{
		String n=String(c);
		if (httpServer.hasArg("rfs"+n))
			ini.rf_cmd[c].flags |= RF_FLAG_STOP2ND;
		else
			ini.rf_cmd[c].flags &= ~RF_FLAG_STOP2ND;

		if (httpServer.hasArg("rfc"+n))
			ini.rf_cmd[c].code = atoi(httpServer.arg("rfc"+n).c_str()); // only 31 bits of code supported
			//ini.rf_cmd[c].code = strtoul(httpServer.arg("rfc"+n).c_str(), NULL, 10); // for 32 rf codes, adding 400 bytes of code

		if (httpServer.hasArg("rfa"+n))
			ini.rf_cmd[c].action = atoi(httpServer.arg("rfa"+n).c_str());
	}

	SaveSettings(&ini, sizeof(ini));
}

String RF_save()
{
	String out;
	out += F("<tr class=\"sect_name\"><td colspan=\"2\">\n<input id=\"save\" type=\"submit\" name=\"save\" value=\"");
	out += FLF("Save", "Сохранить");
	out += F("\">\n<input id=\"cancel\" type=\"button\" name=\"cancel\" onclick=\"RFCancel();\" value=\"");
	out += FLF("Cancel", "Отмена");
	out += F("\">\n</td></tr>\n");
	return out;
}

String HTML_header();
String HTML_footer();
void HTTP_redirect(String link);
void HTTP_Activity();

void RF_handleHTTP()
{
	String out;

	HTTP_Activity();

	if (httpServer.hasArg("save"))
	{
		RF_saveSettings();
		HTTP_redirect("/settings?ok=1");
		return;
	}
	rf_page_open = true;

	out = HTML_header();

	out.reserve(16384);

	out += F("<section class=\"settings\" id=\"settings_rf\">\n");

	out += F("<form method=\"post\" action=\"/rf\">\n");

	out += F("<table class=\"rf\">\n");
	//out += HTML_save();

	out += RF_save();
	out += F("<tr><td colspan=\"2\"><p>");
	out += FLF("RF commands are disabled while this page is open. Press Save or Cancel to finish setup</p>\n<p>Last RF code",
		"Управление c пульта отключено, пока открыта эта страница. Нажмите Сохранить или Отмена для выхода из режима настройки</p>\n<p>Последний код");
	out += F(": <span id=\"lastcode\">0</span></p></td>\n</tr>\n");
	out += F("<tr><td>");
	out += FLF("Code</td><td>Action", "Код</td><td>Действие");
	out += F("</td></tr>\n");

	for (int i=0; i < MAX_RF_CMDS; i++)
	{
		out += F("<tr><td colspan=\"2\"><hr/></td></tr>\n<tr>\n<td class=\"val_p\"><input type=\"text\" name=\"rfc");
		out += i;
		out += F("\" id=\"rfc");
		out += i;
		out += F("\" value=\"");
		out += ini.rf_cmd[i].code;
		out += F("\" maxlength=\"10\"/>\n<input type=\"button\" value=\"");
		out += FLF("Set", "Уст");
		out += F("\" id=\"btn");
		out += i;
		out += F("\" onclick=\"GetRFKey(");
		out += i;
		out += FLF(", '<click!>'", ", '<жми!>'");
		out += F(");\">\n</td>\n<td>\n<select id=\"rfa");
		out += i;
		out += F("\" name=\"rfa");
		out += i;
		out += F("\"></select>\n<span><label for=\"rfs");
		out += i;
		out += F("\"><input type=\"checkbox\" id=\"rfs");
		out += i;
		out += F("\"");
		if (ini.rf_cmd[i].flags & RF_FLAG_STOP2ND) out += "checked";
		out += F(" name=\"rfs");
		out += i;
		out += FLF("\">Stop on 2nd click", "\">Стоп по 2му клику");
		out += F("</label></span>\n</td>\n</tr>");
	}
	out += F("<script>const opts1 = ");
	out += FLF("[0,'None',101,'Open',20,'20%',40,'40%',60,'60%',80,'80%',100,'Close',111,'Preset 1',112,'Preset 2',113,'Preset 3',114,'Preset 4',115,'Preset 5',102,'Open/Close',103,'Stop',104,'Blink'];\n",
			   "[0,'Нет',101,'Открыть',20,'20%',40,'40%',60,'60%',80,'80%',100,'Закрыть',111,'Пресет 1',112,'Пресет 2',113,'Пресет 3',114,'Пресет 4',115,'Пресет 5',102,'Открыть/Закрыть',103,'Стоп',104,'Мигнуть'];\n");
	for (int i=0; i < MAX_RF_CMDS; i++)
	{
		out += F("AddOption('rfa");
		out += i;
		out += F("', opts1, ");
		out += ini.rf_cmd[i].action;
		out += F(");\n");
	}

	out += F("</script>");

	out += RF_save();
	out += F("</table>\n</form>\n</section>\n");

	out += HTML_footer();

	httpServer.send(200, "text/html", out);
}

#else
void ProcessRF() {}
#endif

// ======================= WiFi =================================

#define SSID_NOT_EMPTY (ini.ssid[0] != 0)

void setup_IP()
{
	if (ini.ip.addr == 0) // IP address 0.0.0.0
		WiFi.config(0, 0, 0, 0); // DHCP
	else
		WiFi.config(ini.ip, ini.gw, ini.mask, ini.dns); // Static
}

void StartSoftAP()
{ // Lets make our own Access Point with blackjack and hookers
	Serial.print(F("Starting access point. SSID: "));
	Serial.println(ini.hostname);
	elog.Add(EI_Wifi_Start_AP, EL_WARN, 0);
	WiFi.mode(WIFI_AP);
	WiFi.softAP(ini.hostname); // ... but without password
	LED_On();
	CP_create();
}

void ProcessWiFi()
{
	if (!WiFi_active) return;

	if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP)
	{ // in soft AP mode, trying to connect to network
		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.println(F("Reconnected to network in STA mode. Closing AP"));
			elog.Add(EI_Wifi_Close_AP, EL_INFO, 0);
			WiFi.softAPdisconnect(true);
			WiFi.mode(WIFI_STA);
			Serial.println(WiFi.localIP());
			elog.Add(EI_Wifi_Got_IP, EL_INFO, (uint32_t)WiFi.localIP());
			LED_Off();
			CP_delete();
			WiFi_AP_disabled = true; // Disabling AP mode until next reboot
		} else
		{
			if (last_reconnect==0) last_reconnect=millis();
			if (millis()-last_reconnect > 60*1000) // every 60 sec
			{
				Serial.println(F("Trying to reconnect"));
				elog.Add(EI_Wifi_Reconnect, EL_INFO, 0);
				last_reconnect=millis();
				setup_IP();
				WiFi.begin(ini.ssid, ini.password);
			}
		}
		return;
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		if (!WiFi_connected)
		{ // Connected successfully
			WiFi_connected = true;
			WiFi.setSleepMode(WIFI_NONE_SLEEP);

			Serial.println(WiFi.localIP());
			elog.Add(EI_Wifi_Got_IP, EL_INFO, (uint32_t)WiFi.localIP());

#if MDNSC
			if (!MDNS.begin(ini.hostname)) Serial.println(F("Error setting up MDNS responder!"));
			else Serial.println(F("mDNS responder started"));
			MDNS.addService("http", "tcp", 80);
#endif
TestDaylight();
		} else return;
	}

	if (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_IDLE_STATUS && WiFi.status() != WL_DISCONNECTED)
	{
		if (SSID_NOT_EMPTY && WiFi_attempts < MAX_RECONNECT_ATTEMPS)
		{
			WiFi.begin(ini.ssid, ini.password);
			Serial.println(F("WiFi failed, retrying."));
			LED_On();
			delay(500);
			LED_Off();
			if (!WiFi_AP_disabled) WiFi_attempts++; // Do not count attempts if AP mode disabled (it is enable only at startup)
		}
		else
		{ // Cannot connect to WiFi
			StartSoftAP();
		}
	}
}
WiFiEventHandler disconnectedEventHandler, authModeChangedEventHandler;
void WiFi_On()
{
	if (SLAVE) Serial.println(F("Enabling WiFi"));
	last_network_time = millis();
	WiFi_active = true;
	WiFi_attempts = 0;
	WiFi.mode(WIFI_STA);
	WiFi.hostname(ini.hostname);
	setup_IP();
	if (SSID_NOT_EMPTY)
		WiFi.begin(ini.ssid, ini.password);
	else
		StartSoftAP();
	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) 
	{
		Serial.println(F("Disconnected"));
		elog.Add(EI_Wifi_Disconnect, EL_ERROR, 0);
	});
	authModeChangedEventHandler = WiFi.onStationModeAuthModeChanged([](const WiFiEventStationModeAuthModeChanged & event) { Serial.println(F("Auth mode changed")); });
	ProcessWiFi();
}

void WiFi_Off()
{
	WiFi_active = false;
	WiFi_connected = false;
	WiFi.mode(WIFI_OFF);
	WiFi.forceSleepBegin();
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
			Serial.println(F("file creation failed"));
		} else
		{
			bytes=len;
			while (bytes>0)
			{
				blk=min(bytes, (unsigned int)sizeof(buf));
				memcpy_P(buf, data, blk);
				data+=blk;
				bytes-=blk;
				f.write(buf, blk);
			}
			f.close();
			Serial.print(F("file written: "));
			Serial.println(filename);
		}
	}
}
#endif

void init_SPIFFS()
{
#ifdef SPIFFS_AUTO_INIT
	#ifdef FAV_COMPRESSED
		SPIFFS.remove(FAV_FILE);
		CreateFile(FAV_FILE ".gz", fav_icon_data, sizeof(fav_icon_data));
	#else
		SPIFFS.remove(FAV_FILE ".gz");
		CreateFile(FAV_FILE, fav_icon_data, sizeof(fav_icon_data));
	#endif
	#ifdef CSS_COMPRESSED
		SPIFFS.remove(CLASS_FILE);
		CreateFile(CLASS_FILE ".gz", css_data, sizeof(css_data));
	#else
		SPIFFS.remove(CLASS_FILE ".gz");
		CreateFile(CLASS_FILE, css_data, sizeof(css_data));
	#endif
	#ifdef JS_COMPRESSED
		SPIFFS.remove(JS_FILE);
		CreateFile(JS_FILE ".gz", js_data, sizeof(js_data));
	#else
		SPIFFS.remove(JS_FILE ".gz");
		CreateFile(JS_FILE, js_data, sizeof(js_data));
	#endif
	if (ini.spiffs_time != spiffs_time)
	{
		ini.spiffs_time=spiffs_time;
		SaveSettings(&ini, sizeof(ini));
	}
#endif
}

void ValidateSettings()
{
	if (ini.lang<0 || ini.lang>=Languages) ini.lang=0;
	if (ini.pinout<0 || ini.pinout>=4) ini.pinout=0;
	if (ini.step_delay_mks < MIN_STEP_DELAY) ini.step_delay_mks=MIN_STEP_DELAY;
	if (ini.step_delay_mks>=65000) ini.step_delay_mks=def_step_delay_mks;
	if (ini.timezone<-11*60 || ini.timezone>=14*60) ini.timezone=0;
	if (ini.full_length<300 || ini.full_length>999999) ini.full_length=10000;
	if (ini.switch_reversed>1) ini.switch_reversed=1;
	if (ini.switch_ignore_steps<5) ini.switch_ignore_steps=DEFAULT_SWITCH_IGNORE_STEPS;
	if (ini.switch_ignore_steps>65000) ini.switch_ignore_steps=65000;
	if (ini.up_safe_limit<0) ini.up_safe_limit=DEFAULT_UP_SAFE_LIMIT;
	if (ini.up_safe_limit>65000) ini.up_safe_limit=65000;
	if (!ini.mqtt_server[0]) strcpy_P(ini.mqtt_server, def_mqtt_server);
	if (ini.mqtt_port == 0) ini.mqtt_port=def_mqtt_port;
	if (ini.mqtt_ping_interval < 5) ini.mqtt_ping_interval=60;
	if (ini.mqtt_topic_state[0] == 0) strcpy_P(ini.mqtt_topic_state, def_mqtt_topic_state);
	if (ini.mqtt_topic_command[0] == 0) strcpy_P(ini.mqtt_topic_command, def_mqtt_topic_command);
	if (ini.mqtt_topic_alive[0] == 0) strcpy_P(ini.mqtt_topic_alive, def_mqtt_topic_alive);
	if (ini.mqtt_topic_aux[0] == 0) strcpy_P(ini.mqtt_topic_aux, def_mqtt_topic_aux);
	if (ini.mqtt_topic_info[0] == 0) strcpy_P(ini.mqtt_topic_info, def_mqtt_topic_info);
	if (ini.mqtt_state_type>3) ini.mqtt_state_type=0;
	if (ini.led_mode >= LED_MODE_MAX) ini.led_mode=0;
	if (ini.led_level >= LED_LEVEL_MAX) ini.led_level=0;
	for (int i=0; i<MAX_PRESETS; i++)
		if (ini.preset[i] > ini.full_length) ini.preset[i] = ini.full_length;
}

void setup_Settings(void)
{
	memset(&ini, 0, sizeof(ini));
		ini.up_safe_limit=DEFAULT_UP_SAFE_LIMIT;
	if (LoadSettings(&ini, sizeof(ini)))
	{
		Serial.println(F("Settings loaded"));
		elog.Add(EI_Settings_Loaded, EL_INFO, 0);
	} else
	{
		sprintf_P(ini.hostname , def_hostname, ESP.getChipId() & 0xFFFFFF);
		strcpy_P(ini.ssid      , def_ssid);
		strcpy_P(ini.password  , def_password);
		strcpy_P(ini.ntpserver , def_ntpserver);
		ini.lang=0;
		ini.pinout=2;
		ini.reversed=false;
		ini.step_delay_mks=def_step_delay_mks;
		ini.timezone=3*60; // Default City time zone by default :)
		ini.full_length=11300;
		ini.spiffs_time=0;
		ini.mqtt_enabled=false;
		strcpy_P(ini.mqtt_server, def_mqtt_server);
		strcpy_P(ini.mqtt_login, def_mqtt_login);
		strcpy_P(ini.mqtt_password, def_mqtt_password);
		ini.mqtt_port=def_mqtt_port;
		ini.mqtt_ping_interval=60;
		strcpy_P(ini.mqtt_topic_state, def_mqtt_topic_state);
		strcpy_P(ini.mqtt_topic_command, def_mqtt_topic_command);
		strcpy_P(ini.mqtt_topic_alive, def_mqtt_topic_alive);
		strcpy_P(ini.mqtt_topic_aux, def_mqtt_topic_aux);
		strcpy_P(ini.mqtt_topic_info, def_mqtt_topic_info);
		ini.mqtt_state_type=0;
		ini.switch_ignore_steps=DEFAULT_SWITCH_IGNORE_STEPS;
		ini.up_safe_limit=DEFAULT_UP_SAFE_LIMIT;
		ini.led_mode=0;
		ini.led_level=0;
		IP4_ADDR(&ini.ip, 0, 0, 0, 0);
		IP4_ADDR(&ini.mask, 255, 255, 255, 0);
		IP4_ADDR(&ini.gw, 0, 0, 0, 0);
		IP4_ADDR(&ini.dns, 8, 8, 8, 8);
		Serial.println(F("Settings set to default"));
		elog.Add(EI_Settings_Not_Loaded, EL_ERROR, 0);
	}
	ValidateSettings();
	led_mode=ini.led_mode;
	led_level=ini.led_level;
}

void print_SPIFFS_info()
{
	FSInfo fs_info;
	if (SPIFFS.info(fs_info))
	{
		Serial.printf_P(PSTR("SPIFFS totalBytes: %i\n"), fs_info.totalBytes);
		Serial.printf_P(PSTR("SPIFFS usedBytes: %i\n"), fs_info.usedBytes);
		Serial.printf_P(PSTR("SPIFFS blockSize: %i\n"), fs_info.blockSize);
		Serial.printf_P(PSTR("SPIFFS pageSize: %i\n"), fs_info.pageSize);
		Serial.printf_P(PSTR("SPIFFS maxOpenFiles: %i\n"), fs_info.maxOpenFiles);
		Serial.printf_P(PSTR("SPIFFS maxPathLength: %i\n"), fs_info.maxPathLength);
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {
				Serial.print(dir.fileName());
				Serial.print(" ");
				File f = dir.openFile("r");
				Serial.println(f.size());
				f.close();
		}
	} else
		Serial.println(F("SPIFFS.info() failed"));
}

void setup_SPIFFS()
{
	if (SPIFFS.begin())
	{
		Serial.println(F("SPIFFS Active"));
		// print_SPIFFS_info();
	} else {
		Serial.println(F("Unable to activate SPIFFS"));
	}
}

#if ARDUINO_OTA
void setup_OTA()
{
	ArduinoOTA.onStart([]() {
		StopTimer();
		Serial.println(F("Updating"));
	});
	ArduinoOTA.onEnd([]() {
		Serial.println(F("\nEnd"));
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.print(".");
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf_P(PSTR("Error[%u]: "), error);
		if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
		else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
		else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
		else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
		else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
	});
	ArduinoOTA.setHostname(ini.hostname);
	ArduinoOTA.begin();
}

void Process_OTA()
{
	ArduinoOTA.handle();
}

#else
void setup_OTA() {}
void Process_OTA() {}
#endif

void HTTP_handleRoot(void);
void HTTP_handleOpen(void);
void HTTP_handleClose(void);
void HTTP_handleStop(void);
void HTTP_handleTest(void);
void HTTP_handleSettings(void);
void HTTP_handleAlarms(void);
void HTTP_handleReboot(void);
void HTTP_handleFormat(void);
void HTTP_handleXML(void);
void HTTP_handleSet(void);
void HTTP_handleUpdate(void);
void HTTP_handleLog(void);
void HTTP_redirect(String link);

void setup()
{
	rst_info *resetInfo;

	pinMode(PIN_LED, OUTPUT);
	LED_On();

	if (WiFi.getMode() != WIFI_OFF)
	{
		WiFi.persistent(true);
		WiFi.mode(WIFI_OFF);
	}
	WiFi.persistent(false);

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Booting..."));

	resetInfo = ESP.getResetInfoPtr();
	elog.Add(EI_Started, EL_INFO, resetInfo->reason);

	setup_SPIFFS();
	setup_Settings(); // setup and load settings.
	init_SPIFFS(); // create static HTML files, if needed

	Serial.print(F("Hostname: "));
	Serial.println(ini.hostname);

	lastUARTping = millis();

	LED_Off();
	if (!SLAVE) WiFi_On(); else { WiFi.setSleepMode(WIFI_MODEM_SLEEP); WiFi.forceSleepBegin(); }

	httpUpdater.setup(&httpServer, update_path, update_username, update_password);
	httpServer.on("/",         HTTP_handleRoot);
	httpServer.on("/open",     HTTP_handleOpen);
	httpServer.on("/close",    HTTP_handleClose);
	httpServer.on("/stop",     HTTP_handleStop);
	httpServer.on("/test",     HTTP_handleTest);
	httpServer.on("/settings", HTTP_handleSettings);
	httpServer.on("/alarms",   HTTP_handleAlarms);
	httpServer.on("/reboot",   HTTP_handleReboot);
	httpServer.on("/format",   HTTP_handleFormat);
	httpServer.on("/xml",      HTTP_handleXML);
	httpServer.on("/set",      HTTP_handleSet);
	httpServer.on("/update",   HTTP_handleUpdate);
	httpServer.on("/log",      HTTP_handleLog);
#if RF
	httpServer.on("/rf",       RF_handleHTTP);
#endif
	httpServer.serveStatic(FAV_FILE, SPIFFS, FAV_FILE, "max-age=86400");
	httpServer.serveStatic(CLASS_URL, SPIFFS, CLASS_FILE, "max-age=86400");
	httpServer.serveStatic(JS_URL, SPIFFS, JS_FILE, "max-age=86400");
	httpServer.serveStatic(INI_FILE, SPIFFS, INI_FILE);
	httpServer.onNotFound([]() {
		String message = F("Not found URI: ");
		message += httpServer.uri();

		HTTP_redirect("/settings");
		Serial.println(message);
	});
	httpServer.begin();

	pinMode(PIN_SWITCH, INPUT_PULLUP);
	pinMode(PIN_A, OUTPUT);
	pinMode(PIN_B, OUTPUT);
	pinMode(PIN_C, OUTPUT);
	pinMode(PIN_D, OUTPUT);
	pinMode(PIN_EN, OUTPUT);
	pinMode(PIN_ST, OUTPUT);
	pinMode(PIN_DR, OUTPUT);
	MotorOff();

	setup_OTA();
	setup_NTP();
	setup_MQTT();

	// start position is twice as fully open. So on first close we will go up till home position
	position=ini.full_length*2+ini.up_safe_limit;
	if (IsSwitchPressed()) position=0; // Fully open, if switch is pressed
	roll_to=position;

	FillStepsTable();

	SetupTimer();
	setup_Button();

	voltage_available = GetVoltage() > 3300; // Voltage data available if reading is above 3.3V
}

// ============================= HTTP ===================================
String HTML_mainmenu();
String HTML_status();

String HTML_header()
{
	String ret;
	String name;

	ret.reserve(1024);

	name=FL(F("Lazy rolls"), F("Ленивые шторы"));
	if (ini.name[0]) name=ini.name;
	ret = F("<!doctype html>\n" \
	"<html>\n" \
	"<head>\n" \
	"<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\">" \
	"<meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">\n" \
	"<meta name = \"viewport\" content = \"width=device-width, initial-scale=1\">\n" \
	"<title>");
	ret += name;
	ret += F("</title>\n" \
	"<link rel=\"stylesheet\" href=\"" CLASS_URL "\" type=\"text/css\">\n" \
	"<script src=\"" JS_URL "\"></script>\n" \
	"</head>\n" \
	"<body onload=\"{ active=true; GetStatus(); PinChange(); };\">\n" \
	"<div id=\"wrapper\">\n" \
	"<header onclick=\"ShowMain();\">");
	ret += name;
	ret += F("</header><!--free mem: ");
	ret += ESP.getFreeHeap();
	ret += F("-->\n");

	ret += HTML_mainmenu();
	ret += HTML_status();
	return ret;
}

String HTML_footer()
{
	String ret = F("</div>\n" \
	"<footer></footer>\n" \
	"</body>\n" \
	"</html>");
	return ret;
}

String HTML_tableLine(const char *name, String val, const char *id=NULL)
{
	String ret=F("<tr><td>");
	ret += name;
	if (id==NULL)
		ret += F("</td><td>");
	else
	{
		ret += F("</td><td id=\"");
		ret += String(id);
		ret += F("\">");
	}
	ret += val;
	ret += F("</td></tr>\n");
	return ret;
}

String HTML_addCheckbox(const char* text, const char* id, bool checked)
{
	char buf[255];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td colspan=\"2\"><label for=\"%s\">\n<input type=\"checkbox\" id=\"%s\" name=\"%s\"%s/>\n%s</label></td></tr>\n"),
		id, id, id, (checked ? " checked" : ""), text);
	return buf;
}

String HTML_editString(const __FlashStringHelper *header, const __FlashStringHelper *id, const char *inistr, int len)
{
	String out;

	out=F("<tr><td class=\"idname\">");
	out += header;
	out += F("</td><td class=\"val\"><input type=\"text\" name=\"");
	out += id;
	out += F("\" id=\"");
	out += id;
	out += F("\" value=\"");
	out += inistr;
	out += F("\" maxlength=\"");
	out += len;
	out += F("\"/></td></tr>\n");

	return out;
}

String HTML_editIP(const __FlashStringHelper* header, const __FlashStringHelper* id, ip4_addr* inifield)
{
	String out;

	out=F("<tr><td class=\"idip\">");
	out += header;
	out += F("</td><td class=\"val_ip\">");
	for (int i=0; i<4; i++)
	{
		out += F("<input type=\"text\" name=\"");
		out += id;
		out += i+1;
		out += F("\" value=\"");
		out += String(ip4_addr_get_byte(inifield, i));
		//out += F("\" maxlength=3\"");
		out += F("\"/>");
		if (i<3) out += " . ";
	}
	out += F("</td></tr>\n");

	return out;
}

String HTML_addOption(int value, int selected, const __FlashStringHelper *text, const char *id = NULL)
{
	String s;
	s = F("<option value=\"");
	s += String(value);
	s += F("\"");
	s += (selected==value ? F(" selected=\"selected\"") : F(""));
	if (id) { s += F(" id=\""); s += id; s += F("\""); };
	s += F(">");
	s += text;
	s += F("</option>\n");
	return s;
}

String HTML_section(const __FlashStringHelper* section)
{
	String out;
	out = F("<tr class=\"sect_name\"><td colspan=\"2\">");
	out += section;
	out += F("</td></tr>\n");
	return out;
}

String MemSize2Str(uint32_t mem)
{
	if (mem%(1024*1024) == 0) return String(mem/1024/1024)+ FLF(" MB", " МБ");
	if (mem%1024 == 0) return String(mem/1024)+FLF(" KB"," КБ");
	return String(mem)+FLF(" B", " Б");
}

String HTML_status()
{
	uint32_t realSize = ESP.getFlashChipRealSize();
	uint32_t ideSize = ESP.getFlashChipSize();
	FlashMode_t ideMode = ESP.getFlashChipMode();
	String out;

	out.reserve(4096);

	out += F("    <section class=\"info hide\" id=\"info\"><table>\n");
	out += HTML_section(FLF("Status", "Статус"));
	out += HTML_tableLine(L("Version", "Версия"), VERSION);
	out += HTML_tableLine(L("IP", "IP"), WiFi.localIP().toString());
	if (lastSync==0)
		out += HTML_tableLine(L("Time", "Время"), SL("unknown", "хз"), "time");
	else
		out += HTML_tableLine(L("Time", "Время"), TimeStr() + " ["+ DoWName(DayOfWeek(getTime())) +"]", "time");

	out += HTML_tableLine(L("Uptime", "Аптайм"), UptimeStr(), "uptime");
	out += HTML_tableLine(L("RSSI", "RSSI"), String(WiFi.RSSI())+SL(" dBm", " дБм"), "RSSI");
	if (voltage_available)
		out += HTML_tableLine(L("Power", "Питание"), GetVoltageStr()+SL("V", "В"), "voltage");
	out += HTML_tableLine(L("<a href=\"/log\" onclick=\"return ShowLog();\">Log</a>", 
		"<a href=\"/log\" onclick=\"return ShowLog();\">Лог</a>"), String((int)elog.Count()), "log_count");

	out += HTML_section(FLF("Position", "Положение"));
	out += HTML_tableLine(L("Now", "Сейчас"), String(position), "pos");
	out += HTML_tableLine(L("Roll to", "Цель"), String(roll_to), "dest");
	out += HTML_tableLine(L("Switch", "Концевик"), onoff[ini.lang][IsSwitchPressed()], "switch");

	out += HTML_section(FLF("Memory", "Память"));
	out += HTML_tableLine(L("Flash id", "ID чипа"), String(ESP.getFlashChipId(), HEX));
	out += HTML_tableLine(L("Real size", "Реально"), MemSize2Str(realSize));
	out += HTML_tableLine(L("IDE size", "Прошивка"), MemSize2Str(ideSize));
	if(ideSize != realSize) {
		out += HTML_tableLine(L("Config", "Конфиг"), L("error!", "ошибка!"));
	} else {
		out += HTML_tableLine(L("Config", "Конфиг"), "OK!");
	}
	out += HTML_tableLine(L("Speed", "Частота"), String(ESP.getFlashChipSpeed()/1000000)+SL("MHz", "МГц"));
	out += HTML_tableLine(L("Mode", "Режим"), (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
	//out += HTML_tableLine("Host", String(ini.hostname));
	FSInfo fs_info;
	out += HTML_section(FLF("SPIFFS", "SPIFFS"));
	if (SPIFFS.info(fs_info))
	{
		out += HTML_tableLine(L("Size", "Выделено"), MemSize2Str(fs_info.totalBytes));
		out += HTML_tableLine(L("Used", "Занято"), MemSize2Str(fs_info.usedBytes));
	} else
		out += HTML_tableLine(L("Error", "Ошибка"), "<a href=\"/format\">"+SL("Format", "Формат-ть")+"</a>");
#if MQTT
	out += HTML_section(FLF("MQTT", "MQTT"));
	out += HTML_tableLine(L("MQTT", "MQTT"), MQTTstatus(), "mqtt");
#endif
	out += HTML_section(FLF("LED", "LED"));
	out += HTML_tableLine(L("Mode", "Функция"), LEDModeString(), "led_mode");
	out += HTML_tableLine(L("Brightness", "Яркость"), LEDLevelString(), "led_level");

	if (SLAVE)
	{
		out += HTML_section(FLF("Slave", "Ведомый"));
		out += HTML_tableLine(L("Errors", "Ошибок"), String(uart_crc_errors), "uart_crc_errors");
	}

	out += HTML_section(FLF("Links", "Ссылки"));
	out += HTML_tableLine("<a href=\"https://github.com/ACE1046/LazyRolls\">[Github]</a>", "<a href=\"https://t.me/lazyrolls\">[Telegram]</a>");
	out += HTML_tableLine("<a href=\"mailto:ace@imlazy.ru\">[E-mail]</a>", "<a href=\"http://imlazy.ru\">[Website]</a>");

	out += F("</table></section>\n");

	return out;
}

String HTML_mainmenu(void)
{
	String out;

	out.reserve(1024);
	out += F("<div id=\"heading\" class=\"status\">\n" \
		"<ul><li class=\"menuopen\"><a href=\"open\" onclick=\"return Open();\"><div class=\"svg\"></div>[");
	out += FL(F("Open"), F("Открыть"));
	out += F("]</a>\n" \
		"</li><li class=\"menuclose\"><a href=\"close\" onclick=\"return Close();\"><div class=\"svg\"></div>[");
	out += FL(F("Close"), F("Закрыть"));
	out += F("]</a>\n" \
		"</li><li class=\"menustop\"><a href=\"stop\" onclick=\"return Stop();\"><div class=\"svg\"></div>[");
	out += FL(F("Stop"), F("Стоп"));
	out += F("]</a>\n" \
		"</li><li class=\"menuinfo\"><a href=\"/\" onclick=\"return ShowInfo();\"><div class=\"svg\"></div>[");
	out += FL(F("Info"), F("Инфо"));
	out += F("]</a>\n" \
		"</li><li class=\"menusettings\"><a href=\"/settings\" onclick=\"return ShowSettings();\"><div class=\"svg\"></div>[");
	out += FL(F("Settings"), F("Настройки"));
	out += F("]</a>\n" \
		"</li><li class=\"menualarms\"><a href=\"/alarms\" onclick=\"return ShowAlarms();\"><div class=\"svg\"></div>[");
	out += FL(F("Schedule"), F("Расписание"));
	out += F("]</a></li></ul>\n</div>\n");
	return out;
}

String HTML_save(int span=2)
{
	String s;
	s=F("<tr class=\"sect_name\"><td colspan=\"");
	s += String(span);
	s += F("\"><input id=\"save\" type=\"submit\" name=\"save\" value=\"");
	s += FL(F("Save"), F("Сохранить"));
	s += F("\"></td></tr>\n");
	return s;
}

void HTTP_Activity()
{
	if (led_mode == LED_HTTP || led_mode == LED_MQTT_HTTP) LED_Blink();
	NetworkActivity();
	rf_page_open = false;
}

void HTTP_handleRoot(void)
{
	String out;

	HTTP_Activity();

	out = HTML_header();

	out += F("<section class=\"main\" id=\"main\"><p>" \
		"<ul>\n" \
		"<li class=\"menuopen\"><a href=\"open\" onclick=\"return Open();\"><div class=\"svg\"></div>[Open]</a>\n" \
		"</li><li class=\"menustop\"><a href=\"stop\" onclick=\"return Stop();\"><div class=\"svg\"></div>[Stop]</a>\n" \
		"</li><li class=\"menuclose\"><a href=\"close\" onclick=\"return Close();\"><div class=\"svg\"></div>[Close]</a>\n" \
		"</li></p>");

	out += FL(F("<p>Reminder. After reboot both commands open and close will open cover first to find zero point (at endstop).</p>"),
		F("<p>Напоминание. После перезагрузки, по любой команде (открыть или закрыть) штора вначале едет вверх, до концевика, " \
			"чтобы найти нулевую точку. Это нормально.</p>"));
	out += F("</section>\n");

	out += HTML_footer();
	httpServer.send(200, "text/html", out);
}

void SaveString(const __FlashStringHelper *id, char *inistr, int len)
{
	String s;
	if (!httpServer.hasArg(id)) return;

	s=httpServer.arg(id);
	s.trim(); // remove leading and trailing whitespace
	strncpy(inistr, s.c_str(), len-1);
	inistr[len-1]='\0';
}

void SaveInt(const __FlashStringHelper *id, uint8_t *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=atoi(httpServer.arg(id).c_str());
}
void SaveInt(const char *id, uint16_t *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=min(65535, atoi(httpServer.arg(id).c_str()));
}
void SaveInt(const __FlashStringHelper *id, uint16_t *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=min(65535, atoi(httpServer.arg(id).c_str()));
}
void SaveInt(const __FlashStringHelper *id, uint32_t *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=atoi(httpServer.arg(id).c_str());
}
void SaveInt(const __FlashStringHelper *id, int *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=atoi(httpServer.arg(id).c_str());
}
void SaveInt(const __FlashStringHelper *id, bool *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint=atoi(httpServer.arg(id).c_str());
}

void SaveIP(const __FlashStringHelper *id1, const __FlashStringHelper *id2, const __FlashStringHelper *id3, const __FlashStringHelper *id4, ip4_addr *iniip)
{
	if (!httpServer.hasArg(id1)) return;
	if (!httpServer.hasArg(id2)) return;
	if (!httpServer.hasArg(id3)) return;
	if (!httpServer.hasArg(id4)) return;
	IP4_ADDR(iniip, 
		atoi(httpServer.arg(id1).c_str()),
		atoi(httpServer.arg(id2).c_str()),
		atoi(httpServer.arg(id3).c_str()),
		atoi(httpServer.arg(id4).c_str())
	);
}

void HTTP_handleUpdate(void)
{
	String out;
	int mem = ESP.getFlashChipRealSize();

	HTTP_Activity();

	out = HTML_header();
	out += F("<section class=\"main\" id=\"main\"><p>" \
	 "<form method='POST' action='/update2' enctype='multipart/form-data'>");
	out += FL(F("Firmware:"), F("Прошивка:"));
	out += F("<br><input type='file' accept='.bin,.bin.gz' name='firmware'>" \
		"<input type='submit' value='");
	out += FL(F("Update Firmware"), F("Обновить прошивку"));
	out += F("'></form></p><p>");
	out += FL(F("Choose file for firmware update.<br/>New firmware can be downloaded from "), 
		F("Выберите файл прошивки (Choose File) для обновления.<br/>Новые прошивки можно скачать тут: "));
	out += F("<a href=\"https://github.com/ACE1046/LazyRolls/tree/master/Firmware\">Github</a>.<br/>");
	if (mem == 1024*1024)
		out += FL(F("Choose *.1Mbyte.bin.<br/>"), F("Выбирайте *.1Mbyte.bin.<br/>"));
	if (mem == 4*1024*1024)
		out += FL(F("Choose *.4Mbyte.bin.<br/>"), F("Выбирайте *.4Mbyte.bin.<br/>"));
	out += FL(F("Settings will be lost, if downgrading to previous version. Default password admin admin."), 
		F("Настройки сбрасываются, если прошивается более старая версия. Пароль по умолчанию admin admin"));

	out += F("</section>\n");

	out += HTML_footer();
	httpServer.send(200, "text/html", out);
}

String HTML_steps(String lbl, String id, int val, String name)
{
	String out;
	out += F("<tr><td class=\"idname\">");
	out += lbl;
	out += F("</td><td class=\"val_p\"><input type=\"text\" name=\"");
	out += name;
	out += F("\" id=\"");
	out += id;
	out += F("\" value=\"");
	out += String(val);
	out += F("\" maxlength=\"6\"/>\n<input type=\"button\" value=\"");
	out += FLF("Test", "Тест");
	out += F("\" onclick=\"TestPreset('");
	out += name;
	out += F("')\">\n<input type=\"button\" value=\"");
	out += FLF("Here", "Тут");
	out += F("\" onclick=\"SetPreset('");
	out += name;
	out += F("')\">\n</td></tr>\n");
	return out;
}

String HTML_hint(const __FlashStringHelper* hint)
{
	String s;
	s = F("<tr><td></td><td>");
	s += hint;
	s += F("</td></tr>\n");
	return s;
}

String HTML_hint(const String &hint)
{
	String s;
	s = F("<tr><td></td><td>");
	s += hint;
	s += F("</td></tr>\n");
	return s;
}

void HTTP_saveSettings()
{
	char pass[max(sizeof(ini.password), sizeof(ini.mqtt_password))];

	pass[0]='*';
	pass[1]='\0';
	SaveString(F("hostname"), ini.hostname, sizeof(ini.hostname));
	SaveString(F("name"),     ini.name,     sizeof(ini.name));
	SaveString(F("ssid"),     ini.ssid,     sizeof(ini.ssid));
	SaveString(F("password"), pass,         sizeof(ini.password));
	SaveString(F("ntp"),      ini.ntpserver,sizeof(ini.ntpserver));
	if (strcmp(pass, "*")!=0) memcpy(ini.password, pass, sizeof(ini.password));

#define IP_ID(a) F(a "1"), F(a "2"), F(a "3"), F(a "4")
	SaveIP(IP_ID("ip"),   &ini.ip);
	SaveIP(IP_ID("mask"), &ini.mask);
	SaveIP(IP_ID("gw"),   &ini.gw);
	SaveIP(IP_ID("dns"),  &ini.dns);

	SaveInt(F("lang"), &ini.lang);
	SaveInt(F("pinout"), &ini.pinout);
	SaveInt(F("reversed"), &ini.reversed);
	SaveInt(F("delay"), &ini.step_delay_mks);
	SaveInt(F("timezone"), &ini.timezone);
	SaveInt(F("length"), &ini.full_length);
	SaveInt(F("switch"), &ini.switch_reversed);
	SaveInt(F("sw_at_bottom"), &ini.sw_at_bottom);
	SaveInt(F("switch_ignore"), &ini.switch_ignore_steps);
	SaveInt(F("btn_pin"), &ini.btn_pin);
	SaveInt(F("aux_pin"), &ini.aux_pin);
	SaveInt(F("rf_pin"), &ini.rf_pin);
	SaveInt(F("up_safe_limit"), &ini.up_safe_limit);
	for (int i=0; i<MAX_PRESETS; i++)
		SaveInt(String("preset"+String(i)).c_str(), &ini.preset[i]);

	pass[0]='*';
	pass[1]='\0';
	ini.mqtt_enabled=httpServer.hasArg("mqtt_enabled");
	SaveString(F("mqtt_server"), ini.mqtt_server, sizeof(ini.mqtt_server));
	SaveInt(F("mqtt_port"), &ini.mqtt_port);
	SaveString(F("mqtt_login"), ini.mqtt_login, sizeof(ini.mqtt_login));
	SaveString(F("mqtt_password"), pass, sizeof(ini.mqtt_password));
	SaveString(F("ntp"), ini.ntpserver,sizeof(ini.ntpserver));
	if (strcmp(pass, "*")!=0) memcpy(ini.mqtt_password, pass, sizeof(ini.mqtt_password));
	SaveInt(F("mqtt_ping_interval"), &ini.mqtt_ping_interval);
	SaveString(F("mqtt_topic_state"), ini.mqtt_topic_state, sizeof(ini.mqtt_topic_state));
	SaveString(F("mqtt_topic_command"), ini.mqtt_topic_command, sizeof(ini.mqtt_topic_command));
	SaveString(F("mqtt_topic_alive"), ini.mqtt_topic_alive, sizeof(ini.mqtt_topic_alive));
	SaveString(F("mqtt_topic_aux"), ini.mqtt_topic_aux, sizeof(ini.mqtt_topic_aux));
	SaveString(F("mqtt_topic_info"), ini.mqtt_topic_info, sizeof(ini.mqtt_topic_info));
	SaveInt(F("mqtt_state_type"), &ini.mqtt_state_type);
	ini.mqtt_invert=httpServer.hasArg("mqtt_invert");
	ini.mqtt_discovery=httpServer.hasArg("mqtt_discovery");
	SaveInt(F("led_mode"), &ini.led_mode);
	SaveInt(F("led_level"), &ini.led_level);
	SaveInt(F("slave"), &ini.slave);

	led_mode=ini.led_mode;
	led_level=ini.led_level;

	ValidateSettings();

	SaveSettings(&ini, sizeof(ini));
	elog.Add(EI_Settings_Saved, EL_WARN, 0);

	setup_MQTT();
	setup_Button();
	
	FillStepsTable();
	AdjustTimerInterval();

	if(WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP)
	{ // in soft AP mode, trying to connect to network
		Serial.println(F("Trying to reconnect"));
		elog.Add(EI_Wifi_Reconnect, EL_INFO, 0);
		WiFi.begin(ini.ssid, ini.password);
		if (WiFi.waitForConnectResult() == WL_CONNECTED)
		{
			Serial.println(F("Reconnected to network in STA mode. Closing AP"));
			elog.Add(EI_Wifi_Close_AP, EL_INFO, 0);
			HTTP_redirect("http://"+WiFi.localIP().toString()+"/settings");
			delay(3000);
			WiFi.softAPdisconnect(true);
			WiFi.mode(WIFI_STA);
			LED_Off();
			CP_delete();
		}
	}

	WiFi.hostname(ini.hostname);
}

String AddOptions(const __FlashStringHelper *id, const __FlashStringHelper *var, const __FlashStringHelper *arr, int val)
{
	String out;
	out = F("<script>const ");
	out += var;
	out += F("=[");
	out += arr;
	out += F("];AddOption('");
	out += id;
	out += F("', ");
	out += var;
	out += F(", ");
	out += val;
	out += F(");</script>\n");
	return out;
}

String AddOptions(const __FlashStringHelper *id, const __FlashStringHelper *var, int val)
{
	String out;
	out = F("<script>AddOption('");
	out += id;
	out += F("', ");
	out += var;
	out += F(", ");
	out += val;
	out += F(");</script>\n");
	return out;
}

void HTTP_handleSettings(void)
{
	String out;

	HTTP_Activity();

	if (httpServer.hasArg("save"))
	{
		HTTP_saveSettings();
		HTTP_redirect("/settings?ok=1");
		return;
	}

	out = HTML_header();

	out.reserve(16384);

	out += F("<section class=\"settings\" id=\"settings\">\n");

	if (httpServer.hasArg("ok"))
		out += FLF("<p>Saved!<br/>Network settings will be applied after reboot.<br/><a href=\"reboot\">[Reboot]</a></p>\n",
			"<p>Сохранено!<br/>Настройки сети будут применены после перезагрузки.<br/><a href=\"reboot\">[Перезагрузить]</a></p>\n");

	out += F("<form method=\"post\" action=\"/settings\">\n");

	out += F("<table>\n");
	out += HTML_save();
	out += "<tr><td>";
	out += FLF("Language: ", "Язык: ");
	out += F("</td><td><select id=\"lang\" name=\"lang\">\n");
	for (int i=0; i < Languages; i++)
	{
		out += HTML_addOption(i, ini.lang, Language[i]);
	}
	out += F("</select></td></tr>\n");
	out += HTML_editString(FLF("Name:", "Название:"),       F("name"),     ini.name,     sizeof(ini.name)-1);
	out += HTML_section(FLF("Network", "Сеть"));
	out += HTML_editString(FLF("Hostname:", "Имя в сети:"), F("hostname"), ini.hostname, sizeof(ini.hostname)-1);
	out += HTML_editString(FLF("SSID:", "Wi-Fi сеть:"),     F("ssid"),     ini.ssid,     sizeof(ini.ssid)-1);
	out += HTML_editString(FLF("Password:", "Пароль:"),     F("password"), "*",          sizeof(ini.password)-1);
	out += HTML_hint(FLF("Leave zeros for DHCP", "Оставить нули для DHCP"));
	out += HTML_editIP(F("IP"), F("ip"), &ini.ip);
	out += HTML_editIP(FLF("Mask", "Маска"), F("mask"), &ini.mask);
	out += HTML_editIP(FLF("Gateway", "Шлюз"), F("gw"), &ini.gw);
	out += HTML_editIP(F("DNS"), F("dns"), &ini.dns);

	out += HTML_section(FLF("Time", "Время"));
	out += HTML_editString(FLF("NTP-server:", "NTP-сервер:"),F("ntp"),     ini.ntpserver,sizeof(ini.ntpserver)-1);
	out += F("<tr><td>");
	out += FLF("Timezone: ", "Пояс: ");
	out += F("</td><td><select id=\"timezone\" name=\"timezone\">\n");
	// for (int i=-11*60; i<=14*60; i+=30) // timezones from -11:00 to +14:00 every 30 min
	// {
	// 	char b[7];
	// 	sprintf_P(b, PSTR("%+d:%02d"), i/60, abs(i%60));
	// 	if (i<0) b[0]='-';
	// 	out += "<option value=\""+String(i)+"\"";
	// 	if (i==ini.timezone) out += " selected=\"selected\"";
	// 	out += +">UTC"+String(b)+"</option>\n";
	// }
	out += F("</select></td></tr>\n");
	out += F("<script>AddOption('timezone', tzs, ");
	out += ini.timezone;
	out += F(");</script>\n");

	out += HTML_section(FLF("Motor", "Мотор"));
	out += F("<tr><td>");
	out += FLF("Pinout:", "Подключение:");
	out += F("</td><td><select id=\"pinout\" name=\"pinout\">\n");
	// out += HTML_addOption(2, ini.pinout, F("A-B-C-D"));
	// out += HTML_addOption(0, ini.pinout, F("A-C-B-D"));
	// out += HTML_addOption(1, ini.pinout, F("A-B-D-C"));
	// out += HTML_addOption(3, ini.pinout, F("Step/Dir"));
	out += F("</select></td></tr>\n");
	out += AddOptions(F("pinout"), F("po"), F("2,\"A-B-C-D\",0,\"A-C-B-D\",1,\"A-B-D-C\",3,\"Step/Dir\""), ini.pinout);
	out += F("<tr><td>");
	out += FLF("Direction:", "Направление:");
	out += F("</td><td><select id=\"reversed\" name=\"reversed\">\n");
	out += HTML_addOption(1, ini.reversed, FLF("Normal", "Прямое"));
	out += HTML_addOption(0, ini.reversed, FLF("Reversed", "Обратное"));
	out += F("</select></td></tr>\n");
	out += HTML_editString(FLF("Step delay:", "Время шага:"), F("delay"), String(ini.step_delay_mks).c_str(), 5);
	out += HTML_hint(FLF("(microsecs, " TOSTRING(MIN_STEP_DELAY) "-65000, default 1500)", "(в мкс, " TOSTRING(MIN_STEP_DELAY) "-65000, обычно 1500)"));
	out += F("<tr><td colspan=\"2\">\n" \
	"<input id=\"btn_up\" type=\"button\" name=\"up\" value=\"");
	out += FLF("Test up", "Тест вверх");
	out += F("\" onclick=\"TestUp()\">\n" \
	"<input id=\"btn_dn\" type=\"button\" name=\"down\" value=\"");
	out += FLF("Test down", "Тест вниз");
	out += F("\" onclick=\"TestDown()\">\n" \
	"</td></tr>\n");

	out += HTML_section(FLF("Curtain", "Штора"));
	out += HTML_steps(SL(F("Length:"), F("Длина:")), "length", ini.full_length, "length");
	out += HTML_hint(FLF("(closed position, steps)", "(шагов до полного закрытия)"));
	for (int i=0; i<MAX_PRESETS; i++)
	{
		out += HTML_steps(SL("Preset", "Позиция")+" "+String(i+1)+":", "preset"+String(i), ini.preset[i], "preset"+String(i));
	}

	out += HTML_section(FLF("Endstop", "Концевик"));
	out += F("<tr><td>");
	out += FLF("Type:", "Тип:");
	out += F("</td><td><select id=\"switch\" name=\"switch\">\n");
	out += HTML_addOption(0, ini.switch_reversed, FLF("Normal closed", "Нормально замкнут"));
	out += HTML_addOption(1, ini.switch_reversed, FLF("Normal open", "Нормально разомкнут"));
	out += F("</select></td></tr>\n");
	out += F("<tr><td>");
	out += FLF("Position:", "Положение:");
	out += F("</td><td><select id=\"sw_at_bottom\" name=\"sw_at_bottom\">\n");
	out += HTML_addOption(0, ini.sw_at_bottom, FLF("At fully open", "На открыто"));
	out += HTML_addOption(1, ini.sw_at_bottom, FLF("At fully closed", "На закрыто"));
	out += F("</select></td></tr>\n");
	out += HTML_editString(FLF("Length:", "Длина:"), F("switch_ignore"), String(ini.switch_ignore_steps).c_str(), 5);
	out += HTML_hint(FLF("(switch ignore zone, steps, default 100)", "(игнорировать концевик первые шаги, обычно 100)"));
	out += HTML_editString(FLF("Extra:", "Запас:"), F("up_safe_limit"), String(ini.up_safe_limit).c_str(), 5);
	out += HTML_hint(FLF("(Maximum steps below zero on open, default 300. Do not change if not sure)", 
		"(Шагов в минус при открытии, до срабатывания концевика, обычно 300. Не менять, если не уверены.)"));

#if MQTT
	out += HTML_section(FLF("MQTT", "MQTT"));
	String s=FLF("MQTT enabled Help:", "MQTT включен Помощь:");
	s += F(" <a href=\"http://imlazy.ru/rolls/mqtt.html\">imlazy.ru/rolls/mqtt.html</a>");
	out += HTML_addCheckbox(s.c_str(), "mqtt_enabled", ini.mqtt_enabled);
	out += HTML_editString(FLF("Server:", "Сервер:"), F("mqtt_server"), ini.mqtt_server, sizeof(ini.mqtt_server)-1);
	out += HTML_editString(FLF("Port:", "Порт:"), F("mqtt_port"), String(ini.mqtt_port).c_str(), 5);
	out += HTML_editString(FLF("Login:", "Логин:"), F("mqtt_login"), ini.mqtt_login, sizeof(ini.mqtt_login)-1);
	out += HTML_editString(FLF("Password:", "Пароль:"), F("mqtt_password"), "*", sizeof(ini.mqtt_password)-1);
	out += HTML_editString(FLF("Keep-alive:", "Keep-alive:"), F("mqtt_ping_interval"), String(ini.mqtt_ping_interval).c_str(), 5);
	out += HTML_editString(FLF("Commands:", "Команды:"), F("mqtt_topic_command"), ini.mqtt_topic_command, sizeof(ini.mqtt_topic_command)-1);
	out += HTML_hint(FLF("Allowed commands: on/open/off/close/stop, 0 - 100 (percents), =123 (steps), @1 (preset)", 
		"Допустимые команды: on/open/off/close/stop, 0 - 100 (проценты), =123 (шаги), @1 (пресет)"));
	out += HTML_editString(FLF("State:", "Статус:"), F("mqtt_topic_state"), ini.mqtt_topic_state, sizeof(ini.mqtt_topic_state)-1);
	out += F("<tr><td>");
	out += FLF("Type:", "Формат:");
	out += F("</td><td><select id=\"mqtt_state_type\" name=\"mqtt_state_type\">\n");
	out += HTML_addOption(0, ini.mqtt_state_type, F("0-100 (%)"));
	out += HTML_addOption(1, ini.mqtt_state_type, F("ON/OFF"));
	out += HTML_addOption(2, ini.mqtt_state_type, F("0/1"));
	out += HTML_addOption(3, ini.mqtt_state_type, F("JSON"));
	out += F("</select></td></tr>\n");
	out += HTML_editString(FLF("Alive:", "Живой:"), F("mqtt_topic_alive"), ini.mqtt_topic_alive, sizeof(ini.mqtt_topic_alive)-1);
	out += HTML_editString(FLF("Info:", "Инфо:"), F("mqtt_topic_info"), ini.mqtt_topic_info, sizeof(ini.mqtt_topic_info)-1);
	out += HTML_addCheckbox(L("Invert percentage (0% = closed)", "Инвертировать проценты (0% = закрыто)"), "mqtt_invert", ini.mqtt_invert);
	out += HTML_addCheckbox(L("Home Assistant MQTT discovery", "Home Assistant MQTT discovery"), "mqtt_discovery", ini.mqtt_discovery);
#endif

#define PIN_LIST F("0,\"---\",1,\"GPIO0 (D3/DTR)\",2,\"GPIO2 (D4)\",3,\"GPIO3 (RX)\",4,\"GPIO15 (D8)\"")
	out += HTML_section(FLF("Button", "Кнопка"));
	out += F("<tr><td>");
	out += FLF("Pin:", "Пин:");
	out += F("</td><td><select id=\"btn_pin\" name=\"btn_pin\" onchange=\"PinChange()\">\n");
	out += AddOptions(F("btn_pin"), F("pins"), PIN_LIST, ini.btn_pin);
	out += F("</select></td></tr>\n");
	out += HTML_hint(FLF("(Hardware button. Connect to Gnd and selected pin. Click to open/close/stop, " \
		"long click to go to preset 1 (or 2, if already in 1). Double click - change direction in motion.)",
		"(Кнопка. Подключать к Gnd и выбраному пину. Клик - открыть/закрыть/стоп, долгий клик - пресет 1 (или 2, если уже в 1). " \
		"Двойной клик в движении - сменить направление.)"));

#if RF
	out += HTML_section(FLF("RF remote", "Радио пульт"));
	out += F("<tr><td>");
	out += FLF("Pin:", "Пин:");
	out += F("</td><td><select id=\"rf_pin\" name=\"rf_pin\" onchange=\"PinChange()\">\n");
	out += F("</select></td></tr>\n");
	out += AddOptions(F("rf_pin"), F("pins"), /*PIN_LIST,*/ ini.rf_pin);
	out += HTML_hint(FLF("(Radio remote control. <a href=\"/rf\">Settings</a>)",
		"(Радио пульт. <a href=\"/rf\">Настройки</a>)"));
#endif

#if MQTT
	out += HTML_section(FLF("Aux input", "Доп. вход"));
	out += F("<tr><td>");
	out += FLF("Pin:", "Пин:");
	out += F("</td><td><select id=\"aux_pin\" name=\"aux_pin\" onchange=\"PinChange()\">\n");
	out += F("</select></td></tr>\n");
	out += AddOptions(F("aux_pin"), F("pins"), /*PIN_LIST,*/ ini.aux_pin);
	out += HTML_editString(FLF("MQTT topic:", "MQTT топик:"), F("mqtt_topic_aux"), ini.mqtt_topic_aux, sizeof(ini.mqtt_topic_aux)-1);
	out += HTML_hint(FLF("(Auxiliary input. Connect to Gnd and selected pin. Will send \"ON/OFF\" payloads to selected topic on change)",
		"(Доп. вход. Подключать к Gnd и выбраному пину. При изменении будет отправлять \"ON/OFF\" в указанный топик)"));
#endif

	out += HTML_section(FLF("Master/slave", "Главный/ведомый"));
	out += F("<tr><td>");
	out += FLF("Role:", "Роль:");
	out += F("</td><td><select id=\"slave\" name=\"slave\" onchange=\"PinChange()\">\n");
	// out += HTML_addOption(0, ini.slave, FLF("Standalone", "Независимый"));
	// out += HTML_addOption(255, ini.slave, FLF("Master", "Главный"));
	// out += HTML_addOption(1, ini.slave, FLF("Slave 1", "Ведомый 1"));
	// out += HTML_addOption(2, ini.slave, FLF("Slave 2", "Ведомый 2"));
	// out += HTML_addOption(3, ini.slave, FLF("Slave 3", "Ведомый 3"));
	// out += HTML_addOption(4, ini.slave, FLF("Slave 4", "Ведомый 4"));
	// out += HTML_addOption(5, ini.slave, FLF("Slave 5", "Ведомый 5"));
	out += F("</select></td></tr>\n");
	out += AddOptions(F("slave"), F("mss"),
		FLF("0,\"Standalone\",255,\"Master\",1,\"Slave 1\",2,\"Slave 2\",3,\"Slave 3\",4,\"Slave 4\",5,\"Slave 5\"",
			"0,\"Независимый\",255,\"Главный\",1,\"Ведомый 1\",2,\"Ведомый 2\",3,\"Ведомый 3\",4,\"Ведомый 4\",5,\"Ведомый 5\""),
		ini.slave);
	out += HTML_hint(SL(F("Help:"), F("Помощь:")) + " <a href=\"http://imlazy.ru/rolls/master.html\">imlazy.ru/rolls/master.html</a>");

	out += HTML_section(FLF("LED", "Светодиод"));
//	out += "<tr><td colspan=\"2\"><a href=\"http://imlazy.ru/rolls/cmd.html\">imlazy.ru/rolls/cmd.html</a></label></td></tr>\n";
	out += F("<tr><td>");
	out += FLF("Mode:", "Функция:");
	out += F("</td><td><select id=\"led_mode\" name=\"led_mode\">\n");
#define MODE_OPT(x) out += HTML_addOption(x, ini.led_mode, LEDModeString(x));
	MODE_OPT(LED_OFF);
	MODE_OPT(LED_ON);
	MODE_OPT(LED_MQTT);
	MODE_OPT(LED_HTTP);
	MODE_OPT(LED_MQTT_HTTP);
	MODE_OPT(LED_ALIVE);
	MODE_OPT(LED_BUTTON);
	out += F("</select></td></tr>\n<tr><td>");
	out += FLF("Brightness:", "Яркость:");
	out += F("</td><td><select id=\"led_level\" name=\"led_level\">\n");
#define LEVEL_OPT(x) out += HTML_addOption(x, ini.led_level, LEDLevelString(x));
	LEVEL_OPT(LED_LOW);
	LEVEL_OPT(LED_MED);
	LEVEL_OPT(LED_HIGH);
	out += F("</select></td></tr>\n");
	out += HTML_hint(SL(F("Help:"), F("Помощь:")) + " <a href=\"http://imlazy.ru/rolls/led.html\">imlazy.ru/rolls/led.html</a>");

	out += HTML_save();
	out += F("<tr><td colspan=\"2\"><a href=\"/update\">");
	out += FLF("Firmware update", "Обновление прошивки");
	out += F("</a></td></tr>\n</table>\n</form>\n</section>\n");

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
	sprintf_P(buf, PSTR("%02d:%02d"), t/60%24, t%60);
	return String(buf);
}

void HTTP_handleAlarms(void)
{
	String out;

	HTTP_Activity();

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
				if (ini.alarms[a].percent_open>100+MAX_PRESETS) ini.alarms[a].percent_open=100;
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

	out=HTML_header();

	out.reserve(20480);

	out += F("<section class=\"alarms\" id=\"alarms\">\n"
		"<form method=\"post\" action=\"/alarms\">\n"
		"<table width=\"100%\">\n");
	out += HTML_save(3);
	out += F("<tr><td colspan=\"3\">");
	out += FLF("To execute command one time, remove all day of week marks. Command will be disabled after execution.",
		"Для выполнения пункта расписания один раз, в ближайшие сутки, снимите все галочки дней недели. После выполнения пункт отключится.");

	for (int a=0; a<ALARMS; a++)
	{
		String n=String(a);
		out += F("<tr><td colspan=\"3\"><hr/></td></tr>\n"
			"<tr class=\"en\"><td class=\"en\"><label for=\"en");
		out += n;
		out += F("\">\n");
		out += F("<input type=\"checkbox\" id=\"en");
		out += n;
		out += F("\" name=\"en");
		out += n;
		out += F("\"");
		out += ((ini.alarms[a].flags & ALARM_FLAG_ENABLED) ? " checked" : "");
		out += F("/>\n");
		out += FLF("Enable", "Вкл.");
		out += F("</label></td>\n");

		out += F("<td class=\"narrow\"><label for=\"time");
		out += n;
		out += F("\">");
		out += FLF("Time:", "Время:");
		out += F("</label> <br/><input type=\"time\" id=\"time");
		out += n;
		out += F("\" name=\"time");
		out += n;
		out += F("\" value=\"");
		out += TimeToStr(ini.alarms[a].time);
		out += F("\" required></td>\n");

		out += F("<td><label for=\"dest");
		out += n;
		out += F("\"> ");
		out += FLF("Position:", "Положение:");
		out += F("</label> <br/><select id=\"dest");
		out += n;
		out += F("\" name=\"dest");
		out += n;
		out += F("\">\n");
		for (int p=0; p<=100; p+=20)
		{
			String s = String(p)+"%";
			if (p==0)   s=FL(F("Open"), F("Открыть"));
			if (p==100) s=FL(F("Close"), F("Закрыть"));
			out += "<option value=\""+String(p)+"\""+
				(ini.alarms[a].percent_open==p ? " selected" : "")+">"+s+"</option>\n";
		}
		for (int p=0; p<MAX_PRESETS; p++)
		{
			out += "<option value=\""+String(101+p)+"\""+
				(ini.alarms[a].percent_open==101+p ? " selected" : "")+">";
			out += FLF("Preset", "Позиция");
			out += " "+String(p+1)+"</option>\n";
		}
		out += F("</select>\n");
		out += F("</td></tr><tr><td>");

		out += FLF("Repeat:", "Повтор:");
		out += F("</td><td colspan=\"2\" class=\"days\">\n");
		for (int d=0; d<7; d++)
		{
			String id="\"d"+n+"_"+String(d)+"\"";
			out += "<label for="+id+">";
			out += "<input type=\"checkbox\" id="+id+" name="+id+
					((ini.alarms[a].day_of_week & (1 << d)) ? " checked" : "")+">";
			out += DoWName(d);
			out += F("</label> \n");
		}
		out += F("</td></tr>\n");
	}

	out += HTML_save(3);

	out += F("</table>\n" \
		"</form>\n" \
		"</section>\n");
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

void HTTP_handleFormat(void)
{
	HTTP_redirect(String("/"));
	delay(500);
	SPIFFS.end();
	SPIFFS.format();
	setup_SPIFFS();
	SaveSettings(&ini, sizeof(ini));
	Serial.println(F("SPIFFS formatted. rebooting"));
	delay(500);
	ESP.reset();
}

const char * const blank_xml = "<xml></xml>";

void Return200()
{
	httpServer.send(200, "text/XML", blank_xml);
}

void HTTP_handleOpen(void)
{
	HTTP_Activity();
	elog.Add(EI_Cmd_Open, EL_INFO, ES_HTTP);
	if (httpServer.hasArg("addr"))
		Open(atoi(httpServer.arg("addr").c_str()));
	else
		Open();
	Return200();
}

void HTTP_handleClose(void)
{
	HTTP_Activity();
	elog.Add(EI_Cmd_Close, EL_INFO, ES_HTTP);
	if (httpServer.hasArg("addr"))
		Close(atoi(httpServer.arg("addr").c_str()));
	else
		Close();
	Return200();
}

void HTTP_handleStop(void)
{
	HTTP_Activity();
	elog.Add(EI_Cmd_Stop, EL_INFO, ES_HTTP);
	if (httpServer.hasArg("addr"))
		Stop(atoi(httpServer.arg("addr").c_str()));
	else
		Stop();
	Return200();
}

void HTTP_handleSet(void)
{
	uint8_t addr=ADDR_ALL;
	HTTP_Activity();
	if (httpServer.hasArg("addr")) addr=atoi(httpServer.arg("addr").c_str());
	if (httpServer.hasArg("pos"))
	{
		int pos=atoi(httpServer.arg("pos").c_str());
		elog.Add(EI_Cmd_Percent, EL_INFO, ES_HTTP + (pos << 8));
		if (pos==0) Open(addr);
		else if (pos==100) Close(addr);
		else if (pos>0 && pos<100) ToPercent(pos, addr);
		Return200();
	}
	else if (httpServer.hasArg("steps"))
	{
		int pos=atoi(httpServer.arg("steps").c_str());
		elog.Add(EI_Cmd_Steps, EL_INFO, ES_HTTP + (pos << 8));
		ToPosition(pos, addr);
		Return200();
	}
	else if (httpServer.hasArg("stepsovr"))
	{
		int pos=atoi(httpServer.arg("stepsovr").c_str());
		if (position<0) position=0;
		roll_to=pos;
		Return200();
	}
	else if (httpServer.hasArg("preset"))
	{
		int preset=atoi(httpServer.arg("preset").c_str());
		elog.Add(EI_Cmd_Preset, EL_INFO, ES_HTTP + (preset << 8));
		ToPreset(preset, addr);
		Return200();
	}
	else if (httpServer.hasArg("led"))
	{
		String s = httpServer.arg("led");
		s.toLowerCase();
		if (LED_Command(s.c_str()))
			Return200();
		else
			httpServer.send(400, "text/XML", blank_xml); // 400 Bad Request
	}
	else if (httpServer.hasArg("wake"))
	{
		SendUART(UART_CMD_WAKE, addr);
		Return200();
	}
	else if (httpServer.hasArg("click"))
	{
		elog.Add(EI_Cmd_Click, EL_INFO, ES_HTTP);
		ButtonClick(addr);
		Return200();
	}
	else if (httpServer.hasArg("longclick"))
	{
		elog.Add(EI_Cmd_LClick, EL_INFO, ES_HTTP);
		ButtonLongClick(addr);
		Return200();
	}
	else if (httpServer.hasArg("blink"))
	{
		SendUART(UART_CMD_BLINK, addr);
		Return200();
	}
	else
		httpServer.send(400, "text/XML", blank_xml); // 400 Bad Request
}

void HTTP_handleTest(void)
{
	bool dir=false;
	uint8_t bak_pinout;
	bool bak_reversed;
	uint16_t bak_step_delay_mks;
	int steps=300;

	HTTP_Activity();

	bak_pinout=ini.pinout;
	bak_reversed=ini.reversed;
	bak_step_delay_mks=ini.step_delay_mks;

	if (httpServer.hasArg("up")) dir=true;
	if (httpServer.hasArg("reversed")) ini.reversed=atoi(httpServer.arg("reversed").c_str());
	if (httpServer.hasArg("pinout")) ini.pinout=atoi(httpServer.arg("pinout").c_str());
	if (ini.pinout>=4) ini.pinout=0;
	if (httpServer.hasArg("delay")) ini.step_delay_mks=atoi(httpServer.arg("delay").c_str());
	if (ini.step_delay_mks<MIN_STEP_DELAY) ini.step_delay_mks=MIN_STEP_DELAY;
	if (ini.step_delay_mks>65000) ini.step_delay_mks=65000;
	if (httpServer.hasArg("steps")) steps=atoi(httpServer.arg("steps").c_str());
	if (steps<0) steps=0;

	FillStepsTable();
	AdjustTimerInterval();

	switch_ignore_steps=ini.switch_ignore_steps;
	if (dir ^ ini.sw_at_bottom)
		roll_to=position-steps;
	else
		roll_to=position+steps;

	uint32_t start_time=millis();
	while(roll_to != position && (millis()-start_time < 10000))
	{
		delay(10);
		yield();
	}
	roll_to=position;

	ini.pinout=bak_pinout;
	ini.reversed=bak_reversed;
	ini.step_delay_mks=bak_step_delay_mks;

	FillStepsTable();
	AdjustTimerInterval();

	httpServer.send(200, "text/html", String(position));
}

String MakeNode(const __FlashStringHelper *name, String val)
{
	char buf[128], buf2[16];
	strcpy_P(buf2, (char*)name);
	snprintf_P(buf, sizeof(buf),PSTR("<%s>%s</%s>"), buf2, val.c_str(), buf2);
	return buf;
}
void HTTP_handleXML(void)
{
#if RF
	if (httpServer.hasArg("rf"))
	{
		RF_handleXML();
		return;
	}
#endif

	String XML, s;
	uint32_t realSize = ESP.getFlashChipRealSize();
	uint32_t ideSize = ESP.getFlashChipSize();
	FlashMode_t ideMode = ESP.getFlashChipMode();

	XML.reserve(1024);
	XML = F("<?xml version='1.0'?>");
	XML += F("<Curtain>");
	XML += F("<Info>");
	XML += MakeNode(F("Version"), VERSION);
	XML += MakeNode(F("IP"), WiFi.localIP().toString());
	s=String(ini.name);
	s.replace(F("<"), F("&lt;"));
	s.replace(F(">"), F("&gt;"));
	XML += MakeNode(F("Name"), s);
	XML += MakeNode(F("Hostname"), String(ini.hostname));
	XML += MakeNode(F("Time"), ((lastSync == 0) ? SL("unknown", "хз") : TimeStr() + " [" + DoWName(DayOfWeek(getTime())) + "]"));
	XML += MakeNode(F("UpTime"), UptimeStr());
	XML += MakeNode(F("RSSI"), String(WiFi.RSSI()) + SL(" dBm", " дБм"));
	XML += MakeNode(F("MQTT"), MQTTstatus());
	if (voltage_available)
		XML += MakeNode(F("Voltage"), GetVoltageStr() + SL("V", "В"));
	XML += MakeNode(F("Log"), String(elog.Count()));
//XML += MakeNode(F("Debug"), payload);
	XML += F("</Info>");

	XML += F("<ChipInfo>");
	XML += MakeNode(F("ID"), String(ESP.getChipId(), HEX));
	XML += MakeNode(F("FlashID"), String(ESP.getFlashChipId(), HEX));
	XML += MakeNode(F("RealSize"), MemSize2Str(realSize));
	XML += MakeNode(F("IdeSize"), MemSize2Str(ideSize));
	XML += MakeNode(F("Speed"), String(ESP.getFlashChipSpeed() / 1000000) + SL("MHz", "МГц"));
	XML += MakeNode(F("IdeMode"), (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
	XML += F("</ChipInfo><RF>");

#if RF
	XML += MakeNode(F("LastCode"), String(last_rf_code));
	XML += MakeNode(F("Hex"), String(last_rf_code, HEX));
#endif

	XML += F("</RF><Position>");
	XML += MakeNode(F("Now"), String(position));
	XML += MakeNode(F("Dest"), String(roll_to));
	XML += MakeNode(F("Max"), String(ini.full_length));
	XML += MakeNode(F("End1"), onoff[ini.lang][IsSwitchPressed()]);
	XML += F("</Position><LED>");
	XML += MakeNode(F("Mode"), LEDModeString());
	XML += MakeNode(F("Level"), LEDLevelString());
	XML += F("</LED></Curtain>");

	httpServer.sendHeader(F("Access-Control-Allow-Origin"), "*"); // Allowing Cross-Origin Resource Sharing
	httpServer.send(200, "text/XML", XML);
}

void date2str(char* buf, uint32_t t)
{
	const uint32_t day_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int year = 1970, month = 1, hour, minute, sec;
	uint32_t days;
	uint8_t leap=2;

	t += ini.timezone*60;
	sec = t % 60;
	minute = (t / 60) % 60;
	hour = (t / (60*60)) % 24;
	t /= 24*60*60;
	while(1)
	{
		days=365;
		if (!leap) days++;
		if (t < days) break;
		t -= days;
		year++;
		leap = (leap + 1) % 4;
	}
	while (1)
	{
		uint32_t dm = day_month[month-1];
		if (!leap && month == 2) dm++; // Feb in leap year
		if (t < dm) break;
		month++;
		t -= dm;
	}
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", year, month, t+1, hour, minute, sec);
	//return String(year)+"."+String(month)+"."+String(t+1);
}

#define STR_(X) #X
#define STR(X) STR_(X)
void HTTP_handleLog(void)
{
	String out;
	char buf[20]; // 2021-01-01 00:00:00
	const LogEntry* e;
	uint32_t t;
	idx i;
	bool ajax;

	HTTP_Activity();

	ajax = httpServer.hasArg("table"); // request only log table
	if (!ajax)
	{
		out.reserve(10240);
		out = HTML_header();
		out += F("<section class=\"log\" id=\"log\"><table id=\"log_table\"><tr class=\"sect_name\"><td colspan=\"2\">");
		out += FLF("Last " STR(MAX_LOG_ENTRIES) " log entries", "Последние " STR(MAX_LOG_ENTRIES) " записи");
		out += F("</td></tr>");
	}

	i = elog.Count();
	while (i-- > 0)
	{
		e = elog.Get(i);
		if (e)
		{
			out += F("<tr><td class=\"");
			switch (e->level)
			{
				case EL_WARN: out += F("warn"); break;
				case EL_ERROR: out += F("error"); break;
				case EL_DEBUG: out += F("debug"); break;
				default: out+= F("info"); break;
			}
			out += F("\">[");
			t = e->time;
			if (t > 100*24*60*60)
			{ // less than 100 days -> no ntp data, time from reboot in seconds
				date2str(buf, t);
				out += buf; // time->tm_year;
			} else
			{
				out += t;
				out += " sec";
			}
			out += F("]</td><td>");
			out += FPSTR((char*)pgm_read_dword(&(event_txt[e->event])));
			out += F(" ");
			switch (e->event)
			{
				case EI_NTP_Sync: date2str(buf, e->val); out += buf; break;
				case EI_Cmd_Percent:
				case EI_Cmd_Steps:
				case EI_Cmd_Preset: // LSByte - source of command, 3*MSByte - value
					out += F("Value: ");
					out += (e->val >> 8);
					out += F(" ");
					// no break here, continue
				case EI_Cmd_Stop:
				case EI_Cmd_Open:
				case EI_Cmd_Close:
				case EI_Cmd_Click:
				case EI_Cmd_LClick: 
					out += FPSTR((char*)pgm_read_dword(&(event_src_txt[e->val & 0xFF]))); break;
				case EI_Wifi_Got_IP:
					out += IPAddress(e->val).toString(); break;
				case EI_Endstop_Hit:
				case EI_Endstop_Hit_Error:
					out += F("Pos: ");
					out += (int32_t)e->val;
					break;
				case EI_Started:
					out += F("after ");
					switch (e->val)
					{
						case REASON_DEFAULT_RST: out += F("power on"); break;
						case REASON_WDT_RST: out += F("WDT reset"); break;
						case REASON_EXCEPTION_RST: out += F("exception"); break;
						case REASON_SOFT_WDT_RST: out += F("soft WDT reset"); break;
						case REASON_SOFT_RESTART: out += F("soft restart"); break;
						case REASON_DEEP_SLEEP_AWAKE: out += F("deep sleep"); break;
						case REASON_EXT_SYS_RST: out += F("external reset"); break;
						default: out += F("Unknown"); break;
					}
					break;
				default: break;//out += e->val; break;
			}
			out += F("</td></tr>\n");
		}
	}

	if (!ajax)
	{
		out += F("</table>\n");
		out += HTML_footer();
	}
	httpServer.send(200, "text/html", out);
}

int dumb;
void Scheduler()
{
	uint32_t t, p;

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

		if (ini.alarms[a].percent_open <= 100)
		{ // percentage
			p = ini.alarms[a].percent_open;
			elog.Add(EI_Cmd_Percent, EL_INFO, ES_SCHEDULE + (p << 8));
			ToPercent(p);
		} else if (ini.alarms[a].percent_open <= 100+MAX_PRESETS)
		{ // preset
			p = ini.alarms[a].percent_open-100;
			elog.Add(EI_Cmd_Preset, EL_INFO, ES_SCHEDULE + (p << 8));
			ToPreset(p);
		}
	}
	dumb=0;
}

void loop(void)
{
	ProcessWiFi();
	ProcessLED();
	ProcessRF();

	httpServer.handleClient();
	Process_OTA();
	SyncNTPTime();
	Scheduler();
	if (!SLAVE) ProcessMQTT();
	CP_process();
	process_Button();
	process_Aux();
	if (MASTER) SendUARTPing();
	if (SLAVE) ProcessUART();

	if (millis() - last_network_time > 10000)
		WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	if (SLAVE && WiFi_active &&
		(millis() - last_network_time > SLAVE_SLEEP_TIMEOUT_MS) &&
		(millis() - lastUARTping < SLAVE_MAX_NO_PING_MS))
	{
		Serial.println(F("Network idle. WiFi shutdown"));
		WiFi_Off();
	}
	if (endstop_hit)
	{
		if (endstop_hit != EL_NONE)
		{
			if (endstop_hit == EL_ERROR)
				elog.Add(EI_Endstop_Hit_Error, EL_ERROR, endstop_hit_pos);
			else
				elog.Add(EI_Endstop_Hit, EL_INFO, endstop_hit_pos);
			endstop_hit = EL_NONE;
		}
	}

	delay(10); // this delay enables light sleep mode
	static int heap=65536;
	int h;
	h=ESP.getFreeHeap();
	if (h<heap)
	{
		heap=h;
		Serial.println(heap);
	}
}
