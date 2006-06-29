//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _TestRegistrar_h_
#define _TestRegistrar_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include "net/SipUserAgent.h"
#include "os/OsServerTask.h"
// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

typedef struct TestRegistrarUsers
{
    char szUsername[256];
    char szPassword[256];
} TestRegistrarUsers;


/**
 */
class TestRegistrar : public OsServerTask
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
  public:

/* ============================ CREATORS ================================== */

   /**
    * Default constructor
    */
   TestRegistrar();
     
   /**
    * Copy constructor
    */
   TestRegistrar(const TestRegistrar& rTestRegistrar);     

   /**
    * Destructor
    */
   virtual ~TestRegistrar();
   
   /**
    * Starts the registrar and waits for events
    */
   void init();
   
    /**
     * Implementation of OsServerTask's pure virtual method
     */
    UtlBoolean handleMessage(OsMsg& rMsg);   

    void pause(bool bPause);
/* ============================ MANIPULATORS ============================== */

   /**
    * Assignment operator
    *
    * @param rhs right hand side of the equals operator
    */
   TestRegistrar& operator=(const TestRegistrar& rhs);  

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
  protected:
    /**
     * Instance of the user agent object.
     */
    SipUserAgent* mpUserAgent;
/* //////////////////////////// PRIVATE /////////////////////////////////// */
  private:
    /**
     * Message handler for register requests.
     */
    UtlBoolean handleRegisterRequest(SipMessage message);
    bool mbPaused;

};

/* ============================ INLINE METHODS ============================ */

#endif  // _TestRegistrar_h_
