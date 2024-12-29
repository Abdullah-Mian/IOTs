#include <ESP32Servo.h>
#include <WebSocketsClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>

  // LCD setup
  LiquidCrystal_I2C lcd(0x27, 16, 2);

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

  volatile bool isOpen = false; // System starts armed

  // Wi-Fi credentials
  const char *ssid = "Redmi 9T";     // Replace with your hotspot SSID
  const char *password = "68986898"; // Replace with your hotspot password

  // Server details
  const char *host = "192.168.18.39"; // Replace with the IP shown in the app
  const int port = 5000;

  WiFiClient client;
  WebSocketsClient webSocket;

  void setup()
  {
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
    myServo.write(0);  // Start servo at 0°
    myServo1.write(0); // Start servo at 0°

    // Initialize the LCD with error checking
    Wire.begin();
    lcd.begin();
    lcd.backlight(); // Turn on the backlight
    lcd.setCursor(0, 0);
    lcd.print("System Armed!");

    Serial.println("Setup complete!");

    // Clear any existing WiFi connections
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Connect as a WebSocket client to the phone's IP and port
    webSocket.begin(host, port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);      // Attempt reconnection every second
    webSocket.enableHeartbeat(15000, 3000, 2); // Enable heartbeat to maintain connection
    Serial.println("WebSocket client mode started.");
  }

  void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
  {
    switch (type)
    {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected!");
      // Just stop servos without detaching
      myServo.write(0);
      myServo1.write(0);
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected!");
      break;
    case WStype_TEXT:
      if (length == 1)
      { // We expect single character commands
        char command = payload[0];
        switch (command)
        {
        case 'X':
          isOpen = true;
          break;
        case 'Y':
          isOpen = false;
          break;
        default:
          break;
        }
      }
      break;
    default:
      break;
    }
  }

  void loop()
  {
    webSocket.loop();

    // Add reconnection logic
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.reconnect();
      delay(100);
    }

    bool ir1Detected = digitalRead(ir1Pin) == LOW; // LOW means obstacle detected
    bool ir2Detected = digitalRead(ir2Pin) == LOW;
    bool ir3Detected = digitalRead(ir3Pin) == LOW;

    // Handle IR1 detection
    if (ir1Detected)
    {
      Serial.println("IR1: Obstacle detected! Setting pin 16 HIGH.");
      digitalWrite(outPin16, HIGH); // Turn on LED 1
      lcd.setCursor(1, 1);          // Position for parking spot 1
      lcd.print("X");
    }
    else
    {
      Serial.println("IR1: No obstacle. Setting pin 16 LOW.");
      digitalWrite(outPin16, LOW); // Turn off LED 1
      lcd.setCursor(1, 1);         // Position for parking spot 1
      lcd.print(" ");
    }

    // Handle IR2 detection
    if (ir2Detected)
    {
      Serial.println("IR2: Obstacle detected! Setting pin 17 HIGH.");
      digitalWrite(outPin17, HIGH); // Turn on LED 2
      lcd.setCursor(5, 1);          // Position for parking spot 2
      lcd.print("X");
    }
    else
    {
      Serial.println("IR2: No obstacle. Setting pin 17 LOW.");
      digitalWrite(outPin17, LOW); // Turn off LED 2
      lcd.setCursor(5, 1);         // Position for parking spot 2
      lcd.print(" ");
    }

    // Handle IR3 detection and servo control
    if (ir3Detected)
    {
      Serial.println("IR3: Obstacle detected! Setting pin 5 HIGH and rotating servos to 180°.");
      digitalWrite(outPin5, HIGH); // Turn on LED 3
      myServo.write(180);          // Rotate servo to 180°
      myServo1.write(180);         // Rotate servo to 180°
    }
    else if (isOpen)
    {
      Serial.println("IR3: Obstacle detected! Setting pin 5 HIGH and rotating servos to 180°.");
      digitalWrite(outPin5, HIGH); // Turn on LED 3
      myServo.write(180);          // Rotate servo to 180°
      myServo1.write(180);         // Rotate servo to 180°
    }
    else
    {
      Serial.println("IR3: No obstacle. Setting pin 5 LOW and returning servos to 0°.");
      digitalWrite(outPin5, LOW); // Turn off LED 3
      myServo.write(0);           // Return servos to 0°
      myServo1.write(0);          // Return servos to 0°
    }

    delay(500); // Small delay for stability
  }
