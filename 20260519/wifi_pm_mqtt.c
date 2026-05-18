#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ================= 1. 設定區 =================
const char* ssid = "aaron";  // 改成你的熱點名稱
const char* password = "876543210"; // 改成你的熱點密碼

// 環境部 API
char apiUrl[512] ;
const char *API_KEY = "------------------------"; // 改成你的金鑰

// MQTT 設定 (根據 image_9b7e61.png)
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_topic  = "stust-myliao/lab4/pm25"; // 把 myliao 改成你的學號
const char* myClientID  = "mqttx_myliao"; // 把 myliao 改成你的學號

#define PIN_WS2812  48 
#define NUM_PIXELS  1
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ================= 2. 主程式 =================

void setup() {
  snprintf(apiUrl, sizeof(apiUrl), "%s%s", "https://data.moenv.gov.tw/api/v2/aqx_p_02?api_key=", API_KEY);
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(50);
  pixels.show();

  connectToWiFi();
  
  // 設定 EMQX 伺服器
  mqttClient.setServer(mqtt_server, 1883);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // 每 60 秒抓取並發布一次
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 10000) {
    getAQIDataAndPublish();
    lastTime = millis();
  }
}

// ================= 3. 功能函式 =================

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] 嘗試連線至 EMQX: %s\n", mqtt_server);
    
    // 使用指定的 ClientID 連線
    if (mqttClient.connect(myClientID)) {
      Serial.println(">> MQTT 已連線");
      setLedColor(pixels.Color(0, 255, 0)); // 連上亮綠燈
    } else {
      Serial.print("連線失敗, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" 5秒後重試");
      setLedColor(pixels.Color(255, 0, 0)); // 失敗亮紅燈
      delay(5000);
    }
  }
}

void getAQIDataAndPublish() {
  WiFiClientSecure client;
  client.setInsecure(); 
  HTTPClient http;

  if (http.begin(client, apiUrl)) {
    int httpResponseCode = http.GET();
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      // 搜尋林森站點
      int targetIndex = payload.indexOf("\"site\": \"林森\"");
      if (targetIndex != -1) {
        int startIndex = payload.lastIndexOf('{', targetIndex);
        int endIndex = payload.indexOf('}', targetIndex);

        if (startIndex != -1 && endIndex != -1) {
          String linsenJson = payload.substring(startIndex, endIndex + 1);
          
          JsonDocument doc; // V7
          deserializeJson(doc, linsenJson);

          const char* time = doc["datacreationdate"];
          const char* pm25Str = doc["pm25"];
          int pmValue = (pm25Str != NULL && strlen(pm25Str) > 0) ? atoi(pm25Str) : -1;

          // 硬體反饋
          if (pmValue != -1 && pmValue <= 15) {
            setLedColor(pixels.Color(0, 255, 0)); // 良好
          } else {
            setLedColor(pixels.Color(255, 255, 0)); // 警戒
          }

          // 發布至 EMQX
          String output;
          JsonDocument mqttDoc;
          mqttDoc["id"] = myClientID;
          mqttDoc["site"] = "林森";
          mqttDoc["pm25"] = pmValue;
          mqttDoc["time"] = time;
          serializeJson(mqttDoc, output);

          if (mqttClient.publish(mqtt_topic, output.c_str())) {
            Serial.println(">> 已發送至 EMQX: " + output);
          }
        }
      }
    }
    http.end();
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    setLedColor(pixels.Color(0, 0, 255)); delay(250);
    setLedColor(pixels.Color(0, 0, 0));   delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void setLedColor(uint32_t color) {
  pixels.setPixelColor(0, color);
  pixels.show();
}
