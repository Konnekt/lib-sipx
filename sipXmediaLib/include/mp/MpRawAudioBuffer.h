//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _MpRawAudioBuffer_h_
#define _MpRawAudioBuffer_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
* 
*/
class MpRawAudioBuffer {
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

/**
* Default constructor
*/
MpRawAudioBuffer(const char* pFilePath);

/**
* Destructor
*/
~MpRawAudioBuffer();

/* ============================ MANIPULATORS ============================== */

/* ============================ ACCESSORS ================================= */

char* getAudio(char*& prAudio, unsigned long& rLength);

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
char          *mpAudioBuffer;            // Pointer to the raw audio data
unsigned long mAudioBufferLength;        // Length, in bytes, of the audio data
};

#endif  // _MpRawAudioBuffer_h_

