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
#include <sipXtapiDriver/ConferenceCreateCommand.h>

ConferenceCreateCommand::ConferenceCreateCommand(const SIPX_INST hInst, 
												 SIPX_CONF* phConference)
{
	hInstance = hInst;
	phConf = phConference;
}

int ConferenceCreateCommand::execute(int argc, char *argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;
	if(argc == 1) 
	{
		if(sipxConferenceCreate(hInstance, phConf) == SIPX_RESULT_SUCCESS)
		{
			printf("Conference created with ID: %d \n", *phConf);
		}
		else
		{
			printf("Conference failed to be created.\n");
		}
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

void ConferenceCreateCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
    usage->append(" No parameters necessary.\n");
}
