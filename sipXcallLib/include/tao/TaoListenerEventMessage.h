//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _TaoListenerEventMessage_h_
#define _TaoListenerEventMessage_h_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include "os/OsMsg.h"
#include "ptapi/PtEvent.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//: Message Sent to Tao from the lower layers in notification of events (call backs).
// The lower layer subsystems generate TaoListenerEventMessages which
// are sent to listeners (i.e. tao) registered on the subsystem.
// These messages indicate events which have occurred in the subsystem,
// typically a state change on an object(s).  The state change (new and 
// previous), the object(s) involved are contained in the data members
// of this message.  The actual signature (i.e. usage and meaning of
// the data members) for each of these messages differs for each type
// of event.  It is expected that the TaoServer can generically 
// serialize these messages and send them through the transport layer.
// It is only the TaoEventDispatcher (or its registered listeners) that has  
// to understand the signature, in order to invoke the method on the 
// registered listener (call back).  This seems to hint at an event factory 
// which knows how to construct a specific class of Event based upon the 
// signature of the message.
// <BR><BR>
//
// The TaoListenerEventMessage contains two simple data types
// strings and ints.  It contains 3 of each type, the application of
// which (i.e. signature) is specific to the event.  As a general
// rule of thumb it is recommended that object handle(s) or reference(s)
// are set in the lower numbers and the state change (both new
// and previous value) in the lowest available members.
// <BR><BR>
//
// The eventId is the identifier of the signature for the message.  This 
// can be used to identify which event class is to be constructed and 
// which method on the listener is to be invoked.  The eventId's are
// defined in the enumeration PtEvent::PtEventId.  These event id's
// are needed in tao, ptapi and jtapi/jni.  It seems desirable to
// define these in one place and ptapi seemed the lesser of evils.
// <BR><BR>
//
// Lower level subsystems which support this message for tao should
// implement the following methods:
// <BR>
// addTaoListener(OsServer& rMessageConsumer)
// <BR>
// removeTaoListener(OsServer& rMessageConsumer)
// <BR>
// Instances of this message are queued on the listener's queue
// as the events occur.  It is exdpected that the addTaoListener
// method will be extended in the future to pass some sort of
// mask that the subsystem will use to filter out messages which
// the registering tao listener is not interested in.  Most likely
// the mask will operate based upon the eventId.

class TaoListenerEventMessage : public OsMsg
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

    TaoListenerEventMessage(PtEvent::PtEventId eventId = PtEvent::EVENT_INVALID,
                           int intData1 = 0,
                           int intData2 = 0,
                           int intData3 = 0,
                           const char* stringData1 = NULL, 
                           const char* stringData2 = NULL, 
                           const char* stringData3 = NULL); 
     //:Default constructor

   TaoListenerEventMessage(const TaoListenerEventMessage& rTaoListenerEventMessage);
     //:Copy constructor

   virtual
   ~TaoListenerEventMessage();
     //:Destructor

/* ============================ MANIPULATORS ============================== */

   TaoListenerEventMessage& operator=(const TaoListenerEventMessage& rhs);
     //:Assignment operator

   void setStringData1(const char* stringData);
   void setStringData2(const char* stringData);
   void setStringData3(const char* stringData);

   void setEventId(int id) { mEventId = (PtEvent::PtEventId)id; };
/* ============================ ACCESSORS ================================= */

   int getEventId() { return mEventId; };

   void getStringData1(UtlString& stringData);
   void getStringData2(UtlString& stringData);
   void getStringData3(UtlString& stringData);

   int getIntData1();
   void setIntData1(int intData);
   int getIntData2();
   void setIntData2(int intData);
   int getIntData3();
   void setIntData3(int intData);

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
    PtEvent::PtEventId mEventId;

    UtlString mStringData1;
    UtlString mStringData2;
    UtlString mStringData3;

    int mIntData1;
    int mIntData2;
    int mIntData3;


};

/* ============================ INLINE METHODS ============================ */

#endif  // _TaoListenerEventMessage_h_
