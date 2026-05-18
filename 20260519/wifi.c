#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

// --- 設定區 ---
const char* ssid = "aaron";
const char* password = "876543210";

#define PIN_WS2812  48    // WS2812 資料引腳
#define NUM_PIXELS  1     // 燈珠數量
#define BRIGHTNESS  50    // 亮度 (0-255)

// 初始化 NeoPixel
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_WS2812, NEO_GRB + NEO_KHZ800);

// --- 顏色定義 ---
uint32_t RED   = pixels.Color(255, 0, 0);
uint32_t GREEN = pixels.Color(0, 255, 0);
uint32_t BLUE  = pixels.Color(0, 0, 255);
uint32_t OFF   = pixels.Color(0, 0, 0);

void setup() {
  Serial.begin(115200);
  
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.show(); // 初始熄滅

  connectToWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setLedColor(RED); // 斷線亮紅燈
    Serial.println("Reconnecting...");
    connectToWiFi();
  } else {
    setLedColor(GREEN); // 連線亮綠燈
  }
  
  delay(5000);
}

// --- 功能函式 ---

// 封裝 WiFi 連線邏輯
void connectToWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    // 連線中，讓燈閃爍藍色作為提示
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

// 快速設定顏色的函式
void setLedColor(uint32_t color) {
  for(int i=0; i<NUM_PIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
}

