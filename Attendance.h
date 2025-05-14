// Attendance.h
#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define MAX_STUDENTS 10

struct Student {
  String id;
  char code[10];
  char name[30];
};

enum Mode {
  WRITE_UID       = 1,
  ATTENDANCE_LOG,
  CREATE_NEW_CARD
};

class Attendance {
public:
  Attendance(const char* ssid,
             const char* password,
             const String& url,
             U8G2_SSD1306_128X64_NONAME_F_HW_I2C& display,
             MFRC522& rfid,
             int btnIO,
             int ledIO,
             int btnCreate,
             int ledCreate,
             int buzzerPin);

  bool begin();
  void loop();

private:
  void      readUID();
  bool      readDataSheet();       // changed to bool
  void      writeUIDSheet();
  void      writeLogSheet();
  void      writeNewCardSheet();
  void      beep(int n, int d);
  String    urlencode(const String& str);
  char*     getStudentNameById(char* uid);
  int       countElements(const char* data, char delim);

  const char*                                  _ssid;
  const char*                                  _password;
  String                                       _webAppUrl;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C&          _display;
  MFRC522&                                     _rfid;

  Student                                      _students[MAX_STUDENTS];
  int                                          _studentCount;
  String                                       _uidString;
  Mode                                         _mode;

  int                                          _btnIO;
  int                                          _ledIO;
  bool                                         _btnIOState;
  unsigned long                                _ioDebounce;
  bool                                         _inOutState;

  int                                          _btnCreate;
  int                                          _ledCreate;
  bool                                         _btnCreateState;
  unsigned long                                _createDebounce;

  int                                          _buzzer;
  unsigned long                                _lastUIDRead;   // ← added
};

#endif // ATTENDANCE_H
