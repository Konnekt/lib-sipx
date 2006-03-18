
#include "os/OsBeginThread.h"

uintptr_t osBeginThreadDefault (const char* name, void * sec, unsigned stack,	unsigned (__stdcall * cb) (void *) 
, void * args, unsigned flag, unsigned * addr) {
	return _beginthreadex(sec, stack, cb, args, flag, addr);
}


OsBeginThreadType osBeginThread = osBeginThreadDefault;
