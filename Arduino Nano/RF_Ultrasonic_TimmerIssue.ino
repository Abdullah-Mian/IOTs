#include <RH_ASK.h>
#include <SPI.h>

#define TRIG_PIN 9
#define ECHO_PIN 10
#define BUZZER 3

// Reconfigure RH_ASK to avoid pin conflicts
// RH_ASK(speed, rxPin, txPin, pttPin)
RH_ASK rf_driver(2000, 4, 5, 6); // RX=4, TX=5, PTT=6

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  if (!rf_driver.init()) {
    Serial.println("RF driver init failed");
  } else {
    Serial.println("RF driver initialized successfully");
  }
}

void loop() {
  long duration;
  float distance_cm;
  
  // Trigger the sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo duration with timeout
  duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  
  if (duration == 0) {
    Serial.println("No echo received (timeout)");
    delay(1000);
    return;
  }
  
  distance_cm = duration * 0.0343 / 2.0;
  
  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");
  
  if (distance_cm < 10) {
    char message[] = "STOP";
    rf_driver.send((uint8_t *)message, strlen(message));
    rf_driver.waitPacketSent();
    Serial.print("Sent: ");
    Serial.println(message);
    digitalWrite(BUZZER,HIGH);
    delay(1000);
    digitalWrite(BUZZER,LOW);

  }
  
  delay(1000);
}