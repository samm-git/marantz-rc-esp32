// marantz wired remote control web server
// inspired by https://github.com/Arduino-IRremote/Arduino-IRremote
// see https://smallhacks.wordpress.com/2021/07/07/controlling-marantz-amplifier-using-arduino-via-remote-socket/

#define IR_TX_PIN 12 // pin to use for tx, connect via diode to "remote in" socket

// Replace with your network credentials
const char* ssid = "<wifi_ssid>";
const char* password = "<wifi_password>";
// bssid of the wifi AP if you want to connect to fixed base
byte bssid[] = {0x01,0x02,0x03,0x04,0x05,0x06};

/* some definitions from the IRremote Arduino Library */
#define RC5_ADDRESS_BITS 5
#define RC5_COMMAND_BITS 6
#define RC5_EXT_BITS 6
#define RC5_COMMAND_FIELD_BIT 1
#define RC5_TOGGLE_BIT 1
#define RC5_BITS (RC5_COMMAND_FIELD_BIT + RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS)  // 13
#define RC5X_BITS (RC5_BITS + RC5_EXT_BITS) // 19
#define RC5_UNIT 889  // (32 cycles of 36 kHz)
#define RC5_DURATION (15L * RC5_UNIT)  // 13335
#define RC5_REPEAT_PERIOD (128L *RC5_UNIT)  // 113792
#define RC5_REPEAT_SPACE (RC5_REPEAT_PERIOD - RC5_DURATION)  // 100 ms

// Import required libraries
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "html.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

#// define TRACE // print binary commands to serial

uint8_t sLastSendToggleValue = 0;

/* 
 *  normal Philips RC-5 as in the https://en.wikipedia.org/wiki/RC-5
 *  code taken from IRremote with some changes
 */

int sendRC5(uint8_t aAddress, uint8_t aCommand, uint_fast8_t aNumberOfRepeats)
{

  digitalWrite(IR_TX_PIN, LOW);

  uint16_t tIRData = ((aAddress & 0x1F) << RC5_COMMAND_BITS);

  if (aCommand < 0x40)
  {
    // set field bit to lower field / set inverted upper command bit
    tIRData |= 1 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS);
  }
  else
  {
    // let field bit zero
    aCommand &= 0x3F;
  }

  tIRData |= aCommand;

  tIRData |= 1 << RC5_BITS;

  if (sLastSendToggleValue == 0)
  {
    sLastSendToggleValue = 1;
    // set toggled bit
    tIRData |= 1 << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS);
  }
  else
  {
    sLastSendToggleValue = 0;
  }

  uint_fast8_t tNumberOfCommands = aNumberOfRepeats + 1;

  while (tNumberOfCommands > 0)
  {
    for (int i = 13; 0 <= i; i--)
    {
#ifdef TRACE
      Serial.print((tIRData &(1 << i)) ? '1' : '0');
#endif
        (tIRData &(1 << i)) ? send_1() : send_0();
    }
    tNumberOfCommands--;
    if (tNumberOfCommands > 0)
    {
      // send repeated command in a fixed raster
      delay(RC5_REPEAT_SPACE / 1000);
    }
#ifdef TRACE
    Serial.print("\n");
#endif
  }

  digitalWrite(IR_TX_PIN, LOW);
  return 0;
}

/* 
 *  Marantz 20 bit RC5 extension, see 
 *  http://lirc.10951.n7.nabble.com/Marantz-RC5-22-bits-Extend-Data-Word-possible-with-lircd-conf-semantic-td9784.html 
 *  could be combined with sendRC5, but ATM split to simplify debugging
 */
 
int sendRC5_X(uint8_t aAddress, uint8_t aCommand, uint8_t aExt, uint_fast8_t aNumberOfRepeats)
{

  uint32_t tIRData = (uint32_t)(aAddress & 0x1F) << (RC5_COMMAND_BITS + RC5_EXT_BITS);

  digitalWrite(IR_TX_PIN, LOW);

  if (aCommand < 0x40)
  {
    // set field bit to lower field / set inverted upper command bit
    tIRData |= (uint32_t) 1 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  }
  else
  {
    // let field bit zero
    aCommand &= 0x3F;
  }

  tIRData |= (uint32_t)(aExt & 0x3F);
  tIRData |= (uint32_t) aCommand << RC5_EXT_BITS;
  tIRData |= (uint32_t) 1 << RC5X_BITS;

  if (sLastSendToggleValue == 0)
  {
    sLastSendToggleValue = 1;
    // set toggled bit
    tIRData |= (uint32_t) 1 << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  }
  else
  {
    sLastSendToggleValue = 0;
  }

  uint_fast8_t tNumberOfCommands = aNumberOfRepeats + 1;

  while (tNumberOfCommands > 0)
  {

    for (int i = 19; 0 <= i; i--)
    {
#ifdef TRACE
      Serial.print((tIRData &((uint32_t) 1 << i)) ? '1' : '0');
#endif
        (tIRData &((uint32_t) 1 << i)) ? send_1() : send_0();
      if (i == 12)
      {
#ifdef TRACE
        Serial.print("<p>");
#endif
        // space marker for marantz rc5 extension
        delayMicroseconds(RC5_UNIT *2 *2);
      }
    }
#ifdef TRACE
    Serial.print("\n");
#endif
    tNumberOfCommands--;
    if (tNumberOfCommands > 0)
    {
      // send repeated command in a fixed raster
      delay(RC5_REPEAT_SPACE / 1000);
    }
  }
  digitalWrite(IR_TX_PIN, LOW);
  return 0;
}

void send_0()
{
  digitalWrite(IR_TX_PIN, HIGH);
  delayMicroseconds(RC5_UNIT);
  digitalWrite(IR_TX_PIN, LOW);
  delayMicroseconds(RC5_UNIT);
}

void send_1()
{
  digitalWrite(IR_TX_PIN, LOW);
  delayMicroseconds(RC5_UNIT);
  digitalWrite(IR_TX_PIN, HIGH);
  delayMicroseconds(RC5_UNIT);
}

void setup()
{
  //start serial connection
  Serial.begin(115200);
  // configure output pin
  pinMode(IR_TX_PIN, OUTPUT);
  // Connect to Wi-Fi
  WiFi.setHostname("marantz");

  // Disable wifi power-save mode to improve stability
  WiFi.mode (WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);

  WiFi.begin(ssid, password, 0, bssid);
  // or, if you dont want bssid locking, use
  // WiFi.begin(ssid, password); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
  // Enable automatic reconnect to AP
  WiFi.setAutoReconnect(true);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to <ESP_IP>/update?button=<name>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam("button")) {
      inputMessage1 = request->getParam("button")->value();
      if (strcmp ("standby", inputMessage1.c_str()) == 0) {
         sendRC5(16, 12, 1);
      }
      if (strcmp ("phono", inputMessage1.c_str()) == 0) {
         sendRC5(21, 63, 1);
      }
      if (strcmp ("cd", inputMessage1.c_str()) == 0) {
         sendRC5(20, 63, 1);
      }
      if (strcmp ("tuner", inputMessage1.c_str()) == 0) {
         sendRC5(17, 63, 1);
      }
      if (strcmp ("aux1", inputMessage1.c_str()) == 0) {
         sendRC5_X(16, 0, 6, 1);
      }
      if (strcmp ("aux2", inputMessage1.c_str()) == 0) {
         sendRC5_X(16, 0, 7, 1);
      }
      if (strcmp ("dcc", inputMessage1.c_str()) == 0) {
         sendRC5(23, 63, 1);
      }
      if (strcmp ("tape", inputMessage1.c_str()) == 0) {
         sendRC5(18, 63, 1);
      }
      if (strcmp ("volume_up", inputMessage1.c_str()) == 0) {
         sendRC5(16, 16, 1);
      }
      if (strcmp ("volume_down", inputMessage1.c_str()) == 0) {
         sendRC5(16, 17, 1);
      }
    }
    else {
      inputMessage1 = "No message sent";
    }
    Serial.print("Button: ");
    Serial.print(inputMessage1);
    Serial.print("\n");
    request->send(200, "text/plain", "OK\n");
  });
  
  // Start server
  server.begin();
}

void loop()
{
}
