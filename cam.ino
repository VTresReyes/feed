#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include "esp_camera.h"

// ==== WiFi Credentials ====
const char* ssid = "animal_shelter";
const char* password = "";
String CAMERA_ID = "CAMERA01";

// ==== Camera Pins (AI Thinker model) ====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flag to track if trigger received
volatile bool capture_trigger = false;

// Callback when data is received via ESP-NOW
void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Serial.println("=============================");
  Serial.println("TRIGGER RECEIVED!");
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.print("Data length: ");
  Serial.println(len);
  Serial.print("Data value: ");
  Serial.println(data[0]);
  Serial.println("=============================");
  
  capture_trigger = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\nESP32-CAM Initializing...");
  Serial.println("=========================");

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // Init camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }
  Serial.println("Camera initialized successfully!");

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Print MAC Address (COPY THIS to ESP32 sender code!)
  Serial.print("ESP32-CAM MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());
  Serial.println(">>> Copy this MAC address to the ESP32 sender! <<<");
  Serial.println();

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed!");
    return;
  }
  
  Serial.println("ESP-NOW initialized successfully!");
  
  // Register receive callback
  esp_now_register_recv_cb(on_receive);
  
  Serial.println("Ready! Waiting for triggers...");
  Serial.println("=========================\n");
}

void captureAndUpload() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Cannot upload image.");
    return;
  }

  Serial.println(">>> Starting capture and upload <<<");
  
  // Capture photo
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("ERROR: Camera capture failed!");
    return;
  }

  Serial.print("Photo captured! Size: ");
  Serial.print(fb->len);
  Serial.println(" bytes");
  Serial.println("Uploading to server...");

  HTTPClient http;
  http.begin("http://raspberrypi.local:8080/upload_image");
  
  // Create multipart form data boundary
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);

  // Build multipart form data payload
  String payloadStart = "--" + boundary + "\r\n";
  payloadStart += "Content-Disposition: form-data; name=\"camera_id\"\r\n\r\n";
  payloadStart += CAMERA_ID + "\r\n";
  payloadStart += "--" + boundary + "\r\n";
  payloadStart += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
  payloadStart += "Content-Type: image/jpeg\r\n\r\n";
  
  String payloadEnd = "\r\n--" + boundary + "--\r\n";
  
  // Calculate total size
  int totalLen = payloadStart.length() + fb->len + payloadEnd.length();
  
  // Allocate buffer for complete payload
  uint8_t *fullPayload = (uint8_t*)malloc(totalLen);
  if (!fullPayload) {
    Serial.println("ERROR: Memory allocation failed!");
    esp_camera_fb_return(fb);
    return;
  }
  
  // Combine all parts
  memcpy(fullPayload, payloadStart.c_str(), payloadStart.length());
  memcpy(fullPayload + payloadStart.length(), fb->buf, fb->len);
  memcpy(fullPayload + payloadStart.length() + fb->len, payloadEnd.c_str(), payloadEnd.length());
  
  // Send POST request
  int res = http.POST(fullPayload, totalLen);

  Serial.print("Upload response code: ");
  Serial.println(res);
  
  if (res == 200) {
    String response = http.getString();
    Serial.println("✓ Upload successful!");
    Serial.print("Server response: ");
    Serial.println(response);
  } else if (res == 400) {
    String response = http.getString();
    Serial.println("✗ Bad request - Missing camera_id or image");
    Serial.println(response);
  } else if (res == 404) {
    String response = http.getString();
    Serial.println("✗ Camera not registered or inactive!");
    Serial.println(response);
  } else if (res > 0) {
    String response = http.getString();
    Serial.print("✗ Upload failed with code: ");
    Serial.println(res);
    Serial.println(response);
  } else {
    Serial.print("✗ Connection error: ");
    Serial.println(http.errorToString(res));
  }

  // Cleanup
  free(fullPayload);
  http.end();
  esp_camera_fb_return(fb);
  
  Serial.println(">>> Capture and upload complete <<<\n");
}

void loop() {
  // Check if trigger received
  if (capture_trigger) {
    capture_trigger = false; // Reset flag
    captureAndUpload();
  }
  
  delay(100);
}
