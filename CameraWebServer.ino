#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <map>
const char* ssid = "Hackathon";
const char* password = "hackathon2024";

WebServer server(80);

// Blocked IP List
const int MAX_BLOCKED_IPS = 10;
String blockedIPs[MAX_BLOCKED_IPS];
int blockedIPCount = 0;

// Login attempt tracking
const int MAX_ATTEMPTS = 40; // 1분 동안 최대 로그인 시도 횟수
const unsigned long BLOCK_DURATION = 180000; // 차단 유지 시간 (3분)
const int MAX_FAILED_ATTEMPTS = 5; // IP 차단을 위한 최대 실패 횟수

unsigned long lastAttemptTime = 0;
int attemptCount = 0;
bool serverBlocked = false;

std::map<String, int> failedAttempts;
std::map<String, unsigned long> failedAttemptsTime;

// Server URL (insert js servers url)
const char* serverURL = "http://127.0.0.1:8000/log";

void logAccess(String clientIP, String url, String method, String status) {
  Serial.print("접속 IP: ");
  Serial.println(clientIP);
  Serial.print("요청 URL: ");
  Serial.println(url);
  Serial.print("요청 메서드: ");
  Serial.println(method);
  Serial.print("상태: ");
  Serial.println(status);
  
  HTTPClient http;
  String serverName = "http://127.0.0.1:8080/log";
  
  http.begin(serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "clientIP=" + clientIP + "&url=" + url + "&method=" + method + "&status=" + status;

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
    Serial.print("차단된 IP: ");
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
    failedAttempts.erase(clientIP);
    failedAttemptsTime.erase(clientIP);
  }
}

void checkDDoS() {
  unsigned long currentTime = millis();
  
  if (serverBlocked) {
    if (currentTime - lastAttemptTime > BLOCK_DURATION) {
      serverBlocked = false;
      Serial.println("서버 로그인 차단 해제");
    }
  } else {
    if (currentTime - lastAttemptTime > 60000) { // 1분 지났다면 카운터 리셋
      lastAttemptTime = currentTime;
      attemptCount = 0;
    }
    
    attemptCount++;
    
    if (attemptCount > MAX_ATTEMPTS) {
      serverBlocked = true;
      lastAttemptTime = currentTime; // 차단 시작 시간 기록
      Serial.println("서버 로그인 차단됨 (DDoS 공격 감지)");
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
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "IP 차단됨");
    server.send(403, "text/plain", "Access from blocked IP");
    return;
  }

  logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "로그인 시도");

  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == "admin" && password == "password") {
      server.sendHeader("Location", "/cctv");
      server.send(302, "text/plain", "redirecting");
    } else {
      server.send(403, "text/plain", "login failed");
      handleFailedLogin(clientIP); // 로그인 실패가 일정 횟수 초과 시 IP 차단
    }
  } else {
    server.send(400, "text/plain", "Required info missing");
  }

  checkDDoS(); // DDoS 공격 체크
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
                ".container { max-width: 800px; margin: 30px auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); text-align: center; }"
                "h2 { color: #333; margin-bottom: 20px; }"
                "pre { font-family: monospace; white-space: pre-wrap; text-align: center; background-color: #f9f9f9; padding: 15px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }"
                "button { background-color: #4CAF50; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; transition: background-color 0.3s; }"
                "button:hover { background-color: #45a049; }"
                "</style></head><body>"
                "<header><h1>CCTV Admin Page</h1></header>"
                "<div class='container'>"
                "<h2>CCTV Live Feed</h2>"
                "<pre>Login to manage CCTV</pre>"
                "<button onclick=\"window.location.href='/login'\">Login</button>"
                "</div>"
                "</body></html>";
  server.send(200, "text/html", html);
}



void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi 연결됨");

  server.on("/", handleHomePage);
  server.on("/login", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/cctv", HTTP_GET, handleCCTV);

  server.begin();
  Serial.println("웹 서버 시작");

  Serial.print("웹 서버 주소: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
