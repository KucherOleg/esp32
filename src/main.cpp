//#include "CSE7766.h"
#include "PubSubClient.h"
#include "main.h"
#include "sav_button.h"
//#include "GetStatusRelay.h"
#include "read_config.h"




WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
int j = 0;
int attempt = 0;
int countClient = 0; //количество клиентов
unsigned long time_data, timeOnOffBlue;
unsigned long epcohTime =0;

bool readSpiffs() {
    if (!LittleFS.begin()) {Serial.println("failed to mount FS"); return (false);}
    Serial.println("mounted file system");

    File txtFile = LittleFS.open("/ssid.txt", "r");
    if (!txtFile) { Serial.println("отсутствует ssid.txt"); LittleFS.end(); return (false);}
    ssid = txtFile.readString();
    Serial.println(ssid);

    txtFile = LittleFS.open("/password.txt", "r");
    if (!txtFile) {Serial.println("отсутствует password.txt"); LittleFS.end(); return (false);}
    password = txtFile.readString();
    Serial.println(password);
        
    txtFile = LittleFS.open("/deviceId.txt", "r");
    if (!txtFile) {Serial.println("отсутствует deviceId.txt"); LittleFS.end(); return (false);}
    String Id  = txtFile.readString();
    Id.trim();
    deviceId = Id;
    int indexOrg = Id.indexOf('/');
    orgId = "";
    if (indexOrg == -1) {Serial.println("отсутствует организация"); LittleFS.end(); return (false);}
    orgId = Id.substring(0,indexOrg);
    int indexProject = Id.indexOf('/', indexOrg+1);
    projectId = "";
    deviceId = "";
    if (indexProject == -1) {Serial.println("отсутствует проект"); LittleFS.end(); return (false);}
    projectId = Id.substring(indexOrg+1,indexProject);
    deviceId  = Id.substring(indexProject+1);    
    txtFile = LittleFS.open("/TH10.ver", "r");
    if (!txtFile) {Serial.println("отсутствует TH10.ver"); LittleFS.end(); return (false);}
    FW_VERSION  = txtFile.readString();
    Serial.println(FW_VERSION);
    LittleFS.end();
    return (true);
}

void sendData(unsigned long time) {
    const int capacity = JSON_OBJECT_SIZE(2)+50;
    DynamicJsonDocument event(capacity);
    boolean publishMQTT;
    String payload;
    JsonArray sensors = sensorsDJD["sensors"];

    for (int i=0; i < sensorCount; i++) {
        String sensorId = sensors[i]["id"];
        String sensorModel = sensors[i]["model"];
        Serial.printf("Sensor: %s", sensorModel.c_str());
        float oldFirst = siTemperature;
        float oldSecond = siHumidity;
        if (sensorModel == String("Si7021")) ;//readSi7021();
        if (sensorModel == String("DS18B20")) siTemperature = DS18();
//        if (sensorModel == String("POW")) readCSE7766();
        siTemperature = round(siTemperature*10.0)/10.0;
        siHumidity = round(siHumidity*10.0)/10.0;
        event["eventTime"].set(time);
        for (j=0; j < sensors[i]["valuesCount"]; j++) {
            String velueId = sensors[i]["values"][j]["id"];
            String velueIdUp = velueId;
            velueIdUp.toUpperCase();
            publishMQTT = true;
            payload="";
            event["value"] = String("0");
            if (velueIdUp == String("HUMIDITY")) {
               event["value"] = String(siHumidity, 1);
               if (oldSecond == siHumidity) publishMQTT = false;
            }                
            if (velueIdUp == String("TEMPERATURE")) {
               event["value"] = String(siTemperature, 1);
               if (oldFirst == siTemperature) publishMQTT = false; 
            }
            if (velueIdUp == String("VOLTAGE")) {
               event["value"] = String(siTemperature, 1);
               if (oldFirst == siTemperature) publishMQTT = false; 
            }
            if (velueIdUp == String("CURRENT")) {
               event["value"] = String(siHumidity, 1);
               if (oldSecond == siHumidity) publishMQTT = false;
            }        
            if (publishMQTT) {
                serializeJson(event, payload);
                Serial.printf(" %s", payload.c_str());
                String topicSensor = "org/" + orgId +"/project/"+projectId+"/device/"+deviceId+"/sensor/"+sensorId +"/value/"+ velueId;
                mqttClient.publish(topicSensor.c_str(), payload.c_str());
            }
        }
        Serial.println();
    }
}

void callback(char* topicChar, byte* payload, int length) {

    String value = "";
    for(int i=0; i<length; i++){
        char c = (char) payload[i];
        value+=c;
    }
    const int capacity = JSON_OBJECT_SIZE(1)+30;
    DynamicJsonDocument event(capacity);
    deserializeJson(event,value);
    Serial.printf("Получил из %s : %s\n", topicChar, value.c_str());
    String topic = "org/" + orgId + "/project/" + projectId + "/device/" + deviceId + "/config/response";
    if (strcmp(topicChar, topic.c_str()) == 0) {
        getConfig = true;
        setConfig(value);
    }
    else {
        topic = "org/" + orgId + "/project/" + projectId + "/device/" + deviceId + "/command";
        if(strcmp(topicChar, topic.c_str()) == 0) {
            String httpUrl = event["update"];
            if ( !httpUrl.isEmpty() ) updateFirmware();
        }
        else {
            JsonArray relays = relaysDJD["relays"];
            for (int i=0; i < relayCount; i++){  // Для всех реле
                const char* relayId = relays[i]["id"];
                topic = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId);
                topicRelay = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId) + "/event";
                if (strcmp(topicChar, topic.c_str()) == 0) {
                    switch (int(event["value"])) {
                    case 0:
                        mqttClient.publish(topicRelay.c_str(), "{\"value\":0}", true);
                        break;
                    case 1:
                        mqttClient.publish(topicRelay.c_str(), "{\"value\":1}", true);
                        break;
                    }
                }
                if (strcmp(topicChar, topicRelay.c_str()) == 0) {
                    switch (int(event["value"])) {
                    case 0:
                        digitalWrite(Red[i], LOW);
                        sw[i] = false;
                        break;
                    case 1:
//                    pinMode(Red[i], OUTPUT);
                        digitalWrite(Red[i], HIGH);
                        sw[i] = true;
                        break;
            
                    }
                }
            }
        } 
    }   
}

void setup() {
    Serial.begin(9600);
    while (!Serial)delay(10);
    delay(1000);
    Serial.println("Booting");
    countMQTT = 0;
    
    readSpiffs();
    pinMode(Red[0], OUTPUT);
    pinMode(Blue, OUTPUT);
    
    pinMode(0, INPUT_PULLUP);
    int memButton = digitalRead(0);
    if ( memButton == 1 ) sw[0] = false; else sw[0] = true;

    Serial.println(ssid + " : " + password);
    Serial.println("Организация : " + orgId);           
    Serial.println("Проект : " + projectId);
    Serial.println(deviceId);
    digitalWrite(Blue, HIGH);
    while (WiFi.status() != WL_CONNECTED) {
        
        attempt++;
        Serial.print("Подключамся к wi-fi. Попытка "); Serial.println(attempt);
        connectWifi();
//        delay(50);        
        if (attempt > 4 || orgId.isEmpty() || projectId.isEmpty() || deviceId.isEmpty()) {
            configureWIFI();
            attempt = 0;
        }
        digitalWrite(Blue, 1 - digitalRead(Blue));
    }
    attempt = 0;
    digitalWrite(Blue, LOW);
    
    Serial.println(ssid + " : " + password);
    Serial.println("Организация : " + orgId);           
    Serial.println("Проект : " + projectId);
    Serial.println(deviceId);
    chip_Id = "IDESP32";// + ESP.getEfuseMac();
    Serial.println("Chip " + chip_Id);
    Serial.println("");

// обновление прошивки
//    updateFirmware();

    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    mqttClient.setCallback(callback);
    mqttClient.setBufferSize(1024);

    sensors.begin();
    delay(10);
    //si.begin();
    delay(500);
    
// Initialize CSE7766 for POW
//    myCSE7766.setRX(1);
//    myCSE7766.begin(); // will initialize serial to 4800 bps

    time_data = millis();
    timeOnOffBlue= millis();
    timeClient.begin();
       
}

void loop() {
    mqttClient.loop();
    if (WiFi.status() != WL_CONNECTED){
        if (millis()-timeOnOffBlue > 500) { 
            digitalWrite(Blue, 1 - digitalRead(Blue));
            delay(50);
            digitalWrite(Blue, 1 - digitalRead(Blue));
            timeOnOffBlue = millis();
        }
        attempt++;
        connectWifi();
    } else digitalWrite(Blue, LOW);
    if (!mqttClient.connected()) {
        connectMQTT();
        subscribeAll();
        if(getConfig == false){
            String topic = "org/" + orgId + "/project/" + projectId + "/device/" + deviceId + "/config/request";
            mqttClient.publish(topic.c_str(), "00");
            Serial.printf("Публикую 00 в %s\n", topic.c_str());
            attempt++;
            mqttClient.loop();
        }        
    }
    if (attempt > 10) {
        configureWIFI();
        attempt = 0;
    }
    
    JsonArray relays = relaysDJD["relays"];
    for(int i=0; i < relayCount; i++){
        const char* relayId = relays[i]["id"];
        switch (i) {
        case 0 :
            switch (myButton1.Loop()) {
            case SB_CLICK :
                sw[i] = !sw[i];
                digitalWrite(Red[i], sw[i]);
                topicRelay = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId) + "/event";
                if (sw[i]) mqttClient.publish(topicRelay.c_str(), "{\"value\":1}", true);
                else mqttClient.publish(topicRelay.c_str(), "{\"value\":0}", true);
                break;
            case SB_LONG_CLICK :
                configureWIFI();
                break;
            }
            break;
        case 1 :
            switch (myButton2.Loop()) {
            case SB_CLICK :
                sw[i] = !sw[i];
                digitalWrite(Red[i], sw[i]);
                topicRelay = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId) + "/event";
                if (sw[i]) { 
                    mqttClient.publish(topicRelay.c_str(), "{\"value\":1}", true); 
                }
                else { 
                    mqttClient.publish(topicRelay.c_str(), "{\"value\":0}", true);
                }
                break;
            }
            break;
        case 2 :
            switch (myButton3.Loop()) {
            case SB_CLICK :
                sw[i] = !sw[i];
                digitalWrite(Red[i], sw[i]);
                topicRelay = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId) + "/event";
                if (sw[i]) { 
                    mqttClient.publish(topicRelay.c_str(), "{\"value\":1}", true);
                }
                else { mqttClient.publish(topicRelay.c_str(), "{\"value\":0}", true); }
                break;
            }
            break;
        case 3:
            switch (myButton4.Loop()) {
            case SB_CLICK :
                sw[i] = !sw[i];
                digitalWrite(Red[i], sw[i]);
                topicRelay = "org/" + orgId +"/project/"+projectId+"/device/" + deviceId + "/relay/" + String(relayId) + "/event";
                if (sw[i]) { mqttClient.publish(topicRelay.c_str(), "{\"value\":1}", true); }
                else { mqttClient.publish(topicRelay.c_str(), "{\"value\":0}", true); }
                break;   
            }         
        }
    }
    if ( sensorCount > 0 ) {
        if (millis() - time_data > 9999) { // Пауза для данных 10 c.
            time_data = millis();
            if (WiFi.status() == WL_CONNECTED) {
                timeClient.update();
                epcohTime =  timeClient.getEpochTime();
            }
            sendData(epcohTime);
            if (mqttClient.connected()) mqttClient.loop();
        }
    }

    // Give a time for ESP
    yield();
//    delay(1000);
}
