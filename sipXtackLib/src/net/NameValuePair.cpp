//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


// SYSTEM INCLUDES

#include <string.h>
#include <ctype.h>
#include <stdio.h>

// APPLICATION INCLUDES
#include "net/NameValuePair.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
int NameValuePair::count = 0;
OsMutex   NameValuePair::mCountLock(OsMutex::Q_PRIORITY);

int getNVCount()
{
        return NameValuePair::count;
}
/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
NameValuePair::NameValuePair(const char* name, const char* value) :
        UtlString(name)
{
   valueString = NULL;
   setValue(value);

#ifdef TEST_ACCOUNT
        mCountLock.acquire();
   count++;
        mCountLock.release();
#endif
}

// Copy constructor
NameValuePair::NameValuePair(const NameValuePair& rNameValuePair) :
UtlString(rNameValuePair)
{
    // Slow copy does implicit const./dest of UtlString
    //((UtlString) *this) = rNameValuePair;

    // Use parent copy constructor
    this->UtlString::operator=(rNameValuePair);

    valueString = NULL;
    setValue(rNameValuePair.valueString);
}

// Destructor
NameValuePair::~NameValuePair()
{
   if(valueString)
   {
                delete[] valueString;
                valueString = 0;
   }

#ifdef TEST_ACCOUNT
        mCountLock.acquire();
   count--;
        mCountLock.release();
#endif
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
NameValuePair&
NameValuePair::operator=(const NameValuePair& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   ((UtlString) *this) = rhs.data();
   setValue(rhs.valueString);

   return *this;
}

/* ============================ ACCESSORS ================================= */
const char* NameValuePair::getValue()
{
        return(valueString);
}

void NameValuePair::setValue(const char* newValue)
{
        if(newValue)
        {
                int len = strlen(newValue);

                if(valueString && len > (int) strlen(valueString))
                {
                        delete[] valueString;
                        valueString = new char[len + 1];
                }
                else
                if (!valueString)
                        valueString = new char[len + 1];

                strcpy(valueString, newValue);
        }
        else if(valueString)
        {
                delete[] valueString;
                valueString = 0;
        }
}

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */
