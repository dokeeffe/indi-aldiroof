/*******************************************************************************
Aldi hoist powered observatory roof driver.

Controls an arduino using firmata to switch on/off relays connected to a 550w 220v electric hoist.

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

  fullOpenLimitSwitch   = ISS_ON;
  fullClosedLimitSwitch = ISS_OFF;

   DomeCapability cap;

   cap.canAbort = true;
   cap.canAbsMove = false;
   cap.canRelMove = false;
   cap.hasShutter = false;
   cap.canPark    = true;
   cap.hasVariableSpeed = false;

   MotionRequest=0;

   SetDomeCapability(&cap);   

}

/************************************************************************************
 *
* ***********************************************************************************/
bool RollOff::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);

    addAuxControls();

    return true;
}

bool RollOff::SetupParms()
{
    // If we have parking data
    if (InitPark())
    {
        if (isParked())
        {
            fullOpenLimitSwitch   = ISS_OFF;
            fullClosedLimitSwitch = ISS_ON;
        }
        else
        {
            fullOpenLimitSwitch   = ISS_ON;
            fullClosedLimitSwitch = ISS_OFF;
        }
    }
    // If we don't have parking data
    else
    {
        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;
    }



    return true;
}


bool RollOff::Connect()
{
    for( int a = 0; a < 20; a = a + 1 )
    {
    	string usbPort = "/dev/ttyACM" +  std::to_string(a);
    	DEBUG(INDI::Logger::DBG_SESSION, "Attempting connection .\n");
        sf = new Firmata(usbPort.c_str());
        if (sf->portOpen && strstr(sf->firmata_name, "SimpleDigitalFirmataRoofController")) {
    	    IDLog("ARDUINO BOARD CONNECTED.\n");
	    IDLog("FIRMATA VERSION:%s\n",sf->firmata_name);
	    IDSetSwitch (getSwitch("CONNECTION"),"CONNECTED.FIRMATA VERSION:%s\n",sf->firmata_name);
            return true;
        } else {
	DEBUG(INDI::Logger::DBG_SESSION,"Failed, trying next port.\n");
	//delete sf;
	//return false;
        }
    }
    IDLog("ARDUINO BOARD FAIL TO CONNECT.\n");
    IDSetSwitch (getSwitch("CONNECTION"),"ARDUINO BOARD FAIL TO CONNECT. CHECK PORT NAME\n"); //TODO: is IDSetSwitch correct?
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
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
    }

    return true;
}

bool RollOff::Disconnect()
{
    sf->closePort();
    delete sf;	
    IDLog("ARDUINO BOARD DISCONNECTED.\n");
    IDSetSwitch (getSwitch("CONNECTION"),"DISCONNECTED\n");
    return true;
}


void RollOff::TimerHit()
{

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
               SetParked(false);
               return;
           }
       }
       // Roll Off is closing
       else if (DomeMotionS[DOME_CCW].s == ISS_ON)
       {
           if (getFullClosedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
               SetParked(true);
               return;
           }
       }

       SetTimer(1000);
   }


    //SetTimer(1000);
}

bool RollOff::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully opened.");
            return false;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions are in the danger zone. Cannot open roof.");
            return false;
        }
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully closed.");
            return false;
        }
        else if (dir == DOME_CW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 2");
            sf->writeDigitalPin(3,ARDUINO_LOW);
            sf->writeDigitalPin(4,ARDUINO_LOW);
            sf->writeDigitalPin(2,ARDUINO_HIGH);
        }                    
        else if (dir == DOME_CCW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 3");
            sf->writeDigitalPin(2,ARDUINO_LOW);
            sf->writeDigitalPin(4,ARDUINO_LOW);
            sf->writeDigitalPin(3,ARDUINO_HIGH);
        }                    

        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;
        MotionRequest = ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        SetTimer(1000);
        return true;
    }
    else
    {
        return Dome::Abort();

    }

    return false;

}

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

bool RollOff::getFullOpenedLimitSwitch()
{    
    sf->OnIdle();
    if (sf->pin_info[8].value > 0) {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully open switch ON");
        DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 4(stop)");
        sf->writeDigitalPin(2,ARDUINO_LOW);
        sf->writeDigitalPin(3,ARDUINO_LOW);
        sf->writeDigitalPin(4,ARDUINO_HIGH);
        return true;
    } else {
//        DEBUG(INDI::Logger::DBG_SESSION, "Fully open switch OFF");
        return false;
    }
}

bool RollOff::getFullClosedLimitSwitch()
{    
    if (sf->pin_info[9].value > 0) {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully Closed switch ON");
        DEBUG(INDI::Logger::DBG_SESSION, "Switching ON arduino pin 4(stop)");
        sf->writeDigitalPin(2,ARDUINO_LOW);
        sf->writeDigitalPin(3,ARDUINO_LOW);
        sf->writeDigitalPin(4,ARDUINO_HIGH);
        return true;
    } else {
//        DEBUG(INDI::Logger::DBG_SESSION, "Fully Closed switch OFF");
        return false;
    }
}

