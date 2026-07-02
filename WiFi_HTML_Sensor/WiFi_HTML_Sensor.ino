// ==========================================
// WI-FI CREDENTIALS
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

// ==========================================
// PIN DEFINITIONS
// ==========================================
// Motors
#define IN1 2
#define IN2 3
#define IN3 4
#define IN4 5

// Sensors
#define TRIG_PIN 6
#define ECHO_PIN 7
#define BUZZER_PIN 8
#define VOLTAGE_PIN 26 // ADC0

// ==========================================
// ROVER CONTROL FUNCTIONS
// ==========================================
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void moveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void turnLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

// ==========================================
// SENSOR READING FUNCTIONS
// ==========================================
float getDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999.0;
  return (duration / 2.0) * 0.0343; // Convert to cm
}

float getVoltage() {
  int adc = analogRead(VOLTAGE_PIN);
  // Assuming a standard 25V voltage divider module (R1=30k, R2=7.5k) -> ratio is 5.0
  float pinVoltage = (adc / 4095.0) * 3.3;
  return pinVoltage * 5.0; 
}

// ==========================================
// DASHBOARD HTML & DATA (Minified to save RAM)
// ==========================================
String get_dashboard_html() {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial,text-align:center;background:#222;color:#fff;}";
  html += "button{padding:20px;margin:5px;font-size:20px;border-radius:10px;border:none;background:#007BFF;color:white;width:80px;}";
  html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:400px;margin:auto;}";
  html += ".card{background:#333;padding:15px;border-radius:10px;}</style>";
  html += "<script>";
  // JS function to send motor commands
  html += "function cmd(dir) { fetch('/action?dir=' + dir); }";
  // JS function to fetch sensor data every 2 seconds
  html += "setInterval(()=>{ fetch('/data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('dist').innerText = d.dist + ' cm';";
  html += "document.getElementById('volt').innerText = d.volt + ' V';";
  html += "document.getElementById('temp').innerText = d.temp + ' C';";
  html += "document.getElementById('imu').innerText = d.imu;";
  html += "})}, 2000);";
  html += "</script></head><body>";
  
  html += "<h2>Rover Command Center</h2>";
  
  // Sensor Cards
  html += "<div class='grid'>";
  html += "<div class='card'>Distance<br><b id='dist'>-- cm</b></div>";
  html += "<div class='card'>Battery<br><b id='volt'>-- V</b></div>";
  html += "<div class='card'>DHT Temp<br><b id='temp'>-- C</b></div>";
  html += "<div class='card'>IMU (Pitch)<br><b id='imu'>--</b></div>";
  html += "</div><br><br>";
  
  // D-Pad Controls
  html += "<button onclick=\"cmd('F')\">W</button><br>";
  html += "<button onclick=\"cmd('L')\">A</button>";
  html += "<button onclick=\"cmd('S')\" style='background:#dc3545;'>O</button>";
  html += "<button onclick=\"cmd('R')\">D</button><br>";
  html += "<button onclick=\"cmd('B')\">S</button>";
  
  html += "</body></html>";
  return html;
}

// Creates a JSON string of the sensors to send to the dashboard
String get_sensor_json() {
  float dist = getDistance();
  float volt = getVoltage();
  
  // If distance is too close, sound the buzzer!
  if (dist < 15.0) digitalWrite(BUZZER_PIN, HIGH);
  else digitalWrite(BUZZER_PIN, LOW);

  // Mock values for DHT and IMU (Replace these when you install the libraries)
  float temp = 24.5; 
  float imu_pitch = 5.2;

  String json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n";
  json += "{\"dist\":\"" + String(dist, 1) + "\",\"volt\":\"" + String(volt, 2) + "\",";
  json += "\"temp\":\"" + String(temp, 1) + "\",\"imu\":\"" + String(imu_pitch, 1) + "\"}";
  return json;
}

// ==========================================
// SERVER CONTROL LOGIC
// ==========================================
void handle_web_clients() {
  if (Serial1.find("+IPD,")) {
    int connectionId = Serial1.parseInt();
    String request = Serial1.readStringUntil('\n'); // Read the HTTP request

    String response = "";

    // 1. Check if the browser is asking for Sensor Data (JSON)
    if (request.indexOf("GET /data") >= 0) {
      response = get_sensor_json();
    } 
    // 2. Check if the browser is pressing a Motor Button
    else if (request.indexOf("GET /action") >= 0) {
      if (request.indexOf("dir=F") >= 0) moveForward();
      else if (request.indexOf("dir=B") >= 0) moveBackward();
      else if (request.indexOf("dir=L") >= 0) turnLeft();
      else if (request.indexOf("dir=R") >= 0) turnRight();
      else if (request.indexOf("dir=S") >= 0) stopMotors();
      
      // Send a tiny success response so the browser knows it worked
      response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
    } 
    // 3. Otherwise, serve the main HTML Dashboard
    else {
      response = get_dashboard_html();
    }

    // Send the response back to the ESP
    Serial1.print("AT+CIPSEND=");
    Serial1.print(connectionId);
    Serial1.print(",");
    Serial1.println(response.length());
    
    if (Serial1.find(">")) {
      Serial1.print(response); 
    }
    delay(10);
    Serial1.print("AT+CIPCLOSE=");
    Serial1.println(connectionId);
  }
}

// ==========================================
// MAIN SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200); 
  Serial1.setTimeout(1000); 
  
  // Set up Motor & Sensor Pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  analogReadResolution(12); // 12-bit ADC for Pico

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
  
  // Start Server
  Serial1.println("AT+CIPMUX=1"); delay(500);
  while(Serial1.available()) Serial1.read();
  Serial1.println("AT+CIPSERVER=1,80"); delay(500);
  while(Serial1.available()) Serial1.read();
  
  Serial.println("\nRover Server is LIVE!");
}

void loop() {
  handle_web_clients();
}