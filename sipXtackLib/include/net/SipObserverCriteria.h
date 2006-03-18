//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _SipObserverCriteria_h_
#define _SipObserverCriteria_h_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include <os/OsMsgQ.h>


// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS
class SipSession;

//:Class short description which may consist of multiple lines (note the ':')
// Class detailed description which may extend to multiple lines
class SipObserverCriteria : public UtlString
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   SipObserverCriteria(void* observerData = NULL,
                      OsMsgQ* messageQueue = NULL,
                      const char* sipMethod = NULL,
                      UtlBoolean wantRequests = TRUE,
                      UtlBoolean wantResponses = TRUE,
                      UtlBoolean wantIncoming = TRUE,
                      UtlBoolean wantOutGoing = TRUE,
                      const char* eventName = NULL,
                      SipSession* pSession = NULL);
     //:Default constructor

   virtual
   ~SipObserverCriteria();
     //:Destructor

/* ============================ MANIPULATORS ============================== */


/* ============================ ACCESSORS ================================= */

   OsMsgQ* getObserverQueue();
   void* getObserverData();
   void getEventName(UtlString& eventName);
   SipSession* getSession();

/* ============================ INQUIRY =================================== */

   UtlBoolean wantsRequests();
   UtlBoolean wantsResponses();
   UtlBoolean wantsIncoming();
   UtlBoolean wantsOutGoing();

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

   void* mObserverData;
   UtlBoolean mWantsRequests;
   UtlBoolean mWantsResponses;
   UtlBoolean mWantsIncoming;
   UtlBoolean mWantsOutGoing;
   OsMsgQ* mpMessageObserverQueue;
   UtlString mEventName;
   SipSession* mpSession ;

   SipObserverCriteria& operator=(const SipObserverCriteria& rhs);
     //:Assignment operator (not implemented)

   SipObserverCriteria(const SipObserverCriteria& rSipObserverCriteria);
     //:Copy constructor (not implemented)

};

/* ============================ INLINE METHODS ============================ */

#endif  // _SipObserverCriteria_h_
