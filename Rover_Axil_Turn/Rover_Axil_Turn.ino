#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

// --- FRONT L298N ---
#define F_ENA 10 // Left PWM
#define F_IN1 2
#define F_IN2 3
#define F_ENB 11 // Right PWM
#define F_IN3 4
#define F_IN4 5

// --- REAR L298N ---
#define R_ENA 12 // Left PWM
#define R_IN1 14
#define R_IN2 15
#define R_ENB 13 // Right PWM
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
bool tippingHazard = false;

// ==========================================
// 4WD PWM MOTOR CONTROL
// ==========================================
void setMotorPWM(int speed) {
  analogWrite(F_ENA, speed); analogWrite(F_ENB, speed);
  analogWrite(R_ENA, speed); analogWrite(R_ENB, speed);
}

void stopMotors() {
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, LOW);
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
  setMotorPWM(0);
}

void moveForward(int speed) {
  if (tippingHazard) return; // Prevent driving if flipped
  
  // Left Side Forward
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  // Right Side Forward
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(speed);
}

void moveBackward(int speed) {
  // Left Side Backward
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, HIGH);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  // Right Side Backward
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  setMotorPWM(speed);
}

// Point Turn: Left wheels forward, Right wheels backward
void turnRight(int speed) {
  if (tippingHazard) return;
  // Left Forward
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  // Right Backward
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  
  // Applying equal PWM to all 4 wheels eliminates longitudinal slip
  setMotorPWM(speed); 
}

// Point Turn: Left wheels backward, Right wheels forward
void turnLeft(int speed) {
  if (tippingHazard) return;
  // Left Backward
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, HIGH);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  // Right Forward
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(speed);
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
  // JS now reads the slider value and sends it with the direction command
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
  // Read ADXL345
  sensors_event_t event;
  accel.getEvent(&event);
  
  // Calculate Pitch using basic trigonometry
  currentPitch = atan2(event.acceleration.x, sqrt(event.acceleration.y * event.acceleration.y + event.acceleration.z * event.acceleration.z)) * 180.0 / PI;
  
  // Safety cutoff logic
  int warning = 0;
  if (abs(currentPitch) > 35.0) {
    tippingHazard = true;
    warning = 1;
    stopMotors(); // Instantly kill motors if tilted too far
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    tippingHazard = false;
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Get other sensors
  float dist = 25.0; // Mocked for brevity in this example, replace with your ping code
  float volt = (analogRead(VOLTAGE_PIN) / 4095.0) * 3.3 * 5.0;
  float temp = dht.readTemperature();
  String tempStr = isnan(temp) ? "--" : String(temp, 1);

  String json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";
  json += "{\"dist\":\"" + String(dist, 1) + "\",\"volt\":\"" + String(volt, 2) + "\",";
  json += "\"temp\":\"" + tempStr + "\",\"imu\":\"" + String(currentPitch, 1) + "\",\"warn\":\"" + String(warning) + "\"}";
  return json;
}

void handle_web_clients() {
  if (Serial1.find("+IPD,")) {
    int connectionId = Serial1.parseInt();
    String request = Serial1.readStringUntil('\n'); 
    String response = "";

    if (request.indexOf("GET /data") >= 0) {
      response = get_sensor_json();
    } 
    else if (request.indexOf("GET /action") >= 0) {
      // Parse the speed from the URL (e.g. /action?dir=F&spd=200)
      int targetSpeed = 150; // Default fallback
      int spdIndex = request.indexOf("spd=");
      if (spdIndex > 0) {
        targetSpeed = request.substring(spdIndex + 4, spdIndex + 7).toInt();
      }

      if (request.indexOf("dir=F") >= 0) moveForward(targetSpeed);
      else if (request.indexOf("dir=B") >= 0) moveBackward(targetSpeed);
      else if (request.indexOf("dir=L") >= 0) turnLeft(targetSpeed); // Turns use the same speed mapping to prevent drag
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

void setup() {
  Serial.begin(115200); Serial1.begin(115200); Serial1.setTimeout(1000); 
  
  // I2C Setup for ADXL345
  Wire.setSDA(20); Wire.setSCL(21);
  Wire.begin();
  if(!accel.begin()) {
    Serial.println("No ADXL345 detected.");
  }
  
  // Set up Motor Pins
  pinMode(F_ENA, OUTPUT); pinMode(F_IN1, OUTPUT); pinMode(F_IN2, OUTPUT);
  pinMode(F_ENB, OUTPUT); pinMode(F_IN3, OUTPUT); pinMode(F_IN4, OUTPUT);
  pinMode(R_ENA, OUTPUT); pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  pinMode(R_ENB, OUTPUT); pinMode(R_IN3, OUTPUT); pinMode(R_IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  analogReadResolution(12);
  dht.begin();

  // (Include the standard Wi-Fi connection loop here that we wrote previously)
}

void loop() {
  handle_web_clients();
}