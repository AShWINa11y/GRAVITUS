#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// ==========================================
// WI-FI CREDENTIALS
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

// ==========================================
// PIN DEFINITIONS
// ==========================================
// --- FRONT L298N ---
#define F_ENA 10 // Front Left PWM
#define F_IN1 2
#define F_IN2 3
#define F_ENB 11 // Front Right PWM
#define F_IN3 4
#define F_IN4 5

// --- REAR L298N ---
#define R_ENA 12 // Rear Left PWM
#define R_IN1 14
#define R_IN2 15
#define R_ENB 13 // Rear Right PWM
#define R_IN3 16
#define R_IN4 17

// --- SENSORS ---
#define TRIG_PIN 6
#define ECHO_PIN 7
#define BUZZER_PIN 8
#define VOLTAGE_PIN 26
#define DHTPIN 9
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

float currentPitch = 0.0;
float current_x = 0.0;
float current_y = 0.0;
float current_z = 0.0;
bool tippingHazard = false;

// ==========================================
// 4WD DIFFERENTIAL MOTOR CONTROL
// ==========================================
// Now accepts independent left and right speeds
void setMotorPWM(int leftSpeed, int rightSpeed) {
  analogWrite(F_ENA, leftSpeed); 
  analogWrite(R_ENA, leftSpeed);
  analogWrite(F_ENB, rightSpeed); 
  analogWrite(R_ENB, rightSpeed);
}

void stopMotors() {
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, LOW);
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
  setMotorPWM(0, 0);
}

void moveForward(int speed) {
  if (tippingHazard) return; 
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(speed, speed);
}

void moveBackward(int speed) {
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, HIGH);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  setMotorPWM(speed, speed);
}

// Sweeping Right Turn: Left wheels fast, Right wheels slow (Both Forward)
void turnRight(int speed) {
  if (tippingHazard) return;
  int slowSpeed = speed * 0.4; // Inner wheels run at 40% speed
  
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  
  setMotorPWM(speed, slowSpeed); 
}

// Sweeping Left Turn: Right wheels fast, Left wheels slow (Both Forward)
void turnLeft(int speed) {
  if (tippingHazard) return;
  int slowSpeed = speed * 0.4; // Inner wheels run at 40% speed
  
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  
  setMotorPWM(slowSpeed, speed); 
}

// ==========================================
// SENSOR READING
// ==========================================
float getDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return 999.0;
  return (duration / 2.0) * 0.0343; 
}

float getVoltage() {
  int adc = analogRead(VOLTAGE_PIN);
  float pinVoltage = (adc / 4095.0) * 3.3;
  return pinVoltage * 5.0; // Assuming 1:5 voltage divider
}

// ==========================================
// DASHBOARD & TELEMETRY
// ==========================================
String get_dashboard_html() {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial,text-align:center;background:#222;color:#fff;}";
  html += "button{padding:20px;margin:5px;font-size:20px;border-radius:10px;border:none;background:#007BFF;color:white;width:80px;}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:400px;margin:auto;}";
  html += ".card{background:#333;padding:15px;border-radius:10px;}";
  html += "input[type=range]{width: 80%; margin: 20px 0;}</style>";
  
  html += "<script>";
  html += "function cmd(dir) { var s = document.getElementById('spd').value; fetch('/action?dir=' + dir + '&spd=' + s); }";
  html += "setInterval(()=>{ fetch('/data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('dist').innerText = d.dist + ' cm';";
  html += "document.getElementById('volt').innerText = d.volt + ' V';";
  html += "document.getElementById('temp').innerText = d.temp + ' C';";
  html += "document.getElementById('imu').innerText = d.imu + ' deg';";
  html += "if(d.warn == 1) { document.getElementById('warn').innerText = 'TIPPING HAZARD!'; } else { document.getElementById('warn').innerText = ''; }";
  html += "})}, 2000);";
  html += "</script></head><body>";
  
  html += "<h2>Rover Command Center</h2>";
  html += "<h3 id='warn' style='color:red;'></h3>";
  
  html += "<div class='grid'>";
  html += "<div class='card'>Distance<br><b id='dist'>-- cm</b></div>";
  html += "<div class='card'>Battery<br><b id='volt'>-- V</b></div>";
  html += "<div class='card'>DHT Temp<br><b id='temp'>-- C</b></div>";
  html += "<div class='card'>Pitch<br><b id='imu'>-- deg</b></div>";
  html += "</div>";
  
  html += "<h3>Speed Control (PWM)</h3>";
  html += "<input type='range' id='spd' min='80' max='255' value='180'><br>";
  
  html += "<button onclick=\"cmd('F')\">W</button><br>";
  html += "<button onclick=\"cmd('L')\">A</button>";
  html += "<button onclick=\"cmd('S')\" style='background:#dc3545;'>O</button>";
  html += "<button onclick=\"cmd('R')\">D</button><br>";
  html += "<button onclick=\"cmd('B')\">S</button>";
  
  html += "</body></html>";
  return html;
}

String get_sensor_json() {
  // Read ADXL345 Pitch
  sensors_event_t event;
  accel.getEvent(&event);
  currentPitch = atan2(event.acceleration.x, sqrt(event.acceleration.y * event.acceleration.y + event.acceleration.z * event.acceleration.z)) * 180.0 / PI;
  int warning = 0;
  if (abs(currentPitch) > 35.0) {
    tippingHazard = true;
    warning = 1;
    stopMotors(); 
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    tippingHazard = false;
    digitalWrite(BUZZER_PIN, LOW);
  }

  float dist = getDistance(); 
  float volt = getVoltage();
  float temp = dht.readTemperature();
  String tempStr = isnan(temp) ? "--" : String(temp, 1);

  String json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";
  json += "{\"dist\":\"" + String(dist, 1) + "\",\"volt\":\"" + String(volt, 2) + "\",";
  json += "\"temp\":\"" + tempStr + "\",\"imu\":\"" + String(currentPitch, 1) + "\",\"warn\":\"" + String(warning) + "\"}";
  return json;
}

// ==========================================
// SERVER LOGIC
// ==========================================
void handle_web_clients() {
  if (Serial1.find("+IPD,")) {
    int connectionId = Serial1.parseInt();
    String request = Serial1.readStringUntil('\n'); 
    String response = "";

    if (request.indexOf("GET /data") >= 0) {
      response = get_sensor_json();
    } 
    else if (request.indexOf("GET /action") >= 0) {
      int targetSpeed = 180;
      int spdIndex = request.indexOf("spd=");
      if (spdIndex > 0) {
        targetSpeed = request.substring(spdIndex + 4, spdIndex + 7).toInt();
      }

      if (request.indexOf("dir=F") >= 0) moveForward(targetSpeed);
      else if (request.indexOf("dir=B") >= 0) moveBackward(targetSpeed);
      else if (request.indexOf("dir=L") >= 0) turnLeft(targetSpeed); 
      else if (request.indexOf("dir=R") >= 0) turnRight(targetSpeed);
      else if (request.indexOf("dir=S") >= 0) stopMotors();
      
      response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
    } 
    else {
      response = get_dashboard_html();
    }

    Serial1.print("AT+CIPSEND="); Serial1.print(connectionId); Serial1.print(","); Serial1.println(response.length());
    if (Serial1.find(">")) Serial1.print(response); 
    delay(10);
    Serial1.print("AT+CIPCLOSE="); Serial1.println(connectionId);
  }
}

// ==========================================
// MAIN SETUP & WI-FI LOOP
// ==========================================
void setup() {
  Serial.begin(115200); Serial1.begin(115200); Serial1.setTimeout(1000); 
  
  // I2C Setup for ADXL345
  Wire.setSDA(20); Wire.setSCL(21);
  Wire.begin();
  if(!accel.begin()) Serial.println("No ADXL345 detected. Check wiring!");
  
  // Set up Pins
  pinMode(F_ENA, OUTPUT); pinMode(F_IN1, OUTPUT); pinMode(F_IN2, OUTPUT);
  pinMode(F_ENB, OUTPUT); pinMode(F_IN3, OUTPUT); pinMode(F_IN4, OUTPUT);
  pinMode(R_ENA, OUTPUT); pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  pinMode(R_ENB, OUTPUT); pinMode(R_IN3, OUTPUT); pinMode(R_IN4, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  analogReadResolution(12);
  dht.begin();

  while (!Serial) delay(10); 
  Serial.println("\n--- Booting ESP8285 ---");
  delay(2000); 
  while(Serial1.available()) Serial1.read(); 

  Serial1.println("AT+CWMODE=1");
  delay(500);
  while(Serial1.available()) Serial1.read();

  bool connected = false;
  while (!connected) {
    Serial.println("\nAttempting to connect to Wi-Fi...");
    Serial1.print("AT+CWJAP=\"");
    Serial1.print(WIFI_SSID);
    Serial1.print("\",\"");
    Serial1.print(WIFI_PASSWORD);
    Serial1.println("\"");
    
    unsigned long startTimer = millis();
    String responseBuffer = "";
    
    while (millis() - startTimer < 10000) {
      if (Serial1.available()) {
        char c = Serial1.read();
        responseBuffer += c;
        Serial.write(c);
      }
      if (responseBuffer.indexOf("WIFI GOT IP") >= 0 || responseBuffer.indexOf("OK") >= 0) {
        connected = true; break;
      }
      if (responseBuffer.indexOf("FAIL") >= 0 || responseBuffer.indexOf("ERROR") >= 0) break;
    }
    if (!connected) {
      Serial.println("\nFailed. Retrying in 3 seconds...");
      delay(3000); 
    }
  }

  Serial.println("\n--- Connected! Fetching IP Address ---");
  delay(2000);
  while (Serial1.available()) Serial1.read();
  
  Serial1.println("AT+CIFSR");
  unsigned long ipTimer = millis();
  while (millis() - ipTimer < 3000) {
    if (Serial1.available()) {
      Serial.write(Serial1.read());
      ipTimer = millis(); 
    }
  }
  
  Serial1.println("AT+CIPMUX=1"); delay(500);
  while(Serial1.available()) Serial1.read();
  Serial1.println("AT+CIPSERVER=1,80"); delay(500);
  while(Serial1.available()) Serial1.read();
  
  Serial.println("\nRover Server is LIVE!");
}
// Add a timer variable right above your loop
unsigned long lastPrintTime = 0;

void loop() {
  // 1. Constantly check for web commands as fast as possible (Non-blocking)
  handle_web_clients();

  // 2. Print telemetry to the Serial Monitor every 2000 milliseconds (2 seconds)
  if (millis() - lastPrintTime >= 2000) {
    lastPrintTime = millis(); // Reset the stopwatch

    // Fetch the latest sensor readings
    float dist = getDistance();
    float volt = getVoltage();
    float temp = dht.readTemperature();
    String tempStr = isnan(temp) ? "--" : String(temp, 1);

    // Fetch the latest IMU reading
    sensors_event_t event;
    accel.getEvent(&event);
    currentPitch = atan2(event.acceleration.x, sqrt(event.acceleration.y * event.acceleration.y + event.acceleration.z * event.acceleration.z)) * 180.0 / PI;

    // Print everything in a clean, single-line format
    Serial.print("--- TELEMETRY | ");
    Serial.print("Dist: "); Serial.print(dist, 1); Serial.print(" cm | ");
    Serial.print("Volt: "); Serial.print(volt, 2); Serial.print(" V | ");
    Serial.print("Temp: "); Serial.print(tempStr); Serial.print(" C | ");
    Serial.print("Pitch: "); Serial.print(currentPitch, 1); Serial.print(" deg ");

    // Add the tipping hazard warning to the monitor
    if (abs(currentPitch) > 35.0) {
      Serial.println("| [WARNING: TIPPING HAZARD!]");
    } else {
      Serial.println(); // Just print a new line
    }
  }
}