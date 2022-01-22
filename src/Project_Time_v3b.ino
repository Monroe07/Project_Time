/*
  Name:		ESP8266_Clock_v3b.ino
  Created:	10/05/2020 11:51 AM
  Updated : 12/04/2020
  Author:	KAMon
*/
// File System Includes
#include <FS.h>                   // File System Includes. This needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          // ESP8266 Libraries
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <TimeLib.h>            // Time Libraries

#include "settings.h"         // Settings for Wifi and Device Name
#include <ArduinoJson.h>          // Include for Saving configuration parameters to json config file. https://github.com/bblanchon/ArduinoJson

#if defined TM1637
#include <TM1637Display.h>    // Screen Includes for TM1637 Screen

#elif defined HT16K33         // Screen Includes for HT16K33
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#else
#endif
/*
   NTP VARIABLES
*/

int timeZone = 0;               // Timezone Offset used for Local Time Calculation
char offset[3];                 // Placeholder to hold the GMT Offset from File / Config Page
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
/*
   CONFIG VARIABLES
*/
bool resetWifi = false;       // Erase Wifi Setings Falg
bool eraseSPIFFS = false;     // Erase SPIFFS (SPI Flash File System) Flag
bool shouldSaveConfig = false;  //flag for saving data

/*
   DISPLAY VARIABLES
*/
boolean colon = true;           // Flag used to determine wether the colon is currently displayed

// Amazon DIYMORE Screen
#if defined TM1637
TM1637Display display(CLK, DIO);

// Adafruit i2c Screen
#elif defined HT16K33         // Screen Includes for HT16K33
Adafruit_7segment matrix = Adafruit_7segment();

#else
#endif


int dst = -1;
int prevDst = -1;
int prevHour = -1;
int prevMin = -1;
bool autoConfigFlag = true;
time_t prevDisplay = 0; // when the digital clock was displayed
int blinker;


/*
   FUNCTION PROTOTYPES
*/
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress& address);
//void clockDisplay();
void displaySketchName();
void setupFunction(bool autoConfig);
void configModeCallback (WiFiManager *myWiFiManager);
void saveConfigCallback ();

int val;



void setup()
{

  //clean FS, for testing
  if (eraseSPIFFS)
  {
    SPIFFS.format();
  }
  // Daylight Savings Pin
  pinMode(adj, INPUT_PULLUP);
  pinMode(configPin, INPUT_PULLUP);

  Serial.begin(9600);

  displaySketchName();    // Function to Display Sketch Name
#if defined HT16K33
  matrix.begin(0x70);
  //matrix.clear();
  matrix.print(8888);
  matrix.writeDisplay();
#endif

  //while (!Serial) ; // Needed for Leonardo only
  delay(250);
  /*
    Serial.println(NAME);
    Serial.print("Connecting to ");
    Serial.println(ssid);
  */
  WiFi.hostname(NAME);
  //WiFi.begin(ssid, pass);

  setupFunction(autoConfigFlag);
  MDNS.begin(NAME);
  /*
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
      }
  */
  Serial.print("\nIP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);


}



void loop()
{
  // Read the config button status
  if (digitalRead(configPin) == LOW)
  {
    // Put Clock into config Mode and start the Config Webpage
    autoConfigFlag = false;
    setupFunction(autoConfigFlag);
    autoConfigFlag = true;

  }
#if defined TM1637
  display.setBrightness(0x0f);
#endif

  // Blinking Colon between Hour and Minute
  if (millis() % 500 == 0)
  {
    colon = !colon;
    if (colon)
    {
      blinker = 1;
    }
    else
    {
      blinker = 0;
    }
#if defined HT16K33
    matrix.drawColon(colon);
    matrix.writeDisplay();
#elif defined TM1637
    display.showNumberDecEx(val, (0x80 >> blinker), false);
#else
#endif
  }

  // Daylight Savings Time Adjustment Routine
  if (digitalRead(adj) == HIGH) {
    setDST(false);
    //Serial.println("DST: ON");
  }
  else {
    setDST(true);
    //Serial.println("DST: OFF");
  }




  // Check if Time is recieved0
  if (timeStatus() != timeNotSet) {
    int m = minute();
    int h = hour();
    if (m != prevMin || h != prevHour) {
      //if (now() != prevDisplay) { //update the display only if time has changed
      //prevDisplay = now();
      digitalClockDisplay(); // Serial Output Time/Date
      refreshClockDisplay(); // Update Screen Output Time
      prevMin = m;
      prevHour = h;

    }
  }

}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(month());
  Serial.print("/");
  Serial.print(day());
  Serial.print("/");
  Serial.print(year());
  Serial.println();
}

void refreshClockDisplay()
{

  int h = hourFormat12();
  int m = minute();
  int digitOne = h / 10;
  int digitTwo = h % 10;
  int digitThree = m / 10;
  int digitFour = m % 10;
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // USING ADAFRUIT SCREEN //////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined HT16K33

  matrix.clear();
  matrix.writeDisplay();
  if (h > 9)
  {
    matrix.writeDigitNum(0, digitOne, false);
  }

  matrix.writeDigitNum(1, digitTwo, false);


  matrix.writeDigitNum(3, digitThree, false);
  matrix.writeDigitNum(4, digitFour, false);
  matrix.writeDisplay();

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // USING DIY MORE SCREEN //////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#elif defined TM1637
  uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
  display.setBrightness(15);
  /*
    if (h > 9)
    {

    data[0] = display.encodeDigit(digitOne);
    data[1] = display.encodeDigit(digitTwo);
    }
    else
    {
    data[0] = display.encodeDigit(0x00);
    data[1] = display.encodeDigit(digitTwo);
    }

    data[2] = display.encodeDigit(digitThree);
    data[3] = display.encodeDigit(digitFour);
    display.setSegments(data);
  */
  val = (h * 100) + m;
  display.showNumberDec(val, false);
#else
#endif
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0); // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// Print Sketch Name dor Future Debug/Update
void displaySketchName(void)
{
  String the_path = __FILE__;
  int slash_loc = the_path.lastIndexOf('\\');
  String the_cpp_name = the_path.substring(slash_loc + 1);
  int dot_loc = the_cpp_name.lastIndexOf('.');
  String the_sketchname = the_cpp_name.substring(0, dot_loc);

  Serial.print("\nFirmware: ");
  Serial.println(the_sketchname);
  Serial.print("Compiled on: ");
  Serial.print(__DATE__);
  Serial.print(" at ");
  Serial.print(__TIME__);
  Serial.println("\n");

}

// Function Called When Clock is In Setup Mode
void setupFunction(bool autoConfig)
{
  //display.setBrightness(0x0f);
  String num;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // For Accessing the SPIFFS File System containing config ///////////////////////////////////////////////////////
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(offset, json["offset"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read ///////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // Setup Custom Parameter
  //
  // id/name placeholder/prompt/ default length
  WiFiManagerParameter custom_timezone("Offset", "Offset from GMT", offset, 4);


  WiFiManager wifiManager;
  //reset settings - for testing
  ////////////////////////////////////////
  if (resetWifi)
  {
    wifiManager.resetSettings();
  }

  ////////////////////////////////////////
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_timezone);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(600);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "NAME"
  //and goes into a blocking loop awaiting configuration
  if (autoConfig)
  {
    // Run AutoConfig Routine
    if (!wifiManager.autoConnect(NAME)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    Serial.println("Finished Auto Config");
  }
  else
  {
    // Manually Setup AccessPoint
    Serial.println("Manual Config Page Triggered");
    if (!wifiManager.startConfigPortal(NAME)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    Serial.println("Finished Manual Config");
  }



  Serial.println("Connected!");
  strcpy(offset, custom_timezone.getValue());


  num += offset[0];
  num += offset[1];
  num += offset[2];
  timeZone = (num.toInt()+1);

  // Force NTP Refresh
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  Serial.print("Offset String: ");
  Serial.println(num);

  Serial.print("Offset: ");
  Serial.println(timeZone);

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["offset"] = offset;


    json["ip"] = WiFi.localIP().toString();
    json["gateway"] = WiFi.gatewayIP().toString();
    json["subnet"] = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {

  Serial.println("Entered config mode");
#if defined HT16K33
  matrix.clear();
  matrix.writeDigitNum(3, 4, true);
  matrix.writeDigitNum(4, 1, false);
  matrix.writeDisplay();
#elif defined TM1637
  display.setBrightness(0x0f);
  int n = 401;
  display.showNumberDec(n, true);
#else
#endif

  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
