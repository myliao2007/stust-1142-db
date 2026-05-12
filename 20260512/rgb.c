#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48  // 參考 image_db4f94.jpg
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixels.begin();
  pixels.setBrightness(50); // 避免太刺眼
}

void loop() {
  // 紅燈
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  delay(5000);

  // 綠燈
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();
  delay(5000);

  // 黃燈 (紅+綠)
  pixels.setPixelColor(0, pixels.Color(255, 255, 0));
  pixels.show();
  delay(2000);
}
