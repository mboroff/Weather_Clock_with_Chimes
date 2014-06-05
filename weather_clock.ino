/********************************************************          my weather clock - Marty Boroff
          uses Ethernet for time and weather data
          uses mp3 card for westminister chimes
          uses tft breakout in 8 pin mode for display 
          most code was originally cut and paste the modified
          
          NTP Taken from
          created 4 Sep 2010 
          by Michael Margolis
          modified 9 Apr 2012
          by Tom Igoe          
          
          TFT code copied from Adafruit examples
          mp3 code from Adafruit examples
          
          clock formuals for calculating the face and hands
          modeled from Henning Karlsen
***************************************************************/          

//simple client 
//for use with IDE 1.0.1
//with DNS, DHCP, and Host
//open serial monitor and send an e to test
//for use with W5100 based ethernet shields

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include "Wire.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SPI.h>
#include <DHT.h>
#include <TouchScreen.h>
#include <avr/wdt.h>
#include <TextFinder.h>
#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

//#define DEBUG 1

File myFile;
File root;

byte mac[] = { 0x00, 0x00, 0x01, 0x03, 0x05, 0x02 }; //physical mac address
byte ip[] = { 192, 168, 0, 177 };         //Using a fixed ip
byte gateway[] = { 192, 168, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };
IPAddress mydns(192,168,0,1);
char serverName[] = "api.worldweatheronline.com";
EthernetClient client;
TextFinder finder( client ); //Setup TextFinder
char timeServer[] = "0.north-america.pool.ntp.org";  //Time server you wish to use
//Time Zone Selection
//const int TZ_OFFSET = 4*3600;  //AST UTC-4
//const int TZ_OFFSET = 5*3600;  //EST UTC-5
const int TZ_OFFSET = 6*3600;  //CST UTC-6
//const int TZ_OFFSET = 7*3600;  //MST UTC-7
//const int TZ_OFFSET = 8*3600;  //PST UTC-8
//const int TZ_OFFSET = 9*3600;  //AKST UTC-9
//const int TZ_OFFSET = 10*3600;  //HST UTC-10
//---------------------------------------------
//
const int NTP_PACKET_SIZE = 48;  //NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  //Buffer to hold incoming and outgoing packets
time_t prevDisplay;
EthernetUDP Udp;


#define DAYSINWEEK 8
#define CHARSINDAY 11
#define MONTHSINYEAR 14
#define CHARSINMONTH 6

static char shortdays[DAYSINWEEK][4] =
   {"Sat", "Sun", "Mon", "Tue", "Wed",
    "Thr", "Fri", "Sat" };

static char days[DAYSINWEEK][CHARSINDAY] =
   {"Saturday", "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday" };
static char months[MONTHSINYEAR][CHARSINMONTH] =
   {"Dec.", "Jan.", "Feb.", "March", "April", "May", "June", 
    "July", "Aug.", "Sept.", "Oct.", "Nov.", "Dec."};
static int daysofmo[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
#define YM A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YP 23   // can be a digital pin
#define XP 22   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940
// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
//TSPoint p;
// The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// When using the BREAKOUT BOARD only, use these 8 data lines to the LCD:
// For the Arduino Uno, Duemilanove, Diecimila, etc.:
//   D0 connects to digital pin 8  (Notice these are
//   D1 connects to digital pin 9   NOT in order!)
//   D2 connects to digital pin 2
//   D3 connects to digital pin 3
//   D4 connects to digital pin 4
//   D5 connects to digital pin 5
//   D6 connects to digital pin 6
//   D7 connects to digital pin 7
// For the Arduino Mega, use digital pins 22 through 29
// (on the 2-row header at the end of the board).

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
#define BOXSIZE 40
#define PENRADIUS 3
#define MINPRESSURE 10
#define MAXPRESSURE 1000

int oldcolor, currentcolor;

// If using the shield, all control and data lines are fixed, and
// a simpler declaration can optionally be used:
// Adafruit_TFTLCD tft;


// DECLARED VARIABLES HERE FOR GLOBAL USE
// define the pins used
//#define CLK 13  or 52     // SPI Clock, shared with SD card  
//#define MISO 12 or 50     // Input data, from VS1053/SD card
//#define MOSI 11 or 51     // Output data, to VS1053/SD card
// Connect CLK, MISO and MOSI to hardware SPI pins. 

// See http://arduino.cc/en/Reference/SPI "Connections"

// These can be any pins:
#define RESET 9      // VS1053 reset pin (output)
#define CS 10        // VS1053 chip select pin (output)
#define DCS 8        // VS1053 Data/command select pin (output)
#define CARDCS 43     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin


Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, CS, DCS, DREQ, CARDCS);

#define DHTPIN 2     // what pin we're connected to Changed from 2

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);
int dhtCtr = 0;
int menuCtr = 0;
#define MAXFILES 120
static char fileName[MAXFILES][13];
//static char fileSize[MAXFILES][9];
int fileCtr = 0;

char prevBuff[80];
char printBuff[80];

int humidityW;
char humidityC[4];
char observeTime[25];
int tempcW;
char tempcC[4];
int tempfW;
char tempfC[4];
int visibilityW;
char visibilityC[4];
char weatherDesc[30];
char winddir[10];
int windSpeedW;
char windSpeedC[4];
int feelsLikefW;
char feelsLikefC[4];
char sunSetC[10];
char sunRiseC[10];
char moonSetC[10];
char moonRiseC[10];

int prevHp = 0;
int prevTp = 0;
int texTx = 5;
int texTy = 120;
int texTs = 1;

int clockCenterX=120;   // Using 240 X 320 resolution
int clockCenterY=120;   // Center is offset to allow character lines of information
int xDiff = 0;
int yDiff = 0;
int roTation = 0;
int circleSize = 80;
int texTsize = 1;
int iNy = 210;
int timeDisplaYy = 15;
int dateDisplaYy = 5;     

boolean displayDigitalClock = false;
boolean grabhumidity = false;                   // switches used in buffer scan after a keyword is found
boolean grabtempc = false; 
boolean grabtempf = false;
boolean grabvisibility = false;
boolean grabwinddir16Point = false;
boolean grabwindspeed = false;  
boolean grabobservetime = false;  
boolean grabdesc = false;  
boolean setthetime = true;

int dayOfYear;                         // time variables
int mYsecond;
int mYminute;
int mYhour;
int mYweekDay;
int mYmonthDay;
int mYmonth;
int mYyear;
int prevSec;
int prevMin;
int prevHour;
int prevDay;

int weatherTempHighDay[32];          // array for collecting the month's hgh and low temperatures
int weatherTempLowDay[32];

String wRmm;                         // variables from reading in the weather log
String wRdd;
String wRyyyy;
String wRhr;
String wRmin;
String wRsec;
String wRobTime;
String wRdesc;
String wRtempc;
String wRtempF;
String wRvisibility;
String wRwinddir;
String wRwindspeed;
int monthLow=200;

// climate
#define DAYSINWEEK 8
#define CHARSINDAY 11
#define MONTHSINYEAR 14
#define CHARSINMONTHR 10
float absMaxTempc[12];
float absMaxTempf[12];
float avgMinTemp[12];
float avgMinTempf[12];
int monthIndex[12];
char monthName[MONTHSINYEAR][CHARSINMONTHR];

// current conditions
int cloudCover;
int feelsLikec;
float precipMM;
unsigned long pressure;
int weatherCode;
char weatherDescr[25];
char weatherIconurl[120];
int winddirDegree;
int windSpeedKmph;
int windspeedMiles;

// request
char query[50];
char type[25];

// weather astronomy
char moonRise[9];
char moonSet[9];
char sunRise[9];
char sunSet[9];
int forecastprecipMM[5];
int tempMaxC[5];
int tempMaxF[5];
int tempMinC[5];
int tempMinF[5];

char forecastdate[12][5];
int forecastweatherCode[8];
int forecastchanceofrain[8];
int forecastchanceofsnow[8];
int  maxtempC[5];
int  maxtempF[5];
int  mintempC[5];
int  mintempF[5];

void setup(){
 pinMode(CARDCS, OUTPUT);
 digitalWrite(CARDCS, HIGH);
 Serial.begin(115200);       // got set to this speed because an earlier version used the CC3300 wirles breakout


  Wire.begin();               // used at one time for a RTC board but found NTP works better. Now I don't necessariy know if it is still needed
  Serial.println(F("init"));
  Serial.println(F("TFT LCD test"));

#ifdef USE_ADAFRUIT_SHIELD_PINOUT
  Serial.println(F("Using Adafruit 2.8\" TFT Arduino Shield Pinout"));
#else
  Serial.println(F("Using Adafruit 2.8\" TFT Breakout Board Pinout"));
#endif

  tft.reset();                 // reset the tft display copied from Adafruit examples

  uint16_t identifier = tft.readID();

  if(identifier == 0x9325) {
    Serial.println(F("Found ILI9325 LCD driver"));
    } 
  else if(identifier == 0x9328) {
    Serial.println(F("Found ILI9328 LCD driver"));
  } else if(identifier == 0x7575) {
    Serial.println(F("Found HX8347G LCD driver"));
  } else if(identifier == 0x9341) {
    Serial.println(F("Found ILI9341 LCD driver"));
  } else {
     Serial.print(F("Unknown LCD driver chip: "));
     Serial.println(identifier, HEX);
     Serial.println(F("If using the Adafruit 2.8\" TFT Arduino shield, the line:"));
     Serial.println(F("  #define USE_ADAFRUIT_SHIELD_PINOUT"));
     Serial.println(F("should appear in the library header (Adafruit_TFT.h)."));
     Serial.println(F("If using the breakout board, it should NOT be #defined!"));
     Serial.println(F("Also if using the breakout, double-check that all wiring"));
     Serial.println(F("matches the tutorial."));
     return;
     }

  tft.begin(identifier);          // Let's start the tft
  
  tft.setTextColor(WHITE, BLUE);  // all text is white over blue background
  tft.setRotation(roTation);       // using default rotation but could easily change. Would require adjustment to face and numbers
  tft.setTextSize(texTsize);

  uint16_t time = millis();
  tft.fillScreen(BLUE);
  time = millis() - time;
  prevSec = mYsecond;
  prevMin = mYminute;
  prevHour = mYhour;
  Serial.println(time, DEC);
  delay(500);
  drawFace();                            // Here we go draw the clock face

  Serial.println(F("Init VS1053"));
  musicPlayer.begin(); // initialise the music player
  SD.begin(CARDCS);    // initialise the SD card
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);

  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
// musicPlayer.playFullFile("track001.mp3");
  Serial.println(F("Ethernet begin"));
  digitalWrite(10, LOW);
  Ethernet.begin(mac, ip, mydns, gateway, subnet);
  Serial.println(F("UDP Begin"));
  Udp.begin(8888);
  delay(3000);  
  Serial.println(F("Sync interval"));
  setSyncInterval(7200);  //Every 2 hours
  setSyncProvider (requestNTP);
  Serial.print(F("timeStatus = "));
  Serial.println(timeStatus());
  texTx = 5;                                   // TFT Display info
  texTy = iNy+10;
  tft.setCursor(texTx, texTy);
  tft.print(F("timeStatus = "));
  tft.print(timeStatus());

  while (timeStatus() < 2);  //Wait for time sync
  prevDisplay = now();
  getTime();
  getWeather();

 mYhour = 13;
 playHourly();
 getTime();

}

void loop() {
 
  checkFortouch(); 
  displayInsidetemp ();               // display the humidity and temperature from the indor sensor DTH22
  getTime();                          // call the get time routine0
  if (prevSec != mYsecond) {          // of it is a different second thant the previous do the clock thing
      printDate();
      drawSec();
      drawHour();
      drawMin();
      if (mYminute == 0 && mYsecond == 0){     // Start looking for things to do at specific times
       //   getWeather();                        // get weather data on the hour then
          playHourly();                        // play the hourly chimes defined
          }
         
      if (mYminute == 2) {          // get weather data on the 1/4 hour
         if (mYsecond == 0){
#ifdef Debug
             Serial.print(mYmonth);  Serial.print(F("/"));  Serial.print(mYmonthDay); Serial.print(F("/"));
             Serial.print(mYyear);  Serial.print(F(" "));   Serial.print(mYhour);  Serial.print(F(":"));  Serial.print(mYminute);
             Serial.print(F(":"));  Serial.println(mYsecond);
#endif  
             getWeather();

#ifdef DEBUG
             Serial.print(mYmonth);  Serial.print(F("/"));  Serial.print(mYmonthDay); Serial.print(F("/"));
             Serial.print(mYyear);  Serial.print(F(" "));   Serial.print(mYhour);  Serial.print(F(":"));  Serial.print(mYminute);
             Serial.print(F(":"));  Serial.println(mYsecond);
#endif
        }
      }
  }
}

  
void playHourly(){                                         // time the play the chimes unless it is 11 on Ina's or my birthday
  musicPlayer.begin(); // initialise the music player
  SD.begin(CARDCS);    // initialise the SD card
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);

  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
 
  digitalWrite(10, HIGH);                                 // Chimes are played 10 A.M. to 8 P.M.
//#ifdef DEBUG
  Serial.println(F("Starting Music"));
//#endif
  if (mYhour == 10) musicPlayer.playFullFile("houra.mp3");
  if (mYhour == 11) {
      if (mYmonth == 7 && mYmonthDay == 31) {
          musicPlayer.playFullFile("Marty.mp3");
      }
      else {
        if (mYmonth == 11 && mYmonthDay == 2) {
          musicPlayer.playFullFile("Ina.mp3");
        }
    else {
     musicPlayer.playFullFile("hourb.mp3");
         }
      }
  }
  if (mYhour == 12) musicPlayer.playFullFile("hourc.mp3");
  if (mYhour == 13) musicPlayer.playFullFile("hour1.mp3");
  if (mYhour == 14) musicPlayer.playFullFile("hour2.mp3");
  if (mYhour == 15) musicPlayer.playFullFile("hour3.mp3");
  if (mYhour == 16) musicPlayer.playFullFile("hour4.mp3");
  if (mYhour == 17) musicPlayer.playFullFile("hour5.mp3");
  if (mYhour == 18) musicPlayer.playFullFile("hour6.mp3");
  if (mYhour == 19) musicPlayer.playFullFile("hour7.mp3");
  if (mYhour == 20) musicPlayer.playFullFile("hour8.mp3");     
//  if (mYhour == 21) musicPlayer.playFullFile("hour9.mp3");   
  musicPlayer.stopPlaying();

  }

void startEthernet() {                    // need to be able to start the ethernet client only when it is time to get something
  Ethernet.begin(mac, ip);                //    udp is allways active
  Serial.print(F("Connected"));
}


void getTime() {                  //  Systemclock updated by ntp request easy to switch to rtc      

  mYsecond = second();
  mYminute = minute();
  mYhour = hour();
  mYweekDay = weekday();
  mYmonthDay = day();
  mYmonth = month();
  mYyear = year();

  }

void printDate(){
  //print the date EG   3/1/11 23:59:59
// #ifdef DEBUG
if (displayDigitalClock == true) {
  texTx = 100;
  texTy = dateDisplaYy+10;
  tft.setCursor(texTx, texTy);
  if (mYhour < 10) tft.print("0");
      tft.print(mYhour);
      tft.print(":");
      if (mYminute < 10) tft.print("0");
      tft.print(mYminute);
      tft.print(":");
      if (mYsecond < 10) tft.print("0");
      tft.print(mYsecond);
}
//#endif

   
  if (mYmonthDay != prevDay) {                                     // If the date has changed Erase the previous date display
      prevDay = mYmonthDay;                                       // and print the new date
      texTx = 5;
      texTy = dateDisplaYy;
      tft.setCursor(texTx, texTy);
      tft.print(months[mYmonth]);
      tft.print(" ");
      if (mYmonthDay < 10) tft.print("0");
      tft.print(mYmonthDay);
      tft.print(", ");
      tft.print(mYyear);

      texTx = 105;
      tft.setCursor(texTx, texTy);
      tft.print(days[mYweekDay]);
      tft.print("    ");
      
      texTx = 190;
      tft.setCursor(texTx, texTy);
      tft.print("Day ");
      int jd = daysofmo[mYmonth] + mYmonthDay;
       if (checkLeapYear(mYyear) == true) jd++;
      tft.print(jd);
  }
}  

void checkFortouch() {             // check if any pressure point was pressed. if yes call options menu
                                   // when returning from options menu clear the screeen, build the clock face
                                   // reset the time compares to force display of items that change only when certain
                                   // parts need to be displayed i.e. top date line info only changes when the
                                   // day changes
  digitalWrite(13, HIGH);          
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
       Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  if (p.z !=0) {
      if (p.y >600) {
        forecast();
        tft.fillScreen(BLUE);                      // we are back from options
        drawFace();                           // re-do the setup stuff
        prevDisplay = now();
        getTime();
        client.stop();
        client.flush();
        SD.begin(CARDCS);    // initialise the SD card
        getWeather();
        prevSec = 99;
        prevDay = 99;
        return;
      }
      menuCtr = 1;                                // here we go to the options menu
      displayOptions();
      tft.fillScreen(BLUE);                      // we are back from options
      drawFace();                           // re-do the setup stuff
      prevDisplay = now();
      getTime();
      client.stop();
      client.flush();
      SD.begin(CARDCS);    // initialise the SD card
      getWeather();
      prevSec = 99;
      prevDay = 99;
#ifdef DEBUG
     Serial.println("end of touch");
#endif     
  }                               // end of it pressure check
  
}

void menuShell(boolean left, boolean right){            // common top & bottom of data screens
 int x = 5;
 int y = 10;
 int radius = 4;
 int height = 20;
 int width = 60;
 tft.fillRect(0, 0, 0, 60, BLUE);
 if (left == true) {
  tft.fillRect(x-2, y-2, width+5, height+4, BLACK);
  tft.fillRect(x, y, width, height, WHITE);
 }
 if (right == true){
  tft.fillRect(x+163, y-2,width+5, height+4, BLACK);  
 tft.fillRect(x+165, y, width, height, WHITE);
 }

 height = 13;
 width = 160;
 x = 30;
 y = 303;

    tft.fillRoundRect(x-2, y-2, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y, width, height,radius, WHITE);
 
 tft.setTextColor(BLACK, WHITE);
 if (left == true) {
   texTx = 25;
   texTy = 15;
   tft.setCursor(texTx, texTy);
   tft.print("Prev");
 }
 if (right == true) {
     texTx=190;
     texTy = 15;
     tft.setCursor(texTx, texTy);
     tft.print("Next");
  }
  tft.setTextColor(WHITE, BLUE);
}

void playMusic(int musicSelection){
  musicPlayer.begin(); // initialise the music player
  SD.begin(CARDCS);    // initialise the SD card
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);

  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  digitalWrite(10, HIGH);
//#ifdef DEBUG
  Serial.println(F("Starting Music"));
//#endif
  if (musicSelection == 1) musicPlayer.playFullFile("track001.mp3");     // Puff the magic dragon    
  if (musicSelection == 2) musicPlayer.playFullFile("popular.mp3");      // Popular from Wicked
  if (musicSelection == 3) musicPlayer.playFullFile("wworld.mp3");       // It's a Wonderful World
  if (musicSelection == 4) {
     mYhour = 12;
     playHourly();
  }
  if (musicSelection == 5) musicPlayer.playFullFile("Rainbow.mp3");      // Hawian Somewhere over the rainbow
  if (musicSelection == 6) musicPlayer.playFullFile("ina.mp3");          // happy birthday to Ina
  musicPlayer.stopPlaying();
 
}


void drawMygraph(int lowTemp,int xCorr1, int yCorr1) {     // draw a monthly temperature graph

 int x = xCorr1;
 int y = yCorr1;
 int yTemp = 0;
 tft.drawLine(x,y, x, y-50, WHITE);                         // lay out the x and y axis
 tft.drawLine(x,y, x+130, y, WHITE);
 tft.drawLine(x - 5,y-11, x+5, y-11, WHITE);
 tft.drawLine(x - 5,y-21, x+5, y-21, WHITE);
 tft.drawLine(x - 5,y-31, x+5, y-31, WHITE);
 tft.drawLine(x - 5,y-41, x+5, y-41, WHITE);
 tft.drawLine(x +21,y-5, x+21, y+5, WHITE);
 tft.drawLine(x +41,y-5, x+41, y+5, WHITE);
 tft.drawLine(x +61,y-5, x+61, y+5, WHITE); 
 tft.drawLine(x +81,y-5, x+81, y+5, WHITE);
 tft.drawLine(x +101,y-5, x+101, y+5, WHITE);  
 tft.drawLine(x +121,y-5, x+121, y+5, WHITE);  
 tft.drawLine(x + 130, y - 20, x+150, y-20, RED);            // legends
 tft.drawLine(x + 130, y - 30, x+150, y-30, GREEN);
 tft.setCursor(x +158, y - 35);
 tft.print("Highs");
 tft.setCursor(x +158, y - 25);
 tft.print("Lows");
 tft.setCursor(x +138, y-3);
 tft.print(months[mYmonth]);
 tft.setCursor(x+18, y+10);
 tft.print("5");
 tft.setCursor(x+34, y+10);
 tft.print("10");
 tft.setCursor(x+56, y+10);
 tft.print("15");
 tft.setCursor(x+74, y+10);
 tft.print("20");
 tft.setCursor(x+96, y+10);
 tft.print("25");
 tft.setCursor(x+116, y+10);
 tft.print("30");

//  Serial.print("--> ");Serial.print(yTemp); Serial.print(" ");  Serial.println(lowTemp);
 yTemp = (lowTemp/10)*10;
//   Serial.print("--> ");Serial.print(yTemp); Serial.print(" ");  Serial.println(lowTemp);
 if (yTemp < 0) yTemp = 0;
 tft.setCursor(x-25, y-14);
 yTemp = yTemp+10;
// Serial.print("--> ");Serial.print(yTemp); Serial.print(" ");  Serial.println(lowTemp);
 tft.print(yTemp);
 tft.setCursor(x-25, y-35);
 tft.print(yTemp+20);
 int curx=0;

 for (int d = 1 ; d <32; d++) {                    // Now spin through the array will be used for plotting
/*
 if (weatherTempLowDay[d] == -25){               // used in testing
   weatherTempLowDay[d] = 70;
 } 
  if (weatherTempHighDay[d] == 120){
   weatherTempHighDay[d] = 70;
 }
*/                                                         //   Y shifts on the monthly low start at rounded down 10
                                                           // if low was 49 then first legend is 50
 if (weatherTempLowDay[d] != -25 && weatherTempLowDay[d+1] != -25 && weatherTempLowDay[d+1] < 110 ) {
//     Serial.print("X = "); Serial.print(curx+x); Serial.print(" y = "); Serial.println((y+lowTemp)-weatherTempLowDay[d]);
   tft.drawLine(curx+x,((y-8+lowTemp)-weatherTempLowDay[d]),curx+x+4,((y-8+lowTemp)-weatherTempLowDay[d+1]), RED);
 }
  if (weatherTempHighDay[d] != 120 && weatherTempHighDay[d+1] != 120 && weatherTempHighDay[d+1] > -10) {  
   tft.drawLine(curx+x,((y-8+lowTemp)-weatherTempHighDay[d]+1),curx+x+4,((y-8+lowTemp)-weatherTempHighDay[d+1]+1), GREEN);
  }
    curx = curx+4;
    }
  }    

 void drawMonthgraph(int xCorr2,int yCorr2){   // print a requested month at a certain x y coordinate
   int x = xCorr2;
   int y = yCorr2;
   int m = mYmonth;
      switch (m) {
    case 1:
        listFile("jan.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 2:
        listFile("feb.txt");
        drawMygraph(monthLow, x, y);
        break; 
    case 3:
        listFile("march.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 4:
        listFile("april.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 5:
        listFile("may.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 6:
        listFile("june.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 7:
        listFile("july.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 8:
        listFile("aug.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 9:
        listFile("sept.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 10:
        listFile("oct.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 11:
        listFile("nov.txt");
        drawMygraph(monthLow, x, y);
        break;
    case 12:
        listFile("dec.txt");
        drawMygraph(monthLow, x, y);
        break;
    default:
        listFile("jan.txt");
        drawMygraph(monthLow, x, y);
   }
 }

void displayTempgraphs(){                  // display temperature history
#ifdef DEBUG
Serial.println("Graphs");
#endif
 tft.fillScreen(BLUE);                        // clear the screen
  if (menuCtr == 1) {
    menuShell(0, 1);
  }
 drawMenugraphs(menuCtr);               // draw the graphs for thrre months

  mYmonth = month();
  while (1) {  
 
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
#ifdef DEBUG
    Serial.print(tft.width()); Serial.print(" "); 
    Serial.println(tft.height());
      Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
#endif  
    
    // scale from 0->1023 to tft.width
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY,0, tft.height());
#ifdef DEBUG   
      Serial.print("("); Serial.print(p.x);
      Serial.print(", "); Serial.print(p.y);
      Serial.println(")");
#endif   
      if (p.x < 230 && p.y <70) {        // if user pressed previous selection and menu ctr goes less than one
          menuCtr--;
          if (menuCtr < 1) {
              return;                     // return to options meneu
          } else {
                if (menuCtr == 1) {      // display previous option or not
                   menuShell(0, 1);
                   }
                   else menuShell(1,1);
              drawMenugraphs(menuCtr); 
          }
       } else if (p.x >240 && p.y <70) {   // user pressed page forward
         menuCtr++;
          if (menuCtr > 10) {              // ran out of months so go back to options menu
              return;
          }
         if (menuCtr == 10) {
             menuShell(1, 0);
             }
             else menuShell(1,1);
         drawMenugraphs(menuCtr); 
       } else if (p.y >260) {
         return;
         break;
       }
 
   }     //  end of pressure test

  }   // end of menu   while
}    

void drawMenugraphs(int whichMonth) {           //  display three months of data

 tft.fillRect(0, 60, 240, 240, BLUE);
 texTx = 60;
 texTy = 305;
 tft.setCursor(texTx, texTy);
 tft.setTextColor(BLACK, WHITE);
 tft.print("Return to Options");
 tft.setTextColor(WHITE, BLUE);

  mYmonth = whichMonth;
  drawMonthgraph(50, 120);
  mYmonth++;
  drawMonthgraph(50, 190);
  mYmonth++;
  drawMonthgraph(50, 260);

}

void menuSounds(){                           // sound menu
#ifdef DEBUG
  Serial.println("menuSound");  
#endif
  tft.fillScreen(BLUE);
  if (menuCtr == 1) {
    menuShell(0, 0);
  }
 texTx = 60;
 texTy = 305;
 tft.setCursor(texTx, texTy);
 tft.setTextColor(BLACK, WHITE);
 tft.print("Return to options");


int x = 30;
int y = 75;
int radius = 4;
int height = 20;
int width = 175;
    tft.fillRoundRect(x-2, y-2, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y, width, height,radius, WHITE);          //draw the   menu

    tft.fillRoundRect(x-2, y+38, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+40, width, height,radius, WHITE);          //draw the   menu

    tft.fillRoundRect(x-2, y+78, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+80, width, height,radius, WHITE);          //draw the   menu

    tft.fillRoundRect(x-2, y+118, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+120, width, height,radius, WHITE);          //draw the   menu

    tft.fillRoundRect(x-2, y+158, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+160, width, height,radius, WHITE);          //draw the   menu


  x = 0;
  y = 0;
  width = 44;
  height = 20;
  texTx = 100;
  texTy = 25;
  tft.setCursor(texTx, texTy);
  tft.fillRect(x+94, y+18, width+4, height+4, BLACK);
  tft.fillRect(x+96, y+20, width, height, WHITE);          //draw the   menu
  tft.print("Sounds");

  texTx = 40;
  texTy = 80;
  tft.setCursor(texTx, texTy);
  tft.print("Puff the Magic Dragon");

  texTx = 40;
  texTy = 120;
  tft.setCursor(texTx, texTy);
  tft.print("Popular");

  texTx = 40;
  texTy = 160;
  tft.setCursor(texTx, texTy);
  tft.print("What A Wonderful World");

  texTx = 40;
  texTy = 200;
  tft.setCursor(texTx, texTy);
  tft.print("Chimes");

  texTx = 40;
  texTy = 240;
  tft.setCursor(texTx, texTy);
  tft.print("Somewhere Over the Rainbow");
   tft.setTextColor(WHITE, BLUE);
 
    while (1) {                      // menu loop
 
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
 
   if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {

#ifdef DEBUG      
      Serial.print("Before Map X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
#endif
    // scale from 0->1023 to tft.width
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY,0, tft.height());
//#ifdef DEBUG
      Serial.print("After Map X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
//#endif
      if (p.x < 230 && p.y <70) {                       // user pressed page back 
          menuCtr--;
          if (menuCtr < 1) {
              break;
              } 
              else 
                 {
                  drawMenugraphs(menuCtr); 
                  }
              }
         if (p.x >240 && p.y <70) {                      // user press page forward
             menuCtr++;
             if (menuCtr > 1) {                         // ran out of pages
                 break;
                 }
             drawMenugraphs(menuCtr); 
             } 
         if (p.y >285) {

           Serial.println(p.y);

             menuCtr = 1;
             break;
             }
 
         if (p.y > 70 && p.y < 90) {

           Serial.println(p.y); 

             playMusic(1);
             p.y = 800;           // ensure only this song plays
             }
         if (p.y > 110 && p.y < 130) {

           Serial.println(p.y);

            playMusic(2);
             p.y = 800;           // ensure only this song plays
            }
         if (p.y >150 && p.y < 170) {

            Serial.println(p.y);

            playMusic(3);
             p.y = 800;           // ensure only this song plays
             }
         if (p.y >190 && p.y < 210) {

            Serial.println(p.y);

            playMusic(4);
             p.y = 800;           // ensure only this song plays
             }
         if (p.y > 230 && p.y < 250) {

            Serial.println(p.y);

            playMusic(5);
             p.y = 800;           // ensure only this song plays
             }

   }            // end of menu while loop

  }  // end of menu

}

void displayDirectory(){                  // display SD Directory
#ifdef DEBUG
Serial.println("Display Directory");
#endif
 tft.fillScreen(BLUE);
  if (menuCtr == 1) {
    menuShell(1, 1);
  }
  int x = 0;
  int y = 0;
  int width = 86;
  int height = 15;
  texTx = 80;
  texTy = 40;
  tft.setCursor(texTx, texTy);
  tft.setTextColor(BLACK, WHITE);
  tft.fillRect(x+70, y+34, width+4, height+4, BLACK);
  tft.fillRect(x+72, y+35, width, height, WHITE);          //draw the   menu
  tft.print("SD Directory");

 displayFilelist(menuCtr);
  
  while (1) {  
 
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
#ifdef DEBUG
    Serial.print(tft.width()); Serial.print(" "); 
    Serial.println(tft.height());
      Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
#endif  
    // scale from 0->1023 to tft.width
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY,0, tft.height());
#ifdef DEBUG   
      Serial.print("("); Serial.print(p.x);
      Serial.print(", "); Serial.print(p.y);
      Serial.println(")");
#endif   
      if (p.x < 230 && p.y <70) {        // if user pressed previous selection and menu ctr goes less than one
          menuCtr--;
          if (menuCtr < 1) {
              return;                     // return to options meneu
              } 
          if (menuCtr == 1) {      // display previous option or not
              menuShell(0, 1);
              }
            else menuShell(1,1);
          tft.setTextColor(BLACK, WHITE);
          displayFilelist(menuCtr); 
          tft.setTextColor(WHITE, BLUE);
          }
          else if (p.x >240 && p.y <70) {   // user pressed page forward
          menuCtr++;
          if ((menuCtr * 60) - 60 > fileCtr) {              // ran out of months so go back to options menu
              return;
          }
         if (menuCtr == 10) {
             menuShell(1, 0);
             }
             else menuShell(1,1);
         tft.setTextColor(BLACK, WHITE);

         displayFilelist(menuCtr);  
       } else if (p.y >260) {
         return;
         break;
       }
 
   }     //  end of pressure test

  }   // end of menu   while
}    

void displayFilelist(int menuCtr) {
  tft.fillRect(0, 60, 240, 240, BLUE);
  texTx = 60;
  texTy = 305;
  tft.setCursor(texTx, texTy);
  tft.print("Return to options");
  tft.setTextColor(WHITE, BLUE);

  int x = 5;
  int y = 70;
  int fileIndex = (menuCtr * 60) - 60;
  tft.setTextColor(WHITE, BLUE);
  for(int nl = 0; nl < 3; nl++) {
   for (int l = 0; l < 20; l++) {
    tft.setCursor(x, y);
    tft.print(fileName[fileIndex]);
/*
    tft.setCursor(x + 72, y);                  // just in case I want to display sizes
    tft.print(fileSize[fileIndex]);
*/
    fileIndex++;
    if (fileIndex > fileCtr) break;
    y = y+10;
  }    
  x = x + 75;
  y = 70;
 }

}

void fileLookup(){                // scan the directory of the SD
 fileCtr = 0;
  root = SD.open("/");
  printDirectory(root, 0);
  selectionSort2d();
  Serial.println("done!");
}

void printDirectory(File dir, int numTabs) {    // Extract filenames and sizes - display of sizes takes up too much real estate
  // Begin at the start of the directory
  dir.rewindDirectory();
  
  while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');   // we'll have a nice indentation
     }
     // Print the 8.3 name
     Serial.print(entry.name());
     // Recurse for directories, otherwise print the file size
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
       strcpy(fileName[fileCtr], entry.name());
/*
       char tempBuff[16];
       dtostrf(entry.size(), 8, 0, tempBuff);
       strcpy(fileSize[fileCtr], tempBuff);
*/
       fileCtr++;
       if (fileCtr > MAXFILES) break;
     }
     entry.close();
   }
}
void selectionSort2d() {
    int i, j, min;
    char t[13];
    char fs[9];
    for (i = 0; i < fileCtr; i++) {
        min = i;
        for (j = i+1; j < fileCtr; j++) {
            if (strcmp(fileName[j],fileName[min]) < 0) {
                min = j;
            }
        }
        strcpy(t, fileName[min]);
  //      strcpy(fs, fileSize[min]);
        strcpy(fileName[min], fileName[i]);
  //      strcpy(fileSize[min], fileSize[i]);
        strcpy(fileName[i], t);
  //      strcpy(fileSize[i], fs);
    }
}

void displayOptions(){              // Options menu
#ifdef DEBUG
  Serial.println("Options Menu");  
#endif
   drawOptions();
    while (1) {                                     // stay in this loop until the user selects the return to options 
                                                   // by pressing at the botton of display 
#ifdef DEBUG
 Serial.println("Options While");
#endif
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
#ifdef DEBUG
      Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
#endif

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    // scale from 0->1023 to tft.width
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY,0, tft.height());
#ifdef DEBUG
      Serial.print("Options After Map X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
#endif
         if (p.y >285) {                            // check if bottom pressed
#ifdef DEBUG
             Serial.println(p.y);
#endif
             menuCtr = 0;
             return;
             }

         if (p.y < 100) {                           // user selected graphs
#ifdef DEBUG
             Serial.println(p.y);
#endif  
             menuCtr = 1;
             displayTempgraphs();             // Display the temperature graphs
#ifdef DEBUG
             Serial.println("back from graphs");
#endif
             menuCtr = 1;
             drawOptions();                    // refresh this menu
             p.y = 280;                       // ensure we stay in the while loop 
             }                                // and don't go to another option

         if (p.y < 140) {                       // user selectd sounds
#ifdef DEBUG
            Serial.println(p.y);
#endif
            menuCtr = 1;
            menuSounds();                        // go tothe Sounds menu
            menuCtr = 1;
            drawOptions();                        // refresh this menu
            p.y = 280;
#ifdef DEBUG
            Serial.println("Back from sounds");
#endif
            }
         if (p.y < 180) {
            menuCtr = 1;
            fileLookup();
            displayDirectory();
            menuCtr = 1;
            drawOptions();                        // refresh this menu
             p.y = 280;           // ensure only this menu only
#ifdef DEBUG
            Serial.println("Back from file directory");
#endif
             }
         if (p.y < 220) {
            menuCtr = 1;
            forecast();
            menuCtr = 1;
            drawOptions();                        // refresh this menu
             p.y = 280;           // ensure only this menu only
#ifdef DEBUG
            Serial.println("Back from file forecast");
#endif
             }
         if (p.y < 280) {
            if (displayDigitalClock == true) {
              displayDigitalClock = false;
            }
            else {
              displayDigitalClock = true;
            }
            drawOptions();                        // refresh this menu
             p.y = 280;           // ensure only this menu only
#ifdef DEBUG
            Serial.println("Back from file directory");
#endif
             }
             

#ifdef DEBUG
            Serial.println("Options end of pressure");
#endif
   }                                   // end of if pressure
#ifdef DEBUG
        Serial.println("Options end of while 1");
#endif
  }// end of menu while
}


void drawOptions() {                 // Display options menu
 tft.fillScreen(BLUE);               // clear the screen
    if (menuCtr == 1) {              // if displaying first page of menus do not display page back info
    menuShell(0, 0);                 // draw the bottom button, no prev or next
  }
  texTx = 70;                        // text for bottom button
  texTy = 305;
  tft.setCursor(texTx, texTy);
  tft.setTextColor(BLACK, WHITE);
  tft.print("Return to Clock    ");

  int x = 40;                      // construct the menu
  int y = 75;
  int radius = 4;
  int height = 20;
  int width = 140;                  // draw the buttons
    tft.fillRoundRect(x-2, y-2, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y, width, height,radius, WHITE);          
 
    tft.fillRoundRect(x-2, y+38, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+40,width, height,radius, WHITE);


    tft.fillRoundRect(x-2, y+78, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+80,width, height,radius, WHITE);

    tft.fillRoundRect(x-2, y+118, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+120,width, height,radius, WHITE);

    tft.fillRoundRect(x-2, y+158, width+5, height+4,radius, BLACK);
  tft.fillRoundRect(x, y+160,width, height,radius, WHITE);


  x = 0;                         // display the option item text
  y = 0;
  width = 48;
  height = 20;
  texTx = 100;
  texTy = 25;
  tft.setCursor(texTx, texTy);
  tft.fillRect(x+94, y+18, width+4, height+4, BLACK);
  tft.fillRect(x+96, y+20, width, height, WHITE);        
  tft.print("Options");

  texTx = 50;
  texTy = 80;
  tft.setCursor(texTx, texTy);
  tft.print("Tempurature Graphs");

  texTx = 80;
  texTy = 120;
  tft.setCursor(texTx, texTy);
  tft.print("Sounds");

  texTx = 60;
  texTy = 160;
  tft.setCursor(texTx, texTy);
  tft.print("SD Directory");

  texTx = 60;
  texTy = 200;
  tft.setCursor(texTx, texTy);
  tft.print("Forecast");

  texTx = 45;
  texTy = 240;
  tft.setCursor(texTx, texTy);
  if (displayDigitalClock == false) {
    tft.print("Display Digital Clock");
  }
  else {
   tft.print("Turn Off Digital Clock");
  }
  
  tft.setTextColor(WHITE, BLUE);
}

//
unsigned long requestNTP() 
{ 
  //Send NTP packet
  //Set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  //Init values needed to form NTP request
  packetBuffer[0] = 0b11100011;  //LI, Version, Mode
  packetBuffer[1] = 0;  //Stratum, or type of clock
  packetBuffer[2] = 6;  //Polling Interval
  packetBuffer[3] = 0xEC;  //Peer Clock Precision
  //Skip 8 bytes and leave zeros for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  //All NTP fields have been given values, now
  //you can send a packet requesting a timestamp: 		   
  Udp.beginPacket(timeServer, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket(); 
  //Wait to see if a reply is available
  delay(250);  //Adjust this delay for time server (effects accuracy, use shortest delay possible)
  if (Udp.parsePacket())
  {  
    //We've received a packet, read the data from it
    Serial.println(F("NTP Packet received"));
    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    //The timestamp starts at byte 40 of the received packet and is four bytes,
    //or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    //Combine the four bytes (two words) into a long integer
    //This is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Now convert NTP time into everyday time:
    //Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long SEVENTY_YEARS = 2208988800UL;     
    //Subtract seventy years:
    unsigned long epoch = secsSince1900 - SEVENTY_YEARS;
    epoch = epoch - TZ_OFFSET;
    epoch = epoch + dstOffset(epoch);  //Adjust for DLT
    return epoch;
  }
 return 0; 
}
int dstOffset (unsigned long unixTime)
{
  //Receives unix epoch time and returns seconds of offset for local DST
  //Valid thru 2099 for US only, Calculations from "http://www.webexhibits.org/daylightsaving/i.html"
  //Code idea from jm_wsb @ "http://forum.arduino.cc/index.php/topic,40286.0.html"
  //Get epoch times @ "http://www.epochconverter.com/" for testing
  //DST update wont be reflected until the next time sync
  time_t t = unixTime;
  int beginDSTDay = (14 - (1 + year(t) * 5 / 4) % 7);  
  int beginDSTMonth=3;
  int endDSTDay = (7 - (1 + year(t) * 5 / 4) % 7);
  int endDSTMonth=11;
  if (((month(t) > beginDSTMonth) && (month(t) < endDSTMonth))
    || ((month(t) == beginDSTMonth) && (day(t) > beginDSTDay))
    || ((month(t) == beginDSTMonth) && (day(t) == beginDSTDay) && (hour(t) >= 2))
    || ((month(t) == endDSTMonth) && (day(t) < endDSTDay))
    || ((month(t) == endDSTMonth) && (day(t) == endDSTDay) && (hour(t) < 1)))
    return (3600);  //Add back in one hours worth of seconds - DST in effect
  else
    return (0);  //NonDST
}

 void listFile(char *filename) {
Serial.println("Listing ");
Serial.println(filename);
  pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
   // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  for (int x = 0 ; x <32; x++) {
       weatherTempHighDay[x] = 120;
       weatherTempLowDay[x]  = -25;
       }
  File myFile = SD.open(filename);
  String buff = "";
  int foundIndex = 0;
  int searchIndex = 0;
  int j;
  monthLow = 200;
  // if the file is available, read it
  if (myFile) {
    while (myFile.available()) {
      char c = myFile.read();
     
      if (c == 0x0D || c == 0x0A) {
        j = buff.length();
        if (j > 0) {
          wRdd = buff.substring(3, 5);
//          find first comma
          searchIndex = buff.indexOf(",");
          for (int x = 0; x < 3; x++) {
          foundIndex = buff.indexOf(",", searchIndex + 1);
          searchIndex = buff.indexOf(",", foundIndex + 1);  
          } 
          wRtempF = buff.substring(foundIndex + 1, searchIndex);
          int mo = wRdd.toInt();
          int tempf = wRtempF.toInt();
//          Serial.print(tempf); Serial.print(" "); Serial.println(monthLow);
        if (tempf > -10 && tempf < 110) {
          if (tempf < monthLow) monthLow = tempf;
          if (weatherTempHighDay[mo] == 120) weatherTempHighDay[mo] = tempf;  // prime the array for the day high and low
          if (weatherTempLowDay[mo] == -25) weatherTempLowDay[mo] = tempf;
          if (tempf > weatherTempLowDay[mo]) weatherTempHighDay[mo] = tempf;   // figure out if the current read char is high or low
          if (tempf < weatherTempHighDay[mo]) weatherTempLowDay[mo] = tempf;   
        }
          buff = "";
        }
       c = myFile.read();
        buff = "";
      }
      else
      buff += c;
     }
    myFile.close();
      for (int x = 0 ; x <32; x++) {                    // Now spin through the array willbe used for plotting
       if (weatherTempLowDay[x] != -25) {

         Serial.print(x);
           Serial.print(" ");
           Serial.print(weatherTempLowDay[x]);
           Serial.print(" ");
           Serial.println(weatherTempHighDay[x]);

          }
       }

  }  
  // if the file isn't open, pop up an error:
  else {
    Serial.print("error opening "); Serial.println(filename);
  } 
}

//////////////////////////

void getWeather() //client function to send/receive GET request data.
{
 
  if (client.connect(serverName, 80)) {  //starts client connection, checks for connection
      Serial.println(F("connected"));
//      client.println("GET http://api.worldweatheronline.com/premium/v1/weather.ashx?q=skokie&format=json&num_of_days=5&tp=tp%3D12&key=cyc4ezspmb6hnehnyxtevejh HTTP/1.1"); //download text premium key
      client.println("GET /free/v1/weather.ashx?q=skokie&format=json&num_of_days=5&key=GETYOUROWNKEY HTTP/1.1"); //download text free key
      client.println("Host: api.worldweatheronline.com");
      client.println("Connection: close");  //close 1.1 persistent connection  
      client.println(); //end of get request
      } 
  else {
      Serial.println(F("connection failed")); //error message if no client connect
      Serial.println();
  }

  Serial.println(F("Reading web"));
 
  while (client.connected()) { 
   while(client.available()) { //connected or data available
/*

for (int i = 0; i < 12; i++) {  // read each month's Max temp and average tep
   finder.find("absMaxTemp");
   absMaxTempc[i] = (finder.getValue());

  finder.find("absMaxTemp_F");
  absMaxTempf[i] = (finder.getValue());

  finder.find("avgMinTemp");
  avgMinTemp[i] = (finder.getValue());

  finder.find("avgMinTemp_F");
  avgMinTempf[i] = (finder.getValue());

 finder.find("index");
 monthIndex[i] = (finder.getValue()); 

 finder.find("name");
 finder.getString(" \"", "\"",  monthName[i],(9));
   
}                                                            // get current weather conditions
*/
       finder.find("cloudcover");  //                     Cloud Cover
        cloudCover = (finder.getValue());
/*
       finder.find("FeelsLikeC");  //                     Feels like Centirade
        feelsLikec = (finder.getValue());

     finder.find("FeelsLikeF");  //                     Feels like F
        feelsLikefW = (finder.getValue());
      dtostrf(feelsLikefW, 2, 0, feelsLikefC);        
*/
     finder.find("humidity");  //                        humidity
        humidityW = (finder.getValue());
       dtostrf(humidityW, 2, 0, humidityC);        
       
     observeTime[1] = '\0';   //                               Observation Time
     finder.find("observation_time");
     finder.find(":");
     finder.getString(" \"", "\"", observeTime,(9));    

     finder.find("precipMM");  //                     Percipitation in MM
      precipMM = (finder.getValue());


    finder.find("pressure");  //                     Pressure
      pressure = (finder.getValue());
    
     finder.find("temp_C");  //                    temp in Centigrade
        tempcW = (finder.getValue());
       dtostrf(tempcW, 2, 0, tempcC);        

     finder.find("temp_F");  //                    temp in Farenhite
        tempfW = (finder.getValue());
       dtostrf(tempfW, 2, 0, tempfC);        
       
      finder.find("visibility");  //                        visibility
        visibilityW = (finder.getValue());
       dtostrf(visibilityW, 2, 0, visibilityC);        

       finder.find("weatherCode");  //                     Weather code
        weatherCode = (finder.getValue());
       
     weatherDesc[1] = '\0';           //                    Weather description
     finder.find("weatherDesc");
     finder.find("value");
     finder.find(":");
     finder.getString(" \"", "\"", weatherDesc,(25));

     weatherIconurl[1] = '\0';
     finder.find("weatherIconUrl");
     finder.find("value");
     finder.find(":");
     finder.getString(" \"", "\"", weatherIconurl,(119));


     finder.find("winddir16Point");  //                         wind direction
     finder.find(":");  //  wind direction  
     winddir[1] = '\0';
     finder.getString(" \"", "\"", winddir,(4));

       finder.find("winddirDegree");  //                     wind direction
        winddirDegree = (finder.getValue());

       finder.find("windspeedKmph");  //                     wind speed
        windSpeedKmph = (finder.getValue());

     finder.find("windspeedMiles");  //                         windspeed
        windSpeedW = (finder.getValue());
        dtostrf(windSpeedW, 2, 0, windSpeedC);              
 

     query[1] = '\0';                                // what kind of query was made
     finder.find("query");
     finder.find(":");
     finder.getString(" \"", "\"", query,(49));

      type[1] = '\0';
     finder.find("type");                           // type zip city ......
     finder.find(":");
     finder.getString(" \"", "\"", type,(10));
/*
     moonRiseC[1] = '\0';                             // moon rise
     finder.find("moonrise");
     finder.find(":");
     finder.getString(" \"", "\"", moonRiseC,(9));

     moonSetC[1] = '\0';                               // moon set
     finder.find("moonset");
     finder.find(":");
     finder.getString(" \"", "\"", moonSetC,(9));

     sunRiseC[1] = '\0';                                // sun rise
     finder.find("sunrise");
     finder.find(":");
     finder.getString(" \"", "\"", sunRiseC,(9));

     sunSetC[1] = '\0';                               // sunset
     finder.find("sunset");
     finder.find(":");
     finder.getString(" \"", "\"", sunSetC,(9));
*/
     for (int i= 0; i < 5; i++){
         texTx = 5;                                   // TFT Display info
         texTy = iNy+10;
         tft.setCursor(texTx, texTy);
         tft.print(F("Scannig for forecast "));
         tft.print(i+1);
         tft.print("                              ");
         
       forecastdate[1][i] = '\0';                               // sunset
           finder.find("date");
           finder.find(":");
           finder.getString(" \"", "\"", forecastdate[i],(11)); 
// free

     finder.find("precipMM");  //                     Percipitation in MM
      forecastprecipMM[i] = (finder.getValue());

     finder.find("tempMaxC");  //                     Percipitation in MM
      tempMaxC[i] = (finder.getValue());

     finder.find("tempMaxF");  //                     Percipitation in MM
      tempMaxF[i] = (finder.getValue());

     finder.find("tempMinC");  //                     Percipitation in MM
      tempMinC[i] = (finder.getValue());

     finder.find("tempMinF");  //                     Percipitation in MM
      tempMinF[i] = (finder.getValue());

      finder.find("weatherCode");  //                     Weather code
      forecastweatherCode[i] = (finder.getValue());
/*          // premium
           findChanceof();
  
           finder.find("maxtempC"); 
           maxtempC[i] = (finder.getValue());

           finder.find("maxtempF"); 
           maxtempF[i] = (finder.getValue());

           finder.find("mintempC"); 
           mintempC[i] = (finder.getValue());

          finder.find("mintempF"); 
          mintempF[i] = (finder.getValue());
 */
          }
 
    //stop the client, flush the data,

    client.stop();
    client.flush();
   }
  }
    switch (month()) {                                  // which month to log 
    case 1:
       Serial.println(F("Writing to jan.txt"));  
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("jan.txt")) SD.remove("jan.txt");
        }
        myFile = SD.open("jan.txt", FILE_WRITE);
        break;
    case 2:
       Serial.println(F("Writing to feb.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("feb.txt")) SD.remove("feb.txt");
        }
        myFile = SD.open("feb.txt", FILE_WRITE);
        break; 
    case 3:
       Serial.println(F("Writing to march.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("march.txt")) SD.remove("march.txt");
        }
        myFile = SD.open("march.txt", FILE_WRITE);
        break;
    case 4:
       Serial.println(F("Writing to april.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("april.txt")) SD.remove("april.txt");
        }
        myFile = SD.open("april.txt", FILE_WRITE);
        break;
    case 5:
       Serial.println(F("Writing to may.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("may.txt")) SD.remove("may.txt");
        }
        myFile = SD.open("may.txt", FILE_WRITE);
        break;
    case 6:
       Serial.println(F("Writing to june.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("june.txt")) SD.remove("june.txt");
        }
        myFile = SD.open("june.txt", FILE_WRITE);
        break;
    case 7:
       Serial.println(F("Writing to july.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("july.txt")) SD.remove("july.txt");
        }
        myFile = SD.open("july.txt", FILE_WRITE);
        break;
    case 8:
       Serial.println(F("Writing to aug.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("aug.txt")) SD.remove("aug.txt");
        }
        myFile = SD.open("aug.txt", FILE_WRITE);
        break;
    case 9:
       Serial.println(F("Writing to sept.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("sept.txt")) SD.remove("sept.txt");
        }
        myFile = SD.open("sept.txt", FILE_WRITE);
        break;
    case 10:
       Serial.println(F("Writing to oct.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("oct.txt")) SD.remove("oct.txt");
        }
        myFile = SD.open("oct.txt", FILE_WRITE);
        break;
    case 11:
       Serial.println(F("Writing to nov.txt"));
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("nov.txt")) SD.remove("nov.txt");
        }
        myFile = SD.open("nov.txt", FILE_WRITE);
        break;
    case 12:
       Serial.println(F("Writing to dec.txt"));    
        if (mYmonthDay == 1 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("dec.txt")) SD.remove("dec.txt");
        }
        myFile = SD.open("dec.txt", FILE_WRITE);
        break;
    default:
       Serial.println(F("Writing to weather.txt"));
        if (mYmonth == 0 && mYhour == 0 && mYminute == 0 && mYsecond == 0) {
             if (SD.exists("weather.txt")) SD.remove("weather.txt");
        }
        myFile = SD.open("weather.txt", FILE_WRITE);
   }
   //-------------------   writing weather info to SD ----------------
if (sizeof(tempfC) > 0) {   
  if (mYmonth < 10) myFile.print(F("0")); myFile.print(mYmonth);  myFile.print(F("/")); 
  if (mYmonthDay < 10) myFile.print(F("0")); myFile.print(mYmonthDay);  myFile.print(F("/"));  
  myFile.print(mYyear); myFile.print(F(","));
  if (mYhour < 10) myFile.print(F("0")); myFile.print(mYhour);  myFile.print(F(":"));  
  if (mYminute < 10) myFile.print(F("0")); myFile.print(mYminute);  myFile.print(F(":")); 
  if (mYsecond < 10) myFile.print(F("0")); myFile.print(mYsecond);

  myFile.print(F(","));               
  myFile.print(observeTime);
  myFile.print(F(","));
  myFile.print(humidityC);
  myFile.print(F(","));
  myFile.print(weatherDesc);  
  myFile.print(F(","));
  myFile.print(tempcC);  
  myFile.print(F(","));
  myFile.print(tempfC);  
  myFile.print(F(","));
  myFile.print(visibilityC);  
  myFile.print(F(","));
  myFile.print(winddir);  
  myFile.print(F(","));
  myFile.println(windSpeedC);  
}
  myFile.close();
#ifdef DEBUG  
  Serial.println(F("-------------------------------------")); 

 for (int i = 0; i < 12; i++) {
  Serial.print("absMaxTemp = ");
  Serial.println(absMaxTempc[i]);
  Serial.print("absMaxTempf = ");
  Serial.println(absMaxTempf[i]);
  Serial.print("avgMinTemp = ");
  Serial.println(avgMinTemp[i]);
  Serial.print("avgMinTempf = ");
  Serial.println(avgMinTempf[i]);
  Serial.print("monthIndex = ");
  Serial.println(monthIndex[i]);
  Serial.print("monthName = ");
  Serial.println(monthName[i]);
  Serial.println(" ");
}
  Serial.print(F("cloudCover = "));
  Serial.println(cloudCover);  
  Serial.print(F("feels like c = "));
  Serial.println(feelsLikec);  
  Serial.print(F("feels like f = "));
  Serial.println(feelsLikefC);  
  Serial.print(F("humidity = "));
  Serial.println(humidityC);  
  Serial.print(F("observation time = "));
  Serial.println(observeTime);  
  Serial.print(F("precipMM = "));
  Serial.println(precipMM);  
  Serial.print(F("pressure = "));
  Serial.println(pressure);  
  Serial.print(F("temp_c = "));
  Serial.println(tempcC);  
  Serial.print(F("temp_F = "));
  Serial.println(tempfC);  
  Serial.print(F("visibility = "));
  Serial.println(visibilityC);  
  Serial.print(F("weatherCode = "));
  Serial.println(weatherCode);  
  Serial.print(F("weatherDesc = "));
  Serial.println(weatherDesc);  
  Serial.print(F("weatherIconurl = "));
  Serial.println(weatherIconurl);  
  Serial.print(F("winddir = "));
  Serial.println(winddir);  
  Serial.print(F("winddirDegree = "));
  Serial.println(winddirDegree);  
  Serial.print(F("windSpeedKmph = "));
  Serial.println(windSpeedKmph);  
  Serial.print(F("wind speed = "));
  Serial.println(windSpeedC);  
  Serial.print(F("sunrise = "));
  Serial.println(sunRiseC);  
  Serial.print(F("query = "));
  Serial.println(query);  
  Serial.print(F("type = "));
  Serial.println(type);  
  Serial.print(F("sunset = "));
  Serial.println(sunSetC);  
  Serial.print(F("moonrise = "));
  Serial.println(moonRiseC);  
  Serial.print(F("moonset = "));
  Serial.println(moonSetC);  
  for (int i =0; i < 5; i++) {  
       Serial.print("date = ");
       Serial.println(forecastdate[i]);
       Serial.print("forecast chance of rain = ");
       Serial.println(forecastchanceofrain[i+3]);
       Serial.print("forecast chance of snow = ");
       Serial.println(forecastchanceofsnow[i+3]);
       Serial.print("forecastweatherCode = ");
       Serial.println(forecastweatherCode[i+3]);  
       Serial.print("maxtempC = ");
       Serial.println(maxtempC[i]);
       Serial.print("maxtempF = ");
       Serial.println(maxtempF[i]);
       Serial.print("mintempC = ");
       Serial.println(mintempC[i]);
       Serial.print("mintempF = ");
       Serial.println(mintempF[i]);
     }
  Serial.println(F("-------------------------------------")); 
  Serial.print("Done @ ");
  getTime();
  Serial.print(mYmonth);  Serial.print(F("/"));  Serial.print(mYmonthDay);  Serial.print(F("/"));  Serial.print(mYyear);  Serial.print(F(" "));
  Serial.print(mYhour);  Serial.print(F(":"));  Serial.print(mYminute);  Serial.print(F(":"));  Serial.println(mYsecond);
#endif
    texTx = 0;                                   // TFT Display info
    texTy = iNy+10;
    tft.setCursor(texTx, texTy);
    strcpy(printBuff, "Out: ");                 // Out:
    strcat(printBuff, tempfC);
/*
    strcat(printBuff, "F FLK ");
    strcat(printBuff, feelsLikefC);
*/
    strcat(printBuff, "F ");
    strcat(printBuff, humidityC);                     // Humidity %
    strcat(printBuff, "% Wind  ");
    strcat(printBuff, winddir);
    strcat(printBuff, " ");
    strcat(printBuff, windSpeedC);
    strcat(printBuff, " Mph");
    tft.print(printBuff); 
/*
    texTx = 0;                                   // TFT Display info
    texTy = iNy+20;
    tft.setCursor(texTx, texTy);
    tft.print("SunRise: ");
    tft.print(sunRiseC);
    tft.print("       Sunset: ");
    tft.print(sunSetC);
 */ 
  if (weatherCode == 119) bmpDraw("Cloudy.bmp", 0,clockCenterY-15);
  if (weatherCode == 248 || weatherCode == 260) bmpDraw("fog.bmp", 0,clockCenterY-15);
  if (weatherCode == 263 || weatherCode == 293 ||weatherCode == 296
   || weatherCode == 353 || weatherCode == 386) bmpDraw("lterain.bmp", 0,clockCenterY-15);
        
  if (weatherCode == 143 || weatherCode == 176 || weatherCode == 182
   || weatherCode == 266 || weatherCode == 281 || weatherCode == 326
   || weatherCode == 329 || weatherCode == 362 || weatherCode == 374) bmpDraw("lteShowr.bmp", 0,clockCenterY-15);
        
  if (weatherCode == 185 || weatherCode == 284 || weatherCode == 323
   || weatherCode == 392 || weatherCode == 179 || weatherCode == 362) bmpDraw("ltesnow.bmp", 0,clockCenterY-15);

  if (weatherCode == 122) bmpDraw("Ovrc32.bmp", 0,clockCenterY-15);

  if (weatherCode == 116) bmpDraw("SunInvl.bmp", 0,clockCenterY-15);

  if (weatherCode == 299 || weatherCode == 302 || weatherCode == 305
   || weatherCode == 308 || weatherCode == 311 || weatherCode == 314
   || weatherCode == 317 || weatherCode == 320 || weatherCode == 356
   || weatherCode == 365 || weatherCode == 368 || weatherCode == 377
   || weatherCode == 389) bmpDraw("rain.bmp", 0,clockCenterY-15);

  if (weatherCode == 227 || weatherCode == 230 || weatherCode == 332
   || weatherCode == 335 || weatherCode == 338 || weatherCode == 350
   || weatherCode == 371 || weatherCode == 394) bmpDraw("snow.bmp", 0,clockCenterY-15);

  if (weatherCode == 113) bmpDraw("Sunny.bmp", 0,clockCenterY-15);
  
  if (weatherCode == 200) bmpDraw("Tstorms.bmp", 0,clockCenterY-15);  
  drawMonthgraph(50, 300);

/*
  if (strcmp(weatherDesc, "Fog") == 0 || strcmp(weatherDesc,"Freezing fog") == 0) bmpDraw("fog.bmp", 0,clockCenterY-15);
  if (strcmp(weatherDesc, "Moderate rain") == 0 || strcmp(weatherDesc, "Light rain") == 0
    || strcmp(weatherDesc, "Mist") == 0  || strcmp(weatherDesc, "Patchy rain nearby") == 0
    || strcmp(weatherDesc, "Patchy light drizzle") == 0 || strcmp(weatherDesc, "Light rain shower") == 0)  bmpDraw("lterain.bmp", 0, clockCenterY-15);
  if (strcmp(weatherDesc, "Sunny") == 0  || strcmp(weatherDesc, "Clear") == 0)  bmpDraw("Sunny.bmp", 0, clockCenterY-5);
  if (strcmp(weatherDesc, "Partly Cloudy") == 0)  bmpDraw("SunInvl.bmp", 0, clockCenterY-15);
  if (strcmp(weatherDesc, "Overcast") == 0)  bmpDraw("Ovrc32.bmp", 0, clockCenterY-15);
  if (strcmp(weatherDesc, "Moderate rain at times") == 0 || strcmp(weatherDesc, "Moderate rain") == 0
    || strcmp(weatherDesc, "Heavy rain at times") == 0   || strcmp(weatherDesc, "Heavy rain") == 0)  bmpDraw("rain.bmp", 0, clockCenterY-15);
 */
  }
                  // premium  
void findChanceof(){
 /*
  for (int i = 0; i < 8; i++) {
      finder.find("chanceofrain");  //                     Weather code
      forecastchanceofrain[i] = (finder.getValue());
      finder.find("chanceofsnow");  //                     Weather code
      forecastchanceofsnow[i] = (finder.getValue());
      finder.find("weatherCode");  //                     Weather code
      forecastweatherCode[i] = (finder.getValue());
  }
  */
}

void forecast() {
   tft.fillScreen(BLUE);  
   prevDay = 99;  
   printDate();
   texTx = 0;
   texTy = 0;
   int x = 0;
   int y = 0;
   tft.setCursor(texTx, texTy);
 
   tft.fillRect(x, y+40, 238, 2, WHITE);
   tft.setTextSize(2);
   tft.setCursor(texTx=5, texTy+ 50);
   tft.print(tempfC);
   tft.print(" F");
   tft.drawCircle(x+34, y + 53, 2, WHITE);      
   tft.drawCircle(x+34, y + 53, 3, WHITE);
   tft.setTextSize(1);      

   if (weatherCode == 119) bmpDraw("Cldy64.bmp", texTx+90, texTy+80);
   if (weatherCode == 248 || weatherCode == 260) bmpDraw("fog64.bmp", texTx+90, texTy+80);
   if (weatherCode == 263 || weatherCode == 293 ||weatherCode == 296
    || weatherCode == 353 || weatherCode == 386) bmpDraw("lteran64.bmp",  texTx+90, texTy+80);
        
   if (weatherCode == 143 || weatherCode == 176 || weatherCode == 182
    || weatherCode == 266 || weatherCode == 281 || weatherCode == 326
    || weatherCode == 329 || weatherCode == 362 || weatherCode == 374) bmpDraw("lteShw64.bmp", texTx+90, texTy+80);
        
   if (weatherCode == 185 || weatherCode == 284 || weatherCode == 323
    || weatherCode == 392 || weatherCode == 179 || weatherCode == 362) bmpDraw("ltesnw64.bmp", texTx+90, texTy+80);

   if (weatherCode == 122) bmpDraw("Ovrcst64.bmp", texTx+90, texTy+80);

   if (weatherCode == 116) bmpDraw("Sunint64.bmp", texTx+90, texTy+80);

   if (weatherCode == 299 || weatherCode == 302 || weatherCode == 305
    || weatherCode == 308 || weatherCode == 311 || weatherCode == 314
    || weatherCode == 317 || weatherCode == 320 || weatherCode == 356
    || weatherCode == 365 || weatherCode == 368 || weatherCode == 377
    || weatherCode == 389) bmpDraw("rain64.bmp", texTx+90, texTy+80);

   if (weatherCode == 227 || weatherCode == 230 || weatherCode == 332
    || weatherCode == 335 || weatherCode == 338 || weatherCode == 350
    || weatherCode == 371 || weatherCode == 394) bmpDraw("snow64.bmp", texTx+90, texTy+80); 

   if (weatherCode == 113) bmpDraw("Suny64.bmp", texTx+90, texTy+80); 
  
   if (weatherCode == 200) bmpDraw("Thdst64.bmp", texTx+90, texTy+80);  
   int bmpY = 230;
   for (int i = 0; i < 4 ; i++) {
      tft.fillRect(x, y+200, 239, 2, WHITE);
      tft.setCursor(texTx+10+(i*60), texTy+210);
      int d = weekday();
      d = d + i + 1;
      if (d > 7) d = d - 7 ; 
      tft.print(shortdays[d]);
      if (forecastweatherCode[i+1] == 119) bmpDraw("Cldy32.bmp", (i*60)+10, bmpY);
      
      if (forecastweatherCode[i+1] == 248 || forecastweatherCode[i+1] == 260) bmpDraw("fog32.bmp", x+10+(i*60), bmpY);
      if (forecastweatherCode[i+1] == 263 || forecastweatherCode[i+1] == 293 ||forecastweatherCode[i+1] == 296
       || forecastweatherCode[i+1] == 353 || forecastweatherCode[i+1] == 386) bmpDraw("lteran32.bmp",  (i*60)+10, bmpY);
        
      if (forecastweatherCode[i+1] == 143 || forecastweatherCode[i+1] == 176 || forecastweatherCode[i+1] == 182
       || forecastweatherCode[i+1] == 266 || forecastweatherCode[i+1] == 281 || forecastweatherCode[i+1] == 326
       || forecastweatherCode[i+1] == 329 || forecastweatherCode[i+1] == 362 || forecastweatherCode[i+1] == 374) bmpDraw("lteShw32.bmp", (i*60)+10, bmpY);
        
      if (forecastweatherCode[i+1] == 185 || forecastweatherCode[i+1] == 284 || forecastweatherCode[i+1] == 323
       || forecastweatherCode[i+1] == 392 || forecastweatherCode[i+1] == 179 || forecastweatherCode[i+1] == 362) bmpDraw("ltesnw32.bmp", (i*60)+10, bmpY);

      if (forecastweatherCode[i+1] == 122) bmpDraw("Ovrcst32.bmp",(i*60)+10, bmpY);

      if (forecastweatherCode[i+1] == 116) bmpDraw("Sunint32.bmp", (i*60)+10, bmpY);

      if (forecastweatherCode[i+1] == 299 || forecastweatherCode[i+1] == 302 || forecastweatherCode[i+1] == 305
       || forecastweatherCode[i+1] == 308 || forecastweatherCode[i+1] == 311 || forecastweatherCode[i+1] == 314
       || forecastweatherCode[i+1] == 317 || forecastweatherCode[i+1] == 320 || forecastweatherCode[i+1] == 356
       || forecastweatherCode[i+1] == 365 || forecastweatherCode[i+1] == 368 || forecastweatherCode[i+1] == 377
       || forecastweatherCode[i+1] == 389) bmpDraw("rain32.bmp", (i*60)+10, bmpY);

     if (forecastweatherCode[i+1] == 227 || forecastweatherCode[i+1] == 230 || forecastweatherCode[i+1] == 332
      || forecastweatherCode[i+1] == 335 || forecastweatherCode[i+1] == 338 || forecastweatherCode[i+1] == 350
      || forecastweatherCode[i+1] == 371 || forecastweatherCode[i+1] == 394) bmpDraw("snow32.bmp", (i*60)+10, bmpY);

     if (forecastweatherCode[i + 1] == 113) bmpDraw("Suny32.bmp", (i*60)+10, bmpY);

     if (forecastweatherCode[i+1] == 200) bmpDraw("Thdst32.bmp", (i*60)+10, bmpY);  
          tft.setCursor(texTx+5+(i*60), texTy+270);
          tft.print(tempMinF[i+1]); 
          tft.print("/"); 
          tft.print(tempMaxF[i+1]); 
          tft.print("F");
          tft.setCursor(texTx+5+(i*60), texTy+280);
          tft.print("Prec ");
          tft.print(forecastprecipMM[i+1]);
          tft.print("%");
         }
         while(1){
 digitalWrite(13, HIGH);          
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  if (p.z != 0) {
   break;
#ifdef DEBUG
     Serial.println("end of touch");
#endif     
  }                               // end of it pressure check
         }
}
// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, int x, int y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.println(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.println(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if(lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        } 
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void drawFace() {                             // Draw the face of the clock - The numbers are based off the top left pixle

  //   draw two circles
  tft.drawCircle(clockCenterX, clockCenterY, circleSize, WHITE);
  tft.drawCircle(clockCenterX, clockCenterY, circleSize + 3, WHITE);
  tft.drawCircle(clockCenterX, clockCenterY, circleSize - 20, WHITE);
  tft.drawCircle(clockCenterX, clockCenterY, circleSize - 17, WHITE);

//      XII
  int x = clockCenterX-19;
  int y = clockCenterY-75;
  tft.drawRect(x,y,9,1, WHITE);
  tft.drawRect(x+13,y,8,1, WHITE);
  tft.drawRect(x,y+8,9,1, WHITE);         
  tft.drawRect(x+13,y+8,8,1, WHITE); 
  tft.drawLine(x+3,y+1,x+15,y+7, WHITE);     
  tft.drawLine(x+4,y+1,x+16,y+7, WHITE);   
  tft.drawLine(x+5,y+1,x+17,y+7, WHITE);                 
  tft.drawLine(x+16,y+1,x+3,y+7, WHITE);
     
  tft.drawRect(x+23,y,8,1, WHITE);
  tft.drawRect(x+26,y+1,2,8, WHITE);
  tft.drawRect(x+23,y+8,8,1, WHITE);  
  
  tft.drawRect(x+35,y,8,1, WHITE);
  tft.drawRect(x+38,y+1,2,8, WHITE);
  tft.drawRect(x+35,y+8,8,1, WHITE);  
    

//         III
  x = clockCenterX+66;
  y = clockCenterY-15;
  tft.drawRect(x+1, y+3, 9, 2, WHITE);
  tft.drawRect(x+1, y+11, 9, 2, WHITE); 
  tft.drawRect(x+1, y+20, 9, 2, WHITE);        
  tft.drawRect(x, y, 1, 8, WHITE); 
  tft.drawRect(x,y+9, 1, 7, WHITE); 
  tft.drawRect(x, y+17, 1, 7, WHITE);     
  tft.drawRect(x+10, y, 1, 8, WHITE); 
  tft.drawRect(x+10, y+9, 1, 7, WHITE); 
  tft.drawRect(x+10, y+18, 1, 7, WHITE);     


//     upside dowm VI
  x = clockCenterX-10;
  y = clockCenterY+65;
  tft.fillRect(x+3, y, 3,10, WHITE);
  tft.drawLine(x, y, x+8, y, WHITE);
  tft.drawLine(x, y+9, x+7,y+9, WHITE);

  tft.drawLine(x+17,y,x+12,y+9, WHITE);   
  tft.drawLine(x+17,y,x+21,y+9, WHITE);          
  tft.drawLine(x+18,y,x+22,y+9, WHITE); 
  tft.drawLine(x+19,y,x+23,y+9, WHITE); 
  tft.drawLine(x+9,y+9,x+16,y+9, WHITE);
  tft.drawLine(x+19,y+9,x+25,y+9, WHITE);     
     
//   sideways XI 
  x = clockCenterX-76;
  y = clockCenterY-15;
  tft.drawLine(x+9,y,x+9,y+9, WHITE); 
  tft.drawLine(x+9,y+14,x+9,y+23, WHITE);          
  tft.drawLine(x,y, x,y+9, WHITE); 
  tft.drawLine(x,y+14, x,y+23, WHITE);          
  tft.drawLine(x,y+4,x+8,y+19, WHITE); 
  tft.drawLine(x+1,y+3,x+8,y+18, WHITE); 
  tft.drawLine(x+8,y+4,x,y+19, WHITE);       


  tft.fillRect(x,y+28,10,3,WHITE);
  tft.drawLine(x+9,y+26,x+9,y+33, WHITE);     
  tft.drawLine(x,y+26,x,y+33, WHITE);     

}

void drawHour() {                             // Draw the hour hand
  float x1, y1, x2, y2;
  int ph = prevHour;

  if (prevMin ==0){
      ph=((ph-1)*30)+((prevMin+59)/2);
      }
  else
      {
     ph= (ph*30)+((prevMin-1)/2);
     }

  ph = ph+270;

  x1=50*cos(ph*0.0175)+xDiff;                 // calculate the angle of the hand from center then erase the previous display the write the new display
  y1=50*sin(ph*0.0175)-yDiff;
  x2=1*cos(ph*0.0175)+xDiff;
  y2=1*sin(ph*0.0175)-yDiff;

  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,BLUE);
  ph = mYhour;
  if (mYminute ==0){
      ph=((ph-1)*30)+((mYminute+59)/2);
      }
  else
      {
      ph= (ph*30)+((mYminute-1)/2);
      }
  ph = ph+270;

  x1=50*cos(ph*0.0175)+xDiff;
  y1=50*sin(ph*0.0175)-yDiff;
  x2=1*cos(ph*0.0175)+xDiff;
  y2=1*sin(ph*0.0175)-yDiff;
  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,WHITE);
  prevHour = mYhour;

}

void drawSec(){                                                // Draw the second hand
  float x1, y1, x2, y2;
  int ps = prevSec;

  ps=ps*6;
  ps=ps+270;
  x1=58*cos(ps*0.0175) + xDiff ;
  y1=58*sin(ps*0.0175) - yDiff;
  x2=1*cos(ps*0.0175) + xDiff;
  y2=1*sin(ps*0.0175) - yDiff;
  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,BLUE);
  
  ps=mYsecond*6;
  ps=ps+270;
  x1=58*cos(ps*0.0175) + xDiff;
  y1=58*sin(ps*0.0175) - yDiff;
  x2=1*cos(ps*0.0175) + xDiff;
  y2=1*sin(ps*0.0175) - yDiff;
  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,RED);

  prevSec = mYsecond;

}

void drawMin(){                                   // Draw the minute hand
 
  float x1, x2, y1, y2, x3, y3, x4, y4;
  int pm = prevMin;

  if (pm==-1) pm = 59;
  pm = pm*6;
  pm=pm+270;

  x1=58*cos(pm*0.0175) + xDiff;
  y1=58*sin(pm*0.0175) - yDiff;
  x2=1*cos(pm*0.0175) + xDiff;
  y2=1*sin(pm*0.0175) - yDiff;
  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,BLUE);

  pm = mYminute;

  if (pm==-1) pm = 59;
  pm = pm*6;
  pm=pm+270;

  x1=58*cos(pm*0.0175) + xDiff;
  y1=58*sin(pm*0.0175) - yDiff;
  x2=1*cos(pm*0.0175) + xDiff;
  y2=1*sin(pm*0.0175) - yDiff;
  tft.drawLine(x1+clockCenterX,y1+clockCenterY, x2+clockCenterX,y2+clockCenterY,WHITE);
  
  prevMin = mYminute;

}

void displayInsidetemp () {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
 dhtCtr++; 
 if (dhtCtr < 5) return;
 dhtCtr = 0;
 float h = dht.readHumidity();
  float t = dht.readTemperature();
  float dewPoint;
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println(F("Failed to read from DHT"));
  } else {
    float temperatureF = (t * 9.0 / 5.0) + 32.0;
    char tempBuff[8];
    int j;
    int hp;
    hp = h;
    int tf;
    tf = temperatureF;
    texTx = 0;
    texTy = iNy;
    tft.setCursor(texTx,texTy);
    j = sprintf(printBuff, " In: %d", tf);
    strcat(printBuff, "F ");
    tft.print(printBuff);

      j = sprintf(printBuff, "%d", hp);
      strcat(printBuff, "% ");
      tft.print(printBuff);

      dewPoint = calculateDewpoint(t, h);
      int dp;
      dp = dewPoint;
      j = sprintf(printBuff, "%d", dp);
      strcat(printBuff, " D");
//      tft.print(printBuff);

      
#ifdef DEBUG
       Serial.print(tf);
       Serial.print(F(" "));
       Serial.println(printBuff);
#endif
     }
   } 
float calculateDewpoint(float T, float RH)
    {
    // approximate dewpoint using the formula from wikipedia's article on dewpoint

    float dp = 0.0;
    float gTRH = 0.0;
    float a = 17.271;
    float b = 237.7;

    gTRH = ((a*T)/(b+T))+log(RH/100);
    dp = (b*gTRH)/(a-gTRH);

    return dp;
    }

	boolean checkLeapYear(int yearRequest){              // check for leap year
 if ( yearRequest%4 == 0 && yearRequest%100 != 0 || yearRequest % 400 == 0 ) return true;
     else  return false;
}

