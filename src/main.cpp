/*
 * ESP32 Smart Feeder
 * IoT-enabled smart pet feeder with stepper motor control, 
 * load cell weight measurement, and web server for remote control
 */

#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <HX711.h>

// WiFi Configuration
const char* ssid = "Casa da Opera";
const char* password = "naovoudizer";

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
const unsigned long weightDisplayInterval = 30000;  // 30 seconds

// Function Prototypes
void setupWiFi();
void handleRoot();
void handleDispense();
void handleWeight();
void handleNotFound();
void dispenseFood();
float getWeight();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 Smart Feeder - Initializing...");
  
  // Initialize Stepper Motor
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);  // Disable motor initially
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  
  // Initialize IR Sensor
  pinMode(IR_SENSOR_PIN, INPUT);
  
  // Initialize Load Cell
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();  // Reset the scale to 0
  
  // Connect to WiFi
  setupWiFi();
  
  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/dispense", handleDispense);
  server.on("/weight", handleWeight);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started!");
  Serial.print("Access the feeder at: http://");
  Serial.println(WiFi.localIP());
  
  Serial.println("Setup complete!");
}

void loop() {
  server.handleClient();
  
  // Display weight every 30 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastWeightDisplay >= weightDisplayInterval) {
    float weight = getWeight();
    Serial.print("Current weight: ");
    Serial.print(weight);
    Serial.println(" g");
    lastWeightDisplay = currentMillis;
  }
  
  // Run stepper motor if needed
  stepper.run();
  
  delay(10);
}

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
}

void handleRoot() {
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
  html += "setInterval(updateWeight, 30000);";  // Auto-refresh every 30 seconds
  html += "</script>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleDispense() {
  Serial.println("Dispense command received via web");
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
  // Check IR sensor for safety
  // Most IR obstacle sensors output LOW when obstacle detected, HIGH when clear
  int irValue = digitalRead(IR_SENSOR_PIN);
  
  if (irValue == LOW) {  // LOW means obstacle detected
    Serial.println("IR sensor detects obstruction - dispensing blocked!");
    return;
  }
  
  Serial.println("Dispensing food...");
  
  // Enable motor
  digitalWrite(ENABLE_PIN, LOW);
  delay(10);
  
  // Move stepper motor
  stepper.move(DISPENSE_STEPS);
  
  // Wait for movement to complete
  while (stepper.run()) {
    delay(1);
  }
  
  // Disable motor to save power
  digitalWrite(ENABLE_PIN, HIGH);
  
  Serial.println("Food dispensing complete!");
  
  // Wait for food to settle before next reading
  delay(1000);
}

float getWeight() {
  if (scale.is_ready()) {
    float reading = scale.get_units(10);  // Average of 10 readings
    if (reading < 0) {
      reading = 0.0;  // Don't return negative weights
    }
    return reading;
  } else {
    Serial.println("HX711 not found.");
    return 0.0;
  }
}
