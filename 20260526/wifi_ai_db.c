#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>       
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>      

// ==========================================
// 1. WiFi 與硬體（WS2812 RGB LED）設定區
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
const char* gemini_key  = "改成你的 Google Gemini API key"; 
const char* gemini_host = "generativelanguage.googleapis.com";
const char* gemini_path = "/v1beta/models/gemini-flash-latest:generateContent";

// ==========================================
// 3. 網路版 SQLite (Turso Cloud) 設定區
// ==========================================
// 💡 ⭐ 重要：請務必確認結尾改為 /v2/pipeline 
const String turso_url   = "https://wifilogs-myliao2007.aws-ap-northeast-1.turso.io/v2/pipeline"; // 改成你的網址
const String turso_token = "改成你的 Token";

// 宣告全域安全連線通道
WiFiClientSecure global_secure_client;

void setLedColor(uint32_t color);
void connectToWiFi();
void scanAndProcessWiFi(); 
void sendPromptToGemini(String userPrompt);
void uploadBatchToTurso(String jsonBody);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1000); 
  
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show(); 

  global_secure_client.setInsecure(); // 跳過憑證驗證

  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    scanAndProcessWiFi();
  }

  Serial.println("\n==================================================");
  Serial.println("【核心系統】初始化完畢！終端機監聽就緒。");
  Serial.println("==================================================");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setLedColor(RED); 
    connectToWiFi();
    return; 
  } 
  setLedColor(GREEN); 
  
  if (Serial.available() > 0) {
    String inputPrompt = Serial.readStringUntil('\n');
    inputPrompt.trim(); 
    if (inputPrompt.length() > 0) {
      Serial.print("\n[系統捕捉提問] -> "); Serial.println(inputPrompt);
      setLedColor(YELLOW); delay(500); setLedColor(OFF); delay(500);
      setLedColor(BLUE); 
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
    setLedColor(BLUE); delay(250); setLedColor(OFF); delay(250);
    Serial.print("."); attempts++;
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
// 核心處理模組：利用 Heap 動態指標建構安全 JSON
// ==========================================
void scanAndProcessWiFi() {
  Serial.println("\n🔍 正在掃描周遭的 WiFi 網路...");
  setLedColor(YELLOW); 

  int n = WiFi.scanNetworks();
  Serial.println("掃描結束。");
  
  if (n == 0) {
    Serial.println("❌ 未找到任何 WiFi 網路。");
    return;
  }

  Serial.printf("共找到 %d 個網路，準備進行資料庫同步與 AI 分析...\n", n);
  
  String aiPrompt = "以下是我用 ESP32 微控制器在周遭掃描到的 WiFi 資訊列表（包含 SSID、MAC 位址與訊號強度）。\n"
                    "請扮演網路專家，特別依據 MAC 位址的前三碼（OUI 組織唯一識別碼），查詢並指出這些網路設備是由哪家硬體製造廠商所生產的，並說明該網路的潛在用途。\n"
                    "請用台灣正體中文整理成一個乾淨的 Markdown 表格回覆（包含：編號、SSID、MAC位址、訊號強度、設備製造商、分析備註）：\n\n";

  // ⭐ 核心修正：利用 new 在 Heap（堆）中動態分配大型 JSON 記憶體，完全不佔用微弱的 Stack 空間！
  DynamicJsonDocument* tursoDoc = new DynamicJsonDocument(16384); // 給予極寬裕的 16KB 空間處理大批量跳脫
  JsonArray requests = tursoDoc->createNestedArray("requests");

  for (int i = 0; i < n; ++i) {
    String currentSSID = WiFi.SSID(i);
    String bssidStr = WiFi.BSSIDstr(i); 
    int32_t rssi = WiFi.RSSI(i);        
    
    if (currentSSID.length() == 0) {
      currentSSID = "[隱藏網路]";
    }

    // 在本地序列埠印出
    Serial.printf("   [佇列加入] -> 序號 %d: SSID: %s | MAC: %s | %d dBm\n", i + 1, currentSSID.c_str(), bssidStr.c_str(), rssi);
    
    // 串接至 Gemini Prompt (保留最原始命名方便 AI 分析)
    aiPrompt += String(i + 1) + ". SSID: \"" + currentSSID + "\" | MAC: " + bssidStr + " | RSSI: " + String(rssi) + " dBm\n";
    
    // 處理 SQL 內部的單引號
    currentSSID.replace("'", "''"); 
    String sqlCommand = "INSERT INTO wifi_logs (ssid, mac, rssi) VALUES ('" + currentSSID + "', '" + bssidStr + "', " + String(rssi) + ");";

    // 建立 v2 Pipeline 節點：自動化跳脫 & 符號、雙引號、反斜線
    JsonObject reqObj = requests.createNestedObject();
    reqObj["type"] = "execute";
    JsonObject stmtObj = reqObj.createNestedObject("stmt");
    stmtObj["sql"] = sqlCommand;
  }

  // 補上管道收尾指令
  JsonObject closeObj = requests.createNestedObject();
  closeObj["type"] = "close";

  // 序列化為安全 JSON
  String tursoPayload;
  serializeJson(*tursoDoc, tursoPayload);

  // ⭐ 核心修正：序列化一結束，立刻回收 Heap 記憶體，把 16KB 的記憶體完整還給晶片！
  delete tursoDoc; 
  WiFi.scanDelete();

  // 1. 同步上傳到 Turso 雲端資料庫
  uploadBatchToTurso(tursoPayload);
  
  delay(1000); 

  // 2. 送交 Gemini 大模型
  Serial.println("\n--------------------------------------------------");
  Serial.println("【雲端任務】正在將完整的 SSID+MAC 清單送交 Gemini AI 進行分析...");
  setLedColor(BLUE); 
  sendPromptToGemini(aiPrompt);
  setLedColor(GREEN); 
}

// ==========================================
// 穩定的單次批次上傳（對應 /v2/pipeline 格式）
// ==========================================
void uploadBatchToTurso(String jsonBody) {
  Serial.println("\n📊 [資料庫任務] 正在建立安全連線，準備將批次資料發送至 Turso 雲端 SQLite...");
  
  HTTPClient http;
  http.setTimeout(15000); 
  
  if (http.begin(global_secure_client, turso_url)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + turso_token);

    int httpCode = http.POST(jsonBody);
    
    Serial.println("📊 [資料庫任務] 雲端伺服器已回應。");
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("   ✅ 【資料庫同步成功】所有掃描紀錄已批次寫入雲端 SQLite！");
    } else {
      Serial.printf("   ❌ 【資料庫同步失敗】狀態碼: %d | 回傳內容: %s\n", httpCode, http.getString().c_str());
    }
    http.end();
  } else {
    Serial.println("   ❌ 無法與 Turso 資料庫建立安全連線通道。");
  }
}

// ==========================================
// 雲端 AI 運算模組 (Gemini API)
// ==========================================
void sendPromptToGemini(String userPrompt) {
  Serial.println("正在連線至 Gemini API 伺服器...");
  HTTPClient http;
  String url = "https://" + String(gemini_host) + String(gemini_path);
  
  if (http.begin(global_secure_client, url)) {
    http.setTimeout(30000); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-goog-api-key", gemini_key);

    DynamicJsonDocument doc(2048);
    JsonArray contents = doc.createNestedArray("contents");
    JsonObject contentObj = contents.createNestedObject();
    JsonArray parts = contentObj.createNestedArray("parts");
    JsonObject partObj = parts.createNestedObject();
    partObj["text"] = userPrompt;

    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpCode = http.POST(requestBody);
    if (httpCode == HTTP_CODE_OK) {
      String responseBody = http.getString();
      DynamicJsonDocument resDoc(12288); 
      DeserializationError error = deserializeJson(resDoc, responseBody);
      if (!error) {
        const char* llm_result = resDoc["candidates"][0]["content"]["parts"][0]["text"];
        if (llm_result) {
          Serial.println("\n====== [🤖 Gemini 回應答覆] ======");
          Serial.println(llm_result);
          Serial.println("==================================\n");
        }
      }
    }
    http.end();
  }
}

void setLedColor(uint32_t color) {
  for(int i = 0; i < NUM_PIXELS; i++) pixels.setPixelColor(i, color);
  pixels.show();
}
