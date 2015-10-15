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
const int maxHoistOnTime = 15000;
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


byte previousPIN[TOTAL_PORTS];  // PIN means PORT for input
byte previousPORT[TOTAL_PORTS];

void outputPort(byte portNumber, byte portValue)
{
  // only send the data when it changes, otherwise you get too many messages!
//  if (previousPIN[portNumber] != portValue) {
    Firmata.sendDigitalPort(portNumber, portValue);
    previousPIN[portNumber] = portValue;
//  }
}

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
  Firmata.begin(57600);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, INPUT);
  pinMode(7, INPUT);
  pinMode(8, INPUT);
  pinMode(9, INPUT);
}

void loop()
{
  byte i;

  for (i = 0; i < TOTAL_PORTS; i++) {
    outputPort(i, readPort(i, 0xff));
  }
//  if(digitalRead(8)!=previousFullyClosedSwitchState) {
//    previousFullyClosedSwitchState=digitalRead(8);
//    Firmata.sendDigitalPort(8, digitalRead(8));
//  }
//  if(digitalRead(9)!=previousFullyClosedSwitchState) {
//    previousFullyClosedSwitchState=digitalRead(9);
//    Firmata.sendDigitalPort(8, digitalRead(9));
//  }

  while (Firmata.available()) {
    Firmata.processInput();
  }
  handleState();
  //Enforoce max time for roof hoist motor being on
  if (roofMotorRunDuration() > maxHoistOnTime) {
    roofState = roofStopped;
  }
  
  //if any of the full stop switches are ON then just stop the motors & send data back to indi driver
  if(digitalRead(8)==HIGH || digitalRead(9)==HIGH) {  
    roofState = roofStopped; 
  }
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
