// Attendance.cpp
#include "Attendance.h"
#include <cstring>

// onboard status LED when reading Google sheet
#define On_Board_LED_PIN 2

Attendance::Attendance(const char* ssid,
                       const char* password,
                       const String& url,
                       U8G2_SSD1306_128X64_NONAME_F_HW_I2C& display,
                       MFRC522& rfid,
                       int btnIO,
                       int ledIO,
                       int btnCreate,
                       int ledCreate,
                       int buzzerPin)
  : _ssid(ssid),
    _password(password),
    _webAppUrl(url),
    _display(display),
    _rfid(rfid),
    _btnIO(btnIO),
    _ledIO(ledIO),
    _btnCreate(btnCreate),
    _ledCreate(ledCreate),
    _buzzer(buzzerPin),
    _studentCount(0),
    _uidString(""),
    _mode(ATTENDANCE_LOG),
    _btnIOState(HIGH),
    _ioDebounce(0),
    _inOutState(false),
    _btnCreateState(HIGH),
    _createDebounce(0),
    _lastUIDRead(0)
{}

bool Attendance::begin() {
  pinMode(_buzzer, OUTPUT);
  digitalWrite(_buzzer, LOW);

  pinMode(_btnIO, INPUT_PULLUP);
  pinMode(_ledIO, OUTPUT);

  pinMode(_btnCreate, INPUT_PULLUP);
  pinMode(_ledCreate, OUTPUT);

  _display.begin();
  _display.clearBuffer();
  _display.setFont(u8g2_font_ncenB08_tr);
  _display.drawStr(25, 30, "INSERT YOUR");
  _display.drawStr(30, 50, "CARD HERE");
  _display.sendBuffer();

  Serial.println("\n-------------");
  Serial.println("WIFI mode : STA");
  Serial.println("-------------");
  Serial.print("Connecting to "); Serial.println(_ssid);
  WiFi.begin(_ssid, _password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(1000);
  }

  if (!readDataSheet())
    Serial.println("Can't read data from google sheet!");

  SPI.begin();
  _rfid.PCD_Init();
  return true;
}

void Attendance::loop() {
  unsigned long now = millis();

  if (now - _lastUIDRead > 500) {
    readUID();
    _lastUIDRead = now;
  }

  // toggle check-in / check-out
  if (digitalRead(_btnIO) == LOW) {
    if (_btnIOState == HIGH && now - _ioDebounce > 500) {
      _inOutState = !_inOutState;
      digitalWrite(_ledIO, _inOutState);
      _ioDebounce = now;
    }
    _btnIOState = LOW;
  } else {
    _btnIOState = HIGH;
  }

  // enter “add new card” mode
  if (digitalRead(_btnCreate) == LOW) {
    if (_btnCreateState == HIGH && now - _createDebounce > 500) {
      _mode = CREATE_NEW_CARD;
      digitalWrite(_ledCreate, HIGH);
      _createDebounce = now;
    }
    _btnCreateState = LOW;
  } else {
    _btnCreateState = HIGH;
  }
}

void Attendance::readUID() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if (!_rfid.PICC_IsNewCardPresent()) return;
  if (!_rfid.PICC_ReadCardSerial())    return;

  _uidString = "";
  for (byte i = 0; i < _rfid.uid.size; i++) {
    if (_rfid.uid.uidByte[i] < 0x10) _uidString.concat("0");
    _uidString.concat(String(_rfid.uid.uidByte[i], HEX));
  }
  _uidString = _uidString.substring(1, _uidString.length() - 4);
  _uidString.toUpperCase();
  Serial.println("Card UID: " + _uidString);

  beep(1, 200);
  if      (_mode == WRITE_UID)       writeUIDSheet();
  else if (_mode == ATTENDANCE_LOG) writeLogSheet();
  else if (_mode == CREATE_NEW_CARD) writeNewCardSheet();

  byte piccType = _rfid.PICC_GetType(_rfid.uid.sak);
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
   && piccType != MFRC522::PICC_TYPE_MIFARE_1K
   && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    return;
  }
}

bool Attendance::readDataSheet() {
  if (WiFi.status() != WL_CONNECTED) return false;
  digitalWrite(On_Board_LED_PIN, HIGH);

  String url = _webAppUrl + "?sts=read";
  Serial.println("\n-------------\nRead data from Google Spreadsheet...");
  Serial.println("URL : " + url);

  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  Serial.print("HTTP Status Code : "); Serial.println(httpCode);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Payload : " + payload);
    char buf[payload.length() + 1];
    payload.toCharArray(buf, sizeof(buf));
    int numEls = countElements(buf, ',');
    char* token = strtok(buf, ",");
    _studentCount = 0;
    while (token && _studentCount < numEls/3) {
      _students[_studentCount].id = token;
      token = strtok(NULL, ",");
      strcpy(_students[_studentCount].code, token);
      token = strtok(NULL, ",");
      strcpy(_students[_studentCount].name, token);
      _studentCount++;
      token = strtok(NULL, ",");
    }
    for (int i = 0; i < _studentCount; i++) {
      Serial.print("ID: ");   Serial.println(_students[i].id);
      Serial.print("Code: "); Serial.println(_students[i].code);
      Serial.print("Name: "); Serial.println(_students[i].name);
    }
  }
  http.end();
  digitalWrite(On_Board_LED_PIN, LOW);
  return _studentCount > 0;
}

void Attendance::writeUIDSheet() {
  String url = _webAppUrl + "?sts=writeuid&uid=" + _uidString;
  Serial.println("\n-------------\nSend data to Google Spreadsheet...");
  Serial.println("URL : " + url);
  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = http.GET();
  Serial.print("HTTP Status Code : "); Serial.println(code);
  if (code > 0) Serial.println("Payload : " + http.getString());
  http.end();
}

void Attendance::writeLogSheet() {
  char buf[_uidString.length() + 1];
  _uidString.toCharArray(buf, sizeof(buf));
  char* studentName = getStudentNameById(buf);
  if (!studentName) {
    Serial.print("Không tìm thấy học sinh với ID "); Serial.println(_uidString);
    beep(3, 500);
    return;
  }

  String url = _webAppUrl + "?sts=writelog";
  url += "&uid=" + _uidString;
  url += "&name=" + urlencode(String(studentName));
  if (!_inOutState) {
    url += "&inout=" + urlencode("VÀO");
    _display.clearBuffer();
    _display.drawStr(25,30,"CHECK-IN");
    _display.drawStr(25,50,"COMPLETE");
    _display.sendBuffer();
  } else {
    url += "&inout=" + urlencode("RA");
    _display.clearBuffer();
    _display.drawStr(25,30,"CHECK-OUT");
    _display.drawStr(25,50,"COMPLETE");
    _display.sendBuffer();
  }
  delay(1000);
  _display.clearBuffer();
  _display.drawStr(25,30,"INSERT YOUR");
  _display.drawStr(30,50,"CARD HERE");
  _display.sendBuffer();

  Serial.println("\n-------------\nSend data to Google Spreadsheet...");
  Serial.println("URL : " + url);
  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = http.GET();
  Serial.print("HTTP Status Code : "); Serial.println(code);
  if (code > 0) Serial.println("Payload : " + http.getString());
  http.end();
}

void Attendance::writeNewCardSheet() {
  String url = _webAppUrl + "?sts=newcard&uid=" + _uidString;
  Serial.println("\n-------------\nCreate-mode URL: " + url);
  HTTPClient http;
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = http.GET();
  String payload = (code > 0 ? http.getString() : "");
  Serial.printf("HTTP %d – %s\n", code, payload.c_str());

  _display.clearBuffer();
  if (payload.indexOf("ALREADY EXIST") >= 0) {
    _display.drawStr(10,30,"ALREADY EXIST");
  } else {
    _display.drawStr(15,30,"ADDED TO SHEET");
  }
  _display.sendBuffer();
  delay(1000);

  _mode = ATTENDANCE_LOG;
  digitalWrite(_ledCreate, LOW);
  _display.clearBuffer();
  _display.drawStr(25,30,"INSERT YOUR");
  _display.drawStr(30,50,"CARD HERE");
  _display.sendBuffer();
  http.end();
}

void Attendance::beep(int n, int d) {
  for (int i = 0; i < n; i++) {
    digitalWrite(_buzzer, HIGH);
    delay(d);
    digitalWrite(_buzzer, LOW);
    delay(d);
  }
}

String Attendance::urlencode(const String& str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == ' ') encoded += '+';
    else if (isalnum(c)) encoded += c;
    else {
      char buf[4];
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}

char* Attendance::getStudentNameById(char* uid) {
  for (int i = 0; i < _studentCount; i++) {
    if (strcmp(_students[i].code, uid) == 0) {
      return _students[i].name;
    }
  }
  return nullptr;
}

int Attendance::countElements(const char* data, char delim) {
  char tmp[strlen(data) + 1];
  strcpy(tmp, data);
  int cnt = 0;
  char* tok = strtok(tmp, &delim);
  while (tok) {
    cnt++;
    tok = strtok(NULL, &delim);
  }
  return cnt;
}
