// Joystick pins
#define X_AXIS A0
#define Y_AXIS A1
#define BUTTON_PIN 2

void setup() {
  Serial.begin(9600);

  // Configure pins
  pinMode(X_AXIS, INPUT);
  pinMode(Y_AXIS, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Internal pull-up resistor
}

void loop() {
  // Read X and Y axes
  int xValue = analogRead(X_AXIS);
  int yValue = analogRead(Y_AXIS);

  // Read button state
  bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;

  // Print values to Serial Monitor
  Serial.print("X: ");
  Serial.print(xValue);
  Serial.print(" | Y: ");
  Serial.print(yValue);
  Serial.print(" | Button: ");
  Serial.println(buttonPressed ? "Pressed" : "Released");

  delay(2000);  // Small delay to make output readable
}
