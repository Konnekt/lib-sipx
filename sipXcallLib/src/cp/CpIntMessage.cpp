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
#include <cp/CpIntMessage.h>
#include <cp/CallManager.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
CpIntMessage::CpIntMessage(unsigned char messageSubtype, int intData) :
OsMsg(OsMsg::PHONE_APP, messageSubtype)
{
        mIntData = intData;
}

// Copy constructor
CpIntMessage::CpIntMessage(const CpIntMessage& rCpIntMessage):
OsMsg(OsMsg::PHONE_APP, rCpIntMessage.getMsgType())
{
        mIntData = rCpIntMessage.mIntData;
}

// Destructor
CpIntMessage::~CpIntMessage()
{

}

OsMsg* CpIntMessage::createCopy(void) const
{
        return(new CpIntMessage(getMsgSubType(), mIntData));
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
CpIntMessage&
CpIntMessage::operator=(const CpIntMessage& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   OsMsg::operator=(rhs);
        mIntData = rhs.mIntData;

   return *this;
}

/* ============================ ACCESSORS ================================= */

void CpIntMessage::getIntData(int& intData) const
{
        intData = mIntData;
}

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */
