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

  Serial.println("\n----------------- [ 算力大對決結果 ] -----------------");
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
