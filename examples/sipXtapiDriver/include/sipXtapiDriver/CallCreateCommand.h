//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
#ifndef _CallCreateCommand_h_
#define _CallCreateCommand_h_

// APPLICATION INCLUDES
#include <sipXtapiDriver/CommandProcessor.h>

class CallCreateCommand : public Command
{
public:
	CallCreateCommand(const SIPX_INST hInst, 
                      SIPX_CALL*  phCall);
	/* ============================ MANIPULATORS ============================== */

	virtual int execute(int argc, char* argv[]);

	/* ============================ ACCESSORS ================================= */

	virtual void getUsage(const char* commandName, UtlString* usage) const;
private:
	SIPX_INST hInstance;
	SIPX_CALL* phCall2;
};
#endif //_CallCreateCommand_h_
