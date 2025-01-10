#include <Arduino.h>
#include <stack>
// Motor control pins
const int IN1 = 18;
const int IN2 = 19;
const int IN3 = 21;
const int IN4 = 20;

// Ultrasonic sensor pins
const int trigLeft = 6;
const int echoLeft = 7;
const int trigCenter = 5;
const int echoCenter = 8;
const int trigRight = 3;
const int echoRight = 4;
const int TRIG_ForwardLeft = 9;
const int ECHO_ForwardLeft = 10;
const int TRIG_ForwardRight = 11;
const int ECHO_ForwardRight = 12;

// PWM configurations
const int pwmChannel1 = 0;   // PWM channel for IN1
const int pwmChannel2 = 1;   // PWM channel for IN2
const int pwmChannel3 = 2;   // PWM channel for IN3
const int pwmChannel4 = 3;   // PWM channel for IN4
const int pwmFreq = 5000;    // PWM frequency (Hz)
const int pwmResolution = 8; // PWM resolution (8-bit: 0-255)

// Shared variables
volatile int directionCommand = 0; // Command to control direction
SemaphoreHandle_t xSemaphore;      // Semaphore for thread safety
int lastCommand = -99;             // Tracks the last executed command
std::stack<int> pathHistory;       // Stack to remember the path taken

// Function to set motor speed
void setMotorSpeed(int channel, int dutyCycle)
{
  ledcWrite(channel, dutyCycle); // Set PWM duty cycle (0-255)
}

// Function to read ultrasonic distance
long readUltrasonicDistance(int trigPin, int echoPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout to avoid hanging
  if (duration == 0)
  {
    return 400; // Return a large distance if no echo
  }
  return (duration / 2) * 0.0343; // Distance in cm
}

// Motor control functions
void forward()
{
  setMotorSpeed(pwmChannel1, 0);   // IN1 OFF
  setMotorSpeed(pwmChannel2, 255); // IN2 Half speed
  setMotorSpeed(pwmChannel3, 0);   // IN3 OFF
  setMotorSpeed(pwmChannel4, 255); // IN4 Half speed
}

void reverse()
{
  setMotorSpeed(pwmChannel1, 255); // IN1 Half speed
  setMotorSpeed(pwmChannel2, 0);   // IN2 OFF
  setMotorSpeed(pwmChannel3, 255); // IN3 Half speed
  setMotorSpeed(pwmChannel4, 0);   // IN4 OFF
}

void right()
{
  setMotorSpeed(pwmChannel1, 0);   // IN1 OFF
  setMotorSpeed(pwmChannel2, 255); // IN2 Half speed
  setMotorSpeed(pwmChannel3, 255); // IN3 Half speed
  setMotorSpeed(pwmChannel4, 0);   // IN4 OFF
}

void left()
{
  setMotorSpeed(pwmChannel1, 255); // IN1 Half speed
  setMotorSpeed(pwmChannel2, 0);   // IN2 OFF
  setMotorSpeed(pwmChannel3, 0);   // IN3 OFF
  setMotorSpeed(pwmChannel4, 255); // IN4 Half speed
}

void stopMotors()
{
  setMotorSpeed(pwmChannel1, 0); // Stop all motors
  setMotorSpeed(pwmChannel2, 0);
  setMotorSpeed(pwmChannel3, 0);
  setMotorSpeed(pwmChannel4, 0);
}

// Sensor task
void sensorTask(void *parameter)
{
  while (true)
  {
    long distanceLeft = readUltrasonicDistance(trigLeft, echoLeft);
    long distanceCenter = readUltrasonicDistance(trigCenter, echoCenter);
    long distanceRight = readUltrasonicDistance(trigRight, echoRight);
    long distanceForwardLeft = readUltrasonicDistance(TRIG_ForwardLeft, ECHO_ForwardLeft);
    long distanceForwardRight = readUltrasonicDistance(TRIG_ForwardRight, ECHO_ForwardRight);

    Serial.print("Left: ");
    Serial.print(distanceLeft);
    Serial.print(" cm | Center: ");
    Serial.print(distanceCenter);
    Serial.print(" cm | Right: ");
    Serial.println(distanceRight);
    Serial.print("Forward_Left: ");
    Serial.print(distanceForwardLeft);
    Serial.print(" cm | Forward_Right: ");
    Serial.println(distanceForwardRight);

    int command = 3; // Default to forward

    // Ultrasonic sensors data to avoid obstacles
    if (distanceForwardLeft < 20 && distanceForwardRight < 20)
    {
      command = -1; // Reverse
    }
    else if (distanceForwardLeft < 20)
    {
      command = 5; // Go right
    }
    else if (distanceForwardRight < 20)
    {
      command = 6; // Go left
    }

    // Ultrasonic sensors data to determine node in the maze

    if (distanceCenter < 20 && distanceRight < 20 && distanceLeft < 20)
    {
      command = -1; // Reverse
    }
    else if (distanceCenter < 20 && distanceRight < 20)
    {
      command = 1; // Go left
    }
    else if (distanceCenter < 20 && distanceLeft < 20)
    {
      command = 2; // Go right
    }
    else if (distanceLeft < 20 && distanceRight < 20)
    {
      command = 3; // Go forward
    }
    else if (distanceCenter < 20)
    { // Only forward obstacle
      if (distanceLeft >= 1000)
      {
        command = 1; // Go left
      }
      else if (distanceRight >= 1000)
      {
        command = 2; // Go right
      }
      else
      {
        command = -1; // Reverse
      }
    }
    else if (distanceLeft < 20)
    { // Only left obstacle
      if (distanceCenter >= 1000)
      {
        command = 3; // Go forward
      }
      else if (distanceRight >= 1000)
      {
        command = 2; // Go right
      }
      else
      {
        command = -1; // Reverse
      }
    }
    else if (distanceRight < 20)
    { // Only right obstacle
      if (distanceCenter >= 1000)
      {
        command = 3; // Go forward
      }
      else if (distanceLeft >= 1000)
      {
        command = 1; // Go left
      }
      else
      {
        command = -1; // Reverse
      }
    }
    else
    {
      command = 3; // Move forward (default)
    }

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
    {
      directionCommand = command;
      xSemaphoreGive(xSemaphore);
    }

    delay(100);
  }
}

// Movement task
void movementTask(void *parameter)
{
  while (true)
  {
    int command;

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
    {
      command = directionCommand;
      xSemaphoreGive(xSemaphore);
    }

    if (command != lastCommand)
    {
      stopMotors();
      lastCommand = command;

      switch (command)
      {
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
      case 3:
        forward();
        pathHistory.push(3);
        break;
      case 5:
        right();
        // not pushing to stack as it is just to avoid collision with wall or any obstacle
        break;
      case 6:
        left();
        // not pushing to stack as it is just to avoid collision with wall or any obstacle
        break;
      default:
        forward();
        pathHistory.push(3);

        break;
      }
    }

    delay(50);
  }
}

void setup()
{
  Serial.begin(115200);

  // Initialize motor PWM channels
  ledcSetup(pwmChannel1, pwmFreq, pwmResolution);
  ledcAttachPin(IN1, pwmChannel1);
  ledcSetup(pwmChannel2, pwmFreq, pwmResolution);
  ledcAttachPin(IN2, pwmChannel2);
  ledcSetup(pwmChannel3, pwmFreq, pwmResolution);
  ledcAttachPin(IN3, pwmChannel3);
  ledcSetup(pwmChannel4, pwmFreq, pwmResolution);
  ledcAttachPin(IN4, pwmChannel4);

  // Initialize ultrasonic sensor pins
  pinMode(trigLeft, OUTPUT);
  pinMode(echoLeft, INPUT);
  pinMode(trigCenter, OUTPUT);
  pinMode(echoCenter, INPUT);
  pinMode(trigRight, OUTPUT);
  pinMode(echoRight, INPUT);
  pinMode(TRIG_ForwardLeft, OUTPUT);
  pinMode(ECHO_ForwardLeft, INPUT);
  pinMode(TRIG_ForwardRight, OUTPUT);
  pinMode(ECHO_ForwardRight, INPUT);

  // Create semaphore
  xSemaphore = xSemaphoreCreateMutex();

  // Create tasks
  xTaskCreatePinnedToCore(sensorTask, "Sensor Task", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(movementTask, "Movement Task", 2048, NULL, 1, NULL, 1);

  Serial.println("System Initialized");
}

void loop()
{
  // Main loop intentionally left empty as tasks handle the logic
}
