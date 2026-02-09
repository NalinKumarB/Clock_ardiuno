#include <avr/interrupt.h>
#include <Wire.h>

#define BLANK 10

// ---------------- DISPLAY PINS ----------------
const byte segPins[7]   = {2,3,4,5,6,7,8};
const byte digitPins[4]= {10,11,12,13};

// ---------------- BUTTONS ----------------
#define MODE_BTN A0
#define INC_BTN  A1

// ---------------- DIGIT MAP ----------------
const byte digitMap[11][7] = {
  {1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},
  {1,1,1,1,0,0,1},{0,1,1,0,0,1,1},{1,0,1,1,0,1,1},
  {1,0,1,1,1,1,1},{1,1,1,0,0,0,0},{1,1,1,1,1,1,1},
  {1,1,1,1,0,1,1},
  {0,0,0,0,0,0,0}
};

volatile byte displayDigits[4] = {0,0,0,0};
volatile byte currentDigit = 0;

// ---------------- TIME VARIABLES ----------------
byte hour = 0, minute = 0;
byte setMode = 0;

// ---------------- TIMER2 ----------------
void setupTimer2() {
  cli();
  TCCR2A = 0x00;
  TCCR2B = 0x04;
  TCNT2  = 6;
  TIMSK2 = 0x01;
  sei();
}

ISR(TIMER2_OVF_vect) {
  TCNT2 = 6;

  for (byte i = 0; i < 4; i++)
    digitalWrite(digitPins[i], HIGH);

  byte num = displayDigits[currentDigit];
  for (byte i = 0; i < 7; i++)
    digitalWrite(segPins[i], digitMap[num][i]);

  digitalWrite(digitPins[currentDigit], LOW);

  currentDigit++;
  if (currentDigit >= 4) currentDigit = 0;
}

// ---------------- DS3231 ----------------
#define DS3231_ADDR 0x68

byte bcdToDec(byte val) {
  return (val >> 4) * 10 + (val & 0x0F);
}

byte decToBcd(byte val) {
  return ((val / 10) << 4) | (val % 10);
}

void readTime() {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_ADDR, 3);
  Wire.read();
  minute = bcdToDec(Wire.read());
  hour   = bcdToDec(Wire.read() & 0x3F);
}

void writeTime() {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(0);
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.endTransmission();
}

// ---------------- BLUETOOTH TIME SET ----------------
void checkBluetooth() {

  if (Serial.available()) {

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if(cmd == "status"){
      Serial.print(hour);
      Serial.print(" : ");
      Serial.println(minute);
    }

    if (cmd.startsWith("T=") && cmd.length() >= 7) {

      byte newHour   = cmd.substring(2,4).toInt();
      byte newMinute = cmd.substring(5,7).toInt();

      if (newHour < 24 && newMinute < 60) {

        hour = newHour;
        minute = newMinute;

        writeTime();
        setMode = 0;     // exit setting mode
      }
    }
  }
}

// ---------------- SETUP ----------------
void setup() {

  for (byte i = 0; i < 7; i++) pinMode(segPins[i], OUTPUT);
  for (byte i = 0; i < 4; i++) pinMode(digitPins[i], OUTPUT);

  pinMode(MODE_BTN, INPUT);
  pinMode(INC_BTN, INPUT);

  Serial.begin(9600);   // Bluetooth

  Wire.begin();
  setupTimer2();
}

// ---------------- LOOP ----------------
void loop() {

  static unsigned long lastRead = 0;
  static unsigned long lastBtn = 0;

  // -------- Bluetooth check --------
  checkBluetooth();

  // --- MODE button ---
  if (!digitalRead(MODE_BTN) && millis() - lastBtn > 300) {
    lastBtn = millis();
    setMode++;

    if (setMode > 2) {
      setMode = 0;
      writeTime();
    }
  }

  // --- INC button ---
  if (!digitalRead(INC_BTN) && millis() - lastBtn > 300) {
    lastBtn = millis();

    if (setMode == 1) hour = (hour + 1) % 24;
    if (setMode == 2) minute = (minute + 1) % 60;
  }

  // --- Normal RTC read ---
  if (setMode == 0 && millis() - lastRead >= 1000) {
    lastRead = millis();
    readTime();
  }

  // --- Blink setting ---
  bool blink = (millis() / 300) % 2;

  if (setMode == 1 && blink) {
    displayDigits[0] = displayDigits[1] = BLANK;
  } else {
    displayDigits[0] = hour / 10;
    displayDigits[1] = hour % 10;
  }

  if (setMode == 2 && blink) {
    displayDigits[2] = displayDigits[3] = BLANK;
  } else {
    displayDigits[2] = minute / 10;
    displayDigits[3] = minute % 10;
  }
}
