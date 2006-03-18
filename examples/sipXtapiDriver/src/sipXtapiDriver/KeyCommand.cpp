//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
// APPLICATION INCLUDES
#include <sipXtapiDriver/KeyCommand.h>

int KeyCommand::execute(int argc, char* argv[])
{
	int commandStatus = CommandProcessor::COMMAND_FAILED;
	if(argc == 1) 
	{
		printf("\t\t\t  Shortcut Keys\n");
		printf("al ---------> addline\t\t\taacc ---------> autoaccept\n");
		printf("aans ---------> autoanswer\t\tarej ---------> autoreject\n");
		printf("cacc ---------> callaccept\t\tcans ---------> callanswer\n");
		printf("cco ---------> callconnect\t\tccr --------->callcreate\n");
		printf("cd ---------> calldestroy\t\tch ---------> callhold\n");
		printf("pf ---------> callplayfile\t\tcr ---------> callredirect\n");
		printf("si ---------> callsendinfo\t\tst ---------> callstarttone\n");
		printf("csub ---------> callsubscribe\t\tcu ---------> callunhold\n");
		printf("cfa ---------> conferenceadd\t\tcfcr ---------> conferencecreate\n");
		printf("cfd ---------> conferencedestroy\tcfgc ---------> conferencegetcalls\n");
		printf("cfh ---------> conferencehold\t\tcfj ---------> conferencejoin\n");
		printf("cfu ---------> conferenceunhold\t\tcp ---------> createpublisher\n");
		printf("dp ---------> destroypublisher\t\tup ---------> updatepublisher\n");
		printf("es ---------> enablestun\t\ter ---------> enablerport\n");
		printf("\t\t\thi ---------> history\n");
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

void KeyCommand::getUsage(const char* commandName, UtlString* usage) const
{
	Command::getUsage(commandName, usage);
    usage->append(" No parameters necessary\n");
}