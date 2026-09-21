// ============================================================
// IoT-based Smart Poultry Farm Automation System
// EEE 416 - Microprocessor and Embedded System Laboratory
// Section: C2, Group: 06
// Extracted from Final Project Report
// ============================================================

// ---- Blynk Configuration ----
#define BLYNK_TEMPLATE_ID   "TMPL6bBGNkNAC"
#define BLYNK_TEMPLATE_NAME "Automated Poultry Farm System"
#define BLYNK_AUTH_TOKEN    "ic1DFZNAtZnvuScyPZYgy_7-4OMY1zmP"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ---- WiFi Credentials ----
char ssid[] = "Arjun";
char pass[] = "arjunpaul";

// ---- Pin Definitions ----
#define LDR_PIN              34  // Light sensor
#define DHT_PIN              4   // Temperature/Humidity
#define GAS_PIN              35  // MQ135 Gas sensor
#define WATER_PIN            32  // Water level sensor
#define SERVO_PIN            13  // Food dispenser servo
#define FAN_RELAY            25  // Cooling fan
#define HEAT_BULB_RELAY      26  // Incandescent heating bulb
#define OUTDOOR_LIGHT_RELAY  27  // LED outdoor light
#define EXHAUST_RELAY        14  // Exhaust fan
#define PUMP_RELAY           33  // Water pump (moved from GPIO12)
#define HUMIDIFIER_RELAY     12  // Humidifier control

// ---- Ultrasonic Sensor Pins ----
#define TRIG_PIN 16
#define ECHO_PIN 17

// ---- Sensor Objects ----
DHT dht(DHT_PIN, DHT22);
Servo foodServo;

// ---- System Thresholds ----
float tempHighThreshold = 30.0; // °C (cooling trigger)
float tempLowThreshold  = 20.0; // °C (heating trigger)
int   lightThreshold    = 2000; // LDR value for outdoor light (higher = darker)
int   gasThreshold      = 3000; // MQ135 air quality threshold
int   waterLowerThreshold = 20; // % (pump on threshold)
int   waterUpperThreshold = 80; // % (pump off threshold)
float humLowThreshold   = 60.0; // % (humidifier on threshold)
float humHighThreshold  = 75.0; // % (humidifier off threshold)

// ---- Food Control Thresholds (in cm) ----
int foodEmptyThreshold = 40; // Distance when food is empty (dispense)
int foodFullThreshold  = 20; // Distance when food is full (stop)

// ---- System State ----
bool manualMode = false;
bool foodDispensedFlag = false;
bool foodRetractedFlag = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[System Initializing]");

  // Initialize sensors
  dht.begin();

  // Setup ultrasonic sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Setup servo motor
  ESP32PWM::allocateTimer(0);
  foodServo.attach(SERVO_PIN);
  foodServo.write(90); // Neutral position
  delay(500);
  foodServo.detach();

  // Initialize relay outputs
  pinMode(FAN_RELAY, OUTPUT);
  pinMode(HEAT_BULB_RELAY, OUTPUT);
  pinMode(OUTDOOR_LIGHT_RELAY, OUTPUT);
  pinMode(EXHAUST_RELAY, OUTPUT);
  pinMode(PUMP_RELAY, OUTPUT);
  pinMode(HUMIDIFIER_RELAY, OUTPUT);

  // Start with all relays OFF (HIGH for active-low)
  digitalWrite(FAN_RELAY, HIGH);
  digitalWrite(HEAT_BULB_RELAY, HIGH);
  digitalWrite(OUTDOOR_LIGHT_RELAY, HIGH);
  digitalWrite(EXHAUST_RELAY, HIGH);
  digitalWrite(PUMP_RELAY, HIGH);
  digitalWrite(HUMIDIFIER_RELAY, HIGH);

  // Connect to Blynk
  Serial.print("Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Connected!");

  Serial.println("\n[System Ready]");
  Serial.println("Current Thresholds:");
  Serial.println("-------------------");
  Serial.print("Temp High: "); Serial.print(tempHighThreshold); Serial.println("°C");
  Serial.print("Temp Low: "); Serial.print(tempLowThreshold); Serial.println("°C");
  Serial.print("Humidity Low: "); Serial.print(humLowThreshold); Serial.println("%");
  Serial.print("Light Threshold: "); Serial.println(lightThreshold);
  Serial.print("Gas Threshold: "); Serial.println(gasThreshold);
  Serial.print("Water Thresholds: "); Serial.print(waterLowerThreshold);
  Serial.print("%-"); Serial.print(waterUpperThreshold); Serial.println("%");
  Serial.print("Food Thresholds: "); Serial.print(foodEmptyThreshold);
  Serial.print("cm-"); Serial.print(foodFullThreshold); Serial.println("cm");
}

void loop() {
  if (!Blynk.connected()) {
    Serial.println("Blynk disconnected - attempting reconnect");
    Blynk.connect();
  }
  Blynk.run();

  readSensors();

  if (!manualMode) {
    automaticControl();
  }

  sendDataToBlynk();
  delay(2000);
}

float getFoodDistance() {
  // Trigger the ultrasonic sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the echo response
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.0343 / 2; // Convert to cm

  // Validate the reading
  if (distance >= 2 && distance <= 400) { // Valid range (2cm to 4m)
    return distance;
  }
  return -1; // Invalid reading
}

void readSensors() {
  // Temperature and Humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Light Level
  int lightValue = analogRead(LDR_PIN);

  // Gas Level
  int gasValue = analogRead(GAS_PIN);

  // Water Level (0-100%)
  int waterLevel = map(analogRead(WATER_PIN), 0, 4095, 0, 100);

  // Food Level (distance in cm)
  float foodDistance = getFoodDistance();

  // Print sensor readings
  Serial.println("\n========================================");
  Serial.println("           SENSOR READINGS");
  Serial.println("========================================");
  Serial.println("| Sensor        | Value  | Unit |");
  Serial.println("----------------------------------------");
  Serial.print("| Temperature   | "); Serial.print(temperature); Serial.println(" | °C   |");
  Serial.print("| Humidity      | "); Serial.print(humidity); Serial.println(" | %    |");
  Serial.print("| Light Level   | "); Serial.print(lightValue); Serial.println(" | raw  |");
  Serial.print("| Gas Level     | "); Serial.print(gasValue); Serial.println(" | raw  |");
  Serial.print("| Water Level   | "); Serial.print(waterLevel); Serial.println(" | %    |");
  Serial.print("| Food Distance | "); Serial.print(foodDistance); Serial.println(" | cm   |");
  Serial.println("========================================");
}

void automaticControl() {
  if (manualMode) return; // Skip if in manual mode

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightValue = analogRead(LDR_PIN);
  int gasValue = analogRead(GAS_PIN);
  int waterLevel = map(analogRead(WATER_PIN), 0, 4095, 0, 100);
  float foodDistance = getFoodDistance();

  Serial.println("\n[Control Actions]");
  Serial.println("-----------------");

  // Temperature Control (Heating Bulb)
  if (temperature < tempLowThreshold) {
    digitalWrite(HEAT_BULB_RELAY, LOW);
    Serial.println("Heating Bulb: ON (Low Temperature)");
  } else {
    digitalWrite(HEAT_BULB_RELAY, HIGH);
    Serial.println("Heating Bulb: OFF");
  }

  // Humidity Control (Humidifier)
  if (humidity < humLowThreshold) {
    digitalWrite(HUMIDIFIER_RELAY, LOW);
    Serial.println("Humidifier: ON (Low Humidity)");
  } else if (humidity > humHighThreshold) {
    digitalWrite(HUMIDIFIER_RELAY, HIGH);
    Serial.println("Humidifier: OFF (High Humidity)");
  }

  // Light Control (Outdoor Light)
  if (lightValue > lightThreshold) {
    digitalWrite(OUTDOOR_LIGHT_RELAY, LOW);
    Serial.println("Outdoor Light: ON (Low Light)");
  } else {
    digitalWrite(OUTDOOR_LIGHT_RELAY, HIGH);
    Serial.println("Outdoor Light: OFF");
  }

  // Cooling Fan (Temperature/Humidity)
  if (temperature > tempHighThreshold || humidity > humHighThreshold) {
    digitalWrite(FAN_RELAY, LOW);
    Serial.println("Cooling Fan: ON (High Temp/Humidity)");
  } else {
    digitalWrite(FAN_RELAY, HIGH);
    Serial.println("Cooling Fan: OFF");
  }

  // Exhaust Fan (Air Quality)
  if (gasValue > gasThreshold) {
    digitalWrite(EXHAUST_RELAY, LOW);
    Serial.println("Exhaust Fan: ON (Poor Air Quality)");
  } else {
    digitalWrite(EXHAUST_RELAY, HIGH);
    Serial.println("Exhaust Fan: OFF");
  }

  // Water Pump Control
  if (waterLevel < waterLowerThreshold) {
    digitalWrite(PUMP_RELAY, LOW);
    Serial.println("Water Pump: ON (Low Level)");
  } else if (waterLevel > waterUpperThreshold) {
    digitalWrite(PUMP_RELAY, HIGH);
    Serial.println("Water Pump: OFF (Tank Full)");
  }

  // Food Dispenser Control (using ultrasonic sensor)
  if (foodDistance > foodEmptyThreshold && !foodDispensedFlag) {
    foodServo.attach(SERVO_PIN);
    foodServo.write(0); // Dispense position
    delay(1000);
    foodServo.write(90); // Neutral position
    delay(500);
    foodServo.detach();
    foodDispensedFlag = true;
    foodRetractedFlag = false;
    Serial.println("Food Dispensed (Low Level)");
  }
  else if (foodDistance < foodFullThreshold && !foodRetractedFlag) {
    foodServo.attach(SERVO_PIN);
    foodServo.write(180); // Retract position
    delay(1000);
    foodServo.write(90); // Neutral position
    delay(500);
    foodServo.detach();
    foodRetractedFlag = true;
    foodDispensedFlag = false;
    Serial.println("Food Retracted (Adequate Level)");
  }
}

void sendDataToBlynk() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int lightValue = analogRead(LDR_PIN);
  int gasValue = analogRead(GAS_PIN);
  int waterLevel = map(analogRead(WATER_PIN), 0, 4095, 0, 100);
  float foodDistance = getFoodDistance();

  Blynk.virtualWrite(V10, temperature); // Temperature gauge
  Blynk.virtualWrite(V11, humidity);    // Humidity gauge
  Blynk.virtualWrite(V12, gasValue);    // Gas level
  Blynk.virtualWrite(V13, lightValue);  // Light level
  Blynk.virtualWrite(V14, waterLevel);  // Water level
  Blynk.virtualWrite(V15, foodDistance);// Food distance
  Blynk.virtualWrite(V1, manualMode);   // Mode indicator
}

// ============================================================
// Blynk Control Handlers
// ============================================================

BLYNK_WRITE(V0) { // Mode switch
  manualMode = param.asInt();
  Serial.print("Mode changed to: ");
  Serial.println(manualMode ? "MANUAL" : "AUTO");

  // When switching to auto mode, ensure all manual controls are turned off
  if (!manualMode) {
    digitalWrite(FAN_RELAY, HIGH);
    digitalWrite(HEAT_BULB_RELAY, HIGH);
    digitalWrite(OUTDOOR_LIGHT_RELAY, HIGH);
    digitalWrite(EXHAUST_RELAY, HIGH);
    digitalWrite(PUMP_RELAY, HIGH);
    digitalWrite(HUMIDIFIER_RELAY, HIGH);
    Serial.println("All manual controls reset to OFF");

    // Reset all Blynk buttons to OFF state
    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
    Blynk.virtualWrite(V5, 0);
    Blynk.virtualWrite(V6, 0);
    Blynk.virtualWrite(V7, 0);
    Blynk.virtualWrite(V8, 0);
  }
}

BLYNK_WRITE(V2) { // Manual fan control
  if (manualMode) {
    digitalWrite(FAN_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Fan: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V2, 0); // Reset the button in Blynk if not in manual mode
  }
}

BLYNK_WRITE(V3) { // Manual heat bulb control
  if (manualMode) {
    digitalWrite(HEAT_BULB_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Heat Bulb: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V3, 0);
  }
}

BLYNK_WRITE(V4) { // Manual outdoor light control
  if (manualMode) {
    digitalWrite(OUTDOOR_LIGHT_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Outdoor Light: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V4, 0);
  }
}

BLYNK_WRITE(V5) { // Manual exhaust control
  if (manualMode) {
    digitalWrite(EXHAUST_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Exhaust: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V5, 0);
  }
}

BLYNK_WRITE(V6) { // Manual water pump control
  if (manualMode) {
    digitalWrite(PUMP_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Water Pump: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V6, 0);
  }
}

BLYNK_WRITE(V7) { // Manual food dispense
  if (manualMode && param.asInt()) {
    foodServo.attach(SERVO_PIN);
    foodServo.write(0); // Dispense position
    delay(1000);        // Longer delay for dispensing
    foodServo.write(90);// Neutral position
    delay(500);
    foodServo.detach();
    Serial.println("Manual Food Dispensed");
    Blynk.virtualWrite(V7, 0); // Reset the button immediately
  }
}

BLYNK_WRITE(V8) { // Manual humidifier control
  if (manualMode) {
    digitalWrite(HUMIDIFIER_RELAY, param.asInt() ? LOW : HIGH);
    Serial.print("Manual Humidifier: ");
    Serial.println(param.asInt() ? "ON" : "OFF");
  } else {
    Blynk.virtualWrite(V8, 0);
  }
}
