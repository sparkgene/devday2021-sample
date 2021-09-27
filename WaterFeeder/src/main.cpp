#include <M5StickC.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "app_config.h"

// Application setting
#define LOOP_INTERVAL 1000 * 1           // Detection cycle(1sec)
#define PUBLISH_INTERVAL 1000 * 60        // millsecond
uint32_t lcd_background = BLACK;

// MQTT client
WiFiClientSecure https_client;
PubSubClient mqtt_client(https_client);

// MQTT topic
#define TOPIC_BASE "devday2021/"
#define TOPIC_PUMP_CONTROL TOPIC_BASE "cmd/" AWS_IOT_THING_NAME
#define TOPIC_MOISTURE_DATA TOPIC_BASE "data/" AWS_IOT_THING_NAME

// sensor pins
#define INPUT_PIN 33
#define PUMP_PIN 32
#define MOISTURE_MAX_VALUE 2100.0     // sensor dry value
#define MOISTURE_RANGE_VALUE 600.0

bool pumpStatus = false;
int rawADC;
DynamicJsonDocument data_json(1000);
unsigned long last_send_ms = 0, current_ms = 0;

/**
 * Draw network connection status
 **/
void drawConnectionStatus(const char text[]){
    M5.Lcd.fillRect(1, 1, M5.Lcd.width(), 20, lcd_background);
    M5.Lcd.drawString(text, 1, 1);
}

/**
 * Connect to WiFi access point
 **/
void connect_wifi()
{
    Serial.print("Connecting: ");
    Serial.println(APP_CONFIG_WIFI_SSID);
    drawConnectionStatus("disconnecting");

    WiFi.disconnect(true);
    delay(1000);

    char msg[255];
    sprintf(msg, "connecting to %s", APP_CONFIG_WIFI_SSID);
    drawConnectionStatus(msg);
    WiFi.begin(APP_CONFIG_WIFI_SSID, APP_CONFIG_WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }
    Serial.println("WiFi Connected");
    Serial.print("IPv4: ");
    Serial.println(WiFi.localIP().toString().c_str());
    sprintf(msg, "connected: %s", APP_CONFIG_WIFI_SSID);
    drawConnectionStatus(msg);
}

/**
 * Reconnect to WiFi access point
 **/
void reconnect_wifi()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        connect_wifi();
    }
}

/**
 * Subscribe callback
 * This callback recives all subscribed messages
 **/
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived: ");
    Serial.println(topic);

    String recievedCommand = "";
    for (int i = 0; i < length; i++)
    {
        recievedCommand += (char)payload[i];
    }
    Serial.println(recievedCommand);
    DynamicJsonDocument command_json(1000);
    deserializeJson(command_json, recievedCommand.c_str());
    if(command_json["pump"] == 1){
        pumpStatus = true;
    }
    else{
        pumpStatus = false;
    }
    digitalWrite(PUMP_PIN, pumpStatus);
    last_send_ms = 0;
}

/**
 * Initialize MQTT connection
 **/
void init_mqtt()
{
    https_client.setCACert(AWS_ROOT_CA_CERTIFICATE);
    https_client.setCertificate(AWS_IOT_CERTIFICATE);
    https_client.setPrivateKey(AWS_IOT_PRIVATE_KEY);
    mqtt_client.setServer(AWS_IOT_ENDPOINT, AWS_IOT_MQTT_PORT);
    mqtt_client.setCallback(callback);
    mqtt_client.setBufferSize(AWS_IOT_MQTT_MAX_PAYLOAD_SIZE);
}

/**
 * Connect to AWS IoT
 **/
void connect_awsiot()
{
    reconnect_wifi();

    while (!mqtt_client.connected())
    {
        Serial.println("Start MQTT connection...");
        if (mqtt_client.connect(AWS_IOT_THING_NAME)) {
            Serial.println("connected");
            // subscribe to command topic
            mqtt_client.subscribe(TOPIC_PUMP_CONTROL);
        }
        else {
            Serial.print("[WARNING] failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println("try again in 5 seconds.");
            delay(5000);
        }
    }
}


void setup() { 
    Serial.begin(115200);
    M5.begin();
    M5.Axp.ScreenBreath(15);
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(lcd_background);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.setTextFont(2);

    pinMode(INPUT_PIN,INPUT);
    pinMode(PUMP_PIN,OUTPUT);

    // initialize data JSON
    DeserializationError err = deserializeJson(data_json, "{\"moisture\": 0, \"pump\": \"off\"}");
    if (err) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(err.c_str());
    }

    connect_wifi();
    init_mqtt();
}

void loop() { 

    connect_awsiot();

    mqtt_client.loop();

    M5.update();
    rawADC = analogRead(INPUT_PIN);
    int moisture = (MOISTURE_MAX_VALUE - rawADC) / MOISTURE_RANGE_VALUE * 100;
    if(M5.BtnA.wasPressed()){
        pumpStatus = !pumpStatus;
        digitalWrite(PUMP_PIN, pumpStatus);
        last_send_ms = 0;
    }

    M5.lcd.fillRect(0, 30, 160, 100, lcd_background);
    M5.Lcd.setCursor(5, 30);
    String pumpString = pumpStatus? "on" : "off";
    M5.Lcd.print("Pump Status: " + pumpString);
    Serial.println("Pump: " + pumpString);

    M5.Lcd.setCursor(5, 60);
    M5.Lcd.print("Moisture: " + String(moisture) + " %");
    Serial.println("Moisture: " + String(moisture) + " %");

    if( mqtt_client.connected() ){
        unsigned long current_ms = millis();
        if(last_send_ms == 0 || current_ms - last_send_ms > PUBLISH_INTERVAL) {
            data_json["moisture"] = moisture;
            data_json["pump"] = pumpStatus? 1 : 0;

            String data_string;
            serializeJson(data_json, data_string);
            Serial.print("publish message :");
            Serial.println(data_string.c_str());

            last_send_ms = current_ms;
            mqtt_client.publish(TOPIC_MOISTURE_DATA, data_string.c_str(), true);  // use retain message
        }
    }

    delay(LOOP_INTERVAL);
}