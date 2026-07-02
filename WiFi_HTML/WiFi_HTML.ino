// ==========================================
// YOUR WI-FI CREDENTIALS
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

// ==========================================
// DASHBOARD HTML FUNCTION
// ==========================================
// This function stores your web page. You can write any HTML/CSS here!
String get_dashboard_html() {
  // 1. The HTTP Header (Tells the browser "Here comes a webpage!")
  String html = "HTTP/1.1 200 OK\r\n";
  html += "Content-Type: text/html\r\n";
  html += "Connection: close\r\n\r\n";
  
  // 2. The HTML Payload
  html += "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>Pico Dashboard</title>";
  html += "</head><body style=\"font-family: Arial; text-align: center; margin-top: 50px;\">";
  
  html += "<h2>RP2040 Hardware Dashboard</h2>";
  html += "<h3 style=\"color: green;\">Status: Online</h3>";
  
  // The text you requested to indicate more needs to be added:
  html += "<div style=\"margin-top: 30px; padding: 20px; background-color: #f0f0f0; border-radius: 8px; display: inline-block;\">";
  html += "<p><strong>Placeholder:</strong> This dashboard is currently empty.</p>";
  html += "<p><em>Sensor readings, motor controls, and interface buttons need to be added here!</em></p>";
  html += "</div>";
  
  html += "</body></html>";
  return html;
}

// ==========================================
// SERVER CONTROL FUNCTIONS
// ==========================================
void setup_web_server() {
  Serial.println("\n--- Starting Web Server ---");
  
  // Enable multiple connections (Required for a web server)
  Serial1.println("AT+CIPMUX=1");
  delay(500);
  while(Serial1.available()) Serial.write(Serial1.read());
  
  // Start the server on Port 80 (Standard HTTP port)
  Serial1.println("AT+CIPSERVER=1,80");
  delay(500);
  while(Serial1.available()) Serial.write(Serial1.read());
  
  Serial.println("\nServer is LIVE! Type the IP address above into your phone/PC browser.");
}

void handle_web_clients() {
  // If the ESP receives data from a browser, it always starts with "+IPD,"
  if (Serial1.find("+IPD,")) {
    // Read the connection ID assigned to this browser request (usually 0, 1, 2, etc.)
    int connectionId = Serial1.parseInt();
    
    Serial.print("\n[+] Browser connected! Serving dashboard to ID: ");
    Serial.println(connectionId);
    
    // Fetch our HTML string
    String webpage = get_dashboard_html();
    
    // Tell the ESP exactly how many bytes of data we are about to send
    Serial1.print("AT+CIPSEND=");
    Serial1.print(connectionId);
    Serial1.print(",");
    Serial1.println(webpage.length());
    
    // The ESP will reply with a ">" symbol when it is ready to receive our HTML
    if (Serial1.find(">")) {
      Serial1.print(webpage); // Fire the HTML to the browser!
    }
    
    delay(10); // Brief pause to ensure transmission finishes
    
    // Close the connection so the browser finishes loading
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
  
  // Set a 1-second timeout for our find() commands so the loop doesn't freeze
  Serial1.setTimeout(1000); 
  
  while (!Serial) delay(10); 
  
  Serial.println("\n--- Booting ESP8285 ---");
  delay(2000); 
  while(Serial1.available()) Serial1.read(); 

  // Set Client Mode
  Serial1.println("AT+CWMODE=1");
  delay(500);
  while(Serial1.available()) Serial1.read();

  // --- AUTO-RECONNECT LOOP ---
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
    
    // Wait up to 10 seconds for the ESP to reply
    while (millis() - startTimer < 10000) {
      if (Serial1.available()) {
        char c = Serial1.read();
        responseBuffer += c;
        Serial.write(c); // Print progress to the monitor
      }
      
      // If we see success, break the timer loop
      if (responseBuffer.indexOf("WIFI GOT IP") >= 0 || responseBuffer.indexOf("OK") >= 0) {
        connected = true;
        break;
      }
      
      // If we see a hard failure, break the timer loop immediately
      if (responseBuffer.indexOf("FAIL") >= 0 || responseBuffer.indexOf("ERROR") >= 0) {
        break;
      }
    }
    
    if (!connected) {
      Serial.println("\nConnection failed. Retrying in 3 seconds...");
      delay(3000); // Wait 3 seconds before trying again
    }
  }

  // --- FETCH IP ADDRESS ---
  Serial.println("\n--- Connected! Fetching IP Address ---");
  
  // THE FIX: Wait 2 seconds for the ESP to finish its internal network setup
  delay(2000); 
  
  // Flush out any lingering "busy p..." or "OK" messages from the buffer
  while (Serial1.available()) Serial1.read();

  // NOW ask for the IP address
  Serial1.println("AT+CIFSR");
  
  unsigned long ipTimer = millis();
  while (millis() - ipTimer < 3000) {
    if (Serial1.available()) {
      Serial.write(Serial1.read());
      ipTimer = millis(); 
    }
  }
  
  // Start the server
  setup_web_server();
}

void loop() {
  // Constantly check if a browser is trying to load our page
  handle_web_clients();
}
