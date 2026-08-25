/*
 * F4GOH 2026
pio project init --ide netbeans --board lolin32
GPS TX  ─────────> GPIO16 (ESP32 RX2)
GPS RX  <───────── GPIO17 (ESP32 TX2)
GPS GND ────────── GND
GPS VCC ────────── 3.3V ou 5V suivant module
PGM button GPIO 0   active low
MENU button GPIO 14 active low
LED BEAT GPIO 2
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "AprsClient.h"
#include "Position.h"
#include "ParserNMEA.h"


#define SETUP_PIN 14
#define RXD2 16
#define TXD2 17
#define LED_BEAT 2

void syncGps();
void txing();


AprsClient aprs(false);
Position *pos=nullptr;
ParserNMEA gps;

Preferences preferences;
byte second_prec = 0;
bool newData = false;

typedef struct {
  char callsign[10] = "F4GOH-4";
  char comment[31] = "hello";
  char tableSymbol = '/';
  char symbol = '>';
  uint8_t minute = 1;
  uint8_t second = 0;
  bool smart = false;
  bool compressed = false;
  bool altitude = false;
  bool display = false;
  uint32_t baud = 9600;
} configuration;

configuration config;

char mqttServer[64] = "192.168.1.20";

class IntParameter : public WiFiManagerParameter {
public:
  IntParameter(const char *id,const char *placeholder,long value,const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id,placeholder,String(value).c_str(),length,"",WFM_LABEL_BEFORE);
  }

  long getValue() {
    return String(WiFiManagerParameter::getValue()).toInt();
  }
};

class BoolParameter : public WiFiManagerParameter {
public:
  BoolParameter(const char *id,const char *placeholder,bool value) : WiFiManagerParameter("") {
    init(id,placeholder,value ? "ON" : "OFF",4,"",WFM_LABEL_BEFORE);
  }

  bool getValue() {
    return !strcasecmp(WiFiManagerParameter::getValue(),"ON");
  }
};

class BaudParameter : public WiFiManagerParameter {
public:
  BaudParameter(const char *id, const char *placeholder, uint32_t value)
    : WiFiManagerParameter("") {
    init(id, placeholder, String(value).c_str(), 6, "", WFM_LABEL_BEFORE);
  }

  uint32_t getValue() {
    uint32_t baud = String(WiFiManagerParameter::getValue()).toInt();

    switch (baud) {
      case 1200:
      case 2400:
      case 4800:
      case 9600:
      case 19200:
      case 38400:
      case 57600:
      case 115200:
        return baud;

      default:
        return 9600;
    }
  }
};


void loadConfig()
{
  preferences.begin("aprs",true);
  preferences.getString("callsign",config.callsign,sizeof(config.callsign));
  preferences.getString("comment",config.comment,sizeof(config.comment));
  config.tableSymbol = preferences.getChar("table",config.tableSymbol);
  config.symbol = preferences.getChar("symbol",config.symbol);
  config.minute = preferences.getUChar("minute",config.minute);
  config.second = preferences.getUChar("second",config.second);
  config.smart = preferences.getBool("smart",config.smart);
  config.compressed = preferences.getBool("compressed",config.compressed);
  config.altitude = preferences.getBool("altitude",config.altitude);
  config.display = preferences.getBool("display",config.display);
  config.baud = preferences.getUInt("baud", config.baud);
  preferences.getString("mqtt",mqttServer,sizeof(mqttServer));
  preferences.end();
}

void saveConfig()
{
  preferences.begin("aprs",false);
  preferences.putString("callsign",config.callsign);
  preferences.putString("comment",config.comment);
  preferences.putChar("table",config.tableSymbol);
  preferences.putChar("symbol",config.symbol);
  preferences.putUChar("minute",config.minute);
  preferences.putUChar("second",config.second);
  preferences.putBool("smart",config.smart);
  preferences.putBool("compressed",config.compressed);
  preferences.putBool("altitude",config.altitude);
  preferences.putBool("display",config.display);
  preferences.putUInt("baud",config.baud);
  preferences.putString("mqtt",mqttServer);
  preferences.end();
}

void printConfig()
{
  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("Configuration APRS");
  Serial.println("--------------------------------");
  Serial.print("WiFi SSID : ");
  Serial.println(WiFi.SSID());
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());
  Serial.print("Callsign : ");
  Serial.println(config.callsign);
  Serial.print("Comment : ");
  Serial.println(config.comment);
  Serial.print("Table : ");
  Serial.println(config.tableSymbol);
  Serial.print("Symbol : ");
  Serial.println(config.symbol);
  Serial.print("Interval : ");
  Serial.print(config.minute);
  Serial.print(" min ");
  Serial.print(config.second);
  Serial.println(" sec");
  Serial.print("SmartBeaconing : ");
  Serial.println(config.smart ? "ON" : "OFF");
  Serial.print("Compressed : ");
  Serial.println(config.compressed ? "ON" : "OFF");
  Serial.print("Altitude : ");
  Serial.println(config.altitude ? "ON" : "OFF");
  Serial.print("Display : ");
  Serial.println(config.display ? "ON" : "OFF");
  Serial.print("Baud : ");
  Serial.println(config.baud);
  Serial.print("MQTT : ");
  Serial.println(mqttServer);
  Serial.println("--------------------------------");
}

void openConfigPortal()
{
  Serial.println();
  Serial.println("================================");
  Serial.println("OUVERTURE PORTAIL CONFIGURATION");
  Serial.println("================================");

  WiFiManager wm;

  WiFiManagerParameter param_callsign("callsign","Callsign + SSID",config.callsign,sizeof(config.callsign));
  WiFiManagerParameter param_comment("comment","Comment",config.comment,sizeof(config.comment));
  WiFiManagerParameter param_table("table","Table Symbol",&config.tableSymbol,1);
  WiFiManagerParameter param_symbol("symbol","Symbol",&config.symbol,1);

  IntParameter param_minute("minute","Beacon interval (min)",config.minute,4);
  IntParameter param_second("second","Beacon interval (sec)",config.second,4);
  BaudParameter param_baud("baud", "Baud rate", config.baud);


  BoolParameter param_smart("smart","SmartBeaconing",config.smart);
  BoolParameter param_compressed("compressed","Compressed",config.compressed);
  BoolParameter param_altitude("altitude","Altitude",config.altitude);
  BoolParameter param_display("display","Display",config.display);

  WiFiManagerParameter param_mqtt("mqtt","Serveur MQTT",mqttServer,sizeof(mqttServer));

  wm.addParameter(&param_callsign);
  wm.addParameter(&param_comment);
  wm.addParameter(&param_table);
  wm.addParameter(&param_symbol);
  wm.addParameter(&param_minute);
  wm.addParameter(&param_second);
  wm.addParameter(&param_baud);
  wm.addParameter(&param_smart);
  wm.addParameter(&param_compressed);
  wm.addParameter(&param_altitude);
  wm.addParameter(&param_display);
  wm.addParameter(&param_mqtt);

  const char *menu[] = {"wifi","param","info","sep","exit"};
  wm.setMenu(menu,5);

  wm.setParamsPage(true);

  wm.setConfigPortalTimeout(300);

  wm.startConfigPortal("APRS-POCKET");

  Serial.println();
  Serial.println("================================");
  Serial.println("LECTURE PARAMETRES APRS");
  Serial.println("================================");

  strncpy(config.callsign,param_callsign.getValue(),sizeof(config.callsign) - 1);
  config.callsign[sizeof(config.callsign) - 1] = '\0';

  strncpy(config.comment,param_comment.getValue(),sizeof(config.comment) - 1);
  config.comment[sizeof(config.comment) - 1] = '\0';

  if (strlen(param_table.getValue()) > 0) {
    config.tableSymbol = param_table.getValue()[0];
  }

  if (strlen(param_symbol.getValue()) > 0) {
    config.symbol = param_symbol.getValue()[0];
  }

  long minute = param_minute.getValue();

  if (minute >= 0 && minute <= 255) {
    config.minute = minute;
  }

  long second = param_second.getValue();

  if (second >= 0 && second <= 59) {
    config.second = second;
  }

  config.baud = param_baud.getValue();
  
  config.smart = param_smart.getValue();
  config.compressed = param_compressed.getValue();
  config.altitude = param_altitude.getValue();
  config.display = param_display.getValue();

  strncpy(mqttServer,param_mqtt.getValue(),sizeof(mqttServer) - 1);
  mqttServer[sizeof(mqttServer) - 1] = '\0';

  Serial.print("Callsign recu : ");
  Serial.println(config.callsign);
  Serial.print("Comment recu : ");
  Serial.println(config.comment);
  Serial.print("Table recu : ");
  Serial.println(config.tableSymbol);
  Serial.print("Symbol recu : ");
  Serial.println(config.symbol);
  Serial.print("Minute recue : ");
  Serial.println(config.minute);
  Serial.print("Second recu : ");
  Serial.println(config.second);
  Serial.print("Baud recu : ");
  Serial.println(config.baud);
  Serial.print("SmartBeaconing : ");
  Serial.println(config.smart ? "ON" : "OFF");
  Serial.print("Compressed : ");
  Serial.println(config.compressed ? "ON" : "OFF");
  Serial.print("Altitude : ");
  Serial.println(config.altitude ? "ON" : "OFF");
  Serial.print("Display : ");
  Serial.println(config.display ? "ON" : "OFF");
  Serial.print("MQTT : ");
  Serial.println(mqttServer);

  saveConfig();

  Serial.println("Configuration APRS sauvegardee");

  loadConfig();

  Serial.println();
  Serial.println("Configuration rechargee depuis Preferences");

  printConfig();
}

void setup()
{
  Serial.begin(115200);
  
   
  pinMode(SETUP_PIN,INPUT_PULLUP);
  pinMode(LED_BEAT, OUTPUT);
  digitalWrite(LED_BEAT, LOW);
  
  loadConfig();

  Serial.println();
  Serial.println("Configuration chargee depuis Preferences");

  printConfig();

  if (digitalRead(SETUP_PIN) == LOW)
  {
    Serial.println();
    Serial.println("Bouton configuration appuye");

    delay(100);

    openConfigPortal();

    while (digitalRead(SETUP_PIN) == LOW) {
      delay(10);
    }

    Serial.println();
    Serial.println("Configuration terminee");
  }
  else
  {
    Serial.println();
    Serial.println("Connexion WiFi...");

    WiFiManager wm;

    if (!wm.autoConnect("APRS-POCKET"))
    {
      Serial.println("Connexion WiFi impossible");
      delay(1000);
      ESP.restart();
    }

    Serial.println("WiFi connecte");

    printConfig();
    
    pos = new Position(47.89027,0.276740,config.comment,config.tableSymbol,config.symbol);
        
    aprs.connectToServer("rotate.aprs2.net", 14580);

    aprs.authenticate(String(config.callsign),""); //aprs.authenticate("F4GOH-4", "r/48.85/2.35/50");

    aprs.startListening([](const String& message) {
        Serial.print("[APRS RX] ");
        Serial.println(message);
    });

        Serial.print("[RAM] libre : ");
        Serial.println(ESP.getFreeHeap());

        Serial.print("[RAM] minimum : ");
        Serial.println(ESP.getMinFreeHeap());

        Serial.print("[APRS] stack libre : ");
        Serial.println(uxTaskGetStackHighWaterMark(nullptr));

    Serial2.begin(config.baud,SERIAL_8N1,RXD2,TXD2);
    //test
    //Serial.print("[APRS] PDU : ");
    //Serial.println(pos->getPduAprs(config.compressed,config.altitude));
    //aprs.sendPosition(*pos,config.compressed,config.altitude);

  }
}

void loop() {
    static uint32_t lastButton = 0;

    if (digitalRead(SETUP_PIN) == LOW) {
        if (millis() - lastButton > 1000) {
            lastButton = millis();

            delay(50);

            if (digitalRead(SETUP_PIN) == LOW) {
                openConfigPortal();

                loadConfig();

                Serial.println();
                Serial.println("Configuration rechargee depuis Preferences");

                printConfig();
                Serial2.begin(config.baud,SERIAL_8N1,RXD2,TXD2);
                //update position
                pos->setComment(config.comment);
                pos->setSymbol(config.symbol);
                while (digitalRead(SETUP_PIN) == LOW) {
                    delay(10);
                }
                Serial.println("Configuration terminee");
            }
        }
    }
    syncGps();
}

void syncGps() {
    // For one second we parse GPS data and report some key values
    newData = false;
    while (Serial2.available()) {
        char c = Serial2.read();
        //Serial.write(c); // uncomment this line if you want to see the GPS data flowing
        if (gps.encode(c)) // Did a new valid sentence come in?
            newData = true;
    }
    if (newData) {
        newData = false;
        if (gps.isTimeValid() && gps.getSecond() != second_prec) {
            second_prec = gps.getSecond();
            Serial.println(gps.getTime());
            digitalWrite(LED_BEAT, digitalRead(LED_BEAT) ^ 1);
            
            if (gps.isCoordValid()) {
                float lat = gps.getLatDec();
                float lon = gps.getLongDec();
                pos->setLatitude(lat);
                pos->setLongitude(lon);
                if (gps.isAltValid()) pos->setAltitude(gps.getAltitudeMeters());
                if (config.minute == 0) {
                    if (config.second > 0 && gps.getSecond() % config.second == 0) {
                        txing();
                    }
                } else {
                    if ((gps.getMinute() % config.minute == 0) && (gps.getSecond() == config.second)) {
                        txing();
                    }
                }
            }
        }
    }
 
}


void txing()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("              APRS TX");
  Serial.println("========================================");

  Serial.println("[GPS] Position");

  Serial.printf("  Latitude  : %.6f°\r\n", pos->getLatitude());
  Serial.printf("  Longitude : %.6f°\r\n", pos->getLongitude());

  Serial.println("----------------------------------------");

  const char *pdu = pos->getPduAprs(config.compressed, config.altitude);

  Serial.println("[APRS] PDU générée :");
  Serial.println(pdu);

  Serial.println("----------------------------------------");

  Serial.println("[APRS] Envoi position...");

  aprs.sendPosition(*pos, config.compressed, config.altitude);

  Serial.println("[APRS] Position envoyée");
  Serial.println("========================================");
  Serial.println();
}
