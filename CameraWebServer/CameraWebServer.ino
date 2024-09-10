#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "****";
const char* password = "****";

WebServer server(80);

// Blocked IP List
const int MAX_BLOCKED_IPS = 10;
String blockedIPs[MAX_BLOCKED_IPS];
int blockedIPCount = 0;

// functions of log
void logAccess(String clientIP, String url, String method, String status) {
  Serial.print("접속 IP: ");
  Serial.println(clientIP);
  Serial.print("요청 URL: ");
  Serial.println(url);
  Serial.print("요청 메서드: ");
  Serial.println(method);
  Serial.print("상태: ");
  Serial.println(status);
  // 외부 서버로 로그 전송
}

// IP 차단 함수
void blockIP(String ip) {
  if (blockedIPCount < MAX_BLOCKED_IPS) {
    blockedIPs[blockedIPCount++] = ip;
    Serial.print("차단된 IP: ");
    Serial.println(ip);
  }
}

// IP 차단 체크 함수
bool isIPBlocked(String ip) {
  for (int i = 0; i < blockedIPCount; i++) {
    if (blockedIPs[i] == ip) {
      return true;
    }
  }
  return false;
}

// login form handler
void handleLogin() {
  String clientIP = server.client().remoteIP().toString();

  // if the ip is blocked, deny login
  if (isIPBlocked(clientIP)) {
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "IP 차단됨");
    server.send(403, "text/plain", "Access from blocked ip");
    return;
  }

  logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "로그인 시도");

  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    // simple access logic
    if (username == "admin" && password == "password") {
      server.sendHeader("Location", "/cctv"); //if login success, mov cctv page
      server.send(302, "text/plain", "redirecting");
    } else {
      server.send(403, "text/plain", "login failed");
      blockIP(clientIP); // 로그인 실패가 일정 횟수 초과 시 IP 차단 추가 가능
    }
  } else {
    server.send(400, "text/plain", "Required info missing");
  }
}

// login page handler
void handleLoginPage() {
  String html = "<html><body>"
                "<h2>Login page</h2>"
                "<form action='/login' method='POST'>"
                "user: <input type='text' name='username'><br>"
                "password: <input type='password' name='password'><br>"
                "<input type='submit' value='login'>"
                "</form>"
                "</body></html>";
  server.send(200, "text/html", html);
}

// cctv screen handler
void handleCCTV() {
  // CCTV 페이지를 제공하는 코드, 예를 들어 HTML 또는 이미지 데이터를 반환
  server.send(200, "text/html", "<html><body><h1>CCTV Screen</h1><img src=\"/camera\"> </body></html>");
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

  // root page
  server.on("/", []() {
    server.send(200, "text/plain", "<h2>Web server running</h2><p><a href='/login'>mov to login_page</a></p>");
  });

  // 로그인 페이지(GET 요청 처리)
  server.on("/login", HTTP_GET, handleLoginPage);

  // 로그인 처리(POST 요청 처리)
  server.on("/login", HTTP_POST, handleLogin);

  // CCTV 화면 처리
  server.on("/cctv", HTTP_GET, handleCCTV);

  server.begin();
  Serial.println("웹 서버 시작");

  // IP 주소 출력
  Serial.print("웹 서버 주소: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
