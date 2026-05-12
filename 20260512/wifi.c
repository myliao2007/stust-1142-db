#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Adafruit_NeoPixel.h>

// --- 網路設定 ---
const char* ssid = "Aaron";
const char* password = "876543210";

// --- 硬體定義 ---
#define RGB_PIN 48 
Adafruit_NeoPixel pixels(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

// GouuuuTech S3-CAM 腳位配置
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM    13

// --- 系統狀態全域變數 ---
bool isStreaming = false;              // 記錄是否有 HTTP 串流連線
bool wasConnected = false;             // 記錄前一次的 WiFi 狀態
unsigned long lastWifiCheckTime = 0;   // WiFi 重連計時器
unsigned long manualOverrideEndTime = 0; // 手動控制覆蓋時間

// --- 網頁介面 ---
static const char* index_html = R"rawterm(
<!DOCTYPE html><html><head><title>S3-CAM Monitor</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { text-align: center; font-family: sans-serif; background: #f4f4f4; }
  .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; }
  img { width: 100%; border-radius: 5px; }
  .btn { padding: 15px; width: 80px; margin: 5px; border: none; cursor: pointer; color: white; border-radius: 5px; font-weight: bold; }
  .red { background: #ff4d4d; } .green { background: #2ecc71; } .blue { background: #3498db; }
</style></head>
<body><div class="container">
  <h2>AIoT Monitor</h2>
  <img src="/stream">
  <div style="margin-top:20px;">
    <button class="btn red" onclick="fetch('/led?color=red')">RED</button>
    <button class="btn green" onclick="fetch('/led?color=green')">GREEN</button>
    <button class="btn blue" onclick="fetch('/led?color=blue')">BLUE</button>
  </div>
</div></body></html>)rawterm";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t cmd_httpd = NULL;

// 影像串流處理
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];

    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    isStreaming = true; // 標記：HTTP 串流已建立

    while(true) {
        fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }
        size_t hlen = snprintf((char *)part_buf, 64, "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        
        if(httpd_resp_send_chunk(req, (const char *)part_buf, hlen) != ESP_OK) { 
            esp_camera_fb_return(fb); 
            break; 
        }
        if(httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) { 
            esp_camera_fb_return(fb); 
            break; 
        }
        esp_camera_fb_return(fb);
    }
    
    isStreaming = false; // 標記：HTTP 串流已斷開 (使用者關閉網頁)
    return res;
}

// 網頁根目錄
static esp_err_t index_handler(httpd_req_t *req) {
    return httpd_resp_send(req, index_html, strlen(index_html));
}

// LED 控制處理程序
static esp_err_t led_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    char color_val[16] = {0};

    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "color", color_val, sizeof(color_val)) == ESP_OK) {
                
                // 設定手動覆蓋時間：接下來的 3000 毫秒內，不更新系統狀態燈號
                manualOverrideEndTime = millis() + 3000; 

                if (strcmp(color_val, "red") == 0) pixels.setPixelColor(0, pixels.Color(255, 0, 0));
                else if (strcmp(color_val, "green") == 0) pixels.setPixelColor(0, pixels.Color(0, 255, 0));
                else if (strcmp(color_val, "blue") == 0) pixels.setPixelColor(0, pixels.Color(0, 0, 255));
                pixels.show();
            }
        }
        free(buf);
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

void setup() {
    Serial.begin(115200);
    pixels.begin();
    
    // 開機初始狀態：紅色 (代表尚未連線)
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
    pixels.show();

    // 相機初始化
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM; config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2; // N16R8 利用 8MB PSRAM

    if (esp_camera_init(&config) != ESP_OK) { Serial.println("Camera Failed"); return; }

    // 啟動 WiFi (非阻塞設計的起點，這裡先嘗試連線)
    WiFi.begin(ssid, password);

    // 啟動 HTTP 伺服器
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = 80;
    httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
    httpd_uri_t led_uri = { .uri = "/led", .method = HTTP_GET, .handler = led_handler };
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };

    if (httpd_start(&cmd_httpd, &server_config) == ESP_OK) {
        httpd_register_uri_handler(cmd_httpd, &index_uri);
        httpd_register_uri_handler(cmd_httpd, &led_uri);
        httpd_register_uri_handler(cmd_httpd, &stream_uri);
    }
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. 檢查 WiFi 連線狀態與重連邏輯
    if (WiFi.status() != WL_CONNECTED) {
        if (wasConnected) {
            Serial.println("WiFi Disconnected!");
            wasConnected = false;
        } 
        // 每 5 秒嘗試重新連線一次
        if (currentMillis - lastWifiCheckTime >= 5000) {
            Serial.print("Attempting to reconnect WiFi to ");
            Serial.println(ssid);
            WiFi.disconnect();
            WiFi.begin(ssid, password);
            lastWifiCheckTime = currentMillis;
        }
    } else {
        if (!wasConnected) {
            Serial.println("\nWiFi Connected Successfully!");
            Serial.print("Current IP Address: ");
            Serial.println(WiFi.localIP()); // 印出 IP
            wasConnected = true;
        }
    }

    // 2. 系統狀態 LED 燈號更新
    // 只有在「沒有被網頁按鈕手動覆蓋」的期間，才由系統自動接管燈號
    if (currentMillis > manualOverrideEndTime) {
        if (!wasConnected) {
            pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // 斷線：紅色
        } else if (isStreaming) {
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // HTTP 串流中：藍色
        } else {
            pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // 已連線且閒置：綠色
        }
        pixels.show();
    }

    delay(100); // 降低 CPU 負擔
}
