// ============================================================================
// Ceas LCD 20x4 I2C (8/9) cu Open-Meteo Craiova pentru ESP32-S3 SuperMini (Nicu Florica / niq_ro)
// Adaptat dupa schita originala de la https://www.youtube.com/watch?v=8txWH-ZVqIE&t=1049s 
// ============================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// Configurare pini I2C ceruti pentru ESP32-S3 SuperMini
#define I2C_SDA 8
#define I2C_SCL 9

// Initializare LCD 20x4 la adresa standard 0x27
LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiMulti wifiMulti;

char *Mon[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
char *Day[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// Fusul orar pentru Romania (GMT+2 / +3 cu DST gestionat de configTime)
long timezone = 2 * 3600; 
int dst = 3600; // Seteaza 0 daca observi decalaje in afara perioadei orei de vara
char ntp_server1[] = "pool.ntp.org";
char ntp_server2[] = "time.nist.gov";

// Variabile meteo Open-Meteo Craiova
int temperature = 0;
int umiditate = 0;
bool dateMeteoValide = false;
String stareVreme = "---"; // Noua variabilă pentru starea vremii

int msecond, second, minute, hour, dayOfWeek, dayOfMonth, month, year, dots;
unsigned long lastSecond = millis();
unsigned long currentMillis = 0;
unsigned long scrollMillis = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastUpdateData = 0;
unsigned int stringLength;
char timeBuffer[100]; // Dimensiune marita si corectata pentru textul extins

const int MESSAGE_LENGTH = sizeof(timeBuffer);
const int DISPLAY_WIDTH = 20;
int g_nPosition = 0;
String upperSec = "0";
String underSec = "0";

#define B 0x20

// Matricea de caractere mari pastrata din codul tau original
char bn1[] = {
  8, 2, 1, 2, 1, B, 2, 2, 1, 2, 2, 1, 3, B, 8, 8, 2, 2, 8, 2, 2, 2, 2, 8, 8, 2, 1, 8, 2, 1, 8, 2, 1, 8, 2, 1, 3, 2, 2, 8, 2, 1, 8, 2, 2, 8, 2, 2, 8, 2, 1, 8, B, 8, B, 8, B, B, B, 8, 8, B, 8, 8, B, B, 8, 3, 1, 8, B, 1, 8, 2, 1, 8, 2, 1, 8, 2, 1, 8, 2, 1, 8, 2, 5, 2, 8, 2, 8, B, 8, 8, B, 8, 8, B, 8, 8, B, 8, 8, B, 8, 2, 2, 1, B, B, B
};
char bn2[] = {
  8, B, 8, B, 8, B , 3, 2, 2, B, 6, 1, 4, 3, 8, 5, 6, 7, 8, 6, 7, B, 3, 2, 8, 6, 8, 5, 6, 8, 8, 6, 8, 8, 6, 1, 8, B, B, 8, B, 8, 8, 6, B, 8, 6, B, 8, B, 3, 8, 6, 8, B, 8, B, B, B, 8, 8, 6, 2, 8, B, B, 8, B, 8, 8, 4, 8, 8, B, 8, 8, 2, 2, 8, B, 8, 8, 3, 8, 2, 2, 1, B, 8, B, 8, B, 8, 8, B, 8, 8, B, 8, B, 6, B, 2, 3, 2, B, 6, B, B, B, B
};
char bn3[] = {
  4, 3, 8, 3, 8, 3, 8, 3, 3, 3, 3, 8, B, B, 8, 4, 3, 8, 4, 3, 8, B, 8, B, 4, 3, 8, B, B, 8, 8, B, 8, 8, 3, 8, 4, 3, 3, 8, 3, 8, 8, 3, 3, 8, B, B, 4, 3, 8, 8, B, 8, B, 8, B, 4, 8, B, 8, B, 1, 8, 3, 3, 8, B, 8, 8, B, 8, 4, 3, 8, 8, B, B, 4, 3, 7, 8, B, 1, 4, 3, 8, B, 8, B, 4, 3, 8, 2, 3, 2, 4, 2, 8, 8, B, 8, B, 8, B, 4, 3, 3, B, B, B
};

void DefineLargeChar();
void printTwoNumber(uint8_t number, uint8_t position);
void printColons(uint8_t position);
void printNoColons(uint8_t position);
void updateData();
void updateTime();
void concatenate();
void scrollDisplay(int line, int speed);

void setup() {
  Serial.begin(115200);

  // Pornirea I2C cu pinii ceruti (8 si 9) inainte de initializarea LCD
  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.begin();
  lcd.backlight();
  DefineLargeChar(); 
  lcd.clear();
  lcd.print("Conectare WiFi...");

  // Adaugare cele doua retele stabilite anterior
  wifiMulti.addAP("bbk", "internet");
  wifiMulti.addAP("bbk2", "internet2");

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  lcd.clear();
  lcd.print("WiFi Conectat!");
  delay(1000);

  // Configurare timp local cu serverele NTP
  configTime(timezone, dst, ntp_server1, ntp_server2);
  lcd.clear();
  lcd.print("Sincronizare NTP...");
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }

  lcd.clear();
  updateTime();
  updateData();
}

void loop() {
  currentMillis = millis();

  // Verificare Wi-Fi si actualizari periodice
  if (WiFi.status() != WL_CONNECTED) {
    wifiMulti.run();
  }

  if ((currentMillis - lastUpdateTime) > 300000) updateTime();
  if ((currentMillis - lastUpdateData) > 900000) updateData(); // Update meteo la 15 minute
  if (year < 2000) updateTime();

  dots = (currentMillis % 1000) < 500;
  if (dots) printColons(9); else printNoColons(9);

  if ((millis() - lastSecond) > 1000) {
    lastSecond = millis();
    second++;
    if (second > 59) {
      second = 0;
      minute++;
      if (minute > 59) {
        minute = 0;
        hour++;
        if (hour > 23) hour = 0;
      }
    }

    if (second < 10) {
      upperSec = "0";
      underSec = (String(second)).substring(0, 1);
    } else {
      upperSec = (String(second)).substring(0, 1);
      underSec = (String(second)).substring(1, 2);
    }

    printTwoNumber(hour, 1);
    printTwoNumber(minute, 11);
    lcd.setCursor(19, 0); lcd.print(upperSec);
    lcd.setCursor(19, 1); lcd.print(underSec);
  }

  concatenate();
  scrollDisplay(3, 350); // Derulare text pe linia 4
}

// Rescris complet pentru utilizarea HTTPS securizat Open-Meteo Craiova
void updateData() {
  lastUpdateData = millis();
  
  if (WiFi.status() == WL_CONNECTED) {
    String latitudinea = "44.3311";
    String longitudinea = "23.8147";
    String adresaCompleta = "https://api.open-meteo.com/v1/forecast?latitude=" + latitudinea + 
                            "&longitude=" + longitudinea + "&current=temperature_2m,relative_humidity_2m";

    WiFiClientSecure client;
    client.setInsecure(); // Ignora verificarea greoaie a certificatelor publice

    HTTPClient http;
    http.setTimeout(5000);

    if (http.begin(client, adresaCompleta)) {
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // Alocare compatibila cu versiunea 6 de ArduinoJson pe care o detii
        DynamicJsonDocument doc(1024); 
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          float tempFloat = doc["current"]["temperature_2m"];
          temperature = (int)round(tempFloat);
          umiditate = doc["current"]["relative_humidity_2m"];
          dateMeteoValide = true;
          Serial.printf("[Open-Meteo] Craiova Temp: %d C, Umiditate: %d%%\n", temperature, umiditate);
        } else {
          dateMeteoValide = false;
        }
      } else {
        dateMeteoValide = false;
      }
      http.end();
    }
  }
}

// Actualizare folosind functiile native stabile de ESP32
void updateTime() {
  lastUpdateTime = millis();
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    year = timeinfo.tm_year + 1900;
    month = timeinfo.tm_mon;
    dayOfMonth = timeinfo.tm_mday;
    dayOfWeek = timeinfo.tm_wday;
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
  }
}

// Concatenare text scrollbar cu suport Open-Meteo (Temperatura + Umiditate)
void concatenate() {
  int i;
  memset(timeBuffer, ' ', (MESSAGE_LENGTH - 1));
  
  if (dateMeteoValide) {
    // Am inlocuit \xDFC cu \337C pentru a genera cercul de grad corect pe LCD
    snprintf(timeBuffer, (MESSAGE_LENGTH - 1), "  %s %02d %s %04d Temp: %d\337C RH: %d%% ", 
             Day[dayOfWeek], dayOfMonth, Mon[month], year, temperature, umiditate);
  } else {
    snprintf(timeBuffer, (MESSAGE_LENGTH - 1), "  %s %02d %s %04d Meteo Error ", 
             Day[dayOfWeek], dayOfMonth, Mon[month], year);
  }
  
  for (i = 0; i < 3; i++) {
    timeBuffer[(MESSAGE_LENGTH - 2) + i] = ' ';
  }

  String timestr = String(timeBuffer);
  stringLength = timestr.length();
}

// Toate functiile de desenare custom din schita ta originală au fost pastrate intacte mai jos
void scrollDisplay(int line, int speed) {
  int i;
  if ((currentMillis - scrollMillis) > speed) {
    scrollMillis = millis();

    if (g_nPosition < stringLength - DISPLAY_WIDTH) {
      for (i = 0; i < DISPLAY_WIDTH; i++) {
        lcd.setCursor(i, line);
        lcd.print(timeBuffer[g_nPosition + i]);
        lcd.print(timeBuffer[stringLength] = ' ');
      }
    } else {
      int nChars = stringLength - g_nPosition;
      for (i = 0; i < nChars; i++) {
        lcd.setCursor(i, line);
        lcd.print(timeBuffer[g_nPosition + i]);
        lcd.print(timeBuffer[stringLength] = ' ');
      }
      for (i = 0; i < (DISPLAY_WIDTH - nChars); i++) {
        lcd.setCursor(nChars + i, line);
        lcd.print(timeBuffer[i]);
        lcd.print(timeBuffer[stringLength] = ' ');
      }
    }

    g_nPosition++;
    if (g_nPosition >= stringLength) {
      g_nPosition = 0;
    }
  }
}

void printTwoNumber(uint8_t number, uint8_t position) {
  int digit0 = number % 10;
  int digit1 = number / 10;

  lcd.setCursor(position, 0);
  lcd.write(bn1[digit1 * 3]); lcd.write(bn1[digit1 * 3 + 1]); lcd.write(bn1[digit1 * 3 + 2]);
  lcd.write(B);
  lcd.write(bn1[digit0 * 3]); lcd.write(bn1[digit0 * 3 + 1]); lcd.write(bn1[digit0 * 3 + 2]);

  lcd.setCursor(position, 1);
  lcd.write(bn2[digit1 * 3]); lcd.write(bn2[digit1 * 3 + 1]); lcd.write(bn2[digit1 * 3 + 2]);
  lcd.write(B);
  lcd.write(bn2[digit0 * 3]); lcd.write(bn2[digit0 * 3 + 1]); lcd.write(bn2[digit0 * 3 + 2]);

  lcd.setCursor(position, 2);
  lcd.write(bn3[digit1 * 3]); lcd.write(bn3[digit1 * 3 + 1]); lcd.write(bn3[digit1 * 3 + 2]);
  lcd.write(B);
  lcd.write(bn3[digit0 * 3]); lcd.write(bn3[digit0 * 3 + 1]); lcd.write(bn3[digit0 * 3 + 2]);
}

void printColons(uint8_t position) {
  lcd.setCursor(position, 0); lcd.write(3);
  lcd.setCursor(position, 2); lcd.write(6);
}

void printNoColons(uint8_t position) {
  lcd.setCursor(position, 0); lcd.write(B);
  lcd.setCursor(position, 2); lcd.write(B);
}

void DefineLargeChar() {
  uint8_t cc1[8] = { B11100, B11110, B11111, B11111, B11111, B11111, B11111, B11111 };
  uint8_t cc2[8] = { B11111, B11111, B11111, B11111, B11111, B00000, B00000, B00000 };
  uint8_t cc3[8] = { B00000, B00000, B00000, B11111, B11111, B11111, B11111, B11111 };
  uint8_t cc4[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B01111, B00111 };
  uint8_t cc5[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B00000, B00000 };
  uint8_t cc6[8] = { B00000, B11111, B11111, B11111, B11111, B11111, B00000, B00000 };
  uint8_t cc7[8] = { B00000, B11100, B11110, B11111, B11111, B11111, B11111, B11111 };
  uint8_t cc8[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };

  lcd.createChar(1, cc1); lcd.createChar(2, cc2); lcd.createChar(3, cc3); lcd.createChar(4, cc4);
  lcd.createChar(5, cc5); lcd.createChar(6, cc6); lcd.createChar(7, cc7); lcd.createChar(8, cc8);
}
