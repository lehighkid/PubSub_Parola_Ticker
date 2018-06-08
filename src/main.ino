/*
  Parola Message Ticker controlled by MQTT running on nodemcu esp8266

  Multiple example sketchs and code leveraged by
  Aaron Drago (lehighkid@gmail.com) for use in AutoRunHome
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "config.h"

// Define wifi client for use in PubSubClient for MQTT
WiFiClientSecure espClient;
// Define the MQTT object
PubSubClient client(mqtt_server, mqtt_port, espClient);

// Define hardware SPI connection for Parola
// TODO: Add options to config for HW vs. SW and update definition here
MD_Parola P = MD_Parola(pin_CS, parola_max_devices);

// Create mqtt message queue for processing
Queue<mqttMessage> mqttMessageQueue(mqtt_msg_queue_max_limit);

// Create display message queue for processing
Queue<message> messageQueue(msg_queue_max_limit);

// Iterates until a wifi connection is made and status updated accordingly
void wifi_connect()
{
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  DPRINTLN();
  DPRINTLN();
  DPRINT("Wait for WiFi... ");

  while (WiFi.status() != WL_CONNECTED) {
    DPRINT(".");
    delay(wifi_recon_time);
  }
  DPRINTLN("");
  DPRINTLN("WiFi connected");
  DPRINTLN("IP address: ");
  DPRINTLN(WiFi.localIP());

  delay(wifi_recon_time);
}

bool wifi_verifyTLS()
{
  // Use WiFiClientSecure class to create TLS connection
  DPRINT("\nConnecting to ");
  DPRINTLN(mqtt_server);
  if (!espClient.connect(mqtt_server, mqtt_port)) {
    DPRINTLN("\nConnection failed");
    return false;
  }

  if (!espClient.verify(mqtt_fingerprint, mqtt_server)) {
    DPRINT("\nCertificate doesn't match");
    return false;
  }
  return true;
}

// Setup function to set proper pin state
void initialize_pins()
{
  pinMode(pin_led, OUTPUT);
  digitalWrite(pin_led, HIGH);
}

// GPIO toggle function to create light flash or trip relay, etc.
void toggle_pin(int pin, int duration, int intervals)
{
  bool orig_state = digitalRead(pin);
  for (int i = 0; i < intervals; i++)
  {
   digitalWrite(pin, !digitalRead(pin));
   delay(duration);
  }
  digitalWrite(pin, orig_state);
}

// [Re]Connect to MQTT Broker then subscribe to topics
void mqtt_connect()
{
  // Check for secure connection
  // if(!wifi_verifyTLS()){
  //   return;
  // }

  // Initialize failure count for breakout routine
  unsigned int failedAttempts = 0;
  // Attempt connection routine
  while (!client.connected()) {
    //ESP.wdtFeed();
    DPRINTLN("\nConnecting to MQTT");
    // Check number of failed attempts so far
    if(failedAttempts <= failed_breakout_limit) {
      // Attempt to connect and check status
      //if (client.connect(mqtt_client_type.c_str(), mqtt_username, mqtt_password)){
      if (client.connect(mqtt_client_type.c_str())){
        DPRINTLN("\nMQTT connected");
        mqtt_subscribe();
      // Connection was unnsuccessful
      } else
      {
        DPRINT("\nMQTT connect failed, rc=");
        DPRINTLN(client.state());
        // Increment failed attempts and wait 5 seconds before retrying
        failedAttempts++;
        DPRINT("\nFailed attempt number:");
        DPRINTLN(failedAttempts);
        // Wait before trying again
        delay(mqtt_recon_time);
      }
    // Failed attempts thresholds exceeded
    } else {
      // Give up and restart the module in attempts of flushing the issue
      DPRINT("\nMQTT failed connection attempts exceeded - restarting device.");
      ESP.restart();
    }
  }
}

// Set up topics for subscription
void mqtt_subscribe()
{
  // Loop through of configured subscriptions
  for (unsigned int c = 0; c < (sizeof(channels)/sizeof(int)); c++){
    // Construct topic string for subscription
    sprintf(mqtt_endpoint_subscribe, "%s/%08X/%s", mqtt_endpoint, controller_id, channels[c]);
    // Subscribe to individual topic & check response
    if(!client.subscribe(mqtt_endpoint_subscribe))
    {
      DPRINTLN("Subscribe unsuccessful");
      // Subscribe failed - disconnect and exit
      client.disconnect();
      return;
    };
    DPRINTLN(mqtt_endpoint_subscribe);
  }
}

String convertPayloadToString(byte* payload, unsigned int length)
{
  payload[length]     = null_str;   // add null string terminator for String conversion
  return String((char*)payload);
}

message convertmqttMessageToMessage(mqttMessage mqtt_msg)
{
  // Extract mqtt message components from struct
  byte* payload         = mqtt_msg.payload;
  unsigned int length   = mqtt_msg.length;

  // Messages should be in JSON format
  DynamicJsonBuffer  jsonBuffer;

  // Parse payload of JSON format
  JsonObject& payRoot = jsonBuffer.parseObject(convertPayloadToString(payload, length));

  // Test if parsing succeeds
  if (payRoot.success())
  {
    long endTick = millis() + (long)payRoot["duration"];

    return message {
      strdup(payRoot["messageId"]),
      strdup(payRoot["text"]),
      payRoot["speed"],
      payRoot["pause"],
      payRoot["duration"],
      payRoot["justification"],
      payRoot["entryEffect"],
      payRoot["exitEffect"],
      strdup(payRoot["displayType"]),
      endTick,
      payRoot["runCount"],
      0,
      strdup(payRoot["action"]),
      payRoot["wipe"]
    };
  }
  return message {};
}

// Debug function used to output mqtt message details
void print_mqttMessage(mqttMessage mqttMessage)
{
  DPRINTLN();
  DPRINT("Topic: ");
  DPRINTLN(mqttMessage.topic);
  DPRINT("Payload: ");
  DPRINTLN(convertPayloadToString(mqttMessage.payload, mqttMessage.length));
  DPRINT("Length: ");
  DPRINTLN(mqttMessage.length);
}

// Debug function used to output message details
void print_message(message message)
{
  DPRINTLN();
  DPRINT("Id: ");
  DPRINTLN(message.id);
  DPRINT("Text: ");
  DPRINTLN(message.text);
  DPRINT("DisplayType: ");
  DPRINTLN(message.displayType);
  DPRINT("EntryEffect: ");
  DPRINTLN(message.entryEffect);
  DPRINT("ExitEffect: ");
  DPRINTLN(message.exitEffect);
  DPRINT("Speed: ");
  DPRINTLN(message.speed);
  DPRINT("Pause: ");
  DPRINTLN(message.pause);
  DPRINT("Justification: ");
  DPRINTLN(message.justification);
  DPRINT("Run Count: ");
  DPRINTLN(message.runCount);
  DPRINT("Times Run: ");
  DPRINTLN(message.timesRun);
  DPRINT("Duration: ");
  DPRINTLN(message.duration);
  DPRINT("End Tick: ");
  DPRINTLN(message.endTick);
  DPRINT("Now: ");
  DPRINTLN(millis());
  DPRINT("Msg Action: ");
  DPRINTLN(message.msgAction);
  DPRINT("Queue Wipe: ");
  DPRINTLN(message.queueWipe);
  DPRINT("\nHeap: ");
  DPRINTLN(ESP.getFreeHeap());
}

// Conversion function - int to textPosition_t
textPosition_t get_justification(int just)
{
  textPosition_t rtnJust = PA_LEFT;
  switch (just)
  {
    case 0:   rtnJust =   PA_LEFT;    break;
    case 1:   rtnJust =   PA_CENTER;  break;
    case 2:   rtnJust =   PA_RIGHT;   break;
    default:  rtnJust =   PA_LEFT;    break;
  }
  return rtnJust;
}

// Conversion function - int to textEffect_t
textEffect_t get_effect(int effect)
{
  textEffect_t  rtnEffect = PA_SCROLL_LEFT;
  switch (effect)
  {
    case 0:  rtnEffect = PA_PRINT;             break;
    case 1:  rtnEffect = PA_SLICE;             break;
    case 2:  rtnEffect = PA_MESH;              break;
    case 3:  rtnEffect = PA_FADE;              break;
    case 4:  rtnEffect = PA_WIPE;              break;
    case 5:  rtnEffect = PA_WIPE_CURSOR;       break;
    case 6:  rtnEffect = PA_OPENING;           break;
    case 7:  rtnEffect = PA_OPENING_CURSOR;    break;
    case 8:  rtnEffect = PA_BLINDS;            break;
    case 9:  rtnEffect = PA_DISSOLVE;          break;
    case 10: rtnEffect = PA_SCAN_HORIZ;        break;
    case 11: rtnEffect = PA_SCAN_VERT;         break;
    case 12: rtnEffect = PA_GROW_UP;           break;
    case 13: rtnEffect = PA_GROW_DOWN;         break;
    case 14: rtnEffect = PA_SCROLL_UP;         break;
    case 15: rtnEffect = PA_SCROLL_DOWN;       break;
    case 16: rtnEffect = PA_SCROLL_LEFT;       break;
    case 17: rtnEffect = PA_SCROLL_RIGHT;      break;
    case 18: rtnEffect = PA_SCROLL_UP_LEFT;    break;
    case 19: rtnEffect = PA_SCROLL_UP_RIGHT;   break;
    case 20: rtnEffect = PA_SCROLL_DOWN_LEFT;  break;
    case 21: rtnEffect = PA_SCROLL_DOWN_RIGHT; break;
    case 22: rtnEffect = PA_SPRITE;            break;
    default: rtnEffect = PA_NO_EFFECT;         break;
  }

  return rtnEffect;

  // TODO:  change to array/struct and do lookup by int vs. switch
  // TODO:  -1 would yield Random - add logic
}

// Used to send MQTT messages (aka publish)
void send_message(const char* channel, const char* data)
{
  sprintf(mqtt_endpoint_message, "%s/%s", mqtt_endpoint, channel);
  DPRINT("\nSending message to: ");
  DPRINTLN(mqtt_endpoint_message);
  toggle_pin(pin_led, 100, 1);
  client.publish(mqtt_endpoint_message, data);
}

// Ping function to ensure keep alive on MQTT server - special call of send_message
void send_ping()
{
  char ping_msg[20];
  sprintf(ping_msg, "id=%08X, ip=%d:%d:%d:%d", controller_id, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DPRINT("\nSending ping: ");
  DPRINTLN(ping_msg);
  send_message(mqtt_topic_ping, ping_msg);
}

// Used to wipe out all messages - leaves messages queue empty
void wipe_messages()
{
  messageQueue.clear();
  DPRINTLN("\nMessage Queue has been wiped");
}

// Used to delete specific message from messages queue - uses id
void delete_message(const char* id)
{
  // TODO: Build looping logic to iterate message queue - possibly move from one queue to another
  DPRINTLN("\nDelete requested but not implemented");
}

// Used to insert new message into messages queue
void add_message(message msg)
{
  messageQueue.push(msg);
  DPRINTLN("\nInsert completed");
}

// Used to update message in message queue based on id
boolean update_message(message msg)
{
  // TODO: Build looping logic to iterate message queue - possibly move from one queue to another
  boolean updateStatus = false;
  DPRINTLN("\nUpdate requested but not implemented");
  return updateStatus; // returning false allows upsert to leverage insert action
}

// Limited function to capture message receipt and leave flag for processing when animation idle
void receive_message(const char* topic, byte* payload, unsigned int length)
{
  DPRINT("\nMessage Received");

  // flash LED to indicate receipt
  toggle_pin(pin_led, 100, 3);

  // Add mqtt message to the queue to be processed at next loop
  mqttMessageQueue.push(mqttMessage {strdup(topic), payload, length});
}

// Used to decipher incoming MQTT messages - currently hardcoded instances
void process_message()
{
  // Loop through queue to process all waiting mqtt messages
  while(mqttMessageQueue.count() > 0)
  {
    // Check if MQTT message is waiting
    DPRINT("\nMQTT messages waiting: ");
    DPRINTLN(mqttMessageQueue.count());

    DPRINTLN("\nMessage being processed");
    // Pull the latest - FIFO
    mqttMessage mqtt_msg = mqttMessageQueue.pop();

    //print_mqttMessage(mqtt_msg);

    // Extract mqtt message components from struct
    char* topic           = strdup(mqtt_msg.topic);

    // Check message topic
    if(strstr(topic, mqtt_topic_msg))
    {
      message curMessage = convertmqttMessageToMessage(mqtt_msg);

      //print_message(curMessage);

      bool queueWipe = curMessage.queueWipe;
      const char* msgAction = curMessage.msgAction;

      // Check if queue wipe is requested
      if(queueWipe){
        // Clear queue
        wipe_messages();
      }

      // Check for Delete action and that Queue wipe wasn't requested
      if(strcmp(msgAction, mqtt_msg_del) == 0 && !queueWipe) {
        // Attempt to delete message
        delete_message(curMessage.id);
        return;
      }

      // Check for Update action and that Queue wipe wasn't requested
      if(strcmp(msgAction, mqtt_msg_upd) == 0 && !queueWipe) {
        // Attempt to update message
        update_message(curMessage);
        return;
      }

      // Local variable to identify whether Insert action is necessary for Upsert action
      bool upsIns = false;
      // Check for Upsert action and that Queue wipe wasn't requested
      if(strcmp(msgAction, mqtt_msg_ups) == 0 && !queueWipe) {
        // Attempt to update message and set insert flag if update fails
        upsIns = !update_message(curMessage);
      }

      // Check for Insert OR Upsert AND and Failed Update
      if(strcmp(msgAction, mqtt_msg_ins) == 0 || (strcmp(msgAction, mqtt_msg_ups) == 0 && upsIns)){
        // Attempt to insert message
        add_message(curMessage);
        return;
      }
    }
    // Check if Reset topic specified
    else if (strstr(topic, mqtt_topic_rst))
    {
      DPRINTLN("\nSending restart command");
      ESP.restart();  // Not sure if this is the right command...
    }
    // Check if Info topic specified
    else if (strstr(topic, mqtt_topic_info))
    {
      DPRINT("\nInfo topic received");
      //TODO: Add message back w/ settings, status, etc.
    }
    // Unknown topic specified
    else {
      DPRINT("\nMessage topic unknown: ");
      DPRINTLN(topic);
    }
  }
}

// Pulls from the message queue for the next message to be displayed with appropriate details
void set_message()
{
  if(messageQueue.count() == 0) {
    return;
  }

  // Pull next message from queue to check duration length and run count
  message curMessage = messageQueue.pop();

  // Increment times run counter
  curMessage.timesRun++;

  // Check against calculated endTick or number of specified runs and text to see if message should be added back into queue to persist
  if((curMessage.endTick > millis() || curMessage.runCount > curMessage.timesRun) && strcmp(curMessage.text, "\0") != 0) {
    // Message should persist - add it back to the queue at last position
    add_message(curMessage);
  } else {
    // Message EOL or has no text - ignore and exit
    return;
  }

  print_message(curMessage);

  // Lookup indexes for Position and Effects
  textPosition_t  just        =   get_justification(curMessage.justification);
  textEffect_t    entryEffect =   get_effect(curMessage.entryEffect);
  textEffect_t    exitEffect  =   get_effect(curMessage.exitEffect);

  // Convert text to char*
  char* msgText = strdup(curMessage.text);

  // Set  message based on display type
  if(strcmp(curMessage.displayType, "SCROLL") == 0)
  {
    P.displayScroll(msgText, just, entryEffect, curMessage.speed);
  }
  else
  {
    P.displayText(msgText, just, curMessage.speed, curMessage.pause, entryEffect, exitEffect);
  }
}

// Set up parola library and initialize message queue
void setup_parola()
{
  P.begin();
  P.setInvert(false);

  // Override '^' with custom character for degrees F
  P.addChar('^', degF);

  // Check if startup message is enabled
  if(startup_msg_enabled){
    // Initialization message using settings from config
    P.displayText((char*)startup_msg_txt, startup_msg_just, startup_msg_speed, startup_msg_pause, startup_msg_entry_effect, startup_msg_exit_effect);
    // Initialization of sprites using settings from config
    P.setSpriteData(startup_msg_entry_sprite, startup_msg_entry_sprite_W, startup_msg_entry_sprite_F, startup_msg_exit_sprite, startup_msg_exit_sprite_W, startup_msg_exit_sprite_F);
  }
}

void setup()
{
  Serial.begin(115200);
  // Wait for Serial Port to Initialize
  while (!Serial) {}

  DPRINTLN("\nControllerId: ");
  DPRINT(controller_id);

  // Setup GPIO
  initialize_pins();

  // Setup Parola
  setup_parola();

  // Setup WiFi
  //wifi_connect();
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  wifiManager.autoConnect(wifi_ap_name);

  espClient.setCertificate(certificates_esp8266_bin_crt, certificates_esp8266_bin_crt_len);
  espClient.setPrivateKey(certificates_esp8266_bin_key, certificates_esp8266_bin_key_len);

  // Setup MQTT
  client.setCallback(receive_message);
  mqtt_connect();
}

void loop()
{
  if (!client.connected()) {
    // Attempt to reconnect
    mqtt_connect();
  }
  // Client connected
  else
  {
    // Parola animations are completed and waiting instructions
    // this is critical to avoid jitters and wdt rsts
    // it only processes MQTT responses between animations
    if(P.displayAnimate())
    {
      long now = millis();
      // Check for last subscription interval - to overcome receiving messages just stopping but still connected
      if(now - lastLoop > mqtt_loop_time){
        // MQTT routine
        client.loop();
        lastLoop = now;
        // Process waiting messages
        process_message();
      }
      // Check for last subscription interval - to overcome receiving messages just stopping but still connected
      if(now - lastSub > mqtt_resub_time){
        mqtt_subscribe();
        lastSub = now;
      }
      // Check for ping interval - to keep MQTT connection alive
      if(now - lastMsg > ping_int){
        // Send ping
        send_ping();
        // Reset timer
        lastMsg = now;
      }
      // Provide next sequenced message in queue
      set_message();
      //P.displayReset();
    }
  }
}
