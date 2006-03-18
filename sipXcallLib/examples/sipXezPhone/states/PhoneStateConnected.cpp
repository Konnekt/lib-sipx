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
#include "PhoneStateConnected.h"
#include "PhoneStateIdle.h"
#include "PhoneStateDisconnectRequested.h"
#include "PhoneStateTransferRequested.h"
#include "PhoneStateLocalHoldRequested.h"
#include "PhoneStateCallHeldRemotely.h"
#include "../sipXezPhoneApp.h"


// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
extern sipXezPhoneApp* thePhoneApp;
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
// MACRO CALLS

PhoneStateConnected::PhoneStateConnected()
{
}

PhoneStateConnected::~PhoneStateConnected(void)
{
}

PhoneState* PhoneStateConnected::OnFlashButton()
{
   return (new PhoneStateDisconnectRequested());
}

PhoneState* PhoneStateConnected::OnDisconnected(SIPX_CALL hCall)
{
   //if (hCall == sipXmgr::getInstance().getCurrentCall())
   //{
      if (sipXmgr::getInstance().disconnect(hCall, false))
      {
        return (new PhoneStateIdle());
      }
      else
      {
          return this;
      }
   /*}
   else
   {
      return this;
   }*/
}

PhoneState* PhoneStateConnected::OnHoldButton()
{
    return (new PhoneStateLocalHoldRequested(mhCall));
}

PhoneState* PhoneStateConnected::OnTransferRequested(const wxString phoneNumber)
{
    return (new PhoneStateTransferRequested(phoneNumber));
}

PhoneState* PhoneStateConnected::OnConnectedInactive()
{
    return (new PhoneStateCallHeldRemotely());
}

PhoneState* PhoneStateConnected::Execute()
{
   thePhoneApp->setStatusMessage("Connected.");
   
   return this;
}
