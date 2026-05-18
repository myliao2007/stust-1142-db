#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiClientSecure.h>

// --- 設定區 ---
const char* ssid = "aaron";	
const char* password = "876543210";
char apiUrl[512];
const char *API_KEY = "this-is-fake-key";

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
    printJsonAsCSV(); // 執行 JSON 轉 CSV 格式化輸出
  }
  
  // 每 30 秒更新一次
  delay(30000); 
}

// --- 核心解析輔助函式：從指定關鍵字後方切出引號內的字串內容 ---
String extractValue(const String& record, const String& key) {
  int keyIndex = record.indexOf("\"" + key + "\":");
  if (keyIndex == -1) return "";
  
  // 尋找數值的起始引號 (跳過冒號與可能的空格)
  int startQuote = record.indexOf("\"", keyIndex + key.length() + 3);
  if (startQuote == -1) return "";
  
  int endQuote = record.indexOf("\"", startQuote + 1);
  if (endQuote == -1) return "";
  
  return record.substring(startQuote + 1, endQuote);
}

// --- 核心功能：下載並即時解析區塊，格式化成逗點分隔 (CSV) 輸出 ---
void printJsonAsCSV() {
  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  Serial.println("\n[API] 正在下載並格式化為 CSV 格式 (逗點分隔)...");
  
  http.setTimeout(15000); 

  if (http.begin(client, apiUrl)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
      WiFiClient* stream = http.getStreamPtr();
      
      Serial.println("\n--- 📊 CSV 格式化輸出開始 (更新時間,縣市,站點,PM2.5) ---");
      
      String chunkBuffer = "";
      uint8_t readBuf[128];
      
      while (http.connected() && (stream->available() > 0 || stream->connected())) {
        int availableBytes = stream->available();
        if (availableBytes > 0) {
          int c = stream->read(readBuf, min(availableBytes, (int)(sizeof(readBuf) - 1)));
          if (c > 0) {
            readBuf[c] = '\0';
            chunkBuffer += (char*)readBuf; // 將收到的字元追加到快取字串中
            
            // 尋找完整的大括號區塊 { ... }
            int startPos = chunkBuffer.indexOf('{');
            int endPos = chunkBuffer.indexOf('}');
            
            // 當在快取中找到完整的一個物件物件區塊時
            while (startPos != -1 && endPos != -1 && endPos > startPos) {
              // 擷取單一測站的 JSON 區塊文字
              String record = chunkBuffer.substring(startPos, endPos + 1);
              
              // 自行手動解析欄位，不依靠外部函式庫
              String site  = extractValue(record, "site");
              String county = extractValue(record, "county");
              String pm25   = extractValue(record, "pm25");
              String time   = extractValue(record, "datacreationdate");
              
              // 欄位防空處理 (若維護中無資料，填入 N/A)
              if (pm25.length() == 0) pm25 = "N/A";
              
              // 格式化為逗點分隔 (CSV) 印出
              if (site.length() > 0) {
                Serial.printf("%s,%s,%s,%s\n", time.c_str(), county.c_str(), site.c_str(), pm25.c_str());
              }
              
              // 將已處理完的區塊從快取中移除
              chunkBuffer = chunkBuffer.substring(endPos + 1);
              
              // 繼續檢查快取中是否還有下一個完整區塊
              startPos = chunkBuffer.indexOf('{');
              endPos = chunkBuffer.indexOf('}');
            }
          }
        }
        delay(1);
      }
      Serial.println("--- 🛑 CSV 格式化輸出結束 🛑 ---\n");
      
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
