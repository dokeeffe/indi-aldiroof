#ifndef AldiRoof_H
#define AldiRoof_H

#include <indidome.h>

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>

/* Firmata */
#include "firmata.h"


class AldiRoof : public INDI::Dome
{

    public:
        AldiRoof();
        virtual ~AldiRoof();

        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();

      protected:

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState Park();
        virtual IPState UnPark();                
        virtual bool Abort();

        virtual bool getFullOpenedLimitSwitch();
        virtual bool getFullClosedLimitSwitch();

    private:

        ISState fullOpenLimitSwitch;
        ISState fullClosedLimitSwitch;

        double MotionRequest;
        struct timeval MotionStart;
        bool SetupParms();
        
        float CalcTimeLeft(timeval);
        
        Firmata* sf;

};

#endif
