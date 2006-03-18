//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
#ifndef _AutoRejectCommand_h_
#define _AutoRejectCommand_h_

// APPLICATION INCLUDES
#include <sipXtapiDriver/CommandProcessor.h>
#include <tapi/sipXtapiEvents.h>

class AutoRejectCommand : public Command
{
public:
	//Constructor
	AutoRejectCommand(SIPX_INST hInst, SIPX_CALL* hCall, BOOL* isDestroyed);
	/* ============================ MANIPULATORS ============================== */

	virtual int execute(int argc, char* argv[]);

	/* ============================ ACCESSORS ================================= */

	virtual void getUsage(const char* commandName, UtlString* usage) const;
private:
	SIPX_INST hInstance;
	SIPX_CALL* callHandle;
	BOOL* destroy;
};
#endif //_AutoRejectCommand_h_
