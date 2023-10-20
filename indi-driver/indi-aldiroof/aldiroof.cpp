/*******************************************************************************
Aldi hoist powered observatory roof driver.

Controls an arduino using firmata to switch on/off relays + 2x2pole contactors connected to a 550w 220v electric hoist.

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
#include "connectionplugins/connectionserial.h"

std::unique_ptr<AldiRoof> rollOff(new AldiRoof());

#define MAX_ROLLOFF_DURATION    19      // This is the max ontime for the motors. Safety cut out. Although a lot of damage can be done on this time!!

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
  SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/**
 * Init all properties
 */
bool AldiRoof::initProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Init props");
    INDI::Dome::initProperties();
    SetParkDataType(PARK_NONE);
    addAuxControls();
    IUFillText(&CurrentStateT[0],"State","Roof State",NULL);
    IUFillTextVector(&CurrentStateTP,CurrentStateT,1,getDeviceName(),"STATE","ROOF_STATE",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);
    return true;
}

bool AldiRoof::ISSnoopDevice (XMLEle *root)
{
	return INDI::Dome::ISSnoopDevice(root);
}


bool AldiRoof::SetupParms()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Setting up params");
    //InitPark();
    fullOpenLimitSwitch   = ISS_OFF;
    fullClosedLimitSwitch = ISS_OFF;
    string roofStateString = "UNKNOWN";
    if (getFullOpenedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_DEBUG, "Setting open flag on NOT PARKED");
        fullOpenLimitSwitch = ISS_ON;
        setDomeState(DOME_IDLE);
        roofStateString = "OPEN";
    }
    if (getFullClosedLimitSwitch()) {
        DEBUG(INDI::Logger::DBG_SESSION, "Setting closed flag on PARKED");
        fullClosedLimitSwitch = ISS_ON;
        setDomeState(DOME_PARKED);
        if(isParked()) {
          roofStateString = "PARKED CLOSED";
        } else {
          roofStateString = "CLOSED";
        }
    }
    char status[32];
    strcpy(status, roofStateString.c_str());
    IUSaveText(&CurrentStateT[0], status);
    IDSetText(&CurrentStateTP, NULL);
    return true;
}


bool AldiRoof::Connect()
{
    sf = new Firmata(serialConnection->port());
    if (sf->portOpen) {
		if (strstr(sf->firmata_name, "SimpleDigitalFirmataRoofController")) {
			DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD CONNECTED.");
			DEBUGF(INDI::Logger::DBG_SESSION, "FIRMATA VERSION:%s",sf->firmata_name);
			return true;
		} else {
		    DEBUG(INDI::Logger::DBG_SESSION, "ARDUINO BOARD INCOMPATABLE FIRMWARE.");
		    DEBUGF(INDI::Logger::DBG_SESSION, "FIRMATA VERSION:%s",sf->firmata_name);
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

bool AldiRoof::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}


bool AldiRoof::updateProperties()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Updating props");
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
        defineText(&CurrentStateTP);
    } else
    {
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
           string stateString = "ABORTED";
           char status[32];
           strcpy(status, stateString.c_str());
           IUSaveText(&CurrentStateT[0], status);
           IDSetText(&CurrentStateTP, NULL);
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
               setDomeState(DOME_UNPARKED);
               DEBUG(INDI::Logger::DBG_SESSION, "Sending ABORT to stop motion");
               sf->sendStringData((char *)"ABORT");
               SetParked(false);
               IUResetSwitch(&ParkSP);
               ParkS[1].s = ISS_ON;
               ParkSP.s = IPS_OK;
               //IDSetSwitch(&ParkSP, NULL);
               string stateString = "OPEN";
               char status[32];
               strcpy(status, stateString.c_str());
               IUSaveText(&CurrentStateT[0], status);
               IDSetText(&CurrentStateTP, NULL);
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
           if (getFullClosedLimitSwitch())
           {
                DEBUG(INDI::Logger::DBG_SESSION, "Sending ABORT to stop motion");
                sf->sendStringData((char *)"ABORT");
                DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
                setDomeState(DOME_PARKED);
                SetParked(true);
                string stateString = "CLOSED";
                char status[32];
                strcpy(status, stateString.c_str());
                IUSaveText(&CurrentStateT[0], status);
                IDSetText(&CurrentStateTP, NULL);
                return;
           }
           if (CalcTimeLeft(MotionStart) <= 0) {
               DEBUG(INDI::Logger::DBG_SESSION, "Exceeded max motor run duration. Aborting.");
               Abort();
           }
       }
       SetTimer(500);
   }
}

bool AldiRoof::saveConfigItems(FILE *fp)
{
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
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close dome when mount is locking. See: Telescope parkng policy, in options tab");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully closed.");
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
        DEBUG(INDI::Logger::DBG_SESSION, "return IPS_BUSY");
        return IPS_BUSY;
    }
    else
    {
        DEBUG(INDI::Logger::DBG_SESSION, "WTF WTF ");
        return (Dome::Abort() ? IPS_OK : IPS_ALERT);

    }
    DEBUG(INDI::Logger::DBG_SESSION, "return IPS_ALERT");
    return IPS_ALERT;

}

/**
 * Park the roof = close
 **/
IPState AldiRoof::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc==IPS_BUSY)
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
    if (rc==IPS_BUSY) {
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

    // If both limit switches are off, then we're neither parked nor unparked or a hardware failure (cable / rollers / jam).
    if (getFullOpenedLimitSwitch() == false && getFullClosedLimitSwitch() == false)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_ALERT;
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
