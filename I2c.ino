#include <Wire.h>

void initI2C() {
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  Serial.println("Getting ACCEL ID:");
  Serial.println(readByte(0x0F));
}

byte readByte(byte reg) {
  Wire.beginTransmission(ACCEL);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(byte(ACCEL), byte(1));
  
  if (Wire.available() == 1) {
    return Wire.read();
  }
  return 0;
}

int16_t readReg(byte reg) {
  byte low = readByte(reg);
  byte high = readByte(reg + 1);
  return int16_t(high << 8) | (int16_t)low;
}

void writeReg(byte reg, byte val) {
  Wire.beginTransmission(ACCEL);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
