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
#include "ps/PsTaoDisplay.h"
#include <os/OsLock.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
PsTaoDisplay::PsTaoDisplay()
{
}

PsTaoDisplay::PsTaoDisplay(const UtlString& rComponentName, int componentType) :
PsTaoComponent(rComponentName, componentType)
{
}

// Copy constructor
PsTaoDisplay::PsTaoDisplay(const PsTaoDisplay& rPsTaoDisplay)
{
}

// Destructor
PsTaoDisplay::~PsTaoDisplay()
{
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
PsTaoDisplay& 
PsTaoDisplay::operator=(const PsTaoDisplay& rhs)
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

