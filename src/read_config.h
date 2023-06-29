#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>


DynamicJsonDocument sensorsDJD(1000);
DynamicJsonDocument relaysDJD(1000);
String topicRelay;

SButton myButton1(0, 50, 2000, 0, 0, 0);
SButton myButton2(9, 50, 2000, 0, 0, 0);
SButton myButton3(10, 50, 2000, 0, 0, 0);
SButton myButton4(14, 50, 2000, 0, 0, 0);


void connectMQTT() {
    String topicDevice = "org/" + orgId + "/project/"+projectId+"/device/"+deviceId;
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            Serial.printf("Connecting to MQTT server %s atempt %d\n", chip_Id.c_str(), countMQTT);
            if (mqttClient.connect(chip_Id.c_str(), mqtt_user, mqtt_pass, topicDevice.c_str(), 1, true, "{\"connectivityStatus\":2}", false)) {
                Serial.printf("Connected to MQTT server, Id %s\n", chip_Id.c_str());
                countMQTT = 0;
                mqttClient.subscribe(topicDevice.c_str());
                String topicPayload = "{\"connectivityStatus\":1, \"firmware\":\"" + FW_VERSION.substring(FW_VERSION.indexOf('.')+1, FW_VERSION.lastIndexOf('.')) + "\"}";
                mqttClient.publish(topicDevice.c_str(), topicPayload.c_str());
                Serial.printf("Публикую %s {\"connectivityStatus\":1}\n", topicDevice.c_str());              
            } else {
                countMQTT++;
                Serial.printf(" %i - Could not connect to MQTT server %s\n", mqttClient.state(), deviceId.c_str());
                if (countMQTT > 10) {
                    WiFi.disconnect();
                    delay(1000);
                    ESP.restart();
                    delay(1000);
                }
            }
        }
    }
}

void subscribeAll() {   
    if (WiFi.status() == WL_CONNECTED) {
        String topic = "org/" + orgId + "/project/" + projectId + "/device/" + deviceId + "/config/response";
        mqttClient.subscribe(topic.c_str());
        Serial.printf("Подписываю %s\n", topic.c_str());
        String topicDevice = "org/" + orgId +"/project/"+projectId+"/device/"+deviceId;
        JsonArray relays = relaysDJD["relays"];
                for (int i=0; i < relayCount; i++){
                    const char* relayId = relays[i]["id"];
                    topicRelay = topicDevice + "/relay/" + String(relayId);
                    Serial.printf("Подписываю %s\n", topicRelay.c_str());
                    mqttClient.subscribe(topicRelay.c_str());
                    topicRelay = topicRelay + "/event";
                    Serial.printf("Подписываю %s\n", topicRelay.c_str());
                    mqttClient.subscribe(topicRelay.c_str());
                    topicRelay = topicDevice + "/command";
                    Serial.printf("Подписываю %s\n", topicRelay.c_str());
                    mqttClient.subscribe(topicRelay.c_str());
                }
    }    
    else {
        ESP.restart();
        delay(1000);
    }
}

void setConfig( String newConfig ) {
    DynamicJsonDocument configDevice(2000);
    boolean status = false;
    String apiRelay = "/api/v1/events/relay/";
    DeserializationError error = deserializeJson(configDevice,newConfig);
    if (error) {
        Serial.println("Нет ответа API");
        if (LittleFS.begin()) {
            File txtFile = LittleFS.open("/config.txt", "r");
            if (txtFile) {
                newConfig = txtFile.readString();
                txtFile.close();
                Serial.println("Конфигурация восстановлена");
            }
            LittleFS.end();
        }
        error = deserializeJson(configDevice, newConfig);
        if (error){
            Serial.printf("deserializeJson() failed: %s\n", error.c_str());
            mqttClient.disconnect();
            WiFi.disconnect();
            delay(1000);
            ESP.restart();
            delay(1000);
        }
    }
    else {
        if (LittleFS.begin()) {
            File txtFile = LittleFS.open("/config.txt", "w+");
            if (txtFile) {
                newConfig.trim();
                txtFile.print(newConfig);
                txtFile.close();
                Serial.println("Конфигурация сохранена");
            }
            LittleFS.end();
        }
    } 
    serializeJson(configDevice, Serial);
    const char* oId = configDevice["tenantId"];
    projectId = String(oId);
    const char* pId = configDevice["projectId"];
    projectId = String(pId);
    JsonObject next = configDevice["attachments"];
    sensorCount = 0; relayCount = 0;
    JsonArray relays  = relaysDJD.createNestedArray("relays");
    JsonArray sensors = sensorsDJD.createNestedArray("sensors");
    for (JsonPair kv : next) {
        String nextKey = kv.key().c_str();
        int nextPin = nextKey.toInt();
        JsonObject nextAttach = next[nextKey];
        String nextType = nextAttach["type"];
        String nextId = nextAttach["id"];
        String nextModel = nextAttach["templateId"];
        if (nextType == "RELAY") {
            JsonObject nextRelay = relays.createNestedObject();
            nextRelay["pin"] = nextPin;
            nextRelay["model"] = nextAttach["templateId"];
            nextRelay["id"] = nextAttach["id"];            
//            status = GetStatusRelay(wClient, api_server, apiRelay + nextId + "/status"); Сделать получение статуса реле через mqtt
            pinMode(nextPin, INPUT_PULLUP);
            pinMode(Red[relayCount], OUTPUT);
            sw[relayCount] = status;
            relayCount ++;
            switch (relayCount){
                case 1 :
                    myButton1.begin();                    
                    break;
                case 2 :
                    myButton2.begin();                                      
                    break;
                case 3 :
                    myButton3.begin();                    
                    break;
                case 4 :
                    myButton4.begin();                    
                    break;                    
            } 
        }
        if (nextType == "SENSOR") {
            JsonObject nextSensor = sensors.createNestedObject();
            nextSensor["pin"] = nextPin;
            nextSensor["id"] = nextId;
            nextSensor["model"] = nextModel;
            nextSensor["valuesCount"] = nextAttach["valuesCount"];
            int velueCount = nextAttach["valuesCount"];
            JsonArray velues = nextSensor.createNestedArray("values");
            for (int i=0; i < velueCount; i++) {
                JsonObject nextVelue = velues.createNestedObject();                
                nextVelue["id"] = nextAttach["values"][i];
//                nextVelue["valueFormat"] = nextAttach["values"][i]["valueFormat"]; 
                nextVelue["valueFormat"] = "DECIMAL";
            }
            sensorCount ++;
        }
    }
    sortByTerminal(relaysDJD["relays"]);
    relaysDJD.garbageCollect();
    Serial.println();
    JsonArray temp = relaysDJD["relays"];
    serializeJson(temp, Serial);
    Serial.println();
    temp = sensorsDJD["sensors"];
    serializeJson(temp, Serial);
    Serial.println();
    mqttClient.disconnect();
    connectMQTT();
    subscribeAll();
}

void updateFirmware(){
    String fVerURL = SDLab_httpUpdate;
    fVerURL.concat("TH10.ver");
    HTTPClient httpClient;
    httpClient.begin(wClient,fVerURL);
    int httpCode = httpClient.GET();
    Serial.print(httpCode); Serial.print(" "); Serial.println(SDLab_httpUpdate);
    if ( httpCode == 200) {
        String newFWVersion = httpClient.getString();
        newFWVersion.trim();
        FW_VERSION.trim();
        String fImageURL = SDLab_httpUpdate;
        fImageURL.concat(newFWVersion);
        httpClient.begin(wClient,fVerURL);
        httpCode = httpClient.GET();
        if ( (newFWVersion != FW_VERSION) && (httpCode == 200)) {
            Serial.printf("Current firmware version: %s\n", FW_VERSION.c_str());
            Serial.printf("Available new firmware version: %s\n",newFWVersion.c_str());
            Serial.printf("Preparing to update.\n");     
            String topicDevice = "org/" + orgId + "/project/"+projectId+"/device/"+deviceId;
            String topicPayload = "{\"connectivityStatus\":3}";
            mqttClient.publish(topicDevice.c_str(), topicPayload.c_str());
            Serial.printf("Публикую %s {\"connectivityStatus\":3}\n", topicDevice.c_str());
            LittleFS.begin();
            File txtFile = LittleFS.open("/TH10.ver", "w+");
            if (txtFile) {                
                txtFile.print(newFWVersion);
                txtFile.close();
            }
            LittleFS.end();
//            ESPhttpUpdate.onStart(update_started); сделать отключение нагрузок при обновлении
            httpUpdate.update(wClient, fImageURL);
            ESP.restart();
            delay(1000);
        }
    }
    httpClient.end();


}