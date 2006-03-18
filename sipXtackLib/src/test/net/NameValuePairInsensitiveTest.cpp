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
#include <sipxunit/TestUtilities.h>

#include <net/NameValuePairInsensitive.h>

class TestTemplate : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(TestTemplate);
    CPPUNIT_TEST(testAccessors);
    CPPUNIT_TEST_SUITE_END();


public:
    void testAccessors()
    {
        const char* name = "xxx";
        const char* name_different_case = "XXX";
        const char* value = "xxx-value";
        const char* valueRef;

        NameValuePairInsensitive* nv = new NameValuePairInsensitive(name);
        NameValuePairInsensitive* nv_different_case =
           new NameValuePairInsensitive(name_different_case);

        valueRef = nv->getValue();
        CPPUNIT_ASSERT_MESSAGE("value should be null", NULL == valueRef);

        nv->setValue(value);
        valueRef = nv->getValue();
        ASSERT_STR_EQUAL_MESSAGE("incorrect value retrieved", value, valueRef);

        nv->setValue(name);
        valueRef = nv->getValue();
        ASSERT_STR_EQUAL_MESSAGE("incorrect value retrieved", valueRef, name);

        CPPUNIT_ASSERT_MESSAGE("nv should be == nv", nv->compareTo(nv) == 0);
        CPPUNIT_ASSERT_MESSAGE("nv should be == nv_different_case",
                               nv->compareTo(nv_different_case) == 0);

        delete nv;
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestTemplate);
