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
#include "PhoneStateTransferRequested.h"
#include "PhoneStateIdle.h"
#include "PhoneStateMachine.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
// MACRO CALLS

PhoneStateTransferRequested::PhoneStateTransferRequested(const wxString phoneNumber) : 
    mPhoneNumber(phoneNumber)
{
}

PhoneStateTransferRequested::~PhoneStateTransferRequested(void)
{
}


PhoneState* PhoneStateTransferRequested::Execute()
{
   sipxCallBlindTransfer(sipXmgr::getInstance().getCurrentCall(), mPhoneNumber);
   return this;
}
