#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiClientSecure.h>

// --- 設定區 ---
const char* ssid = "aaron"; 		// modify the ssid 
const char* password = "876543210";	// modify the password
char apiUrl[512];
const char *API_KEY = "6008063a-4108-44c4-9838-c017a4d7c55b";	// modify the API_KEY

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
    printRawJson(); // 執行直接輸出原始 JSON 的函式
  }
  
  // 每 30 秒更新一次
  delay(30000); 
}

// --- 核心功能：不進行解析，直接將收到的原始 JSON 串流輸出 ---
void printRawJson() {
  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  Serial.println("\n[API] 正在連線並準備輸出原始 JSON 資料...");
  
  http.setTimeout(15000); 

  if (http.begin(client, apiUrl)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
      // 獲取網路串流指標
      WiFiClient* stream = http.getStreamPtr();
      
      Serial.println("--- 🟢 原始 JSON 資料開始 🟢 ---");
      
      // 建立一個 128 位元組的小緩衝區，邊讀邊印
      uint8_t buffer[128];
      
      while (http.connected() && (stream->available() > 0 || stream->connected())) {
        int availableBytes = stream->available();
        if (availableBytes > 0) {
          // 讀取資料到緩衝區（最多讀取緩衝區大小減 1，留空間給字串結束字元）
          int c = stream->read(buffer, min(availableBytes, (int)(sizeof(buffer) - 1)));
          if (c > 0) {
            buffer[c] = '\0'; // 加上字串結束符號
            Serial.print((char*)buffer); // 直接印出文字
          }
        }
        delay(1); // 微小延遲，給予網路硬體緩衝時間
      }
      
      Serial.println("\n--- 🛑 原始 JSON 資料結束 🛑 ---");
      
    } else {
      Serial.printf("HTTP 連線失敗，回應代碼: %d\n", httpResponseCode);
    }
    http.end();
  }
}

// --- 印出 WiFi MAC 位址 ---
void printMacAddress() {
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
