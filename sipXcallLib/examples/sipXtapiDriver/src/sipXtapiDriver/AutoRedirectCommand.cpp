//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
#include <sipXtapiDriver/CommandProcessor.h>
#include <tapi/sipXtapi.h>
#include <tapi/sipXtapiEvents.h>
#include <sipXtapiDriver/AutoRedirectCommand.h>
#include <os/OsDefs.h>
#include <net/SipUserAgent.h>

AutoRedirectCommand::AutoRedirectCommand(SIPX_INST hInst, SIPX_CALL hCall, BOOL* isDestroyed)
{
	hInstance = hInst;
	callHandle = hCall;
	destroy = isDestroyed;
}

void AutoRedirectCallbackProc(SIPX_CALL hCall,
                              SIPX_LINE hLine,
							  SIPX_CALLSTATE_MAJOR eMajor,
                              SIPX_CALLSTATE_MINOR eMinor,
						      void* pUser)
{
    char szBuffer[128] ;
    char* szEventDesc = sipxCallEventToString(eMajor, eMinor, szBuffer, sizeof(szBuffer)) ;
	if(eMinor == OFFERING_ACTIVE)
	{
		if(sipxCallRedirect(hCall, ((const char*)pUser)) == SIPX_RESULT_SUCCESS)
		{
			printf("Call with ID %d has been redirected.\n", hCall);
		}
		else
		{
			printf("Call with ID %d failed to be redirected.\n", hCall);
		}
	}
	else if(eMajor == DISCONNECTED)
	{
		sipxCallDestroy(hCall);
	}
}

int AutoRedirectCommand::execute(int argc, char* argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;

	if(argc == 2)
	{
		sipxListenerAdd(hInstance, AutoRedirectCallbackProc, argv[1]);
		while(*destroy == FALSE) {} //while call is still connected
		sipxListenerRemove(hInstance, AutoRedirectCallbackProc, argv[1]);
		
	}
	else
	{
		UtlString usage;
        getUsage(argv[0], &usage);
        printf("%s", usage.data());
        commandStatus = CommandProcessor::COMMAND_BAD_SYNTAX;
	}

	return commandStatus;
}

void AutoRedirectCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
    usage->append(" <url to redirect the call to>\n");
}

