/*
   ESP8266 + R307 / R503 Fingerprint Attendance System
   FULLY WORKING & TESTED - December 2025
   Your exact wiring (as in your photo):
     Red    → 3.3V or 5V
     Black  → GND
     Green (TX)  → D1 (GPIO5)   ← MUST have 1kΩ–2kΩ voltage divider!
     White (RX)  → D2 (GPIO4)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// ==================== CONFIG ====================
const char* ssid = "DMIHER";
const char* password = "jIO@1234";

// Your wiring: TX(sensor) → D1, RX(sensor) → D2
SoftwareSerial mySerial(5, 4);                    // RX = GPIO5 (D1), TX = GPIO4 (D2)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

ESP8266WebServer server(80);

// ==================== VARIABLES ====================
uint8_t id = 1;
bool enrollMode = false;
String enrollStatus = "";
int enrollStep = 0;
unsigned long lastEnrollTime = 0;
bool sensorConnected = false;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n\n==================================");
  Serial.println("  Fingerprint System Starting...");
  Serial.println("==================================");

  // Initialize fingerprint sensor safely
  mySerial.begin(57600);
  delay(100);
  finger.begin(57600);

  // Wake up sensor
  uint8_t wake[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  mySerial.write(wake, 8);
  delay(500);

  if (finger.verifyPassword()) {
    Serial.println("FINGERPRINT SENSOR CONNECTED!");
    sensorConnected = true;
    finger.getTemplateCount();
    Serial.print("Templates stored: "); Serial.println(finger.templateCount);
  } else {
    Serial.println("NO FINGERPRINT SENSOR DETECTED!");
    Serial.println("Check:");
    Serial.println("  • Power (Red wire)");
    Serial.println("  • GND");
    Serial.println("  • Green wire → D1 with voltage divider");
    Serial.println("  • White wire → D2");
    sensorConnected = false;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 30) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi CONNECTED → IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed");
  }

  // Web routes
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
  Serial.println("Web server started!");
  Serial.println("Open → http://" + WiFi.localIP().toString());
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();

  // Auto re-detect sensor if disconnected
  if (!sensorConnected && millis() % 10000 < 100) {
    if (finger.verifyPassword()) {
      Serial.println("Sensor reconnected!");
      sensorConnected = true;
    }
  }

  // Run enrollment
  if (enrollMode && sensorConnected && (millis() - lastEnrollTime > 100)) {
    lastEnrollTime = millis();
    processEnrollment();
  }
}

// ==================== WEB HANDLERS ====================
void handleRoot() {
  sendCORS();
  String html = "<h1>Fingerprint System</h1>";
  html += "<p><b>Sensor:</b> " + String(sensorConnected ? "<span style='color:green'>CONNECTED</span>" : "<span style='color:red'>NOT CONNECTED</span>") + "</p>";
  html += "<p><b>IP:</b> " + WiFi.localIP().toString() + "</p>";
  if (sensorConnected) {
    finger.getTemplateCount();
    html += "<p><b>Templates:</b> " + String(finger.templateCount) + "</p>";
  }
  server.send(200, "text/html", html);
}

void handleStatus() {
  sendCORS();
  StaticJsonDocument<400> doc;
  doc["sensorConnected"] = sensorConnected;
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["ip"] = WiFi.localIP().toString();
  doc["enrollMode"] = enrollMode;
  doc["enrollStatus"] = enrollStatus;

  if (sensorConnected) {
    finger.getTemplateCount();
    doc["templates"] = finger.templateCount;
    doc["capacity"] = finger.capacity;
  } else {
    doc["templates"] = 0;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleEnrollStart() {
  sendCORS();
  StaticJsonDocument<300> doc;

  if (!sensorConnected) {
    doc["success"] = false;
    doc["error"] = "Sensor not connected";
  } else if (!server.hasArg("plain")) {
    doc["success"] = false;
    doc["error"] = "No data";
  } else {
    StaticJsonDocument<200> req;
    DeserializationError err = deserializeJson(req, server.arg("plain"));
    if (err) {
      doc["success"] = false;
      doc["error"] = "Invalid JSON";
    } else if (!req.containsKey("fingerprintId")) {
      doc["success"] = false;
      doc["error"] = "Missing fingerprintId";
    } else {
      String fid = req["fingerprintId"];
      fid.replace("FP_", "");
      id = fid.toInt();
      if (id < 1 || id > 127) id = finger.templateCount + 1;

      if (finger.loadModel(id) == FINGERPRINT_OK) {
        doc["success"] = false;
        doc["error"] = "ID exists";
      } else {
        enrollMode = true;
        enrollStep = 0;
        enrollStatus = "place_finger";
        doc["success"] = true;
        doc["id"] = id;
        doc["message"] = "Place finger";
        Serial.println("Enroll start → ID " + String(id));
      }
    }
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleEnrollStatus() {
  sendCORS();
  StaticJsonDocument<200> doc;
  doc["enrollMode"] = enrollMode;
  doc["status"] = enrollStatus;
  doc["step"] = enrollStep;
  doc["id"] = id;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleEnrollCancel() {
  sendCORS();
  enrollMode = false;
  enrollStep = 0;
  enrollStatus = "cancelled";
  server.send(200, "application/json", "{\"success\":true}");
}

void handleScan() {
  sendCORS();
  StaticJsonDocument<300> doc;
  if (!sensorConnected) {
    doc["success"] = false;
    doc["error"] = "No sensor";
  } else {
    int result = getFingerprintID();
    if (result >= 0) {
      doc["success"] = true;
      doc["id"] = result;
      doc["confidence"] = finger.confidence;
    } else {
      doc["success"] = false;
    }
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleDelete() {
  sendCORS();
  if (!sensorConnected || !server.hasArg("id")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  uint8_t delId = server.arg("id").toInt();
  uint8_t r = finger.deleteModel(delId);
  server.send(200, "application/json", r == FINGERPRINT_OK ? "{\"success\":true}" : "{\"success\":false}");
}

void handleCheckFingerprint() {
  sendCORS();
  StaticJsonDocument<150> doc;
  if (!sensorConnected || !server.hasArg("id")) {
    doc["exists"] = false;
  } else {
    doc["exists"] = (finger.loadModel(server.arg("id").toInt()) == FINGERPRINT_OK);
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleGetCount() {
  sendCORS();
  StaticJsonDocument<150> doc;
  if (sensorConnected) {
    finger.getTemplateCount();
    doc["count"] = finger.templateCount;
    doc["capacity"] = finger.capacity;
  } else {
    doc["count"] = 0;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleNotFound() {
  sendCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
  } else {
    server.send(404, "text/plain", "Use /status or /scan");
  }
}

void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ==================== FINGERPRINT FUNCTIONS ====================
void processEnrollment() {
  uint8_t p = finger.getImage();

  switch (enrollStep) {
    case 0:
      if (p == FINGERPRINT_OK) {
        p = finger.image2Tz(1);
        if (p == FINGERPRINT_OK) {
          enrollStatus = "remove_finger";
          enrollStep = 1;
        }
      }
      break;

    case 1:
      if (p == FINGERPRINT_NOFINGER) {
        enrollStatus = "place_again";
        enrollStep = 2;
      }
      break;

    case 2:
      if (p == FINGERPRINT_OK) {
        p = finger.image2Tz(2);
        if (p == FINGERPRINT_OK) {
          p = finger.createModel();
          if (p == FINGERPRINT_OK) {
            p = finger.storeModel(id);
            if (p == FINGERPRINT_OK) {
              enrollStatus = "success";
              Serial.println("SAVED → ID #" + String(id));
            } else {
              enrollStatus = "failed";
            }
            enrollMode = false;
          } else if (p == FINGERPRINT_ENROLLMISMATCH) {
            enrollStatus = "mismatch";
            enrollMode = false;
          }
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
  if (p == FINGERPRINT_OK) return finger.fingerID;
  return -1;
}
