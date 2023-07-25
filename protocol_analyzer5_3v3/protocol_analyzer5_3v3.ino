#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

#include "NotoSansBold15.h"
#include "NotoSansBold36.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36

TFT_eSPI tft = TFT_eSPI();

uint8_t command[8] = {0xf1,0xf1,0x00,0x00,0x00,0xc3,0x28,0xbe}; 

char    hex[5];
long    counter = 0;
unsigned long prev;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(38400);
  prev = millis();
 
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(AA_FONT_LARGE); // Load another different font

  tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
  tft.setCursor(0, 0);
}

void loop() {

  if ((millis() - prev) > 2000) {

    for (int i = 0 ; i < 8 ; i++) Serial.write(command[i]);
    prev = millis();
    counter = 0;

  }

  if (Serial.available()) {
    uint8_t c = Serial.read();
    sprintf(hex, "%02x.", c);
    counter++;

    tft.print(hex);
    tft.print(" ");
  }
}
