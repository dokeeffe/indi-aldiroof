/*
   Firmware for a roll off roof. A simple state-machine based on StandardFirmata,
   comminicates with indiserver indi_aldiroof driver.

   Firmata is a generic protocol for communicating with arduino microcontrollers.
   This code based on a the SimpleFirmata and EchoString firmware that comes with the ArduinoIDE (file>examples>firmata)

   4 commands are sent from the driver to this firmware, [ABORT,OPEN,CLOSE,QUERY].
   'QUERY' is used to determine if the roof is fully open or fully closed.
   Unlike the usual firmata scenario, the client does not have direct control over the pins.


   Plug pins to motor controller (this is a custom plug wired beween contactors and motor to enable easy maintenance, not used in code)
   1 Live Supply
   2 Neutral Supply
   3 Close live
   4 Open live
   5 Close neutral
   6 Open neutral
   7 NA
   8 NA

*/
#include <Wire.h>
#include <Firmata.h>

/*==============================================================================
   ROOF SPECIFIC GLOBAL VARIABLES
  ============================================================================*/
//pin constants
const int relayRoofOpenPin1 =  3;     
const int relayRoofOpenPin2 =  6;     
const int relayRoofClosePin1 =  4;      
const int relayRoofClosePin2 =  5;      
const int fullyOpenStopSwitchPin =  8;
const int fullyClosedStopSwitchPin =  9;
const int ledPin =  13;
//Roof state constants
const int roofClosed = 0;
const int roofOpen = 1;
const int roofStopped = 2;
const int roofClosing = 3;
const int roofOpening = 4;
//actual state of the roof
volatile int roofState = roofStopped;
volatile int previousRoofState = roofStopped;
long hoistOnTime = 0;
long ledToggleTime = 0;
bool ledState;

/*==============================================================================
   SETUP()
  ============================================================================*/
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
  pinMode(ledPin, OUTPUT);
}

/*==============================================================================
   LOOP()
  ============================================================================*/
void loop()
{
  while (Firmata.available()) {
    Firmata.processInput();
  }
  handleState();
}

/*==============================================================================
   ROLL OFF ROOF SPECIFIC COMMANDS
  ============================================================================*/
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
    if (digitalRead(fullyOpenStopSwitchPin) == HIGH) {
      Firmata.sendString("OPEN");
    } else if (digitalRead(fullyClosedStopSwitchPin) == HIGH) {
      Firmata.sendString("CLOSED");
    } else {
      Firmata.sendString("UNKNOWN");
    }
  }
}

/**
   Handle the state of the roof. Act on state change
*/
void handleState() {
  handleLEDs();
  safetyCutout();
  if (roofState != previousRoofState) {
    previousRoofState = roofState;
    if (roofState == roofOpening) {
      motorFwd();
    } else if (roofState == roofClosing) {
      motorReverse();
    }  else {
      motorOff();
    }
  }
}

/**
   Switch off all motor relays
*/
void motorOff() {
  digitalWrite(relayRoofOpenPin1, LOW);
  digitalWrite(relayRoofClosePin1, LOW);
  digitalWrite(relayRoofOpenPin2, LOW);
  digitalWrite(relayRoofClosePin2, LOW);
  delay(1000);
}

/**
   Switch on relays to move motor rev
*/
void motorReverse() {
  motorOff();
  digitalWrite(relayRoofOpenPin1, HIGH);
  digitalWrite(relayRoofOpenPin2, HIGH);
  hoistOnTime = millis();
}

/**
   Switch on relays to fwd motor
*/
void motorFwd() {
  motorOff();
  digitalWrite(relayRoofClosePin1, HIGH);
  digitalWrite(relayRoofClosePin2, HIGH);
  hoistOnTime = millis();
}

/**
   Return the duration in miliseconds that the roof motors have been running
*/
long roofMotorRunDuration() {
  if (roofState == roofOpening || roofState == roofClosing) {
    return millis() - hoistOnTime;
  } else {
    return 0;
  }
}


void safetyCutout() {
  if (roofMotorRunDuration() > 1000) {
    if ((roofState == roofOpening && digitalRead(fullyOpenStopSwitchPin) == HIGH) || (roofState == roofClosing && digitalRead(fullyClosedStopSwitchPin) == HIGH)) {
      roofState = roofStopped;
    }    
  }
}

/**
   LED is used to provide visual clues to the state of the roof controller.
   Really not necessary, but handy for debugging wiring and mechanical problems
*/
void handleLEDs() {
  //Set an LED on if the corresponding fully open switch is on.
  if (digitalRead(fullyClosedStopSwitchPin) == HIGH && digitalRead(fullyOpenStopSwitchPin) == LOW) {
    toggleLed(50);
  } else if (digitalRead(fullyOpenStopSwitchPin) == HIGH && digitalRead(fullyClosedStopSwitchPin) == LOW) {
    toggleLed(1000);
  } else {
    digitalWrite(ledPin, LOW);
  }
}

void toggleLed(int duration) {
  if (millis() - ledToggleTime > duration) {
    ledToggleTime = millis();
    if (ledState == false ) {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
  }
}
