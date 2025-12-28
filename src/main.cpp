/*
 * ESP32 Smart Feeder
 * IoT-enabled smart pet feeder with stepper motor control, 
 * load cell weight measurement, and MQTT communication
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <AccelStepper.h>
#include <HX711.h>

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Configuration
const char* mqtt_server = "192.168.1.100";  // Change to your MQTT broker IP
const char* mqtt_topic_command = "kennel/feeder/1/command";
const char* mqtt_topic_weight = "kennel/feeder/1/weight";

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

// WiFi and MQTT Clients
WiFiClient espClient;
PubSubClient client(espClient);

// Timing Variables
unsigned long lastWeightPublish = 0;
const unsigned long weightPublishInterval = 30000;  // 30 seconds

// Function Prototypes
void setupWiFi();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
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
  
  // Setup MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  Serial.println("Setup complete!");
}

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  // Publish weight every 30 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - lastWeightPublish >= weightPublishInterval) {
    float weight = getWeight();
    char weightStr[20];
    dtostrf(weight, 4, 2, weightStr);
    
    if (client.publish(mqtt_topic_weight, weightStr)) {
      Serial.print("Weight published: ");
      Serial.print(weightStr);
      Serial.println(" g");
    } else {
      Serial.println("Failed to publish weight");
    }
    
    lastWeightPublish = currentMillis;
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

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    if (client.connect("ESP32SmartFeeder")) {
      Serial.println("connected!");
      
      // Subscribe to command topic
      client.subscribe(mqtt_topic_command);
      Serial.print("Subscribed to: ");
      Serial.println(mqtt_topic_command);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Check if it's a dispense command
  if (strcmp(topic, mqtt_topic_command) == 0) {
    if (strcmp(message, "dispense") == 0) {
      Serial.println("Dispense command received");
      dispenseFood();
    }
  }
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
  
  // Publish updated weight after dispensing
  delay(1000);  // Wait for food to settle
  float weight = getWeight();
  char weightStr[20];
  dtostrf(weight, 4, 2, weightStr);
  client.publish(mqtt_topic_weight, weightStr);
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
