#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Wire.h>
#include "SparkFunHTU21D.h"
#include "TSL2561.h"

#define IPSET_STATIC { 192, 168, 1, 117 }
#define IPSET_GATEWAY { 192, 168, 1, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 1, 1 }

ADC_MODE(ADC_VCC); // Sæt ADC til intern forsyningsspænding

TSL2561 tsl(TSL2561_ADDR_LOW); // Initialiser TSL2561 til ADDR_LOW

HTU21D SHT21; // Initialiser SHT21

WiFiClient client; // Initialiser WiFi.client

uint8_t  MAC_STA[] = {0,0,0,0,0,0}; // var til MAC adresse
char MAC_char[18]; // Buffer til MAC adresse parsing

const uint16_t port = 2000; // Node JS serverens port
const char * host = "192.168.1.114"; // Node JS serverens IP adresse

void setup() {
  pinMode(12, OUTPUT); // Sæt pin 12 til output
  pinMode(14, OUTPUT); // Sæt pin 14 til output
    
  Wire.setClockStretchLimit(1500); // ESP8266 I2C hack

  SHT21.begin(); // Start SHT21

  tsl.setGain(TSL2561_GAIN_0X); // Sæt TSL2561 gain til 0x
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS); // Sæt TSL2561 intergratinstid til 13 mS
  
  Serial.begin(115200); // Start UART til debugging
  
  Serial.println(""); // Newline

  WiFi.mode(WIFI_STA); // Sæt WiFi mode til station mode
  WiFi.disconnect(); // Disconnect WiFi før WiFi.begin()
  delay(100); // Vent lidt inden start af WiFi

  String hostnameStr = "ESP_IoT_"; // String objekt til hostname
  hostnameStr += ESP.getChipId(); // Hent ESP chip UID og tilføj til hostname

  WiFi.hostname(hostnameStr); // Sæt hostname
  WiFi.begin("roving1", "gag0v44i"); // Connect to AP
  
  //WiFi.config(IPAddress(IPSET_STATIC), IPAddress(IPSET_GATEWAY), IPAddress(IPSET_SUBNET), IPAddress(IPSET_DNS)); // Opsæt WiFi med statisk IP adresse

  Serial.print("Connecting to WiFi AP"); // Debug output

  int trys = 0; // var til at holde styr på forbindelses forsøg
  while (WiFi.status() != WL_CONNECTED) { // mens ESP opretter forbindelse til AP
    if (trys > 30) ESP.deepSleep(892 * 1000000); // Hvis problemer med at oprette forbindelse, gå i sleep mode
    Serial.print("."); // Debug output
    trys++; // Inkrement trys var
    delay(250);
  }

  Serial.println(""); // Debug output
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(50); // Vent lidt med at starte main loop
}

void loop() {
  if(!client.connected()) { // hvis ikke forbundet til server, opret forbindelse
    Serial.println(WiFi.status()); // Debug output
    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.println("..");

    if (!client.connect(host, port)) { // hvis forbindelses fejl, gå i sleep mode
        Serial.println("Connection failed"); // Debug output
        Serial.println("Sleep..");
        ESP.deepSleep(892 * 1000000); // Sleep mode i 15 minutter
    }
  }

  float temp = SHT21.readTemperature(); // Hent temperatur fra SHT21
  float humd = SHT21.readHumidity(); // Hent relativ fugtighed fra SHT21

  uint32_t lum = tsl.getFullLuminosity(); // Hent lysintensitet fra TSL2561
  uint16_t ir, full; // vars til parsing af lux
  
  ir = lum >> 16; // Parse lux
  full = lum & 0xFFFF;

  tsl.disable(); // Sæt TSL2561 i sleep mode (SHT21 går automatisk i sleep mode)

  WiFi.macAddress(MAC_STA); // hent MAC adresse til MAC_STA var
  for (int i = 0; i < sizeof(MAC_STA); ++i){ // Parse MAC adresse
    sprintf(MAC_char,"%s%02x:",MAC_char,MAC_STA[i]);
  }
  MAC_char[17] = 0; // Slet sidste ':' tegn fra MAC adresse
  
  String message = MAC_char; // String objekt til sensor data
  message += ",";
  message += ESP.getChipId();
  message += ",";
  message += millis();
  message += ",";
  message += ESP.getVcc();
  message += ",";
  message += temp;
  message += ",";
  message += humd;
  message += ",";
  message += tsl.calculateLux(full, ir);
      
  Serial.println(message); // Debug output
  client.print(message); // Send string objekt til server
    
  String tmpTxt = "From server: "; // String objekt til server svar
  String line = client.readStringUntil('\r'); // Hent svar fra server ind i string objekt
  String tmpAss = tmpTxt + line; // Sammensæt string objekter
  if (line.length() > 1) { // Hvis svar fra server > 1
    Serial.println(tmpAss); // Debug output
  } 
  
  Serial.println("Closing connection.."); // Debug output
  client.stop(); // Luk forbindelse til server
  digitalWrite(14, HIGH); // Blink med LED for at indikere success
  delay(100);
  digitalWrite(14, LOW);
  delay(100);

  Serial.println("Sleep 892 secs.."); // Debug output
  
  if (ESP.getVcc() < 3000) { // Hvis Vcc < 3000 mV gå i uendelig sleep mode
    ESP.deepSleep(0);
  }
  
  ESP.deepSleep(892 * 1000000); // Gå i sleep mode i 15 minutter
}
