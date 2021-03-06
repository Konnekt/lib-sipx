//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


#ifndef _NameValueTokenizer_h_
#define _NameValueTokenizer_h_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include "utl/UtlString.h"

// DEFINES
#define NEWLINE '\n'
#define CARRIAGE_RETURN '\r'
#define CARRIAGE_RETURN_NEWLINE "\r\n"

// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//: Parses name value pairs from multiple lines of text
//
class NameValueTokenizer
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   NameValueTokenizer(const char* multiLineText, int textLength = -1);
     //:Default constructor



   virtual
   ~NameValueTokenizer();
     //:Destructor

/* ============================ MANIPULATORS ============================== */
   static int findNextLineTerminator(const char* text, int length,
                                     int* nextLineIndex);
   //: Finds the index to the next line terminator
   //! param: text - the char array in which to search for the
   // terminator
   //! param: length - the length of the text array
   //! param: nextLineIndex - the index to the begining of the next
   // line.  This may be -1 if the end of the string is encountered
   //! returns: index into the text char array to the line terminator
   // Note: the line terminator may be 1 or 2 characters

   static void frontTrim(UtlString* string, const char* whiteSpace);
   static void backTrim(UtlString* string, const char* whiteSpace);
   static void frontBackTrim(UtlString* string, const char* whiteSpace);

   static UtlBoolean getSubField(const char* textField,
                                 int subfieldIndex,
                                 const char* subfieldSeparator,
                                 UtlString* subfieldText,
                                 int* lastCharIndex = NULL);

   static UtlBoolean getSubField(const char* textField,
                                 int textFieldLength,
                                 int subfieldIndex,
                                 const char* subfieldSeparators,
                                 const char*& subfieldPtr,
                                 int& subFieldLength,
                                 int* lastCharIndex);

   UtlBoolean getNextPair(char separator, UtlString* name, UtlString* value);

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */
    UtlBoolean isAtEnd();
    int getProcessedIndex();

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

    const char* textPtr;
    int textLen;
    int bytesConsumed;

    NameValueTokenizer(const NameValueTokenizer& rNameValueTokenizer);
    //:disable Copy constructor

    NameValueTokenizer& operator=(const NameValueTokenizer& rhs);
    //: disable Assignment operator

};

/* ============================ INLINE METHODS ============================ */

#endif  // _NameValueTokenizer_h_
