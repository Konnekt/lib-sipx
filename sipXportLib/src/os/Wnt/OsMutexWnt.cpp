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

#include "utl/UtlRscTrace.h"

// APPLICATION INCLUDES
#include "os/Wnt/OsMutexWnt.h"
#include "os/Wnt/OsUtilWnt.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor allowing the user to specify options
OsMutexWnt::OsMutexWnt(const unsigned options)
{
   // Under Windows NT, we ignore the options argument to the constructor
   mMutexImp = CreateMutex(NULL, FALSE, NULL);
}

// Destructor
OsMutexWnt::~OsMutexWnt()
{
   UtlBoolean res;
   res = CloseHandle(mMutexImp);

   assert(res == TRUE);   // CloseHandle should always return TRUE
}

/* ============================ MANIPULATORS ============================== */

// Block the task until the mutex is acquired or the timeout expires
OsStatus OsMutexWnt::acquire(const OsTime& rTimeout)
{
   return OsUtilWnt::synchObjAcquire(mMutexImp, rTimeout);
}

// Conditionally acquire the mutex (i.e., don't block)
// Return OS_BUSY if the lock is held by some other task
OsStatus OsMutexWnt::tryAcquire(void)
{
   return OsUtilWnt::synchObjTryAcquire(mMutexImp);
}

// Release the semaphore
OsStatus OsMutexWnt::release(void)
{
   if (ReleaseMutex(mMutexImp))
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

/* ============================ ACCESSORS ================================= */

// Print mutex information to the console
void OsMutexWnt::OsMutexShow(void)
{
   osPrintf("OsMutex object 0x%08x, no debug info available\n",
            (void*) this);
}
/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */


