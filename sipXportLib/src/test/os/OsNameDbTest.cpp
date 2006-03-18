//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCase.h>

#include "utl/UtlString.h"

#include <os/OsNameDb.h>
#include <sipxunit/TestUtilities.h>

class OsNameDbTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(OsNameDbTest);
    CPPUNIT_TEST(testNameDb);
    CPPUNIT_TEST_SUITE_END();


public:
    void testNameDb()
    {
        OsNameDb* pNameDb;
        
        int storedInt;

        pNameDb = OsNameDb::getNameDb();
        /*
         * Because OsNameDb is a singleton, other tests may have already
         * instantiated it and stored things in it.  So this one can't assume
         * that it was initially empty.
         */
        int startingEntries;        
        startingEntries = pNameDb->numEntries();

        CPPUNIT_ASSERT_EQUAL(OS_SUCCESS, pNameDb->insert("test1", 1));
        CPPUNIT_ASSERT(!pNameDb->isEmpty());
        CPPUNIT_ASSERT_EQUAL(startingEntries+1, pNameDb->numEntries());

        CPPUNIT_ASSERT_EQUAL(OS_NAME_IN_USE, pNameDb->insert("test1", 2));
        
        CPPUNIT_ASSERT_EQUAL(OS_SUCCESS, pNameDb->insert("test2", 2));
        CPPUNIT_ASSERT_EQUAL(startingEntries+2, pNameDb->numEntries());

        CPPUNIT_ASSERT_EQUAL(OS_SUCCESS, pNameDb->lookup("test1", NULL));
        CPPUNIT_ASSERT_EQUAL(OS_SUCCESS, pNameDb->lookup("test1", &storedInt));
        CPPUNIT_ASSERT_EQUAL(1, storedInt);
        CPPUNIT_ASSERT_EQUAL(OS_SUCCESS, pNameDb->lookup("test2", &storedInt));
        CPPUNIT_ASSERT_EQUAL(2, storedInt);
        CPPUNIT_ASSERT_EQUAL(OS_NOT_FOUND, pNameDb->lookup("test3", NULL));
        
        pNameDb->remove("test1");
        pNameDb->remove("test2");
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(OsNameDbTest);

