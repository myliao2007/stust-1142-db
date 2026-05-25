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
const char* gemini_key  = "改成你的金鑰啦!"; 
const char* gemini_host = "generativelanguage.googleapis.com";
const char* gemini_path = "/v1beta/models/gemini-flash-latest:generateContent";

void setLedColor(uint32_t color);
void connectToWiFi();
void scanAndAnalyzeWiFi(); // ⭐ 新增：掃描並讓 AI 分析的函式
void sendPromptToGemini(String userPrompt);

void setup() {
  Serial.begin(115200);
  
  // 💡 配合換行讀取，將超時時間設為 1000 毫秒，維持系統穩定
  Serial.setTimeout(1000); 
  
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show(); 

  // 1. 先行連線 WiFi
  connectToWiFi();

  // 2. ⭐ 連線成功後，執行 WiFi 掃描與 AI 辨識公司名稱
  if (WiFi.status() == WL_CONNECTED) {
    scanAndAnalyzeWiFi();
  }

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
  
  // 💡 設定為 STA 模式（同時支援掃描與連線）
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

// ==========================================
// ⭐ 升級功能：掃描 SSID 與 MAC，並讓 AI 根據 MAC (OUI) 判斷設備製造商
// ==========================================
void scanAndAnalyzeWiFi() {
  Serial.println("\n🔍 正在掃描周遭的 WiFi 網路（同時擷取 MAC 位址）...");
  setLedColor(YELLOW); // 掃描時亮黃燈提示

  // WiFi.scanNetworks() 會回傳找到的網路數量
  int n = WiFi.scanNetworks();
  Serial.println("掃描結束。");
  
  if (n == 0) {
    Serial.println("❌ 未找到任何 WiFi 網路。");
    return;
  }

  Serial.printf("共找到 %d 個網路：\n", n);
  
  // 建立一個結構化的 Prompt，引導 AI 著重利用 MAC 位址的前三碼 (OUI) 來識別製造商
  String aiPrompt = "以下是我用 ESP32 微控制器在周遭掃描到的 WiFi 資訊列表（包含 SSID、MAC 位址與訊號強度）。\n"
                    "請扮演網路專家，協助我分析這些資料。請特別依據 MAC 位址的前三碼（OUI 組織唯一識別碼），查詢並精準指出這些網路設備是由哪家硬體製造廠商（例如 Cisco, TP-Link, Arcadyan 智易科技, ASUSTek 華碩, Foxconn 等）所生產的，並說明該網路的潛在用途。\n"
                    "請用台灣正體中文整理成一個乾淨的 Markdown 表格回覆（包含：編號、SSID、MAC位址、訊號強度、設備製造商、分析備註）：\n\n";

  // 巡迴印出所有網路資訊，並將其串接至給 AI 的提問中
  for (int i = 0; i < n; ++i) {
    String currentSSID = WiFi.SSID(i);
    String bssidStr = WiFi.BSSIDstr(i); // ⭐ 關鍵：取得基地台的 MAC 位址字串
    int32_t rssi = WiFi.RSSI(i);        // 訊號強度
    
    // 如果 SSID 為空，代表是隱藏網路，我們可以把它標示出來
    if (currentSSID.length() == 0) {
      currentSSID = "[隱藏網路]";
    }

    // 輸出到序列埠監控窗，方便學生對照
    Serial.printf("%d: %s | MAC: %s | 訊號: %d dBm\n", i + 1, currentSSID.c_str(), bssidStr.c_str(), rssi);
    
    // 將詳細資訊串接到 Prompt 中
    aiPrompt += String(i + 1) + ". SSID: \"" + currentSSID + "\" | MAC: " + bssidStr + " | RSSI: " + String(rssi) + " dBm\n";
    
    delay(10);
  }

  // 刪除掃描結果以釋放記憶體空間
  WiFi.scanDelete();

  Serial.println("\n--------------------------------------------------");
  Serial.println("【自動任務】正在將 SSID + MAC 清單送交 Gemini AI 進行 OUI 廠商解碼...");
  setLedColor(BLUE); // 傳送中亮藍燈
  
  // 呼叫原本寫好的 Gemini 傳送函式
  sendPromptToGemini(aiPrompt);
  
  setLedColor(GREEN); // 完成後恢復綠燈
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

    // 💡 因為 SSID 列表加上 Prompt 字串比較長，將 JSON 空間放大至 2048
    DynamicJsonDocument doc(2048);
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
        
        // 💡 放大解析回應的記憶體空間至 12288，預防 AI 回傳的表格內容較多
        DynamicJsonDocument resDoc(12288);
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
