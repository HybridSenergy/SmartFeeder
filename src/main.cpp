/*
 * ESP32 Smart Feeder
 * IoT-enabled smart pet feeder with stepper motor control, 
 * load cell weight measurement, and web server for remote control
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <HX711.h>

// WiFi Configuration
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// DEBUG: Set to true to skip WiFi (for testing in Wokwi)
#define SKIP_WIFI false  // Set to true to disable WiFi completely

// Pin Definitions
#define STEP_PIN 2      // A4988 STEP pin
#define DIR_PIN 4       // A4988 DIR pin
#define ENABLE_PIN 5    // A4988 ENABLE pin
#define DT_PIN 18       // HX711 DT pin
#define SCK_PIN 19      // HX711 SCK pin
#define IR_SENSOR_PIN 21 // IR Sensor OUT pin

// Stepper Motor Configuration
#define MOTOR_INTERFACE_TYPE 1  // Driver interface
#define STEPS_PER_REVOLUTION 200
#define MAX_SPEED 1000.0
#define ACCELERATION 500.0
#define DISPENSE_STEPS 400  // Adjust based on desired food amount

// Load Cell Configuration
float calibration_factor = -7050.0;  // Adjust based on your load cell
HX711 scale;

// Stepper Motor Object
AccelStepper stepper(MOTOR_INTERFACE_TYPE, STEP_PIN, DIR_PIN);

// Web Server
WebServer server(80);

// Timing Variables
unsigned long lastWeightDisplay = 0;
const unsigned long weightDisplayInterval = 5000;  // 5 seconds for testing (change to 30000 for production)

// Function Prototypes
void setupWiFi();
void handleRoot();
void handleDispense();
void handleWeight();
void handleNotFound();
void dispenseFood();
float getWeight();

void setup() {
  // CRITICAL: Start Serial FIRST - exactly like the working example
  Serial.begin(115200);
  
  // CRITICAL: Print immediately - SIMPLE, like working example
  Serial.println("Initializing WiFi...");
  WiFi.mode(WIFI_STA);
  Serial.println("Setup done!");
  
  // Now continue with Smart Feeder initialization
  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP32 Smart Feeder - Starting...");
  Serial.println("========================================");
  delay(200);
  
  // Now do WiFi setup (scan and connect)
  Serial.println("Setting up WiFi connection...");
  delay(100);
  
  #if SKIP_WIFI
    Serial.println("WiFi SKIPPED (for testing)");
  #else
    setupWiFi();
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
  #endif
  delay(100);
  
  // NOW initialize hardware (after WiFi/Serial is working)
  Serial.println("Initializing hardware...");
  delay(100);
  
  // Initialize Stepper Motor
  Serial.println("  - Stepper motor...");
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);  // Disable motor initially
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  Serial.println("    ✓ Done");
  delay(50);
  
  // Initialize IR Sensor
  Serial.println("  - IR sensor...");
  pinMode(IR_SENSOR_PIN, INPUT);
  int irInit = digitalRead(IR_SENSOR_PIN);
  Serial.print("    ✓ Done (status: ");
  Serial.print(irInit == LOW ? "OBSTRUCTION" : "CLEAR");
  Serial.println(")");
  delay(50);
  
  // Initialize Load Cell
  Serial.println("  - Load cell (HX711)...");
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor);
  delay(100);
  if (scale.is_ready()) {
    scale.tare();
    Serial.println("    ✓ Done (HX711 ready)");
  } else {
    Serial.println("    ⚠ HX711 not detected (simulation mode)");
    scale.tare();
  }
  delay(50);
  
  // Setup Web Server
  Serial.println("Setting up web server...");
  server.on("/", handleRoot);
  server.on("/dispense", handleDispense);
  server.on("/weight", handleWeight);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("  ✓ Web server started!");
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("🌐 WEB SERVER ACCESS");
  Serial.println("========================================");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✅ WiFi CONNECTED!");
    Serial.print("📍 Access your Smart Feeder at:");
    Serial.println();
    Serial.print("   👉 http://");
    Serial.println(WiFi.localIP());
    Serial.println();
    Serial.println("   Open this URL in your browser to control the feeder");
  } else {
    Serial.println();
    Serial.println("⚠️  WiFi not connected");
    Serial.println("   Web server is running but may not be accessible");
    Serial.println("   (This is normal in Wokwi simulation)");
  }
  
  Serial.println("========================================");
  Serial.println();
  Serial.println("Setup complete! Entering main loop...");
  Serial.println();
}

void loop() {
  // Continuous output to verify loop is running (like the working example)
  static unsigned long lastStatus = 0;
  unsigned long now = millis();
  
  // Print status every 5 seconds
  if (now - lastStatus >= 5000) {
    Serial.println("Status update:");
    Serial.print("  Weight: ");
    float weight = getWeight();
    Serial.print(weight, 2);
    Serial.print(" g | IR: ");
    int irStatus = digitalRead(IR_SENSOR_PIN);
    Serial.println(irStatus == LOW ? "OBSTRUCTION" : "CLEAR");
    lastStatus = now;
  }
  
  // Handle web server
  server.handleClient();
  
  // Run stepper motor if needed
  stepper.run();
  
  delay(10);
}

void setupWiFi() {
  Serial.println("[DEBUG] ===== setupWiFi() STARTED =====");
  Serial.print("[DEBUG] Target SSID: ");
  Serial.println(ssid);
  Serial.print("[DEBUG] Password: ");
  Serial.println(strlen(password) > 0 ? "***" : "(empty)");
  Serial.flush();
  delay(100);
  
  // Step 1: Set WiFi mode
  Serial.println("[DEBUG] Step 1: Setting WiFi mode to STA...");
  Serial.flush();
  WiFi.mode(WIFI_STA);
  delay(100);
  Serial.println("[DEBUG] ✓ WiFi mode set to STA");
  Serial.flush();
  
  // Step 2: Scan for networks to check if target network exists
  Serial.println("[DEBUG] Step 2: Scanning for available networks...");
  Serial.flush();
  delay(500);
  
  int n = WiFi.scanNetworks();
  Serial.print("[DEBUG] Scan complete. Found ");
  Serial.print(n);
  Serial.println(" networks");
  Serial.flush();
  
  bool networkFound = false;
  if (n > 0) {
    Serial.println("[DEBUG] Available networks:");
    for (int i = 0; i < n; i++) {
      Serial.print("[DEBUG]   ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm)");
      
      // Check if this is our target network
      if (WiFi.SSID(i) == String(ssid)) {
        Serial.print(" <-- TARGET FOUND!");
        networkFound = true;
      }
      Serial.println();
      Serial.flush();
    }
  } else {
    Serial.println("[DEBUG] ⚠ No networks found in scan");
    Serial.flush();
  }
  
  // Step 3: Only connect if network was found
  if (networkFound) {
    Serial.println("[DEBUG] Step 3: Target network found! Attempting connection...");
    Serial.flush();
    delay(100);
    
    Serial.println("[DEBUG] Calling WiFi.begin()...");
    Serial.flush();
    WiFi.begin(ssid, password);
    
    Serial.println("[DEBUG] WiFi.begin() returned - waiting for connection...");
    Serial.flush();
    delay(500);
    
    // Wait for connection with timeout
    int attempts = 0;
    int maxAttempts = 15;  // 7.5 seconds max
    Serial.print("[DEBUG] Connection status: ");
    
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");
      Serial.flush();
      attempts++;
      
      // Print attempt number every 3 attempts
      if (attempts % 3 == 0) {
        Serial.print("[");
        Serial.print(attempts);
        Serial.print("]");
        Serial.flush();
      }
    }
    
    Serial.println();
    Serial.print("[DEBUG] Final connection status: ");
    int status = WiFi.status();
    Serial.println(status);
    Serial.flush();
    delay(100);
    
    if (status == WL_CONNECTED) {
      Serial.println("[DEBUG] ✓✓✓ WiFi CONNECTED SUCCESSFULLY! ✓✓✓");
      Serial.print("[DEBUG] IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("[DEBUG] Signal strength (RSSI): ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("[DEBUG] ⚠ Connection attempt failed");
      Serial.print("[DEBUG] Status code: ");
      Serial.println(status);
      Serial.println("[DEBUG]   (WL_IDLE_STATUS=0, WL_NO_SSID_AVAIL=1, WL_SCAN_COMPLETED=2)");
      Serial.println("[DEBUG]   (WL_CONNECTED=3, WL_CONNECT_FAILED=4, WL_CONNECTION_LOST=5)");
      Serial.println("[DEBUG]   (WL_DISCONNECTED=6)");
    }
  } else {
    Serial.println("[DEBUG] Step 3: Target network NOT found in scan");
    Serial.println("[DEBUG] ⚠ Skipping connection attempt");
    Serial.println("[DEBUG]   Network may be out of range or hidden");
    Serial.println("[DEBUG]   Continuing without WiFi connection");
  }
  
  Serial.println("[DEBUG] ===== setupWiFi() COMPLETE =====");
  Serial.flush();
}

void handleRoot() {
  Serial.println("[DEBUG] handleRoot() called");
  float weight = getWeight();
  int irStatus = digitalRead(IR_SENSOR_PIN);
  String irStatusText = (irStatus == LOW) ? "OBSTRUCTION DETECTED" : "Clear";
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Smart Feeder</title>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; background: #f0f0f0; padding: 20px; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; }";
  html += ".status { margin: 20px 0; padding: 15px; background: #e8f5e9; border-radius: 5px; }";
  html += ".status.obstruction { background: #ffebee; }";
  html += "button { background: #4CAF50; color: white; padding: 15px 30px; font-size: 18px; border: none; border-radius: 5px; cursor: pointer; margin: 10px; }";
  html += "button:hover { background: #45a049; }";
  html += "button:disabled { background: #cccccc; cursor: not-allowed; }";
  html += ".weight { font-size: 24px; color: #2196F3; font-weight: bold; margin: 20px 0; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🐾 ESP32 Smart Feeder</h1>";
  html += "<div class='weight'>Current Weight: " + String(weight, 2) + " g</div>";
  html += "<div class='status " + String((irStatus == LOW) ? "obstruction" : "") + "'>";
  html += "IR Sensor: " + irStatusText + "</div>";
  html += "<button onclick='dispenseFood()' " + String((irStatus == LOW) ? "disabled" : "") + ">Dispense Food</button>";
  html += "<button onclick='updateWeight()'>Refresh Weight</button>";
  html += "<script>";
  html += "function dispenseFood() {";
  html += "  fetch('/dispense').then(r => r.text()).then(data => {";
  html += "    alert(data);";
  html += "    setTimeout(() => location.reload(), 2000);";
  html += "  });";
  html += "}";
  html += "function updateWeight() {";
  html += "  fetch('/weight').then(r => r.text()).then(data => {";
  html += "    document.querySelector('.weight').innerHTML = 'Current Weight: ' + data + ' g';";
  html += "  });";
  html += "}";
  html += "setInterval(updateWeight, 30000);";
  html += "</script>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleDispense() {
  Serial.println("[DEBUG] Dispense command received via web");
  dispenseFood();
  
  float weight = getWeight();
  String response = "Food dispensed! Current weight: " + String(weight, 2) + " g";
  server.send(200, "text/plain", response);
}

void handleWeight() {
  float weight = getWeight();
  server.send(200, "text/plain", String(weight, 2));
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void dispenseFood() {
  Serial.println("[DEBUG] dispenseFood() called");
  int irValue = digitalRead(IR_SENSOR_PIN);
  
  Serial.print("[DEBUG] IR Sensor status: ");
  Serial.println(irValue == LOW ? "OBSTRUCTION DETECTED" : "CLEAR");
  
  if (irValue == LOW) {
    Serial.println("[DEBUG] ❌ Dispensing BLOCKED - obstruction detected!");
    return;
  }
  
  Serial.println("[DEBUG] ✓ Starting food dispensing...");
  Serial.print("[DEBUG] Steps to move: ");
  Serial.println(DISPENSE_STEPS);
  
  digitalWrite(ENABLE_PIN, LOW);
  delay(10);
  
  stepper.move(DISPENSE_STEPS);
  
  Serial.println("[DEBUG] Motor running...");
  while (stepper.run()) {
    delay(1);
  }
  
  digitalWrite(ENABLE_PIN, HIGH);
  
  Serial.println("[DEBUG] ✓ Food dispensing complete!");
  delay(1000);
  Serial.println();
}

float getWeight() {
  if (scale.is_ready()) {
    float reading = scale.get_units(10);
    if (reading < 0) {
      reading = 0.0;
    }
    return reading;
  } else {
    return 0.0;
  }
}
