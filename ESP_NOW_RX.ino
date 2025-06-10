/*
ESP32 ESP-NOW Receiver with Fast ACK System
Optimized for immediate acknowledgment response
*/

#include <esp_now.h>
#include <WiFi.h>

typedef struct data_message {
  uint8_t msgType;    // 0 = data, 1 = ack
  uint16_t msgId;     // Message ID for tracking
  char data;          // Single character data
  uint32_t timestamp; // For latency measurement
} data_message;

data_message incomingMsg;
data_message ackMsg;

// Sender MAC will be captured automatically
uint8_t senderMAC[6];
esp_now_peer_info_t peerInfo = {};
bool peerAdded = false;

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  memcpy(&incomingMsg, incomingData, sizeof(data_message));
  
  // Auto-add sender as peer for ACK (only once)
  if (!peerAdded) {
    memcpy(senderMAC, recv_info->src_addr, 6);
    memcpy(peerInfo.peer_addr, senderMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      peerAdded = true;
      Serial.println("Sender auto-paired for ACK");
    }
  }
  
  // Process data messages only
  if (incomingMsg.msgType == 0) {
    // Display received character immediately
    Serial.print("Received: ");
    Serial.println(incomingMsg.data);
    
    // Send immediate ACK (critical for low latency)
    sendAck(incomingMsg.msgId);
  }
}

void sendAck(uint16_t msgId) {
  if (!peerAdded) return;
  
  // Prepare ACK message
  ackMsg.msgType = 1;  // ACK message
  ackMsg.msgId = msgId;
  ackMsg.data = 0;     // Not used for ACK
  ackMsg.timestamp = millis();
  
  // Send ACK immediately - no delay
  esp_now_send(senderMAC, (uint8_t*)&ackMsg, sizeof(data_message));
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Silent ACK sending - no serial output to maintain speed
}

void setup() {
  Serial.begin(115200);
  
  // Basic WiFi initialization - Arduino compatible
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  delay(500);
  
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  
  // Register callbacks
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  
  Serial.println("Fast ESP-NOW Receiver ready. Auto-ACK enabled.");
}

void loop() {
  // ESP-NOW handles everything via callbacks
  // Minimal delay to prevent watchdog issues
  delay(1);
}
