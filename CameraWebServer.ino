#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <map>
#include <time.h>

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9;
const int daylightOffset_sec = 0;

const char* ssid = "Hackathon";
const char* password = "hackathon2024";

WebServer server(80);

// Blocked IP List
const int MAX_BLOCKED_IPS = 10;
struct BlockedIP {
  unsigned long blockTime;
  unsigned long unblockTime;
};

std::map<String, BlockedIP> blockedIPList;

// DDoS 및 DoS 설정
const int MAX_ATTEMPTS_DDOS = 5;
const int MAX_ATTEMPTS_DOS = 5;
const unsigned long BLOCK_DURATION = 180000; // 3 minutes
const unsigned long DDOS_TIME_WINDOW = 1000; // 1 second
const unsigned long DDOS_STATE_DURATION = 60000; // 1 minute

unsigned long lastAttemptTime = 0;
int attemptCount = 0;
bool ddosDetected = false;
unsigned long ddosStartTime = 0;

std::map<String, int> failedAttempts;
std::map<String, unsigned long> failedAttemptsTime;
std::map<String, unsigned long> lastLoginAttemptTime;

String logBuffer = ""; // Buffer to store logs
const unsigned long LOG_SEND_INTERVAL = 60000; // 1 minute
unsigned long lastLogSendTime = 0;

void sendLogToServer() {
  if (logBuffer.length() == 0) {
    return;
  }

  HTTPClient http;
  http.begin("http://your-server-address/log"); // Replace with your server's URL
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "log=" + logBuffer;

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server response: " + response);
  } else {
    Serial.println("Error on sending POST: " + String(httpResponseCode));
  }
  http.end();
  logBuffer = ""; // Clear buffer after sending
}

void logAccess(String clientIP, String url, String method, String status, String attackType) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

  String serialNumber = "F234c1";
  String logLine = String(timeStr) + "/" + clientIP + "/" + serialNumber + "/" + status + "/" + attackType;

  Serial.println(logLine);

  logBuffer += logLine + "\n";
}

void blockIP(String ip) {
  if (blockedIPList.size() < MAX_BLOCKED_IPS) {
    unsigned long currentTime = millis();
    BlockedIP blockedIP;
    blockedIP.blockTime = currentTime;
    blockedIP.unblockTime = currentTime + BLOCK_DURATION; // 차단 해제 시간을 설정합니다.
    
    blockedIPList[ip] = blockedIP;
    Serial.print("Blocked IP: ");
    Serial.println(ip);
  }
}

bool isIPBlocked(String ip) {
  auto it = blockedIPList.find(ip);
  if (it != blockedIPList.end()) {
    unsigned long currentTime = millis();
    if (currentTime > it->second.unblockTime) {
      // 차단 해제 시간이 지난 경우
      blockedIPList.erase(it);
      return false;
    }
    return true;
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
  
  if (failedAttempts[clientIP] >= MAX_ATTEMPTS_DOS) {
    blockIP(clientIP);
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "Dos");

    // DoS 공격 감지 시에도 로그를 계속 전송
    server.send(403, "text/plain", "Access from blocked IP");

    failedAttempts.erase(clientIP);
    failedAttemptsTime.erase(clientIP);
  }
}

void checkDDoS(String clientIP) {
  unsigned long currentTime = millis();
  
  if (lastLoginAttemptTime.find(clientIP) != lastLoginAttemptTime.end()) {
    if (currentTime - lastLoginAttemptTime[clientIP] <= DDOS_TIME_WINDOW) {
      attemptCount++;
      if (attemptCount >= MAX_ATTEMPTS_DDOS) {
        ddosDetected = true;
        ddosStartTime = currentTime;
        logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "DDoS");
        Serial.println("Server login blocked (DDoS attack detected)");

        // Reset attempt count and last attempt time
        attemptCount = 0;
        lastLoginAttemptTime.erase(clientIP);
        return;
      }
    } else {
      attemptCount = 1; // Reset attempt count after time window
    }
  } else {
    attemptCount = 1; // Initialize attempt count
  }

  lastLoginAttemptTime[clientIP] = currentTime;
}

void handleLogin() {
  String clientIP = server.client().remoteIP().toString();

  if (isIPBlocked(clientIP)) {
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "Dos");
    server.send(403, "text/plain", "Access from blocked IP");
    return;
  }

  if (ddosDetected) {
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "DDoS");
    server.send(403, "text/plain", "Access denied due to high number of login attempts");
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastAttemptTime <= DDOS_TIME_WINDOW) {
    attemptCount++;
  } else {
    attemptCount = 1; // Initialize attempt count
  }

  lastAttemptTime = currentMillis;
  
  if (attemptCount >= MAX_ATTEMPTS_DDOS) {
    ddosDetected = true;
    ddosStartTime = currentMillis;
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "Attack", "DDoS");
    Serial.println("Server login blocked (DDoS attack detected)");
    return;
  }

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

  checkDDoS(clientIP); // Check for DDoS attacks
}

void handleLog() {
  server.send(200, "text/plain", logBuffer);
}

void handleCCTV() {
  // CCTV 페이지 처리 코드 추가
  server.send(200, "text/html", "<html><body><h1>CCTV Feed</h1></body></html>");
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
                "input[type=text], input[type=password] {"
                "  width: 100%;"
                "  padding: 10px;"
                "  margin: 10px 0;"
                "  border: 1px solid #ddd;"
                "  border-radius: 5px;"
                "}"
                "input[type=submit] {"
                "  width: 100%;"
                "  padding: 10px;"
                "  border: none;"
                "  border-radius: 5px;"
                "  background-color: #4CAF50;"
                "  color: white;"
                "  font-size: 16px;"
                "}"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h2>Login</h2>"
                "<form method='POST' action='/login'>"
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

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/cctv", HTTP_GET, handleCCTV);

  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogSendTime >= LOG_SEND_INTERVAL) {
    sendLogToServer();
    lastLogSendTime = currentMillis;
  }

  if (ddosDetected && millis() - ddosStartTime > DDOS_STATE_DURATION) {
    ddosDetected = false;
    Serial.println("DDoS state expired. Returning to normal operation.");
  }
}
