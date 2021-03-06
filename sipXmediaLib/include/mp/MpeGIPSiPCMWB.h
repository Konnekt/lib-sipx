//
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
////////////////////////////////////////////////////////////////////////
//////


#ifdef HAVE_GIPS /* [ */

#ifndef _MpeGIPSiPCMWB_h_
#define _MpeGIPSiPCMWB_h_

// APPLICATION INCLUDES
#include "mp/MpEncoderBase.h"
#include "mp/GIPS/gips_typedefs.h"

//:Derived class for GIPS iPCMWB encoder.
class MpeGIPSiPCMWB: public MpEncoderBase
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

   MpeGIPSiPCMWB(int payloadType);
     //:Constructor
     // Returns a new decoder object.
     //!param: payloadType - (in) RTP payload type associated with this decoder

   virtual ~MpeGIPSiPCMWB(void);
     //:Destructor

   virtual OsStatus initEncode(void);
     //:Initializes a codec data structure for use as an encoder
     //!param: pConnection - (in) Pointer to the MpConnection container
     //!retcode: OS_SUCCESS - Success
     //!retcode: OS_NO_MEMORY - Memory allocation failure

   virtual OsStatus freeEncode(void);
     //:Frees all memory allocated to the encoder by <i>initEncode</i>
     //!retcode: OS_SUCCESS - Success
     //!retcode: OS_DELETED - Object has already been deleted

/* ============================ MANIPULATORS ============================== */

   virtual OsStatus encode(const short* pAudioSamples,
                   const int numSamples,
                   int& rSamplesConsumed,
                   unsigned char* pCodeBuf,
                   const int bytesLeft,
                   int& rSizeInBytes,
                   UtlBoolean& sendNow,
                   MpBufSpeech& rAudioCategory);
     //:Encode audio samples
     // Processes the array of audio samples.  If sufficient samples to encode
     // a frame are now available, the encoded data will be written to the
     // <i>pCodeBuf</i> array.  The number of bytes written to the
     // <i>pCodeBuf</i> array is returned in <i>rSizeInBytes</i>.
     //!param: pAudioSamples - (in) Pointer to array of PCM samples
     //!param: numSamples - (in) number of samples at pAudioSamples
     //!param: rSamplesConsumed - (out) Number of samples encoded
     //!param: pCodeBuf - (out) Pointer to array for encoded data
     //!param: bytesLeft - (in) number of bytes available at pCodeBuf
     //!param: rSizeInBytes - (out) Number of bytes written to the <i>pCodeBuf</i> array
     //!param: sendNow - (out) if true, the packet is complete, send it.
     //!param: rAudioCategory - (out) Audio type (e.g., unknown, silence, comfort noise)
     //!retcode: OS_SUCCESS - Success

/* ============================ ACCESSORS ================================= */

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
   static const MpCodecInfo smCodecInfo;  // static information about the codec
   IPCMWB_inst *pEncoderState;
};

#endif  // _MpeGIPSiPCMWB_h_
#endif /* HAVE_GIPS ] */
