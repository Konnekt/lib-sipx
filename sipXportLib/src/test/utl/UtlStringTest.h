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
using namespace std ; 

/** Base class for all the UtlString test unit tests
*/
class UtlStringTest : public  CppUnit::TestCase
{

protected :
    struct BasicStringVerifier
    {
        const char* testDescription ; 
        const char* input ; 
        int length ; 
    };
    
    static const int INDEX_NOT_FOUND ; 
    static const char* longAlphaString  ;
    static const char* splCharString ;
    
    static const int commonTestSetLength ; 
    static const BasicStringVerifier commonTestSet[] ; 

    enum IndexFirstOrContainsType { TEST_INDEX, TEST_FIRST, TEST_CONTAINS} ; 
    enum PrependInsertOrReplace {TEST_PREPEND, TEST_INSERT, TEST_REPLACEFIRST} ; 
    enum AppendInsertReplaceOrPlusEqual{TEST_APPEND, TEST_INSERTLAST, TEST_PLUS, \
                                        TEST_PLUSEQUAL, TEST_REPLACELAST} ;
    enum StringType {TYPE_CHARSTAR, TYPE_UTLSTRING} ;  
    enum CharacterCase {CASE_LOWER, CASE_UPPER} ; 

    
public:

    UtlStringTest() ;

    virtual ~UtlStringTest() ;
} ;

