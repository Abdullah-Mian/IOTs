/*
ESP32 ESP-NOW Transmitter with Fast ACK System
Optimized for low latency and reliability
*/

#include <esp_now.h>
#include <WiFi.h>

// Replace with actual receiver MAC address
uint8_t receiverMAC[] = {0xCC, 0xDB, 0xA7, 0x15, 0x36, 0xC4};

typedef struct data_message {
  uint8_t msgType;    // 0 = data, 1 = ack
  uint16_t msgId;     // Message ID for tracking
  char data;          // Single character data
  uint32_t timestamp; // For latency measurement
} data_message;

data_message outgoingMsg;
data_message incomingMsg;

// Transmission control variables
uint16_t currentMsgId = 0;
bool ackReceived = false;
unsigned long sendTime = 0;
const unsigned long ACK_TIMEOUT = 50;  // 50ms timeout (very fast)
const uint8_t MAX_RETRIES = 3;

// Performance optimization settings
esp_now_peer_info_t peerInfo = {};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Don't print status to reduce serial overhead - just track timing
  if (status != ESP_NOW_SEND_SUCCESS) {
    // Hardware send failed - immediate retry
    esp_now_send(receiverMAC, (uint8_t*)&outgoingMsg, sizeof(data_message));
  }
}

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  memcpy(&incomingMsg, incomingData, sizeof(data_message));
  
  // Process only ACK messages
  if (incomingMsg.msgType == 1 && incomingMsg.msgId == currentMsgId) {
    ackReceived = true;
    unsigned long latency = millis() - sendTime;
    Serial.print("ACK received! Latency: ");
    Serial.print(latency);
    Serial.println("ms");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Basic WiFi initialization - Arduino compatible
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  delay(500);
  
  Serial.print("Transmitter MAC: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  
  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Configure peer with performance settings
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  Serial.println("Fast ESP-NOW Transmitter ready. Enter character:");
}

void loop() {
  if (Serial.available()) {
    char inputChar = Serial.read();
    
    if (inputChar >= 32 && inputChar <= 126) {
      sendWithRetry(inputChar);
    }
  }
}

void sendWithRetry(char data) {
  currentMsgId++;
  ackReceived = false;
  
  // Prepare message
  outgoingMsg.msgType = 0;  // Data message
  outgoingMsg.msgId = currentMsgId;
  outgoingMsg.data = data;
  outgoingMsg.timestamp = millis();
  
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    sendTime = millis();
    
    // Send message
    esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&outgoingMsg, sizeof(data_message));
    
    if (result == ESP_OK) {
      // Wait for ACK with minimal delay checking
      unsigned long startWait = millis();
      while (!ackReceived && (millis() - startWait) < ACK_TIMEOUT) {
        delay(1); // Minimal delay for ESP-NOW processing
      }
      
      if (ackReceived) {
        Serial.print("✓ Sent '");
        Serial.print(data);
        Serial.print("' (attempt ");
        Serial.print(attempt + 1);
        Serial.println(")");
        return; // Success!
      }
    }
    
    // Failed - brief delay before retry
    if (attempt < MAX_RETRIES - 1) {
      delay(5); // Very short retry delay
      Serial.print("⟲ Retry ");
      Serial.println(attempt + 2);
    }
  }
  
  // All retries failed
  Serial.print("✗ Failed to send '");
  Serial.print(data);
  Serial.println("' after all retries");
}
