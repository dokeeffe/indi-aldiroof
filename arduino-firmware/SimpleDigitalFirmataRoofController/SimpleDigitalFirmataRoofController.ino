/*
 * Firmware for a roll off roof. A simple state-machine based on StandardFirmata, 
 * comminicates with indiserver indi_aldiroof driver on linux host.
 *
 * Firmata is a generic protocol for communicating with arduino microcontrollers. 
 * This code based on a the SimpleFirmata and EchoString firmware that comes with the ArduinoIDE (file>examples>firmata) 
 * 
 * 4 commands are sent from the driver to this firmware, [ABORT,OPEN,CLOSE,QUERY].
 * 'QUERY' is used to determine if the roof is fully open or fully closed.
 * Unlike the usual firmata scenario, the client does not have direct control over the pins.
 * 
 */
#include <Wire.h>
#include <Firmata.h>

/*==============================================================================
 * ROOF SPECIFIC GLOBAL VARIABLES
 *============================================================================*/
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
//actual state of the roof
volatile int roofState = roofStopped;// variable for reading the pushbutton status
volatile int previousRoofState = roofStopped;
volatile int previousRoofDirection = roofClosing;
volatile int previousFullyClosedSwitchState = LOW;
volatile int previousFullyOpenSwitchState = LOW;
long hoistOnTime = 0;
long ledOnTime = 0;
bool ledState;

/*==============================================================================
 * SETUP()
 *============================================================================*/
void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_MAJOR_VERSION, FIRMATA_MINOR_VERSION);
  Firmata.attach(STRING_DATA, stringCallback);
  Firmata.begin(57600);
  pinMode(relayRoofOpenPin1, OUTPUT); 
  pinMode(relayRoofClosePin1, OUTPUT);
  pinMode(relayRoofOpenPin2, OUTPUT); 
  pinMode(relayRoofClosePin2, OUTPUT); 
  pinMode(fullyOpenStopSwitchPin, INPUT);
  pinMode(fullyClosedStopSwitchPin, INPUT); 
  pinMode(fullyOpenLedPin, OUTPUT); 
  pinMode(fullyClosedLedPin, OUTPUT); 
}

/*==============================================================================
 * LOOP()
 *============================================================================*/
void loop()
{
  while (Firmata.available()) {
    Firmata.processInput();
  }
  handleState();
}

/*==============================================================================
 * ROLL OFF ROOF SPECIFIC COMMANDS
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
  } else if (commandString.equals("QUERY")) {
    if(digitalRead(fullyOpenStopSwitchPin)==HIGH) {
      Firmata.sendString("OPEN");
    } else if(digitalRead(fullyClosedStopSwitchPin)==HIGH) {
      Firmata.sendString("CLOSED");
    } else {
      Firmata.sendString("UNKNOWN");
    }
  } 
}

/**
 * Handle the state of the roof. Act on state change
 */
void handleState() {
  handleLEDs();
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

