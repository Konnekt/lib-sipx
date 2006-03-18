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

#include <string.h>
#include <net/Url.h>
#include <net/HttpMessage.h>
#include <net/NetMd5Codec.h>
#include <utl/UtlTokenizer.h>

#define MISSING_PARAM  "---missing---"

#define ASSERT_ARRAY_MESSAGE(message, expected, actual) \
  UrlTest::assertArrayMessage((expected),(actual), \
      CPPUNIT_SOURCELINE(), (message))

class UrlTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(UrlTest);
    CPPUNIT_TEST(testFileBasic);
    CPPUNIT_TEST(testFileWithPortAndPath);
    CPPUNIT_TEST(testHttpBasic);
    CPPUNIT_TEST(testHttpWithPortAndPath);
    CPPUNIT_TEST(testHttpWithQuery);
    CPPUNIT_TEST(testHttpWithQueryNameAddr);
    CPPUNIT_TEST(testHttpWithQueryAddrSpec);
    CPPUNIT_TEST(testSipBasic); 
    CPPUNIT_TEST(testSipBasicWithPort);
    CPPUNIT_TEST(testIpBasicWithBrackets);
    CPPUNIT_TEST(testSemiHeaderParam);
    CPPUNIT_TEST(testSipAdvanced); 
    CPPUNIT_TEST(testSipComplexUser);
    CPPUNIT_TEST(testLongHostname);
    CPPUNIT_TEST(testSipParameters);
    CPPUNIT_TEST(testFullSip);
    CPPUNIT_TEST(testQuotedName);
    CPPUNIT_TEST(testFancyNames);
    CPPUNIT_TEST(testEncoded);
    CPPUNIT_TEST(testNoAngleParam);
    CPPUNIT_TEST(testNoFieldParams);
    CPPUNIT_TEST(testNoHeaderParams);
    CPPUNIT_TEST(testCorrection);
    CPPUNIT_TEST(testIpAddressOnly);
    CPPUNIT_TEST(testMissingAngles);
    CPPUNIT_TEST(testNoAngleParam);
    CPPUNIT_TEST(testHostAddressOnly);
    CPPUNIT_TEST(testHostAndPort);
    CPPUNIT_TEST(testIPv6Host);
    CPPUNIT_TEST(testBogusPort);
    CPPUNIT_TEST(testNoBracketUrlWithAllParamsWithVaryingSpace);
    CPPUNIT_TEST(testConstruction);
    CPPUNIT_TEST(testHttpConstruction);
    CPPUNIT_TEST(testComplexConstruction);
    CPPUNIT_TEST(testAddAttributesToExisting);
    CPPUNIT_TEST(testChangeValues);
    CPPUNIT_TEST(testRemoveAttributes);
    CPPUNIT_TEST(testRemoveUrlParameterCase);
    CPPUNIT_TEST(testRemoveFieldParameterCase);
    CPPUNIT_TEST(testRemoveHeaderParameterCase);
    CPPUNIT_TEST(testRemoveAllTypesOfAttributes);
    CPPUNIT_TEST(testRemoveAngleBrackets);
    CPPUNIT_TEST(testReset);
    CPPUNIT_TEST(testAssignment);
    CPPUNIT_TEST(testGetAllParameters);
    CPPUNIT_TEST(testGetDuplicateNamedParameters);
    CPPUNIT_TEST(testGetOnlyUrlParameters);
    CPPUNIT_TEST(testGetOnlyHeaderParameters);
    CPPUNIT_TEST(testGetOnlyFieldParameters);
    CPPUNIT_TEST(testGetUrlParameterCase);
    CPPUNIT_TEST(testGetFieldParameterCase);
    CPPUNIT_TEST(testGetHeaderParameterCase);
    CPPUNIT_TEST(testIsDigitString);
    CPPUNIT_TEST(testIsUserHostPortEqualExact);
    CPPUNIT_TEST(testIsUserHostPortVaryCapitalization);
    CPPUNIT_TEST(testIsUserHostPortNoPort);
    CPPUNIT_TEST(testIsUserHostPortNoMatch);
    CPPUNIT_TEST(testIsUserHostPortPorts);
    CPPUNIT_TEST(testToString);
    CPPUNIT_TEST(testGetIdentity);
    CPPUNIT_TEST_SUITE_END();

private:

    UtlString *assertValue;

    char msg[1024];

public:

    void setUp()
    {
        assertValue = new UtlString();
    }

    void tearDown()
    {
        delete assertValue;
    }

    void testFileBasic()
    {
        const char* szUrl =  "file://www.sipfoundry.org/dddd/ffff.txt";        
#ifdef _WIN32
        KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif        
        Url url(szUrl);
        sprintf(msg, "simple file url : %s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "www.sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "file", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt", getPath(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());
    }

    void testFileWithPortAndPath()
    {
        const char* szUrl = "file://server:8080/dddd/ffff.txt";

#ifdef _WIN32
        KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif   
        sprintf(msg, "file url w/path and port : %s", szUrl);
        Url url(szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "server", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "file", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt", getPath(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 8080, url.getHostPort());
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, getUri(url));
    }

    void testHttpBasic()
    {
        const char* szUrl =  "http://www.sipfoundry.org";        

        Url url(szUrl);
        sprintf(msg, "simple http url : %s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "www.sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "http", getUrlType(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());
    }

   void testHttpWithPortAndPath()
      {
         const char* szUrl = "http://server:8080/dddd/ffff.txt";
#ifdef _WIN32
         KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif   
         sprintf(msg, "url w/path and port : %s", szUrl);
         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "server", getHostAddress(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "http", getUrlType(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt", getPath(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt", getPath(url,TRUE));
         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 8080, url.getHostPort());
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, getUri(url));
      }

   void testHttpWithQuery()
      {
         const char* szUrl = "http://server:8080/dddd/ffff.txt?p1=v1&p2=v2";
#ifdef _WIN32
         KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif   
         sprintf(msg, "url w/path and port : %s", szUrl);
         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "server", getHostAddress(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "http", getUrlType(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt", getPath(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "/dddd/ffff.txt?p1=v1&p2=v2", getPath(url,TRUE));

         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 8080, url.getHostPort());

         ASSERT_STR_EQUAL_MESSAGE(msg, "v1", getHeaderParam("p1",url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "v2", getHeaderParam("p2",url));

         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, getUri(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
      }

   void testHttpWithQueryNameAddr()
      {
         const char* szUrl = "https://localhost:8091/cgi-bin/voicemail/mediaserver.cgi?action=deposit&mailbox=111&from=%22Dale+Worley%22%3Csip%3A173%40pingtel.com%3E%3Btag%253D3c11304";
#ifdef _WIN32
        KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif   
         Url url(szUrl);
         sprintf(msg, "http url with query (name-addr) : %s", szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));

         const char* szUrl2 = "https://localhost/mediaserver.cgi?foo=bar";

         Url url2(szUrl2);
         sprintf(msg, "http url with query (name-addr) : %s", szUrl2);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl2, toString(url2));
      }

   void testHttpWithQueryAddrSpec()
      {
         const char* szUrl = "https://localhost:8091/cgi-bin/voicemail/mediaserver.cgi?action=deposit&mailbox=111&from=%22Dale+Worley%22%3Csip%3A173%40pingtel.com%3E%3Btag%253D3c11304";
#ifdef _WIN32
         KNOWN_FATAL_BUG("Returned path separator is wrong under Win32", "XSL-74");
#endif   
         Url url(szUrl, TRUE);
         sprintf(msg, "http url with query (addr-spec) : %s", szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));

         const char* szUrl2 = "https://localhost/mediaserver.cgi?foo=bar";

         Url url2(szUrl2, TRUE);
         sprintf(msg, "http url with query (addr-spec) : %s", szUrl2);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl2, toString(url2));
      }


   void testSipBasic()
    {
        const char* szUrl = "sip:rschaaf@10.1.1.89";

        sprintf(msg, "sip url with ip address: %s", szUrl);
        Url url(szUrl); 
        ASSERT_STR_EQUAL_MESSAGE(msg, "10.1.1.89", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, getUri(url));
    }

    void testSipBasicWithPort() 
    {
        const char* szUrl = "sip:fsmith@sipfoundry.org:5555";

        sprintf(msg, "sip url with port: %s", szUrl);
        Url url(szUrl); 
        ASSERT_STR_EQUAL_MESSAGE(msg, "sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 5555, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, getUri(url));
    }

    void testIpBasicWithBrackets()
    {
        char msg[1024] = { 0 };

        const char* szUrl = "<sip:rschaaf@sipfoundry.org>";

        sprintf(msg, "url sip address: %s", szUrl);
        Url url(szUrl); 
        ASSERT_STR_EQUAL_MESSAGE(msg, "sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "rschaaf", getUserId(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, "sip:rschaaf@sipfoundry.org", toString(url));
        url.includeAngleBrackets();

        ASSERT_STR_EQUAL_MESSAGE(msg, "<sip:rschaaf@sipfoundry.org>", toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip:rschaaf@sipfoundry.org", getUri(url));
    }

    void testSipAdvanced()
    {
        const char* szUrl = "Rich Schaaf<sip:sip.tel.sipfoundry.org:8080>" ;

        sprintf(msg, "advanced bracketed sip address: %s", szUrl);
        Url url(szUrl); 

        ASSERT_STR_EQUAL_MESSAGE(msg, "sip.tel.sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "", getUserId(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "Rich Schaaf", getDisplayName(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 8080, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip:sip.tel.sipfoundry.org:8080", getUri(url));
    }

    void testSipComplexUser()
    {
        const char* szUrl = "Raghu Venkataramana<sip:user-tester.my/place?"
            "&yourplace@sipfoundry.org>";

        sprintf(msg, "complex user sip address: %s", szUrl);
        Url url(szUrl);

        ASSERT_STR_EQUAL_MESSAGE(msg, "sipfoundry.org", 
            getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "user-tester.my/place?&yourplace", 
            getUserId(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip:user-tester.my/place?&yourplace@sipfoundry.org",
                                 getUri(url));
    }


    void testLongHostname()
    {
        const char* szUrl =
           "Raghu Venkataramana<sip:125@testing.stage.inhouse.sipfoundry.org>" ; 

        sprintf(msg, "long hostname sip address: %s", szUrl);
        Url url(szUrl);

        ASSERT_STR_EQUAL_MESSAGE(msg, "testing.stage.inhouse.sipfoundry.org", 
            getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "125", getUserId(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip:125@testing.stage.inhouse.sipfoundry.org", getUri(url));
    }

    void testSipParameters()
    {
        const char *szUrl = "<sip:username@10.1.1.225:555;tag=xxxxx;transport=TCP;"
            "msgId=4?call-Id=call2&cseq=2+INVITE>;fieldParam1=1234;fieldParam2=2345";

        const char *szUrlCorrected = "<sip:username@10.1.1.225:555;tag=xxxxx;"
            "transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE>;fieldParam1=1234;"
            "fieldParam2=2345";

        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrlCorrected, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "10.1.1.225", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "username", getUserId(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 555, url.getHostPort());
        ASSERT_STR_EQUAL_MESSAGE(msg, "xxxxx", getParam("tag", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "TCP", getParam("transport", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "4", getParam("msgId", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "call2", getHeaderParam("call-Id", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "2 INVITE", getHeaderParam("cseq", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "1234", getFieldParam("fieldParam1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "2345", getFieldParam("fieldParam2", url));
    }

    void testFullSip()
    {
        const char *szUrl = "Display Name<sip:uname@sipserver:555;"
            "tag=xxxxx;transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE>;"
            "fieldParam1=1234;fieldParam2=2345";
        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, "sipserver", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "uname", getUserId(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "Display Name", getDisplayName(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 555, url.getHostPort());
        ASSERT_STR_EQUAL_MESSAGE(msg, "xxxxx", getParam("tag", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "TCP", getParam("transport", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "4", getParam("msgId", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "call2", getHeaderParam("call-Id", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "2 INVITE", getHeaderParam("cseq", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "1234", getFieldParam("fieldParam1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "2345", getFieldParam("fieldParam2", url));

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(
           msg,
           "sip:uname@sipserver:555;tag=xxxxx;transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE",
           getUri(url)
                                 );
    }

   void testQuotedName()
      {
         const char *szUrl = "\"Display \\\"Name\"<sip:easy@sipserver>";
         Url url(szUrl);
         sprintf(msg, "%s", szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "\"Display \\\"Name\"", getDisplayName(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "easy", getUserId(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "sipserver", getHostAddress(url));
         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, PORT_NONE, url.getHostPort());

         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "sip:easy@sipserver", getUri(url));
      }

   void testFancyNames()
      {
         const char *szUrl = "\"(Display \\\"< @ Name)\"  <sip:?$,;silly/user+(name)_&=.punc%2d!bing*bang~'-@sipserver:555;"
            "tag=xxxxx;transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE>;"
            "fieldParam1=1234;fieldParam2=2345";
         const char *szUrl_corrected = "\"(Display \\\"< @ Name)\"<sip:?$,;silly/user+(name)_&=.punc%2d!bing*bang~'-@sipserver:555;"
            "tag=xxxxx;transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE>;"
            "fieldParam1=1234;fieldParam2=2345";
         Url url(szUrl);
         sprintf(msg, "%s", szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "\"(Display \\\"< @ Name)\"", getDisplayName(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "sip", getUrlType(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "?$,;silly/user+(name)_&=.punc%2d!bing*bang~'-", getUserId(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "sipserver", getHostAddress(url));
         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 555, url.getHostPort());
         ASSERT_STR_EQUAL_MESSAGE(msg, "xxxxx", getParam("tag", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "TCP", getParam("transport", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "4", getParam("msgId", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "call2", getHeaderParam("call-Id", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "2 INVITE", getHeaderParam("cseq", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "1234", getFieldParam("fieldParam1", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "2345", getFieldParam("fieldParam2", url));

         ASSERT_STR_EQUAL_MESSAGE(msg, szUrl_corrected, toString(url));
         ASSERT_STR_EQUAL_MESSAGE(
            msg,
            "sip:?$,;silly/user+(name)_&=.punc%2d!bing*bang~'-@sipserver:555;"
            "tag=xxxxx;transport=TCP;msgId=4?call-Id=call2&cseq=2+INVITE",
            getUri(url));
      }

   void testEncoded()
    {
        const char *szUrl = "D Name<sip:autoattendant@sipfoundry.org:5100;"
            "play=http%3A%2F%2Flocalhost%3A8090%2Fsipx-cgi%2Fmediaserver.cgi"
            "%3Faction%3Dautoattendant?hp1=hval1>;fp1=fval1";
        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, "sipfoundry.org", getHostAddress(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "autoattendant", getUserId(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "autoattendant@sipfoundry.org:5100", 
            getIdentity(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 5100, url.getHostPort());
        ASSERT_STR_EQUAL_MESSAGE(msg, "http://localhost:8090/sipx-cgi/mediaserver.cgi?"
            "action=autoattendant", getParam("play", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "hval1", getHeaderParam("hp1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "fval1", getFieldParam("fp1", url));

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg,
                                 "sip:autoattendant@sipfoundry.org:5100;"
                                 "play=http%3A%2F%2Flocalhost%3A8090%2Fsipx-cgi%2Fmediaserver.cgi"
                                 "%3Faction%3Dautoattendant?hp1=hval1",
                                 getUri(url));
    }

    void testNoFieldParams()
    {
        const char *szUrl = "<sip:tester@sipfoundry.org;up1=uval1;up2=uval2?"
            "hp1=hval1&hp2=hval2>";
        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, "uval1", getParam("up1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "uval2", getParam("up2", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "hval1", getHeaderParam("hp1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "hval2", getHeaderParam("hp2", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getFieldParam("up1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getFieldParam("hp1", url));

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(
           msg,
           "sip:tester@sipfoundry.org;up1=uval1;up2=uval2?hp1=hval1&hp2=hval2",
           getUri(url)
                                 );
    }

    void testNoHeaderParams()
    {
        const char *szUrl = "Display Name<sip:tester@sipfoundry.org;up1=uval1;up2=uval2>"
            ";Fp1=fval1;Fp2=fval2";
        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, "uval1", getParam("up1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "uval2", getParam("up2", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "fval1", getFieldParam("Fp1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "fval2", getFieldParam("Fp2", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getHeaderParam("up1", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getHeaderParam("Fp1", url));

        ASSERT_STR_EQUAL_MESSAGE(msg, szUrl, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg,
                                 "sip:tester@sipfoundry.org;up1=uval1;up2=uval2",
                                 getUri(url));
    }

    void testCorrection()
    {
        const char *szUrl = "Display Name <Sip:tester@sipfoundry.org>";

        const char *szUrlCorrected = "Display Name<sip:tester@sipfoundry.org>";

        Url url(szUrl);
        sprintf(msg, "has space and wrong capitalization in Sip: %s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrlCorrected, toString(url));
    }

   void testSemiHeaderParam()
      {
         const char *withAngles          = "<sip:tester@sipfoundry.org?foo=bar;bing>";
         const char *withoutAngles        = "sip:tester@sipfoundry.org?foo=bar;bing";
         const char *withAnglesEscaped   = "<sip:tester@sipfoundry.org?foo=bar%3Bbing>";
         const char *withoutAnglesEscaped = "sip:tester@sipfoundry.org?foo=bar%3Bbing";

         Url urlHdr(withAngles);
         sprintf(msg, "with angle brackets %s", withAngles);

         ASSERT_STR_EQUAL_MESSAGE(msg, "bar;bing", getHeaderParam("foo", urlHdr));

         ASSERT_STR_EQUAL_MESSAGE(msg, withAnglesEscaped, toString(urlHdr));
         ASSERT_STR_EQUAL_MESSAGE(msg, withoutAnglesEscaped, getUri(urlHdr));

         Url url(withoutAngles, TRUE /* parse as a request uri */ );
         sprintf(msg, "without angle brackets %s", withoutAngles);

         ASSERT_STR_EQUAL_MESSAGE(msg, "bar;bing", getHeaderParam("foo", url));

         ASSERT_STR_EQUAL_MESSAGE(msg, withAnglesEscaped, toString(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, withoutAnglesEscaped, getUri(url));
      }

   void testMissingAngles()
      {
         const char *szUrl = "sip:tester@sipfoundry.org?foo=bar";

         const char *szUrlCorrected = "<sip:tester@sipfoundry.org?foo=bar>";

         Url url(szUrl);
         sprintf(msg, "needed angle brackets %s", szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, szUrlCorrected, toString(url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "bar", getHeaderParam("foo", url));
      }

   void testNoAngleParam()
      {
         const char *szUrl = "sip:tester@sipfoundry.org;foo=bar";

         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getParam("foo", url));
         ASSERT_STR_EQUAL_MESSAGE(msg, "bar", getFieldParam("foo", url));

         Url requrl(szUrl, TRUE);
         ASSERT_STR_EQUAL_MESSAGE(msg, "bar", getParam("foo", requrl));
         ASSERT_STR_EQUAL_MESSAGE(msg, MISSING_PARAM, getFieldParam("foo", requrl));
      }


    void testIpAddressOnly()
    {
        const char *szUrl = "10.1.1.225";
        Url url(szUrl);
        ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:10.1.1.225", toString(url));
    }

   void testHostAddressOnly()
      {
         const char *szUrl = "somewhere.sipfoundry.org";
         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "somewhere.sipfoundry.org", getHostAddress(url));
         ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:somewhere.sipfoundry.org", toString(url));

         const char *szUrl2 = "some-where.sipfoundry.org";
         Url url2(szUrl2);
         ASSERT_STR_EQUAL_MESSAGE(msg, "some-where.sipfoundry.org", getHostAddress(url2));
         ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:some-where.sipfoundry.org", toString(url2));
      }

   void testHostAndPort()
      {
         const char *szUrl = "somewhere.sipfoundry.org:333";
         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "somewhere.sipfoundry.org", getHostAddress(url));
         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 333, url.getHostPort());        

         ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:somewhere.sipfoundry.org:333", toString(url));
      }

    void testBogusPort()
    {
        const char *szUrl = "sip:1234@sipserver:abcd";
        Url url(szUrl);
        ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:1234@sipserver", toString(url));
        ASSERT_STR_EQUAL_MESSAGE(szUrl, "sipserver", getHostAddress(url));
	// The port will be returned as PORT_NONE, because Url::Url()
	// could not parse it.
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, PORT_NONE, url.getHostPort());
    }

   void testIPv6Host()
      {
         const char *szUrl = "[a0:32:44::99]:333";
         Url url(szUrl);
         ASSERT_STR_EQUAL_MESSAGE(msg, "[a0:32:44::99]", getHostAddress(url));
         CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 333, url.getHostPort());        

         ASSERT_STR_EQUAL_MESSAGE(szUrl, "sip:[a0:32:44::99]:333", toString(url));
      }


   void testNoBracketUrlWithAllParamsWithVaryingSpace()
    {
        const char *szUrl = "<sip:fsmith@sipfoundry.org:5555 ? call-id=12345 > ; "
            "msgId=5 ;msgId=6;transport=TCP";
        const char *szUrlCorrected = "<sip:fsmith@sipfoundry.org:5555?call-id=12345>;"
            "msgId=5;msgId=6;transport=TCP";

        Url url(szUrl);
        sprintf(msg, "%s", szUrl);
        ASSERT_STR_EQUAL_MESSAGE(msg, szUrlCorrected, toString(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "sipfoundry.org", getHostAddress(url));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 5555, url.getHostPort());        
        ASSERT_STR_EQUAL_MESSAGE(msg, "fsmith", getUserId(url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "12345", getHeaderParam("call-Id", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "5", getFieldParam("msgId", url));
        ASSERT_STR_EQUAL_MESSAGE(msg, "5", getFieldParam("msgId", url, 0));
        ASSERT_STR_EQUAL_MESSAGE(msg, "6", getFieldParam("msgId", url, 1));
        ASSERT_STR_EQUAL_MESSAGE(msg, "TCP", getFieldParam("transport", url));
    }

    void testConstruction()
    {
        Url url;
        url.setUrlType("sip");
        url.setUserId("someuser") ; 
        url.setHostAddress("main.sip") ; 
        ASSERT_STR_EQUAL("sip:someuser@main.sip", toString(url));
        url.includeAngleBrackets() ; 
        ASSERT_STR_EQUAL("<sip:someuser@main.sip>", toString(url));
    }

    void testHttpConstruction()
    {
        Url url;
        ASSERT_STR_EQUAL("sip:", toString(url));
        url.setUrlType("http") ; 
        url.setHostAddress("web.server") ; 
        url.setPath("/somewhere/in/cyber") ; 
        url.setHostPort(8080) ; 
        ASSERT_STR_EQUAL("http://web.server:8080/somewhere/in/cyber", 
            toString(url));
    }

    void testComplexConstruction()
    {
        Url url;

        // Type should be set to sip by default. Verify that by not setting 
        // anything for the type
        ASSERT_STR_EQUAL("sip:", toString(url));

        url.setUserId("raghu");
        url.setPassword("rgpwd");
        url.setHostAddress("sf.org");
        ASSERT_STR_EQUAL("sip:raghu:rgpwd@sf.org", toString(url));

        url.setUrlParameter("up1", "uval1");
        url.setUrlParameter("up2", "uval2");
        ASSERT_STR_EQUAL("<sip:raghu:rgpwd@sf.org;up1=uval1;up2=uval2>", toString(url));

        url.setHeaderParameter("hp1", "hval1");
        url.setHeaderParameter("hp2", "hval2");
        ASSERT_STR_EQUAL("<sip:raghu:rgpwd@sf.org;up1=uval1;up2=uval2?hp1=hval1&hp2=hval2>",
                         toString(url));

        url.setFieldParameter("fp1", "fval1");
        ASSERT_STR_EQUAL("<sip:raghu:rgpwd@sf.org;"
                         "up1=uval1;up2=uval2?hp1=hval1&hp2=hval2>;fp1=fval1", toString(url));

        url.setDisplayName("Raghu Venkataramana");
        ASSERT_STR_EQUAL("Raghu Venkataramana<sip:raghu:rgpwd@sf.org;"
                         "up1=uval1;up2=uval2?hp1=hval1&hp2=hval2>;fp1=fval1", toString(url));
    }

    void testAddAttributesToExisting()
    {
        Url url("sip:u@host");
        url.setDisplayName("New Name");
        url.setHostPort(5070);
        url.setUrlParameter("u1", "uv1");
        url.setHeaderParameter("h1", "hv1");
        url.setFieldParameter("f1", "fv1");
        ASSERT_STR_EQUAL("New Name<sip:u@host:5070;u1=uv1?h1=hv1>;f1=fv1", 
            toString(url));
    }

    void testChangeValues()
    {
        Url url("New Name<sip:u@host:5070;u1=uv1?h1=hv1>;f1=fv1"); 
        url.setDisplayName("Changed Name");
        url.setHostPort(PORT_NONE);
        url.setHeaderParameter("h1", "hchanged1");
        url.setHeaderParameter("h2", "hnew2");

        // Only the changed attributes should actually changed
        ASSERT_STR_EQUAL("Changed Name<sip:u@host;u1=uv1?"
            "h1=hv1&h1=hchanged1&h2=hnew2>;f1=fv1", toString(url));
    }

    void testRemoveAttributes()
    {
        Url url("Changed Name<sip:u@host;u1=uv1;u2=uv2?"
                "h1=hv1&h1=hchanged1&h2=hnew2>;f1=fv1;f2=fv2");
        url.removeHeaderParameter("h1") ; 
        url.removeFieldParameter("f1") ; 
        url.removeUrlParameter("u1") ; 

        ASSERT_STR_EQUAL("Changed Name<sip:u@host;u2=uv2?h2=hnew2>;f2=fv2", 
            toString(url));
        ASSERT_STR_EQUAL(MISSING_PARAM, getHeaderParam("h1", url));
        ASSERT_STR_EQUAL(MISSING_PARAM, getFieldParam("f1", url));
        ASSERT_STR_EQUAL(MISSING_PARAM, getParam("u1", url));
    }

    // Test that removeUrlParameter is case-insensitive in parameter names.
    void testRemoveUrlParameterCase()
    {
        Url url1("<sip:600-3@cdhcp139.pingtel.com;q=0.8>");
        url1.removeUrlParameter("q");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url1));

        Url url2("<sip:600-3@cdhcp139.pingtel.com;q=0.8;z=q>");
        url2.removeUrlParameter("Q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com;z=q>",
                         toString(url2));

        Url url3("<sip:600-3@cdhcp139.pingtel.com;abcd=27;Q=0.8>");
        url3.removeUrlParameter("q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com;abcd=27>",
                         toString(url3));

        Url url4("<sip:600-3@cdhcp139.pingtel.com;Q=0.8;CXV>");
        url4.removeUrlParameter("Q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com;CXV>",
                         toString(url4));

        Url url5("<sip:600-3@cdhcp139.pingtel.com;mIxEdCaSe=0.8>");
        url5.removeUrlParameter("MiXeDcAsE");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url5));
    }

    // Test that removeFieldParameter is case-insensitive in parameter names.
    void testRemoveFieldParameterCase()
    {
        Url url1("<sip:600-3@cdhcp139.pingtel.com>;q=0.8");
        url1.removeFieldParameter("q");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url1));

        Url url2("<sip:600-3@cdhcp139.pingtel.com>;q=0.8;z=q");
        url2.removeFieldParameter("Q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com>;z=q",
                         toString(url2));

        Url url3("<sip:600-3@cdhcp139.pingtel.com>;abcd=27;Q=0.8");
        url3.removeFieldParameter("q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com>;abcd=27",
                         toString(url3));

        Url url4("<sip:600-3@cdhcp139.pingtel.com>;Q=0.8;CXV");
        url4.removeFieldParameter("Q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com>;CXV",
                         toString(url4));

        Url url5("<sip:600-3@cdhcp139.pingtel.com>;mIxEdCaSe=0.8");
        url5.removeFieldParameter("MiXeDcAsE");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url5));
    }

    // Test that removeHeaderParameter is case-insensitive in parameter names.
    void testRemoveHeaderParameterCase()
    {
        Url url1("<sip:600-3@cdhcp139.pingtel.com&q=0.8>");
        url1.removeHeaderParameter("q");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url1));

        Url url2("<sip:600-3@cdhcp139.pingtel.com?q=0.8&z=q>");
        url2.removeHeaderParameter("Q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com?z=q>",
                         toString(url2));

        Url url3("<sip:600-3@cdhcp139.pingtel.com?abcd=27&Q=0.8>");
        url3.removeHeaderParameter("q");

        ASSERT_STR_EQUAL("<sip:600-3@cdhcp139.pingtel.com?abcd=27>",
                         toString(url3));

        Url url5("<sip:600-3@cdhcp139.pingtel.com?mIxEdCaSe=0.8>");
        url5.removeHeaderParameter("MiXeDcAsE");

        ASSERT_STR_EQUAL("sip:600-3@cdhcp139.pingtel.com",
                         toString(url5));
    }

    void testRemoveAllTypesOfAttributes()
    {
        const char* szUrl = "Changed Name<sip:u@host;u1=uv1;u2=uv2?"
            "h1=hv1&h1=hchanged1&h2=hnew2>;f1=fv1;f2=fv2";

        Url noHeader(szUrl);
        noHeader.removeHeaderParameters();
        ASSERT_STR_EQUAL("Changed Name<sip:u@host;u1=uv1;u2=uv2>;f1=fv1;f2=fv2", 
            toString(noHeader));

        Url noUrl(szUrl);
        noUrl.removeUrlParameters();
        ASSERT_STR_EQUAL("Changed Name<sip:u@host?h1=hv1&h1=hchanged1&h2=hnew2>;f1=fv1;f2=fv2",
            toString(noUrl));

        Url noField(szUrl);
        noField.removeFieldParameters();
        ASSERT_STR_EQUAL("Changed Name<sip:u@host;u1=uv1;u2=uv2?h1=hv1&h1=hchanged1&h2=hnew2>", 
            toString(noField));

        Url noParameters(szUrl);
        noParameters.removeParameters();
        ASSERT_STR_EQUAL("Changed Name<sip:u@host>", 
            toString(noParameters));
    }

    void testRemoveAngleBrackets()
    {
        Url url("<sip:u@host:5070>") ; 
        url.removeAngleBrackets() ; 
        ASSERT_STR_EQUAL("sip:u@host:5070", toString(url));
    }

    void testReset()
    {
        Url url("Changed Name<sip:u@host;u1=uv1;u2=uv2?h1=hv1"
                "&h1=hchanged1&h2=hnew2>;f1=fv1;f2=fv2") ;
        url.reset(); 
        ASSERT_STR_EQUAL("sip:", toString(url));
    }

    void testAssignment()
    {
        const char* szUrl = "Raghu Venkataramana<sip:raghu:rgpwd@sf.org;"
                "up1=uval1;up2=uval2?hp1=hval1&hp2=hval2>;fp1=fval1";
        Url equalsSz = szUrl;
        ASSERT_STR_EQUAL(szUrl, toString(equalsSz));

        Url equalsUrl = Url(szUrl);
        ASSERT_STR_EQUAL(szUrl, toString(equalsUrl));
    }

    void testGetAllParameters()
    {
        UtlString paramNames[16];
        UtlString paramValues[16];
        int paramCount = 0;

        const char* szUrl = "<sip:1234@ss.org;u1=uv1;u2=uv2;u3=uv3?h1=hv1&h2=hv2>;"
            "f1=fv1;f2=fv2;f3=fv3";

        // URL params
        Url url(szUrl);
        sprintf(msg, "Test false when invalid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.getUrlParameters(0, NULL, NULL, paramCount));

        sprintf(msg, "Test true when valid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(szUrl, url.getUrlParameters(3, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, 3, paramCount);

        sprintf(msg, "Test valid arguments %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "u1 u2 u3", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "uv1 uv2 uv3", paramValues);

        // Header params
        sprintf(msg, "valid header param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getHeaderParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 2, paramCount);

        sprintf(msg, "header params  %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "h1 h2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "hv1 hv2", paramValues);

        // Field params
        sprintf(msg, "valid field param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getFieldParameters(3, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 3, paramCount);

        sprintf(msg, "field params  %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "f1 f2 f3", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "fv1 fv2 fv3", paramValues);
    }

    void testGetDuplicateNamedParameters()
    {
        UtlString paramNames[16];
        UtlString paramValues[16];
        int paramCount = 0;

        const char* szUrl = "D Name<sip:abc@server:5050;p1=u1;p2=u2;p1=u3?"
            "p1=h1&p2=h2>;p1=f1;p2=f2";

        // URL params
        Url url(szUrl);
        sprintf(msg, "Test true when valid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(szUrl, url.getUrlParameters(3, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, 3, paramCount);

        sprintf(msg, "Test valid arguments %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "p1 p2 p1", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "u1 u2 u3", paramValues);

        // Header params
        sprintf(msg, "valid header param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getHeaderParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 2, paramCount);

        sprintf(msg, "header params  %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "p1 p2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "h1 h2", paramValues);

        // Field params
        sprintf(msg, "valid field param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getFieldParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 2, paramCount);

        sprintf(msg, "field params  %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "p1 p2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "f1 f2", paramValues);
    }

    void testGetOnlyUrlParameters()
    {
        UtlString paramNames[16];
        UtlString paramValues[16];
        int paramCount = 0;

        const char* szUrl = "D Name<sip:abc@server;up1=u1;up2=u2>";
        // URL params
        Url url(szUrl);
        sprintf(msg, "Test true when valid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(szUrl, url.getUrlParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, 2, paramCount);

        sprintf(msg, "Test valid arguments %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "up1 up2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "u1 u2", paramValues);

        // Header params
        sprintf(msg, "valid header param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.getHeaderParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 0, paramCount);

        // Field params
        sprintf(msg, "valid field param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.getFieldParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 0, paramCount);
    }


    void testGetOnlyHeaderParameters()
    {
        UtlString paramNames[16];
        UtlString paramValues[16];
        int paramCount = 0;

        const char* szUrl = "D Name<sip:abc@server?h1=hv1&h2=hv2>";
        // URL params
        Url url(szUrl);
        sprintf(msg, "Test true when valid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(szUrl, !url.getUrlParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, 0, paramCount);

        // Header params
        sprintf(msg, "valid header param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getHeaderParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 2, paramCount);

        sprintf(msg, "Test valid arguments %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "h1 h2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "hv1 hv2", paramValues);

        // Field params
        sprintf(msg, "valid field param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.getFieldParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 0, paramCount);
    }

    void testGetOnlyFieldParameters()
    {
        UtlString paramNames[16];
        UtlString paramValues[16];
        int paramCount = 0;

        const char* szUrl = "D Name<sip:abc@server>;f1=fv1;f2=fv2";
        // URL params
        Url url(szUrl);
        sprintf(msg, "Test true when valid arguments %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(szUrl, !url.getUrlParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(szUrl, 0, paramCount);

        // Header params
        sprintf(msg, "valid header param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.getHeaderParameters(10, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 0, paramCount);

        // Field params
        sprintf(msg, "valid field param count %s", szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.getFieldParameters(2, paramNames, 
            paramValues, paramCount));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, 2, paramCount);

        sprintf(msg, "Test valid arguments %s", szUrl);
        ASSERT_ARRAY_MESSAGE(msg, "f1 f2", paramNames);
        ASSERT_ARRAY_MESSAGE(msg, "fv1 fv2", paramValues);
    }

    // Test that getUrlParameter is case-insensitive in parameter names.
    void testGetUrlParameterCase()
    {
       UtlString value;

       Url url1("<sip:600-3@cdhcp139.pingtel.com;q=0.8>");

       url1.getUrlParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url2("<sip:600-3@cdhcp139.pingtel.com;q=0.8;z=q>");

       url2.getUrlParameter("Q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url3("<sip:600-3@cdhcp139.pingtel.com;abcd=27;Q=0.8>");

       url3.getUrlParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url4("<sip:600-3@cdhcp139.pingtel.com;Q=0.8;CXV>");

       url4.getUrlParameter("Q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url5("<sip:600-3@cdhcp139.pingtel.com;mIxEdCaSe=0.8>");

       url5.getUrlParameter("MiXeDcAsE", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       // Fetch instances of a paramerter that is present multiple times.
       // (Though multiple presences aren't allowed by RFC 3261 sec. 19.1.1.)

       Url url6("<sip:600-3@cdhcp139.pingtel.com;abcd=27;Q=0.8;Abcd=\"12\";ABCD=EfG>");

       url6.getUrlParameter("abcd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getUrlParameter("abcd", value, 1);
       // getUrlParameter does not de-quote field values!
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getUrlParameter("abcd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getUrlParameter("ABCD", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getUrlParameter("ABCD", value, 1);
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getUrlParameter("ABCD", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getUrlParameter("aBCd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getUrlParameter("aBCd", value, 1);
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getUrlParameter("aBCd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());
    }

    // Test that getFieldParameter is case-insensitive in parameter names.
    void testGetFieldParameterCase()
    {
       UtlString value;

       Url url1("<sip:600-3@cdhcp139.pingtel.com>;q=0.8");

       url1.getFieldParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url2("<sip:600-3@cdhcp139.pingtel.com>;q=0.8;z=q");

       url2.getFieldParameter("Q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url3("<sip:600-3@cdhcp139.pingtel.com>;abcd=27;Q=0.8");

       url3.getFieldParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url4("<sip:600-3@cdhcp139.pingtel.com>;Q=0.8;CXV");

       url4.getFieldParameter("Q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url5("<sip:600-3@cdhcp139.pingtel.com>;mIxEdCaSe=0.8");

       url5.getFieldParameter("MiXeDcAsE", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       // Fetch instances of a paramerter that is present multiple times.
       // (Though multiple presences aren't allowed by RFC 3261 sec. 19.1.1.)

       Url url6("<sip:600-3@cdhcp139.pingtel.com>;abcd=27;Q=0.8;Abcd=\"12\";ABCD=EfG");

       url6.getFieldParameter("abcd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getFieldParameter("abcd", value, 1);
       // getFieldParameter does not de-quote field values!
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getFieldParameter("abcd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getFieldParameter("ABCD", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getFieldParameter("ABCD", value, 1);
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getFieldParameter("ABCD", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getFieldParameter("aBCd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getFieldParameter("aBCd", value, 1);
       ASSERT_STR_EQUAL("\"12\"", value.data());
       url6.getFieldParameter("aBCd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());
    }

    // Test that getHeaderParameter is case-insensitive in parameter names.
    void testGetHeaderParameterCase()
    {
       UtlString value;

       Url url1("<sip:600-3@cdhcp139.pingtel.com?q=0.8>");

       url1.getHeaderParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url2("<sip:600-3@cdhcp139.pingtel.com?q=0.8&z=q>");

       url2.getHeaderParameter("Q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url3("<sip:600-3@cdhcp139.pingtel.com?abcd=27&Q=0.8>");

       url3.getHeaderParameter("q", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       Url url5("<sip:600-3@cdhcp139.pingtel.com?mIxEdCaSe=0.8>");

       url5.getHeaderParameter("MiXeDcAsE", value, 0);
       ASSERT_STR_EQUAL("0.8", value.data());

       // Fetch instances of a header that is present multiple times.

       Url url6("<sip:600-3@cdhcp139.pingtel.com?abcd=27&Q=0.8&Abcd=12&ABCD=EfG>");

       url6.getHeaderParameter("abcd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getHeaderParameter("abcd", value, 1);
       ASSERT_STR_EQUAL("12", value.data());
       url6.getHeaderParameter("abcd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getHeaderParameter("ABCD", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getHeaderParameter("ABCD", value, 1);
       ASSERT_STR_EQUAL("12", value.data());
       url6.getHeaderParameter("ABCD", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());

       url6.getHeaderParameter("aBCd", value, 0);
       ASSERT_STR_EQUAL("27", value.data());
       url6.getHeaderParameter("aBCd", value, 1);
       ASSERT_STR_EQUAL("12", value.data());
       url6.getHeaderParameter("aBCd", value, 2);
       ASSERT_STR_EQUAL("EfG", value.data());
    }

    void testIsDigitString()
    {
        CPPUNIT_ASSERT_MESSAGE("Verify isDigitString for a single digit", 
            Url::isDigitString("1")) ; 

        CPPUNIT_ASSERT_MESSAGE("Verify isDigitString for a long numeric string", 
            Url::isDigitString("1234567890234586")) ; 

        CPPUNIT_ASSERT_MESSAGE("Verify isDigitString returns false for an alpha string", 
            !Url::isDigitString("abcd")) ; 

        CPPUNIT_ASSERT_MESSAGE("Verify isDigitString returns false for alpha then pound key", 
            !Url::isDigitString("1234#")) ; 

        CPPUNIT_ASSERT_MESSAGE("Verify isDigitString for a string that has a star key in it", 
            Url::isDigitString("*6")) ; 
    }

    void testIsUserHostPortEqualExact()
    {        
        const char *szUrl = "Raghu Venkatarmana<sip:raghu@sf.org:5080;blah=b?bl=a>;bdfdf=ere";
        const char *szTest                       = "raghu@sf.org:5080";
        Url url(szUrl);
        
        sprintf(msg, "test=%s, url=%s", szTest, szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.isUserHostPortEqual(szTest));
    }

    void testIsUserHostPortVaryCapitalization()
    {
        const char *szUrl = "R V<sip:raghu@SF.oRg:5080>";
        const char *szTest =        "raghu@sf.org:5080";
        Url url(szUrl);
        
        sprintf(msg, "test=%s, url=%s", szTest, szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.isUserHostPortEqual(szTest));
    }

    void testIsUserHostPortNoPort()
    {
        const char *szUrl = "R V<sip:raghu@sf.org>";
        const char *szTest =        "raghu@sf.org:5060";
        Url url(szUrl);
        
        sprintf(msg, "test=%s, url=%s", szTest, szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, url.isUserHostPortEqual(szTest));
    }

    void testIsUserHostPortNoMatch()
    {
        const char *szUrl = "R V<sip:Raghu@SF.oRg:5080>";
        const char *szTest =        "raghu@sf.org:5080";
        Url url(szUrl);
        
        sprintf(msg, "test=%s, url=%s", szTest, szUrl);
        CPPUNIT_ASSERT_MESSAGE(msg, !url.isUserHostPortEqual(szTest));
    }

    void testIsUserHostPortPorts()
    {
       const char *szUrl = "R V<sip:Raghu@SF.oRg:5080>";
       const char *szTest =        "raghu@sf.org:5080";
       Url url(szUrl);
        
       sprintf(msg, "test=%s, url=%s", szTest, szUrl);
       CPPUNIT_ASSERT_MESSAGE(msg, !url.isUserHostPortEqual(szTest));

       const char* strings[] = {
          "sip:foo@bar",
          "sip:foo@bar:5060",
          "sip:foo@bar:1",
          "sip:foo@bar:100",
          "sip:foo@bar:65535",
       };
       Url urls[sizeof (strings) / sizeof (strings[0])];
       unsigned int i;

       // Set up the Url objects.
       for (i = 0; i < sizeof (strings) / sizeof (strings[0]);
            i++)
       {
          urls[i] = strings[i];
       }

       // Do all the comparisons.
       for (i = 0; i < sizeof (strings) / sizeof (strings[0]);
            i++)
       {
          for (unsigned int j = 0;
               j < sizeof (strings) / sizeof (strings[0]);
               j++)
          {
             int expected = (i == j);
             // Until XSL-96 is fixed, url[0] == url[1].
             if ((i == 0 || i == 1) && (j == 0 || j == 1))
             {
                expected = 1;
             }
             int actual = urls[i].isUserHostPortEqual(urls[j]);
             char msg[80];
             sprintf(msg, "%s == %s", strings[i], strings[j]);
             CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, expected, actual);
          }
       }
    }

    void testToString()
    {
        const char* szUrl = "sip:192.168.1.102" ;
        Url url(szUrl) ;
        UtlString toString("SHOULD_BE_REPLACED");
        url.toString(toString) ;

        // Verify that toString replaces (as opposed to append)
        ASSERT_STR_EQUAL(szUrl, toString.data()) ;
    }

    void testGetIdentity()
    {
       const char* strings[] = {
          "sip:foo@bar",
          "sip:foo@bar:5060",
          "sip:foo@bar:1",
          "sip:foo@bar:100",
          "sip:foo@bar:65535",
       };
       Url urls[sizeof (strings) / sizeof (strings[0])];
       const char* identities[sizeof (strings) / sizeof (strings[0])] = {
          "foo@bar",
          "foo@bar",
          "foo@bar:1",
          "foo@bar:100",
          "foo@bar:65535",
       };

       for (unsigned int i = 0; i < sizeof (strings) / sizeof (strings[0]);
            i++)
       {
          urls[i] = strings[i];
          ASSERT_STR_EQUAL_MESSAGE(strings[i], identities[i],
                                   getIdentity(urls[i]));
       }
    }

    /////////////////////////
    // Helper Methods

    const char *getParam(const char *szName, Url &url)
    {
        UtlString name(szName);        
        if (!url.getUrlParameter(name, *assertValue))
        {
            assertValue->append(MISSING_PARAM);
        }

        return assertValue->data();
    }

    const char *getHeaderParam(const char *szName, Url &url)
    {
        UtlString name(szName);        
        if (!url.getHeaderParameter(name, *assertValue))
        {
            assertValue->append(MISSING_PARAM);
        }

        return assertValue->data();
    }

    const char *getFieldParam(const char *szName, Url &url, int ndx)
    {
        UtlString name(szName);        
        if (!url.getFieldParameter(name, *assertValue, ndx))
        {
            assertValue->append(MISSING_PARAM);
        }

        return assertValue->data();
    }

    const char *getFieldParam(const char *szName, Url &url)
    {
        UtlString name(szName);        
        if (!url.getFieldParameter(name, *assertValue))
        {
            assertValue->append(MISSING_PARAM);
        }

        return assertValue->data();
    }

    const char *toString(const Url& url)
    {
        assertValue->remove(0);
        url.toString(*assertValue);

        return assertValue->data();
    }

    const char *getHostAddress(const Url& url)
    {
        assertValue->remove(0);
        url.getHostAddress(*assertValue);

        return assertValue->data();
    }

    const char *getUrlType(const Url& url)
    {
        assertValue->remove(0);
        url.getUrlType(*assertValue);

        return assertValue->data();
    }

    /** API not declared as const **/
    const char *getUri(Url& url)
    {
        assertValue->remove(0);
        url.getUri(*assertValue);

        return assertValue->data();
    }

    /** API not declared as const **/
    const char *getPath(Url& url, UtlBoolean withQuery = FALSE)
    {
        assertValue->remove(0);
        url.getPath(*assertValue, withQuery);

        return assertValue->data();
    }

    /** API not declared as const **/
    const char *getUserId(Url& url)
    {
        assertValue->remove(0);
        url.getUserId(*assertValue);

        return assertValue->data();
    }

    /** API not declared as const **/
    const char *getIdentity(Url& url)
    {
        assertValue->remove(0);
        url.getIdentity(*assertValue);

        return assertValue->data();
    }

    const char *getDisplayName(const Url &url)
    {
        assertValue->remove(0);
        url.getDisplayName(*assertValue);

        return assertValue->data();
    }

    void assertArrayMessage(const char *expectedTokens, UtlString *actual, 
        CppUnit::SourceLine sourceLine, std::string msg)
    {
        UtlString expected;
        UtlTokenizer toks(expectedTokens);
        for (int i = 0; toks.next(expected, " "); i++)
        {
            TestUtilities::assertEquals(expected.data(), actual[i].data(), 
                sourceLine, msg);
            expected.remove(0);
        }
    }
}; 

CPPUNIT_TEST_SUITE_REGISTRATION(UrlTest);
