#include <Wire.h>

void setup() {
  Serial.begin(115200); // Start serial communication at 115200 baud
  Wire.begin();         // Initialize I2C communication

  Serial.println("\nI2C Scanner");
}

void loop() {
  Serial.println("Scanning for I2C devices...");
  
  int deviceCount = 0;
  
  // Scan I2C addresses from 0x03 to 0x77
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Device found at address 0x");
      if (address < 16) Serial.print("0"); // Add leading zero for single-digit hex
      Serial.println(address, HEX);
      deviceCount++;
    } else if (error == 4) {
      Serial.print("Error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.println("I2C scan complete.");
  }
  
  delay(1000); // Wait 5 seconds before scanning again
}
