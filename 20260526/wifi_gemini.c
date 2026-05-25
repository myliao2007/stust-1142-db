#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>       // 處理 HTTP 分塊傳輸與連線
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>      

// ==========================================
// 1. WiFi 與硬體（WS2812）設定區
// ==========================================
const char* ssid = "aaron";
const char* password = "876543210";

#define PIN_WS2812  48    
#define NUM_PIXELS  1     
#define BRIGHTNESS  50    

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

uint32_t RED    = pixels.Color(255, 0, 0);
uint32_t GREEN  = pixels.Color(0, 255, 0);
uint32_t BLUE   = pixels.Color(0, 0, 255);
uint32_t YELLOW = pixels.Color(255, 150, 0); 
uint32_t OFF    = pixels.Color(0, 0, 0);

// ==========================================
// 2. Google Gemini API 設定區
// ==========================================
const char* gemini_key  = "改成你的 api key"; 
const char* gemini_host = "generativelanguage.googleapis.com";
const char* gemini_path = "/v1beta/models/gemini-flash-latest:generateContent";

void setLedColor(uint32_t color);
void connectToWiFi();
void sendPromptToGemini(String userPrompt);

void setup() {
  Serial.begin(115200);
  
  // 💡 配合換行讀取，將超時時間設為 1000 毫秒，維持系統穩定
  Serial.setTimeout(1000); 
  
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show(); 

  connectToWiFi();

  Serial.println("\n==================================================");
  Serial.println("【換行觸發版】終端機就緒！");
  Serial.println("請確認右下角已設定為「Newline」或「Both NL & CR」");
  Serial.println("==================================================");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setLedColor(RED); 
    Serial.println("[警告] WiFi 斷線，嘗試重新連線...");
    connectToWiFi();
    return; 
  } 
  
  setLedColor(GREEN); 
  
  // 💡 換行版本讀取機制
  if (Serial.available() > 0) {
    
    // 一直讀取直到遇到換行符號 ('\n')
    String inputPrompt = Serial.readStringUntil('\n');
    
    // 清除字串前後的空白與不可見字元 (例如 \r)
    inputPrompt.trim(); 

    // 確認清除空白後，字串確實有內容才觸發
    if (inputPrompt.length() > 0) {
      Serial.print("\n[系統捕捉提問] -> ");
      Serial.println(inputPrompt);
      
      // 收到有效文字，閃爍一秒黃燈
      setLedColor(YELLOW); 
      delay(500);
      setLedColor(OFF);
      delay(500);
      
      setLedColor(BLUE); // 傳輸中亮藍燈
      
      // 執行發送
      sendPromptToGemini(inputPrompt);
    }
    
    setLedColor(GREEN);
    Serial.println("\n--------------------------------------------------");
    Serial.println("請輸入下一個問題...");
  }
  delay(10); 
}

void connectToWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    setLedColor(BLUE);
    delay(250);
    setLedColor(OFF);
    delay(250);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    setLedColor(GREEN);
  } else {
    Serial.println("\nConnection Failed.");
    setLedColor(RED);
  }
}

void setLedColor(uint32_t color) {
  for(int i=0; i<NUM_PIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
}

// ==========================================
// 雲端 AI 傳送函式 (防超時與分塊處理版)
// ==========================================
void sendPromptToGemini(String userPrompt) {
  Serial.println("正在連線至 Gemini API 伺服器...");

  WiFiClientSecure client;
  client.setInsecure(); // 跳過憑證驗證

  HTTPClient http;
  String url = "https://" + String(gemini_host) + String(gemini_path);
  
  if (http.begin(client, url)) {
    
    // 設定 30 秒的超時，耐心等待大模型生成長文
    http.setTimeout(30000); 
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-goog-api-key", gemini_key);

    StaticJsonDocument<1024> doc;
    JsonArray contents = doc.createNestedArray("contents");
    JsonObject contentObj = contents.createNestedObject();
    JsonArray parts = contentObj.createNestedArray("parts");
    JsonObject partObj = parts.createNestedObject();
    partObj["text"] = userPrompt;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("資料已送出，等待雲端 AI 回應答覆 (LLM 思考中，請耐心等待數秒)...");
    
    int httpCode = http.POST(requestBody);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String responseBody = http.getString();
        
        DynamicJsonDocument resDoc(8192);
        DeserializationError error = deserializeJson(resDoc, responseBody);

        if (error) {
          Serial.print("JSON 解析錯誤: ");
          Serial.println(error.c_str());
          Serial.println(responseBody); 
        } else {
          const char* llm_result = resDoc["candidates"][0]["content"]["parts"][0]["text"];

          if (llm_result) {
            Serial.println("\n====== [🤖 Gemini 回應答覆] ======");
            Serial.println(llm_result);
            Serial.println("==================================\n");
          } else {
            Serial.println("無法找到 text 節點，請檢查 API 回傳格式！");
          }
        }
      } else {
        Serial.printf("[HTTP] POST 失敗，伺服器回傳狀態碼: %d\n", httpCode);
        Serial.println(http.getString()); 
      }
    } else {
      Serial.printf("[HTTP] POST 請求發生錯誤: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
  } else {
    Serial.println("[HTTP] 無法建立連線。");
  }
}
