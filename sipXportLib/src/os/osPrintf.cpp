//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <os/OsDefs.h>
#include "utl/UtlString.h"

#ifdef _WIN32
#include <assert.h>
#endif

// EXTERNAL FUNCTIONS

#if defined(_WIN32)
#include <windows.h>

void PrintIt(const char *s)
{
    printf("%s",s);
    OutputDebugString(s);
}

#else
#define PrintIt(x) printf("%s", (char *) (x))
#endif

// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
static int bEnableConsoleOutput = FALSE ;  /**< Should osPrintf print to console? */
/*RL*/
static FILE* fEnableFileOutput = 0;
// FUNCTIONS


// Layer of indirection over printf

extern "C" void enableConsoleOutput(int bEnable) 
{
    bEnableConsoleOutput = bEnable ;
}

extern "C" void enableFileOutput(FILE* file) 
{
    fEnableFileOutput = file ;
}


extern "C" void osPrintf(const char* format, ...)
{
	if (fEnableFileOutput) {
        va_list args;
        va_start(args, format);
		vfprintf(fEnableFileOutput, format, args);
		fflush(fEnableFileOutput);
		va_end(args);
	}
	if (bEnableConsoleOutput)
    {
        va_list args;
        va_start(args, format);

        /* Guess we need no more than 128 bytes. */
        int n, size = 128;
        char *p;

        p = (char*) malloc(size) ;
     
        while (p != NULL)
        {
            /* Try to print in the allocated space. */
#ifdef _WIN32
            n = _vsnprintf (p, size-1, format, args); /*RL*/
#else
            n = vsnprintf (p, size, format, args);
#endif

            /* If that worked, return the string. */
            if (n > -1 && n < size)
            {
				p[n] = 0; /*RL*/
                break;
            }
            /* Else try again with more space. */
            if (n > -1)    /* glibc 2.1 */
                size = n+1; /* precisely what is needed */
            else           /* glibc 2.0 */
                size *= 2;  /* twice the old size */

            if ((p = (char*) realloc (p, size)) == NULL)
            {
                break;
            }
        }

        if (p != NULL)
        {
            PrintIt(p) ;
            free(p) ;
        }

        va_end(args) ;
    }
}

