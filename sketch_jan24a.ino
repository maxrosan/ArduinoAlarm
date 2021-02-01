#include "RTClib.h" //INCLUSÃO DA BIBLIOTECA
#include <Wire.h>
#include <EEPROM.h>

DS3231 rtc; //OBJETO DO TIPO RTC_DS3231
uint32_t incrementValue = 0;
uint8_t lastTimeFed[3] = { 0, 0, 0 };

uint32_t savedUnixTime = 0;
uint32_t unixTime = 0;

uint8_t FEEDER_PIN = 7;
uint8_t PUMP_PIN = 6;

uint16_t alarms = 0;

uint32_t secondsToStoreUnitTime = 3600, lastTimeItStored = 0;
uint32_t periodToFeed = 1500;

/*
 * 0, 1, 2, 3 -> incrementValue
 * 4, 5, 6 -> Last time it fed
 * 7, 8, 9, 10 -> UNIX time saved
 */

void saveUnixTime() {

  EEPROM.write(7, unixTime & 0xFF);
  EEPROM.write(8, ( unixTime >> 8 ) & 0xFF);
  EEPROM.write(9, ( unixTime >> 16 ) & 0xFF);  
  EEPROM.write(10, ( unixTime >> 24 ) & 0xFF);  
  
}

void restoreUnixTime() {

  unixTime = EEPROM.read(7);
  unixTime = ( ( (uint32_t) EEPROM.read(8) ) << 8 ) + unixTime; 
  unixTime = ( ( (uint32_t) EEPROM.read(9) ) << 16 ) + unixTime;
  unixTime = ( ( (uint32_t) EEPROM.read(10) ) << 24 ) + unixTime;
  
}

void zeroEEPROM() {
  EEPROM.write(0, 0);
  EEPROM.write(1, 0);
  EEPROM.write(2, 0);
  EEPROM.write(3, 0);

  EEPROM.write(4, 0);
  EEPROM.write(5, 0);
  EEPROM.write(6, 0);

  EEPROM.write(7, 0);
  EEPROM.write(8, 0);
  EEPROM.write(9, 0);
  EEPROM.write(10, 0);
}

void incrementEEPROM() {
  incrementValue++;
  EEPROM.write(0, incrementValue & 0xFF);
  EEPROM.write(1, ( incrementValue >> 8 ) & 0xFF);
  EEPROM.write(2, ( incrementValue >> 16 ) & 0xFF);
  EEPROM.write(3, ( incrementValue >> 24 ) & 0xFF);

  
  EEPROM.write(4, lastTimeFed[0]);
  EEPROM.write(5, lastTimeFed[1]);
  EEPROM.write(6, lastTimeFed[2]);  
}

uint32_t readEEPROM() {
  uint32_t result;
  result = EEPROM.read(0);
  result = ( ( (uint32_t) EEPROM.read(1) ) << 8 ) + result; 
  result = ( ( (uint32_t) EEPROM.read(2) ) << 16 ) + result;
  result = ( ( (uint32_t) EEPROM.read(3) ) << 24 ) + result;

  lastTimeFed[0] = EEPROM.read(4);
  lastTimeFed[1] = EEPROM.read(5);
  lastTimeFed[2] = EEPROM.read(6);
  
  return result;
}

bool clockAdjusted = false;

void setup() {
  // put your setup code here, to run once:

  Wire.begin();
  Serial.begin(9600);
  rtc.begin();

  pinMode(FEEDER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(FEEDER_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);


  //zeroEEPROM();
  incrementValue = readEEPROM();

  if (true) {
  //if (!rtc.isrunning()) {
    Serial.println("RTC adjusting");
    clockAdjusted = true;
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  restoreUnixTime();

  delay(3000);

  DateTime now = rtc.now();

  Serial.println("UNIX time");
  Serial.print("now = ");
  Serial.println(now.unixtime());
  Serial.print("stored = ");
  Serial.println(unixTime);
  
  if (unixTime > now.unixtime()) {
    rtc.adjust(DateTime(unixTime));
    Serial.println("Adjusting again");
  }

  lastTimeItStored = now.unixtime();

}

uint8_t PUMP_STATE = LOW;
uint8_t FEEDER_STATE = LOW;

void loop() {
  // put your main code here, to run repeatedly:

#define DEFINE_ALARM(HOUR, MINUTE, PIN, PERIOD, VAR, STATE) \
  if ( now.hour() == HOUR && now.minute() == MINUTE && !( alarms & (1 << STATE) ) ) { \
    lastTimeFed[0] = now.hour(); \
    lastTimeFed[1] = now.minute(); \
    lastTimeFed[2] = now.second(); \
    alarms = 1 << STATE; \
    Serial.println("Alarm!"); \
    digitalWrite(PIN, HIGH); VAR = HIGH; \
    delay(3000); \
    digitalWrite(PIN, LOW); VAR = LOW; \
    incrementEEPROM(); \
  }
  
  DateTime now = rtc.now();

  unixTime = now.unixtime();

  if ((unixTime - lastTimeItStored) > secondsToStoreUnitTime) {
    saveUnixTime();
    Serial.println("Saving UNIX time");
    Serial.println(unixTime);
    lastTimeItStored = unixTime;
  }  

  Serial.println(clockAdjusted);
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  Serial.println("----");
  Serial.print("Alarm sounded ");
  Serial.print(incrementValue);
  Serial.println(" times");
  Serial.print("Last time it fed: ");
  Serial.print(lastTimeFed[0]);
  Serial.print(":");
  Serial.print(lastTimeFed[1]);
  Serial.print(":");
  Serial.print(lastTimeFed[2]);
  Serial.println();
  Serial.println(unixTime);

  //DEFINE_ALARM(6, 00,  FEEDER_PIN, periodToFeed);
  DEFINE_ALARM(8, 19,  FEEDER_PIN, periodToFeed, FEEDER_STATE, 0);
  DEFINE_ALARM(10, 00, FEEDER_PIN, periodToFeed, FEEDER_STATE, 1);
  DEFINE_ALARM(12, 00, FEEDER_PIN, periodToFeed, FEEDER_STATE, 2);
  DEFINE_ALARM(14, 00, FEEDER_PIN, periodToFeed, FEEDER_STATE, 3);
  DEFINE_ALARM(15, 42, FEEDER_PIN, periodToFeed, FEEDER_STATE, 4);

  if (
      ( now.hour() >= 7 && now.hour() <= 14 ) ||
      ( now.hour() >= 19 && now.hour() <= 23 ) ||
      ( now.hour() >= 0 && now.hour() <= 6 )
    ) {
    Serial.println("It should be pumping!");
    if (PUMP_STATE == LOW) {
      digitalWrite(PUMP_PIN, HIGH);
      PUMP_STATE = HIGH;
    }
  } else {
    digitalWrite(PUMP_PIN, LOW);
    PUMP_STATE = LOW;
  }


  delay(1000);

}
