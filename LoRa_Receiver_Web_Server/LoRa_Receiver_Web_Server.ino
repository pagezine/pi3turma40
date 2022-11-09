/*********
Receptor LORA 
Projeto III - Univesp Eng. Computação 
Versão 0.10 beta
*********/

// Import Wi-Fi biblioteca
#include <WiFi.h>
#include "ESPAsyncWebServer.h"

#include <SPIFFS.h>

//Biblioteca LoRa
#include <SPI.h>
#include <LoRa.h>

//Biblioteca OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Biblioteca para obter tempo do NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

//pinos do módulo emissor-receptor Lora
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

//433E6 for Asia
//866E6 for Europe // o mesmo usado no Brasil
//915E6 for North America
#define BAND 866E6

//OLED pinos
#define OLED_SDA 4
#define OLED_SCL 15 
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, em pixels
#define SCREEN_HEIGHT 64 // OLED display height, em  pixels

// coloque aqui as credenciais da rede
const char* ssid     = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

//Definir cliente NTP para obter tempo
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variáveis para salvar data e hora
String formattedDate;
String day;
String hour;
String timestamp;


// Inicializar variáveis para obter e guardar dados LoRa
int rssi;
String loRaMessage;
String temperature;
String humidity;
String pressure;
String readingID;

// Cria AsyncWebServer  na porta 80
AsyncWebServer server(80);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Substitui placeholder por valores DHT
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return temperature;
  }
  else if(var == "HUMIDITY"){
    return humidity;
  }
  else if(var == "PRESSURE"){
    return pressure;
  }
  else if(var == "TIMESTAMP"){
    return timestamp;
  }
  else if (var == "RRSI"){
    return String(rssi);
  }
  return String();
}

//Inicaliza OLED display
void startOLED(){
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  //Inicializa OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 atribuição falhou"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("LORA EMISSOR");
}

//Inicializa LoRa module
void startLoRA(){
  int counter;
  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //configuração LoRa transmissor module
  LoRa.setPins(SS, RST, DIO0);

  while (!LoRa.begin(BAND) && counter < 10) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 10) {
    //Leitura incremento do ID a cada nova leitura
    Serial.println("Falha de inicialização LoRa!"); 
  }
  Serial.println("LoRa inicializado OK!");
  display.setCursor(0,10);
  display.clearDisplay();
  display.print("LoRa inicializando OK!");
  display.display();
  delay(2000);
}

void connectWiFi(){
  // Conecta na rede Wi-Fi com SSID e senha
  Serial.print("Conectando-se a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Imprimir endereço IP local e iniciar servidor web
  Serial.println("");
  Serial.println("WiFi conectado.");
  Serial.println("IP endereço: ");
  Serial.println(WiFi.localIP());
  display.setCursor(0,20);
  display.print("Acesso Servidor Web em: ");
  display.setCursor(0,30);
  display.print(WiFi.localIP());
  display.display();
}

// Lendo pacote LoRA e obtendo as leituras do sensor
void getLoRaData() {
  Serial.print("Lora pacote recebido: ");
  // Read packet
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    // LoRaData format: readingID/temperature&soilMoisture#batterylevel
    // String example: 1/27.43&654#95.34
    Serial.print(LoRaData); 
    
    // Obtendo a leitura/id temperatura e humidade
    int pos1 = LoRaData.indexOf('/');
    int pos2 = LoRaData.indexOf('&');
    int pos3 = LoRaData.indexOf('#');
    readingID = LoRaData.substring(0, pos1);
    temperature = LoRaData.substring(pos1 +1, pos2);
    humidity = LoRaData.substring(pos2+1, pos3);
    pressure = LoRaData.substring(pos3+1, LoRaData.length());    
  }
  // Pega RSSI
  rssi = LoRa.packetRssi();
  Serial.print(" com RSSI ");    
  Serial.println(rssi);
}

// Função para obter data e hora do NTPClient
void getTimeStamp() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // Extraindo data e hora
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extraindo data
  int splitT = formattedDate.indexOf("T");
  day = formattedDate.substring(0, splitT);
  Serial.println(day);
  // Extraindo hora
  hour = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.println(hour);
  timestamp = day + " " + hour;
}

void setup() { 
  // Inicializando Serial Monitor
  Serial.begin(115200);
  startOLED();
  startLoRA();
  connectWiFi();
  
  if(!SPIFFS.begin()){
    Serial.println("Ocorreu um erro durante a montagem SPIFFS");
    return;
  }
  //Rota para a raiz / página web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temperature.c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", humidity.c_str());
  });
  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", pressure.c_str());
  });
  server.on("/timestamp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", timestamp.c_str());
  });
  server.on("/rssi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(rssi).c_str());
  });
  server.on("/winter", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/winter.jpg", "image/jpg");
  });
  // Iniciando Servidor
  server.begin();
  
  // Inicializar um NTPClient obter tempo
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);
}

void loop() {
  //Verificar se há pacotes LoRa disponíveis
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    getLoRaData();
    getTimeStamp();
  }
}
