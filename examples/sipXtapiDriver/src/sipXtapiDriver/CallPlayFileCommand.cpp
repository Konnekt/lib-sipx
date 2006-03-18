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
#include <sipXtapiDriver/CallPlayFileCommand.h>

int CallPlayFileCommand::execute(int argc, char* argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;
	if(argc == 5)
	{
		if(sipxCallPlayFile(atoi(argv[1]), argv[2], atoi(argv[3]), atoi(argv[4])) 
			== SIPX_RESULT_SUCCESS)
		{
			printf("File with filename: %s is being played.\n", argv[2]);
		}
		else
		{
			printf("File with filename: %s was unable to be played.\n", argv[2]);
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

void CallPlayFileCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
    usage->append(" <Call handle> <Filename> <1 if the audio file is to be rendered locally, 0 otherwise>");
	usage->append(" <1 if the audio file is to be rendered by the remote endpoint, 0 otherwise>\n");
}

