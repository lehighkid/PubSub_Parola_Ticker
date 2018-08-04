#ifndef main_h
#define main_h

#include <Arduino.h>

// Iterates until a wifi connection is made and status updated accordingly
void wifi_connect();

bool wifi_verifyTLS();

// Setup function to set proper pin state
void initialize_pins();

// GPIO toggle function to create light flash or trip relay, etc.
void toggle_pin(int pin, int duration, int intervals);

// Set up topics for subscription
void mqtt_subscribe();

// [Re]Connect to MQTT Broker then subscribe to topics
void mqtt_connect();

char* convert_payloadToChar(byte* payload, unsigned int length);

message convertmqttMessageToMessage(mqttMessage mqtt_msg);

// Debug function used to output mqtt message details
void print_mqttMessage(mqttMessage mqttMessage);

// Debug function used to output message details
void print_message(message message);

// Conversion function - int to textPosition_t
textPosition_t get_justification(int just);

// Conversion function - int to textEffect_t
textEffect_t get_effect(int effect);

// Used to send MQTT messages (aka publish)
void send_message(const char* channel, const char* data);

// Ping function to ensure keep alive on MQTT server - special call of send_message
void send_ping();

// Used to wipe out all messages - leaves messages queue empty
void wipe_messages();

// Used to delete specific message from messages queue - uses id
void delete_message(const char* id);

// Used to insert new message into messages queue
void add_message(message msg);

// Used to update message in message queue based on id
boolean update_message(message msg);

// Limited function to capture message receipt and leave flag for processing when animation idle
void receive_message(const char* topic, byte* payload, unsigned int length);

// Used to decipher incoming MQTT messages - currently hardcoded instances
void process_message();

// Pulls from the message queue for the next message to be displayed with appropriate details
void set_message();

// Set up parola library and initialize message queue
void setup_parola();

#endif
