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

#include <ptapi/PtAudioCodec.h>

/**
 * Unittest for PtAudioCodec
 */
class PtAudioCodecTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(PtAudioCodecTest);
    CPPUNIT_TEST(testCreators);
    CPPUNIT_TEST(testManipulators);
    CPPUNIT_TEST(testAccessors);
    CPPUNIT_TEST_SUITE_END();


public:
    void testCreators()
    {
        // test the constructor 
        PtAudioCodec *pTempCodec = new PtAudioCodec();
        delete pTempCodec;

        // test the copy constructor 
        pTempCodec = new PtAudioCodec(PtAudioCodec::MU_LAW_8B_8K);
        PtAudioCodec *pTempCodec_1 = new PtAudioCodec(*pTempCodec);
        delete pTempCodec;
        delete pTempCodec_1;
    }

    void testManipulators()
    {
        PtAudioCodec*        pTempCodec;
        PtAudioCodec*        pTempCodec_1;

        // test assignment operator
        pTempCodec = new PtAudioCodec(PtAudioCodec::UNKNOWN_CODEC);
        pTempCodec_1 = new PtAudioCodec(PtAudioCodec::MU_LAW_8B_8K);
        *pTempCodec = *pTempCodec_1;
        delete pTempCodec;
        delete pTempCodec_1;
    }
                                                                                
    void testAccessors()
    {
        PtAudioCodec *pTempAudioCodec = new PtAudioCodec(PtAudioCodec::A_LAW_8B_8K);
        pTempAudioCodec->getRtpCodecType();
        pTempAudioCodec->getRtpEncodingMethod();
        pTempAudioCodec->getSampleSize();
        pTempAudioCodec->getSampleRate();
        pTempAudioCodec->getNumChannels();

        delete pTempAudioCodec;
   }
};

CPPUNIT_TEST_SUITE_REGISTRATION(PtAudioCodecTest);
