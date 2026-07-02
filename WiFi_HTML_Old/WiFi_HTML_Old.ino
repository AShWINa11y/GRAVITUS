#include <Arduino.h>

// Define Wi-Fi Credentials
const char* ssid     = "Ashwin's Galaxy";
const char* password = "VIJJAYVIDASHVJ";

// Use Serial1 (GP0 as TX, GP1 as RX) for communicating with the ESP8285
#define EspSerial Serial1 
const int onboardLED = LED_BUILTIN; // Pico Onboard LED Pin

// Helper function to send AT commands and wait for a specific response
bool sendATCommand(String cmd, const char* expectedResponse, int timeoutMs) {
  EspSerial.println(cmd);
  long int time = millis();
  String response = "";
  
  while ((millis() - time) < timeoutMs) {
    while (EspSerial.available()) {
      char c = EspSerial.read();
      response += c;
    }
    if (response.indexOf(expectedResponse) != -1) {
      return true; // Found the expected response (e.g., "OK")
    }
  }
  return false; // Timed out
}

void setup() {
  // Initialize standard hardware Serial for debugging via PC USB
  Serial.begin(115200);
  pinMode(onboardLED, OUTPUT);
  digitalWrite(onboardLED, LOW);

  // Initialize UART1 communication with the ESP8285 (Default baud rate is 115200)
  EspSerial.begin(115200);
  delay(2000); // Give the module time to stabilize
  
  Serial.println("Initializing ESP8285...");

  // 1. Reset the ESP8285 chip
  sendATCommand("AT+RST", "OK", 2000);
  delay(3000);

  // 2. Set Wi-Fi mode to Station (Client mode)
  if (!sendATCommand("AT+CWMODE=1", "OK", 2000)) {
    Serial.println("Failed to set Wi-Fi mode.");
  }

  // 3. Connect to your local Wi-Fi network
  String joinCmd = "AT+CWJAP=\"" + String(ssid) + "\",\"" + String(password) + "\"";
  Serial.println("Connecting to Wi-Fi...");
  if (sendATCommand(joinCmd, "OK", 10000)) {
    Serial.println("Connected to Wi-Fi successfully!");
  } else {
    Serial.println("Wi-Fi connection failed.");
  }

  // 4. Query and print the IP address obtained by the ESP8285
  EspSerial.println("AT+CIFSR");
  delay(1000);
  while (EspSerial.available()) {
    Serial.write(EspSerial.read()); // Prints the IP address to the PC Serial Monitor
  }

  // 5. Enable multiple TCP connections (Required to run a web server)
  sendATCommand("AT+CIPMUX=1", "OK", 2000);

  // 6. Start the HTTP Web Server on Port 80
  if (sendATCommand("AT+CIPSERVER=1,80", "OK", 2000)) { //
    Serial.println("Server started! Enter the IP address into your browser.");
  } else {
    Serial.println("Failed to start server.");
  }
}

// Function to construct and serve the HTML dashboard page
void sendHTTPResponse(int linkId) {
  String html = "<!DOCTYPE html><html><head><title>Pico Control</title></head>";
  html += "<body style=\"text-align:center; font-family:sans-serif; margin-top:50px;\">";
  html += "<h1>Raspberry Pi Pico + ESP8285 Browser Control</h1>";
  html += "<p><a href=\"/light/on\"><button style=\"padding:20px; font-size:20px; background-color:green; color:white;\">Turn ON</button></a></p>";
  html += "<p><a href=\"/light/off\"><button style=\"padding:20px; font-size:20px; background-color:red; color:white;\">Turn OFF</button></a></p>";
  html += "</body></html>";

  String httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + String(html.length()) + "\r\nConnection: close\r\n\r\n" + html;

  // AT+CIPSEND=<link_id>,<length> tells ESP to prepare for transmitting data
  String sendCmd = "AT+CIPSEND=" + String(linkId) + "," + String(httpResponse.length());
  if (sendATCommand(sendCmd, ">", 2000)) {
    EspSerial.print(httpResponse); // Feed the raw string to the ESP to push out over Wi-Fi
    delay(100);
  }
}

void loop() {
  // Check if the ESP8285 is routing data to the Pico
  if (EspSerial.available()) {
    // Look for the incoming stream identification string "+IPD,"
    if (EspSerial.find("+IPD,")) {
      delay(100);
      
      // Parse out the connection ID and the actual raw HTTP request data
      int linkId = EspSerial.read() - 48; // Convert ASCII digit to integer
      String request = EspSerial.readStringUntil('\n');
      
      Serial.println("Incoming Request: " + request);

      // Route evaluation matching the requested URL
      if (request.indexOf("/light/on") != -1) {
        digitalWrite(onboardLED, HIGH); // Hardware ON
        Serial.println("LED turned ON");
      } 
      else if (request.indexOf("/light/off") != -1) {
        digitalWrite(onboardLED, LOW);  // Hardware OFF
        Serial.println("LED turned OFF");
      }

      // Serve the interface web page back to the requesting browser
      sendHTTPResponse(linkId);
    }
  }
}
