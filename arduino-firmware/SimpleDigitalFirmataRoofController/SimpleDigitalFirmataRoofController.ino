/*
 * Firmware for a roll off roof. A simple state-machine based on StandardFirmata, 
 * comminicates with indiserver indi_aldiroof driver on linux host.
 *
 * Firmata is a generic protocol for communicating with arduino microcontrollers. 
 * This code based on a the SimpleFirmata and EchoString firmware that comes with the ArduinoIDE (file>examples>firmata) 
 * 
 * 3 commands are sent from the driver to this firmware, [ABORT,OPEN,CLOSE].
 * The driver also gets the pin state of pins 8 and 9 to determine if the roof is fully open or fully closed.
 * Unlike the usual firmata scenario, the client does not have direct control over the pins.
 * 
 */
#include <Servo.h>
#include <Wire.h>
#include <Firmata.h>

#define I2C_WRITE                   B00000000
#define I2C_READ                    B00001000
#define I2C_READ_CONTINUOUSLY       B00010000
#define I2C_STOP_READING            B00011000
#define I2C_READ_WRITE_MODE_MASK    B00011000
#define I2C_10BIT_ADDRESS_MODE_MASK B00100000
#define MAX_QUERIES                 8
#define REGISTER_NOT_SPECIFIED      -1

// the minimum interval for sampling analog input
#define MINIMUM_SAMPLING_INTERVAL 10


/*==============================================================================
 * ROOF SPECIFIC GLOBAL VARIABLES
 *============================================================================*/
const int maxHoistOnTime = 17000;
//pin constants
const int relayRoofOpenPin1 =  2;      // the number of the relay pin
const int relayRoofClosePin1 =  3;      // the number of the relay pin
const int relayRoofOpenPin2 =  4;      // the number of the relay pin
const int relayRoofClosePin2 =  5;      // the number of the relay pin
const int fullyOpenStopSwitchPin =  8;      
const int fullyClosedStopSwitchPin =  9;      
const int fullyOpenLedPin =  10;      
const int fullyClosedLedPin =  11;      
//Roof state constants
const int roofClosed = 0;
const int roofOpen = 1;
const int roofStopped = 2;
const int roofClosing = 3;
const int roofOpening = 4;
//state of the roof
volatile int roofState = roofStopped;// variable for reading the pushbutton status
volatile int previousRoofState = roofStopped;
volatile int previousRoofDirection = roofClosing;
volatile int previousFullyClosedSwitchState = LOW;
volatile int previousFullyOpenSwitchState = LOW;
long hoistOnTime = 0;
long ledOnTime = 0;
bool ledState;

/*==============================================================================
 * FIRMATA GLOBAL VARIABLES
 *============================================================================*/
/* analog inputs */
int analogInputsToReport = 0; // bitwise array to store pin reporting

/* digital input ports */
byte reportPINs[TOTAL_PORTS];       // 1 = report this port, 0 = silence
byte previousPINs[TOTAL_PORTS];     // previous 8 bits sent

/* pins configuration */
byte pinConfig[TOTAL_PINS];         // configuration of every pin
byte portConfigInputs[TOTAL_PORTS]; // each bit: 1 = pin in INPUT, 0 = anything else
int pinState[TOTAL_PINS];           // any value that has been written

/* timer variables */
unsigned long currentMillis;        // store the current value from millis()
unsigned long previousMillis;       // for comparison with currentMillis
unsigned int samplingInterval = 19; // how often to run the main loop (in ms)

/* i2c data */
struct i2c_device_info {
  byte addr;
  int reg;
  byte bytes;
};

/* for i2c read continuous more */
i2c_device_info query[MAX_QUERIES];

byte i2cRxData[32];
boolean isI2CEnabled = false;
signed char queryIndex = -1;
// default delay time between i2c read request and Wire.requestFrom()
unsigned int i2cReadDelayTime = 0;

Servo servos[MAX_SERVOS];
byte servoPinMap[TOTAL_PINS];
byte detachedServos[MAX_SERVOS];
byte detachedServoCount = 0;
byte servoCount = 0;

boolean isResetting = false;

/* utility functions */
void wireWrite(byte data)
{
#if ARDUINO >= 100
  Wire.write((byte)data);
#else
  Wire.send(data);
#endif
}

byte wireRead(void)
{
#if ARDUINO >= 100
  return Wire.read();
#else
  return Wire.receive();
#endif
}

/*==============================================================================
 * FUNCTIONS
 *============================================================================*/

void readAndReportData(byte address, int theRegister, byte numBytes) {
  // allow I2C requests that don't require a register read
  // for example, some devices using an interrupt pin to signify new data available
  // do not always require the register read so upon interrupt you call Wire.requestFrom()
  if (theRegister != REGISTER_NOT_SPECIFIED) {
    Wire.beginTransmission(address);
    wireWrite((byte)theRegister);
    Wire.endTransmission();
    // do not set a value of 0
    if (i2cReadDelayTime > 0) {
      // delay is necessary for some devices such as WiiNunchuck
      delayMicroseconds(i2cReadDelayTime);
    }
  } else {
    theRegister = 0;  // fill the register with a dummy value
  }

  Wire.requestFrom(address, numBytes);  // all bytes are returned in requestFrom

  // check to be sure correct number of bytes were returned by slave
  if (numBytes < Wire.available()) {
    Firmata.sendString("I2C: Too many bytes received");
  } else if (numBytes > Wire.available()) {
    Firmata.sendString("I2C: Too few bytes received");
  }

  i2cRxData[0] = address;
  i2cRxData[1] = theRegister;

  for (int i = 0; i < numBytes && Wire.available(); i++) {
    i2cRxData[2 + i] = wireRead();
  }

  // send slave address, register and received bytes
  Firmata.sendSysex(SYSEX_I2C_REPLY, numBytes + 2, i2cRxData);
}

void outputPort(byte portNumber, byte portValue, byte forceSend)
{
  // pins not configured as INPUT are cleared to zeros
  portValue = portValue & portConfigInputs[portNumber];
  // only send if the value is different than previously sent
  if (forceSend || previousPINs[portNumber] != portValue) {
    Firmata.sendDigitalPort(portNumber, portValue);
    previousPINs[portNumber] = portValue;
  }
}

/* -----------------------------------------------------------------------------
 * check all the active digital inputs for change of state, then add any events
 * to the Serial output queue using Serial.print() */
void checkDigitalInputs(void)
{
  /* Using non-looping code allows constants to be given to readPort().
   * The compiler will apply substantial optimizations if the inputs
   * to readPort() are compile-time constants. */
  if (TOTAL_PORTS > 0 && reportPINs[0]) outputPort(0, readPort(0, portConfigInputs[0]), false);
  if (TOTAL_PORTS > 1 && reportPINs[1]) outputPort(1, readPort(1, portConfigInputs[1]), false);
  if (TOTAL_PORTS > 2 && reportPINs[2]) outputPort(2, readPort(2, portConfigInputs[2]), false);
  if (TOTAL_PORTS > 3 && reportPINs[3]) outputPort(3, readPort(3, portConfigInputs[3]), false);
  if (TOTAL_PORTS > 4 && reportPINs[4]) outputPort(4, readPort(4, portConfigInputs[4]), false);
  if (TOTAL_PORTS > 5 && reportPINs[5]) outputPort(5, readPort(5, portConfigInputs[5]), false);
  if (TOTAL_PORTS > 6 && reportPINs[6]) outputPort(6, readPort(6, portConfigInputs[6]), false);
  if (TOTAL_PORTS > 7 && reportPINs[7]) outputPort(7, readPort(7, portConfigInputs[7]), false);
  if (TOTAL_PORTS > 8 && reportPINs[8]) outputPort(8, readPort(8, portConfigInputs[8]), false);
  if (TOTAL_PORTS > 9 && reportPINs[9]) outputPort(9, readPort(9, portConfigInputs[9]), false);
  if (TOTAL_PORTS > 10 && reportPINs[10]) outputPort(10, readPort(10, portConfigInputs[10]), false);
  if (TOTAL_PORTS > 11 && reportPINs[11]) outputPort(11, readPort(11, portConfigInputs[11]), false);
  if (TOTAL_PORTS > 12 && reportPINs[12]) outputPort(12, readPort(12, portConfigInputs[12]), false);
  if (TOTAL_PORTS > 13 && reportPINs[13]) outputPort(13, readPort(13, portConfigInputs[13]), false);
  if (TOTAL_PORTS > 14 && reportPINs[14]) outputPort(14, readPort(14, portConfigInputs[14]), false);
  if (TOTAL_PORTS > 15 && reportPINs[15]) outputPort(15, readPort(15, portConfigInputs[15]), false);
}

// -----------------------------------------------------------------------------
/* sets the pin mode to the correct state and sets the relevant bits in the
 * two bit-arrays that track Digital I/O and PWM status
 */
void setPinModeCallback(byte pin, int mode)
{
  if (pinConfig[pin] == IGNORE)
    return;

  if (pinConfig[pin] == I2C && isI2CEnabled && mode != I2C) {
    // disable i2c so pins can be used for other functions
    // the following if statements should reconfigure the pins properly
    disableI2CPins();
  }
  
  if (IS_PIN_DIGITAL(pin)) {
    if (mode == INPUT) {
      portConfigInputs[pin / 8] |= (1 << (pin & 7));
    } else {
      portConfigInputs[pin / 8] &= ~(1 << (pin & 7));
    }
  }
  pinState[pin] = 0;
  switch (mode) {
    case ANALOG:
      if (IS_PIN_ANALOG(pin)) {
        if (IS_PIN_DIGITAL(pin)) {
          pinMode(PIN_TO_DIGITAL(pin), INPUT);    // disable output driver
          digitalWrite(PIN_TO_DIGITAL(pin), LOW); // disable internal pull-ups
        }
        pinConfig[pin] = ANALOG;
      }
      break;
    case INPUT:
      if (IS_PIN_DIGITAL(pin)) {
        pinMode(PIN_TO_DIGITAL(pin), INPUT);    // disable output driver
        digitalWrite(PIN_TO_DIGITAL(pin), LOW); // disable internal pull-ups
        pinConfig[pin] = INPUT;
      }
      break;
    case OUTPUT:
      if (IS_PIN_DIGITAL(pin)) {
        digitalWrite(PIN_TO_DIGITAL(pin), LOW); // disable PWM
        pinMode(PIN_TO_DIGITAL(pin), OUTPUT);
        pinConfig[pin] = OUTPUT;
      }
      break;
    case PWM:
      if (IS_PIN_PWM(pin)) {
        pinMode(PIN_TO_PWM(pin), OUTPUT);
        analogWrite(PIN_TO_PWM(pin), 0);
        pinConfig[pin] = PWM;
      }
      break;
    case I2C:
      if (IS_PIN_I2C(pin)) {
        // mark the pin as i2c
        // the user must call I2C_CONFIG to enable I2C for a device
        pinConfig[pin] = I2C;
      }
      break;
    default:
      Firmata.sendString("Unknown pin mode"); // TODO: put error msgs in EEPROM
  }
  // TODO: save status to EEPROM here, if changed
}


void reportDigitalCallback(byte port, int value)
{
  if (port < TOTAL_PORTS) {
    reportPINs[port] = (byte)value;
    // Send port value immediately. This is helpful when connected via
    // ethernet, wi-fi or bluetooth so pin states can be known upon
    // reconnecting.
    if (value) outputPort(port, readPort(port, portConfigInputs[port]), true);
  }
  // do not disable analog reporting on these 8 pins, to allow some
  // pins used for digital, others analog.  Instead, allow both types
  // of reporting to be enabled, but check if the pin is configured
  // as analog when sampling the analog inputs.  Likewise, while
  // scanning digital pins, portConfigInputs will mask off values from any
  // pins configured as analog
}

/* disable the i2c pins so they can be used for other functions */
void disableI2CPins() {
  isI2CEnabled = false;
  // disable read continuous mode for all devices
  queryIndex = -1;
}

/*==============================================================================
 * SETUP()
 *============================================================================*/
void systemResetCallback()
{
  isResetting = true;

  // initialize a defalt state
  // TODO: option to load config from EEPROM instead of default

  if (isI2CEnabled) {
    disableI2CPins();
  }

  for (byte i = 0; i < TOTAL_PORTS; i++) {
    reportPINs[i] = false;    // by default, reporting off
    portConfigInputs[i] = 0;  // until activated
    previousPINs[i] = 0;
  }

  for (byte i = 0; i < TOTAL_PINS; i++) {
    // pins with analog capability default to analog input
    // otherwise, pins default to digital output
    if (IS_PIN_ANALOG(i)) {
      // turns off pullup, configures everything
      setPinModeCallback(i, ANALOG);
    } else {
      // sets the output to 0, configures portConfigInputs
      setPinModeCallback(i, OUTPUT);
    }

    servoPinMap[i] = 255;
  }
  // by default, do not report any analog inputs
  analogInputsToReport = 0;

  detachedServoCount = 0;
  servoCount = 0;

  isResetting = false;
}

void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_MAJOR_VERSION, FIRMATA_MINOR_VERSION);
  Firmata.attach(REPORT_DIGITAL, reportDigitalCallback);
  Firmata.attach(SET_PIN_MODE, setPinModeCallback);
  Firmata.attach(SYSTEM_RESET, systemResetCallback);
  Firmata.attach(STRING_DATA, stringCallback);
  Firmata.begin(57600);
  systemResetCallback();  // reset to default config
  pinMode(2, OUTPUT); //Relay
  pinMode(3, OUTPUT); //Relay
  pinMode(4, OUTPUT); //Relay
  pinMode(5, OUTPUT); //Relay
  pinMode(8, OUTPUT); //Relay
  pinMode(9, OUTPUT); //Relay
}

/*==============================================================================
 * LOOP()
 *============================================================================*/
void loop()
{
  byte pin, analogPin;

  /* DIGITALREAD - as fast as possible, check for changes and output them to the
   * FTDI buffer using Serial.print()  */
  checkDigitalInputs();

  /* STREAMREAD - processing incoming messagse as soon as possible, while still
   * checking digital inputs.  */
  while (Firmata.available())
    Firmata.processInput();

  // TODO - ensure that Stream buffer doesn't go over 60 bytes

  currentMillis = millis();
  if (currentMillis - previousMillis > samplingInterval) {
    previousMillis += samplingInterval;
    /* ANALOGREAD - do all analogReads() at the configured sampling interval */
    for (pin = 0; pin < TOTAL_PINS; pin++) {
      if (IS_PIN_ANALOG(pin) && pinConfig[pin] == ANALOG) {
        analogPin = PIN_TO_ANALOG(pin);
        if (analogInputsToReport & (1 << analogPin)) {
          Firmata.sendAnalog(analogPin, analogRead(analogPin));
        }
      }
    }
    // report i2c data for all device with read continuous mode enabled
    if (queryIndex > -1) {
      for (byte i = 0; i < queryIndex + 1; i++) {
        readAndReportData(query[i].addr, query[i].reg, query[i].bytes);
      }
    }
  }
  //end standard firmata loop code.
  //handle roof state;
  handleState();
}

/*==============================================================================
 * ROOF SPECIFIC FUNCTIONS Used to by client to set state
 *============================================================================*/
void stringCallback(char *myString)
{
  String commandString = String(myString);
  if (commandString.equals("OPEN")) {
    roofState = roofOpening;
  } else if (commandString.equals("CLOSE")) {
    roofState = roofClosing;
  } else if (commandString.equals("ABORT")) {
    roofState = roofStopped;
  }
}

/**
 * Handle the state of the roof. Act on state change
 */
void handleState() {
  //Enforoce max time for roof hoist motor being on
//  if (roofMotorRunDuration() > maxHoistOnTime) {
//    roofState = roofStopped;
//  }
  //switch on led if any stop-switch is on. Flash them if roof moving.
  handleLEDs();
  //Serial.print(roofState);
  if (roofState != previousRoofState) {
    previousRoofState = roofState;
    if (roofState == roofOpening) {
      if (previousRoofDirection == roofClosing) {
        motorOff();
        delay(500); //delay on direction change to save motor
      }
      motorFwd();
      hoistOnTime = millis();
    } else if (roofState == roofClosing) {
      if (previousRoofDirection == roofOpening) {
        motorOff();
        delay(500); //delay on direction change to save motor
      }
      motorReverse();
      hoistOnTime = millis();
    }  else {
      //Switch of motor
      motorOff();
    }
  }
}

/**
 * Switch off all motor relays
 */
void motorOff() {
  digitalWrite(relayRoofOpenPin1, LOW);
  digitalWrite(relayRoofClosePin1, LOW);
  digitalWrite(relayRoofOpenPin2, LOW);
  digitalWrite(relayRoofClosePin2, LOW);
}

/**
 * Switch on relays to move motor rev
 */
void motorReverse() {
  motorOff();
  digitalWrite(relayRoofOpenPin1, HIGH);
  digitalWrite(relayRoofOpenPin2, HIGH);
  previousRoofDirection = roofOpening;
}

/**
 * Switch on relays to fwd motor
 */
void motorFwd() {
  motorOff();
  digitalWrite(relayRoofClosePin1, HIGH);
  digitalWrite(relayRoofClosePin2, HIGH);
  previousRoofDirection = roofClosing;
}

/**
 * Return the duration in miliseconds that the roof motors have been running
 */
long roofMotorRunDuration() {
  if (roofState == roofOpening || roofState == roofClosing) {
    return millis() - hoistOnTime;
  } else {
    return 0;
  }
}

/**
 * LEDs are used to provide visual clues to the state of the roof controller. 
 * Really not necessary, but handy when stuff goes wrong.
 * Flash LEDS when moving, Switch one LED on if the corresponding stop switch is on.
 */
void handleLEDs() {
  if (roofState == roofOpening || roofState == roofClosing) {  
    if (millis() - ledOnTime > 100) {
       ledOnTime = millis();
       if (ledState == false ) {
        ledState = true;
        digitalWrite(fullyOpenLedPin, HIGH);
        digitalWrite(fullyClosedLedPin, HIGH);
       } else {
          ledState = false;
          digitalWrite(fullyOpenLedPin, LOW);
          digitalWrite(fullyClosedLedPin, LOW);
       }
    }
    
  } else {
    //Set an LED on if the corresponding fully open switch is on.
    if(digitalRead(fullyClosedStopSwitchPin)==HIGH) {
      digitalWrite(fullyClosedLedPin, HIGH);
    } else {
      digitalWrite(fullyClosedLedPin, LOW);
    }
    if(digitalRead(fullyOpenStopSwitchPin)==HIGH) {
      digitalWrite(fullyOpenLedPin, HIGH);
    } else {
      digitalWrite(fullyOpenLedPin, LOW);
    }
  }
}

