//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "os/OsDefs.h"
#include "os/OsStatus.h"
#include "mp/MpTypes.h"

void ConvertUnsigned8ToSigned16(unsigned char *in_buffer, Sample *out_buffer, int numBytesToConvert);

//returns the GCD of a and b
//don't pass it negative numbers or (0, 0)
int gcd(int a, int b);

//downsamples from current rate to new rate
//doesn't upsample yet
int reSample(char * charBuffer, int numBytes, int currentSampleRate, int newSampleRate);

//merges two or more channels into one
//takes size in bytes as input.  returns new size in bytes
int mergeChannels(char * charBuffer, int Size, int nTotalChannels);

//works with 16bit wavs only.  (for now)
OsStatus mergeWaveUrls(UtlString rSourceUrls[], UtlString &rDestFile);

//works with 16bit wavs only.  (for now)
OsStatus mergeWaveFiles(UtlString rSourceFiles[], UtlString &rDestFile);

//routines for compressing & decompressing aLaw and uLaw
void InitG711Tables();
size_t DecompressG711MuLaw(Sample *buffer,size_t length);
size_t DecompressG711ALaw(Sample *buffer, size_t length);
unsigned char ALawEncode2(Sample s);
unsigned char MuLawEncode2(Sample s);
Sample MuLawDecode2(unsigned char ulaw);
Sample ALawDecode2(unsigned char alaw);
