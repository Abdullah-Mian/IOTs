// RadioHead Amplitude Shift Keying Library
#include <RH_ASK.h>
#include <SPI.h>

// Amplitude Shift Keying Object
RH_ASK rf_driver;

void setup() {
  // Initialize ASK Object
  rf_driver.init();
  Serial.begin(9600);
  Serial.println("Transmitting character 'W' every 10 seconds");
}

void loop() {
  // Send single character 'W'
  const char *msg = "W";
  rf_driver.send((uint8_t *)msg, 1);  // Send only 1 byte
  rf_driver.waitPacketSent();
  
  Serial.println("Sent 'W'");
  
  // Wait 10 seconds before sending again
  delay(10000);
}