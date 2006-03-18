//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

// The cppunit/TestRunner.h file has to be included twice
//   once before cppunit/TestResult.h for cppunit 1.11
#include <cppunit/TestRunner.h>
#include <cppunit/TestResult.h>
//   once after cppunit/TestResult.h for cppunit 1.10
#include <sipxunit/TestRunner.h>

#include <sipxunit/TestOutputter.h>
#include <sipxunit/TestMonitor.h>

TestRunner::TestRunner()
    : CppUnit::TestRunner()
{
    m_monitor = new TestMonitor();
    m_outputter = new TestOutputter(m_monitor);
    m_result = new CppUnit::TestResult();
    
    m_result->addListener(m_monitor);
}


TestRunner::~TestRunner()
{
    delete m_result;
    delete m_outputter;
    delete m_monitor;
}

bool TestRunner::run()
{
    CppUnit::TestRunner *pthis = this;
    pthis->run(*m_result);
    m_outputter->write();

    return m_monitor->wasSuccessful();
}

