#include <Arduino.h>
#include <stack>

// Motor control pins
const int IN1 = 10;
const int IN2 = 11;
const int IN3 = 12;
const int IN4 = 13;

// Ultrasonic sensor pins
const int trigLeft = 16;
const int echoLeft = 15;
const int trigCenter = 9;
const int echoCenter = 48;
const int trigRight = 8;
const int echoRight = 3;

// PWM configurations
const int pwmChannel1 = 0;
const int pwmChannel2 = 1;
const int pwmChannel3 = 2;
const int pwmChannel4 = 3;
const int pwmFreq = 5000;
const int pwmResolution = 8;

// Shared variables
volatile int directionCommand = 0;
SemaphoreHandle_t xSemaphore;
int lastCommand = -99;
std::stack<int> pathHistory;  // Stack to remember the path taken

// Function to set motor speed
void setMotorSpeed(int channel, int dutyCycle) {
  ledcWrite(channel, dutyCycle);
}

// Function to read ultrasonic distance
long readUltrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) {
    return 400;
  }
  return (duration / 2) * 0.0343;
}

// Motor control functions
void forward() {
  setMotorSpeed(pwmChannel1, 0);
  setMotorSpeed(pwmChannel2, 128);
  setMotorSpeed(pwmChannel3, 128);
  setMotorSpeed(pwmChannel4, 0);
}

void reverse() {
  setMotorSpeed(pwmChannel1, 128);
  setMotorSpeed(pwmChannel2, 0);
  setMotorSpeed(pwmChannel3, 0);
  setMotorSpeed(pwmChannel4, 128);
}

void left() {
  setMotorSpeed(pwmChannel1, 0);
  setMotorSpeed(pwmChannel2, 128);
  setMotorSpeed(pwmChannel3, 0);
  setMotorSpeed(pwmChannel4, 128);
}

void right() {
  setMotorSpeed(pwmChannel1, 128);
  setMotorSpeed(pwmChannel2, 0);
  setMotorSpeed(pwmChannel3, 128);
  setMotorSpeed(pwmChannel4, 0);
}

void stopMotors() {
  setMotorSpeed(pwmChannel1, 0);
  setMotorSpeed(pwmChannel2, 0);
  setMotorSpeed(pwmChannel3, 0);
  setMotorSpeed(pwmChannel4, 0);
}

// Sensor task
void sensorTask(void *parameter) {
  while (true) {
    long distanceLeft = readUltrasonicDistance(trigLeft, echoLeft);
    long distanceCenter = readUltrasonicDistance(trigCenter, echoCenter);
    long distanceRight = readUltrasonicDistance(trigRight, echoRight);

    Serial.print("Left: ");
    Serial.print(distanceLeft);
    Serial.print(" cm | Center: ");
    Serial.print(distanceCenter);
    Serial.print(" cm | Right: ");
    Serial.println(distanceRight);

    int command = 4;  // Default to forward

    if (distanceCenter < 20 && distanceRight < 20 && distanceLeft < 20) {
      command = -1;  // Reverse
    } else if (distanceCenter < 20 && distanceRight < 20) {
      command = 1;  // Go left
    } else if (distanceCenter < 20 && distanceLeft < 20) {
      command = 2;  // Go right
    } else if (distanceCenter < 20) {
      if (distanceLeft >= 1000) {
        command = 1;
      } else if (distanceRight >= 1000) {
        command = 2;
      } else {
        command = -1;
      }
    } else {
      command = 4;
    }

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      directionCommand = command;
      xSemaphoreGive(xSemaphore);
    }

    delay(100);
  }
}

// Movement task
void movementTask(void *parameter) {
  while (true) {
    int command;

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      command = directionCommand;
      xSemaphoreGive(xSemaphore);
    }

    if (command != lastCommand) {
      stopMotors();
      lastCommand = command;

      switch (command) {
        case -1:
          reverse();
          pathHistory.push(-1);
          break;
        case 1:
          left();
          pathHistory.push(1);
          break;
        case 2:
          right();
          pathHistory.push(2);
          break;
        case 4:
          forward();
          pathHistory.push(4);
          break;
        default:
          forward();
          pathHistory.push(4);
          break;
      }
    }

    delay(50);
  }
}

void setup() {
  Serial.begin(115200);

  ledcSetup(pwmChannel1, pwmFreq, pwmResolution);
  ledcAttachPin(IN1, pwmChannel1);
  ledcSetup(pwmChannel2, pwmFreq, pwmResolution);
  ledcAttachPin(IN2, pwmChannel2);
  ledcSetup(pwmChannel3, pwmFreq, pwmResolution);
  ledcAttachPin(IN3, pwmChannel3);
  ledcSetup(pwmChannel4, pwmFreq, pwmResolution);
  ledcAttachPin(IN4, pwmChannel4);

  pinMode(trigLeft, OUTPUT);
  pinMode(echoLeft, INPUT);
  pinMode(trigCenter, OUTPUT);
  pinMode(echoCenter, INPUT);
  pinMode(trigRight, OUTPUT);
  pinMode(echoRight, INPUT);

  xSemaphore = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(movementTask, "Movement Task", 2048, NULL, 1, NULL, 1);

  Serial.println("System Initialized");
}

void loop() {
  if (!pathHistory.empty()) {
    Serial.print("Path: ");
    Serial.println(pathHistory.top());
  }
  delay(1000);
}
