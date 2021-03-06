//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
//////////////////////////////////////////////////////////////////////////////
#ifndef _AutoAnswerCommand_h_
#define _AutoAnswerCommand_h_

// APPLICATION INCLUDES
#include <sipXtapiDriver/CommandProcessor.h>

class AutoAnswerCommand : public Command
{
public:
	//Constructor
	AutoAnswerCommand(SIPX_INST hInst, SIPX_CALL* hCall, BOOL* isDestroyed);
	/* ============================ MANIPULATORS ============================== */

	virtual int execute(int argc, char* argv[]);

	/* ============================ ACCESSORS ================================= */

	virtual void getUsage(const char* commandName, UtlString* usage) const;
	
private:
	SIPX_INST hInstance;
	SIPX_CALL* callHandle;
	BOOL* destroy;
};
#endif //_AutoAnswerCommand_h_
