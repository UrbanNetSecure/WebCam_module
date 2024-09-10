#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <map>
#include <time.h>

//
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9;
const int daylightOffset_sec = 0;

const char* ssid = "Hackathon";
const char* password = "hackathon2024";

WebServer server(80);

// Blocked IP List
const int MAX_BLOCKED_IPS = 10;
String blockedIPs[MAX_BLOCKED_IPS];
int blockedIPCount = 0;

//
const int MAX_ATTEMPTS = 40;
const unsigned long BLOCK_DURATION = 180000;
const int MAX_FAILED_ATTEMPTS = 5;

unsigned long lastAttemptTime = 0;
int attemptCount = 0;
bool serverBlocked = false;

std::map<String, int>failedAttempts;
std::map<String, unsigned long> failedAttemptsTime;

String logBuffer = ""; // Buffer to store logs

//
//

void logAccess(String clientIP, String url, String method, String status, String attackType) {
  // 
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
  //String serverName = serverURL;
  
  //http.begin(serverName);
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
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "Dos");

    server.send(403, "text/plain", "Access from blocked IP");

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
    if (currentTime - lastAttemptTime > 1000) { // Reset counter after 1 minute
      lastAttemptTime = currentTime;
      attemptCount = 0;
    }
    
    attemptCount++;
    
    if (attemptCount >= 5) {
      serverBlocked = true;
      lastAttemptTime = currentTime; // Record block start time
      logAccess(server.client().remoteIP().toString(), server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "DDoS");
      //server.send(403, "text/plain", "Access from blocked IP");
      
      Serial.println("Server login blocked (DDoS attack detected)");

      // DDoS 탐지되면 DoS 처리를 중단
      return;
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
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "Dos");
    server.send(403, "text/plain", "Access from blocked IP");
    return;
  }

  //logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Normal", "Login_Failed");

  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == "admin" && password == "password") {
      logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Normal", "Login_Success");
      server.sendHeader("Location", "/cctv");
      server.send(302, "text/plain", "redirecting");
    } else {
      logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Normal", "Login_Failed");
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

void handleCCTV() {
  server.send(200, "text/html", "<html><body><h1>CCTV Screen</h1><img src=\"/camera\"> </body></html>");
}

void handleHomePage() {
  String html = "<html><head><style>"
                "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0; }"
                "header { background-color: #444; padding: 20px; color: white; text-align: center; }"
                "header h1 { margin: 0; font-size: 2em; }"
                ".container { max-width: 800px; margin: 30px auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }"
                "</style></head><body><header><h1>Home Page</h1></header>"
                "<div class='container'><h2>Welcome!</h2><p>This is the home page. Click <a href='/login'>here</a> to login.</p></div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  server.on("/", HTTP_GET, handleHomePage); 
  server.on("/login", HTTP_POST, handleLogin); 
  server.on("/login", HTTP_GET, handleLoginPage); 
  server.on("/cctv", HTTP_GET, handleCCTV);
  server.on("/log", HTTP_GET, handleLog);
  
  server.begin();
}

void loop() {
  server.handleClient();
}
