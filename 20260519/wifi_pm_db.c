#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiClientSecure.h>
#include <sqlite3.h>
#include "esp_heap_caps.h"

// --- 設定區 ---
const char* ssid = "aaron";
const char* password = "876543210";
char apiUrl[512];
const char *API_KEY = "Change_This_Key!";

#define PIN_WS2812  48 
#define NUM_PIXELS  1
#define BRIGHTNESS  50

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

uint32_t RED    = pixels.Color(255, 0, 0);
uint32_t GREEN  = pixels.Color(0, 255, 0);
uint32_t BLUE   = pixels.Color(0, 0, 255);
uint32_t OFF    = pixels.Color(0, 0, 0);

// --- SQLite 全域變數 ---
sqlite3 *db;
char *zErrMsg = 0;
int rc;

// 函數宣告
void parseCsvToSQLite();
void printAllDatabaseContent();
void printFirstFiveRecords();
void printMenu();

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show();

  // 1. 初始化 PSRAM（確認硬體 PSRAM 正常）
  if (!psramInit()) {
    Serial.println("PSRAM 初始化失敗！");
    return;
  }

  // 2. 初始化 SQLite 並建立 In-Memory 資料庫於 PSRAM
  sqlite3_initialize();
  rc = sqlite3_open(":memory:", &db);
  if (rc) {
    Serial.printf("無法打開記憶體資料庫: %s\n", sqlite3_errmsg(db));
    return;
  }
  Serial.println("成功在 PSRAM 中建立 SQLite 資料庫！");

  // 3. 建立資料表
  const char *sql_create = "CREATE TABLE IF NOT EXISTS aq_data ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "time TEXT,"
                           "county TEXT,"
                           "site TEXT,"
                           "pm25 INTEGER);";
  Serial.printf("\n[SQL Command] %s\n", sql_create);
  rc = sqlite3_exec(db, sql_create, NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL 建立資料表失敗: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }

  snprintf(apiUrl, sizeof(apiUrl), "%s%s", "https://data.moenv.gov.tw/api/v2/aqx_p_02?api_key=", API_KEY);
  
  connectToWiFi();

  // 開機自動抓取第一次資料，填滿資料庫
  if (WiFi.status() == WL_CONNECTED) {
    setLedColor(GREEN);
    parseCsvToSQLite();
    printMenu(); // 顯示功能選單
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setLedColor(RED);
    connectToWiFi();
    return;
  }

  // 監聽序列埠輸入
  if (Serial.available() > 0) {
    char inputChar = Serial.read();
    
    // 跳過換行字元 (\n 或 \r)
    if (inputChar == '\n' || inputChar == '\r') return;

    Serial.printf("\n收到指令: [%c]\n", inputChar);

    const char* sql_delete = "DELETE FROM aq_data;";
    switch (inputChar) {
      case '1':
        printAllDatabaseContent(); // 按 1: 顯示全部資料庫內容
        printMenu();
        break;
        
      case '2':
        printFirstFiveRecords();   // 按 2: 顯示資料庫前五筆資料
        printMenu();
        break;
        
      case '3':
        Serial.println("♻️ 正在清除舊數據並重新連線更新資料庫...");
        Serial.printf("[SQL Command] %s\n", sql_delete);
        sqlite3_exec(db, sql_delete, NULL, NULL, NULL);
        
        parseCsvToSQLite();        // 按 3: 更新資料庫
        printMenu();
        break;
        
      default:
        Serial.println("❌ 未知指令！請輸入 1, 2 或 3。");
        printMenu();
        break;
    }
  }
  delay(50);
}

// --- 提示選單印出 ---
void printMenu() {
  Serial.println("\n-------------------------------------------------");
  Serial.println("👉 請輸入指令進行控制：");
  Serial.println("   [1] 顯示「全部」資料庫內容");
  Serial.println("   [2] 顯示資料庫「前五筆」資料");
  Serial.println("   [3] 重新連線 API 更新資料庫");
  Serial.println("-------------------------------------------------");
}

// --- 按 1：顯示全部資料庫內容 ---
void printAllDatabaseContent() {
  const char* sql_query = "SELECT id, time, county, site, pm25 FROM aq_data ORDER BY id ASC;";
  Serial.printf("\n[SQL Command] %s\n", sql_query);
  Serial.println("===== 🌐 資料庫全部內容清單 =====");
  
  sqlite3_stmt *res;
  if (sqlite3_prepare_v2(db, sql_query, -1, &res, 0) == SQLITE_OK) {
    int count = 0;
    while (sqlite3_step(res) == SQLITE_ROW) {
      int id = sqlite3_column_int(res, 0);
      const unsigned char* time = sqlite3_column_text(res, 1);
      const unsigned char* county = sqlite3_column_text(res, 2);
      const unsigned char* site = sqlite3_column_text(res, 3);
      int pm = sqlite3_column_int(res, 4);
      
      Serial.printf("ID: %03d | 更新時間: %s | %s縣市 -> %s站 | PM2.5: %d μg/m3\n", id, time, county, site, pm);
      count++;
    }
    if (count == 0) Serial.println("資料庫目前為空，請先按 3 更新資料。");
  } else {
    Serial.println("SQL 查詢失敗。");
  }
  sqlite3_finalize(res);
  Serial.println("=========================================");
}

// --- 按 2：顯示資料庫前五筆資料 (利用 LIMIT 5) ---
void printFirstFiveRecords() {
  const char* sql_query = "SELECT id, time, county, site, pm25 FROM aq_data ORDER BY id ASC LIMIT 5;";
  Serial.printf("\n[SQL Command] %s\n", sql_query);
  Serial.println("===== 📊 資料庫前五筆資料 =====");
  
  sqlite3_stmt *res;
  if (sqlite3_prepare_v2(db, sql_query, -1, &res, 0) == SQLITE_OK) {
    int count = 0;
    while (sqlite3_step(res) == SQLITE_ROW) {
      int id = sqlite3_column_int(res, 0);
      const unsigned char* time = sqlite3_column_text(res, 1);
      const unsigned char* county = sqlite3_column_text(res, 2);
      const unsigned char* site = sqlite3_column_text(res, 3);
      int pm = sqlite3_column_int(res, 4);
      
      Serial.printf("ID: %03d | 更新時間: %s | %s縣市 -> %s站 | PM2.5: %d μg/m3\n", id, time, county, site, pm);
      count++;
    }
    if (count == 0) Serial.println("資料庫目前為空，請先按 3 更新資料。");
  } else {
    Serial.println("SQL 查詢失敗。");
  }
  sqlite3_finalize(res);
  Serial.println("=========================================");
}

// --- 字串切割輔助函式：切出欄位值 ---
String extractValue(const String& record, const String& key) {
  int keyIndex = record.indexOf("\"" + key + "\":");
  if (keyIndex == -1) return "";
  
  int startQuote = record.indexOf("\"", keyIndex + key.length() + 3);
  if (startQuote == -1) return "";
  
  int endQuote = record.indexOf("\"", startQuote + 1);
  if (endQuote == -1) return "";
  
  return record.substring(startQuote + 1, endQuote);
}

// --- 核心功能：下載 JSON、解析字串並完整印出每一筆 INSERT 寫入至 SQLite ---
void parseCsvToSQLite() {
  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  Serial.println("\n[API] 下載中並即時結構化至 PSRAM SQLite...");
  
  http.setTimeout(15000); 

  if (http.begin(client, apiUrl)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK) {
      WiFiClient* stream = http.getStreamPtr();
      
      Serial.println("[SQL] >>> 開始寫入資料庫... <<<");
      
      // 開啟 Transaction (交易模式)
      const char* sql_begin = "BEGIN TRANSACTION;";
      Serial.printf("[SQL Command] %s\n", sql_begin); 
      sqlite3_exec(db, sql_begin, NULL, NULL, NULL);

      String chunkBuffer = "";
      uint8_t readBuf[128];
      int count = 0;
      
      while (http.connected() && (stream->available() > 0 || stream->connected())) {
        int availableBytes = stream->available();
        if (availableBytes > 0) {
          int c = stream->read(readBuf, min(availableBytes, (int)(sizeof(readBuf) - 1)));
          if (c > 0) {
            readBuf[c] = '\0';
            chunkBuffer += (char*)readBuf;
            
            int startPos = chunkBuffer.indexOf('{');
            int endPos = chunkBuffer.indexOf('}');
            
            while (startPos != -1 && endPos != -1 && endPos > startPos) {
              String record = chunkBuffer.substring(startPos, endPos + 1);
              
              String site  = extractValue(record, "site");
              String county = extractValue(record, "county");
              String pm25Str = extractValue(record, "pm25");
              String time   = extractValue(record, "datacreationdate");
              
              if (site.length() > 0) {
                int pmValue = -1;
                if (pm25Str.length() > 0) {
                  pmValue = pm25Str.toInt();
                } else {
                  chunkBuffer = chunkBuffer.substring(endPos + 1);
                  startPos = chunkBuffer.indexOf('{');
                  endPos = chunkBuffer.indexOf('}');
                  continue;
                }

                // 組装 SQL 指令
                char sql_insert[256];
                snprintf(sql_insert, sizeof(sql_insert), 
                         "INSERT INTO aq_data (time, county, site, pm25) VALUES ('%s', '%s', '%s', %d);",
                         time.c_str(), county.c_str(), site.c_str(), pmValue);
                
                // 完整印出當前這一筆的 INSERT SQL Command
                Serial.printf("[SQL Command] %s\n", sql_insert);
                
                // 執行寫入到 PSRAM SQLite
                sqlite3_exec(db, sql_insert, NULL, NULL, NULL);
                count++;
              }
              
              chunkBuffer = chunkBuffer.substring(endPos + 1);
              startPos = chunkBuffer.indexOf('{');
              endPos = chunkBuffer.indexOf('}');
            }
          }
        }
        delay(1);
      }
      
      // 提交交易
      const char* sql_commit = "COMMIT;";
      Serial.printf("[SQL Command] %s\n", sql_commit); 
      sqlite3_exec(db, sql_commit, NULL, NULL, NULL);
      Serial.printf("[SQL] >>> 寫入完成！共計成功寫入 %d 筆數據 <<<\n", count);
      
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
