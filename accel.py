import mraa
import time
import math
import urllib2
import datetime
import sys

REG_X=0x28
REG_Y=0x2A
REG_Z=0x2C

i2c=mraa.I2c(2, True)

i2c.address(0x19)

#Turn the accelometer on
#Turn on the sensor and set the polling frequency to 25Hz
i2c.writeReg(0x20, 0x57)

#CTRL_REG4
#1 - BDU: Block data update. This ensures that both the high and the low bytes for each 16bit represent the same sample
#0 - BLE: Big/little endian. Set to little endian
#00 - FS1-FS0: Full scale selection. 00 represents +-2g
#1 - HR: High resolution mode enabled.
#00 - ST1-ST0: Self test. Disabled
#0 - SIM: SPI serial interface mode. Default is 0.
i2c.writeReg(0x23, 0x88)

def readReg16(address):
  low = i2c.readReg(address)
  high = i2c.readReg(address+1)
  return low | (high << 8)

def uint16ToInt(i):
  sign = i & 0x8000
  if sign:
    return i - 0x10000
  else:
    return i

def int16ToFloat(i):
  sint = uint16ToInt(i)
  return float(sint) / 16000

def blink(pin, value):
  urllib2.urlopen("http://blynk-cloud.com/b3e42dd400e84c5586f122328b83616f/update/" + pin + "?value=" + str(value)).read()

while True:
  xint = readReg16(REG_X)
  yint = readReg16(REG_Y)
  zint = readReg16(REG_Z)
  
  x = int16ToFloat(xint)
  y = int16ToFloat(yint)
  z = int16ToFloat(zint)
  
  total = math.sqrt(x**2 + y**2 + z**2)
  
  n = datetime.datetime.now()
  timestamp = int(round(time.time() * 1000))
  print "{},{},{}".format(timestamp, "x", x)
  print "{},{},{}".format(timestamp, "y", y)
  print "{},{},{}".format(timestamp, "z", z)
  sys.stdout.flush()
  time.sleep(0.010)
