/*
  ESP32-S3 SIP Client for Arduino IDE
  Compatible with MicroSIP server
  
  Required Libraries (install via Library Manager):
  - WiFi (built-in with ESP32)
  - WiFiUdp (built-in with ESP32)
  
  No external libraries needed!
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>

// WiFi credentials
const char* ssid = "HUAWEI6955-9654MWZ";
const char* password = "dU9151TdJ4548B";

// SIP server configuration (from your MicroSIP image)
const char* SIP_SERVER_IP = "88.99.150.54";
const int SIP_SERVER_PORT = 5060;
const char* SIP_USER = "9102";
const char* SIP_DOMAIN = "88.99.150.54";
const char* SIP_PASSWORD = "8c5dcdb911af9502bd7ff087fe87f291"; // Your actual password

// SIP client variables
WiFiUDP udp;
String callID;
String localTag;
String branch;
int cseq = 1;
bool registered = false;
String localIP;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  delay(1000);
  
  Serial.println("ESP32-S3 SIP Client Starting...");
  
  // Generate random identifiers
  generateRandomIDs();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Start UDP
  udp.begin(5060);
  
  // Register with SIP server
  sipRegister();
  
  Serial.println("SIP Client ready!");
  Serial.println("Commands:");
  Serial.println("  'call' - Make a call to echo test (*43)");
  Serial.println("  'hangup' - End current call");
  Serial.println("  'register' - Re-register with server");
  Serial.println("  'status' - Show registration status");
}

void loop() {
  // Handle incoming SIP messages
  handleIncomingSIP();
  
  // Handle serial commands
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    command.toLowerCase();
    
    if (command == "call") {
      makeCall("*43"); // Echo test
    } else if (command == "hangup") {
      hangupCall();
    } else if (command == "register") {
      sipRegister();
    } else if (command == "status") {
      Serial.println("Registration status: " + String(registered ? "Registered" : "Not registered"));
      Serial.println("Local IP: " + localIP);
    } else {
      Serial.println("Unknown command: " + command);
    }
  }
  
  delay(100);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  localIP = WiFi.localIP().toString();
  Serial.println("IP address: " + localIP);
}

void generateRandomIDs() {
  // Generate random Call-ID
  callID = String(esp_random()) + "@" + "esp32s3";
  
  // Generate random tag
  localTag = String(esp_random(), HEX);
  
  // Generate random branch
  branch = "z9hG4bK" + String(esp_random(), HEX);
}

void sipRegister() {
  Serial.println("Registering with SIP server...");
  
  String registerMsg = "REGISTER sip:" + String(SIP_DOMAIN) + " SIP/2.0\r\n";
  registerMsg += "Via: SIP/2.0/UDP " + localIP + ":5060;branch=" + branch + "\r\n";
  registerMsg += "From: <sip:" + String(SIP_USER) + "@" + String(SIP_DOMAIN) + ">;tag=" + localTag + "\r\n";
  registerMsg += "To: <sip:" + String(SIP_USER) + "@" + String(SIP_DOMAIN) + ">\r\n";
  registerMsg += "Call-ID: " + callID + "\r\n";
  registerMsg += "CSeq: " + String(cseq++) + " REGISTER\r\n";
  registerMsg += "Contact: <sip:" + String(SIP_USER) + "@" + localIP + ":5060>\r\n";
  registerMsg += "Max-Forwards: 70\r\n";
  registerMsg += "User-Agent: ESP32-S3-SIP-Client\r\n";
  registerMsg += "Expires: 3600\r\n";
  registerMsg += "Content-Length: 0\r\n";
  registerMsg += "\r\n";
  
  sendSIPMessage(registerMsg);
}

void makeCall(String number) {
  if (!registered) {
    Serial.println("Not registered! Please register first.");
    return;
  }
  
  Serial.println("Making call to: " + number);
  
  // Generate new identifiers for the call
  generateRandomIDs();
  
  String inviteMsg = "INVITE sip:" + number + "@" + String(SIP_DOMAIN) + " SIP/2.0\r\n";
  inviteMsg += "Via: SIP/2.0/UDP " + localIP + ":5060;branch=" + branch + "\r\n";
  inviteMsg += "From: <sip:" + String(SIP_USER) + "@" + String(SIP_DOMAIN) + ">;tag=" + localTag + "\r\n";
  inviteMsg += "To: <sip:" + number + "@" + String(SIP_DOMAIN) + ">\r\n";
  inviteMsg += "Call-ID: " + callID + "\r\n";
  inviteMsg += "CSeq: " + String(cseq++) + " INVITE\r\n";
  inviteMsg += "Contact: <sip:" + String(SIP_USER) + "@" + localIP + ":5060>\r\n";
  inviteMsg += "Max-Forwards: 70\r\n";
  inviteMsg += "User-Agent: ESP32-S3-SIP-Client\r\n";
  inviteMsg += "Content-Type: application/sdp\r\n";
  
  // Simple SDP (Session Description Protocol)
  String sdp = "v=0\r\n";
  sdp += "o=" + String(SIP_USER) + " 0 0 IN IP4 " + localIP + "\r\n";
  sdp += "s=ESP32 SIP Call\r\n";
  sdp += "c=IN IP4 " + localIP + "\r\n";
  sdp += "t=0 0\r\n";
  sdp += "m=audio 4000 RTP/AVP 0\r\n";
  sdp += "a=rtpmap:0 PCMU/8000\r\n";
  
  inviteMsg += "Content-Length: " + String(sdp.length()) + "\r\n";
  inviteMsg += "\r\n";
  inviteMsg += sdp;
  
  sendSIPMessage(inviteMsg);
}

void hangupCall() {
  Serial.println("Sending BYE to hangup call...");
  
  String byeMsg = "BYE sip:*43@" + String(SIP_DOMAIN) + " SIP/2.0\r\n";
  byeMsg += "Via: SIP/2.0/UDP " + localIP + ":5060;branch=" + branch + "\r\n";
  byeMsg += "From: <sip:" + String(SIP_USER) + "@" + String(SIP_DOMAIN) + ">;tag=" + localTag + "\r\n";
  byeMsg += "To: <sip:*43@" + String(SIP_DOMAIN) + ">\r\n";
  byeMsg += "Call-ID: " + callID + "\r\n";
  byeMsg += "CSeq: " + String(cseq++) + " BYE\r\n";
  byeMsg += "Max-Forwards: 70\r\n";
  byeMsg += "User-Agent: ESP32-S3-SIP-Client\r\n";
  byeMsg += "Content-Length: 0\r\n";
  byeMsg += "\r\n";
  
  sendSIPMessage(byeMsg);
}

void sendSIPMessage(String message) {
  udp.beginPacket(SIP_SERVER_IP, SIP_SERVER_PORT);
  udp.print(message);
  udp.endPacket();
  
  Serial.println("=== SENT SIP MESSAGE ===");
  Serial.println(message);
  Serial.println("========================");
}

void handleIncomingSIP() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buffer[1024];
    int len = udp.read(buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    
    String response = String(buffer);
    Serial.println("=== RECEIVED SIP MESSAGE ===");
    Serial.println(response);
    Serial.println("============================");
    
    // Parse response
    if (response.startsWith("SIP/2.0 200 OK")) {
      if (response.indexOf("REGISTER") > 0) {
        Serial.println("✓ Successfully registered!");
        registered = true;
      } else if (response.indexOf("INVITE") > 0) {
        Serial.println("✓ Call accepted!");
        sendAck();
      }
    } else if (response.startsWith("SIP/2.0 401 Unauthorized")) {
      Serial.println("✗ Authentication required - implementing digest auth...");
      handleAuthentication(response);
    } else if (response.startsWith("SIP/2.0 407 Proxy Authentication Required")) {
      Serial.println("✗ Proxy authentication required");
      handleAuthentication(response);
    } else if (response.startsWith("INVITE")) {
      Serial.println("📞 Incoming call detected");
      // You could implement incoming call handling here
    }
  }
}

void sendAck() {
  String ackMsg = "ACK sip:*43@" + String(SIP_DOMAIN) + " SIP/2.0\r\n";
  ackMsg += "Via: SIP/2.0/UDP " + localIP + ":5060;branch=" + branch + "\r\n";
  ackMsg += "From: <sip:" + String(SIP_USER) + "@" + String(SIP_DOMAIN) + ">;tag=" + localTag + "\r\n";
  ackMsg += "To: <sip:*43@" + String(SIP_DOMAIN) + ">\r\n";
  ackMsg += "Call-ID: " + callID + "\r\n";
  ackMsg += "CSeq: " + String(cseq - 1) + " ACK\r\n";
  ackMsg += "Max-Forwards: 70\r\n";
  ackMsg += "Content-Length: 0\r\n";
  ackMsg += "\r\n";
  
  sendSIPMessage(ackMsg);
}

void handleAuthentication(String response) {
  // Basic authentication handling
  // For production, you'd need to implement full digest authentication
  Serial.println("⚠️  Authentication required but not fully implemented in this basic version");
  Serial.println("This basic SIP client doesn't support digest authentication yet.");
  Serial.println("Your server requires authentication with username/password.");
  
  // To implement digest auth, you'd need to:
  // 1. Parse WWW-Authenticate header
  // 2. Calculate MD5 hash response
  // 3. Send new request with Authorization header
}

void printWiFiStatus() {
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("IP: " + WiFi.localIP().toString());
  }
}