//
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
////////////////////////////////////////////////////////////////////////
//////

// SYSTEM INCLUDES
#include <assert.h>
#ifdef __pingtel_on_posix__
#include <unistd.h>
#endif

// APPLICATION INCLUDES
#include "os/OsDefs.h"
#include "os/OsSysLog.h"
#include "os/OsReadLock.h"
#include "os/OsWriteLock.h"
#include "os/OsTask.h"
#include "mp/MpMisc.h"
#include "mp/MpBuf.h"
#include "mp/MprRecorder.h"
#include "os/OsProtectEventMgr.h"

#ifndef ABS
#define ABS(x) (max((x), -(x)))
#endif

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES

// CONSTANTS
#define MIN_SPEECH_ENERGY_THRESHOLD 20000
#define MAX_SPEECH_ENERGY_THRESHOLD 70000

static const int NO_WAIT = 0;

// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
MprRecorder::MprRecorder(const UtlString& rName,
                             int samplesPerFrame, int samplesPerSec)
:  MpResource(rName, 1, 1, 0, 1, samplesPerFrame, samplesPerSec),
   mTermKey(-1),
   mFileDescriptor(-1),
   mTotalBytesWritten(0),
   mTotalSamplesWritten(0),
   mConsecutiveInactive(0),
   mPrevEnergy(MIN_SPEECH_ENERGY_THRESHOLD),
   mpEvent(NULL),
   mFramesToRecord(0),
   mStatus(RECORD_IDLE),
   mEventMutex(OsMutex::Q_FIFO)
{
}

// Destructor
MprRecorder::~MprRecorder()
{
   if (mFileDescriptor != -1)
   {
       // If when we get to the destructor and our file descriptor is not set to -1
       // then close it now.

       if (mRecFormat == WAV_PCM_16)
       {
          updateWaveHeaderLengths(mFileDescriptor);
       }
       close(mFileDescriptor);
       mFileDescriptor = -1;
   }
}

/* ============================ MANIPULATORS ============================== */

OsStatus MprRecorder::setup(int file, RecordFileFormat recFormat, int timeMS, int silenceLength, OsEvent* event)
{
   MpFlowGraphMsg msg(SETUP, this, (void*) event, (void*)silenceLength, file, timeMS);

   mRecFormat = recFormat;
   if (isEnabled()) {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::setup"
         " -- attempt to setup while enabled!!\n");
      return OS_INVALID;
   }
   return postMessage(msg);
}

OsStatus MprRecorder::begin(void)
{
   if (isEnabled()) {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::begin"
         " -- attempt to begin while enabled!!\n");
      return OS_INVALID;
   }
   handleBegin();
   return OS_SUCCESS;
}

OsStatus MprRecorder::stop(void)
{
   MpFlowGraphMsg msg(STOP, this, NULL, NULL, 0, 0);

   if (!isEnabled()) {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::stop"
         " -- attempt to stop while disabled!!\n");
      return OS_INVALID;
   }
   return postMessage(msg);
}

UtlBoolean MprRecorder::enable(void)
{
   if (mFileDescriptor > -1) {
      mStatus = RECORDING;
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::enable\n");
      return MpResource::enable();
   } else {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::enable (No file designated!)\n");
   }
   return FALSE;
}

UtlBoolean MprRecorder::disable(Completion code)
{
   UtlBoolean res = FALSE;

   if (mFileDescriptor > -1)
   {
      OsWriteLock lock(mEventMutex);
      if (mRecFormat == WAV_PCM_16)
      {
         updateWaveHeaderLengths(mFileDescriptor);
      }
   }
   if (mStatus != code)
   {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::disable to report progress mState(%d) code(%d), mFileDescriptor(0x%08x)",
                     mStatus, code, mFileDescriptor);
      progressReport(code);
   }

   if (RECORDING == mStatus) {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::disable -- stopping recorder\n");
      progressReport(RECORD_STOPPED);
   } else {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::disable (not already recording)\n");
   }
   mConsecutiveInactive = 0;

   OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::disable setting mpEvent (0x%08x) to NULL", (int)mpEvent);
   
   {
      OsWriteLock lock(mEventMutex); 
      if (mpEvent != NULL)
      {
         mpEvent = NULL;  // event may be released, do not signal the event any more, 
      }

      if (mFileDescriptor > -1) 
      {
         close(mFileDescriptor);
         mFileDescriptor = -1;
      }
      res = (MpResource::disable() && (mFileDescriptor == -1));
   }

   return res;
}

UtlBoolean MprRecorder::closeRecorder()
{  
  UtlBoolean ret = TRUE;

  OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::closeRecorder entering - mFileDescriptor=%d, mStatus=%d",
         mFileDescriptor, mStatus);
  if (isEnabled()) 
    ret = disable(RECORD_STOPPED);

  OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::closeRecorder leaving - mFileDescriptor=%d, mStatus=%d",
         mFileDescriptor, mStatus);
  return ret;
}

UtlBoolean MprRecorder::termDtmf(int currentToneKey)
{
	UtlBoolean res = FALSE;
	if ((currentToneKey == 11) || (currentToneKey == 1) || (currentToneKey == 0) ||
		(currentToneKey == -1)) 
	{
	 // Only if it's the # or 1 or 0 key, we terminate the recording.
	 // This is a temp solution for Weck.
	 //  -  We may want to make this configurable in the near future.
	 //  -  Or we may want to modify the VXI engine to hanfle grammar
	 //     inside a record field.

		OsSysLog::add(FAC_MP, PRI_INFO, "MprRecorder::termDtmf entering - key=%d, mFileDescriptor=%d, mStatus=%d",
         currentToneKey, mFileDescriptor, mStatus);

		mTermKey = currentToneKey;
		if (mTermKey != -1) 
			res = closeRecorder();
	}

	return res;
}

/* ============================ ACCESSORS ================================= */

void MprRecorder::getRecorderStats(double& nBytes,
                                 double& nSamples, Completion& status)
{
   nBytes = mTotalBytesWritten;
   nSamples = mTotalSamplesWritten;
   status = mStatus;
}

void MprRecorder::getRecorderStats(struct MprRecorderStats* p)
{
   p->mTotalBytesWritten = mTotalBytesWritten;
   p->mTotalSamplesWritten = mTotalSamplesWritten;
   p->mFinalStatus = mStatus;
}

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
UtlBoolean MprRecorder::updateWaveHeaderLengths(int handle)
{
    UtlBoolean retCode = FALSE;

    //find out how many bytes were written so far
    unsigned long length = lseek(handle,0,SEEK_END);
    
    //no go back to beg
    lseek(handle,4,SEEK_SET);
    
    //and update the RIFF length
    unsigned long rifflength = length-8;
    write(handle, (char*)&rifflength,sizeof(length));

    //now seek to the data length
    lseek(handle,40,SEEK_SET);
    
    //this should be the length of just the data
    unsigned long datalength = length-44;
    write(handle, (char*)&datalength,sizeof(datalength));

    return retCode;
}

/* //////////////////////////// PRIVATE /////////////////////////////////// */

UtlBoolean MprRecorder::doProcessFrame(MpBufPtr inBufs[],
                                   MpBufPtr outBufs[],
                                   int inBufsSize,
                                   int outBufsSize,
                                   UtlBoolean isEnabled,
                                   int samplesPerFrame,
                                   int samplesPerSecond)
{
   int numBytes = 0;
   int numSamples = 0;
   MpBufPtr in = NULL;
   Sample* input;

   //try to pass along first input
   if (inBufsSize > 0) 
   {
      in = *inBufs;
   }

   if (numOutputs() > 0) 
   {
      if (inBufsSize > 0) *inBufs = NULL;
      *outBufs = in;
   }

   if (!isEnabled) {
      return TRUE;
   }

   if (mFileDescriptor < 0) 
   {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::doProcessFrame to disable recording because mFileDescriptor=%d, mStatus=%d",
            mFileDescriptor, mStatus);
      disable(RECORD_STOPPED); // just in case...
   }

   if (inBufsSize == 0) {
      // no input buffers, indicate config error
      disable(INVALID_SETUP);
      return TRUE;
   }

   // maximum record time reached or final silence timeout.
   if ((0 >= mFramesToRecord--) || (mSilenceLength <= mConsecutiveInactive)) {
      // Get previous MinVoiceEnergy for debug printouts, and reset it to MIN_SPEECH_ENERGY_THRESHOLD.
      unsigned long prevValue = MpBuf_setMVE(MIN_SPEECH_ENERGY_THRESHOLD); 

      OsSysLog::add(FAC_MP, PRI_INFO,
         "MprRecorder::doProcessFrame to disable recording because"
         " mFramesToRecord=%d, mStatus=%d mSilenceLength=%d,"
         " mConsecutiveInactive=%d, MinVoiceEnergy=%lu", mFramesToRecord,
         mStatus, mSilenceLength, mConsecutiveInactive, prevValue);
      disable(RECORD_FINISHED);
   } else {

      int bytesWritten = 0;

      //now write the buffer out

      if (NULL == in) {
         in = MpBuf_getFgSilence();
      } else {
        MpBuf_addRef(in);
      }

      if (MpBuf_isActiveAudio(in)) {
        mConsecutiveInactive = 0;
      } else {
        mConsecutiveInactive++;
      }

      input = MpBuf_getSamples(in);
      numSamples = MpBuf_getNumSamples(in);
      numBytes = numSamples * sizeof(Sample);
      if (mFileDescriptor > -1)
        bytesWritten = write(mFileDescriptor, (char *)input, numBytes);
   
      if (bytesWritten != numBytes) {
         disable(WRITE_ERROR);
      } else {
         mTotalBytesWritten += numBytes;
         mTotalSamplesWritten += samplesPerFrame;
      }
      MpBuf_delRef(in);
   }
   return TRUE;
}

void MprRecorder::progressReport(Completion code)
{
   int ud;

   mStatus = code;

   {
      OsWriteLock lock(mEventMutex); 

      if (NULL != mpEvent) 
      {
         mpEvent->getUserData(ud);
         OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::progressReport(%d), event=0x%x, &data=0x%X\n",
            code, (int) mpEvent, ud);
         if (0 != ud) 
         {
            MprRecorderStats *rs = (MprRecorderStats*) ud;
      
            //report current stats
            rs->mTotalBytesWritten   = mTotalBytesWritten;
            rs->mTotalSamplesWritten = mTotalSamplesWritten;
            rs->mFinalStatus = code;
            int sps = getSamplesPerSec();
            rs->mDuration = (1000 * mTotalSamplesWritten) / sps;
            rs->mDtmfTerm = mTermKey;
            OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::progressReport mTotalSamplesWritten(%d), sample per second(%d) duration (%d)\n", (int)mTotalSamplesWritten, (int)sps, (int)rs->mDuration);

            //now trigger the event...and pass back our struct
            OsStatus ret;
            if (OS_SUCCESS != (ret = mpEvent->signal(code)))
            {
               OsSysLog::add(FAC_MP, PRI_WARNING, "MprRecorder::progressReport signal failed, returned %d, try again", (int)ret);
               // the event was probably just reset, try again after waiting for 10 ms.
               OsTask::delay(10);
               int userdata;
               mpEvent->getUserData(userdata);
               OsSysLog::add(FAC_MP, PRI_WARNING, "user data - old (0x%08x), new (0x%08x), event (0x%08x) ", 
                              ud, userdata, (int)mpEvent);

// Comment out the assert for production system
//	            assert(userdata == ud);
               if (userdata) 
               {
                  ret = mpEvent->signal(code);
                  OsSysLog::add(FAC_MP, PRI_WARNING, "MprRecorder::progressReport signal again, returned %d ", (int)ret);
               }
            }
         }
         else
         {
               OsSysLog::add(FAC_MP, PRI_WARNING, "MprRecorder::progressReport did not signal user data is 0 for event 0x%08x", (int)mpEvent);
         }
      }
      else
      {
         OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::progressReport(%d) (No event)\n", code);
      }
   }
}

// Handle messages for this resource.
UtlBoolean MprRecorder::handleSetup(int file, int timeMS, int silenceLength, OsProtectedEvent* event)
{
   int iMsPerFrame = (1000 * getSamplesPerFrame()) / getSamplesPerSec();

   if (isEnabled()) {
      OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::handleSetup"
         " -- attempt to setup while enabled!!\n");
      return TRUE;
   }

   if (timeMS > 0) {
      mFramesToRecord = timeMS / iMsPerFrame;
   } else {
      mFramesToRecord = 2000000000;
   }

   if (silenceLength > 0) {
      mSilenceLength = 1000 * silenceLength / iMsPerFrame;
   } else {
      mSilenceLength = 5000 / iMsPerFrame; /*5 seconds */
   }

   unsigned long prevValue = MpBuf_setMVE(MIN_SPEECH_ENERGY_THRESHOLD); // any energy lower than this will be regarded as silence
   OsSysLog::add(FAC_MP, PRI_INFO, "MprRecorder::handleSetup, set MinVoiceEnergy to %d, was %lu\n", MIN_SPEECH_ENERGY_THRESHOLD, prevValue);

   mFileDescriptor = file;

   {
      OsWriteLock lock(mEventMutex); 
      mpEvent = event;   
   }

   mStatus = RECORD_IDLE;
   OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::handleSetup(%d, %d, 0x%X)... #frames=%d\n",
      file, timeMS, (int) event, mFramesToRecord);
   return TRUE;
}

UtlBoolean MprRecorder::handleBegin()
{
   mTotalBytesWritten = 0;
   mTotalSamplesWritten = 0;
   progressReport(RECORDING);
   enable();
   return TRUE;
}

UtlBoolean MprRecorder::handleStop()
{
   disable(RECORD_STOPPED);
   return TRUE;
}

UtlBoolean MprRecorder::handleMessage(MpFlowGraphMsg& rMsg)
{
   OsSysLog::add(FAC_MP, PRI_DEBUG, "MprRecorder::handleMessage(%d)\n", rMsg.getMsg());
   switch (rMsg.getMsg())
   {
   case SETUP:
      {
      //params:  Int1 = file descriptor,
      //         Int2 = time to record in millisecs,
      //         Ptr1 = OsEvent pointer
         int file = rMsg.getInt1();
         int iMSec = rMsg.getInt2();
         OsProtectedEvent* pEvent = (OsProtectedEvent*) rMsg.getPtr1();
         int silenceLength = (int) rMsg.getPtr2();
         return handleSetup(file, iMSec, silenceLength, pEvent);
      }
      break;

   case BEGIN:
      return handleBegin();
      break;

   case STOP:
      return handleStop();
      break;
   }
   return MpResource::handleMessage(rMsg);
}

/* ============================ FUNCTIONS ================================= */

