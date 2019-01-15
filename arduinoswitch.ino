#include <Wire.h>
#include <CRC32.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <TimerOne.h>
#include <RtcDS3231.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>
#include <avr/wdt.h>

#define sw1       14
#define sw2       15
#define sw3       16

#define buz       3   // PWM
#define RGB       5   // PWM 

#define NUM_LEDS    1
#define WAIT        10
#define BRIGHTNESS  50

RtcDS3231<TwoWire> Rtc(Wire);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, RGB, NEO_GRBW + NEO_KHZ800);
SoftwareSerial SerialWifi(10, 11); //Rx 10, Tx 11

bool reset = false;
int debug = 0;
int sw = 0, pressed = 0;
int sw_delay = 300, eeAdd = 0, mqtt = 0, rgb_led = 1, sw_err = 0, sw_loop = 1, sw_fun = 0;
char datestring[20];

// Catch all an unhandled interrupt,
ISR(BADISR_vect)
{
  for (;;) UDR0 = '!';
}

struct vote
{
  int myswitch;
  unsigned long mytime;
  unsigned long mydate;
} myvote;

template <class T> int EEPROM_write(int ee, const T& value)
{
  const byte* p = (const byte*)(const void*)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_read(int ee, T& value)
{
  byte* p = (byte*)(void*)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

void setup(void) {
  Serial.begin(9600);
  delay(1000);
  wdt_disable();
  Serial.println(F(""));
  Serial.println(F("Firmware : TFB_ARD_v1.7"));
  Serial.println(F("Company  : Avinashi Ventures Pvt Ltd"));
  Serial.println(F("Author   : Dharmendra Savaliya"));

  SerialWifi.begin(9600);
  SerialWifi.end();
  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  colorWipe(strip.Color(BRIGHTNESS, 0, 0), WAIT);
  pinMode(buz, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite (buz, HIGH) ;
    delay (250) ;
    digitalWrite (buz, LOW) ;
    delay (1000) ;
  }

  pinMode(sw1, OUTPUT);
  pinMode(sw2, OUTPUT);
  pinMode(sw3, OUTPUT);

  Serial.println(F("RTC begin"));
  beginRTC();

  Serial.print(F("\nReset Reason :"));
  if (MCUSR & (1 << PORF )) Serial.println(F("Power-on reset."));
  if (MCUSR & (1 << EXTRF)) Serial.println(F("External reset."));
  if (MCUSR & (1 << BORF )) Serial.println(F("Brownout reset."));
  if (MCUSR & (1 << WDRF )) Serial.println(F("Watchdog reset."));
  MCUSR = 0;

  colorWipe(strip.Color(0, BRIGHTNESS, 0), WAIT);
  Serial.println(F("Press"));
  Serial.flush();
  variableInfo();

  SerialWifi.begin(9600);
  SerialWifi.setTimeout(10000);
  SerialWifi.listen();
}

void loop(void) {
  if (rgb_led) {
    if (mqtt == 0) {
      for (int i = 30; i < 100; i = i + 5) {
        colorWipe(strip.Color(0, i, 0), WAIT);
      }
      delay(80);
      for (int i = 95; i > 30; i = i - 5) {
        colorWipe(strip.Color(0, i, 0), WAIT);
      }
      delay(20);
    } else if (mqtt == 1) {
      colorWipe(strip.Color(0, BRIGHTNESS, 0), WAIT);
      delay(5);
    } else if (mqtt == 2) {
      colorWipe(strip.Color(BRIGHTNESS, 0, BRIGHTNESS), WAIT);
      delay(1000);
      mqtt = 1;
      delay(5);
    }
  }

  String dataWifi = "";
  SerialWifi.listen();
  while (SerialWifi.available()) {
    dataWifi = SerialWifi.readStringUntil('.');
    yield();
  }

  if (dataWifi == "data") {
    Serial.println(dataWifi);
    int line = 0;
    int ok_count = 0;
    int newAdd = 0;

    for (int i = 0; i <  eeAdd / sizeof(myvote); i++) {
      //double mls = millis();
      EEPROM_read(i * sizeof(myvote), myvote);

      char buf1[8], buf2[6];
      sprintf(buf1, "%08lu", myvote.mydate);
      sprintf(buf2, "%06lu", myvote.mytime);

      String temp = String(myvote.myswitch) + "," + String(buf1) + String(buf2);
      String thisVote = "V," + temp + ".";
      SerialWifi.print(thisVote);
      SerialWifi.flush();

      if (myvote.myswitch != 0) {
        String message = temp;
        uint8_t byteBuffer[message.length()];
        size_t numBytes = sizeof(byteBuffer) - 1;
        message.getBytes(byteBuffer, message.length());

        CRC32 crc;
        for (size_t i = 0; i < numBytes; i++)
        {
          crc.update(byteBuffer[i]);
        }

        uint32_t checkard = crc.finalize();

        delay(30);             // Wait for Wifi CRC

        String ddataWifi = "";
        SerialWifi.listen();
        while (SerialWifi.available()) {
          ddataWifi = SerialWifi.readStringUntil('.');
          yield();
        }

        line++;
        if (ddataWifi[0] == 'C') {
          ddataWifi.remove(0, 2);
          if (ddataWifi == String(checkard)) {
            ok_count++;
          } else {
            EEPROM_write(newAdd * sizeof(myvote), myvote);  // No CRC ok received
            newAdd++;
          }
        } else {
          EEPROM_write(newAdd * sizeof(myvote), myvote);  // No CRC ok received
          newAdd++;
        }
      }
    }
    eeAdd = newAdd * sizeof(myvote);

    if (ok_count != 0) {
      if (line == ok_count) {
        //Serial.println(F("All data send"));
        eeAdd = 0;
        for (int i = 0 ; i < EEPROM.length() ; i++) {
          EEPROM.write(i, 0);
        }
      }
    }
    dataWifi = "";
  }

  if (dataWifi[0] == 'D') {
    dataWifi.remove(0, 2);
    int tempdelay;
    tempdelay = dataWifi.toInt();
    sw_delay = tempdelay * 10;
    dataWifi = "";
  }

  if (dataWifi[0] == 'M') {
    dataWifi.remove(0, 2);
    mqtt = dataWifi.toInt();
    if (mqtt) {
      Serial.println(F("Online"));
    } else {
      Serial.println(F("Offline"));
    }
    dataWifi = "";
  }

  if (digitalRead(sw1) == 0 && digitalRead(sw2) == 0 && digitalRead(sw3) == 0) {
    pressed = 0;
  }

  if (!pressed) {
    if (sw_loop) {
      if (digitalRead(sw1)) {
        sw = 1;
        Serial.println(F("sw1 pressed"));
        colorWipe(strip.Color(0, 0, BRIGHTNESS), WAIT);
      } else if (digitalRead(sw2)) {
        sw = 2;
        Serial.println(F("sw2 pressed"));
        colorWipe(strip.Color(0, 0, BRIGHTNESS), WAIT);
      } else if (digitalRead(sw3)) {
        sw = 3;
        Serial.println(F("sw3 pressed"));
        colorWipe(strip.Color(0, 0, BRIGHTNESS), WAIT);
      }
    }

    if (!sw_loop) {
      if (sw_err) {
        if (digitalRead(sw1) == 1 || digitalRead(sw2) == 1 || digitalRead(sw3) == 1) {
          pressed = 1;
          variableInfo();
          errorBuzzer();
          //sw_err = 0;   // Stop repeat error buzzer sound
        }
      }
    }
  }

  if (sw != 0) {
    RtcDateTime now = Rtc.GetDateTime();
    char nowDate[8], nowTime[6];
    sprintf(nowDate, "%02d%02d%02d", now.Day(), now.Month(), now.Year());
    sprintf(nowTime, "%02d%02d%02d", now.Hour(), now.Minute(), now.Second());

    String temp2 = String(sw) + "," + String(nowDate) + String(nowTime);
    String newVote = "V," + temp2 + ".";
    SerialWifi.print(newVote);
    SerialWifi.flush();
    //Serial.println(newVote);
    Serial.flush();

    String message = temp2;
    uint8_t byteBuffer[message.length()];
    size_t numBytes = sizeof(byteBuffer) - 1;
    message.getBytes(byteBuffer, message.length());

    CRC32 crc;
    for (size_t i = 0; i < numBytes; i++)
    {
      crc.update(byteBuffer[i]);
    }

    uint32_t checkard = crc.finalize();

    delay(25);

    String ddataWifi = "";
    SerialWifi.listen();
    while (SerialWifi.available()) {
      ddataWifi = SerialWifi.readStringUntil('.');
      yield();
    }

    if (ddataWifi[0] == 'C') {
      ddataWifi.remove(0, 2);

      if (ddataWifi == String(checkard)) {
        Serial.println(F("Vote Send"));
      } else {
        int point = 0;
        point = eeAdd / sizeof(myvote);

        unsigned long a, ab, b, c, x, xy, y, z;
        a = now.Day() * 1000000;
        ab = now.Month();
        b = ab * 10000 ;
        c = now.Year();
        x = now.Hour();
        xy = x * 10000;
        y = now.Minute() * 100;
        z = now.Second();
        myvote.myswitch = sw;
        myvote.mydate = a  + b + c ;
        myvote.mytime = xy  + y + z ;

        EEPROM_write(eeAdd, myvote);    // [address, variable]
        eeAdd = eeAdd + sizeof(myvote);       //Move address to the next byte after float 'f'.
      }
    } else {
      int point = 0;
      point = eeAdd / sizeof(myvote);

      unsigned long a, ab, b, c, x, xy, y, z;
      a = now.Day() * 1000000;
      ab = now.Month();
      b = ab * 10000 ;
      c = now.Year();
      x = now.Hour();
      xy = x * 10000;
      y = now.Minute() * 100;
      z = now.Second();
      myvote.myswitch = sw;
      myvote.mydate = a  + b + c ;
      myvote.mytime = xy  + y + z ;

      EEPROM_write(eeAdd, myvote);    // [address, variable]
      eeAdd = eeAdd + sizeof(myvote);       //Move address to the next byte after float 'f'.
    }
  }
  sw_err = 0;
  sw_loop = 0;
  rgb_led = 0;
  sw = 0;
  sw_fun = 0;
  pressed = 1;
  digitalWrite (buz, HIGH) ;
  delay(5);
  Timer1.initialize(100000);
  Timer1.attachInterrupt(switchfun);
}


void variableInfo() {
  Serial.print(F("Sw:"));
  Serial.print(sw);
  Serial.print(F(",SwLoop:"));
  Serial.print(sw_loop);
  Serial.print(F(",Press:"));
  Serial.println(pressed);
}

void switchfun(void) {
  sw_fun++;

  switch (sw_fun) {
    case 12:
      digitalWrite (buz, LOW) ;
      break;
    case 20:
      digitalWrite (buz, HIGH) ;
      break;
    case 32:
      digitalWrite (buz, LOW) ;
      break;
    case 40:
      digitalWrite (buz, HIGH) ;
      break;
    case 52:
      digitalWrite (buz, LOW) ;
      break;
    case 60:
      digitalWrite (buz, HIGH) ;
      break;
    case 72:
      digitalWrite (buz, LOW) ;
      break;
    case 80:
      digitalWrite (buz, HIGH) ;
      break;
    case 92:
      digitalWrite (buz, LOW) ;
      sw_err = 1;
      colorWipe(strip.Color(BRIGHTNESS, 0, 0), WAIT);
      break;
    default:
      break;
  }
  if (sw_fun >= sw_delay) {
    Serial.println(F("Press..."));
    rgb_led = 1;
    sw_loop = 1;
    sw_fun = 0;
    Timer1.detachInterrupt();
    colorWipe(strip.Color(0, BRIGHTNESS, 0), WAIT);
  }
}

void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

uint8_t red(uint32_t c) {
  return (c >> 16);
}
uint8_t green(uint32_t c) {
  return (c >> 8);
}
uint8_t blue(uint32_t c) {
  return (c);
}

void errorBuzzer(void) {
  for (int i = 0; i < 5; i++) {
    digitalWrite (buz, HIGH) ;
    delay(50);                      // wait for a second
    digitalWrite (buz, LOW) ;
    delay(10);                      // wait for a second
  }
  colorWipe(strip.Color(BRIGHTNESS, 0, 0), WAIT);
}

int freeRam (void)
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

String getStringPartByNr(String data, char separator, int index) // split by separator
{
  int stringData = 0; // variable to count data part nr
  String dataPart = ""; // variable to hole the return text

  for (int i = 0; i < data.length(); i++) { // Walk through the text one letter at a time

    if (data[i] == separator) { // Count the number of times separator character appears in the text
      stringData++;

    } else if (stringData == index) { // get the text when separator is the rignt one
      dataPart.concat(data[i]);

    } else if (stringData > index) { // return text and stop if the next separator appears - to save CPU-time
      return dataPart;
      break;
    }
  }
  return dataPart;
}
