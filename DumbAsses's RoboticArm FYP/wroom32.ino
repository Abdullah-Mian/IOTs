#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "DESKTOP-9KJA4DL 2973";
const char* password = "2152s4#D";

// WebSocket server details (Python script)
const char* websocket_host = "192.168.18.103";  // Updated to hotspot gateway IP
const int websocket_port = 8765;

// Servo objects for 5-servo robotic arm
Servo servo1;  // Base rotation
Servo servo2;  // Shoulder
Servo servo3;  // Elbow
Servo servo4;  // Wrist
Servo servo5;  // Gripper

// Servo pin definitions
const int SERVO1_PIN = 13;  // Base
const int SERVO2_PIN = 12;  // Shoulder
const int SERVO3_PIN = 14;  // Elbow
const int SERVO4_PIN = 27;  // Wrist
const int SERVO5_PIN = 26;  // Gripper

// Servo calibration values based on your specifications
// Servo 1: 50=right, 70=mean, 120=left
const int SERVO1_RIGHT = 50;
const int SERVO1_MEAN = 70;
const int SERVO1_LEFT = 120;

// Servo 2: 70=up, 60=mean, 50=down - ADJUST MEAN TO BE 20 DEGREES LOWER
const int SERVO2_UP = 70;
const int SERVO2_MEAN = 50;  // Changed from 60 to 40 (20 degrees lower)
const int SERVO2_DOWN = 40;

// Servo 3: 150=up, 160=mean, 170=down
const int SERVO3_UP = 150;
const int SERVO3_MEAN = 160;
const int SERVO3_DOWN = 170;

// Servo 4: 160=right, 150=mean, 140=left
const int SERVO4_RIGHT = 160;
const int SERVO4_MEAN = 150;
const int SERVO4_LEFT = 140;

// Servo 5: 120=close, 80=open
const int SERVO5_CLOSE = 120;
const int SERVO5_OPEN = 80;

// Servo position variables - initialize to mean positions
int servo1_pos = SERVO1_MEAN;  // Base
int servo2_pos = SERVO2_MEAN;  // Shoulder
int servo3_pos = SERVO3_MEAN;  // Elbow
int servo4_pos = SERVO4_MEAN;  // Wrist
int servo5_pos = SERVO5_OPEN;  // Gripper (start open)

// Camera resolution for coordinate mapping
const int CAMERA_WIDTH = 320;
const int CAMERA_HEIGHT = 240;
const int TARGET_X = 80;   // Target pickup X position (80±7)
const int TARGET_Y = 227;  // Target pickup Y position (227±7)
const int TARGET_TOLERANCE = 7;  // ±7 pixel tolerance for both coordinates

// Robotic arm workspace limits (adjust based on your arm)
const int ARM_MIN_X = 0;
const int ARM_MAX_X = 180;
const int ARM_MIN_Y = 0;
const int ARM_MAX_Y = 180;

// WebSocket client
WebSocketsClient webSocket;

// Connection status
bool websocket_connected = false;
unsigned long last_reconnect_attempt = 0;
const unsigned long reconnect_interval = 5000; // 5 seconds
bool initial_connection_made = false;
int reconnect_attempts = 0;

// Movement parameters
const int MOVE_DELAY = 15;  // Delay between servo movements (ms)
const int CENTERING_STEP = 2;  // Reduced step size for finer control
const int ELBOW_STEP = 1;  // Smaller step for elbow fine adjustments

// Direction memory for lost target recovery
struct LastDirection {
  int base_dir;     // -1=left, 0=none, 1=right
  int shoulder_dir; // -1=down, 0=none, 1=up
  int elbow_dir;    // -1=up, 0=none, 1=down
  unsigned long last_move_time;
};

LastDirection last_direction = {0, 0, 0, 0};
// Remove timeout constants that cause waiting
const unsigned long COORDINATE_TIMEOUT = 500;  // Reduced from 2000ms to 500ms
const unsigned long DIRECTION_MEMORY_TIMEOUT = 1000; // Reduced from 3000ms to 1000ms
const unsigned long NO_DATA_TIMEOUT = 1000;  // Reduced from 3000ms to 1000ms

// Current target coordinates and state management
struct CottonTarget {
  int x;
  int y;
  int area;
  bool is_centered;
};

CottonTarget current_target;
bool has_target = false;
bool centering_in_progress = false;
unsigned long last_coordinate_update = 0;
unsigned long last_data_received = 0;
bool auto_home_triggered = false;

// System states
enum ArmState {
  IDLE,
  CENTERING_TARGET,
  PICKING_COTTON,
  DROPPING_COTTON,
  RETURNING_HOME
};

ArmState current_state = IDLE;

// Zone definitions based on provided coordinate data
struct CameraZone {
  int min_x, max_x, min_y, max_y;
  const char* name;
};

// Define camera zones based on your coordinate data
const CameraZone zones[] = {
  {0, 60, 0, 80, "TOP_LEFT"},        // top left(48,215) -> (11,67)
  {10, 50, 90, 130, "TOP_CENTER"},   // top center(30,112)
  {0, 30, 50, 90, "TOP_RIGHT"},      // top right(11,67)
  {40, 80, 190, 240, "LEFT_CENTER"}, // left center(170,214)
  {130, 180, 80, 130, "CENTER"},     // center (150-165,100-110)
  {140, 160, 40, 70, "RIGHT_CENTER"}, // right center (150,55)
  {250, 290, 200, 240, "BOTTOM_LEFT"}, // bottom left(268,222)
  {240, 280, 100, 140, "BOTTOM_CENTER"}, // bottom center(256,123)
  {260, 290, 30, 60, "BOTTOM_RIGHT"}  // bottom right(274,42)
};

const int NUM_ZONES = 9;

void setup() {
  Serial.begin(115200);
  Serial.println("🤖 ESP32 Robotic Arm Controller Starting...");
  
  // Initialize servos
  initializeServos();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize WebSocket connection
  initializeWebSocket();
  
  // Move to home position
  setHomePosition();
  
  Serial.println("✓ System ready!");
}

void initializeServos() {
  Serial.println("🔧 Initializing servos...");
  
  // Attach servos to pins
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  servo4.attach(SERVO4_PIN);
  servo5.attach(SERVO5_PIN);
  
  // Set all servos to mean positions
  servo1.write(servo1_pos);
  servo2.write(servo2_pos);
  servo3.write(servo3_pos);
  servo4.write(servo4_pos);
  servo5.write(servo5_pos);
  
  delay(1000);
  Serial.println("✓ Servos initialized to mean positions");
  Serial.printf("   Base: %d, Shoulder: %d, Elbow: %d, Wrist: %d, Gripper: %d\n",
                servo1_pos, servo2_pos, servo3_pos, servo4_pos, servo5_pos);
}

void connectToWiFi() {
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("✓ Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeWebSocket() {
  Serial.println("🌐 Initializing WebSocket connection...");
  
  // Keep trying to connect until successful
  while (!websocket_connected) {
    Serial.printf("🔄 Attempting WebSocket connection to %s:%d (Attempt %d)\n", 
                  websocket_host, websocket_port, ++reconnect_attempts);
    
    webSocket.begin(websocket_host, websocket_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(reconnect_interval);
    
    // Wait a bit and check connection status
    unsigned long connection_start = millis();
    while (!websocket_connected && (millis() - connection_start < 10000)) { // 10 second timeout
      webSocket.loop();
      delay(100);
    }
    
    if (!websocket_connected) {
      Serial.printf("✗ WebSocket connection failed. Retrying in %d seconds...\n", reconnect_interval/1000);
      delay(reconnect_interval);
    }
  }
  
  initial_connection_made = true;
  Serial.println("✓ WebSocket connected successfully!");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("✗ WebSocket Disconnected!");
      websocket_connected = false;
      // Reset state when disconnected
      current_state = IDLE;
      has_target = false;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("✓ WebSocket Connected to: %s\n", payload);
      websocket_connected = true;
      reconnect_attempts = 0;
      // Reset state on new connection
      current_state = IDLE;
      has_target = false;
      break;
      
    case WStype_TEXT:
      Serial.printf("📨 Received: %s\n", payload);
      handleCoordinateMessage((char*)payload);
      break;
      
    case WStype_ERROR:
      Serial.printf("✗ WebSocket Error: %s\n", payload);
      websocket_connected = false;
      break;
      
    default:
      break;
  }
}

void handleCoordinateMessage(const char* message) {
  // Parse JSON message
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("✗ JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Check if it's a cotton coordinates message
  if (doc["type"] == "cotton_coordinates") {
    JsonArray coordinates = doc["coordinates"];
    
    // Update last coordinate update time
    last_coordinate_update = millis();
    last_data_received = millis();
    auto_home_triggered = false;
    
    if (coordinates.size() > 0) {
      // Take the first detected cotton object
      JsonObject firstCotton = coordinates[0];
      
      current_target.x = firstCotton["x"];
      current_target.y = firstCotton["y"];
      current_target.area = firstCotton["area"];
      current_target.is_centered = isTargetAtPickupPosition(current_target.x, current_target.y);
      
      has_target = true;
      
      Serial.printf("🎯 Target at (%d, %d) - Pickup position: (%d, %d) - ", 
                    current_target.x, current_target.y, TARGET_X, TARGET_Y);
      
      if (current_target.is_centered) {
        Serial.println("AT PICKUP POSITION ✓");
        // Only start picking if we're in IDLE or CENTERING state (NOT after dropping)
        if (current_state == IDLE || current_state == CENTERING_TARGET) {
          Serial.println("🤏 Target is at pickup position, preparing for pickup...");
          
          // Keep shoulder at mean position (no pre-lowering)
          moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);
          servo2_pos = SERVO2_MEAN;
          
          // Elbow to mean position
          moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);
          servo3_pos = SERVO3_MEAN;
          
          // Remove delay - go directly to pickup
          current_state = PICKING_COTTON;
          has_target = false;
        } else {
          // If not in correct state, ignore this target to prevent multiple cycles
          Serial.println("⚠️ Ignoring target - arm is busy with current cycle");
          has_target = false;
        }
      } else {
        Serial.printf("NOT AT PICKUP POSITION (off by %d,%d)\n", 
                      current_target.x - TARGET_X, current_target.y - TARGET_Y);
        // Only start centering if we're in IDLE state
        if (current_state == IDLE) {
          Serial.println("🎯 Starting positioning process...");
          current_state = CENTERING_TARGET;
          // Reset direction memory when starting new centering
          last_direction = {0, 0, 0, millis()};
        } else {
          // If not in IDLE, ignore this target
          Serial.println("⚠️ Ignoring target - arm is busy");
          has_target = false;
        }
      }
    } else {
      // No targets detected
      if (has_target) {
        Serial.println("📭 No cotton detected - target lost");
        has_target = false;
        if (current_state == CENTERING_TARGET) {
          Serial.println("🔍 Lost target during positioning, attempting recovery...");
          // Try to recover using last known direction
          recoverLostTarget();
        }
      }
    }
  }
}

const char* getZoneName(int x, int y) {
  for (int i = 0; i < NUM_ZONES; i++) {
    if (x >= zones[i].min_x && x <= zones[i].max_x &&
        y >= zones[i].min_y && y <= zones[i].max_y) {
      return zones[i].name;
    }
  }
  return "UNKNOWN";
}

void moveToZone(const char* zone_name) {
  Serial.printf("🎯 Moving to zone: %s\n", zone_name);
  
  if (strcmp(zone_name, "TOP_LEFT") == 0) {
    // Move base left, shoulder up
    moveServoSmoothly(servo1, servo1_pos, SERVO1_LEFT);     // Base left (120)
    servo1_pos = SERVO1_LEFT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_UP);       // Shoulder up (70)
    servo2_pos = SERVO2_UP;
  }
  else if (strcmp(zone_name, "TOP_CENTER") == 0) {
    // Move base center, shoulder up
    moveServoSmoothly(servo1, servo1_pos, SERVO1_MEAN);     // Base center (70)
    servo1_pos = SERVO1_MEAN;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_UP);       // Shoulder up (70)
    servo2_pos = SERVO2_UP;
  }
  else if (strcmp(zone_name, "TOP_RIGHT") == 0) {
    // Move base right, shoulder up
    moveServoSmoothly(servo1, servo1_pos, SERVO1_RIGHT);    // Base right (50)
    servo1_pos = SERVO1_RIGHT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_UP);       // Shoulder up (70)
    servo2_pos = SERVO2_UP;
  }
  else if (strcmp(zone_name, "LEFT_CENTER") == 0) {
    // Move base left, shoulder center
    moveServoSmoothly(servo1, servo1_pos, SERVO1_LEFT);     // Base left (120)
    servo1_pos = SERVO1_LEFT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);     // Shoulder center (60)
    servo2_pos = SERVO2_MEAN;
  }
  else if (strcmp(zone_name, "CENTER") == 0) {
    // Move to center positions
    moveServoSmoothly(servo1, servo1_pos, SERVO1_MEAN);     // Base center (70)
    servo1_pos = SERVO1_MEAN;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);     // Shoulder center (60)
    servo2_pos = SERVO2_MEAN;
  }
  else if (strcmp(zone_name, "RIGHT_CENTER") == 0) {
    // Move base right, shoulder center
    moveServoSmoothly(servo1, servo1_pos, SERVO1_RIGHT);    // Base right (50)
    servo1_pos = SERVO1_RIGHT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);     // Shoulder center (60)
    servo2_pos = SERVO2_MEAN;
  }
  else if (strcmp(zone_name, "BOTTOM_LEFT") == 0) {
    // Move base left, shoulder down
    moveServoSmoothly(servo1, servo1_pos, SERVO1_LEFT);     // Base left (120)
    servo1_pos = SERVO1_LEFT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_DOWN);     // Shoulder down (50)
    servo2_pos = SERVO2_DOWN;
  }
  else if (strcmp(zone_name, "BOTTOM_CENTER") == 0) {
    // Move base center, shoulder down
    moveServoSmoothly(servo1, servo1_pos, SERVO1_MEAN);     // Base center (70)
    servo1_pos = SERVO1_MEAN;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_DOWN);     // Shoulder down (50)
    servo2_pos = SERVO2_DOWN;
  }
  else if (strcmp(zone_name, "BOTTOM_RIGHT") == 0) {
    // Move base right, shoulder down
    moveServoSmoothly(servo1, servo1_pos, SERVO1_RIGHT);    // Base right (50)
    servo1_pos = SERVO1_RIGHT;
    moveServoSmoothly(servo2, servo2_pos, SERVO2_DOWN);     // Shoulder down (50)
    servo2_pos = SERVO2_DOWN;
  }
  
  // Set elbow and wrist to ready position for all zones
  moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);       // Elbow ready (160)
  servo3_pos = SERVO3_MEAN;
  moveServoSmoothly(servo4, servo4_pos, SERVO4_MEAN);       // Wrist level (150)
  servo4_pos = SERVO4_MEAN;
  
  Serial.printf("✓ Moved to zone %s\n", zone_name);
}

void pickUpCotton() {
  Serial.println("🤏 Picking up cotton...");
  
  // Step 1: Lower shoulder 5 degrees below mean for pickup
  int pickup_shoulder_pos = SERVO2_MEAN - 5;  // 5 degrees lower (45)
  if (pickup_shoulder_pos < SERVO2_DOWN) pickup_shoulder_pos = SERVO2_DOWN;
  
  Serial.printf("🔧 Lowering shoulder 5° for pickup: %d -> %d\n", servo2_pos, pickup_shoulder_pos);
  moveServoSmoothly(servo2, servo2_pos, pickup_shoulder_pos);
  servo2_pos = pickup_shoulder_pos;
  
  // Step 2: Lower elbow for pickup reach
  moveServoSmoothly(servo3, servo3_pos, SERVO3_DOWN);  // Elbow down (170)
  servo3_pos = SERVO3_DOWN;
  
  // Step 3: Position wrist
  moveServoSmoothly(servo4, servo4_pos, SERVO4_MEAN);  // Wrist level (150)
  servo4_pos = SERVO4_MEAN;
  
  // Step 4: Open gripper
  moveServoSmoothly(servo5, servo5_pos, SERVO5_OPEN);  // Open (80)
  servo5_pos = SERVO5_OPEN;
  // Remove delay
  
  // Step 5: Close gripper to grab cotton
  moveServoSmoothly(servo5, servo5_pos, SERVO5_CLOSE);  // Close (120)
  servo5_pos = SERVO5_CLOSE;
  delay(500);  // Reduced from 1000ms to 500ms - minimal time to grab
  
  // Step 6: Lift elbow up
  moveServoSmoothly(servo3, servo3_pos, SERVO3_UP);  // Elbow up (150)
  servo3_pos = SERVO3_UP;
  
  // Step 7: Raise shoulder 5 degrees above mean before moving to drop
  int transport_shoulder_pos = SERVO2_MEAN + 5;  // 5 degrees higher (55)
  if (transport_shoulder_pos > SERVO2_UP) transport_shoulder_pos = SERVO2_UP;
  
  Serial.printf("🔧 Raising shoulder 5° for transport: %d -> %d\n", servo2_pos, transport_shoulder_pos);
  moveServoSmoothly(servo2, servo2_pos, transport_shoulder_pos);
  servo2_pos = transport_shoulder_pos;
  
  Serial.println("✓ Cotton picked up and ready for transport!");
  
  // Clear target data and flush coordinates
  has_target = false;
  current_target.x = 0;
  current_target.y = 0;
  current_target.area = 0;
  current_target.is_centered = false;
  last_direction = {0, 0, 0, 0};
  
  // Clear any pending coordinate data
  last_coordinate_update = 0;
  
  current_state = DROPPING_COTTON;
}

void dropCotton() {
  Serial.println("📦 Dropping cotton...");
  
  // Move to drop position - turn base to left side (opposite of pickup)
  moveServoSmoothly(servo1, servo1_pos, SERVO1_LEFT);   // Base to drop zone (120)
  servo1_pos = SERVO1_LEFT;
  
  // Lower shoulder to mean for drop
  moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);   // Shoulder position (50)
  servo2_pos = SERVO2_MEAN;
  
  // Extend elbow for drop
  moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);   // Elbow position (160)
  servo3_pos = SERVO3_MEAN;
  
  // Open gripper to release cotton
  moveServoSmoothly(servo5, servo5_pos, SERVO5_OPEN);  // Open (80)
  servo5_pos = SERVO5_OPEN;
  delay(300);  // Reduced from 1000ms to 300ms - minimal time to release
  
  Serial.println("✓ Cotton dropped!");
  
  // Complete coordinate flush and prepare for next cycle
  flushCoordinatesAndCheckNext();
}

void flushCoordinatesAndCheckNext() {
  Serial.println("🔄 Flushing coordinates and checking for more objects...");
  
  // Clear all target and coordinate data
  has_target = false;
  current_target.x = 0;
  current_target.y = 0;
  current_target.area = 0;
  current_target.is_centered = false;
  last_direction = {0, 0, 0, 0};
  last_coordinate_update = 0;
  
  // Reset state to check for new objects
  current_state = RETURNING_HOME;
  
  Serial.println("✓ Coordinates flushed - system ready for next object detection");
}

void setHomePosition() {
  Serial.println("🏠 Moving to home position...");
  
  // Set all servos to mean positions
  moveServoSmoothly(servo1, servo1_pos, SERVO1_MEAN);  // Base mean (70)
  servo1_pos = SERVO1_MEAN;
  
  moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);  // Shoulder mean (50)
  servo2_pos = SERVO2_MEAN;
  
  moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);  // Elbow mean (160)
  servo3_pos = SERVO3_MEAN;
  
  moveServoSmoothly(servo4, servo4_pos, SERVO4_MEAN);  // Wrist mean (150)
  servo4_pos = SERVO4_MEAN;
  
  moveServoSmoothly(servo5, servo5_pos, SERVO5_OPEN);  // Gripper open (80)
  servo5_pos = SERVO5_OPEN;
  
  // Clear all target and state data
  has_target = false;
  current_target.x = 0;
  current_target.y = 0;
  current_target.area = 0;
  current_target.is_centered = false;
  auto_home_triggered = false;
  
  // Reset to IDLE to check for new objects
  current_state = IDLE;
  Serial.println("✓ Home position reached - Ready for new targets");
  Serial.printf("   Positions: Base=%d, Shoulder=%d, Elbow=%d, Wrist=%d, Gripper=%d\n",
                servo1_pos, servo2_pos, servo3_pos, servo4_pos, servo5_pos);
  Serial.println("🔍 Scanning for new cotton objects...");
  
  // Add a delay to prevent immediate re-detection of the same object
  delay(2000);  // Wait 2 seconds before accepting new targets
}

void recoverLostTarget() {
  // Check if we have recent direction memory
  if (millis() - last_direction.last_move_time > DIRECTION_MEMORY_TIMEOUT) {
    Serial.println("🏠 Direction memory expired, returning to mean position");
    returnToMeanPosition();
    current_state = IDLE;
    return;
  }
  
  Serial.println("🔍 Attempting to recover lost target using last known direction...");
  
  bool recovery_attempted = false;
  
  // BASE recovery for Y-coordinate (corrected directions)
  if (last_direction.base_dir != 0) {
    int base_recovery = last_direction.base_dir * (CENTERING_STEP + 1);
    int new_base_pos = constrain(servo1_pos + base_recovery, SERVO1_RIGHT, SERVO1_LEFT);
    if (new_base_pos != servo1_pos) {
      moveServoSmoothly(servo1, servo1_pos, new_base_pos);
      servo1_pos = new_base_pos;
      Serial.printf("🔍 Recovery: Base %d -> %d [Y-CONTROL]\n", 
                    servo1_pos - base_recovery, new_base_pos);
      recovery_attempted = true;
    }
  }
  
  // ELBOW recovery for X-coordinate (corrected directions)
  if (last_direction.elbow_dir != 0) {
    int elbow_recovery = last_direction.elbow_dir * (ELBOW_STEP + 1);
    int new_elbow_pos = constrain(servo3_pos + elbow_recovery, SERVO3_UP, SERVO3_DOWN);
    if (new_elbow_pos != servo3_pos) {
      moveServoSmoothly(servo3, servo3_pos, new_elbow_pos);
      servo3_pos = new_elbow_pos;
      Serial.printf("🔍 Recovery: Elbow %d -> %d [X-CONTROL]\n", 
                    servo3_pos - elbow_recovery, new_elbow_pos);
      recovery_attempted = true;
    }
  }
  
  if (recovery_attempted) {
    Serial.println("🔍 Recovery movement completed, waiting for target to reappear...");
    delay(100);  // Reduced from 300ms to 100ms
  } else {
    Serial.println("🏠 No recovery movement possible, returning to mean position");
    returnToMeanPosition();
    current_state = IDLE;
  }
}

bool validatePickupPosition() {
  // Check if shoulder is at mean position (ready for pickup lowering)
  return (abs(servo2_pos - SERVO2_MEAN) <= 3); // Within 3 degrees tolerance
}

void loop() {
  // Handle WebSocket events
  webSocket.loop();
  
  // Check and maintain WebSocket connection
  checkWebSocketConnection();
  
  // Real-time cotton processing based on current state
  if (websocket_connected) {
    switch (current_state) {
      case IDLE:
        // Wait for new targets - coordinates are handled in handleCoordinateMessage
        break;
        
      case CENTERING_TARGET:
        // Only center if we have a target
        if (has_target) {
          centerTarget();
        } else {
          Serial.println("⚠️ No target during centering, returning to IDLE");
          current_state = IDLE;
          // Reset direction memory
          last_direction = {0, 0, 0, 0};
        }
        break;
        
      case PICKING_COTTON:
        // Execute pickup sequence only once
        pickUpCotton();
        break;
        
      case DROPPING_COTTON:
        // Execute drop sequence only once
        dropCotton();
        break;
        
      case RETURNING_HOME:
        // Return to home position only once
        setHomePosition();
        break;
    }
  }
  
  // Show status periodically when idle
  static unsigned long last_status_print = 0;
  if (current_state == IDLE && millis() - last_status_print > 5000) { // Increased to 5 seconds
    if (websocket_connected) {
      Serial.println("💤 Idle - Waiting for cotton coordinates...");
    } else {
      Serial.println("📡 Disconnected - Attempting to reconnect...");
    }
    last_status_print = millis();
  }
  
  // If completely disconnected and we had initial connection, try more aggressive reconnection
  if (!websocket_connected && initial_connection_made && 
      (millis() - last_reconnect_attempt > reconnect_interval)) {
    
    Serial.println("🔄 Attempting aggressive WebSocket reconnection...");
    
    // Try to reconnect more aggressively
    webSocket.disconnect();
    delay(1000);
    
    webSocket.begin(websocket_host, websocket_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(reconnect_interval);
    
    last_reconnect_attempt = millis();
  }
  
  // Increased delay to prevent overwhelming the system
  delay(100);  // Increased from 20ms to 100ms for better stability
}

bool isTargetAtPickupPosition(int x, int y) {
  int dx = abs(x - TARGET_X);
  int dy = abs(y - TARGET_Y);
  return (dx <= TARGET_TOLERANCE && dy <= TARGET_TOLERANCE);
}

void centerTarget() {
  if (!has_target) {
    Serial.println("⚠️ No target to position, attempting recovery or returning to mean position");
    recoverLostTarget();
    return;
  }
  
  int dx = current_target.x - TARGET_X;
  int dy = current_target.y - TARGET_Y;
  
  Serial.printf("🎯 Positioning: Target(%d,%d) PickupPos(%d,%d) Offset(%d,%d)\n", 
                current_target.x, current_target.y, TARGET_X, TARGET_Y, dx, dy);
  
  // Check if already at pickup position
  if (isTargetAtPickupPosition(current_target.x, current_target.y)) {
    Serial.println("✓ Target is now at pickup position!");
    Serial.println("🔧 Ready for pickup - keeping shoulder at mean...");
    
    // Keep shoulder at mean position
    moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);
    servo2_pos = SERVO2_MEAN;
    
    // Keep elbow at mean for pickup approach
    moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);
    servo3_pos = SERVO3_MEAN;
    
    // Remove delay - go directly to pickup
    current_state = PICKING_COTTON;
    has_target = false;
    return;
  }
  
  // CORRECTED MOVEMENT LOGIC BASED ON USER CLARIFICATION:
  // Base RIGHT -> object moves BOTTOM (Y increases)
  // Base LEFT -> object moves TOP (Y decreases)  
  // Elbow UP -> object moves RIGHT (X increases)
  // Elbow DOWN -> object moves LEFT (X decreases)
  
  int base_adjustment = 0;    // For Y-coordinate control
  int elbow_adjustment = 0;   // For X-coordinate control
  
  // VERTICAL POSITIONING - Use BASE servo
  if (abs(dy) > TARGET_TOLERANCE) {
    if (dy > 0) {
      // Target is BELOW pickup position (Y too high)
      // Need to move base LEFT to bring object UP on screen (decrease Y)
      base_adjustment = CENTERING_STEP;
      last_direction.base_dir = 1;
      Serial.printf("🔄 Y-Control: Target BELOW by %d pixels, moving BASE LEFT\n", dy);
    } else {
      // Target is ABOVE pickup position (Y too low)
      // Need to move base RIGHT to bring object DOWN on screen (increase Y)
      base_adjustment = -CENTERING_STEP;
      last_direction.base_dir = -1;
      Serial.printf("🔄 Y-Control: Target ABOVE by %d pixels, moving BASE RIGHT\n", abs(dy));
    }
  }
  
  // HORIZONTAL POSITIONING - Use ELBOW servo
  if (abs(dx) > TARGET_TOLERANCE) {
    if (dx > 0) {
      // Target is RIGHT of pickup position (X too high)
      // Need to move elbow DOWN to bring object LEFT on screen (decrease X)
      elbow_adjustment = ELBOW_STEP;
      last_direction.elbow_dir = 1;
      Serial.printf("🔄 X-Control: Target RIGHT by %d pixels, moving ELBOW DOWN\n", dx);
    } else {
      // Target is LEFT of pickup position (X too low)
      // Need to move elbow UP to bring object RIGHT on screen (increase X)
      elbow_adjustment = -ELBOW_STEP;
      last_direction.elbow_dir = -1;
      Serial.printf("🔄 X-Control: Target LEFT by %d pixels, moving ELBOW UP\n", abs(dx));
    }
  }
  
  // Update direction memory
  last_direction.last_move_time = millis();
  
  // EXECUTE MOVEMENTS
  bool moved = false;
  
  // 1. BASE movement for Y-coordinate control (vertical positioning)
  if (base_adjustment != 0) {
    int new_base_pos = constrain(servo1_pos + base_adjustment, SERVO1_RIGHT, SERVO1_LEFT);
    if (new_base_pos != servo1_pos) {
      servo1.write(new_base_pos);
      Serial.printf("🔄 Base: %d -> %d [Y-CONTROL]\n", servo1_pos, new_base_pos);
      servo1_pos = new_base_pos;
      moved = true;
    }
  }
  
  // 2. ELBOW movement for X-coordinate control (horizontal positioning)
  if (elbow_adjustment != 0) {
    int new_elbow_pos = constrain(servo3_pos + elbow_adjustment, SERVO3_UP, SERVO3_DOWN);
    if (new_elbow_pos != servo3_pos) {
      servo3.write(new_elbow_pos);
      Serial.printf("🔄 Elbow: %d -> %d [X-CONTROL]\n", servo3_pos, new_elbow_pos);
      servo3_pos = new_elbow_pos;
      moved = true;
    }
  }
  
  if (!moved) {
    Serial.println("⚠️ No servo movement possible, target may be at servo limits");
  }
  
  delay(100); // Reduced from 200ms to 100ms for faster response
}

void returnToMeanPosition() {
  Serial.println("🏠 Returning to mean position after lost target...");
  
  // Move all servos back to mean positions smoothly
  moveServoSmoothly(servo1, servo1_pos, SERVO1_MEAN);  // Base mean (70)
  servo1_pos = SERVO1_MEAN;
  
  moveServoSmoothly(servo2, servo2_pos, SERVO2_MEAN);  // Shoulder mean (50)
  servo2_pos = SERVO2_MEAN;
  
  moveServoSmoothly(servo3, servo3_pos, SERVO3_MEAN);  // Elbow mean (160)
  servo3_pos = SERVO3_MEAN;
  
  moveServoSmoothly(servo4, servo4_pos, SERVO4_MEAN);  // Wrist mean (150)
  servo4_pos = SERVO4_MEAN;
  
  Serial.println("✓ Returned to mean position");
}

void moveServoSmoothly(Servo& servo, int current_pos, int target_pos) {
  if (current_pos == target_pos) return;
  
  int step = (target_pos > current_pos) ? 1 : -1;
  
  for (int pos = current_pos; pos != target_pos; pos += step) {
    servo.write(pos);
    delay(MOVE_DELAY);
  }
  servo.write(target_pos);
  delay(50);  // Final position settle time
}

void checkWebSocketConnection() {
  // Only attempt reconnection if we've made initial connection and are now disconnected
  if (!websocket_connected && initial_connection_made && 
      (millis() - last_reconnect_attempt > reconnect_interval)) {
    
    Serial.printf("🔄 WebSocket disconnected. Attempting to reconnect... (Attempt %d)\n", 
                  ++reconnect_attempts);
    
    // Disconnect and reconnect
    webSocket.disconnect();
    delay(1000);  // Give it a moment
    
    webSocket.begin(websocket_host, websocket_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(reconnect_interval);
    
    last_reconnect_attempt = millis();
    
    Serial.println("⏳ Reconnection attempt initiated. Waiting for connection...");
  }
  
  // If we lose connection completely, keep trying more aggressively
  if (!websocket_connected && initial_connection_made) {
    // Show connection status periodically
    static unsigned long last_status_print = 0;
    if (millis() - last_status_print > 10000) {  // Print every 10 seconds
      Serial.printf("⚠️ WebSocket still disconnected. Attempts: %d\n", reconnect_attempts);
      last_status_print = millis();
    }
  }
}