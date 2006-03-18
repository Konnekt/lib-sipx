//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef HAVE_GIPS /* [ */

#include "assert.h"
// APPLICATION INCLUDES
#include "mp/MpeSipxPcma.h"
#include "mp/JB/JB_API.h"

const MpCodecInfo MpeSipxPcma::smCodecInfo(
         SdpCodec::SDP_CODEC_PCMA, JB_API_VERSION, true,
         8000, 8, 1, 160, 64000, 1280, 1280, 1280, 160);

MpeSipxPcma::MpeSipxPcma(int payloadType)
   : MpEncoderBase(payloadType, &smCodecInfo)
{

}

MpeSipxPcma::~MpeSipxPcma()
{
   freeEncode();
}

OsStatus MpeSipxPcma::initEncode(void)
{
   return OS_SUCCESS;
}

OsStatus MpeSipxPcma::freeEncode(void)
{
   return OS_SUCCESS;
}

OsStatus MpeSipxPcma::encode(const short* pAudioSamples,
                             const int numSamples,
                             int& rSamplesConsumed,
                             unsigned char* pCodeBuf,
                             const int bytesLeft,
                             int& rSizeInBytes,
                             UtlBoolean& sendNow,
                             MpBufSpeech& rAudioCategory)
{
   JB_ret res;
   JB_size size;

   res = G711A_Encoder(numSamples, (short*) pAudioSamples, pCodeBuf, &size);
   rSizeInBytes = size;
   rAudioCategory = MP_SPEECH_UNKNOWN;
   sendNow = FALSE;
   rSamplesConsumed = numSamples;

   return OS_SUCCESS;
}
#endif /* HAVE_GIPS ] */
