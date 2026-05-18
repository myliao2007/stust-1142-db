#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiClientSecure.h>

// --- 設定區 ---
const char* ssid = "aaron";
const char* password = "876543210";
char apiUrl[512];
const char *API_KEY = "Modify_me_please XD";

#define PIN_WS2812  48 
#define NUM_PIXELS  1
#define BRIGHTNESS  50

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

uint32_t RED    = pixels.Color(255, 0, 0);
uint32_t GREEN  = pixels.Color(0, 255, 0);
uint32_t BLUE   = pixels.Color(0, 0, 255);
uint32_t OFF    = pixels.Color(0, 0, 0);

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show();

  snprintf(apiUrl, sizeof(apiUrl), "%s%s", "https://data.moenv.gov.tw/api/v2/aqx_p_02?api_key=", API_KEY);
  
  connectToWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setLedColor(RED);
    connectToWiFi();
  } else {
    setLedColor(GREEN);
    getAQIDataV7(); // 執行 V7 解析函式
  }
  
  // 每 30 秒更新一次 (演示建議不要太快，API 有流量限制)
  delay(30000); 
}

// --- 新增的功能：解讀 PM2.5 指標 ---
void printAQIStatus(int pm) {
  Serial.print("空氣品質等級: ");
  
  if (pm <= 15) {
    Serial.println("🟢 良好 (Good)");
    Serial.println("建議：非常適合戶外活動。");
  } 
  else if (pm <= 35) {
    Serial.println("🟡 普通 (Moderate)");
    Serial.println("建議：一般人群可正常活動，極敏感族群需注意。");
  } 
  else if (pm <= 54) {
    Serial.println("🟠 對敏感族群不健康 (Unhealthy for Sensitive Groups)");
    Serial.println("建議：敏感族群應減少體力消耗。");
  } 
  else if (pm <= 150) {
    Serial.println("🔴 對所有族群不健康 (Unhealthy)");
    Serial.println("建議：應減少戶外劇烈活動。");
  } 
  else {
    Serial.println("🟣 非常不健康 或 🟤 危害 (Hazardous)");
    Serial.println("建議：應留在室內並減少體力消耗。");
  }
}

// --- 修改後的解析函式 ---
void getAQIDataV7() {
  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  Serial.println("\n[API] 正在取得資料並進行區塊掃描...");
  
  http.setTimeout(15000); 

  if (http.begin(client, apiUrl)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      int targetIndex = payload.indexOf("\"site\": \"林森\"");
      
      if (targetIndex != -1) {
        int startIndex = payload.lastIndexOf('{', targetIndex);
        int endIndex = payload.indexOf('}', targetIndex);

        if (startIndex != -1 && endIndex != -1) {
          String linsenJson = payload.substring(startIndex, endIndex + 1);
          
          JsonDocument doc;
          deserializeJson(doc, linsenJson);

          const char* site = doc["site"];
          const char* pm25Str = doc["pm25"]; // 原始資料是字串
          const char* time = doc["datacreationdate"];
          
          Serial.println("================================");
          Serial.printf("站點: %s\n", site);
          
          if (pm25Str != NULL && strlen(pm25Str) > 0) {
            int pmValue = atoi(pm25Str); // 將字串轉為整數
            Serial.printf("PM2.5 濃度: %d μg/m3\n", pmValue);
            
            // 呼叫新功能：印出空氣品質與建議
            printAQIStatus(pmValue);
          } else {
            Serial.println("PM2.5 濃度: 目前無資料 (設備維護中)");
          }
          
          Serial.printf("更新時間: %s\n", time);
          Serial.println("================================");
        }
      } else {
        Serial.println("找不到 '林森' 站資料。");
      }
    }
    http.end();
  }
}

// --- 新增的功能：印出 WiFi MAC 位址 ---
void printMacAddress() {
  // WiFi.macAddress() 會回傳一個 String，例如 "AA:BB:CC:DD:EE:FF"
  String mac = WiFi.macAddress();
  
  Serial.println("================================");
  Serial.print("ESP32 網路實體位址 (MAC): ");
  Serial.println(mac);
  Serial.println("================================");
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    setLedColor(BLUE); delay(250);
    setLedColor(OFF); delay(250);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    printMacAddress();
    setLedColor(GREEN);
  } else {
    setLedColor(RED);
  }
}

void setLedColor(uint32_t color) {
  for(int i=0; i<NUM_PIXELS; i++) pixels.setPixelColor(i, color);
  pixels.show();
}
