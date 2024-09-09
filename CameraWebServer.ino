#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "****";
const char* password = "****$";

WebServer server(80);

//blocked IP list

const int MAX_BLOCKED_IPS = 10;
String blockedIPs [MAX_BLOCKED_IPS];
int blockedIPCount = 0;

// functions to save logs
void logAccess(String clientIP, String url, String method, String status) {
  Serial.print("접속 IP: ");
  Serial.println(clientIP);
  Serial.print("오청 URL: ");
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
    blockedIPs [blockedIPCount++] = ip;
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

void handleLogin() {
  String clientIP = server.client().remoteIP().toString();

  // IP가 차단된 경우 로그인 시도 차단
  if (isIPBlocked(clientIP)) {
    logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "IP BLOCKED");
    server.send(403, "text/plain", "차단된 IP에서의 접근입니다");
    return;
  }

  logAccess(clientIP, server.uri(), server.method() == HTTP_GET ? "GET" : "POST", "로그인 시도");

  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");

    // simple auth logic
    if(username == "admin" && password == "password") {
      server.send(200, "text/plain", "Login Success");
      //로그인 성공시 차단된 IP 목록 초기화 또는 다른 처리
    }
    else {
      server.send(403, "text/plain", "로그인 실패");
      blockIP(clientIP); // 로그인 실패가 일정 횟수 초과 시 IP 추가 가능
    }
  }
  else {
    server.send(400, "text/plain", "필요한 정보가 없습니다");
  }
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
    server.send(200, "text/plain", "<h2>web server running</h2><p><a href='/login'>move to login page </a></p>");
  });

  //login page ex
  server.on("/login", HTTP_POST, handleLogin);

  server.begin();
  Serial.println("웹 서버 시작");

  //print ip addr
  Serial.print("웹 서버 주소: http://");
  Serial.println(WiFi.localIP());
}
void loop() {
  server.handleClient();
}
