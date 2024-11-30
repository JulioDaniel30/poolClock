
// CONNECTIONS:
// DS1302 CLK/SCLK --> 5
// DS1302 DAT/IO --> 4
// DS1302 RST/CE --> 2
// DS1302 VCC --> 3.3v - 5v
// DS1302 GND --> GND
//https://github.com/Makuna/Rtc/blob/master/examples/DS1302_Simple/DS1302_Simple.ino
//https://github.com/Makuna/Rtc/blob/master/examples/DS1302_Memory/DS1302_Memory.ino
#include <RtcDS1302.h>

const int RelePin = 13;
const int btPIn = 12;
const int btpausePin = 11; // bt pause
const int ledBLig = 10;
bool lig = false;
bool blig = false;
bool btpause = false;
int hlig = 7;
int hdes = 17;

ThreeWire myWire(4,5,3); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

void setup () 
{
    Serial.begin(57600);

    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    pinMode(RelePin, OUTPUT); // seta o pino como saída
    pinMode(ledBLig, OUTPUT);
    pinMode(btPIn, INPUT_PULLUP); // set input bt pause
    pinMode(btpausePin, INPUT_PULLUP);
    digitalWrite(RelePin, LOW); // seta o pino com nivel logico baixo

    Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Causes:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
}

void loop () 
{
    RtcDateTime now = Rtc.GetDateTime();

    printDateTime(now);
    Serial.println();

    if( digitalRead(btPIn) == LOW){
      blig = !blig;
      Serial.println(blig);
    }
    if( digitalRead(btpausePin) == LOW){
      btpause = !btpause;
      Serial.println(btpause);
    }
    if (blig){
      digitalWrite(ledBLig, HIGH);
    }else{
      digitalWrite(ledBLig, HIGH);
    }
    
    if(now.Hour() >= hlig & now.Hour() < hdes){
      lig = true;
    }else{
      lig = false;
    }
    if(now.Hour() == 22 || now.Hour() == 00){
        blig = false;
    }
    if(!btpause){
      if( lig || blig){
        digitalWrite(RelePin, HIGH);
      }else{
        digitalWrite(RelePin, LOW);
      }
    }else{
        digitalWrite(RelePin, LOW);
    }

    if (!now.IsValid())
    {
        // Common Causes:
        //    1) the battery on the device is low or even missing and the power line was disconnected
        Serial.println("RTC lost confidence in the DateTime!");
    }

    delay(1000); // ten seconds
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[26];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
