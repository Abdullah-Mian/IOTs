#include <RH_ASK.h>
#include <SPI.h>

RH_ASK rf_driver;

// Motor control pins
#define IN1 3  // Motor A
#define IN2 4
#define IN3 5  // Motor B
#define IN4 6
#define IN5 7

void setup() {
  rf_driver.init();
  Serial.begin(9600);

  // Set all motor control pins as output
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT); 
  pinMode(IN5, OUTPUT); 

  // Stop motors initially
  stopMotors();

  Serial.println("Receiver ready, waiting for direction commands...");
}

void loop() {
  uint8_t buf[2];
  uint8_t buflen = sizeof(buf);

  if (rf_driver.recv(buf, &buflen)) {
    buf[buflen] = '\0'; // Null terminate
    char command = buf[0];

    switch (command) {
      case 'W': // Forward
        Serial.println("Received 'W' - Moving Forward");
        moveForward();
        break;
      case 'B': // Backward
        Serial.println("Received 'B' - Moving Backward");
        moveBackward();
        break;
      case 'L': // Left
        Serial.println("Received 'L' - Turning Left");
        turnLeft();
        break;
      case 'R': // Right
        Serial.println("Received 'R' - Turning Right");
        turnRight();
        break;
      default:
        Serial.print("Received unknown: ");
        Serial.println((char*)buf);
        stopMotors();
    }
  }

  delay(50); // Small delay to stabilize reception
}

// Helper functions for motor control

void moveForward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(IN5, LOW);
}

void moveBackward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, HIGH);
  digitalWrite(IN5, LOW);
}

void turnLeft() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(IN5, LOW);
}

void turnRight() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  digitalWrite(IN5, LOW);
}

void stopMotors() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, HIGH);
  digitalWrite(IN5, HIGH);
}
