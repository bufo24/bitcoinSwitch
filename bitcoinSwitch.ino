#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>

fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true
#include <JC_Button.h>
#include "qrcoded.h"

#include <ArduinoJson.h>

#include <WebSocketsClient.h>

#define PARAM_FILE "/elements.json"

/////////////////////////////////
///////////CHANGE////////////////
/////////////////////////////////

bool usingM5 = false; // false if not using M5Stack          
int portalPin = 4;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

// Access point variables
String payloadStr;
String password;
String serverFull;
String lnbitsServer;
String ssid;
String wifiPassword;
String deviceId;
String highPin;
String timePin;
String pinFlip;
String lnurl;
String dataId;
String payReq;

int balance;
int oldBalance;

bool paid;
bool down = false;
bool triggerUSB = false; 

TFT_eSPI tft = TFT_eSPI();
WebSocketsClient webSocket;

struct KeyValue {
  String key;
  String value;
};

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  if(usingM5 == true){
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(true);
    logoScreen();
  }
  const byte BUTTON_PIN_A = 39;
  Button BTNA(BUTTON_PIN_A);
  BTNA.begin();
  int timer = 0;
  pinMode (2, OUTPUT);

  while (timer < 2000)
  {
    digitalWrite(2, LOW);
    if (usingM5 == true){
      if (BTNA.read() == 1){
        Serial.println("Launch portal");
        triggerUSB = true;
        timer = 5000;
      }
    }
    else{
      Serial.println(touchRead(portalPin));
      if(touchRead(portalPin) < 60){
        Serial.println("Launch portal");
        triggerUSB = true;
        timer = 5000;
      }
    }
    digitalWrite(2, HIGH);
    timer = timer + 100;
    delay(300);
  }
  timer = 0;

  FlashFS.begin(FORMAT_ON_FAIL);

  // get the saved details and store in global variables
  readFiles();

  delay(2000);
  
  WiFi.begin(ssid.c_str(), wifiPassword.c_str());
  while (WiFi.status() != WL_CONNECTED && timer < 8000) {
    delay(1000);
    Serial.print(".");
    timer = timer + 1000;
    if(timer > 7000){
      triggerUSB = true;
    }
  }
  
  if (triggerUSB == true)
  {
    Serial.println("USB triggered");
    configOverSerialPort();
  }

  lnbitsScreen();
  delay(1000);
  pinMode(highPin.toInt(), OUTPUT);
  onOff();
  Serial.println(lnbitsServer + "/lnurldevice/ws/" + deviceId);
  webSocket.beginSSL(lnbitsServer, 443, "/lnurldevice/ws/" + deviceId);
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  while(WiFi.status() != WL_CONNECTED){
    if(usingM5 == true){
      connectionError();
    }
    Serial.println("Failed to connect");
    delay(500);
  }
  Serial.println(highPin.toInt());

  if(lnurl == "true"){
    if(usingM5 == true){
      qrdisplayScreen();
    }
    paid = false;
    while(paid == false){
      webSocket.loop();
      if(paid){
        Serial.println(payloadStr);
        highPin = getValue(payloadStr, '-', 0);
        Serial.println(highPin);
        timePin = getValue(payloadStr, '-', 1);
        Serial.println(timePin);
        if(usingM5 == true){
          completeScreen();
        }
        onOff();
      }
    }
    Serial.println("Paid");
    if(usingM5 == true){
      paidScreen();
    }
  }
  else{
    getInvoice();
    if(down){
      errorScreen();
      getInvoice();
      delay(5000);
    }
    if(payReq != ""){
      if(usingM5 == true){
        qrdisplayScreen();
      }
      delay(5000);
    }
    while(paid == false && payReq != ""){
      webSocket.loop();
      if(paid){
        Serial.println(payloadStr);
        highPin = getValue(payloadStr, '-', 0);
        Serial.println(highPin);
        timePin = getValue(payloadStr, '-', 1);
        Serial.println(timePin);
        if(usingM5 == true){
          completeScreen();
        }
        onOff();
      }
    }
    payReq = "";
    dataId = "";
    paid = false;
    delay(4000);
  }
}

//////////////////HELPERS///////////////////

void onOff()
{ 
  pinMode (highPin.toInt(), OUTPUT);
  if(pinFlip == "true"){
    digitalWrite(highPin.toInt(), LOW);
    delay(timePin.toInt());
    digitalWrite(highPin.toInt(), HIGH); 
    delay(2000);
  }
  else{
    digitalWrite(highPin.toInt(), HIGH);
    delay(timePin.toInt());
    digitalWrite(highPin.toInt(), LOW); 
    delay(2000);
  }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void readFiles()
{
  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile)
  {
    StaticJsonDocument<1500> doc;
    DeserializationError error = deserializeJson(doc, paramFile.readString());

    const JsonObject maRoot0 = doc[0];
    const char *maRoot0Char = maRoot0["value"];
    password = maRoot0Char;
    Serial.println(password);

    const JsonObject maRoot1 = doc[1];
    const char *maRoot1Char = maRoot1["value"];
    ssid = maRoot1Char;
    Serial.println(ssid);

    const JsonObject maRoot2 = doc[2];
    const char *maRoot2Char = maRoot2["value"];
    wifiPassword = maRoot2Char;
    Serial.println(wifiPassword);

    const JsonObject maRoot3 = doc[3];
    const char *maRoot3Char = maRoot3["value"];
    serverFull = maRoot3Char;
    lnbitsServer = serverFull.substring(5, serverFull.length() - 38);
    deviceId = serverFull.substring(serverFull.length() - 22);

    const JsonObject maRoot4 = doc[4];
    const char *maRoot4Char = maRoot4["value"];
    lnurl = maRoot4Char;
    Serial.println(lnurl);
  }
  paramFile.close();
}

//////////////////DISPLAY///////////////////

void serverError()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(3);
  tft.setTextColor(TFT_RED);
  tft.println("Server connect fail");
}

void connectionError()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(3);
  tft.setTextColor(TFT_RED);
  tft.println("Wifi connect fail");
}

void connection()
{
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(3);
  tft.setTextColor(TFT_RED);
  tft.println("Wifi connected");
}

void logoScreen()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_PURPLE);
  tft.println("bitcoinSwitch");
}

void portalLaunched()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_PURPLE);
  tft.println("PORTAL LAUNCH");
}

void processingScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(40, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.println("PROCESSING");
}

void lnbitsScreen()
{ 
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.setTextSize(3);
  tft.setTextColor(TFT_BLACK);
  tft.println("POWERED BY LNBITS");
}

void portalScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(30, 80);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);
  tft.println("PORTAL LAUNCHED");
}

void paidScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(110, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.println("PAID");
}

void completeScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(60, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.println("COMPLETE");
}

void errorScreen()
{ 
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(70, 80);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.println("ERROR");
}

void qrdisplayScreen()
{
  String qrCodeData;
  if(lnurl == "true"){
    qrCodeData = lnurl;
  }
  else{
    qrCodeData = payReq;
  }
  tft.fillScreen(TFT_WHITE);
  qrCodeData.toUpperCase();
  const char *qrDataChar = qrCodeData.c_str();
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 11, 0, qrDataChar);
  for (uint8_t y = 0; y < qrcoded.size; y++)
  {
    // Each horizontal module
    for (uint8_t x = 0; x < qrcoded.size; x++)
    {
      if (qrcode_getModule(&qrcoded, x, y))
      {
        tft.fillRect(65 + 3 * x, 20 + 3 * y, 3, 3, TFT_BLACK);
      }
      else
      {
        tft.fillRect(65 + 3 * x, 20 + 3 * y, 3, 3, TFT_WHITE);
      }
    }
  }
}

//////////////////NODE CALLS///////////////////

void checkConnection(){
  WiFiClientSecure client;
  client.setInsecure();
  const char* lnbitsserver = lnbitsServer.c_str();
  if (!client.connect(lnbitsserver, 443)){
    down = true;
    serverError();
    return;   
  }
}

void getInvoice(){
  WiFiClientSecure client;
  client.setInsecure();
  const char* lnbitsserver = lnbitsServer.c_str();
  if (!client.connect(lnbitsserver, 443)){
    down = true;
    return;   
  }
  StaticJsonDocument<500> doc;
  DeserializationError error;
  char c;
  String line;
  String url = "/lnurldevice/api/v1/lnurl/";
  client.print(String("GET ") + url + deviceId +" HTTP/1.1\r\n" +
                "Host: " + lnbitsserver + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Content-Type: application/json\r\n" +
                "Connection: close\r\n\r\n");
  while (client.connected()) {
    line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String callback;
  while (client.available()) {
    line = client.readStringUntil('\n');
    callback = line;
  }
  Serial.println(callback);
  delay(500);
  error = deserializeJson(doc, callback);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  const char* callbackChar = doc["callback"];
  String callbackStr = callbackChar;
  getCallback(callbackStr);
}

void getCallback(String callbackStr){
  WiFiClientSecure client;
  client.setInsecure();
  const char* lnbitsserver = lnbitsServer.c_str();
  if (!client.connect(lnbitsserver, 443)){
    down = true;
    return;   
  }
  StaticJsonDocument<500> doc;
  DeserializationError error;
  char c;
  String line;
  client.print(String("GET ") + callbackStr.substring(8 + lnbitsServer.length()) +" HTTP/1.1\r\n" +
                "Host: " + lnbitsserver + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Content-Type: application/json\r\n" +
                "Connection: close\r\n\r\n");
   while (client.connected()) {
   String line = client.readStringUntil('\n');
   Serial.println(line);
    if (line == "\r") {
      break;
    }
  }
  line = client.readString();
  Serial.println(line);
  error = deserializeJson(doc, line);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  const char* temp = doc["pr"];
  payReq = temp;
}
//////////////////WEBSOCKET///////////////////


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WSc] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            {
                Serial.printf("[WSc] Connected to url: %s\n",  payload);
                

			    // send message to server when Connected
				webSocket.sendTXT("Connected");
            }
            break;
        case WStype_TEXT:
            payloadStr = (char*)payload;
            paid = true;
            
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
    }

}
