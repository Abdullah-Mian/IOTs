// Optimized for your specific ESP32-CAM (based on diagnostics)
#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// WiFi credentials
const char* ssid = "Victus";
const char* password = "68986898";

// WebSocket server details
const char* websocket_host = "192.168.137.1";
const int websocket_port = 8765;

// OPTIMIZED camera settings for your hardware (4MB PSRAM, 281KB free RAM)
#define FRAME_SIZE FRAMESIZE_VGA     // Can use VGA (640x480) with your PSRAM!
#define JPEG_QUALITY 12              // Good quality
#define FB_COUNT 2                   // Can use 2 buffers with your PSRAM

WebSocketsClient webSocket;
bool websocket_connected = false;
unsigned long last_frame_time = 0;
const unsigned long FRAME_INTERVAL = 200;  // 5 FPS - your hardware can handle it!

void startCamera() {
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
  
  // Use PSRAM for camera buffers (you have 4MB available!)
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  
  Serial.println("✓ Using PSRAM for camera buffers (4MB available)");

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("✗ Camera init failed: 0x%x\n", err);
    while (1) delay(1000);
  }
  
  Serial.println("✓ Camera initialized with VGA resolution");
  Serial.printf("📊 Free RAM after init: %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("📊 Free PSRAM after init: %d MB\n", ESP.getFreePsram() / (1024*1024));
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("✗ WebSocket Disconnected!");
      websocket_connected = false;
      break;
      
    case WStype_CONNECTED:
      {
        Serial.printf("✓ WebSocket Connected to: %s\n", payload);
        websocket_connected = true;
        
        StaticJsonDocument<200> doc;
        doc["type"] = "camera_connected";
        doc["device"] = "ESP32-CAM";
        doc["resolution"] = "640x480";  // VGA
        doc["fps"] = 5;
        doc["psram"] = "4MB";
        
        String message;
        serializeJson(doc, message);
        webSocket.sendTXT(message);
        break;
      }
      
    case WStype_TEXT:
      Serial.printf("📨 Received: %s\n", payload);
      break;
      
    case WStype_ERROR:
      Serial.printf("✗ WebSocket Error: %s\n", payload);
      websocket_connected = false;
      break;
      
    default:
      break;
  }
}

void connectToWebSocket() {
  Serial.println("🌐 Connecting to WebSocket server...");
  
  webSocket.begin(websocket_host, websocket_port, "/camera");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  unsigned long start_time = millis();
  while (!websocket_connected && (millis() - start_time < 15000)) {
    webSocket.loop();
    delay(100);
  }
  
  if (websocket_connected) {
    Serial.println("✓ WebSocket connected successfully!");
  } else {
    Serial.println("✗ WebSocket connection failed!");
  }
}

void sendCameraFrame() {
  if (!websocket_connected) return;
  
  unsigned long current_time = millis();
  if (current_time - last_frame_time < FRAME_INTERVAL) return;
  
  // With your abundant PSRAM, we don't need strict memory checks
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("✗ Camera capture failed");
    return;
  }
  
  // Your hardware can handle larger frames
  webSocket.sendBIN(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  last_frame_time = current_time;
  
  static int frame_count = 0;
  if (++frame_count % 50 == 0) {
    Serial.printf("📹 Sent %d frames | RAM: %dKB | PSRAM: %dMB\n", 
                  frame_count, ESP.getFreeHeap()/1024, ESP.getFreePsram()/(1024*1024));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("📷 ESP32-CAM Optimized WebSocket Streamer");
  
  // Print your excellent hardware specs
  Serial.printf("🔧 Hardware: Total RAM: %dKB, PSRAM: %dMB\n", 
                ESP.getHeapSize()/1024, ESP.getPsramSize()/(1024*1024));
  
  startCamera();
  
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("✓ Connected! IP: ");
  Serial.println(WiFi.localIP());
  
  connectToWebSocket();
  
  Serial.println("✅ System ready - High quality streaming enabled!");
}

void loop() {
  webSocket.loop();
  
  if (websocket_connected) {
    sendCameraFrame();
  } else {
    static unsigned long last_reconnect = 0;
    if (millis() - last_reconnect > 5000) {
      Serial.println("🔄 Reconnecting WebSocket...");
      connectToWebSocket();
      last_reconnect = millis();
    }
  }
  
  delay(10);
}