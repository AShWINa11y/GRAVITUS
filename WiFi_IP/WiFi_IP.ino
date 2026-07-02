// ==========================================
// ENTER YOUR WI-FI CREDENTIALS HERE
// ==========================================
const String WIFI_SSID = "Ashwin's Galaxy";
const String WIFI_PASSWORD = "VIJJAYVIDASHVJ";

// Helper function to send an AT command and read the response
String send_at_command(String command, int delay_ms) {
  Serial.print("Sending: ");
  Serial.println(command);
  
  // Send the command to the ESP8285. println automatically adds the \r\n
  Serial1.println(command); 
  
  // Wait for the ESP to negotiate with the router
  delay(delay_ms);
  
  String response = "";
  // Read all incoming characters from the Wi-Fi chip
  while (Serial1.available()) {
    char c = Serial1.read();
    response += c;
  }
  
  Serial.println("Response:");
  Serial.println(response);
  return response;
}

void setup() {
  // Initialize the USB connection to your laptop
  Serial.begin(115200);
  
  // Initialize the hardware UART connection to the ESP8285 (TX: Pin 0, RX: Pin 1)
  Serial1.begin(115200);
  
  // Wait for the Serial Monitor to open before starting
  while (!Serial) delay(10); 
  
  Serial.println("\n\n--- RP2040 ESP8285 Arduino Wi-Fi Connection Test ---");
  Serial.println("--- Initializing Wi-Fi ---");
  
  // 1. Set to Station Mode (Client mode)
  send_at_command("AT+CWMODE=1", 1000);
  
  // 2. Connect to the router
  Serial.print("\nAttempting to connect to '");
  Serial.print(WIFI_SSID);
  Serial.println("'...");
  
  // Construct the join command using String concatenation
  String joinCmd = "AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASSWORD + "\"";
  String result = send_at_command(joinCmd, 8000);
  
  // 3. Verify connection
  // indexOf() searches our response String. If it doesn't find the phrase, it returns -1.
  if (result.indexOf("WIFI GOT IP") >= 0 || result.indexOf("OK") >= 0) {
    Serial.println("\n*** SUCCESS: Connected to Wi-Fi! ***");
    Serial.println("Fetching IP Address...");
    send_at_command("AT+CIFSR", 2000);
  } else {
    Serial.println("\n*** ERROR: Failed to connect. Please check your SSID and password. ***");
  }
}

void loop() {
  // The main loop is empty because we only need to connect once at startup.
  // We can add code here later to constantly check a web server or send sensor data!
}