//
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
////////////////////////////////////////////////////////////////////////

#ifndef _MpMediaTaskMsg_h_
#define _MpMediaTaskMsg_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "os/OsMsg.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//:Message object used to communicate with the media processing task
class MpMediaTaskMsg : public OsMsg
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

   // Phone set message types
   enum MpMediaTaskMsgType
   {
      MANAGE,
      SET_FOCUS,
      START,
      STOP,
      UNMANAGE,
      WAIT_FOR_SIGNAL,
      START_SEND_RTP,
      STOP_SEND_RTP,
      START_RECEIVE_RTP,
      STOP_RECEIVE_RTP,
   };

/* ============================ CREATORS ================================== */

   MpMediaTaskMsg(int msg=-1, void* pPtr1=NULL, void* pPtr2=NULL,
                  int int1=-1, int int2=-1);
     //:Constructor

   MpMediaTaskMsg(const MpMediaTaskMsg& rMpMediaTaskMsg);
     //:Copy constructor

   virtual OsMsg* createCopy(void) const;
     //:Create a copy of this msg object (which may be of a derived type)

   virtual
   ~MpMediaTaskMsg();
     //:Destructor

/* ============================ MANIPULATORS ============================== */

   MpMediaTaskMsg& operator=(const MpMediaTaskMsg& rhs);
     //:Assignment operator

   void setPtr1(void* p);
     //:Set pointer 1 (void*) of the media task message

   void setPtr2(void* p);
     //:Set pointer 2 (void*) of the media task message

   void setInt1(int i);
     //:Set integer 1 of the media task message

   void setInt2(int i);
     //:Set integer 2 of the media task message

/* ============================ ACCESSORS ================================= */

   int getMsg(void) const;
     //:Return the type of the media task message

   void* getPtr1(void) const;
     //:Return pointer 1 (void*) of the media task message

   void* getPtr2(void) const;
     //:Return pointer 2 (void*) of the media task message

   int getInt1(void) const;
     //:Return integer 1 of the message

   int getInt2(void) const;
     //:Return integer 2 of the message

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
   void* mpPtr1;       // Message pointer 1
   void* mpPtr2;       // Message pointer 2
   int   mInt1;        // Message integer 1
   int   mInt2;        // Message integer 2

};

/* ============================ INLINE METHODS ============================ */

#endif  // _MpMediaTaskMsg_h_
