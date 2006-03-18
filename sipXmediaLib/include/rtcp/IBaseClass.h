//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


//  Border Guard
#ifndef _IBaseClass_h
#define _IBaseClass_h

#include "rtcp/RtcpConfig.h"
#include "BaseClass.h"

/**
 *
 * Class Name:  IBaseClass
 *
 * Inheritance: IBaseClass           - Base Class Implementation
 *
 *
 * Interfaces:  None
 *
 * Description: The IBaseClass Class contains  basic object management services
 *              inherited and used by all object instances within the
 *              Pingtel programming environment.
 *
 * Notes:       The IBaseClass is inherited by all RTCP Report objects.
 *
 */
interface IBaseClass
{

//  Public Methods
public:



/**
 *
 * Method Name:  AddRef
 *
 *
 * Inputs:      None
 *
 * Outputs:     None
 *
 * Returns:     unsigned long
 *
 * Description: Increments the number of references to this object.
 *
 * Usage Notes:
 *
 *
 */
    virtual unsigned long AddRef(void)=0;

/**
 *
 * Method Name:  Release
 *
 *
 * Inputs:      None
 *
 * Outputs:     None
 *
 * Returns:     unsigned long
 *
 * Description: Decrements the number of references to this object.
 *
 * Usage Notes:
 *
 *
 */
    virtual unsigned long Release(void)=0;


};


#endif
