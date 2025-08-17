#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "ELEVEN";      // Your WiFi network name
const char* password = "areusingle";  // Your WiFi password

// WebSocket server settings
const char* ws_server = "192.168.18.126";  // Your laptop's IP address
// Alternate IP if needed: const char* ws_server = "192.168.137.1";
const int ws_port = 8765;

// LED pin definitions
const int LED1 = 13;  // LED for character 'E'
const int LED2 = 12;  // LED for character 'D'
const int LED3 = 14;  // LED for character 'C'
const int LED4 = 27;  // LED for character 'C'

// Confidence threshold
const float CONFIDENCE_THRESHOLD = 0.95;

WebSocketsClient webSocket;
unsigned long lastConnectionAttempt = 0;
const unsigned long reconnectInterval = 5000;  // Try to reconnect every 5 seconds
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;  // Send heartbeat every 30 seconds

void controlLEDs(const String& character, float confidence) {
  // First turn all LEDs off
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  
  // Only turn on LED if confidence meets threshold
  if (confidence >= CONFIDENCE_THRESHOLD) {
    if (character == "E") {
      digitalWrite(LED1, HIGH);
      Serial.println("LED1 (E) ON");
    } 
    else if (character == "D") {
      digitalWrite(LED2, HIGH);
      Serial.println("LED2 (D) ON");
    }
    else if (character == "C") {
      digitalWrite(LED3, HIGH);
      digitalWrite(LED4, HIGH);
      Serial.println("LED3 (C) ON");
    }
  } else {
    Serial.println("All LEDs OFF (confidence below threshold or character not recognized)");
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected!");
      // Turn off all LEDs when disconnected
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      digitalWrite(LED3, LOW);
      digitalWrite(LED4, LOW);
      break;
      
    case WStype_CONNECTED:
      Serial.printf("WebSocket connected to: %s\n", ws_server);
      // Request initial prediction after connection
      webSocket.sendTXT("get_prediction");
      break;
      
    case WStype_TEXT: {
      // Convert payload to null-terminated string for debugging
      char* payloadStr = (char*)malloc(length + 1);
      memcpy(payloadStr, payload, length);
      payloadStr[length] = 0;
      Serial.printf("Received: %s\n", payloadStr);
      
      // Parse JSON data with character and confidence
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payloadStr);
      free(payloadStr);  // Free memory
      
      if (!error) {
        String character = doc["character"].as<String>();
        float confidence = doc["confidence"].as<float>();
        
        // Display on serial monitor
        Serial.println("-------------------------");
        Serial.printf("Character: %s\n", character.c_str());
        Serial.printf("Confidence: %.2f\n", confidence);
        
        // Control LEDs based on the prediction
        controlLEDs(character, confidence);
        
        Serial.println("-------------------------");
      } else {
        Serial.printf("Failed to parse JSON data: %s\n", error.c_str());
      }
      break;
    }
      
    case WStype_ERROR:
      Serial.println("WebSocket error");
      break;
      
    case WStype_BIN:
      Serial.println("Received binary data (ignoring)");
      break;
  }
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  Serial.println("\n\nESP32 Character Recognition Client with LED Control");
  
  // Initialize LED pins as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  
  // Turn off all LEDs initially
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Connecting to WebSocket server at %s:%d\n", ws_server, ws_port);
  
  // Configure WebSocket client
  webSocket.begin(ws_server, ws_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // Try to reconnect every 5 seconds
  
  // Enable debugging
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  Serial.println("WebSocket client started");
  Serial.println("LED1 = E, LED2 = D, LED3 = C");
  Serial.printf("Confidence threshold: %.2f\n", CONFIDENCE_THRESHOLD);
}

void loop() {
  // Handle WebSocket client
  webSocket.loop();
  
  // Send periodic heartbeat/keepalive
  unsigned long now = millis();
  if (now - lastHeartbeat > heartbeatInterval) {
    Serial.println("Sending heartbeat...");
    webSocket.sendTXT("ping");
    lastHeartbeat = now;
  }
  
  // Handle WiFi reconnection if needed
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (currentTime - lastConnectionAttempt > reconnectInterval) {
      Serial.println("Reconnecting to WiFi...");
      WiFi.reconnect();
      lastConnectionAttempt = currentTime;
    }
  }
  
  delay(10);  // Small delay to prevent watchdog trigger
}