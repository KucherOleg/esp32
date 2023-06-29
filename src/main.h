#include <WiFi.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <DHT.h>

//настройки сервера обновлений
const char *SDLab_httpUpdate = "http://www.sdlab.zzz.com.ua/esp32/";
//const char *update_user = "sdlab@sdlab.zzz.com.ua";
//const char *update_pass = "SDlab459-11-34";

// сервер MQTT
String mqtt_server = "mqtt.dev.sdlab.io"; // Брокер MQTT
// сервер API
String api_server = "dev.sdlab.io";
const int mqtt_port = 1883; // порт брокера MQTT

String chip_Id;

WiFiClient wClient;
PubSubClient mqttClient(wClient);
const char *mqtt_user = "admin";
const char *mqtt_pass = "public";
int countMQTT = 0; // количество попыток подключений к MQTT
int sensorCount, relayCount;

OneWire oneWire(14);  // датчик DS18B20 
DallasTemperature sensors(&oneWire);
//CSE7766 myCSE7766;

//DHT si(14, DHT21); // датчик Si7021
float siTemperature, siHumidity;
WebServer server(80);
boolean serverOK = false;
String ssid, password, deviceId, projectId, orgId;
String FW_VERSION;
uint8_t Red[4] = {12,5,4,15}; // PIO красных лампочек в 4CH
uint8_t Blue = 13;
byte mac[6];
boolean sw[4]; // состояние кнопок реле 
boolean getConfig = false;

/*void readSi7021() {
    // Определяем температуру от датчика AM2301 или Si7021
    si.read();
    delay(100);
    siTemperature = si.readTemperature();
    siHumidity = si.readHumidity();
}*/

/*void readCSE7766() {
    // Определяем ток и напряжение для POW
    // read CSE7766
    myCSE7766.handle();
    delay(100);
    siTemperature = myCSE7766.getVoltage();
    siHumidity = myCSE7766.getCurrent();
    char outstr[15];
    dtostrf(siTemperature,7, 2, outstr);
    Serial.printf("Voltage: %.4f V", siTemperature);
    dtostrf(siHumidity,7, 2, outstr);
    Serial.printf("  Current: %.4f A", siHumidity);
    Serial.printf("  Energy %.4f W\n", myCSE7766.getEnergy());

}*/

float DS18() {
    // Определяем температуру от датчика DS18
    sensors.requestTemperatures();
    delay(100);
    float temperature = sensors.getTempCByIndex(0);
    return (temperature);
}

int compareTerminal(JsonVariantConst a, JsonVariantConst b) {
  return a["pin"].as<int>() - b["pin"].as<int>();
}

void swap(JsonVariant a, JsonVariant b) {
  StaticJsonDocument<300> tmp = a;
  a.set(b);
  b.set(tmp);
}

void sortByTerminal(JsonArray arr) {
  for (auto i = arr.begin(); i != arr.end(); ++i)
    for (auto j = i; j != arr.end(); ++j)
      if (compareTerminal(*i, *j) > 0)
        swap(*i, *j);
}

void handleRoot() {
  String html_header = "<html>\
 <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\
 <head>\
   <title>ESP8266 Settings</title>\
   <style>\
     body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
   </style>\
 </head>";
  String str = "";
  str += html_header;
  str += "<body>\
   <form method=\"POST\" action=\"ok\">\
     <input name=\"ssid\" value=\""+String(ssid)+"\" size=\"18\"> Ssid your WIFI</br></br>\
     <input name=\"pswd\" value=\""+String(password)+"\"size=\"18\"> Password</br></br>\
     <input name=\"orgId\" value=\""+String(orgId)+"\"size=\"36\"> Organization</br></br>\
     <input name=\"projectId\" value=\""+String(projectId)+"\"size=\"36\"> Project</br></br>\
     <input name=\"deviceId\" value=\""+String(deviceId)+"\"size=\"36\"> deviceId</br></br>\
     <input type=SUBMIT value=\"Save settings\">\
   </form>\
  </body>\
  </html>";
  server.send ( 200, "text/html", str );
}

void handleOk() {
    String ssid_ap, pass_ap, deviceId_ap, projectId_ap, orgId_ap;
    String str;

    ssid_ap = server.arg(0);
    pass_ap = server.arg(1);
    orgId_ap = server.arg(2);
    projectId_ap = server.arg(3);
    deviceId_ap = server.arg(4);

    if (ssid_ap != "") {
        LittleFS.begin();
        File txtFile = LittleFS.open("/ssid.txt", "w+");
        if (txtFile) {
            ssid_ap.trim();
            txtFile.print(ssid_ap);
            txtFile.close();
        }
        txtFile = LittleFS.open("/password.txt", "w+");
        if (txtFile) {
            pass_ap.trim();
            txtFile.print(pass_ap);
            txtFile.close();
        }
        txtFile = LittleFS.open("/deviceId.txt", "w+");
        if (txtFile) {
            deviceId_ap.trim();
            projectId_ap.trim();
            orgId_ap.trim();
            txtFile.print(orgId_ap + "/" + projectId_ap + "/" + deviceId_ap);
            txtFile.close();
        }
        txtFile = LittleFS.open("/TH10.ver", "r");
        if (!txtFile){
            txtFile = LittleFS.open("/TH10.ver", "w+");
            if (txtFile) txtFile.print("001");
        }
        txtFile.close();
        LittleFS.end();
        serverOK = true;
    }

    str = "{\"id\":\"" + deviceId_ap + "\",\"mac\":\"" + String(mac[0], 16) + String(mac[1], 16) + String(mac[2], 16) +
          String(mac[3], 16) + String(mac[4], 16) + String(mac[5], 16) +"\",\"VER\":\"" + FW_VERSION + "\"}";
    server.send(200, "application/json", str);
    delay(1000);
    server.stop();
}

void handleboot() {
  ESP.restart();
  delay(1000);
}

void connectWifi(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    WiFi.waitForConnectResult();
    if (WiFi.status() == WL_CONNECTED)digitalWrite(Blue, LOW);
}

void configureWIFI() {
    serverOK = false;
    digitalWrite(Blue, HIGH);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP_start");
    WiFi.macAddress(mac);
    delay(1000);
    server.on("/", handleRoot);
    server.on("/ok", handleOk);
    server.on("/boot", handleboot);
    server.begin();
    unsigned long timeWifi = millis();
    unsigned long timeBlue = timeWifi;
    while (true) {
        server.handleClient();
        if (serverOK) break;
        if (millis()-timeBlue > 500) { 
            digitalWrite(Blue, 1 - digitalRead(Blue));
            timeBlue = millis();
        }
        if (millis() - timeWifi > 180000) break; // Ожидание новых данных 3 мин.
    }
    WiFi.disconnect();
    mqttClient.disconnect();
    ESP.restart();
    delay(1000);
}
