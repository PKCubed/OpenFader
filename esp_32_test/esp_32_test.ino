#include <Wire.h>

#define OPEN_FADER_ADDRESS 0x42

void send_data(uint8_t data_byte_1, uint8_t data_byte_2) {
  Wire.beginTransmission(OPEN_FADER_ADDRESS);
  Wire.write(data_byte_1);
  Wire.write(data_byte_2);
  Wire.endTransmission();
}

uint8_t read_byte(uint8_t register_to_read) {
  byte data = 0xFF;
  Wire.beginTransmission(OPEN_FADER_ADDRESS);
  Wire.write(register_to_read);
  Wire.endTransmission(false);
  Wire.requestFrom(OPEN_FADER_ADDRESS, 1);
  data = Wire.read();
  return data;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin(15, 16);
  Serial.println("Hello World!");
}

int fader_pos = 0;

void loop() {
  // put your main code here, to run repeatedly:
  fader_pos = read_byte(0x01);
  Serial.println(fader_pos);
  delay(100);
}
