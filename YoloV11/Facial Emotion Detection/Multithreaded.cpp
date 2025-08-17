#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
  #error Bluetooth is not enabled! Please run `make menuconfig` and enable it
#endif

// WiFi credentials
const char* ssid = "ELEVEN";      // Your WiFi network name
const char* password = "areusingle";  // Your WiFi password

// WebSocket server settings
const char* ws_server = "192.168.18.126";  // Your laptop's IP address
const int ws_port = 8765;

// LED pin definitions
const int LED1 = 13;  // LED for character 'E'
const int LED2 = 12;  // LED for character 'D'
const int LED3 = 14;  // LED for character 'C'
const int LED4 = 27;  // LED for character 'C'

// Motor control pins for L298N
const int IN1 = 26; // Motor 1 forward
const int IN2 = 25; // Motor 1 backward
const int IN3 = 33; // Motor 2 forward
const int IN4 = 32; // Motor 2 backward

// Confidence threshold
const float CONFIDENCE_THRESHOLD = 0.95;

// Task handles for FreeRTOS
TaskHandle_t WebSocketTaskHandle;
TaskHandle_t BluetoothTaskHandle;

// WebSocket client
WebSocketsClient webSocket;
unsigned long lastConnectionAttempt = 0;
const unsigned long reconnectInterval = 5000;  // Try to reconnect every 5 seconds
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;  // Send heartbeat every 30 seconds

// Bluetooth Serial
BluetoothSerial SerialBT;

// Function to prevent watchdog timer from resetting ESP32


// Function to control LEDs based on recognized character
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
      Serial.println("LED3 & LED4 (C) ON");
    }
  } else {
    Serial.println("All LEDs OFF (confidence below threshold or character not recognized)");
  }
}

// Motor control functions
void forward() {
  digitalWrite(IN1, LOW);  // Motor 1 forward
  digitalWrite(IN2, HIGH); // Motor 1 backward
  digitalWrite(IN3, LOW);  // Motor 2 forward
  digitalWrite(IN4, HIGH); // Motor 2 backward
  Serial.println("Moving Forward");
}

void backward() {
  digitalWrite(IN1, HIGH); // Motor 1 forward
  digitalWrite(IN2, LOW);  // Motor 1 backward
  digitalWrite(IN3, HIGH); // Motor 2 forward
  digitalWrite(IN4, LOW);  // Motor 2 backward
  Serial.println("Moving Backward");
}

void left() {
  digitalWrite(IN1, LOW);  // Motor 1 forward
  digitalWrite(IN2, HIGH); // Motor 1 backward
  digitalWrite(IN3, HIGH); // Motor 2 forward
  digitalWrite(IN4, LOW);  // Motor 2 backward
  Serial.println("Turning Left");
}

void right() {
  digitalWrite(IN1, HIGH); // Motor 1 forward
  digitalWrite(IN2, LOW);  // Motor 1 backward
  digitalWrite(IN3, LOW);  // Motor 2 forward
  digitalWrite(IN4, HIGH); // Motor 2 backward
  Serial.println("Turning Right");
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  Serial.println("Motors Stopped");
}

// WebSocket event handler
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

// WebSocket task function
void WebSocketTask(void * parameter) {
  Serial.println("WebSocket Task started on core " + String(xPortGetCoreID()));
  
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
  Serial.println("LED1 = E, LED2 = D, LED3&4 = C");
  Serial.printf("Confidence threshold: %.2f\n", CONFIDENCE_THRESHOLD);
  
  for(;;) {
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
    
    // Feed the watchdog and add a small delay
    delay(10);
  }
}

// Bluetooth motor control task function
void BluetoothTask(void * parameter) {
  Serial.println("Bluetooth Motor Control Task started on core " + String(xPortGetCoreID()));
  
  // Initialize Bluetooth
  SerialBT.begin("ESP32_Motor_Control");
  Serial.println("Bluetooth device started, you can pair with it now!");
  
  // Initialize all motors to stopped state
  stopMotors();
  
  for(;;) {
    // Check for Bluetooth commands
    if (SerialBT.available()) {
      char btChar = SerialBT.read();
      
      // Process the command
      switch (btChar) {
        case 'W':
        case 'w':
          forward();
          break;
        case 'S':
        case 's':
          stopMotors();
          break;
        case 'A':
        case 'a':
          left();
          break;
        case 'D':
        case 'd':
          right();
          break;
        case 'B':
        case 'b':
          backward();
          break;
        default:
          // Ignore unrecognized commands
          break;
      }
    }
    
    // Feed the watchdog and add a small delay
    delay(50);
  }
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  Serial.println("\n\nESP32 Multithreaded WebSocket and Bluetooth Control");
  
  // Initialize LED pins as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  
  // Initialize motor pins as outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Turn off all LEDs and motors initially
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  stopMotors();
  
  Serial.println("Starting tasks on separate cores...");
  
  // Create tasks on separate cores
  xTaskCreatePinnedToCore(
    WebSocketTask,         // Task function
    "WebSocketTask",       // Name of task
    8192,                  // Stack size (bytes)
    NULL,                  // Parameter to pass
    1,                     // Task priority
    &WebSocketTaskHandle,  // Task handle
    0                      // Core where the task should run (Core 0)
  );
  
  xTaskCreatePinnedToCore(
    BluetoothTask,         // Task function
    "BluetoothTask",       // Name of task
    4096,                  // Stack size (bytes)
    NULL,                  // Parameter to pass
    1,                     // Task priority
    &BluetoothTaskHandle,  // Task handle
    1                      // Core where the task should run (Core 1)
  );
  
  Serial.println("Tasks created successfully!");
  Serial.println("WebSocket task on Core 0, Bluetooth Motor Control on Core 1");
}

void loop() {
  // Main loop is empty as all functionality is in tasks
  delay(1000);
  
  // Print some stats occasionally
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}