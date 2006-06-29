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
#ifdef _WIN32
#include <windows.h>
#endif

// APPLICATION INCLUDES
#include "utl/UtlVoidPtr.h"
#include "utl/UtlInt.h"
#include "tapi/SipXHandleMap.h"
#include "utl/UtlHashMapIterator.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
SipXHandleMap::SipXHandleMap(int startingHandle)
    : mLock(OsMutex::Q_FIFO)
    , mNextHandle(startingHandle)
{
}

// Destructor
SipXHandleMap::~SipXHandleMap()
{
}

/* ============================ MANIPULATORS ============================== */

void SipXHandleMap::addHandleRef(SIPXHANDLE hHandle)
{
    lock() ;    

    UtlInt handle(hHandle) ;

    mLockCountHash.findValue(&handle);
    UtlInt* count = static_cast<UtlInt*>(mLockCountHash.findValue(&handle)) ;
    if (count == NULL)
    {
        mLockCountHash.insertKeyAndValue(new UtlInt(hHandle), new UtlInt(1));
    }
    else
    {
        count->setValue(count->getValue() + 1);
    }
    
    unlock() ;
}

void SipXHandleMap::releaseHandleRef(SIPXHANDLE hHandle)
{
    lock();

    UtlInt handle(hHandle) ;

    UtlInt* pCount = static_cast<UtlInt*>(mLockCountHash.findValue(&handle)) ;
    if (pCount == NULL)
    {
        mLockCountHash.insertKeyAndValue(new UtlInt(hHandle), new UtlInt(0));
    }
    else
    {
        pCount->setValue(pCount->getValue() - 1);
    }

    unlock();
}

void SipXHandleMap::lock()
{
    mLock.acquire() ;
}


void SipXHandleMap::unlock() 
{
    mLock.release() ;
}


SIPXHANDLE SipXHandleMap::allocHandle(const void* pData) 
{
    lock() ;

    SIPXHANDLE hCall = mNextHandle++ ;
    insertKeyAndValue(new UtlInt(hCall), new UtlVoidPtr((void*) pData)) ;
    addHandleRef(hCall);

    unlock() ;

    return hCall ;
}

const void* SipXHandleMap::findHandle(SIPXHANDLE handle) 
{
    lock() ;

    const void* pRC = NULL ;
    UtlInt key(handle) ;
    UtlVoidPtr* pValue ;

    pValue = (UtlVoidPtr*) findValue(&key) ;
    if (pValue != NULL)
    {
        pRC = pValue->getValue() ;
    }

    unlock() ;

    return pRC ;
}


const void* SipXHandleMap::removeHandle(SIPXHANDLE handle) 
{
    lock() ;

    releaseHandleRef(handle);
    const void* pRC = NULL ;
    UtlInt key(handle) ;
        
    UtlInt* pCount = static_cast<UtlInt*>(mLockCountHash.findValue(&key)) ;
        
    if (pCount == NULL || pCount->getValue() < 1)
    {
        UtlVoidPtr* pValue ;
            
        pValue = (UtlVoidPtr*) findValue(&key) ;
        if (pValue != NULL)
        {
            pRC = pValue->getValue() ;                
            destroy(&key) ;
        }

        if (pCount)
        {
            mLockCountHash.destroy(&key);
        }
    }
    
    unlock() ;
    return pRC ;
}


/* ============================ ACCESSORS ================================= */

void SipXHandleMap::dump() 
{
    UtlHashMapIterator itor(*this) ;
    UtlInt* pKey ;
    UtlVoidPtr* pValue ;
        
    while (pKey = (UtlInt*) itor())
    {
        pValue = (UtlVoidPtr*) findValue(pKey) ;
        printf("\tkey=%d, value=%08X\n", pKey->getValue(), 
                pValue ? pValue->getValue() : 0) ;                        
    }       
}

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */

