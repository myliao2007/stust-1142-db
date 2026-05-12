#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48      // 板載 WS2812 腳位
#define TRIGGER_THRESHOLD 15 // 影像變化閾值

Adafruit_NeoPixel statusLight(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  statusLight.begin();
  
  // 相機初始化配置 (針對 S3-CAM)
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 11; config.pin_d1 = 9;  config.pin_d2 = 8;  config.pin_d3 = 10;
  config.pin_d4 = 12; config.pin_d5 = 18; config.pin_d6 = 17; config.pin_d7 = 16;
  config.pin_xclk = 15; config.pin_pclk = 13; config.pin_vsync = 6;
  config.pin_href = 7;  config.pin_sscb_sda = 4; config.pin_sscb_scl = 5;
  config.pin_pwdn = -1; config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; // 使用灰階利於運算
  config.frame_size = FRAMESIZE_QQVGA;      // 低解析度加快辨識速度
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera Init Failed");
    return;
  }
}

void loop() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return;

  // 簡易偵測邏輯：計算影像中心區域的亮度均值
  long sum = 0;
  for (int i = 0; i < fb->len; i += 10) { // 抽樣計算
    sum += fb->buf[i];
  }
  int avg = sum / (fb->len / 10);

  static int lastAvg = 0;
  int diff = abs(avg - lastAvg);

  if (diff > TRIGGER_THRESHOLD) {
    Serial.println("Object Detected! Changing to GREEN.");
    statusLight.setPixelColor(0, statusLight.Color(0, 255, 0)); // 綠燈亮
    statusLight.show();
    delay(3000); // 綠燈持續時間
  } else {
    statusLight.setPixelColor(0, statusLight.Color(255, 0, 0)); // 紅燈亮
    statusLight.show();
  }

  lastAvg = avg;
  esp_camera_fb_return(fb);
  delay(100);
}
