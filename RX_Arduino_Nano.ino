#include <RH_ASK.h>
#include <SPI.h>

// Create Amplitude Shift Keying Object
RH_ASK rf_driver;

// Define pin for output
#define OUTPUT_PIN 5  // Using D5 for output

void setup() {
  // Initialize ASK Object
  rf_driver.init();
  
  // Setup Serial Monitor
  Serial.begin(9600);
  
  // Setup output pin
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);  // Start with output LOW
  
  Serial.println("Receiver ready, waiting for 'W' character...");
}

void loop() {
  // Set buffer for single character
  uint8_t buf[2];  // Small buffer for single char plus null terminator
  uint8_t buflen = sizeof(buf);

  // Check if received packet is correct size
  if (rf_driver.recv(buf, &buflen)) {
    // Ensure null termination for character
    buf[buflen] = '\0';
    
    // Check if we received the character 'W'
    if (buflen == 1 && buf[0] == 'W') {
      Serial.println("Received 'W' - Setting D5 HIGH for 5 seconds");
      
      // Set D5 pin HIGH
      digitalWrite(OUTPUT_PIN, HIGH);
      
      // Wait 5 seconds
      delay(5000);
      
      // Set D5 pin LOW again
      digitalWrite(OUTPUT_PIN, LOW);
      Serial.println("D5 set back to LOW");
    } else {
      // Print what was received for debugging
      Serial.print("Received other data: ");
      Serial.println((char*)buf);
    }
  }
}

