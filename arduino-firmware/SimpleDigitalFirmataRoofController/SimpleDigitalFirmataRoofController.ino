/*
 * INDIDUINO firmware for a roll off roof. Based on Firmata, comminicates with indiserver indi_duino driver on linux host.
 *
 * Firmata is a generic protocol for communicating with microcontrollers
 * from software on a host computer. It is intended to work with
 * any host computer software package.
 *
 * To download a host software package, please clink on the following link
 * to open the download page in your default browser.
 *
 * http://firmata.org/wiki/Download
 */
#include <Firmata.h>

//constants
const int maxHoistOnTime = 17000;
//pin constants
const int relayRoofOpenPin1 =  2;      // the number of the relay pin
const int relayRoofClosePin1 =  3;      // the number of the relay pin
const int relayRoofOpenPin2 =  4;      // the number of the relay pin
const int relayRoofClosePin2 =  5;      // the number of the relay pin

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

byte previousPIN[TOTAL_PORTS];  // PIN means PORT for input
byte previousPORT[TOTAL_PORTS];

/**
 * Send change messages to clients. (not actually needed for the indi driver as it asksState)
 */
void outputPort(byte portNumber, byte portValue)
{
  // only send the data when it changes, otherwise you get too many messages!
 if (previousPIN[portNumber] != portValue) {
    Firmata.sendDigitalPort(portNumber, portValue);
    previousPIN[portNumber] = portValue;
  }
}


void reportDigitalCallback(byte port, int value)
{
  if (port < TOTAL_PORTS) {
    Firmata.sendDigitalPort(port, value);
  }
}


/**
 * Used to by client to set state
 */
void digitalWriteCallback(byte port, int value)
{
  byte i;
  byte currentPinValue, previousPinValue;

  if (port < TOTAL_PORTS && value != previousPORT[port]) {
    for (i = 0; i < 8; i++) {
      currentPinValue = (byte) value & (1 << i);
      previousPinValue = previousPORT[port] & (1 << i);
      int pin = i + (port * 8);
      if (pin == 2 && currentPinValue > 0) {
        roofState = roofOpening;
      } else if (pin == 3 && currentPinValue > 0) {
        roofState = roofClosing;
      } else if (pin == 4 && currentPinValue > 0) {
        roofState = roofStopped;
      }
    }
    previousPORT[port] = value;
  }
}

void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_MAJOR_VERSION, FIRMATA_MINOR_VERSION);
  Firmata.attach(DIGITAL_MESSAGE, digitalWriteCallback);
//  Firmata.attach(REPORT_DIGITAL, reportDigitalCallback);
  Firmata.begin(57600);
  pinMode(2, OUTPUT); //Relay
  pinMode(3, OUTPUT); //Relay
  pinMode(4, OUTPUT); //Relay
  pinMode(5, OUTPUT); //Relay
  pinMode(13, OUTPUT); //LED just for testing the roof stop switches are working
  pinMode(8, INPUT); //limit switch
  pinMode(9, INPUT); //limit switch
  pinMode(10, OUTPUT); //LED
  pinMode(11, OUTPUT); //LED
}

void loop()
{
  byte i;
  for (i = 0; i < TOTAL_PORTS; i++) {
    outputPort(i, readPort(i, 0xff));
  }

  while (Firmata.available()) {
    Firmata.processInput();
  }
  handleState();
  //Enforoce max time for roof hoist motor being on
  if (roofMotorRunDuration() > maxHoistOnTime) {
    roofState = roofStopped;
  }

  //switch on led if any stop-switch is on. Flash them if roof moving.
  handleLEDs();
}

/**
 * Handle the state of the roof. Act on state change
 */
void handleState() {
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
 * Switch on relays to move motor forward
 */
void motorFwd() {
  motorOff();
  digitalWrite(relayRoofOpenPin1, HIGH);
  digitalWrite(relayRoofOpenPin2, HIGH);
  previousRoofDirection = roofOpening;
}

/**
 * Switch on relays to reverse motor
 */
void motorReverse() {
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
 * LEDs are used to provide visual clues to the state of the roof controller. Really not needed but handy when stuff goes wrong.
 * Flash LEDS when moving, Switch one LED on if the corresponding stop switch is on.
 */
void handleLEDs() {
  if (roofState == roofOpening || roofState == roofClosing) {  
    if (millis() - ledOnTime > 100) {
       ledOnTime = millis();
       if (ledState == false ) {
        ledState = true;
        digitalWrite(10, HIGH);
        digitalWrite(11, HIGH);
       } else {
          ledState = false;
          digitalWrite(10, LOW);
          digitalWrite(11, LOW);
       }
    }
    
  } else {
    //Set an LED on if the corresponding fully open switch is on.
    if(digitalRead(9)==HIGH) {
      digitalWrite(11, HIGH);
    } else {
      digitalWrite(11, LOW);
    }
    if(digitalRead(8)==HIGH) {
      digitalWrite(10, HIGH);
    } else {
      digitalWrite(10, LOW);
    }
  }
}

