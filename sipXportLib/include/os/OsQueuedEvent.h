//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


#ifndef _OsQueuedEvent_h_
#define _OsQueuedEvent_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "os/OsDefs.h"
#include "os/OsBSem.h"
#include "os/OsMsgQ.h"
#include "os/OsNotification.h"
#include "os/OsTime.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//:Queued events are used to send event notifications using a message queue
// When the corresponding event occurs, the Notifier sends a message the
// designated message queue. The Listener must wait on the queue to receive
// the event messages.
// <p><b>Background</b>
// <p>First, a little bit of terminology.  The task that wishes to be notified
// when an event occurs is the "Listener" task. The task that signals when
// a given event occurs is the "Notifier" task.  A Notifier informs the
// Listener that a given event has occurred by sending an "Event
// Notification".
//
// <p><b>Expected Usage</b>
// <p>The Listener passes an OsQueuedEvent object to the Notifier which
// includes a message queue identifier for that message queue that will be
// used for event notifications.  When the corresponding event occurs,
// the Notifier sends a message the designated message queue.  The
// Listener waits on the queue to receive the event notification.
// This mechanism allows a task to receive notifications for multiple
// events. The same message queue that is used to receive event
// notifications may also be used to receive other types of messages.

class OsQueuedEvent : public OsNotification
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   OsQueuedEvent(OsMsgQ& rMsgQ, const int userData);
     //:Constructor

   virtual
   ~OsQueuedEvent();
     //:Destructor

/* ============================ MANIPULATORS ============================== */

   virtual OsStatus signal(const int eventData);
     //:Set the event data and send an event message to the designated queue
     // Return the result of the message send operation.

   virtual OsStatus setUserData(int userData);
     //:Set the user data value for this object
     // Always returns OS_SUCCESS.

/* ============================ ACCESSORS ================================= */

   virtual OsStatus getUserData(int& rUserData) const;
     //:Return the user data specified when this object was constructed
     // Always returns OS_SUCCESS.

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

   int     mUserData;      // data specified on behalf of the user and
                           //  not otherwise used by this class -- the user
                           //  data is specified as an argument to the class
                           //  constructor
   OsMsgQ* mpMsgQ;         // message queue where event notifications will
                           //  be sent

   OsStatus doSendEventMsg(const int msgType, const int eventData) const;
     //:Send an event message to the designated message queue
     // Return the result of the message send operation.

   OsQueuedEvent(const OsQueuedEvent& rOsQueuedEvent);
     //:Copy constructor (not implemented for this class)

   OsQueuedEvent& operator=(const OsQueuedEvent& rhs);
     //:Assignment operator (not implemented for this class)

};

/* ============================ INLINE METHODS ============================ */

#endif  // _OsQueuedEvent_h_

