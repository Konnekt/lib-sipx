//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES
#include <assert.h>

// APPLICATION INCLUDES
#include "os/linux/OsFileInfoLinux.h"


// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
OsFileInfoLinux::OsFileInfoLinux()
{
}

// Copy constructor
OsFileInfoLinux::OsFileInfoLinux(const OsFileInfoLinux& rOsFileInfo)
{
}

// Destructor
OsFileInfoLinux::~OsFileInfoLinux()
{
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
OsFileInfoLinux&
OsFileInfoLinux::operator=(const OsFileInfoLinux& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

UtlBoolean  OsFileInfoLinux::isReadOnly() const
{
    UtlBoolean retval = TRUE;

    return retval;
}

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */


/* ============================ FUNCTIONS ================================= */



