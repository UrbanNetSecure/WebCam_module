#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <map>
#include <time.h>

// NTP 서버 및 시간대 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9; // 시간대 오프셋 (UTC+1)
const int daylightOffset_sec = 0; // 서머타임 오프셋

const char* ssid = "Hackathon";
const char* password = "hackathon2024";

WebServer server(80);

// Blocked IP List
const int MAX_BLOCKED_IPS = 10;
String blockedIPs[MAX_BLOCKED_IPS];
int blockedIPCount = 0;

// Login attempt tracking
const int MAX_ATTEMPTS = 40; // Maximum login attempts in 1 minute
const unsigned long BLOCK_DURATION = 180000; // Block duration (3 minutes)
const int MAX_FAILED_ATTEMPTS = 5; // Maximum failed attempts before IP blocking

unsigned long lastAttemptTime = 0;
int attemptCount = 0;
bool serverBlocked = false;

std::map<String, int> failedAttempts;
std::map<String, unsigned long> failedAttemptsTime;

String logBuffer = ""; // Buffer to store logs

// Server URL (insert your server URL)
const char* serverURL = "http://127.0.0.1:8000/log";

void logAccess(String clientIP, String url, String method, String status, String attackType) {
  // Get current time from NTP
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Fixed serial number
  String serialNumber = "F234c1";

  // Create formatted log string
  String logLine = String(timeStr) + "/" + clientIP + "/" + serialNumber + "/" + status + "/" + attackType;
  
  Serial.println(logLine);

  // Save log to buffer
  logBuffer += logLine + "\n";

  HTTPClient http;
  String serverName = serverURL;
  
  http.begin(serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "dateTime=" + String(timeStr) + "&clientIP=" + clientIP + "&serialNumber=" + serialNumber + "&url=" + url + "&method=" + method + "&status=" + status + "&attackType=" + attackType;

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server response: " + response);
  } else {
    Serial.println("Error on sending POST: " + String(httpResponseCode));
  }
  http.end();
}

void blockIP(String ip) {
  if (blockedIPCount < MAX_BLOCKED_IPS) {
    blockedIPs[blockedIPCount++] = ip;
    Serial.print("Blocked IP: ");
    Serial.println(ip);
  }
}

bool isIPBlocked(String ip) {
  for (int i = 0; i < blockedIPCount; i++) {
    if (blockedIPs[i] == ip) {
      return true;
    }
  }
  return false;
}

void handleFailedLogin(String clientIP) {
  unsigned long currentTime = millis();
  
  if (failedAttemptsTime.find(clientIP) == failedAttemptsTime.end() || 
      currentTime - failedAttemptsTime[clientIP] > 60000) {
    failedAttempts[clientIP] = 0;
    failedAttemptsTime[clientIP] = currentTime;
  }
  
  failedAttempts[clientIP]++;
  
  if (failedAttempts[clientIP] >= MAX_FAILED_ATTEMPTS) {
    blockIP(clientIP);
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "IP_Blocked", "Dos");
    failedAttempts.erase(clientIP);
    failedAttemptsTime.erase(clientIP);
  }
}

void checkDDoS() {
  unsigned long currentTime = millis();
  
  if (serverBlocked) {
    if (currentTime - lastAttemptTime > BLOCK_DURATION) {
      serverBlocked = false;
      Serial.println("Server login block lifted");
    }
  } else {
    if (currentTime - lastAttemptTime > 60000) { // Reset counter after 1 minute
      lastAttemptTime = currentTime;
      attemptCount = 0;
    }
    
    attemptCount++;
    
    if (attemptCount > MAX_ATTEMPTS) {
      serverBlocked = true;
      lastAttemptTime = currentTime; // Record block start time
      logAccess(server.client().remoteIP().toString(), server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Server Blocked", "DDoS");
      Serial.println("Server login blocked (DDoS attack detected)");
    }
  }
}

void handleLogin() {
  if (serverBlocked) {
    server.send(403, "text/plain", "Access denied due to high number of login attempts");
    return;
  }
  
  String clientIP = server.client().remoteIP().toString();

  if (isIPBlocked(clientIP)) {
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "IP blocked", "Dos");
    server.send(403, "text/plain", "Access from blocked IP");
    return;
  }

  logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Normal", "Login_Failed");

  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == "admin" && password == "password") {
      server.sendHeader("Location", "/cctv");
      server.send(302, "text/plain", "redirecting");
    } else {
      server.send(403, "text/plain", "login failed");
      handleFailedLogin(clientIP); // Block IP after exceeding failed login attempts
    }
  } else {
    server.send(400, "text/plain", "Required info missing");
  }

  checkDDoS(); // Check for DDoS attacks
}

void handleLog() {
  server.send(200, "text/plain", logBuffer);
}

void handleLoginPage() {
  String html = "<html>"
                "<head>"
                "<style>"
                "body {"
                "  font-family: Arial, sans-serif;"
                "  background-color: #f0f0f0;"
                "  display: flex;"
                "  justify-content: center;"
                "  align-items: center;"
                "  height: 100vh;"
                "  margin: 0;"
                "}"
                ".container {"
                "  background: white;"
                "  border-radius: 10px;"
                "  box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
                "  padding: 20px;"
                "  width: 300px;"
                "}"
                "h2 {"
                "  text-align: center;"
                "  color: #333;"
                "}"
                "input[type='text'], input[type='password'] {"
                "  width: 100%;"
                "  padding: 10px;"
                "  margin: 10px 0;"
                "  border: 1px solid #ddd;"
                "  border-radius: 5px;"
                "  box-sizing: border-box;"
                "}"
                "input[type='submit'] {"
                "  width: 100%;"
                "  padding: 10px;"
                "  border: none;"
                "  background-color: #4CAF50;"
                "  color: white;"
                "  border-radius: 5px;"
                "  cursor: pointer;"
                "  font-size: 16px;"
                "}"
                "input[type='submit']:hover {"
                "  background-color: #45a049;"
                "}"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h2>Login Page</h2>"
                "<form action='/login' method='POST'>"
                "<label for='username'>Username:</label>"
                "<input type='text' id='username' name='username' placeholder='Enter username' required><br>"
                "<label for='password'>Password:</label>"
                "<input type='password' id='password' name='password' placeholder='Enter password' required><br>"
                "<input type='submit' value='Login'>"
                "</form>"
                "</div>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Start server routes
  server.on("/", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/log", HTTP_GET, handleLog);

  server.begin();
}

void loop() {
  server.handleClient();
}
