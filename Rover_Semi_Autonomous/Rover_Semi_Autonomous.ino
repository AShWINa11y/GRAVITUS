#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <Servo.h>

// ==========================================
// WI-FI CREDENTIALS
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "YOUR_PASSWORD_HERE";

// ==========================================
// THRESHOLDS (The Override Engine)
// ==========================================
#define SAFE_DISTANCE       30.0
#define WARNING_DISTANCE    15.0
#define SAFE_TILT           25.0
#define WARNING_TILT        30.0
#define CRITICAL_TILT       35.0
#define MAX_TEMP            40.0
#define FULL_VOLTAGE        12.0
#define LOW_VOLTAGE         11.0
#define CRITICAL_VOLTAGE    10.0

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define F_ENA 10
#define F_IN1 2
#define F_IN2 3
#define F_ENB 11
#define F_IN3 4
#define F_IN4 5

#define R_ENA 12
#define R_IN1 14
#define R_IN2 15
#define R_ENB 13
#define R_IN3 16
#define R_IN4 17

#define TRIG_PIN 6
#define ECHO_PIN 7
#define BUZZER_PIN 8
#define VOLTAGE_PIN 26
#define DHTPIN 9
#define DHTTYPE DHT11
#define SERVO_PIN 19

// ==========================================
// GLOBAL OBJECTS & SYSTEM STATE
// ==========================================
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
Servo radarServo;

float currentDistance = 999.0;
float currentPitch = 0.0;
float currentVolt = 12.0;
float currentTemp = 25.0;

int system_status = 0; // 0=OK, 1=WARN, 2=CRITICAL
String alert_msg = "SYSTEM NOMINAL";

// Radar state
unsigned long lastRadarTime = 0;
int radarAngle = 0;
int radarDirection = 15;

// Sensor polling timer
unsigned long lastSensorTime = 0;

// ==========================================
// MOTOR CONTROL & OVERRIDE ENGINE
// ==========================================
void setMotorPWM(int leftSpeed, int rightSpeed) {
  analogWrite(F_ENA, leftSpeed);  analogWrite(R_ENA, leftSpeed);
  analogWrite(F_ENB, rightSpeed); analogWrite(R_ENB, rightSpeed);
}

void activeBrake() {
  // Electronic Braking: Setting all IN pins HIGH safely shorts the motor coils
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, HIGH);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, HIGH);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, HIGH);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, HIGH);
  setMotorPWM(255, 255); // Max braking force
}

void stopMotors() {
  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, LOW);
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
  setMotorPWM(0, 0);
}

void moveForward(int speed) {
  // OVERRIDE: Stop immediately if tilt is bad, temp is high, or obstacle is too close
  if (system_status == 2 || currentDistance <= WARNING_DISTANCE) {
    activeBrake();
    return; 
  }
  
  // OVERRIDE: Halve the speed if battery is low or minor tilt
  if (system_status == 1 || currentVolt <= LOW_VOLTAGE) {
    speed = speed / 2;
  }

  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(speed, speed);
}

void moveBackward(int speed) {
  // Obstacles in front don't prevent moving backward, but tilt/temp still do
  if (system_status == 2) { activeBrake(); return; }
  if (system_status == 1 || currentVolt <= LOW_VOLTAGE) speed = speed / 2;

  digitalWrite(F_IN1, LOW); digitalWrite(F_IN2, HIGH);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  digitalWrite(F_IN3, LOW); digitalWrite(F_IN4, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  setMotorPWM(speed, speed);
}

void turnRight(int speed) {
  if (system_status == 2) { activeBrake(); return; }
  int slowSpeed = speed * 0.4; 
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(speed, slowSpeed); 
}

void turnLeft(int speed) {
  if (system_status == 2) { activeBrake(); return; }
  int slowSpeed = speed * 0.4; 
  digitalWrite(F_IN1, HIGH); digitalWrite(F_IN2, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(F_IN3, HIGH); digitalWrite(F_IN4, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  setMotorPWM(slowSpeed, speed); 
}

// ==========================================
// BACKGROUND SENSOR POLLING & SAFETY LOGIC
// ==========================================
float pingDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float duration = pulseIn(ECHO_PIN, HIGH, 20000); 
  if (duration == 0) return 999.0;
  return (duration / 2.0) * 0.0343; 
}

void handle_radar() {
  if (millis() - lastRadarTime >= 150) {
    lastRadarTime = millis();
    currentDistance = pingDistance();
    
    radarAngle += radarDirection;
    if (radarAngle >= 180 || radarAngle <= 0) radarDirection = -radarDirection;
    radarServo.write(radarAngle);
  }
}

void updateSafetyState() {
  if (millis() - lastSensorTime >= 500) {
    lastSensorTime = millis();

    // 1. Read IMU
    sensors_event_t event;
    accel.getEvent(&event);
    currentPitch = atan2(event.acceleration.x, sqrt(event.acceleration.y * event.acceleration.y + event.acceleration.z * event.acceleration.z)) * 180.0 / PI;

    // 2. Read Voltage
    currentVolt = (analogRead(VOLTAGE_PIN) / 4095.0) * 3.3 * 5.0;

    // 3. Evaluate Thresholds (Highest severity wins)
    system_status = 0;
    alert_msg = "SYSTEM NOMINAL";
    digitalWrite(BUZZER_PIN, LOW);

    // Warnings (Level 1)
    if (abs(currentPitch) >= WARNING_TILT) { system_status = 1; alert_msg = "TILT WARNING"; }
    else if (currentVolt <= LOW_VOLTAGE) { system_status = 1; alert_msg = "LOW BATTERY"; }
    else if (currentDistance <= SAFE_DISTANCE && currentDistance > WARNING_DISTANCE) { system_status = 1; alert_msg = "OBSTACLE DETECTED"; }

    // Critical (Level 2)
    if (abs(currentPitch) >= CRITICAL_TILT) { system_status = 2; alert_msg = "CRITICAL TILT: MOTORS HALTED"; activeBrake(); digitalWrite(BUZZER_PIN, HIGH); }
    else if (currentVolt <= CRITICAL_VOLTAGE) { system_status = 2; alert_msg = "CRITICAL BATTERY: HALTED"; activeBrake(); }
    else if (currentDistance <= WARNING_DISTANCE) { system_status = 2; alert_msg = "COLLISION IMMINENT: BRAKING"; digitalWrite(BUZZER_PIN, HIGH); }
    // Note: Temp checking is skipped in this 500ms loop to avoid blocking, handled separately if needed.
  }
}

// ==========================================
// DASHBOARD CHUNKING ENGINE
// ==========================================
void send_html_chunk(int connectionId, String chunk) {
  Serial1.print("AT+CIPSEND=");
  Serial1.print(connectionId);
  Serial1.print(",");
  Serial1.println(chunk.length());
  if (Serial1.find(">")) {
    Serial1.print(chunk);
  }
  delay(15);
}

void serve_dashboard(int connectionId) {
  // Part 1: Head & Config
  String p1 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE html><html class='dark' lang='en'><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1.0'/><title>ARES-1 Control</title><script src='https://cdn.tailwindcss.com'></script><link href='https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:wght,FILL@100..700,0..1&display=swap' rel='stylesheet'/><style>.material-symbols-outlined{font-family:'Material Symbols Outlined';font-weight:normal;font-style:normal;font-size:24px;line-height:1;display:inline-block;white-space:nowrap;word-wrap:normal;direction:ltr;-webkit-font-smoothing:antialiased;}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;height:24px;width:16px;border-radius:2px;background:#00dbe9;cursor:pointer;margin-top:-8px;}input[type=range]::-webkit-slider-runnable-track{width:100%;height:8px;cursor:pointer;background:#3b494b;border-radius:2px;border:1px solid #111318;}</style>";
  
  // Part 2: Tailwind Config & Layout
  String p2 = "<script>tailwind.config={darkMode:'class',theme:{extend:{colors:{'surface-tint':'#00dbe9','surface-container-high':'#282a2f','surface-container':'#1e1f25','outline-variant':'#3b494b','surface':'#111318','error':'#ffb4ab'},fontFamily:{'label-caps':['monospace']}}}}</script></head><body class='bg-surface text-white min-h-screen flex flex-col font-sans selection:bg-surface-tint'><header class='w-full border-b border-outline-variant bg-surface/80 p-4'><h1 class='text-surface-tint tracking-widest uppercase font-bold'>ARES-1 Mission Control</h1></header><main class='flex-grow flex flex-col gap-6 p-4 pb-20'><section class='grid grid-cols-2 gap-2'><div class='bg-surface-container border border-outline-variant rounded p-3 border-l-2 border-l-surface-tint'><span class='text-xs text-gray-400 uppercase'>Voltage</span><br><span id='volt' class='text-xl text-surface-tint'>--</span></div><div class='bg-surface-container border border-outline-variant rounded p-3 border-l-2 border-l-surface-tint'><span class='text-xs text-gray-400 uppercase'>Temp</span><br><span id='temp' class='text-xl'>--</span></div>";
  
  // Part 3: Radar, Alerts, UI
  String p3 = "<div class='bg-surface-container border border-outline-variant rounded p-3 border-l-2 border-l-surface-tint'><span class='text-xs text-gray-400 uppercase'>Pitch</span><br><span id='pitch' class='text-xl'>--</span></div><div class='bg-surface-container border border-error rounded p-3 border-l-4 border-l-error'><span class='text-xs text-error uppercase'>Radar Distance</span><br><span id='dist' class='text-xl text-error'>--</span></div></section><section id='alert_box' class='bg-surface-container-high border border-outline-variant rounded p-4 flex items-center justify-between'><div class='flex flex-col gap-1'><span class='text-xs text-error uppercase flex items-center gap-2'><span class='w-2 h-2 rounded-full bg-error animate-pulse'></span>SYS_ALERTS</span><span id='sys_alerts' class='text-sm text-gray-400'>Initializing...</span></div></section>";
  
  // Part 4: Controls & Scripts
  String p4 = "<section class='bg-surface-container-high border border-outline-variant rounded-lg p-6 flex flex-col items-center gap-4 relative overflow-hidden'><h2 class='text-xs text-gray-400 tracking-widest'>Navigation</h2><div class='grid grid-cols-3 gap-2 w-full max-w-[240px]'><button onclick=\"cmd('L')\" class='col-start-1 row-start-2 p-4 bg-surface border border-outline-variant rounded'>A</button><button onclick=\"cmd('F')\" class='col-start-2 row-start-1 p-4 bg-surface border border-outline-variant rounded'>W</button><button onclick=\"cmd('BRK')\" class='col-start-2 row-start-2 p-4 bg-error text-black font-bold rounded'>BRAKE</button><button onclick=\"cmd('B')\" class='col-start-2 row-start-3 p-4 bg-surface border border-outline-variant rounded'>S</button><button onclick=\"cmd('R')\" class='col-start-3 row-start-2 p-4 bg-surface border border-outline-variant rounded'>D</button></div><input id='spd_slider' type='range' min='0' max='100' value='65' class='w-full mt-4'></section></main>";
  
  // Part 5: JS AJAX Loop
  String p5 = "<script>function cmd(dir){let s=document.getElementById('spd_slider').value; fetch('/action?dir='+dir+'&spd='+s);} setInterval(()=>{fetch('/data').then(r=>r.json()).then(d=>{document.getElementById('volt').innerHTML=d.v+' V'; document.getElementById('temp').innerHTML=d.t+' C'; document.getElementById('pitch').innerHTML=d.p+' DEG'; document.getElementById('dist').innerHTML=d.d+' CM @ '+d.a+'&deg;'; document.getElementById('sys_alerts').innerText=d.al; let ab=document.getElementById('alert_box'); if(d.st==2) ab.className='bg-error/20 border border-error rounded p-4 flex'; else if(d.st==1) ab.className='bg-yellow-900/40 border border-yellow-500 rounded p-4 flex'; else ab.className='bg-surface-container-high border border-outline-variant rounded p-4 flex';})}, 1500);</script></body></html>";

  send_html_chunk(connectionId, p1);
  send_html_chunk(connectionId, p2);
  send_html_chunk(connectionId, p3);
  send_html_chunk(connectionId, p4);
  send_html_chunk(connectionId, p5);
}

// ==========================================
// SERVER LOGIC
// ==========================================
String get_sensor_json() {
  // Occasionally read temp here so it doesn't block the fast safety loop
  currentTemp = dht.readTemperature();
  String tStr = isnan(currentTemp) ? "--" : String(currentTemp, 1);

  String json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";
  json += "{\"d\":\"" + String(currentDistance, 1) + "\",\"a\":\"" + String(radarAngle) + "\",\"v\":\"" + String(currentVolt, 1) + "\",";
  json += "\"t\":\"" + tStr + "\",\"p\":\"" + String(currentPitch, 1) + "\",\"st\":\"" + String(system_status) + "\",\"al\":\"" + alert_msg + "\"}";
  return json;
}

void handle_web_clients() {
  if (Serial1.find("+IPD,")) {
    int connectionId = Serial1.parseInt();
    String request = Serial1.readStringUntil('\n'); 

    if (request.indexOf("GET /data") >= 0) {
      String response = get_sensor_json();
      send_html_chunk(connectionId, response);
    } 
    else if (request.indexOf("GET /action") >= 0) {
      // Parse Web Speed (0-100) and map to PWM (0-255)
      int targetSpeed = 165; // Default ~65%
      int spdIndex = request.indexOf("spd=");
      if (spdIndex > 0) {
        int webSpeed = request.substring(spdIndex + 4, spdIndex + 7).toInt();
        targetSpeed = map(webSpeed, 0, 100, 0, 255);
      }

      if (request.indexOf("dir=F") >= 0) moveForward(targetSpeed);
      else if (request.indexOf("dir=B") >= 0) moveBackward(targetSpeed);
      else if (request.indexOf("dir=L") >= 0) turnLeft(targetSpeed); 
      else if (request.indexOf("dir=R") >= 0) turnRight(targetSpeed);
      else if (request.indexOf("dir=BRK") >= 0) activeBrake(); // Explicit manual brake
      
      String response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
      send_html_chunk(connectionId, response);
    } 
    else {
      serve_dashboard(connectionId);
    }

    Serial1.print("AT+CIPCLOSE="); 
    Serial1.println(connectionId);
  }
}

// ==========================================
// MAIN SETUP
// ==========================================
void setup() {
  Serial.begin(115200); Serial1.begin(115200); Serial1.setTimeout(1000); 
  
  Wire.setSDA(20); Wire.setSCL(21);
  Wire.begin();
  if(!accel.begin()) Serial.println("No ADXL345 detected.");
  
  pinMode(F_ENA, OUTPUT); pinMode(F_IN1, OUTPUT); pinMode(F_IN2, OUTPUT);
  pinMode(F_ENB, OUTPUT); pinMode(F_IN3, OUTPUT); pinMode(F_IN4, OUTPUT);
  pinMode(R_ENA, OUTPUT); pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  pinMode(R_ENB, OUTPUT); pinMode(R_IN3, OUTPUT); pinMode(R_IN4, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  radarServo.attach(SERVO_PIN);
  radarServo.write(90);
  
  analogReadResolution(12);
  dht.begin();

  while (!Serial) delay(10); 
  delay(2000); while(Serial1.available()) Serial1.read(); 

  Serial1.println("AT+CWMODE=1"); delay(500);
  while(Serial1.available()) Serial1.read();

  bool connected = false;
  while (!connected) {
    Serial.println("Connecting to Wi-Fi...");
    Serial1.print("AT+CWJAP=\""); Serial1.print(WIFI_SSID); Serial1.print("\",\"");
    Serial1.print(WIFI_PASSWORD); Serial1.println("\"");
    
    unsigned long startTimer = millis();
    String responseBuffer = "";
    
    while (millis() - startTimer < 10000) {
      if (Serial1.available()) {
        char c = Serial1.read();
        responseBuffer += c;
        Serial.write(c);
      }
      if (responseBuffer.indexOf("WIFI GOT IP") >= 0 || responseBuffer.indexOf("OK") >= 0) { connected = true; break; }
      if (responseBuffer.indexOf("FAIL") >= 0 || responseBuffer.indexOf("ERROR") >= 0) break;
    }
    if (!connected) delay(3000); 
  }

  Serial.println("\nFetching IP...");
  delay(2000); while (Serial1.available()) Serial1.read();
  
  Serial1.println("AT+CIFSR");
  unsigned long ipTimer = millis();
  while (millis() - ipTimer < 3000) { if (Serial1.available()) { Serial.write(Serial1.read()); ipTimer = millis(); } }
  
  Serial1.println("AT+CIPMUX=1"); delay(500); while(Serial1.available()) Serial1.read();
  Serial1.println("AT+CIPSERVER=1,80"); delay(500); while(Serial1.available()) Serial1.read();
  
  Serial.println("\nARES-1 Online!");
}

void loop() {
  updateSafetyState();
// 1. Constantly check for web commands as fast as possible (Non-blocking)
  handle_web_clients();
// 2. Automatically sweep the radar and ping (Non-blocking)
  handle_radar();
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