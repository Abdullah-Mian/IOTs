#include <Wire.h>
#include <BluetoothSerial.h>

#define ADXL345_ADDRESS 0x53 // I2C address of ADXL345
#define SHOCK_THRESHOLD 0.95  // Adjust this value for sensitivity (in g)
#define FILTER_SAMPLES 10    // Number of samples for averaging

BluetoothSerial SerialBT;

// Define the data structure
struct AccelerationData {
  float averageX;
  float averageY;
  float averageZ;
  float crashDetected;
  float touch;
};

AccelerationData accelerationData;

bool deviceConnected = false;

void setup() {
  Serial.begin(115200); // Initialize serial monitor
  SerialBT.begin("ESP32_BT"); // Set Bluetooth device name
  Serial.println("Bluetooth Started! Ready to pair and connect...");

  Wire.begin(); // Initialize I2C communication
  writeRegister(0x2D, 0x08); // Set ADXL345 to measure mode

  // Initialize acceleration data
  accelerationData.averageX = 0;
  accelerationData.averageY = 0;
  accelerationData.averageZ = 0;
  accelerationData.crashDetected = 0; // 0 = no crash, 1 = crash detected
  accelerationData.touch = 0; // 0 = no touch, 1 = touch detected

  Serial.println("Helmet Shock Detector Ready!");
}

void loop() {
  // Check if a new device has connected
  if (!deviceConnected) {
    if (SerialBT.hasClient()) { // Check if a client (phone) has connected
      deviceConnected = true;
      Serial.println("Bluetooth Connected!");
    } else {
      Serial.println("Waiting for Bluetooth connection...");
    }
  }

  // Check if the device has disconnected
  if (deviceConnected && !SerialBT.hasClient()) {
    deviceConnected = false;
    Serial.println("Bluetooth Disconnected!");
  }

  // Read and filter acceleration data
  accelerationData.averageX = readFilteredAcceleration(0x32); // X-axis
  accelerationData.averageY = readFilteredAcceleration(0x34); // Y-axis
  accelerationData.averageZ = readFilteredAcceleration(0x36); // Z-axis

  // Calculate total acceleration vector (in g)
  float totalAccel = sqrt(accelerationData.averageX * accelerationData.averageX +
                          accelerationData.averageY * accelerationData.averageY +
                          accelerationData.averageZ * accelerationData.averageZ);

  // Check for shock
  if (totalAccel > SHOCK_THRESHOLD) {
    accelerationData.crashDetected = 1; // Crash detected
    Serial.println("Shock Detected! Possible Accident!");
  } else {
    accelerationData.crashDetected = 0; // No crash
  }

  // Send data if connected
  if (deviceConnected) {
    // Convert the data to a human-readable string
    String dataString = "X: " + String(accelerationData.averageX, 2) + 
                        ", Y: " + String(accelerationData.averageY, 2) + 
                        ", Z: " + String(accelerationData.averageZ, 2) + 
                        ", Crash: " + String(accelerationData.crashDetected) + 
                        ", Touch: " + String(accelerationData.touch);

    // Send the string to the app
    SerialBT.println(dataString);
    Serial.println("Data sent to app: " + dataString);
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