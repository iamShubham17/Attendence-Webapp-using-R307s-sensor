/*
 * ESP8266 Fingerprint Attendance System
 * Using R307 Fingerprint Sensor
 * 
 * Connections:
 * R307 -> ESP8266
 * VCC  -> 5V
 * GND  -> GND
 * TX   -> D2 (GPIO4) - voltage divider
 * RX   -> D1 (GPIO5)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Happy Home";
const char* password = "Giradkar@1717";

// Software Serial: RX=D2(GPIO4), TX=D1(GPIO5)
SoftwareSerial mySerial(6, 5); // RX=4, TX=5
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

ESP8266WebServer server(80);

// Variables
uint8_t id = 1;
bool enrollMode = false;
String enrollStatus = "";
int enrollStep = 0;
unsigned long lastEnrollAttempt = 0;
bool sensorConnected = false;
String currentFingerprintId = "";

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n================================");
  Serial.println("ESP8266 Fingerprint System Starting...");
  Serial.println("================================");
  
  // Fingerprint sensor init
  Serial.println("Initializing fingerprint sensor...");
  mySerial.begin(57600);
  delay(100);
  
  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor found at 57600 baud!");
    sensorConnected = true;
  } else {
    Serial.println("❌ Sensor not found - check wiring!");
    sensorConnected = false;
  }
  
  if (sensorConnected) {
    finger.getTemplateCount();
    Serial.print("Templates: "); Serial.println(finger.templateCount);
  }
  
  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed!");
  }
  
  // Routes (explicit HTTP_GET)
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/enroll/start", HTTP_POST, handleEnrollStart);
  server.on("/enroll/status", HTTP_GET, handleEnrollStatus);
  server.on("/enroll/cancel", HTTP_POST, handleEnrollCancel);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/check", HTTP_GET, handleCheckFingerprint);
  server.on("/count", HTTP_GET, handleGetCount);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("✅ HTTP server started on port 80");
  Serial.println("Test: http://" + WiFi.localIP().toString() + "/status");
}

void loop() {
  server.handleClient();
  if (enrollMode && sensorConnected) {
    processEnrollment();
  }
}

// Handlers
void handleRoot() {
  sendCORSHeaders();
  String html = "<h1>ESP8266 Fingerprint Scanner</h1>";
  html += "<p>Sensor: " + String(sensorConnected ? "Connected" : "Not Connected") + "</p>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  if (sensorConnected) {
    html += "<p>Templates: " + String(finger.templateCount) + "</p>";
  }
  server.send(200, "text/html", html);
}

void handleStatus() {
  sendCORSHeaders();
  
  if (!sensorConnected) {
    sensorConnected = finger.verifyPassword();
  }
  
  StaticJsonDocument<300> doc;
  doc["sensorConnected"] = sensorConnected;
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["ip"] = WiFi.localIP().toString();
  
  if (sensorConnected) {
    finger.getTemplateCount();
    doc["templates"] = finger.templateCount;
    doc["capacity"] = finger.capacity;
  } else {
    doc["templates"] = 0;
    doc["capacity"] = 0;
    doc["error"] = "Sensor not connected";
  }
  doc["enrollMode"] = enrollMode;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  Serial.println("Status requested - Connected: " + String(sensorConnected));
}

void handleEnrollStart() {
  sendCORSHeaders();
  
  StaticJsonDocument<300> doc;
  
  if (!sensorConnected) {
    doc["success"] = false;
    doc["error"] = "Fingerprint sensor not connected";
  } else if (server.hasArg("plain")) {
    StaticJsonDocument<200> requestDoc;
    deserializeJson(requestDoc, server.arg("plain"));
    
    if (requestDoc.containsKey("fingerprintId")) {
      currentFingerprintId = requestDoc["fingerprintId"].as<String>();
      String idStr = currentFingerprintId;
      idStr.replace("FP_", "");
      id = idStr.toInt();
      
      if (id == 0 || id > 127) {
        id = finger.templateCount + 1;
      }
      
      uint8_t p = finger.loadModel(id);
      if (p == FINGERPRINT_OK) {
        doc["success"] = false;
        doc["error"] = "Fingerprint ID already exists";
        doc["existingId"] = id;
      } else {
        enrollMode = true;
        enrollStep = 0;
        enrollStatus = "place_finger";
        
        doc["success"] = true;
        doc["message"] = "Enrollment started";
        doc["status"] = "place_finger";
        doc["id"] = id;
        
        Serial.println("Enrollment started for ID: " + String(id));
      }
    } else {
      doc["success"] = false;
      doc["error"] = "Missing fingerprintId";
    }
  } else {
    doc["success"] = false;
    doc["error"] = "Invalid request - use POST with JSON body";
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleEnrollStatus() {
  sendCORSHeaders();
  
  StaticJsonDocument<300> doc;
  doc["enrollMode"] = enrollMode;
  doc["status"] = enrollStatus;
  doc["step"] = enrollStep;
  doc["id"] = id;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleEnrollCancel() {
  sendCORSHeaders();
  
  enrollMode = false;
  enrollStep = 0;
  enrollStatus = "cancelled";
  
  StaticJsonDocument<200> doc;
  doc["success"] = true;
  doc["message"] = "Enrollment cancelled";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleScan() {
  sendCORSHeaders();
  
  StaticJsonDocument<300> doc;
  
  if (!sensorConnected) {
    doc["success"] = false;
    doc["error"] = "Sensor not connected";
  } else {
    int result = getFingerprintID();
    if (result >= 0) {
      doc["success"] = true;
      doc["id"] = result;
      doc["confidence"] = finger.confidence;
      doc["message"] = "Fingerprint matched!";
    } else {
      doc["success"] = false;
      doc["message"] = "No match found";
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleDelete() {
  sendCORSHeaders();
  
  StaticJsonDocument<200> doc;
  
  if (!sensorConnected) {
    doc["success"] = false;
    doc["error"] = "Sensor not connected";
  } else if (server.hasArg("id")) {
    uint8_t deleteId = server.arg("id").toInt();
    uint8_t p = finger.deleteModel(deleteId);
    
    if (p == FINGERPRINT_OK) {
      doc["success"] = true;
      doc["message"] = "Deleted successfully";
    } else {
      doc["success"] = false;
      doc["error"] = "Delete failed";
    }
  } else {
    doc["success"] = false;
    doc["error"] = "Missing ID";
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleCheckFingerprint() {
  sendCORSHeaders();
  
  StaticJsonDocument<200> doc;
  
  if (!sensorConnected) {
    doc["exists"] = false;
    doc["error"] = "Sensor not connected";
  } else if (server.hasArg("id")) {
    uint8_t checkId = server.arg("id").toInt();
    uint8_t p = finger.loadModel(checkId);
    doc["exists"] = (p == FINGERPRINT_OK);
    doc["id"] = checkId;
  } else {
    doc["exists"] = false;
    doc["error"] = "Missing ID";
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetCount() {
  sendCORSHeaders();
  
  StaticJsonDocument<200> doc;
  
  if (sensorConnected) {
    finger.getTemplateCount();
    doc["count"] = finger.templateCount;
    doc["capacity"] = finger.capacity;
  } else {
    doc["count"] = 0;
    doc["capacity"] = 0;
    doc["error"] = "Sensor not connected";
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  sendCORSHeaders();
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
  } else {
    server.send(404, "text/plain", "Not Found - Available: /status, /enroll/start, /scan");
  }
}

void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// Enrollment process (same as your original)
void processEnrollment() {
  if (millis() - lastEnrollAttempt < 100) return;
  lastEnrollAttempt = millis();
  
  uint8_t p = 0;
  switch (enrollStep) {
    case 0:
      p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("Image 1 taken");
        p = finger.image2Tz(1);
        if (p == FINGERPRINT_OK) {
          enrollStatus = "remove_finger";
          enrollStep = 1;
        } else {
          enrollStatus = "error_converting";
        }
      } else if (p != FINGERPRINT_NOFINGER) {
        enrollStatus = "error_imaging";
      }
      break;
    case 1:
      p = finger.getImage();
      if (p == FINGERPRINT_NOFINGER) {
        Serial.println("Finger removed");
        enrollStatus = "place_again";
        enrollStep = 2;
      }
      break;
    case 2:
      p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("Image 2 taken");
        p = finger.image2Tz(2);
        if (p == FINGERPRINT_OK) {
          p = finger.createModel();
          if (p == FINGERPRINT_OK) {
            p = finger.storeModel(id);
            if (p == FINGERPRINT_OK) {
              Serial.println("Stored ID " + String(id));
              enrollStatus = "success";
              enrollMode = false;
              enrollStep = 0;
            } else {
              enrollStatus = "error_storing";
              enrollMode = false;
              enrollStep = 0;
            }
          } else if (p == FINGERPRINT_ENROLLMISMATCH) {
            enrollStatus = "mismatch";
            enrollMode = false;
            enrollStep = 0;
          } else {
            enrollStatus = "error_creating_model";
            enrollMode = false;
            enrollStep = 0;
          }
        } else {
          enrollStatus = "error_converting_2";
        }
      }
      break;
  }
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID;
  }
  
  return -1;
}
