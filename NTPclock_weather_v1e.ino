// ============================================================================
// Ceas LCD 20x4 I2C (8/9) cu Open-Meteo Craiova pentru ESP32-S3 SuperMini (Nicu Florica / niq_ro)
// Adaptat dupa schita originala de la https://www.youtube.com/watch?v=8txWH-ZVqIE&t=1049s 
// ============================================================================
/*
       +-----------------------+              +-----------------------+
       |   ESP32-S3 SUPERMINI  |              |    AFIȘAJ LCD 20X4    |
       |     (Vedere de sus)   |              |   (Modulul I2C spate) |
       +-----------------------+              +-----------------------+
       |                       |              |                       |
       |  [ 5V ]---------------+------------->| [ VCC ]               |
       |  [ GND]---------------+------------->| [ GND ]               |
       |                       |              |                       |
       |  [ IO8 ] (SDA)--------+------------->| [ SDA ]               |
       |  [ IO9 ] (SCL)--------+------------->| [ SCL ]               |
       |                       |              |                       |
       +-----------------------+              +-----------------------+
v.1.a - adaugat actualizare ora vara/iarna automat (DST)
v.1.b - adaugat test animatie tip "flip clock" pe 3 randuri pentru unitatile de minute (0..9)
v.1.c - extins animatia flip clock la toate cifrele + tranzitie la pornire din 00:00 in ora curenta
v.1.d - ascundere automata indice UV din textul derulant cand UV < 0.1
v.1.e - rafinat animatia flip clock in 4 trepte (randul 1 coboara pe 2, apoi apare randul 1 nou)
        integrat animatia in 4 trepte pe toate cifrele + readaugat tranzitia de pornire 00:00 -> ora curenta
*/
    
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

char *Mon[] = {"Ian", "Feb", "Mar", "Apr", "Mai", "Iun", "Iul", "Aug", "Sep", "Oct", "Noi", "Dec"};
char *Day[] = {"Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata"};

char ntp_server1[] = "pool.ntp.org";
char ntp_server2[] = "time.nist.gov";

// Variabile meteo Open-Meteo Craiova
int temperature = 0;
int umiditate = 0;
bool dateMeteoValide = false;
String stareVreme = "---"; 
int presiune_mmhg = 0;
int w_pres_mmhg = 0;
float w_temp = 0.0;
int w_hum = 0;
float w_uv_index = 0.0;
unsigned long ultimaVerificareMeteo = 0;

int msecond, second, minute, hour, dayOfWeek, dayOfMonth, month, year, dots;
unsigned long lastSecond = millis();
unsigned long currentMillis = 0;
unsigned long scrollMillis = 0;
unsigned long lastUpdateTime = 0;
unsigned long lastUpdateData = 0;
unsigned int stringLength;
char timeBuffer[100]; 

const int MESSAGE_LENGTH = sizeof(timeBuffer);
const int DISPLAY_WIDTH = 20;
int g_nPosition = 0;
String upperSec = "0";
String underSec = "0";

// Variabile pentru memorarea ultimelor cifre afisate (pentru animatia flip pe toate cele 4 cifre)
int prevHourTens = 0;
int prevHourUnit = 0;
int prevMinTens  = 0;
int prevMinUnit  = 0;

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
void printSingleDigit(uint8_t digit, uint8_t position);
void flipSingleDigit(uint8_t oldDigit, uint8_t newDigit, uint8_t position);
void startupFlipTransition();
void printColons(uint8_t position);
void printNoColons(uint8_t position);
void getWeatherCraiova();
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

  // Fus orar Romania cu schimbare automata vara/iarna
  configTime(0, 0, ntp_server1, ntp_server2);
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();
  
  lcd.clear();
  lcd.print("Sincronizare NTP...");
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.printf(
    "DST=%d  Ora=%02d:%02d:%02d\n",
    timeinfo.tm_isdst,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  lcd.clear();
  updateTime();
  getWeatherCraiova();

  // Executa animatia de pornire din 00:00 in ora curenta
  startupFlipTransition();
}

void loop() {
  currentMillis = millis();

  // Verificare Wi-Fi si actualizari periodice
  if (WiFi.status() != WL_CONNECTED) {
    wifiMulti.run();
  }

  if ((currentMillis - lastUpdateTime) > 1000)
    updateTime();
  if ((currentMillis - lastUpdateData) > 900000) getWeatherCraiova(); // Update meteo la 15 minute
  if (year < 2000) updateTime();

  dots = (currentMillis % 1000) < 500;
  if (dots) printColons(9); else printNoColons(9);

  if ((millis() - lastSecond) > 1000) {
    lastSecond = millis();
    updateTime();

    if (second < 10) {
      upperSec = "0";
      underSec = String(second);
    } else {
      upperSec = String(second / 10);
      underSec = String(second % 10);
    }

    int curHourTens = hour / 10;
    int curHourUnit = hour % 10;
    int curMinTens  = minute / 10;
    int curMinUnit  = minute % 10;

    // 1. Zeci de ore (pozitia 1)
    if (curHourTens != prevHourTens) {
      flipSingleDigit(prevHourTens, curHourTens, 1);
      prevHourTens = curHourTens;
    } else {
      printSingleDigit(curHourTens, 1);
    }

    // 2. Unitati de ore (pozitia 5)
    if (curHourUnit != prevHourUnit) {
      flipSingleDigit(prevHourUnit, curHourUnit, 5);
      prevHourUnit = curHourUnit;
    } else {
      printSingleDigit(curHourUnit, 5);
    }

    // 3. Zeci de minute (pozitia 11)
    if (curMinTens != prevMinTens) {
      flipSingleDigit(prevMinTens, curMinTens, 11);
      prevMinTens = curMinTens;
    } else {
      printSingleDigit(curMinTens, 11);
    }

    // 4. Unitati de minute (pozitia 15)
    if (curMinUnit != prevMinUnit) {
      flipSingleDigit(prevMinUnit, curMinUnit, 15);
      prevMinUnit = curMinUnit;
    } else {
      printSingleDigit(curMinUnit, 15);
    }

    // Afisare secunde in dreapta sus
    lcd.setCursor(19, 0);
    lcd.print(upperSec);
    lcd.setCursor(19, 1);
    lcd.print(underSec);
  }

  concatenate();
  scrollDisplay(3, 350); // Derulare text pe linia 4
}

// ============================================================================
// FUNCTII ANIMATIE FLIP CLOCK & TRANZITIE PORNIRE
// ============================================================================

// Tranzitie la pornire: afiseaza 00:00 si face flip secvential catre ora curenta
void startupFlipTransition() {
  // Afisare initiala 00:00
  printSingleDigit(0, 1);
  printSingleDigit(0, 5);
  printColons(9);
  printSingleDigit(0, 11);
  printSingleDigit(0, 15);

  lcd.setCursor(19, 0); lcd.print("0");
  lcd.setCursor(19, 1); lcd.print("0");

  delay(550); // Pauza scurta pentru a se vedea 00:00 inainte de declansare

  // Reimprospatam ora exacta inainte de tranzitie
  updateTime();
  int targetHourTens = hour / 10;
  int targetHourUnit = hour % 10;
  int targetMinTens  = minute / 10;
  int targetMinUnit  = minute % 10;

  // Flip pe rand de la stanga la dreapta folosind animatia in 4 trepte
  if (targetHourTens != 0) {
    flipSingleDigit(0, targetHourTens, 1);
    delay(80);
  }
  prevHourTens = targetHourTens;

  if (targetHourUnit != 0) {
    flipSingleDigit(0, targetHourUnit, 5);
    delay(80);
  }
  prevHourUnit = targetHourUnit;

  if (targetMinTens != 0) {
    flipSingleDigit(0, targetMinTens, 11);
    delay(80);
  }
  prevMinTens = targetMinTens;

  if (targetMinUnit != 0) {
    flipSingleDigit(0, targetMinUnit, 15);
  }
  prevMinUnit = targetMinUnit;

  // Actualizare finala pentru secundele curente
  updateTime();
  if (second < 10) {
    upperSec = "0";
    underSec = String(second);
  } else {
    upperSec = String(second / 10);
    underSec = String(second % 10);
  }
  lcd.setCursor(19, 0); lcd.print(upperSec);
  lcd.setCursor(19, 1); lcd.print(underSec);
  lastSecond = millis();
}

// Deseneaza o singura cifra mare (3 coloane x 3 randuri) la pozitia dorita
void printSingleDigit(uint8_t digit, uint8_t position) {
  lcd.setCursor(position, 0);
  lcd.write(bn1[digit * 3]); lcd.write(bn1[digit * 3 + 1]); lcd.write(bn1[digit * 3 + 2]);

  lcd.setCursor(position, 1);
  lcd.write(bn2[digit * 3]); lcd.write(bn2[digit * 3 + 1]); lcd.write(bn2[digit * 3 + 2]);

  lcd.setCursor(position, 2);
  lcd.write(bn3[digit * 3]); lcd.write(bn3[digit * 3 + 1]); lcd.write(bn3[digit * 3 + 2]);
}

// Executa animatia de cadere in 4 trepte intre oldDigit si newDigit
void flipSingleDigit(uint8_t oldDigit, uint8_t newDigit, uint8_t position) {
  int flipDelay = 110; // Viteza fiecarui cadru in milisecunde

  // PASUL 1: Randul 1 vechi coboara pe Randul 2; Randul 1 ramane gol (clapa s-a desprins)
  lcd.setCursor(position, 0);
  lcd.write(B); lcd.write(B); lcd.write(B);

  lcd.setCursor(position, 1);
  lcd.write(bn1[oldDigit * 3]); lcd.write(bn1[oldDigit * 3 + 1]); lcd.write(bn1[oldDigit * 3 + 2]);

  delay(flipDelay);

  // PASUL 2: Pe Randul 1 apare Randul 1 al cifrei NOI (in spate)
  lcd.setCursor(position, 0);
  lcd.write(bn1[newDigit * 3]); lcd.write(bn1[newDigit * 3 + 1]); lcd.write(bn1[newDigit * 3 + 2]);

  delay(flipDelay);

  // PASUL 3: Clapa coboara jos -> Randul 2 primeste Randul 2 NOU, Randul 3 primeste Randul 2 VECHI
  lcd.setCursor(position, 1);
  lcd.write(bn2[newDigit * 3]); lcd.write(bn2[newDigit * 3 + 1]); lcd.write(bn2[newDigit * 3 + 2]);

  lcd.setCursor(position, 2);
  lcd.write(bn2[oldDigit * 3]); lcd.write(bn2[oldDigit * 3 + 1]); lcd.write(bn2[oldDigit * 3 + 2]);

  delay(flipDelay);

  // PASUL 4: Apare cifra NOUA completa pe toate cele 3 randuri
  printSingleDigit(newDigit, position);
}

// ============================================================================
// METEO OPEN-METEO CRAIOVA & TIMP NTP
// ============================================================================

void getWeatherCraiova() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[VREME] WiFi deconectat!");
    return; 
  }

  WiFiClient client;
  HTTPClient http;
  
  String urlWeather = "http://api.open-meteo.com/v1/forecast?latitude=44.31&longitude=23.80&current=temperature_2m,relative_humidity_2m,pressure_msl,weather_code,uv_index&forecast_days=1&timezone=auto";
  
  Serial.println("[VREME] Se preiau datele meteo și UV...");
  http.begin(client, urlWeather);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject current = doc["current"];
      w_temp = current["temperature_2m"].as<float>(); 
      w_hum = current["relative_humidity_2m"].as<int>(); 
      w_uv_index = current["uv_index"].as<float>();
      
      float pres_hpa = current["pressure_msl"].as<float>(); 
      if (pres_hpa > 0) { 
        w_pres_mmhg = pres_hpa * 0.750062 + 0.5; 
      }
      
      temperature = (int)round(w_temp); 
      umiditate = w_hum;                
      if (umiditate > 99) umiditate = 99;
      presiune_mmhg = w_pres_mmhg;      

      int wmo = current["weather_code"].as<int>(); 
      if (wmo == 0)                    stareVreme = "Senin";
      else if (wmo >= 1 && wmo <= 3)   stareVreme = "Innorat";
      else if (wmo >= 45 && wmo <= 48) stareVreme = "Ceata";
      else if (wmo >= 51 && wmo <= 55) stareVreme = "Burnita";
      else if (wmo >= 61 && wmo <= 65) stareVreme = "Ploaie";
      else if (wmo >= 71 && wmo <= 75) stareVreme = "Zapada";
      else if (wmo >= 80 && wmo <= 82) stareVreme = "Averse";
      else if (wmo >= 95)              stareVreme = "Furtuna";
      else                             stareVreme = "Altfel";

      dateMeteoValide = true; 
      lastUpdateData = millis();

      Serial.println("[VREME] Meteo & UV OK -> ");
      Serial.printf("Temp: %.1f°C, Hum: %d%%, UV: %.1f, Pres: %d mmHg, Stare: %s\n", 
                    w_temp, w_hum, w_uv_index, presiune_mmhg, stareVreme.c_str());
    } else {
      dateMeteoValide = false;
    }
  } else {
    Serial.printf("[VREME] Eroare HTTP: %d\n", httpCode);
    dateMeteoValide = false;
  }
  http.end();
}

void updateTime() {
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    year       = timeinfo.tm_year + 1900;
    month      = timeinfo.tm_mon;
    dayOfMonth = timeinfo.tm_mday;
    dayOfWeek  = timeinfo.tm_wday;

    hour       = timeinfo.tm_hour;
    minute     = timeinfo.tm_min;
    second     = timeinfo.tm_sec;

    lastUpdateTime = millis();

    static int oldDay = -1;
    if (oldDay != dayOfMonth) {
      oldDay = dayOfMonth;
      concatenate();
    }
  }
}

void concatenate() {
  int i;
  memset(timeBuffer, ' ', (MESSAGE_LENGTH - 1));
  
  if (dateMeteoValide) {
    if (w_uv_index >= 0.1) {
      snprintf(timeBuffer, (MESSAGE_LENGTH - 1), " %s %02d-%s-%04d Temp: %+d\337C RH: %d%% Pres: %dmmHg UV: %.1f (%s) ", 
               Day[dayOfWeek], dayOfMonth, Mon[month], year, temperature, umiditate, presiune_mmhg, w_uv_index, stareVreme.c_str());
    } else {
      snprintf(timeBuffer, (MESSAGE_LENGTH - 1), " %s %02d-%s-%04d Temp: %+d\337C RH: %d%% Pres: %dmmHg (%s) ", 
               Day[dayOfWeek], dayOfMonth, Mon[month], year, temperature, umiditate, presiune_mmhg, stareVreme.c_str());
    }
  } else {
    snprintf(timeBuffer, (MESSAGE_LENGTH - 1), " %s %02d-%s-%04d (Eroare Meteo) ", 
             Day[dayOfWeek], dayOfMonth, Mon[month], year);
  }
  
  for (i = 0; i < 3; i++) {
    timeBuffer[(MESSAGE_LENGTH - 2) + i] = ' ';
  }

  String timestr = String(timeBuffer);
  stringLength = timestr.length();
}

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
