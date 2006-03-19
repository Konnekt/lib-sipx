//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <sipxunit/TestRunner.h>
#include <conio.h>
#include <io.h>
#include <stdio.h>
#include <os/OsSysLog.h>

unsigned int __cdecl sipxLineLookupHandleByURI(const char* szURI) {
	/*placeholder*/
	return 0;
}

/**
 * include this in your project and it will find all Suite's that have been
 * registered with the CppUnit TextFactoryRegistry and run them
 */
int main( int argc, char* argv[] )
{
    TestRunner runner;
	OsSysLog::initialize(0, // do not cache any log messages in memory
                        "sipXtest"); // name for messages from this program
	OsSysLog::enableConsoleOutput(false);
	unlink("sipxtest.log");
	OsSysLog::setOutputFile(0, "sipxtest.log");
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
	OsSysLog::setLoggingPriority(PRI_DEBUG) ;
    runner.addTest(registry.makeTest());
    bool wasSucessful = runner.run();

	printf("Press anuthong to continue...");
	getch();

    return !wasSucessful;
}


