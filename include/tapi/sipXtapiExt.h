#ifndef _sipXtapiExt_h_
#define _sipXtapiExt_h_

#include "sipXtapi.h"

#include <stdstring.h>

SIPXTAPI_API SIPX_RESULT sipxQOSDebug(SIPX_INST phInst, CStdString& txt);
SIPXTAPI_API SIPX_RESULT sipxQOSRating(SIPX_INST phInst, int &rating);


SIPXTAPI_API void sipxStartMedia();

SIPXTAPI_API void sipxStopMedia();



#endif