//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
//////////////////////////////////////////////////////////////////////////////
#include <sipXtapiDriver/CommandProcessor.h>
#include <tapi/sipXtapi.h>
#include <sipXtapiDriver/DestroyPublisherCommand.h>

int DestroyPublisherCommand::execute(int argc, char* argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;
	if(argc == 4)
	{
		if(sipxPublisherDestroy(atoi(argv[1]), argv[2], argv[3], sizeof(argv[3]))
			== SIPX_RESULT_SUCCESS)
		{
			printf("Publishing context with ID: %d destroyed\n", atoi(argv[1]));
		}
		else
		{
			printf("Publishing context was unable to be destroyed.\n");
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

void DestroyPublisherCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
	usage->append(" <publisher handle> <the content type being published> <the NOTIFY message's body content>\n");
}
