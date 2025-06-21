/*
ESP32 S Transmitter Wiring:
Transmitter Module (RF-5V, 4 pins) -> ESP32 S
VCC -> 5V (use VIN pin or external 5V supply)
GND -> GND
DATA -> GPIO 19 (connected to transmitter's data pin) **MISO Standard Output Pin for SPI** 
Second DATA pin -> Leave unconnected
*/

#include <RH_ASK.h>
#include <SPI.h>

// Create ASK driver with 2000bps speed
// Arguments: speed, rx pin (unused=0), tx pin (GPIO19), ptt pin (unused=0)
RH_ASK driver(2000, 0, 19, 0);

void setup() {
  Serial.begin(115200);
  
  // Initialize ASK driver
  if (!driver.init()) {
    Serial.println("RF init failed");
    while(1);
  }
  
  Serial.println("433MHz RadioHead ESP32 Transmitter ready. Enter text to send:");
}

void loop() {
  if (Serial.available() > 0) {
    // Read the incoming string until newline
    String inputStr = Serial.readStringUntil('\n');
    
    // Convert to char array for RadioHead library
    const char *msg = inputStr.c_str();
    
    // Send the message
    driver.send((uint8_t *)msg, strlen(msg));
    driver.waitPacketSent();
    
    Serial.print("Sent: ");
    Serial.println(inputStr);
  }
}