#include <WiFi.h>

const char* ssid = "Abdullah Mian";
const char* password = "68986898";
const char* serverIP = "192.168.18.103";
const int serverPort = 5000;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

void loop() {
  if (!client.connected()) {
    if (client.connect(serverIP, serverPort)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Connection failed");
      delay(1000);
      return;
    }
  }
  
  client.println("get_prediction");
  while (client.connected() && !client.available()) delay(10);
  if (client.available()) {
    String response = client.readStringUntil('\n');
    Serial.println(response);
  }
  delay(1000);  // Request every second
}