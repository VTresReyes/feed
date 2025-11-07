#include "HX711.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <ArduinoJson.h>

// Wi-Fi Credentials
const char* ssid = "animal_shelter";
const char* password = "";

// Hardcoded device no. which serves as the module ID
String MODULE_ID = "MODULE01";

// ESP32-CAM's MAC Address
uint8_t camera_address[] = { 0xEC, 0xE3, 0x34, 0xB4, 0x70, 0x40 };

// HX711 objects
HX711 scale1;
HX711 scale2;

// Load cell pins
const int LOADCELL1_DOUT = 26;
const int LOADCELL1_SCK = 27;

const int LOADCELL2_DOUT = 25;
const int LOADCELL2_SCK = 33;

// Calibration factors
float calibration_factor1 = -218.784;
float calibration_factor2 = -218.784;

// Servo
Servo servo1, servo2;

// variables for checking if the remaining weight is low
bool alert_acknowledged = false;
unsigned long last_button_press = 0;
const unsigned long DEBOUNCE_DELAY = 50;

unsigned long last_schedule_check = 0;
unsigned long last_weight_alert = 0;
unsigned long dispense_time = 0;  // Track when dispensing happened
bool waiting_for_camera = false;  // Flag to track if we're waiting

const unsigned long SCHEDULE_CHECK_INTERVAL = 30000;  // 30 seconds
const unsigned long CAMERA_DELAY = 180000;             // 3 minutes (180000 ms)


// Servo pins
const int SERVO1_PIN = 14;
const int SERVO2_PIN = 12;

// Manual Button Pin
const int MANUAL_BUTTON = 32;

// Reset Button Pin
const int RESET_BUTTON = 4;

// Buzzer Pin
const int BUZZER_PIN = 15;

// weight threshold in grams, before the alarm turns on
const int weight_threshold = 1000;

// ESP-NOW callback
void on_sent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.print("Camera trigger status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32 Dispenser Module Initializing...");
  Serial.println("======================================");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    Serial.println("ESP-NOW initialized successfully!");

    // Register send callback
    esp_now_register_send_cb(on_sent);

    // Register peer (ESP32-CAM)
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, camera_address, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
      Serial.println("Failed to add ESP32-CAM peer");
    } else {
      Serial.println("ESP32-CAM peer added successfully!");
    }
  }

  // Initialize load cells
  scale1.begin(LOADCELL1_DOUT, LOADCELL1_SCK);
  scale2.begin(LOADCELL2_DOUT, LOADCELL2_SCK);

  // Initialize servo motors
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  pinMode(MANUAL_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(RESET_BUTTON, INPUT_PULLUP);

  Serial.println("\nTaring scales...");
  scale1.tare();
  scale2.tare();

  scale1.set_scale(calibration_factor1);
  scale2.set_scale(calibration_factor2);

  delay(1000);

  Serial.println("\n======================================");
  Serial.println("System ready! Waiting for schedule...");
  Serial.println("======================================\n");

  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if it's time to trigger camera (3 minutes after dispensing)
  if (waiting_for_camera && (currentMillis - dispense_time >= CAMERA_DELAY)) {
    Serial.println("\n>>> 3 MINUTES ELAPSED - TRIGGERING CAMERA <<<");
    send_camera_trigger();
    waiting_for_camera = false;  // Reset flag
    Serial.println(">>> Camera trigger sent <<<\n");
  }

  // Check for scheduled feeding every 30 seconds
  if (currentMillis - last_schedule_check >= SCHEDULE_CHECK_INTERVAL) {
    Serial.println("=== Schedule check ===");
    last_schedule_check = currentMillis;

    int feed_amount = 0;
    int schedule_id = 0;

    if (check_schedule(feed_amount, schedule_id)) {
      Serial.println("\n>>> DISPENSE TRIGGERED! <<<");

      // Dispense food
      bool dispense_success = dispense(feed_amount);

      if (dispense_success) {
        // Mark schedule as complete
        complete_schedule(schedule_id);
        Serial.println("✓ Schedule completed");

        // Update weight after dispensing
        weight_update();

        // Start 3-minute countdown for camera
        dispense_time = currentMillis;
        waiting_for_camera = true;
        Serial.println("⏱️  Camera will trigger in 3 minutes...");
      } else {
        Serial.println("✗ Dispensing failed - schedule NOT marked complete");
      }

      Serial.println(">>> Dispense cycle complete <<<\n");
    }
  }

  // Manual dispense (always available)
  if (digitalRead(MANUAL_BUTTON) == LOW) {
    Serial.print("Manual dispense");
    manual_dispense();
  }

  // Small delay to prevent watchdog issues
  delay(10);
}

void send_camera_trigger() {
  Serial.println("Sending trigger to ESP32-CAM...");
  uint8_t trigger = 1;
  esp_err_t result = esp_now_send(camera_address, &trigger, sizeof(trigger));

  if (result == ESP_OK) {
    Serial.println("✓ Camera trigger sent!");
  } else {
    Serial.println("✗ Camera trigger send failed");
  }
}

bool check_schedule(int& amount, int& schedule_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected!");
    return false;
  }

  HTTPClient http;
  http.begin("http://raspberrypi.local:8080/check_schedule");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload = "module_id=" + MODULE_ID;
  int code = http.POST(payload);

  if (code == 200) {
    String response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);

    http.end();

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      amount = 0;
      schedule_id = 0;
      return false;
    }

    bool dispense = doc["dispense"] | false;

    if (dispense) {
      amount = int(doc["amount"]) | 0;       // Store weight in grams
      schedule_id = doc["schedule_id"] | 0;  // Store schedule_id
      const char* scheduled_time = doc["scheduled_time"] | "unknown";

      low_weight_alert();

      Serial.println("Dispensing scheduled!");
      Serial.print("Schedule ID: ");
      Serial.println(schedule_id);
      Serial.print("Scheduled time: ");
      Serial.println(scheduled_time);
      Serial.print("Amount: ");
      Serial.println(amount);

      return true;
    } else {
      Serial.println("No pending schedule");
      amount = 0;
      schedule_id = 0;
      return false;
    }
  } else if (code == 404) {
    String response = http.getString();
    Serial.println("Error: Invalid or inactive module_id");
    Serial.println(response);
    http.end();
    amount = 0;
    schedule_id = 0;
    return false;
  } else if (code == 400) {
    String response = http.getString();
    Serial.println("Error: Missing module_id parameter");
    Serial.println(response);
    http.end();
    amount = 0;
    schedule_id = 0;
    return false;
  } else if (code > 0) {
    String response = http.getString();
    Serial.print("Failed to check schedule. HTTP code: ");
    Serial.println(code);
    Serial.println(response);
    http.end();
    amount = 0;
    schedule_id = 0;
    return false;
  } else {
    Serial.print("Connection error: ");
    Serial.println(http.errorToString(code));
    http.end();
    amount = 0;
    schedule_id = 0;
    return false;
  }
}

bool complete_schedule(int schedule_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Cannot complete schedule.");
    return false;
  }

  if (schedule_id == 0) {
    Serial.println("Invalid schedule_id");
    return false;
  }

  Serial.print("Marking schedule as complete: ");
  Serial.println(schedule_id);

  HTTPClient http;
  http.begin("http://raspberrypi.local:8080/complete_schedule");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload = "schedule_id=" + String(schedule_id) + "&module_id=" + MODULE_ID;
  int code = http.POST(payload);

  Serial.print("Complete schedule response code: ");
  Serial.println(code);

  if (code == 200) {
    String response = http.getString();
    Serial.println("✓ Schedule completed successfully!");
    Serial.print("Server response: ");
    Serial.println(response);
    http.end();
    return true;
  } else if (code == 400) {
    String response = http.getString();
    Serial.println("✗ Bad request - Schedule already completed or missing data");
    Serial.println(response);
    http.end();
    return false;
  } else if (code == 403) {
    String response = http.getString();
    Serial.println("✗ Module ID mismatch - not authorized");
    Serial.println(response);
    http.end();
    return false;
  } else if (code == 404) {
    String response = http.getString();
    Serial.println("✗ Schedule not found");
    Serial.println(response);
    http.end();
    return false;
  } else if (code > 0) {
    String response = http.getString();
    Serial.print("✗ Failed to complete schedule. HTTP code: ");
    Serial.println(code);
    Serial.println(response);
    http.end();
    return false;
  } else {
    Serial.print("✗ Connection error: ");
    Serial.println(http.errorToString(code));
    http.end();
    return false;
  }
}

void weight_update() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Cannot update weight.");
    return;
  }

  Serial.println("Updating weight on server...");

  // float weight = random(0, 10000) / 10.0;  // Random weight 0-1000g for testing
  float weight = get_current_weight() - 100;

  Serial.print("Current weight: ");
  Serial.print(weight, 2);  // Show 2 decimal places
  Serial.println(" g");

  // Validate weight locally before sending
  if (weight < 0 || weight > 10000) {
    Serial.println("✗ Invalid weight value, skipping update");
    return;
  }

  HTTPClient http;
  http.begin("http://raspberrypi.local:8080/weight_update");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload = "module_id=" + MODULE_ID + "&weight=" + String(weight, 2);

  int httpResponseCode = http.POST(payload);

  Serial.print("Weight update response code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("✓ Weight updated successfully!");
    Serial.print("Server response: ");
    Serial.println(response);
  } else if (httpResponseCode == 400) {
    String response = http.getString();
    Serial.println("✗ Bad request - Missing or invalid data");
    Serial.println(response);
  } else if (httpResponseCode == 403) {
    String response = http.getString();
    Serial.println("✗ Module not registered on server!");
    Serial.println(response);
    Serial.println("Please register this module first.");
  } else if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("✗ Weight update failed with code: ");
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("✗ Connection error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
  low_weight_alert_test();
}

void low_weight_alert() {
  float weight = get_current_weight();

  if (weight < weight_threshold) {
    Serial.println("⚠️ WARNING: Feed is below threshold!");

    do {
      // Beep pattern
      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(BUZZER_PIN, LOW);
      delay(300);

      // Keep updating weight
      weight = get_current_weight();

      // Stop if weight returns to normal
      if (weight >= weight_threshold) {
        Serial.println("✅ Feed weight back to normal. Stopping alert.");
        break;
      }

      // Stop if reset button pressed
      if (digitalRead(RESET_BUTTON) == LOW) {
        delay(50); // debounce
        if (digitalRead(RESET_BUTTON) == LOW) {
          Serial.println("✓ Alert acknowledged by button press");
          break;
        }
      }

    } while (true); // loop until one of the breaks above

    digitalWrite(BUZZER_PIN, LOW); // make sure buzzer stops
  }
}

void low_weight_alert_test() {
  float weight = 905;

  if (weight < weight_threshold) {
    Serial.println("⚠️ WARNING: Feed is below threshold!");

    do {
      // Beep pattern
      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(BUZZER_PIN, LOW);
      delay(300);

      // Keep updating weight
      weight = get_current_weight();

      // Stop if weight returns to normal
      if (weight >= weight_threshold) {
        Serial.println("✅ Feed weight back to normal. Stopping alert.");
        break;
      }

      // Stop if reset button pressed
      if (digitalRead(RESET_BUTTON) == LOW) {
        delay(50); // debounce
        if (digitalRead(RESET_BUTTON) == LOW) {
          Serial.println("✓ Alert acknowledged by button press");
          break;
        }
      }

    } while (true); // loop until one of the breaks above

    digitalWrite(BUZZER_PIN, LOW); // make sure buzzer stops
  }
}


void rotate_motors() {
  servo1.write(0);
  servo2.write(0);
}

void stop_motors() {
  servo1.write(90);
  servo2.write(90);
}

bool dispense(int feed_amount) {
  Serial.println("\n=== Starting Dispense ===");

  // // Validate feed amount
  // if (feed_amount <= 0 || feed_amount > 500) {
  //   Serial.println("✗ Invalid feed amount (must be 1-500g)");
  //   return false;
  // }

  // Get initial weight
  float initial_weight = get_current_weight();

  Serial.print("Initial weight: ");
  Serial.print(initial_weight, 1);
  Serial.println(" g");
  Serial.print("Target amount: ");
  Serial.print(feed_amount);
  Serial.println(" g");

  // Calculate target weight
  float target_weight = initial_weight - feed_amount;

  if (target_weight < 0) {
    Serial.println("Insufficient amount left.");
    return false;
  }

  float current_weight = initial_weight;

  Serial.print("Target weight: ");
  Serial.print(target_weight, 1);
  Serial.println(" g");

  // Safety: Maximum dispensing time (30 seconds)
  unsigned long start_time = millis();
  const unsigned long MAX_DISPENSE_TIME = 30000;

  // Dispensing loop
  Serial.println("Dispensing...");
  while (current_weight > target_weight) {
    // Safety timeout check
    if (millis() - start_time > MAX_DISPENSE_TIME) {
      stop_motors();
      Serial.println("✗ Dispense timeout! Stopping motors.");
      return false;
    }

    rotate_motors();
    delay(50);

    current_weight -= 10;  // get_current_weight();
  }

  stop_motors();

  // Calculate actual dispensed amount
  float actual_dispensed = initial_weight - current_weight;

  Serial.println("\n=== Dispense Complete ===");
  Serial.print("Final weight: ");
  Serial.print(current_weight, 1);
  Serial.println(" g");
  Serial.print("Dispensed amount: ");
  Serial.print(actual_dispensed, 1);
  Serial.println(" g");

  // Verify dispensing accuracy (±15g tolerance)
  const float TOLERANCE = 15.0;
  float difference = abs(actual_dispensed - feed_amount);

  Serial.print("Difference from target: ");
  Serial.print(difference, 1);
  Serial.println(" g");

  if (difference <= TOLERANCE) {
    Serial.println("✓ Dispense successful!");
    return true;
  } else {
    Serial.println("⚠️ Dispense completed but outside tolerance");
    // Still return true if at least some food was dispensed
    if (actual_dispensed >= feed_amount * 0.5) {
      Serial.println("✓ Acceptable amount dispensed (>50% of target)");
      return true;
    } else {
      Serial.println("✗ Insufficient food dispensed");
      return false;
    }
  }
}

void manual_dispense() {
  Serial.print("Manuuuuuual");
  while (digitalRead(MANUAL_BUTTON) == LOW) {
    rotate_motors();
    delay(50);
  }
  stop_motors();

  // Update weight after dispensing
  weight_update();
}

float get_current_weight() {
  // float w1 = scale1.get_units(5);
  // float w2 = scale2.get_units(5);
  // return (w1 + w2) / 2.0;
  Serial.println("=== get_current_weight invoked ===");
  return 1005;
}
