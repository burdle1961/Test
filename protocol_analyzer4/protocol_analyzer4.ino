//Serial2 : Command (from Program to controller)
//Serial3 : response (from controller to Program)

// packet data is started with 0xF1, 0xF1, 0xC3, 0x00
// 0xC3, 0x00 --> packet length including 2 bytes of header (0xf1, 0xf1)

// [0:3] : header (0xf1, 0xf1, 0xc3, 0x00)
// [13]: contracting brake voltage
// [15]: contracting brake delay


boolean LF = false;
char    hex[5];
int     counter;

uint8_t packet[512];     // receive buffer
uint8_t pCount;           // Received data length
uint8_t pStart = 0xF1;    // packet start header

float Battery;  
float krpm;
float Rpm;
float throttle;

// index within data packet.
int   iBattery = 50;      // Battery data index in the packet
int   ikrpm = 78;         // rpm in Kilo index in the packet
int   ithrottle = 54;     // throttle value index in the packet

void Hex2Float(float *result, uint8_t *source) {
uint8_t buf[4];

  buf[0] = *(source+3);
  buf[1] = *(source+2);
  buf[2] = *(source+1);
  buf[3] = *source;

  memcpy(result, buf, 4);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(38400);
  Serial3.begin(38400);

}

int state;
unsigned long lastReceived;

void loop() {
uint8_t c;
uint8_t buf[4];


  // flushing data from buffer
  while (Serial3.available()) Serial3.read();

  // Request data packet to controller
  delay(1000);
  uint8_t command[8] = {0xf1,0xf1,0x00,0x00,0x00,0xc3,0x28,0xbe}; 
  for (int i = 0 ; i < 8 ; i++) Serial2.write(command[i]);
  delay(30);
  

  lastReceived = millis();        // last timing of receive.
  state = 0;                      // wait for packet header

  Serial.print("Wait for Header  ==>");
 
  while (state < 4) {
    if (Serial3.available()) { 
      c = Serial3.read();
//      Serial.print(c);
//      Serial.print(".");
      NextState(c);
    }
  }  

  Serial.println("Header Found");

  pCount = 0;
//  Serial.println("Read Data");

  while (true) {
    
    if (Serial3.available()) {
      c = Serial3.read();
    } else {      // time-out exception 
      if ((millis() - lastReceived) > 2000) break;
      else                                 continue;
    }
    
    packet[pCount++] = c;
       
    if (pCount == 0xBF) {   // all data received. (0XC3 - 4)

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

//      buf[0] = packet[ithrottle+3];
//      buf[1] = packet[ithrottle+2];
//      buf[2] = packet[ithrottle+1];
//      buf[3] = packet[ithrottle];
//      
//      memcpy(&throttle, buf, 4);             // kRPM value

      Hex2Float(&throttle, &packet[ithrottle]);
            
      Serial.print("Battery = ");
      Serial.println(Battery);
      Serial.print("RPM = ");
      Serial.print(Rpm);
      Serial.print(" : ");
      Serial.println(Rpm * 0.94);
      Serial.print("Throttle = ");
      Serial.println(throttle);

//      Serial.print("Battery = ");
//      Serial.print(packet[iBattery], HEX);
//      Serial.print(packet[iBattery+1], HEX);
//      Serial.print(packet[iBattery+2], HEX);
//      Serial.println(packet[iBattery+3], HEX);
//      
//      Serial.print("Throttle = ");
//      Serial.print(packet[ithrottle], HEX);
//      Serial.print(packet[ithrottle+1], HEX);
//      Serial.print(packet[ithrottle+2], HEX);
//      Serial.println(packet[ithrottle+3], HEX);
//
//      Serial.print("RPM = ");
//      Serial.print(packet[ikrpm], HEX);
//      Serial.print(packet[ikrpm+1], HEX);
//      Serial.print(packet[ikrpm+2], HEX);
//      Serial.println(packet[ikrpm+3], HEX);

    
      Serial.println("\n===========================\n");     
      break;

    }
  }

}

// State transition 
// State 0 : initial (wait for header)
// State 1 : 1st byte of header received.
// State 2 : 2nd byte of header received.
// State 3 : 3rd byte of header received.
// State 4 : 4th byte of header received. (ready to accept data part)
// #0 - (0xF1) - #1 - (0xF1) - #2 - (0xC3) - #3 - (0x00) - #4
 
void NextState(uint8_t data) {

  if (state == 4) return;

  if (state == 0 && data == pStart) {
//    Serial.print(" 0 -> 1 ");
    state = 1;
    return;
  }
  
  if (state == 1) {
    if (data == pStart) {
//      Serial.print(" 1 -> 2 ");
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
        state = 4;
        return;
    } else {  
        state = 0;
        return;
    }
  }
}
