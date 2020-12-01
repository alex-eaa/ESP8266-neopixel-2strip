  //int timeStart = micros();
  //int deltaTime = micros() - timeStart;
  //Serial.print("sendToWsClient time=");  Serial.println(deltaTime);

//Print configuration parameters
void printConfiguration() 
{
  Serial.println(F("\nPrint ALL VARIABLE:"));
  Serial.print(F("wifiAP_mode="));  Serial.println(wifiAP_mode);
  Serial.print(F("p_ssidAP="));     Serial.println(p_ssidAP);
  Serial.print(F("p_passwordAP=")); Serial.println(p_passwordAP);
  Serial.print(F("p_ssid="));       Serial.println(p_ssid);
  Serial.print(F("p_password="));   Serial.println(p_password);
  Serial.print(F("static_IP="));     Serial.println(static_IP);
  Serial.print(F("ip="));    Serial.print(ip[0]);   Serial.print(":");  Serial.print(ip[1]);  Serial.print(":");  Serial.print(ip[2]);  Serial.print(":");  Serial.println(ip[3]);
  Serial.print(F("sbnt="));  Serial.print(sbnt[0]); Serial.print(":");  Serial.print(sbnt[1]);  Serial.print(":");  Serial.print(sbnt[2]);  Serial.print(":");  Serial.println(sbnt[3]);
  Serial.print(F("gtw="));   Serial.print(gtw[0]);  Serial.print(":");  Serial.print(gtw[1]);  Serial.print(":");  Serial.print(gtw[2]);  Serial.print(":");  Serial.println(gtw[3]);
}



void printChipInfo() 
{
  Serial.print(F("\n<-> LAST RESET REASON: "));  Serial.println(ESP.getResetReason());
  Serial.print(F("<-> ESP8266 CHIP ID: "));      Serial.println(String(ESP.getChipId(), HEX));
  Serial.print(F("<-> CORE VERSION: "));         Serial.println(ESP.getCoreVersion());
  Serial.print(F("<-> SDK VERSION: "));          Serial.println(ESP.getSdkVersion());
  Serial.print(F("<-> CPU FREQ MHz: "));         Serial.println(ESP.getCpuFreqMHz());
  Serial.print(F("<-> FLASH CHIP ID: "));        Serial.println(String(ESP.getFlashChipId(), HEX));
  Serial.print(F("<-> FLASH CHIP SIZE: "));      Serial.println(ESP.getFlashChipSize());
  Serial.print(F("<-> FLASH CHIP REAL SIZE: ")); Serial.println(ESP.getFlashChipRealSize());
  Serial.print(F("<-> FLASH CHIP SPEED: "));     Serial.println(ESP.getFlashChipSpeed());
  Serial.print(F("<-> FREE MEMORY: "));          Serial.println(ESP.getFreeHeap());
  Serial.print(F("<-> SKETCH SIZE: "));          Serial.println(ESP.getSketchSize());
  Serial.print(F("<-> FREE SKETCH SIZE: "));     Serial.println(ESP.getFreeSketchSpace());
  Serial.print(F("<-> CYCLE COUNTD: "));         Serial.println(ESP.getCycleCount());
  Serial.println("");
}



//Monitoring Status WiFi module to serial
void WifiStatus(void)
{
  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.print(F(": WiFi.status = WL_CONNECTED. ")); //3
    Serial.print(F("IP address: "));  Serial.println(WiFi.localIP());
  }
  else if (WiFi.status() == WL_NO_SHIELD) 
  {
    Serial.println(F(": WiFi.status = WL_NO_SHIELD ")); //255
  }
  else if (WiFi.status() == WL_IDLE_STATUS) 
  {
    Serial.println(F(": WiFi.status = WL_IDLE_STATUS ")); //0
  }
  else if (WiFi.status() == WL_NO_SSID_AVAIL) 
  {
    Serial.println(F(": WiFi.status = WL_NO_SSID_AVAIL ")); //1
  }
  else if (WiFi.status() == WL_SCAN_COMPLETED) 
  {
    Serial.println(F(": WiFi.status = WL_SCAN_COMPLETED ")); //2
  }
  else if (WiFi.status() == WL_CONNECT_FAILED) 
  {
    Serial.println(F(": WiFi.status = WL_CONNECT_FAILED ")); //4
  }
  else if (WiFi.status() == WL_CONNECTION_LOST) 
  {
    Serial.println(F(": WiFi.status = WL_CONNECTION_LOST ")); //5
  }
  else if (WiFi.status() == WL_DISCONNECTED) 
  {
    Serial.println(F(": WiFi.status = WL_DISCONNECTED ")); //6
  }
}
