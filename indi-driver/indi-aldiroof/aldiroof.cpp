/*******************************************************************************
Aldi hoist powered observatory roof driver.

Controls an arduino using firmata to switch on/off relays connected to a 550w 220v electric hoist.

NOTE: Firmata does not function well over USB3. Always use a USB2 port!

There are several safety overrides in place to stop the motors from 'going mad'.
1) Electro-Mechanical: This is the primary safety cut out. The hoist has 2 microswitches which are normally used to stop the hoist when the load is fully lifted or cable is fully extended.
       These microswitches are attached (via bicycle brake cables) to mechanical-levers on the roof that get actuated when fully open/closed.
       These will cut power to the hoist when fully open/closed
2) INDI driver: 2 additional microswitches are used as digital inputs to the arduino. These are attached to the same mechanical-levers in 1 above. These digital inputs are used as the FullyClosedLimitSwitch and FullyOpenLimitSwitch in the code below. The indi driver (this code) will send a signal to the arduino to stop the motors once the fully open/close switch is activated

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

std::unique_ptr<AldiRoof> rollOff(new AldiRoof());

#define MAX_ROLLOFF_DURATION    17      // This is the max ontime for the motors. Safety cut out. Although a lot of damage can be done on this time!! 

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(rollOff.get() == 0) rollOff.reset(new AldiRoof());

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

AldiRoof::AldiRoof()
{
  fullOpenLimitSwitch   = ISS_OFF;
  fullClosedLimitSwitch = ISS_OFF;
  MotionRequest=0;
  IsTelescopeParked=false;
  SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/**
 * Init all properties
 */
bool AldiRoof::initProperties()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "Init props");
    INDI::Dome::initProperties();
    SetParkDataType(PARK_NONE);
    addAuxControls();
    IDSnoopDevice(ActiveDeviceT[0].text,"TELESCOPE_PARK");
    IUFillSwitch(&ParkableWhenScopeUnparkedS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&ParkableWhenScopeUnparkedS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&ParkableWhenScopeUnparkedSP, ParkableWhenScopeUnparkedS, 2, getDeviceName(), "DOME_PARKABLEWHENSCOPEUNPARKED", "Scope park aware", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillText(&CurrentStateT[0],"State","Roof State","UNKNOWN");
    IUFillTextVector(&CurrentStateTP,CurrentStateT,2,getDeviceName(),"STATE","ROOF_STATE",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);
    return true;
}

/**
 * Snoop on the telescope's park state then delegate to the base indi dome
 * 
 */
bool AldiRoof::ISSnoopDevice (XMLEle *root)
{
	XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");
    if (!strcmp("TELESCOPE_PARK", propName))
    {
	for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "PARK"))
            {
                if (!strcmp(pcdataXMLEle(ep), "On"))
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "snooped park state PARKED");
					IsTelescopeParked = true;
                }
                else
                {
                    DEBUG(INDI::Logger::DBG_DEBUG, "snooped park state UNPARKED");
					IsTelescopeParked = false;
                }
            }
        }
        return true;
    }
	return INDI::Dome::ISSnoopDevice(root);
}


bool AldiRoof::SetupParms()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "Setting up params");
    fullOpenLimitSwitch   = ISS_OFF;
    fullClosedLimitSwitch = ISS_OFF;
    if (getFullOpenedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_DEBUG, "Setting open flag on NOT PARKED");
        fullOpenLimitSwitch = ISS_ON;
        setDomeState(DOME_IDLE);
    }
    if (getFullClosedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_DEBUG, "Setting closed flag on PARKED");
        fullClosedLimitSwitch = ISS_ON;
        setDomeState(DOME_PARKED);
    }
    
    return true;
}


bool AldiRoof::Connect()
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Attempting connection %s",PortT[0].text);
    if(!strcmp(PortT[0].text, "*")) {
		DEBUG(INDI::Logger::DBG_SESSION, "Wildcard specified. Checking all ttyACM* devices for roof");
	}
    sf = new Firmata(PortT[0].text);
    if (sf->portOpen) {
		if (strstr(sf->firmata_name, "SimpleDigitalFirmataRoofController")) {
			DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD CONNECTED.");
			DEBUGF(INDI::Logger::DBG_DEBUG, "FIRMATA VERSION:%s",sf->firmata_name);
			return true;
		} else {
		    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD INCOMPATABLE FIRMWARE.");
			DEBUGF(INDI::Logger::DBG_DEBUG, "FIRMATA VERSION:%s",sf->firmata_name);
			return false;
		}
    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD FAIL TO CONNECT");
        delete sf;
        return false;
    }
}

AldiRoof::~AldiRoof()
{

}

const char * AldiRoof::getDefaultName()
{
        return (char *)"Aldi Roof";
}

/**
 * Handle the custom switch in the options tab for telescope park awareness
 */
bool AldiRoof::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
		if (!strcmp(name, ParkableWhenScopeUnparkedSP.name))
        {
            IUUpdateSwitch(&ParkableWhenScopeUnparkedSP, states, names, n);

            ParkableWhenScopeUnparkedSP.s = IPS_OK;

            if (ParkableWhenScopeUnparkedS[0].s == ISS_ON)
                DEBUG(INDI::Logger::DBG_WARNING, "Warning: Roof is parkable when telescope state is unparked or unknown. Only enable this option is parking the dome at any time will not cause damange to any equipment.");
            else
                DEBUG(INDI::Logger::DBG_SESSION, "Scope park aware is disabled. Roof can close when scope unparked or unknown");

            IDSetSwitch(&ParkableWhenScopeUnparkedSP, NULL);

            return true;
        }
	}
	return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}


bool AldiRoof::updateProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Updating props");
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
        defineSwitch(&ParkableWhenScopeUnparkedSP);
        defineText(&CurrentStateTP);
    } else 
    {
		deleteProperty(ParkableWhenScopeUnparkedSP.name);
		deleteProperty(CurrentStateTP.name);
	}

    return true;
}

/**
* Disconnect from the arduino
**/
bool AldiRoof::Disconnect()
{
    sf->closePort();
    delete sf;	
    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD DISCONNECTED.");
    IDSetSwitch (getSwitch("CONNECTION"),"DISCONNECTED\n");
    return true;
}

/**
 * Returns true if the snooped property for telescope park state is true
 */ 
bool AldiRoof::isTelescopeParked()
{
  return IsTelescopeParked;
}

/**
 * TimerHit gets called by the indi client every 1sec when the roof is moving.
 */
void AldiRoof::TimerHit()
{

    DEBUG(INDI::Logger::DBG_DEBUG, "Timer hit");
    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore    

   if (DomeMotionSP.s == IPS_BUSY)
   {
       // Abort called
       if (MotionRequest < 0)
       {
           DEBUG(INDI::Logger::DBG_SESSION, "Roof motion is stopped.");
           setDomeState(DOME_IDLE);
           IDSetText(&CurrentStateTP, "ABORTED");
           SetTimer(500);
           return;
       }

       // Roll off is opening
       if (DomeMotionS[DOME_CW].s == ISS_ON)
       {
           IDSetText(&CurrentStateTP, "OPENING");
           if (getFullOpenedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is open.");
               IDSetText(&CurrentStateTP, "OPEN");
               setDomeState(DOME_IDLE);
               DEBUG(INDI::Logger::DBG_SESSION, "Sending ABORT to stop motion");
               sf->sendStringData((char *)"ABORT");
               //SetParked(false); 
               //calling setParked(false) here caauses the driver to crash with nothing logged (looks like possibly an issue writing parking data). Therefore the next 4 lines are doing what is done in indidome.cpp' function. We dont care about parking data anyway as we get the parked state directly from the roof stop-switches.
               IUResetSwitch(&ParkSP);
               ParkS[1].s = ISS_ON;
               ParkSP.s = IPS_OK;
               //IDSetSwitch(&ParkSP, NULL);
               return;
           }
           if (CalcTimeLeft(MotionStart) <= 0) {
               DEBUG(INDI::Logger::DBG_SESSION, "Exceeded max motor run duration. Aborting.");
               Abort();
           }
       }
       // Roll Off is closing
       else if (DomeMotionS[DOME_CCW].s == ISS_ON)
       {
           IDSetText(&CurrentStateTP, "CLOSING");
           if (getFullClosedLimitSwitch())
           {
                DEBUG(INDI::Logger::DBG_SESSION, "Sending ABORT to stop motion");
                sf->sendStringData((char *)"ABORT");
                DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
                IDSetText(&CurrentStateTP, "CLOSED");
                setDomeState(DOME_PARKED);
                //SetParked(true);
                return;
           }
           if (CalcTimeLeft(MotionStart) <= 0) {
               DEBUG(INDI::Logger::DBG_SESSION, "Exceeded max motor run duration. Aborting.");
               IDSetText(&CurrentStateTP, "OVERRUN");
               Abort();
           }
       }
       SetTimer(500);
   }
}

bool AldiRoof::saveConfigItems(FILE *fp)
{
    IUSaveConfigSwitch(fp, &ParkableWhenScopeUnparkedSP);
    return INDI::Dome::saveConfigItems(fp);
}

/**
 * Move the roof. Send the command string over frimata to the arduino.
 * 
 **/
IPState AldiRoof::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        updateProperties();
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
        else if (dir == DOME_CCW && isTelescopeParked() == false && ParkableWhenScopeUnparkedS[0].s == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close roof until the telescope is parked. Please park the scope or disable Scope park aware in the options");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Sending command OPEN");
            sf->sendStringData((char *)"OPEN");
        }                    
        else if (dir == DOME_CCW)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Sending command CLOSE");
            sf->sendStringData((char *)"CLOSE");
        }                    

        MotionRequest = MAX_ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        SetTimer(500);
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
IPState AldiRoof::Park()
{    
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (!rc==IPS_ALERT)
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
IPState AldiRoof::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (!rc==IPS_ALERT)
    {       
           DEBUG(INDI::Logger::DBG_SESSION, "Roll off is unparking...");
           return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

/**
 * Abort motion.
 **/
bool AldiRoof::Abort()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Sending command ABORT");
    sf->sendStringData((char *)"ABORT");
    MotionRequest=-1;

    // If both limit switches are off, then we're neither parked nor unparked.
    if (getFullOpenedLimitSwitch() == false && getFullClosedLimitSwitch() == false)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        IDSetSwitch(&ParkSP, NULL);
    }

    return true;
}

float AldiRoof::CalcTimeLeft(timeval start)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=MotionRequest-timesince;
    return timeleft;
}

/**
 * Get the state of the full open limit switch. This function will also switch off the motors as a safety override.
 **/
bool AldiRoof::getFullOpenedLimitSwitch()
{    
    DEBUG(INDI::Logger::DBG_SESSION, "Sending QUERY command to determine roof state");
    sf->sendStringData((char*)"QUERY");
    sf->OnIdle();
    DEBUGF(INDI::Logger::DBG_SESSION, "QUERY resp=%s",sf->string_buffer);
    if (strcmp(sf->string_buffer,"OPEN")==0) { 
        fullOpenLimitSwitch = ISS_ON;
        return true;
    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully open switch OFF");
        return false;
    }
}

/**
 * Get the state of the full closed limit switch. This function will also switch off the motors as a safety override.
 **/
bool AldiRoof::getFullClosedLimitSwitch()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Sending QUERY command to determine roof state");
    sf->sendStringData((char*)"QUERY");
    sf->OnIdle();
    DEBUGF(INDI::Logger::DBG_SESSION, "QUERY resp=%s",sf->string_buffer);
    if (strcmp(sf->string_buffer,"CLOSED")==0) { 
        fullClosedLimitSwitch = ISS_ON;
        return true;
    } else {
        DEBUG(INDI::Logger::DBG_SESSION, "Fully Closed switch OFF");
        return false;
    }
}

