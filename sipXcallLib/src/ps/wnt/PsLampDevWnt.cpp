//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////



// SYSTEM INCLUDES
#include <assert.h>

// APPLICATION INCLUDES
#include "ps/wnt/PsLampDevWnt.h"
#include "os/OsUtil.h"
#include "ps/PsLampId.h"
#include "ps/PsLampInfo.h"
#include "ps/PsLampTask.h"

// EXTERNAL FUNCTIONS
extern void JNI_LightButton(long);
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor.
// Initialize the lamp device
PsLampDevWnt::PsLampDevWnt(PsLampTask* pLampTask)
: PsLampDev(pLampTask)
{
   assert(pLampTask != NULL);

   // initialize lamp information
   pLampTask->init(4);
   pLampTask->setLampInfo(0, LAMP_HEADSET,  "HEADSET",          PsLampInfo::OFF);
   pLampTask->setLampInfo(1, LAMP_HOLD,     "HOLD",                     PsLampInfo::OFF);
   pLampTask->setLampInfo(2, LAMP_MESSAGES, "VOICE_MAIL",       PsLampInfo::OFF);
   pLampTask->setLampInfo(3, LAMP_MUTE,     "MUTE",                     PsLampInfo::OFF);
   pLampTask->setLampInfo(4, LAMP_SPEAKER,  "SPEAKER",          PsLampInfo::OFF);

}

// Destructor
PsLampDevWnt::~PsLampDevWnt()
{
   // nothing to be done
}

/* ============================ MANIPULATORS ============================== */

// Turn on the indicated lamps
// Each lamp is represented by a bit in the onLamps parameter.
void PsLampDevWnt::lightLamps(unsigned long onLamps)
{
   // Need to call the device in order to light the lamps
//      osPrintf("Lamp bits: %X \n",onLamps);
        JNI_LightButton((long)onLamps);

}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */
