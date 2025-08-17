#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Camera pin configuration for AI Thinker
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// WiFi credentials
const char* ssid = "DESKTOP-9KJA4DL 2973";
const char* password = "2152s4#D";

// WebSocket server details (Python script)
const char* websocket_host = "192.168.18.103";  // Laptop hotspot IP
const int websocket_port = 8766;  // Different port for camera stream

// Camera settings for performance
#define FRAME_SIZE FRAMESIZE_QVGA  // 320x240
#define JPEG_QUALITY 12
#define FB_COUNT 2

WebSocketsClient webSocket;
bool websocket_connected = false;
unsigned long last_frame_time = 0;
const unsigned long FRAME_INTERVAL = 200;  // Send frame every 200ms (5 FPS)

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

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("✗ Camera init failed");
    while (1) delay(1000);
  }
  Serial.println("✓ Camera initialized");
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
        
        // Send identification message
        StaticJsonDocument<200> doc;
        doc["type"] = "camera_connected";
        doc["device"] = "ESP32-CAM";
        doc["resolution"] = "320x240";
        doc["fps"] = 5;
        
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
  
  // Wait for connection
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
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("✗ Camera capture failed");
    return;
  }
  
  // Create JSON message with base64 encoded image
  StaticJsonDocument<1000> doc;
  doc["type"] = "camera_frame";
  doc["timestamp"] = current_time;
  doc["size"] = fb->len;
  
  // Send binary data directly (more efficient than base64)
  webSocket.sendBIN(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  last_frame_time = current_time;
  
  // Print status every 50 frames (10 seconds at 5 FPS)
  static int frame_count = 0;
  if (++frame_count % 50 == 0) {
    Serial.printf("📹 Sent %d frames to server\n", frame_count);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("📷 ESP32-CAM WebSocket Streamer Starting...");
  
  // Initialize camera
  startCamera();
  
  // Connect to WiFi
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("✓ Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Connect to WebSocket server
  connectToWebSocket();
  
  Serial.println("✓ System ready - Streaming to WebSocket server!");
}

void loop() {
  // Handle WebSocket events
  webSocket.loop();
  
  // Send camera frames
  if (websocket_connected) {
    sendCameraFrame();
  } else {
    // Try to reconnect if disconnected
    static unsigned long last_reconnect = 0;
    if (millis() - last_reconnect > 5000) {
      Serial.println("🔄 Attempting to reconnect WebSocket...");
      connectToWebSocket();
      last_reconnect = millis();
    }
  }
  
  delay(10);  // Small delay
}
