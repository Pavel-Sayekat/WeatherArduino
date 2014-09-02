// Filename ArduinoWeather.ino
// Version 1.0 7/23/14
// SwitchDoc Labs, LLC
//
//
// From:
// Filename ArduinoConnectServer.ino
// Version 1.5 08/15/13 RV MiloCreek
//

//#define WEBDUINO_SERIAL_DEBUGGING  1

#define ETHERNET


#include <SPI.h>

#ifdef ETHERNET
#include "Ethernet.h"
#endif

#define DEBUG


#include "WebServer.h"
#include "MemoryFree.h"
#include "avr/pgmspace.h"

#include "elapsedMillis.h"

#include "SDL_RasPiGraphLibrary.h"

#include "Config.h"

long messageCount;

static uint8_t mac[] = LOCALMAC;
static uint8_t ip[] = LOCALIP;

// this is our current command object structure.  It is only valid inside void jsonCmd 
typedef struct {
    char ObjectID[40];
    char ObjectFlags[40];
    char ObjectAction[40]; 
    char ObjectName[40];
    char ObjectServerID[40];
    char Password[40];
    char ObjectType[40];
    char Validate[40]; 
} currentObjectStructure;
  
char *md5str;

char ST1Text[40];   // used in ST-1 Send text control


char bubbleStatus[40];   // What to send to the Bubble status

char windSpeedBuffer[150];  // wind speed graph
char windGustBuffer[150];  // wind speed graph
char windDirectionBuffer[150];  // wind speed graph

float windSpeedMin;
float windSpeedMax;
float windGustMin;
float windGustMax;
float windDirectionMin;
float windDirectionMax;


float currentWindSpeed;
float currentWindGust;

float rainTotal;

// weather
#include "SDL_Weather_80422.h"

#define pinLED     13   // LED connected to digital pin 13
#define pinAnem    18  // Anenometer connected to pin 18 - Int 5
#define pinRain    2   // Rain Bucket connecto to pin 19 - Int 4
#define intAnem    5
#define intRain    0

// for mega, have to use Port B - only Port B works.
/*
 Arduino Pins         PORT
 ------------         ----
 Digital 0-7          D
 Digital 8-13         B
 Analog  0-5          C
*/


// initialize SDL_Weather_80422 library
SDL_Weather_80422 weatherStation(pinAnem, pinRain, intAnem, intRain, A0, SDL_MODE_INTERNAL_AD);


#include "MD5.h"

#include "smallJSON.h"

#include "ExecuteJSONCommand.h"




#include <stdarg.h>
void p(char *fmt, ... ){
        char tmp[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(tmp, 128, fmt, args);
        va_end (args);
        Serial.print(tmp);
}


// no-cost stream operator as described at 
// http://sundial.org/arduino/?page_id=119
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ obj.print(arg); return obj; }

#define PREFIX ""

WebServer webserver(PREFIX, WEB_SERVER_PORT);

// commands are functions that get called by the webserver framework
// they can read any posted data from client, and they output to server

void jsonCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{

  currentObjectStructure parsedObject; 
  initParsedObject(&parsedObject);
  
#ifdef DEBUG 
Serial.println("------------------");
#endif    


  char myBuffer[1024];
  


 
  server.httpSuccess("application/json");
  
  if (type == WebServer::HEAD)
    return;
    
    int myChar;
    int count;
    myChar = server.read();
    count = 0;
 
    
    while (myChar > 0)
    {
     myBuffer[count] = (char) myChar;
     
      myChar = server.read();
      
      count++;

    }
    myBuffer[count] = '\0';

    delay(25);

    messageCount++;
 
    char returnJSON[500] = "";
   
  
   ExecuteCommandAndReturnJSONString(myBuffer, messageCount, md5str, &parsedObject, returnJSON, returnJSON);


#ifdef DEBUG
    Serial.print("returnJSON =");
    Serial.println(returnJSON);
#endif


  
  server << returnJSON;
  
#ifdef DEBUG
    Serial.print("Mem1=");
    Serial.println(freeMemory());
    Serial.println("------------------");
#endif
}


elapsedMillis timeElapsed; //declare global if you don't want it reset every time loop runs

// setup the RasPiConnect Graph Arrays
SDL_RasPiGraphLibrary windSpeedGraph(10,SDL_MODE_LABELS);
SDL_RasPiGraphLibrary windGustGraph(10,SDL_MODE_LABELS);
SDL_RasPiGraphLibrary windDirectionGraph(10,SDL_MODE_LABELS);

   
void setup()
{
  
  Serial.begin(57600);           // set up Serial library at 9600 bps
  
  Serial.print(F("WeatherArduino "));
  Serial.print(1.0);
  Serial.println(F(" 07/31/14")); 
  Serial.print(F("Compiled at:"));
  Serial.print (F(__TIME__));
  Serial.print(F(" "));
  Serial.println(F(__DATE__));
  

  //p("ip address = %i.%i.%i.%i\n", ip[0], ip[1], ip[2], ip[3]);
  p("port number = %i\n", WEB_SERVER_PORT);
  //p("mac address = %#X %#X %#X %#X %#X %#X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  
  randomSeed(analogRead(0));

#ifdef ETHERNET
  Ethernet.begin(mac, ip);
#endif


  webserver.begin();


  
  webserver.setDefaultCommand(&jsonCmd);
  webserver.addCommand("arduino", &jsonCmd);
  messageCount=0;
  
  
  unsigned char* hash=MD5::make_hash(PASSWORD);
  //generate the digest (hex encoding) of our hash
  md5str = MD5::make_digest(hash, 16);

  strcpy(bubbleStatus, "ArduinoWeather Started\0");
  
  weatherStation.setWindMode(SDL_MODE_SAMPLE, 5.0);
  rainTotal = 0;

  timeElapsed = 0;
  

}

void loop()
{
  // process incoming connections one at a time forever
  webserver.processConnection();
  //Serial.println(freeMemory());
  
  // update weather
  currentWindSpeed = weatherStation.current_wind_speed()/1.6;


  if (timeElapsed > 5000) 
  {				
    
    Serial.print("time=");
    Serial.println(millis());
    Serial.print("micro time=");
    Serial.println(micros());
    currentWindSpeed = weatherStation.current_wind_speed()/1.6;
    currentWindGust = weatherStation.get_wind_gust()/1.6;
    
    float oldRain = rainTotal;
    rainTotal = rainTotal + weatherStation.get_current_rain_total()*0.03937;
    if (oldRain < rainTotal)
    {
      strcpy(bubbleStatus, "It is Raining\0");
    }
      
     
    timeElapsed = 0;			 // reset the counter to 0 so the counting starts over...
    windSpeedGraph.add_value(currentWindSpeed);
    windGustGraph.add_value(currentWindGust);
    windDirectionGraph.add_value(weatherStation.current_wind_direction());
    
    windSpeedGraph.getRasPiString(windSpeedBuffer, windSpeedBuffer);
    windGustGraph.getRasPiString(windGustBuffer, windGustBuffer);
    windDirectionGraph.getRasPiString(windDirectionBuffer, windDirectionBuffer);
    
    windSpeedMin = windSpeedGraph.returnMinValue();
    windSpeedMax = windSpeedGraph.returnMaxValue();
    windGustMin = windGustGraph.returnMinValue();
    windGustMax = windGustGraph.returnMaxValue();
    windDirectionMin = windDirectionGraph.returnMinValue();
    windDirectionMax = windDirectionGraph.returnMaxValue();
    
    Serial.print("windSpeedMin =");
    Serial.print(windSpeedMin);   
    Serial.print(" windSpeedMax =");
    Serial.println(windSpeedMax);
    
    Serial.print("windSpeedBuffer=");
    Serial.println(windSpeedBuffer);
    
    Serial.print("windGustMin =");
    Serial.print(windGustMin);   
    Serial.print(" windGustMax =");
    Serial.println(windGustMax);
        
    Serial.print("windGustBuffer=");
    Serial.println(windGustBuffer);
    
    Serial.print("windDirectionMin =");
    Serial.print(windDirectionMin);   
    Serial.print(" windDirectionMax =");
    Serial.println(windDirectionMax);
        
    
    Serial.print("windDirectionBuffer=");
    Serial.println(windDirectionBuffer);
    
    Serial.print(" currentWindSpeed=");
    Serial.print(currentWindSpeed);
  
    Serial.print(" \tcurrentWindGust=");
    Serial.print (currentWindGust);
  
    Serial.print(" \tWind Direction=");
    Serial.print(weatherStation.current_wind_direction());
  
   
    Serial.print(" \t\tCumulative Rain = ");
    Serial.print(rainTotal);
    Serial.print("\t\tmemory free=");
    Serial.println(freeMemory());
  
    Serial.println("---------------");
  }

  // if you wanted to do other work based on a connecton, it would go here
}
