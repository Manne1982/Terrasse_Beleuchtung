#include <Arduino.h>
//Für Wifi-Verbindung notwendig
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//für Webserver notwendig
#include <ESPAsyncWebServer.h>
//MQTT
#include <PubSubClient.h>
//für Uhrzeitabruf notwendig
#include <NTPClient.h>
#include <WiFiUdp.h>
//Allgemeine Bibliotheken
#include <string.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
//Projektspezifisch
#include "HTML_Var.h"

//Funktionsdefinitionen
void WiFi_Start_STA(char *ssid_sta, char *password_sta);
void WiFi_Start_AP();
bool WIFIConnectionCheck(bool with_reconnect);
void notFound(AsyncWebServerRequest *request);
void EinstSpeichern();
void EinstLaden();
char ResetVarLesen();
void ResetVarSpeichern(char Count);
bool MQTTinit();  //Wenn verbunden Rückgabewert true
void MQTT_callback(char* topic, byte* payload, unsigned int length);
bool MQTT_sendOutputState();
bool MQTT_sendInputState();
String IntToStr(int _var);
String IntToStr(float _var);
String IntToStrHex(int _var);
String IntToStr(uint32_t _var);
int switchRelais(int _var);

//Projektvariablen
NWConfig varConfig;
unsigned long Break_h = 0;
unsigned long Break_s = 0;
unsigned int stateRelais = 0;
String RelaisText[2] = {"Bewegungsmelder", "Terrasse"};
int InputState;  //Status am Eingang 2 (Bewegungsmelder aktiv)
unsigned long Ausschaltverz = 0; //wenn diese überschritten, wird Status auf LOW gesetzt (50 Hz Ripple ausgleich)


//Erstellen Serverelement
AsyncWebServer server(80);

// WiFi Variablen
const char *ssid_AP = "ESP_Terrasse_01";
const char *password_AP = ""; //Wenn Passwort, dann muss es eine Laenge von 8 - 32 Zeichen haben

//Uhrzeit Variablen
WiFiUDP ntpUDP;
NTPClient *timeClient;

//MQTT Variablen
WiFiClient wifiClient;
PubSubClient MQTTclient(wifiClient);


void setup() {
  Serial.end();
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
//  digitalWrite(0, LOW);
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  InputState = digitalRead(2);

  char ResetCount = 0;
  ResetCount = ResetVarLesen();
  if((ResetCount < 0)||(ResetCount > 5))  //Prüfen ob Wert Plausibel, wenn nicht rücksetzen
    ResetCount = 0;
    //ResetCount++;
  ResetVarSpeichern(ResetCount);
  delay(5000);
  if (ResetCount < 5) //Wenn nicht 5 mal in den ersten 5 Sekunden der Startvorgang abgebrochen wurde
    EinstLaden();
  ResetVarSpeichern(0);
  //WLAN starten
    if (varConfig.WLAN_AP_Aktiv == 1)
    WiFi_Start_AP();
  else
    WiFi_Start_STA(varConfig.WLAN_SSID, varConfig.WLAN_Password);
  
  //WiFi_Start_STA(ssid_sta, password_sta);

  //Zeitserver Einstellungen
  if (strlen(varConfig.NW_NTPServer))
    timeClient = new NTPClient(ntpUDP, (const char *)varConfig.NW_NTPServer);
  else
    timeClient = new NTPClient(ntpUDP, "fritz.box");
  delay(1000);
  timeClient->begin();
  timeClient->setTimeOffset(varConfig.NW_NTPOffset * 3600);

  //MQTT
  MQTTinit();

  //OTA
  ArduinoOTA.setHostname("TerrasseOTA");
  ArduinoOTA.setPassword("Terrasse!123");
  ArduinoOTA.begin();
  for(int i = 0; i<5; i++)
  {
    //OTA
    ArduinoOTA.handle();
    delay(1000);
  }
  //Webserver
  server.onNotFound(notFound);
  server.begin();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              char *Header_neu = new char[(strlen(html_header) + 50)];
              char *Body_neu = new char[(strlen(html_NWconfig)+250)];
              char *HTMLString = new char[(strlen(html_header) + 50)+(strlen(html_NWconfig)+250)];
              //Vorbereitung Datum
              unsigned long epochTime = timeClient->getEpochTime();
              struct tm *ptm = gmtime((time_t *)&epochTime);
              int monthDay = ptm->tm_mday;
              int currentMonth = ptm->tm_mon; // + 1;
              int currentYear = ptm->tm_year; // + 1900;
              char *pntSelected[5];

              for (int i = 0; i < 5; i++)
                if (i == (varConfig.NW_NTPOffset + 2))
                  pntSelected[i] = (char *)varSelected[1].c_str();
                else
                  pntSelected[i] = (char *)varSelected[0].c_str();
              sprintf(Header_neu, html_header, timeClient->getFormattedTime().c_str(), WeekDays[timeClient->getDay()].c_str(), monthDay, currentMonth, currentYear);
              sprintf(Body_neu, html_NWconfig, Un_Checked[(short)varConfig.WLAN_AP_Aktiv].c_str(), varConfig.WLAN_SSID, Un_Checked[(short)varConfig.NW_StatischeIP].c_str(), varConfig.NW_IPAdresse, varConfig.NW_NetzName, varConfig.NW_SubMask, varConfig.NW_Gateway, varConfig.NW_DNS, varConfig.NW_NTPServer, pntSelected[0], pntSelected[1], pntSelected[2], pntSelected[3], pntSelected[4], varConfig.MQTT_Server, varConfig.MQTT_Port, varConfig.MQTT_Username, varConfig.MQTT_rootpath);
              sprintf(HTMLString, "%s%s", Header_neu, Body_neu);
              request->send_P(200, "text/html", HTMLString);
              delete[] HTMLString;
              delete[] Body_neu;
              delete[] Header_neu;
            });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              int parameter = request->params();
              unsigned short int *submitBereich;
              if (parameter)
              {
                submitBereich = (unsigned short int *)request->getParam(0)->name().c_str();
                switch (*submitBereich)
                {
                case subwl:
                  varConfig.WLAN_AP_Aktiv = 0;
                  for (int i = 0; i < parameter; i++)
                  {
                    if (request->getParam(i)->name() == "wlAP")
                      varConfig.WLAN_AP_Aktiv = 1;
                    else if (request->getParam(i)->name() == "wlSSID")
                    {
                      if (request->getParam(i)->value().length() < 40)
                        strcpy(varConfig.WLAN_SSID, request->getParam(i)->value().c_str());
                      else
                      {
                        request->send_P(200, "text/html", "SSID zu lang<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                        return;
                      }
                    }
                    else if (request->getParam(i)->name() == "wlPassword")
                    {
                      if (request->getParam(i)->value().length() <= 60)
                        strcpy(varConfig.WLAN_Password, request->getParam(i)->value().c_str());
                      else
                      {
                        request->send_P(200, "text/html", "Passwort zu lang<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                        return;
                      }
                    }
                    else
                    {
                      request->send_P(200, "text/html", "Unbekannter Rueckgabewert<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                      return;
                    }
                  }
                  request->send_P(200, "text/html", "Daten wurden uebernommen und ESP wird neu gestartet!<br><a href=\\Settings\\>Zurueck</a> <meta http-equiv=\"refresh\" content=\"15; URL=\\\">"); //<a href=\>Startseite</a>
                  EinstSpeichern();
                  ESP.restart();
                  break;
                case subnw:
                {
                  char tmp_StatischeIP = 0;
                  String tmp_IPAdressen[4];
                  String tmp_NTPServer;
                  String tmp_NetzName;
                  int tmp_NTPOffset;
                  for (int i = 0; i < parameter; i++)
                  {
                    if (request->getParam(i)->name() == "nwSIP")
                      tmp_StatischeIP = 1;
                    else if (request->getParam(i)->name() == "nwIP")
                      tmp_IPAdressen[0] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwNetzName")
                      tmp_NetzName = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwSubnet")
                      tmp_IPAdressen[1] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwGateway")
                      tmp_IPAdressen[2] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwDNS")
                      tmp_IPAdressen[3] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwNTPServer")
                      tmp_NTPServer = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "nwNTPOffset")
                      sscanf(request->getParam(i)->value().c_str(), "%d", &tmp_NTPOffset);
                    else
                    {
                      request->send_P(200, "text/html", "Unbekannter Rueckgabewert<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                      return;
                    }
                  }
                  if (tmp_StatischeIP)
                    if ((tmp_IPAdressen[0].length() == 0) || (tmp_IPAdressen[1].length() == 0))
                    {
                      request->send_P(200, "text/html", "Bei Statischer IP-Adresse wird eine IP-Adresse und eine Subnet-Mask benoetigt!<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                      return;
                    }
                  varConfig.NW_StatischeIP = tmp_StatischeIP;
                  strcpy(varConfig.NW_IPAdresse, tmp_IPAdressen[0].c_str());
                  strcpy(varConfig.NW_NetzName, tmp_NetzName.c_str());
                  strcpy(varConfig.NW_SubMask, tmp_IPAdressen[1].c_str());
                  strcpy(varConfig.NW_Gateway, tmp_IPAdressen[2].c_str());
                  strcpy(varConfig.NW_DNS, tmp_IPAdressen[3].c_str());
                  strcpy(varConfig.NW_NTPServer, tmp_NTPServer.c_str());
                  varConfig.NW_NTPOffset = tmp_NTPOffset;
                  request->send_P(200, "text/html", "Daten wurden uebernommen und ESP wird neu gestartet!<br><meta http-equiv=\"refresh\" content=\"10; URL=\\\">"); //<a href=\>Startseite</a>
                }
                  EinstSpeichern();
                  ESP.restart();
                  break;
                case submq:
                {
                  String Temp[6];
                  for (int i = 0; i < parameter; i++)
                  {
                    if (request->getParam(i)->name() == "mqServer")
                      Temp[0] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "mqPort")
                      Temp[1] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "mqUser")
                      Temp[2] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "mqPassword")
                      Temp[3] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "mqRootpath")
                      Temp[4] = request->getParam(i)->value();
                    else if (request->getParam(i)->name() == "mqFPrint")
                      Temp[5] = request->getParam(i)->value();
                    else
                    {
                      request->send_P(200, "text/html", "Unbekannter Rueckgabewert<form> <input type=\"button\" value=\"Go back!\" onclick=\"history.back()\"></form>");
                      return;
                    }
                  }
                  if((Temp[0].length()<49)&&(Temp[0].length()>5))
                    strcpy(varConfig.MQTT_Server, Temp[0].c_str());
                  if((Temp[1].length()<6)&&(Temp[1].length()>1))
                    varConfig.MQTT_Port = Temp[1].toInt();
                  if((Temp[2].length()<19)&&(Temp[2].length()>5))
                    strcpy(varConfig.MQTT_Username, Temp[2].c_str());
                  if((Temp[3].length()<=60)&&(Temp[3].length()>5)&&(Temp[3]!= "xxxxxx"))
                    strcpy(varConfig.MQTT_Password, Temp[3].c_str());
                  if((Temp[4].length()<95)&&(Temp[4].length()>5))
                    strcpy(varConfig.MQTT_rootpath, Temp[4].c_str());
                  if((Temp[5].length()<=65)&&(Temp[5].length()>5)&&(Temp[5]!= "xxxxxx"))
                    strcpy(varConfig.MQTT_fprint, Temp[5].c_str());
                }
                  EinstSpeichern();
                  if(MQTTinit())
                    request->send_P(200, "text/html", "Daten wurden uebernommen, Verbindung zu MQTT-Server hergestellt!<br><meta http-equiv=\"refresh\" content=\"10; URL=\\\">"); //<a href=\>Startseite</a>
                  else
                    request->send_P(200, "text/html", "Daten wurden uebernommen, Verbindung zu MQTT-Server konnte nicht hergestellt werden!<br><meta http-equiv=\"refresh\" content=\"10; URL=\\\">"); //<a href=\>Startseite</a>

                  break;
                
                default:
                  char strFailure[50];
                  sprintf(strFailure, "Anweisung unbekannt, Empfangen: %u", *submitBereich);
                  request->send_P(200, "text/html", strFailure);
                  break;
                }
              }
            });
  MQTT_sendInputState();
}
uint16 Test = 0;
void loop() {
  //OTA
  ArduinoOTA.handle();
  //MQTT wichtige Funktion
  if(WiFi.status()== WL_CONNECTED)  //MQTTclient.loop() wichtig damit die Daten im Hintergrund empfangen werden
  {
    if(!MQTTclient.loop())
    {
      MQTTclient.disconnect();
      MQTTinit();
    }
  }
  int Temp = digitalRead(2);
  if(Temp == LOW)
  {
    if(InputState == HIGH)
    {
      InputState = LOW;
      MQTT_sendInputState();
    }
    Ausschaltverz = millis() + 100;
  }
  else
  {
    if((InputState == LOW)&&(millis() > Ausschaltverz))
    {
      InputState = HIGH;
      MQTT_sendInputState();
    }
  }
  //Anweisungen werden alle 3600 Sekunden (1h) ausgefuehrt
  if (Break_h < millis())
  {
    Break_h = millis() + 3600000;
    timeClient->update();
    if(WIFIConnectionCheck(true))
      MQTT_sendOutputState();
  }
  if (Break_s < millis())
  {
    Break_s = millis() + 1000;
    if(WIFIConnectionCheck(true))
      MQTT_sendInputState();
  }


}

//---------------------------------------------------------------------
//Wifi Funtkionen
void WiFi_Start_STA(char *ssid_sta, char *password_sta)
{
  unsigned long timeout;
  unsigned int Adresspuffer[4];
  if (varConfig.NW_StatischeIP)
  {
    sscanf(varConfig.NW_IPAdresse, "%d.%d.%d.%d", &Adresspuffer[0], &Adresspuffer[1], &Adresspuffer[2], &Adresspuffer[3]);
    IPAddress IP(Adresspuffer[0], Adresspuffer[1], Adresspuffer[2], Adresspuffer[3]);
    sscanf(varConfig.NW_Gateway, "%d.%d.%d.%d", &Adresspuffer[0], &Adresspuffer[1], &Adresspuffer[2], &Adresspuffer[3]);
    IPAddress IPGate(Adresspuffer[0], Adresspuffer[1], Adresspuffer[2], Adresspuffer[3]);
    sscanf(varConfig.NW_SubMask, "%d.%d.%d.%d", &Adresspuffer[0], &Adresspuffer[1], &Adresspuffer[2], &Adresspuffer[3]);
    IPAddress IPSub(Adresspuffer[0], Adresspuffer[1], Adresspuffer[2], Adresspuffer[3]);
    sscanf(varConfig.NW_Gateway, "%d.%d.%d.%d", &Adresspuffer[0], &Adresspuffer[1], &Adresspuffer[2], &Adresspuffer[3]);
    IPAddress IPDNS(Adresspuffer[0], Adresspuffer[1], Adresspuffer[2], Adresspuffer[3]);
    WiFi.config(IP, IPDNS, IPGate, IPSub);
  }
  WiFi.mode(WIFI_STA); // Client
  WiFi.hostname(varConfig.NW_NetzName);
  WiFi.begin(ssid_sta, password_sta);
  timeout = millis() + 12000L;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout)
  {
    delay(10);
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    //    server.begin();
    //    my_WiFi_Mode = WIFI_STA;
#ifdef BGTDEBUG
    Serial.print("Connected IP - Address : ");
    for (int i = 0; i < 3; i++)
    {
      Serial.print(WiFi.localIP()[i]);
      Serial.print(".");
    }
    Serial.println(WiFi.localIP()[3]);
#endif
  }
  else
  {
    WiFi.mode(WIFI_OFF);
#ifdef BGTDEBUG
    Serial.println("WLAN-Connection failed");
#endif
  }
}
void WiFi_Start_AP()
{
  WiFi.mode(WIFI_AP); // Accesspoint
                      //  WiFi.hostname(varConfig.NW_NetzName);

  WiFi.softAP(ssid_AP, password_AP);
  server.begin();
  //IPAddress myIP = WiFi.softAPIP();
  //  my_WiFi_Mode = WIFI_AP;
  WiFi.softAPIP();

#ifdef BGTDEBUG
  Serial.print("Accesspoint started - Name : ");
  Serial.print(ssid_AP);
  Serial.print(" IP address: ");
  Serial.println(myIP);
#endif
}
bool WIFIConnectionCheck(bool with_reconnect = true)
{
  if(WiFi.status()!= WL_CONNECTED)
  {
    if(with_reconnect)
    {
      WiFi.reconnect();
    }
    return false;
  }
  return true;
}
//---------------------------------------------------------------------
void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}
//---------------------------------------------------------------------
//Einstellungen laden und Speichern im EEPROM bzw. Flash
void EinstSpeichern()
{
  unsigned long int Checksumme = 0;
  unsigned char *pointer;
  pointer = (unsigned char *)&varConfig;
  for (unsigned int i = 0; i < sizeof(varConfig); i++)
    Checksumme += pointer[i];

  //EEPROM initialisieren
  EEPROM.begin(sizeof(varConfig) + 14);

  EEPROM.put(0, varConfig);
  EEPROM.put(sizeof(varConfig) + 1, Checksumme);

  EEPROM.commit(); // Only needed for ESP8266 to get data written
  EEPROM.end();    // Free RAM copy of structure
}
void EinstLaden()
{
  NWConfig varConfigTest;
  unsigned long int Checksumme = 0;
  unsigned long int ChecksummeEEPROM = 0;
  unsigned char *pointer;
  pointer = (unsigned char *)&varConfigTest;
  //EEPROM initialisieren
  unsigned int EEPROMSize;
  EEPROMSize = sizeof(varConfig) + 14;
  EEPROM.begin(EEPROMSize);

  EEPROM.get(0, varConfigTest);
  EEPROM.get(sizeof(varConfigTest) + 1, ChecksummeEEPROM);

  for (unsigned int i = 0; i < sizeof(varConfigTest); i++)
    Checksumme += pointer[i];
  if ((Checksumme == ChecksummeEEPROM) && (Checksumme != 0))
  {
    EEPROM.get(0, varConfig);
  }
  else
    Serial.println("Fehler beim Dateneinlesen");

  delay(200);
  EEPROM.end(); // Free RAM copy of structure
}
//---------------------------------------------------------------------
//Resetvariable die hochzaehlt bei vorzeitigem Stromverlust um auf Standard-Wert wieder zurueckzustellen.
void ResetVarSpeichern(char Count)
{
  EEPROM.begin(sizeof(varConfig) + 14);

  EEPROM.put(sizeof(varConfig) + 10, Count);

  EEPROM.commit(); // Only needed for ESP8266 to get data written
  EEPROM.end();    // Free RAM copy of structure
}
char ResetVarLesen()
{
  unsigned int EEPROMSize;
  char temp = 0;
  EEPROMSize = sizeof(varConfig) + 14;
  EEPROM.begin(EEPROMSize);
  EEPROM.get(EEPROMSize - 4, temp);
  delay(200);
  EEPROM.end(); // Free RAM copy of structure
  return temp;
}
//---------------------------------------------------------------------
//MQTT-Funktionen
void MQTT_callback(char* topic, byte* payload, unsigned int length)
{
  int lengthTopic = strlen(varConfig.MQTT_rootpath);
  char SubscribeVar[lengthTopic+20];
  char payloadTemp[length + 2];
  for (unsigned int i = 0; i < length; i++){
    payloadTemp[i] = (char) payload[i];
  }
  payloadTemp[length] = 0;
  //Ausgänge setzen
  sprintf(SubscribeVar, "%s/Output", varConfig.MQTT_rootpath);
  if(!strcmp(topic, SubscribeVar))
  {
    unsigned int var = 0;
    if(sscanf(payloadTemp, "%u", &var)==1)
    {
      switchRelais(var);
      return;
    }
  }
}
bool MQTTinit()
{
  if(MQTTclient.connected())
    MQTTclient.disconnect();
  IPAddress IPTemp;
  if(IPTemp.fromString(varConfig.MQTT_Server))
  {
    MQTTclient.setServer(IPTemp, varConfig.MQTT_Port);
  }
  else{
    MQTTclient.setServer(varConfig.MQTT_Server, varConfig.MQTT_Port);
  }
  MQTTclient.setCallback(MQTT_callback);
  unsigned long int StartTime = millis();
  while ((millis() < (StartTime + 5000)&&(!MQTTclient.connect(WiFi.macAddress().c_str(), varConfig.MQTT_Username, varConfig.MQTT_Password)))){
    delay(200);
  }
  if(MQTTclient.connected()){
    int laenge = strlen(varConfig.MQTT_rootpath);
    char SubscribeVar[laenge+15];
    sprintf(SubscribeVar, "%s/Output", varConfig.MQTT_rootpath);
    MQTTclient.subscribe(SubscribeVar);
    return true;
  }
  else
    return false;
}
bool MQTT_sendOutputState()
{
  int lenPath = strlen(varConfig.MQTT_rootpath);
  char strPathVar[lenPath+20];
  if(stateRelais > 1)
    return false;

  sprintf(strPathVar, "%s/OutputState", varConfig.MQTT_rootpath);
  return MQTTclient.publish(strPathVar, RelaisText[stateRelais].c_str(), true);
}
bool MQTT_sendInputState()
{
  int lenPath = strlen(varConfig.MQTT_rootpath);
  char strPathVar[lenPath+20];
  sprintf(strPathVar, "%s/InputState", varConfig.MQTT_rootpath);
  return MQTTclient.publish(strPathVar, Ein_Aus[InputState].c_str(), true);
}

String IntToStr(int _var)
{
    char Temp[20];
    sprintf(Temp,"%d",_var);
    return Temp;
}
String IntToStr(char _var)
{
    char Temp[20];
    sprintf(Temp,"%d",_var);
    return Temp;
}
String IntToStr(long int _var)
{
    char Temp[20];
    sprintf(Temp,"%ld",_var);
    return Temp;
}
String IntToStr(uint32_t _var)
{
    char Temp[20];
    sprintf(Temp,"%u",_var);
    return Temp;
}
String IntToStr(float _var)
{
    char Temp[20];
    sprintf(Temp,"%f",_var);
    return Temp;
}
String IntToStrHex(int _var)
{
    char Temp[20];
    sprintf(Temp,"%x",_var);
    return Temp;
}

int switchRelais(int _var = 3)
{
  switch(_var)
  {
    case 1:
      digitalWrite(0, LOW);
      stateRelais = 1;
      MQTT_sendOutputState();
      return 1;
    case 2:
      if(stateRelais)
        return switchRelais(0);
      else
        return switchRelais(1);
    case 0:
    default:
      digitalWrite(0, HIGH);
      stateRelais = 0;
      MQTT_sendOutputState();
      return 0;
      break;
  }
}
