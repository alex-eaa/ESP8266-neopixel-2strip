//Сохранение переменных в файл формата JSON
bool saveFile(char *filename)
{
  //Serial.print(F("Save file: "));   Serial.println(filename);
  SPIFFS.remove(filename);

  DynamicJsonDocument doc(1024);

  if (filename == FILE_NETWORK) {
    doc["wifiAP_mode"] = wifiAP_mode;
    doc["p_ssidAP"] = p_ssidAP;
    doc["p_passwordAP"] = p_passwordAP;
    doc["p_ssid"] = p_ssid;
    doc["p_password"] = p_password;
    doc["static_IP"] = static_IP;
    JsonArray ipJsonArray = doc.createNestedArray("ip");
    for (int n = 0; n < 4; n++)  ipJsonArray.add(ip[n]);
    JsonArray sbntJsonArray = doc.createNestedArray("sbnt");
    for (int n = 0; n < 4; n++)  sbntJsonArray.add(sbnt[n]);
    JsonArray gtwJsonArray = doc.createNestedArray("gtw");
    for (int n = 0; n < 4; n++)  gtwJsonArray.add(gtw[n]);
  }
  else if (filename == FILE_CONF) {
    doc["ledBridhtness"] = ledBridhtness;
    doc["minBridhtness"] = minBridhtness;
    doc["maxBridhtness"] = maxBridhtness;
    doc["varForArrConstLedTemp"] = varForArrConstLedTemp;
    doc["nAnimeOn"] = nAnimeOn;
    doc["nAnimeOff"] = nAnimeOff;
    JsonArray arrConstLedTemp0 = doc.createNestedArray("arrConstLedTemp0");
    for (int n = 0; n < 3; n++)  arrConstLedTemp0.add(arrConstLedTemp[0][n]);
    JsonArray arrConstLedTemp1 = doc.createNestedArray("arrConstLedTemp1");
    for (int n = 0; n < 3; n++)  arrConstLedTemp1.add(arrConstLedTemp[1][n]);
    JsonArray arrConstLedTemp2 = doc.createNestedArray("arrConstLedTemp2");
    for (int n = 0; n < 3; n++)  arrConstLedTemp2.add(arrConstLedTemp[2][n]);
  }

  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.print(F("Failed to open file for writing"));   Serial.println(filename);
    return 0;
  }
  bool rezSerialization = serializeJson (doc, file);
#ifdef DEBUG
  //Serial.println("\nSaveFile:");
  Serial.print("\nSave to File ");
  Serial.println(filename);
  serializeJson(doc, Serial);
  Serial.println("");
#endif
  file.close();

  if (rezSerialization == 0)  Serial.print(F("Failed write to file: "));
  else {
#ifdef DEBUG
    Serial.println(F("Save file complited: \n"));
#endif
  }
  return rezSerialization;
}



//Чтение переменных из файла в формате JSON
bool loadFile(char *filename) {
  //Serial.print(F("Load file: "));   Serial.println(filename);
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print(F("Failed read file: "));   Serial.println(filename);
    return 0;
  }

  DynamicJsonDocument doc(1024);
  //ReadBufferingStream bufferedFile {file, 64 };
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("Failed deserialization file: "));   Serial.println(filename);
    file.close();
    return 0;
  }

  // Copy values from the JsonDocument to the value
  if (filename == FILE_NETWORK) {
    delete[] p_passwordAP;
    delete[] p_ssidAP;
    delete[] p_password;
    delete[] p_ssid;

    String stemp = doc["p_ssid"].as<String>();
    p_ssid = new char [stemp.length() + 1];
    stemp.toCharArray(p_ssid, stemp.length() + 1);

    stemp = doc["p_password"].as<String>();
    p_password = new char [stemp.length() + 1];
    stemp.toCharArray(p_password, stemp.length() + 1);

    stemp = doc["p_ssidAP"].as<String>();
    p_ssidAP = new char [stemp.length() + 1];
    stemp.toCharArray(p_ssidAP, stemp.length() + 1);

    stemp = doc["p_passwordAP"].as<String>();
    p_passwordAP = new char [stemp.length() + 1];
    stemp.toCharArray(p_passwordAP, stemp.length() + 1);

    wifiAP_mode = doc["wifiAP_mode"];    //Serial.println(wifiAP_mode);
    static_IP = doc["static_IP"];        //Serial.println(static_IP);
    for (int n = 0; n < 4; n++){
      ip[n] = doc["ip"][n];              //Serial.println(ip[n]);
      sbnt[n] = doc["sbnt"][n];          //Serial.println(sbnt[n]);
      gtw[n] = doc["gtw"][n];            //Serial.println(gtw[n]);
    }
  }
  
  else if (filename == FILE_CONF) {
    ledBridhtness = doc["ledBridhtness"];                   //Serial.println(ledBridhtness);
    minBridhtness = doc["minBridhtness"];                   //Serial.println(minBridhtness);
    maxBridhtness = doc["maxBridhtness"];                   //Serial.println(maxBridhtness);
    varForArrConstLedTemp = doc["varForArrConstLedTemp"];   //Serial.println(varForArrConstLedTemp);
    nAnimeOn = doc["nAnimeOn"];                             //Serial.println(nAnimeOn);
    nAnimeOff = doc["nAnimeOff"];                           //Serial.println(nAnimeOff);
    for (int n = 0; n < 3; n++){
      arrConstLedTemp[0][n] = doc["arrConstLedTemp0"][n];   //Serial.println(arrConstLedTemp[0][n]);
      arrConstLedTemp[1][n] = doc["arrConstLedTemp1"][n];   //Serial.println(arrConstLedTemp[1][n]);
      arrConstLedTemp[2][n] = doc["arrConstLedTemp2"][n];   //Serial.println(arrConstLedTemp[2][n]);
    }
  }

  file.close();
  return 1;
}



//Prints the content of a file to the Serial
void printFile(const char *filename)
{
  Serial.print(F("Print file: "));   Serial.println(filename);
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print(F("Failed to read file: "));   Serial.println(filename);
    return;
  }

  // Extract each characters by one by one
  Serial.print(F("   "));
  while (file.available()) {
    Serial.print((char)file.read());
  }
  file.close();
  Serial.println("\n");
}



//format bytes
String formatBytes(size_t bytes)
{
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}


//Функция сканирования всех файлов и отображение их имен и размеров
void scanAllFile()
{
  //Serial.println("\nScan files:");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
}
