#pragma once

#include "tapi/sipXtapiInternal.h"

bool validCallData(const SIPX_CALL_DATA* pData, bool bRequireConnection);
SIPXTAPI_API SIPX_RESULT sipxDeinitialize(SIPX_INST& phInst);
