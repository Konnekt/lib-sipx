//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


// SYSTEM INCLUDES
#include <assert.h>

// APPLICATION INCLUDES
#include <net/SipServerBase.h>
#include <net/SipUserAgent.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
SipServerBase::SipServerBase(SipUserAgent* sipUserAgent,
                             const UtlString& defaultDomain)
:mSipUserAgent(NULL)
{
   mSipUserAgent = sipUserAgent;
   if(!defaultDomain.isNull())
   {
      mDefaultDomain.remove(0);
      mDefaultDomain.append(defaultDomain);
   }

}

// Copy constructor
SipServerBase::SipServerBase(const SipServerBase& rSipServerBase)
{
}

// Destructor
SipServerBase::~SipServerBase()
{
   mSipUserAgent = NULL;
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
SipServerBase& 
SipServerBase::operator=(const SipServerBase& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}



/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */




