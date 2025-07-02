#define TRIG_PIN 9
#define ECHO_PIN 10

void setup() {
  Serial.begin(9600);        // Start serial communication
  pinMode(TRIG_PIN, OUTPUT); // Trig as output
  pinMode(ECHO_PIN, INPUT);  // Echo as input
}

void loop() {
  long duration;
  float distance_cm;

  // Clear trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send 10us pulse to trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the echo duration (in microseconds)
  duration = pulseIn(ECHO_PIN, HIGH);

  // Convert duration to distance
  distance_cm = duration * 0.0343 / 2;

  // Print result to serial monitor
  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");

  delay(500); // Wait before next reading
}

