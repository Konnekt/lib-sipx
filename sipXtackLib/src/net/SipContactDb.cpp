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
#include "net/SipContactDb.h"
#include "utl/UtlInt.h"
#include "utl/UtlVoidPtr.h"
#include "os/OsLock.h"
#include "utl/UtlHashMapIterator.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
SipContactDb::SipContactDb() : 
    mNextContactId(1),
    mLock(OsMutex::Q_FIFO),
    mbTurnEnabled(FALSE)
{
    
}

// Destructor
SipContactDb::~SipContactDb()
{
    UtlHashMapIterator iterator(mContacts);
    
    UtlInt* pKey = NULL;
    while (pKey = (UtlInt*)iterator())
    {
        UtlVoidPtr* pValue = NULL;
        pValue = (UtlVoidPtr*)iterator.value();
        if (pValue)
        {
            delete pValue->getValue();
        }
    }
    mContacts.destroyAll();
}


/* ============================ MANIPULATORS ============================== */

const bool SipContactDb::addContact(CONTACT_ADDRESS& contact)
{
    OsLock lock(mLock);
    bool bRet = false;
    
    assert (contact.id < 1);
    
    if (!isDuplicate(contact.cIpAddress, contact.iPort, contact.eContactType, contact.transportType))
    {
        assignContactId(contact);

        CONTACT_ADDRESS* pContactCopy = new CONTACT_ADDRESS(contact);
        mContacts.insertKeyAndValue(new UtlInt(pContactCopy->id), new UtlVoidPtr(pContactCopy));

        // If turn is enabled, duplicate the contact with a relay type
        if (mbTurnEnabled)
        {
            pContactCopy = new CONTACT_ADDRESS(contact);
            pContactCopy->eContactType = RELAY ;
            assignContactId(*pContactCopy) ;
            mContacts.insertKeyAndValue(new UtlInt(pContactCopy->id), new UtlVoidPtr(pContactCopy));
        }
        bRet = true;
    }
    else
    {
        // fill out the information in the contact,
        // to match what is already in the database
        contact = *(find(contact.cIpAddress, contact.iPort, contact.eContactType));
    }
    return bRet;
}

const bool SipContactDb::deleteContact(const CONTACT_ID id)
{
    OsLock lock(mLock);
    UtlInt idKey(id);
    return mContacts.destroy(&idKey);
}

CONTACT_ADDRESS* SipContactDb::find(CONTACT_ID id)
{
    OsLock lock(mLock);
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt idKey(id);
    
    UtlVoidPtr* pValue = (UtlVoidPtr*)mContacts.findValue(&idKey);
    if (pValue)
    {
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
    }
    
    return pContact;
}


// Finds the first contact by a given contact type
CONTACT_ADDRESS* SipContactDb::findByType(CONTACT_TYPE type, OsSocket::SocketProtocolTypes transportType) 
{
    OsLock lock(mLock);
    UtlHashMapIterator iterator(mContacts);
    CONTACT_ADDRESS* pRC = NULL ;

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        assert(pContact) ;
        if (transportType != OsSocket::UNKNOWN && transportType == pContact->transportType)
        {
            if (pContact->eContactType == type)
            {
                pRC = pContact ;
                break ;
            }
        }
    }

    return pRC ;
}

// Find the local contact from a contact id
CONTACT_ADDRESS* SipContactDb::getLocalContact(CONTACT_ID id)
{
    OsLock lock(mLock);

    CONTACT_ADDRESS* pRC = NULL ;
    CONTACT_ADDRESS* pOriginal = find(id) ;
    if (pOriginal)
    {
        if (pOriginal->eContactType == LOCAL)
        {
            pRC = pOriginal ;
        }
        else
        {
            UtlHashMapIterator iterator(mContacts);
            UtlVoidPtr* pValue = NULL;
            CONTACT_ADDRESS* pContact = NULL;
            UtlInt* pKey;
            while (pKey = (UtlInt*)iterator())
            {
                pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
                assert(pValue);
                
                pContact = (CONTACT_ADDRESS*)pValue->getValue();
                assert(pContact) ;
                if ((strcmp(pContact->cInterface, pOriginal->cInterface) == 0) && 
                    (pContact->eContactType == LOCAL))
                {
                    pRC = pContact ;
                    break ;
                }
            }
        }
    }

    return pRC ;
}


CONTACT_ADDRESS* SipContactDb::find(const UtlString ipAddress, const int port, CONTACT_TYPE type)
{
    OsLock lock(mLock);
    bool bFound = false;
    UtlHashMapIterator iterator(mContacts);

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        if (    (pContact->eContactType == type) &&
                (strcmp(pContact->cIpAddress, ipAddress.data()) == 0))
        {
            if (port < 0 || port == pContact->iPort)
            {
                bFound = true;
                break;
            }
        }
    }
    
    if (!bFound)
    {
        pContact = NULL;
    }
        
    return pContact;
}

void SipContactDb::getAll(CONTACT_ADDRESS* contacts[], int& actualNum) const
{

    OsLock lock(mLock);
    UtlHashMapIterator iterator(mContacts);

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    actualNum = 0; // array index
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        contacts[actualNum] = pContact;
        actualNum++;
    }
    
    return;
}
const bool SipContactDb::getRecordForAdapter(CONTACT_ADDRESS& contact,
                                             const char* szAdapter,
                                             const CONTACT_TYPE typeFilter) const
{
    bool bRet = false;

    OsLock lock(mLock);
    UtlHashMapIterator iterator(mContacts);

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        
        if (0 != strcmp(pContact->cInterface, szAdapter))
        {
            continue;
        }
        if (pContact->eContactType != typeFilter)
        {
            continue;
        }

        contact = *pContact;
        bRet = true;
        break;
    }
    return bRet;
}
                                             
void SipContactDb::getAllForAdapter(const CONTACT_ADDRESS* contacts[],
                                    const char* szAdapter,
                                    int& actualNum, 
                                    const CONTACT_TYPE typeFilter) const
{

    OsLock lock(mLock);
    UtlHashMapIterator iterator(mContacts);

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    actualNum = 0; // array index
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        
        if (0 != strcmp(pContact->cInterface, szAdapter))
        {
            continue;
        }
        if (typeFilter != ALL && pContact->eContactType != typeFilter)
        {
            continue;
        }

        contacts[actualNum] = pContact;
        actualNum++;
    }
    
    return;
}


void SipContactDb::enableTurn(bool bEnable) 
{
    OsLock lock(mLock);
    UtlHashMapIterator iterator(mContacts);

    mbTurnEnabled = bEnable ;    

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        if (pContact)
        {
            if (mbTurnEnabled)
            {
                if (pContact->eContactType != RELAY)
                {
                    CONTACT_ADDRESS* pContactCopy = new CONTACT_ADDRESS(*pContact);
                    pContactCopy->eContactType = RELAY ;
                    assignContactId(*pContactCopy) ;
                    mContacts.insertKeyAndValue(new UtlInt(pContactCopy->id), new UtlVoidPtr(pContactCopy));
                }
            }
            else
            {
                if (pContact->eContactType == RELAY)
                {
                    deleteContact(pContact->id) ;
                }
            }
        }        
    }
}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */



/* //////////////////////////// PROTECTED ///////////////////////////////// */


/* //////////////////////////// PRIVATE /////////////////////////////////// */

const bool SipContactDb::isDuplicate(const CONTACT_ID id)
{
    OsLock lock(mLock);
    bool bRet = false;
    UtlInt idKey(id);
    
    UtlVoidPtr* pValue = (UtlVoidPtr*)mContacts.findValue(&idKey);
    if (pValue)
    {
        bRet = true;
    }
    return bRet;
}

const bool SipContactDb::isDuplicate(const UtlString& ipAddress, 
                                     const int port, CONTACT_TYPE type, 
                                     OsSocket::SocketProtocolTypes transportType)
{
    OsLock lock(mLock);
    bool bRet = false;
    UtlHashMapIterator iterator(mContacts);

    UtlVoidPtr* pValue = NULL;
    CONTACT_ADDRESS* pContact = NULL;
    UtlInt* pKey;
    while (pKey = (UtlInt*)iterator())
    {
        pValue = (UtlVoidPtr*)mContacts.findValue(pKey);
        assert(pValue);
        
        pContact = (CONTACT_ADDRESS*)pValue->getValue();
        if (    (pContact->eContactType == type) &&
                (strcmp(pContact->cIpAddress, ipAddress.data()) == 0) &&
                pContact->transportType == transportType)
        {
            if (port < 0 || port == pContact->iPort)
            {
                bRet = true;
                break;
            }
        }
    }
    return bRet;    
}

const bool SipContactDb::assignContactId(CONTACT_ADDRESS& contact)
{
    OsLock lock(mLock);
    
    contact.id = mNextContactId;
    mNextContactId++;
    
    return true;
}

/* ============================ TESTING =================================== */

/* ============================ FUNCTIONS ================================= */
