import mraa
import time
import math
import json
import urllib2
import datetime
import sys
import variance
import median 

REG_X=0x28
REG_Y=0x2A
REG_Z=0x2C

i2c=mraa.I2c(2, True)

i2c.address(0x19)

#Turn the accelometer on
#Turn on the sensor and set the polling frequency to 50Hz
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

def notify(msg):
  data = {'body': msg}
  req = urllib2.Request('http://blynk-cloud.com/b3e42dd400e84c5586f122328b83616f/notify')
  req.add_header('Content-Type', 'application/json')

  urllib2.urlopen(req, json.dumps(data))

def push(pin, value):
  req = urllib2.Request('http://blynk-cloud.com/b3e42dd400e84c5586f122328b83616f/update/V' + str(pin))
  req.add_header('Content-Type', 'application/json')
  req.get_method = lambda: 'PUT'
  urllib2.urlopen(req, "[" + str(value) + "]")

#Number of samples over which to calculate the variance
#50 samples represents about 1 second of data
numSamples = 50
varx = variance.Variance(numSamples)
vary = variance.Variance(numSamples)
varz = variance.Variance(numSamples)
medx = median.Median(3)
medy = median.Median(3)
medz = median.Median(3)

vibrations = 0
machineon = 0
lastpush = 0
vibrationthreshold = 0.05
lastwakeup = int(round(time.time() * 1000))
sleeptime = 180 * 1000
waketime = 5 * 1000
machineoffdelay = 10 * 60 * 1000
stayawakethreshold = 50
machineonthreshold = 5000

while True:
  xint = readReg16(REG_X)
  yint = readReg16(REG_Y)
  zint = readReg16(REG_Z)
  
  x = int16ToFloat(xint)
  y = int16ToFloat(yint)
  z = int16ToFloat(zint)
  
  total = math.sqrt(x**2 + y**2 + z**2)

  medx.add_variable(x)
  medy.add_variable(y)
  medz.add_variable(z)
 
  varx.add_variable(medx.get_median())
  vary.add_variable(medy.get_median())
  varz.add_variable(medz.get_median())

  v = max(varx.get_max(), vary.get_max(), varz.get_max())

  if v > vibrationthreshold:
    vibrations += 1
  elif vibrations > 0:
    vibrations -= 1

  timestamp = int(round(time.time() * 1000))
  print "{},{},{},{},{},{},{}".format(timestamp, x, y, z, total, v, vibrations)
  sys.stdout.flush()

  if vibrations > machineonthreshold:
    machineon = timestamp

  #If all vibrations have stopped, and the machine was on at least 15 mins ago
  #then notify
  if vibrations == 0 and machineon > 0 and timestamp - machineon > machineoffdelay:
    machineon = 0
    print "0,0,0,0,0,0,0,Washing done at {}".format(timestamp)
    dt = datetime.datetime.fromtimestamp(timestamp / 1000)
    notify("Washing done at {}".format(dt))
  elif vibrations > 0 and timestamp - lastpush >= 1000:
    #Send vibration count to graph with Blynk
    push(1, vibrations)
    lastpush = timestamp

  #Micro sleep as long as there is vibration or we are in our 5 second wake period
  if vibrations > stayawakethreshold or timestamp - lastwakeup < waketime:
    time.sleep(0.015)
  else:
    #Otherwise sleep for a couple of minutes
    time.sleep(sleeptime / 1000.0)
    lastwakeup = int(round(time.time() * 1000))

