#include <Arduino.h>
#include <Wire.h>

// Definirea pinilor I2C ceruți
#define I2C_SDA 8
#define I2C_SCL 9

void setup() {
  // Inițializare comunicare Serială la 115200 baud
  Serial.begin(115200);
  
  // O mică pauză pentru a permite deschiderea portului Serial pe ESP32-S3
  delay(2000); 
  
  Serial.println("\n==================================");
  Serial.println("Initializare Scanner I2C...");
  Serial.printf("Pini utilizati: SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
  Serial.println("==================================");

  // Pornirea magistralei I2C cu pinii specificați explicit
  Wire.begin(I2C_SDA, I2C_SCL);
}

void loop() {
  byte eroare, adresa;
  int dispozitiveGasite = 0;

  Serial.println("Scanare in curs...");

  // Magistrala I2C folosește adrese de la 1 la 127
  for (adresa = 1; adresa < 127; adresa++) {
    // Începem transmisia către adresa curentă
    Wire.beginTransmission(adresa);
    eroare = Wire.endTransmission();

    // Dacă valoarea returnată este 0, înseamnă că un dispozitiv a răspuns (ACK)
    if (eroare == 0) {
      Serial.print("-> Dispozitiv I2C gasit la adresa: 0x");
      if (adresa < 16) {
        Serial.print("0"); // Adaugă un zero în față pentru formatare estetică
      }
      Serial.print(adresa, HEX);
      Serial.println(" !");

      dispozitiveGasite++;
    }
    // Eroarea 4 înseamnă o problemă tehnică pe linie (scurtcircuit sau lipsă pull-up)
    else if (eroare == 4) {
      Serial.print("Eroare necunoscuta la adresa 0x");
      if (adresa < 16) {
        Serial.print("0");
      }
      Serial.println(adresa, HEX);
    }
  }

  if (dispozitiveGasite == 0) {
    Serial.println("Niciun dispozitiv I2C nu a fost detectat.\n");
  } else {
    Serial.printf("Scanare finalizata. Total dispozitive: %d\n\n", dispozitiveGasite);
  }

  // Așteaptă 5 secunde înainte de următoarea scanare
  delay(5000);
}
