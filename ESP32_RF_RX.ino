/*
ESP32 WROOM-32 Receiver Wiring:
Receiver Module (3 pins) -> ESP32
VCC -> 3.3V
GND -> GND  
DATA -> GPIO 23 (MOSI standard SPI input pin)
*/

#include <RH_ASK.h>
#include <SPI.h>

// Create ASK driver with 2000bps speed to match transmitter
// Arguments: speed, rx pin (GPIO 23 - MOSI), tx pin (not used=0), ptt pin (unused=0)
RH_ASK driver(2000, 23, 0, 0);

void setup() {
  Serial.begin(115200);
  
  // Initialize ASK driver
  if (!driver.init()) {
    Serial.println("RF driver init failed");
    while(1);
  }
  
  Serial.println("433MHz RadioHead ESP32 Receiver ready. Listening on MOSI (GPIO 23)...");
}

void loop() {
  // Set buffer for receiving data
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);
  
  // Check if packet received
  if (driver.recv(buf, &buflen)) {
    // Null-terminate the received data
    buf[buflen] = '\0';
    
    // Print the message
    Serial.print("Received: ");
    Serial.println((char*)buf);
    
    // Print message length
    Serial.print("Message length: ");
    Serial.println(buflen);
    
    // Optional: Print raw data bytes in hex for debugging
    Serial.print("Raw data: ");
    for (int i = 0; i < buflen; i++) {
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}