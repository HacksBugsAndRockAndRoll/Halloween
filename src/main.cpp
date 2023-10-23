#include "DFRobotDFPlayerMini.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <index.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncElegantOTA.h>


void handleLDR();
const int LDR = 35;
enum Mode {WEB, LIGHT};
Mode currentMode = WEB;
const int lightDark = 3500;
const int lightBright = 3000;
long lastLdrRead = -1;
long ldrReadInterval = 1000;
long ldrOn = -1;
long ldrMinOnCycles = 20;
int ldrNumOff = 0;

void setupPlayer();
void playRandom();
void printDetail(uint8_t type, int value);
int numberOfFiles = 16;
DFRobotDFPlayerMini player;

void setupWiFi();
void setupWebServer();
AsyncWebServer server(80);
DNSServer dns;

const char* PARAM_MESSAGE = "message";
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {
  pinMode(LDR, INPUT);        
  //debug serial
  Serial.begin(115200);
  //player serial
  Serial2.begin(9600);

  randomSeed(analogRead(0));
 
  setupPlayer();
  setupWebServer();
}
void setupWiFi(){
    AsyncWiFiManager wifiManager(&server,&dns);
    wifiManager.setTimeout(180);
    wifiManager.autoConnect("HalloweenSounds");
}
void setupPlayer(){
  Serial.printf("setup player\n");
  
  if (player.begin(Serial2,true,true)) {
    Serial.println("OK");
    player.volume(20);
  } else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }
}

void loop() {
  handleLDR();

}

void handleLDR(){
  if(lastLdrRead + ldrReadInterval < millis() && currentMode == LIGHT){
    int ldr = analogRead(LDR);
    lastLdrRead = millis();
    Serial.printf("ldr: %d\n",ldr);
    if( ldr <= lightBright) {
      //bright
      if(ldrOn == -1){
        Serial.println("switching player on");
        ldrOn = millis();
        player.randomAll();
        ldrNumOff = 0;
      }
    }else if(ldr >= lightDark){
      //dark
      ldrNumOff++;
      if(ldrOn != -1 && ldrNumOff > ldrMinOnCycles){
        Serial.println("switching player off");
        player.stop();
        ldrOn = -1;
        ldrNumOff = 0;
      }
    }
  }
}

void playRandom(){
  player.play(random(numberOfFiles)+1);
}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}
void setupWebServer() {
    int wificounter = 0;
    while(!WiFi.isConnected()){
      setupWiFi();
      if(++wificounter > 3){
        Serial.printf("giving up on WiFi\n");
        return;
      }
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", INDEX_HTML);
    });

    server.on("/loop", HTTP_GET, [] (AsyncWebServerRequest *request) {
        currentMode = WEB;
        char buffer[1024];
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
        } else {
            sprintf(buffer, "playing random loop");
            player.randomAll();
        }
        Serial.println(buffer);
        request->redirect("/");
    });
    server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
        char buffer[1024];
        String message;
        int f = 0;
        currentMode = WEB;
        if (request->hasParam("file")) {
            message = request->getParam("file")->value();
            f = atoi(message.c_str());
            if(f > numberOfFiles)
              f=numberOfFiles;
            if(f < 0)
              f=1;
            sprintf(buffer, "playing file %d",f);
        } else {
            f = random(numberOfFiles)+1;
            sprintf(buffer, "playing random %d",f);
        }
        player.play(f);
        Serial.println(buffer);
        playRandom();
        request->redirect("/");
    });
    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
        currentMode = WEB;
        player.stop();
        Serial.println("stopped");
        request->redirect("/");
    });
    server.on("/ldr", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("enable LDR mode..");
        currentMode = LIGHT;
        player.stop();
        request->redirect("/");
    });
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("restarting..");
        request->redirect("/");
        ESP.restart();
    });
    server.on("/volume", HTTP_GET, [] (AsyncWebServerRequest *request) {
        char buffer[1024];
        String message;
        if (request->hasParam("set")) {
            message = request->getParam("set")->value();
            int v = atoi(message.c_str());
            if(v > 30)
              v=30;
            if(v < 0)
              v=0;
            player.volume(v);
            sprintf(buffer, "setting volume to %d",v);
        } else {
            sprintf(buffer, "current volume is %d",player.readVolume());
        }
        Serial.println(buffer);
        request->redirect("/");
    });
    server.onNotFound(notFound);
    AsyncElegantOTA.begin(&server); 
    server.begin();
}