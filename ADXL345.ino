#include <Wire.h>

#define ADXL345_ADDRESS 0x53 // I2C address of ADXL345
#define SHOCK_THRESHOLD 0.95  // Adjust this value for sensitivity (in g)
#define FILTER_SAMPLES 10    // Number of samples for averaging

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Initialize ADXL345
  writeRegister(0x2D, 0x08); // Set measure mode

  Serial.println("Helmet Shock Detector Ready!");
}

void loop() {
  // Read and filter acceleration data
  float x = readFilteredAcceleration(0x32); // X-axis
  float y = readFilteredAcceleration(0x34); // Y-axis
  float z = readFilteredAcceleration(0x36); // Z-axis

  // Print X, Y, Z values
  Serial.print("X: "); Serial.print(x);
  Serial.print("  Y: "); Serial.print(y);
  Serial.print("  Z: "); Serial.println(z);

  // Calculate total acceleration vector (in g)
  float totalAccel = sqrt(x * x + y * y + z * z);
  Serial.print("  totalAccel: "); Serial.println(totalAccel);
  // Check for shock
  if (totalAccel > SHOCK_THRESHOLD) {
    Serial.println("Shock Detected! Possible Accident!");
  }

  delay(100); // Adjust delay as needed
}

// Function to write to a register
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADXL345_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Function to read acceleration data
int16_t readAcceleration(uint8_t reg) {
  Wire.beginTransmission(ADXL345_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345_ADDRESS, 2);
  return (Wire.read() | (Wire.read() << 8));
}

// Function to read and filter acceleration data
float readFilteredAcceleration(uint8_t reg) {
  static float filteredValue = 0;
  int16_t rawValue = readAcceleration(reg);

  // Convert raw value to g (assuming ±2g range)
  float gValue = rawValue * 0.0039; // 3.9 mg/LSB for ±2g range

  // Apply simple low-pass filter
  filteredValue = (filteredValue * (FILTER_SAMPLES - 1) + gValue) / FILTER_SAMPLES;

  return filteredValue;
}