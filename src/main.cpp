#include <Wire.h>               // libreria de comunicacion por I2C
#include <LiquidCrystal_I2C.h>  // libreria para LCD por I2C
#include <RTClib.h>             // libraria para el modulo RTC, para medir el tiempo
#include "FS.h"                 // Librerias de para memoria SD
#include "SD.h"
#include "SPI.h"
#include <vector>  // Libreria para declarar vectores
#include <TMCStepper.h>
#include <Arduino.h>
#include <vector>  // Libreria para declarar vectores
#include <WiFi.h>
#include <HTTPClient.h>
#include <iostream>
#include <string>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <secrets.h>
#include <cstring>

// ----LCD CONFIGURATION ----

LiquidCrystal_I2C lcd(0x27, 20, 4);  // DIR, E, RW, RS, D4, D5, D6, D7

// ----LCD CONFIGURATION ----

RTC_DS3231 rtc;


const int sd_chip_select = 5;  // Pin CS para la tarjeta SD

//Counter of steps in a routine
//Steps of the routine
volatile int X[20];
volatile int Y[20];
int MINUTOS[40];
int SEGUNDOS[40];

// Variables for encoders
int CLK_A = 33;
int DT_A = 32;
int BUTTON_A = 39;
int CLK_B = 35;
int DT_B = 34;
int BUTTON_B = 36;

//--------- STEP MOTORS -----------------

int DIRX = 12;
int PULX = 27;

int DIRY = 25;
int PULY = 26;
int VEL = 300;

int FACTOR_MOVEMENT = 400;  // pasos/mm

int TOTAL_STEPS_MOVEMENT = 100;
int GENERAL_STEP_X = 3;  // GENERAL_STEP_X*TOTAL_MOVEMENT = 3*100 = 300 mm = 30cm
int GENERAL_STEP_Y = 2;  // GENERAL_STEP_Y*TOTAL_MOVEMENT = 2*100 = 200 mm = 20cm

//Variables for drivers
#define RXD2 16
#define TXD2 17

static constexpr float R_SENSE = 0.11f;
static constexpr uint8_t DRIVER_ADDR_1 = 0;
static constexpr uint8_t DRIVER_ADDR_2 = 1;

int current_usteps = 8;

HardwareSerial TMCSerial(2);
TMC2209Stepper driver1(&TMCSerial, R_SENSE, DRIVER_ADDR_1);
TMC2209Stepper driver2(&TMCSerial, R_SENSE, DRIVER_ADDR_2);


bool is_process_in_execution = false;

//--------- STEP MOTORS -----------------


// -------- END POINT -------------------


String token;

Preferences preferences;
String jwtToken = "";

// -------- END POINT -------------------


// -------- CONECTIVITY -----------------

String device_id = "esp32_0000001";
String session_id = "1";

String network;
String password;
String folderNetwork = "/credentials";


const char* nombreArchivo = "/pos.txt";

bool is_wifi_connected;
bool is_sd_connected;
bool is_rtc_connected;

File file;

int BATCH_SIZE = 15;

unsigned long MyTestTimer = 0;  // variables MUST be of type unsigned long
const byte OnBoard_LED = 2;


// -------- CONECTIVITY ---------------


// Lista completa de caracteres
const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{};:,.<>?/";
const int charsetSize = 63;

struct Asociacion {  //la estructura crea una relación entre cada nombre de archivo y un número
  int identificador;
  String nombreArchivo;
};

DateTime startTime;


void encoder1();
void encoder2();
void push_a();
void push_b();

void printDriverYInfo(const char* tag);
void printDriverXInfo(const char* tag);

void printCurrentTime();
void printDirectory(File dir, int numTabs);

char printCurrentCharacter(int currentIndex);

void startTimeForOut();

bool has_interval_passed(unsigned long& previousTime, unsigned long interval);
boolean TimePeriodIsOver(unsigned long& periodStartTime, unsigned long TimePeriod);
bool clearDirectory(String dirPath);

void BlinkHeartBeatLED(int IO_Pin, int BlinkPeriod);
void flashLED(int duration_ms);



class APIEndPoint {
  public:
    std::vector<Asociacion> networks;
    std::vector<Asociacion> saveNetworks;
    const char* fileName = "/network.txt";
    String networkSelected = "";

    APIEndPoint() {
    }

    bool establishConnection(String network, String password) {
      WiFi.mode(WIFI_STA);
      Serial.println("WiFi.mode(WIFI_STA)");

      int myCount = 0;

      Serial.print("trying to connect to #");
      Serial.print(network);
      Serial.println("#");
      WiFi.begin(network, password);

      lcd.clear();
      lcd.setCursor(1, 1);
      lcd.print("trying to connect");
      lcd.setCursor(1, 2);
      lcd.print(network);
      lcd.print("#");

      // Wait for connection
      while (WiFi.status() != WL_CONNECTED && myCount < 101) {
        BlinkHeartBeatLED(OnBoard_LED, 50);  // blink LED fast during attempt to connect
        yield();
        if (TimePeriodIsOver(MyTestTimer, 500)) {  // once every 500 miliseconds
          Serial.print(".");                       // print a dot
          myCount++;

          if (myCount > 100) {  // after 30 dots = 15 seconds restart
            Serial.println();
            Serial.print("not yet connected executing ESP.restart();");
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("Failed to connect...");
            lcd.setCursor(0, 2);
            lcd.print("stay close to WIFI");
            delay(10000);
          }
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.print("Connected to #");
        Serial.print(network);
        Serial.print("# IP address: ");
        Serial.println(WiFi.localIP());

        lcd.clear();
        lcd.setCursor(3, 1);
        lcd.print("Connected to...");
        lcd.setCursor(2, 2);
        lcd.print(network);
        lcd.print("#");
        delay(2000);

        return true;
      }else{
        return false;
      }
    }

    bool loadLastValidCredentials() {
      String currentPath = folderNetwork + "/current_network.txt";

      File currentFile = SD.open(currentPath, FILE_READ);
      if (!currentFile) {
        Serial.println("No se pudo abrir current_network.txt");
        return false;
      }

      String currentNetwork = currentFile.readStringUntil('\n');
      currentNetwork.trim();
      currentFile.close();

      if (currentNetwork == "") {
        Serial.println("No hay una red actual guardada.");
        return false;
      }

      String credentialPath = folderNetwork + "/" + currentNetwork + ".txt";
      File credFile = SD.open(credentialPath, FILE_READ);

      if (!credFile) {
        Serial.println("No se pudo abrir el archivo de credenciales de la red actual.");
        return false;
      }

      network = credFile.readStringUntil('\n');
      network.trim();

      password = credFile.readStringUntil('\n');
      password.trim();

      credFile.close();

      Serial.println("Ultimas credenciales validas cargadas:");
      Serial.println(network);
      Serial.println(password);

      return true;
    }

    void saveNetworkAssociations() {
      Asociacion aux;
      Serial.println("Redes guardadas:");
      


      File dir = SD.open(folderNetwork);

      if (!dir) {
        Serial.println("No se pudo abrir la carpeta de redes.");
        return;
      }

      File file = dir.openNextFile();

      int count = 0;

      while (file) {

        String filename = file.name();

        // ignorar el archivo que guarda la red actual
        if (filename != "current_network.txt") {

          // quitar la extensión .txt
          filename.replace(".txt", "");

          Serial.print(count);
          Serial.print(": ");
          Serial.println(filename);

          aux = {count, filename};
          saveNetworks.insert(saveNetworks.begin(), aux);

          count++;
        }

        file.close();
        file = dir.openNextFile();
      }

      dir.close();

      if (count == 0) {
        Serial.println("No hay redes guardadas.");
      }
    }

    void verifyConnection() {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to the WiFi network");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        is_wifi_connected = true;
      } else {
        Serial.println("WiFi connection failed (timeout).");
        is_wifi_connected = false;
      }
    }

    void sendNetworkName(String fileName) {
      networkSelected = fileName;
    }

    void saveNetworkName() {
      File file = SD.open(fileName, FILE_WRITE);

      if (file) {
        file.seek(0);
        Serial.println("red: " + networkSelected);
        delay(2000);
        file.print(networkSelected);
        file.close();
        Serial.println("Contenido del archivo guardado correctamente.");
      } else {
        Serial.println("¡Error al abrir el archivo!");
      }
    }


    String getNetworkName(int identificador) {
      for (const auto& network : networks) {
        if (network.identificador == identificador) {
          return network.nombreArchivo;
        }
      }
      // Si no se encuentra ninguna asociación para el identificador dado
      return "";
    }

    void selectLastNetwork() {
      // if (getNameFile() == "") {  //si no hay un archivo seleccionado, selecciona el último que se guardo
      //   networkAssociations();
      //   fileSelected = getNetworkName(num_archivos - 1);  // Del número de archivos contados se queda con el último

      //   processData(fileSelected);
      // } else {                      //si hay un arhivo seleccionado, obtiene los datos de él
      //   processData(fileSelected);  //recupera los datos del archivo seleccionado
      // }
    }

    void networkAssociations() {
      int n = WiFi.scanNetworks();
      Asociacion aux;

      Serial.println("scan done");
      if (n == 0) {
          Serial.println("no networks found");
      } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {

          Serial.print(WiFi.SSID(i));
          aux = {i, WiFi.SSID(i)};
          networks.insert(networks.begin(), aux);

        }

      }

    }

    bool sendDataToEndpoint(JsonDocument& doc) {

      verifyConnection();

      if (!is_wifi_connected) {
        Serial.println("ERROR | FAILED | error: No WIFI connectivity found...");
        return false;
      }

      WiFiClientSecure client;
      client.setInsecure();  // evita problemas con certificados

      HTTPClient http;

      http.setTimeout(20000);
      http.begin(client, INGEST_BATCH_URL);

      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer " + jwtToken);

      String requestBody;
      serializeJson(doc, requestBody);

      int httpResponseCode = http.POST(requestBody);

      if (httpResponseCode >= 200 && httpResponseCode < 300) {

        String response = http.getString();

        Serial.println("INFO | API RESPONSE: " + String(httpResponseCode));
        Serial.println("INFO | BODY: " + response);

        http.end();
        return true;

      } else {

        String response = http.getString();

        Serial.println("ERROR | API RESPONSE: " + String(httpResponseCode));
        Serial.println("ERROR | BODY: " + response);

        http.end();
        return false;
      }
    }




    String loginAndGetToken(const String& url, const String& email, const String& password) {
      WiFiClientSecure client;
      client.setInsecure();  // para pruebas

      HTTPClient https;

      if (!https.begin(client, url)) {
        Serial.println("No se pudo iniciar la conexión HTTPS");
        return "";
      }

      https.addHeader("Content-Type", "application/json");

      StaticJsonDocument<256> doc;
      doc["email"] = email;
      doc["password"] = password;

      String body;
      serializeJson(doc, body);

      int httpCode = https.POST(body);

      if (httpCode <= 0) {
        Serial.print("Error en la petición: ");
        Serial.println(https.errorToString(httpCode));
        https.end();
        return "";
      }

      String response = https.getString();
      https.end();

      if (httpCode != 200) {
        Serial.print("Login falló. Código HTTP: ");
        Serial.println(httpCode);
        Serial.println(response);
        return "";
      }

      StaticJsonDocument<1024> resDoc;
      DeserializationError error = deserializeJson(resDoc, response);

      if (error) {
        Serial.print("Error al leer JSON: ");
        Serial.println(error.c_str());
        return "";
      }

      String token = resDoc["access_token"] | "";
      return token;
    }


  private:
};



class SaveSensorData {
  public:
    String file_name;
    String main_dir = "/iot/daily";
    File myFile;
    String base_name;
    APIEndPoint ApiEndpoint;

    // // establish file name
    SaveSensorData() {
    }


    void establishConnection() {
      Serial.println("Connecting to SD...");

      if (!SD.begin(sd_chip_select)) {
        unsigned long start_time = millis();
        const unsigned long timeout = 10000;

        Serial.println("Failed while connecting with SD");

        do {
          Serial.println("Retrying...");
          delay(3000);

          if (has_interval_passed(start_time, timeout)) {
            Serial.println("SD connection timeout reached.");
            break;
          }

        } while (!SD.begin(sd_chip_select));
      }


      verifyConnection();
    }

    void verifyConnection() {
      if (SD.begin(sd_chip_select)) {
        // Serial.println("SD connected successfully.");
        is_sd_connected = true;
      } else {
        // Serial.println("SD connection failed.");
        is_sd_connected = false;
      }
    }

    void sendAllFilesInDirectory(const char* dirPath) {
      bool file_success;
      int number_loading_file = 0;

      File dir = SD.open(dirPath);
      if (!dir) {
        Serial.println("Failed to open directory");
        return;
      }
      if (!dir.isDirectory()) {
        Serial.println("Path is not a directory");
        dir.close();
        return;
      }

      lcd.clear();
      lcd.setCursor(3, 1);
      lcd.print("SENDING FILES");
      delay(5000);

      jwtToken = ApiEndpoint.loginAndGetToken(
        LOGIN_ENDPOINT,
        EMAIL,
        PASSWORD
      );

      if (jwtToken != "") {
        lcd.clear();
        lcd.setCursor(5, 1);
        lcd.print("CONNECTION");
        lcd.setCursor(4, 2);
        lcd.print("ESTABLISHED");
        delay(5000);
      } else {
        lcd.clear();
        lcd.setCursor(5, 1);
        lcd.print("CONNECTION");
        lcd.setCursor(6, 2);
        lcd.print("FAILED");
        delay(5000);
        return;
      }


      File entry;
      while (true) {
        entry = dir.openNextFile();

        if (!entry && number_loading_file == 0) {
          lcd.clear();
          lcd.setCursor(5, 1);
          lcd.print("NO FILES");
          lcd.setCursor(5, 2);
          lcd.print("TO LOAD...");
          delay(5000);
          lcd.clear();
        }

        if (!entry) break;  // no more files
        String fullPath = String(dirPath) + "/" + String(entry.name());

        if (!entry.isDirectory()) {

          Serial.println("INFO | STARTED | Sending file: " + fullPath);


          lcd.clear();
          lcd.setCursor(4, 1);
          lcd.print("SENDING FILE:");
          lcd.setCursor(1, 2);
          lcd.print(entry.name());
          delay(5000);

          file_success = sendFileInBatches(fullPath);
          if (file_success) {
            Serial.println("INFO | FINISHED | Sending file: " + fullPath);

            lcd.clear();
            lcd.setCursor(2, 1);
            lcd.print("CORRECTLY SEND:");
            lcd.setCursor(1, 2);
            lcd.print(entry.name());
            delay(5000);
            lcd.clear();

            int lastSlash = fullPath.lastIndexOf('/');
            String fileName = fullPath.substring(lastSlash + 1);

            String newPath = "/iot/saved/" + fileName;

            SD.rename(fullPath, newPath);
            SD.remove(fullPath);

            // if (SD.rename(fullPath, newPath)) {
            //     Serial.println("INFO | DONE | File moved to new directory: " + newPath);
            // } else {
            //     Serial.println("INFO | FAILED | File moved to new directory: " + newPath);
            // }

          } else {
            Serial.println("ERROR | FAILED | Sending file: " + fullPath);

            lcd.clear();
            lcd.setCursor(2, 1);
            lcd.print("FAILED FOR FILE:");
            lcd.setCursor(1, 2);
            lcd.print(entry.name());
            delay(5000);
            lcd.clear();

          }
        }

        entry.close();
        number_loading_file += 1;
      }

      dir.close();
    }



    bool sendFileInBatches(String filePath) {


      // verify connectivity and file existence with SD
      verifyConnection();
      if (!is_sd_connected) {
        return 0;
      }

      File file = SD.open(filePath, FILE_READ);
      if (!file) {
        Serial.println("Error opening file");
        return 0;
      }


      int seq = 0;


      Serial.println("INFO | STARTED | Batch uploading");
      while (file.available()) {

        // Crear JSON dinámico
        StaticJsonDocument<4096> doc;

        doc["device_id"] = device_id;
        doc["session_id"] = session_id;

        String batch_id = session_id + "_" + String(seq);
        doc["batch_id"] = batch_id;
        doc["seq"] = seq;

        JsonArray rows = doc.createNestedArray("rows");

        int count = 0;

        while (file.available() && count < BATCH_SIZE) {
          String line = file.readStringUntil('\n');
          // String lineAux = "HOla Mundo";
          line.trim();
          if (line.length() > 0) {
            rows.add(line);
            count++;
          }
        }

        if (rows.size() > 0) {
          bool success = sendWithRetry(doc, 10);

          if (!success) {
            delay(2000);
            Serial.println("ERROR | FAILED | Sending Batch " + String(seq) + " to API");
            seq++;
            return 0;
            // continue;  // retry same seq
          } else {
            Serial.println("INFO | DONE | Sending Batch " + String(seq) + " to API");
            flashLED(30);
          }

          seq++;
        }
      }

      file.close();
      Serial.println("INFO | FINISHED | Batch uploading");
      return 1;
    }


    bool sendWithRetry(JsonDocument& doc, int maxRetries = 3) {
      for (int attempt = 1; attempt <= maxRetries; attempt++) {
        if (ApiEndpoint.sendDataToEndpoint(doc)) {
          return true;
        }
        Serial.println("INFO | FAILED | Retrying Batch | Attempt: " + String(attempt));
        delay(500);
      }
      return 0;
    }

    bool moveFile(const String& sourcePath, const String& destinationDir) {

      File sourceFile = SD.open(sourcePath, FILE_READ);
      Serial.println(sourcePath);
      if (!sourceFile) {
        Serial.println("Failed to open source file");
        return false;
      }

      if (!SD.exists("/iot/saved")) {
        SD.mkdir("/iot/saved");
      }

      // Extract file name from path
      int lastSlash = sourcePath.lastIndexOf('/');
      String fileName = sourcePath.substring(lastSlash + 1);

      String destinationPath = destinationDir;
      Serial.print("File: ");
      Serial.println(destinationPath);

      File destFile = SD.open(destinationPath, FILE_WRITE);
      if (!destFile) {
        Serial.println("Failed to create destination file");
        sourceFile.close();
        return false;
      }

      // Copy content
      while (sourceFile.available()) {
        destFile.write(sourceFile.read());
      }

      sourceFile.close();
      destFile.close();

      // Delete original
      if (SD.remove(sourcePath)) {
        Serial.println("File moved successfully");
        return true;
      } else {
        Serial.println("Failed to delete original file");
        return false;
      }
    }

    // initialize dir if not exist
    void create(String glanularity) {
      DateTime current_time = rtc.now();
      base_name = obtenerNombreArchivo(current_time, glanularity) + ".csv";
      file_name = main_dir + "/" + base_name;


      // Serial.print("Writing in file: ");
      // Serial.println(file_name);

      // Make sure SD is mounted before this block (SD.begin(...))

      // 1) Ensure /iot exis
      if (!SD.exists("/iot")) {
        // Serial.println("Creating /iot ...");
        if (!SD.mkdir("/iot")) {
          // Serial.println("ERROR: failed to create /iot");
        }
      }

      // 2) Ensure /iot/daily exists
      if (SD.exists("/iot")) {
        if (!SD.exists("/iot/daily")) {
          // Serial.println("Creating /iot/daily ...");
          if (!SD.mkdir("/iot/daily")) {
            // Serial.println("ERROR: failed to create /iot/daily");
          }
        }
      }
      // 2) Ensure /iot/daily exists
      if (SD.exists("/iot")) {
        if (!SD.exists("/iot/saved")) {
          // Serial.println("Creating /iot/saved ...");
          if (!SD.mkdir("/iot/saved")) {
            // Serial.println("ERROR: failed to create /iot/daily");
          }
        }
      }



      // 3) Final verification
      if (SD.exists("/iot/daily")) {
        // Serial.println("Folder /iot/daily ready ✅");
      } else {
        // Serial.println("Folder /iot/daily NOT available ❌");
      }

      if (SD.exists(file_name)) {
        // Serial.println("Existe el archivo");
      } else {
        // Serial.println("No existe el archivo");
      }

      myFile = SD.open(file_name, FILE_APPEND);
    }

    void write(const String& line) {
      if (myFile) {
        myFile.print(line);
        // Serial.println("Wrote: " + line);

      } else {
        Serial.println("File not open!");
      }
    }

    void close() {
      myFile.flush();  // important to actually write to SD
      myFile.close();
    }

    void printAll() {

      File file = SD.open(file_name, FILE_READ);

      Serial.println("---- FILE CONTENT ----");

      while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);
      }

      Serial.println("---- END OF FILE ----");

      file.close();
    }

    bool deleteOnlyFilesInDirectory(const char* dirPath) {

      File dir = SD.open(dirPath);
      if (!dir) {
        Serial.println("Failed to open directory");
        return false;
      }

      if (!dir.isDirectory()) {
        Serial.println("Path is not a directory");
        dir.close();
        return false;
      }

      File entry;

      while (true) {
        entry = dir.openNextFile();
        if (!entry) break;  // no more files

        if (!entry.isDirectory()) {

          String filePath = String(dirPath) + "/" + String(entry.name());

          Serial.print("Deleting file: ");
          Serial.println(filePath);

          entry.close();  // IMPORTANT: close before deleting
          SD.remove(filePath);
        } else {
          // It is a directory → skip it
          entry.close();
        }
      }

      dir.close();

      Serial.println("Finished deleting files.");
      return true;
    }



  private:

  String obtenerNombreArchivo(DateTime fechaHora, String glanularity) {
    String nombreArchivo = "";
    // Construir el nombre del archivo usando los componentes de fecha y hora
    nombreArchivo += String(fechaHora.year(), DEC);
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.month());
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.day());
    nombreArchivo += "_";

    if (glanularity == "hour") {
      nombreArchivo += dosDigitos(fechaHora.hour());
    }

    if (glanularity == "min") {
      nombreArchivo += dosDigitos(fechaHora.hour());
      nombreArchivo += "-";
      nombreArchivo += dosDigitos(fechaHora.minute());
    }

    if (glanularity == "sec") {
      nombreArchivo += dosDigitos(fechaHora.hour());
      nombreArchivo += "-";
      nombreArchivo += dosDigitos(fechaHora.minute());
      nombreArchivo += "-";
      nombreArchivo += dosDigitos(fechaHora.second());
    }

    return nombreArchivo;
  }

  String dosDigitos(int numero) {
    if (numero < 10) {
      return "0" + String(numero);
    } else {
      return String(numero);
    }
  }
};



class TimeManager {
  public:
    int current_day = -1;
    DateTime started_time;

    TimeManager() {
    }

    void establishConnection() {
      Serial.println("Connecting to RTC module...");
      if (!rtc.begin()) {

        unsigned long start_time = millis();
        const unsigned long timeout = 10000;

        Serial.println("Failed while connecting with RTC module...");

        do {
          Serial.println("Retrying...");
          delay(3000);

          if (has_interval_passed(start_time, timeout)) {
            Serial.println("RTC connection timeout reached.");
            break;
          }

        } while (!rtc.begin());
      }

      // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

      if (rtc.begin()) {
        started_time = rtc.now();
      }
      verifyConnection();
      
    }

    void verifyConnection() {
      if (rtc.begin()) {
        Serial.println("RTC connected successfully.");
        // rtc.adjust(DateTime(__DATE__,__TIME__));
        is_rtc_connected = true;

      } else {
        Serial.println("RTC connection failed.");
        is_rtc_connected = false;
      }
    }

    bool everyCertainMinutes(int minutes) {
      DateTime current_time = rtc.now();

      if ((current_time.unixtime() - started_time.unixtime() >= minutes * 60)) {
        started_time = rtc.now();
        return true;
      } else {
        return false;
      }
    }

    bool isSpecificTime(int targetHour, int targetMinute) {


      if (is_rtc_connected) {
        DateTime now = rtc.now();
        if (now.hour() == targetHour && now.minute() == targetMinute && current_day != now.day()) {
          current_day = now.day();

          return true;
        } else {
          return false;
        }
      }
    }

  private:
};

APIEndPoint ApiEndpoint;
SaveSensorData DataWriter;
TimeManager Time;
TimeManager Time2;
File root;


class Files {
  private:
    int num_archivos = 0;
    String generalPathToSaveModes = "/modes";

  public:

  std::vector<Asociacion> asociaciones;

  String fileSelected = "";

  String posFile = "/pos.txt";
  String fileName = "/fileName.txt";
  int stepNumber = 0;
  volatile int AUX_STEPS_X = 0;
  volatile int AUX_STEPS_Y = 0;


  volatile int getAUX_STEPS_X() {
    return AUX_STEPS_X;
  }
  volatile int getAUX_STEPS_Y() {
    return AUX_STEPS_Y;
  }

  void saveFileName() {
    File file = SD.open(fileName, FILE_WRITE);

    if (file) {
      file.seek(0);
      file.print(fileSelected);
      Serial.println("Modo guardado: " + fileSelected);
      delay(1000);
      file.close();
      fileSelected = "";
      Serial.println("Contenido del archivo guardado correctamente.");
    } else {
      Serial.println("¡Error al abrir el archivo!");
    }
  }
  String getNameFile() {
    File file = SD.open(fileName, FILE_READ);
    if (file) {
      while (file.available()) {
        fileSelected += (char)file.read();
      }
      file.close();
      Serial.println("Configuración leída correctamente.");
      Serial.println("El modo seleccionado es: " + fileSelected);
      return fileSelected;
    } else {
      Serial.println("Archivo de configuración no encontrado. Usando valores predeterminados.");
      return "";
    }
  }


  void sendFileName(String fileNameString) {
    fileSelected = fileNameString;
  }

  void selectLastFile() {
    if (getNameFile() == "") {  //si no hay un archivo seleccionado, selecciona el último que se guardo
      fileAssociations();
      fileSelected = getFileName(num_archivos - 1);  // Del número de archivos contados se queda con el último

      processData(fileSelected);
    } else {                      //si hay un arhivo seleccionado, obtiene los datos de él
      processData(fileSelected);  //recupera los datos del archivo seleccionado
    }
  }

  void fileAssociations() {
    //se crea una relación de números 1-1 a los nombres de archivos de los programas

    // Abrir el directorio raíz
    File directorio = SD.open(generalPathToSaveModes);

    num_archivos = 0;
    if (directorio) {
      while (true) {
        // Abrir el siguiente archivo
        File archivo = directorio.openNextFile();
        Asociacion aux;

        // Si no hay más archivos, salir del bucle
        if (!archivo) {
          break;
        }

        // Imprimir el nombre del archivo
        //Serial.println(archivo.name());

        // Agregar una nueva asociación al final del vector
        aux = { num_archivos, archivo.name() };
        asociaciones.insert(asociaciones.begin(), aux);

        //asociaciones.push_back({num_archivos, archivo.name()});

        // Cerrar el archivo
        archivo.close();
        num_archivos = num_archivos + 1;
      }

      // Cerrar el directorio
      directorio.close();

      // Iterar sobre las asociaciones y mostrarlas en la consola
      //for (const auto& asociacion : asociaciones) {
      //  lcd.setCursor(contador_1+1, 0);
      //  lcd.print(asociacion.nombreArchivo);
      //}
    } else {
      // Si no se puede abrir el directorio
      Serial.println("Error al abrir el directorio raíz.");
    }
  }


  String getStringName(DateTime fechaHora) {
    String nombreArchivo = "";
    // Construir el nombre del archivo usando los componentes de fecha y hora
    nombreArchivo += String(fechaHora.year(), DEC);
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.month());
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.day());
    nombreArchivo += "_";
    nombreArchivo += dosDigitos(fechaHora.hour());
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.minute());
    nombreArchivo += "-";
    nombreArchivo += dosDigitos(fechaHora.second());

    return nombreArchivo;
  }

  String getFileName(int identificador) {
    for (const auto& asociacion : asociaciones) {
      if (asociacion.identificador == identificador) {
        return asociacion.nombreArchivo;
      }
    }
    // Si no se encuentra ninguna asociación para el identificador dado
    return "";
  }
  int getstepNumber() {
    return stepNumber;
  }
  void processData(String nombreArchivo) {

    if (!SD.exists(generalPathToSaveModes)) {
        SD.mkdir(generalPathToSaveModes);
    }

    File archivo = SD.open(generalPathToSaveModes + "/" + nombreArchivo);

    if (!archivo) {
      Serial.println("Error al abrir el archivo.");
      return;
    }

    Serial.println("Leyendo datos del archivo:");

    // Leer cada línea del archivo
    int contador = 0;
    int aux = 0;
    while (archivo.available()) {
      String linea = archivo.readStringUntil('\n');  // Leer una línea del archivo
      // Extraer los números de la línea
      X[contador] = linea.substring(0, linea.indexOf(' ')).toInt();
      linea.remove(0, linea.indexOf(' ') + 1);  // Eliminar la parte ya leída de la línea
      Y[contador] = linea.substring(0, linea.indexOf(' ')).toInt();
      linea.remove(0, linea.indexOf(' ') + 1);
      MINUTOS[contador] = linea.substring(0, linea.indexOf(' ')).toInt();
      linea.remove(0, linea.indexOf(' ') + 1);
      SEGUNDOS[contador] = linea.toInt();


      /*
        // Imprimir los datos en la consola
        Serial.print("Columna 1: ");
        Serial.print(X[contador]);
        Serial.print(", Columna 2: ");
        Serial.print(Y[contador]);
        Serial.print(", Columna 3: ");
        Serial.print(MINUTOS[contador]);
        Serial.print(", Columna 4: ");
        Serial.println(SEGUNDOS[contador]);
      */
      if (X[contador] == 0 && Y[contador] == 0 && MINUTOS[contador] == 0 && SEGUNDOS[contador] == 0 && aux == 0) {
        stepNumber = contador + 1;
        aux = 1;
      }
      contador = contador + 1;
    }

    archivo.close();  // Cerrar el archivo
  }

  void readPreviousSteps() {  //Lee la posición guardada x y de los motores
    Serial.println("Leyendo configuración desde la tarjeta SD...");

    // Abre el archivo en modo de lectura
    File file = SD.open(posFile);
    if (file) {
      // Lee las posiciones desde el archivo
      AUX_STEPS_X = file.parseInt();
      AUX_STEPS_Y = file.parseInt();

      // Cierra el archivo
      file.close();
      Serial.println("Configuración leída correctamente.");
    } else {
      Serial.println("Archivo de configuración no encontrado. Usando valores predeterminados.");
    }
  }

  void saveStep(int x, int y, int minutos, int segundos, int stepNumber) {
    X[stepNumber] = x;
    Y[stepNumber] = y;
    MINUTOS[stepNumber] = minutos;
    SEGUNDOS[stepNumber] = segundos;
  }

  void cleanVariables(){
    stepNumber = 0;

    // Clean volatile arrays
    memset((void*)X, 0, sizeof(X));
    memset((void*)Y, 0, sizeof(Y));

    // Clean regular arrays
    memset(MINUTOS, 0, sizeof(MINUTOS));
    memset(SEGUNDOS, 0, sizeof(SEGUNDOS));

  }

  void savePos(int AUX_STEPS_X, int AUX_STEPS_Y) {  //Guarda la posición x y de los motores

    // Abre el archivo en modo de lectura y escritura
    File file = SD.open(nombreArchivo, FILE_WRITE);
    if (file) {
      // Posiciona el puntero de escritura al principio del archivo
      file.seek(0);

      // Sobrescribe los valores existentes
      file.print(AUX_STEPS_X);
      file.print('\n');
      file.print(AUX_STEPS_Y);

      // Trunca el archivo para eliminar cualquier contenido adicional
      // Cierra y vuelve a abrir el archivo en modo de escritura para truncar
      file.close();

    } else {
      Serial.println("Error al abrir el archivo para guardar configuración.");
    }
  }


  void saveMode() {
    DateTime inicio = rtc.now();
    fileSelected = getStringName(inicio) + ".txt";


    //Verify folder existance
    if (!SD.exists(generalPathToSaveModes)) {
        SD.mkdir(generalPathToSaveModes);
    }

    // Abrir el archivo en modo escritura
    File archivo = SD.open(generalPathToSaveModes + "/" + fileSelected, FILE_WRITE);
    if (archivo) {
      // Escribir cada elemento del arreglo en el archivo
      for (int i = 0; i < sizeof(X) / sizeof(X[0]); i++) {
        archivo.print(X[i]);
        archivo.print(' ');  // Nueva línea
        archivo.print(Y[i]);
        archivo.print(' ');  // Nueva línea
        archivo.print(MINUTOS[i]);
        archivo.print(' ');  // Nueva línea
        archivo.print(SEGUNDOS[i]);
        archivo.print('\n');  // Nueva línea
      }

      archivo.close();  // Cerrar el archivo
      lcd.clear();
      lcd.setCursor(2, 1);
      lcd.print("GUARDADO EXITOSO");
    } else {
      // Si no se pudo abrir el archivo
      lcd.clear();
      lcd.setCursor(2, 1);
      lcd.print("ERROR AL GUARDAR.");
    }
    delay(2000);

    lcd.clear();
  }


  String dosDigitos(int numero) {
    if (numero < 10) {
      return "0" + String(numero);
    } else {
      return String(numero);
    }
  }
};

class Encoder {
  private:
    unsigned long ultimoTiempo = 0;
    unsigned long debounceDelay = 300;  // Tiempo de debounce en milisegundos

  public:
  int aux = 2;
  bool bol = 0;
  bool aux_submenu = 0;
  volatile int AUX_STEPS_X = 0;
  volatile int AUX_POS_A = 0;
  volatile int AUX_STEPS_Y = 0;
  volatile int POS_A = 0;  // variable POS_A con valor inicial de 50 y definida
  volatile int POS_B = 0;
  volatile int AUX_POS_A2 = 0;
  volatile int STEPSX = 0;
  volatile int STEPSY = 0;
  //Counters for movements of stepper motors

  int AUX_STEPSX;
  int AUX_STEPSY;


  bool PUSH_A = 0;
  bool PUSH_B = 0;
  volatile int limit_POS_A;
  volatile int limit_POS_B;


  void setEncoderToZero() {
    POS_A = 0;
    STEPSX = 0;
    STEPSY = 0;
    POS_B = 0;
    AUX_POS_A = 0;
  }
  void sendPUSH_A(int value) {
    PUSH_A = value;
  }
  void sendPUSH_B(int value) {
    PUSH_B = value;
  }
  void sendSTEPSX(volatile int value) {
    STEPSX = value;
  }
  void sendSTEPSY(volatile int value) {
    STEPSY = value;
  }
  void sendSTEPSY() {
  }

  void restoreData(volatile int value1, volatile int value2, volatile int value3, volatile int value4) {
    POS_A = value1;
    POS_B = value2;
    STEPSX = value3;
    STEPSY = value4;
  }

  int getSTEPSX() {
    return STEPSX;
  }
  int getSTEPSY() {
    return STEPSY;
  }
  bool getPUSH_B() {
    return PUSH_B;
  }
  bool getPUSH_A() {
    return PUSH_A;
  }
  void savePOS_A(volatile int aux) {
    POS_A = aux;
  }
  void saveAUX_POS_A(volatile int aux) {
    AUX_POS_A = aux;
  }

  void savelimit_POS_A(volatile int aux) {
    limit_POS_A = aux;
  }
  void savelimit_POS_B(volatile int aux) {
    limit_POS_B = aux;
  }
  volatile int getPOS_A() {
    return POS_A;
  }
  volatile int getPOS_B() {
    return POS_B;
  }
  void eraseValues() {
    bol = 0;
    POS_A = 0;
    AUX_POS_A = 0;
  }
  bool getBol() {
    return bol;
  }
  volatile int getAUX_POS_A() {
    return AUX_POS_A;
  }

  void encoder1() {
    static unsigned long ultimaInterrupcion = 0;  // variable static con ultimo valor de // tiempo de interrupcion
    unsigned long tiempoInterrupcion = millis();  // variable almacena valor de func. millis

    if (tiempoInterrupcion - ultimaInterrupcion > 5) {  // rutina antirebote desestima  // pulsos menores a 5 mseg.
      if (digitalRead(DT_A) == HIGH)                    // si B es HIGH, sentido horario
      {
        POS_A++;  // incrementa POS_A en 1
        STEPSX = STEPSX + GENERAL_STEP_X;
      } else {    // si B es LOW, senti anti horario
        POS_A--;  // decrementa POS_A en 1
        STEPSX = STEPSX - GENERAL_STEP_X;
      }

      POS_A = min(limit_POS_A, max(0, POS_A));
      STEPSX = min(15000, max(0, STEPSX));  // establece limite inferior de 0 y

      ultimaInterrupcion = tiempoInterrupcion;  // guarda valor actualizado del tiempo
    }
  }
  void encoder2() {
    static unsigned long ultimaInterrupcion = 0;  // variable static con ultimo valor de // tiempo de interrupcion
    unsigned long tiempoInterrupcion = millis();  // variable almacena valor de func. millis

    if (tiempoInterrupcion - ultimaInterrupcion > 5) {  // rutina antirebote desestima // pulsos menores a 5 mseg.
      if (digitalRead(DT_B) == HIGH)                    // si B es HIGH, sentido horario
      {
        POS_B++;  // incrementa POS_A en 1
        STEPSY = STEPSY + GENERAL_STEP_Y;
      } else {    // si B es LOW, senti anti horario
        POS_B--;  // decrementa POS_A en 1
        STEPSY = STEPSY - GENERAL_STEP_Y;
      }


      POS_B = min(limit_POS_B, max(0, POS_B));  // establece limite inferior de 0 y
      STEPSY = min(10000, max(0, STEPSY));      // establece limite inferior de 0 y

      //savesteps();
      // superior de 100 para POS_A
      ultimaInterrupcion = tiempoInterrupcion;  // guarda valor actualizado del tiempo


    }  // de la interrupcion en variable static
  }

  void push_a() {
    //Variable la cual hará que en determinado menú, nos regresemos al AUX_PRINT_A
    if (millis() - ultimoTiempo > debounceDelay) {
      // Actualiza el tiempo del último cambio del botón
      ultimoTiempo = millis();
      if (digitalRead(BUTTON_B) == LOW) {
        Serial.println("FUNCIONA_B");
        PUSH_B = 1;

        // Espera hasta que pase el intervalo
      }
    }
  }




  void push_b() {

    if (millis() - ultimoTiempo > debounceDelay) {
      // Actualiza el tiempo del último cambio del botón
      ultimoTiempo = millis();
      if (digitalRead(BUTTON_A) == LOW) {
        Serial.println("FUNCIONA_A");
        PUSH_A = 1;
      }
    }
  }

  int max(int num1, int num2) {
    if (num1 > num2) {
      return num1;
    } else {
      return num2;
    }
  }

  int min(int num1, int num2) {
    if (num1 > num2) {
      return num2;
    } else {
      return num1;
    }
  }
};

Encoder Encoders;


class Values {
  public:
  int STEPSX, STEPSY;
  int POS_A, POS_B;

  virtual void saveData(Encoder& EncoderObject) {
  }
  virtual void restoreData(Encoder& EncoderObject) {
  }
};




class MotorMovement : public Values {
  private:
  public:
  volatile int POS_A;
  volatile int POS_B;
  volatile int STEPSX;
  volatile int STEPSY;

  volatile int getPOS_A() {
    return POS_A;
  }
  volatile int getPOS_B() {
    return POS_B;
  }
  volatile int getSTEPSX() {
    return STEPSX;
  }
  volatile int getSTEPSY() {
    return STEPSY;
  }

  void saveData(Encoder& EncoderObject) override {
    POS_A = EncoderObject.getPOS_A();
    POS_B = EncoderObject.getPOS_B();
    STEPSX = EncoderObject.getSTEPSX();
    STEPSY = EncoderObject.getSTEPSY();
  }

  void restoreData(Encoder& EncoderObject) override {
    EncoderObject.restoreData(POS_A, POS_B, STEPSX, STEPSY);
  }

  void stepMotor_y(int mm, int intervalUs) {
    int steps = mm * FACTOR_MOVEMENT;
    for (int i = 0; i < steps; i++) {
      digitalWrite(PULY, HIGH);
      delayMicroseconds(10);  // pulso seguro
      digitalWrite(PULY, LOW);
      delayMicroseconds(intervalUs);
    }
  }

  void stepMotor_x(int mm, int intervalUs) {
    int steps = mm * FACTOR_MOVEMENT;
    for (int i = 0; i < steps; i++) {
      digitalWrite(PULX, HIGH);
      delayMicroseconds(10);  // pulso seguro
      digitalWrite(PULX, LOW);
      delayMicroseconds(intervalUs);
    }
  }

  void moveFromTo(Files& FilesObject, int AUX_STEPS_X, int AUX_STEPS_Y, int STEPSX, int STEPSY) {
    //Movimiento de motores y

    while (AUX_STEPS_Y < STEPSY) {
      digitalWrite(DIRY, HIGH);
      stepMotor_y(GENERAL_STEP_Y, 150);
      AUX_STEPS_Y += GENERAL_STEP_Y;

      if (is_process_in_execution) {
        DataWriter.create("hour");
        printDriverYInfo("Driver 1 Info");
        DataWriter.close();
      }
    }

    while (AUX_STEPS_Y > STEPSY) {
      digitalWrite(DIRY, LOW);
      stepMotor_y(GENERAL_STEP_Y, 150);
      AUX_STEPS_Y -= GENERAL_STEP_Y;

      if (is_process_in_execution) {
        DataWriter.create("hour");
        printDriverYInfo("Driver 1 Info");
        DataWriter.close();
      }
    }
    while (AUX_STEPS_X < STEPSX) {
      digitalWrite(DIRX, HIGH);
      stepMotor_x(GENERAL_STEP_X, 150);
      AUX_STEPS_X += GENERAL_STEP_X;

      if (is_process_in_execution) {
        DataWriter.create("hour");
        printDriverXInfo("Driver 2 Info");
        DataWriter.close();
      }
    }

    while (AUX_STEPS_X > STEPSX) {
      digitalWrite(DIRX, LOW);
      stepMotor_x(GENERAL_STEP_X, 150);
      AUX_STEPS_X -= GENERAL_STEP_X;

      if (is_process_in_execution) {
        DataWriter.create("hour");
        printDriverXInfo("Driver 2 Info");
        DataWriter.close();
      }
    }

    FilesObject.savePos(AUX_STEPS_X, AUX_STEPS_Y);
  }
};

//------Classes refer to LCD options------

class ILCDBaseNavigation {
  public:
  volatile int POS_A;      // variable POS_A con valor inicial de 50 y definida
  volatile int AUX_POS_A;  //almacena el valor de pos_A
  bool aux_PUSH_A;
  bool aux_PUSH_B;
  volatile int OptionNumber;     //Me dice cuantas opciones estará desplegando
  volatile int OptionSelection;  //Me dirá que opción está eligiendo el usuario
  volatile int currentOption = 0;
  bool autoScreenOut;
  bool aux;  //Indica que está en ejecución el ciclo


  ILCDBaseNavigation()
    : OptionSelection(-1), aux(1) {}

  virtual void Refresh(Encoder& EncoderObject) {
    Serial.print("Funciona");
  }

  virtual void Refresh(int i, int j) {
    Serial.print("Funciona");
  }

  virtual void RefreshTwo() {
    Serial.print("Funciona");
  }

  bool getScreenStatus() {
    return aux;
  }

  void setAutoScreenOut(bool bol){
    autoScreenOut = bol;
  }

  void initializeScreen(Encoder& EncoderObject) {
    //Initialize screen giving everything the 0 value
    EncoderObject.setEncoderToZero();
    OptionSelection = -1;
    currentOption = 0;
    aux = 1;

    if (autoScreenOut) {
      startTimeForOut();
    }
  }

  void returnToPreviousScreen(Encoder& EncoderObject) {
    EncoderObject.setEncoderToZero();
    aux = 0;
    EncoderObject.sendPUSH_B(0);
    aux_PUSH_B = 0;
    currentOption = 0;

    lcd.clear();
  }


  void checkTimeForOut(Encoder& EncoderObject) {
    DateTime finalTime = startTime + TimeSpan(0, 0, 1, 0);  // Calcula el momento en que terminará el período
    if (rtc.now() >= finalTime) {
      EncoderObject.setEncoderToZero();
      aux = 0;
      EncoderObject.sendPUSH_B(0);
      aux_PUSH_B = 0;
      currentOption = 0;
      startTimeForOut();
      lcd.clear();
    }
  }


  virtual void verifyScreenExit(Encoder& EncoderObject, Files& FilesObject) {
  }

  virtual void verifyScreenExit(Encoder& EncoderObject) {
    aux_PUSH_B = EncoderObject.getPUSH_B();  //Optine el valor

    if (autoScreenOut){
      checkTimeForOut(EncoderObject);
    }

    while (aux_PUSH_B == 1) {
      EncoderObject.setEncoderToZero();
      aux = 0;
      EncoderObject.sendPUSH_B(0);
      aux_PUSH_B = 0;
      currentOption = 0;

      lcd.clear();
    }
  }
  void obtainEncoderButtomStatus(Encoder& EncoderObject, int predefinedValue = -1) {
    aux_PUSH_A = EncoderObject.getPUSH_A();
    while (aux_PUSH_A == 1) {
      if (predefinedValue == -1) {
        OptionSelection = currentOption;
      } else {
        OptionSelection = predefinedValue;
      }
      EncoderObject.sendPUSH_A(0);
      aux_PUSH_A = 0;
      lcd.clear();
    }
  }

  String dosDigitos(int numero) {
    if (numero < 10) {
      return "0" + String(numero);
    } else {
      return String(numero);
    }
  }




  void printValues() {
    Serial.println("POS_A: " + String(POS_A) + " AUX_POS_A: " + String(AUX_POS_A));
  }
};

class LCDRefreshRunMode : public ILCDBaseNavigation {
  private:
  public:
  DateTime fecha;
  //LCDRefreshRunMode(int option_number) : ILCDBaseNavigation(option_number) {}

  void startClock() {
    fecha = rtc.now();  // Momento en que comienza el período
  }
  void Refresh(int i, int j) override {

    DateTime inicio = rtc.now();                                      // Momento en que comienza el período
    DateTime fin = inicio + TimeSpan(0, 0, MINUTOS[j], SEGUNDOS[j]);  // Calcula el momento en que terminará el período
    while (rtc.now() <= fin) {
      DateTime ahora = rtc.now();
      TimeSpan Transcurrido = ahora - inicio;
      TimeSpan Tiempo_total = ahora - fecha;

      //Impresión del paso en el que va
      lcd.setCursor(7, 1);
      lcd.print(String(j));

      //Impresión del número de la capa en la que va
      lcd.setCursor(16, 1);
      lcd.print(String(i));

      //impresión del tiempo del paso
      lcd.setCursor(9, 2);
      lcd.print(dosDigitos(Transcurrido.minutes()));  // funcion millis() / 1000 para segundos transcurridos
      lcd.print(":");
      lcd.print(dosDigitos(Transcurrido.seconds()));  // funcion millis() / 1000 para segundos transcurridos
      lcd.print("  ");

      //impresión del tiempo total desde el inicio del ciclo
      lcd.setCursor(10, 3);
      lcd.print(dosDigitos(Tiempo_total.minutes()));
      lcd.print(":");
      lcd.print(dosDigitos(Tiempo_total.seconds()));
      lcd.print("  ");
    }
  }

  void inter() {
    lcd.setCursor(1, 0);
    lcd.print("*PROCESO EN CURSO*");
    lcd.setCursor(1, 1);
    lcd.print("PASO: ");

    lcd.setCursor(11, 1);
    lcd.print("CAPA: ");

    lcd.setCursor(1, 2);
    lcd.print("T PASO: ");

    lcd.setCursor(1, 3);
    lcd.print("T TOTAL: ");
  }
};

class LCDLineRefresh : public ILCDBaseNavigation {
  private:
  public:
    std::vector<Asociacion> palabras;
    int currentPage = 0;
    int auxiliar;
    int numeroElementos;
    std::size_t variable;
    
    //Esta función inserta el listado de opciones
    void OptionNames(const std::vector<Asociacion>& lista_palabras) {
        palabras = lista_palabras;
        variable = palabras.size();
        numeroElementos = static_cast<int>(variable);
    }

    void lineRefresh(Encoder& EncoderObject) {
      POS_A = EncoderObject.getPOS_A();
      AUX_POS_A = EncoderObject.getAUX_POS_A();

      //Este código nos dice en que página estamos del listado de opciones

      if (POS_A > 20 * (currentPage + 1) && POS_A < 20 * (currentPage + 2)) {
        currentPage = currentPage + 1;
        lcd.clear();
      } else if (POS_A >= 20 * (currentPage - 1) && POS_A < 20 * currentPage) {
        currentPage = currentPage - 1;
        lcd.clear();
      }

      Serial.println("CurrentPage:" + String(currentPage));

      if (POS_A != 0) {
        if (POS_A < 5 * (currentOption + 2) && POS_A > 5 * (currentOption + 1)) {
          currentOption = currentOption + 1;
        } else if (POS_A < 5 * currentOption && POS_A >= 5 * (currentOption - 1)) {
          currentOption = currentOption - 1;
        }
      }


      Serial.println("CurrentOption: " + String(currentOption));
      //Serial.println(POS_A);


      int contador = 0;
      int total_pages = numeroElementos / 4;
      int limit = currentPage * 4 + 4;

      for (int i = currentPage * 4; i < min(limit, numeroElementos); i++) {
        lcd.setCursor(1, contador);
        lcd.print(palabras[i].nombreArchivo.substring(0, 16));
        contador = contador + 1;
        if (contador > 4) {
          contador = 0;
        }
      }

      if (numeroElementos > 4) {
        if (currentPage == total_pages) {
          if (numeroElementos % 4 == 0) {
            auxiliar = 4;
          } else if (POS_A % 4 == 1){
            auxiliar = 1;
          } else if (POS_A % 4 == 2){
            auxiliar = 2;
          } else if (POS_A % 4 == 3){
            auxiliar = 3;
          }
          
        } else {
          auxiliar = 4;
        }
      } else if (numeroElementos <= 4) {
        auxiliar = numeroElementos;
      }

      Serial.println(POS_A);

      //Establece la cota superior de POS_A
      EncoderObject.savelimit_POS_A(numeroElementos * 5);

      if (currentOption % 4 == 0){
            lcd.setCursor(0, 0);
            lcd.print("-");
            lcd.setCursor(0, 1);
            lcd.print(" ");
            lcd.setCursor(0, 2);
            lcd.print(" ");
            lcd.setCursor(0, 3);
            lcd.print(" ");
      } else if (currentOption % 4 == 1){
            lcd.setCursor(0, 0);
            lcd.print(" ");
            lcd.setCursor(0, 1);
            lcd.print("-");
            lcd.setCursor(0, 2);
            lcd.print(" ");
            lcd.setCursor(0, 3);
            lcd.print(" ");
      }else if (currentOption % 4 == 2){
            lcd.setCursor(0, 0);
            lcd.print(" ");
            lcd.setCursor(0, 1);
            lcd.print(" ");
            lcd.setCursor(0, 2);
            lcd.print("-");
            lcd.setCursor(0, 3);
            lcd.print(" ");

      }else if (currentOption % 4 == 3){
            lcd.setCursor(0, 0);
            lcd.print(" ");
            lcd.setCursor(0, 1);
            lcd.print(" ");
            lcd.setCursor(0, 2);
            lcd.print(" ");
            lcd.setCursor(0, 3);
            lcd.print("-");
      }

  }


  int max(int num1, int num2) {
    if (num1 > num2) {
      return num1;
    } else {
      return num2;
    }
  }

  int min(int num1, int num2) {
    if (num1 > num2) {
      return num2;
    } else {
      return num1;
    }
  }
};



class LCDInitialMenu : public LCDLineRefresh {
  public:
    void Refresh(Encoder& EncoderObject) override {
      DateTime fecha = rtc.now();
      lcd.setCursor(0, 3);  // ubica cursor en columna 0 y linea 1
      lcd.print("HORA: ");
      lcd.print(dosDigitos(fecha.hour()));  // funcion millis() / 1000 para segundos transcurridos
      lcd.print(":");
      lcd.print(dosDigitos(fecha.minute()));  // funcion millis() / 1000 para segundos transcurridos
      lcd.print(":");
      lcd.print(dosDigitos(fecha.second()));  // funcion millis() / 1000 para segundos transcurridos
    }
};

class LCDsetPassword : public ILCDBaseNavigation{
  public:
    int numeroElementos = charsetSize;
    int currentPage = 0;
    int auxiliar;
    String password = "";

    void lineRefresh(Encoder& EncoderObject) {
      char caracter;
      EncoderObject.savelimit_POS_A(numeroElementos);
      
      POS_A = EncoderObject.getPOS_A();
      caracter = printCurrentCharacter(POS_A);

      Serial.println(POS_A);
      Serial.println(caracter);

      lcd.setCursor(1, 1);
      lcd.print("SELECT CHAR: '"+String(caracter)+"'");
      lcd.setCursor(1, 2);
      lcd.print(password);

    }



    void saveSelectedOption(){
      password += printCurrentCharacter(POS_A);
      Serial.println(password);  
    }

    void saveSelectedNetwork(String aux_network){
      network = aux_network;
    }

    void saveSelectedPassword(String aux_password){
      password = aux_password;
    }

    void saveCredentialsAsDefault() {

      String path = folderNetwork + "/current_network.txt";

      SD.remove(path);   // elimina archivo anterior

      File file = SD.open(path, FILE_WRITE);

      if (file) {

        file.println(network);
        file.println(password);

        file.close();

        password = ""; //restart variable
        Serial.println("Red actual guardada correctamente.");

      } else {
        Serial.println("Error al guardar la red actual.");
      }
    }

    String getPasswordByNetwork(String network) {

      String path = folderNetwork + "/" + network + ".txt";

      File file = SD.open(path, FILE_READ);

      if (!file) {
        Serial.println("No se pudo abrir el archivo de la red.");
        return "";
      }

      String savedNetwork = file.readStringUntil('\n');
      savedNetwork.trim();

      String password = file.readStringUntil('\n');
      password.trim();

      file.close();

      if (savedNetwork != network) {
        Serial.println("El nombre de la red no coincide.");
        return "";
      }

      return password;
    }

    void saveCredentials() {
      
      bool WIFIValidation = ApiEndpoint.establishConnection(network, password);

      if (WIFIValidation){
        if (!SD.exists(folderNetwork)) {
          SD.mkdir(folderNetwork);
        }

        String path = folderNetwork + "/" + network + ".txt";

        // Mejor abrir en FILE_WRITE pero recreando el archivo si ya existía
        SD.remove(path);  
        File file = SD.open(path, FILE_WRITE);

        if (file) {
          file.println(network);
          file.println(password);
          file.close();

          saveCredentialsAsDefault();

          Serial.println("Credenciales guardadas correctamente.");
        } else {
          Serial.println("Error al abrir el archivo para guardar credenciales.");
        }
      }
    }
     
};

class LCDRunMode : public ILCDBaseNavigation {
  private:
  public:
    int layerNumber;

    //LCDRunMode (int option_number) : ILCDBaseNavigation(option_number){}

    void Refresh(Encoder& EncoderObject) override {
      int POS_A = EncoderObject.getPOS_A();
      lcd.setCursor(2, 1);
      lcd.print("NUMERO DE CAPAS:");
      lcd.setCursor(0, 2);
      lcd.print("         ");
      lcd.setCursor(0, 2);
      lcd.print("         ");
      lcd.print(POS_A);
      lcd.print("    ");
    }
    void saveLayerNumber(Encoder& EncoderObject) {
      POS_A = EncoderObject.getPOS_A();
      layerNumber = POS_A;
    }
    int getlayerNumber() {
      return layerNumber;
    }
};


Files SD_Files;

class LCDNewModeSteps : public ILCDBaseNavigation {
  private:
  public:
  int POS_A;
  int POS_B;
  int stepNumber = 0;
  void Refresh(Encoder& EncoderObject) override {
    POS_A = EncoderObject.getPOS_A();
    POS_B = EncoderObject.getPOS_B();

    lcd.setCursor(4, 0);
    lcd.print("*NUEVO MODO*");
    lcd.setCursor(1, 1);
    lcd.print("PASO: ");
    lcd.print(stepNumber);
    lcd.setCursor(1, 2);
    lcd.print("EJEX (0-" + String(TOTAL_STEPS_MOVEMENT) + "): ");
    lcd.print(POS_A);
    lcd.print("  ");
    lcd.setCursor(1, 3);
    lcd.print("EJEY (0-" + String(TOTAL_STEPS_MOVEMENT) + "): ");
    lcd.print(POS_B);
    lcd.print("  ");
  }

  void verifyScreenExit(Encoder& EncoderObject, Files& FilesObject) override {
    LCDLineRefresh LCD_LRSaveModeYesNo;
    std::vector<Asociacion> c6;

    aux_PUSH_B = EncoderObject.getPUSH_B();  //Obtine el valor

    if (autoScreenOut){
      checkTimeForOut(EncoderObject);
    }

    while (aux_PUSH_B == 1) {

    c6 = {{0, "GUARDAR"},{1, "CANCELAR"}};                         
    LCD_LRSaveModeYesNo.setAutoScreenOut(false);
    LCD_LRSaveModeYesNo.initializeScreen(Encoders);
    LCD_LRSaveModeYesNo.OptionNames(c6);  // Give class the option names
    while(LCD_LRSaveModeYesNo.getScreenStatus()){
      switch (LCD_LRSaveModeYesNo.OptionSelection){
        case 0:
          EncoderObject.setEncoderToZero();
          aux = 0;
          EncoderObject.sendPUSH_B(0);
          aux_PUSH_B = 0;
          currentOption = 0;

          FilesObject.saveMode();
          FilesObject.saveFileName();
          FilesObject.cleanVariables();
          stepNumber = 0;
          
          SD_Files.selectLastFile();  //Hace lo necesario para recopilar los datos del último archivo

          lcd.clear();
          LCD_LRSaveModeYesNo.returnToPreviousScreen(Encoders);

          LCD_LRSaveModeYesNo.OptionSelection = -1;
        break;
        case 1:
          EncoderObject.setEncoderToZero();
          aux = 0;
          EncoderObject.sendPUSH_B(0);
          aux_PUSH_B = 0;
          currentOption = 0;
          
          FilesObject.cleanVariables();
          stepNumber = 0;

          LCD_LRSaveModeYesNo.returnToPreviousScreen(Encoders);
          LCD_LRSaveModeYesNo.OptionSelection = -1;
        break;
        default:
          LCD_LRSaveModeYesNo.lineRefresh(Encoders);
          LCD_LRSaveModeYesNo.obtainEncoderButtomStatus(Encoders);
          LCD_LRSaveModeYesNo.verifyScreenExit(Encoders);

      }
    }
  }
  }

};

class LCDNewModeTime : public ILCDBaseNavigation {
  public:
  int POS_A;
  int POS_B;

  void Refresh(Encoder& EncoderObject) override {
    POS_A = EncoderObject.getPOS_A();
    POS_B = EncoderObject.getPOS_B();

    lcd.setCursor(1, 0);
    lcd.print("*TIEMPO DEL PASO*");


    lcd.setCursor(1, 2);
    lcd.print("MINUTOS: ");
    lcd.print(POS_A);
    lcd.print("  ");
    lcd.setCursor(1, 3);
    lcd.print("SEGUNDOS: ");
    lcd.print(POS_B);
    lcd.print("  ");
  }
};

//------Classes refer to LCD options------



void setup() {
  Serial.begin(115200);
  delay(1000);

  lcd.setBacklight(3);  // puerto P3 de PCF8574 como positivo
  lcd.init(21, 22);                  // 16 columnas por 2 lineas para LCD 1602A
	lcd.backlight();
  lcd.clear();


  //Encoder Pines
  pinMode(CLK_A, INPUT_PULLUP);
  pinMode(DT_A, INPUT_PULLUP);
  pinMode(CLK_B, INPUT_PULLUP);
  pinMode(DT_B, INPUT_PULLUP);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);

  //Stepper motors Pines
  pinMode(DIRX, OUTPUT);
  pinMode(DIRY, OUTPUT);
  pinMode(PULX, OUTPUT);
  pinMode(PULY, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_B), push_a, FALLING);  // interrupcion sobre pin A con
  attachInterrupt(digitalPinToInterrupt(BUTTON_A), push_b, FALLING);  // interrupcion sobre pin A con
  attachInterrupt(digitalPinToInterrupt(CLK_A), encoder1, FALLING);   // interrupcion sobre pin A con
  attachInterrupt(digitalPinToInterrupt(CLK_B), encoder2, FALLING);   // interrupcion sobre pin A con

  if (!rtc.begin()) {
    Serial.println("¡Modulo RTC no encontrado!");
    while (1)
      ;
  }
  if (!SD.begin(sd_chip_select)) {
    Serial.println("La inicialización de la tarjeta SD falló. Verifique la tarjeta SD en el ESP32 o Arduino.");
    //Impresión del paso en el que va
    lcd.setCursor(1, 1);
    lcd.print("SD card not found!");
    lcd.setCursor(1, 2);
    lcd.print("connect and restart");

    while (1)
      ;
  }

  // --------API CONFIGURATION--------

  DataWriter.establishConnection();
  Time.establishConnection();

  // Serial.println("----- CONNECTION STATUS -----");
  // Serial.print("WiFi: ");
  // Serial.println(is_wifi_connected ? "CONNECTED" : "DISCONNECTED");
  // Serial.print("SD Card: ");
  // Serial.println(is_sd_connected ? "CONNECTED" : "DISCONNECTED");
  // Serial.print("RTC: ");
  // Serial.println(is_rtc_connected ? "CONNECTED" : "DISCONNECTED");
  // Serial.println("-----------------------------");



  // clearDirectory("/");
  // clearDirectory("/modes");
  // clearDirectory("/iot/saved");

  // delay(10000);

  Serial.println("DIRECTORIO /iot/daily");
  root = SD.open("/iot/daily");
  // DataWriter.deleteOnlyFilesInDirectory("/iot/daily");
  printDirectory(root, 0);


  delay(1000);

  Serial.println("DIRECTORIO /iot/saved");
  root = SD.open("/iot/saved");
  // DataWriter.deleteOnlyFilesInDirectory("/iot/saved");
  printDirectory(root, 0);

  // --------API CONFIGURATION--------

  //Drivers configuration
  TMCSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);

  // Driver initialization
  driver1.begin();
  driver2.begin();

  // UART recomendado
  driver1.pdn_disable(true);  // usar PDN_UART como UART
  driver2.pdn_disable(true);  // usar PDN_UART como UART

  driver1.I_scale_analog(false);  // no depender del pot
  driver2.I_scale_analog(false);  // no depender del pot

  // Chopper ON (si toff=0 no mueve)
  driver1.toff(3);
  driver2.toff(3);

  // Corriente: tu motor es 0.7A -> empieza seguro
  driver1.rms_current(500);  // sube/baja luego (600–700 recomendado)
  driver2.rms_current(500);  // sube/baja luego (600–700 recomendado)

  driver1.microsteps(current_usteps);
  driver2.microsteps(current_usteps);

  // Lee una vez
  Serial.print("IFCNT: ");
  Serial.println(driver1.IFCNT());
  Serial.print("GSTAT: 0x");
  Serial.println(driver1.GSTAT(), HEX);
  Serial.print("IOIN: 0x");
  Serial.println(driver1.IOIN(), HEX);
  Serial.print("DRV_STATUS: 0x");
  Serial.println(driver1.DRV_STATUS(), HEX);
  Serial.print("cs_actual: ");
  Serial.println(driver1.cs_actual());

  Serial.print("IFCNT: ");
  Serial.println(driver2.IFCNT());
  Serial.print("GSTAT: 0x");
  Serial.println(driver2.GSTAT(), HEX);
  Serial.print("IOIN: 0x");
  Serial.println(driver2.IOIN(), HEX);
  Serial.print("DRV_STATUS: 0x");
  Serial.println(driver2.DRV_STATUS(), HEX);
  Serial.print("cs_actual: ");
  Serial.println(driver2.cs_actual());

  Serial.println("=== END SETUP READ ===");
}

void loop() {
  //Names of the options
  std::vector<Asociacion> c1;
  std::vector<Asociacion> c2;
  std::vector<Asociacion> c5;

  LCDLineRefresh LCD_LRSettings;
  LCDLineRefresh LCD_LRChangeMode;
  LCDLineRefresh LCD_LRChangeNetwork;
  LCDLineRefresh LCD_LRFinalDecision;
  LCDLineRefresh LCD_LRChangeNetworkMenu;
  LCDLineRefresh LCD_LRChangeSaveNetwork;
  LCDsetPassword LCD_SetPassword;

  LCDInitialMenu LCD_StartMenu;
  LCDRunMode LCD_RunMode;
  LCDRefreshRunMode LCD_RefreshRunMode;
  LCDNewModeSteps LCD_NewModeSteps;
  LCDNewModeTime LCD_NewModeTime;

  MotorMovement Motors;




  //alueRefresh valueRefresh(0);
  //ILCDBaseNavigation* LCD_RunMode = &valueRefresh;

  c1 = { { 0, "CORRER PASOS" }, { 1, "CONFIGURACION" } };
  LCD_StartMenu.OptionNames(c1);
  LCD_StartMenu.setAutoScreenOut(false);
  LCD_StartMenu.initializeScreen(Encoders);
  while (LCD_StartMenu.getScreenStatus()) {
    
    switch (LCD_StartMenu.OptionSelection) {
      case 0:  //Opcion Iniciar ciclo
        LCD_RunMode.setAutoScreenOut(true);
        LCD_RunMode.initializeScreen(Encoders);
        Encoders.savelimit_POS_A(100);
        Encoders.savelimit_POS_B(100);
        while (LCD_RunMode.getScreenStatus()) {

          switch (LCD_RunMode.OptionSelection) {
            case 0:
              LCD_RefreshRunMode.setAutoScreenOut(false);
              LCD_RefreshRunMode.initializeScreen(Encoders);
              while (LCD_RefreshRunMode.getScreenStatus()) {
                is_process_in_execution = true;
                LCD_RefreshRunMode.inter();
                LCD_RefreshRunMode.startClock();  //Empieza a medir el tiempo inicial del proceso

                SD_Files.selectLastFile();  //Hace lo necesario para recopilar los datos del último archivo

                for (int i = 0; i < LCD_RunMode.getlayerNumber(); i++) {
                  for (int j = 0; j < SD_Files.getstepNumber()-1; j++) {
                    volatile int STEPS_X = X[j] * GENERAL_STEP_X;
                    volatile int STEPS_Y = Y[j] * GENERAL_STEP_Y;

                    
                    SD_Files.readPreviousSteps();
                    Motors.moveFromTo(SD_Files, SD_Files.getAUX_STEPS_X(), SD_Files.getAUX_STEPS_Y(), STEPS_X, STEPS_Y);
                    Serial.println("PASOS X: " + String(STEPS_X));
                    Serial.println("PASOS Y: " + String(STEPS_Y));

                    LCD_RefreshRunMode.Refresh(i, j);  //Empezará a contar el tiempo
                  }
                }
                LCD_RefreshRunMode.verifyScreenExit(Encoders);
                LCD_RefreshRunMode.returnToPreviousScreen(Encoders);
              }
              LCD_RunMode.OptionSelection = -1;

              break;
            default:
              LCD_RunMode.Refresh(Encoders);                    //Imprime los valores actualizados en la pantalla
              LCD_RunMode.saveLayerNumber(Encoders);            //Guarda la opción seleccionada
              LCD_RunMode.obtainEncoderButtomStatus(Encoders);  //Verifica si el usuario presiono el encoder
              LCD_RunMode.verifyScreenExit(Encoders);  //Configura la opción de salida del while
          }
        }
        LCD_StartMenu.OptionSelection = -1;
        LCD_StartMenu.currentOption = 0;

        break;

      case 1:  //Menú 2 de configuración
        LCD_LRSettings.setAutoScreenOut(true);
        LCD_LRSettings.initializeScreen(Encoders);
        while (LCD_LRSettings.getScreenStatus()) {
          c2 = { { 0, "CAMBIAR MODO" }, { 1, "MODO NUEVO" }, { 2, "ENVIAR INFO" }, { 3, "MODIFICAR RED" } };
          switch (LCD_LRSettings.OptionSelection) {
            case 0:
              LCD_LRChangeMode.setAutoScreenOut(true);
              LCD_LRChangeMode.initializeScreen(Encoders);
              SD_Files.fileAssociations();                          // Extract modes from memory
              LCD_LRChangeMode.OptionNames(SD_Files.asociaciones);  // Give class the option names
              while (LCD_LRChangeMode.getScreenStatus()) {
                switch (LCD_LRChangeMode.OptionSelection) {
                  case 0:
                    SD_Files.sendFileName(SD_Files.asociaciones[LCD_LRChangeMode.currentOption].nombreArchivo);
                    SD_Files.saveFileName();
                    LCD_LRChangeMode.OptionSelection = -1;
                    // LCD_LRChangeMode.currentOption = 0;
                    break;

                  default:
                    LCD_LRChangeMode.lineRefresh(Encoders);
                    LCD_LRChangeMode.obtainEncoderButtomStatus(Encoders, 0);
                    LCD_LRChangeMode.verifyScreenExit(Encoders);
                }
              }

              LCD_LRSettings.OptionSelection = -1;
              LCD_LRSettings.currentOption = 0;

              break;



            case 1:
              LCD_NewModeSteps.setAutoScreenOut(false);
              LCD_NewModeSteps.initializeScreen(Encoders);
              Encoders.savelimit_POS_A(TOTAL_STEPS_MOVEMENT);
              Encoders.savelimit_POS_B(TOTAL_STEPS_MOVEMENT);
              is_process_in_execution = false;

              while (LCD_NewModeSteps.getScreenStatus()) {

                switch (LCD_NewModeSteps.OptionSelection) {
                  //Se ejecuta esta opción en caso de que se presione de nuevo el encoder 1
                  case 0:
                    //aqui debería guardarse los valores de los encoders previos
                    Motors.saveData(Encoders);
              
                    LCD_NewModeTime.setAutoScreenOut(false);
                    LCD_NewModeTime.initializeScreen(Encoders);
                    Encoders.savelimit_POS_A(TOTAL_STEPS_MOVEMENT);
                    Encoders.savelimit_POS_B(TOTAL_STEPS_MOVEMENT);
                    while (LCD_NewModeTime.getScreenStatus()) {
                      switch (LCD_NewModeTime.OptionSelection) {
                        case 0:
                          SD_Files.saveStep(Motors.getPOS_A(), Motors.getPOS_B(), Encoders.getPOS_A(), Encoders.getPOS_B(), LCD_NewModeSteps.stepNumber);
                          LCD_NewModeTime.returnToPreviousScreen(Encoders);
                          Motors.restoreData(Encoders);
                          LCD_NewModeSteps.stepNumber += 1;
                          break;
                        default:
                          LCD_NewModeTime.Refresh(Encoders);
                          LCD_NewModeTime.obtainEncoderButtomStatus(Encoders);
                          LCD_NewModeTime.verifyScreenExit(Encoders);
                      }
                    }
                    LCD_NewModeSteps.OptionSelection = -1;

                    //Encoders.saveSTEPSX(SD_Files.getAUX_STEPS_X());
                    //Encoders.saveSTEPSY(SD_Files.getAUX_STEPS_Y());
                    break;
                  default:
                    LCD_NewModeSteps.Refresh(Encoders);
                    LCD_NewModeSteps.obtainEncoderButtomStatus(Encoders);

                    SD_Files.readPreviousSteps();
                    Motors.moveFromTo(SD_Files, SD_Files.getAUX_STEPS_X(), SD_Files.getAUX_STEPS_Y(), Encoders.getSTEPSX(), Encoders.getSTEPSY());

                    LCD_NewModeSteps.verifyScreenExit(Encoders, SD_Files);  //Esta salida debería ser especial y que al salir guarde los datos sacas
                                                               //              //Se guardan los valores x y dados por el usuario, esto para formar un paso
                                                               //SD_Files.saveSteps(SD_Files.getAUX_STEPS_X(), SD_Files.getAUX_STEPS_Y()); //Guarda la posición hecha por el usuario
                }
              }
              LCD_LRSettings.OptionSelection = -1;
              LCD_LRSettings.currentOption = 0;

              break;


            case 2:

              if(ApiEndpoint.loadLastValidCredentials()){
                bool firstWIFIValidation = ApiEndpoint.establishConnection(network,password); // Send only does exist internet connection
                if (firstWIFIValidation){
                  DataWriter.sendAllFilesInDirectory("/iot/daily");
                }
                LCD_LRSettings.OptionSelection = -1;
                LCD_LRSettings.currentOption = 0;
              }
              break;


            case 3:
              
              LCD_LRChangeNetworkMenu.setAutoScreenOut(true);
              LCD_LRChangeNetworkMenu.initializeScreen(Encoders);
              c5 = { {0 , "CAMBIAR RED"}, {1, "AGREGAR RED"}};
              LCD_LRChangeNetworkMenu.OptionNames(c5);  // Give class the option names
              while(LCD_LRChangeNetworkMenu.getScreenStatus()){
                switch (LCD_LRChangeNetworkMenu.OptionSelection){
                  case 0:
                  Serial.println(LCD_LRChangeNetworkMenu.getScreenStatus());

                  LCD_LRChangeSaveNetwork.setAutoScreenOut(true);
                  LCD_LRChangeSaveNetwork.initializeScreen(Encoders);
                  ApiEndpoint.saveNetworkAssociations();
                  LCD_LRChangeSaveNetwork.OptionNames(ApiEndpoint.saveNetworks);  // Give class the option names 
                  while(LCD_LRChangeSaveNetwork.getScreenStatus()){
                    switch(LCD_LRChangeSaveNetwork.OptionSelection){
                      case 0:
                        LCD_SetPassword.saveSelectedNetwork(ApiEndpoint.saveNetworks[LCD_LRChangeSaveNetwork.currentOption].nombreArchivo);
                        LCD_SetPassword.saveSelectedPassword(LCD_SetPassword.getPasswordByNetwork(ApiEndpoint.saveNetworks[LCD_LRChangeSaveNetwork.currentOption].nombreArchivo));
                        LCD_SetPassword.saveCredentials();
                        LCD_LRChangeSaveNetwork.OptionSelection = -1;
                      break;
                      default:
                        LCD_LRChangeSaveNetwork.lineRefresh(Encoders);
                        LCD_LRChangeSaveNetwork.obtainEncoderButtomStatus(Encoders, 0);
                        LCD_LRChangeSaveNetwork.verifyScreenExit(Encoders);
                      
                    }
                  }

                  LCD_LRChangeNetworkMenu.OptionSelection = -1;
                  LCD_LRChangeNetworkMenu.currentOption = 0;

                  break;
                  case 1:
                    // agregar red, validar y cambiar red original
                    LCD_LRChangeNetwork.setAutoScreenOut(true);
                    LCD_LRChangeNetwork.initializeScreen(Encoders);
                    ApiEndpoint.networkAssociations();                          // Extract modes from memory
                    LCD_LRChangeNetwork.OptionNames(ApiEndpoint.networks);  // Give class the option names
                    while (LCD_LRChangeNetwork.getScreenStatus()) {
                      switch (LCD_LRChangeNetwork.OptionSelection) {
                        case 0:
                          LCD_SetPassword.setAutoScreenOut(false);
                          LCD_SetPassword.initializeScreen(Encoders);
                          LCD_SetPassword.saveSelectedNetwork(ApiEndpoint.networks[LCD_LRChangeNetwork.currentOption].nombreArchivo);
                          while(LCD_SetPassword.getScreenStatus()){
                            switch(LCD_SetPassword.OptionSelection){
                              case 0:
                                LCD_SetPassword.OptionSelection = -1;
                                LCD_SetPassword.saveSelectedOption();
                              break;
                              default:
                                LCD_SetPassword.lineRefresh(Encoders);
                                LCD_SetPassword.obtainEncoderButtomStatus(Encoders, 0);
                                LCD_SetPassword.verifyScreenExit(Encoders);
                            }
                          }
                          LCD_SetPassword.saveCredentials();
                          // ApiEndpoint.sendNetworkName(ApiEndpoint.networks[LCD_LRChangeNetwork.currentOption].nombreArchivo);
                          // ApiEndpoint.saveNetworkName();
                          LCD_LRChangeNetwork.OptionSelection = -1;
                        break;

                        default:
                          LCD_LRChangeNetwork.lineRefresh(Encoders);
                          LCD_LRChangeNetwork.obtainEncoderButtomStatus(Encoders, 0);
                          LCD_LRChangeNetwork.verifyScreenExit(Encoders);
                      }
                    }

                    LCD_LRChangeNetworkMenu.OptionSelection = -1;
                    LCD_LRChangeNetworkMenu.currentOption = 0;
                  break;
                  default:
                    LCD_LRChangeNetworkMenu.lineRefresh(Encoders);
                    LCD_LRChangeNetworkMenu.obtainEncoderButtomStatus(Encoders);
                    LCD_LRChangeNetworkMenu.verifyScreenExit(Encoders);
                }
              }



              // agregar red, validar y cambiar red original

              LCD_LRSettings.OptionSelection = -1;
              LCD_LRSettings.currentOption = 0;
            
            break;

            default:


              LCD_LRSettings.OptionNames(c2);
              LCD_LRSettings.lineRefresh(Encoders);
              LCD_LRSettings.obtainEncoderButtomStatus(Encoders);
              LCD_LRSettings.verifyScreenExit(Encoders);
          }
        }
        LCD_StartMenu.OptionSelection = -1;
        LCD_StartMenu.currentOption = 0;

        break;

      default:  //Imprime el menú inicial

        //LCD_StartMenu.inter();
        LCD_StartMenu.Refresh(Encoders);
        LCD_StartMenu.lineRefresh(Encoders);
        LCD_StartMenu.verifyScreenExit(Encoders);
        LCD_StartMenu.obtainEncoderButtomStatus(Encoders);
    }
  }
}


void encoder1() {
  Encoders.encoder1();
}

void encoder2() {
  Encoders.encoder2();
}

void push_a() {
  Encoders.push_a();
}

void push_b() {
  Encoders.push_b();
}

void printDriverYInfo(const char* tag) {
  String logRow = "";

  DateTime now = rtc.now();
  logRow += now.timestamp() + ",";
  // Serial.print("\n==================== ");
  // Serial.print(tag);
  // Serial.println(" ====================");
  logRow += String(tag) + ",";

  // ---------- UART real check (IFCNT should increase on successful write) ----------
  uint8_t if_before = driver1.IFCNT();
  driver1.toff(3);  // write something safe
  // delay(50);
  uint8_t if_after = driver1.IFCNT();

  // Serial.print("IFCNT before/after: ");
  // Serial.print(if_before);
  // Serial.print(" -> ");
  // Serial.println(if_after);
  logRow += String(if_before) + "->" + String(if_after) + ",";

  // Serial.print("IFCNT (read): ");
  // Serial.println(driver1.IFCNT());
  logRow += String(driver1.IFCNT()) + ",";

  // ---------- Core status ----------
  // Serial.print("PWM_SCALE: ");
  // Serial.println(driver1.PWM_SCALE());
  logRow += String(driver1.PWM_SCALE()) + ",";

  // Serial.print("GSTAT: 0x");
  // Serial.println(driver1.GSTAT(), HEX);
  logRow += String(driver1.GSTAT(), HEX) + ",";

  // Serial.print("DRV_STATUS: 0x");
  // Serial.println(driver1.DRV_STATUS(), HEX);
  logRow += String(driver1.DRV_STATUS(), HEX) + ",";

  // Serial.print("IOIN: 0x");
  // Serial.println(driver1.IOIN(), HEX);
  logRow += String(driver1.IOIN(), HEX) + ",";

  // ---------- Motion / sensing ----------
  // Serial.print("TSTEP: ");
  // Serial.println(driver1.TSTEP());
  logRow += String(driver1.TSTEP()) + ",";

  // Serial.print("SG_RESULT: ");
  // Serial.println(driver1.SG_RESULT());
  logRow += String(driver1.SG_RESULT()) + ",";

  // Serial.print("MSCNT (microstep counter): ");
  // Serial.println(driver1.MSCNT());
  logRow += String(driver1.MSCNT()) + ",";

  // Serial.print("MSCURACT (coil currents): 0x");
  // Serial.println(driver1.MSCURACT(), HEX);
  logRow += String(driver1.MSCURACT(), HEX) + ",";

  // ---------- Current / microsteps ----------
  // Serial.print("Current scale (cs_actual): ");
  // Serial.println(driver1.cs_actual());
  logRow += String(driver1.cs_actual()) + ",";

  // Serial.print("Microsteps (set): ");
  // Serial.println(current_usteps);
  logRow += String(current_usteps) + ",";

  // Serial.print("IHOLD_IRUN: 0x");
  // Serial.println(driver1.IHOLD_IRUN(), HEX);
  logRow += String(driver1.IHOLD_IRUN(), HEX) + ",";

  // ---------- Configuration snapshots ----------
  // Serial.print("GCONF: 0x");
  // Serial.println(driver1.GCONF(), HEX);
  logRow += String(driver1.GCONF(), HEX) + ",";

  // Serial.print("CHOPCONF: 0x");
  // Serial.println(driver1.CHOPCONF(), HEX);
  logRow += String(driver1.CHOPCONF(), HEX) + ",";

  // Serial.print("PWMCONF: 0x");
  // Serial.println(driver1.PWMCONF(), HEX);
  logRow += String(driver1.PWMCONF(), HEX) + ",";

  // ---------- Thresholds (mode switching / stall features) ----------
  // Serial.print("TPWMTHRS: ");
  // Serial.println(driver1.TPWMTHRS());
  logRow += String(driver1.TPWMTHRS()) + ",";

  // Serial.print("TCOOLTHRS: ");
  // Serial.println(driver1.TCOOLTHRS());
  logRow += String(driver1.TCOOLTHRS()) + ",";

  // Serial.print("COOLCONF: 0x");
  // Serial.println(driver1.COOLCONF(), HEX);
  logRow += String(driver1.COOLCONF(), HEX) + ",";

  // ---------- Safety flags (decoded) ----------
  uint8_t otpw = driver1.otpw();
  uint8_t ot = driver1.ot();
  uint8_t s2ga = driver1.s2ga();
  uint8_t s2gb = driver1.s2gb();
  uint8_t ola = driver1.ola();
  uint8_t olb = driver1.olb();

  // Serial.print("Temp warning (otpw): ");
  // Serial.println(otpw);
  logRow += String(otpw) + ",";

  // Serial.print("Temp shutdown (ot): ");
  // Serial.println(ot);
  logRow += String(ot) + ",";

  // Serial.print("Short to GND A (s2ga): ");
  // Serial.println(s2ga);
  logRow += String(s2ga) + ",";

  // Serial.print("Short to GND B (s2gb): ");
  // Serial.println(s2gb);
  logRow += String(s2gb) + ",";

  // Serial.print("Open load A (ola): ");
  // Serial.println(ola);
  logRow += String(ola) + ",";

  // Serial.print("Open load B (olb): ");
  // Serial.println(olb);
  logRow += String(olb) + "\n";

  DataWriter.write(logRow);
}

void printDriverXInfo(const char* tag) {
  String logRow = "";

  DateTime now = rtc.now();

  logRow += now.timestamp() + ",";

  logRow += String(tag) + ",";

  // ---------- UART real check (IFCNT should increase on successful write) ----------
  uint8_t if_before = driver2.IFCNT();
  driver2.toff(3);  // write something safe
  // delay(50);
  uint8_t if_after = driver2.IFCNT();

  // Serial.print("IFCNT before/after: ");
  // Serial.print(if_before);
  // Serial.print(" -> ");
  // Serial.println(if_after);
  logRow += String(if_before) + "->" + String(if_after) + ",";

  // Serial.print("IFCNT (read): ");
  // Serial.println(driver2.IFCNT());
  logRow += String(driver2.IFCNT()) + ",";

  // // ---------- Core status ----------
  // Serial.print("PWM_SCALE: ");
  // Serial.println(driver2.PWM_SCALE());
  logRow += String(driver2.PWM_SCALE()) + ",";

  // Serial.print("GSTAT: 0x");
  // Serial.println(driver2.GSTAT(), HEX);
  logRow += String(driver2.GSTAT(), HEX) + ",";

  // Serial.print("DRV_STATUS: 0x");
  // Serial.println(driver2.DRV_STATUS(), HEX);
  logRow += String(driver2.DRV_STATUS(), HEX) + ",";

  // Serial.print("IOIN: 0x");
  // Serial.println(driver2.IOIN(), HEX);
  logRow += String(driver2.IOIN(), HEX) + ",";

  // // ---------- Motion / sensing ----------
  // Serial.print("TSTEP: ");
  // Serial.println(driver2.TSTEP());
  logRow += String(driver2.TSTEP()) + ",";

  // Serial.print("SG_RESULT: ");
  // Serial.println(driver2.SG_RESULT());
  logRow += String(driver2.SG_RESULT()) + ",";

  // Serial.print("MSCNT (microstep counter): ");
  // Serial.println(driver2.MSCNT());
  logRow += String(driver2.MSCNT()) + ",";

  // Serial.print("MSCURACT (coil currents): 0x");
  // Serial.println(driver2.MSCURACT(), HEX);
  logRow += String(driver2.MSCURACT(), HEX) + ",";

  // // ---------- Current / microsteps ----------
  // Serial.print("Current scale (cs_actual): ");
  // Serial.println(driver2.cs_actual());
  logRow += String(driver2.cs_actual()) + ",";

  // Serial.print("Microsteps (set): ");
  // Serial.println(current_usteps);
  logRow += String(current_usteps) + ",";

  // Serial.print("IHOLD_IRUN: 0x");
  // Serial.println(driver2.IHOLD_IRUN(), HEX);
  logRow += String(driver2.IHOLD_IRUN(), HEX) + ",";

  // // ---------- Configuration snapshots ----------
  // Serial.print("GCONF: 0x");
  // Serial.println(driver2.GCONF(), HEX);
  logRow += String(driver2.GCONF(), HEX) + ",";

  // Serial.print("CHOPCONF: 0x");
  // Serial.println(driver2.CHOPCONF(), HEX);
  logRow += String(driver2.CHOPCONF(), HEX) + ",";

  // Serial.print("PWMCONF: 0x");
  // Serial.println(driver2.PWMCONF(), HEX);
  logRow += String(driver2.PWMCONF(), HEX) + ",";

  // // ---------- Thresholds (mode switching / stall features) ----------
  // Serial.print("TPWMTHRS: ");
  // Serial.println(driver2.TPWMTHRS());
  logRow += String(driver2.TPWMTHRS()) + ",";

  // Serial.print("TCOOLTHRS: ");
  // Serial.println(driver2.TCOOLTHRS());
  logRow += String(driver2.TCOOLTHRS()) + ",";

  // Serial.print("COOLCONF: 0x");
  // Serial.println(driver2.COOLCONF(), HEX);
  logRow += String(driver2.COOLCONF(), HEX) + ",";

  // ---------- Safety flags (decoded) ----------
  uint8_t otpw = driver2.otpw();
  uint8_t ot = driver2.ot();
  uint8_t s2ga = driver2.s2ga();
  uint8_t s2gb = driver2.s2gb();
  uint8_t ola = driver2.ola();
  uint8_t olb = driver2.olb();

  // Serial.print("Temp warning (otpw): ");
  // Serial.println(otpw);
  logRow += String(otpw) + ",";

  // Serial.print("Temp shutdown (ot): ");
  // Serial.println(ot);
  logRow += String(ot) + ",";

  // Serial.print("Short to GND A (s2ga): ");
  // Serial.println(s2ga);
  logRow += String(s2ga) + ",";

  // Serial.print("Short to GND B (s2gb): ");
  // Serial.println(s2gb);
  logRow += String(s2gb) + ",";

  // Serial.print("Open load A (ola): ");
  // Serial.println(ola);
  logRow += String(ola) + ",";

  // Serial.print("Open load B (olb): ");
  // Serial.println(olb);
  logRow += String(olb) + "\n";

  DataWriter.write(logRow);
}



void printCurrentTime() {

  DateTime now = rtc.now();

  Serial.print("Current time: ");
  Serial.print(now.year());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.day());
  Serial.print(" ");

  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
}


void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;  // End of directory

    for (uint8_t i = 0; i < numTabs; i++) Serial.print('\t');
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);  // Recursive call
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);  // File size
    }
    entry.close();
  }
}






char printCurrentCharacter(int currentIndex) {
  return charset[currentIndex];
}

// Vector que contiene las asociaciones entre identificadores y nombres de archivo

void startTimeForOut() {
  startTime = rtc.now();  // Momento en que comienza el período
}


bool has_interval_passed(unsigned long& previousTime, unsigned long interval) {
  unsigned long currentTime = millis();

  if (currentTime - previousTime >= interval) {
    previousTime = currentTime;
    return true;
  }

  return false;
}

boolean TimePeriodIsOver(unsigned long& periodStartTime, unsigned long TimePeriod) {
  unsigned long currentMillis = millis();
  if (currentMillis - periodStartTime >= TimePeriod) {
    periodStartTime = currentMillis;  // set new expireTime
    return true;                      // more time than TimePeriod) has elapsed since last time if-condition was true
  } else return false;                // not expired
}

void BlinkHeartBeatLED(int IO_Pin, int BlinkPeriod) {
  static unsigned long MyBlinkTimer;
  pinMode(IO_Pin, OUTPUT);

  if (TimePeriodIsOver(MyBlinkTimer, BlinkPeriod)) {
    digitalWrite(IO_Pin, !digitalRead(IO_Pin));
  }
}

void flashLED(int duration_ms = 30) {
    digitalWrite(OnBoard_LED, HIGH);
    delay(20);
    digitalWrite(OnBoard_LED, LOW);
}



bool clearDirectory(String dirPath) {

  File dir = SD.open(dirPath);

  if (!dir) {
    Serial.println("Error: no se pudo abrir el directorio.");
    return false;
  }

  if (!dir.isDirectory()) {
    Serial.println("Error: la ruta no es un directorio.");
    dir.close();
    return false;
  }

  File file = dir.openNextFile();

  while (file) {
    String filePath = String(dirPath) + "/" + String(file.name());

    file.close();  // cerrar antes de borrar

    if (SD.remove(filePath)) {
      Serial.println("Archivo eliminado: " + filePath);
    } else {
      Serial.println("No se pudo eliminar: " + filePath);
    }

    file = dir.openNextFile();
  }

  dir.close();
  return true;
}