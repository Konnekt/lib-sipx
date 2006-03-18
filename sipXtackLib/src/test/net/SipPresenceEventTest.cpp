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

#include <os/OsDefs.h>
#include <net/SipPresenceEvent.h>


/**
 * Unit test for SipPresenceEvent
 */
class SipPresenceEventTest : public CppUnit::TestCase
{
   CPPUNIT_TEST_SUITE(SipPresenceEventTest);
   CPPUNIT_TEST(testGetPresencePackage);
   CPPUNIT_TEST(testPresencePackageParser);
   CPPUNIT_TEST_SUITE_END();

public:

   void testGetPresencePackage()
      {
         const char *package = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"dong@panther.pingtel.com\">\n"
            "<tuple id=\"1\">\n"
            "<status>\n"
            "<basic>open</basic>\n"
            "</status>\n"
            "<contact>tel:+0123456789</contact>\n"
            "</tuple>\n"
            "</presence>\n"
            ;
       
         SipPresenceEvent presenceEvent("dong@panther.pingtel.com");
         
         UtlString id = "1";
         Tuple* tuple = new Tuple(id);

         tuple->setStatus("open");
         tuple->setContact("tel:+0123456789", 1.0);

         presenceEvent.insertTuple(tuple);

         UtlString bodyString;
         int bodyLength;
       
         presenceEvent.getBytes(&bodyString, &bodyLength);
         //printf("body = \n%s\n", bodyString.data());

         CPPUNIT_ASSERT(strcmp(bodyString.data(), package) == 0);

         int otherLength = presenceEvent.getLength();
         CPPUNIT_ASSERT_EQUAL_MESSAGE("content length is not equal",
                                      bodyLength, otherLength);
      }

   void testPresencePackageParser()
      {
         const char *package = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"dong@panther.pingtel.com\">\n"
            "<tuple id=\"1\">\n"
            "<status>\n"
            "<basic>open</basic>\n"
            "</status>\n"
            "<contact>tel:+0123456789</contact>\n"
            "</tuple>\n"
            "</presence>\n"
            ;
       
         SipPresenceEvent body("dong@panther.pingtel.com", package);

         UtlString bodyString;
         int bodyLength;
       
         body.getBytes(&bodyString, &bodyLength);
         //printf("body = \n%s\n", bodyString.data());
       
         CPPUNIT_ASSERT(strcmp(bodyString.data(), package) == 0);

         int otherLength = body.getLength();
         CPPUNIT_ASSERT_EQUAL_MESSAGE("content length is not equal",
                                      bodyLength, otherLength);
      }
};

CPPUNIT_TEST_SUITE_REGISTRATION(SipPresenceEventTest);
