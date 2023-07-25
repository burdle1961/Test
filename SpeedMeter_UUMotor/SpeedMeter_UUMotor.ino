#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>
#include "Button2.h"
#include "EEPROM.h"

#include <PNGdec.h>
#include "HA40.h"     // Brand Logo
#include "HARIDE40.h"

#define MAX_IMAGE_WDITH 240 // Adjust for your images

#include "NotoSansBold15.h"
#include "NotoSansBold36.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36

PNG png;                // PNG decoder inatance       
int16_t xpos = 0;
int16_t ypos = 0;

TFT_eSPI tft = TFT_eSPI();  // Invoke library
TFT_eSprite speed = TFT_eSprite(&tft);      // km/h sprite 선언
TFT_eSprite dist = TFT_eSprite(&tft);       // distance sprite 선언
int   wheelDiameter = 10;                   // wheel diameter in inches

#include "Free_Fonts.h" // Include the header file attached to this sketch
// Specify sprite 160 x 128 pixels (needs 40Kbytes of RAM for 16 bit colour)
#define IWIDTH  220
#define IHEIGHT 160

boolean LF = false;
char    hex[5];
int     counter;

uint8_t packet[512];      // receive buffer
uint8_t pCount;           // Received data length
uint8_t pStart = 0xF1;    // packet start header

float Battery;  
float krpm;
float Rpm;
float throttle;
float kmh;

// index within data packet.
int   iBattery = 50;      // Battery data index in the packet
int   ikrpm = 78;         // rpm in Kilo index in the packet
int   ithrottle = 54;     // throttle value index in the packet

// EEPROM에 기록할 데이터
// unsigned long --> 4 bytes * 3
#define EEPROM_SIZE 12

union RTIME {
  uint8_t val[EEPROM_SIZE];
  unsigned long trip[3];
  // trip[0] : 구입후 누적 운행 거리 (x10 kilometers)
  // trip[1] : 배터리 완충 후의 누적 운행 거리 (x10 kilometers)
  // trip[2] : power on 이후 누적 운행 거리 (meters)
} runtime;

unsigned long thistime;       // power-on 이후의 운행 시간 (msec)
unsigned long oldtime;        // 이전 데이터 수신 시각 millis() 기준
unsigned long cumtime;        // 최종 EEPROM write 시각 millis() 기준
unsigned long checkpoint;     // last write time to EEPROM  --> EEPROM write 수명 관리를 위한 write 간격 제어용.

float mdistance = 0.0f;
float ddistance = 0.0f;
int   displayMode = 2;        // 2 : 현재 주행 거리 (default)

// 3 buttons on DISPLAY MODULE
volatile boolean buttonL = false;       // button Left (GPIO38)
volatile boolean buttonC = false;       // button Center (GPIO37)
volatile boolean buttonR = false;       // botton Right (GPIO39)

// button 눌림을 감지하는 ISR routine

void IRAM_ATTR isrL() {
    buttonL = true;
}
void IRAM_ATTR isrC() {
    buttonC = true;
}
void IRAM_ATTR isrR() {
    buttonR = true;
}

void display_logo();

void Hex2Float(float *result, uint8_t *source) {
uint8_t buf[4];

  buf[0] = *(source+3);
  buf[1] = *(source+2);
  buf[2] = *(source+1);
  buf[3] = *source;

  memcpy(result, buf, 4);
}

void setup() {

  pinMode(37, INPUT_PULLUP);      // Button Center
  pinMode(38, INPUT_PULLUP);      // Button Left
  pinMode(39, INPUT_PULLUP);      // Button Right
  attachInterrupt(37, isrC, FALLING);    // 스위치를 누르는 순간 확인
  attachInterrupt(38, isrL, FALLING);    // 스위치를 누르는 순간 확인
  attachInterrupt(39, isrR, FALLING);    // 스위치를 누르는 순간 확인

  // put your setup code here, to run once:
  // Serial.begin(115200);
  Serial.begin(38400);       // MX-ES controller UART 38400 bps
  // delay(500);                 // delay for Serial ready.

  tft.init();
  tft.setRotation(0);     // Portrait - 버튼이 아래쪽으로,
  tft.fillScreen(TFT_BLACK);
  tft.loadFont(AA_FONT_LARGE); // Load another different font

  // display_logo();

  if (EEPROM.begin(EEPROM_SIZE))
  {
    delay(100);       // power on 후 EEPROM access를 위한 대기 시간 -->> 필요 여부 다시 확인 할것 (Feb20, 2023)

//    tft.println("Read previous saved data from EEPROM");

    //  EEPROM에 기록된 운행 정보를 읽어 들임.
    for (int i = 0 ; i < EEPROM_SIZE ; i++) runtime.val[i] = EEPROM.read(i);

    // EEPROM에 저장된 직전의 운행 시간을 누적 시간 값에 추가
    runtime.trip[0] += runtime.trip[2];   // 누적 거리 (after purchase)
    runtime.trip[1] += runtime.trip[2];   // 누적 거리 (after recharge)
    runtime.trip[2] = 0;                  // 이동 거리 (after power-on, initializing)

    // 기동시에 누적시간 처리 결과를 EEPROM에 다시 저장
    for (int i = 0 ; i < EEPROM_SIZE ; i++) EEPROM.write(i, runtime.val[i]);
    EEPROM.commit(); 
    delay(100);                                 //write 후 대기???

    // sprite 기본 설정
    speed.createSprite(IWIDTH, IHEIGHT);
    speed.fillSprite(TFT_BLACK);
    speed.setTextSize(2);
    speed.setTextColor(TFT_WHITE, TFT_BLACK);

    // tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
    // tft.setCursor(50, 25);
    // tft.print("EEPROM OK!!");

// 주행 거리 표시용 sprite를 별도로 처리 --> flicking 때문에 일단 제외
//    dist.createSprite(IWIDTH, IHEIGHT/4);
//    dist.fillSprite(TFT_BLACK);
//    dist.setTextSize(2);
//    dist.setTextColor(TFT_WHITE, TFT_BLACK);
    
  } else {
    // Serial.println("EEPROM failed.");
    // while (1) ;
    tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
    tft.setCursor(50, 25);
    tft.print("EEPROM Error");
  }
  display_logo();

  // 이동 거리 계산을 위하여 이전 데이터 수신 시각 기억
  // oldtime : 이전 수신 시각, checkpoint : EEPROM update 시각
  oldtime = millis();
  checkpoint = oldtime;
  // millis()의 overflow는 power on 이후 운행 지속시간이 < 8hr 일 것이므로 별도 처리 생략.
 
}

int state;
unsigned long lastReceived;
uint8_t command[8] = {0xf1,0xf1,0x00,0x00,0x00,0xc3,0x28,0xbe}; 

void loop() {
uint8_t c;
uint8_t buf[4];

  // flushing data from buffer
  while (Serial.available()) {
    Serial.read();
    delay(30);
  }
  // Request data packet to controller
  delay(100);
  tft.fillRect(0, 100, 20, 20, TFT_GREEN);
  for (int i = 0 ; i < 8 ; i++) Serial.write(command[i]);
  delay(100);
  
  lastReceived = millis();        // last timing of receive.
  state = 0;                      // wait for packet header

//  Serial.print("Wait for Header  ==>");
  // tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
  // tft.setCursor(50, 200);
  // tft.print("Waiting");

// Wait to finish receiving headers of 4 bytes. 

  while (state < 4) {
    if (Serial.available()) { 
      c = Serial.read();
      NextState(c);
    } 

    if ((millis() - lastReceived) > 2000) {
      tft.fillRect(20, 100, 20, 20, TFT_YELLOW);
      tft.fillRect(0, 100, 20, 20, TFT_BLACK);
      return;     // 1 sec time-out. repeat loop() 
      // break;
    }
  }  
  tft.fillRect(20, 100, 20, 20, TFT_BLUE);

//  Serial.println("Header Found");
  // tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
  // tft.setCursor(50, 25);
  // tft.print("Received.");
  tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
  tft.setCursor(50, 200);

  pCount = 0;
//  Serial.println("Read Data");

// Receiving real payload of 0xBF bytes with 2000 msec timeout
  while (true) {
    
    if (Serial.available()) {
      c = Serial.read();
    } else {      // time-out exception 
      if ((millis() - lastReceived) > 2000) break;
      else                                  continue;
    }
    
    packet[pCount++] = c;
       
    if (pCount == 0xBF) {   // all data received. (0XC3 - 4)
      // tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
      // tft.setCursor(50, 90);
      // tft.print("All data arrived...");

//      if (!CheckSum()) break;       // checksum error handing 

//      for (int i = 40 ; i < 100 ; i++) {
//        Serial.print(packet[i], HEX);
//        Serial.print(" ");
//      }
//      Serial.println();

//      buf[0] = packet[iBattery+3];
//      buf[1] = packet[iBattery+2];
//      buf[2] = packet[iBattery+1];
//      buf[3] = packet[iBattery+0];
//      memcpy(&Battery, buf, 4);       // battery voltage;
      
      Hex2Float(&Battery, &packet[iBattery]);
      Battery = Battery * 1000;

//      buf[0] = packet[ikrpm+3];
//      buf[1] = packet[ikrpm+2];
//      buf[2] = packet[ikrpm+1];
//      buf[3] = packet[ikrpm];
//      
//      memcpy(&krpm, buf, 4);             // kRPM value
      
      Hex2Float(&krpm, &packet[ikrpm]);
      Rpm = krpm * 1000 / 4;                      // convert to RPM (4 = speedRatio);

      kmh = Rpm * 60 * wheelDiameter * 2.54 * 3.14 / 10000;

//      buf[0] = packet[ithrottle+3];
//      buf[1] = packet[ithrottle+2];
//      buf[2] = packet[ithrottle+1];
//      buf[3] = packet[ithrottle];
//      
//      memcpy(&throttle, buf, 4);             // kRPM value

      Hex2Float(&throttle, &packet[ithrottle]);

      break;

    }       // finish a cycle of process data to display
  }

  delay(500);
  display_speed((int)kmh);
}

// State transition 
// State 0 : initial (wait for header)
// State 1 : 1st byte of header received.
// State 2 : 2nd byte of header received.
// State 3 : 3rd byte of header received.
// State 4 : 4th byte of header received. (ready to accept data part)
// #0 - (0xF1) - #1 - (0xF1) - #2 - (0xC3) - #3 - (0x00) - #4
 
void NextState(uint8_t data) {

  // sprintf(hex,"%02X", data);
  // tft.print(hex);
  // sprintf(hex,"[%d] ", state);
  // tft.print(hex);

  if (state == 4) return;

  if (state == 0 && data == pStart) {
//    Serial.print(" 0 -> 1 ");
    tft.fillRect(20, 100, 5, 20, TFT_BLUE);

    state = 1;
    return;
  }
  
  if (state == 1) {
    if (data == pStart) {
//      Serial.print(" 1 -> 2 ");
      tft.fillRect(25, 100, 5, 20, TFT_BLUE);

      state = 2;
      return;
    } else {
      state = 0;
      return;
    }
  }
  
  if (state == 2) {
      if (data == 0xC3) {
//        Serial.print(" 2 -> 3 ");
        tft.fillRect(30, 100, 5, 20, TFT_BLUE);
        state = 3;
        return;
      }
      if (data != 0xF1) {
        state = 0;
        return;
      }
  }
  
  if (state == 3) {
    if (data == 0x00) {
//        Serial.print(" 3 -> 4 ");
        tft.fillRect(35, 100, 5, 20, TFT_BLUE);
        state = 4;
        return;
    } else {  
        state = 0;
        return;
    }
  }
}

//=========================================v==========================================
//                                      pngDraw
//====================================================================================
// This next function will be called during decoding of the png file to
// render each image line to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
// Callback function to draw pixels to the display
void pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WDITH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}

void display_logo()
{

  tft.fillRect(0, 4, 239, 42, TFT_GREEN);

  xpos = 20; ypos = 5;
  int16_t rc = png.openFLASH((uint8_t *) HA40, sizeof(HARIDE40), pngDraw);
  // int16_t rc = png.openFLASH((uint8_t *)HA_HARIDE, sizeof(HA_HARIDE), pngDraw);
  if (rc == PNG_SUCCESS) {
    //Serial.println("Successfully png file");
    //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    tft.startWrite();
    uint32_t dt = millis();
    rc = png.decode(NULL, 0);
    //Serial.print(millis() - dt); Serial.println("ms");
    tft.endWrite();
    // png.close(); // not needed for memory->memory decode
  }
  
  xpos = 60; ypos = 5;
  rc = png.openFLASH((uint8_t *) HARIDE40, sizeof(HARIDE40), pngDraw);
  if (rc == PNG_SUCCESS) {
    //Serial.println("Successfully png file");
    //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    tft.startWrite();
    uint32_t dt = millis();
    rc = png.decode(NULL, 0);
    //Serial.print(millis() - dt); Serial.println("ms");
    tft.endWrite();
    // png.close(); // not needed for memory->memory decode
  }

  // xpos = 150; ypos = 110;
  // rc = png.openFLASH((uint8_t *) HA_black, sizeof(HA_black), pngDraw);
  // if (rc == PNG_SUCCESS) {
  //   //Serial.println("Successfully png file");
  //   //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
  //   tft.startWrite();
  //   uint32_t dt = millis();
  //   rc = png.decode(NULL, 0);
  //   //Serial.print(millis() - dt); Serial.println("ms");
  //   tft.endWrite();
  //   // png.close(); // not needed for memory->memory decode
  // }

  delay(100);

}


// void display_speed(int spd, uint8_t *packet, int cnt) {
void display_speed(int spd) {
char  string[20];

//  img.fillScreen(LFN_BACKGROUND);
//  delay(10);
  // speed.drawRect(0,0, IWIDTH, IHEIGHT, TFT_BLACK);
  // delay(5);
  // speed.pushSprite(10, 170);
//  dist.drawRect(0,0, IWIDTH, IHEIGHT/4, LFN_BACKGROUND);
//  dist.fillScreen(LFN_BACKGROUND);
//  dist.pushSprite(10,170);
  
  // 10 km/h 보다 느린 경우, 자리수 위치 보정을 위하여 leading 0는 바탕색(black)으로 표시
  if (spd < 100) {
      sprintf(string, "%3.1f", (float)spd/10);
      speed.setTextColor(TFT_BLACK, TFT_BLACK); // leading zero with black
      speed.drawString("0", 0, 0, 7);         
      speed.setTextColor(TFT_WHITE, TFT_BLACK);
      speed.drawString(string, 64, 0, 7);
  } else {
      sprintf(string, "%4.1f", (float)spd/10);
      speed.setTextColor(TFT_WHITE, TFT_BLACK);
      speed.drawString(string, 0, 0, 7);
  }
  speed.drawLine(0,100, IWIDTH-10, 100, TFT_BLUE);
  speed.setTextColor(TFT_YELLOW, TFT_BLACK);
  speed.drawString("km/h", 160, 104, 1);

  // // ddistance 는 buttonC에 따라, Power-on/recharge후/구입후 운행 거리를 표시
  // ddistance = runtime.trip[displayMode];

  // switch (displayMode) {
  //   case 0 :
  //     sprintf(string, "Trip (C)");
  //     break;
  //   case 1 :
  //     sprintf(string, "Trip (R)");
  //     break;
  //   case 2 :
  //     sprintf(string, "Trip (T)");
  //     break;
  //   default :
  //     sprintf(string,"Trip");
  //     break;
  // }
  // speed.drawString(string, 5, 140, 1);

  // if (ddistance < 1000) {
  //   sprintf(string, "%4.0f Meters", ddistance);  
  // } else {
  //   sprintf(string, "%-5.2f KM's", ddistance/1000);
  // }
  // speed.drawString(string, 100, 140, 1);
  // delay(5);

  speed.pushSprite(10,180);

  delay(100);
}
