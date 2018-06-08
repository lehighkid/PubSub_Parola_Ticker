
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#if USE_LIBRARY_SPI
#include <SPI.h>
#endif
#include "queue.h"
#include "chars.h"
#include "certs.h"
#define DEBUG
#include "debug.h"

// GPIO pins
int                   pin_led                       = LED_PIN;
int                   pin_CLK                       = CLK_PIN;
int                   pin_DATA                      = DATA_PIN;
int                   pin_CS                        = CS_PIN;

// MQTT connection details
int                   mqtt_port                     = 8883;
const char*           mqtt_server                   = "acdrago.com";
const char*           mqtt_fingerprint              = "0C:4B:FA:99:E4:B5:46:E3:1A:E0:47:C1:69:D6:60:03:E6:CE:6F:F0";
String                mqtt_client_type              = "ESP8266Ticker-" + String(random(0xffff), HEX);
const uint32_t        controller_id                 = ESP.getChipId();
const long            mqtt_recon_time               = 5000;
const long            mqtt_resub_time               = 1200000;
const long            mqtt_loop_time                = 100;
const char*           mqtt_username                 = "ticker";
const char*           mqtt_password                 = "ticker";
const char*           mqtt_endpoint                 = "tickers";
char                  mqtt_endpoint_register[50];
char                  mqtt_endpoint_subscribe[50];
char                  mqtt_endpoint_message[100];

// Wifi credentials
const char*           wifi_ap_name                  = mqtt_client_type.c_str();
const char*           wifi_ssid                     = "";
const char*           wifi_password                 = "";
const long            wifi_recon_time               = 500;

// MQTT topic channels
const char*           mqtt_topic_stat               = "status";
const char*           mqtt_topic_ping               = "ping";
const char*           mqtt_topic_msg                = "message";
const char*           mqtt_topic_rst                = "restart";
const char*           mqtt_topic_info               = "info";

// TODO: Add ability to make this dynamic - currently code hinges on specified topic
const char*           channels[]                    = {mqtt_topic_msg, mqtt_topic_rst, mqtt_topic_info};

// MQTT message commands & responses
const char*           mqtt_msg_succ                 = "0";
const char*           mqtt_msg_fail                 = "-1";
const char*           mqtt_msg_ins                  = "INSERT";
const char*           mqtt_msg_upd                  = "UPDATE";
const char*           mqtt_msg_ups                  = "UPSERT";
const char*           mqtt_msg_del                  = "DELETE";
boolean               mqtt_msg_wtg                  = false;
unsigned int          mqtt_msg_queue_max_limit      = 24;
char*                 mqtt_msg_topic;
byte*                 mqtt_msg_payload;
unsigned int          mqtt_msg_length;

// Initial Loop Variables
long                    lastMsg                     = 0;
long                    lastSub                     = 0;
long                    lastLoop                    = 0;
unsigned int            failed_breakout_limit       = 5;

// Other constants
const long            ping_int                      = 119000;
byte                  null_str                      = '\0';

// Parola
int                   parola_max_devices            = DEV_CNT;
const char*           msg_display_scroll            = "SCROLL";
const char*           msg_display_static            = "STATIC";
const int             msg_queue_max_limit           = 24;
bool                  startup_msg_enabled           = true;
const char*           startup_msg_txt               = "hello ajd!";
uint16_t              startup_msg_speed             = 15;
uint16_t              startup_msg_pause             = 3000;
textPosition_t        startup_msg_just              = PA_CENTER;
textEffect_t          startup_msg_entry_effect      = PA_SPRITE;
textEffect_t          startup_msg_exit_effect       = PA_SPRITE;
uint8_t*              startup_msg_entry_sprite      = (uint8_t*)pacman1;
const uint8_t         startup_msg_entry_sprite_F    = F_PMAN1;
const uint8_t         startup_msg_entry_sprite_W    = W_PMAN1;
uint8_t*              startup_msg_exit_sprite       = (uint8_t*)pacman2;
const uint8_t         startup_msg_exit_sprite_F     = F_PMAN2;
const uint8_t         startup_msg_exit_sprite_W     = W_PMAN2;

typedef struct
{
  const char*   id;
  const char*   text;
  uint16_t      speed;
  uint16_t      pause;
  long          duration;
  int           justification;
  int           entryEffect;
  int           exitEffect;
  const char*   displayType;
  long          endTick;
  int           runCount;
  int           timesRun;
  const char*   msgAction;
  bool          queueWipe;
} message;

typedef struct
{
  char*         topic;
  byte*         payload;
  unsigned int  length;
} mqttMessage;
