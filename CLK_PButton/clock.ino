#include <avr/interrupt.h>
#include <Wire.h>

#define BLANK 10

// ---------------- DISPLAY PINS ----------------
// a b c d e f g
const byte segPins[7]   = {2,3,4,5,6,7,8};
// D1 D2 D3 D4
const byte digitPins[4] = {10,11,12,13};

// ---------------- BUTTONS ----------------
#define MODE_BTN A0
#define INC_BTN  A1

// ---------------- DIGIT MAP ----------------
const byte digitMap[11][7] = {
  {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1},
  {1,1,1,1,0,0,1}, {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1},
  {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0}, {1,1,1,1,1,1,1},
  {1,1,1,1,0,1,1},
  {0,0,0,0,0,0,0}  // BLANK
};

volatile byte displayDigits[4] = {0,0,0,0};
volatile byte currentDigit = 0;

// ---------------- TIME VARIABLES ----------------
byte hour = 12;
byte minute = 0;
bool isPM = false;        // AM/PM flag
byte setMode = 0;         // 0=normal, 1=set hour, 2=set minute

// ---------------- TIMER2 SETUP ----------------
void setupTimer2() {
  cli();
  TCCR2A = 0x00;
  TCCR2B = 0x04;      // prescaler 64
  TCNT2  = 6;
  TIMSK2 = 0x01;
  sei();
}

// ---------------- TIMER2 ISR ----------------
ISR(TIMER2_OVF_vect) {
  TCNT2 = 6;

  // Turn OFF all digits
  for (byte i = 0; i < 4; i++)
    digitalWrite(digitPins[i], LOW);

  byte num = displayDigits[currentDigit];

  // Common anode → segment ON = LOW
  for (byte i = 0; i < 7; i++)
    digitalWrite(segPins[i], !digitMap[num][i]);

  digitalWrite(digitPins[currentDigit], HIGH);

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

// Convert RTC 24h → 12h format
void readTime() {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_ADDR, 3);

  Wire.read(); // seconds
  minute = bcdToDec(Wire.read());

  byte rawHour = bcdToDec(Wire.read() & 0x3F);

  if (rawHour == 0) {
    hour = 12;
    isPM = false;
  }
  else if (rawHour == 12) {
    hour = 12;
    isPM = true;
  }
  else if (rawHour > 12) {
    hour = rawHour - 12;
    isPM = true;
  }
  else {
    hour = rawHour;
    isPM = false;
  }
}

// Convert 12h → 24h before writing
void writeTime() {
  byte hour24;

  if (isPM) {
    if (hour == 12) hour24 = 12;
    else hour24 = hour + 12;
  } else {
    if (hour == 12) hour24 = 0;
    else hour24 = hour;
  }

  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(0);  // reset seconds
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour24));
  Wire.endTransmission();
}

// ---------------- SETUP ----------------
void setup() {
  for (byte i = 0; i < 7; i++) pinMode(segPins[i], OUTPUT);
  for (byte i = 0; i < 4; i++) pinMode(digitPins[i], OUTPUT);

  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(INC_BTN, INPUT_PULLUP);

  Wire.begin();
  setupTimer2();
}

// ---------------- LOOP ----------------
void loop() {

  static unsigned long lastRead = 0;
  static unsigned long lastBtn = 0;

  // --- MODE button ---
  if (!digitalRead(MODE_BTN) && millis() - lastBtn > 300) {
    lastBtn = millis();
    setMode++;
    if (setMode > 2) {
      setMode = 0;
      writeTime();   // save time to RTC
    }
  }

  // --- INC button ---
  if (!digitalRead(INC_BTN) && millis() - lastBtn > 300) {
    lastBtn = millis();

    if (setMode == 1) {   // Set hour
      hour++;
      if (hour > 12) hour = 1;

      // Toggle AM/PM when passing 11 → 12
      if (hour == 12) isPM = !isPM;
    }

    if (setMode == 2) {   // Set minute
      minute++;
      if (minute >= 60) minute = 0;
    }
  }

  // --- Normal mode read from RTC every 1 sec ---
  if (setMode == 0 && millis() - lastRead >= 1000) {
    lastRead = millis();
    readTime();
  }

  // --- Blink effect ---
  bool blink = (millis() / 300) % 2;

  // Hour display
  if (setMode == 1 && blink) {
    displayDigits[0] = displayDigits[1] = BLANK;
  } else {
    displayDigits[0] = hour / 10;
    displayDigits[1] = hour % 10;
  }

  // Minute display
  if (setMode == 2 && blink) {
    displayDigits[2] = displayDigits[3] = BLANK;
  } else {
    displayDigits[2] = minute / 10;
    displayDigits[3] = minute % 10;
  }
}
