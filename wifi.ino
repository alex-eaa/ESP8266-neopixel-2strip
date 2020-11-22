///////////////////////////////////////////////////////////
//////  WI-FI  WI-FI  WI-FI  //////////////////////////////
///////////////////////////////////////////////////////////

#define BLINK_TIME_LED_WIFI_1 250             //частота моргания LED_WIFI при поиске точки доступа, мс
#define BLINK_TIME_LED_WIFI_2 1000            //частота моргания LED_WIFI при работе в режиме точки доступа, мс

//Сохраняемые переменные (настройки сети)
bool wifiAP_mode = 0;                   //флаг работы WIFI модуля в режиме точки доступа, (1-работает в режиме AP)
bool static_IP = 0;                     //флаг работы со стачискими настройками сети
byte ip[4] = {192, 168, 1, 43};
byte sbnt[4] = {255, 255, 255, 0};
byte gtw[4] = {192, 168, 1, 1};
char *p_ssid = new char[0];
char *p_password = new char[0];
char *p_ssidAP = new char[0];
char *p_passwordAP = new char[0];

unsigned int wifiConnectTimer = millis();     //перемнная для таймера включения WIFI
unsigned int ledBlinkTimer = millis();        //перемнная для мигания LED WIFI
bool wlConnectedMsgSend = 0;                  //флаг - подключения к WIFI, запуска MDNS и включения LED_WIFI
bool wifiAP_runned = 0;                       //флаг - запущен режим точки доступа АР


void wifi_update()
{
  //При подключении к точке доступа WIFI, запуск MDNS и включение LED_WIFI
  if (WiFi.status() == WL_CONNECTED && wlConnectedMsgSend == 0) 
  {
#ifdef DEBUG
    Serial.println(F("\nCONNECTED to WiFi AP!"));
    Serial.print(F("My IP address: "));   Serial.println(WiFi.localIP());
    Serial.print(F("Subnet mask: "));     Serial.println(WiFi.subnetMask());
    Serial.print(F("Subnet gateway: "));  Serial.println(WiFi.gatewayIP());
    Serial.print(F("My macAddress: "));   Serial.println(WiFi.macAddress());
    Serial.print(F("My default hostname: "));   Serial.println(WiFi.hostname());
    Serial.print(F("Connected to AP with SSID: "));       Serial.println(WiFi.SSID());
    Serial.print(F("Connected to AP with password: "));   Serial.println(WiFi.psk());
#endif
    startMDNS();
    digitalWrite(GPIO_LED_WIFI, 0);
    wlConnectedMsgSend = 1;
  }

  if (wifiAP_mode == 0 && wifiAP_runned == 0)
  {
    //Мигание LED WIFI при поиске точки доступа
    if (WiFi.status() != WL_CONNECTED && millis() - ledBlinkTimer > BLINK_TIME_LED_WIFI_1) 
    {
      digitalWrite(GPIO_LED_WIFI, !digitalRead(GPIO_LED_WIFI));
      ledBlinkTimer = millis();
      wlConnectedMsgSend = 0;
    }
  } 
  else if (wifiAP_runned == 1) 
  {
    if (wifiAP_runned == 1)   //Мигание LED WIFI при работе точки доступа
    {
      if (millis() - ledBlinkTimer > BLINK_TIME_LED_WIFI_2) 
      {
        digitalWrite(GPIO_LED_WIFI, !digitalRead(GPIO_LED_WIFI));
        ledBlinkTimer = millis();
      }
    }
  }
}


//Функция запуска модуля в режиме AP
void startAp(char *ap_ssid, const char *ap_password)
{
#ifdef DEBUG
  Serial.println(F("\n\nSTART ESP in AP WIFI mode"));
#endif
  if (WiFi.getPersistent() == true)    WiFi.persistent(false);   //disable saving wifi config into SDK flash area
  WiFi.disconnect();
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.persistent(true);                                         //enable saving wifi config into SDK flash area
  wifiAP_runned = 1;
  startMDNS();
#ifdef DEBUG
  Serial.print(F("SSID AP: "));               Serial.println(ap_ssid);
  Serial.print(F("Password AP: "));           Serial.println(ap_password);
  Serial.print(F("Start AP with SSID: "));    Serial.println(WiFi.softAPSSID());
  Serial.print(F("Soft-AP IP: "));            Serial.println(WiFi.softAPIP());
  Serial.print(F("Soft-AP MAC: "));           Serial.println(WiFi.softAPmacAddress());
#endif
}


//Настройка статических параметров сети. Не сохраняется во flash память.
void set_staticIP()
{
  IPAddress ipAdr(ip[0], ip[1], ip[2], ip[3]);
  IPAddress gateway(gtw[0], gtw[1], gtw[2], gtw[3]);
  IPAddress subnet(sbnt[0], sbnt[1], sbnt[2], sbnt[3]);
  WiFi.config(ipAdr, gateway, gateway, subnet);          //второй параметр установка DNS
#ifdef DEBUG
  Serial.println(F("Set static ip, sbnt, gtw."));
#endif
}



void startMDNS() {
  String mdnsNameStr = DEVICE_TYPE + String(ESP.getChipId(), HEX);
  //Serial.print(F("Host name for mDNS: "));        Serial.println(mdnsNameStr);
  char mdnsName[mdnsNameStr.length()];
  mdnsNameStr.toCharArray(mdnsName, mdnsNameStr.length() + 1);
#ifdef DEBUG
  Serial.print(F("Host name for mDNS: "));        Serial.println(mdnsName);
#endif
  if (!MDNS.begin(mdnsName)) {
#ifdef DEBUG
    Serial.println(F("Error setting up MDNS responder!"));
#endif
  } else {
#ifdef DEBUG
    Serial.println(F("mDNS responder started"));
#endif
    MDNS.addService("http", "tcp", 80);
    //MDNS.addService("ws", "tcp", 81);
  }
}
