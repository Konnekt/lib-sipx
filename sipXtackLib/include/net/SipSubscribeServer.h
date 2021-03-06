// 
// 
// Copyright (C) 2005, 2006 SIPez LLC
// Licensed to SIPfoundry under a Contributor Agreement.
//
// Copyright (C) 2005, 2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// $$
//////////////////////////////////////////////////////////////////////////////
// Author: Dan Petrie (dpetrie AT SIPez DOT com)

#ifndef _SipSubscribeServer_h_
#define _SipSubscribeServer_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include <os/OsServerTask.h>
#include <os/OsDefs.h>
#include <os/OsRWMutex.h>
#include <utl/UtlString.h>
#include <utl/UtlHashMap.h>
#include <net/SipUserAgent.h>


// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// FORWARD DECLARATIONS
class SipSubscribeServerEventHandler;
class SipUserAgent;
class SipPublishContentMgr;
class SipSubscriptionMgr;
class OsMsg;
class SipMessage;


// TYPEDEFS


//! Top level class for accepting and processing SUBSCRIBE requests
/*! This implements a generic RFC 3265 SUBSCRIBE server or sometimes
 *  called a NOTIFIER.  This class receives SUBSCRIBE requests,
 *  retrieves the SIP Event content from the SipPublisherContentMgr
 *  generates a NOTIFY request with the retrieved Event content and send 
 *  the NOFITY using the SipUserAgent.  The SipSubscribeServer is
 *  designed so that it can handle several different event types or
 *  so that you can have multiple instances of the SipSubscribeServer
 *  each handling different event type.  However you can not have an
 *  event type that is handled by more than one SipSubScribeServer.
 *
 *  \par Event Specific Handling and Processing
 *  Event types are enabled with the enableEventType method.  This method
 *  handling and processing of the specified Event type to be specialized
 *  by providing an Event specific: SipEventPlugin, SipUserAgent and/or
 *  SipPublisherContentMgr.
 *
 *  \par Application Use
 *  An application which provides event state for a specific event type
 *  uses the SipPublishContentMgr to provide event state defaults as well
 *  as state specific to a resource.  The application enables the event
 *  type with the enableEventType method, providing the SipPublishContentMgr
 *  which contains the event state content for the event type.  The
 *  SipSubscribeServer provides the content for specific resource 
 *  contained in the SipPublishContentMgr to subscribers.  The SipPublishContentMgr
 *  notifies the SipSubscribeServer (via callback) of content changes made 
 *  by the application.
 *
 *  \par Subscription State
 *  The SipSubscriptionMgr is used by SipSubscribeServer to maintain
 *  the subscription state (SUBSCRIBE dialog state not Event state
 *  content).
 *
 *  \par Overall Data Flow
 *  The SipSubscribeServer needs to address 4 general stimulus:
 *  1) Respond to incoming SUBSCRIBE requests and send the cooresponding
 *     NOTIFY request.
 *  2) Generate NOTIFY requests when the event state changes for a resource
 *     that has an on-expired subscription.
 *  3) Generate NOTIFY requests to subscriptions when they expire.
 *  4) Some notification error responses should cause the subscription to expire
 *
 *  When enabling a SIP event type via the enableEventType method, 
 *  the SipSubscribeServer registers with
 *  the SipUserAgent to receive SUBSCRIBE requests and NOTIFY responses 
 *  for the event type which are processed by the handleMessage method.
 *  Applications that publish event state use the SipPublishContentMgr
 *  to update resource specific or default event states.  The SipSubscribeServer
 *  is notified by the SipPublishContentMgr (via callback) that the
 *  content has changed and sends a NOTIFY to those subscribed to the
 *  resourceId for the event type key.  The SipSubscribeServer uses
 *  timers to keep track of when event subscription expire.  When a timer
 *  fires, a message gets queued on the SipSubscribeServer which is that
 *  passed to handleMessage.
 */
class SipSubscribeServer : public OsServerTask
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:


/* ============================ CREATORS ================================== */

    //! Helper utility to build a basic server with default behavior
    static SipSubscribeServer* buildBasicServer(SipUserAgent& userAgent,
                                                const char* eventType = NULL);

    //! Default Dialog constructor
    SipSubscribeServer(SipUserAgent& defaultUserAgent,
                       SipPublishContentMgr& defaultContentMgr,
                       SipSubscriptionMgr& defaultSubscriptionMgr,
                       SipSubscribeServerEventHandler& defaultPlugin);


    //! Destructor
    virtual
    ~SipSubscribeServer();


/* ============================ MANIPULATORS ============================== */

    //! Callback invoked by SipPublishContentMgr when content changes
    /*! This is used to tell the SipSubscribeServer that a new notify
     *  needs to be sent as the event state content has changed.
     */
    static void contentChangeCallback(void* applicationData,
                                       const char* resourceId,
                                       const char* eventTypeKey,
                                       const char* eventType,
                                       UtlBoolean isDefaultContent);

    //! Send a NOTIFY to all subscribers to resource and event state
    UtlBoolean notifySubscribers(const char* resourceId, 
                                 const char* eventTypeKey,
                                 const char* eventType,
                                 UtlBoolean isDefaultContent);

    //! Tell subscribe server to support given event type
    UtlBoolean enableEventType(const char* eventType,
                                 SipUserAgent* userAgent = NULL,
                                 SipPublishContentMgr* contentMgr = NULL,
                                 SipSubscribeServerEventHandler* eventPlugin = NULL,
                                 SipSubscriptionMgr* subscriptionMgr = NULL);

    //! Tell subscribe server to stop supporting given event type
    UtlBoolean disableEventType(const char* eventType,
                                SipUserAgent*& userAgent,
                                SipPublishContentMgr*& contentMgr,
                                SipSubscribeServerEventHandler*& eventPlugin,
                                SipSubscriptionMgr*& subscriptionMgr);

    //! Handler for SUBSCRIBE requests, NOTIFY responses and timers
    UtlBoolean handleMessage(OsMsg &eventMessage);

/* ============================ ACCESSORS ================================= */

    //! Get the event handler for the given eventType
    /*! WARNING: there is no locking of the event handler once it is
     *  returned.  If the eventHandler is removed via disableEventType
     *  and destroyed, there is no locking protection.  The eventHandler
     *  is only safe to use if the application knows that it is not going
     *  to get the rug pulled out from under it.  Returns the default
     *  event handler if there is not an event specific handler.
     */
    SipSubscribeServerEventHandler* 
        getEventHandler(const UtlString& eventType);

    //! Get the content manager for the given event type
    /*! WARNING: there is no locking of the content manager once it is
     *  returned.  If the content manager is removed via disableEventType
     *  and destroyed, there is no locking protection.  The content manager
     *  is only safe to use if the application knows that it is not going
     *  to get the rug pulled out from under it.  Returns the default
     *  content manager if there is not an event specific content manager.
     */
    SipPublishContentMgr* getPublishMgr(const UtlString& eventType);

    //! Get the subscription manager for the given event type
    /*! WARNING: there is no locking of the subscription manager once it is
     *  returned.  If the subscription manager is removed via disableEventType
     *  and destroyed, there is no locking protection.  The subscription manager
     *  is only safe to use if the application knows that it is not going
     *  to get the rug pulled out from under it.  Returns the default
     *  subscription manager if there is not an event specific subscription 
     *  manager.
     */
    SipSubscriptionMgr* getSubscriptionMgr(const UtlString& eventType);

/* ============================ INQUIRY =================================== */


    //! Inquire if the given event type is enabled in the server
    UtlBoolean isEventTypeEnabled(const UtlString& eventType);

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:


/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
    //! Copy constructor NOT ALLOWED
    SipSubscribeServer(const SipSubscribeServer& rSipSubscribeServer);

    //! Assignment operator NOT ALLOWED
    SipSubscribeServer& operator=(const SipSubscribeServer& rhs);

    //! Handle SUBSCIRBE requests
    UtlBoolean handleSubscribe(const SipMessage& subscribeRequest);

    //! Handle NOTIFY responses
    UtlBoolean handleNotifyResponse(const SipMessage& notifyResponse);

    //! Handle subscription expiration timer events
    UtlBoolean handleExpiration(UtlString* subscribeDialogHandle,
                                OsTimer* timer);

    //! lock for single thread write access (add/remove event handlers)
    void lockForWrite();

    //! unlock for use
    void unlockForWrite();

    //! lock for multiple-thread read access
    void lockForRead();

    //! unlock for use
    void unlockForRead();

    SipUserAgent* mpDefaultUserAgent;
    SipPublishContentMgr* mpDefaultContentMgr;
    SipSubscriptionMgr* mpDefaultSubscriptionMgr;
    SipSubscribeServerEventHandler* mpDefaultEventHandler;
    UtlHashMap mEventDefinitions; 
    OsRWMutex mSubscribeServerMutex;
};

/* ============================ INLINE METHODS ============================ */

#endif  // _SipSubscribeServer_h_
