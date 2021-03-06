//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////

// Author: Daniel Petrie
//         dgpetrie AT yahoo DOT com
//////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES

//#define TEST_PRINT

#include <assert.h>

// APPLICATION INCLUDES
#if defined(_WIN32)
#       include "resparse/wnt/nterrno.h"
#elif defined(__pingtel_on_posix__)
#	include <sys/types.h>
#       include <sys/socket.h>
#       include <stdlib.h>
#endif

#include <utl/UtlHashBagIterator.h>
#include <utl/UtlHashMapIterator.h>
#include <net/SipSrvLookup.h>
#include <net/SipUserAgent.h>
#include <net/SipSession.h>
#include <net/SipMessageEvent.h>
#include <net/NameValueTokenizer.h>
#include <net/SipObserverCriteria.h>
#include <os/HostAdapterAddress.h>
#include <net/Url.h>
#ifdef SIP_TLS
#include <net/SipTlsServer.h>
#endif
#include <net/SipTcpServer.h>
#include <net/SipUdpServer.h>
#include <net/SipLineMgr.h>
#include <net/TapiMgr.h>
#include <os/OsDateTime.h>
#include <os/OsEvent.h>
#include <os/OsQueuedEvent.h>
#include <os/OsTimer.h>
#include <os/OsTimerTask.h>
#include <os/OsEventMsg.h>
#include <os/OsRpcMsg.h>
#include <os/OsConfigDb.h>
#include <os/OsRWMutex.h>
#include <os/OsReadLock.h>
#include <os/OsWriteLock.h>
#ifndef _WIN32
// version.h is generated as part of the build by other platforms.  For
// windows, the sip stack version is defined under the project settings.
#include <net/version.h>
#endif
#include <os/OsSysLog.h>
#include <os/OsFS.h>
#include <utl/UtlTokenizer.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS

#define MAXIMUM_SIP_LOG_SIZE 100000
#define SIP_UA_LOG "sipuseragent.log"
#define CONFIG_LOG_DIR SIPX_LOGDIR

#ifndef  VENDOR
# define VENDOR "sipX"
#endif

#ifndef PLATFORM_UA_PARAM
#if defined(_WIN32)
#  define PLATFORM_UA_PARAM " (WinNT)"
#elif defined(_VXWORKS)
#  define PLATFORM_UA_PARAM " (VxWorks)"
#elif defined(__MACH__)
#  define PLATFORM_UA_PARAM " (Darwin)"
#elif defined(__linux__)
#  define PLATFORM_UA_PARAM " (Linux)"
#elif defined(sun)
#  define PLATFORM_UA_PARAM " (Solaris)"
#endif
#endif /* PLATFORM_UA_PARAM */

//#define TEST_PRINT 1
//#define LOG_TIME

// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
SipUserAgent::SipUserAgent(int sipTcpPort,
                           int sipUdpPort,
                           int sipTlsPort,
                           const char* publicAddress,
                           const char* defaultUser,
                           const char* defaultAddress,
                           const char* sipProxyServers,
                           const char* sipDirectoryServers,
                           const char* sipRegistryServers,
                           const char* authenticationScheme,
                           const char* authenticateRealm,
                           OsConfigDb* authenticateDb,
                           OsConfigDb* authorizeUserIds,
                           OsConfigDb* authorizePasswords,
                           const char* natPingUrl,
                           int natPingFrequency,
                           const char* natPingMethod,
                           SipLineMgr* lineMgr,
                           int sipFirstResendTimeout,
                           UtlBoolean defaultToUaTransactions,
                           int readBufferSize,
                           int queueSize,
                           UtlBoolean bUseNextAvailablePort,
                           UtlBoolean doUaMessageChecks
                           ) 
        : SipUserAgentBase(sipTcpPort, sipUdpPort, sipTlsPort, queueSize)
        , mSipTcpServer(NULL)
        , mSipUdpServer(NULL)
#ifdef SIP_TLS
        , mSipTlsServer(NULL)
#endif
        , mMessageLogRMutex(OsRWMutex::Q_FIFO)
        , mMessageLogWMutex(OsRWMutex::Q_FIFO)
        , mpLineMgr(NULL)
        , mbUseRport(FALSE)
        , mbIncludePlatformInUserAgentName(TRUE)
        , mDoUaMessageChecks(doUaMessageChecks)
        , mbShuttingDown(FALSE)
        , mbShutdownDone(FALSE)
		, watchHeadersMutex(OsRWMutex::Q_FIFO) /*RL*/
{
   OsSysLog::add(FAC_SIP, PRI_DEBUG,
                 "SipUserAgent::_ sipTcpPort = %d, sipUdpPort = %d, sipTlsPort = %d",
                 sipTcpPort, sipUdpPort, sipTlsPort);
                 
    // Get pointer to line manager
    mpLineMgr = lineMgr;

   // Create and start the SIP TLS, TCP and UDP Servers
#ifdef SIP_TLS
    if (mTlsPort != PORT_NONE)
    {
        mSipTlsServer = new SipTlsServer(mTlsPort, this, bUseNextAvailablePort);
        mSipTlsServer->startListener();
        mTlsPort = mSipTlsServer->getServerPort() ;
    }
#endif
    if (mTcpPort != PORT_NONE)
    {
        mSipTcpServer = new SipTcpServer(mTcpPort, this, SIP_TRANSPORT_TCP, 
                "SipTcpServer-%d", bUseNextAvailablePort, defaultAddress);
        mSipTcpServer->startListener();
        mTcpPort = mSipTcpServer->getServerPort() ;
    }

    if (mUdpPort != PORT_NONE)
    {
        mSipUdpServer = new SipUdpServer(mUdpPort, this,
                natPingUrl, natPingFrequency, natPingMethod,
                readBufferSize, bUseNextAvailablePort, defaultAddress );
        mSipUdpServer->startListener();
        mUdpPort = mSipUdpServer->getServerPort() ;
    }

    mMaxMessageLogSize = MAXIMUM_SIP_LOG_SIZE;
    mMaxForwards = SIP_DEFAULT_MAX_FORWARDS;

    // TCP sockets not used for an hour are garbage collected
    mMaxTcpSocketIdleTime = 3600;

    // INVITE transactions need to stick around for a minimum of
    // 3 minutes
    mMinInviteTransactionTimeout = 180;

    mForkingEnabled = TRUE;
    mRecurseOnlyOne300Contact = FALSE;

    // By default copy all of the Vias from incoming requests that have
    // a max-forwards == 0
    mReturnViasForMaxForwards = TRUE;

    mMaxSrvRecords = 4;
    mDnsSrvTimeout = 4; // seconds

#ifdef TEST_PRINT
    // Default the log on
    startMessageLog();
#else
    // Disable the message log
    stopMessageLog();
#endif

    // Authentication
    if(authenticationScheme)
    {
        mAuthenticationScheme.append(authenticationScheme);
        HttpMessage::cannonizeToken(mAuthenticationScheme);
        // Do not require authentication if not BASIC or DIGEST
        if(   0 != mAuthenticationScheme.compareTo(HTTP_BASIC_AUTHENTICATION,
                                                   UtlString::ignoreCase
                                                   )
           && 0 !=mAuthenticationScheme.compareTo(HTTP_DIGEST_AUTHENTICATION,
                                                  UtlString::ignoreCase
                                                  )
           )
        {
            mAuthenticationScheme.remove(0);
        }

    }
    if(authenticateRealm)
    {
        mAuthenticationRealm.append(authenticateRealm);
    }

    if(authenticateDb)
    {
        mpAuthenticationDb = authenticateDb;
    }
    else
    {
        mpAuthenticationDb = new OsConfigDb();
    }

    if(authorizeUserIds)
    {
        mpAuthorizationUserIds = authorizeUserIds;
    }
    else
    {
        mpAuthorizationUserIds = new OsConfigDb();
    }

    if(authorizePasswords)
    {
        mpAuthorizationPasswords = authorizePasswords;
    }
    else
    {
        mpAuthorizationPasswords = new OsConfigDb();
    }

    // SIP Server info
    if(sipProxyServers)
    {
        proxyServers.append(sipProxyServers);
    }
    if(sipDirectoryServers)
    {
        directoryServers.append(sipDirectoryServers);
    }
    if(defaultUser)
    {
        defaultSipUser.append(defaultUser);
        NameValueTokenizer::frontBackTrim(&defaultSipUser, " \t\n\r");
    }

    if (!defaultAddress || strcmp(defaultAddress, "0.0.0.0") == 0)
    {
        // get the first local address and
        // make it the default address
        const HostAdapterAddress* addresses[MAX_IP_ADDRESSES];
        int numAddresses = 0;
        memset(addresses, 0, sizeof(addresses));
        getAllLocalHostIps(addresses, numAddresses);
        if (numAddresses > 0)
        {
           // Bind to the first address in the list.
           defaultSipAddress = (char*)addresses[0]->mAddress.data();
           // Now free up the list.
           for (int i = 0; i < numAddresses; i++)
           {
              delete addresses[i];       
           }
        }
        else
        {
           OsSysLog::add(FAC_SIP, PRI_WARNING, "SipUserAgent::_ no IP addresses found.");
        }
    }
    else
    {
        defaultSipAddress.append(defaultAddress);
    }
    if(sipRegistryServers)
    {
        registryServers.append(sipRegistryServers);
    }

    if(publicAddress && *publicAddress)
    {
        sipIpAddress.append(publicAddress);
        mConfigPublicAddress = publicAddress ;
        
        // make a config CONTACT entry
        char szAdapter[256];
        CONTACT_ADDRESS contact;
        contact.eContactType = CONFIG;
        strcpy(contact.cIpAddress, publicAddress);

        getContactAdapterName(szAdapter, defaultAddress);

        strcpy(contact.cInterface, szAdapter);
        contact.iPort = mUdpPort; // what about the tcp port?
        mContactDb.addContact(contact);
    }
    else
    {
        OsSocket::getHostIp(&sipIpAddress);
    }

    mSipPort = PORT_NONE;

    UtlString hostIpAddress(sipIpAddress.data());

    //Timers
    if ( sipFirstResendTimeout <= 0)
    {
        mFirstResendTimeoutMs = SIP_DEFAULT_RTT;
    }
    else if ( sipFirstResendTimeout > 0  && sipFirstResendTimeout < 100)
    {
        mFirstResendTimeoutMs = SIP_MINIMUM_RTT;
    }
    else
    {
        mFirstResendTimeoutMs = sipFirstResendTimeout;
    }
    mLastResendTimeoutMs = 8 * mFirstResendTimeoutMs;
    mReliableTransportTimeoutMs = 2 * mLastResendTimeoutMs;
    mTransactionStateTimeoutMs = 10 * mLastResendTimeoutMs;

    // How long before we expire transactions by default
    mDefaultExpiresSeconds = 180; // mTransactionStateTimeoutMs / 1000;
    mDefaultSerialExpiresSeconds = 20;

    if(portIsValid(mUdpPort))
    {
        SipMessage::buildSipUrl(&mContactAddress,
                hostIpAddress.data(),
                (mUdpPort == SIP_PORT) ? PORT_NONE : mUdpPort,
                (mUdpPort == mTcpPort) ? "" : SIP_TRANSPORT_UDP,
                defaultSipUser.data());

#ifdef TEST_PRINT
        osPrintf("UDP default contact: \"%s\"\n", mContactAddress.data());
#endif

    }

    if(portIsValid(mTcpPort) && mTcpPort != mUdpPort)
    {
        SipMessage::buildSipUrl(&mContactAddress, hostIpAddress.data(),
                (mTcpPort == SIP_PORT) ? PORT_NONE : mUdpPort,
                SIP_TRANSPORT_TCP, defaultSipUser.data());
#ifdef TEST_PRINT
        osPrintf("TCP default contact: \"%s\"\n", mContactAddress.data());
#endif
    }

    // Initialize the transaction id seed
    SipTransaction::smBranchIdBase = mContactAddress;

    mPingLock = FALSE;
    if(natPingUrl && *natPingUrl && natPingFrequency > 0)
    {
        mPingLock = TRUE;
        mSipUdpServer->start();
        mNatPingUrl = natPingUrl;
        mNatPingPeriod = natPingFrequency;
        mNatPingMethod = natPingMethod ? natPingMethod : "";
        OsSysLog::add(FAC_SIP, PRI_DEBUG, "UDP Server started URL: %s frequency: %d method: %s",
                natPingUrl, natPingFrequency, natPingMethod ? natPingMethod : "");
    }

    // Allow the default SIP methods
    allowMethod(SIP_INVITE_METHOD);
    allowMethod(SIP_ACK_METHOD);
    allowMethod(SIP_CANCEL_METHOD);
    allowMethod(SIP_BYE_METHOD);
    allowMethod(SIP_REFER_METHOD);
    allowMethod(SIP_OPTIONS_METHOD);

    defaultUserAgentName.append( VENDOR "/" SIP_STACK_VERSION);

    OsMsgQ* incomingQ = getMessageQueue();
    mpTimer = new OsTimer(incomingQ, 0);
    // Convert from mSeconds to uSeconds
    OsTime lapseTime(0, mTransactionStateTimeoutMs * 1000);
    mpTimer->periodicEvery(lapseTime, lapseTime);

    OsTime time;
    OsDateTime::getCurTimeSinceBoot(time);
    mLastCleanUpTime = time.seconds();

    mIsUaTransactionByDefault = defaultToUaTransactions;
}

// Copy constructor
SipUserAgent::SipUserAgent(const SipUserAgent& rSipUserAgent) :
        mMessageLogRMutex(OsRWMutex::Q_FIFO),
        mMessageLogWMutex(OsRWMutex::Q_FIFO),
		watchHeadersMutex(OsRWMutex::Q_FIFO) /*RL*/
{
}

// Destructor
SipUserAgent::~SipUserAgent()
{
    mPingLock = TRUE;
    mpTimer->stop();
    delete mpTimer;
    mpTimer = NULL;

    // Wait until this OsServerTask has stopped or handleMethod
    // might access something we are about to delete here.
    waitUntilShutDown();

    if(mSipTcpServer)
    {
       //mSipTcpServer->shutdownListener();
       mSipTcpServer->requestShutdown();
       delete mSipTcpServer;
       mSipTcpServer = NULL;
    }
#ifdef SIP_TLS
    if(mSipTlsServer)
    {
       //mSipTlsServer->shutdownListener();
       mSipTlsServer->requestShutdown();
       delete mSipTlsServer;
       mSipTlsServer = NULL;
    }
#endif
    if(mSipUdpServer)
    {
       mSipUdpServer->shutdownListener();
       mSipUdpServer->requestShutdown();
       delete mSipUdpServer;
       mSipUdpServer = NULL;
    }

    if(mpAuthenticationDb)
    {
        delete mpAuthenticationDb;
        mpAuthenticationDb = NULL;
    }

    if(mpAuthorizationUserIds)
    {
        delete mpAuthorizationUserIds;
        mpAuthorizationUserIds = NULL;
    }

    if(mpAuthorizationPasswords)
    {
        delete mpAuthorizationPasswords;
        mpAuthorizationPasswords = NULL;
    }

    allowedSipMethods.destroyAll();
    mMessageObservers.destroyAll();
}

/* ============================ MANIPULATORS ============================== */


// Assignment operator
SipUserAgent&
SipUserAgent::operator=(const SipUserAgent& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}

/*RL:*/
void SipUserAgent::initializeLineMgr(SipLineMgr* lineMgr) {
	this->mpLineMgr = lineMgr;
}


void SipUserAgent::shutdown(UtlBoolean blockingShutdown)
{
    mbShuttingDown = TRUE;
    mSipTransactions.stopTransactionTimers();

    if(blockingShutdown == TRUE)
    {
        OsEvent shutdownEvent;
        OsStatus res;
        int rpcRetVal;

        mbBlockingShutdown = TRUE;

        OsRpcMsg shutdownMsg(OsMsg::PHONE_APP, SipUserAgent::SHUTDOWN_MESSAGE, shutdownEvent);
        postMessage(shutdownMsg);

        res = shutdownEvent.wait();
        assert(res == OS_SUCCESS);

        res = shutdownEvent.getEventData(rpcRetVal);
        assert(res == OS_SUCCESS && rpcRetVal == OS_SUCCESS);

        mbShutdownDone = TRUE;
    }
    else
    {
        mbBlockingShutdown = FALSE;
        OsMsg shutdownMsg(OsMsg::PHONE_APP, SipUserAgent::SHUTDOWN_MESSAGE);
        postMessage(shutdownMsg);
    }
}

void SipUserAgent::enableStun(const char* szStunServer, 
                              int refreshPeriodInSecs, 
                              int stunOptions,
                              OsNotification* pNotification,
                              const char* szIp) 
{
    if (mSipUdpServer)
    {
        mSipUdpServer->enableStun(szStunServer, 
                szIp, 
                refreshPeriodInSecs, 
                stunOptions, 
                pNotification) ;
    }
}

void SipUserAgent::addMessageConsumer(OsServerTask* messageEventListener)
{
        // Need to do the real thing by keeping a list of consumers
        // and putting a mutex around the add to list
        //if(messageListener)
        //{
        //      osPrintf("WARNING: message consumer is NOT a LIST\n");
        //}
        //messageListener = messageEventListener;
    if(messageEventListener)
    {
        addMessageObserver(*(messageEventListener->getMessageQueue()));
    }
}

void SipUserAgent::addMessageObserver(OsMsgQ& messageQueue,
                                      const char* sipMethod,
                                      UtlBoolean wantRequests,
                                      UtlBoolean wantResponses,
                                      UtlBoolean wantIncoming,
                                      UtlBoolean wantOutGoing,
                                      const char* eventName,
                                      SipSession* pSession,
                                      void* observerData)
{
    SipObserverCriteria* observer = new SipObserverCriteria(observerData,
        &messageQueue,
        sipMethod, wantRequests, wantResponses, wantIncoming,
        wantOutGoing, eventName, pSession);

        {
            // Add the observer and its filter criteria to the list lock scope
        OsWriteLock lock(mObserverMutex);
        mMessageObservers.insert(observer);

        // Allow the specified method
        if(sipMethod && *sipMethod && wantRequests)
            allowMethod(sipMethod);
    }
}


UtlBoolean SipUserAgent::removeMessageObserver(OsMsgQ& messageQueue, void* pObserverData /*=NULL*/)
{
    OsWriteLock lock(mObserverMutex);
    SipObserverCriteria* pObserver = NULL ;
    UtlBoolean bRemovedObservers = FALSE ;

    // Traverse all of the observers and remove any that match the
    // message queue/observer data.  If the pObserverData is null, all
    // matching message queue/observers will be removed.  Otherwise, only
    // those observers that match both the message queue and observer data
    // are removed.
    UtlHashBagIterator iterator(mMessageObservers);
    while ((pObserver = (SipObserverCriteria*) iterator()))
    {
        if (pObserver->getObserverQueue() == &messageQueue)
        {
            if ((pObserverData == NULL) ||
                    (pObserverData == pObserver->getObserverData()))
            {
                bRemovedObservers = true ;
                UtlContainable* wasRemoved = mMessageObservers.removeReference(pObserver);

                if(wasRemoved)
                {
                   delete wasRemoved;
                }

            }
        }
    }

    return bRemovedObservers ;
}

void SipUserAgent::allowMethod(const char* methodName, const bool bAllow)
{
    if(methodName)
    {
        UtlString matchName(methodName);
        // Do not add the name if it is already in there
        if(NULL == allowedSipMethods.find(&matchName))
        {
            if (bAllow)
            {
                allowedSipMethods.append(new UtlString(methodName));
            }
        }
        else
        {
            if (!bAllow)
            {
                allowedSipMethods.destroy(allowedSipMethods.find(&matchName));
            }
        }
    }
}


UtlBoolean SipUserAgent::send(SipMessage& message,
                            OsMsgQ* responseListener,
                            void* responseListenerData)
{
   if(mbShuttingDown)
   {
      return FALSE;
   }

   UtlBoolean sendSucceeded = FALSE;
   UtlBoolean isResponse = message.isResponse();

   // If we are trying to do a NAT ping for the first time
   // hold off on all message sends until we have had time for
   // the first ping response.
   if(mPingLock)
   {
      OsTask::delay(5000);
      mPingLock = FALSE;
   }

   // ===========================================

   // Do all the stuff that does not require transaction locking first

   // Make sure the date field is set
   long epochDate;
   if(!message.getDateField(&epochDate))
   {
      message.setDateField();
   }

   UtlString method;
   if(isResponse)
   {
      // Add the authorization if the request required it
      //addResponseAuthorization(&message);

      int num = 0;
      message.getCSeqField(&num , &method);

      // All none failure responses need a contact
      UtlString recordRouteField;
      UtlString contactField;
      if(message.getResponseStatusCode() < SIP_MULTI_CHOICE_CODE &&
         ! message.getContactUri(0, &contactField) &&
         ( method.compareTo(SIP_REGISTER_METHOD, UtlString::ignoreCase) != 0))
      {
         // Build a contact uri- sipIpAddress.data(),
         // mUdpPort == SIP_PORT ? PORT_NONE : mUdpPort
         UtlString contactUri;
         SipMessage::buildSipUrl(&contactUri,
                                 message.getLocalIp().data(),
                                 mUdpPort == SIP_PORT ? PORT_NONE : mUdpPort,
                                 NULL, // Unspecified transport protocol
                                 defaultSipUser.data());
         message.setContactField(contactUri.data());
         contactUri.remove(0);
      }

   }
   else
   {
      message.getRequestMethod(&method);

      // Make sure that max-forwards is set
      int maxForwards;
      if(!message.getMaxForwards(maxForwards))
      {
         message.setMaxForwards(mMaxForwards);
      }

   }

   // ===========================================

   // Do the stuff that requires the transaction type knowledge
   // i.e. UA verse proxy transaction

   if(!isResponse)
   {
      // This should always be true now:
      if(message.isFirstSend())
      {
         // Save the transaction listener info
         if (responseListener)
         {
            message.setResponseListenerQueue(responseListener);
         }
         if (responseListenerData)
         {
            message.setResponseListenerData(responseListenerData);
         }
      }

      // This is not the first time this message has been sent
      else
      {
         // Should not be getting here.
         OsSysLog::add(FAC_SIP, PRI_WARNING, "SipUserAgent::send message being resent");
      }
   }

   // ===========================================

   // Find or create a transaction:
   UtlBoolean isUaTransaction = TRUE;
   enum SipTransaction::messageRelationship relationship;

   //mSipTransactions.lock();

   // verify that the transaction does not already exist
   SipTransaction* transaction = mSipTransactions.findTransactionFor(
      message,
      TRUE, // outgoing
      relationship);

   // Found a transaction for this message
   if(transaction)
   {
      isUaTransaction = transaction->isUaTransaction();

      // Response for which a transaction already exists
      if(isResponse)
      {
         if(isUaTransaction)
         {
            // It seems that the polite thing to do is to add the
            // allowed methods to all final responses
            UtlString allowedMethodsSet;
            if(message.getResponseStatusCode() >= SIP_OK_CODE &&
               !message.getAllowField(allowedMethodsSet))
            {
               UtlString allowedMethods;
               getAllowedMethods(&allowedMethods);
               message.setAllowField(allowedMethods);
            }
         }
      }

      // Request for which a transaction already exists
      else
      {
         // should not get here unless this is a CANCEL or ACK
         // request
         if((method.compareTo(SIP_CANCEL_METHOD) == 0) ||
            (method.compareTo(SIP_ACK_METHOD) == 0))
         {
         }

         // A request for which a transaction already exists
         // other than ACK and CANCEL
         else
         {
            // Should not be getting here
            OsSysLog::add(FAC_SIP, PRI_WARNING,
                          "SipUserAgent::send %s request matches existing transaction",
                          method.data());

            // We pretend there is no match so this becomes a
            // new transaction branch.  Make sure we unlock the
            // transaction before we reset to NULL.
            mSipTransactions.markAvailable(*transaction);
            transaction = NULL;
         }
      }
   }

   // No existing transaction for this message
   if(transaction == NULL)
   {
      if(isResponse)
      {
         // Should not get here except possibly on a server
         OsSysLog::add(FAC_SIP, PRI_WARNING,
                       "SipUserAgent::send response without an existing transaction"
                       );
      }
      else
      {
         // If there is already a via in the request this must
         // be a proxy transaction
         UtlString viaField;
         SipTransaction* parentTransaction = NULL;
         enum SipTransaction::messageRelationship parentRelationship;
         if(message.getViaField(&viaField, 0))
         {
            isUaTransaction = FALSE;

            // See if there is a parent server proxy transaction
            parentTransaction =
               mSipTransactions.findTransactionFor(message,
                                                   FALSE, // incoming
                                                   parentRelationship);
         }

         // Create a new transactions
         // This should only be for requests
         transaction = new SipTransaction(&message, TRUE,
                                          isUaTransaction);
         transaction->markBusy();
         mSipTransactions.addTransaction(transaction);

         if(!isUaTransaction &&
            parentTransaction)
         {
            if(parentRelationship ==
               SipTransaction::MESSAGE_DUPLICATE)
            {
               // Link the parent server transaction to the
               // child client transaction
               parentTransaction->linkChild(*transaction);
               // The parent will be unlocked with the transaction
            }
            else
            {
               UtlString parentRelationshipString;
               SipTransaction::getRelationshipString(parentRelationship, parentRelationshipString);
               OsSysLog::add(FAC_SIP, PRI_WARNING,
                             "SipUserAgent::send proxied client transaction not "
                             "part of server transaction, parent relationship: %s",
                             parentRelationshipString.data());

               if(parentTransaction)
               {
                  mSipTransactions.markAvailable(*parentTransaction);
               }
            }
         }
         else if(!isUaTransaction)
         {
            // this happens all the time in the authproxy, so log only at debug
            OsSysLog::add(FAC_SIP, PRI_DEBUG,
                          "SipUserAgent::send proxied client transaction does not have parent");
         }
         else if(parentTransaction)
         {
            mSipTransactions.markAvailable(*parentTransaction);
         }

         relationship = SipTransaction::MESSAGE_UNKNOWN;
      }
   }

   if(transaction)
   {
      // Make sure the User Agent field is set
      if(isUaTransaction)
      {
         setUserAgentHeader(message);

         // Make sure the accept language is set
         UtlString language;
         message.getAcceptLanguageField(&language);
         if(language.isNull())
         {
            message.setAcceptLanguageField("en");
         }

         // add allow field to Refer and Invite request . It is
         // mandatory for refer method
         UtlString allowedMethodsSet;
         if (   ! message.getAllowField(allowedMethodsSet)
             && (   method.compareTo(SIP_REFER_METHOD) == 0
                 || method.compareTo(SIP_INVITE_METHOD) == 0
                 )
             )
         {
            UtlString allowedMethods;
            getAllowedMethods(&allowedMethods);
            message.setAllowField(allowedMethods);
         }

         // Set the supported extensions if this is not
         // an ACK request and the Supported field is not already set.
         if(   method.compareTo(SIP_ACK_METHOD) != 0
            && !message.getHeaderValue(0, SIP_SUPPORTED_FIELD)
            )
         {
            UtlString supportedExtensions;
            getSupportedExtensions(supportedExtensions);
            if (supportedExtensions.length() > 0)
            {
               message.setSupportedField(supportedExtensions.data());
               supportedExtensions.remove(0);
            }
         }

         // There seems to be a move to making contact mandatory in INVITE
         // If this is an invite make sure there is a contact
         UtlString contactField;
         if(   (   method.compareTo(SIP_INVITE_METHOD) == 0
                || method.compareTo(SIP_REFER_METHOD) == 0
                || method.compareTo(SIP_SUBSCRIBE_METHOD) == 0
                )
            && ! message.getContactUri(0, &contactField)
            )
         {
            OsSysLog::add(FAC_SIP, PRI_INFO,"SipUserAgent::send added Contact to '%s'",
                          method.data());

            // Build a contact uri - sipIpAddress.data(), mUdpPort == SIP_PORT ? 0 : mUdpPort
            UtlString contactUri;
            SipMessage::buildSipUrl(&contactUri,
                                    sipIpAddress.data(),
                                    mUdpPort == SIP_PORT ? PORT_NONE : mUdpPort,
                                    NULL, // Unspecified transport protocol
                                    defaultSipUser.data());

            message.setContactField(contactUri.data());
            contactUri.remove(0);
         }
      }

      // If this is the top most parent and it is a client transaction
      //  There is no server transaction, so cancel all of the children
      if(   !isResponse
         && (method.compareTo(SIP_CANCEL_METHOD) == 0)
         && transaction->getTopMostParent() == NULL
         && !transaction->isServerTransaction()
         )
      {
         transaction->cancel(*this, mSipTransactions);
      }
      else
      {
         //  All other messages just get sent.
         sendSucceeded = transaction->handleOutgoing(message,
                                                     *this,
                                                     mSipTransactions,
                                                     relationship);
      }

      mSipTransactions.markAvailable(*transaction);
   }
   else
   {
      OsSysLog::add(FAC_SIP, PRI_ERR,"SipUserAgent::send failed to construct new transaction");
   }

   return(sendSucceeded);
}

UtlBoolean SipUserAgent::sendUdp(SipMessage* message,
                                 const char* serverAddress,
                                 int port)
{
  UtlBoolean isResponse = message->isResponse();
  UtlString method;
  int seqNum;
  UtlString seqMethod;
  int responseCode = 0;
  UtlBoolean sentOk = FALSE;
  UtlString msgBytes;
  UtlString messageStatusString = "SipUserAgent::sendUdp ";
  int timesSent = message->getTimesSent();

  if(!isResponse)
    {
      message->getRequestMethod(&method);
    }
  else
    {
      message->getCSeqField(&seqNum, &seqMethod);
      responseCode = message->getResponseStatusCode();
    }

  if(timesSent == 0)
    {
#ifdef TEST_PRINT
      osPrintf("First UDP send of message\n");
#endif

      message->touchTransportTime();

#ifdef TEST_PRINT
      osPrintf("SipUserAgent::sendUdp Sending UDP message\n");
#endif
    }
  // get the message if it was previously sent.
  else
    {
      char buffer[20];
      sprintf(buffer, "%d", timesSent);
      messageStatusString.append("resend ");
      messageStatusString.append(buffer);
      messageStatusString.append(" of UDP message\n");
    }

  // Send the message

  // Disallow an address begining with * as it gets broadcasted on NT
  if(! strchr(serverAddress, '*') && *serverAddress)
    {
      sentOk = mSipUdpServer->send(message, serverAddress, port);
    }
  else if(*serverAddress == '\0')
    {
      // Only bother processing if the logs are enabled
      if (    isMessageLoggingEnabled() ||
              OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
        {
          UtlString msgBytes;
          int msgLen;
          message->getBytes(&msgBytes, &msgLen);
          msgBytes.insert(0, "No send address\n");
          msgBytes.append("--------------------END--------------------\n");
          logMessage(msgBytes.data(), msgBytes.length());
          OsSysLog::add(FAC_SIP_OUTGOING, PRI_INFO, "%s", msgBytes.data());
        }
      sentOk = FALSE;
    }
  else
    {
      sentOk = FALSE;
    }

#ifdef TEST_PRINT
  osPrintf("SipUserAgent::sendUdp sipUdpServer send returned: %d\n",
           sentOk);
  osPrintf("SipUserAgent::sendUdp isResponse: %d method: %s seqmethod: %s responseCode: %d\n",
           isResponse, method.data(), seqMethod.data(), responseCode);
#endif
  // If we have not failed schedule a resend
  if(sentOk)
    {
      messageStatusString.append("UDP SIP User Agent sent message:\n");
      messageStatusString.append("----Remote Host:");
      messageStatusString.append(serverAddress);
      messageStatusString.append("---- Port: ");
      char buff[10];
      sprintf(buff, "%d", !portIsValid(port) ? 5060 : port);
      messageStatusString.append(buff);
      messageStatusString.append("----\n");

#ifdef TEST_PRINT
      osPrintf("%s", messageStatusString.data());
#endif
    }
  else
    {
      messageStatusString.append("UDP SIP User Agent failed to send message:\n");
      messageStatusString.append("----Remote Host:");
      messageStatusString.append(serverAddress);
      messageStatusString.append("---- Port: ");
      char buff[10];
      sprintf(buff, "%d", !portIsValid(port) ? 5060 : port);
      messageStatusString.append(buff);
      messageStatusString.append("----\n");
      message->logTimeEvent("FAILED");
    }

  // Only bother processing if the logs are enabled
  if (    isMessageLoggingEnabled() ||
          OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
    {
      int len;
      message->getBytes(&msgBytes, &len);
      msgBytes.insert(0, messageStatusString.data());
      msgBytes.append("--------------------END--------------------\n");
#ifdef TEST_PRINT
      osPrintf("%s", msgBytes.data());
#endif
      logMessage(msgBytes.data(), msgBytes.length());
      if (msgBytes.length())
      {
        OsSysLog::add(FAC_SIP_OUTGOING, PRI_INFO, "%s", msgBytes.data());
      }
    }

  // if we failed to send it is the calling functions problem to deal with the error

  return(sentOk);
}

UtlBoolean SipUserAgent::sendSymmetricUdp(const SipMessage& message,
                                        const char* serverAddress,
                                        int port)
{
    UtlBoolean sentOk = mSipUdpServer->sendTo(message,
                                             serverAddress,
                                             port);

    // Don't bother processing unless the logs are enabled
    if (    isMessageLoggingEnabled() ||
            OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
    {
        UtlString msgBytes;
        int msgLen;
        message.getBytes(&msgBytes, &msgLen);
        UtlString outcomeMsg;
        char portString[20];
        sprintf(portString, "%d", !portIsValid(port) ? 5060 : port);

        if(sentOk)
        {
            outcomeMsg.append("UDP SIP User Agent sentTo message:\n----Remote Host:");
            outcomeMsg.append(serverAddress);
            outcomeMsg.append("---- Port: ");
            outcomeMsg.append(portString);
            outcomeMsg.append("----\n");
            msgBytes.insert(0, outcomeMsg);
            msgBytes.append("--------------------END--------------------\n");
        }
        else
        {
            outcomeMsg.append("SIP User agent FAILED sendTo message:\n----Remote Host:");
            outcomeMsg.append(serverAddress);
            outcomeMsg.append("---- Port: ");
            outcomeMsg.append(portString);
            outcomeMsg.append("----\n");
            msgBytes.insert(0, outcomeMsg);
            msgBytes.append("--------------------END--------------------\n");
        }

        logMessage(msgBytes.data(), msgBytes.length());
        OsSysLog::add(FAC_SIP_OUTGOING, PRI_INFO, "%s", msgBytes.data());
    }

    return(sentOk);
}

UtlBoolean SipUserAgent::sendStatelessResponse(SipMessage& rresponse)
{
    UtlBoolean sendSucceeded = FALSE;

    // Forward via the server tranaction
    SipMessage responseCopy(rresponse);
    responseCopy.removeLastVia();
    responseCopy.resetTransport();
    responseCopy.clearDNSField();

    UtlString sendProtocol;
    UtlString sendAddress;
    int sendPort;
    int receivedPort;
    UtlBoolean receivedSet;
    UtlBoolean maddrSet;
    UtlBoolean receivedPortSet;

    // use the via as the place to send the response
    responseCopy.getLastVia(&sendAddress, &sendPort, &sendProtocol,
        &receivedPort, &receivedSet, &maddrSet,
        &receivedPortSet);

    // If the sender of the request indicated support of
    // rport (i.e. received port) send this response back to
    // the same port it came from
    if(portIsValid(receivedPort) &&
        receivedSet && receivedPortSet)
    {
        sendPort = receivedPort;
    }

    if(sendProtocol.compareTo(SIP_TRANSPORT_UDP, UtlString::ignoreCase) == 0)
    {
        sendSucceeded = sendUdp(&responseCopy, sendAddress.data(), sendPort);
    }
    else if(sendProtocol.compareTo(SIP_TRANSPORT_TCP, UtlString::ignoreCase) == 0)
    {
        sendSucceeded = sendTcp(&responseCopy, sendAddress.data(), sendPort);
    }
#ifdef SIP_TLS
    else if(sendProtocol.compareTo(SIP_TRANSPORT_TLS, UtlString::ignoreCase) == 0)
    {
        sendSucceeded = sendTls(&responseCopy, sendAddress.data(), sendPort);
    }
#endif

    return(sendSucceeded);
}

UtlBoolean SipUserAgent::sendStatelessRequest(SipMessage& request,
                           UtlString& address,
                           int port,
                           OsSocket::IpProtocolSocketType protocol,
                           UtlString& branchId)
{
    // Convert the enum to a protocol string
    UtlString viaProtocolString;
    SipMessage::convertProtocolEnumToString(protocol,
                                            viaProtocolString);

    // Get via info
    UtlString viaAddress;
    int viaPort;
    getViaInfo(protocol,
                         viaAddress,
                         viaPort);

    // Add the via field data
    request.addVia(viaAddress.data(),
                   viaPort,
                   viaProtocolString,
                   branchId.data());


    // Send using the correct protocol
    UtlBoolean sendSucceeded = FALSE;
    if(protocol == OsSocket::UDP)
    {
        sendSucceeded = sendUdp(&request, address.data(), port);
    }
    else if(protocol == OsSocket::TCP)
    {
        sendSucceeded = sendTcp(&request, address.data(), port);
    }
#ifdef SIP_TLS
    else if(protocol == OsSocket::SSL_SOCKET)
    {
        sendSucceeded = sendTls(&request, address.data(), port);
    }
#endif

    return(sendSucceeded);
}

UtlBoolean SipUserAgent::sendTcp(SipMessage* message,
                                 const char* serverAddress,
                                 int port)
{
    int sendSucceeded = FALSE;
    int len;
    UtlString msgBytes;
    UtlString messageStatusString = "SipUserAgent::sendTcp ";

    // Disallow an address begining with * as it gets broadcasted on NT
    if(!strchr(serverAddress,'*') && *serverAddress)
    {
        if (mSipTcpServer)
        {
            sendSucceeded = mSipTcpServer->send(message, serverAddress, port);
        }
    }
    else if(*serverAddress == '\0')
    {
        if (    isMessageLoggingEnabled() ||
                OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
        {
            message->getBytes(&msgBytes, &len);
            msgBytes.insert(0, "No send address\n");
            msgBytes.append("--------------------END--------------------\n");
            logMessage(msgBytes.data(), msgBytes.length());
            OsSysLog::add(FAC_SIP_OUTGOING, PRI_INFO, "%s", msgBytes.data());
        }
        sendSucceeded = FALSE;
    }
    else
    {
        sendSucceeded = FALSE;
    }

    if(sendSucceeded)
    {
        messageStatusString.append("TCP SIP User Agent sent message:\n");
        //osPrintf("%s", messageStatusString.data());
    }
    else
    {
        messageStatusString.append("TCP SIP User Agent failed to send message:\n");
        //osPrintf("%s", messageStatusString.data());
        message->logTimeEvent("FAILED");
    }

    if (   isMessageLoggingEnabled()
        || OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO)
        )
    {
        message->getBytes(&msgBytes, &len);
        messageStatusString.append("----Remote Host:");
            messageStatusString.append(serverAddress);
            messageStatusString.append("---- Port: ");
            char buff[10];
            sprintf(buff, "%d", !portIsValid(port) ? 5060 : port);
            messageStatusString.append(buff);
            messageStatusString.append("----\n");

        msgBytes.insert(0, messageStatusString.data());
        msgBytes.append("--------------------END--------------------\n");
#ifdef TEST_PRINT
        osPrintf("%s", msgBytes.data());
#endif
        logMessage(msgBytes.data(), msgBytes.length());
        OsSysLog::add(FAC_SIP_OUTGOING , PRI_INFO, "%s", msgBytes.data());
    }

    return(sendSucceeded);
}


UtlBoolean SipUserAgent::sendTls(SipMessage* message,
                                                                const char* serverAddress,
                                                                int port)
{
#ifdef SIP_TLS
   int sendSucceeded = FALSE;
   int len;
   UtlString msgBytes;
   UtlString messageStatusString;

   // Disallow an address begining with * as it gets broadcasted on NT
   if(!strchr(serverAddress,'*') && *serverAddress)
   {
      sendSucceeded = mSipTlsServer->send(message, serverAddress, port);
   }
   else if(*serverAddress == '\0')
   {
      if (    isMessageLoggingEnabled() ||
          OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
      {
         message->getBytes(&msgBytes, &len);
         msgBytes.insert(0, "No send address\n");
         msgBytes.append("--------------------END--------------------\n");
         logMessage(msgBytes.data(), msgBytes.length());
         OsSysLog::add(FAC_SIP_OUTGOING, PRI_INFO, "%s", msgBytes.data());
      }
      sendSucceeded = FALSE;
   }
   else
   {
      sendSucceeded = FALSE;
   }

   if(sendSucceeded)
   {
      messageStatusString.append("TLS SIP User Agent sent message:\n");
      //osPrintf("%s", messageStatusString.data());

   }
   else
   {
      messageStatusString.append("TLS SIP User Agent failed to send message:\n");
      //osPrintf("%s", messageStatusString.data());
      message->logTimeEvent("FAILED");
   }

   if (    isMessageLoggingEnabled() ||
       OsSysLog::willLog(FAC_SIP_OUTGOING, PRI_INFO))
   {
      message->getBytes(&msgBytes, &len);
      messageStatusString.append("----Remote Host:");
      messageStatusString.append(serverAddress);
      messageStatusString.append("---- Port: ");
      char buff[10];
      sprintf(buff, "%d", !portIsValid(port) ? 5060 : port);
      messageStatusString.append(buff);
      messageStatusString.append("----\n");

      msgBytes.insert(0, messageStatusString.data());
      msgBytes.append("--------------------END--------------------\n");
#ifdef TEST_PRINT
      osPrintf("%s", msgBytes.data());
#endif
      logMessage(msgBytes.data(), msgBytes.length());
      OsSysLog::add(FAC_SIP_OUTGOING , PRI_INFO, "%s", msgBytes.data());
   }

   return(sendSucceeded);
#else
   return FALSE ;
#endif
}

void SipUserAgent::dispatch(SipMessage* message, int messageType)
{
   if(mbShuttingDown)
   {
       delete message;
       return;
   }

   int len;
   UtlString msgBytes;
   UtlString messageStatusString;
   UtlBoolean resentWithAuth = FALSE;
   UtlBoolean isResponse = message->isResponse();
   UtlBoolean shouldDispatch = FALSE;
   SipMessage* delayedDispatchMessage = NULL;

#ifdef LOG_TIME
   OsTimeLog eventTimes;
   eventTimes.addEvent("start");
#endif

   // Get the message bytes for logging before the message is
   // potentially deleted or nulled out.
   if (   isMessageLoggingEnabled()
       || OsSysLog::willLog(FAC_SIP_INCOMING_PARSED, PRI_DEBUG)
       || OsSysLog::willLog(FAC_SIP, PRI_DEBUG))
   {
      message->getBytes(&msgBytes, &len);
   }

	/*RL*/
   // Monitoring of header fields...
   this->checkWatchedFields(message);

   if(messageType == SipMessageEvent::APPLICATION)
   {
#ifdef TEST_PRINT
      osPrintf("SIP User Agent received message via protocol: %d\n",
               message->getSendProtocol());
      message->logTimeEvent("DISPATCHING");
#endif

      UtlBoolean isUaTransaction = mIsUaTransactionByDefault;
      enum SipTransaction::messageRelationship relationship;
      SipTransaction* transaction =
         mSipTransactions.findTransactionFor(*message,
                                             FALSE, // incoming
                                             relationship);

#ifdef LOG_TIME
      eventTimes.addEvent("found TX");
#endif
      if(transaction == NULL)
      {
         if(isResponse)
         {
            OsSysLog::add(FAC_SIP, PRI_WARNING,"SipUserAgent::dispatch "
                          "received response without transaction");

#ifdef TEST_PRINT
            if (OsSysLog::willLog(FAC_SIP, PRI_DEBUG))
            {
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "=Response w/o request=>\n%s\n======================>\n",
                             msgBytes.data());

               UtlString transString;
               mSipTransactions.toStringWithRelations(transString, *message, FALSE);
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "Transaction list:\n%s\n===End transaction list===",
                             transString.data());
            }
#endif

            UtlString callId;
            message->getCallIdField(&callId);
            // Check the call-id to see if this is the ping
            // response
            if(callId.index("-ping@") > 0)
            {
               UtlString natAddress;
               int dummyPort;
               UtlString dummyProtocol;
               int natPort;
               UtlBoolean receivedSet;
               UtlBoolean maddrSet;
               UtlBoolean receivedPortSet;
               message->getLastVia(&natAddress, &dummyPort, &dummyProtocol,
                                   &natPort, &receivedSet, &maddrSet, &receivedPortSet);
               if(receivedSet && receivedPortSet)
               {
                  Url newContact;
                  newContact.setHostAddress(natAddress.data());
                  newContact.setHostPort(natPort);
                  newContact.toString(mContactAddress);

                  // Set the address and port for the via as
                  // well
                  sipIpAddress = natAddress;
                  mSipPort = natPort;
                  OsSysLog::add(FAC_SIP, PRI_DEBUG,"SipUserAgent:dispatch set new contact: %s",
                                mContactAddress.data());
               }
               mPingLock = FALSE;
            }
         }

         // New transaction for incoming request
         else
         {
            transaction = new SipTransaction(message, FALSE,
                                             isUaTransaction);

            // Add the new transaction to the list
            transaction->markBusy();
            mSipTransactions.addTransaction(transaction);

            UtlString method;
            message->getRequestMethod(&method);

            if(method.compareTo(SIP_ACK_METHOD) == 0)
            {
               // This may be normal - it will occur whenever the ACK is not traversing
               // the same proxy where the transaction is completing was origniated.
               // This happens on each call setup in the authproxy, for example, because
               // the original transaction was in the forking proxy.
               relationship = SipTransaction::MESSAGE_ACK;
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "SipUserAgent::dispatch received ACK without transaction");
            }
            else if(method.compareTo(SIP_CANCEL_METHOD) == 0)
            {
               relationship = SipTransaction::MESSAGE_CANCEL;
               OsSysLog::add(FAC_SIP, PRI_WARNING,
                             "SipUserAgent::dispatch received CANCEL without transaction");
            }
            else
            {
               relationship = SipTransaction::MESSAGE_REQUEST;
            }
         }
      }

#ifdef LOG_TIME
      eventTimes.addEvent("handling TX");
#endif
      // This is a message that was already recieved once
      if (   transaction
          && relationship == SipTransaction::MESSAGE_DUPLICATE
          )
      {
         // Resends of final INVITE responses need to be
         // passed through if they are 2xx class or the ACk
         // needs to be resent if it was a failure (i.e. 3xx,4xx,5xx,6xx)
         if(message->isResponse())
         {
            int responseCode = message->getResponseStatusCode();
            UtlString transactionMethod;
            int respCseq;
            message->getCSeqField(&respCseq, &transactionMethod);

            if (   responseCode >= SIP_2XX_CLASS_CODE 
                && transactionMethod.compareTo(SIP_INVITE_METHOD) == 0
                )
            {
               transaction->handleIncoming(*message,
                                           *this,
                                           relationship,
                                           mSipTransactions,
                                           delayedDispatchMessage);

               // Should never dispatch a resendof a 2xx
               if(delayedDispatchMessage)
               {
                  delete delayedDispatchMessage;
                  delayedDispatchMessage = NULL;
               }
            }
         }

         messageStatusString.append("Received duplicate message\n");
#ifdef TEST_PRINT
         osPrintf("%s", messageStatusString.data());
#endif
      }

      // The first time we received this message
      else if (transaction)
      {
         switch (relationship)
         {
         case SipTransaction::MESSAGE_FINAL:
         case SipTransaction::MESSAGE_PROVISIONAL:
         case SipTransaction::MESSAGE_CANCEL_RESPONSE:
         {
            int delayedResponseCode = -1;
            SipMessage* request = transaction->getRequest();
            isUaTransaction = transaction->isUaTransaction();

            shouldDispatch =
               transaction->handleIncoming(*message,
                                           *this,
                                           relationship,
                                           mSipTransactions,
                                           delayedDispatchMessage);

            if(delayedDispatchMessage)
            {
               delayedResponseCode =
                  delayedDispatchMessage->getResponseStatusCode();
            }

            // Check for Authentication Error
            if(   request
               && delayedDispatchMessage
               && delayedResponseCode == HTTP_UNAUTHORIZED_CODE
               && isUaTransaction
               )
            {
               resentWithAuth =
                  resendWithAuthorization(delayedDispatchMessage,
                                          request,
                                          &messageType,
                                          HttpMessage::SERVER);
            }

            // Check for Proxy Authentication Error
            if(   request
               && delayedDispatchMessage
               && delayedResponseCode == HTTP_PROXY_UNAUTHORIZED_CODE
               && isUaTransaction
               )
            {
               resentWithAuth =
                  resendWithAuthorization(delayedDispatchMessage,
                                          request,
                                          &messageType,
                                          HttpMessage::PROXY);
            }

            // If we requested authentication for this response,
            // validate the authorization
            UtlString requestAuthScheme;
            if(   request
               && request->getAuthenticationScheme(&requestAuthScheme,
                                                   HttpMessage::SERVER))
            {
               UtlString reqUri;
               request->getRequestUri(&reqUri);

               if(authorized(message, reqUri.data()))
               {
#ifdef TEST_PRINT
                  osPrintf("response is authorized\n");
#endif
               }

               // What do we do with an unauthorized response?
               // For now we just let it through.
               else
               {
                  OsSysLog::add(FAC_SIP, PRI_WARNING, "UNAUTHORIZED RESPONSE");
#                 ifdef TEST_PRINT
                  osPrintf("WARNING: UNAUTHORIZED RESPONSE\n");
#                 endif
               }
            }

            // If we have a request for this incoming response
            // Forward it on to interested applications
            if (   request
                && (shouldDispatch || delayedDispatchMessage)
                )
            {
               UtlString method;
               request->getRequestMethod(&method);
               OsMsgQ* responseQ = NULL;
               responseQ =  request->getResponseListenerQueue();
               if (responseQ  && shouldDispatch)
               {
                  SipMessage * msg = new SipMessage(*message);
                  msg->setResponseListenerData(request->getResponseListenerData() );
                  SipMessageEvent eventMsg(msg);
                  eventMsg.setMessageStatus(messageType);
                  responseQ->send(eventMsg);
                  // The SipMessage gets freed with the SipMessageEvent
                  msg = NULL;
               }

               if(responseQ  && delayedDispatchMessage)
               {
                  SipMessage* tempDelayedDispatchMessage =
                     new SipMessage(*delayedDispatchMessage);

                  tempDelayedDispatchMessage->setResponseListenerData(
                     request->getResponseListenerData()
                                                                      );

                  SipMessageEvent eventMsg(tempDelayedDispatchMessage);
                  eventMsg.setMessageStatus(messageType);
                  responseQ->send(eventMsg);
                  // The SipMessage gets freed with the SipMessageEvent
                  tempDelayedDispatchMessage = NULL;
               }
            }
         }
         break;

         case SipTransaction::MESSAGE_REQUEST:
         {
            // if this is a request check if it is supported
            SipMessage* response = NULL;
            UtlString disallowedExtensions;
            UtlString method;
            UtlString allowedMethods;
            UtlString contentEncoding;
            UtlString toAddress;
            UtlString fromAddress;
            UtlString uriAddress;
            UtlString protocol;
            UtlString sipVersion;
            int port;
            int seqNumber;
            UtlString seqMethod;
            UtlString callIdField;
            int maxForwards;

            message->getRequestMethod(&method);
            if(isUaTransaction)
            {
               getAllowedMethods(&allowedMethods);
               whichExtensionsNotAllowed(message, &disallowedExtensions);
               message->getContentEncodingField(&contentEncoding);

               //delete leading and trailing white spaces
               disallowedExtensions = disallowedExtensions.strip(UtlString::both);
               allowedMethods = allowedMethods.strip(UtlString::both);
               contentEncoding = contentEncoding.strip(UtlString::both);
            }

            message->getToAddress(&toAddress, &port, &protocol);
            message->getFromAddress(&fromAddress, &port, &protocol);
            message->getUri(&uriAddress, &port, &protocol);
            message->getRequestProtocol(&sipVersion);
            sipVersion.toUpper();
            message->getCSeqField(&seqNumber, &seqMethod);
            seqMethod.toUpper();
            message->getCallIdField(&callIdField);

            // Check if the method is supported
            if(   isUaTransaction
               && !isMethodAllowed(method.data())
               )
            {
               response = new SipMessage();

               response->setRequestUnimplemented(message);
            }

            // Check if the extensions are supported
            else if(   mDoUaMessageChecks
                    && isUaTransaction
                    && !disallowedExtensions.isNull()
                    )
            {
               response = new SipMessage();
               response->setRequestBadExtension(message,
                                                disallowedExtensions);
            }

            // Check if the encoding is supported
            // i.e. no encoding
            else if(   mDoUaMessageChecks
                    && isUaTransaction
                    && !contentEncoding.isNull()
                    )
            {
               response = new SipMessage();
               response->setRequestBadContentEncoding(message,"");
            }

            // Check the addresses are present
            else if(toAddress.isNull() || fromAddress.isNull() ||
                    uriAddress.isNull())
            {
               response = new SipMessage();
               response->setRequestBadAddress(message);
            }

            // Check SIP version
            else if(strcmp(sipVersion.data(), SIP_PROTOCOL_VERSION) != 0)
            {
               response = new SipMessage();
               response->setRequestBadVersion(message);
            }

            // Check for missing CSeq or Call-Id
            else if(callIdField.isNull() || seqNumber < 0 ||
                    strcmp(seqMethod.data(), method.data()) != 0)
            {
               response = new SipMessage();
               response->setRequestBadRequest(message);
            }

            // Authentication Required
            else if(isUaTransaction &&
                    shouldAuthenticate(message))
            {
               if(!authorized(message))
               {
#ifdef TEST_PRINT
                  osPrintf("SipUserAgent::dispatch message Unauthorized\n");
#endif
                  response = new SipMessage();
                  response->setRequestUnauthorized(message,
                                                   mAuthenticationScheme.data(),
                                                   mAuthenticationRealm.data(),
                                                   "1234567890", // :TODO: nonce should be generated by SipNonceDb
                                                   "abcdefghij"  // opaque
                                                   );
               }
#ifdef TEST_PRINT
               else
               {
                  osPrintf("SipUserAgent::dispatch message Authorized\n");
               }
#endif //TEST_PRINT
            }

            // Process Options requests :TODO: - in the redirect server does this route?
            else if(isUaTransaction &&
                    !message->isResponse() &&
                    method.compareTo(SIP_OPTIONS_METHOD) == 0)
            {
               // Send an OK, the allowed field will get added to all final responces.
               response = new SipMessage();
               response->setResponseData(message,
                                         SIP_OK_CODE,
                                         SIP_OK_TEXT);

               delete(message);
               message = NULL;
            }

            else if(message->getMaxForwards(maxForwards))
            {
               if(maxForwards <= 0)
               {


                  response = new SipMessage();
                  response->setResponseData(message,
                                            SIP_TOO_MANY_HOPS_CODE,
                                            SIP_TOO_MANY_HOPS_TEXT);

                  setUserAgentHeader(*response);
                  
                  // If we are suppose to return the vias in the
                  // error response for Max-Forwards exeeded
                  if(mReturnViasForMaxForwards)
                  {

                     // The setBody method frees up the body before
                     // setting the new one, if there is a body
                     // We remove the body so that we can serialize
                     // the message without getting the body
                     message->setBody(NULL);

                     UtlString sipFragString;
                     int sipFragLen;
                     message->getBytes(&sipFragString, &sipFragLen);

                     // Create a body to contain the Vias from the request
                     HttpBody* sipFragBody =
                        new HttpBody(sipFragString.data(),
                                     sipFragLen,
                                     CONTENT_TYPE_MESSAGE_SIPFRAG);

                     // Attach the body to the response
                     response->setBody(sipFragBody);

                     // Set the content type of the body to be sipfrag
                     response->setContentType(CONTENT_TYPE_MESSAGE_SIPFRAG);
                  }

                  delete(message);
                  message = NULL;
               }
            }
            else
            {
               message->setMaxForwards(mMaxForwards);
            }

            // If the request is invalid
            if(response)
            {
               // Send the error response
               transaction->handleOutgoing(*response,
                                           *this,
                                           mSipTransactions,
                                           SipTransaction::MESSAGE_FINAL);
               delete response;
               response = NULL;
               if(message) delete message;
               message = NULL;
            }
            else if(message)
            {
               shouldDispatch =
                  transaction->handleIncoming(*message,
                                              *this,
                                              relationship,
                                              mSipTransactions,
                                              delayedDispatchMessage);
            }
            else
            {
               OsSysLog::add(FAC_SIP, PRI_ERR, "SipUserAgent::dispatch NULL message to handle");
               //osPrintf("ERROR: SipUserAgent::dispatch NULL message to handle\n");
            }
         }
         break;
         
         case SipTransaction::MESSAGE_ACK:
         case SipTransaction::MESSAGE_2XX_ACK:
         case SipTransaction::MESSAGE_CANCEL:
         {
            int maxForwards;

            // Check the ACK max-forwards has not gone too many hopes
            if(!isResponse &&
               (relationship == SipTransaction::MESSAGE_ACK ||
                relationship == SipTransaction::MESSAGE_2XX_ACK) &&
               message->getMaxForwards(maxForwards) &&
               maxForwards <= 0 )
            {

               // Drop ACK on the floor.
               if(message) delete(message);
               message = NULL;
            }

            else if(message)
            {
               shouldDispatch =
                  transaction->handleIncoming(*message,
                                              *this,
                                              relationship,
                                              mSipTransactions,
                                              delayedDispatchMessage);
            }
         }
         break;

         case SipTransaction::MESSAGE_NEW_FINAL:
         {
            // Forward it on to interested applications
            SipMessage* request = transaction->getRequest();
            shouldDispatch = TRUE;
            if( request)
            {
               UtlString method;
               request->getRequestMethod(&method);
               OsMsgQ* responseQ = NULL;
               responseQ =  request->getResponseListenerQueue();
               if (responseQ)
               {
                  SipMessage * msg = new SipMessage(*message);
                  msg->setResponseListenerData(request->getResponseListenerData() );
                  SipMessageEvent eventMsg(msg);
                  eventMsg.setMessageStatus(messageType);
                  responseQ->send(eventMsg);
                  // The SipMessage gets freed with the SipMessageEvent
                  msg = NULL;
               }
            }
         }
         break;

         default:
         {
            if (OsSysLog::willLog(FAC_SIP, PRI_WARNING))
            {
               UtlString relationString;
               SipTransaction::getRelationshipString(relationship, relationString);
               OsSysLog::add(FAC_SIP, PRI_WARNING, 
                             "SipUserAgent::dispatch unhandled incoming message: %s",
                             relationString.data());
            }
         }
         break;
         }
      }

      if(transaction)
      {
         mSipTransactions.markAvailable(*transaction);
      }
   }
   else
   {
      shouldDispatch = TRUE;
      messageStatusString.append("SIP User agent FAILED to send message:\n");
   }

#ifdef LOG_TIME
   eventTimes.addEvent("queuing");
#endif

   if (    isMessageLoggingEnabled()
       || OsSysLog::willLog(FAC_SIP_INCOMING_PARSED, PRI_DEBUG)
       )
   {
      msgBytes.insert(0, messageStatusString.data());
      msgBytes.append("++++++++++++++++++++END++++++++++++++++++++\n");
#ifdef TEST_PRINT
      osPrintf("%s", msgBytes.data());
#endif
      logMessage(msgBytes.data(), msgBytes.length());
      OsSysLog::add(FAC_SIP_INCOMING_PARSED, PRI_DEBUG, "%s", msgBytes.data());
   }

   if(message && shouldDispatch)
   {
#ifdef TEST_PRINT
      osPrintf("DISPATCHING message\n");
#endif

      queueMessageToObservers(message, messageType);
   }
   else
   {
      delete message;
      message = NULL;
   }

   if(delayedDispatchMessage)
   {
      if (   isMessageLoggingEnabled()
          || OsSysLog::willLog(FAC_SIP_INCOMING_PARSED, PRI_DEBUG)
          )
      {
         UtlString delayMsgString;
         int delayMsgLen;
         delayedDispatchMessage->getBytes(&delayMsgString,
                                          &delayMsgLen);
         delayMsgString.insert(0, "SIP User agent delayed dispatch message:\n");
         delayMsgString.append("++++++++++++++++++++END++++++++++++++++++++\n");
#ifdef TEST_PRINT
         osPrintf("%s", delayMsgString.data());
#endif
         logMessage(delayMsgString.data(), delayMsgString.length());
         OsSysLog::add(FAC_SIP_INCOMING_PARSED, PRI_DEBUG, "%s",
                       delayMsgString.data());
      }

      queueMessageToObservers(delayedDispatchMessage, messageType);
   }

#ifdef LOG_TIME
   eventTimes.addEvent("GC");
#endif

   // All garbage collection should now be done in the
   // context of the SipUserAgent to prevent hickups in
   // the reading of SipMessages off the sockets.
   //garbageCollection();

#ifdef LOG_TIME
   eventTimes.addEvent("finish");
   UtlString timeString;
   eventTimes.getLogString(timeString);
   OsSysLog::add(FAC_SIP, PRI_DEBUG, "SipUserAgent::dispatch time log: %s",
                 timeString.data());
#endif
}

void SipUserAgent::queueMessageToObservers(SipMessage* message,
                                           int messageType)
{
   UtlString callId;
   message->getCallIdField(&callId);
   UtlString method;
   message->getRequestMethod(&method);

   // Create a new message event
   SipMessageEvent event(message);
   event.setMessageStatus(messageType);

   // Find all of the observers which are interested in
   // this method and post the message
   UtlBoolean isRsp = message->isResponse();
   if(isRsp)
   {
      int cseq;
      message->getCSeqField(&cseq, &method);
   }

   queueMessageToInterestedObservers(event, method);
   // send it to those with no method descrimination as well
   queueMessageToInterestedObservers(event, "");

   // Do not delete the message it gets deleted with the event
   message = NULL;
}

void SipUserAgent::queueMessageToInterestedObservers(SipMessageEvent& event,
                                                     const UtlString& method)
{
   const SipMessage* message;
   if((message = event.getMessage()))
   {
      // Find all of the observers which are interested in
      // this method and post the message
      UtlString messageEventName;
      message->getEventField(messageEventName);

      // do these constructors before taking the lock
      UtlString observerMatchingMethod(method);

      // lock the message observer list
      OsReadLock lock(mObserverMutex);

      UtlHashBagIterator observerIterator(mMessageObservers, &observerMatchingMethod);
      SipObserverCriteria* observerCriteria;
      while ((observerCriteria = (SipObserverCriteria*) observerIterator()))
      {
         // Check message direction and type
         if (   (  message->isResponse() && observerCriteria->wantsResponses())
             || (! message->isResponse() && observerCriteria->wantsRequests())
             )
         {
            // Decide if the event filter applies
            bool useEventFilter = false;
            bool matchedEvent = false;
            if (! message->isResponse()) // events apply only to requests
            {
               UtlString criteriaEventName;
               observerCriteria->getEventName(criteriaEventName);

               useEventFilter = ! criteriaEventName.isNull();
               if (useEventFilter)
               {
                  // see if the event type matches
                  matchedEvent = (   (   method.compareTo(SIP_SUBSCRIBE_METHOD,
                                                          UtlString::ignoreCase)
                                      == 0
                                      || method.compareTo(SIP_NOTIFY_METHOD,
                                                          UtlString::ignoreCase)
                                      == 0
                                      )
                                  && 0==messageEventName.compareTo(criteriaEventName,
                                                                   UtlString::ignoreCase
                                                                   )
                                  );
               }
            } // else - this is a response - event filter is not applicable

            // Check to see if the session criteria matters
            SipSession* pCriteriaSession = observerCriteria->getSession() ;
            bool useSessionFilter = (NULL != pCriteriaSession);
            UtlBoolean matchedSession = FALSE;
            if (useSessionFilter)
            {
               // it matters; see if it matches
               matchedSession = pCriteriaSession->isSameSession((SipMessage&) *message);
            }

            // We have a message type (req|rsp) the observer wants - apply filters
            if (   (! useSessionFilter || matchedSession)
                && (! useEventFilter   || matchedEvent)
                )
            {
               // This event is interesting, so send it up...
               OsMsgQ* observerQueue = observerCriteria->getObserverQueue();
               void* observerData = observerCriteria->getObserverData();

               // Cheat a little and set the observer data to be passed back
               ((SipMessage*) message)->setResponseListenerData(observerData);

               // Put the message in the observers queue
               int numMsgs = observerQueue->numMsgs();
               int maxMsgs = observerQueue->maxMsgs();
               if (numMsgs < maxMsgs)
               {
                  observerQueue->send(event);
               }
               else
               {
                  OsSysLog::add(FAC_SIP, PRI_ERR,
                                "queueMessageToInterestedObservers - queue full (numMsgs=%d)",
                                numMsgs);
               }
            }
         }
         else
         {
            // either direction or req/rsp not a match
         }
      } // while observers
   }
   else
   {
      OsSysLog::add(FAC_SIP, PRI_CRIT, "queueMessageToInterestedObservers - no message");
   }
}


UtlBoolean checkMethods(SipMessage* message)
{
        return(TRUE);
}

UtlBoolean checkExtensions(SipMessage* message)
{
        return(TRUE);
}


UtlBoolean SipUserAgent::handleMessage(OsMsg& eventMessage)
{
   UtlBoolean messageProcessed = FALSE;
   //osPrintf("SipUserAgent: handling message\n");
   int msgType = eventMessage.getMsgType();
   int msgSubType = eventMessage.getMsgSubType();

   if(msgType == OsMsg::PHONE_APP)
   {
      // Final message from SipUserAgent::shutdown - all timers are stopped and are safe to delete
      if(msgSubType == SipUserAgent::SHUTDOWN_MESSAGE)
      {
#ifdef TEST_PRINT
         osPrintf("SipUserAgent::handleMessage shutdown complete message.\n");
#endif
         mSipTransactions.deleteTransactionTimers();

         if(mbBlockingShutdown == TRUE)
         {
            OsEvent* pEvent = ((OsRpcMsg&)eventMessage).getEvent();

            OsStatus res = pEvent->signal(OS_SUCCESS);
            assert(res == OS_SUCCESS);
         }
         else
         {
            mbShutdownDone = TRUE;
         }
      }
      else
      {
         SipMessage* sipMsg = (SipMessage*)((SipMessageEvent&)eventMessage).getMessage();
         if(sipMsg)
         {
            //messages for which the UA is consumer will end up here.
            OsSysLog::add(FAC_SIP, PRI_DEBUG, "SipUserAgent::handleMessage posting message");

            // I cannot remember what kind of message ends up here???
            if (OsSysLog::willLog(FAC_SIP, PRI_DEBUG))
            {
               int len;
               UtlString msgBytes;
               sipMsg->getBytes(&msgBytes, &len);
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "??????????????????????????????????????\n"
                             "%s???????????????????????????????????\n",
                             msgBytes.data());
            }
         }
      }
      messageProcessed = TRUE;
   }

   // A timer expired
   else if(msgType == OsMsg::OS_EVENT &&
           msgSubType == OsEventMsg::NOTIFY)
   {
      OsTimer* timer;
      SipMessageEvent* sipEvent = NULL;

      ((OsEventMsg&)eventMessage).getUserData((int&)sipEvent);
      ((OsEventMsg&)eventMessage).getEventData((int&)timer);

      if(sipEvent)
      {
         const SipMessage* sipMessage = sipEvent->getMessage();
         int msgEventType = sipEvent->getMessageStatus();

         // Resend timeout
         if(msgEventType == SipMessageEvent::TRANSACTION_RESEND)
         {
            if(sipMessage)
            {
               // Note: only delete the timer and notifier if there
               // is a message AND we can get a lock on the transaction.  
               //  WARNING: you cannot touch the contents of the transaction
               // attached to the message until the transaction has been
               // locked (via findTransactionFor, if no transaction is 
               // returned, it either no longer exists or we could not get
               // a lock for it.

#              ifdef TEST_PRINT
               {
                  UtlString callId;
                  int protocolType = sipMessage->getSendProtocol();
                  sipMessage->getCallIdField(&callId);

                  if(sipMessage->getSipTransaction() == NULL)
                  {
                     osPrintf("SipUserAgent::handleMessage "
                              "resend Timeout message with NULL transaction\n");
                  }
                  osPrintf("SipUserAgent::handleMessage "
                           "resend Timeout of message for %d protocol, callId: \"%s\" \n",
                           protocolType, callId.data());
               }
#              endif


               int nextTimeout = -1;
               enum SipTransaction::messageRelationship relationship;
               //mSipTransactions.lock();
               SipTransaction* transaction =
                  mSipTransactions.findTransactionFor(*sipMessage,
                                                      TRUE, // timers are only set for outgoing messages I think
                                                      relationship);
               if(transaction)
               {
                   if(timer)
                   {
                      transaction->removeTimer(timer);

                      delete timer;
                      timer = NULL;
                   }

                   // If we are in shutdown mode, unlock the transaction
                   // and set it to null.  We pretend that the transaction
                   // does not exist (i.e. noop).
                   if(mbShuttingDown)
                   {
                       mSipTransactions.markAvailable(*transaction);
                       transaction = NULL;
                   }
               }


               // If we cannot lock it, it does not exist (or atleast
               // pretend it does not exist.  The transaction will be
               // null if it has been deleted or we cannot get a lock
               // on the transaction.  
               if(transaction)
               {
                  SipMessage* delayedDispatchMessage = NULL;
                  transaction->handleResendEvent(*sipMessage,
                                                 *this,
                                                 relationship,
                                                 mSipTransactions,
                                                 nextTimeout,
                                                 delayedDispatchMessage);

                  if(nextTimeout == 0)
                  {
                     if (OsSysLog::willLog(FAC_SIP, PRI_DEBUG))
                     {
                        UtlString transactionString;
                        transaction->toString(transactionString, TRUE);
                        transactionString.insert(0,
                                                 "SipUserAgent::handleMessage "
                                                 "timeout send failed\n"
                                                 );
                        OsSysLog::add(FAC_SIP, PRI_DEBUG, "%s\n", transactionString.data());
                        //osPrintf("%s\n", transactionString.data());
                     }
                  }

                  if(delayedDispatchMessage)
                  {
                     // Only bother processing if the logs are enabled
                     if (    isMessageLoggingEnabled() ||
                         OsSysLog::willLog(FAC_SIP_INCOMING, PRI_DEBUG))
                     {
                        UtlString delayMsgString;
                        int delayMsgLen;
                        delayedDispatchMessage->getBytes(&delayMsgString,
                                                         &delayMsgLen);
                        delayMsgString.insert(0, "SIP User agent delayed dispatch message:\n");
                        delayMsgString.append("++++++++++++++++++++END++++++++++++++++++++\n");
#ifdef TEST_PRINT
                        osPrintf("%s", delayMsgString.data());
#endif
                        logMessage(delayMsgString.data(), delayMsgString.length());
                        OsSysLog::add(FAC_SIP_INCOMING_PARSED, PRI_DEBUG,"%s",
                                      delayMsgString.data());
                     }

                     queueMessageToObservers(delayedDispatchMessage,
                                             SipMessageEvent::APPLICATION
                                             );

                     // delayedDispatchMessage gets freed in queueMessageToObservers
                     delayedDispatchMessage = NULL;
                  }
               }

               // No transaction for this timeout
               else
               {
                  OsSysLog::add(FAC_SIP, PRI_ERR, "SipUserAgent::handleMessage "
                                "SIP message timeout expired with no matching transaction");

                  // Somehow the transaction got deleted perhaps it timed
                  // out and there was a log jam that prevented the handling
                  // of the timeout ????? This should not happen.
               }

               if(transaction)
               {
                  mSipTransactions.markAvailable(*transaction);
               }
               
               // Do this outside so that we do not get blocked
               // on locking or delete the transaction out
               // from under ouselves
               if(nextTimeout == 0)
               {
                  // Make a copy and dispatch it
                  dispatch(new SipMessage(*sipMessage),
                           SipMessageEvent::TRANSPORT_ERROR);
               }

               // The timer made its own copy of this message.
               // It is deleted by dispatch ?? if it is not
               // rescheduled.
            } // End if sipMessage
         } // End SipMessageEvent::TRANSACTION_RESEND

         // Timeout for garbage collection
         else if(msgEventType == SipMessageEvent::TRANSACTION_GARBAGE_COLLECTION)
         {
#ifdef TEST_PRINT
            OsSysLog::add(FAC_SIP, PRI_DEBUG,
                          "SipUserAgent::handleMessage garbage collecting");
            osPrintf("SipUserAgent::handleMessage garbage collecting\n");
#endif
         }

         // Timeout for an transaction to expire
         else if(msgEventType == SipMessageEvent::TRANSACTION_EXPIRATION)
         {
#           ifdef TEST_PRINT
            OsSysLog::add(FAC_SIP, PRI_DEBUG, "SipUserAgent::handleMessage transaction expired");
#           endif
            if(sipMessage)
            {
               // Note: only delete the timer and notifier if there
               // is a message AND we can get a lock on the transaction.  
               //  WARNING: you cannot touch the contents of the transaction
               // attached to the message until the transaction has been
               // locked (via findTransactionFor, if no transaction is 
               // returned, it either no longer exists or we could not get
               // a lock for it.

#ifdef TEST_PRINT
               if(sipMessage->getSipTransaction() == NULL)
               {
                  osPrintf("SipUserAgent::handleMessage expires Timeout message with NULL transaction\n");
               }
#endif
               int nextTimeout = -1;
               enum SipTransaction::messageRelationship relationship;
               //mSipTransactions.lock();
               SipTransaction* transaction =
                  mSipTransactions.findTransactionFor(*sipMessage,
                                                      TRUE, // timers are only set for outgoing?
                                                      relationship);
               if(transaction)
               {
                   if(timer)
                   {
                      transaction->removeTimer(timer);

                      delete timer;
                      timer = NULL;
                   }

                   // If we are in shutdown mode, unlock the transaction
                   // and set it to null.  We pretend that the transaction
                   // does not exist (i.e. noop).
                   if(mbShuttingDown)
                   {
                       mSipTransactions.markAvailable(*transaction);
                       transaction = NULL;
                   }
               }

               if(transaction)
               {
                  SipMessage* delayedDispatchMessage = NULL;
                  transaction->handleExpiresEvent(*sipMessage,
                                                  *this,
                                                  relationship,
                                                  mSipTransactions,
                                                  nextTimeout,
                                                  delayedDispatchMessage);

                  mSipTransactions.markAvailable(*transaction);

                  if(delayedDispatchMessage)
                  {
                     // Only bother processing if the logs are enabled
                     if (    isMessageLoggingEnabled() ||
                         OsSysLog::willLog(FAC_SIP_INCOMING_PARSED, PRI_DEBUG))
                     {
                        UtlString delayMsgString;
                        int delayMsgLen;
                        delayedDispatchMessage->getBytes(&delayMsgString,
                                                         &delayMsgLen);
                        delayMsgString.insert(0, "SIP User agent delayed dispatch message:\n");
                        delayMsgString.append("++++++++++++++++++++END++++++++++++++++++++\n");
#ifdef TEST_PRINT
                        osPrintf("%s", delayMsgString.data());
#endif
                        logMessage(delayMsgString.data(), delayMsgString.length());
                        OsSysLog::add(FAC_SIP_INCOMING_PARSED, PRI_DEBUG,"%s",
                                      delayMsgString.data());
                     }

                     queueMessageToObservers(delayedDispatchMessage,
                                             SipMessageEvent::APPLICATION
                                             );

                     //delayedDispatchMessage gets freed in queueMessageToObservers
                     delayedDispatchMessage = NULL;
                  }
               }
               else // Could not find a transaction for this exired message
               {
                  if (OsSysLog::willLog(FAC_SIP, PRI_DEBUG))
                  {
                     UtlString noTxMsgString;
                     int noTxMsgLen;
                     sipMessage->getBytes(&noTxMsgString, &noTxMsgLen);

                     OsSysLog::add(FAC_SIP, PRI_DEBUG,
                                   "SipUserAgent::handleMessage "
                                   "event timeout with no matching transaction: %s",
                                   noTxMsgString.data());
                  }
               }
            }
         }

         // Unknown timeout
         else
         {
            OsSysLog::add(FAC_SIP, PRI_WARNING,
                          "SipUserAgent::handleMessage unknown timeout event: %d.", msgEventType);
#           ifdef TEST_PRINT
            osPrintf("ERROR: SipUserAgent::handleMessage unknown timeout event: %d.\n",
                     msgEventType);
#           endif
         }

         // As this is OsMsg is attached as a void* to the timeout event
         // it must be explicitly deleted.  The attached SipMessage
         // will get freed with it.
         delete sipEvent;
         sipEvent = NULL;
      } // end if sipEvent
      messageProcessed = TRUE;
   }

   else
   {
#ifdef TEST_PRINT
      osPrintf("SipUserAgent: Unknown message type: %d\n", msgType);
#endif
      messageProcessed = TRUE;
   }

   // Only GC if no messages are waiting -- othewise we may delete a timer 
   // that is queued up for us.
   if (getMessageQueue()->isEmpty())
   {
      garbageCollection();
   }
   return(messageProcessed);
}

void SipUserAgent::garbageCollection()
{
    OsTime time;
    OsDateTime::getCurTimeSinceBoot(time);
    long bootime = time.seconds();

    long then = bootime - (mTransactionStateTimeoutMs / 1000);
    long tcpThen = bootime - mMaxTcpSocketIdleTime;
    long oldTransaction = then - (mTransactionStateTimeoutMs / 1000);
    long oldInviteTransaction = then - mMinInviteTransactionTimeout;

    // If the timeout is negative we never timeout or garbage collect
    // tcp connections
    if(mMaxTcpSocketIdleTime < 0)
    {
        tcpThen = -1;
    }

    if(mLastCleanUpTime < then)
    {
#      ifdef LOG_TIME
       OsSysLog::add(FAC_SIP, PRI_DEBUG,
                     "SipUserAgent::garbageCollection"
                     " bootime: %ld then: %ld tcpThen: %ld"
                     " oldTransaction: %ld oldInviteTransaction: %ld",
                     bootime, then, tcpThen, oldTransaction,
                     oldInviteTransaction);
#      endif
       mSipTransactions.removeOldTransactions(oldTransaction,
                                              oldInviteTransaction);
#      ifdef LOG_TIME
       OsSysLog::add(FAC_SIP, PRI_DEBUG,
                     "SipUserAgent::garbageCollection starting removeOldClients(udp)");
#      endif
       mSipUdpServer->removeOldClients(then);
       if (mSipTcpServer)
       {
#         ifdef LOG_TIME
          OsSysLog::add(FAC_SIP, PRI_DEBUG,
                        "SipUserAgent::garbageCollection starting removeOldClients(tcp)");
#         endif
          mSipTcpServer->removeOldClients(tcpThen);
       }
#if 0 // def SIP_TLS
       if (mSipTlsServer)
       {
          OsSysLog::add(FAC_SIP, PRI_DEBUG,
                        "SipUserAgent::garbageCollection starting removeOldClients(tls)");
          mSipTlsServer->removeOldClients(tcpThen);
       }
#endif
#      ifdef LOG_TIME
       OsSysLog::add(FAC_SIP, PRI_DEBUG,
                     "SipUserAgent::garbageCollection done");
#      endif
       mLastCleanUpTime = bootime;
    }
}

/* ============================ ACCESSORS ================================= */

/*RL*/
void SipUserAgent::setPublicAddress(const char* szAddress, int port) {
	Url contact(this->mContactAddress);
	contact.setHostAddress(szAddress);
	if (port) {
		contact.setHostPort(port);
	}
	this->mContactAddress = contact.toString();
}



// Enable or disable the outbound use of rport (send packet to actual
// port -- not advertised port).
UtlBoolean SipUserAgent::setUseRport(UtlBoolean bEnable)
{
    UtlBoolean bOld = mbUseRport ;

    mbUseRport = bEnable ;

    return bOld ;
}

// Is use report set?
UtlBoolean SipUserAgent::getUseRport() const
{
    return mbUseRport ;
}

void SipUserAgent::setUserAgentName(const UtlString& name)
{
    defaultUserAgentName = name;
    return;
}

const UtlString& SipUserAgent::getUserAgentName() const
{
    return defaultUserAgentName;
}


// Get the manually configured public address
UtlBoolean SipUserAgent::getConfiguredPublicAddress(UtlString* pIpAddress, int* pPort)
{
    UtlBoolean bSuccess = FALSE ;

    if (mConfigPublicAddress.length())
    {
        if (pIpAddress)
        {
            *pIpAddress = mConfigPublicAddress ;
        }

        if (pPort)
        {
            *pPort = mSipUdpServer->getServerPort() ;
        }

        bSuccess = TRUE ;
    }

    return bSuccess ;
}

// Get the local address and port
UtlBoolean SipUserAgent::getLocalAddress(UtlString* pIpAddress, int* pPort)
{
    if (pIpAddress)
    {   
        if (defaultSipAddress.length() > 0)
        {
            *pIpAddress = defaultSipAddress;
        }
        else
        {
            OsSocket::getHostIp(pIpAddress) ;
        }   
    }

    if (pPort)
    {
        *pPort = mSipUdpServer->getServerPort() ;
    }

    return TRUE ;
}


// Get the NAT mapped address and port
UtlBoolean SipUserAgent::getNatMappedAddress(UtlString* pIpAddress, int* pPort)
{
    UtlBoolean bRet(FALSE);
    
    if (mSipUdpServer)
    {
        bRet = mSipUdpServer->getStunAddress(pIpAddress, pPort);
    }
    else if (mSipTcpServer)
    {
        // TODO - a TCP server should also be able to return a stun address
        //bRet = mSipTcpServer->getStunAddress(pIpAddress, pPort);
    }
    return bRet;
}


void SipUserAgent::setIsUserAgent(UtlBoolean isUserAgent)
{
    mIsUaTransactionByDefault = isUserAgent;
}

// setUserAgentHeaderProperty
//      provides a string to be appended to the standard User-Agent
//      header value between "<vendor>/<version>" and the platform (eg "(VxWorks)")
//      Value should be formated either as "token/token" or "(string)"
//      with no leading or trailing space.
void SipUserAgent::setUserAgentHeaderProperty( const char* property )
{
    if ( property )
    {
       mUserAgentHeaderProperties.append(" ");
       mUserAgentHeaderProperties.append( property );
    }
}


void SipUserAgent::setMaxForwards(int maxForwards)
{
    if(maxForwards > 0)
    {
        mMaxForwards = maxForwards;
    }
    else
    {
        OsSysLog::add(FAC_SIP, PRI_DEBUG,"SipUserAgent::setMaxForwards maxForwards <= 0: %d",
            maxForwards);
    }
}

int SipUserAgent::getMaxForwards()
{
    int maxForwards;
    if(mMaxForwards <= 0)
    {
        OsSysLog::add(FAC_SIP, PRI_DEBUG,"SipUserAgent::getMaxForwards maxForwards <= 0: %d",
            mMaxForwards);

        maxForwards = SIP_DEFAULT_MAX_FORWARDS;
    }
    else
    {
        maxForwards = mMaxForwards;
    }

    return(maxForwards);
}

int SipUserAgent::getMaxSrvRecords() const
{
    return(mMaxSrvRecords);
}

void SipUserAgent::setMaxSrvRecords(int maxSrvRecords)
{
    mMaxSrvRecords = maxSrvRecords;
}

int SipUserAgent::getDnsSrvTimeout()
{
    return(mDnsSrvTimeout);
}

void SipUserAgent::setDnsSrvTimeout(int timeout)
{
    mDnsSrvTimeout = timeout;
}

void SipUserAgent::setForking(UtlBoolean enabled)
{
    mForkingEnabled = enabled;
}

void SipUserAgent::getAllowedMethods(UtlString* allowedMethods)
{
   UtlDListIterator iterator(allowedSipMethods);
   allowedMethods->remove(0);
   UtlString* method;

   while ((method = (UtlString*) iterator()))
   {
      if(!method->isNull())
      {
         if(!allowedMethods->isNull())
         {
            allowedMethods->append(", ");
         }
         allowedMethods->append(method->data());
      }
   }
}

void SipUserAgent::getViaInfo(int protocol,
                              UtlString& address,
                              int& port)
{
    if(protocol == OsSocket::TCP)
    {
        port = mTcpPort == SIP_PORT ? PORT_NONE : mTcpPort;
    }
#ifdef SIP_TLS
    else if(protocol == OsSocket::SSL_SOCKET)
    {
        port = mTlsPort == SIP_TLS_PORT ? PORT_NONE : mTlsPort;
    }
#endif
    else
    {
        // Default to UDP and warning if the protocol type is not UDP
        if(protocol != OsSocket::UDP)
        {
            OsSysLog::add(FAC_SIP, PRI_WARNING,
                "SipUserAgent::getViaInfo unknown protocol: %d",
                protocol);
        }

        if(portIsValid(mSipPort))
        {
            port = mSipPort;
        }
        else if(mUdpPort == SIP_PORT)
        {
            port = PORT_NONE;
        }
        else
        {
            port = mUdpPort;
        }
    }

    address = sipIpAddress;
}

void SipUserAgent::getFromAddress(UtlString* address, int* port, UtlString* protocol)
{
   UtlTokenizer tokenizer(registryServers);
   UtlString regServer;

   tokenizer.next(regServer, ",");
   SipMessage::parseAddressFromUri(regServer.data(), address,
                                   port, protocol);

    if(address->isNull())
    {
            protocol->remove(0);
            // TCP only
            if(portIsValid(mTcpPort) && !portIsValid(mUdpPort))
            {
                    protocol->append(SIP_TRANSPORT_TCP);
                    *port = mTcpPort;
            }
            // UDP only
            else if(portIsValid(mUdpPort) && !portIsValid(mTcpPort))
            {
                    protocol->append(SIP_TRANSPORT_UDP);
                    *port = mUdpPort;
            }
            // TCP & UDP on non-standard port
            else if(mTcpPort != SIP_PORT)
            {
                    *port = mTcpPort;
            }
            // TCP & UDP on standard port
            else
            {
                    *port = PORT_NONE;
            }

            // If there is an address configured use it
            NameValueTokenizer::getSubField(defaultSipAddress.data(), 0,
                    ", \t", address);

            // else use the local host ip address
            if(address->isNull())
            {
            address->append(sipIpAddress);
                    //OsSocket::getHostIp(address);
            }
    }
}

void SipUserAgent::getDirectoryServer(int index, UtlString* address,
                                      int* port, UtlString* protocol)
{
        UtlString serverAddress;
        NameValueTokenizer::getSubField(directoryServers.data(), 0,
                SIP_MULTIFIELD_SEPARATOR, &serverAddress);

        address->remove(0);
        *port = PORT_NONE;
        protocol->remove(0);
        SipMessage::parseAddressFromUri(serverAddress.data(),
                address, port, protocol);
        serverAddress.remove(0);
}

void SipUserAgent::getProxyServer(int index, UtlString* address,
                                  int* port, UtlString* protocol)
{
        UtlString serverAddress;
        NameValueTokenizer::getSubField(proxyServers.data(), 0,
                SIP_MULTIFIELD_SEPARATOR, &serverAddress);

        address->remove(0);
        *port = PORT_NONE;
        protocol->remove(0);
        SipMessage::parseAddressFromUri(serverAddress.data(), address, port, protocol);
        serverAddress.remove(0);
}

void SipUserAgent::setProxyServers(const char* sipProxyServers)
{
    if (sipProxyServers)
    {
        proxyServers = sipProxyServers ;
    }
    else
    {
        proxyServers.remove(0) ;
    }
}

int SipUserAgent::getSipStateTransactionTimeout()
{
    return mTransactionStateTimeoutMs;
}

int SipUserAgent::getReliableTransportTimeout()
{
    return(mReliableTransportTimeoutMs);
}

int SipUserAgent::getFirstResendTimeout()
{
    return(mFirstResendTimeoutMs);
}

int SipUserAgent::getLastResendTimeout()
{
    return(mLastResendTimeoutMs);
}

int SipUserAgent::getDefaultExpiresSeconds() const
{
    return(mDefaultExpiresSeconds);
}

void SipUserAgent::setDefaultExpiresSeconds(int expiresSeconds)
{
    if(expiresSeconds > 0 &&
       expiresSeconds <= mMinInviteTransactionTimeout)
    {
        mDefaultExpiresSeconds = expiresSeconds;
    }
    else
    {
        OsSysLog::add(FAC_SIP, PRI_ERR,
                      "SipUserAgent::setDefaultExpiresSeconds "
                      "illegal expiresSeconds value: %d IGNORED",
                      expiresSeconds);
    }
}

int SipUserAgent::getDefaultSerialExpiresSeconds() const
{
    return(mDefaultSerialExpiresSeconds);
}

void SipUserAgent::setDefaultSerialExpiresSeconds(int expiresSeconds)
{
    if(expiresSeconds > 0 &&
       expiresSeconds <= mMinInviteTransactionTimeout)
    {
        mDefaultSerialExpiresSeconds = expiresSeconds;
    }
    else
    {
        OsSysLog::add(FAC_SIP, PRI_ERR, "SipUserAgent::setDefaultSerialExpiresSeconds "
                      "illegal expiresSeconds value: %d IGNORED",
                      expiresSeconds);
    }
}

void SipUserAgent::setMaxTcpSocketIdleTime(int idleTimeSeconds)
{
    if(mMinInviteTransactionTimeout < idleTimeSeconds)
    {
        mMaxTcpSocketIdleTime = idleTimeSeconds;
    }
    else
    {
        OsSysLog::add(FAC_SIP, PRI_ERR, "SipUserAgent::setMaxTcpSocketIdleTime "
                      "idleTimeSeconds: %d less than mMinInviteTransactionTimeout: %d IGNORED",
                      idleTimeSeconds, mMinInviteTransactionTimeout);
    }
}

void SipUserAgent::setHostAliases(UtlString& aliases)
{
    UtlString aliasString;
    int aliasIndex = 0;
    while(NameValueTokenizer::getSubField(aliases.data(), aliasIndex,
                    ", \t", &aliasString))
    {
        Url aliasUrl(aliasString);
        UtlString hostAlias;
        aliasUrl.getHostAddress(hostAlias);
        int port = aliasUrl.getHostPort();

        if(!portIsValid(port))
        {
            hostAlias.append(":5060");
        }
        else
        {
            char portString[20];
            sprintf(portString, ":%d", port);
            hostAlias.append(portString);
        }

        UtlString* newAlias = new UtlString(hostAlias);
        mMyHostAliases.insert(newAlias);
        aliasIndex++;
    }
}

void SipUserAgent::printStatus()

{
    if(mSipUdpServer)
    {
        mSipUdpServer->printStatus();
    }
    if(mSipTcpServer)
    {
        mSipTcpServer->printStatus();
    }
#ifdef SIP_TLS
    if(mSipTlsServer)
    {
        mSipTlsServer->printStatus();
    }
#endif

    UtlString txString;
    mSipTransactions.toString(txString);

    osPrintf("Transactions:\n%s\n", txString.data());
}

void SipUserAgent::startMessageLog(int newMaximumLogSize)
{
    if(newMaximumLogSize > 0) mMaxMessageLogSize = newMaximumLogSize;
    if(newMaximumLogSize == -1) mMaxMessageLogSize = -1;
    mMessageLogEnabled = TRUE;

    {
                OsWriteLock Writelock(mMessageLogWMutex);
                OsReadLock ReadLock(mMessageLogRMutex);
                if(mMaxMessageLogSize > 0)
                        mMessageLog.capacity(mMaxMessageLogSize);
        }
}

void SipUserAgent::stopMessageLog()
{
    mMessageLogEnabled = FALSE;
}

void SipUserAgent::clearMessageLog()
{
        OsWriteLock Writelock(mMessageLogWMutex);
        OsReadLock Readlock(mMessageLogRMutex);
        mMessageLog.remove(0);
}

void SipUserAgent::logMessage(const char* message, int messageLength)
{
    if(mMessageLogEnabled)
    {
#      ifdef TEST_PRINT
       osPrintf("SIP LOGGING ENABLED\n");
#      endif
       {// lock scope
          OsWriteLock Writelock(mMessageLogWMutex);
          // Do not allow the log go grow beyond the maximum
          if(mMaxMessageLogSize > 0 &&
             ((((int)mMessageLog.length()) + messageLength) > mMaxMessageLogSize))
          {
             mMessageLog.remove(0,
                                mMessageLog.length() + messageLength - mMaxMessageLogSize);
          }

          mMessageLog.append(message, messageLength);
       }//lock scope
    }
#ifdef TEST_PRINT
    else osPrintf("SIP LOGGING DISABLED\n");
#endif
}

void SipUserAgent::getMessageLog(UtlString& logData)
{
        OsReadLock Readlock(mMessageLogRMutex);
        logData = mMessageLog;
}

void SipUserAgent::allowExtension(const char* extension)
{
#ifdef TEST_PRINT
    osPrintf("Allowing extension: \"%s\"\n", extension);
#endif
    UtlString* extensionName = new UtlString(extension);
    allowedSipExtensions.append(extensionName);
}

void SipUserAgent::getSupportedExtensions(UtlString& extensionsString)
{
    extensionsString.remove(0);
    UtlString* extensionName = NULL;
    UtlDListIterator iterator(allowedSipExtensions);
    while ((extensionName = (UtlString*) iterator()))
    {
        if(!extensionsString.isNull()) extensionsString.append(", ");
        extensionsString.append(extensionName->data());
    }
}

void SipUserAgent::setRecurseOnlyOne300Contact(UtlBoolean recurseOnlyOne)
{
    mRecurseOnlyOne300Contact = recurseOnlyOne;
}

SipMessage* SipUserAgent::getRequest(const SipMessage& response)
{
    // If the transaction exists and can be locked it
    // is returned.
    enum SipTransaction::messageRelationship relationship;
    SipTransaction* transaction =
        mSipTransactions.findTransactionFor(response,
                                             FALSE, // incoming
                                             relationship);
    SipMessage* request = NULL;

    if(transaction && transaction->getRequest())
    {
        // Make a copy to return
        request = new SipMessage(*(transaction->getRequest()));
    }

    // Need to unlock the transaction
    if(transaction)
        mSipTransactions.markAvailable(*transaction);

    return(request);
}

int SipUserAgent::getTcpPort() const
{
    int iPort = PORT_NONE ;

    if (mSipTcpServer)
    {
        iPort = mSipTcpServer->getServerPort() ;
    }

    return iPort ;
}

int SipUserAgent::getUdpPort() const
{
    int iPort = PORT_NONE ;

    if (mSipUdpServer)
    {
        iPort = mSipUdpServer->getServerPort() ;
    }

    return iPort ;
}

int SipUserAgent::getTlsPort() const
{
    int iPort = PORT_NONE ;

#ifdef SIP_TLS
    if (mSipTlsServer)
    {
        iPort = mSipTlsServer->getServerPort() ;
    }
#endif

    return iPort ;
}


/* ============================ INQUIRY =================================== */

UtlBoolean SipUserAgent::isMethodAllowed(const char* method)
{
        UtlString methodName(method);
        UtlBoolean isAllowed = (allowedSipMethods.occurrencesOf(&methodName) > 0);

        if (!isAllowed)
        {
           /* The method was not explicitly requested, but check for whether the 
            * application has registered for the wildcard.  If so, the method is 
            * allowed, but we do not advertise that fact in the Allow header.*/
           UtlString wildcardMethod;
           
           OsReadLock lock(mObserverMutex);
           isAllowed = mMessageObservers.contains(&wildcardMethod);
        }
        
        return(isAllowed);
}

UtlBoolean SipUserAgent::isExtensionAllowed(const char* extension) const
{
#ifdef TEST_PRINT
    osPrintf("isExtensionAllowed extension: \"%s\"\n", extension);
#endif
    UtlString extensionString;
    if(extension) extensionString.append(extension);
    extensionString.toLower();
        UtlString extensionName(extensionString);
        extensionString.remove(0);
        return(allowedSipExtensions.occurrencesOf(&extensionName) > 0);
}

void SipUserAgent::whichExtensionsNotAllowed(const SipMessage* message,
                                                           UtlString* disallowedExtensions) const
{
        int extensionIndex = 0;
        UtlString extension;

        disallowedExtensions->remove(0);
        while(message->getRequireExtension(extensionIndex, &extension))
        {
                if(!isExtensionAllowed(extension.data()))
                {
                        if(!disallowedExtensions->isNull())
                        {
                                disallowedExtensions->append(SIP_MULTIFIELD_SEPARATOR);
                                disallowedExtensions->append(SIP_SINGLE_SPACE);
                        }
                        disallowedExtensions->append(extension.data());
                }
                extensionIndex++;
        }
        extension.remove(0);
}

UtlBoolean SipUserAgent::isMessageLoggingEnabled()
{
    return(mMessageLogEnabled);
}

UtlBoolean SipUserAgent::isReady()
{
    return(isStarted() && !mPingLock);
}

UtlBoolean SipUserAgent::waitUntilReady()
{
    // Lazy hack, should be a semaphore or event
        int count = 0;
    while(!isReady())
    {
        delay(500);
                count++;
                if ( count > 10)
                {
                        mPingLock = FALSE;
                }
    }
        return(TRUE);
}

UtlBoolean SipUserAgent::isForkingEnabled()
{
    return(mForkingEnabled);
}

UtlBoolean SipUserAgent::isMyHostAlias(Url& route)
{
    UtlString hostAlias;
    route.getHostAddress(hostAlias);
    int port = route.getHostPort();

    if(port == PORT_NONE)
    {
        hostAlias.append(":5060");
    }
    else
    {
        char portString[20];
        sprintf(portString, ":%d", port);
        hostAlias.append(portString);
    }

    UtlString aliasMatch(hostAlias);
    UtlContainable* found = mMyHostAliases.find(&aliasMatch);

    return(found != NULL);
}

UtlBoolean SipUserAgent::recurseOnlyOne300Contact()
{
    return(mRecurseOnlyOne300Contact);
}


UtlBoolean SipUserAgent::isOk(OsSocket::IpProtocolSocketType socketType)
{
    UtlBoolean retval = FALSE;
    switch(socketType)
    {
        case OsSocket::TCP :
            if (mSipTcpServer)
            {
                retval = mSipTcpServer->isOk();
            }
            break;
        case OsSocket::UDP :
            if (mSipUdpServer)
            {
                retval = mSipUdpServer->isOk();
            }
            break;
#ifdef SIP_TLS
        case OsSocket::SSL_SOCKET :
            if (mSipTlsServer)
            {
                retval = mSipTlsServer->isOk();
            }
            break;
#endif
        default :
           OsSysLog::add(FAC_SIP, PRI_ERR, "SipUserAgent::isOK - invalid socket type %d",
                         socketType);
            break;
    }

    return retval;
}

UtlBoolean SipUserAgent::isShutdownDone()
{
    return mbShutdownDone;
}

/* //////////////////////////// PROTECTED ///////////////////////////////// */
UtlBoolean SipUserAgent::shouldAuthenticate(SipMessage* message) const
{
    UtlString method;
    message->getRequestMethod(&method);

    //SDUA - Do not authenticate if a CANCEL or an ACK req/res from other side
    UtlBoolean methodCompare = TRUE ;
    if (   strcmp(method.data(), SIP_ACK_METHOD) == 0
        || strcmp(method.data(), SIP_CANCEL_METHOD) == 0
        )
    {
       methodCompare = FALSE;
    }

    method.remove(0);
    return(   methodCompare
           && (   0 == mAuthenticationScheme.compareTo(HTTP_BASIC_AUTHENTICATION,
                                                       UtlString::ignoreCase
                                                       )
               || 0 == mAuthenticationScheme.compareTo(HTTP_DIGEST_AUTHENTICATION,
                                                       UtlString::ignoreCase
                                                       )
               )
           );
}

UtlBoolean SipUserAgent::authorized(SipMessage* request, const char* uri) const
{
    UtlBoolean allowed = FALSE;
    // Need to create a nonce database for nonce's created
    // for each message (or find the message for the previous
    // sequence number containing the authentication response
    // and nonce for this request)
    const char* nonce = "1234567890"; // :TBD: should be using nonce from the message

    if(mAuthenticationScheme.compareTo("") == 0)
    {
        allowed = TRUE;
    }

    else
    {
        UtlString user;
        UtlString password;

        // Get the user id
        request->getAuthorizationUser(&user);
        // Look up the password
        mpAuthenticationDb->get(user.data(), password);

#ifdef TEST_PRINT
        osPrintf("SipUserAgent::authorized user:%s password found:\"%s\"\n",
            user.data(), password.data());

#endif
        // If basic is set allow basic or digest
        if(mAuthenticationScheme.compareTo(HTTP_BASIC_AUTHENTICATION,
                                           UtlString::ignoreCase
                                           ) == 0
           )
        {
            allowed = request->verifyBasicAuthorization(user.data(),
                password.data());


            // Try Digest if basic failed
            if(! allowed)
            {
#ifdef TEST_PRINT
                osPrintf("SipUserAgent::authorized basic auth. failed\n");
#endif
                allowed = request->verifyMd5Authorization(user.data(),
                                                password.data(),
                                                nonce,
                                                mAuthenticationRealm.data(),
                                                uri);
            }
#ifdef TEST_PRINT
            else
            {
                osPrintf("SipUserAgent::authorized basic auth. passed\n");
            }
#endif
        }

        // If digest is set allow only digest
        else if(mAuthenticationScheme.compareTo(HTTP_DIGEST_AUTHENTICATION,
                                                UtlString::ignoreCase
                                                ) == 0
                )
        {
            allowed = request->verifyMd5Authorization(user.data(),
                                                password.data(),
                                                nonce,
                                                mAuthenticationRealm.data(),
                                                uri);
        }
        user.remove(0);
        password.remove(0);
    }

    return(allowed);
}

void SipUserAgent::addAuthentication(SipMessage* message) const
{
    message->setAuthenticationData(mAuthenticationScheme.data(),
                        mAuthenticationRealm.data(),
                        "1234567890",  // nonce
                        "abcdefghij"); // opaque
}

UtlBoolean SipUserAgent::resendWithAuthorization(SipMessage* response,
                                                 SipMessage* request,
                                                 int* messageType,
                                                 int authorizationEntity)
{
        UtlBoolean requestResent =FALSE;
        int sequenceNum;
        UtlString method;
        response->getCSeqField(&sequenceNum, &method);

    // The transaction sends the ACK for error cases now
        //if(method.compareTo(SIP_INVITE_METHOD , UtlString::ignoreCase) == 0)
        //{
                // Need to send an ACK to finish transaction
        //      SipMessage ackMessage;
        //      ackMessage.setAckData(response, request);
        //      send(ackMessage);
        //}

        SipMessage* authorizedRequest = new SipMessage();

#       ifdef TEST_PRINT
        osPrintf("**************************************\n");
        osPrintf("CREATING message in resendWithAuthorization @ address: %X\n",authorizedRequest);
        osPrintf("**************************************\n");
#       endif

        if ( mpLineMgr && mpLineMgr->buildAuthenticatedRequest(response, request,authorizedRequest))
        {
#          ifdef TEST_PRINT
           osPrintf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
           UtlString authBytes;
           int authBytesLen;
           authorizedRequest->getBytes(&authBytes, &authBytesLen);
           osPrintf("Auth. message:\n%s", authBytes.data());
           osPrintf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
#          endif

           requestResent = send(*authorizedRequest);
           // Send the response back to the application
           // to notify it of the CSeq change for the response
           *messageType = SipMessageEvent::AUTHENTICATION_RETRY;
        }
#       ifdef TEST
        else
        {
           osPrintf("Giving up on entity %d authorization, userId: \"%s\"\n",
                    authorizationEntity, dbUserId.data());
           osPrintf("authorization failed previously sent: %d\n",
                    request->getAuthorizationField(&authField, authorizationEntity));
        }
#       endif

    delete authorizedRequest;

    return(requestResent);
}

void SipUserAgent::lookupSRVSipAddress(UtlString protocol, UtlString& sipAddress, int& port)
{
    OsSocket::IpProtocolSocketType transport = OsSocket::UNKNOWN;

    if (sipIpAddress != "127.0.0.1")
    {
        server_t *server_list;
        server_list = SipSrvLookup::servers(sipAddress.data(),
                                            "sip",
                                            transport,
                                            port);

        // The returned value is a sorted array of server_t with last element having host=NULL.
        // The servers are arranged in order of decreasing preference.
        if ( !server_list )
        {
#ifdef TEST_PRINT
            osPrintf("The DNS server is not SRV capable; \nbind servers v8.0 and above are SRV capable\n");
#endif
        }
        else
        {
            // The result array contains the hostname,
            //   socket type, IP address and port (in network byte order)
            //   DNS preference and weight
            server_t toServerUdp;
            server_t toServerTcp;
            int i;

#ifdef TEST_PRINT
            osPrintf("\n   Pref   Wt   Type    Name(IP):Port\n");
            for (i=0; SipSrvLookup::isValidServerT(server_list[i]); i++)
            {
                UtlString name;
                UtlString ip;
                    SipSrvLookup::getHostNameFromServerT(server_list[i],
                                                        name);
                    SipSrvLookup::getIpAddressFromServerT(server_list[i],
                                                        ip);
                osPrintf( "%6d %5d %5d   %s(%s):%d\n",
                    SipSrvLookup::getPreferenceFromServerT(server_list[i]),
                    SipSrvLookup::getWeightFromServerT(server_list[i]),
                    SipSrvLookup::getProtocolFromServerT(server_list[i]),
                    name.data(),
                    ip.data(),
                    SipSrvLookup::getPortFromServerT(server_list[i]) );
            }
#endif

            for (i=0; server_list[i].isValidServerT(); i++)
            {
                if (server_list[i].getProtocolFromServerT() ==
                    OsSocket::UDP)
                {
                    if (! toServerUdp.isValidServerT())
                    {
                        toServerUdp = server_list[i];
#ifdef TEST_PRINT
                        UtlString name;
                        SipSrvLookup::getHostNameFromServerT(toServerUdp,
                                                            name);
                        osPrintf("UDP server %s\n", name.data());
#endif
                    }
                }
                else if (server_list[i].getProtocolFromServerT() ==
                         OsSocket::TCP)
                {
                    if (toServerTcp.isValidServerT())
                    {
                        toServerTcp = server_list[i];
#ifdef TEST_PRINT
                        UtlString name;
                        SipSrvLookup::getHostNameFromServerT(toServerTcp,
                                                            name);
                        osPrintf("TCP server %s\n", name.data());
#endif
                    }
                }
            }

            if (!protocol.compareTo("TCP") &&
                toServerTcp.isValidServerT())
            {
                int newPort = toServerTcp.getPortFromServerT();
                if (portIsValid(newPort))
                {
                    toServerTcp.getIpAddressFromServerT(sipAddress);
                    port = newPort;
                }
                OsSysLog::add(FAC_SIP, PRI_DEBUG,"SipUserAgent:: found TCP server %s port %d",
                              sipAddress.data(), newPort
                              );
            }
            else if (toServerUdp.isValidServerT())
            {
                int newPort = toServerUdp.getPortFromServerT();
                if (portIsValid(newPort))
                {
                    toServerUdp.getIpAddressFromServerT(sipAddress);
                    port = newPort;
                }
#ifdef TEST_PRINT
                osPrintf("found UDP server %s port %d/%d\n",
                   sipAddress.data(), newPort,
                   SipSrvLookup::getPortFromServerT(toServerUdp));
#endif
            }

            delete[] server_list;
        }
    }
}

void SipUserAgent::setUserAgentHeader(SipMessage& message)
{
   UtlString uaName;
   message.getUserAgentField(&uaName);

   if(uaName.isNull())
   {
      uaName = defaultUserAgentName;

      if ( !mUserAgentHeaderProperties.isNull() )
      {
         uaName.append(mUserAgentHeaderProperties);
      }

      if (mbIncludePlatformInUserAgentName)
      {
         uaName.append(PLATFORM_UA_PARAM);
      }      

      message.setUserAgentField(uaName);
   }
}

void SipUserAgent::setIncludePlatformInUserAgentName(const bool bInclude)
{
    mbIncludePlatformInUserAgentName = bInclude;
}

const bool SipUserAgent::addContactAddress(CONTACT_ADDRESS& contactAddress)
{
    return mContactDb.addContact(contactAddress);
}

void SipUserAgent::getContactAddresses(CONTACT_ADDRESS* pContacts[], int &numContacts)
{
    mContactDb.getAll(pContacts, numContacts);
}

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */

/*RL*/
// Field Watch

void SipUserAgent::addFieldWatch(const char* field) {
	UtlString* fieldStr = new UtlString(field);
	UtlString* valueStr = new UtlString();
	this->watchHeadersMutex.acquireWrite();
	if (!this->watchHeaders.insertKeyAndValue(fieldStr, valueStr)) {
		delete valueStr;
		delete fieldStr;
	}
	this->watchHeadersMutex.releaseWrite();
}

/*RL*/
bool SipUserAgent::getFieldWatch(const char* field, UtlString& value) {
	UtlString* fieldStr = new UtlString(field);
	this->watchHeadersMutex.acquireRead();
	UtlString* valueStr = static_cast<UtlString*>( this->watchHeaders.findValue(fieldStr) );
	delete fieldStr;
	if (valueStr) {
		value = *valueStr;
	}
	this->watchHeadersMutex.releaseRead();
	return valueStr != 0;
}


/*RL*/
void SipUserAgent::checkWatchedFields(SipMessage* message) {
	if (this->watchHeaders.isEmpty()) return;
	this->watchHeadersMutex.acquireRead();
	UtlHashMapIterator it (this->watchHeaders);
	UtlString* key;// = static_cast<UtlString*>( it.key() );
	this->watchHeadersMutex.releaseRead();
	bool gotNew = false;

	SIPX_HEADERWATCH_INFO info;
	info.nSize = 0;

	while ( (key = static_cast<UtlString*>( it() )) ) {
		this->watchHeadersMutex.acquireWrite();
		UtlString* valueStr = static_cast<UtlString*>(it.value());
		const char* value = message->getHeaderValue(0, key->data());
		if (value) {
			if (valueStr->compareTo(value)) {
				if (this->mpLineMgr) {
					/* We fill this up now and once, for optimization... */
					if (info.nSize == 0) {
						Url url;
						UtlString lineId;
						message->getToUrl(url);
						url.getIdentity(lineId);
						lineId = "sip:" + lineId;
						info.nSize = sizeof(SIPX_HEADERWATCH_INFO);
						info.hLine = TapiMgr::getInstance().getLineHandle(lineId.data());
					}
					info.field = key->data();
					info.oldValue = valueStr->data();
					info.newValue = value;
					TapiMgr::getInstance().fireEvent(this, EVENT_CATEGORY_HEADERWATCH, &info);
				}
				// store the value for future compares...
				*valueStr = value;
			}
		}
		this->watchHeadersMutex.releaseWrite();
	}



}
