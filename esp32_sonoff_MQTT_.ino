
#include "config.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <SimpleTimer.h>


WiFiClient espClient;
PubSubClient mqttClient(espClient);
SimpleTimer timer;

#define gpio13Led         13
#define gpio12Relay       12
#define SONOFF_BUTTON     0
#define mqttAllRequest "sonoff_all/request"
#define mqttAllResponse "sonoff_all/response"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqtt_server = MQTT_ADDRESS;

String hostName = "esp8266-" + deviceNames[hostNumber];
String mqttRequest =  hostName + "/request";
String mqttResponse = hostName + "/response";

int counter = 0;
int buttonState = HIGH;
static long startPress = 0;
bool mqConnected = false;


void setup(void) {
  pinMode(SONOFF_BUTTON, INPUT);
  attachInterrupt(SONOFF_BUTTON, changeButtonState, CHANGE);
  pinMode(gpio13Led, OUTPUT);
  changeLedStatus("off");

  pinMode(gpio12Relay, OUTPUT);
  changeRelayStatus("off");

  Serial.begin(9600);
  delay(2000);
  Serial.println("Connecting ... ");
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    if (counter % 2 == 0) {
      changeLedStatus("on");
    }
    else {
      changeLedStatus("off");
    }
    delay(600);
    Serial.print(".");
    ++counter;
    Serial.print("counter ==> ");
    Serial.println(counter);
    Serial.println(getRelayStatus());
    if (counter > 150 ) {
      if (isRelayOpen()) {
        counter = 0;
      }
      else {
        restart();
      }
    }
  }
  counter = 0;
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  changeLedStatus("on");
  delay(5000);
  changeLedStatus("off");
  delay(2000);

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callbackMQTT);

  otaProcess();
  timer.setInterval(60000L, myTimerEvent);
}

void loop(void) {
  //Serial.println(getRelayStatus());
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  else
  {
    mqttClient.loop();
  }
  ArduinoOTA.handle();
  timer.run();
}

void myTimerEvent() {
  Serial.println("myTimerEvent girdi...");
  if (mqttClient.connected()) {
      Serial.println(getRelayStatus());
      sendMqMessage(getRelayStatus());
  }
}

void changeButtonState() {
  controlButtonState();
}

void toogleRelayStatus() {
  //  Serial.println("toogleRelayStatus giris");
  if (isRelayOpen()) {
    digitalWrite(gpio12Relay, LOW);
  }
  else {
    digitalWrite(gpio12Relay, HIGH);
  }
  if (mqConnected) {
    sendMqMessage(getRelayStatus());
  }
}

void changeRelayStatus(String rStatus) {

  if (rStatus == "on") {
    digitalWrite(gpio12Relay, HIGH);
  }
  else {
    digitalWrite(gpio12Relay, LOW);
  }
  delay(100);
  if (mqConnected) {
    sendMqMessage(getRelayStatus());
  }
}

void reconnectMQTT() {
  Serial.println("reconnectMQTT girdi...");
  counter = 0;
  mqConnected = false;
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    changeLedStatus("off");
    // Attempt to connect
    if (WiFi.status() == WL_CONNECTED && mqttClient.connect(string2char(hostName))) {
      //Connected....
      mqConnected = true;
      Serial.println("connected");
      counter = 0;
      mqttClient.subscribe(string2char(mqttRequest));
      mqttClient.subscribe(string2char(mqttAllRequest));
      Serial.print("request status sending ==> ");
      Serial.println(getRelayStatus());
      sendMqMessage(getRelayStatus());
      changeLedStatus("on");

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 1 second");
      ++counter;
      Serial.print("counter ==> ");
      Serial.println(counter);
      Serial.println(getRelayStatus());
      if (counter > 10) {
        if (isRelayOpen()) {
          counter = 0;
        }
        else {
          restart();
        }
      }
      // Wait .3 seconds before retrying
      for (int i = 0; i <= 3; i++) {
        changeLedStatus("on");
        delay(200);
        changeLedStatus("off");
        delay(200);
      }
      ArduinoOTA.handle();
    }
  }
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived [");
  Serial.print(topic);
  String topicStr = String(topic);
  String value = "";
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    value += String(receivedChar);
  }
  printy(topicStr, value);

  if (topicStr == mqttRequest) {
    if (value == "on") {
      Serial.println("request on ");
      changeRelayStatus("on");
    }
    else if (value == "off") {
      Serial.println("request off ");
      changeRelayStatus("off");
    }
    else if (value == "status") {
      Serial.print("request status sending ==> ");
      Serial.println(getRelayStatus());
      sendMqMessage(getRelayStatus());

    }
  }
  else if(topicStr == mqttAllRequest){
    if (value == "showip") {
      Serial.println("showip");
      mqttClient.publish(string2char(mqttAllResponse), string2char(hostName + " ==>" + WiFi.localIP().toString()));
    }
  }
  
}

void changeLedStatus(String rStatus) {
  if (rStatus == "on") {
    digitalWrite(gpio13Led, LOW);
  }
  else {
    digitalWrite(gpio13Led, HIGH);
  }
}

String getRelayStatus() {
  return digitalRead(gpio12Relay) == HIGH ? "on" : "off";
}

boolean isRelayOpen() {
  Serial.println("isRelayOpen giris");
  return digitalRead(gpio12Relay) == HIGH ? true : false;
}

void sendMqMessage(String msg) {
  mqttClient.publish(string2char(mqttResponse), string2char(msg));
}

void otaProcess()
{
  //OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(string2char(hostName));
  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"xxx");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void printy(String p1, int p2)
{
  printy(p1, String(p2));
}

void printy(String p1, double p2)
{
  printy(p1, String(p2));
}

void printy(String p1, float p2)
{
  printy(p1, String(p2));
}
void printy(String p1, String p2)
{
  Serial.print(p1);
  Serial.print(" ==> ");
  Serial.println(p2);
}

char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
}

void restart() {
  Serial.println("reseting ...");
  changeLedStatus("off");
  delay(1000);
  ESP.reset();
  delay(1000);
}

void controlButtonState() {
  int currentState = digitalRead(SONOFF_BUTTON);
  if (currentState != buttonState) {
    if (buttonState == LOW && currentState == HIGH) {
      long duration = millis() - startPress;
      if (duration < 1000) {
        Serial.println("short press - toggle relay");
        toogleRelayStatus();
      } else if (duration < 5000) {
        Serial.println("medium press - reset");
        restart();
      } else if (duration < 60000) {
        Serial.println("long press - reset settings");
        restart();
      }
    } else if (buttonState == HIGH && currentState == LOW) {
      startPress = millis();
    }
    buttonState = currentState;
  }
}
