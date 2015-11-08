/*******************************************************************************
Aldi hoist powered observatory roof driver.

Controls an arduino using firmata to switch on/off relays connected to a 550w 220v electric hoist.

There are several safety overrides in place to stop the motors from 'going mad'.
1) Electro-Mechanical: This is the primary safety cut out. The hoist has 2 microswitches which are normally used to stop the hoist when the load is fully lifted or cable is fully extended.
       These microswitches are attached (via bicycle brake cables) to mechanical-levers on the roof that get actuated when fully open/closed.
       These will cut power to the hoist when fully open/closed
2) INDI driver: 2 additional microswitches are used as digital inputs to the arduino. These are attached to the same mechanical-levers in 1 above. These digital inputs are used as the FullyClosedLimitSwitch and FullyOpenLimitSwitch in the code below. The indi driver (this code) will send a signal to the arduino to stop the motors once the fully open/close switch is activated
    (this is essentially made redundant by 1 above)

Sept 2015 Derek OKeeffe
*******************************************************************************/
#include "aldiroof.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

#include <indicom.h>

// We declare an auto pointer to RollOff.
std::auto_ptr<RollOff> rollOff(0);

#define ROLLOFF_DURATION    10      // TODO: remove this timer stuff which came from the simulator

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(rollOff.get() == 0) rollOff.reset(new RollOff());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        rollOff->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        rollOff->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        rollOff->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        rollOff->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    rollOff->ISSnoopDevice(root);
}

RollOff::RollOff()
{
  fullOpenLimitSwitch   = ISS_OFF;
  fullClosedLimitSwitch = ISS_OFF;
  MotionRequest=0;
  SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RollOff::initProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Init props");
    INDI::Dome::initProperties();
    SetParkDataType(PARK_NONE);
    addAuxControls();
    return true;
}

bool RollOff::SetupParms()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Setting up params");
    fullOpenLimitSwitch   = ISS_OFF;
    fullClosedLimitSwitch = ISS_OFF;
    if (getFullClosedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_SESSION, "Setting closed flag on");
        fullClosedLimitSwitch = ISS_ON;
    }
    if (getFullOpenedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_SESSION, "Setting open flag on");
        fullOpenLimitSwitch = ISS_ON;
    }
    return true;
}


/**
* Connect to the arduino. Will iterate over all /dev/ttyACM* from 0-20 to find one with matching firmata name.
* After finding and init of the Firmata object just return true.
* CAUTION: If you have another non firmata device at /dev/ttyACM? then this may cause it to misbehave.
**/
bool RollOff::Connect()
{
    for( int a = 0; a < 20; a = a + 1 )
    {
    	string usbPort = "/dev/ttyACM" +  std::to_string(a);
    	DEBUG(INDI::Logger::DBG_SESSION, "Attempting connection");
        sf = new Firmata(usbPort.c_str());
        if (sf->portOpen && strstr(sf->firmata_name, "SimpleDigitalFirmataRoofController")) {
    	    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD CONNECTED.");
	        DEBUGF(INDI::Logger::DBG_SESSION, "FIRMATA VERSION:%s",sf->firmata_name);
	        sf->reportDigitalPorts(1);
	        sf->writeDigitalPin(4,ARDUINO_HIGH); //set pin4 to high (switch off motor). For some reason the first call to sf to set a pin will fail. So thats why this call is here. Its the safest call to make.
	        return true;
        } else {
            DEBUG(INDI::Logger::DBG_SESSION,"Failed, trying next port.\n");
        }
    }
    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD FAIL TO CONNECT");
    delete sf;
    return false;
}

RollOff::~RollOff()
{

}

const char * RollOff::getDefaultName()
{
        return (char *)"Aldi roof";
}

bool RollOff::updateProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Updating props");
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
    }

    return true;
}

/**
* Disconnect from the arduino
**/
bool RollOff::Disconnect()
{
    sf->closePort();
    delete sf;	
    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD DISCONNECTED.");
    IDSetSwitch (getSwitch("CONNECTION"),"DISCONNECTED\n");
    return true;
}


void RollOff::TimerHit()
{

    DEBUG(INDI::Logger::DBG_SESSION, "Timer hit");
    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore    

   if (DomeMotionSP.s == IPS_BUSY)
   {
       // Abort called
       if (MotionRequest < 0)
       {
           DEBUG(INDI::Logger::DBG_SESSION, "Roof motion is stopped.");
           setDomeState(DOME_IDLE);
           SetTimer(1000);
           return;
       }

       // Roll off is opening
       if (DomeMotionS[DOME_CW].s == ISS_ON)
       {
           if (getFullOpenedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is open.");
               //SetParked(false);
               return;
           }
       }
       // Roll Off is closing
       else if (DomeMotionS[DOME_CCW].s == ISS_ON)
       {
           if (getFullClosedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
               setDomeState(DOME_PARKED);
               //SetParked(true);
               return;
           }
       }
       SetTimer(1000);
   }
}

/**
 * Move the roof. The arduino will take a request to set pin 3 on to switch on the relays to open the roof
 * The arduino will take a request to set pin 2 on to switch on the relays to close the roof
 * The arduino will take a request to set pin 4 to abort and switch off all relays
 **/
IPState RollOff::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions are in the danger zone. Cannot open roof.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 3");
            sf->writeDigitalPin(3,ARDUINO_HIGH);
            sf->writeDigitalPin(4,ARDUINO_LOW);
            sf->writeDigitalPin(2,ARDUINO_LOW);
        }                    
        else if (dir == DOME_CCW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 2");
            sf->writeDigitalPin(2,ARDUINO_HIGH);
            sf->writeDigitalPin(4,ARDUINO_LOW);
            sf->writeDigitalPin(3,ARDUINO_LOW);
        }                    

//        fullOpenLimitSwitch   = ISS_OFF;
//        fullClosedLimitSwitch = ISS_OFF;
        MotionRequest = ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        SetTimer(1000);
        return IPS_BUSY;
    }
    else
    {
        return (Dome::Abort() ? IPS_OK : IPS_ALERT);

    }

    return IPS_ALERT;

}

/**
 * Park the roof = close
 **/
IPState RollOff::Park()
{    
    bool rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/**
 * Unpark the roof = open
 **/
IPState RollOff::UnPark()
{
    bool rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc)
    {       
           DEBUG(INDI::Logger::DBG_SESSION, "Roll off is unparking...");
           return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/**
 * Abort motion. The arduino will take a request to set pin 4 on to mean switch off all relays
 **/
bool RollOff::Abort()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 4(abort)");
    sf->writeDigitalPin(2,ARDUINO_LOW);
    sf->writeDigitalPin(3,ARDUINO_LOW);
    sf->writeDigitalPin(4,ARDUINO_HIGH);
            
    MotionRequest=-1;

    // If both limit switches are off, then we're neither parked nor unparked.
    if (fullOpenLimitSwitch == false && fullClosedLimitSwitch == false)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        IDSetSwitch(&ParkSP, NULL);
    }

    return true;
}

/**
 * Get the state of the full open limit switch. This function will also switch off the motors as a safety override.
 **/
bool RollOff::getFullOpenedLimitSwitch()
{    
    DEBUG(INDI::Logger::DBG_SESSION, "Checking pin 8 state");
    sf->askPinState(8);
    if (sf->pin_info[8].value > 0) {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully open switch ON");
        DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 4(stop)");
        sf->writeDigitalPin(2,ARDUINO_LOW);
        sf->writeDigitalPin(3,ARDUINO_LOW);
        sf->writeDigitalPin(4,ARDUINO_HIGH);
        return true;
    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully open switch OFF");
        return false;
    }
}

/**
 * Get the state of the full closed limit switch. This function will also switch off the motors as a safety override.
 **/
bool RollOff::getFullClosedLimitSwitch()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Checking pin 9 state");
    sf->askPinState(9);
    if (sf->pin_info[9].value > 0) {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully Closed switch ON");
        DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 4(stop)");
        sf->writeDigitalPin(2,ARDUINO_LOW);
        sf->writeDigitalPin(3,ARDUINO_LOW);
        sf->writeDigitalPin(4,ARDUINO_HIGH);
        return true;
    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully Closed switch OFF");
        return false;
    }
}

