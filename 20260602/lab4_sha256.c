#include <Arduino.h>
#include "mbedtls/sha256.h"
#include <Adafruit_NeoPixel.h> // 引入標準 NeoPixel 程式庫

// =========================================================
// 燈號硬體定義 (ESP32-S3-CAM 板載 RGB 燈在 GPIO 48)
// =========================================================
#define RGB_LED_PIN   48
#define NUM_PIXELS    1
#define GROUP		"[Group 0] "

Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =========================================================
// 設定測試參數
// =========================================================
#define DATA_SIZE 32768  // 32KB 資料塊
#define TEST_ROUNDS 50   // 重複運算 50 次以計算平均

uint8_t *test_buffer;
uint8_t hash_result[32]; // SHA-256 產出 32 bytes (256 bits)

// =========================================================
// 1. 純軟體 SHA-256 實作 (Mini Pure C) - 教學觀測用
// =========================================================
#define ROTRIGHT(word,bits) (((word) >> (bits)) | ((word) << (32-(bits))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const uint32_t k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

struct SW_SHA256_CTX {
  uint8_t data[64];
  uint32_t datalen;
  unsigned long long bitlen;
  uint32_t state[8];
};

void sw_sha256_transform(SW_SHA256_CTX *ctx, const uint8_t data[]) {
  uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
  for (i = 0, j = 0; i < 16; ++i, j += 4)
    m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
  for ( ; i < 64; ++i)
    m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

  a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
  e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

  for (i = 0; i < 64; ++i) {
    t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
    t2 = EP0(a) + MAJ(a, b, c);
    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }
  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
  ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void sw_sha256_init(SW_SHA256_CTX *ctx) {
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

void sw_sha256_update(SW_SHA256_CTX *ctx, const uint8_t data[], size_t len) {
  for (size_t i = 0; i < len; ++i) {
    ctx->data[ctx->datalen] = data[i];
    ctx->datalen++;
    if (ctx->datalen == 64) {
      sw_sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

void sw_sha256_final(SW_SHA256_CTX *ctx, uint8_t hash[]) {
  uint32_t i = ctx->datalen;
  if (ctx->datalen < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56) ctx->data[i++] = 0x00;
  } else {
    ctx->data[i++] = 0x80;
    while (i < 64) ctx->data[i++] = 0x00;
    sw_sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }
  ctx->bitlen += ctx->datalen * 8;
  ctx->data[63] = ctx->bitlen;
  ctx->data[62] = ctx->bitlen >> 8;
  ctx->data[61] = ctx->bitlen >> 16;
  ctx->data[60] = ctx->bitlen >> 24;
  ctx->data[59] = ctx->bitlen >> 32;
  ctx->data[58] = ctx->bitlen >> 40;
  ctx->data[57] = ctx->bitlen >> 48;
  ctx->data[56] = ctx->bitlen >> 56;
  sw_sha256_transform(ctx, ctx->data);
  for (i = 0; i < 4; ++i) {
    hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
  }
}

// =========================================================
// 2. 測試主程式
// =========================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);

  // 初始化 NeoPixel RGB
  pixels.begin();
  pixels.clear();
  pixels.show();

  // 配置緩衝區至 ESP32-S3 的高容量 PSRAM
  test_buffer = (uint8_t*)ps_malloc(DATA_SIZE);
  if (test_buffer == NULL) {
    Serial.println("PSRAM 配置失敗！");
    pixels.setPixelColor(0, pixels.Color(100, 0, 0)); // 亮紅燈警告
    pixels.show();
    return;
  }
  for(int i=0; i<DATA_SIZE; i++) test_buffer[i] = (uint8_t)(i & 0xFF);

  Serial.println("\n\n=================================================");
  Serial.println("  ⛓️  ESP32-S3 區塊鏈黑科技：極速礦機算力大賽 ⛓️ ");
  Serial.println("=================================================");
  Serial.printf("[區塊規格] 待打包交易資料: %d KB\n", DATA_SIZE / 1024);
  Serial.printf("[模擬難度] 預計重複開採輪數: %d 次\n", TEST_ROUNDS);
  Serial.printf("[總運算量] 算力目標總計: %.2f MB\n", (float)DATA_SIZE * TEST_ROUNDS / 1024.0 / 1024.0);
  Serial.println("-------------------------------------------------");

  bool led_state = false;

  // --- 1. 硬體加速測試 (mbedtls) ---
  Serial.println("🚀 [第一回合] 啟動 ESP32-S3 內建『硬體 ASIC 晶片礦機』...");
  unsigned long start_hw = micros();
  
  mbedtls_sha256_context ctx_hw;
  
  for(int i=0; i<TEST_ROUNDS; i++) {
    // ✨ 執行中藍閃爍
    if (i % 2 == 0) {
      led_state = !led_state;
      pixels.setPixelColor(0, pixels.Color(0, 0, led_state ? 50 : 0));
      pixels.show();
    }

    mbedtls_sha256_init(&ctx_hw);
    
    // 🛠️ 修正處：改回相容您環境的 mbedtls_sha256_starts
    mbedtls_sha256_starts(&ctx_hw, 0); 
    
    mbedtls_sha256_update(&ctx_hw, (const unsigned char*)test_buffer, DATA_SIZE);
    mbedtls_sha256_finish(&ctx_hw, hash_result);
    mbedtls_sha256_free(&ctx_hw);
  }
  unsigned long end_hw = micros();

  // --- 2. 純軟體測試 (Pure C) ---
  Serial.println("🐌 [第二回合] 降級切換至傳統『純軟體 CPU 模擬礦機』...");
  unsigned long start_sw = micros();
  
  SW_SHA256_CTX ctx_sw;
  
  for(int i=0; i<TEST_ROUNDS; i++) {
    // ✨ 執行中藍閃爍
    if (i % 2 == 0) {
      led_state = !led_state;
      pixels.setPixelColor(0, pixels.Color(0, 0, led_state ? 50 : 0));
      pixels.show();
    }

    sw_sha256_init(&ctx_sw);
    sw_sha256_update(&ctx_sw, test_buffer, DATA_SIZE);
    sw_sha256_final(&ctx_sw, hash_result);
  }
  unsigned long end_sw = micros();

  // --- 計算結果 ---
  float time_hw = (end_hw - start_hw) / 1000000.0;
  float time_sw = (end_sw - start_sw) / 1000000.0;
  float total_mb = (float)DATA_SIZE * TEST_ROUNDS / 1024.0 / 1024.0;

  Serial.print(GROUP);
  Serial.println("\n----------------- [ 算力大對決結果 ]  -----------------");
  Serial.printf("🤖 硬體 ASIC 礦機：耗時 %.4f 秒 | 算力速度: %.2f MB/s\n", time_hw, total_mb / time_hw);
  Serial.printf("💻 純軟體 CPU 礦機：耗時 %.4f 秒 | 算力速度: %.2f MB/s\n", time_sw, total_mb / time_sw);
  Serial.println("-----------------------------------------------------");
  Serial.printf("🔥 結論：硬體加速引擎比軟體代碼快了高達 %.2f 倍！\n", time_sw / time_hw);
  
  // 印出防偽交易簽章 Hash 值
  Serial.print("📦 成功封裝之區塊雜湊特徵 (Block Hash): ");
  for(int i=0; i<8; i++) Serial.printf("%02X", hash_result[i]);
  Serial.println("...[後面省略]");
  Serial.println("=====================================================");

  free(test_buffer);

  // ✨ 3. 執行成功：切換為定色【綠燈】
  pixels.setPixelColor(0, pixels.Color(0, 80, 0)); 
  pixels.show();
  Serial.println("\n>>> 🎉 [系統提示] 區塊驗證成功！礦機已停止，板載 RGB 轉為【綠燈】恆亮。");
}

void loop() {}
