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
#include <sipXtapiDriver/EnableRportCommand.h>

EnableRportCommand::EnableRportCommand(SIPX_INST hInst)
{
	hInst2 = hInst;
}

int EnableRportCommand::execute(int argc, char* argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;
	if(argc == 2) 
	{
		if(atoi(argv[1]) == 1)
		{
			sipxConfigEnableRport(hInst2, true);
			printf("\nRport Enabled.\n");
		}
		else 
		{
			sipxConfigEnableRport(hInst2, false);
			printf("\nRport Disabled.\n");
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

void EnableRportCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
    usage->append(" <1 = enable, 0 = disable>\n");
}
