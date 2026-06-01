#include <Arduino.h>
#include "esp_system.h"
#include <Adafruit_NeoPixel.h>

#define RGB_LED_PIN   48
#define NUM_PIXELS    1
#define GROUP "group0"

Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =========================================================
// 應用 1：賽博朋克大樂透開獎 (自動生成不重複的 6 個幸運號碼)
// =========================================================
void generateLotteryNumbers() {
  int lottery[6];
  int count = 0;
  
  Serial.println("\n🔮 [命運之輪] 正在利用晶片熱雜訊為您抽取 6 個『不重複』的大樂透號碼...");

  while(count < 6) {
    // 透過 TRNG 取得 1 ~ 49 的隨機數
    int ball = (esp_random() % 49) + 1;
    
    // 檢查是否有重複號碼
    bool duplicate = false;
    for(int i = 0; i < count; i++) {
      if(lottery[i] == ball) {
        duplicate = true;
        break;
      }
    }
    
    // 若不重複，則加入開獎名單
    if(!duplicate) {
      lottery[count] = ball;
      count++;
    }
  }

  // 簡單做個排序，讓輸出更美觀
  for(int i = 0; i < 5; i++) {
    for(int j = i + 1; j < 6; j++) {
      if(lottery[i] > lottery[j]) {
        int temp = lottery[i];
        lottery[i] = lottery[j];
        lottery[j] = temp;
      }
    }
  }

  // 印出本期中獎號碼
  Serial.print("👉 本期幸運開獎結果為：[ ");
  for(int i = 0; i < 6; i++) {
    Serial.printf("%02d ", lottery[i]);
  }
  Serial.println("] 祝您中大獎！");
}

// =========================================================
// 應用 2：軍規級一次性動態密碼密鑰 (OTP Generator)
// =========================================================
void generateSecureToken() {
  // 模擬銀行交易或虛擬貨幣錢包的 6 位數高強度交易 PIN 碼
  uint32_t token = esp_random() % 1000000; 
  
  // 模擬區塊鏈等級的 16 進位鹽值 (Salt Key)
  uint8_t salt[8];
  esp_fill_random(salt, 8);

  Serial.println("\n🛡️  [國安級安全防護] 偵測到一筆模擬交易，正在生成一次性動態密鑰...");
  Serial.printf("👉 交易認證碼 (6-Digit Token): %06u\n", token);
  Serial.print("👉 硬體動態鹽值 (Hex Salt)   : ");
  for (int i = 0; i < 8; i++) Serial.printf("%02X", salt[i]);
  Serial.println("\n⚠️  [安全警告] 該密鑰由大氣熱雜訊生成，數學上絕對無法被預測與逆向逆推！");
}

// =========================================================
// Setup
// =========================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);

  pixels.begin();
  pixels.clear();
  pixels.show();

  Serial.println("\n\n=================================================");
  Serial.println(GROUP);
  Serial.println("   ⚡ ESP32-S3 賽博朋克：命運與安全亂數主機 ⚡");
  Serial.println("=================================================");
  Serial.println("系統開機成功。本機已成功連線量子級射頻熱雜訊通道。");
}

// =========================================================
// Loop
// =========================================================
void loop() {
  Serial.println("\n=================================================");
  Serial.println(GROUP);
  Serial.println(">>> 🎲 倒數計時！開始執行新一輪命運採樣... 🎲");
  
  // ✨ 1. 執行中：顯示藍閃爍效果
  // 這次故意做出快慢交替的閃爍感，模擬太空船主電腦在「量子運算」的科技感
  for(int i = 0; i < 6; i++) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 80)); // 強藍光
    pixels.show();
    delay(40 + (i * 20)); // 閃爍間隔越來越長
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(40);
  }

  // 2. 執行核心應用運算
  generateLotteryNumbers();
  generateSecureToken();
  
  Serial.println("=================================================");

  // ✨ 3. 執行成功：切換為定色【綠燈】代表結果出爐，安全落庫
  pixels.setPixelColor(0, pixels.Color(0, 80, 0)); 
  pixels.show();

  // 每 30 秒大開獎一次
  delay(30000); 
}
