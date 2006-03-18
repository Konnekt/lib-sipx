//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _SIPPRESENCEMONITOR_H_
#define _SIPPRESENCEMONITOR_H_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include <os/OsBSem.h>
#include <net/StateChangeNotifier.h>
#include <cp/PresenceDialInServer.h>
#include <net/SipUserAgent.h>
#include <net/SipDialogMgr.h>
#include <net/SipSubscriptionMgr.h>
#include <net/SipSubscribeServerEventHandler.h>
#include <net/SipSubscribeServer.h>
#include <net/SipRefreshManager.h>
#include <net/SipPublishContentMgr.h>
#include <net/SipPresenceEvent.h>
#include <utl/UtlSList.h>
#include <utl/UtlHashMap.h>
#include <cp/CallManager.h>
#include <net/SdpCodecFactory.h>

// DEFINES
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
 * A SipPresenceMonitor is an object that is used for keeping track of the SIP
 * user agents' presence status. All the presence information is stored in a
 * NOTIFIER so that other clients can subscribe the information via
 * SUBSCRIBE/NOTIFY. Furthermore, if a StateChangeNotifier is registered with
 * SipPresenceMonitor, the state change will also be sent out via the
 * StateChangeNotifier.
 *
 */

class SipPresenceMonitor : public StateChangeNotifier
{
  public:

   SipPresenceMonitor(SipUserAgent* userAgent, /**<
                                               * Sip user agent for sending out
                                               * SUBSCRIBEs and receiving NOTIFYs
                                               */
                    UtlString& domainName,    ///< sipX domain name
                    int hostPort,             ///< Host port
                    UtlString& configFile,    ///< Xml formated configuration
                    bool toBePublished);      ///< option to publish for other subscriptions

   virtual ~SipPresenceMonitor();

   /// Add an extension to a group to be monitored
   bool addExtension(UtlString& groupName, Url& contactUrl);

   /// Remove an extension from a group to be monitored
   bool removeExtension(UtlString& groupName, Url& contactUrl); 

   /// Registered a StateChangeNotifier
   void addStateChangeNotifier(const char* fileUrl, StateChangeNotifier* notifier);

   /// Unregistered a StateChangeNotifier
   void removeStateChangeNotifier(const char* fileUrl);

   /// Set the status value
   virtual bool setStatus(const Url& aor, const Status value);

  protected:

   /// Add the contact and presence event to the subscribe list
   bool addPresenceEvent(UtlString& contact, SipPresenceEvent* presenceEvent);

   /// Publish the presence event package to the resource list
   void publishContent(UtlString& contact, SipPresenceEvent* presenceEvent);

   /// Send the state change to the notifier
   void notifyStateChange(UtlString& contact, SipPresenceEvent* presenceEvent);
                                   
  private:

   CallManager* mpCallManager;
   SdpCodecFactory mCodecFactory;
   PresenceDialInServer* mpDialInServer;
   SipUserAgent* mpUserAgent;  
   UtlString mDomainName;
   bool mToBePublished;
   
   OsBSem mLock;
   
   SipDialogMgr mDialogMgr;
   SipSubscriptionMgr* mpSubscriptionMgr;
   SipSubscribeServerEventHandler mPolicyHolder;
   SipPublishContentMgr mSipPublishContentMgr;
   SipSubscribeServer* mpSubscribeServer;

   UtlHashMap mMonitoredLists;
   UtlHashMap mPresenceEventList;
   UtlHashMap mStateChangeNotifiers;   

   /// Disabled copy constructor
   SipPresenceMonitor(const SipPresenceMonitor& rSipPresenceMonitor);

   /// Disabled assignment operator
   SipPresenceMonitor& operator=(const SipPresenceMonitor& rhs);
};

#endif // _SIPPRESENCEMONITOR_H_
