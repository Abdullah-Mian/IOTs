
#include <RH_ASK.h>
#include <SPI.h>

// Create Amplitude Shift Keying Object
RH_ASK rf_driver;

void setup() {
    // Initialize ASK Object
    rf_driver.init();
    // Setup Serial Monitor
    Serial.begin(9600);
}

void loop() {
    // Set buffer to size of expected message
    uint8_t buf[24];
    uint8_t buflen = sizeof(buf);

    // Check if received packet is correct size
    if (rf_driver.recv(buf, &buflen)) {
        // Message received with valid checksum
        Serial.print("Message Received: ");
        Serial.println((char*)buf);
    }
}

