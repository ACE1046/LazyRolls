/*
LazyRolls
(C) 2019-2022 ACE, a_c_e@mail.ru
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
09.12.2022 v0.13
20.04.2023 v0.14
28.12.2023 v0.15

*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#define FS_NO_GLOBALS 1
#include <FS.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include "settings.h"
extern "C" {
#include <lwip/etharp.h> // gratuitous arp
#include <ping.h>
}

#define VERSION "0.15 beta"
#define MQTT 1 // MQTT & HA functionality
#define ARDUINO_OTA 1 // Firmware update from Arduino IDE
#define MDNSC 0 // mDNS responder. Required for ArduinoIDE web port discovery
#define DAYLIGHT 1 // Sunrise functions
#define RF 1 // RF receiver support
#define SPIFFS_AUTO_INIT

#if FLASH_MAP_SUPPORT
#define AUTOMEMSIZE
#include <flash_hal.h>
// Custom memory map, 128K SPIFFS for 1MB modules, 1M for 4MB
#define FLASH_MAP_LAZYROLL \
	{ \
		{ .eeprom_start = 0x402fb000, .fs_start = 0x402db000, .fs_end = 0x402fb000, .fs_block_size = 0x1000, .fs_page_size = 0x100, .flash_size_kb = 1024 }, \
		{ .eeprom_start = 0x403fb000, .fs_start = 0x403c0000, .fs_end = 0x403fb000, .fs_block_size = 0x1000, .fs_page_size = 0x100, .flash_size_kb = 2048 }, \
		{ .eeprom_start = 0x405fb000, .fs_start = 0x40500000, .fs_end = 0x405fa000, .fs_block_size = 0x2000, .fs_page_size = 0x100, .flash_size_kb = 4096 }, \
		{ .eeprom_start = 0x409fb000, .fs_start = 0x40400000, .fs_end = 0x409fa000, .fs_block_size = 0x2000, .fs_page_size = 0x100, .flash_size_kb = 8192 }, \
		{ .eeprom_start = 0x411fb000, .fs_start = 0x40400000, .fs_end = 0x411fa000, .fs_block_size = 0x2000, .fs_page_size = 0x100, .flash_size_kb = 16384 }, \
		{ .eeprom_start = 0x4027b000, .fs_start = 0x40273000, .fs_end = 0x4027b000, .fs_block_size = 0x1000, .fs_page_size = 0x100, .flash_size_kb = 512 }, \
	}
  FLASH_MAP_SETUP_CONFIG(FLASH_MAP_LAZYROLL);
#endif

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
	#include "daylight.h"
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
#define PIN_PWM 15
#define PIN_ENC_A PIN_C
#define PIN_ENC_B PIN_D
#define DIR_UP (-1)
#define DIR_DN (1)
#define MAX_PINOUT 8 // Motor types. 3xStepper + 1 step/dir + 4 DC/PWM/Enc
#define DEFAULT_UP_SAFE_LIMIT 300 // make extra rotations up if not hit switch
#define DEFAULT_SWITCH_IGNORE_STEPS 100 // ignore endstop for first step on moving down
#define MIN_STEP_DELAY 50 // minimal motor step time in mks
#define UART_PING_INTERVAL_MS (30*1000) // Master ping slaves every 30 seconds
#define SLAVE_MAX_NO_PING_MS (70*1000) // Enable WiFi in slave mode if no ping from master
#define SLAVE_SLEEP_TIMEOUT_MS (180*1000) // Disable WiFi in slave mode after network idle
#define PINGER_WARN 4 // Log sfter N ping fails
#define PINGER_ACTION 7 // Action (log/reconnect/reboot) after N ping fails
#define PINGER_INTERVAL_S 60 // Ping interval, seconds
#define PINGER_REPEAT_INTERVAL_S 10 // Ping interval after ping fail, seconds
#define ALARMS 10
#define DAY (24*60*60) // day length in seconds
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define MASTER (ini.slave == 255)
#define SLAVE (ini.slave != 255 && ini.slave != 0)
#define ADDR_MASTER 0
#define ADDR_SELF_ONLY 0
#define ADDR_ALL 255
#define ADDR_DEFAULT 254
#define MQTT_INFO_SECONDS 5*60 // Send mqtt info (rssi, uptime, etc) every N seconds

const int Languages = 2;
const char ru_ru[] PROGMEM = "Русский";
const char en_en[] PROGMEM = "English";
const __FlashStringHelper *Language[Languages]={ (__FlashStringHelper*)en_en, (__FlashStringHelper*)ru_ru };
const PROGMEM char *onoff[][Languages]={{"off", "on"}, {"выкл", "вкл"}};

const uint8_t steps[3][4]={
	{PIN_A, PIN_C, PIN_B, PIN_D},
	{PIN_A, PIN_B, PIN_D, PIN_C},
	{PIN_A, PIN_B, PIN_C, PIN_D}
};

const uint8_t hardware_pins[] = { 0, 0, 2, 3, 15 }; // available GPIOs, must correspond to next line
#define PIN_LIST F("0,\"---\",1,\"GPIO0 (D3/DTR) - Gnd\",2,\"GPIO2 (D4) - Gnd\",3,\"GPIO3 (RX) - Gnd\",4,\"GPIO15 (D8) - 3.3V\"")

volatile int position; // current motor steps position. 0 - at main endstop
volatile int roll_to; // position to go to
uint16_t step_c[8], step_s[8]; // bitmasks for every step
volatile uint8_t endstop_hit = 0; // endstop hit flag. Set at ISR, reset at user level
volatile int endstop_hit_pos = 0; // position of endstop hit
volatile bool TestMode = 0; // Ignoring endstop in test mode
bool voltage_available = 0;
uint32_t last_network_time = 0; // last network activity time
bool WiFi_active = false;
bool WiFi_connected = false;
bool WiFi_AP_disabled = false; // AP mode disabled after successfull connection to home network or after first ping msg from master
int WiFi_attempts = 0;
unsigned long last_reconnect = 0;
uint32_t uart_crc_errors = 0;
#define MAX_RECONNECT_ATTEMPS 2 // reconnect attemps before creating Access Point
bool rf_page_open = 0; // true while RF settings open
bool mem_problem = 0; // memory error flag, incorrect compile settings
volatile uint8_t pwm_LED = 0; // software LED PWM. 0 - disabled
uint16_t pwm_phase_low_ticks, pwm_phase_high_ticks;
volatile bool virtual_endstop_hit = 0;
uint16_t ticks_per_step = 4; // timer ticks per motor step, used for soft start
uint8_t Master_IP = 0; // Master's IP, last byte. Slaves report to it after wakeup
uint8_t Slaves_IP[6]; // IPs of slaves after they woke up

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient espClient;

typedef struct {
	uint16_t tmin; // minimal time to sun option
	uint16_t time;
	uint8_t percent_open; // 0 - open, 100 - close, 101-105 - presets
	uint8_t day_of_week; // LSB - monday
	uint8_t flags;
	uint8_t m_s; // master / slaves bitmask
	uint32_t reserved;
} alarm_type;
#define ALARM_FLAG_ENABLED 0x01
#define ALARM_FLAG_SUNRISE 0x02
#define ALARM_FLAG_SUNSET  0x04
#define ALARM_FLAG_SLOW    0x08

typedef struct {
	uint32_t code;
	uint8_t action;
	uint8_t flags; // reserved
	uint16_t reserved;
} rf_cmd_type;
#define RF_FLAG_STOP2ND 0x01

typedef struct {
	uint8_t num; // slave group number
	uint8_t ip4; // ip address last byte, 0 = empty
	uint16_t reserved;
} ip_slave_type;

#define MAX_SLAVE 5
#define MAX_IP_SLAVE 10
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
	uint8_t btn1_pin; // hardware button pin selection
	uint8_t btn_mode; // auto/mqtt report (reserved)
	uint8_t slave; // master/slave mode
	uint8_t aux_pin; // auxiliary pin selection
	char mqtt_topic_aux[127+1]; // auxiliary input topic
	char mqtt_topic_info[127+1]; // information topic (IP, RSSI, etc)
	char name[64+1]; // Name
	ip4_addr ip, mask, gw, dns; // Network config
	rf_cmd_type rf_cmd[MAX_RF_CMDS]; // RF433 commands
	uint8_t rf_pin; // RF remote input pin
	int32_t lat; // latitude
	int32_t lng; // longitude
	uint8_t btn2_pin; // hardware button pin selection
	uint8_t btn1_click, btn2_click; // Action on button click
	uint8_t btn1_long, btn2_long; // Action on button long click
	uint8_t btn1_c_addr, btn2_c_addr; // Button click, master and slaves selection, bitmask
	uint8_t btn1_l_addr, btn2_l_addr; // Long click, master and slaves selection, bitmask
	char ap_pswd[16+1]; // Access Point password
	uint8_t end2_pin; // second endstop pin selection
	uint8_t switch2_reversed; // reverse second switch logic. 0 - NC, 1 - NO
	uint16_t step_delay_mks2; // delay (mks) for each step of motor, 2nd speed
	uint8_t ping_enabled; // pinger watchdog enabled
	uint8_t ping_act; // action on no ping response
	ip4_addr ping_ip; // ip address for pinger
	ip_slave_type ip_slaves[MAX_IP_SLAVE]; // ip slaves list
} ini;

// language functions
const char * L(const char *s1, const char *s2)
{
	return (ini.lang == 0) ? s1 : s2;
}
const __FlashStringHelper *FL(const __FlashStringHelper *s1, const __FlashStringHelper *s2)
{
	return (ini.lang == 0) ? s1 : s2;
}
#define FLF(a, b) FL(F(a), F(b))
String SL(const char *s1, const char *s2)
{
	return (ini.lang == 0) ? String(s1) : String(s2);
}
String SL(String s1, String s2)
{
	return (ini.lang == 0) ? s1 : s2;
}

void NetworkActivity(void)
{
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	last_network_time = millis();
}

#define SPEED2 (-1)
void ToPercent(uint8_t pos, uint8_t address = ADDR_ALL, int step_delay = 0);
void ToPosition(int pos, uint8_t address = ADDR_ALL, int step_delay = 0);
void ToPreset(uint8_t preset, uint8_t address = ADDR_ALL, int step_delay = 0);
void Open(uint8_t address = ADDR_ALL, int step_delay = 0);
void Close(uint8_t address = ADDR_ALL, int step_delay = 0);
void Stop(uint8_t address);
void ButtonClick(uint8_t btnnumber, uint8_t address);
void ButtonLongClick(uint8_t btnnumber, uint8_t address);
void WiFi_On();
void WiFi_Off();
uint32_t getTime();
uint32_t getTimeUTC();
String MakeNode(const __FlashStringHelper *name, String val);
void CalcAlarmTimes();
void AdjustTimerInterval(int step_delay_mks = 0); // 0 - default, -1 - 2nd speed

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
const char ET_Cmd_Click[] PROGMEM = "Click btn 1";
const char ET_Cmd_LClick[] PROGMEM = "Long click btn 1";
const char ET_Cmd_Click2[] PROGMEM = "Click btn 2";
const char ET_Cmd_LClick2[] PROGMEM = "Long click btn 2";
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
const char ET_Stall[] PROGMEM = "Motor stalled";
const char ET_Auto[] PROGMEM = "Open/Close";
const char ET_Cmd_Zero[] PROGMEM = "Zero reset";
const char ET_Pingfail[] PROGMEM = "Ping failed ";
const char ET_Cmd_Wakeup[] PROGMEM = "Wake up";
const char ET_IP_Slave_Err[] PROGMEM = "WiFi slave error";

const char EQ_HTTP[] PROGMEM = "Src: HTTP";
const char EQ_MQTT[] PROGMEM = "Src: MQTT";
const char EQ_UART_MASTER[] PROGMEM = "Src: TX/RX master";
const char EQ_HTTP_MASTER[] PROGMEM = "Src: WIFI master";
const char EQ_SCHEDULE[] PROGMEM = "Src: Scheduler";
const char EQ_BUTTON[] PROGMEM = "Src: Button";
const char EQ_RF[] PROGMEM = "Src: RF";

enum EVENT_LEVEL { EL_NONE = 0, EL_DEBUG, EL_INFO, EL_WARN, EL_ERROR };
enum EVENT_ID                           { EI_Err1, EI_NTP_Sync, EI_Settings_Loaded, EI_Settings_Saved, EI_Settings_Not_Loaded, EI_Cmd_Stop, EI_Cmd_Open, EI_Cmd_Close,
	EI_Cmd_Percent, EI_Cmd_Steps, EI_Cmd_Preset, EI_Cmd_Click, EI_Cmd_LClick, EI_Cmd_Click2, EI_Cmd_LClick2, EI_Slave_No_Ping, EI_Wifi_Close_AP, EI_Wifi_Reconnect,
	EI_Wifi_Got_IP, EI_Wifi_Start_AP, EI_Endstop_Hit, EI_Endstop_Hit_Error, EI_MQTT_Connect, EI_MQTT_Connecting, EI_Started, EI_Wifi_Disconnect, EI_Stall, EI_Auto,
	EI_Cmd_Zero, EI_Pingfail, EI_Cmd_Wakeup, EI_IP_Slave_Err };
const char* const event_txt[] PROGMEM = { ET_Err1, ET_NTP_Sync, ET_Settings_Loaded, ET_Settings_Saved, ET_Settings_Not_Loaded, ET_Cmd_Stop, ET_Cmd_Open, ET_Cmd_Close,
	ET_Cmd_Percent, ET_Cmd_Steps, ET_Cmd_Preset, ET_Cmd_Click, ET_Cmd_LClick, ET_Cmd_Click2, ET_Cmd_LClick2, ET_Slave_No_Ping, ET_Wifi_Close_AP, ET_Wifi_Reconnect,
	ET_Wifi_Got_IP, ET_Wifi_Start_AP, ET_Endstop_Hit, ET_Endstop_Hit_Error, ET_MQTT_Connect, ET_MQTT_Connecting, ET_Started, ET_Wifi_Disconnect, ET_Stall, ET_Auto,
	ET_Cmd_Zero, ET_Pingfail, ET_Cmd_Wakeup, ET_IP_Slave_Err };

enum EVENT_SRC                              { ES_HTTP, ES_MQTT, ES_UART_MASTER, ES_HTTP_MASTER, ES_SCHEDULE, ES_BUTTON, ES_RF };
const char* const event_src_txt[] PROGMEM = { EQ_HTTP, EQ_MQTT, EQ_UART_MASTER, EQ_HTTP_MASTER, EQ_SCHEDULE, EQ_BUTTON, EQ_RF };

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
	LogEntry log[MAX_LOG_ENTRIES]; // 12*32 = 384 bytes of RAM
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
	pwm_LED = 0;
	digitalWrite(PIN_LED, LOW);
}

void LED_Off()
{
	pwm_LED = 0;
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
	static uint32_t last_blink = 0;

	if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP) return;

	if (led_mode == LED_ON)
	{
		if (led_level == LED_LOW) pwm_LED = 240; else
		if (led_level == LED_MED) pwm_LED = 192; else
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
bool cp_active = false;
const uint16_t DNS_PORT = 53;

void CP_create()
{
	dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
	dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
	cp_active = true;
}

void CP_delete()
{
	cp_active = false;
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
uint8_t NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
const uint32_t seventyYears = 2208988800UL;
uint32_t UNIXTime, last_restart_time = 0;
WiFiUDP UDP;

void UpdateMQTTInfo();

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
		lastSync = millis();
		if (!last_restart_time)
		{
			last_restart_time = UNIXTime;
			UpdateMQTTInfo();
		}
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

String TimeToStr(int32_t t)
{
	char buf[6];
	sprintf_P(buf, PSTR("%02d:%02d"), t/60%24, t%60);
	return String(buf);
}

void Time2YMD(uint32_t t, int &year, int &month, int &day)
{
	uint32_t days;
	uint8_t leap = 2;
	const uint32_t day_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	t /= DAY;
	year = 1970;
	month = 1;
	while(1)
	{
		days = 365;
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
	day = t + 1;
}

String UptimeStr()
{
	char buf[9];
	uint32_t t = millis() / 1000;
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

// ===================== Daylight =============================

#if DAYLIGHT

uint16_t GetSunTime(int sun_height, int sunrise, bool tomorrow = false)
{
	int y, m, d;
	if (sun_height < -5 || sun_height > 5) return (uint16_t)-1;
	Time2YMD(getTime(), y, m, d);
	d = computeDayOfYear(y, m, d);
	ZENITH = FROMFLOAT(-.83f + sun_height);
	if (tomorrow) d++;
	return calculateSunriseSunset(d, ini.lat, ini.lng, ini.timezone, 0, sunrise);
}

String PrintSunriseTable()
{
	String out = "";
	out.reserve(800);
	out += F("<table class=\"sun\"><tr><th>");
	out += F("</th><th colspan=\"2\">");
	out += FLF("Today", "Сегодня");
	out += F("</th><th colspan=\"2\">");
	out += FLF("Tomorrow", "Завтра");
	out += F("</th></tr>\n<tr><th>");
	out += FLF("Sun height", "Высота солнца");
	out += F("</th><th>");
	out += FLF("Sunrise", "Восход");
	out += F("</th><th>");
	out += FLF("Sunset", "Закат");
	out += F("</th><th>");
	out += FLF("Sunrise", "Восход");
	out += F("</th><th>");
	out += FLF("Sunset", "Закат");
	out += F("</th></tr>\n");
	for (int i = 5; i >= -5; i--)
	{
		out += ("<tr><td>");
		out += FLF("Horizon ", "Горизонт ");
		if (i > 0) out += "+" + String(i);
		if (i < 0) out += "−" + String(-i);
		if (i != 0) out += ("&deg;");
		out += F("</td><td>");
		out += TimeToStr(GetSunTime(i, 1));
		out += ("</td><td>");
		out += TimeToStr(GetSunTime(i, 0));
		out += ("</td><td>");
		out += TimeToStr(GetSunTime(i, 1, true));
		out += ("</td><td>");
		out += TimeToStr(GetSunTime(i, 0, true));
		out += ("</td></tr>\n");
	}
	out += ("</table>\n");
	return out;
}
#else
String PrintSunriseTable() { return FLF("<p>Sun time functions are disabled in this build</p>", "<p>Солнечное расписание отключено в этой прошивке</p>"); }
uint32_t GetSunTime(int sun_height, int sunrise, bool tomorrow = false)
{
	return (uint32_t)-1;
}
#endif

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
#define UART_CMD_CLICK2 'l' // button 2 click
#define UART_CMD_LONGCLICK2 'L' // button 2 long press
#define CRC_INIT 0xEA // just a random nonzero number for checksum
#define WIFI_SLAVE_ATTEMPTS 2 // http commands send attempts

uint32_t lastUARTping = 0;
char const *int2hex="0123456789ABCDEF";

void ReportToMaster()
{
	if (Master_IP == 0) return;

	HTTPClient http;
	char url[50];

	int httpResponseCode;
	snprintf_P(url, sizeof(url), PSTR("http://10.0.2.%d/set?slave_ip=%d"), Master_IP, WiFi.localIP()[3]);
	for (int r=0; r<WIFI_SLAVE_ATTEMPTS; r++)
	{
		http.setTimeout(1000);
		http.begin(espClient, url);
		httpResponseCode = http.GET();
		if (httpResponseCode == 200) break; // OK
	}
	if (httpResponseCode != 200)
		elog.Add(EI_IP_Slave_Err, EL_WARN, httpResponseCode * 256 + Master_IP);
	http.end();
}

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

void WakeUp(uint32_t val)
{
	if (!SLAVE || val > 255) return;
	Master_IP = val;
	if (WiFi_connected)
		ReportToMaster();
	else
		WiFi_On();
}

void ExecuteSlaveCommand(char cmd, uint32_t val, EVENT_SRC src = ES_UART_MASTER)
{
	int speed = 0;
	if (cmd > 0x80) speed = SPEED2; // 2nd speed
	switch (cmd & 0x7F)
	{
		case UART_CMD_OPEN:       Open(ADDR_SELF_ONLY, speed);            elog.Add(EI_Cmd_Open,    EL_INFO, src); break;
		case UART_CMD_CLOSE:      Close(ADDR_SELF_ONLY, speed);           elog.Add(EI_Cmd_Close,   EL_INFO, src); break;
		case UART_CMD_STOP:       Stop(ADDR_SELF_ONLY);                   elog.Add(EI_Cmd_Stop,    EL_INFO, src); break;
		case UART_CMD_PERCENT:    ToPercent(val, ADDR_SELF_ONLY, speed);  elog.Add(EI_Cmd_Percent, EL_INFO, src + (val<<8)); break;
		case UART_CMD_POSITION:   ToPosition(val, ADDR_SELF_ONLY, speed); elog.Add(EI_Cmd_Steps,   EL_INFO, src + (val<<8)); break;
		case UART_CMD_PRESET:     ToPreset(val, ADDR_SELF_ONLY, speed);   elog.Add(EI_Cmd_Preset,  EL_INFO, src + (val<<8)); break;
		case UART_CMD_WAKE:       WakeUp(val);                            elog.Add(EI_Cmd_Wakeup,  EL_INFO, src); break;
		case UART_CMD_BLINK:      LED_Blink(LED_HIGH); break;
		case UART_CMD_PING:       UARTPingReceived(val); break;
		case UART_CMD_CLICK:      ButtonClick(1, ADDR_SELF_ONLY);         elog.Add(EI_Cmd_Click,   EL_INFO, src); break;
		case UART_CMD_LONGCLICK:  ButtonLongClick(1, ADDR_SELF_ONLY);     elog.Add(EI_Cmd_LClick,  EL_INFO, src); break;
		case UART_CMD_CLICK2:     ButtonClick(2, ADDR_SELF_ONLY);         elog.Add(EI_Cmd_Click,   EL_INFO, src); break;
		case UART_CMD_LONGCLICK2: ButtonLongClick(2, ADDR_SELF_ONLY);     elog.Add(EI_Cmd_LClick,  EL_INFO, src); break;
	}
	// All commands received from HTTP master will be sent to UART slaves
	if (src == ES_HTTP_MASTER && MASTER) SendUART(cmd, ADDR_ALL, val);
}

void ProcessUART()
{
	static uint8_t buf[11];
	static uint8_t inbuf = 0;
	uint8_t crc;
	uint32_t val;

	// Enable WiFi in slave mode if no ping from master for SLAVE_MAX_NO_PING_MS
	if (SLAVE && !WiFi_active && (millis() - lastUARTping > SLAVE_MAX_NO_PING_MS))
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
					char cmd = buf[2];
					if ((cmd & 0x7F) == UART_CMD_PING)
						val = ((uint32_t)buf[3] << 24) + ((uint32_t)buf[4] << 16) + ((uint32_t)buf[5] << 8) + buf[6];
					else
						val = (buf[3]-'0')*10000 + (buf[4]-'0')*1000 + (buf[5]-'0')*100 + (buf[6]-'0')*10 + (buf[7]-'0');
					ExecuteSlaveCommand(cmd, val);
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

void IfMasterSendUART(uint8_t cmd, uint8_t address, uint32_t val=0)
{
	// UART slaves
	if (MASTER && (address != ADDR_MASTER)) SendUART(cmd, address, val);

	// IP slaves
	if (SLAVE || !WiFi_connected) return;
	HTTPClient http;
	char url[50];

	for (int i=0; i<MAX_IP_SLAVE; i++)
	{
		uint8_t ip = ini.ip_slaves[i].ip4;
		if (ip !=0 && (address == ADDR_ALL || ini.ip_slaves[i].num + 1 == address))
		{
			int httpResponseCode;
			snprintf_P(url, sizeof(url), PSTR("http://10.0.2.%d/set?slave=%d&val=%d"), ip, cmd, val);
			for (int r=0; r<WIFI_SLAVE_ATTEMPTS; r++)
			{
				http.setTimeout(1000);
				http.begin(espClient, url);
				httpResponseCode = http.GET();
				if (httpResponseCode == 200) break; // OK
			}
			if (httpResponseCode != 200)
				elog.Add(EI_IP_Slave_Err, EL_WARN, httpResponseCode * 256 + ip);
		}
	}
	http.end();
}

bool IsIPMaster()
{
	if (SLAVE) return false; // UART slave can't be IP Master
	for (int i=0; i<MAX_IP_SLAVE; i++)
		if (ini.ip_slaves[i].ip4) return true; // IP Master if there is non zero slave IP
	return false;
}

//----------------------- Motor ----------------------------------------
#define PINOUT_SD 3 // step/dir motor
#define PINOUT_DC 4 // DC motor
#define PINOUT_DC_PWM 5
#define PINOUT_DC_ENC 6
#define PINOUT_DC_PWM_ENC 7

#define MOTOR_TYPE_DC (ini.pinout == PINOUT_DC || ini.pinout == PINOUT_DC_PWM || ini.pinout == PINOUT_DC_ENC || ini.pinout == PINOUT_DC_PWM_ENC)
#define MOTOR_WITH_PWM (ini.pinout == PINOUT_DC_PWM || ini.pinout == PINOUT_DC_PWM_ENC)
#define MOTOR_WITH_ENC (ini.pinout == PINOUT_DC_ENC || ini.pinout == PINOUT_DC_PWM_ENC)

void IRAM_ATTR EnablePWM(bool en)
{
	volatile static bool pwm_active = 0;
	if (en)
	{
		if (!pwm_active)
		{
			pwm_active = 1;
			GPOC = 1 << PIN_PWM;
			ETS_CCOMPARE0_ENABLE();
			timer0_write(ESP.getCycleCount() + pwm_phase_low_ticks);
		}
	} else
	{
		pwm_active = 0;
		ETS_CCOMPARE0_DISABLE();
	}
}

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

	if (ini.pinout >= PINOUT_SD) return; // not needed for step/dir or DC

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

void SetupMotorPins()
{
	if (ini.pinout < PINOUT_SD)
	{
		pinMode(PIN_A, OUTPUT);
		pinMode(PIN_B, OUTPUT);
		pinMode(PIN_C, OUTPUT);
		pinMode(PIN_D, OUTPUT);
	} else if (ini.pinout == PINOUT_SD)
	{
		pinMode(PIN_EN, OUTPUT);
		pinMode(PIN_ST, OUTPUT);
		pinMode(PIN_DR, OUTPUT);
	} else
	{
		pinMode(PIN_A, OUTPUT);
		pinMode(PIN_B, OUTPUT);
		if (MOTOR_WITH_ENC)
		{
			pinMode(PIN_ENC_A, INPUT);
			pinMode(PIN_ENC_B, INPUT);
		}
		if (MOTOR_WITH_PWM)
		{
			pinMode(PIN_PWM, OUTPUT);
		}
	}
	pinMode(PIN_SWITCH, INPUT_PULLUP);
}

void IRAM_ATTR MotorOff()
{
	EnablePWM(false);
	switch (ini.pinout)
	{
		case PINOUT_SD:
			GPOS = (1 << PIN_EN) | (1 << PIN_ST); // Disable, step end
			break;
		case PINOUT_DC:
		case PINOUT_DC_PWM:
		case PINOUT_DC_ENC:
		case PINOUT_DC_PWM_ENC:
			GPOC = (1 << PIN_A) | (1 << PIN_B);
			GPOC = (1 << PIN_PWM);
			break;
		default:
			// digitalWrite(PIN_A, LOW);
			// digitalWrite(PIN_B, LOW);
			// digitalWrite(PIN_C, LOW);
			// digitalWrite(PIN_D, LOW);
			GPOC = (1 << PIN_A) | (1 << PIN_B) | (1 << PIN_C) | (1 << PIN_D);
			break;
	}
}

uint8_t IRAM_ATTR pin2hw_pin(uint8_t pin)
{
	if (pin >= ARRAYSIZE(hardware_pins)) return 0;
	return hardware_pins[pin];
}

bool IRAM_ATTR IsSwitchPressed()
{
	// return GPIP(PIN_SWITCH) ^^ ini.switch_reversed;
	return (((GPI >> ((PIN_SWITCH) & 0xF)) & 1) != ini.switch_reversed);
}

bool IRAM_ATTR IsSwitch2Pressed()
{
	if (!ini.end2_pin) return 0;
	return (((GPI >> ((pin2hw_pin(ini.end2_pin)) & 0xF)) & 1) != ini.switch2_reversed);
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

uint8_t speed2(int step_delay)
{
	if (step_delay == -1) return 0x80; else return 0;
}

void ToPercent(uint8_t pos, uint8_t address, int step_delay)
{
	if ((pos<0) || (pos>100)) return;

	IfMasterSendUART(UART_CMD_PERCENT + speed2(step_delay), address, pos);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (ini.sw_at_bottom) pos=100-pos;

		if (position<0) position=0;
		AdjustTimerInterval(step_delay);
		if (pos == 0)
			roll_to=0-ini.up_safe_limit; // up to 0 and beyond (a little)
		else
			roll_to=ini.full_length * pos / 100;
	}
}

void ToPosition(int pos, uint8_t address, int step_delay)
{
	IfMasterSendUART(UART_CMD_POSITION + speed2(step_delay), address, pos);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (pos<0 || pos>ini.full_length) return;
		if (position<0) position=0;
		AdjustTimerInterval(step_delay);
		roll_to=pos;
	}
}

void ToPreset(uint8_t preset, uint8_t address, int step_delay)
{
	IfMasterSendUART(UART_CMD_PRESET + speed2(step_delay), address, preset);
	if (address == ADDR_MASTER || address == ADDR_ALL)
	{
		if (preset==0 || preset > MAX_PRESETS) return;
		if (position<0) position=0;
		AdjustTimerInterval(step_delay);
		roll_to=ini.preset[preset-1];
	}
}

void Open(uint8_t address /*= ADDR_ALL*/, int step_delay /*= 0*/)
{
	IfMasterSendUART((uint8_t)UART_CMD_OPEN + speed2(step_delay), address);
	if (address == ADDR_MASTER || address == ADDR_ALL) ToPercent(0, ADDR_MASTER, step_delay);
}

void Close(uint8_t address /*= ADDR_ALL*/, int step_delay /*= 0*/)
{
	IfMasterSendUART(UART_CMD_CLOSE + speed2(step_delay), address);
	if (address == ADDR_MASTER || address == ADDR_ALL) ToPercent(100, ADDR_MASTER, step_delay);
}

void Stop(uint8_t address = ADDR_ALL)
{
	IfMasterSendUART(UART_CMD_STOP, address);
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

	v = GetVoltage();
	snprintf_P(buf, sizeof(buf), "%d.%01d", v/1000, v/100%10);
	return buf;
}

void ResetZero()
{
	// start position is twice as fully open. So on first close we will go up till home position
	position = ini.full_length*2 + ini.up_safe_limit;
	if (IsSwitchPressed()) position = 0; // Fully open, if switch is pressed
	if (ini.end2_pin && IsSwitch2Pressed()) position = ini.full_length; // Fully closed, if switch 2 is pressed
	roll_to = position;
}

//===================== Pinger =========================================
uint8_t ping_fails = 0, ping_reply = 0;
uint32_t last_ping = 0;

void ICACHE_FLASH_ATTR user_ping_recv(void *arg, void *pdata)
{
	struct ping_resp *ping_res = (ping_resp*)pdata;

	if (ping_res->ping_err == -1)
		ping_fails++;
	else
		ping_fails = 0;
	ping_reply = 1;
}

void ICACHE_FLASH_ATTR
user_ping_sent(void *arg, void *pdata)
{
	;
}

ping_option po;
void ping()
{
	po.count = 1;

	if (ini.ping_ip.addr) po.ip = ini.ping_ip.addr; else po.ip = WiFi.gatewayIP();
	po.coarse_time = 1;
	po.recv_function = user_ping_recv;
	po.sent_function = user_ping_sent;
	po.reverse = NULL;
	ping_start(&po);
}

void ProcessPing()
{
	if (!ini.ping_enabled) return;
	if (SLAVE && !WiFi_active) return;
	if (!WiFi_connected) return;

	if (ping_reply)
	{
		if (ping_fails == PINGER_WARN) elog.Add(EI_Pingfail, EL_WARN, PINGER_WARN);
		if (ping_fails == PINGER_ACTION)
		{
			ping_fails = 0;
			elog.Add(EI_Pingfail, EL_ERROR, PINGER_ACTION);
			if (ini.ping_act == 1) // reconnect
			{
				Serial.println("Ping failed. Reconnecting");
				WiFi.disconnect();
			}
			if (ini.ping_act == 2) // reboot
			{
				Serial.println("Ping failed. Rebooting");
				ESP.reset();
			}
		}
		ping_reply = 0;
	}
	uint32_t interval = PINGER_INTERVAL_S * 1000;
	if (ping_fails > 0) interval = PINGER_REPEAT_INTERVAL_S * 1000; // faster ping repeat after first ping fail
	if (millis() - last_ping > interval)
	{
		last_ping = millis();
		ping();
	}
}

//===================== MQTT ===========================================

#if MQTT

uint8_t pin_aux;
volatile uint8_t aux_state;
uint32_t last_mqtt=0, last_mqtt_info=0;
String mqtt_topic_sub, mqtt_topic_pub, mqtt_topic_lwt, mqtt_topic_aux, mqtt_topic_inf, mqtt_node_id;

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

void mqtt_callback(char* topic, uint8_t* payload, unsigned int len)
{
	int x, p, speed;
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

	speed = 0;
	if (len>1 && str[0]=='!') // second speed
	{
		speed = SPEED2;
		str++;
		len--;
	}

	if (led_mode == LED_MQTT || led_mode == LED_MQTT_HTTP) LED_Blink();

	NetworkActivity();

	for(unsigned int i = 0; i<len; i++) str[i] = tolower(str[i]); // make it lowercase
	str[len]=0;

	x=strtol(str, NULL, 10);
	if ((x>0 && x<=100) || strcmp(str, "0") == 0)
	{
		if (ini.mqtt_invert)
			ToPercent(100-x, address, speed);
		else
			ToPercent(x, address, speed);
		elog.Add(EI_Cmd_Percent, EL_INFO, ES_MQTT + (x<<8));
	}
	else if (strcmp(str, "on") == 0) { Open(address, speed); elog.Add(EI_Cmd_Open, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "off") == 0) { Close(address, speed); elog.Add(EI_Cmd_Close, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "open") == 0) { Open(address, speed); elog.Add(EI_Cmd_Open, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "close") == 0) { Close(address, speed); elog.Add(EI_Cmd_Close, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "click") == 0) { ButtonClick(1, address); elog.Add(EI_Cmd_Click, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "longclick") == 0) { ButtonLongClick(1, address); elog.Add(EI_Cmd_LClick, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "click2") == 0) { ButtonClick(2, address); elog.Add(EI_Cmd_Click2, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "longclick2") == 0) { ButtonLongClick(2, address); elog.Add(EI_Cmd_LClick2, EL_INFO, ES_MQTT); }
	else if (strcmp(str, "stop") == 0) { Stop(address); elog.Add(EI_Cmd_Stop, EL_INFO, ES_MQTT); last_mqtt=0; } // report current position after stop command
	else if (strncmp(str, "led_", 4) == 0) LED_Command(str+4); // starts with "led_"
	else if (strncmp(str, "=", 1) == 0) { p=strtol(str+1, NULL, 10); ToPosition(p, address, speed); elog.Add(EI_Cmd_Steps, EL_INFO, ES_MQTT + (p<<8)); }
	else if (strncmp(str, "%", 1) == 0) { p=strtol(str+1, NULL, 10); ToPercent(p, address, speed); elog.Add(EI_Cmd_Percent, EL_INFO, ES_MQTT + (p<<8)); }
	else if (strncmp(str, "@", 1) == 0) { p=strtol(str+1, NULL, 10); ToPreset(p, address, speed); elog.Add(EI_Cmd_Preset, EL_INFO, ES_MQTT + (p<<8)); }
	else if (strncmp(str, "report", 6) == 0) { last_mqtt = 0; last_mqtt_info = 0; }
	else if (strncmp(str, "endstop", 7) == 0) { virtual_endstop_hit = 1; }
	else if (strncmp(str, "reboot", 6) == 0) { ESP.reset(); }
	else if (strncmp(str, "zero", 4) == 0) { ResetZero(); elog.Add(EI_Cmd_Zero, EL_INFO, ES_MQTT); }
}

String ReplaceHostname(const char *topic)
{
	String s;
	s = String(topic);
	s.replace("%HOSTNAME%", String(ini.hostname));
	s.replace("$", "_");
	s.replace("*", "_");
	s.replace("+", "_");
	s.replace("#", "_");
	return s;
}

void MakeMQTTNodeId()
{
	char s[sizeof(ini.hostname)];
	for (unsigned int i=0; i<sizeof(ini.hostname); i++)
	{
		s[i] = ini.hostname[i];
		if (ini.hostname[i] == 0) break;
		if ((s[i] >= 'a' && s[i] <= 'z') ||
			(s[i] >= 'A' && s[i] <= 'Z') ||
			(s[i] >= '0' && s[i] <= '9') ||
			(s[i] == '-')) ;
		else s[i] = '_';
	}
	mqtt_node_id = String(s);
}

void setup_MQTT()
{
	mqtt_topic_sub = ReplaceHostname(ini.mqtt_topic_command);
	mqtt_topic_pub = ReplaceHostname(ini.mqtt_topic_state);
	mqtt_topic_lwt = ReplaceHostname(ini.mqtt_topic_alive);
	mqtt_topic_aux = ReplaceHostname(ini.mqtt_topic_aux);
	mqtt_topic_inf = ReplaceHostname(ini.mqtt_topic_info);
	MakeMQTTNodeId();
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
		last_mqtt = last_mqtt_info = millis() - 3000;
		if (mqtt_topic_sub != "-")
		{
			mqtt->publish(mqtt_topic_sub.c_str(), "", true); // delete retained command
			mqtt->subscribe(mqtt_topic_sub.c_str());
		}
		if (ini.mqtt_discovery) MQTT_discover();
		if (mqtt_topic_lwt != "-") mqtt->publish(mqtt_topic_lwt.c_str(), "online", true);
	}
}

void MQTT_discover_delete_sensor(const __FlashStringHelper* sensor_id, bool binary = false)
{
	char topic[100];

	snprintf_P(topic, sizeof(topic), PSTR("homeassistant/%ssensor/%s/%s/config"), (binary ? F("binary_") : F("")), mqtt_node_id.c_str(), sensor_id);
	mqtt->publish(topic, PSTR(""), false);
}

void MQTT_Delete_HA_Sensors()
{
	MQTT_discover_delete_sensor(F("ip"));
	MQTT_discover_delete_sensor(F("rssi"));
	MQTT_discover_delete_sensor(F("uptime"));
	MQTT_discover_delete_sensor(F("last_restart_time"));
	MQTT_discover_delete_sensor(F("voltage"));
	MQTT_discover_delete_sensor(F("aux"), true);
}

void make_pair_f(char *buf, size_t strSize, const char *name, const __FlashStringHelper* val)
{
	buf[0] = 0;
	if (val)
	{
		if (name)
			snprintf_P(buf, strSize, PSTR("'%s':'%s',"), name, val);
		else
			snprintf_P(buf, strSize, PSTR("%s"), val);
	}
}

void make_pair_f(char *buf, size_t strSize, const char *name, const char *val)
{
	buf[0] = 0;
	snprintf_P(buf, strSize, PSTR("'%s':'%s',"), name, val);
}
#define make_pair(buf, size, name, val) char buf[size]; make_pair_f(buf, sizeof(buf), name, val);

void ChangeQuoteSymbol(char *buf)
// changes all ' to "
{
	int i=-1;
	while (buf[++i]) if (buf[i] == '\'') buf[i] = '\"';
}

void MQTT_discover_add_sensor(const char * device_id,
	const __FlashStringHelper* name,
	const __FlashStringHelper* sensor_id,
	const __FlashStringHelper* dev_class,
	const __FlashStringHelper* icon,
	const __FlashStringHelper* unit,
	bool binary = false,
	const __FlashStringHelper* transform = NULL,
	bool remove = false)
{
	char topic[100];
	char data[640];

	snprintf_P(topic, sizeof(topic), PSTR("homeassistant/%ssensor/%s/%s/config"), (binary ? F("binary_") : F("")), mqtt_node_id.c_str(), sensor_id);

	if (remove) data[0] = 0;
	else
	{
		make_pair(dc, 30, PSTR("dev_cla"), dev_class);
		make_pair(ic, 36, PSTR("ic"), icon);
		make_pair(um, 30, PSTR("unit_of_meas"), unit);
		make_pair(tr, 36, NULL, transform);
		make_pair(av, 140, PSTR("avty_t"), mqtt_topic_lwt.c_str());
		if (mqtt_topic_lwt == "-") av[0] = 0;
		snprintf_P(data, sizeof(data),
			PSTR("{'name':'%s','stat_t':'%s','entity_category':'diagnostic',%s'dev':{'ids':['%s']},'unique_id':'%s_%s',%s%s%s'val_tpl':'{{value_json.%s%s}}'}"),
			name, mqtt_topic_inf.c_str(), dc, device_id, device_id, sensor_id, ic, av, um, sensor_id, tr);
		ChangeQuoteSymbol(data);
	}

	//Serial.println(topic);
	//Serial.println(data);
	mqtt->publish(topic, data, true);
}

void MQTT_discover_device(char *id)
{
	char ip[20];
	char topic[60];
	char data[640];

	if (!ini.mqtt_enabled) return;
	if (!mqtt->connected()) return;

	strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));

	snprintf_P(topic, sizeof(topic), PSTR("homeassistant/cover/%s/config"), mqtt_node_id.c_str());

	int clsd;
	make_pair(av, 140, PSTR("avty_t"), mqtt_topic_lwt.c_str());
	if (mqtt_topic_lwt == "-") av[0] = 0;
	if (ini.mqtt_invert) clsd = 0; else clsd = 100;
	snprintf_P(data, sizeof(data),
		PSTR("{'name':'LazyRoll','unique_id':'%s_blind','~':'%s','set_pos_t':'~','pos_t':'%s','stat_t':'%s_HA','cmd_t':'~','dev':{'ids':['%s'],'name':'%s'," \
		"'mdl':'LazyRoll [%s]','mf':'imlazy.ru','cu':'http://%s/settings','sw':'" VERSION "'},'dev_cla':'blind',%s'pos_clsd':%i,'pos_open':%i}"),
		/*ini.hostname,*/ id, mqtt_topic_sub.c_str(), mqtt_topic_pub.c_str(), mqtt_topic_pub.c_str(), id, (ini.name[0] ? ini.name : ini.hostname), ip, ip, av, clsd, 100 - clsd);
	ChangeQuoteSymbol(data);

	//Serial.println(data);
	mqtt->publish(topic, data, true);
}

void MQTT_discover()
{
	char id[17];

	snprintf_P(id, sizeof(id), PSTR("lazyroll%08X"), ESP.getChipId());
	MQTT_discover_device(id);

	// Additional HA sensors
	if (mqtt_topic_inf != "-")
	{
		MQTT_discover_add_sensor(id, F("IP"), F("ip"), NULL, F("mdi:ip-network-outline"), NULL);
		MQTT_discover_add_sensor(id, F("RSSI"), F("rssi"), F("signal_strength"), NULL, F("dBm"));
		MQTT_discover_add_sensor(id, F("voltage"), F("voltage"), F("voltage"), NULL, F("V"));
		MQTT_discover_add_sensor(id, F("Last Restart Time"), F("last_restart_time"), F("timestamp"), F("mdi:clock-time-five-outline"), NULL, false, F(" | timestamp_local | as_datetime"));
		MQTT_discover_add_sensor(id, F("aux"), F("aux"), F("window"), NULL, NULL, true, NULL, ini.aux_pin==0);
		//MQTT_discover_add_sensor(id, F("uptime"), F("uptime"), NULL, F("mdi:clock-time-five-outline"), NULL);
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
		if (val != last_val || last_mqtt==0 || millis()-last_mqtt > 5*60*1000)
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
		if (ini.mqtt_discovery) // open/opening/closing/closed/stopped
		{
			enum { stOpen, stOpening, stClosed, stClosing, stStopped };
			uint8_t state;
			static uint8_t last_state;
			state = stStopped;
			if (roll_to < position) state = (ini.sw_at_bottom ? stClosing : stOpening);
			if (roll_to > position) state = (ini.sw_at_bottom ? stOpening : stClosing);
			if (roll_to == position)
			{
				state = (stStopped);
				if (Position2Percents(position) ==   0) state = stOpen;
				if (Position2Percents(position) == 100) state = stClosed;
			}
			if (state != last_state)
			{
				char buf[10];
				buf[0] = 0;
				last_state = state;
				switch (state)
				{
					case stOpen:    strcpy_P(buf, PSTR("open"));    break;
					case stOpening: strcpy_P(buf, PSTR("opening")); break;
					case stClosed:  strcpy_P(buf, PSTR("closed"));  break;
					case stClosing: strcpy_P(buf, PSTR("closing")); break;
					case stStopped: strcpy_P(buf, PSTR("stopped")); break;
				}
				mqtt->publish((mqtt_topic_pub + "_HA").c_str(), buf);
			}
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
			sprintf_P(buf, PSTR("{\"ip\":\"%d.%d.%d.%d\",\"rssi\":\"%d\",\"uptime\":\"%s\",\"voltage\":\"%s\",\"aux\":\"%s\",\"last_restart_time\":%d}"),
				ip[0], ip[1], ip[2], ip[3], WiFi.RSSI(), UptimeStr().c_str(), GetVoltageStr(), aux_state_str(), last_restart_time);
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

int8_t IRAM_ATTR getEncoder()
{
	uint8_t encA, encB;
	static uint8_t oldA, oldB;
	int8_t pos_change = 0;
	encA = ((GPI >> ((PIN_C) & 0xF)) & 1);
	encB = ((GPI >> ((PIN_D) & 0xF)) & 1);
	if (oldA != encA) (encA != encB ? pos_change++ : pos_change--);
	if (oldB != encB) (encA == encB ? pos_change++ : pos_change--);
	oldA = encA;
	oldB = encB;
	if (ini.reversed) pos_change = 0 - pos_change;
	return pos_change;
}

#define MAX_SPEED 1000
void IRAM_ATTR timer1Isr()
{
	static uint8_t step=0;
	static uint16_t speed=0;
	static uint16_t skip=0;
	bool dir_up;

	if (pwm_LED)
	{
		static uint8_t pwm = 0;
		( pwm >= pwm_LED ? GPOC = 1 << PIN_LED : GPOS = 1 << PIN_LED );
		pwm-=2;
	}

	if (ini.pinout == PINOUT_SD) GPOC = 1 << PIN_ST; // Finish previous step

	if (skip>0)
	{
		// this skip used to reduce total time in ISR.
		skip--;
		return;
	}

	if (position==roll_to)
	{
		MotorOff();
		speed = 0;
		skip = 10;
		if (MOTOR_WITH_ENC)
		{
			position += getEncoder();
			roll_to = position;
			skip = 3;
		}
		return; // stopped, do nothing
	}

	dir_up=(roll_to < position); // up - true

	if (position==0 && !dir_up) switch_ignore_steps=ini.switch_ignore_steps;
	if (dir_up) switch_ignore_steps=0;

	if ((IsSwitchPressed() || virtual_endstop_hit) && !TestMode)
	{
		virtual_endstop_hit = 0;
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

	if (!dir_up && IsSwitch2Pressed() && !TestMode)
	{
		endstop_hit_pos = position;
		endstop_hit = EL_INFO; // Set flag, will add event in Log, but not in ISR
		roll_to = position = ini.full_length; // end point found
		MotorOff();
		return;
	}

	int8_t pos_change = 0;
	switch (ini.pinout)
	{
		case PINOUT_DC_ENC:
		case PINOUT_DC_PWM_ENC:
			// encoder based position
			uint8_t encA, encB;
			static uint8_t oldA, oldB;
			encA = ((GPI >> ((PIN_C) & 0xF)) & 1);
			encB = ((GPI >> ((PIN_D) & 0xF)) & 1);
			if (oldA != encA) (encA != encB ? pos_change++ : pos_change--);
			if (oldB != encB) (encA == encB ? pos_change++ : pos_change--);
			oldA = encA;
			oldB = encB;
			if (ini.reversed) pos_change = 0 - pos_change;
			skip = 1;
			break;
		case PINOUT_DC:
		case PINOUT_DC_PWM:
			// DC motor without encoder, time based position
			static uint32_t last = 0;
			uint32_t ms;
			ms = millis();
			if (last != ms)
			{
				last = ms;
				if (dir_up) pos_change--; else pos_change++;
			}
			skip = 5;
			break;
		default: // all steppers
			if (dir_up)
			{
				if (step == 0) pos_change--;
				step = (step - 1) & 0x07;
			} else
			{
				step = (step + 1) & 0x07;
				if (step == 0) pos_change++;
			}
			if (speed < MAX_SPEED)
			{
				skip = ticks_per_step - 1 + 3 * ticks_per_step * (MAX_SPEED - speed) / MAX_SPEED;
				speed += (MAX_SPEED - speed) / 200 + 1;
			} else skip = ticks_per_step - 1;
	}
	if (!dir_up && pos_change !=0 && switch_ignore_steps>0) switch_ignore_steps--;
	position += pos_change;

	if (MOTOR_WITH_PWM)
	{
		if (pwm_phase_high_ticks == 0) GPOC = 1 << PIN_PWM; else // pwm 0%
		if (pwm_phase_low_ticks  == 0) GPOS = 1 << PIN_PWM; else // pwm 100%
			EnablePWM(true);
	}

	switch (ini.pinout)
	{
		case PINOUT_SD: // step/dir
			GPOC = 1 << PIN_EN; // active - low
			if (dir_up ^ ini.reversed) GPOS = 1 << PIN_DR; else GPOC = 1 << PIN_DR;
			GPOS = 1 << PIN_ST;
		break;
		case PINOUT_DC: // DC motor
		case PINOUT_DC_PWM:
		case PINOUT_DC_ENC:
		case PINOUT_DC_PWM_ENC:
			if (dir_up ^ ini.reversed)
			{
				GPOS = 1 << PIN_A;
				GPOC = 1 << PIN_B;
			} else
			{
				GPOS = 1 << PIN_B;
				GPOC = 1 << PIN_A;
			}
		break;
		default: // stepper
			GPOS = step_s[step % 8];
			GPOC = step_c[step % 8];
		break;
	}
}

#define TIMER1_TICKS_PER_US (APB_CLK_FREQ / 1000000L / 16)
#define pwm_ticks 3500 // 3500 gives ~20 kHz frequency
void AdjustTimerInterval(int step_delay_mks /*= 0*/)
{
	uint16_t l, h;
	uint32_t s;

	if (step_delay_mks == 0) step_delay_mks = ini.step_delay_mks;
	if (step_delay_mks == SPEED2) step_delay_mks = ini.step_delay_mks2;

	if (MOTOR_TYPE_DC)
	{ // Motor without feedback. Step = millisecond
		timer1_write((uint32_t)50*TIMER1_TICKS_PER_US); // 20 kHz
	}
	else
	{
		ticks_per_step = (uint32_t)step_delay_mks / 25;
		if (ticks_per_step < 4) ticks_per_step = 4; // at least 4 timer ticks per step
		if (ticks_per_step > 40) ticks_per_step = 40; // at max 40
		timer1_write((uint32_t)step_delay_mks * TIMER1_TICKS_PER_US / ticks_per_step);
	}

	if (MOTOR_WITH_PWM)
	{
		s = step_delay_mks;
		if (s >= 100) s = 100;

		if (s > 0 && s < 10) s = 10; // not less 10% or 0%
		if (s > 90 && s < 100) s = 90; // not more 90% or 100%

		h = s * pwm_ticks / 100;
		if (h < pwm_ticks / 10) h = 0;
		if (h > pwm_ticks * 9 / 10) h = pwm_ticks;
		l = pwm_ticks - h;

		pwm_phase_low_ticks = l;
		pwm_phase_high_ticks = h;
	} else
		pwm_phase_low_ticks = 0;
}

void IRAM_ATTR timer0Isr(void *para, void *frame)
{
	static uint8_t pwm_phase = 0;
	(void) para;
	(void) frame;
	pwm_phase = !pwm_phase;
	if (pwm_phase)
	{
		GPOC = 1 << PIN_PWM;
		timer0_write(ESP.getCycleCount() + pwm_phase_low_ticks);
	} else {
		GPOS = 1 << PIN_PWM;
		timer0_write(ESP.getCycleCount() + pwm_phase_high_ticks);
	}
}

void SetupTimer()
{
	timer1_isr_init();
	timer1_attachInterrupt(timer1Isr);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	AdjustTimerInterval();
	noInterrupts();
	ETS_CCOMPARE0_INTR_ATTACH(timer0Isr, NULL);
	EnablePWM(false);
	interrupts();
}

void StopTimer()
{
	timer1_disable();
}

// ==================== button =======================================

#define LONG_PRESS_MS 500
#define SHORT_PRESS_MS 50
#define DOUBLE_CLICK_MS 3000
#define AUX_DEBOUNCE_MS 50

uint8_t pin_btn1, button1, pin_btn2, button2;
enum BTN_STATES { NO_PRESS, SHORT_PRESS, LONG_PRESS };
void ICACHE_RAM_ATTR btn1ISR()
{
	static uint32_t lastChange=0;
	static uint8_t lastState=0;

	uint32_t now;
	uint8_t state = digitalRead(pin_btn1);
	if (pin_btn1 != 15) state = !state; // active low for all pins, except GPIO15
	if (state == lastState) return; // ignore duplicate readings

	now=millis();
	if (!state)
	{
		if (now - lastChange >= LONG_PRESS_MS) button1=LONG_PRESS;
		else if (now - lastChange >= SHORT_PRESS_MS) button1=SHORT_PRESS;
	}
	lastChange = now;
	lastState = state;
}

void ICACHE_RAM_ATTR btn2ISR()
{
	static uint32_t lastChange=0;
	static uint8_t lastState=0;

	uint32_t now;
	uint8_t state = digitalRead(pin_btn2);
	if (pin_btn2 != 15) state = !state; // active low for all pins, except GPIO15
	if (state == lastState) return; // ignore duplicate readings

	now=millis();
	if (!state)
	{
		if (now - lastChange >= LONG_PRESS_MS) button2=LONG_PRESS;
		else if (now - lastChange >= SHORT_PRESS_MS) button2=SHORT_PRESS;
	}
	lastChange = now;
	lastState = state;
}

void setup_Button()
{
	detachInterrupt(0);
	detachInterrupt(2);
	detachInterrupt(3);
	detachInterrupt(15);

	if (ini.btn1_pin)
	{
		pin_btn1 = pin2hw_pin(ini.btn1_pin);
		if (pin_btn1 != 15) pinMode(pin_btn1, INPUT_PULLUP); // GPIO15 is inverted, no pull up needed
		attachInterrupt(digitalPinToInterrupt(pin_btn1), btn1ISR, CHANGE);
	}
	if (ini.btn2_pin)
	{
		pin_btn2 = pin2hw_pin(ini.btn2_pin);
		if (pin_btn2 != 15) pinMode(pin_btn2, INPUT_PULLUP); // GPIO15 is inverted, no pull up needed
		attachInterrupt(digitalPinToInterrupt(pin_btn2), btn2ISR, CHANGE);
	}
	if (ini.end2_pin)
	{
		uint8_t pin_end2 = pin2hw_pin(ini.end2_pin);
		if (pin_end2 != 15) pinMode(pin_end2, INPUT_PULLUP); // GPIO15 is inverted, no pull up needed
	}

#if MQTT
	aux_state = AUX_NONE;
	if (ini.aux_pin)
	{
		pin_aux = pin2hw_pin(ini.aux_pin);
		if (pin_aux != 15) pinMode(pin_aux, INPUT_PULLUP); // GPIO15 is inverted, no pull up needed
		attachInterrupt(digitalPinToInterrupt(pin_aux), auxISR, CHANGE);
		auxISR();
	}
#endif

#if RF
	if (ini.rf_pin)
	{
		int pin_rf = pin2hw_pin(ini.rf_pin);
	if (pin_rf != 15) pinMode(pin_rf, INPUT_PULLUP); // GPIO15 is inverted, no pull up needed
		mySwitch.enableReceive(pin_rf); //запускаем RC приемник на gpio XX
	}
#endif
}

#define BTN_ACTIONS FLF("1,'Open',2,'Open/stop',3,'Close',4,'Close/stop',5,'Change',6,'Change/stop',7,'Stop',8,'Change/stop/reverse'," \
	"9,'Preset 1',10,'Preset 2',11,'Preset 3',12,'Preset 4',13,'Preset 5',14,'Preset 1/2'", \
	"1,'Открыть',2,'Открыть/стоп',3,'Закрыть',4,'Закрыть/стоп',5,'Наоборот',6,'Наоборот/стоп',7,'Стоп',8,'Наоборот/стоп/обратно'," \
	"9,'Позиция 1',10,'Позиция 2',11,'Позиция 3',12,'Позиция 4',13,'Позиция 5',14,'Позиция 1/2'")
enum eBTN_ACTIONS { BA_OPEN = 1, BA_OPEN_STOP, BA_CLOSE, BA_CLOSE_STOP, BA_CHANGE, BA_CHANGE_STOP, BA_STOP, BA_AUTO, BA_P1, BA_P2, BA_P3, BA_P4, BA_P5, BA_P1_P2 };

void ButtonAction(uint8_t action, uint8_t addr_bitmap, uint8_t address, uint8_t uart_cmd, bool speed2)
{
	static uint32_t lastClick = 0;
	static uint8_t lastCommand = 0;
	uint8_t open;
	bool slave[1 + MAX_SLAVE] = { 0, 0, 0, 0, 0, 0 }; // master([0]) and slaves([1 - MAX_SLAVE])

	if (MASTER)
	{
		if (address == ADDR_DEFAULT) // from settings
		{
			uint8_t mask;
			mask = 0x01;
			for (uint8_t i=0; i<=MAX_SLAVE; i++)
			{
				slave[i] = addr_bitmap & mask;
				mask = mask << 1;
			}
		}
		if (address == ADDR_ALL)
			for (uint8_t i=0; i<=MAX_SLAVE; i++) slave[0] = 1;
		if (address == ADDR_MASTER) slave[0] = 1;
		if ((address != ADDR_MASTER) && (address <= MAX_SLAVE)) slave[address] = 1;
	} else slave[0] = 1;

	if (!slave[0])
	{ // if master is not selected, we cannot dublicate action to slaves. We will send click/dblclick
		for (address=1; address <= MAX_SLAVE; address++)
			if (slave[address])
				SendUART(uart_cmd, address);
	} else
	{
		bool in_motion = roll_to != position;
		if (action == BA_OPEN_STOP) action = (in_motion ? BA_STOP : BA_OPEN);
		if (action == BA_CLOSE_STOP) action = (in_motion ? BA_STOP : BA_CLOSE);
		if (action == BA_CHANGE_STOP) action = (in_motion ? BA_STOP : BA_CHANGE);
		if (in_motion)
		{
			if (action == BA_P1 ||
				action == BA_P2 ||
				action == BA_P3 ||
				action == BA_P4 ||
				action == BA_P5 ||
				action == BA_P1_P2) action = BA_STOP;
		}
		if (action == BA_AUTO)
		{
			if (in_motion)
			{
				action = BA_STOP;
				lastClick = millis();
			} else
			{
				if (millis() - lastClick < DOUBLE_CLICK_MS)
					open = !lastCommand; // invert direction on double click
				else
				{
					open = position > ini.full_length/2;
					if (ini.sw_at_bottom) open = !open;
				}
				action = (open ? BA_OPEN : BA_CLOSE);
				lastCommand = open;
			}
		}
		if (action == BA_CHANGE)
		{
			if (ini.sw_at_bottom)
				action = (position > ini.full_length/2 ? BA_CLOSE : BA_OPEN);
			else
				action = (position > ini.full_length/2 ? BA_OPEN : BA_CLOSE);
		}
		if (action == BA_P1_P2) action = (position == ini.preset[0] ? BA_P2 : BA_P1);

		for (address=0; address <= MAX_SLAVE; address++) if (slave[address])
		{
			int spd = 0;
			if (speed2) spd = SPEED2;
			if (action == BA_OPEN) { Open(address, spd); } else
			if (action == BA_CLOSE) { Close(address, spd); } else
			if (action == BA_STOP) { Stop(address); } else
			if (action == BA_P1) { ToPreset(1, address, spd); } else
			if (action == BA_P2) { ToPreset(2, address, spd); } else
			if (action == BA_P3) { ToPreset(3, address, spd); } else
			if (action == BA_P4) { ToPreset(4, address, spd); } else
			if (action == BA_P5) { ToPreset(5, address, spd); }
		}
	}
}

void ButtonClick(uint8_t btnnumber=1, uint8_t address=ADDR_DEFAULT)
{
	if (btnnumber == 1)
		ButtonAction(ini.btn1_click & 0x7F, ini.btn1_c_addr, address, UART_CMD_CLICK, ini.btn1_click >= 0x80);
	else
		ButtonAction(ini.btn2_click & 0x7F, ini.btn2_c_addr, address, UART_CMD_CLICK2, ini.btn1_click >= 0x80);
}

void ButtonLongClick(uint8_t btnnumber=1, uint8_t address=ADDR_DEFAULT)
{
	if (btnnumber == 1)
		ButtonAction(ini.btn1_long & 0x7F, ini.btn1_l_addr, address, UART_CMD_LONGCLICK, ini.btn1_long >= 0x80);
	else
		ButtonAction(ini.btn2_long & 0x7F, ini.btn2_l_addr, address, UART_CMD_LONGCLICK2, ini.btn2_long >= 0x80);
}

void process_Button()
{
	if (button1 != NO_PRESS)
	{
		if (button1 == SHORT_PRESS) { ButtonClick(1); elog.Add(EI_Cmd_Click, EL_INFO, ES_BUTTON); }
		if (button1 == LONG_PRESS) { ButtonLongClick(1); elog.Add(EI_Cmd_LClick, EL_INFO, ES_BUTTON); }
		button1=NO_PRESS;
		if (led_mode == LED_BUTTON) LED_Blink();
	}
	if (button2 != NO_PRESS)
	{
		if (button2 == SHORT_PRESS) { ButtonClick(2); elog.Add(EI_Cmd_Click2, EL_INFO, ES_BUTTON); }
		if (button2 == LONG_PRESS) { ButtonLongClick(2); elog.Add(EI_Cmd_LClick2, EL_INFO, ES_BUTTON); }
		button2=NO_PRESS;
		if (led_mode == LED_BUTTON) LED_Blink();
	}
}

void process_Aux()
{
#if MQTT
	static uint8_t last_aux_state = AUX_NONE;
	if (aux_state == last_aux_state) return;
	last_aux_state = aux_state;
	MQTT_ReportAux(aux_state == AUX_ON);
#endif
}

#define BTN_SPEED FLF("0,'Default',1,'Speed 2'", "0,'Обычная',1,'Скорость 2'")

// ===================== RF remote =============================

#if RF
unsigned long last_rf_code = 0;
bool rf_repeat = 0;

// 0, 'None', 101, 'Open', 20, '20%', 40, '40%', 60, '60%', 80, '80%', 100, 'Close', 111, 'Preset 1', 112, 'Preset 2', 113, 'Preset 3',
// 114, 'Preset 4', 115, 'Preset 5',102, 'Open/Close', 103, 'Stop', 104, 'Blink',
// 105, 'Button 1 Click', 106, 'Button 2 Click', 107, 'Button 1 Long', 108, 'Button 2 Long'

void RF_Action(uint8_t action, uint8_t flags)
{
	int speed;
	if (!action) return;
	if (roll_to != position && rf_repeat && (flags & RF_FLAG_STOP2ND))
	{ // stop on second click
		Stop();
		elog.Add(EI_Cmd_Stop, EL_INFO, ES_RF);
		return;
	}
	speed = 0;
	if (action >= 0x80) speed = SPEED2;
	action = action & 0x7F;
	if (action == 101) { Open(ADDR_ALL, speed); elog.Add(EI_Cmd_Open, EL_INFO, ES_RF); } else
	if (action == 100) { Close(ADDR_ALL, speed); elog.Add(EI_Cmd_Close, EL_INFO, ES_RF); } else
	if (action == 102) { ButtonAction(BA_AUTO, 0, ADDR_ALL, 0, speed); elog.Add(EI_Auto, EL_INFO, ES_RF); } else
	if (action == 105) { ButtonClick(1, ADDR_DEFAULT); elog.Add(EI_Cmd_Click, EL_INFO, ES_RF); } else
	if (action == 106) { ButtonClick(2, ADDR_DEFAULT); elog.Add(EI_Cmd_Click2, EL_INFO, ES_RF); } else
	if (action == 107) { ButtonLongClick(1, ADDR_DEFAULT); elog.Add(EI_Cmd_LClick, EL_INFO, ES_RF); } else
	if (action == 108) { ButtonLongClick(2, ADDR_DEFAULT); elog.Add(EI_Cmd_LClick2, EL_INFO, ES_RF); } else
	if (action == 103) { Stop(ADDR_ALL); elog.Add(EI_Cmd_Stop, EL_INFO, ES_RF); } else
	if (action == 104) LED_Blink(); else
	if (action > 0 && action < 100) { ToPercent(action, ADDR_ALL, speed); elog.Add(EI_Cmd_Percent, EL_INFO, ES_RF); } else
	if (action > 110 && action <= 110+MAX_PRESETS) { ToPreset(action-110, ADDR_ALL, speed); elog.Add(EI_Cmd_Preset, EL_INFO, ES_RF); }
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
			Serial.print("Unknown encoding");
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
		{
			ini.rf_cmd[c].action = atoi(httpServer.arg("rfa"+n).c_str()) & 0x7F;
			if (atoi(httpServer.arg("rfw"+n).c_str())) ini.rf_cmd[c].action |= 0x80;
		}
	}

	SaveSettings(&ini, sizeof(ini));
}

String RF_save()
{
	char buf[250];
	snprintf_P(buf, sizeof(buf),
		PSTR("<tr class='sect_name'><td colspan='2'>\n<input id='save' type='submit' name='save' value='%s'\n>" \
		"<input id='cancel' type='button' name='cancel' onclick='RFCancel();' value='%s'>\n</td></tr>\n"),
		FLF("Save", "Сохранить"), FLF("Cancel", "Отмена"));
	ChangeQuoteSymbol(buf);
	return String(buf);
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
		out += F("<tr><td></td><td>\n<select id=\"rfw");
		out += i;
		out += F("\" name=\"rfw");
		out += i;
		out += F("\"></select></td></tr>\n");
	}

	out += F("<script>\nconst opts1 = ");
	out += FLF("[0,'None',101,'Open',20,'20%',40,'40%',60,'60%',80,'80%',100,'Close',111,'Preset 1',112,'Preset 2',113,'Preset 3',114,'Preset 4',115,'Preset 5'," \
		"102,'Open/Close',103,'Stop',104,'Blink',105,'Button 1',106,'Button 2',107,'Button 1 Long',108,'Button 2 Long'];\n",
			   "[0,'Нет',101,'Открыть',20,'20%',40,'40%',60,'60%',80,'80%',100,'Закрыть',111,'Пресет 1',112,'Пресет 2',113,'Пресет 3',114,'Пресет 4',115,'Пресет 5'," \
		"102,'Открыть/Закрыть',103,'Стоп',104,'Мигнуть',105,'Кнопка 1',106,'Кнопка 2',107,'Кнопка 1 длинное',108,'Кнопка 2 длинное'];\n");
	out += F("const spds = [");
	out += BTN_SPEED;
	out += F("];\n");
	for (int i=0; i < MAX_RF_CMDS; i++)
	{
		out += F("AddOption('rfa");
		out += i;
		out += F("', opts1, ");
		out += ini.rf_cmd[i].action & 0x7f;
		out += F(");AddOption('rfw");
		out += i;
		out += F("', spds, ");
		out += (ini.rf_cmd[i].action >= 0x80 ? F("1") : F("0"));
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
	if (WiFi_AP_disabled) return;
	Serial.print(F("Starting access point. SSID: "));
	Serial.println(ini.hostname);
	elog.Add(EI_Wifi_Start_AP, EL_WARN, 0);
	WiFi.mode(WIFI_AP);
	if (!ini.ap_pswd[0])
		WiFi.softAP(ini.hostname); // ... but without password
	else
		WiFi.softAP(ini.hostname, ini.ap_pswd); // or with password
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
			ping_fails = 0;
			elog.Add(EI_Wifi_Got_IP, EL_INFO, (uint32_t)WiFi.localIP());
			LED_Off();
			CP_delete();
			WiFi_AP_disabled = true; // Disabling AP mode until next reboot
			WiFi_attempts = 0;
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
			WiFi_AP_disabled = true; // Disabling AP mode until next reboot
			WiFi_attempts = 0;
			WiFi.setSleepMode(WIFI_NONE_SLEEP);

			Serial.println(WiFi.localIP());
			ping_fails = 0;
			elog.Add(EI_Wifi_Got_IP, EL_INFO, (uint32_t)WiFi.localIP());
			if (Master_IP) ReportToMaster();

#if MDNSC
			if (!MDNS.begin(ini.hostname)) Serial.println(F("Error setting up MDNS responder!"));
			else Serial.println(F("mDNS responder started"));
			MDNS.addService("http", "tcp", 80);
#endif
		}
	}

	if (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_IDLE_STATUS && WiFi.status() != WL_DISCONNECTED)
	{
		if (SSID_NOT_EMPTY && (WiFi_AP_disabled || WiFi_attempts < MAX_RECONNECT_ATTEMPS))
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
	ping_fails = 0;
	WiFi.mode(WIFI_STA);
	WiFi.hostname(ini.hostname);
	setup_IP();
	if (SSID_NOT_EMPTY)
		WiFi.begin(ini.ssid, ini.password);
	else
		StartSoftAP();
	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
	{
		if (WiFi_connected)
		{
			WiFi_connected = false;
			Serial.println(F("Disconnected"));
			elog.Add(EI_Wifi_Disconnect, EL_ERROR, 0);
			//delay(500);
			//WiFi_On();
			if (WiFi_active) WiFi.begin(ini.ssid, ini.password);
		}
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
	if (ini.pinout<0 || ini.pinout>=MAX_PINOUT) ini.pinout=0;
	if (MOTOR_WITH_PWM)
	{
		if (ini.step_delay_mks > 100) ini.step_delay_mks = 100;
		if (ini.step_delay_mks2> 100) ini.step_delay_mks2= 100;
	}
	else
	{
		if (ini.step_delay_mks < MIN_STEP_DELAY) ini.step_delay_mks = MIN_STEP_DELAY;
		if (ini.step_delay_mks2< MIN_STEP_DELAY) ini.step_delay_mks2= MIN_STEP_DELAY;
		if (ini.step_delay_mks >= 65000) ini.step_delay_mks = def_step_delay_mks;
		if (ini.step_delay_mks2>= 65000) ini.step_delay_mks2= def_step_delay_mks;
	}
	if (ini.timezone<-11*60 || ini.timezone>=14*60) ini.timezone=0;
	if (ini.full_length<300 || ini.full_length>999999) ini.full_length=10000;
	if (ini.switch_reversed>1) ini.switch_reversed=1;
	if (ini.switch2_reversed>1) ini.switch2_reversed=1;
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
	if (ini.hostname[0] == 0) sprintf_P(ini.hostname , def_hostname, ESP.getChipId() & 0xFFFFFF);
	if (ini.mqtt_state_type>3) ini.mqtt_state_type=0;
	if (ini.led_mode >= LED_MODE_MAX) ini.led_mode=0;
	if (ini.led_level >= LED_LEVEL_MAX) ini.led_level=0;
	for (int i=0; i<MAX_PRESETS; i++)
		if (ini.preset[i] > ini.full_length) ini.preset[i] = ini.full_length;
	if (!ini.btn1_c_addr) ini.btn1_c_addr = 1;
	if (!ini.btn1_l_addr) ini.btn1_l_addr = 1;
	if (!ini.btn2_c_addr) ini.btn2_c_addr = 1;
	if (!ini.btn2_l_addr) ini.btn2_l_addr = 1;
	if (!ini.btn1_click) ini.btn1_click = BA_AUTO;
	if (!ini.btn1_long) ini.btn1_long = BA_P1_P2;
	if (!ini.btn2_click) ini.btn2_click = BA_AUTO;
	if (!ini.btn2_long) ini.btn2_long = BA_P1_P2;
	for (int i=0; i<ALARMS; i++)
		if (ini.alarms[i].m_s == 0) ini.alarms[i].m_s = 0xFF; // master & all slaves
	if (strlen(ini.ap_pswd) < 8) ini.ap_pswd[0] = 0;
}

void setup_Settings(void)
{
	memset(&ini, 0, sizeof(ini));
	ini.up_safe_limit=DEFAULT_UP_SAFE_LIMIT;
	ini.lat = 116924612; // 55.754 Moscow
	ini.lng = 78896955; // 37.621
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
		ini.step_delay_mks2=def_step_delay_mks;
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
	DEBUGV("Chip size %d\n", flashchip->chip_size);
	DEBUGV("Real Chip size %d\n", ESP.getFlashChipRealSize());
	flashchip->chip_size = ESP.getFlashChipRealSize();
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
		WiFi_connected = false;
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
void HTTP_downloadCfg();
void HTTP_uploadCfg();

	//volatile uint32_t i;
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
	httpServer.on("/lazy.cfg", HTTP_GET, HTTP_downloadCfg);
	httpServer.on("/lazy.cfg", HTTP_POST, [](){ httpServer.send(200); }, HTTP_uploadCfg);
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

	SetupMotorPins();
	MotorOff();

	setup_OTA();
	setup_NTP();
	setup_MQTT();

	ResetZero();

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
	"<body onload=\"OnPageLoad();\">\n" \
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
	char buf[250];
	snprintf_P(buf, sizeof(buf), PSTR("<script>edtStr('%s','%s','%s',%d);</script>\n"), header, id, inistr, len);
	return buf;
}

String HTML_editIP(const __FlashStringHelper* header, const __FlashStringHelper* id, ip4_addr* inifield)
{
	char buf[100];
	snprintf_P(buf, sizeof(buf), PSTR("<script>edtIP('%s','%s',%d);</script>\n"), header, id, inifield->addr);
	return buf;
}

String HTML_steps(String lbl, String id, int val, String name)
{
	char buf[100];
	snprintf_P(buf, sizeof(buf), PSTR("<script>edtSteps('%s','%s',%d,'%s','%s','%s');</script>\n"),
		lbl.c_str(), id.c_str(), val, name.c_str(), FLF("Test", "Тест"), FLF("Here", "Тут"));
	return buf;
}

String HTML_hint(const __FlashStringHelper* hint)
{
	char buf[200];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td></td><td>%s</td></tr>\n"), hint);
	return buf;
}

String HTML_hint(const __FlashStringHelper* hint, const __FlashStringHelper* id)
{
	char buf[200];
	snprintf_P(buf, sizeof(buf), PSTR("<tr id=\"%s\"><td></td><td>%s</td></tr>\n"), id, hint);
	return buf;
}

String HTML_hint(const String &hint)
{
	char buf[200];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td></td><td>%s</td></tr>\n"), hint.c_str());
	return buf;
}

String HTML_test(const __FlashStringHelper* name, int n)
{
	char buf[300];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td>%s:</td><td>\n" \
		"<input id='btn_up%i' type='button' name='up' value='%s' onclick='TestUp(%i);'>\n" \
		"<input id='btn_dn%i' type='button' name='down' value='%s' onclick='TestDown(%i);'>\n</td></tr>\n"),
		name,
		n, FLF("Test up", "Тест вверх"), n,
		n, FLF("Test down", "Тест вниз"), n);
	ChangeQuoteSymbol(buf);
	return buf;
}

String HTML_speed(const char* n1, const char* n2, int i)
{
	char buf[300];
	snprintf_P(buf, sizeof(buf),
		PSTR("<tr id='po_pwm%s'><td>%s%s:</td><td><input type='range' min='0' max='100' value='%i' class='slider' id='pwm%s' name='pwm%s' onchange='PwmChange();'>" \
			"<label id='pwm_num%s'>100%</label></td></tr>\n"),
		n2, FLF("Speed", "Скорость"), n1, i, n2, n2, n2);
	ChangeQuoteSymbol(buf);
	return buf;
}

String HTML_addOption(int value, int selected, const __FlashStringHelper *text, const char *id = NULL)
{
	char ids[20] = "";
	char buf[200];
	if (id) snprintf_P(ids, sizeof(ids), PSTR(" id=\"%s\""), id);
	snprintf_P(buf, sizeof(buf), PSTR("<option value=\"%d\"%s%s>%s</option>\n"), value, (selected==value ? F(" selected=\"selected\"") : F("")), ids, text);
	return buf;
}

String HTML_section(const __FlashStringHelper* section)
{
	char buf[100];
	snprintf_P(buf, sizeof(buf), PSTR("<tr class=\"sect_name\"><td colspan=\"2\">%s</td></tr>\n"), section);
	return buf;
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
		out += HTML_tableLine(L("Time", "Время"), TimeStr() + " [" + DoWName(DayOfWeek(getTime())) +"]", "time");

	out += HTML_tableLine(L("Uptime", "Аптайм"), UptimeStr(), "uptime");
	out += HTML_tableLine(L("RSSI", "RSSI"), String(WiFi.RSSI())+SL(" dBm", " дБм"), "RSSI");
	if (voltage_available)
		out += HTML_tableLine(L("Power", "Питание"), GetVoltageStr()+SL("V", "В"), "voltage");
	out += HTML_tableLine(L("<a href=\"/log\" onclick=\"return ShowLog();\">Log</a>",
		"<a href=\"/log\" onclick=\"return ShowLog();\">Лог</a>"), String((int)elog.Count()), "log_count");

	out += HTML_section(FLF("Position", "Положение"));
	out += HTML_tableLine(L("Now", "Сейчас"), String(position), "pos");
	out += HTML_tableLine(L("Roll to", "Цель"), String(roll_to), "dest");
	out += HTML_tableLine(L("Switch", "Концевик"), onoff[ini.lang][IsSwitchPressed()], "end1");
	if (ini.end2_pin)
		out += HTML_tableLine(L("Switch 2", "Концевик 2"), onoff[ini.lang][IsSwitch2Pressed()], "end2");

	out += HTML_section(FLF("Memory", "Память"));
	out += HTML_tableLine(L("Flash id", "ID чипа"), String(ESP.getFlashChipId(), HEX));
	out += HTML_tableLine(L("Real size", "Реально"), MemSize2Str(realSize));
	out += HTML_tableLine(L("IDE size", "Прошивка"), MemSize2Str(ideSize));
	if (ideSize != realSize) {
		out += HTML_tableLine(L("Config", "Конфиг"), L("error!", "ошибка!"));
		mem_problem = 1;
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
		if (fs_info.totalBytes < 10240) mem_problem = 1;
		out += HTML_tableLine(L("Size", "Выделено"), MemSize2Str(fs_info.totalBytes));
		out += HTML_tableLine(L("Used", "Занято"), MemSize2Str(fs_info.usedBytes));
	} else
	{
		mem_problem = 1;
		out += HTML_tableLine(L("Error", "Ошибка"), "<a href=\"/format\">"+SL("Format", "Формат-ть")+"</a>");
	}
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

	out += F("<section class=\"main\" id=\"main\">");
	if (mem_problem)
		out += FLF("<p style=\"color:red;\">Incorrect memory size selected. Please use correct firmware or build with at least 16K FS</p>\n",
			"<p style=\"color:red;\">Неверный размер памяти. Используйте подходящую прошивку или откомпилируйте с минимумом 16K FS</p>\n");
	out += F("<table>");
	if (MASTER)
	{
		out += F("<tr><td colspan=\"2\"><ul class=\"addr\">");
		for (int8_t i=-1; i<=MAX_SLAVE; i++)
		{
			out += F("<li id=\"ms_");
			if (i<0) out += F("all"); else out += i;
			out += F("\"><a onclick=\"return BtnMS(");
			out += i;
			out += F(");\">");
			if (i<0) out += FLF("All", "Все");
			if (i==0) out += FLF("This", "Эта");
			if (i>0) out += i;
			out += F("</a></li>\n");
		}
		out += F("</ul>\n</td></tr>\n");
	}
	out += F("<tr><td class=\"presets\"><ul>");
	for (int8_t i=1; i<=MAX_PRESETS; i++)
	{
		out += F("<li id=\"ps_");
		if (i<0) out += F("all"); else out += i;
		out += F("\"><a onclick=\"return Preset(");
		out += i;
		out += F(");\">");
		out += i;
		out += F("</a></li>\n");
	}
	out += F("</ul>\n</td><td class=\"ctrl\"><ul>\n" \
		"<li class=\"menuopen\"><a href=\"open\" onclick=\"return OpenA();\"><div class=\"svg\"></div>[Open]</a>\n" \
		"</li><li class=\"menustop\"><a href=\"stop\" onclick=\"return StopA();\"><div class=\"svg\"></div>[Stop]</a>\n" \
		"</li><li class=\"menuclose\"><a href=\"close\" onclick=\"return CloseA();\"><div class=\"svg\"></div>[Close]</a>\n" \
		"</li></ul></td></tr></table>");

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

void SaveIntA(const __FlashStringHelper *id, uint8_t n, uint8_t *iniint)
{
	String s = id;
	s += String(n);
	if (!httpServer.hasArg(s)) return;
	*iniint=atoi(httpServer.arg(s).c_str());
}

void SaveIntAndBit(const __FlashStringHelper *id, const __FlashStringHelper *id2, uint8_t *iniint)
{
	if (!httpServer.hasArg(id)) return;
	*iniint = atoi(httpServer.arg(id).c_str());
	if (!httpServer.hasArg(id2)) return;
	if (atoi(httpServer.arg(id2).c_str())) *iniint = *iniint + 0x80;
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

void SaveMasterSlave(String id, uint8_t *ini_btn_addr)
{
	uint8_t b = 0;
	for (int d = MAX_SLAVE; d >= 0; d--)
	{
		b=b<<1;
		if (httpServer.hasArg(id+d)) b|=1;
	}
	*ini_btn_addr = b;
}

void HTTP_uploadCfg()
{
	static uint8_t *buf = 0;
	static unsigned int pos = 0;

	HTTPUpload& upload = httpServer.upload();
	if(upload.status == UPLOAD_FILE_START)
	{
		Serial.println(upload.filename);
		buf = (uint8_t*)malloc(sizeof(ini));
		memset(buf, 0, sizeof(ini));
		pos = 0;
	}
	else if (upload.status == UPLOAD_FILE_WRITE)
	{
		if (buf)
		{
			if (pos < sizeof(ini))
				memcpy(&(buf[pos]), upload.buf, min((unsigned int)upload.currentSize, (unsigned int)sizeof(ini) - pos));
			pos += upload.currentSize;
		}
	}
	else if (upload.status == UPLOAD_FILE_END)
	{
		if (buf)
		{
			uint8_t buf2[sizeof(ini.password)];
			Serial.println(pos);
			memcpy(buf2, ini.password, sizeof(ini.password)); // saving current wifi password
			memcpy(&ini, buf, min((unsigned int)sizeof(ini), pos));
			memcpy(ini.password, buf2, sizeof(ini.password)); // restoring current wifi password
			free(buf);
		}
		buf = 0;
		httpServer.sendHeader("Location", "/settings");
		httpServer.send(303);
	}
	else if (upload.status == UPLOAD_FILE_ABORTED)
	{
		if (buf) free(buf);
		buf = 0;
		httpServer.sendHeader("Location", "/update");
		httpServer.send(303);
	}
}

void HTTP_downloadCfg()
{
	uint8_t buf[sizeof(ini.password)];
	memcpy(buf, ini.password, sizeof(ini.password));
	memset(ini.password, 0, sizeof(ini.password));
	httpServer.send(200, "application/force-download", (uint8_t*)&ini, sizeof(ini));
	memcpy(ini.password, buf, sizeof(ini.password));
}

void HTTP_handleUpdate(void)
{
	String out;
	int mem = ESP.getFlashChipRealSize();

	HTTP_Activity();

	out = HTML_header();
	out += F("<section class=\"main\" id=\"main\">\n<p>");
	out += FL(F("Firmware:"), F("Прошивка:"));
	out += F("</p>\n<form method='POST' action='/update2' enctype='multipart/form-data'>" \
		"<input type='file' accept='.bin,.bin.gz' name='firmware'>" \
		"<input type='submit' value='");
	out += FL(F("Update Firmware"), F("Обновить прошивку"));
	out += F("'></form>\n<p>");
	out += FL(F("Choose file for firmware update.<br>New firmware can be downloaded from "),
		F("Выберите файл прошивки (Choose File) для обновления.<br/>Новые прошивки можно скачать тут: "));
	out += F("<a href=\"https://github.com/ACE1046/LazyRolls/tree/master/Firmware\">Github</a>.<br>\n");
	if (mem == 1024*1024)
		out += FL(F("Choose *_1"), F("Выбирайте *_1"));
	if (mem == 4*1024*1024)
		out += FL(F("Choose *_4"), F("Выбирайте *_4"));
	out += FL(F("Mbyte.bin.gz / *_auto.bin.gz.<br>\nSettings will be lost, if downgrading to previous version.<br>Default password admin admin.</p>"),
		F("Mbyte.bin.gz / *_auto.bin.gz.<br>\nНастройки сбрасываются, если прошивается более старая версия.<br>Пароль по умолчанию admin admin.</p>"));
	out += FLF("<hr><p>Information:<br>", "<hr><p>Информация:<br>");
	out += ESP.getFullVersion();
	#ifdef AUTOMEMSIZE
	out += F("</p><p>Auto memory size build");
	#endif
	out += F("</p><p>Free mem for firmware update: ");
	out += ESP.getFreeSketchSpace();

	out += F("</p><hr><p><a href=\"/lazy.cfg\">");
	out += FLF("Download config", "Скачать настройки");
	out += F("</a></p><p><form method='POST' action='/lazy.cfg' enctype='multipart/form-data'><input type='file' accept='.cfg' name='config'><input type='submit' value='");
	out += FLF("Load config", "Загрузить настройки");
	out += F("'></form><br>\n");
	out += FLF("After load, check settings and press 'Save'.<br>Wi-Fi password will not be saved/loaded.",
		"После загрузки, проверьте настройки и нажмите 'Сохранить'.<br>Пароль Wi-Fi не сохраняется и не загружается.");
	out += F("</p></section>\n");

	out += HTML_footer();
	httpServer.send(200, "text/html", out);
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
	if (MOTOR_WITH_PWM)
	{
		SaveInt(F("pwm"), &ini.step_delay_mks);
		SaveInt(F("pwm2"), &ini.step_delay_mks2);
	} else {
		SaveInt(F("delay"), &ini.step_delay_mks);
		SaveInt(F("delay2"), &ini.step_delay_mks2);
	}
	SaveInt(F("timezone"), &ini.timezone);
	SaveInt(F("length"), &ini.full_length);
	SaveInt(F("switch"), &ini.switch_reversed);
	SaveInt(F("switch2"), &ini.switch2_reversed);
	SaveInt(F("sw_at_bottom"), &ini.sw_at_bottom);
	SaveInt(F("switch_ignore"), &ini.switch_ignore_steps);
	SaveInt(F("end2_pin"), &ini.end2_pin);
	SaveInt(F("btn1_pin"), &ini.btn1_pin);
	SaveInt(F("btn2_pin"), &ini.btn2_pin);
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
	SaveInt(F("lat"), &ini.lat);
	SaveInt(F("lng"), &ini.lng);
	SaveIntAndBit(F("btn1_click"), F("btn1_c_spd"), &ini.btn1_click);
	SaveIntAndBit(F("btn1_long"), F("btn1_l_spd"), &ini.btn1_long);
	SaveIntAndBit(F("btn2_click"), F("btn2_c_spd"), &ini.btn2_click);
	SaveIntAndBit(F("btn2_long"), F("btn2_l_spd"), &ini.btn2_long);
	if (MASTER)
	{
		SaveMasterSlave(F("b1c_"), &ini.btn1_c_addr);
		SaveMasterSlave(F("b1l_"), &ini.btn1_l_addr);
		SaveMasterSlave(F("b2c_"), &ini.btn2_c_addr);
		SaveMasterSlave(F("b2l_"), &ini.btn2_l_addr);
	}
	SaveString(F("ap_pswd"), ini.ap_pswd, sizeof(ini.ap_pswd));
	ini.ping_enabled = httpServer.hasArg("ping_en");
	SaveIP(IP_ID("ping_ip"),  &ini.ping_ip);
	SaveInt(F("ping_act"), &ini.ping_act);

	for (int i=0; i < MAX_IP_SLAVE; i++)
	{
		SaveIntA(F("sip"), i, &ini.ip_slaves[i].ip4);
		SaveIntA(F("snm"), i, &ini.ip_slaves[i].num);
	}

	led_mode=ini.led_mode;
	led_level=ini.led_level;

	ValidateSettings();

	ping_fails = 0;

	SaveSettings(&ini, sizeof(ini));
	elog.Add(EI_Settings_Saved, EL_WARN, 0);

	setup_MQTT();
	setup_Button();

	FillStepsTable();
	AdjustTimerInterval();
	SetupMotorPins();
	CalcAlarmTimes();

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
	char buf[500];
	snprintf_P(buf, sizeof(buf), PSTR("<script>const %s=[%s];AddOption('%s', %s, %d);</script>\n"), var, arr, id, var, val);
	return buf;
}

String AddOptions(const __FlashStringHelper *id, const __FlashStringHelper *var, int val)
{
	char buf[100];
	snprintf_P(buf, sizeof(buf), PSTR("<script>AddOption('%s', %s, %d);</script>\n"), id, var, val);
	return buf;
}

String AddMasterSlave(const __FlashStringHelper *id, const __FlashStringHelper *var, const __FlashStringHelper *arr, int val)
{
	char buf[300];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td></td><td id='%s' class='%s'>\n<script>const %s=[%s];AddMasterSlave('%s', %s, %d);</script>\n</td></tr>\n"), id, var, var, arr, id, var, val);
	return buf;
}

String AddMasterSlave(const __FlashStringHelper *id, const __FlashStringHelper *var, int val)
{
	char buf[100];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td></td><td id='%s' class='%s'>\n<script>AddMasterSlave('%s', %s, %d);</script>\n</td></tr>\n"), id, var, id, var, val);
	return buf;
}

String AddIPSlaves()
{
	char buf[160], buf2[100];
	int p, p2;
	buf2[0] = 0;
	p = 0;
	for (int i=0; i<MAX_IP_SLAVE; i++)
	{
		if (i > 0) buf2[p++] = ',';
		p2 = snprintf_P(buf2+p, sizeof(buf2)-p, PSTR("%d,%d"), ini.ip_slaves[i].ip4, ini.ip_slaves[i].num);
		if (p2 > 0) p += p2;
	}
	snprintf_P(buf, sizeof(buf), PSTR("<script>self_ip='%s';SetIPSlaves([%s]);ShowIPSlaves();</script>\n"), WiFi.localIP().toString(), buf2);
	return buf;

}

String HTML_Options(const __FlashStringHelper *name, const __FlashStringHelper *id, const __FlashStringHelper *attr, const __FlashStringHelper *var, const __FlashStringHelper *arr, int val)
{
	String out;
	char buf[500];
	snprintf_P(buf, sizeof(buf), PSTR("<tr><td>%s</td><td><select id='%s' name='%s'%s>\n"), name, id, id, attr);
	out = buf;

	if (arr)
		out += AddOptions(id, var, arr, val);
	else
		out += AddOptions(id, var, val);
	out += F("</select></td></tr>\n");
	return out;
}

void HTTP_handleSettings(void)
{
	String out;
	int i, i2;

	HTTP_Activity();

	if (httpServer.hasArg("save"))
	{
		HTTP_saveSettings();
		HTTP_redirect("/settings?ok=1");
		return;
	}

	out = HTML_header();

	if (!out.reserve(20480)) Serial.println("Low mem in HandleSettings");

	out += F("<section class=\"settings\" id=\"settings\">\n");

	if (httpServer.hasArg("ok"))
		out += FLF("<p>Saved!<br/>Network settings will be applied after reboot.<br/><a href=\"reboot\">[Reboot]</a></p>\n",
			"<p>Сохранено!<br/>Настройки сети будут применены после перезагрузки.<br/><a href=\"reboot\">[Перезагрузить]</a></p>\n");

	out += F("<form method=\"post\" action=\"/settings\" onsubmit=\"return Save();\">\n");

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
	out += HTML_editString(FLF("AP pswd:", "Пароль AP:"),   F("ap_pswd"),  ini.ap_pswd,  sizeof(ini.ap_pswd)-1);
	out += HTML_hint(FLF("Access point password, 8 chars minimum or blank", "Пароль режима точки доступа, 8 символов минимум или пусто"));

	out += HTML_section(FLF("Pinger", "Пингер"));
	out += HTML_addCheckbox(L("Ping IP (0.0.0.0 - gateway)", "Пинговать IP (0.0.0.0 - шлюз)"), "ping_en", ini.ping_enabled);
	out += HTML_editIP(F("IP"), F("ping_ip"), &ini.ping_ip);
	out += HTML_Options(FLF("Action:", "Действие:"), F("ping_act"), F(""), F("p_act"),
		FLF("0,'Log',1,'Reconnect',2,'Reboot'", "0,'Лог',1,'Переподключение',2,'Перезагрузка'"), ini.ping_act);

	out += HTML_section(FLF("Time", "Время"));
	out += HTML_editString(FLF("NTP-server:", "NTP-сервер:"),F("ntp"),     ini.ntpserver,sizeof(ini.ntpserver)-1);
	out += F("<tr><td>");
	out += FLF("Timezone: ", "Пояс: ");
	out += F("</td><td><select id=\"timezone\" name=\"timezone\">\n");
	out += F("</select></td></tr>\n");
	out += F("<script>AddOption('timezone', tzs, ");
	out += ini.timezone;
	out += F(");</script>\n");

	out += HTML_section(FLF("Motor", "Мотор"));
	out += F("<tr><td>");
	out += FLF("Pinout:", "Подключение:");
	out += F("</td><td><select id=\"pinout\" name=\"pinout\" onchange=\"PinoutChange();\">\n");
	out += F("</select></td></tr>\n");
	out += AddOptions(F("pinout"), F("po"), F(
		"2,\"A-B-C-D\"," \
		"0,\"A-C-B-D\"," \
		"1,\"A-B-D-C\"," \
		TOSTRING(PINOUT_SD) ",\"Step/Dir\"," \
		TOSTRING(PINOUT_DC) ",\"DC motor\"," \
		TOSTRING(PINOUT_DC_PWM) ",\"DC + PWM\"," \
		TOSTRING(PINOUT_DC_ENC) ",\"DC + Enc\"," \
		TOSTRING(PINOUT_DC_PWM_ENC) ",\"DC + PWM + Enc\""),
		ini.pinout);
	out += F("<tr><td>");
	out += FLF("Direction:", "Направление:");
	out += F("</td><td><select id=\"reversed\" name=\"reversed\">\n");
	out += HTML_addOption(1, ini.reversed, FLF("Normal", "Прямое"));
	out += HTML_addOption(0, ini.reversed, FLF("Reversed", "Обратное"));
	out += F("</select></td></tr>\n");
	i = ini.step_delay_mks;
	i2 = ini.step_delay_mks2;
	if (MOTOR_WITH_PWM) i = i2 = 1500;
	out += HTML_editString(FLF("Step delay:", "Время шага:"), F("delay"), String(i).c_str(), 5);
	out += HTML_editString(FLF("Step delay 2:", "Время шага 2:"), F("delay2"), String(i2).c_str(), 5);
	i = ini.step_delay_mks;
	i2 = ini.step_delay_mks2;
	if (!MOTOR_WITH_PWM) i = i2 = 100;

	out += HTML_speed(PSTR(""), PSTR(""), i);
	out += HTML_speed(PSTR(" 2"), PSTR("2"), i2);

	out += HTML_hint(FLF("(microsecs, " TOSTRING(MIN_STEP_DELAY) "-65000, default 1500)", "(в мкс, " TOSTRING(MIN_STEP_DELAY) "-65000, обычно 1500)"), F("po_step"));
	out += HTML_hint(FLF("(step = 1 millisecond)", "(шаг = 1 миллисекунда)"), F("po_ms"));
	out += HTML_hint(SL(F("Help:"), F("Помощь:")) + " <a href=\"http://imlazy.ru/rolls/motor.html\">imlazy.ru/rolls/motor.html</a>");
	out += HTML_test(FLF("Normal", "Обычная"), 1);
	out += HTML_test(FLF("Speed 2", "Скорость 2"), 2);

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

	out += HTML_section(FLF("Endstop 2", "Концевик 2"));
	out += F("<tr><td>");
	out += FLF("Pin:", "Пин:");
	out += F("</td><td><select id=\"end2_pin\" name=\"end2_pin\" onchange=\"PinChange()\">\n");
	out += AddOptions(F("end2_pin"), F("pins"), PIN_LIST, ini.end2_pin);
	out += F("</select></td></tr>\n");
	out += F("<tr><td>");
	out += FLF("Type:", "Тип:");
	out += F("</td><td><select id=\"switch2\" name=\"switch2\">\n");
	out += HTML_addOption(0, ini.switch2_reversed, FLF("Normal closed", "Нормально замкнут"));
	out += HTML_addOption(1, ini.switch2_reversed, FLF("Normal open", "Нормально разомкнут"));
	out += F("</select></td></tr>\n");

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

#define MASTER_AND_SLAVES FLF("'Master','S1','S2','S3','S4','S5'", "'Главный','В1','В2','В3','В4','В5'")
#define BTN_SPEED FLF("0,'Default',1,'Speed 2'", "0,'Обычная',1,'Скорость 2'")
	out += HTML_section(FLF("Button 1", "Кнопка 1"));
	out += HTML_Options(FLF("Pin:", "Пин:"), F("btn1_pin"), F(" onchange='PinChange()'"), F("pins"), NULL, ini.btn1_pin);
	out += HTML_Options(FLF("Click:", "Клик:"), F("btn1_click"), F(""), F("act"), BTN_ACTIONS, ini.btn1_click & 0x7F);
	out += HTML_Options(F(""), F("btn1_c_spd"), F(""), F("spd"), BTN_SPEED, ini.btn1_click >= 0x80);
	out += AddMasterSlave(F("b1c"), F("m_s"), MASTER_AND_SLAVES, ini.btn1_c_addr);
	out += HTML_Options(FLF("Long:", "Долгий:"), F("btn1_long"), F(""), F("act"), NULL, ini.btn1_long & 0x7F);
	out += HTML_Options(F(""), F("btn1_l_spd"), F(""), F("spd"), NULL, ini.btn1_long >= 0x80);
	out += AddMasterSlave(F("b1l"), F("m_s"), /*MASTER_AND_SLAVES,*/ ini.btn1_l_addr);

	out += HTML_section(FLF("Button 2", "Кнопка 2"));
	out += HTML_Options(FLF("Pin:", "Пин:"), F("btn2_pin"), F(" onchange='PinChange()'"), F("pins"), NULL, ini.btn2_pin);
	out += HTML_Options(FLF("Click:", "Клик:"), F("btn2_click"), F(""), F("act"), NULL, ini.btn2_click & 0x7F);
	out += HTML_Options(F(""), F("btn2_c_spd"), F(""), F("spd"), NULL, ini.btn2_click >= 0x80);
	out += AddMasterSlave(F("b2c"), F("m_s"), /*MASTER_AND_SLAVES,*/ ini.btn2_c_addr);
	out += HTML_Options(FLF("Long:", "Долгий:"), F("btn2_long"), F(""), F("act"), NULL, ini.btn2_long & 0x7F);
	out += HTML_Options(F(""), F("btn2_l_spd"), F(""), F("spd"), NULL, ini.btn2_long >= 0x80);
	out += AddMasterSlave(F("b2l"), F("m_s"), /*MASTER_AND_SLAVES,*/ ini.btn2_l_addr);

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
	out += HTML_hint(FLF("(Auxiliary input. Connect to selected pins. Will send \"ON/OFF\" payloads to selected topic on change)",
		"(Доп. вход. Подключать к выбраным пинам. При изменении будет отправлять \"ON/OFF\" в указанный топик)"));
#endif

	out += HTML_section(FLF("Master/slave", "Главный/ведомый"));
	out += F("<tr><td>");
	out += FLF("Wired:", "Провод:");
	out += F("</td><td><select id=\"slave\" name=\"slave\" onchange=\"PinChange();MSChange();\">\n");
	out += F("</select></td></tr>\n");
	out += AddOptions(F("slave"), F("mss"),
		FLF("0,\"Standalone\",255,\"Master\",1,\"Slave 1\",2,\"Slave 2\",3,\"Slave 3\",4,\"Slave 4\",5,\"Slave 5\"",
			"0,\"Независимый\",255,\"Главный\",1,\"Ведомый 1\",2,\"Ведомый 2\",3,\"Ведомый 3\",4,\"Ведомый 4\",5,\"Ведомый 5\""),
		ini.slave);
	out += F("<tr id=\"tr_sl_ips\"><td>");
	out += FLF("Wake up:", "Разбудить:");
	out += F("</td><td><div id=\"wake_btns\"></div><div id=\"ip_tx_slaves\"></div></td></tr>\n");
	out += F("<script>AddWakeUp();</script>\n");

	out += F("<tr id=\"tr_ips\"><td>");
	out += FLF("WiFi slaves:", "WiFi ведомые:");
	out += F("</td><td><div id=\"ip_slaves\"></div><div id=\"ip_slave_add\"></div></td></tr>\n");
	out += AddIPSlaves();
	out += HTML_hint(SL(F("Help:"), F("Помощь:")) + " <a href=\"http://imlazy.ru/rolls/master.html\">imlazy.ru/rolls/master.html</a>");

#if DAYLIGHT
	out += HTML_section(FLF("Coordinates", "Координаты"));
	out += HTML_editString(FLF("Latitude:", "Широта:"), F("lat"), "", 11);
	out += HTML_editString(FLF("Longitude:", "Долгота:"), F("lng"), "", 11);
	out += F("<script>SetCoord(");
	out += ini.lat;
	out += F(",");
	out += ini.lng;
	out += F(");</script>\n");
#endif

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

void HTTP_handleAlarms(void)
{
	String out;
	int s;

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

			ini.alarms[a].flags &= ~(ALARM_FLAG_SUNRISE | ALARM_FLAG_SUNSET);
			s = 0;
			if (httpServer.hasArg("a_src" + n))
			{
				s = atoi(httpServer.arg("a_src" + n).c_str());
				if (s == 1) ini.alarms[a].flags |= ALARM_FLAG_SUNRISE;
				if (s == 2) ini.alarms[a].flags |= ALARM_FLAG_SUNSET;
			}

			if (s > 0)
			{ // sun time
				if (httpServer.hasArg("sunh" + n))
					ini.alarms[a].time = 10 + atoi(httpServer.arg("sunh" + n).c_str());
				if (httpServer.hasArg("tmin" + n))
					ini.alarms[a].tmin = StrToTime(httpServer.arg("tmin" + n));
			} else
			{ // clock time
				if (httpServer.hasArg("time" + n))
					ini.alarms[a].time = StrToTime(httpServer.arg("time" + n));
			}

			if (httpServer.hasArg("dest" + n))
			{
				ini.alarms[a].percent_open = atoi(httpServer.arg("dest" + n).c_str());
				if (ini.alarms[a].percent_open > 100 + MAX_PRESETS)
					ini.alarms[a].percent_open = 100;
			}

			if (httpServer.hasArg("spd" + n))
			{
				if (atoi(httpServer.arg("spd" + n).c_str()))
					ini.alarms[a].flags |= ALARM_FLAG_SLOW;
				else
					ini.alarms[a].flags &= ~ALARM_FLAG_SLOW;
			}

			uint8_t b = 0;
			for (int d = 6; d >= 0; d--)
			{
				b=b<<1;
				if (httpServer.hasArg("d"+n+"_"+String(d))) b|=1;
			}
			ini.alarms[a].day_of_week = b;

			if (MASTER) SaveMasterSlave("ms"+n+"_", &(ini.alarms[a].m_s));
		}

		SaveSettings(&ini, sizeof(ini));
		CalcAlarmTimes();
	}

	out=HTML_header();

	out.reserve(10240);

	out += F("<section class=\"alarms\" id=\"alarms\">\n"
		"<form method=\"post\" action=\"/alarms\">\n"
		"<table width=\"100%\">\n");
	out += HTML_save(3);
	out += F("<tr><td colspan=\"3\"><p>");
	out += FLF("To execute command one time, remove all day of week marks. Command will be disabled after execution.</p>",
		"Для выполнения пункта расписания один раз, в ближайшие сутки, снимите все галочки дней недели. После выполнения пункт отключится.</p>");

	if (lastSync != 0) out += PrintSunriseTable();
// 	for (int i=0; i<365; i++)
// 	{
// 		Serial.print(i);
// ZENITH = FROMFLOAT(-.83f -5);
// 		Serial.print(F("   "));
// 		Serial.print(TimeToStr(calculateSunriseSunset(i, ini.lat, ini.lng, ini.timezone, 0, 1)));
// 		Serial.print(F(" "));
// 		Serial.print(TimeToStr(calculateSunriseSunset(i, ini.lat, ini.lng, ini.timezone, 0, 0)));
// ZENITH = FROMFLOAT(-.83f + 5);
// 		Serial.print(F("   "));
// 		Serial.print(TimeToStr(calculateSunriseSunset(i, ini.lat, ini.lng, ini.timezone, 0, 1)));
// 		Serial.print(F("         "));
// 		Serial.print(TimeToStr(calculateSunriseSunset(i, ini.lat, ini.lng, ini.timezone, 0, 0)));
// //		Serial.print(calculateSunriseSunset(i, ini.lat, ini.lng, ini.timezone, 0, 0));
// 		Serial.print(F(" "));
// 		Serial.print(ifcos(RADIANS(FROMINT(i))));
// 		Serial.println();
// 	}

	String actions = "";
	for (int p=0; p<=100; p+=20)
	{
		actions += p;
		actions += ",\"";
		if (p==0)   actions += FL(F("Open"), F("Открыть")); else
		if (p==100) actions += FL(F("Close"), F("Закрыть")); else
		{
			actions += p;
			actions += "%";
		}
		actions += "\",";
	}
	for (int p=0; p<MAX_PRESETS; p++)
	{
		actions += 101+p;
		actions += FLF(",\"Preset ", ",\"Позиция ");
		actions += 1+p;
		actions += "\",";
	}

	char buf[700];
	out += F("<script>\n");
	snprintf_P(buf, sizeof(buf), PSTR("AddAlarms(%i,'%s','%s','%s','%s','%s','%s','%s',%s);\n"),
		ALARMS,
		FLF("Enable", "Вкл."),
		FLF("Time:", "Время:"),
		FLF("Not earlier:", "Не ранее:"),
		FLF("Height:", "Высота:"),
		FLF("Action:", "Действие:"),
		FLF("Speed:", "Скорость:"),
		FLF("Repeat:", "Повтор:"),
		(MASTER || IsIPMaster() ? F("true") : F("false")));
	ChangeQuoteSymbol(buf);
	out += buf;

	snprintf_P(buf, sizeof(buf),
		PSTR("const sh_a=[%s];\n" \
			"a_shs=[5,'+5&deg;',4,'+4&deg;',3,'+3&deg;',2,'+2&deg;',1,'+1&deg;',0,'&nbsp;&nbsp;0&deg;',-1,'−1&deg;',-2,'−2&deg;',-3,'−3&deg;',-4,'−4&deg;',-5,'−5&deg;'];\n" \
			"a_srs=[%s];\n" \
			"const dow=[%s];\n" \
			"const a_spd=[0,'1',1,'2'];\n" \
			"const m_s=[%s];\n"),
		actions.c_str(),
		FLF("'clock', 'sunrise', 'sunset'", "'часы', 'восход', 'закат'"),
		FLF("'Mo','Tu','We','Th','Fr','Sa','Su'", "'Пн','Вт','Ср','Чт','Пт','Сб','Вс'"),
		MASTER_AND_SLAVES);
	ChangeQuoteSymbol(buf);
	out += buf;

	for (int a=0; a<ALARMS; a++)
	{
		int r = 0, t = 0;
		if (ini.alarms[a].flags & ALARM_FLAG_SUNRISE) r = 1;
		if (ini.alarms[a].flags & ALARM_FLAG_SUNSET) r = 2;
		if (ini.alarms[a].flags & ALARM_FLAG_SUNRISE || ini.alarms[a].flags & ALARM_FLAG_SUNSET) t = ini.alarms[a].time - 10;
		snprintf_P(buf, sizeof(buf),
			PSTR("SetAlarm(%i,%i,%i,%i,%i,%i,%i,%i,'%s','%s');\n"),
			a,
			ini.alarms[a].percent_open,
			ini.alarms[a].day_of_week,
			r,
			t,
			(ini.alarms[a].flags & ALARM_FLAG_SLOW ? 1 : 0),
			ini.alarms[a].m_s,
			ini.alarms[a].flags & ALARM_FLAG_ENABLED,
			(ini.alarms[a].flags & ALARM_FLAG_SUNRISE || ini.alarms[a].flags & ALARM_FLAG_SUNSET ? PSTR("00:00") : TimeToStr(ini.alarms[a].time).c_str()),
			(ini.alarms[a].flags & ALARM_FLAG_SUNRISE || ini.alarms[a].flags & ALARM_FLAG_SUNSET ? TimeToStr(ini.alarms[a].tmin).c_str() : PSTR("00:00")));
		ChangeQuoteSymbol(buf);
		out += buf;
	}
	out += F("</script>\n");

	out += HTML_save(3);

	out += F("</table>\n</form>\n</section>\n");
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
	int step_delay = 0;
	uint8_t addr = ADDR_ALL;
	HTTP_Activity();
	elog.Add(EI_Cmd_Open, EL_INFO, ES_HTTP);
	if (httpServer.hasArg("speed")) step_delay = atoi(httpServer.arg("speed").c_str());
	if (httpServer.hasArg("addr")) addr = atoi(httpServer.arg("addr").c_str());
	Open(addr, step_delay);
	Return200();
}

void HTTP_handleClose(void)
{
	int step_delay = 0;
	uint8_t addr = ADDR_ALL;
	HTTP_Activity();
	elog.Add(EI_Cmd_Close, EL_INFO, ES_HTTP);
	if (httpServer.hasArg("speed")) step_delay = atoi(httpServer.arg("speed").c_str());
	if (httpServer.hasArg("addr")) addr = atoi(httpServer.arg("addr").c_str());
	Close(addr, step_delay);
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
		int preset = atoi(httpServer.arg("preset").c_str());
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
		memset(Slaves_IP, 0, sizeof(Slaves_IP));
		SendUART(UART_CMD_WAKE, addr, WiFi.localIP()[3]);
		Return200();
	}
	else if (httpServer.hasArg("click"))
	{
		elog.Add(EI_Cmd_Click, EL_INFO, ES_HTTP);
		ButtonClick(1, addr);
		Return200();
	}
	else if (httpServer.hasArg("click2"))
	{
		elog.Add(EI_Cmd_Click2, EL_INFO, ES_HTTP);
		ButtonClick(2, addr);
		Return200();
	}
	else if (httpServer.hasArg("longclick"))
	{
		elog.Add(EI_Cmd_LClick, EL_INFO, ES_HTTP);
		ButtonLongClick(1, addr);
		Return200();
	}
	else if (httpServer.hasArg("longclick2"))
	{
		elog.Add(EI_Cmd_LClick2, EL_INFO, ES_HTTP);
		ButtonLongClick(2, addr);
		Return200();
	}
	else if (httpServer.hasArg("blink"))
	{
		SendUART(UART_CMD_BLINK, addr);
		Return200();
	}
	else if (httpServer.hasArg("zero"))
	{
		elog.Add(EI_Cmd_Zero, EL_INFO, ES_HTTP);
		ResetZero();
		Return200();
	}
	else if (httpServer.hasArg("slave"))
	{
		int val = 0;
		char cmd = atoi(httpServer.arg("slave").c_str());
		if (httpServer.hasArg("val")) val = atoi(httpServer.arg("val").c_str());
		ExecuteSlaveCommand(cmd, val, ES_HTTP_MASTER);
		Return200();
	}
	else if (httpServer.hasArg("slave_ip"))
	{
		char ip = atoi(httpServer.arg("slave_ip").c_str());
		for (unsigned int i=0; i<ARRAYSIZE(Slaves_IP); i++)
			if (Slaves_IP[i] == 0 || Slaves_IP[i] == ip) { Slaves_IP[i] = ip; break; }
		Return200();
	}
	else
		httpServer.send(400, "text/XML", blank_xml); // 400 Bad Request
}

void HTTP_handleTest(void)
{
	bool dir = false;
	uint8_t bak_pinout;
	bool bak_reversed;
	uint16_t bak_step_delay_mks;
	int steps = 300;

	HTTP_Activity();

	bak_pinout = ini.pinout;
	bak_reversed = ini.reversed;
	bak_step_delay_mks = ini.step_delay_mks;

	if (httpServer.hasArg("up")) dir=true;
	if (httpServer.hasArg("reversed")) ini.reversed=atoi(httpServer.arg("reversed").c_str());
	if (httpServer.hasArg("pinout")) ini.pinout=atoi(httpServer.arg("pinout").c_str());
	if (ini.pinout>=MAX_PINOUT) ini.pinout=2;
	if (MOTOR_WITH_PWM)
	{
		if (httpServer.hasArg("pwm")) ini.step_delay_mks = atoi(httpServer.arg("pwm").c_str());
		if (ini.step_delay_mks > 100) ini.step_delay_mks = 100;
	} else
	{
		if (httpServer.hasArg("delay")) ini.step_delay_mks = atoi(httpServer.arg("delay").c_str());
		if (ini.step_delay_mks < MIN_STEP_DELAY) ini.step_delay_mks = MIN_STEP_DELAY;
		if (ini.step_delay_mks > 65000) ini.step_delay_mks = 65000;
	}
	if (httpServer.hasArg("steps")) steps=atoi(httpServer.arg("steps").c_str());
	if (steps<0) steps=0;

	FillStepsTable();
	AdjustTimerInterval();
	SetupMotorPins();

	TestMode = 1;
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
	roll_to = position;
	TestMode = 0;

	ini.pinout = bak_pinout;
	ini.reversed = bak_reversed;
	ini.step_delay_mks = bak_step_delay_mks;

	FillStepsTable();
	AdjustTimerInterval();
	SetupMotorPins();

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
	s = String(ini.name);
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
	XML += MakeNode(F("Master"), (MASTER ? "yes" : "no"));
//XML += MakeNode(F("Debug"), payload);
	s = "";
	for (unsigned int i=0; i<ARRAYSIZE(Slaves_IP); i++)
		if (Slaves_IP[i]) { if (s != "") s += ","; s += String(Slaves_IP[i]); }
	if (s != "")
		XML += MakeNode(F("Slaves"), s);
	XML += F("</Info>");

	XML += F("<Presets>");
	for (int i=1; i<=MAX_PRESETS; i++)
	{
		char buf[30];
		snprintf_P(buf, sizeof(buf), "<Preset%i>%i</Preset%i>", i, ini.preset[i-1], i);
		XML += String(buf);
	}
	XML += F("</Presets>");

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
	if (ini.end2_pin)
		XML += MakeNode(F("End2"), onoff[ini.lang][IsSwitch2Pressed()]);
#if MQTT
	if (ini.aux_pin)
		XML += MakeNode(F("Aux"), onoff[ini.lang][aux_state == AUX_ON]);
#endif
	XML += F("</Position><LED>");
	XML += MakeNode(F("Mode"), LEDModeString());
	XML += MakeNode(F("Level"), LEDLevelString());
	XML += F("</LED></Curtain>");

	httpServer.sendHeader(F("Access-Control-Allow-Origin"), "*"); // Allowing Cross-Origin Resource Sharing
	httpServer.send(200, "text/XML", XML);
}

void date2str(char* buf, uint32_t t)
{
	int year, month, day, hour, minute, sec;

	t += ini.timezone*60;
	sec = t % 60;
	minute = (t / 60) % 60;
	hour = (t / (60*60)) % 24;
	Time2YMD(t, year, month, day);
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, sec);
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
			if (t > 100 * DAY)
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
				case EI_Cmd_Zero:
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
				case EI_Pingfail:
					out += (int32_t)e->val;
					out += F(" times");
					break;
				case EI_IP_Slave_Err:
					out += F("IP: ");
					out += (int32_t)e->val & 0xFF;
					out += F(" Code: ");
					out += (int)e->val >> 8;
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
uint32_t alarm_time[ALARMS];
void CalcAlarmTimes()
{
	for (int a=0; a<ALARMS; a++)
	{
		if (ini.alarms[a].flags & ALARM_FLAG_SUNRISE) alarm_time[a] = max(GetSunTime(ini.alarms[a].time - 10, 1), ini.alarms[a].tmin); else
		if (ini.alarms[a].flags & ALARM_FLAG_SUNSET)  alarm_time[a] = max(GetSunTime(ini.alarms[a].time - 10, 0), ini.alarms[a].tmin); else
		alarm_time[a] = ini.alarms[a].time;
		//Serial.println(alarm_time[a]);
	}
}

void Scheduler()
{
	uint32_t t, p;

	static uint32_t last_t;
	static int last_dow = -1;
	int dayofweek;
	bool slave[1 + MAX_SLAVE] = { 0, 0, 0, 0, 0, 0 }; // master([0]) and slaves([1 - MAX_SLAVE])

	t = getTime();
	if (t == 0) return;
	dayofweek = DayOfWeek(t); // 0 - monday
	t=t % DAY; // time from day start
	t=t/60; // in minutes

	if (t == last_t) return; // this minute already handled
	last_t=t;
	if (last_dow != dayofweek)
	{
		CalcAlarmTimes(); // new day, calculate suntimes for today
		last_dow = dayofweek;
	}

	for (int a=0; a<ALARMS; a++)
	{
		if (!(ini.alarms[a].flags & ALARM_FLAG_ENABLED)) continue;
		if ( (alarm_time[a] != t)) continue;
		if (!(ini.alarms[a].day_of_week & (1<<dayofweek)) && (ini.alarms[a].day_of_week != 0)) continue;

		if (ini.alarms[a].day_of_week == 0) // if no repeat
		{
			ini.alarms[a].flags &= ~ALARM_FLAG_ENABLED; // disabling
			SaveSettings(&ini, sizeof(ini));
		}

		// select master and slaves, from settings
		for (uint8_t i=0; i<=MAX_SLAVE; i++)
		{
			if (MASTER || IsIPMaster())
				slave[i] = ini.alarms[a].m_s & (0x01 << i);
			else
				slave[i] = (i==0 ? 1 : 0);
		}

		if (ini.alarms[a].percent_open <= 100)
		{ // percentage
			p = ini.alarms[a].percent_open;
			elog.Add(EI_Cmd_Percent, EL_INFO, ES_SCHEDULE + (p << 8));
			for (uint8_t address=0; address <= MAX_SLAVE; address++)
				if (slave[address])	ToPercent(p, address, (ini.alarms[a].flags & ALARM_FLAG_SLOW ? SPEED2 : 0));
		} else if (ini.alarms[a].percent_open <= 100+MAX_PRESETS)
		{ // preset
			p = ini.alarms[a].percent_open-100;
			elog.Add(EI_Cmd_Preset, EL_INFO, ES_SCHEDULE + (p << 8));
			for (uint8_t address=0; address <= MAX_SLAVE; address++)
				if (slave[address])	ToPreset(p, address, (ini.alarms[a].flags & ALARM_FLAG_SLOW ? SPEED2 : 0));
		}
	}
	dumb=0;
}

#define GRATUITOUS_ARP_MS 60000 // refresh arp every 60 sec
void GratuitousARP()
{
	struct netif *netif = netif_list;
	static uint32_t last_garp_time = 0;
	if (!WiFi_connected) return;
	if (millis() - last_garp_time < GRATUITOUS_ARP_MS) return;
	last_garp_time = millis();
	while (netif)
	{
		etharp_gratuitous(netif);
		netif = netif->next;
	}
}

void MotorFailsafe()
{ // Shut down motor after some time of work in case of encoder fail or stall
	static uint32_t last_idle, last_move;
	static int last_pos = 0;
	if (MOTOR_WITH_ENC)
	{
		if (position == roll_to)
		{
			last_move = last_idle = millis();
			return;
		}
		if (position < last_pos - 3 || position > last_pos + 3) // position changed at least 3 steps
		{
			last_move = millis();
			last_pos = position;
			return;
		}
		if ((millis() - last_idle > 3 * 60 * 1000) || //  3 min of continuous work
			(millis() - last_move > 2 * 1000)) // 2 seconds of stall
		{
			Stop(ADDR_MASTER);
			elog.Add(EI_Stall, EL_ERROR, 0);
		}
	}
}

void loop(void)
{
	ProcessWiFi();
	if (!ini.mqtt_enabled) GratuitousARP();
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
	MotorFailsafe();
	ProcessPing();

	if (millis() - last_network_time > 10000)
		WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	if (SLAVE && WiFi_active &&
		(millis() - last_network_time > SLAVE_SLEEP_TIMEOUT_MS) &&
		(millis() - lastUARTping < SLAVE_MAX_NO_PING_MS))
	{
		Serial.println(F("Network idle. WiFi shutdown"));
		WiFi_Off();
	}
	if (endstop_hit != EL_NONE)
	{
		if (endstop_hit == EL_ERROR)
			elog.Add(EI_Endstop_Hit_Error, EL_ERROR, endstop_hit_pos);
		else
			elog.Add(EI_Endstop_Hit, EL_INFO, endstop_hit_pos);
		endstop_hit = EL_NONE;
	}

	delay(10); // this delay enables light sleep mode
	// static int heap = 65536;
	// int h;
	// h = ESP.getFreeHeap();
	// if (h < heap)
	// {
	// 	heap = h;
	// 	Serial.println(heap);
	// }

/* 	// cpu load test
	static uint32_t ms;
	if (millis() - ms > 10000)
	{
		ms = millis();
		Serial.println(ESP.getCpuFreqMHz());

		uint32_t t1=millis();
		for (i=0; i<1000000; i++) ;
		Serial.println(millis()-t1);
	}
*/
}
