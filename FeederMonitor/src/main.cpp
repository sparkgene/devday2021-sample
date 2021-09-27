#include <M5Core2.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "app_config.h"

// Application setting
#define LOOP_INTERVAL 100 * 5           // Detection cycle(500 msec)
uint32_t lcd_background = BLACK;

// MQTT client
WiFiClientSecure https_client;
PubSubClient mqtt_client(https_client);

// Monitor device
#define MONITOR_DEVICE "DevDayWaterFeeder"

// MQTT topic
#define TOPIC_BASE "devday2021/"
#define TOPIC_PUMP_CONTROL TOPIC_BASE "cmd/" MONITOR_DEVICE
#define TOPIC_MOISTURE_DATA TOPIC_BASE "data/" MONITOR_DEVICE

#define BTN_LEFT 50
#define BTN_TOP 150
#define BTN_WIDTH 220
#define BTN_HEIGTH 50
#define BTN_TITLE_LEFT 142
#define BTN_TITLE_TOP 155

bool pumpStatus = false;
int rawADC;
DynamicJsonDocument command_json(1000);

void drawConnectionStatus(const char text[]){
    M5.Lcd.fillRect(1, 1, M5.Lcd.width(), 20, lcd_background);
    M5.Lcd.drawString(text, 1, 1);
}

void drawMoisture(int moisture_percentage){
    M5.Lcd.fillRect(0, 40, M5.Lcd.width(), 100, lcd_background);
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print("Moisture: " + String(moisture_percentage) + " %");
    M5.Lcd.drawRect(10, 80, 300, 50, BLUE);
    double gauge_width = 290.0;
    double width = ((double)moisture_percentage / 100.0) * gauge_width;
    Serial.println(width);
    if (width > gauge_width ){
      width = gauge_width;
    }
    else if( width < 0 ){
      width = 0;
    }
    M5.Lcd.fillRect(15, 85, (int)width, 40, CYAN);
}

void drawButton(bool pump_status){

    M5.Lcd.fillRect(0, 140, M5.Lcd.width(), 100, lcd_background);
    if(pump_status){
        M5.Lcd.fillRoundRect(BTN_LEFT, BTN_TOP, BTN_WIDTH, BTN_HEIGTH, 5, GREEN);
        M5.Lcd.setTextColor(WHITE);
    }
    else{
        M5.Lcd.fillRoundRect(BTN_LEFT, BTN_TOP, BTN_WIDTH, BTN_HEIGTH, 5, LIGHTGREY);
        M5.Lcd.setTextColor(BLACK);
    }
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(BTN_TITLE_LEFT, BTN_TITLE_TOP);
    if(pump_status){
        M5.Lcd.printf("O N");
    }
    else{
        M5.Lcd.printf("OFF");
    }
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

    String recievedData = "";
    for (int i = 0; i < length; i++)
    {
        recievedData += (char)payload[i];
    }
    Serial.println(recievedData);
    DynamicJsonDocument data_json(1000);
    deserializeJson(data_json, recievedData.c_str());
    drawMoisture(data_json["moisture"]);
    if(data_json["pump"] == 1){
        if(!pumpStatus){
            pumpStatus = true;
            drawButton(pumpStatus);
        }
    }
    else{
        if(pumpStatus){
          pumpStatus = false;
          drawButton(pumpStatus);
        }
    }
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
            // recive moisture data
            mqtt_client.subscribe(TOPIC_MOISTURE_DATA);
        }
        else {
            Serial.print("[WARNING] failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println("try again in 5 seconds.");
            delay(5000);
        }
    }
}

/**
 * Initial device setup
 **/
void setup()
{
    Serial.begin(115200);
    M5.begin();
    M5.Lcd.fillScreen(lcd_background);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.setTextFont(2);

    // initialize data JSON
    DeserializationError err = deserializeJson(command_json, "{\"pump\": \"off\"}");
    if (err) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(err.c_str());
    }

    connect_wifi();
    init_mqtt();

    drawButton(pumpStatus);
    drawMoisture(0);
}

/**
 * Main loop processing
 **/
void loop()
{

    connect_awsiot();

    mqtt_client.loop();

    M5.update();
    TouchPoint_t pos= M5.Touch.getPressPoint();
    if( pos.x > BTN_LEFT && pos.x < (BTN_LEFT + BTN_WIDTH) && pos.y > BTN_TOP && pos.y < (BTN_TOP+BTN_HEIGTH)){
        pumpStatus = !pumpStatus;
        drawButton(pumpStatus);
        M5.Axp.SetLDOEnable(3, true);
        delay(75);
        M5.Axp.SetLDOEnable(3, false);
        command_json["pump"] = pumpStatus? 1 : 0;
        String data_string;
        serializeJson(command_json, data_string);
        Serial.print("publish message :");
        Serial.println(data_string.c_str());
        mqtt_client.publish(TOPIC_PUMP_CONTROL, data_string.c_str());
    }

    delay(LOOP_INTERVAL);
}