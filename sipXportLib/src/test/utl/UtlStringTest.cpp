//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////

#include <utl/UtlStringTest.h>

const int UtlStringTest::INDEX_NOT_FOUND = -1 ; 
const char* UtlStringTest::longAlphaString = \
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvwzyz"
                   "abcdefghijklmnopqrstuvw" ;
const char* UtlStringTest::splCharString = "Ð墀%$',+*\"?+*a"   ;

const UtlStringTest::BasicStringVerifier UtlStringTest::commonTestSet[] = { \
    {"empty char*", "", 0 }, \
    {"single character char*", "g", 1 }, \
    {"long alpha-num char*", longAlphaString, 257}, \
    {"regular char*", "This makes sense", 16}, \
    {"numeric char*", "12576", 5} , \
    {"special characters char*", splCharString, 16} \
} ; 

const int UtlStringTest::commonTestSetLength  = \
          sizeof(commonTestSet)/sizeof(commonTestSet[0]) ; 

UtlStringTest::UtlStringTest() 
{
}
UtlStringTest::~UtlStringTest() 
{
}
