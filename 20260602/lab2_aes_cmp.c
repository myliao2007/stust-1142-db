#include <Arduino.h>
#include "mbedtls/aes.h"
#include <Adafruit_NeoPixel.h> // 引入標準 NeoPixel 程式庫

// =========================================================
// 燈號硬體定義 (ESP32-S3-CAM 板載 RGB 燈通常在 GPIO 48)
// =========================================================
#define RGB_LED_PIN   48
#define NUM_PIXELS    1  // 板載通常只有 1 顆燈

// 宣告 NeoPixel 物件 (大部分 WS2812B 在此設定為 NEO_GRB + NEO_KHZ800)
Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =========================================================
// 設定測試參數：模擬 40KB 的檔案傳輸
// =========================================================
#define DATA_SIZE 40960  // 40KB (必須是 16 的倍數)
#define TEST_ROUNDS 100  // 重複傳輸 100 次以計算平均

// 準備大緩衝區 (分配至 PSRAM)
uint8_t *large_input_buffer;
uint8_t *large_output_buffer;

// 128-bit Key & IV
const uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
const uint8_t iv_template[16]  = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0};

uint8_t iv_sw[16]; 
uint8_t iv_hw[16];

// =========================================================
// 純軟體 AES 實作 (保持不變)
// =========================================================
static const uint8_t sbox[256] = { 
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 
};

#define ROTL8(x,shift) ((uint8_t) ((x) << (shift)) | ((x) >> (8 - (shift))))
void sw_sub_bytes(uint8_t *state) { for (uint8_t i = 0; i < 16; ++i) state[i] = sbox[state[i]]; }
void sw_shift_rows(uint8_t *state) {
  uint8_t tmp[4];
  tmp[0] = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp[0];
  tmp[0] = state[2]; tmp[1] = state[6]; state[2] = state[10]; state[6] = state[14]; state[10] = tmp[0]; state[14] = tmp[1];
  tmp[0] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = tmp[0];
}
void sw_mix_columns(uint8_t *state) {
  uint8_t tmp, tm, t;
  for (uint8_t i = 0; i < 4; ++i) {
    t = state[i*4];
    tmp = state[i*4] ^ state[i*4+1] ^ state[i*4+2] ^ state[i*4+3];
    tm = state[i*4] ^ state[i*4+1]; tm = (tm & 0x80) ? (tm << 1) ^ 0x1b : (tm << 1); state[i*4] ^= tm ^ tmp;
    tm = state[i*4+1] ^ state[i*4+2]; tm = (tm & 0x80) ? (tm << 1) ^ 0x1b : (tm << 1); state[i*4+1] ^= tm ^ tmp;
    tm = state[i*4+2] ^ state[i*4+3]; tm = (tm & 0x80) ? (tm << 1) ^ 0x1b : (tm << 1); state[i*4+2] ^= tm ^ tmp;
    tm = state[i*4+3] ^ t; tm = (tm & 0x80) ? (tm << 1) ^ 0x1b : (tm << 1); state[i*4+3] ^= tm ^ tmp;
  }
}
void sw_add_round_key(uint8_t *state, const uint8_t *round_key) { for (uint8_t i = 0; i < 16; ++i) state[i] ^= round_key[i]; }

void sw_aes128_encrypt_block(const uint8_t *in, uint8_t *out, const uint8_t *key) {
  uint8_t state[16];
  memcpy(state, in, 16);
  sw_add_round_key(state, key); 
  for(int round=1; round<10; ++round) {
      sw_sub_bytes(state); sw_shift_rows(state); sw_mix_columns(state); sw_add_round_key(state, key);
  }
  sw_sub_bytes(state); sw_shift_rows(state); sw_add_round_key(state, key);
  memcpy(out, state, 16);
}

void sw_aes128_cbc_encrypt_buffer(const uint8_t *input, uint8_t *output, size_t length, uint8_t *iv, const uint8_t *key) {
    uint8_t temp_iv[16];
    memcpy(temp_iv, iv, 16);

    for (size_t i = 0; i < length; i += 16) {
        uint8_t block[16];
        for (int j = 0; j < 16; j++) {
            block[j] = input[i + j] ^ temp_iv[j];
        }
        sw_aes128_encrypt_block(block, output + i, key);
        memcpy(temp_iv, output + i, 16);
    }
}

// =========================================================
// Setup
// =========================================================
void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    // ✨ 初始化 NeoPixel 燈條/燈泡
    pixels.begin();
    pixels.clear();
    pixels.show(); // 初始保持暗燈

    // 檢查 N16R8 的 PSRAM
    if (psramInit()) {
        Serial.printf("PSRAM 初始化成功。剩餘 PSRAM 空間: %d Bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("PSRAM 初始化失敗！");
        // 失敗時亮微弱紅燈以示警告
        pixels.setPixelColor(0, pixels.Color(50, 0, 0));
        pixels.show();
        return;
    }
    
    large_input_buffer = (uint8_t*)ps_malloc(DATA_SIZE);
    large_output_buffer = (uint8_t*)ps_malloc(DATA_SIZE);
    
    if (large_input_buffer == NULL || large_output_buffer == NULL) {
        Serial.println("記憶體配置失敗！");
        return;
    }
    
    for(int i=0; i<DATA_SIZE; i++) large_input_buffer[i] = (uint8_t)(i & 0xFF);

    Serial.println("\n\n===== [Group0] ESP32-S3 AES 大量資料傳輸測試 (40KB x 100次) =====");
    Serial.printf("資料總量: %.2f MB\n\n", (float)(DATA_SIZE * TEST_ROUNDS) / 1024.0 / 1024.0);

    // 用於追蹤閃爍狀態的旗標
    bool led_state = false;

    // --- 測試 1: 硬體加速 ---
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    unsigned long start_hw = micros();
    
    for(int k=0; k<TEST_ROUNDS; k++) {
        // ✨ 執行中閃藍燈：每 5 回合切換一次藍燈狀態
        if (k % 5 == 0) {
            led_state = !led_state;
            // 語法：pixels.setPixelColor(索引值, pixels.Color(R, G, B))
            pixels.setPixelColor(0, pixels.Color(0, 0, led_state ? 40 : 0));
            pixels.show(); // 更新硬體燈號
        }

        memcpy(iv_hw, iv_template, 16); 
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, DATA_SIZE, iv_hw, large_input_buffer, large_output_buffer);
    }
    
    unsigned long end_hw = micros();
    mbedtls_aes_free(&aes);

    // --- 測試 2: 純軟體運算 ---
    unsigned long start_sw = micros();
    
    for(int k=0; k<TEST_ROUNDS; k++) {
        // ✨ 執行中閃藍燈：維持相同頻率切換
        if (k % 5 == 0) {
            led_state = !led_state;
            pixels.setPixelColor(0, pixels.Color(0, 0, led_state ? 40 : 0));
            pixels.show();
        }

        memcpy(iv_sw, iv_template, 16); 
        sw_aes128_cbc_encrypt_buffer(large_input_buffer, large_output_buffer, DATA_SIZE, iv_sw, key);
    }
    
    unsigned long end_sw = micros();

    // --- 計算吞吐量 ---
    float time_hw_sec = (end_hw - start_hw) / 1000000.0;
    float time_sw_sec = (end_sw - start_sw) / 1000000.0;
    
    float total_bytes = (float)DATA_SIZE * TEST_ROUNDS;
    float speed_hw = (total_bytes / 1024.0) / time_hw_sec;
    float speed_sw = (total_bytes / 1024.0) / time_sw_sec;

    // --- 顯示結果 ---
    Serial.println("--- [硬體加速 (HW) Result] ---");
    Serial.printf("總耗時: %.4f 秒\n", time_hw_sec);
    Serial.printf("吞吐量: %.2f KB/s (%.2f MB/s)\n", speed_hw, speed_hw / 1024.0);
    
    Serial.println("\n--- [純軟體 (SW) Result] ---");
    Serial.printf("總耗時: %.4f 秒\n", time_sw_sec);
    Serial.printf("吞吐量: %.2f KB/s (%.2f MB/s)\n", speed_sw, speed_sw / 1024.0);

    Serial.println("\n-----------------------------");
    Serial.printf("ESP32-S3 硬體加速比純軟體快: %.2f 倍\n", speed_hw / speed_sw);
    
    free(large_input_buffer);
    free(large_output_buffer);

    // =========================================================
    // ✨ 運算結束：轉為穩定恆亮的【綠燈】
    // =========================================================
    pixels.setPixelColor(0, pixels.Color(0, 60, 0)); // 亮度設定為 60，清晰且不過刺眼
    pixels.show();
    
    Serial.println("\n>>> [系統提示] 測試完畢，Adafruit NeoPixel 已切換為【定色綠燈】。");
}

void loop() {}
