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

uint32_t secondsToStoreUnitTime = 900, lastTimeItStored = 0;
uint32_t periodToFeed = 3000;

uint8_t hoursToPump[] = { 7, 10, 13, 16, 19, 22, 2, 5 };
uint8_t hoursToPumpLen;

/*
 * 0, 1, 2, 3 -> incrementValue
 * 4, 5, 6 -> Last time it fed
 * 7, 8, 9, 10 -> UNIX time saved
 */

// rate = g/s
inline static uint32_t getSeconds(uint32_t day, uint32_t month, uint32_t rate) {

  uint32_t result = 0;
  if (month == 2) {
    if (day <= 17) {
      result = 500; //782;
    } else if (day <= 25) {
      result = 700; // 1092;
    } else if (day <= 27) {
      result = 900 ;// 1139;
    } else {
      result = 1000; // 1487;
    }
  } else if (month == 3) {
    if (day <= 6) {
      result = 1300; //; 1901;
    } else if (day <= 13) {
      result = 1600; // 2380;
    } else if (day <= 20) {
      result = 2000; // 2958;
    } else if (day <= 27) {
      result = 3633;
    } else {
      result = 3853;
    }
  } else  {
    if (day <= 3) {
      result = 3976;
    } else if(day <= 10) {
        result = 4718;
    } else if(day <= 17) {
      result = 5526;
    } else if (day <= 24) {
      result = 5526;
    } else {
      result = 6263;
    }
  }

  //result = result / 2; // adusting
  
  result = ( result / 4 ) / rate;

  return result;
  
}

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

  hoursToPumpLen = sizeof(hoursToPump);

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
uint8_t i = 0;
boolean shouldBePumping;
DateTime now;

boolean feederTested = false, feederTest = false;

void testFeeder() {
  if (!feederTested) {
    digitalWrite(FEEDER_PIN, HIGH);
    delay(5000);
    digitalWrite(FEEDER_PIN, LOW);
    feederTested = true;
    Serial.println("TESTED");
  }
}

void loop() {
  // put your main code here, to run repeatedly:

#define DEFINE_ALARM(HOUR, MINUTE, PIN, PERIOD, VAR, STATE) \
  periodToFeed = getSeconds(now.day(), now.month(), 15) * 1000; \
  if ( now.hour() == HOUR && now.minute() == MINUTE && !( alarms & (1 << STATE) ) ) { \
    lastTimeFed[0] = now.hour(); \
    lastTimeFed[1] = now.minute(); \
    lastTimeFed[2] = now.second(); \
    alarms = 1 << STATE; \
    Serial.println("Alarm!"); \
    Serial.print(periodToFeed); \
    Serial.println(" sec"); \
    digitalWrite(PIN, HIGH); VAR = HIGH; \
    delay(periodToFeed); \
    digitalWrite(PIN, LOW); VAR = LOW; \
    incrementEEPROM(); \
  }
  
  now = rtc.now();

  unixTime = now.unixtime();

  if (feederTest) {
    testFeeder();
    return;
  }

  if ((unixTime - lastTimeItStored) > secondsToStoreUnitTime) {
    saveUnixTime();
    Serial.println("Saving UNIX time");
    Serial.println(unixTime);
    lastTimeItStored = unixTime;
  }  

  Serial.println(clockAdjusted);
  Serial.print(now.day());
  Serial.print("/");
  Serial.print(now.month());
  Serial.println("----");  
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

  DEFINE_ALARM( 6, 00,  FEEDER_PIN, periodToFeed, FEEDER_STATE, 0);
  DEFINE_ALARM( 9, 00,  FEEDER_PIN, periodToFeed, FEEDER_STATE, 1);
  DEFINE_ALARM(12, 00,  FEEDER_PIN, periodToFeed, FEEDER_STATE, 2);
  DEFINE_ALARM(15, 00,  FEEDER_PIN, periodToFeed, FEEDER_STATE, 3);


  shouldBePumping = false;

  i = 0;
  while ((i < hoursToPumpLen) && !shouldBePumping) {
    shouldBePumping = shouldBePumping || ( now.hour() == hoursToPump[i]);
    i++;
  }

  if (
      shouldBePumping
    ) {
    Serial.println("It should be pumping!");
    if (PUMP_STATE == LOW) {
      digitalWrite(PUMP_PIN, HIGH);
      PUMP_STATE = HIGH;
    }
  } else {
    if (PUMP_STATE == HIGH) {
     digitalWrite(PUMP_PIN, LOW);
     PUMP_STATE = LOW;
    }
    Serial.println("It shouldn't be pumping!");
  }


  delay(5000);

}
