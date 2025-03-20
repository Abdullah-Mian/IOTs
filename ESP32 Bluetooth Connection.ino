#include "BluetoothSerial.h"

// Motor control pins for L298N (same as before)
const int IN1 = 26; // Motor 1 forward
const int IN2 = 25; // Motor 1 backward
const int IN3 = 33; // Motor 2 forward
const int IN4 = 32; // Motor 2 backward

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  // Initialize motor pins as outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Set all motor pins to LOW by default
  stopMotors();
  Serial.println("Motors initialized. All pins set to LOW.");

  // Initialize Bluetooth
  SerialBT.begin("ESP32-RC-Car"); // Bluetooth device name
  Serial.println("Bluetooth Started! Ready to pair...");
}

void loop() {
  // Check for incoming Bluetooth data
  if (SerialBT.available()) {
    char command = SerialBT.read();
    Serial.write(command); // Echo command to serial monitor
    
    switch(command) {
      case 'W':
        forward();
        Serial.print(command);
        break;
      case 'B':
        backward();
        Serial.print(command);
        break;
      case 'S':
        stopMotors();
        Serial.print(command);
        break;
      case 'A':
        Serial.print(command);
        left();
        break;
      case 'D':
        right();
        Serial.print(command);
        break;
      case 'X':
        forward();
        Serial.print(command);
        break;
      case 'Y':
        backward();
        Serial.print(command);
        break;
    }
  }
  
  delay(20); // Small delay to prevent overloading
}

// Motor control functions (remain exactly the same)
void forward() {
  digitalWrite(IN1, HIGH);  
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW);  
  Serial.println("Moving Forward");
}

void backward() {
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH);  
  digitalWrite(IN3, LOW);  
  digitalWrite(IN4, HIGH); 
  Serial.println("Moving Backward");
}

void left() {
  digitalWrite(IN1, HIGH);  
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW);  
  digitalWrite(IN4, HIGH); 
  Serial.println("Turning Left");
}

void right() {
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, HIGH);  
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW);  
  Serial.println("Turning Right");
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  Serial.println("Motors Stopped");
}