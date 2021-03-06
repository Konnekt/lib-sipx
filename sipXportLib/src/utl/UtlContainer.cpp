//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include "utl/UtlContainer.h"
#include "utl/UtlLink.h"
#include "utl/UtlIterator.h"
#include "os/OsLock.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
const UtlContainableType UtlContainer::TYPE = "UtlContainer" ;

// STATIC VARIABLE INITIALIZATIONS
/*
 * spIteratorConnectionLock is implemented as a pointer to a dynamically allocated
 * lock here so that it will be constructed at initialization time (the
 * 'new' below), but _never_ destructed (as it would be if declared an OsBSem rather
 * than an OsBSem*).  This is because we cannot control the order of destructors
 * between modules, and this lock needs to exist until all container and iterator
 * destructors have been run - so we deliberately leak it.
 */
OsBSem* UtlContainer::spIteratorConnectionLock = new OsBSem(OsBSem::Q_PRIORITY, OsBSem::FULL);

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor

UtlContainer::UtlContainer()
   : mContainerLock(OsBSem::Q_PRIORITY, OsBSem::FULL)
{
}


// Destructor
UtlContainer::~UtlContainer() 
{
}

// invalidateIterators() isn't called by the UtlContainer destructor
// (and need not be, since no iterators are defined for UtlContainer),
// but it is used by the methods for various subclasses.
void UtlContainer::invalidateIterators()
{
   UtlLink*        listNode;
   UtlIterator*    foundIterator;

   // The caller is holding the sIteratorConnectionLock and mContainerLock.
       
   // Walk the list to notify the iterators.
   for (listNode = static_cast<UtlLink*>(mIteratorList.head());
        listNode != NULL;
        listNode = listNode->next()
        )
   {
      foundIterator = (UtlIterator*)listNode->data;
      foundIterator->invalidate();
   }

   assert(mIteratorList.isUnLinked());// some iterator failed to call removeIterator
}

/* ============================ MANIPULATORS ============================== */




/* ============================ ACCESSORS ================================= */

// These are here because every UtlContainer is also a UtlContainable

/**
 * Calculate a unique hash code for this object.  If the equals
 * operator returns true between two objects, then both objects
 * must have the same hash code.
 */
unsigned UtlContainer::hash() const
{
   // default implementation
   return (unsigned) this;
}


/**
 * Get the ContainableType for a UtlContainable derived class.
 */
UtlContainableType UtlContainer::getContainableType() const
{
   return UtlContainer::TYPE;
}

/* ============================ INQUIRY =================================== */


/**
 * Compare the this object to another like object.  Results for 
 * comparing with a non-like object are undefined.
 *
 * @returns 0 if equal, <0 if less than and >0 if greater.
 */
int UtlContainer::compareTo(const UtlContainable* otherObject) const
{
   return ((unsigned) this) - ((unsigned) otherObject);
}


/* //////////////////////////// PROTECTED ///////////////////////////////// */

/// Lock the linkage between containers and iterators
void UtlContainer::acquireIteratorConnectionLock()
{
   spIteratorConnectionLock->acquire();
}

/// Unlock the linkage between containers and iterators
void UtlContainer::releaseIteratorConnectionLock()
{
   spIteratorConnectionLock->release();
}

void UtlContainer::addIterator(UtlIterator* newIterator) const
{
   // Caller must be holding this->mContainerLock.
   // But it need not be holding newIterator->mpContainerRefLock, because
   // we do not set newIterator->mpMyContainer.

   // This method is declared const because it makes no change that
   // any other method can detect in the container, but it actually
   // does make a change, so we have to cast away the const.
   UtlContainer* my = const_cast<UtlContainer*>(this);
   
    if(newIterator)
    {
       // :HACK: note that we are storing a UtlIterator* in the UtlContainer* pointer
       UtlLink* iteratorLink = UtlLink::get();
       iteratorLink->data = (UtlContainer*)newIterator;
       iteratorLink->UtlChain::listBefore(&my->mIteratorList, NULL);
    }
}


void UtlContainer::removeIterator(UtlIterator *existingIterator) const
{
   // Caller must be holding this->mContainerLock.
   // But it need not be holding newIterator->mpContainerRefLock, because
   // we do not set newIterator->mpMyContainer.

   // This method is declared const because it makes no change that
   // any other method can detect in the container, but it actually
   // does make a change, so we have to cast away the const.
   
   if(existingIterator)
   {
      UtlContainer *my = (UtlContainer*)this;
      UtlLink* iteratorLink;

      // :HACK: note that we are storing a UtlIterator* in the UtlContainer* pointer
      iteratorLink = UtlLink::findData(&my->mIteratorList, (UtlContainer*)existingIterator);
      if (iteratorLink)
      {
         iteratorLink->detachFrom(&my->mIteratorList);
      }
   }
}
