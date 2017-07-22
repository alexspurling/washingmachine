
int ledPin = 5;
int intPin = 25;

#define SDA_PIN 21
#define SCL_PIN 22

#define ACCEL 0x19 //I2C device address
#define CTRL_REG1 0x20 //Data rate selection and X, Y, Z axis enable register
#define CTRL_REG2 0x21 //High pass filter selection
#define CTRL_REG3 0x22 //Control interrupts
#define CTRL_REG4 0x23 //BDU
#define CTRL_REG5 0x24 //FIFO enable / latch interrupts

#define INT1_CFG 0x30 //Interrupt 1 config
#define INT1_SRC 0x31 //Interrupt status - read in order to reset the latch
#define INT1_THS 0x32 //Define threshold in mg to trigger interrupt
#define INT1_DURATION 0x33 //Define duration for the interrupt to be recognised (not sure about this one)

//Read this value to set the reference values against which accel values are compared when calculating a threshold interrupt
//Also called the REFERENCE register in the datasheet
#define HP_FILTER_RESET 0x26 

#define REG_X 0x28
#define REG_Y 0x2A
#define REG_Z 0x2C


void setup()
{
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  initI2C();

  writeReg(CTRL_REG1, 0x47); //Set data rate to 50hz. Enable X, Y and Z high precision modes

  //1 - BDU: Block data update. This ensures that both the high and the low bytes for each 16bit represent the same sample
  //0 - BLE: Big/little endian. Set to little endian
  //00 - FS1-FS0: Full scale selection. 00 represents +-2g
  //1 - HR: High resolution mode enabled.
  //00 - ST1-ST0: Self test. Disabled
  //0 - SIM: SPI serial interface mode. Default is 0.
  writeReg(CTRL_REG4, 0x88);

  //2 Write 09h into CTRL_REG2 // High-pass filter enabled on data and interrupt1
  writeReg(CTRL_REG2, 0x09);

  //3 Write 40h into CTRL_REG3 // Interrupt driven to INT1 pad
  writeReg(CTRL_REG3, 0x40);
  
  //4 Write 00h into CTRL_REG4 // FS = 2 g
  //5 Write 08h into CTRL_REG5 // Interrupt latched
  writeReg(CTRL_REG5, 0x08);
  
  // Threshold as a multiple of 16mg. 4 * 16 = 64mg
  writeReg(INT1_THS, 0x04);
  
  // Duration = 0 not quite sure what this does yet 
  writeReg(INT1_DURATION, 0x00);

  // Read the reference register to set the reference acceleration values against which
  // we compare current values for interrupt generation
  //8 Read HP_FILTER_RESET
  readReg(HP_FILTER_RESET);
  
  //9 Write 2Ah into INT1_CFG // Configure interrupt when any of the X, Y or Z axes exceeds (rather than stay below) the threshold
  writeReg(INT1_CFG, 0x2a);
  
  pinMode(intPin, INPUT);
  attachInterrupt(intPin, wakeUp, RISING);
  
  Serial.println("Accelerometer enabled");
}

float getAccel(int16_t accel) {
  return float(accel) / 16000;
}

unsigned long previousMillis = 0;
const long interval = 1000;
int ledState = LOW;

bool started = false;

void loop()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    //dataReady();

    //Get ready
    if (!started) {
      dataReady();
      started = true;
    }

    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(ledPin, ledState);
  }
}

void dataReady() {
  float x = getAccel(readReg(REG_X));
  float y = getAccel(readReg(REG_Y));
  float z = getAccel(readReg(REG_Z));

  Serial.print("Read: ");
  printFloat(x, 3);
  Serial.print(", ");
  printFloat(y, 3);
  Serial.print(", ");
  printFloat(z, 3);
  Serial.println();
  
  writeReg(INT1_CFG, 0x2a);
}

void wakeUp() {
  Serial.println("Wake up!!");
  readReg(INT1_SRC);
}






