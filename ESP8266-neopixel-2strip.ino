/*
      Контроллер светильника из двух лент с адресуемыми светодиодами типа WS2812.
      Управление осуществляется жестами, приближением руки к датчику расстояния APDS9930.

      Функционал контроллера:
   - Включение/отключение света проведением руки возле датчика расстояния. Включение и отключение сопровождается
     световым эффектом;
   - Плавная регулировка яркости света подведением руки под датчик расстояния и удержанием ее возле датчика
     на время 1 сек., при включенном освещении.
   - Изменение температуры освещения на три предустановленные значения. Изменение происходит если отключить
     освещение и сразуже его включить (втечении 500 мс);
   - Вебинтерфейс управления светильником позволяет: включать/отключать, плавно регулировать яркость, выбирать из предустановленных
     или плавно регулировать температуру света, изменять значения трех предустановленных уровней температуры света, выбирать
     световые эффекты для включения/отключения света;

   - Работа в режиме WIFI-точки доступа;
   - Работа в режиме WIFI-клиента для подключения к точке доступа;
   - автозапуск в режиме WIFI-точки доступа "По умолчанию" с настройками по умолчанию (для первичной настройки устройства);
   - mDNS-сервис на устройстве для автоматического определения IP-адреса устройства в сети;
   - Web сервер (порт: 80) на устройстве для подключения к устройству по сети из любого web-браузера;
   - Websocket сервер (порт: 81) на устройстве для обмена коммандами и данными между страницей в web-браузере и контроллером;
   - Страница сетевых настроек (/setup.htm);
   - Страница файлового менеджера для просмотра, удаления и загрузки файлов (/edit.htm);
   - Страница обновления прошивки контроллера (/update.htm);

      Подключение датчика растояния (жестов):
   - SCL – GPIO5 (D1)
   - SDA – GPIO4 (D2)
   - VCC – 3.3v
   - GND – Gnd

      Подключение пинов управления светодиодных лент:
   - GPIO13 (D7)
   - GPIO14 (D5)

      Подключение других элементов (для платы типа nodeMCU ESP-12e):
   - GPIO2  (D4) - встроенный голубой wifi светодиод;
   - GPIO12 (D6) - кнопка запуска с настройками сети по умолчанию;
*/

#define DEBUG

#include <NeoPixelBus.h>
#include <Wire.h>
#include <iarduino_APDS9930.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WebSocketsServer.h>    //https://github.com/Links2004/arduinoWebSockets
#include <ArduinoJson.h>         /*https://github.com/bblanchon/ArduinoJson 
                                   https://arduinojson.org/?utm_source=meta&utm_medium=library.properties */

#define SCL 5               //SCL – GPIO5 (D1)
#define SDA 4               //SDA – GPIO4 (D2)
#define GPIO_LED_WIFI 2     // номер пина светодиода GPIO2 (D4)
#define GPIO_BUTTON 12      // номер пина кнопки GPIO12 (D6) 
#define GPIO_WS2812B_1 14   // номер пина управляющий диодами WS2812B лента 1 (D5)
#define GPIO_WS2812B_2 13   // номер пина управляющий диодами WS2812B лента 2 (D7)

#define FILE_CONF    "/conf.txt"      //Имя файла для сохранения настроек
#define FILE_NETWORK "/net.txt"       //Имя файла для сохранения настроек сети

#define DEVICE_TYPE "esplink_pixel_"
#define TIMEOUT_T_broadcastTXT 100000   //таймаут отправки скоростных сообщений T_broadcastTXT, мкс

#define DEFAULT_AP_NAME "ESP"           //имя точки доступа "По умолчанию"
#define DEFAULT_AP_PASS "12345678"      //пароль для точки доступа "По умолчанию"
#define ON 1
#define OFF 0

#define PIXEL_COUNT 74                  //количество пикселей в лентах

//Сохраняемые переменные (настройки сети)
extern bool wifiAP_mode;                   //флаг работы WIFI модуля в режиме точки доступа, (1-работает в режиме AP)
extern bool static_IP;                     //флаг работы со стачискими настройками сети
extern byte ip[4];
extern byte sbnt[4];
extern byte gtw[4];
extern char *p_ssid;
extern char *p_password;
extern char *p_ssidAP;
extern char *p_passwordAP;

bool sendSpeedDataEnable[] = {0, 0, 0, 0, 0};
String ping = "ping";
unsigned int speedT = 150;   //минимальный период отправки данных, миллисек
unsigned int timeT1 = 0;     //вспом. переменная для speedT
bool flagDataUpdate = 0;     //флаг обновленых данных (если 1 значит нужно отправить данные WS клиенту)

//Сохраняемые переменные (настройки параметров светильника)
float ledBridhtness = 0.3f;               //яркость led
float minBridhtness = 0.1f;               //минимальная яркость (0-255)
float maxBridhtness = 0.9f;               //максимальная яркость (0-255)
int varForArrConstLedTemp = 0;            //предустановленный цвет, индекс элемента массива arrConstLedTemp
int nAnimeOn = 0;                         //номер анимации включения
int nAnimeOff = 0;                        //номер анимации отключения
int arrConstLedTemp[3][3] = { {000,255,255},
                              {255,000,204},
                              {200,204,000} };     //предустановленные значения температуры света


int proximity = 0;                        //расстояние от датчика до объекта (0-1024)
bool flagLedState = 0;                    //флаг состояния освещения светильника, вкл./откл.
bool flagToOnOff = 0;                     //флаг перехода в режим изменения состояния освещения на противоположное
bool flagToBrightnessChange = 0;          //флаг перехода в режим настройки яркости
bool flagToTempChange = 0;                //флаг перехода в режим настройки температуры света
bool flagDirectionBrightnessChange = 1;   //направление изменения яркости (1-увеличение / 0-уменьшение) по кругу
bool flagNeedSaveConf = 0;                //флаг необходимости сохранения настроек света

unsigned int timeToOnOff = 60;            //Время задержки для жеста включения-отключения света, мс
unsigned int timeToBrightness = 1000;     //Время задержки для жеста перехода в режим регулировки яркости, мс
unsigned int timeToTemp = 2000;           //Время до повторного включения, для изменения температуры света, мс
unsigned int timeSaveConf = 5000;         //Время до сохранения настроек после изменения яркости или температуры света

bool prevFlagLedState;                    //Вспомогательная переменная
float prevLedBridhtness;                  //Вспомогательная переменная
int prevVarForArrConstLedTemp;            //Вспомогательная переменная
int prevArrConstLedTemp[3];               //Вспомогательная переменная
unsigned int prevTime = 0;                //Вспомогательная переменная для времени жестов
unsigned int prevTimeToTemp = 0;          //Вспомогательная переменная для времени жеста изменеия температуры света
unsigned int prevTimeSaveConf = 0;        //Вспомогательная переменная для времени сохранения настроек

unsigned int timeDebug = 1000;             //Вспомогательная переменная для отладки
unsigned int prevTimeDebug = 0;           //Вспомогательная переменная для отладки
unsigned int timeDebug2 = 500;          //Вспомогательная переменная для отладки
unsigned int prevTimeDebug2 = 0;          //Вспомогательная переменная для отладки

//Создаем необходимые объекты
WebSocketsServer webSocket(81);
ESP8266WebServer server(80);
iarduino_APDS9930 apds;         /*Определяем объект apds для работы с датчиком APDS-9930.
                                Если у датчика не стандартный адрес, то его нужно указать: iarduino_APDS9930 apds(0xFF);  */
NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip1(PIXEL_COUNT, GPIO_WS2812B_1);
NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip2(PIXEL_COUNT, GPIO_WS2812B_2);

RgbColor white(200, 200, 200);
RgbColor black(0);

void setup() {
  
  Serial.begin(115200);
  Serial.println();
  pinMode(GPIO_BUTTON, INPUT_PULLUP);
  pinMode(GPIO_LED_WIFI, OUTPUT);
  digitalWrite(GPIO_LED_WIFI, HIGH);
  strip1.Begin();
  strip1.Show();
  strip2.Begin();
  strip2.Show();

  SPIFFS.begin();

#ifdef DEBUG
  printChipInfo();
  scanAllFile();
  printFile(FILE_NETWORK);
  printFile(FILE_CONF);
#endif

  bool loadFileFILE_CONF = loadFile(FILE_CONF);


  //Запуск точки доступа с параметрами поумолчанию если файл настроек сети отсутствует или зажата кнопка
  if ( !loadFile(FILE_NETWORK) ||  digitalRead(GPIO_BUTTON) == 0)   startAp(DEFAULT_AP_NAME, DEFAULT_AP_PASS);
  //Запуск точки доступа
  else if (digitalRead(GPIO_BUTTON) == 1 && wifiAP_mode == 1)       startAp(p_ssidAP, p_passwordAP);
  //Запуск подключения клиента к точке доступа
  else if (digitalRead(GPIO_BUTTON) == 1 && wifiAP_mode == 0) {
    if (WiFi.getPersistent() == true)    WiFi.persistent(false);
    WiFi.softAPdisconnect(true);
    WiFi.persistent(true);
    //Если пароль или имя точки доступа изменился, подключаемся с новыми
    if (WiFi.SSID() != p_ssid || WiFi.psk() != p_password) {
      Serial.println(F("\nCHANGE password or ssid"));
      WiFi.disconnect();
      WiFi.begin(p_ssid, p_password);
    }
    if (static_IP == 1) {
      set_staticIP();
    }
  }


  //инициализация датчика расстояния
  bool apdsBegin = apds.begin();
#ifdef DEBUG
  if (apdsBegin)  Serial.println(F("Initialization OK!"));
  else   Serial.println(F("Initialization ERROR!"));
#endif

  webServer_init();      //инициализация HTTP сервера
  webSocket_init();      //инициализация webSocket сервера
  
  white = RgbColor(arrConstLedTemp[varForArrConstLedTemp][0],
                  arrConstLedTemp[varForArrConstLedTemp][1],
                  arrConstLedTemp[varForArrConstLedTemp][2]);
}



void loop() {
  wifi_update();            //12-36 мкс
  webSocket.loop();         //40-60 мкс
  server.handleClient();    //4-12  мкс
  MDNS.update();            //25-45 мкс


  //Изменение значения переменной яркости освещения, если флаг яркости установлен и рука возле датчика
  if (flagToBrightnessChange == 1) { 
    if (flagDirectionBrightnessChange == 1 && ledBridhtness < maxBridhtness)
    {
      ledBridhtness = ledBridhtness + 0.01;
      flagDataUpdate = 1; 
    }
    else if (flagDirectionBrightnessChange == 0 && ledBridhtness > minBridhtness){
      ledBridhtness = ledBridhtness - 0.01;
      flagDataUpdate = 1; 
    }
  }

  
  //ПРИМЕНЕНИЕ изменений состояния флагов и параметров светильника
  //Включение ленты на цвет-white и яркость-ledBridhtness
  if (flagLedState == 1 && prevFlagLedState != flagLedState){
    onStrip(RgbColor::LinearBlend(black, white, ledBridhtness), nAnimeOn);
    flagDataUpdate = 1;
    prevFlagLedState = flagLedState;
  }
  //Отключение ленты
  else if (flagLedState == 0 && prevFlagLedState != flagLedState){
    onStrip(black, nAnimeOff);
    flagDataUpdate = 1;
    prevFlagLedState = flagLedState;
  }
  
  //Изменение цвет white на другой предустановленный
  if (prevVarForArrConstLedTemp != varForArrConstLedTemp){
    RgbColor color (arrConstLedTemp[varForArrConstLedTemp][0],
                 arrConstLedTemp[varForArrConstLedTemp][1],
                 arrConstLedTemp[varForArrConstLedTemp][2]); 

    white = RgbColor::LinearBlend(white, color, 1);
    if (flagLedState == 1) updateStrip(RgbColor::LinearBlend(black, white, ledBridhtness));
    prevVarForArrConstLedTemp = varForArrConstLedTemp; 
  }
  
  //Изменение текущего предустановленного цвета при его изменении
  if (prevArrConstLedTemp[0] != arrConstLedTemp[varForArrConstLedTemp][0] ||
      prevArrConstLedTemp[1] != arrConstLedTemp[varForArrConstLedTemp][1] ||
      prevArrConstLedTemp[2] != arrConstLedTemp[varForArrConstLedTemp][2]) {

    RgbColor color (arrConstLedTemp[varForArrConstLedTemp][0],
                 arrConstLedTemp[varForArrConstLedTemp][1],
                 arrConstLedTemp[varForArrConstLedTemp][2]); 

    white = RgbColor::LinearBlend(white, color, 1);
    if (flagLedState == 1) updateStrip(RgbColor::LinearBlend(black, white, ledBridhtness));
               
    prevArrConstLedTemp[0] = arrConstLedTemp[varForArrConstLedTemp][0];
    prevArrConstLedTemp[1] = arrConstLedTemp[varForArrConstLedTemp][1];
    prevArrConstLedTemp[2] = arrConstLedTemp[varForArrConstLedTemp][2];
  }

  //Изменение яркости освещения, если задана новая величина яркости ledBridhtness
  if (prevLedBridhtness != ledBridhtness){
    if (flagLedState == 1)  updateStrip(RgbColor::LinearBlend(black, white, ledBridhtness));
    prevLedBridhtness = ledBridhtness;
  }    
  


  //АНАЛИЗ КОМАНД на датчике расстояния
  proximity = apds.getProximity();   //считать показания расстояния из датчика
  
  //Если к датчику поднесли руку
  if (proximity > 0)
  {
    //Установка флага изменения состояния освещения на противоположное. Если рука находится возле датчика более 100 мс и флаги сброшены.
    if (millis() - prevTime > timeToOnOff && flagToOnOff == 0 && flagToBrightnessChange == 0) {
      flagToOnOff = 1;
    }
    //Установка флага изменения яркости освещения. Если рука находится возле датчика более 1000 мс, свет включен и флаг изменения яркости сброшены.
    if (millis() - prevTime > timeToBrightness  && flagLedState == 1 && flagToBrightnessChange == 0) {
      flagToOnOff = 0;
      flagToBrightnessChange = 1;
    }
    //Изменение предустановленных значений температуры света при быстром повторном включении света и взведеном флаге разрешения
    if (flagToTempChange == 1 && millis() - prevTimeToTemp < timeToTemp) {
      varForArrConstLedTemp ++;
      if (varForArrConstLedTemp > 2)   varForArrConstLedTemp = 0;
      flagToTempChange = 0;
      flagNeedSaveConf = 1;
      flagDataUpdate = 1;
      prevTimeSaveConf = millis();
    }
  }
  //Если руку отвели от датчика
  else{
    //Вкл.-откл. света при отводе руки от датчика, при установленном флаге изменения света,
    //при отключении также взводим флаг разрешения изменения температуры света
    if (flagToOnOff == 1 && flagToBrightnessChange == 0){
      flagLedState = !flagLedState;
      //flagDataUpdate = 1;
      //Dзводим флаг разрешения изменения температуры света
      if (flagLedState == 0){
        flagToTempChange = 1;
        prevTimeToTemp = millis();
      }
      flagToOnOff = 0;
    }
    //Отключение режима настройки яркости при отводе руки от датчика и изменение направления изменения яркости на противоположное
    if (flagToOnOff == 0 && flagToBrightnessChange == 1){
      flagToBrightnessChange = 0;
      flagDirectionBrightnessChange = !flagDirectionBrightnessChange;
      flagNeedSaveConf = 1;
      prevTimeSaveConf = millis();
      //flagDataUpdate = 1;                          
    }
    
    prevTime = millis();
  }


  //СОХРАНЕНИЕ настроек света в файл
  //Сохранение настроек в файл если установлен флаг и прошло время паузы
  if (flagNeedSaveConf == 1  && flagToBrightnessChange == 0 && millis()-prevTimeSaveConf > timeSaveConf){
    //saveFile(FILE_CONF);
    flagNeedSaveConf = 0;
  }



  //Отправка Speed данных клиентам при условии что данныее обновились и клиенты подключены
  if (flagDataUpdate == 1)
  {
    if (sendSpeedDataEnable[0] || sendSpeedDataEnable[1] || sendSpeedDataEnable[2] || sendSpeedDataEnable[3] || sendSpeedDataEnable[4] )
    {
      if ( millis() - timeT1 > speedT )
      {
        String data = serializationToJson_index();
        int startT_broadcastTXT = micros();
#ifdef DEBUG
        Serial.print("\nwebSocket.broadcastTXT: ");
        Serial.println(data);
#endif
        webSocket.broadcastTXT(data);
        int T_broadcastTXT = micros() - startT_broadcastTXT;
        if (T_broadcastTXT > TIMEOUT_T_broadcastTXT)  checkPing();
        timeT1 = millis();
      }
    }
    flagDataUpdate = 0;
  }



#ifdef DEBUG
  if(millis() - prevTimeDebug > timeDebug)
  { 
  //Serial.print((String) "proximity=" + proximity + ", ");
  Serial.print((String) "ON=" + flagLedState + ", ");
  Serial.println((String) "B=" + ledBridhtness + ", ");
  //Serial.print((String) "T=" + varForArrConstLedTemp + "\n");
  
  //Serial.print(F("<-> FREE MEMORY: "));          Serial.println(ESP.getFreeHeap());
  
  //Serial.print((String) "CalculateBrightness=" + white.CalculateBrightness() + "\n");
  prevTimeDebug = millis();
  }
#endif
}



void onStrip(RgbColor color, int nAnime)
{
  switch (nAnime)
  {
  //простое включение (всех светодиодов одновременно)
  case 0:                                 
    for (int n = 0; n < PIXEL_COUNT; n++)
    {
      strip1.SetPixelColor(n, color);
      strip2.SetPixelColor(n, color);
    }
    strip1.Show();    
    strip2.Show();
    break;
    
  //плавное включение (всех светодиодов одновременно)
  case 1:      
    if (color.CalculateBrightness() != 0)
    {
      for (float n = 0.00; n < ledBridhtness; n = n + 0.01)
      {
        updateStrip(RgbColor::LinearBlend(black, white, n));
        delay(30);
      }
      updateStrip(RgbColor::LinearBlend(black, white, ledBridhtness));      
    }
    else
    {
      for (float n = ledBridhtness; n > 0.00; n = n - 0.01)
      {
        updateStrip(RgbColor::LinearBlend(black, white, n));
        delay(30);
      }
      updateStrip(RgbColor::LinearBlend(black, white, 0));       
    }
  break;
    
  //последовательное включение от начала к концу лент (2 ленты одновременно) (начала лент в углу)
  case 2:                                   
    for (int n = 0; n < PIXEL_COUNT; n++)
    {
      strip1.SetPixelColor(n, color);
      strip1.Show();
      strip2.SetPixelColor(n, color);
      strip2.Show();
      delay(15);
    }
  break; 
    
  //последовательное включение от конца к началу лент (2 ленты одновременно) (начала лент в углу)
  case 3:                                   
    for (int n = PIXEL_COUNT-1; n >= 0; n--)
    {
      strip1.SetPixelColor(n, color);
      strip1.Show();
      strip2.SetPixelColor(n, color);
      strip2.Show();
      delay(15);
    }
    break;
    
  //последовательное включение от конца к началу ленты 1 дальше от начала к концу ленты 2 (2 ленты последовательно)
  case 4:                                   
    for (int n = PIXEL_COUNT-1; n >= 0; n--)
    {
      strip1.SetPixelColor(n, color);
      strip1.Show();
      delay(10);
    }  
    for (int n = 0; n < PIXEL_COUNT; n++)
    {
      strip2.SetPixelColor(n, color);
      strip2.Show();
      delay(10);
    }
    break;
    
  //последовательное включение от конца к началу ленты 2 дальше от начала к концу ленты 1 (2 ленты последовательно)
  case 5:                                   
    for (int n = PIXEL_COUNT-1; n >= 0; n--)
    {
      strip2.SetPixelColor(n, color);
      strip2.Show();
      delay(10);
    }  
    for (int n = 0; n < PIXEL_COUNT; n++)
    {
      strip1.SetPixelColor(n, color);
      strip1.Show();
      delay(10);
    }
    break;
    
  //рандомное включение по одному светодиоду (2 ленты одновременно)
  case 6:
    int rndArr[148];
    for (int i = 0; i < 148; i++)  rndArr[i] = i;
    
    for (int i = 0; i < 148; i++)
    {
      int j = random(0, 147);
      int temp = rndArr[j];
      rndArr[j] = rndArr[i];
      rndArr[i] = temp;
    }

    for (int n = 0; n < 2*PIXEL_COUNT; n++)
    {
      if (rndArr[n] < 74)
      {
      strip1.SetPixelColor(rndArr[n], color);
      strip1.Show();        
      }
      else
      {
      strip2.SetPixelColor(rndArr[n]-74, color);
      strip2.Show();         
      }
      delay(10);
    }  
    break;
  }
}


void onOffStrips(int numberStrip, int act)
{
  switch (numberStrip)
  {
    case 1:
    {
      for (int n = 0; n < PIXEL_COUNT; n++){
        if (act == ON)   strip1.SetPixelColor(n, RgbColor::LinearBlend(black, white, ledBridhtness));
        else             strip1.SetPixelColor(n, black);
      }
      strip1.Show();
      break;
    }
    case 2:
    {
      for (int n = 0; n < PIXEL_COUNT; n++){
        if (act == ON)   strip2.SetPixelColor(n, RgbColor::LinearBlend(black, white, ledBridhtness));
        else             strip2.SetPixelColor(n, black);
      }
      strip2.Show();
      break;
    }
  }
}


void updateStrip(RgbColor color)
{
  for (int n = 0; n < PIXEL_COUNT; n++)
  {
    strip1.SetPixelColor(n, color);
    strip2.SetPixelColor(n, color);
  }
  strip1.Show();
  strip2.Show();
}
