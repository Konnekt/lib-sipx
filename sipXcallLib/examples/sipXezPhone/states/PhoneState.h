//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
#ifndef _PhoneState_h_
#define _PhoneState_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include "IStateTransitions.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
 * Abstract Base class from which all PhoneStates are derived.
 * Implements default methods for the transitions.
 */
class PhoneState : public IStateTransitions
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */
   /**
    * PhoneState contructor.
    */
   PhoneState();

   /**
    * PhoneState destructor.
    */
   virtual ~PhoneState();


   virtual PhoneState* OnOffer(const SIPX_CALL hCall);
   virtual PhoneState* OnRemoteBusy();
   virtual PhoneState* OnDisconnected(const SIPX_CALL hCall);



/* ============================ MANIPULATORS ============================== */
/* ============================ ACCESSORS ================================= */
/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:
   SIPX_CALL mhCall;
/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

};
#endif
