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
#include <ActiveCall.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
ActiveCall::ActiveCall(UtlString& callId, CallObject* call)
{
   mCallId = callId;
   mpCall = call;
}


ActiveCall::ActiveCall(UtlString& callId)
{
   mCallId = callId;
}


ActiveCall::~ActiveCall()
{
   mpCall = NULL;
}


int ActiveCall::compareTo(const UtlContainable *b) const
{
   return mCallId.compareTo(((ActiveCall *)b)->mCallId);
}


unsigned int ActiveCall::hash() const
{
    return mCallId.hash();
}


static UtlContainableType DB_ENTRY_TYPE = "ActiveCall";

const UtlContainableType ActiveCall::getContainableType() const
{
    return DB_ENTRY_TYPE;
}


/* ============================ MANIPULATORS ============================== */

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ TESTING =================================== */

/* ============================ FUNCTIONS ================================= */

