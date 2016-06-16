#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Wire.h>
#include "SparkFunHTU21D.h"
#include "TSL2561.h"
#include <Adafruit_NeoPixel.h>

#define IPSET_STATIC { 192, 168, 1, 111 }
#define IPSET_GATEWAY { 192, 168, 1, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 1, 1 }

ADC_MODE(ADC_VCC); // Sæt ADC til intern forsyningsspænding

TSL2561 tsl(TSL2561_ADDR_LOW); // Initialiser TSL2561 til ADDR_LOW

HTU21D SHT21; // Initialiser SHT21

Adafruit_NeoPixel strip = Adafruit_NeoPixel(12, 14, NEO_GRB + NEO_KHZ800); // Initaliser WS2812

WiFiClient client; // Initialiser WiFi.client

uint8_t  MAC_STA[] = {0,0,0,0,0,0}; // var til MAC adresse
char MAC_char[18]; // Buffer til MAC adresse parsing

const uint16_t port = 2000; // Node JS serverens port
const char * host = "192.168.1.114"; // Node JS serverens IP adresse

long unsigned millisLast = 0; // interval var

int pwm1val = 0;
int pwm2val = 0;
int pwm3val = 0;
int newPwm1val = 0;
int newPwm2val = 0;
int newPwm3val = 0;

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbowCycle(uint8_t wait) { // Regnbue animation
  uint16_t i, j;

  for(j=0; j<256*5; j++) {
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

void setup() {
  ESP.eraseConfig(); // Fjern tidligere gemt WiFi konfiguration
  delay(500); 
  SHT21.begin(); // Start SHT21
  Wire.setClockStretchLimit(1500); // ESP8266 I2C hack

  tsl.setGain(TSL2561_GAIN_0X); // Sæt TSL2561 gain til 0x
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS); // Sæt TSL2561 intergratinstid til 13 mS

  strip.begin(); // Start WS2812
  strip.show(); // Initialize all pixels to 'off'
  
  Serial.begin(115200); // Start UART til debugging
  
  Serial.println("");

  WiFi.mode(WIFI_STA); // Sæt WiFi mode til station mode
  WiFi.disconnect(); // Disconnect WiFi før WiFi.begin()
  delay(100); // Vent lidt inden start af WiFi

  String hostnameStr = "ESP_IoT_"; // String objekt til hostname
  hostnameStr += ESP.getChipId(); // Hent ESP chip UID og tilføj til hostname

  WiFi.hostname(hostnameStr); // Sæt hostname
  WiFi.begin("ssid", "passphrase"); // Connect to AP
  
  //WiFi.config(IPAddress(IPSET_STATIC), IPAddress(IPSET_GATEWAY), IPAddress(IPSET_SUBNET), IPAddress(IPSET_DNS)); // Opsæt WiFi med statisk IP adresse

  Serial.print("Connecting to WiFi AP"); // Debug output

  int trys = 0; // var til at holde styr på forbindelses forsøg
  while (WiFi.status() != WL_CONNECTED) { // mens ESP opretter forbindelse til AP
    if (trys > 30) ESP.reset(); // Hvis problemer med at oprette forbindelse, reset ESP
    Serial.print("."); // Debug output
    trys++; // Inkrement trys var
    delay(250);
    
  }

  Serial.println(""); // Debug output
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(50);
  
  WiFi.macAddress(MAC_STA); // hent MAC adresse til MAC_STA var
  for (int i = 0; i < sizeof(MAC_STA); ++i){ // Parse MAC adresse
    sprintf(MAC_char,"%s%02x:",MAC_char,MAC_STA[i]);
  }
  MAC_char[17] = 0; // Slet sidste ':' tegn fra MAC adresse
}

void loop() {
  if(!client.connected()) { // hvis ikke forbundet til server, opret forbindelse
    Serial.println(WiFi.status()); // Debug output
    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.println("..");

    if (!client.connect(host, port)) { // hvis forbindelses fejl, vent 5 sekunder og prøv igen
        Serial.println("Connection failed");
        Serial.println("Wait 5 sec..");
        delay(5000);
        return;
    }
  }

  if (millis() - millisLast > 60000) { // hvis > 60 sekunder siden sidste sample
    millisLast = millis();
    float temp = SHT21.readTemperature(); // Hent temperatur fra SHT21
    float humd = SHT21.readHumidity(); // Hent relativ fugtighed fra SHT21

    uint32_t lum = tsl.getFullLuminosity(); // Hent lysintensitet fra TSL2561
    uint16_t ir, full; // vars til parsing af lux
  
    ir = lum >> 16; // Parse lux
    full = lum & 0xFFFF;

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
  }

  while(client.available()) { // mens der er data i bufferen
    String tmpTxt = "From server: "; // String objekt til server svar
    String line = client.readStringUntil('\r'); // Hent svar fra server ind i string objekt
    String tmpAss = tmpTxt + line; // Sammensæt string objekter
    if (line.length() > 1) { // Hvis svar fra server > 1
      Serial.println(tmpAss); // Debug output
  
      if (line.startsWith("PWM1")) { // hvis svar fra server er PWM1
        String pwm1 = line.substring(5,9); // smid PWM1 væk og hent værdi
        newPwm1val = pwm1.toInt(); // konverter til int
  
        if (newPwm1val > pwm1val) { // hvis ny værdi > gammel værdi
          for (int fadeValue = pwm1val ; fadeValue <= newPwm1val; fadeValue += 1) { // Inkrement PWM på (fade)
            analogWrite(14, fadeValue); // Sæt PWM værdi
            delay(2); // Vent 2 mS for at give fade effekt
          }
          pwm1val = newPwm1val; // Sæt gammel værdi til ny værdi
        }
        
        if (newPwm1val < pwm1val) { // hvis ny værdi < gammel værdi
          for (int fadeValue = pwm1val ; fadeValue >= newPwm1val; fadeValue -= 1) { // Dekrement PWM på (fade)
            analogWrite(14, fadeValue); // Sæt PWM værdi
            delay(2); //Vent 2 mS for at give fade effekt
          }
          pwm1val = newPwm1val; // Sæt gammel værdi til ny værdi
        }
        
        Serial.print("PWM1: "); // Debug output
        Serial.println(pwm1val);
      }
  
      if (line.startsWith("PWM2")) {
        String pwm2 = line.substring(5,9);
        newPwm2val = pwm2.toInt();
  
        if (newPwm2val > pwm2val) {
          for (int fadeValue = pwm2val ; fadeValue <= newPwm2val; fadeValue += 1) {
            analogWrite(12, fadeValue);
            delay(2);
          }
          pwm2val = newPwm2val;
        }
        
        if (newPwm2val < pwm2val) {
          for (int fadeValue = pwm2val ; fadeValue >= newPwm2val; fadeValue -= 1) {
            analogWrite(12, fadeValue);
            delay(2);
          }
          pwm2val = newPwm2val;
        }
        
        Serial.print("PWM2: ");
        Serial.println(pwm2val);
      }
  
      if (line.startsWith("PWM3")) {
        String pwm3 = line.substring(5,9);
        newPwm3val = pwm3.toInt();
  
        if (newPwm3val > pwm3val) {
          for (int fadeValue = pwm3val ; fadeValue <= newPwm3val; fadeValue += 1) {
            analogWrite(13, fadeValue);
            delay(2);
          }
          pwm3val = newPwm3val;
        }
        
        if (newPwm3val < pwm3val) {
          for (int fadeValue = pwm3val ; fadeValue >= newPwm3val; fadeValue -= 1) {
            analogWrite(13, fadeValue);
            delay(2);
          }
          pwm3val = newPwm3val;
        }
        
        Serial.print("PWM3: ");
        Serial.println(pwm3val);
      }
  
      if (line.startsWith("WS2812")) { // hvis svar fra server er WS2812
        String ws2812 = line.substring(7,8); // smid WS2812 væk og hent værdi
        Serial.println(ws2812);
        if (ws2812 == "1") { // hvis værdi 1
          colorWipe(strip.Color(255, 0, 0), 50); // Rød wipe
          colorWipe(strip.Color(0, 255, 0), 50); // Grøn wipe
          colorWipe(strip.Color(0, 0, 255), 50); // Blå wipe
          colorWipe(strip.Color(0, 0, 0), 50); // Blackout
        }

        else if (ws2812 == "2") { // hvis værdi 2
          rainbowCycle(5); // Regnbue animation
          colorWipe(strip.Color(0, 0, 0), 0); // Blackout
        }

        else if (ws2812 == "3") { // hvis værdi 3
          theaterChase(strip.Color(255, 255, 255), 50); // Theater chase animation
          colorWipe(strip.Color(0, 0, 0), 0); // Blackout
        }                
      }
    }
  }
}
