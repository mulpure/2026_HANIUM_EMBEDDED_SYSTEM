#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

int LED = 7;
Adafruit_MPU6050 mpu;

void setup(void) {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  mpu.begin();
  delay(100);
}
void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.print("Acceleration X: ");
  Serial.print(a.acceleration.x);
  Serial.print(", Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(", Z: ");
  Serial.print(a.acceleration.z);
  Serial.print(" m/s^2  ");
  Serial.print("Rotation X: ");
  Serial.print(g.gyro.x);
  Serial.print(", Y: "); 
  Serial.print(g.gyro.y);
  Serial.print(", Z: ");
  Serial.print(g.gyro.z);
  Serial.println(" deg/s");

if(a.acceleration.z<10){
  digitalWrite(LED, HIGH);
}
else if(a.acceleration.z>10){
  digitalWrite(LED, LOW);
}

//Serial.print("Temperature: "); 
//Serial.print(temp.temperature);
//Serial.println(" C");
 
 delay(500);}

