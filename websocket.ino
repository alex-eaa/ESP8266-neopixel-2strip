// функция иннициализации сервера WebSocket
void webSocket_init()
{
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}


//функция обработки входящих на сервер WebSocket сообщений
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch (type) {

    case WStype_DISCONNECTED:
      sendSpeedDataEnable[num] = 0;
#ifdef DEBUG
      Serial.printf("\n[%u] Disconnected!\n", num);
#endif
      //Serial.printf("WStype_DISCONNECTED sendSpeedDataEnable [%u][%u][%u][%u][%u]\n", sendSpeedDataEnable[0], sendSpeedDataEnable[1], sendSpeedDataEnable[2], sendSpeedDataEnable[3], sendSpeedDataEnable[4]);
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
#ifdef DEBUG
        Serial.printf("\n[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
#endif
        if (strcmp((char *)payload, "/index.htm") == 0) {
          sendSpeedDataEnable[num] = 1;
          sendToWsClient(num, serializationToJson_index());
        } else if (strcmp((char *)payload, "/setup.htm") == 0) {
          sendSpeedDataEnable[num] = 0;
          sendToWsClient(num, serializationToJson_setup());
        }
        break;
      }

    case WStype_TEXT:
      {
#ifdef DEBUG
        Serial.printf("\n[%u] get from WS: %s\n", num, payload);
#endif
        if (strcmp((char *)payload, "RESET") == 0) {
          delay(50);
          ESP.reset();
        }
        else {
          deserealizationFromJson((char *)payload);
        }
        break;
      }

    case WStype_BIN:
      //Serial.printf("[%u] get binary length: %u\n", num, length);
      hexdump(payload, length);
      // send message to client
      // webSocket.sendBIN(num, payload, length);
      break;
  }

}


void sendToWsClient(int num, String json) {
#ifdef DEBUG
  Serial.printf("\n[%u] ", num);
  Serial.print("sent to WS_Client: ");
  Serial.println(json);
#endif
  webSocket.sendTXT(num, json);
}




// Проверка состояния соединения с websocket-клиентами. Отключение тех с которыми нет связи.
void checkPing() {
#ifdef DEBUG
  Serial.println("\nStart checkPing");
#endif
  for (uint8_t nums = 0; nums < 5; nums++) {
    int timeStart = micros();
    if ( !webSocket.sendPing(nums, ping) )  webSocket.disconnect(nums);
#ifdef DEBUG
    int timeTotal = micros() - timeStart;
    Serial.printf("TIME ping [%u]: %u\n", nums, timeTotal);
#endif
  }
}
