#include <RH_ASK.h>
#include <SPI.h>

// Joystick pins
#define X_AXIS A7
#define Y_AXIS A6
#define BUTTON_PIN 2

RH_ASK rf_driver;

char lastSent = '\0'; // To keep track of last sent character

void setup() {
  pinMode(X_AXIS, INPUT);
  pinMode(Y_AXIS, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  rf_driver.init();
  Serial.begin(9600);
}

void loop() {
  int xValue = analogRead(X_AXIS);
  int yValue = analogRead(Y_AXIS);
  bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;

  char direction = '\0';

  if (yValue < 400) {
    direction = 'W'; // Forward
  } else if (yValue > 600) {
    direction = 'B'; // Backward
  } else if (xValue < 400) {
    direction = 'L'; // Left
  } else if (xValue > 600) {
    direction = 'R'; // Right
  }

  // Only send if direction changed and it's valid
  if (direction != lastSent) {
    rf_driver.send((uint8_t *)&direction, 1);
    rf_driver.waitPacketSent();
    Serial.print("Sent: ");
    Serial.println(direction);
    lastSent = direction;
  }

  delay(100); // Delay for readability
}
