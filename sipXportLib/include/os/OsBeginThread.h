#ifndef _OsNameDb_h_
#define _OsNameDb_h_

#ifdef _WIN32
#include <process.h>
#endif


typedef uintptr_t (*OsBeginThreadType)(const char*, void *, unsigned,
			unsigned (__stdcall *) (void *), void *, unsigned, unsigned *);

extern OsBeginThreadType osBeginThread;


#endif