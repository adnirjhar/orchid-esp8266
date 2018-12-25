#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Base64.h>

//===========================================
//  DEVICE CREDENTIALS
//  Customize this for each ESP Device
//===========================================
const char *instanceId = "XLVHOME01";
const char *deviceId = "ESPXD01";
//===========================================
//  SERVER DETAILS
//===========================================
const char *mainServer = "192.168.1.4";
const int mainServerPort = 8080;

//===========================================
//  SERVER USER CREDENTIALS
//===========================================
String serverUserName = "device@orchid.com";
String serverPassword = "deviceOrchid123";
String jSessionId = "";

//===========================================
//  API PATH VARIABLES
//===========================================
const char *signInAPI = "/orchid/api/account/login";
const char *deviceConfigAPI = "/orchid/api/admin/getESPConfig";

//===========================================
//  DATA FOR THIS DEVICE ONLY
//===========================================
int sensorPin5 = 5;
bool sensorPin5enabled = false;
int lastSensorPin5Value = 1;
int sensorPin6 = 6;
bool sensorPin6enabled = false;
int lastSensorPin6Value = 1;
int sensorPin7 = 7;
bool sensorPin7enabled = false;
int lastSensorPin7Value = 1;
int sensorPin8 = 8;
bool sensorPin8enabled = false;
int lastSensorPin8Value = 1;

//===========================================
//  Global Variables
//===========================================
WiFiClient httpClient;
WiFiClientSecure securedClient;

ESP8266WiFiMulti wifiMulti;
PubSubClient mqttClient(httpClient);

String ESPConfigResponse[6];
String relayPrefix = "RELAY";
String sensorPrefix = "SENSOR";

long previousMillis = 0;
unsigned long postInterval = 10000;


//===========================================
//  UTILITY METHODS
//===========================================
String encodeCredentials() {
    String cred = String(deviceId) + ":" + String(instanceId);
    Serial.println(cred);
    char inputString[cred.length()];
    cred.toCharArray(inputString,cred.length() + 1);
    int inputStringLength = sizeof(inputString);

    int encodedLength = Base64.encodedLength(inputStringLength);
    char encodedString[encodedLength];
    Base64.encode(encodedString, inputString, inputStringLength);

    return encodedString;
}

String decodeCredentials(String cred) {

    char inputString[cred.length() +1];
    cred.toCharArray(inputString, cred.length() +1);
    int inputStringLength = sizeof(inputString);

    int decodedLength = Base64.decodedLength(inputString, inputStringLength);
    char decodedString[decodedLength];
    Base64.decode(decodedString, inputString, inputStringLength);
    return decodedString;
}

int pinConverter(int boardPin) {
    if (boardPin == 0) return 16;
    if (boardPin == 1) return 5;
    if (boardPin == 2) return 4;
    if (boardPin == 3) return 0;
    if (boardPin == 4) return 2;
    if (boardPin == 5) return 14;
    if (boardPin == 6) return 12;
    if (boardPin == 7) return 13;
    if (boardPin == 8) return 15;
}

//===========================================
//  RESTORE SETTINGS TO PINS
//===========================================
void restoreSettingsToPins() {

    for (int pinDex = 1; pinDex <= 4; pinDex++) {
        int pinValue = (int)EEPROM.read(pinDex);
        if (pinValue == 1 || pinValue == 0) {
            digitalWrite(pinConverter(pinDex),pinValue);
            Serial.println("Restoring " + String(pinDex) + ": " + String(pinValue));
        }
    }
    int pinValue = (int)EEPROM.read(5);
    sensorPin5enabled = pinValue == 1;
    pinValue = (int)EEPROM.read(6);
    sensorPin6enabled = pinValue == 1;
    pinValue = (int)EEPROM.read(7);
    sensorPin7enabled = pinValue == 1;
    pinValue = (int)EEPROM.read(8);
    sensorPin8enabled = pinValue == 1;

}
//===========================================
//  RELAY METHODS
//===========================================
void processMsgForRelay(String relayMessage) {
    relayMessage.replace(relayPrefix,"");

    String pinNoStr = relayMessage.substring(0,relayMessage.indexOf("="));
    String pinValueStr = relayMessage.substring(relayMessage.indexOf("=") + 1,relayMessage.length());

    int pinNo = pinConverter(pinNoStr.toInt());
    int pinValue = pinValueStr.toInt() <= 0 ? HIGH : LOW;
    Serial.println("RELAY PIN:" + String(pinNoStr.toInt()) + " VALUE:" + String(pinValue));
    EEPROM.write(pinNoStr.toInt(),pinValue);
    EEPROM.commit();
    digitalWrite(pinNo,pinValue);
}

//===========================================
//  SENSOR METHODS
//===========================================
void processMsgForSensor(String sensorMessage) {
    sensorMessage.replace(sensorPrefix,"");

    String sensorNoStr = sensorMessage.substring(0,sensorMessage.indexOf("="));
    String sensorStatusStr = sensorMessage.substring(sensorMessage.indexOf("=") + 1,sensorMessage.length());

    int sensorNo = sensorNoStr.toInt();
    bool sensorEnabled = sensorStatusStr.toInt() > 0;

    if (sensorNo == 5) sensorPin5enabled = sensorEnabled;
    if (sensorNo == 6) sensorPin6enabled = sensorEnabled;
    if (sensorNo == 7) sensorPin7enabled = sensorEnabled;
    if (sensorNo == 8) sensorPin8enabled = sensorEnabled;
    Serial.println("SENSOR PIN:" + String(sensorNo) + " VALUE:" + String(sensorEnabled));
    EEPROM.write(sensorNo,sensorEnabled);
    EEPROM.commit();
}

//===========================================
//  MQTT METHODS
//===========================================
void processMQTTMessage(String message) {
    Serial.println("MESSAGE:" + message);

    bool msgIdentified = false;
    //===========================================
    //  PROCESS RELAY PREFIXED MESSAGES
    //===========================================
    if (message.indexOf(relayPrefix) >= 0) {
        processMsgForRelay(message);
    }
    //===========================================
    //  PROCESS SENSOR PREFIXED MESSAGES
    //===========================================
    if (message.indexOf(sensorPrefix) >= 0) {
        processMsgForSensor(message);
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int payloadLength) {
    //===========================================
    //  PROCESS MQTT MESSAGE
    //===========================================
    Serial.println("TOPIC: " + String(topic));
    String mqttResponse = "";
    for (int i = 0; i < payloadLength; i++) {
        mqttResponse += (char)payload[i];
    }
    processMQTTMessage(mqttResponse);
    mqttResponse = "";
}

void publishToMQTT(String message) {
    const char* messagePointer = message.c_str();
    const char* mqttCommonChannel = ESPConfigResponse[4].c_str();
    mqttClient.publish(mqttCommonChannel, messagePointer);
}

//===========================================
//  DETECT INPUT/OUTPUT METHODS
//===========================================
void publishSensorValue(int sensorPin, int sensorValue) {
    String msgType = "LOG";
    String dataToPost =
        msgType + "|" +
        String(deviceId) + "|" +
        String(instanceId) + "|" +
        sensorPrefix + String(sensorPin) + "|" +
        String(sensorValue);

    Serial.println("Posting:" + dataToPost);
    publishToMQTT(dataToPost);
}

void detectSensorTrigger() {
    //===========================================
    //  READ D5 VALUE
    //===========================================
    int pin1Value = digitalRead(pinConverter(sensorPin5));
    if (pin1Value != lastSensorPin5Value && sensorPin5enabled) {
        publishSensorValue(sensorPin5,pin1Value);
        lastSensorPin5Value = pin1Value;
    }
    //===========================================
    //  READ D6 VALUE
    //===========================================
    int pin2Value = digitalRead(pinConverter(sensorPin6));
    if (pin2Value != lastSensorPin6Value && sensorPin6enabled) {
        publishSensorValue(sensorPin6,pin2Value);
        lastSensorPin6Value = pin2Value;
    }
    //===========================================
    //  READ D7 VALUE
    //===========================================
    int pin3Value = digitalRead(pinConverter(sensorPin7));
    if (pin3Value != lastSensorPin7Value && sensorPin7enabled) {
        publishSensorValue(sensorPin7,pin3Value);
        lastSensorPin7Value = pin3Value;
    }
    //===========================================
    //  READ D8 VALUE
    //===========================================
    int pin4Value = digitalRead(pinConverter(sensorPin8));
    if (pin4Value != lastSensorPin8Value && sensorPin8enabled) {
        publishSensorValue(sensorPin8,pin4Value);
        lastSensorPin8Value = pin4Value;
    }
}

//===========================================
//  BOOTUP METHODS
//===========================================
void loginToWifi() {

    WiFi.mode(WIFI_STA);
    wifiMulti.addAP("Rust Chole", "grandship648");
    wifiMulti.addAP("Rust Chole_plus", "grandship648");
    wifiMulti.addAP("Anitam", "nirjhar12");

    Serial.println("Connecting Wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

template<class T> void loginToServer(T webClient) {
    jSessionId = "";
    if (!webClient.connect(mainServer, mainServerPort)) {
        Serial.println("Connection failed");
        return;
    }

    webClient.println("POST " + String(signInAPI) + " HTTP/1.0");
    webClient.println("Host: " + String(mainServer));
    webClient.println("Connection: Keep-Alive");
    webClient.println("Content-Type: application/json");

    String postData = "{\"email\":\"" + serverUserName + "\",\"password\":\"" + serverPassword + "\"}";

    webClient.print("Content-Length: ");
    webClient.println(postData.length());
    webClient.println();
    webClient.println(postData);

    String response = webClient.readString();
    jSessionId = response.substring(response.indexOf("JSESSIONID="),response.length());
    jSessionId = jSessionId.substring(0, jSessionId.indexOf(";"));
    Serial.println(jSessionId);
}

template<typename Client> void fetchESPConfiguration(Client& webClient) {
    if (jSessionId.length() == 0) {
        Serial.println("Device has to login to get cookie");
        return;
    }
    if (!webClient.connect(mainServer, mainServerPort)) {
        Serial.println("Connection failed");
        return;
    }

    String encodedCredUrl = String(deviceConfigAPI) + "/" + encodeCredentials();
    webClient.print(String("GET ") + encodedCredUrl + " HTTP/1.0\r\n" +
               "Host: " + mainServer + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n" +
               "Cookie: " + jSessionId +"\r\n\r\n");


    Serial.println("Request sent as:" + encodedCredUrl);

    String response = "";
    while(webClient.connected()) {
        if (webClient.available()) {
            char c = webClient.read();
            if (c == '[' || response.length() > 0 || c == ']') {
                response += String(c);
                if (c == ']') {
                    break;
                }
            }
        }
    }

    if (response.length() == 0) {
        Serial.println("Response was empty.");
        return;
    }
    response = response.substring(1,response.length() - 1);
    Serial.println("Response was:" + response);

    int a0 = 0, a1 = response.indexOf('|',a0);
    int b0 = a1 + 1, b1 = response.indexOf('|',b0);
    int c0 = b1 + 1, c1 = response.indexOf('|',c0);
    int d0 = c1 + 1, d1 = response.indexOf('|',d0);
    int e0 = d1 + 1, e1 = response.indexOf('|',e0);
    int f0 = e1 + 1, f1 = response.indexOf('|',f0);

    ESPConfigResponse[0] = response.substring(a0,a1); //mqttServer
    ESPConfigResponse[1] = response.substring(b0,b1); //mqttPort
    ESPConfigResponse[2] = response.substring(c0,c1); //mqttUser
    ESPConfigResponse[3] = response.substring(d0,d1); //mqttPassword
    ESPConfigResponse[4] = response.substring(e0,e1); //mqttCommonChannel
    ESPConfigResponse[5] = response.substring(f0,f1); //thisDeviceChannel

    webClient.stop();
}

void loginToMQTT() {

    const char* mqttServer = ESPConfigResponse[0].c_str();
    int mqttPort = ESPConfigResponse[1].toInt();
    const char* mqttUser = ESPConfigResponse[2].c_str();
    const char* mqttPassword = ESPConfigResponse[3].c_str();

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);

    Serial.println("Starting - MQTT...");
    while (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT... :" + mqttClient.state());
        if (mqttClient.connect(deviceId,mqttUser,mqttPassword)) {
            Serial.println("Connected as :" + String(deviceId));
        }
        else {
            Serial.println("Failed with state :" + mqttClient.state());
            delay(1000);
        }

    }
}

void subscribeToMQTT() {
    const char* thisDeviceChannel = ESPConfigResponse[5].c_str();
    mqttClient.subscribe(thisDeviceChannel);
    Serial.println("Subscribed to channel: " + ESPConfigResponse[5]);
}

void setupPinModes() {
    pinMode(pinConverter(0),OUTPUT);
    pinMode(pinConverter(1),OUTPUT);
    pinMode(pinConverter(2),OUTPUT);
    pinMode(pinConverter(3),OUTPUT);
    pinMode(pinConverter(4),OUTPUT);

    pinMode(pinConverter(5),INPUT_PULLDOWN_16);
    pinMode(pinConverter(6),INPUT_PULLDOWN_16);
    pinMode(pinConverter(7),INPUT_PULLDOWN_16);
    pinMode(pinConverter(8),INPUT_PULLDOWN_16);
}

void setup() {

    //===========================================
    //  SETUP ESP8266 DEVICE
    //===========================================
    Serial.begin(115200);
    Serial.println();

    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB only
    }

    Serial.println("=================loginToWifi============================");
    loginToWifi();
    Serial.println("========================================================");

    Serial.println("=================loginToServer============================");
    //===========================================================================
    //  IMPLEMENTATIONS
    //  USE EITHER THE HTTP OR THE HTTPS ONE BASED ON SITUATION
    //
    //===========================================================================
    mainServerPort == 443 ? loginToServer(securedClient) : loginToServer(httpClient);
    Serial.println("========================================================");

    Serial.println("=================fetchESPConfiguration==================");
    mainServerPort == 443 ? fetchESPConfiguration(securedClient) : fetchESPConfiguration(httpClient);
    Serial.println("========================================================");

    Serial.println("=================loginToMQTT============================");
    loginToMQTT();
    Serial.println("========================================================");

    Serial.println("=================subscribeToMqtt========================");
    subscribeToMQTT();
    Serial.println("========================================================");

    Serial.println("=================setupPinModes========================");
    setupPinModes();
    Serial.println("========================================================");

    Serial.println("=================restoreSettingsToPins============================");
    EEPROM.begin(8);
    restoreSettingsToPins();
    Serial.println("========================================================");
}

void sendHeartBeatOnInterval() {
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis > postInterval) {
        previousMillis = currentMillis;
        //===========================================
        //  UPDATE DATA EVERY [postInterval] Seconds
        //===========================================
        //Send heartbeat
    }
}

void loop() {
    mqttClient.loop();
    sendHeartBeatOnInterval();
    detectSensorTrigger();
}
