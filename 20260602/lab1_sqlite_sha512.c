#include <Arduino.h>
#include "mbedtls/sha512.h"
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <sqlite3.h>

// =========================================================
// 實驗組別與硬體參數定義
// =========================================================
#define GROUP         "[Group0] " 
#define RGB_LED_PIN   48
#define NUM_PIXELS    1

Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// 狀態機定義
enum SystemState {
  CMD_WAIT,          
  REG_WAIT_USER,     
  REG_WAIT_PASS,     
  AUTH_WAIT_USER,    
  AUTH_WAIT_PASS,
  CHG_WAIT_PASS
};

SystemState currentState = CMD_WAIT;
String inputBuffer = "";
bool stringComplete = false;

String currentUsername = "";
String currentPassword = "";

// =========================================================
// 實體資料庫設定 (SQLite on LittleFS)
// =========================================================
sqlite3 *db;
const char* dbPath = "/littlefs/shadow.db"; 

int executeSQL(const char* sql) {
  // ✨ 新增：印出直接執行的 SQL 語法
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);
  
  char *zErrMsg = 0;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf(GROUP "SQL 執行錯誤: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  return rc;
}

void initDatabase() {
  if (!LittleFS.begin(true)) {
    Serial.println(GROUP "❌ LittleFS 掛載失敗！");
    return;
  }
  
  sqlite3_initialize();
  if (sqlite3_open(dbPath, &db)) {
    Serial.printf(GROUP "❌ 無法開啟資料庫: %s\n", sqlite3_errmsg(db));
    return;
  }
  
  Serial.println(GROUP "✅ SQLite 資料庫開啟成功！");

  const char *createTableSQL = 
    "CREATE TABLE IF NOT EXISTS Users ("
    "username TEXT PRIMARY KEY, "
    "salt TEXT NOT NULL, "
    "hashHex TEXT NOT NULL"
    ");";
    
  executeSQL(createTableSQL);
}

// =========================================================
// 密碼學核心：mbedTLS SHA-512 運算
// =========================================================
String computeSHA512(String data) {
  for(int i=0; i<3; i++) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 80)); pixels.show(); delay(40);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));  pixels.show(); delay(40);
  }

  mbedtls_sha512_context ctx;
  uint8_t hash[64]; 
  
  mbedtls_sha512_init(&ctx);
  mbedtls_sha512_starts(&ctx, 0); 
  mbedtls_sha512_update(&ctx, (const unsigned char*)data.c_str(), data.length());
  mbedtls_sha512_finish(&ctx, hash);
  mbedtls_sha512_free(&ctx);

  String hexStr = "";
  for (int i = 0; i < 64; i++) {
    char buf[3];
    sprintf(buf, "%02X", hash[i]);
    hexStr += buf;
  }
  
  pixels.setPixelColor(0, pixels.Color(0, 40, 0)); pixels.show();
  return hexStr;
}

String generateRandomSalt() {
  String chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String salt = "";
  for (int i = 0; i < 8; i++) {
    salt += chars[esp_random() % chars.length()];
  }
  return salt;
}

// =========================================================
// 實體資料庫操作 (CRUD)
// =========================================================

bool checkUserExists(String user) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT 1 FROM Users WHERE username = ?;";
  
  // ✨ 新增：印出查詢語法
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);

  bool exists = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      exists = true;
    }
  }
  sqlite3_finalize(stmt);
  return exists;
}

bool addUser(String user, String pass) {
  String salt = generateRandomSalt();
  String computedHash = computeSHA512(salt + pass);

  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO Users (username, salt, hashHex) VALUES (?, ?, ?);";
  
  // ✨ 新增：印出寫入語法
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.println(GROUP "❌ SQL 準備失敗！");
    return false;
  }

  sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, salt.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, computedHash.c_str(), -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc == SQLITE_CONSTRAINT) {
    Serial.print(GROUP); Serial.println("❌ [錯誤] 該帳號已存在於系統中！");
    return false;
  } else if (rc != SQLITE_DONE) {
    Serial.print(GROUP); Serial.println("❌ [錯誤] 資料庫寫入失敗！");
    return false;
  }

  Serial.println();
  Serial.print(GROUP); Serial.println("----------------- [ 新增帳號成功 ] -----------------");
  Serial.print(GROUP); Serial.printf("輸入帳號 : %s\n", user.c_str());
  Serial.print(GROUP); Serial.printf("隨機鹽值 (Salt): %s\n", salt.c_str());
  Serial.print(GROUP); Serial.printf("Linux 格式 shadow 紀錄:\n" GROUP "👉 $6$%s$%s\n", salt.c_str(), computedHash.c_str()); 
  Serial.print(GROUP); Serial.println("---------------------------------------------------\n");
  return true;
}

bool updatePassword(String user, String newPass) {
  String newSalt = generateRandomSalt();
  String computedHash = computeSHA512(newSalt + newPass);

  sqlite3_stmt *stmt;
  const char *sql = "UPDATE Users SET salt = ?, hashHex = ? WHERE username = ?;";
  
  // ✨ 新增：印出更新語法
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.println(GROUP "❌ SQL 更新準備失敗！");
    return false;
  }

  sqlite3_bind_text(stmt, 1, newSalt.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, computedHash.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    Serial.print(GROUP); Serial.println("❌ [錯誤] 資料庫密碼更新失敗！");
    return false;
  }

  Serial.println();
  Serial.print(GROUP); Serial.println("----------------- [ 密碼修改成功 ] -----------------");
  Serial.print(GROUP); Serial.printf("帳號名稱 : %s\n", user.c_str());
  Serial.print(GROUP); Serial.printf("新隨機鹽值 (New Salt): %s\n", newSalt.c_str());
  Serial.print(GROUP); Serial.printf("更新後 Linux shadow 紀錄:\n" GROUP "👉 $6$%s$%s\n", newSalt.c_str(), computedHash.c_str()); 
  Serial.print(GROUP); Serial.println("---------------------------------------------------\n");
  return true;
}

void verifyLogin(String user, String pass) {
  Serial.println();
  Serial.print(GROUP); Serial.println("----------------- [ 執行登入驗證 ] -----------------");
  
  // ✨ 把這兩行加回來：印出準備驗證的帳號與密碼
  Serial.print(GROUP); Serial.printf("學生輸入帳號: %s\n", user.c_str());
  Serial.print(GROUP); Serial.printf("學生輸入密碼: %s\n", pass.c_str());
  
  sqlite3_stmt *stmt;
  const char *sql = "SELECT salt, hashHex FROM Users WHERE username = ?;";
  
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.println(GROUP "❌ 讀取資料庫失敗！");
    return;
  }

  sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    String systemSalt = (const char*)sqlite3_column_text(stmt, 0);
    String systemKnownHash = (const char*)sqlite3_column_text(stmt, 1);
    sqlite3_finalize(stmt); 
    
    // ✨ 把這段加回來：印出資料庫抓出的 Salt 與計算過程
    Serial.print(GROUP); Serial.printf("系統已知該帳號的 Salt: %s\n", systemSalt.c_str());
    Serial.print(GROUP); Serial.printf("系統紀錄的已知雜湊 (Known Hash):\n" GROUP "👉 %s\n", systemKnownHash.c_str());

    String inputComputedHash = computeSHA512(systemSalt + pass);
    Serial.print(GROUP); Serial.printf("本次輸入計算之雜湊 (Input Hash):\n" GROUP "👉 %s\n", inputComputedHash.c_str());

    if (inputComputedHash == systemKnownHash) {
      Serial.print(GROUP); Serial.println("✨ [比對結果] Hash 完全吻合！");
      Serial.print(GROUP); Serial.println("🔓 [驗證結果] 🔴 登入成功 (Login Success)！");
    } else {
      Serial.print(GROUP); Serial.println("⚠️ [比對結果] Hash 不符！密碼錯誤！");
      Serial.print(GROUP); Serial.println("❌ [驗證結果] 登入失敗 (Invalid Password)");
    }
  } else {
    sqlite3_finalize(stmt);
    Serial.print(GROUP); Serial.println("🔍 [檢查結果] 帳號不存在！");
    Serial.print(GROUP); Serial.println("❌ [驗證結果] 登入失敗 (User Not Found)");
  }
  Serial.print(GROUP); Serial.println("---------------------------------------------------\n");
}

static int listUsersCallback(void *data, int argc, char **argv, char **azColName) {
  int *printedCount = (int*)data;
  (*printedCount)++;
  Serial.print(GROUP); 
  Serial.printf(" [%02d] | %-12s | %-10s | %s\n", 
                *printedCount, argv[0] ? argv[0] : "NULL", argv[1] ? argv[1] : "NULL", argv[2] ? argv[2] : "NULL");
  return 0;
}

void listUsers() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 80)); pixels.show(); delay(100);
  pixels.setPixelColor(0, pixels.Color(0, 40, 0)); pixels.show();

  Serial.println();
  Serial.print(GROUP); Serial.println("================================== [ SQLite 實體資料庫傾印 ] ==================================");
  Serial.print(GROUP);  Serial.printf(" %-4s | %-12s | %-10s | %s\n", "編號", "使用者帳號", "鹽巴 (Salt)", "安全雜湊值 (SHA-512 Hash)");
  Serial.print(GROUP); Serial.println("-----------------------------------------------------------------------------------------------------");
  
  int printedCount = 0;
  const char* sql = "SELECT username, salt, hashHex FROM Users;";
  
  // ✨ 新增：印出傾印語法
  Serial.print(GROUP); Serial.print("SQL> "); Serial.println(sql);

  sqlite3_exec(db, sql, listUsersCallback, (void*)&printedCount, nullptr);
  
  if (printedCount == 0) {
    Serial.print(GROUP); Serial.println(" (系統資料庫目前空無一人) ");
  }
  Serial.print(GROUP); Serial.println("=====================================================================================================\n");
}

// =========================================================
// Setup & Loop 主程式
// =========================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);

  pixels.begin();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(0, 40, 0)); 
  pixels.show();

  initDatabase();

  // 預載帳號
  addUser("admin", "nust1234"); 
  addUser("student", "nust1234"); 

  Serial.println();
  Serial.print(GROUP); Serial.println("=================================================");
  Serial.print(GROUP); Serial.println("    🐧 ESP32-S3 SQLite 密碼安全防護實驗室 v3 🐧");
  Serial.print(GROUP); Serial.println("=================================================");
  Serial.print(GROUP); Serial.println("👉 請輸入 [login]      開始登入模擬");
  Serial.print(GROUP); Serial.println("👉 請輸入 [add]        開始建立新帳號");
  Serial.print(GROUP); Serial.println("👉 請輸入 [passwd <name>] 修改指定帳號密碼"); 
  Serial.print(GROUP); Serial.println("👉 請輸入 [list users] 顯示資料庫帳號清單");
  Serial.print(GROUP); Serial.println("-------------------------------------------------");
}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
    } else if (inChar != '\r') {
      inputBuffer += inChar;
    }
  }

  if (stringComplete) {
    inputBuffer.trim();
    
    if (inputBuffer.length() > 0) {
      switch (currentState) {
        case CMD_WAIT:
          if (inputBuffer.startsWith("passwd ") || inputBuffer.equalsIgnoreCase("passwd")) {
            String targetUser = "";
            if (inputBuffer.startsWith("passwd ")) {
              targetUser = inputBuffer.substring(7);
              targetUser.trim();
            }
            
            if (targetUser == "") {
              Serial.print(GROUP); Serial.println("❌ 用法錯誤！請使用：passwd <使用者名稱>");
            } else if (checkUserExists(targetUser)) {
              currentUsername = targetUser;
              Serial.println();
              Serial.print(GROUP); Serial.printf("[進入修改密碼程序] 目標帳號: %s\n", currentUsername.c_str());
              Serial.print(GROUP); Serial.println("請輸入該帳號的「新密碼 (New Password)」:");
              currentState = CHG_WAIT_PASS;
            } else {
              Serial.print(GROUP); Serial.printf("❌ [錯誤] 找不到使用者 '%s'，無法修改密碼！\n", targetUser.c_str());
            }
          }
          else if (inputBuffer.equalsIgnoreCase("login")) {
            Serial.println();
            Serial.print(GROUP); Serial.println("[進入登入程序] 請輸入「帳號 (Username)」:");
            currentState = AUTH_WAIT_USER;
          } else if (inputBuffer.equalsIgnoreCase("add")) {
            Serial.println();
            Serial.print(GROUP); Serial.println("[進入註冊程序] 請輸入欲建立的「新帳號 (New Username)」:");
            currentState = REG_WAIT_USER;
          } else if (inputBuffer.equalsIgnoreCase("list users")) {
            listUsers();
            Serial.print(GROUP); Serial.println(">>> 系統回到待命狀態。");
          } else {
            Serial.print(GROUP); Serial.println("❌ 未知指令。請輸入 [login], [add], [passwd <name>] 或 [list users]");
          }
          break;

        case REG_WAIT_USER:
          currentUsername = inputBuffer;
          Serial.print(GROUP); Serial.printf("帳號設定為: %s\n", currentUsername.c_str());
          Serial.print(GROUP); Serial.println("請輸入該帳號的「密碼 (Password)」:");
          currentState = REG_WAIT_PASS;
          break;

        case REG_WAIT_PASS:
          currentPassword = inputBuffer;
          addUser(currentUsername, currentPassword);
          Serial.print(GROUP); Serial.println(">>> 系統回到待命狀態。");
          currentState = CMD_WAIT;
          break;

        case AUTH_WAIT_USER:
          currentUsername = inputBuffer;
          // ✨ 把這行加回來：印出剛才輸入的帳號
          Serial.print(GROUP); Serial.printf("輸入帳號: %s\n", currentUsername.c_str());
          Serial.print(GROUP); Serial.println("請輸入「密碼 (Password)」:");
          currentState = AUTH_WAIT_PASS;
          break;

        case AUTH_WAIT_PASS:
          currentPassword = inputBuffer;
          verifyLogin(currentUsername, currentPassword);
          Serial.print(GROUP); Serial.println(">>> 系統回到待命狀態。");
          currentState = CMD_WAIT;
          break;

        case CHG_WAIT_PASS:
          currentPassword = inputBuffer;
          updatePassword(currentUsername, currentPassword);
          Serial.print(GROUP); Serial.println(">>> 系統回到待命狀態。");
          currentState = CMD_WAIT;
          break;
      }
    }
    inputBuffer = "";
    stringComplete = false;
  }
}
