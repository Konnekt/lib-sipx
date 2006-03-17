//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////

#ifndef _TestMonitor_h_
#define _TestMonitor_h_

#include <cppunit/TestResultCollector.h>

class CppUnit::Test;
class CppUnit::TestFailure;

/**
 * Monitors each test and prints, accumulated test results and various
 * other operations between individual test runs
 */
class TestMonitor : public CppUnit::TestResultCollector
{
 public:

    TestMonitor();

    virtual ~TestMonitor();
  
    /** Overridden */
    virtual void startTest(CppUnit::Test *test);

    /** Overridden */
    virtual void addFailure(const CppUnit::TestFailure &failure);
    
    /** Overriden */
    virtual bool wasSuccessful() const;

    /**
     * Sends waring message to the error stream
     */
    static void warning(const char *message);

 private:

    bool m_verbose;

    bool m_wasSuccessful;
};

#endif
