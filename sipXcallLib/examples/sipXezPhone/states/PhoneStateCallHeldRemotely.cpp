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
#include "PhoneStateCallHeldRemotely.h"
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

PhoneStateCallHeldRemotely::PhoneStateCallHeldRemotely()
{
}

PhoneStateCallHeldRemotely::~PhoneStateCallHeldRemotely(void)
{
}

PhoneState* PhoneStateCallHeldRemotely::OnFlashButton()
{
   return (new PhoneStateDisconnectRequested());
}

PhoneState* PhoneStateCallHeldRemotely::OnDisconnected()
{
   sipXmgr::getInstance().disconnect();
   return (new PhoneStateIdle());
}

PhoneState* PhoneStateCallHeldRemotely::OnConnected()
{
    return (new PhoneStateConnected());    
}

PhoneState* PhoneStateCallHeldRemotely::Execute()
{
    thePhoneApp->setStatusMessage("Call On Remote Hold.");
   
   return this;
}
