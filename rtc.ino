
#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02d%02d%04d%02d%02d%02d"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  Serial.println(datestring);
}

void beginRTC(void) {
  if (debug) {
    //Serial.println();
    //Serial.print("myTime: ");
    /* String format: __DATE__= "Oct  8 2013" & __TIME__= "00:13:39"    */
    //Serial.print(__DATE__);
    //Serial.println(__TIME__);
  }

  Rtc.Begin();
  RtcDateTime myTime = RtcDateTime(__DATE__, __TIME__);
  //printDateTime(myTime);

  if (!Rtc.IsDateTimeValid())    // Common Cuases:
  { //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
    if (debug) {
      //Serial.println("RTC lost confidence in the DateTime!");
    }
    Rtc.SetDateTime(myTime);
  }

  if (!Rtc.GetIsRunning())
  {
    if (debug) {
      //Serial.println("RTC was not actively running, starting now");
    }
    Rtc.SetIsRunning(true);
  }

  //time_now();

  RtcDateTime now = Rtc.GetDateTime();
  if (now > myTime)     {
    if (debug) {
      //Serial.println("RTC is newer than compile time. (this is expected)");
    }
  } else if (now < myTime)     {
    if (debug) {
      //Serial.println("RTC is older than compile time!  (Updating DateTime)");
    }
    Rtc.SetDateTime(myTime);
  } else if (now == myTime)     {
    if (debug) {
      //Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
  }

  // never assume the Rtc was last configured by you, so just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
}
