//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _STATECHANGENOTIFIER_H_
#define _STATECHANGENOTIFIER_H_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include <net/Url.h>

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS


/**
 * A StateChangeNotifier is used in the dialog monitor when the dialog state is
 * changed from one state to another.
 * 
 * This class is the abstract base from which all state change notifiers must inherit.
 * One method must be implemented by the subclasses:
 * - setState() is for SipDialogMonitor to set the state value based on its
 * notification.
 *
 */

class StateChangeNotifier
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

   enum Status
   {
      PRESENT,
      AWAY,
      OFF_HOOK,
      RINGING,
      ON_HOOK
   };

/* ============================ CREATORS ================================== */

   /// Constructor
   StateChangeNotifier();
  
   /// Destructor
   virtual ~StateChangeNotifier();

   /// Set the status value. Subclasses must provide a definition for this method.
   virtual bool setStatus(const Url& aor, const Status value) = 0;

/* ============================ MANIPULATORS ============================== */

/* ============================ ACCESSORS ================================= */


/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

   /// Disabled copy constructor
   StateChangeNotifier(const StateChangeNotifier& rStateChangeNotifier);

   /// Disabled assignment operator
   StateChangeNotifier& operator=(const StateChangeNotifier& rhs);
   
};

/* ============================ INLINE METHODS ============================ */

#endif  // _STATECHANGENOTIFIER_H_


