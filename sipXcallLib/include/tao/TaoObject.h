//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _TaoObject_h_
#define _TaoObject_h_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

// SYSTEM INCLUDES

// APPLICATION INCLUDES
//#include "os/OsConnectionSocket.h"
#include "tao/TaoDefs.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS

// FORWARD DECLARATIONS

//:Base class for PTAPI entities, contains a type name and object pointer for
// locally maintained objects or a PtTaoServerAddress and TaoObjectHandle for
// remotely maintained objects
class TaoObject
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:
/* ============================ CREATORS ================================== */
        TaoObject() {};

        TaoObject(const TaoObject& rTaoObject);
     //:Copy constructor (not implemented for this class)

        virtual ~TaoObject() {};

/* ============================ MANIPULATORS ============================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:
        TaoObjHandle  mTaoObjHandle;    // the handle associated with an object

//      OsConnectionSocket*       mServerConect;

        unsigned char mTaoObjType;              // TaoObjTypes
        unsigned char mTaoObjSubType;   // TaoObjSubTypes

/* //////////////////////////// Private /////////////////////////////////// */
};

#endif // _TaoObject_h_
