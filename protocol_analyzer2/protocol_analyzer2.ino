// Serial2 : TX (command to controller)
// Serial3 : RX (response from controller)

uint8_t command[8] = {0xf1,0xf1,0x00,0x00,0x00,0xc3,0x28,0xbe}; 

char    hex[5];
long    counter = 0;
unsigned long prev;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(38400);
  Serial3.begin(38400);
  prev = millis();
}

void loop() {

//  if (Serial2.available()) {
//    if (!LF) {
//      LF = true;
//      Serial.println();
//    }
//    uint8_t c = Serial2.read();
//    sprintf(hex, "%02x ", c);
//    Serial.print(hex);
//  }

  if ((millis() - prev) > 2000) {
    Serial.println();
    Serial.println();

    for (int i = 0 ; i < 8 ; i++) Serial2.write(command[i]);
    prev = millis();
    counter = 0;
  }
  if (Serial3.available()) {
    uint8_t c = Serial3.read();
    sprintf(hex, "%02x.", c);
    Serial.print(hex);  
    counter++;
    if (counter > 40) {
      counter = 0;
      Serial.println();
    }
  } 
}
