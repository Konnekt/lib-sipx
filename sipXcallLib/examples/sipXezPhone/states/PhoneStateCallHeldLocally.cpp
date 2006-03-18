//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "../stdwx.h"
#include "../sipXmgr.h"
#include "PhoneStateCallHeldLocally.h"
#include "PhoneStateConnected.h"
#include "PhoneStateIdle.h"
#include "PhoneStateDisconnectRequested.h"
#include "../sipXezPhoneApp.h"


// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
extern sipXezPhoneApp* thePhoneApp;
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
// MACRO CALLS

PhoneStateCallHeldLocally::PhoneStateCallHeldLocally()
{
}

PhoneStateCallHeldLocally::~PhoneStateCallHeldLocally(void)
{
}

PhoneState* PhoneStateCallHeldLocally::OnFlashButton()
{
   return (new PhoneStateDisconnectRequested());
}

PhoneState* PhoneStateCallHeldLocally::OnDisconnected()
{
   sipXmgr::getInstance().disconnect();
   return (new PhoneStateIdle());
}

PhoneState* PhoneStateCallHeldLocally::OnHoldButton()
{
    sipXmgr::getInstance().unholdCurrentCall();
    return (new PhoneStateConnected());    
}

PhoneState* PhoneStateCallHeldLocally::Execute()
{
    sipXmgr::getInstance().holdCurrentCall();
    thePhoneApp->setStatusMessage("Call On Local Hold.");
   
   return this;
}
