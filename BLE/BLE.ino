#include <SoftwareSerial.h>

#define RXD_P 8
#define TXD_P 7

SoftwareSerial bluetooth(RXD_P, TXD_P);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  bluetooth.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly:
  if (bluetooth.available()){
    Serial.write(bluetooth.read());
  }
  if(Serial.available()){
    bluetooth.write(Serial.read());
  }

}
