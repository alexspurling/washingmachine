
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

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10      /* Time ESP32 will go to sleep (in seconds) */
#define AWAKE_TIME 15000

RTC_DATA_ATTR int bootCount = 0;
unsigned long bootTime = 0;

void setup()
{
  Serial.begin(115200);
  delay(100);
  
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  
  pinMode(ledPin, OUTPUT);

  initI2C();

  writeReg(CTRL_REG1, 0x47); //Set data rate to 50hz. Enable X, Y and Z axes

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

  bootTime = millis();
  Serial.println("Accelerometer enabled");

  //Wake up when pin 25 is high
  esp_deep_sleep_enable_ext0_wakeup(GPIO_NUM_25, HIGH);
}

float getAccel(int16_t accel) {
  return float(accel) / 16000;
}

volatile bool machineOn = false;
volatile unsigned int vibrations = 0;

unsigned int loops = 1;
int activeLoopInterval = 100;

bool started = false;

void loop()
{
  if (!started) {
    started = true;
    //Reset the interrupt when we are actually ready to receive it
    readReg(INT1_SRC);
  }
  
  if (machineOn) {
    //Flash every 500ms when machine is on
    if (loops == 1) {
      digitalWrite(ledPin, HIGH);
    } else if (loops == (500 / activeLoopInterval)) {
      digitalWrite(ledPin, LOW);
    } else if (loops == (1000 / activeLoopInterval)) {
      loops = 0;
    }
    loops++;

    Serial.print("Vibrations: ");
    Serial.print(vibrations);
    Serial.print(", loops: ");
    Serial.println(loops);
    
    if (vibrations > 0) {
      vibrations--;
    } else {
      machineOn = false;
    }
    
    delay(activeLoopInterval);
  } else if (millis() - bootTime > AWAKE_TIME) {
    writeReg(CTRL_REG1, 0x2f); //Set data rate to 1z. Enable X, Y and Z axes in low power mode
    writeReg(CTRL_REG4, 0x0); //Turn off BDU, turn off high precision mode
    readReg(HP_FILTER_RESET);
    readReg(INT1_SRC); //Reset the interrupt so it will trigger when we are asleep
    
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  } else {
    Serial.println("Idle");
    dataReady();
    //Slow flash when idling
    digitalWrite(ledPin, HIGH);
    delay(1000);
    digitalWrite(ledPin, LOW);
    delay(1000);
  }
}

void print_wakeup_reason(){
  esp_deep_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_deep_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : 
      Serial.print("Wakeup was not caused by deep sleep: ");
      Serial.println(wakeup_reason);  
      break;
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
  machineOn = true;
  vibrations++;
  readReg(INT1_SRC);
}







