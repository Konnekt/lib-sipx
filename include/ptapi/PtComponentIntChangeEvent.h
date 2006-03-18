//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _PtComponentIntChangeEvent_h_
#define _PtComponentIntChangeEvent_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include "ptapi/PtTerminalComponentEvent.h"
// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//:PtComponentIntChangeEvent contains PtComponent-associated event data,
//:where a component's int type property has changed

class PtComponentIntChangeEvent : public PtTerminalComponentEvent
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   PtComponentIntChangeEvent(const PtComponentIntChangeEvent& rPtComponentIntChangeEvent);
     //:Copy constructor

   virtual
   ~PtComponentIntChangeEvent();
     //:Destructor

/* ============================ MANIPULATORS ============================== */

   PtComponentIntChangeEvent& operator=(const PtComponentIntChangeEvent& rhs);
     //:Assignment operator

/* ============================ ACCESSORS ================================= */
   PtStatus getOldValue(int& rValue);
     //:Returns the component property value before the change as a result 
     //:of the event.
     //!param: (out) rValue - The reference used to return the component property value
     //!retcode: PT_SUCCESS - Success
     //!retcode: PT_PROVIDER_UNAVAILABLE - The provider is not available

   PtStatus getNewValue(int& rValue);
     //:Returns the component property value after the change as a result of 
     //:the event.
     //!param: (out) rValue - The reference used to return the component property value
     //!retcode: PT_SUCCESS - Success
     //!retcode: PT_PROVIDER_UNAVAILABLE - The provider is not available

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
   PtComponentIntChangeEvent();
     //:Default constructor
};

/* ============================ INLINE METHODS ============================ */

#endif  // _PtComponentIntChangeEvent_h_
