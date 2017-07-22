import Adafruit_BBIO.ADC as ADC
import time
import sys

ADC.setup()
from time import sleep
analogPin="P9_39"
while(1):
  #Read the voltage of pin P9_39. The value returned is a float
  #between 0 and 1 where 0 is 0 volts and 1 is 1.8 volts
  #Our voltage divider steps the 5v line down to a max of 1.667 volts
  #so we adjust the input value to a 0-1 where 0 is 0 volts and 1 is
  #5 volts
  v  = ADC.read(analogPin)
  b = v / (1.667 / 1.8)
  timestamp = int(round(time.time() * 1000))
  print "{},{},{}".format(timestamp, v, b)
  sys.stdout.flush()
  sleep(2)
