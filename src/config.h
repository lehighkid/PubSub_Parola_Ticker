
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
int                   mqtt_port                     = 1883;
const char*           mqtt_server                   = "10.0.1.11";
String                mqtt_client_id                = "ESP8266Ticker-" + String(random(0xffff), HEX);
const uint32_t        controller_id                 = ESP.getChipId();
const long            mqtt_recon_time               = 5000;
const long            mqtt_resub_time               = 1200000;
const long            mqtt_loop_time                = 100;
const char*           mqtt_username                 = "ticker1";
const char*           mqtt_password                 = "ticker1";
const char*           mqtt_endpoint                 = "tickers";
char                  mqtt_endpoint_register[50];
char                  mqtt_endpoint_subscribe[50];
char                  mqtt_endpoint_message[100];

// Wifi credentials
const char*           wifi_ap_name                  = mqtt_client_id.c_str();
const char*           wifi_ssid                     = "";
const char*           wifi_password                 = "";
const long            wifi_recon_time               = 500;

// MQTT topic channels
const char*           mqtt_topic_stat               = "status";
const char*           mqtt_topic_ping               = "ping";
const char*           mqtt_topic_msg                = "message";
const char*           mqtt_topic_rst                = "restart";
const char*           mqtt_topic_info               = "info";
const char*           mqtt_topic_pwr_on             = "powerOn";
const char*           mqtt_topic_pwr_off            = "powerOff";
const char*           mqtt_topic_pwr_tog            = "powerToggle";

// TODO: Add ability to make this dynamic - currently code hinges on specified topic
const char*           channels[]                    = {mqtt_topic_msg, mqtt_topic_rst, mqtt_topic_info, mqtt_topic_pwr_on, mqtt_topic_pwr_off, mqtt_topic_pwr_tog};

// MQTT message commands & responses
//const char*           mqtt_msg_succ                 = "0";
//const char*           mqtt_msg_fail                 = "-1";
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
bool                    powerStat                   = true;

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

textEffect_t textEffects[] =
{
  PA_PRINT,
  PA_SLICE,
  PA_MESH,
  PA_FADE,
  PA_WIPE,
  PA_WIPE_CURSOR,
  PA_OPENING,
  PA_OPENING_CURSOR,
  PA_BLINDS,
  PA_DISSOLVE,
  PA_SCAN_HORIZ,
  PA_SCAN_HORIZX,
  PA_SCAN_VERT,
  PA_SCAN_VERTX,
  PA_GROW_UP,
  PA_GROW_DOWN,
  PA_SCROLL_UP,
  PA_SCROLL_DOWN,
  PA_SCROLL_LEFT,
  PA_SCROLL_RIGHT,
  PA_SCROLL_DOWN_LEFT,
  PA_SCROLL_DOWN_RIGHT,
  PA_SPRITE,
  PA_NO_EFFECT,
  PA_RANDOM
};

typedef struct
{
  const uint8_t* entry;
  uint8_t entryWidth;
  uint8_t entryFrames;
  const uint8_t* exit;
  uint8_t exitWidth;
  uint8_t exitFrames;
} sprite;

sprite sprites[] =
{
  { walker, W_WALKER, F_WALKER, walker, W_WALKER, F_WALKER },
  { invader, W_INVADER, F_INVADER, invader, W_INVADER, F_INVADER },
  { chevron, W_CHEVRON, F_CHEVRON, chevron, W_CHEVRON, F_CHEVRON },
  { heart, W_HEART, F_HEART, heart, W_HEART, F_HEART },
  { arrow1, W_ARROW1, F_ARROW1, arrow1, W_ARROW1, F_ARROW1 },
  { steamboat, W_STEAMBOAT, F_STEAMBOAT, steamboat, W_STEAMBOAT, F_STEAMBOAT },
  { fireball, W_FBALL, F_FBALL, fireball, W_FBALL, F_FBALL },
  { rocket, W_ROCKET, F_ROCKET, rocket, W_ROCKET, F_ROCKET },
  { pacman1, W_PMAN1, F_PMAN1, pacman2, W_PMAN2, F_PMAN2 },
  { lines, W_LINES, F_LINES, lines, W_LINES, F_LINES },
  { roll1, W_ROLL1, F_ROLL1, roll2, W_ROLL2, F_ROLL2 },
  { sailboat, W_SAILBOAT, F_SAILBOAT, sailboat, W_SAILBOAT, F_SAILBOAT },
  { arrow1, W_ARROW1, F_ARROW1, arrow2, W_ARROW2, F_ARROW2 },
  { wave, W_WAVE, F_WAVE, wave, W_WAVE, F_WAVE }
};

typedef struct
{
  const char*     id;
  const char*     text;
  uint16_t        speed;
  uint16_t        pause;
  long            duration;
  int             justification;
  int             entryEffect;
  int             exitEffect;
  int             intensity;
  const char*     displayType;
  int             sprite;
  unsigned long   endTick;
  int             runCount;
  int             timesRun;
  const char*     action;
  bool            queueWipe;
} message;

typedef struct
{
  char*         topic;
  byte*         payload;
  unsigned int  length;
} mqttMessage;
