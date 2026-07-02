// ==========================================
// YOUR WI-FI CREDENTIALS
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

void setup() {
  // 1. Initialize laptop connection
  Serial.begin(115200);
  
  // 2. CRITICAL FIX: Initialize hardware connection to ESP8285
  Serial1.begin(115200); 
  
  while (!Serial) delay(10); 
  
  Serial.println("\n--- Minimal RP2040 ESP8285 Boot Sequence ---");
  Serial.println("Booting ESP8285...");
  delay(2000); 
  
  // Flush startup noise
  while(Serial1.available()) Serial1.read(); 

  // 3. Set Client Mode
  Serial1.println("AT+CWMODE=1");
  delay(500);

  // 4. Connect to Router
  Serial.println("Connecting to network...");
  // Sending the command in pieces saves us from building one massive String in memory
  Serial1.print("AT+CWJAP=\"");
  Serial1.print(WIFI_SSID);
  Serial1.print("\",\"");
  Serial1.print(WIFI_PASSWORD);
  Serial1.println("\"");
  
  // Wait a generous amount of time for the handshake
  delay(8000);
  
  // Flush the connection logs
  while(Serial1.available()) Serial1.read();

  // 5. Ask for the IP Address
  Serial.println("\n--- Fetching IP Address ---");
  Serial1.println("AT+CIFSR");
  delay(2000);
  
  // 6. Directly stream the result to the laptop (Zero memory buffer overhead!)
  while(Serial1.available()) {
    Serial.write(Serial1.read());
  }
  // 5. Ask for the IP Address
  Serial.println("\n--- Fetching IP Address ---");
  Serial1.println("AT+CIFSR");
  
  // 6. Read the response with a timeout (The Fix)
  // We will keep listening for 3000 milliseconds (3 seconds) to ensure the whole message arrives
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    if (Serial1.available()) {
      Serial.write(Serial1.read());
      
      // Reset the timer every time we receive a new character so it doesn't cut off early
      startTime = millis(); 
    }
  }
  
  Serial.println("\n--- Setup Complete ---");
}

void loop() {
  // Catch any lingering messages from the Wi-Fi chip
  while (Serial1.available()) {
    Serial.write(Serial1.read());
  }
  
  // Allow you to type commands manually in the Serial Monitor!
  while (Serial.available()) {
    Serial1.write(Serial.read());
  }
}
