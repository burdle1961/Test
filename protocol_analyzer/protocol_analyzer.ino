//Serial2 : Command (from Program to controller)
//Serial3 : response (from controller to Program)

boolean LF = false;
char    hex[5];
int     counter;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(38400);
  Serial3.begin(38400);
}

void loop() {

  if (Serial2.available()) {
    if (!LF) {
      LF = true;
      Serial.println();
    }
    uint8_t c = Serial2.read();
    sprintf(hex, "%02x ", c);
    Serial.print(hex);
  }
  if (Serial3.available()) {
    if (LF) {
      LF = false;
      Serial.println();
      counter = 0;
    }
    uint8_t c = Serial3.read();
    sprintf(hex, "%02x", c);
    Serial.print(hex);
    counter++;
    if (counter > 80) {
      counter = 0;
      Serial.println();
    }
  }
    
}
