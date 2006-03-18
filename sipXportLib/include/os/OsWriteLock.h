//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _OsWriteLock_h_
#define _OsWriteLock_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "OsRWMutex.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//:Lock class allowing exclusive access for a writer within a critical section
// This class uses OsRWMutex objects for synchronization. The constructor
// for the class automatically blocks until the OsRWMutex is acquired
// for writing. Similarly, the destructor automatically releases the lock.
// <p>
// The easiest way to use this object as a guard is to create the
// object as a variable on the stack just before the section that needs to
// lock the resource for writing. When the OsWriteLock object goes out of
// scope, the writer lock will be automatically released.
// An example of this form of use is shown below.
// <p>
// <font face="courier">
// &nbsp;&nbsp;                      someMethod()                    <br>
// &nbsp;&nbsp;                      {                               <br>
// &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;      OsWriteLock lock(myRWMutex);  <br>
//                                                                   <br>
// &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;      < section that needs to be
//                                       locked for writing >        <br>
// &nbsp;&nbsp;                      }                               </font>

class OsWriteLock
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   OsWriteLock(OsRWMutex& rRWMutex)
                : mrRWMutex(rRWMutex) { rRWMutex.acquireWrite(); };
     //:Constructor

   virtual
   ~OsWriteLock()  { mrRWMutex.releaseWrite(); };
     //:Destructor

/* ============================ MANIPULATORS ============================== */

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
   OsRWMutex& mrRWMutex;

   OsWriteLock();
     //:Default constructor (not implemented for this class)

   OsWriteLock(const OsWriteLock& rOsWriteLock);
     //:Copy constructor (not implemented for this class)

   OsWriteLock& operator=(const OsWriteLock& rhs);
     //:Assignment operator (not implemented for this class)

};

/* ============================ INLINE METHODS ============================ */

#endif  // _OsWriteLock_h_
