//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCase.h>

#include <os/OsProcess.h>
#include <os/OsStatus.h>
#include <sipxunit/TestUtilities.h>

class OsProcessIteratorTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(OsProcessIteratorTest);
    CPPUNIT_TEST(testIterator);
    CPPUNIT_TEST_SUITE_END();


public:

    /**
     * Just excersizes AIP. Unclear how to create pass/fail tests
     */
    void testIterator()
    {
        OsStatus stat;
        OsProcess process;
        OsProcessIterator pi;

        stat = pi.findFirst(process);
        KNOWN_BUG("Unknown failure", "XPL-12");
        CPPUNIT_ASSERT_MESSAGE("First process", stat == OS_SUCCESS);

        while (stat == OS_SUCCESS)
        {
            UtlString name;
            process.getProcessName(name);
            #ifdef WIN32
            /*on Windows, the system process is pid 0 */
            CPPUNIT_ASSERT_MESSAGE("Valid PID",process.getPID() >= 0);
            #else
            CPPUNIT_ASSERT_MESSAGE("Valid PID", process.getPID() != 0);
            #endif
            CPPUNIT_ASSERT_MESSAGE("Valid Parent PID", process.getParentPID() >= 0);
            CPPUNIT_ASSERT_MESSAGE("Valid process name", name.data() != NULL);
            
            stat = pi.findNext(process);
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(OsProcessIteratorTest);

