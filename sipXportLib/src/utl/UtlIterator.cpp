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
#include "utl/UtlIterator.h"
#include "utl/UtlContainer.h"
#include "os/OsLock.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor

// in this case, we don't need to acquire the sIteratorConnectionLock or take the
// mContainerRefLock because this is constructor,
// so no one has the pointer to this iterator yet.

UtlIterator::UtlIterator(const UtlContainer& container)
   : mContainerRefLock(OsBSem::Q_PRIORITY, OsBSem::FULL),
     mpMyContainer(const_cast<UtlContainer*>(&container))
{
}
  

// Copy constructor


// Destructor
UtlIterator::~UtlIterator() 
{
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator


/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/*****************************************************************
 * The following routines are here to provide (protected) access
 * to the mpIteratorList in the associated UtlContainer.  These
 * operations are invoked here from the derived iterator class
 * constructors and destructors rather than in the UtlIterator
 * constructor and destructor so that the locks can be taken and
 * released in the correct order.  (E.g., if invalidate() was called
 * from ~UtlContainer, then a subclass's data structure would be
 * released before the iterators were invalidated.)
 *****************************************************************/
  
void UtlIterator::addToContainer(const UtlContainer* container)
{
   // caller is already holding the mContainerLock
   container->addIterator(this);
}

/**
 * invalidate is called by the UtlContainer from its destructor.
 * It disconnects the iterator from its container object (sets mpMyContainer to
 * NULL).
 *
 * Any subsequent invocation of this iterator (other than its
 * destructor) must not attempt to access *mpMyContainer.
 */
void UtlIterator::invalidate()
{
   // Caller holds sIteratorConnectionLock.

   // The caller is holding the sIteratorConnectionLock, so it is OK to lock
   // this.
   OsLock takeContainer(mContainerRefLock);
   mpMyContainer = NULL;
   // it may be that more is needed in the subclasses, but this provides the failsafe
}
