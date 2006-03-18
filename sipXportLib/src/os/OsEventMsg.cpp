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
#include <assert.h>

// APPLICATION INCLUDES
#include "os/OsEventMsg.h"
#include "os/OsQueuedEvent.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
OsEventMsg::OsEventMsg(const unsigned char subType,
                       const OsQueuedEvent& rEvent,
                       const int eventData,
                       const OsTime& rTimestamp)
:  OsMsg(OsMsg::OS_EVENT, subType),
   mEventData(eventData),
   mTimestamp(rTimestamp)
{
   OsStatus res;

   res = rEvent.getUserData(mUserData);
   assert(res == OS_SUCCESS);
}

// Copy constructor
OsEventMsg::OsEventMsg(const OsEventMsg& rOsEventMsg)
:  OsMsg(rOsEventMsg)
{
   mEventData = rOsEventMsg.mEventData;
   mUserData  = rOsEventMsg.mUserData;
   mTimestamp = rOsEventMsg.mTimestamp;
}

// Create a copy of this msg object (which may be of a derived type)
OsMsg* OsEventMsg::createCopy(void) const
{
   return new OsEventMsg(*this);
}

// Destructor
OsEventMsg::~OsEventMsg()
{
   // no work required
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
OsEventMsg& 
OsEventMsg::operator=(const OsEventMsg& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   OsMsg::operator=(rhs);       // assign fields for parent class

   mEventData = rhs.mEventData;
   mUserData  = rhs.mUserData;
   mTimestamp = rhs.mTimestamp;

   return *this;
}

/* ============================ ACCESSORS ================================= */

// Return the size of the message in bytes.
// This is a virtual method so that it will return the accurate size for
// the message object even if that object has been upcast to the type of
// an ancestor class.
int OsEventMsg::getMsgSize(void) const
{
   return sizeof(*this);
}

// Return the event data that was signaled by the notifier task.
// Always returns OS_SUCCESS.
OsStatus OsEventMsg::getEventData(int& rEventData) const
{
   rEventData = mEventData;
   return OS_SUCCESS;
}

// Return the timestamp associated with this event.
// Always returns OS_SUCCESS.
OsStatus OsEventMsg::getTimestamp(OsTime& rTimestamp) const
{
   rTimestamp = mTimestamp;
   return OS_SUCCESS;
}

// Return the user data specified when the OsQueuedEvent was constructed.
// Always returns OS_SUCCESS.
OsStatus OsEventMsg::getUserData(int& rUserData) const
{
   rUserData = mUserData;
   return OS_SUCCESS;
}

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */


