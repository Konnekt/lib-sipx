//
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
////////////////////////////////////////////////////////////////////////
//////


#include "mp/MpEncoderBase.h"

/* ============================ CREATORS ================================== */

// Constructor
// Returns a new encoder object.
// param: payloadType - (in) RTP payload type associated with this encoder
MpEncoderBase::MpEncoderBase(int payloadType, const MpCodecInfo *pInfo) :
   mpCodecInfo(pInfo),
   mPayloadType(payloadType)
{
}
    
// Destructor
MpEncoderBase::~MpEncoderBase()
{
}

/* ============================ ACCESSORS ================================= */

// Get static information about the encoder
// Returns a pointer to an <i>MpCodecInfo</i> object that provides 
// static information about the encoder.
const MpCodecInfo* MpEncoderBase::getInfo(void) const
{
   return(mpCodecInfo);
}

//:Returns the RTP payload type associated with this encoder.
int MpEncoderBase::getPayloadType(void)
{
   return(mPayloadType);
}
