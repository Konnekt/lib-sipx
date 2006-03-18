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
#include "os/OsSharedLibMgr.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
OsSharedLibMgrBase* OsSharedLibMgrBase::spInstance = 0;
OsBSem  OsSharedLibMgrBase::sLock(OsBSem::Q_PRIORITY, OsBSem::FULL);

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

OsSharedLibMgrBase* OsSharedLibMgrBase::getOsSharedLibMgr()
{

   if (spInstance != NULL)
      return spInstance;

   // If the task does not yet exist or hasn't been started, then acquire
   // the lock to ensure that only one instance of the task is started
   sLock.acquire();
   if (spInstance == NULL)
       spInstance = new OsSharedLibMgr();

   sLock.release();

   return spInstance;
}

// Constructor
OsSharedLibMgrBase::OsSharedLibMgrBase()
{
}

// Copy constructor
OsSharedLibMgrBase::OsSharedLibMgrBase(const OsSharedLibMgrBase& rOsSharedLibMgrBase)
{
}

// Destructor
OsSharedLibMgrBase::~OsSharedLibMgrBase()
{
    mLibraryHandles.destroyAll();
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
OsSharedLibMgrBase&
OsSharedLibMgrBase::operator=(const OsSharedLibMgrBase& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */


