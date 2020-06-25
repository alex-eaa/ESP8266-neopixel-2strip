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
      Подключение светодиодной ленты:
   -
      Подключение других элементов (для платы типа nodeMCU ESP-12e):
   - GPIO2  (D4) - встроенный голубой wifi светодиод;
   - GPIO12 (D6) - кнопка запуска с настройками сети по умолчанию;
*/

#define DEBUG 1

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
#define GPIO_BUTTON 12       // номер пина кнопки GPIO12 (D6) 
#define GPIO_WS2812 16       // номер пина управляющий диодами WS2812

#define FILE_CONF    "/conf.txt"     //Имя файла для сохранения настроек
#define FILE_NETWORK "/net.txt"       //Имя файла для сохранения настроек сети

#define DEVICE_TYPE "esplink_pixel_"
#define TIMEOUT_T_broadcastTXT 100000   //таймаут отправки скоростных сообщений T_broadcastTXT, мкс

#define DEFAULT_AP_NAME "ESP"           //имя точки доступа "По умолчанию"
#define DEFAULT_AP_PASS "11111111"      //пароль для точки доступа "По умолчанию"

//Сохраняемые переменные (настройки сети)
bool wifiAP_mode = 0;
bool static_IP = 0;
byte ip[4] = {192, 168, 1, 43};
byte sbnt[4] = {255, 255, 255, 0};
byte gtw[4] = {192, 168, 1, 1};
char *p_ssid = new char[0];
char *p_password = new char[0];
char *p_ssidAP = new char[0];
char *p_passwordAP = new char[0];

bool sendSpeedDataEnable[] = {0, 0, 0, 0, 0};
String ping = "ping";
unsigned int speedT = 200;  //минимальный период отправки данных, миллисек
bool flagDataUpdate = 0;     //флаг обновленых данных (если 1 значит нужно отправить данные WS клиенту)

//Сохраняемые переменные (настройки параметров светильника)
int ledBridhtness = 0;                   //яркость led
int ledTemp = 0;                         //температура свечения led
int arrConstLedTemp[] = {20, 50, 70};    //предустановленные значения температуры света

int proximity = 0;                       //расстояние от датчика до объекта (0-1024)

bool flagLedState = 0;                    //флаг состояния освещения, вкл./откл.
bool flagToOnOff = 0;                     //флаг перехода в режим изменения состояния освещения на противоположное
bool flagToBrightnessChange = 0;          //флаг перехода в режим настройки яркости
bool flagToTempChange = 0;                //флаг перехода в режим настройки температуры света
bool flagDirectionBrightnessChange = 1;   //направление изменения яркости (увеличение/уменьшение) по кругу

unsigned int timeToOnOff = 100;           //Время задержки для жеста включения-отключения света, мс
unsigned int timeToBrightness = 1000;     //Время задержки для жеста перехода в режим регулировки яркости, мс
unsigned int timeToTemp = 800;            //Время задержки для жеста изменения температуры света, мс
unsigned int speedBrightnessChange = 50;  //начальная скорость изменения яркости, мс

int upLimitForBrightnessChange = 1024;    //верхний предел proximity для изменения скорости регулировки яркости
int downLimitForBrightnessChange = 256;   //нижний предел proximity для изменения скорости регулировки яркости
int divider = 16;                         /*делитель пределов proximity для изменения скорости регулировки яркости,
                                           влияет на скорость изменения яркости. upLimitForBrightnessChange/divider
                                           1-самый меделнный, чем больше тем быстрее */
int varForLimits = 0;                     /*вспом.константа для изменения направления скорости регулировки яркости
                                           рука вверх быстрее, вниз медленнее, равна
                                           (upLimitForBrightnessChange + downLimitForBrightnessChange) / divider  */

int varDownForLimits = 0;                     //вспом.константа для нижнего предела proximity, равна downLimitForBrightnessChange/divider
unsigned int prevSpeedBrightnessChange = 0;   //Вспомогательная для скорости изменения яркости, мс
unsigned int prevTime = 0;                    //Вспомогательная переменная для времени жестов
unsigned int prevTimeToTemp = 0;              //Вспомогательная переменная для времени жеста изменеия температуры света
int varForArrConstLedTemp = 0;                //вспм.перменная, индекс элемента массива arrConstLedTemp



//Создаем необходимые объекты
WebSocketsServer webSocket(81);
ESP8266WebServer server(80);
WiFiClient espClient;
WiFiUDP ntpUDP;
iarduino_APDS9930 apds;         /*Определяем объект apds для работы с датчиком APDS-9930.
                                Если у датчика не стандартный адрес, то его нужно указать: iarduino_APDS9930 apds(0xFF);  */


void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(GPIO_BUTTON, INPUT_PULLUP);
  pinMode(GPIO_LED_WIFI, OUTPUT);
  digitalWrite(GPIO_LED_WIFI, HIGH);

  SPIFFS.begin();

#ifdef DEBUG
  printChipInfo();
  scanAllFile();
  printFile(FILE_NETWORK);
#endif


  //Запуск точки доступа с параметрами поумолчанию
  if ( !loadFile(FILE_NETWORK) ||  digitalRead(GPIO_BUTTON) == 0)    startAp(DEFAULT_AP_NAME, DEFAULT_AP_PASS);
  //Запуск точки доступа
  else if (digitalRead(GPIO_BUTTON) == 1 && wifiAP_mode == 1)    startAp(p_ssidAP, p_passwordAP);
  //Запуск подключения клиента к точке доступа
  else if (digitalRead(GPIO_BUTTON) == 1 && wifiAP_mode == 0) {
    if (WiFi.getPersistent() == true)    WiFi.persistent(false);
    WiFi.softAPdisconnect(true);
    WiFi.persistent(true);
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

  varForLimits = (upLimitForBrightnessChange + downLimitForBrightnessChange) / divider;
  varDownForLimits = downLimitForBrightnessChange / divider;
}



void loop() {
  wifi_init();
  webSocket.loop();
  server.handleClient();
  MDNS.update();


  proximity = apds.getProximity();   //считать показания расстояния из датчика

  //Если к датчику поднесли руку
  if (proximity > 0) {
    //Установка флага изменения состояния освещения на противоположное. Если рука находится возле датчика более 100 мс и флаги сброшены.
    if (millis() - prevTime > timeToOnOff && flagToOnOff == 0 && flagToBrightnessChange == 0) {
      flagToOnOff = 1;
    }
    //Установка флага изменения яркости освещения. Если рука находится возле датчика более 1000 мс, свет включен и флаг изменения яркости сброшены.
    if (millis() - prevTime > timeToBrightness  && flagLedState == 1 && flagToBrightnessChange == 0) {
      flagToOnOff = 0;
      flagToBrightnessChange = 1;
    }
    //Вызов функции изменения яркости освещения, если флаг яркости установлен и рука возле датчика, с периодичностью speedBrightnessChange
    if (flagToBrightnessChange == 1) {
      if (millis() - prevSpeedBrightnessChange > speedBrightnessChange)    changeLedBridhtness();
    }
    //Изменение предустановленных значений температуры света при быстром повторном включении света и взведеном флаге разрешения
    if (flagToTempChange == 1 && millis() - prevTimeToTemp < timeToTemp) {
      if (varForArrConstLedTemp > 2)   varForArrConstLedTemp = 0;
      ledTemp = arrConstLedTemp[varForArrConstLedTemp];
      varForArrConstLedTemp ++;
      flagToTempChange = 0;
    }

    //Если руку отвели от датчика
  } else {
    //Вкл.-откл. света при отводе руки от датчика, при установленном флаге изменения света,
    //при отключении также взводим флаг разрешения изменения температуры света
    if (flagToOnOff == 1 && flagToBrightnessChange == 0) {
      flagLedState = !flagLedState;
      if (flagLedState == 0) {
        flagToTempChange = 1;
        prevTimeToTemp = millis();
      }
      flagToOnOff = 0;
    }
    //Отключение режима настройки яркости при отводе руки от датчика и изменение направления изменения яркости
    if (flagToOnOff == 0 && flagToBrightnessChange == 1) {
      flagToBrightnessChange = 0;
      flagDirectionBrightnessChange = !flagDirectionBrightnessChange;
    }
    prevTime = millis();
  }


  //Включение отключение света
  //if (flagLedState == 1)   digitalWrite(GPIO_LED_WIFI, LOW);
  //else  digitalWrite(GPIO_LED_WIFI, HIGH);


  Serial.print((String) "proximity=" + proximity + ", ");
  Serial.print((String) "ON=" + flagLedState + ", ");
  Serial.print((String) "B=" + ledBridhtness + ", ");
  Serial.print((String) "T=" + ledTemp + "\n");

  delay(50);




  //Отправка Speed данных клиентам при условии что данныее обновились и клиенты подключены
  if (flagDataUpdate == 1) {
    if (sendSpeedDataEnable[0] || sendSpeedDataEnable[1] || sendSpeedDataEnable[2] || sendSpeedDataEnable[3] || sendSpeedDataEnable[4] ) {
      String data = serializationToJson_index();
      int startT_broadcastTXT = micros();
#ifdef DEBUG
      Serial.print("\nwebSocket.broadcastTXT: ");
      Serial.println(data);
#endif
      webSocket.broadcastTXT(data);
      int T_broadcastTXT = micros() - startT_broadcastTXT;
      if (T_broadcastTXT > TIMEOUT_T_broadcastTXT)  checkPing();
    }
    flagDataUpdate = 0;
  }

}



void changeLedBridhtness() {
  int var;
  if (flagDirectionBrightnessChange == 1)   var = ledBridhtness + 1;
  else   var = ledBridhtness - 1;

  if (var > 100)       ledBridhtness = 0;
  else if (var < 0)    ledBridhtness = 100;
  else                 ledBridhtness = var;

  //Регулируем скорость изменения яркости взависисмости от расстояния между рукой и датчиком, равна
  //(upLimitForBrightnessChange + downLimitForBrightnessChange)/divider - proximity / divider
  speedBrightnessChange = varForLimits - proximity / divider;
  if (proximity < downLimitForBrightnessChange) {
    speedBrightnessChange = varForLimits - downLimitForBrightnessChange / divider / 8;
  }

  prevSpeedBrightnessChange = millis();
}

