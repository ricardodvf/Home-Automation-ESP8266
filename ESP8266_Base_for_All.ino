//#include <ESP8266WebServerSecureBearSSL.h>
//#include <ESP8266WebServerSecureAxTLS.h>
#include <ESP8266WebServerSecure.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", -21600, 60000);
unsigned long lastRealTime;

char Time[] = "TIME:00:00:00";
char Date[] = "DATE:00/00/2000";
byte last_second, second_, minute_, hour_, day_, month_;
int year_;

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <Free_Fonts.h>
#include <Adafruit_GFX.h>

#define TFT_GREY 0x5AEB

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
RF24 radio(2, 15);  //(2,15)
RF24Network network(radio);
unsigned long lastUpdate;
const uint16_t this_node = 00;
const uint16_t dulce_node = 01;
const uint16_t weather1_node = 02;
const uint16_t masterbedroom_node = 03;
const uint16_t fish_node = 04;
const uint16_t ashley_node = 05;
const uint16_t catfeeder1_node = 014;

typedef struct  {
  long isOnline;
  long isON;
  float temperature;
  float humidity;
  long lastFeeding;
  long nextFeeding;
  float weightLastFeeding;
  float weightNextFeeding;
} CatFeederInformation;

typedef struct  {
  unsigned long instruction1;
  unsigned long instruction2;
} instructions;

instructions instructionsToSend;

typedef struct  {
  long isOnline;
  long isON;
  float temperature;
  float humidity;
  long motion;
  long timerLeft;
  float current;
  unsigned long lastHeartBeat;
  float F1;
  float F2;
  float F3;
  float F4;
  float F5;
  long L1;
  long L2;
  long L3;
  long L4;
  long L5;
} contactorStationInformation;

contactorStationInformation DulceRoom;
contactorStationInformation AshleyRoom;
contactorStationInformation MasterBedRoom;
contactorStationInformation FishTank;
contactorStationInformation CatFeeder;
contactorStationInformation WeatherStation1;

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>   // Include the WebServer library
#include <FS.h>   // Include the SPIFFS library
#include <TFT_eSPI.h>
#include <Free_Fonts.h>

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

void handleRoot();              // function prototypes for HTTP handlers
void handleLED();
void handleNotFound();

int led = 16;
bool pinStatus = LOW;
String screen = "main";

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
bool connectedtoWIFI = true;

unsigned long lastTemperaturePrint;
unsigned long lastClockUpdate;


//************************************************************************************************************************
void setup(void) {
  tft.init();
  SPI.begin();
  radio.begin();
  network.begin(90, this_node);
  radio.setPALevel(RF24_PA_MAX);
  SPIFFS.begin();
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer

  uint16_t calData[5] = { 484, 3256, 413, 3253, 7 };
  tft.setTouch(calData);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 20);  //coordinates and font number
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(0.5);
  tft.setFreeFont(FF21);

  drawBmp("/startup.bmp", 0, 0);
  tft.println("Base for Home Automation!");
  Serial.println('\n');
  tft.setTextSize(0.1);
  tft.setFreeFont(FF17);

  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(15, OUTPUT);

  wifiMulti.addAP("NETGEAR50-5G", "giganticsea176");   // add Wi-Fi networks you want to connect to

  tft.print("Connecting ...");
  Serial.print("Connecting ...");
  int i = 0;
  unsigned long timeOUT = millis();
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
    tft.print('.');
    if (millis() - timeOUT > 10000) {
      connectedtoWIFI = false;
      break ;
    }
  }
  if (connectedtoWIFI) {
    Serial.println('\n');
    tft.println('\n');
    tft.print("Connected to ");
    Serial.print("Connected to ");
    tft.println(WiFi.SSID());
    Serial.println(WiFi.SSID());
    tft.print("IP address:\t");// Tell us what network we're connected to
    Serial.print("IP address:\t");
    tft.println(WiFi.localIP());
    Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

    if (MDNS.begin("homeautomation")) {              // Start the mDNS responder for esp8266.local
    } else {
      tft.println("Error setting up MDNS responder!");
    }
    server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
    server.on("/LED", HTTP_POST, handleLED);  // Call the 'handleLED' function when a POST request is made to URI "/LED"
    server.onNotFound([]() {                              // If the client requests any URI
      if (!handleFileRead(server.uri()))                  // send it if it exists
        server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    });
    server.begin();                           // Actually start the server
    Serial.println("HTTP server started");
  }
  checkAvailabilities();
  delay(100);
  drawBmp("/homemenu2.bmp", 0, 0);
  if (connectedtoWIFI) {
    drawBmp("/wifi.bmp", 0, 0);
    GetDateTime();
  }
  else {
    drawBmp("/wifi_off.bmp", 0, 0);
  }
  tft.fillRect(40, 0, 280, 28, TFT_WHITE);
  
}
//************************************************************************************************************************
void loop(void) {
  if (millis() - lastTemperaturePrint > 60000 && screen == "temperaturesmenu" ) {
    PrintTemperatures1();
  }
  if (millis() - lastRealTime > 43200000) {
      GetDateTime();
  }
  server.handleClient();                    // Listen for HTTP requests from clients
  network.update();
  MDNS.update();
  while (network.available()) {
    contactorStationInformation receivedInstructions;
    RF24NetworkHeader headerReceive;
    network.read(headerReceive, &receivedInstructions, sizeof(receivedInstructions));
    Serial.print("Network Available: "); Serial.println(headerReceive.from_node);
    if (headerReceive.from_node == fish_node) {
      HandleFishNodeMessage(receivedInstructions);
      FishTank.lastHeartBeat = millis()/2;
      FishTank.isOnline = true;
    }
    else if (headerReceive.from_node == dulce_node) {
      HandleDulceNodeMessage(receivedInstructions);
      DulceRoom.lastHeartBeat = millis()/2;
      DulceRoom.isOnline = true;
    }
    else if (headerReceive.from_node == ashley_node) {
      HandleAshleyNodeMessage(receivedInstructions);
      AshleyRoom.lastHeartBeat = millis()/2;
      AshleyRoom.isOnline = true;
    }
    else if (headerReceive.from_node == masterbedroom_node) {
      HandleMasterBedroomNodeMessage(receivedInstructions);
      MasterBedRoom.lastHeartBeat = millis()/2;
      MasterBedRoom.isOnline = true;
    }
    else if (headerReceive.from_node == weather1_node) {
      HandleWeather1NodeMessage(receivedInstructions);
      WeatherStation1.lastHeartBeat = millis()/2;
      WeatherStation1.isOnline = true;
    }
     else if (headerReceive.from_node == catfeeder1_node) {
      HandleCatFeeder1NodeMessage(receivedInstructions);
      CatFeeder.lastHeartBeat = millis()/2;
      CatFeeder.isOnline = true;
    }
  }
  uint16_t x = 0, y = 0; // To store the touch coordinates
  boolean pressed = tft.getTouch(&x, &y);
  SPI.end();
  delay(100);
  SPI.begin();
  if (pressed) {
    handleButtonPressed(x, y);
  }
  CheckHeartBeats();
  UpdateClocks();

}
//***********************************************************************************************************************
void UpdateClocks() {
    tft.setTextSize(0.1);
    tft.setFreeFont(FF17);
    tft.setTextColor(TFT_BLACK);
   
    if (millis() - lastClockUpdate > 1500) {
        lastClockUpdate = millis();
        if (screen == "fishtankmenu") {
            tft.setCursor(40, 20);
            tft.fillRect(40, 0, 280, 28, TFT_WHITE);
            tft.print(Date); tft.print("  "); tft.print(timeClient.getFormattedTime());
        }
    }
}

//***********************************************************************************************************************
void CheckHeartBeats() {
    if (millis()-(FishTank.lastHeartBeat*2) > 70000) {
        FishTank.isOnline = 0;
        if (screen == "fishtankmenu") {
            PrintFishTankInfo();
        }
    }
}
//************************************************************************************************************************
void GetDateTime() {
    //Gets the date and time from the internet
    timeClient.update();
    unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server

    second_ = second(unix_epoch);
    if (last_second != second_) {

        minute_ = minute(unix_epoch);
        hour_ = hour(unix_epoch);
        day_ = day(unix_epoch);
        month_ = month(unix_epoch);
        year_ = year(unix_epoch);

        Time[12] = second_ % 10 + 48;
        Time[11] = second_ / 10 + 48;
        Time[9] = minute_ % 10 + 48;
        Time[8] = minute_ / 10 + 48;
        Time[6] = hour_ % 10 + 48;
        Time[5] = hour_ / 10 + 48;

        Date[5] = day_ / 10 + 48;
        Date[6] = day_ % 10 + 48;
        Date[8] = month_ / 10 + 48;
        Date[9] = month_ % 10 + 48;
        Date[13] = (year_ / 10) % 10 + 48;
        Date[14] = year_ % 10 % 10 + 48;
    }
    Serial.print(Date); Serial.print(" "); Serial.println(Time);
    Serial.println(timeClient.getDay());
    Serial.println(timeClient.getHours());
    Serial.println(timeClient.getMinutes());
    Serial.println(timeClient.getSeconds());
    lastRealTime = millis();
}
//************************************************************************************************************************

void checkAvailabilities() {

  bool ok;
  instructionsToSend.instruction1 = 4;

  RF24NetworkHeader headerSend(dulce_node);
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
    DulceRoom.isOnline = true;
    DulceRoom.lastHeartBeat = millis();
  }

  headerSend.to_node = ashley_node;
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
    AshleyRoom.isOnline = true;
    AshleyRoom.lastHeartBeat = millis();
  }

  headerSend.to_node = masterbedroom_node;
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
    MasterBedRoom.isOnline = true;
    MasterBedRoom.lastHeartBeat = millis();
  }

  headerSend.to_node = fish_node;
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
    FishTank.isOnline = true;
    FishTank.lastHeartBeat = millis();
  }

  headerSend.to_node = weather1_node;
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
    WeatherStation1.isOnline = true;
    WeatherStation1.lastHeartBeat = millis();
  }

  headerSend.to_node = catfeeder1_node;
  if (network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend))) {
      CatFeeder.isOnline = true;
      CatFeeder.lastHeartBeat = millis();
  }

  if (millis() - FishTank.lastHeartBeat > 61000) {
      FishTank.isOnline = false;
  }
  if (millis() - DulceRoom.lastHeartBeat > 61000) {
      DulceRoom.isOnline = false;
  }
  if (millis() - AshleyRoom.lastHeartBeat > 61000) {
      AshleyRoom.isOnline = false;
  }
  if (millis() - MasterBedRoom.lastHeartBeat > 61000) {
      MasterBedRoom.isOnline = false;
  }
  if (millis() - WeatherStation1.lastHeartBeat > 61000) {
      WeatherStation1.isOnline = false;
  }
  if (millis() - CatFeeder.lastHeartBeat > 61000) {
      CatFeeder.isOnline = false;
  }


  Serial.print("Dulce is: "); Serial.println(DulceRoom.isOnline);
  Serial.print("Ashley is: "); Serial.println(AshleyRoom.isOnline);
  Serial.print("Fish tank is: "); Serial.println(FishTank.isOnline);
  Serial.print("Master Bedroom is: "); Serial.println(MasterBedRoom.isOnline);
  Serial.print("Weather Station 1 is: "); Serial.println(WeatherStation1.isOnline);
  Serial.print("Cat Feeder is: "); Serial.println(CatFeeder.isOnline);
}
//************************************************************************************************************************
void handleButtonPressed(long x, long y) {
  tft.setCursor(40, 20);

  tft.setTextSize(0.1);
  tft.setFreeFont(FF17);
  tft.setTextColor(TFT_BLACK);

  Serial.println("Pressed");
  if (screen == "main") {
    handleButtonsMainScreen(x, y);
  }
  else if (screen == "lightsmenu") {
    handleButtonsLightsMenu(x, y);
  }
  else if (screen == "fishtankmenu") {
    handleButtonsFishTankMenu(x, y);
  }
  else if (screen == "temperaturesmenu") {
    handleButtonsTemperaturesMenu(x, y);
  }
}
//************************************************************************************************************************
void handleButtonsLightsMenu(long x, long y) {
  tft.fillRect(40, 0, 280, 28, TFT_WHITE);
  if (x > 0 && x < 157 && y > 29 && y < 124) {
    //UPPER LEFT BUTTON
    tft.print("LIGHT ON CHICKEN ROOM");
    if (DulceRoom.isON == false) {
      instructionsToSend.instruction1 = 1;
      drawBmp("/offbutton.bmp", 99, 55);
    }
    else {
      instructionsToSend.instruction1 = 0;
      drawBmp("/onbutton.bmp", 99, 55);
    }
    if (!SendMessageRF24(dulce_node)) {
      drawBmp("/offline.bmp", 99, 55);
      DulceRoom.isOnline = false;
    }
  }
  else if ( x > 165 && x < 321 && y > 29 && y < 134) {
    //UPPER RIGHT BUTTON
    tft.print("LIGHT ON MASTER BEDROOM");
    if (MasterBedRoom.isON == false) {
      instructionsToSend.instruction1 = 1;
      drawBmp("/offbutton.bmp", 257, 55);
    }
    else {
      instructionsToSend.instruction1 = 0;
      drawBmp("/onbutton.bmp", 257, 55);
    }
    if (!SendMessageRF24(masterbedroom_node)) {
      drawBmp("/offline.bmp", 257, 55);
      MasterBedRoom.isOnline = false;
    }
  }
  else if ( x > 0 && x < 157 && y > 135 && y < 241) {
    //LOWER LEFT BUTTON
    tft.print("LIGHT ON ASHLEY ROOM");
    if (AshleyRoom.isON == false) {
      instructionsToSend.instruction1 = 1;
      drawBmp("/offbutton.bmp", 99, 158);
      AshleyRoom.isON = true;
    }
    else {
      instructionsToSend.instruction1 = 0;
      drawBmp("/onbutton.bmp", 99, 158);
      AshleyRoom.isON = false;
    }
    if (!SendMessageRF24(ashley_node)) {
      drawBmp("/offline.bmp", 99, 158);
      AshleyRoom.isOnline = false;
    }
  }
  else if ( x > 165 && x < 321 && y > 140 && y < 241) {
    //LOWER RIGHT BUTTON
    screen = "main";
    drawBmp("/homemenu2.bmp", 0, 0);
    if (connectedtoWIFI) {
      drawBmp("/wifi.bmp", 0, 0);
    }
    else {
      drawBmp("/wifi_off.bmp", 0, 0);
    }
    tft.fillRect(40, 0, 280, 28, TFT_WHITE);
    tft.setTextSize(0.1);
    tft.setFreeFont(FF17);
    tft.setTextColor(TFT_BLACK);
    tft.print("MAIN MENU");
  }
}
//************************************************************************************************************************
void handleButtonsMainScreen(long x, long y) {
  tft.fillRect(40, 0, 280, 28, TFT_WHITE);
  tft.setTextSize(0.1);
  tft.setFreeFont(FF17);
  tft.setTextColor(TFT_BLACK);
  if (x > 0 && x < 157 && y > 29 && y < 124) {
    //Temperature Button
    tft.print("TEMPERATURE BUTTON");
    screen = "temperaturesmenu";
    drawBmp("/lightsmenu.bmp", 0, 0);
    if (connectedtoWIFI) {
      drawBmp("/wifi.bmp", 0, 0);
    }
    else {
      drawBmp("/wifi_off.bmp", 0, 0);
    }
    tft.fillRect(40, 0, 280, 28, TFT_WHITE);
    tft.setCursor(40, 20);  //coordinates and font number
    tft.print("TEMPERATURE VIEW");
    PrintTemperatures1();

  }
  else if ( x > 165 && x < 321 && y > 29 && y < 134) {
    //HOME Button
    tft.print("HOME BUTTON");
  }
  else if ( x > 0 && x < 157 && y > 135 && y < 241) {
    //FISH BUTTON
    
    drawBmp("/FishMenu.bmp", 0, 0);
    if (connectedtoWIFI) {
        drawBmp("/wifi.bmp", 0, 0);
    }
    else {
        drawBmp("/wifi_off.bmp", 0, 0);
    }
    tft.fillRect(40, 0, 280, 28, TFT_WHITE);
    tft.print(Date); tft.print("  "); tft.print(timeClient.getFormattedTime());
    PrintFishTankInfo();
    screen = "fishtankmenu";

    instructionsToSend.instruction1 = 4;
    instructionsToSend.instruction2 = 1;
    SendMessageRF24(fish_node);
  }
  else if ( x > 165 && x < 321 && y > 140 && y < 241) {
    //LIGHT BUTTON
    checkAvailabilities();
    screen = "lightsmenu";
    drawBmp("/lightsmenu.bmp", 0, 0);
    if (connectedtoWIFI) {
      drawBmp("/wifi.bmp", 0, 0);
    }
    else {
      drawBmp("/wifi_off.bmp", 0, 0);
    }
    tft.fillRect(40, 0, 280, 28, TFT_WHITE);
    tft.print("LIGHT MENU");
    DrawLigthButtons();
  }
}
//************************************************************************************************************************
void PrintTemperatures1() {
  //checkAvailabilities();
  //Change font size and color
  tft.setTextSize(1);
  tft.setFreeFont(FF42);
  tft.setTextColor(TFT_BLUE);
  //UPPER LEFT
  tft.fillRect(100, 45, 50, 80, TFT_WHITE);
  tft.setCursor(100, 75);
  tft.println(DulceRoom.temperature);
  tft.setCursor(100, 100);
  tft.print("   F");
  // UPPER RIGHT
  tft.fillRect(260, 45, 50, 80, TFT_WHITE);
  tft.setCursor(262, 75);
  tft.println(FishTank.temperature);
  tft.setCursor(262, 100);
  tft.print("   F");
  //LOWER LEFT
  tft.fillRect(100, 150, 50, 80, TFT_WHITE);
  tft.setCursor(100, 185);
  tft.println(AshleyRoom.temperature);
  tft.setCursor(100, 210);
  tft.print("   F");
  //back to normal font and color
  tft.setTextSize(0.1);
  tft.setFreeFont(FF17);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(40, 20);  //coordinates and font number
  lastTemperaturePrint = millis();
}
//************************************************************************************************************************
void handleButtonsTemperaturesMenu(long x, long y) {
  tft.fillRect(40, 0, 280, 28, TFT_WHITE);
  if (x > 0 && x < 157 && y > 29 && y < 124) {
    //UPPER LEFT BUTTON
    tft.print("TEMP ON CHICKEN ROOM");

  }
  else if ( x > 165 && x < 321 && y > 29 && y < 134) {
    //UPPER RIGHT BUTTON
    tft.print("TEMP ON FISH TANK");

  }
  else if ( x > 0 && x < 157 && y > 135 && y < 241) {
    //LOWER LEFT BUTTON
    tft.print("TEMP ON ASHLEY ROOM");

  }
  else if ( x > 165 && x < 321 && y > 140 && y < 241) {
    //LOWER RIGHT BUTTON
    screen = "main";
    drawBmp("/homemenu2.bmp", 0, 0);
    if (connectedtoWIFI) {
      drawBmp("/wifi.bmp", 0, 0);
    }
    else {
      drawBmp("/wifi_off.bmp", 0, 0);
    }
    tft.print("MAIN MENU");
    tft.fillRect(40, 0, 280, 28, TFT_WHITE);
  }
}
//************************************************************************************************************************
void DrawLigthButtons() {
  if (AshleyRoom.isOnline) {
    if (AshleyRoom.isON) {
      drawBmp("/offbutton.bmp", 99, 158);
    }
    else {
      drawBmp("/onbutton.bmp", 99, 158);
    }
  }
  else {
    drawBmp("/offline.bmp", 99, 158);
  }
  if (DulceRoom.isOnline) {
    if (DulceRoom.isON) {
      drawBmp("/offbutton.bmp", 99, 55);
    }
    else {
      drawBmp("/onbutton.bmp", 99, 55);
    }
  }
  else {
    drawBmp("/offline.bmp", 99, 55);
  }
  if (MasterBedRoom.isOnline) {
    if (MasterBedRoom.isON) {
      drawBmp("/offbutton.bmp", 257, 55);
    }
    else {
      drawBmp("/onbutton.bmp", 257, 55);
    }
  }
  else {
    drawBmp("/offline.bmp", 257, 55);
  }
} //End Void

//************************************************************************************************************************
void handleRoot() {
  server.send(200, "text/html", "<form action=\"/LED\" method=\"POST\"><input type=\"submit\" value=\"Toggle LED\"></form>");
}
//************************************************************************************************************************
void handleLED() {                          // If a POST request is made to URI /LED
  if (pinStatus == LOW) {
    pinStatus = HIGH;
    instructionsToSend.instruction1 = 1;
    SendMessageRF24(fish_node);
  }
  else {
    pinStatus = LOW;
    instructionsToSend.instruction1 = 0;
    SendMessageRF24(fish_node);
  }

  server.sendHeader("Location", "/");       // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
  Serial.print("Status changed...");
  Serial.println(pinStatus);
  tft.print("Status changed...");
  tft.println(pinStatus);
}
//************************************************************************************************************************
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
//************************************************************************************************************************
bool SendMessageRF24(const uint16_t destination_node) {

  RF24NetworkHeader headerSend(destination_node);
  bool ok = network.write(headerSend, &instructionsToSend, sizeof(instructionsToSend));
  if (ok == false) {
    //tft.println("Failed to send.");
    Serial.println("Failed to send.");
  }
  else {
    Serial.println("Sent data.");
  }

  return ok;
}

//************************************************************************************************************************
bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    fs::File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}
//************************************************************************************************************************
String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
//************************************************************************************************************************
void handleButtonsFishTankMenu(long x, long y) {
    if (x > 110 && x < 320 && y > 29 && y < 125) {
        //FEED THE FISH BUTTON
        instructionsToSend.instruction1 = 2;
        SendMessageRF24(fish_node);

    }
    else  if (x > 0 && x < 100 && y > 29 && y < 125) {
        //RETURN TO SCREEN BUTTON (FISH LOGO)
        
        screen = "main";
        drawBmp("/homemenu2.bmp", 0, 0);
        if (connectedtoWIFI) {
            drawBmp("/wifi.bmp", 0, 0);
        }
        else {
            drawBmp("/wifi_off.bmp", 0, 0);
        }
        tft.setTextSize(0.1);
        tft.setFreeFont(FF17);
        tft.setTextColor(TFT_BLACK);
        tft.print("MAIN MENU");
        tft.fillRect(40, 0, 280, 28, TFT_WHITE);
    }
}
//************************************************************************************************************************
void PrintFishTankInfo() {
    //checkAvailabilities();
    //Change font size and color
    tft.setTextSize(0.5);
    tft.setFreeFont(FF42);
    tft.setTextColor(TFT_BLACK);
    tft.fillRect(155, 123, 93, 114, TFT_WHITE);
    tft.fillRect(234, 150, 86, 90, TFT_WHITE);
    if (FishTank.isOnline == true) {
        //print temperature
        tft.setCursor(160, 160);
        tft.print(FishTank.temperature); tft.println(" F");
        
        //print pump status
        tft.setCursor(160, 190);
        if (FishTank.isON == 1) {
            tft.setTextColor(TFT_DARKGREEN);
            tft.println("Running");
            tft.setTextColor(TFT_BLACK);
        }
        else if(FishTank.isON == 0){
            tft.setTextColor(TFT_RED);
            tft.print("Off");
            tft.setTextColor(TFT_BLACK);
        }
        else if (FishTank.isON == 2) {
            tft.setTextColor(TFT_PURPLE);
            tft.print("Feeding");
            tft.setTextColor(TFT_BLACK);
        }
        
        //print feeding timer
        tft.setCursor(160, 220);
        float x; x = FishTank.timerLeft / 1000;
        tft.print(x); tft.print(" sec");
    }
    else {
        tft.setCursor(160, 160);
        tft.println("NA");

        tft.setCursor(160, 190);
        tft.println("Offline");

        tft.setCursor(160, 220);
        tft.print("NA");
    }
 
   
}

//************************************************************************************************************************
void HandleFishNodeMessage(contactorStationInformation Received) {
  FishTank = Received;
  FishTank.isOnline = true;
  Serial.print("Data Size: "); Serial.println(sizeof(Received));
  Serial.print(F("isON: ")); Serial.println(FishTank.isON);
  Serial.print(F("Temperature: ")); Serial.println(FishTank.temperature);
  Serial.print(F("Humidity: ")); Serial.println(FishTank.humidity);
  Serial.print(F("Motion: ")); Serial.println(FishTank.motion);
  Serial.print(F("Timer Left: ")); Serial.println(FishTank.timerLeft);

  if (screen == "temperaturesmenu") {
    PrintTemperatures1();
   
  }
  if (screen == "fishtankmenu") {
      Serial.println(F("Printing in Fish Screen"));
      PrintFishTankInfo();
  }
}
//************************************************************************************************************************
void HandleCatFeeder1NodeMessage(contactorStationInformation Received) {
  CatFeeder = Received;
  if (screen == "temperaturesmenu") {
    PrintTemperatures1();
    Serial.print(F("isON: ")); Serial.println(FishTank.isON);
    Serial.print(F("Temperature: ")); Serial.println(FishTank.temperature);
    Serial.print(F("Humidity: ")); Serial.println(FishTank.humidity);
    Serial.print(F("Motion: ")); Serial.println(FishTank.motion);
  }
}
//************************************************************************************************************************
void HandleDulceNodeMessage(contactorStationInformation Received) {
  DulceRoom = Received;
  if (screen == "temperaturesmenu") {
    PrintTemperatures1();
  }
}
//************************************************************************************************************************
void HandleAshleyNodeMessage(contactorStationInformation Received) {
  Serial.println("Received from Ashley");
  AshleyRoom = Received;
  if (screen == "temperaturesmenu" ) {
    PrintTemperatures1();
  }

  Serial.print("isON: "); Serial.println(AshleyRoom.isON);
  Serial.print("Temperature: "); Serial.println(Received.temperature);
  Serial.print("Humidity: "); Serial.println(AshleyRoom.humidity);
  Serial.print("Motion: "); Serial.println(AshleyRoom.motion);
}
//************************************************************************************************************************
void HandleMasterBedroomNodeMessage(contactorStationInformation Received) {
  MasterBedRoom = Received;
  if (screen == "temperaturesmenu"  ) {
    PrintTemperatures1();
  }
}
//************************************************************************************************************************
void HandleWeather1NodeMessage(contactorStationInformation Received) {
  WeatherStation1 = Received;
  if (screen == "temperaturesmenu" ) {
    PrintTemperatures1();
  }
}
