/*
 * ESP32-S3 CAM High-Quality MJPEG Streaming with Automatic Discovery
 * - Maximum quality with minimal lag
 * - Automatic mDNS discovery (no hardcoded IPs!)
 * - Optimized for ESP32-S3 8MB PSRAM
 * 
 * Board: ESP32S3 Dev Module
 * PSRAM: OPI PSRAM
 * Partition: 16MB Flash (3MB APP/9.9MB FATFS)
 */

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <WiFi.h>
#include <ESPmDNS.h>

// CORRECT Camera pins for ESP32-S3-WROOM-N16R8 with 24-pin FPC
// These are VERIFIED for your specific board from Digilog.pk
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4   // I2C SDA
#define SIOC_GPIO_NUM     5   // I2C SCL

#define Y9_GPIO_NUM       16  // D7
#define Y8_GPIO_NUM       17  // D6
#define Y7_GPIO_NUM       18  // D5
#define Y6_GPIO_NUM       12  // D4
#define Y5_GPIO_NUM       10  // D3
#define Y4_GPIO_NUM       8   // D2
#define Y3_GPIO_NUM       9   // D1
#define Y2_GPIO_NUM       11  // D0
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// Network configuration
const char* ssid = "Victus";
const char* password = "68986898";
const char* mdns_hostname = "esp32cam";  // Access via: http://esp32cam.local

// HIGH QUALITY settings optimized for ESP32-S3
#define FRAME_SIZE FRAMESIZE_SVGA        // 800x600 - High quality
#define JPEG_QUALITY 10                  // Best quality (1-63, lower = better)
#define FB_COUNT 2                       // Double buffering for smooth streaming

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// MJPEG boundary marker
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Performance monitoring
unsigned long frame_count = 0;
unsigned long last_fps_time = 0;
float current_fps = 0.0;

esp_err_t init_camera() {
  Serial.println("Initializing ESP32-S3 camera with high-quality settings...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAME_SIZE;
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count = FB_COUNT;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  // Use PSRAM for frame buffers (ESP32-S3 has 8MB!)
  config.fb_location = CAMERA_FB_IN_PSRAM;
  
  Serial.printf("Free heap before camera init: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return err;
  }
  
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // Optimize for high quality and smooth streaming
    s->set_framesize(s, FRAME_SIZE);
    s->set_quality(s, JPEG_QUALITY);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_lenc(s, 1);
  }
  
  Serial.println("Camera initialized successfully!");
  Serial.printf("Resolution: 800x600, Quality: %d\n", JPEG_QUALITY);
  Serial.printf("Free heap after init: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM after init: %d bytes\n", ESP.getFreePsram());
  
  return ESP_OK;
}

// MJPEG streaming handler
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;

      // FPS calculation
      frame_count++;
      unsigned long current_time = millis();
      if (current_time - last_fps_time >= 1000) {
        current_fps = frame_count / ((current_time - last_fps_time) / 1000.0);
        last_fps_time = current_time;
        frame_count = 0;
        Serial.printf("Streaming FPS: %.1f | Heap: %d | PSRAM: %d\n", 
                     current_fps, ESP.getFreeHeap(), ESP.getFreePsram());
      }
    }
    
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    }
    
    if (res != ESP_OK) {
      break;
    }
  }
  
  return res;
}

// Single frame capture handler
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// Status page handler
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = 
    "<!DOCTYPE html><html><head><title>ESP32-S3 CAM</title>"
    "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff}"
    "h1{color:#00ff00}.info{background:#2a2a2a;padding:15px;margin:10px 0;border-radius:5px}"
    ".stream{width:100%;max-width:800px;border:2px solid #00ff00}</style></head>"
    "<body><h1>ESP32-S3 CAM High-Quality Stream</h1>"
    "<div class='info'><strong>Resolution:</strong> 800x600 SVGA<br>"
    "<strong>Quality:</strong> 10 (High)<br>"
    "<strong>Access URL:</strong> http://esp32cam.local<br>"
    "<strong>Stream:</strong> <a href='/stream' style='color:#00ff00'>/stream</a><br>"
    "<strong>Snapshot:</strong> <a href='/capture' style='color:#00ff00'>/capture</a></div>"
    "<h2>Live Stream:</h2>"
    "<img class='stream' src='/stream'>"
    "<script>setInterval(()=>{fetch('/status').then(r=>r.text()).then(console.log)},5000)</script>"
    "</body></html>";
  
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

// Status handler
static esp_err_t status_handler(httpd_req_t *req) {
  char json[200];
  snprintf(json, sizeof(json), 
          "{\"fps\":%.1f,\"heap\":%d,\"psram\":%d,\"quality\":%d,\"resolution\":\"800x600\"}",
          current_fps, ESP.getFreeHeap(), ESP.getFreePsram(), JPEG_QUALITY);
  
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, json, strlen(json));
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t status_uri = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
  };

  Serial.println("Starting web server...");
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    Serial.println("HTTP server started successfully!");
  }
}

bool setupMDNS() {
  Serial.printf("Starting mDNS with hostname: %s\n", mdns_hostname);
  
  if (!MDNS.begin(mdns_hostname)) {
    Serial.println("Error starting mDNS");
    return false;
  }
  
  // Advertise HTTP service
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("camera", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "model", "ESP32-S3-CAM");
  MDNS.addServiceTxt("http", "tcp", "resolution", "800x600");
  MDNS.addServiceTxt("camera", "tcp", "stream", "/stream");
  
  Serial.println("mDNS responder started successfully!");
  Serial.printf("Access camera at: http://%s.local\n", mdns_hostname);
  Serial.printf("Stream URL: http://%s.local/stream\n", mdns_hostname);
  Serial.printf("Snapshot URL: http://%s.local/capture\n", mdns_hostname);
  
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n======================================");
  Serial.println("ESP32-S3 CAM High-Quality Streamer");
  Serial.println("With Automatic mDNS Discovery");
  Serial.println("======================================");
  
  // Initialize camera
  if (init_camera() != ESP_OK) {
    Serial.println("Camera initialization failed!");
    return;
  }
  
  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);  // Disable WiFi sleep for better performance
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
    return;
  }
  
  Serial.println();
  Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
  
  // Setup mDNS for automatic discovery
  if (!setupMDNS()) {
    Serial.println("mDNS setup failed, but continuing...");
    Serial.printf("Manual access: http://%s\n", WiFi.localIP().toString().c_str());
  }
  
  // Start camera server
  startCameraServer();
  
  Serial.println("\n======================================");
  Serial.println("System Ready!");
  Serial.println("======================================");
  Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Total Heap: %d bytes\n", ESP.getHeapSize());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("======================================");
}

void loop() {
  // Keep mDNS alive
  delay(1000);
}