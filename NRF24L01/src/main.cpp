#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Define the pipe (address) for communication
const uint64_t my_radio_pipe = 0xE8E8F0F0E1LL; 

// Create RF24 radio object with CNS and CE pins
RF24 radio(9, 10);

// Joystick pins
#define X_AXIS A0
#define Y_AXIS A1
#define BUTTON_PIN 2

// Structure to hold joystick data
struct JoystickData {
  int xValue;     // X-axis value
  int yValue;     // Y-axis value
  bool button;    // Button state
};

JoystickData joystickData;

void setup() {
  Serial.begin(9600);

  // Configure joystick pins
  pinMode(X_AXIS, INPUT);
  pinMode(Y_AXIS, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Internal pull-up resistor

  // Initialize the radio
  radio.begin();
  
  // Set transmission configurations
  radio.setAutoAck(false);                // Disable Auto-Acknowledgment
  radio.setDataRate(RF24_250KBPS);        // Use low data rate for better range
  radio.openWritingPipe(my_radio_pipe);   // Open the writing pipe for this address
  
  Serial.println("Transmitter ready.");
}

void loop() {
  // Read joystick values
  joystickData.xValue = analogRead(X_AXIS);
  joystickData.yValue = analogRead(Y_AXIS);
  joystickData.button = digitalRead(BUTTON_PIN) == LOW; // LOW means pressed

  // Send joystick data
  bool success = radio.write(&joystickData, sizeof(joystickData));

  // Print status
  if (success) {
    Serial.print("Sent: X=");
    Serial.print(joystickData.xValue);
    Serial.print(" | Y=");
    Serial.print(joystickData.yValue);
    Serial.print(" | Button=");
    Serial.println(joystickData.button ? "Pressed" : "Released");
  } else {
    Serial.println("Send failed.");
  }

  delay(500); // Wait 2 seconds before sending again
}

