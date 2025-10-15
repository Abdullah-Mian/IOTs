// ESP32 Line Follower - 4 PIN L298N (PWM on IN pins)
// 5 IR Sensors + FreeRTOS + PID Control

// ========== IR SENSOR PINS ==========
#define IR_1   34  // Leftmost
#define IR_2   35  // Left-Center
#define IR_3   32  // Center
#define IR_4   33  // Right-Center
#define IR_5   25  // Rightmost

// ========== L298N MOTOR PINS (4 PINS ONLY) ==========
#define MOTOR_L_IN1   27  // Left motor IN1 (PWM speed control)
#define MOTOR_L_IN2   26  // Left motor IN2 (PWM speed control)
#define MOTOR_R_IN1   18  // Right motor IN1 (PWM speed control)
#define MOTOR_R_IN2   23  // Right motor IN2 (PWM speed control)

// ========== PID GAINS ==========
float Kp = 2.0;
float Ki = 0.0;
float Kd = 1.5;

// ========== GLOBALS ==========
volatile int sensors[5];
volatile float linePos = 0.0;
int baseSpeed = 50;  // Very slow

float prevError = 0;
float integral = 0;
unsigned long prevTime = 0;

SemaphoreHandle_t mutex;

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  
  // Sensor pins
  pinMode(IR_1, INPUT);
  pinMode(IR_2, INPUT);
  pinMode(IR_3, INPUT);
  pinMode(IR_4, INPUT);
  pinMode(IR_5, INPUT);
  
  // Motor pins
  pinMode(MOTOR_L_IN1, OUTPUT);
  pinMode(MOTOR_L_IN2, OUTPUT);
  pinMode(MOTOR_R_IN1, OUTPUT);
  pinMode(MOTOR_R_IN2, OUTPUT);
  
  // Stop motors initially
  analogWrite(MOTOR_L_IN1, 0);
  analogWrite(MOTOR_L_IN2, 0);
  analogWrite(MOTOR_R_IN1, 0);
  analogWrite(MOTOR_R_IN2, 0);
  
  mutex = xSemaphoreCreateMutex();
  
  // Task 1: Read sensors (Core 0)
  xTaskCreatePinnedToCore(readSensors, "Sensors", 4096, NULL, 2, NULL, 0);
  
  // Task 2: Control motors (Core 1)
  xTaskCreatePinnedToCore(controlMotors, "Motors", 4096, NULL, 1, NULL, 1);
  
  Serial.println("Started!");
}

// ========== TASK 1: READ SENSORS ==========
void readSensors(void *param) {
  while (true) {
    int s1 = analogRead(IR_1);
    int s2 = analogRead(IR_2);
    int s3 = analogRead(IR_3);
    int s4 = analogRead(IR_4);
    int s5 = analogRead(IR_5);
    
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sensors[0] = s1;
      sensors[1] = s2;
      sensors[2] = s3;
      sensors[3] = s4;
      sensors[4] = s5;
      
      // Weighted position (LOW = black line)
      // -2 (left) to +2 (right)
      long sum = 0;
      long total = 0;
      
      sum += s1 * (-2);
      sum += s2 * (-1);
      sum += s3 * (0);
      sum += s4 * (1);
      sum += s5 * (2);
      
      total = s1 + s2 + s3 + s4 + s5;
      
      if (total > 0) {
        linePos = (float)sum / (float)total;
      }
      
      xSemaphoreGive(mutex);
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ========== TASK 2: PID MOTOR CONTROL ==========
void controlMotors(void *param) {
  while (true) {
    float error = 0.0;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      error = linePos;
      xSemaphoreGive(mutex);
    }
    
    unsigned long now = millis();
    float dt = (now - prevTime) / 1000.0;
    
    if (dt > 0) {
      // PID calculation
      float P = Kp * error;
      
      integral += error * dt;
      integral = constrain(integral, -10, 10);
      float I = Ki * integral;
      
      float D = Kd * (error - prevError) / dt;
      
      float correction = P + I + D;
      correction = constrain(correction, -255, 255);
      
      // Motor speeds
      int leftSpeed = baseSpeed + correction;
      int rightSpeed = baseSpeed - correction;
      
      leftSpeed = constrain(leftSpeed, -255, 255);
      rightSpeed = constrain(rightSpeed, -255, 255);
      
      // Drive motors
      driveMotors(leftSpeed, rightSpeed);
      
      prevError = error;
      prevTime = now;
      
      // Debug
      Serial.print("E:");
      Serial.print(error, 2);
      Serial.print(" L:");
      Serial.print(leftSpeed);
      Serial.print(" R:");
      Serial.println(rightSpeed);
    }
    
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ========== DRIVE MOTORS (4-PIN L298N) ==========
void driveMotors(int left, int right) {
  // LEFT MOTOR
  if (left > 0) {
    // Forward
    analogWrite(MOTOR_L_IN1, left);
    analogWrite(MOTOR_L_IN2, 0);
  } else if (left < 0) {
    // Reverse
    analogWrite(MOTOR_L_IN1, 0);
    analogWrite(MOTOR_L_IN2, -left);
  } else {
    // Stop
    analogWrite(MOTOR_L_IN1, 0);
    analogWrite(MOTOR_L_IN2, 0);
  }
  
  // RIGHT MOTOR
  if (right > 0) {
    // Forward
    analogWrite(MOTOR_R_IN1, right);
    analogWrite(MOTOR_R_IN2, 0);
  } else if (right < 0) {
    // Reverse
    analogWrite(MOTOR_R_IN1, 0);
    analogWrite(MOTOR_R_IN2, -right);
  } else {
    // Stop
    analogWrite(MOTOR_R_IN1, 0);
    analogWrite(MOTOR_R_IN2, 0);
  }
}

// ========== MAIN LOOP ==========
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}