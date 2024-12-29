#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// IR sensor pins
const int ir1Pin = 12; // IR sensor 1 input pin
const int ir2Pin = 14; // IR sensor 2 input pin
const int ir3Pin = 27; // IR sensor 3 input pin

// Output control pins
const int outPin16 = 15; // Output pin to control LED 1
const int outPin17 = 17; // Output pin to control LED 2
const int outPin5 = 23;  // Output pin to control LED 3

// Servo control pins
const int servoPin = 25;
const int servoPin1 = 26;

Servo myServo;
Servo myServo1;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Setup IR sensors as inputs
  pinMode(ir1Pin, INPUT);
  pinMode(ir2Pin, INPUT);
  pinMode(ir3Pin, INPUT);

  // Setup output pins as outputs
  pinMode(outPin16, OUTPUT);
  pinMode(outPin17, OUTPUT);
  pinMode(outPin5, OUTPUT);

  // Ensure output pins start LOW
  digitalWrite(outPin16, LOW);
  digitalWrite(outPin17, LOW);
  digitalWrite(outPin5, LOW);

  // Initialize the servos
  myServo.attach(servoPin);
  myServo1.attach(servoPin1);
  myServo.write(0);   // Start servo at 0°
  myServo1.write(0);  // Start servo at 0°

  // Initialize the LCD
  lcd.begin();
  lcd.backlight(); // Turn on the backlight
  lcd.setCursor(0, 0);
  lcd.print("Parking Slots:");
  lcd.setCursor(0, 1);
  lcd.print("[ ] [ ]"); // Initial empty parking spots

  Serial.println("Setup complete!");
}

void loop() {
  // Read the IR sensors
  bool ir1Detected = digitalRead(ir1Pin) == LOW; // LOW means obstacle detected
  bool ir2Detected = digitalRead(ir2Pin) == LOW;
  bool ir3Detected = digitalRead(ir3Pin) == LOW;

  // Handle IR1 detection
  if (ir1Detected) {
    Serial.println("IR1: Obstacle detected! Setting pin 16 HIGH.");
    digitalWrite(outPin16, HIGH); // Turn on LED 1
    lcd.setCursor(1, 1);         // Position for parking spot 1
    lcd.print("X");
  } else {
    Serial.println("IR1: No obstacle. Setting pin 16 LOW.");
    digitalWrite(outPin16, LOW); // Turn off LED 1
    lcd.setCursor(1, 1);         // Position for parking spot 1
    lcd.print(" ");
  }

  // Handle IR2 detection
  if (ir2Detected) {
    Serial.println("IR2: Obstacle detected! Setting pin 17 HIGH.");
    digitalWrite(outPin17, HIGH); // Turn on LED 2
    lcd.setCursor(5, 1);         // Position for parking spot 2
    lcd.print("X");
  } else {
    Serial.println("IR2: No obstacle. Setting pin 17 LOW.");
    digitalWrite(outPin17, LOW); // Turn off LED 2
    lcd.setCursor(5, 1);         // Position for parking spot 2
    lcd.print(" ");
  }

  // Handle IR3 detection and servo control
  if (ir3Detected) {
    Serial.println("IR3: Obstacle detected! Setting pin 5 HIGH and rotating servos to 180°.");
    digitalWrite(outPin5, HIGH);   // Turn on LED 3
    myServo.write(180);            // Rotate servo to 180°
    myServo1.write(180);           // Rotate servo to 180°
  } else {
    Serial.println("IR3: No obstacle. Setting pin 5 LOW and returning servos to 0°.");
    digitalWrite(outPin5, LOW);    // Turn off LED 3
    myServo.write(0);              // Return servos to 0°
    myServo1.write(0);             // Return servos to 0°
  }

  delay(500); // Small delay for stability
}
