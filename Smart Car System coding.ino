#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- WiFi Credentials for Wokwi ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ---------------- PIN DEFINITIONS ----------------
#define KEY_PIN     27
#define BELT_PIN    25
#define HEART_PIN   34
#define PIR_PIN     14
#define SERVO_PIN   13
#define BUZZER_PIN  12
#define LED_PIN     2

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------- VARIABLES ----------------
Servo brakeServo;
unsigned long emergencyTimer = 0;
bool carActive = false;
bool isEmergency = false;

// ---------------- WIFI CONNECT ----------------
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(BELT_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // ---- SERVO SETUP ----
  ESP32PWM::allocateTimer(0);
  brakeServo.setPeriodHertz(50);
  brakeServo.attach(SERVO_PIN, 500, 2400);
  brakeServo.write(0); // Brake released
  delay(500);

  // ---- OLED ----
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("Connecting WiFi...");
  display.display();

  connectWiFi();

  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("INSERT CAR KEY");
  display.display();
}

void loop() {

  // 1. IGNITION
  if (digitalRead(KEY_PIN) == LOW) {
    carActive = true;
  }
  if (!carActive) return;

  // 2. READ SENSORS
  int bpm = map(analogRead(HEART_PIN), 0, 4095, 40, 180);
  bool beltFastened = (digitalRead(BELT_PIN) == LOW);
  bool motionDetected = digitalRead(PIR_PIN);

  // 3. BUZZER LOGIC
  if (!beltFastened || isEmergency) {
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  // 4. DMS LED
  if (!isEmergency) {
    digitalWrite(LED_PIN, motionDetected ? LOW : HIGH);
  }

  // 5. HEART RATE EMERGENCY
  if (bpm < 50 || bpm > 160) {
    if (emergencyTimer == 0) emergencyTimer = millis();

    if (millis() - emergencyTimer > 5000 && !isEmergency) {
      triggerEmergency(bpm);
    }
  } else {
    emergencyTimer = 0;
    if (isEmergency && bpm >= 60 && bpm <= 100) {
      resetSystem();
    }
  }

  if (!isEmergency) {
    updateDisplay(bpm, beltFastened, motionDetected);
  }

  delay(100);
}

// ---------------- EMERGENCY ----------------
void triggerEmergency(int hr) {
  isEmergency = true;

  brakeServo.attach(SERVO_PIN, 500, 2400);
  brakeServo.write(90);
  delay(500);

  digitalWrite(LED_PIN, HIGH);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("EMERGENCY");
  display.setCursor(10, 35);
  display.println("BRAKING");
  display.display();

  Serial.println("!!! SOS SEND TO GOOGLE !!!");
  Serial.println("EMERGENCY BRAKE APPLIED !!!");

  sendSOS(hr);
}

// ---------------- SOS ----------------
void sendSOS(int hr) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://httpbin.org/get?alert=HEART_ISSUE&bpm=" + String(hr);
    http.begin(url);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("SOS Sent. Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending SOS: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot send SOS.");
  }
}

// ---------------- RESET ----------------
void resetSystem() {
  isEmergency = false;
  emergencyTimer = 0;

  brakeServo.attach(SERVO_PIN, 500, 2400);
  brakeServo.write(0);
  delay(150);

  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  Serial.println("SYSTEM RESET: BRAKES RELEASED");
}

// ---------------- DISPLAY ----------------
void updateDisplay(int bpm, bool belt, bool motion) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("BPM: ");
  display.println(bpm);
  display.print("Belt: ");
  display.println(belt ? "SECURE" : "UNBUCKLED!");
  display.print("DMS: ");
  display.println(motion ? "ACTIVE" : "NO MOTION");
  display.display();
}
