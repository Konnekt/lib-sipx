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
#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else /* _WIN32 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* _WIN32 */
#include <math.h>

// APPLICATION INCLUDES
#include "tapi/sipXtapi.h"
#include "tapi/sipXtapiEvents.h"
#include "tapi/sipXtapiInternal.h"
#include "tapi/SipXHandleMap.h"
#include "tapi/SipXMessageObserver.h"
//#include "rtcp/RTCManager.h"
#include "net/SipUserAgent.h"
#include "net/SdpCodecFactory.h"
#include "cp/CallManager.h"
#include "mi/CpMediaInterfaceFactory.h"
#include "mi/CpMediaInterfaceFactoryImpl.h"
#include "mi/CpMediaInterfaceFactoryFactory.h"

#include "ptapi/PtProvider.h"
#include "net/Url.h"
#include "net/NameValueTokenizer.h"
#include "os/OsConfigDb.h"
#include "net/SipLineMgr.h"
#include "net/SipRefreshMgr.h"
#include "os/OsLock.h"
#include "os/OsMutex.h"
#include "os/OsSysLog.h"
#include "os/OsTimerTask.h"
#include "os/OsNatAgentTask.h"
#include "net/TapiMgr.h"
#include "net/SipSrvLookup.h"
#include "net/SipSubscribeServer.h"
#include "net/SipSubscribeClient.h"
#include "net/SipDialogMgr.h"
#include "net/SipPublishContentMgr.h"
#include "os/HostAdapterAddress.h"
#include "utl/UtlSList.h"
#include "utl/UtlHashMapIterator.h"

#ifdef VOICE_ENGINE
#include "include/VoiceEngineFactoryImpl.h"
#endif

#include "os/OsBeginThread.h"


// DEFINES
#define MP_SAMPLE_RATE          8000    // Sample rate (don't change)
#define MP_SAMPLES_PER_FRAME    80      // Frames per second (don't change)
#ifdef WIN32
#define strcasecmp stricmp
#endif

// GLOBAL VARIABLES

// EXTERNAL VARIABLES
extern SipXHandleMap* gpCallHandleMap ;   // sipXtapiInternal.cpp
extern SipXHandleMap* gpLineHandleMap ;   // sipXtapiInternal.cpp
extern SipXHandleMap* gpConfHandleMap ;   // sipXtapiInternal.cpp
extern SipXHandleMap* gpInfoHandleMap ;   // sipXtapiInternal.cpp
extern SipXHandleMap* gpPubHandleMap ;    // sipXtapiInternal.cpp
extern SipXHandleMap* gpSubHandleMap ;    // sipXtapiInternal.cpp
extern UtlDList*      gpSessionList ;     // sipXtapiInternal.cpp
// EXTERNAL FUNCTIONS

// STRUCTURES

/* ============================ FUNCTIONS ================================= */

#if defined(_WIN32)

BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

#endif /* defined(_WIN32) */


#define EXCEPTION_PURE_VIRTUAL_CALL 0xE0006001

int _purecall() {
	const char* args [] = {"SipXTAPI"};
	RaiseException(EXCEPTION_PURE_VIRTUAL_CALL, 0, 1, (ULONG_PTR*)args);
	return 0;
}


static void initLogger()
{
    OsSysLog::initialize(0, // do not cache any log messages in memory
                        "sipXtapi"); // name for messages from this program
}

void destroyCallData(SIPX_CALL_DATA* pData)
{
    if (pData != NULL)
    {
        // Increment call count
        pData->pInst->pLock->acquire() ;
        pData->pInst->nCalls-- ;
        assert(pData->pInst->nCalls >= 0) ;
        pData->pInst->pLock->release() ;

        if (pData->callId != NULL)
        {
            delete pData->callId ;
            pData->callId = NULL ;
        }

        if (pData->lineURI != NULL)
        {
            delete pData->lineURI ;
            pData->lineURI = NULL ;
        }

        if (pData->remoteAddress != NULL)
        {
            delete pData->remoteAddress ;
            pData->remoteAddress = NULL ;
        }

        if (pData->pMutex != NULL)
        {
            delete pData->pMutex ;
            pData->pMutex = NULL ;
        }
        
        if (pData->ghostCallId != NULL)
        {
            delete pData->ghostCallId;
            pData->ghostCallId = NULL;
        }

        if (pData->sessionCallId != NULL)
        {
            delete pData->sessionCallId ;
            pData->sessionCallId = NULL ;
        }

        delete pData ;
    }
}

static void destroyLineData(SIPX_LINE_DATA* pData)
{
    if (pData != NULL)
    {
        if (pData->lineURI != NULL)
        {
            delete pData->pMutex ;
            delete pData->lineURI ;
        }

        delete pData ;
    }
}

static SIPX_LINE_DATA* createLineData(SIPX_INSTANCE_DATA* pInst, const Url& uri)
{
    SIPX_LINE_DATA* pData = new SIPX_LINE_DATA ;
    memset ((void*) pData, 0, sizeof(SIPX_LINE_DATA));

    if (pData != NULL)
    {
        pData->lineURI = new Url(uri) ;
        pData->pInst = pInst ;
        pData->pMutex = new OsRWMutex(OsRWMutex::Q_FIFO) ;
        if ((pData->lineURI == NULL) || (pData->pMutex == NULL))
        {
            destroyLineData(pData) ;
            pData = NULL ;
        }
        else
        {
            pInst->pLock->acquire() ;
            pInst->nLines++ ;
            pInst->pLock->release() ;
        }
    }

    return pData ;
}



/****************************************************************************
 * Initialization
 ***************************************************************************/

static void initAudioDevices(SIPX_INSTANCE_DATA* pInst)
{
    // Clear devices
    for (int i=0; i<MAX_AUDIO_DEVICES; i++)
    {
        pInst->inputAudioDevices[i] = NULL ;
        pInst->outputAudioDevices[i] = NULL ;
    }

#if defined(WIN32)
    WAVEOUTCAPS outcaps ;
    WAVEINCAPS  incaps ;
    int numDevices ;
    int i ;

    numDevices = waveInGetNumDevs();
    for (i=0; i<numDevices && i<MAX_AUDIO_DEVICES; i++)
    {
        waveInGetDevCaps(i, &incaps, sizeof(WAVEINCAPS)) ;
        pInst->inputAudioDevices[i] = strdup(incaps.szPname) ;
    }

    numDevices = waveOutGetNumDevs();
    for (i=0; i<numDevices && i<MAX_AUDIO_DEVICES; i++)
    {
        waveOutGetDevCaps(i, &outcaps, sizeof(WAVEOUTCAPS)) ;
        pInst->outputAudioDevices[i] = strdup(outcaps.szPname) ;
    }
#else
    pInst->inputAudioDevices[0] = strdup("Default") ;
    pInst->outputAudioDevices[0] = strdup("Default") ;
#endif
}


SIPXTAPI_API SIPX_RESULT sipxInitialize(SIPX_INST*  phInst,
                                        const int   tcpPort,
                                        const int   udpPort,
                                        const int   tlsPort,
                                        const int   rtpPortStart,
                                        const int   maxConnections,
                                        const char* szIdentity,
                                        const char* szBindToAddr,
                                        bool        bUseSequentialPorts,
                                        const char* szTLSCertificateNickname,
                                        const char* szTLSCertificatePassword,
                                        const char* szDbLocation)
{
    char cVersion[80] ;
    sipxConfigGetVersion(cVersion, sizeof(cVersion)) ;
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO, cVersion) ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxInitialize tcpPort=%d udpPort=%d tlsPort=%d rtpPortStart=%d"
            " maxConnections=%d identity=%s bindTo=%s sequentialPorts=%d"
            " certNickname=%s, DBLocation=%s",
            tcpPort, udpPort, tlsPort, rtpPortStart, maxConnections,
            ((szIdentity != NULL) ? szIdentity : ""),
            ((szBindToAddr != NULL) ? szBindToAddr : ""),
            bUseSequentialPorts,
            ((szTLSCertificateNickname != NULL) ? szTLSCertificateNickname : ""),
            ((szDbLocation != NULL) ? szDbLocation : "")) ;

    sipxStructureIntegrityCheck();

    // Disable Console by default
    enableConsoleOutput(false) ;

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

#ifdef SIPXTAPI_EVAL_EXPIRATION
    OsDateTime expireDate(EVAL_EXPIRE_YEAR, EVAL_EXPIRE_MONTH, EVAL_EXPIRE_DAY, 23, 59, 59, 0) ;
    OsDateTime nowDate ;         
    OsTime expireTime ;
    OsTime nowTime ;

    OsDateTime::getCurTime(nowDate) ;   
    expireDate.cvtToTimeSinceEpoch(expireTime) ;
    nowDate.cvtToTimeSinceEpoch(nowTime) ;

    if (nowTime > expireTime)
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR, "sipXtapi has expired") ;
        return SIPX_RESULT_EVAL_TIMEOUT ;
    }
#endif

    // check for the security runtime, if it is not there,
    // and a TLS port has been specified,
    // return with failure
#ifdef HAVE_NSS
    if (tlsPort > 0)
    {
        rc = sipxConfigLoadSecurityRuntime();
        if (SIPX_RESULT_SUCCESS != rc)
        {
            return rc;  // return right away 
        }
    }
#else
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO, "sipXtapi built without NSS support") ;
#endif

    // set the sipXtackLib's tapi callback function pointers
    TapiMgr::getInstance().setTapiCallCallback(&sipxFireCallEvent);
    TapiMgr::getInstance().setTapiMediaCallback(&sipxFireMediaEvent);
    TapiMgr::getInstance().setTapiLineCallback(&sipxFireLineEvent);
    TapiMgr::getInstance().setTapiCallback(&sipxFireEvent);

    SIPX_INSTANCE_DATA* pInst = new SIPX_INSTANCE_DATA;
    memset(pInst, 0, sizeof(SIPX_INSTANCE_DATA)) ;

    // Create Line and Refersh Manager
    pInst->pLineManager = new SipLineMgr() ;
    pInst->pRefreshManager = new SipRefreshMgr() ;
    pInst->pRefreshManager->setLineMgr(pInst->pLineManager);

    // Init counts
    pInst->pLock = new OsMutex(OsMutex::Q_FIFO) ;
    pInst->nCalls = 0 ;
    pInst->nLines = 0 ;
    pInst->nConferences = 0 ; 

    // Bind the SIP user agent to a port and start it up
    pInst->pSipUserAgent = new SipUserAgent(
            tcpPort,                    // sipTcpPort
            udpPort,                    // sipUdpPort
            tlsPort,                    // sipTlsPort
            NULL,                       // publicAddress
            NULL,                       // defaultUser
            szBindToAddr,               // default IP Address
            NULL,                       // sipProxyServers
            NULL,                       // sipDirectoryServers
            NULL,                       // sipRegistryServers
            NULL,                       // authenticationScheme
            NULL,                       // authenicateRealm
            NULL,                       // authenticateDb
            NULL,                       // authorizeUserIds
            NULL,                       // authorizePasswords
            NULL,                       // natPingUrl
            0,                          // natPingFrequency
            "PING",                     // natPingMethod
            pInst->pLineManager,        // lineMgr
            SIP_DEFAULT_RTT,            // sipFirstResendTimeout
            TRUE,                       // defaultToUaTransactions
            -1,                         // readBufferSize
            OsServerTask::DEF_MAX_MSGS, // queueSize
            bUseSequentialPorts,        // bUseNextAvailablePort
            szTLSCertificateNickname,
            szTLSCertificatePassword,
            szDbLocation);
    pInst->pSipUserAgent->allowMethod(SIP_INFO_METHOD);

    UtlString defaultBindAddressString;
  	int unused;
    pInst->pSipUserAgent->getLocalAddress(&defaultBindAddressString, &unused, OsSocket::UDP);
  	unsigned long defaultBindAddress = inet_addr(defaultBindAddressString.data());
  	OsSocket::setDefaultBindAddress(defaultBindAddress);
    pInst->pSipUserAgent->start();    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO, "Default bind address %s, udpPort=%d, tcpPort=%d, tlsPort=%d",
            defaultBindAddressString.data(),
            pInst->pSipUserAgent->getUdpPort(),
            pInst->pSipUserAgent->getTcpPort(),
            pInst->pSipUserAgent->getTlsPort()) ;

    // Startup Line Manager  Refresh Manager
    pInst->pLineManager->initializeRefreshMgr(pInst->pRefreshManager) ;
    pInst->pRefreshManager->init(pInst->pSipUserAgent, pInst->pSipUserAgent->getTcpPort(), pInst->pSipUserAgent->getUdpPort()) ;
    pInst->pRefreshManager->StartRefreshMgr();

    // Create and start up a SIP SUBSCRIBE server
    pInst->pSubscribeServer = 
        SipSubscribeServer::buildBasicServer(*pInst->pSipUserAgent);
    pInst->pSubscribeServer->start();

    // Create and start up a SIP SUBSCRIBE client
    pInst->pDialogManager = new SipDialogMgr;
    pInst->pSipRefreshManager = 
        new SipRefreshManager(*pInst->pSipUserAgent,
                                *pInst->pDialogManager);
    pInst->pSipRefreshManager->start();
    pInst->pSubscribeClient = 
        new SipSubscribeClient(*pInst->pSipUserAgent,
                                *pInst->pDialogManager, 
                                *pInst->pSipRefreshManager);
    pInst->pSubscribeClient->start();

    // Enable PCMU, PCMA, Tones/RFC2833 codecs
    pInst->pCodecFactory = new SdpCodecFactory() ;

    // Instantiate the call processing subsystem
    UtlString localAddress;
    UtlString utlIdentity(szIdentity);
    if (!utlIdentity.contains("@"))
    {
        OsSocket::getHostIp(&localAddress);
        char *szBuf = (char*) calloc(64 + utlIdentity.length(), 1) ;
        sprintf(szBuf, "sip:%s@%s:%d", szIdentity, localAddress.data(), pInst->pSipUserAgent->getUdpPort()) ;
        localAddress = szBuf ;
        free(szBuf) ;
    }
    else
    {
        localAddress = utlIdentity;
    }
    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO, "Default Identity: %s\n",
            localAddress.data()) ;

    pInst->pCallManager = new CallManager(FALSE,
                            pInst->pLineManager,
                            TRUE, // early media in 180 ringing
                            pInst->pCodecFactory,
                            rtpPortStart, // rtp start
                            rtpPortStart + (2*maxConnections), // rtp end
                            localAddress.data(),
                            localAddress.data(),
                            pInst->pSipUserAgent,
                            0, // sipSessionReinviteTimer
                            NULL, // mgcpStackTask
                            NULL, // defaultCallExtension
                            Connection::RING, // availableBehavior
                            NULL, // unconditionalForwardUrl
                            -1, // forwardOnNoAnswerSeconds
                            NULL, // forwardOnNoAnswerUrl
                            Connection::BUSY, // busyBehavior
                            NULL, // sipForwardOnBusyUrl
                            NULL, // speedNums
                            CallManager::SIP_CALL, // phonesetOutgoingCallProtocol
                            4, // numDialPlanDigits
                            CallManager::NEAR_END_HOLD, // holdType
                            5000, // offeringDelay
                            "",
                            CP_MAXIMUM_RINGING_EXPIRE_SECONDS,
                            QOS_LAYER3_LOW_DELAY_IP_TOS,
                            10,
                            sipXmediaFactoryFactory(NULL));


    // Start up the call processing system
    pInst->pCallManager->setOutboundLine(localAddress) ;
    pInst->pCallManager->start();

    CpMediaInterfaceFactoryImpl* pInterface = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
    if (pInterface == NULL)
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR, "Unable to create global media interface") ;
    }

    sipxConfigSetAudioCodecPreferences(pInst, AUDIO_CODEC_BW_NORMAL);
#ifdef VIDEO    
    sipxConfigSetVideoBandwidth(pInst, VIDEO_CODEC_BW_NORMAL);
#endif    

    initAudioDevices(pInst) ;

#ifdef VOICE_ENGINE
    sipxCreateLocalAudioConnection(pInst) ;
#endif

    *phInst = pInst ;
    gpSessionList->insert(new UtlVoidPtr(pInst)) ;
    sipxIncSessionCount();
    
#ifdef _WIN32
#ifndef VOICE_ENGINE
    Sleep(500) ;    // Need to wait for UA and MP to startup
                    // TODO: Need to synchronize startup
#endif
#endif

    // create the message observer
    pInst->pMessageObserver = new SipXMessageObserver(pInst);
    pInst->pMessageObserver->start();
    pInst->pSipUserAgent->addMessageObserver(*(pInst->pMessageObserver->getMessageQueue()), SIP_INFO_METHOD, 1, 0, 1, 0, 0, 0, (void*)pInst);

    // Enable ICE by default (only makes sense with STUN, TURN or with mulitple nics 
    // (multiple nic support still needs work).
    //sipxConfigEnableIce(pInst) ;

    pInst->bAllowHeader = true;
    pInst->bDateHeader = true;
    //sipxConfigEnableSipShortNames(pInst, true);

    rc = SIPX_RESULT_SUCCESS ;
    //  check for TLS initialization
#ifdef SIP_TLS
    if (pInst->pSipUserAgent->getTlsServer() && tlsPort > 0 && szTLSCertificateNickname != NULL)
    {
        OsStatus initStatus = pInst->pSipUserAgent->getTlsServer()->getTlsInitCode();
        if (initStatus != OS_SUCCESS)
        {
            rc = SIPX_RESULT_FAILURE;
        }
        else
        {
            sipxConfigSetSecurityParameters((SIPX_INST)pInst,
                                            szTLSCertificateNickname,
                                            szTLSCertificatePassword,
                                            szDbLocation);
        }
    }   
#endif

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxReInitialize(SIPX_INST*  phInst,
                                          const int   tcpPort,
                                          const int   udpPort,
                                          const int   tlsPort,
                                          const int   rtpPortStart,
                                          const int   maxConnections,
                                          const char* szIdentity,
                                          const char* szBindToAddr,
                                          bool        bUseSequentialPorts,
                                          const char* szTLSCertificateNickname,
                                          const char* szTLSCertificatePassword,
                                          const char* szDbLocation)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxReInitialize hInst=%p",
            *phInst);
            
    if (phInst)
    {
        SIPX_INST hOldInst = *phInst ;
        SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) *phInst ;

        if (pInst)
        {
            sipxSubscribeDestroyAll(*phInst) ;
            sipxPublisherDestroyAll(*phInst) ;        
            sipxCallDestroyAll(*phInst) ;
            sipxConferenceDestroyAll(*phInst) ;
            sipxLineRemoveAll(*phInst) ;

            sipxDisableListeners() ;
            sipxUnInitialize(*phInst, true);
            sipxEnableListeners() ;
        }

        rc = sipxInitialize(phInst,
                       tcpPort,
                       udpPort,
                       tlsPort, 
                       rtpPortStart,
                       maxConnections,
                       szIdentity,
                       szBindToAddr,
                       bUseSequentialPorts,
                       szTLSCertificateNickname,
                       szTLSCertificatePassword,
                       szDbLocation);

        if (hOldInst && *phInst)
        {
            sipxUpdateListeners(hOldInst, *phInst) ;
        }
    }

    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxUnInitialize(SIPX_INST hInst,
                                          bool      bForceShutdown)
{
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxUnInitialize hInst=%p",
        hInst);
        
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        // Verify that all calls are torn down and that no lines
        // are present.

        int iAttempts = 0 ;
        int nCalls ;
        int nConferences ;
        int nLines ;
        int nCallManagerCalls ;
        UtlString checkCalls[4] ;

        do 
        {
            pInst->pLock->acquire() ;
            nCalls = pInst->nCalls ;
            nConferences = pInst->nConferences ;
            nLines = pInst->nLines ;
            nCallManagerCalls = 0 ;
            sipxGetActiveCallIds(hInst, 4, nCallManagerCalls, checkCalls)  ;

            pInst->pLock->release() ;

            if ((nCalls != 0) || (nConferences != 0) || (nLines != 0) || (nCallManagerCalls != 0))
            {
                OsTask::delay(250) ;
                iAttempts++ ;

                OsSysLog::add(FAC_SIPXTAPI, PRI_WARNING,
                "Busy SIPX_INST (Waiting) (%p) nCalls=%d, nLines=%d, nConferences=%d nCallManagerCalls=%d",
                        hInst, nCalls, nLines, nConferences, nCallManagerCalls) ;
            }
            else
            {
                break ;
            }
        } while (iAttempts < 20) ;

        if ( bForceShutdown || (nCalls == 0) && (nConferences == 0) && 
                (nLines == 0) && (nCallManagerCalls == 0) )
        {
#ifdef VOICE_ENGINE
            sipxDestroyLocalAudioConnection(pInst) ;
#endif            
            
            // First: Shutdown user agent to avoid processing during teardown
            pInst->pSipUserAgent->shutdown(TRUE) ;

            // get rid of pointer to the line manager in the refresh manager
            pInst->pRefreshManager->setLineMgr(NULL);
            pInst->pLineManager->requestShutdown();
            pInst->pCallManager->requestShutdown();
            pInst->pRefreshManager->requestShutdown();
            pInst->pSipUserAgent->requestShutdown();
            pInst->pSubscribeClient->requestShutdown();
            pInst->pSubscribeServer->requestShutdown();
            pInst->pSipRefreshManager->requestShutdown();
            pInst->pMessageObserver->requestShutdown();
            pInst->pCodecFactory->clearCodecs();

            delete pInst->pSubscribeClient ;
            delete pInst->pSubscribeServer ;
            delete pInst->pRefreshManager ;
            delete pInst->pSipRefreshManager ;
            delete pInst->pDialogManager ;
            delete pInst->pLineManager;            
            delete pInst->pCallManager; // Deletes SipUserAgent
            delete pInst->pCodecFactory;

            // Destroy the timer task to flush timers
            OsTimerTask::destroyTimer() ;
            pInst->pCallManager = NULL;

            sipxDecSessionCount();
            if (sipxGetSessionCount() == 0)
            {
                OsNatAgentTask::releaseInstance();
            }
            sipxDestroyMediaFactoryFactory() ;

            int codecIndex;
            // Did we previously allocate an audio codecs array and store it in our codec settings?
            if (pInst->audioCodecSetting.bInitialized)
            {
                // Free up the previuosly allocated codecs and the array
                for (codecIndex = 0; codecIndex < pInst->audioCodecSetting.numCodecs; codecIndex++)
                {
                    if (pInst->audioCodecSetting.sdpCodecArray[codecIndex])
                    {
                        delete pInst->audioCodecSetting.sdpCodecArray[codecIndex];
                        pInst->audioCodecSetting.sdpCodecArray[codecIndex] = NULL;
                    }
                }
                delete[] pInst->audioCodecSetting.sdpCodecArray;
                pInst->audioCodecSetting.sdpCodecArray = NULL;
                pInst->audioCodecSetting.bInitialized = false;
            }
            // Did we previously allocate a video codecs array and store it in our codec settings?
            if (pInst->videoCodecSetting.bInitialized)
            {
                // Free up the previuosly allocated codecs and the array
                for (codecIndex = 0; codecIndex < pInst->videoCodecSetting.numCodecs; codecIndex++)
                {
                    if (pInst->videoCodecSetting.sdpCodecArray[codecIndex])
                    {
                        delete pInst->videoCodecSetting.sdpCodecArray[codecIndex];
                        pInst->videoCodecSetting.sdpCodecArray[codecIndex] = NULL;
                    }
                }
                delete[] pInst->videoCodecSetting.sdpCodecArray;
                pInst->videoCodecSetting.sdpCodecArray = NULL;
                pInst->videoCodecSetting.bInitialized = false;
            }

            for (int i=0; i<MAX_AUDIO_DEVICES; i++)
            {
                if (pInst->inputAudioDevices[i])
                {
                    free(pInst->inputAudioDevices[i]) ;
                    pInst->inputAudioDevices[i] = NULL ;
                }

                if (pInst->outputAudioDevices[i])
                {                    
                    free(pInst->outputAudioDevices[i]) ;
                    pInst->outputAudioDevices[i] = NULL ;
                }
            }
        
            UtlVoidPtr key(pInst) ;
            gpSessionList->destroy(&key) ;

            if (pInst->pStunNotification != NULL)
            {
                delete pInst->pStunNotification ;
                pInst->pStunNotification = NULL ;
            }

            if (pInst->pMessageObserver)
            {
                delete pInst->pMessageObserver ;
                pInst->pMessageObserver = NULL ;
            }

            delete pInst->pLock ;

            delete pInst;
            pInst = NULL;

            // Destroy the timer task once more -- some of the destructors (SipUserAgent)
            // mistakenly re-creates them when terminating.
            OsTimerTask::destroyTimer() ;

            rc = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                "Unable to shutdown busy SIPX_INST (%p) nCalls=%d, nLines=%d, nConferences=%d",
                        hInst, nCalls, nLines, nConferences) ;
            rc = SIPX_RESULT_BUSY ;
        }
    }
    else
    {
        // sipxDestroyMediaFactoryFactory is now being called regardless of the return code
        // failure to do so could cause a hang, at least it does using the VoiceEngine media adapter
        sipxDestroyMediaFactoryFactory() ;
    }

    return rc ;
}
/****************************************************************************
 * Call Related Functions
 ***************************************************************************/

SIPXTAPI_API SIPX_RESULT sipxCallAccept(const SIPX_CALL   hCall,
                                        SIPX_VIDEO_DISPLAY* const pDisplay,
                                        SIPX_SECURITY_ATTRIBUTES* const pSecurity,
                                        SIPX_CALL_OPTIONS* options)
{
    bool bEnableLocationHeader=false;
    int bandWidth=AUDIO_CODEC_BW_DEFAULT;

    if (options != NULL)
    {
        if (options->cbSize == sizeof(SIPX_CALL_OPTIONS))
        {
            bEnableLocationHeader = options->sendLocation;
            bandWidth = options->bandwidthId;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                "sipxCallAccept - cbSize of SIPX_CALL_OPTIONS does not match size of structure, ignoring options");
        }
    }

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAccept hCall=%d display=%p bEnableLocationHeader=%d bandWidth=%d",
        hCall, pDisplay, bEnableLocationHeader, bandWidth);
       
    if (pSecurity)
    {
        SIPX_RESULT rc = sipxConfigLoadSecurityRuntime();
        if (SIPX_RESULT_SUCCESS != rc)
        {
            return rc;  // return right away 
        }
    }

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;
    char* pLocationHeader = NULL;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        if (pInst && pSecurity)
        {
            // augment the security attributes with the instance's security parameters
            SecurityHelper securityHelper;
            // don't generate a random key, and remove one if one it is there
            // (the caller will provide the agreed upon key)
            pSecurity->setSrtpKey("", 30);
            if (pInst->dbLocation)
            {
                securityHelper.setDbLocation(*pSecurity, pInst->dbLocation);
            }
            if (pInst->myCertNickname)
            {
                securityHelper.setMyCertNickname(*pSecurity, pInst->myCertNickname);
            }
            if (pInst->dbPassword)
            {
                securityHelper.setDbPassword(*pSecurity, pInst->dbPassword);
            }
        }
        assert(remoteAddress.length()) ;
        if (remoteAddress.length())
        {
            // set the display object
            {
                SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
                if (pCallData)
                {
                    if (pDisplay)
                    {
                        pCallData->display = *pDisplay;   
                    }
                    if (pSecurity)
                    {
                        pCallData->security = *pSecurity;
                    }
                    sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
                }
            }
            // Only take focus if something doesn't already have it.
            if (!sipxIsCallInFocus())
            {
                SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE) ;
                if (pCallData)
                {
                    pCallData->bInFocus = true ;                    
                    sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
                }
                pInst->pCallManager->unholdLocalTerminalConnection(callId.data()) ;
            }
            if (pInst)
            {
                pLocationHeader = (bEnableLocationHeader) ? pInst->szLocationHeader : NULL;
            }
            if (pSecurity && pSecurity->getSecurityLevel() > 0)
            {
                SIPX_CALL_DATA* pCallData = sipxCallLookup(hCall, SIPX_LOCK_READ);
                if (pCallData)
                {
                    pInst->pCallManager->acceptConnection(callId.data(),
                            remoteAddress.data(),
                            AUTO, 
                            (void*)&pCallData->display,
                            (void*)&pCallData->security,
                            pLocationHeader,
                            bandWidth) ;
                            
                    sipxCallReleaseLock(pCallData, SIPX_LOCK_READ);
                }
            }
            else
            {
                SIPX_CALL_DATA* pCallData = sipxCallLookup(hCall, SIPX_LOCK_READ);
                if (pCallData)
                {
                    pInst->pCallManager->acceptConnection(callId.data(),
                            remoteAddress.data(),
                            AUTO, 
                            (void*)&pCallData->display,
                            NULL,
                            pLocationHeader,
                            bandWidth);
                }
                sipxCallReleaseLock(pCallData, SIPX_LOCK_READ);
            }
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallReject(const SIPX_CALL hCall,
                                        const int errorCode,
                                        const char* szErrorText)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallReject hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        assert(remoteAddress.length()) ;
        if (remoteAddress.length())
        {
            pInst->pCallManager->rejectConnection(callId.data(), remoteAddress.data()) ;
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallRedirect(const SIPX_CALL hCall, const char* szForwardURL)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallRedirect hCall=%d forwardURL=%s",
        hCall, szForwardURL);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        assert(remoteAddress.length()) ;
        if (remoteAddress.length() && szForwardURL)
        {
            pInst->pCallManager->redirectConnection(callId.data(), remoteAddress.data(), szForwardURL) ;

        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallAnswer(const SIPX_CALL hCall, bool bTakeFocus)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAnswer hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        assert(remoteAddress.length()) ;
        if (remoteAddress.length())
        {
            SIPX_VIDEO_DISPLAY display;
            SIPX_SECURITY_ATTRIBUTES security;

            if (!sipxIsCallInFocus() || bTakeFocus)
            {
                SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE) ;
                if (pCallData)
                {
                    pCallData->bInFocus = true ;                    
                    sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
                }

                // The hold even will remove the bInFocus param from 
                // the other connection
                pInst->pCallManager->unholdLocalTerminalConnection(callId.data()) ;
            }

            SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
            if (pCallData)
            {
                display = pCallData->display;
                security = pCallData->security;
                
                sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
            }

            if (display.handle)
            {
                if (security.getSecurityLevel() == SRTP_LEVEL_NONE)
                {
                    pInst->pCallManager->answerTerminalConnection(callId.data(), remoteAddress.data(), "unused", &display) ;
                }
                else
                {
                    pInst->pCallManager->answerTerminalConnection(callId.data(), remoteAddress.data(), "unused", &display, &pCallData->security) ;
                }
            }
            else
            {
                if (security.getSecurityLevel() == SRTP_LEVEL_NONE)
                {
                    pInst->pCallManager->answerTerminalConnection(callId.data(), remoteAddress.data(), "unused") ;
                }
                else
                {
                    pInst->pCallManager->answerTerminalConnection(callId.data(), remoteAddress.data(), "unused", NULL, &security) ;
                }
            }
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


static SIPX_RESULT sipxCallCreateHelper(const SIPX_INST hInst,
                                        const SIPX_LINE hLine,
                                        const SIPX_CONF hConf,
                                        SIPX_CALL*  phCall)
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_LINE_DATA* pLine = sipxLineLookup(hLine, SIPX_LOCK_READ) ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(phCall != NULL) ;
    assert(pLine != NULL) ;
    assert(pLine->lineURI != NULL) ;

    if (pInst)
    {
        if (phCall && pLine)
        {
            SIPX_CALL_DATA* pData = new SIPX_CALL_DATA ;
            memset((void*)pData, 0, sizeof(SIPX_CALL_DATA));

            if (pData)
            {
                UtlString callId ;

                pInst->pCallManager->createCall(&callId) ;

                // Set Call ID
                pData->callId = new UtlString(callId) ;
                assert(pData->callId != NULL) ;

                // Set Conf handle
                pData->hConf = hConf ;

                // Set Line URI
                pData->hLine = hLine ;
                pData->lineURI = new UtlString(pLine->lineURI->toString().data()) ;
                assert(pData->lineURI != NULL) ;

                // Remote Address
                pData->remoteAddress = NULL ;

                // Connection Id, initialize to -1, cache later
                pData->connectionId = -1;

                // Store Instance
                pData->pInst = pInst ;

                // Create Mutex
                pData->pMutex = new OsRWMutex(OsRWMutex::Q_FIFO) ;

                // Increment call count
                pInst->pLock->acquire() ;
                pInst->nCalls++ ;
                pInst->pLock->release() ;

                if ((pData->callId == NULL) || (pData->lineURI == NULL))
                {
                    *phCall = SIPX_CALL_NULL ;
                    destroyCallData(pData) ;
                    sr = SIPX_RESULT_OUT_OF_MEMORY ;
                }
                else
                {
                    *phCall = gpCallHandleMap->allocHandle(pData) ;
                    assert(*phCall != 0) ;
                    sr = SIPX_RESULT_SUCCESS ;
                }
            }
            else
            {
                sr = SIPX_RESULT_OUT_OF_MEMORY ;
                destroyCallData(pData) ;
                *phCall = SIPX_CALL_NULL ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    sipxLineReleaseLock(pLine, SIPX_LOCK_READ) ;

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxCallCreate(const SIPX_INST hInst,
                                        const SIPX_LINE hLine,
                                        SIPX_CALL*  phCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallCreate hInst=%p hLine=%d phCall=%p",
        hInst, hLine, phCall);
        
    SIPX_RESULT rc = sipxCallCreateHelper(hInst, hLine, SIPX_CONF_NULL, phCall) ;
    if (rc == SIPX_RESULT_SUCCESS)
    {
        SIPX_CALL_DATA* pData = sipxCallLookup(*phCall, SIPX_LOCK_READ) ;
        SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;
        UtlString callId = *pData->callId ;
        sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

        // Notify Listeners
        SipSession session ;
        sipxFireCallEvent(pInst->pCallManager, 
                callId.data(), 
                &session, 
                NULL, 
                CALLSTATE_DIALTONE, 
                CALLSTATE_CAUSE_NORMAL) ;
    }
    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxCallConnect(SIPX_CALL hCall,
                                         const char* szAddress,
                                         SIPX_CONTACT_ID contactId,
                                         SIPX_VIDEO_DISPLAY* const pDisplay,
                                         SIPX_SECURITY_ATTRIBUTES* const pSecurity,
                                         bool bTakeFocus,
                                         SIPX_CALL_OPTIONS* options,
                                         const char* szCallId)
{
    bool bEnableLocationHeader=false;
    int bandWidth=AUDIO_CODEC_BW_DEFAULT;

    if (options != NULL)
    {
        if (options->cbSize == sizeof(SIPX_CALL_OPTIONS))
        {
            bEnableLocationHeader = options->sendLocation;
            bandWidth = options->bandwidthId;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                "sipxCallAccept - cbSize of SIPX_CALL_OPTIONS does not match size of structure, ignoring options");
        }
    }

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallConnect hCall=%d szAddress=%s contactId=%d bEnableLocationHeader=%d bandWidth=%d",
        hCall, szAddress, contactId, bEnableLocationHeader, bandWidth);

    // check for and load security runtime .dlls
    if (pSecurity)
    {
        SIPX_RESULT rc = sipxConfigLoadSecurityRuntime();
        if (SIPX_RESULT_SUCCESS != rc)
        {
            return rc;  // return right away 
        }
    }
#ifdef VIDEO
    if (pDisplay)
    {
        assert(pDisplay->handle);
    }
#endif
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;

    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;
    UtlString lineId ;
    bool bSetFocus = false ;
    char* pLocationHeader = NULL;

    assert(szAddress != NULL) ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, &lineId))
    {
        CONTACT_ADDRESS* pContact = NULL;
        CONTACT_TYPE contactType;

        if (pInst && pSecurity)
        {
            // augment the security attributes with the instance's security parameters
            SecurityHelper securityHelper;
            // generate a random key, if one isn't there
            if (strlen(pSecurity->getSrtpKey()) == 0)
            {
                securityHelper.generateSrtpKey(*pSecurity);
            }
            if (pInst->dbLocation)
            {
                securityHelper.setDbLocation(*pSecurity, pInst->dbLocation);
            }
            if (pInst->myCertNickname)
            {
                securityHelper.setMyCertNickname(*pSecurity, pInst->myCertNickname);
            }
            if (pInst->dbPassword)
            {
                securityHelper.setDbPassword(*pSecurity, pInst->dbPassword);
            }
        }        
        if (contactId > 0)
        {
            pContact = pInst->pSipUserAgent->getContactDb().find(contactId);
            assert(pContact);
            contactType = pContact->eContactType;

            OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
                    "contactId(%d): type: %s, transport: %s interface: %s, ip: %s:%d",
                    contactId,
                    sipxContactTypeToString((SIPX_CONTACT_TYPE) pContact->eContactType),
                    sipxTransportTypeToString((SIPX_TRANSPORT_TYPE) pContact->transportType),
                    pContact->cInterface,
                    pContact->cIpAddress,
                    pContact->iPort) ;
        }
        else
        {
            contactType = AUTO;
        }
        
        if (szAddress)
        {
            PtStatus status ;
            assert(remoteAddress.length() == 0) ;    // should be null

            if (!sipxIsCallInFocus() || bTakeFocus)
            {
                pInst->pCallManager->unholdLocalTerminalConnection(callId.data()) ;
                bSetFocus = true ;
            }

            pInst->pCallManager->setOutboundLineForCall(callId.data(),
                    lineId.data()) ;

            UtlString sessionId(szCallId) ;
            if (sessionId.length() < 1)
            {
                pInst->pCallManager->getNewSessionId(&sessionId) ;
            }
            SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
            if (pData)
            {
                pData->bInFocus = bSetFocus ;
                assert(pData->sessionCallId == NULL) ; // should be null
                if (pData->sessionCallId != NULL)
                {
                    delete pData->sessionCallId ;
                }
                pData->sessionCallId = new UtlString(sessionId.data()) ;
                
                if (pDisplay)
                {
                    pData->display = *pDisplay;
                }
                if (pSecurity)
                {
                    pData->security = *pSecurity;
                }
                sipxCallReleaseLock(pData, SIPX_LOCK_WRITE) ;
            }

            SIPX_SECURITY_ATTRIBUTES* pTempSecurity = NULL;
            if (pSecurity)
            {
                pTempSecurity = &pData->security;
            }
            if (pInst)
            {
                pLocationHeader = (bEnableLocationHeader) ? pInst->szLocationHeader : NULL;
            }
            if (pDisplay && pDisplay->handle)
            {
                status = pInst->pCallManager->connect(callId.data(), szAddress, NULL, sessionId, (CONTACT_ID) contactId, 
                                                      &pData->display, pTempSecurity, pLocationHeader, bandWidth) ;
            }
            else
            {
                status = pInst->pCallManager->connect(callId.data(), szAddress, NULL, sessionId, (CONTACT_ID) contactId, 
                                                      NULL, pTempSecurity, pLocationHeader, bandWidth) ;
            }

            if (status == PT_SUCCESS)
            {
                int numAddresses = 0 ;
                UtlString address ;
                OsStatus rc = pInst->pCallManager->getCalledAddresses(
                        callId.data(), 1, numAddresses, &address) ;
                OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                              "sipxCallConnect connected hCall=%d callId=%s, numAddr = %d, addr = %s",
                              hCall, callId.data(), numAddresses, address.data());
                assert(rc == OS_SUCCESS) ;
                assert(numAddresses == 1) ;

                // Set Remote Connection
                SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
                if (pData)
                {
                    if (pData->remoteAddress)
                    {
                        delete pData->remoteAddress ;
                    }
                    pData->remoteAddress = new UtlString(address) ;
                    assert(pData->remoteAddress != NULL) ;
                    if (pData->remoteAddress == NULL)
                    {
                        sr = SIPX_RESULT_OUT_OF_MEMORY ;
                    }
                    else
                    {
                        sr = SIPX_RESULT_SUCCESS ;
                    }
                    sipxCallReleaseLock(pData, SIPX_LOCK_WRITE) ;
                }
            }
            else
            {
                SipSession session ;
                sipxFireCallEvent(pInst->pCallManager, 
                        callId.data(), 
                        &session, 
                        szAddress, 
                        CALLSTATE_DISCONNECTED, 
                        CALLSTATE_CAUSE_BAD_ADDRESS) ;   
                sr = SIPX_RESULT_BAD_ADDRESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallHold(const SIPX_CALL hCall,
                                      bool  bStopRemoteAudio)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallHold hCall=%d bStopRemoteAudio=%d",
        hCall, bStopRemoteAudio);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;

    UtlString remoteAddress;
    
    SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_READ) ;
    if (pCallData && pCallData->state == SIPX_INTERNAL_CALLSTATE_HELD)
    {
        sr = SIPX_RESULT_INVALID_STATE;
    }    
    else if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        SIPX_CONF hConf = sipxCallGetConf(hCall) ;
        if (hConf == SIPX_CONF_NULL)
        {            
            if (bStopRemoteAudio)
            {
                pInst->pCallManager->holdTerminalConnection(callId.data(), remoteAddress.data(), 0) ;
            }
            pInst->pCallManager->holdLocalTerminalConnection(callId.data()) ;
        }
        else
        {
            pInst->pCallManager->holdTerminalConnection(callId.data(), remoteAddress.data(), 0) ;
        }
        sr = SIPX_RESULT_SUCCESS ;
    }
    sipxCallReleaseLock(pCallData, SIPX_LOCK_READ) ;


    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallUnhold(const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallUnhold hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        SIPX_CONF hConf = sipxCallGetConf(hCall) ;
        if (hConf == SIPX_CONF_NULL)
        {
            SIPX_CALL_DATA *pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE) ;
            if (pCallData && pCallData->state != SIPX_INTERNAL_CALLSTATE_HELD)
            {
                sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
                sr = SIPX_RESULT_INVALID_STATE;
            }
            else if (pCallData)
            {
                pCallData->bInFocus = true ;
                sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
            }
            pInst->pCallManager->unholdTerminalConnection(callId.data(), remoteAddress, NULL);        
            pInst->pCallManager->unholdLocalTerminalConnection(callId.data()) ;
        }
        else
        {
            pInst->pCallManager->unholdTerminalConnection(callId.data(), remoteAddress, NULL);                    
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallDestroy(SIPX_CALL& hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallDestroy hCall=%d",
        hCall);

    SIPX_CONF hConf = sipxCallGetConf(hCall) ;
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteUrl ;
    UtlString ghostCallId ;

    if (hConf != 0)
    {
        sr = sipxConferenceRemove(hConf, hCall) ;
    }
    else if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteUrl, NULL, &ghostCallId))
    {
        if (sipxCallIsRemoveInsteadOfDropSet(hCall))
        {
            pInst->pCallManager->dropConnection(callId, remoteUrl) ;
        }
        else
        {
            pInst->pCallManager->drop(callId.data()) ;
            if (ghostCallId.length() > 0)
            {
                pInst->pCallManager->drop(ghostCallId.data()) ;
            }
            sr = SIPX_RESULT_SUCCESS ;
        }

        // If not remote url, then the call was never completed and no
        // connection object exists (and no way to send an event) -- remove
        // the call handle.
        if (remoteUrl.length() == 0)
        {
            // TODO:: This should fire a destroy event
            sipxCallObjectFree(hCall) ;
        }
    }
    else
    {
        // Not finding the call is ok (torn down) providing
        // that the handle is valid.
        if (hCall != SIPX_CALL_NULL)
        {
            sr = SIPX_RESULT_SUCCESS ;
        }
    }

    hCall = SIPX_CALL_NULL ;

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallGetID(const SIPX_CALL hCall,
                                       char* szId,
                                       const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetID hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    UtlString callId ;

    if (sipxCallGetCommonData(hCall, NULL, &callId, NULL, NULL))
    {
        if (iMaxLength)
        {
            strncpy(szId, callId.data(), iMaxLength) ;
            szId[iMaxLength-1] = 0 ;
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}

SIPXTAPI_API SIPX_RESULT sipxCallGetLocalID(const SIPX_CALL hCall,
                                            char* szId,
                                            const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetLocalID hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    UtlString callId ;
    UtlString lineId ;

    if (sipxCallGetCommonData(hCall, NULL, &callId, NULL, &lineId))
    {
        if (iMaxLength)
        {
            strncpy(szId, lineId.data(), iMaxLength) ;
            szId[iMaxLength-1] = 0 ;
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallGetRemoteID(const SIPX_CALL hCall,
                                             char* szId,
                                             const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetRemoteID hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    UtlString callId ;
    UtlString remoteId ;

    if (sipxCallGetCommonData(hCall, NULL, &callId, &remoteId, NULL))
    {
        if (iMaxLength)
        {
            strncpy(szId, remoteId.data(), iMaxLength) ;
            szId[iMaxLength-1] = 0 ;
        }
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallGetConnectionId(const SIPX_CALL hCall,
                                                 int& connectionId)
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    connectionId = -1;
    
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);
   
    if (pData)        
    {
        if (pData->connectionId != -1)
        {
            connectionId = pData->connectionId;
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

            sr = SIPX_RESULT_SUCCESS;
        }
        else
        {
            if (pData->pInst && pData->pInst->pCallManager && pData->callId && 
                    pData->remoteAddress)
            {
                CallManager* pCallManager = pData->pInst->pCallManager ;
                UtlString callId(*pData->callId) ;
                UtlString remoteAddress(*pData->remoteAddress) ;

                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

                connectionId = pCallManager->getMediaConnectionId(callId, remoteAddress);

                pData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
                pData->connectionId = connectionId;
                sipxCallReleaseLock(pData, SIPX_LOCK_WRITE) ;
                               
                if (-1 != connectionId)
                {
                    sr = SIPX_RESULT_SUCCESS;
                }           
            }
            else
            {
                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
            }    
        }
    }

    return sr;
}                                                 

SIPXTAPI_API SIPX_RESULT sipxCallGetRequestURI(const SIPX_CALL hCall,
                                               char* szUri,
                                               const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetRequestURI hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);
    
    if (pData)        
    {
        if (pData->pInst && pData->pInst->pCallManager && pData->callId &&
            pData->remoteAddress) 
        {
            CallManager* pCallManager = pData->pInst->pCallManager ;
            UtlString callId(*pData->callId) ;
            UtlString remoteAddress(*pData->remoteAddress) ;

            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

            SipDialog sipDialog;
            pCallManager->getSipDialog(callId, remoteAddress, sipDialog);
            
            UtlString uri;
            sipDialog.getRemoteRequestUri(uri);
            if (iMaxLength)
            {
                strncpy(szUri, uri.data(), iMaxLength) ;
                szUri[iMaxLength-1] = 0 ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
        }    
    }

    return sr ;
}

SIPXTAPI_API SIPX_RESULT sipxCallGetRemoteContact(const SIPX_CALL hCall,
                                                  char* szContact,
                                                  const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetRemoteContact hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);
        
    if (pData)        
    {
        if (pData->pInst && pData->pInst->pCallManager && pData->callId && 
                pData->remoteAddress)
        {
            CallManager* pCallManager = pData->pInst->pCallManager ;
            UtlString callId(*pData->callId) ;
            UtlString remoteAddress(*pData->remoteAddress) ;

            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

            SipDialog sipDialog;
            pCallManager->getSipDialog(callId, remoteAddress, sipDialog);
            
            Url contact;
            sipDialog.getRemoteContact(contact);

            if (iMaxLength)
            {
                strncpy(szContact, contact.toString().data(), iMaxLength) ;
                szContact[iMaxLength-1] = 0 ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
        }    
    }


    return sr ;
}

SIPXTAPI_API SIPX_RESULT sipxCallGetRemoteUserAgent(const SIPX_CALL hCall,
                                                    char* szAgent,
                                                    const size_t iMaxLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallGetRemoteUserAgent hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);
        
    if (pData)        
    {
        if (pData->pInst && pData->pInst->pCallManager && pData->callId && 
                pData->remoteAddress)
        {
            CallManager* pCallManager = pData->pInst->pCallManager ;
            UtlString callId(*pData->callId) ;
            UtlString remoteAddress(*pData->remoteAddress) ;
            UtlString userAgent;

            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

            pCallManager->getRemoteUserAgent(callId, remoteAddress, userAgent);

            if (iMaxLength)
            {
                strncpy(szAgent, userAgent.data(), iMaxLength) ;
                szAgent[iMaxLength-1] = 0 ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
        }    
    }

    return sr ;
}

SIPXTAPI_API SIPX_RESULT sipxCallStartTone(const SIPX_CALL hCall,
                                           const TONE_ID toneId,
                                           const bool bLocal,
                                           const bool bRemote)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallStartTone hCall=%d ToneId=%d bLocal=%d bRemote=%d",
        hCall, toneId, bLocal, bRemote);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;
    TONE_ID xlateId;

    if (sipxTranslateToneId(toneId, xlateId) == SIPX_RESULT_SUCCESS)
    {
        if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
        {
#ifndef VOICE_ENGINE
            if (!pInst->toneStates.tonePlaying)
#endif
            {
                gpCallHandleMap->addHandleRef(hCall);  // Add a handle reference, so that
                                                     // if the call ends before
                                                     // the tone is stopped,
                                                     // there is still a valid call handle
                                                     // to use with stopTone
                pInst->pCallManager->toneChannelStart(callId, remoteAddress, xlateId, bLocal, bRemote) ;
                sr = SIPX_RESULT_SUCCESS ;

                if (!pInst->toneStates.bInitialized)
                {
                    pInst->toneStates.bInitialized = true;
                }
#ifndef VOICE_ENGINE
                pInst->toneStates.tonePlaying = true;
#endif                
            }
        }
    }
    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallStopTone(const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallStopTone hCall=%d",
        hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        if (pInst->toneStates.bInitialized
#       ifndef VOICE_ENGINE
            && pInst->toneStates.tonePlaying)
#       else
            )
#       endif
        {
#           ifndef VOICE_ENGINE
            pInst->pCallManager->toneChannelStop(callId, remoteAddress) ;
#           endif
            sipxCallObjectFree(hCall);
        
            sr = SIPX_RESULT_SUCCESS ;
            pInst->toneStates.tonePlaying = false;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallAudioPlayFileStart(const SIPX_CALL hCall,
                                                    const char* szFile,
                                                    const bool bRepeat,
                                                    const bool bLocal,
                                                    const bool bRemote,
                                                    const bool bMixWithMicrophone,
                                                    const float fVolumeScaling) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAudioPlayFileStart hCall=%d File=%s bLocal=%d bRemote=%d bRepeat=%d bMixWithMic=%d, volScale=%f",
        hCall, szFile, bLocal, bRemote, bRepeat, bMixWithMicrophone, fVolumeScaling);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    // Massage volume scaling into range
    float fDownScaling = fVolumeScaling ;
    if (fDownScaling > 1.0)
    {
        fDownScaling = 1.0 ;
    }
    else if (fDownScaling < 0.0)
    {
        fDownScaling = 0.0 ;
    }

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        if (szFile)
        {
            pInst->pCallManager->audioChannelPlay(callId, remoteAddress, szFile, bRepeat, bLocal, bRemote, bMixWithMicrophone, (int) (fDownScaling * 100.0)) ;
            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallAudioPlayFileStop(const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAudioPlayFileStop hCall=%d", hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        pInst->pCallManager->audioChannelStop(callId, remoteAddress) ;
        sr = SIPX_RESULT_SUCCESS ;
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallAudioRecordFileStart(const SIPX_CALL hCall,
                                                      const char* szFile) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAudioRecordFileStart hCall=%d szFile=%s", hCall, szFile);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (szFile && sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {        
        if (pInst->pCallManager->audioChannelRecordStart(callId, remoteAddress, szFile))
        {
            sr = SIPX_RESULT_SUCCESS ;
        }
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallAudioRecordFileStop(const SIPX_CALL hCall) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallAudioRecordFileStop hCall=%d", hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        if (pInst->pCallManager->audioChannelRecordStop(callId, remoteAddress))
        {
            sr = SIPX_RESULT_SUCCESS ;
        }
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxCallPlayBufferStart(const SIPX_CALL hCall,
                                                 const char* szBuffer,
                                                 const int  bufSize,
                                                 const int  bufType,
                                                 const bool bRepeat,
                                                 const bool bLocal,
                                                 const bool bRemote)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallPlayBufferStart hCall=%d Buffer=%p Size=%d Type=%d bLocal=%d bRemote=%d bRepeat=%d",
        hCall, szBuffer, bufSize, bufType, bLocal, bRemote, bRepeat);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, NULL, NULL))
    {
        if (szBuffer)
        {
            pInst->pCallManager->bufferPlay(callId.data(), (int)szBuffer, bufSize, bufType, bRepeat, bLocal, bRemote) ;
            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;

        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallPlayBufferStop(const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallPlayBufferStop hCall=%d", hCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, NULL, NULL))
    {
        pInst->pCallManager->audioStop(callId.data()) ;
        sr = SIPX_RESULT_SUCCESS ;
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;

    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallSubscribe(const SIPX_CALL hCall,
                                           const char* szEventType,
                                           const char* szAcceptType,
                                           SIPX_SUB* phSub,
                                           bool bRemoteContactIsGruu)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallSubscribe hCall=%d szEventType=\"%s\" szAcceptType=\"%s\"", 
        hCall,
        szEventType ? szEventType : "<null>",
        szAcceptType ? szAcceptType : "<null>");

    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA *pInst;
    UtlString callId;
    UtlString remoteAddress;
    UtlString lineId;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, &lineId))
    {
        SIPX_SUBSCRIPTION_DATA* subscriptionData = new SIPX_SUBSCRIPTION_DATA;
        subscriptionData->pDialogHandle = new UtlString;
        subscriptionData->pInst = pInst;
        *phSub = gpSubHandleMap->allocHandle(subscriptionData);
        int subscriptionPeriod = 3600;

        // Need to get the resourceId, To, From and Contact from
        // the associated call
        SipSession session;
        if(pInst->pCallManager->getSession(callId, remoteAddress, session))
        {
            Url toUrl;
            session.getToUrl(toUrl);
            toUrl.removeFieldParameters();
            UtlString toField;
            toUrl.toString(toField);
            UtlString contactField;
            Url contactUrl;
            session.getLocalContact(contactUrl);
            contactUrl.toString(contactField);

            // If remoteContactIsGruu is set we assume the
            // remote contact is a globally routable unique URI (GRUU).
            // Normally one cannot assume the Contact header is a
            // publicly addressable address and we assume that the
            // To or From from the remote side has a better chance of
            // being globally routable as it typically is an address
            // of record (AOR).
            UtlString resourceId;
            Url requestUri;
            if(bRemoteContactIsGruu)
            {
                requestUri = contactUrl;
            }
            else
            {
                requestUri = toUrl;
            }
            requestUri.removeFieldParameters();
            requestUri.toString(resourceId);

            // May need to get the from field from the line manager
            // For now, use the From in the session
            UtlString fromField;
            Url fromUrl;
            session.getFromUrl(fromUrl);
            fromUrl.removeFieldParameters();
            fromUrl.toString(fromField);

            UtlBoolean sessionDataGood = TRUE;
            if(resourceId.length() <= 1 ||
                fromField.length() <= 4 ||
                toField.length() <= 4 ||
                contactField.length() <= 4)
            {
                sessionDataGood = FALSE;
                OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                    "sipxCallSubscribe bad session data: hCall=%d szEventType=\"%s\" szAcceptType=\"%s\" resourceId=\"%s\" From=\"%s\" To=\"%s\" Contact=\"%s\"", 
                    hCall,
                    szEventType ? szEventType : "<null>",
                    szAcceptType ? szAcceptType : "<null>",
                    resourceId.data(),
                    fromField.data(),
                    toField.data(),
                    contactField.data());
            }

            // Session data is big enough
            else
            {
                OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
                    "sipxCallSubscribe subscribing: hCall=%d szEventType=\"%s\" szAcceptType=\"%s\" resourceId=\"%s\" From=\"%s\" To=\"%s\" Contact=\"%s\"", 
                    hCall,
                    szEventType ? szEventType : "<null>",
                    szAcceptType ? szAcceptType : "<null>",
                    resourceId.data(),
                    fromField.data(),
                    toField.data(),
                    contactField.data());
            }

            // Subscribe and keep the subscription refreshed
            if(sessionDataGood &&
               pInst->pSubscribeClient->addSubscription(resourceId, 
                                                        szEventType, 
                                                        szAcceptType,
                                                        fromField, 
                                                        toField, 
                                                        contactField,                                                         
                                                        subscriptionPeriod, 
                                                        (void*)*phSub, 
                                                        sipxSubscribeClientSubCallback,
                                                        sipxSubscribeClientNotifyCallback, 
                                                        *(subscriptionData->pDialogHandle)))
            {
                sipXresult = SIPX_RESULT_SUCCESS;
            }
        }

        // Could not find session for given call handle
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                "sipxCallSubscribe: could not get session for call handle: %d callId: %s remote address: %s",
                hCall, callId.data(), remoteAddress.data());
            sipXresult = SIPX_RESULT_INVALID_ARGS;
        }
    }
    else
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                      "sipxCallSubscribe: could not find call data for call handle: %d",
                      hCall);
        sipXresult = SIPX_RESULT_INVALID_ARGS;

    }

    return sipXresult;
}

SIPXTAPI_API SIPX_RESULT sipxCallUnsubscribe(const SIPX_SUB hSub)
{
    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE;
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallSubscribe hSub=%x", hSub);

    SIPX_SUBSCRIPTION_DATA* subscriptionData = 
        (SIPX_SUBSCRIPTION_DATA*) gpSubHandleMap->findHandle(hSub);

    if(subscriptionData && subscriptionData->pInst)
    {
        if(subscriptionData->pInst->pSubscribeClient->endSubscription(*(subscriptionData->pDialogHandle)))
        {
            sipXresult = SIPX_RESULT_SUCCESS;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                "sipxCallUnsubscribe endSubscription failed for subscription handle: %d dialog handle: \"%s\"",
                hSub,
                subscriptionData->pDialogHandle->data());
            sipXresult = SIPX_RESULT_INVALID_ARGS;
        }

        // Remove and free up the subscription handle and data
        gpSubHandleMap->removeHandle(hSub);

        if(subscriptionData->pDialogHandle)
        {
            delete subscriptionData->pDialogHandle;
            subscriptionData->pDialogHandle = NULL;
        }

        delete subscriptionData;
        subscriptionData = NULL;
    }

    // Invalid subscription handle
    else
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
            "sipxCallUnsubscribe: cannot find subscription data for handle: %d",
            hSub);
        sipXresult = SIPX_RESULT_INVALID_ARGS;
    }

    return(sipXresult);
}

SIPXTAPI_API SIPX_RESULT sipxConfigSubscribe(const SIPX_INST hInst, 
                                             const SIPX_LINE hLine, 
                                             const char* szTargetUrl, 
                                             const char* szEventType, 
                                             const char* szAcceptType, 
                                             const SIPX_CONTACT_ID contactId, 
                                             SIPX_SUB* phSub) 
{ 
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO, 
        "sipxConfigSubscribe hInst=%p szTargetUrl=\"%s\" szEventType=\"%s\" szAcceptType=\"%s\"", 
        hInst, 
        szTargetUrl ?  szTargetUrl  : "<null>", 
        szEventType ? szEventType : "<null>", 
        szAcceptType ? szAcceptType : "<null>"); 

    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE; 
    if (hInst) 
    { 
        SIPX_INSTANCE_DATA *pInst = (SIPX_INSTANCE_DATA*)hInst; 

        SIPX_SUBSCRIPTION_DATA* subscriptionData = new SIPX_SUBSCRIPTION_DATA; 
        subscriptionData->pDialogHandle = new UtlString; 
        subscriptionData->pInst = pInst; 
        *phSub = gpSubHandleMap->allocHandle(subscriptionData); 
        int subscriptionPeriod = 3600; 

        // Need to get the resourceId, To, From and Contact from 
        // the associated call 
        UtlString resourceId(szTargetUrl); 
        UtlString fromField; 
        UtlString toField(szTargetUrl); 
        UtlString contactField; 
        SIPX_LINE_DATA* pLineData = sipxLineLookup(hLine, SIPX_LOCK_READ) ; 

        if(pLineData) 
        { 
            pLineData->lineURI->getIdentity(fromField); 

            // build a contact field 
            UtlString userId(""); 

            CONTACT_ADDRESS* pContact = pInst->pSipUserAgent->getContactDb().find(contactId); 
            if (pContact) 
            { 
                contactField.append("sip:"); 
                pLineData->lineURI->getUserId(userId); 
                contactField.append(userId); 
                contactField.append("@"); 
                contactField.append(pContact->cIpAddress); 
                char szPort[32]; 
                sprintf(szPort, ":%d", pContact->iPort); 
                contactField.append(szPort); 
            } 
            else 
            { 
                contactField.append(fromField); 
            } 

            sipxLineReleaseLock(pLineData, SIPX_LOCK_READ) ; 

            if(resourceId.length() <= 1 || 
                fromField.length() <= 4 || 
                toField.length() <= 4 || 
                contactField.length() <= 4) 
            { 
                OsSysLog::add(FAC_SIPXTAPI, PRI_ERR, 
                    "sipxConfigSubscribe bad parameters"); 
            } 
            // Session data is big enough 
            else 
            { 
                    // Subscribe and keep the subscription refreshed 
                    if(pInst->pSubscribeClient->addSubscription(resourceId, 
                                                                szEventType, 
                                                                szAcceptType,
                                                                fromField, 
                                                                toField, 
                                                                contactField, 
                                                                subscriptionPeriod, 
                                                                (void*)*phSub, 
                                                                sipxSubscribeClientSubCallback, 
                                                                sipxSubscribeClientNotifyCallback, 
                                                                *(subscriptionData->pDialogHandle))) 
                    { 
                        sipXresult = SIPX_RESULT_SUCCESS; 
                    } 

            } 
        } 

    } // if (hInst) 

    return sipXresult; 
} 
    
SIPXTAPI_API SIPX_RESULT sipxConfigUnsubscribe(const SIPX_SUB hSub) 
{ 
    return sipxCallUnsubscribe(hSub); 
} 

SIPXTAPI_API SIPX_RESULT sipxCallBlindTransfer(const SIPX_CALL hCall,
                                               const char* pszAddress)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallBlindTransfer hCall=%d Address=%s",
        hCall, pszAddress);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, NULL, NULL))
    {
        if (pszAddress)
        {
            UtlString ghostCallId ;
            
            // first, get rid of any existing ghost connection
            SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
            if (pData && pData->ghostCallId)
            {
                if (pData->ghostCallId->length() > 0)
                {
                    pInst->pCallManager->drop(pData->ghostCallId->data()) ;
                }
                delete pData->ghostCallId;
                pData->ghostCallId = NULL;
            }
            sipxCallReleaseLock(pData, SIPX_LOCK_WRITE) ;

            // call the transfer
            pInst->pCallManager->transfer_blind(callId.data(), pszAddress, &ghostCallId) ;
            
            pData = sipxCallLookup(hCall, SIPX_LOCK_WRITE);
            if (pData)
            {   
                // set the new ghost call Id
                pData->ghostCallId = new UtlString(ghostCallId);
                
                sipxCallReleaseLock(pData, SIPX_LOCK_WRITE) ;            
                sr = SIPX_RESULT_SUCCESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxCallTransfer(const SIPX_CALL hSourceCall,
                                          const SIPX_CALL hTargetCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallTransfer hSourceCall=%d hTargetCall=%d\n",
        hSourceCall, hTargetCall);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString sourceCallId;
    UtlString sourceAddress;    
    UtlString targetCallId ;
    UtlString targetAddress;

    if (    sipxCallGetCommonData(hSourceCall, &pInst, &sourceCallId, &sourceAddress, NULL) &&
            sipxCallGetCommonData(hTargetCall, NULL, &targetCallId, &targetAddress, NULL))
        
    {
        // call the transfer            
        if (pInst->pCallManager->transfer(sourceCallId, sourceAddress, targetCallId, targetAddress) == PT_SUCCESS)
        {
            sr = SIPX_RESULT_SUCCESS ;
        }            
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxCallSendInfo(SIPX_INFO* phInfo,
                                          const SIPX_CALL hCall,
                                          const char* szContentType,
                                          const char* szContent,
                                          const size_t nContentLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallSendInfo phInfo=%p hCall=%d contentType=%s content=%p contentLength=%d",
        phInfo, hCall, szContentType, szContent, (int) nContentLength);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString lineId;
    UtlString remoteAddress ;
    
    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, &lineId))
    {
        if (callId.length() != 0)
        {
            SIPX_LINE hLine = sipxLineLookupHandleByURI(lineId.data()) ;
            SIPX_INFO_DATA* pInfoData = new SIPX_INFO_DATA;
            memset((void*) pInfoData, 0, sizeof(SIPX_INFO_DATA));
            SIPX_CALL_DATA* pCall = sipxCallLookup(hCall, SIPX_LOCK_READ); 

            pInfoData->pInst = pInst;
            // Create Mutex
            pInfoData->pMutex = new OsRWMutex(OsRWMutex::Q_FIFO);
            pInfoData->infoData.nSize = sizeof(SIPX_INFO_INFO);
            pInfoData->infoData.hCall = hCall;
            pInfoData->infoData.hLine = hLine;
            pInfoData->infoData.szFromURL = strdup(lineId.data());
            pInfoData->infoData.nContentLength = nContentLength;
            pInfoData->infoData.szContentType = strdup(szContentType);
            pInfoData->infoData.pContent = strdup(szContent);

            *phInfo = gpInfoHandleMap->allocHandle(pInfoData) ;
            assert(*phInfo != 0) ;

            SipSession* pSession = new SipSession(callId, pCall->remoteAddress->data(), pInfoData->infoData.szFromURL);
            sipxCallReleaseLock(pCall, SIPX_LOCK_READ);
            
            pInst->pSipUserAgent->addMessageObserver(*(pInst->pMessageObserver->getMessageQueue()), SIP_INFO_METHOD, 0, 1, 1, 0, 0, pSession, (void*)*phInfo);
            delete pSession;

            if (pInst->pCallManager->sendInfo(callId, remoteAddress, szContentType, nContentLength, szContent))
            {
                sr = SIPX_RESULT_SUCCESS ;
            }
            else
            {
                sr = SIPX_RESULT_INVALID_STATE ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    return sr;
} 


SIPXTAPI_API SIPX_RESULT sipxCallGetEnergyLevels(const SIPX_CALL hCall,
                                                 int&            iInputEnergyLevel,
                                                 int&            iOutputEnergyLevel,
                                                 const size_t    nMaxContributors,
                                                 unsigned int    CCSRCs[],
                                                 int             iEnergyLevels[],
                                                 size_t&         nActualContributors) 
{
#if VOICE_ENGINE
    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);    
    if (pData)        
    {
        if (pData->pInst && pData->pInst->pCallManager && pData->callId &&
            pData->remoteAddress) 
        {
            if (pData->state == SIPX_INTERNAL_CALLSTATE_CONNECTED)
            {
                CallManager* pCallManager = pData->pInst->pCallManager ;
                UtlString callId(*pData->callId) ;
                UtlString remoteAddress(*pData->remoteAddress) ;
                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

                int nContributors = (int) nMaxContributors ; 
                if (pCallManager->getAudioEnergyLevels(callId, 
                        remoteAddress, 
                        iInputEnergyLevel,
                        iOutputEnergyLevel,
                        nContributors,
                        CCSRCs,
                        iEnergyLevels))
                {
                    nActualContributors = (size_t) nContributors ;
                    rc = SIPX_RESULT_SUCCESS ;                    
                }
                else
                {
                    nActualContributors = 0 ; 
                }
            }
            else
            {
                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
                rc = SIPX_RESULT_INVALID_STATE ;
            }            
        }
        else
        {
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
        }
    }

#else
    SIPX_RESULT rc = SIPX_RESULT_NOT_IMPLEMENTED ;
#endif

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxCallGetAudioRtpSourceIds(const SIPX_CALL hCall,
                                                      unsigned int& iSendSSRC,
                                                      unsigned int& iReceiveSSRC) 
{
#if VOICE_ENGINE
    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;
    SIPX_CALL_DATA* pData = sipxCallLookup(hCall, SIPX_LOCK_READ);    
    if (pData)        
    {
        if (pData->pInst && pData->pInst->pCallManager && pData->callId &&
            pData->remoteAddress) 
        {
            if (pData->state == SIPX_INTERNAL_CALLSTATE_CONNECTED)
            {
                CallManager* pCallManager = pData->pInst->pCallManager ;
                UtlString callId(*pData->callId) ;
                UtlString remoteAddress(*pData->remoteAddress) ;
                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;

                if (pCallManager->getAudioRtpSourceIDs(
                        callId,
                        remoteAddress, 
                        iSendSSRC,
                        iReceiveSSRC))
                {
                    rc = SIPX_RESULT_SUCCESS ;                    
                }
            }
            else
            {
                sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
                rc = SIPX_RESULT_INVALID_STATE ;
            }            
        }
        else
        {
            sipxCallReleaseLock(pData, SIPX_LOCK_READ) ;
        }
    }

#else
    SIPX_RESULT rc = SIPX_RESULT_NOT_IMPLEMENTED ;
#endif

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxCallGetAudioRtcpStats(const SIPX_CALL hCall,
                                                   SIPX_RTCP_STATS* pStats) 
{
#if VOICE_ENGINE
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    int connectionId ;

    // TODO: This should really be plumbed through the call manager

    if (pStats && pStats->cbSize == sizeof(SIPX_RTCP_STATS))
    {
        if (sipxCallGetConnectionId(hCall, connectionId) == SIPX_RESULT_SUCCESS)
        {
            rc = SIPX_RESULT_FAILURE ;
            GipsVoiceEngineLib* pVoiceEngine = sipxCallGetVoiceEnginePtr(hCall);
            if (pVoiceEngine)
            {
                GIPSVE_CallStatistics stats ;
                memset(&stats, 0, sizeof(GIPSVE_CallStatistics)) ;

                if (pVoiceEngine->GIPSVE_RTCPStat(connectionId, &stats) == 0)
                {
                    pStats->fraction_lost = stats.fraction_lost ;
                    pStats->cum_lost = stats.cum_lost ;
                    pStats->ext_max = stats.ext_max ;
                    pStats->jitter = stats.jitter ;
                    pStats->RTT = stats.RTT ;
                    pStats->bytesSent = stats.bytesSent ;
                    pStats->packetsSent = stats.packetsSent ;
                    pStats->bytesReceived = stats.bytesReceived ;
                    pStats->packetsReceived = stats.packetsReceived ;

                    rc = SIPX_RESULT_SUCCESS ;
                }
            }
        }
    }
#else
    SIPX_RESULT rc = SIPX_RESULT_NOT_IMPLEMENTED ;
#endif

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxCallLimitCodecPreferences(const SIPX_CALL hCall,
                                                       const SIPX_AUDIO_BANDWIDTH_ID audioBandwidth,
                                                       const SIPX_VIDEO_BANDWIDTH_ID videoBandwidth,
                                                       const char* szVideoCodecName)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallLimitCodecPreferences hCall=%d audioBandwidth=%d videoBandwidth=%d szVideoCodecName=\"%s\"", 
        hCall,
        audioBandwidth,
        videoBandwidth,
        (szVideoCodecName != NULL) ? szVideoCodecName : "") ;

    SIPX_RESULT sr = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA *pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    // Test bandwidth for legal values
    if (((audioBandwidth >= AUDIO_CODEC_BW_LOW && audioBandwidth <= AUDIO_CODEC_BW_HIGH) || audioBandwidth == AUDIO_CODEC_BW_DEFAULT) &&
        ((videoBandwidth >= VIDEO_CODEC_BW_LOW && videoBandwidth <= VIDEO_CODEC_BW_HIGH) || videoBandwidth == VIDEO_CODEC_BW_DEFAULT))
    {
        if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
        {
            pInst->pCallManager->limitCodecPreferences(callId, remoteAddress, audioBandwidth, videoBandwidth, szVideoCodecName) ;
            pInst->pCallManager->renegotiateCodecsTerminalConnection(callId, remoteAddress, NULL) ;

            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}

/****************************************************************************
 * Publisher Related Functions
 ***************************************************************************/


SIPXTAPI_API SIPX_RESULT sipxPublisherCreate(const SIPX_INST hInst, 
                                             SIPX_PUB* phPub,
                                             const char* szResourceId,
                                             const char* szEventType,
                                             const char* szContentType,
                                             const char* pContent,
                                             const size_t nContentLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCreatePublisher hInst=%p phPub=%d szResourceId=\"%s\" szEventType=\"%s\" szContentType=\"%s\" pContent=\"%s\" nContentLength=%d", 
        hInst,
        *phPub,
        szResourceId ? szResourceId : "<null>",
        szEventType ? szEventType : "<null>",
        szContentType ? szContentType : "<null>",
        pContent ? pContent : "<null>",
        (int) nContentLength);

    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE;

    // Get the instance data
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    // Verify that no content has been previously published for this
    // resourceID and eventType
    int oldContentCount = 0;
    HttpBody* oldContentPtr = NULL;
    UtlBoolean isDefaultContent;
    SipPublishContentMgr* publishMgr = NULL;

    if(szEventType && *szEventType)
    {
        if(pInst->pSubscribeServer->isEventTypeEnabled(szEventType))
        {
            publishMgr = 
                pInst->pSubscribeServer->getPublishMgr(szEventType);
            if(publishMgr)
            {
                publishMgr->getContent(szResourceId, 
                                       szEventType, 
                                       szContentType,
                                       oldContentPtr, 
                                       isDefaultContent);
            }
            // Default content is ok, ignore it
            if(isDefaultContent && oldContentPtr)
            {
                delete oldContentPtr;
                oldContentPtr = NULL;
            }
        }
    }

    else
    {
        sipXresult = SIPX_RESULT_INVALID_ARGS;
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
            "sipxCreatePublisher: argument szEventType is NULL");
    }

    if(oldContentPtr)
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
            "sipxCreatePublisher: content already exists for resourceId: %s and eventType: %s",
            szResourceId ? szResourceId : "<null>",
            szEventType ? szEventType : "<null>");

        sipXresult = SIPX_RESULT_INVALID_ARGS;
        delete oldContentPtr;
        oldContentPtr = NULL;
    }

    // No prior content published
    else if(szEventType && *szEventType)
    {
        // Create a publish structure for the SIPX_PUB handle
        SIPX_PUBLISH_DATA* pData = new SIPX_PUBLISH_DATA;
        if(pData)
        {
            pData->pInst = pInst;
            pData->pResourceId = new UtlString(szResourceId);

            if(pData->pResourceId)
            {
                pData->pEventType = new UtlString(szEventType);
                if(pData->pEventType)
                {
                    // Create a new HttpBody to publish for the resourceId and eventType
                    pData->pContent = 
                        new HttpBody(pContent, nContentLength, szContentType);
                    if(pData->pContent)
                    {
                        // Register the publisher handle
                        *phPub = gpPubHandleMap->allocHandle(pData);

                        // No publisher for this event type
                        if(publishMgr == NULL)
                        {
                            // Enable the event type for the SUBSCRIBE Server
                            pInst->pSubscribeServer->enableEventType(*pData->pEventType);
                            publishMgr = 
                                pInst->pSubscribeServer->getPublishMgr(*pData->pEventType);
                        }

                        // Publish the content
                        publishMgr->publish(*pData->pResourceId,
                                            *pData->pEventType,
                                            *pData->pEventType,
                                            1, // one content type for event
                                            &pData->pContent,
                                            1, // old content array size
                                            oldContentCount,
                                            &oldContentPtr);
                        sipXresult = SIPX_RESULT_SUCCESS;

                        // There should not be any prior content for this 
                        // resource and event type
                        if(oldContentCount)
                        {
                            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                                "sipxCreatePublisher: content already exists for resourceId: %s and eventType: %s",
                                szResourceId ? szResourceId : "<null>",
                                szEventType ? szEventType : "<null>");

                            sipXresult = SIPX_RESULT_INVALID_ARGS;
                            gpPubHandleMap->removeHandle(*phPub);
                            phPub = NULL;
                            delete pData->pEventType;
                            delete pData->pResourceId;
                            delete pData;
                            pData = NULL;
                        }

                    }
                    else
                    {
                        sipXresult = SIPX_RESULT_OUT_OF_MEMORY;
                        delete pData->pEventType;
                        delete pData->pResourceId;
                        delete pData;
                        pData = NULL;
                    }
                }
                else
                {
                    sipXresult = SIPX_RESULT_OUT_OF_MEMORY;
                    delete pData->pResourceId;
                    delete pData;
                    pData = NULL;
                }
            }
            else
            {
                sipXresult = SIPX_RESULT_OUT_OF_MEMORY;
                delete pData;
                pData = NULL;
            }
        }
        else
        {
            sipXresult = SIPX_RESULT_OUT_OF_MEMORY;
            *phPub = SIPX_PUB_NULL;
        }
    }
    return(sipXresult);
}

SIPXTAPI_API SIPX_RESULT sipxPublisherUpdate(const SIPX_PUB hPub,
                                             const char* szContentType,
                                             const char* pContent,
                                             const size_t nContentLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxUpdatePublisher hPub=%d szContentType=\"%s\" pContent=\"%s\" nContentLength=%d", 
        hPub,
        szContentType ? szContentType : "<null>",
        pContent ? pContent : "<null>",
        (int) nContentLength);

    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE;

    SIPX_PUBLISH_DATA* pData = (SIPX_PUBLISH_DATA*)
        gpPubHandleMap->findHandle(hPub);

    if(szContentType && *szContentType &&
        nContentLength > 0 && 
        pContent && *pContent && 
        pData)
    {
        int oldContentCount = 0;
        HttpBody* oldContentPtr = NULL;
        HttpBody* newContent = 
            new HttpBody(pContent, nContentLength, szContentType);

        SipPublishContentMgr* publishMgr = 
            pData->pInst->pSubscribeServer->getPublishMgr(*pData->pEventType);
        if(publishMgr)
        {
            // Publish the state change
            publishMgr->publish(*pData->pResourceId,
                                *pData->pEventType,
                                *pData->pEventType,
                                1, // one content type for event
                                &newContent,
                                1, // old content array size
                                oldContentCount,
                                &oldContentPtr);
            sipXresult = SIPX_RESULT_SUCCESS;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                "sipxUpdatePublisher: no publisher for event type: %s",
                pData->pEventType->data());
            sipXresult = SIPX_RESULT_FAILURE;
        }

        if(oldContentCount == 1)
        {
            if(oldContentPtr)
            {
                delete oldContentPtr;
                oldContentPtr = NULL;
            }
        }
        else if(oldContentCount > 1)
        {
            sipXresult = SIPX_RESULT_FAILURE;
            OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                "sipxUpdatePublisher old content count: %d",
                oldContentCount);
        }
    }

    // content size < 0 || content is null
    else
    {
        sipXresult = SIPX_RESULT_INVALID_ARGS;
    }

    return(sipXresult);
}

SIPXTAPI_API SIPX_RESULT sipxPublisherDestroy(const SIPX_PUB hPub,
                                              const char* szContentType,
                                              const char* pFinalContent,
                                              const size_t nContentLength)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxDestroyPublisher hPub=%d szContentType=\"%s\" pFinalContent=\"%s\" nContentLength=%d", 
        hPub,
        szContentType ? szContentType : "<null>",
        pFinalContent ? pFinalContent : "<null>",
        (int) nContentLength);

    SIPX_RESULT sipXresult = SIPX_RESULT_FAILURE;
    SIPX_PUBLISH_DATA* pData = (SIPX_PUBLISH_DATA*)
        gpPubHandleMap->findHandle(hPub);

    if(pData)
    {
        UtlBoolean unPublish = FALSE;

        if(szContentType && *szContentType &&
            pFinalContent && *pFinalContent &&
            nContentLength > 0)
        {
            unPublish = TRUE;
            sipxPublisherUpdate(hPub, szContentType, pFinalContent, nContentLength);
        }
        else if(nContentLength > 0 &&
                (szContentType == NULL || *szContentType == '\000' ||
                 pFinalContent == NULL || *pFinalContent))
        {
            unPublish = FALSE;
            sipXresult = SIPX_RESULT_INVALID_ARGS;
        }
        else
        {
            unPublish = TRUE;
        }

        // We remove the handle here as sipxUpdatePublisher needs the handle to
        // be in the handleMap still in order to work correctly
        gpPubHandleMap->removeHandle(hPub);

        if(unPublish)
        {
            HttpBody* oldContent = NULL;
            SipPublishContentMgr* publishMgr = 
                pData->pInst->pSubscribeServer->getPublishMgr(*pData->pEventType);
            if(publishMgr)
            {
                // Publish the state change
                int numOldContentTypes = 0;
                publishMgr->unpublish(*pData->pResourceId, 
                                      *pData->pEventType,
                                      *pData->pEventType,
                                      1, // max published content types
                                      numOldContentTypes,
                                      &oldContent);

                if(oldContent)
                {
                    if(pData->pContent == oldContent)
                    {
                        pData->pContent = NULL;
                    }
                    delete oldContent;
                    oldContent = NULL;
                }
            }

            // We cannot free up the pContent unless we
            // unpublish it.  If it was unpublished it would
            // have been freed above
            if(pData->pContent)
            {
                OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                    "sipxDestroyPublisher: content did not match that which was unpublished %p != %p",
                    oldContent, pData->pContent);
            }

            if(pData->pEventType)
            {
                delete pData->pEventType;
                pData->pEventType = NULL;
            }
            if(pData->pResourceId)
            {
                delete pData->pResourceId;
                pData->pResourceId = NULL;
            }

            delete pData;
            pData = NULL;
            sipXresult = SIPX_RESULT_SUCCESS;
        }
    }

    // Could not find the publish data for the given handle
    else
    {
        sipXresult = SIPX_RESULT_INVALID_ARGS;
    }

    return(sipXresult);

}
                                         
/****************************************************************************
 * Conference Related Functions
 ***************************************************************************/


SIPXTAPI_API SIPX_RESULT sipxConferenceCreate(const SIPX_INST hInst,
                                              SIPX_CONF *phConference)
{
    SIPX_RESULT rc = SIPX_RESULT_OUT_OF_MEMORY ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceCreate hInst=%p phConference=%p",
        hInst, phConference);

    assert(phConference) ;

    if (phConference)
    {
        *phConference = SIPX_CONF_NULL ;

        SIPX_CONF_DATA* pData = new SIPX_CONF_DATA ;
        assert(pData != NULL) ;
        if (pData)
        {
            // Init conference data
            memset(pData, 0, sizeof(SIPX_CONF_DATA)) ;
            pData->pInst = (SIPX_INSTANCE_DATA*) hInst ;

            // Increment conference counter
            pData->pInst->pLock->acquire() ;
            pData->pInst->nConferences++ ;
            pData->pInst->pLock->release() ;

            pData->pMutex = new OsRWMutex(OsRWMutex::Q_FIFO) ;
            *phConference = gpConfHandleMap->allocHandle(pData) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}



SIPXTAPI_API SIPX_RESULT sipxConferenceJoin(const SIPX_CONF hConf,
                                            const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceJoin hConf=%d hCall=%d",
        hConf, hCall);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    bool bDoSplit = false ;
    UtlString sourceCallId ;
    UtlString sourceAddress ;
    UtlString targetCallId ;
    SIPX_INSTANCE_DATA* pInst = NULL ;

    if (hConf && hCall)
    {
        SIPX_CONF_DATA* pConfData = sipxConfLookup(hConf, SIPX_LOCK_WRITE) ;
        if (pConfData)
        {
            SIPX_CALL_DATA * pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE) ;
            if (pCallData)
            {
                if (pCallData->hConf == SIPX_CALL_NULL)
                {
                    if (pConfData->strCallId == NULL)
                    {
                        assert(pConfData->nCalls == 0) ;

                        // This is a virgin conference; use the supplied call 
                        // as the conference shell (CpPeerCall).
                        pConfData->strCallId = new UtlString(*pCallData->callId) ;
                        pConfData->hCalls[pConfData->nCalls++] = hCall ;
                        pCallData->hConf = hConf ;                       
                    }
                    else
                    {
                        // This is an existing conference, need to physically 
                        // join/split.
                        if ((pCallData->state == SIPX_INTERNAL_CALLSTATE_REMOTE_HELD) ||
                            (pCallData->state == SIPX_INTERNAL_CALLSTATE_HELD))
                        {
                            // Mark data for split/drop below
                            bDoSplit = true ;
                            sourceCallId = *pCallData->callId ;
                            sourceAddress = *pCallData->remoteAddress ;
                            targetCallId = *pConfData->strCallId ;
                            pInst = pCallData->pInst ;

                            // Update data structures
                            *pCallData->callId = targetCallId ;
                            pCallData->hConf = hConf ;
                            pConfData->hCalls[pConfData->nCalls++] = hCall ;
                        }
                        else
                        {
                            rc = SIPX_RESULT_INVALID_STATE ;
                        }
                    }
                }
                else
                {
                    rc = SIPX_RESULT_INVALID_STATE ;
                }

                sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
            }
            sipxConfReleaseLock(pConfData, SIPX_LOCK_WRITE) ;
        }

        if (bDoSplit)
        {
            // Do the split
            PtStatus status = pInst->pCallManager->split(sourceCallId, sourceAddress, targetCallId) ;
            if (status != PT_SUCCESS)
            {
                rc = SIPX_RESULT_FAILURE ;
            }
            else
            {
                rc = SIPX_RESULT_SUCCESS ;
            }

            // If the call fails -- hard to recover, drop the call anyways.
            pInst->pCallManager->drop(sourceCallId) ;
        }
        else
        {
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceSplit(const SIPX_CONF hConf,
                                             const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceSplit hConf=%d hCall=%d",
        hConf, hCall);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    UtlString sourceCallId ;
    UtlString sourceAddress ;
    UtlString targetCallId ;
    SIPX_INSTANCE_DATA *pInst = NULL ;

    if (hConf && hCall)
    {
        SIPX_CONF_DATA* pConfData = sipxConfLookup(hConf, SIPX_LOCK_WRITE) ;
        if (pConfData)
        {
            SIPX_CALL_DATA* pCallData = sipxCallLookup(hCall, SIPX_LOCK_WRITE) ;
            if (pCallData)
            {
                if ((pCallData->state == SIPX_INTERNAL_CALLSTATE_REMOTE_HELD) || 
                    (pCallData->state == SIPX_INTERNAL_CALLSTATE_HELD))
                {
                    // Record data for split
                    pInst = pCallData->pInst ;
                    sourceCallId = *pCallData->callId ;
                    sourceAddress = *pCallData->remoteAddress ;

                    // Remove from conference handle
                    sipxRemoveCallHandleFromConf(hConf, hCall) ;                

                    // Create a CpPeerCall call to hold connection
                    pCallData->pInst->pCallManager->createCall(&targetCallId) ;

                    // Update call structure
                    if (pCallData->sessionCallId)
                    {
                        *pCallData->sessionCallId = sourceCallId ;
                    }
                    else
                    {
                        pCallData->sessionCallId = new UtlString(sourceCallId) ;
                    }
                    *pCallData->callId = targetCallId ;
                    pCallData->hConf = SIPX_CALL_NULL ;                                    

                    rc = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    rc = SIPX_RESULT_INVALID_STATE ;
                }

                sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;
            }
            sipxConfReleaseLock(pConfData, SIPX_LOCK_WRITE) ;
        }

        // Initiate Split
        if (rc == SIPX_RESULT_SUCCESS)
        {
            PtStatus status = pInst->pCallManager->split(sourceCallId, sourceAddress, targetCallId) ;
            if (status != PT_SUCCESS)
            {
                rc = SIPX_RESULT_FAILURE ;
            }
        }
    }


    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceAdd(const SIPX_CONF hConf,
                                           const SIPX_LINE hLine,
                                           const char* szAddress,
                                           SIPX_CALL* phNewCall,
                                           SIPX_CONTACT_ID contactId,
                                           SIPX_VIDEO_DISPLAY* const pDisplay,
                                           SIPX_SECURITY_ATTRIBUTES* const pSecurity,
                                           bool bTakeFocus,
                                           SIPX_CALL_OPTIONS* options)
{
    bool bEnableLocationHeader=false;
    int bandWidth=AUDIO_CODEC_BW_DEFAULT;

    if (options != NULL)
    {
        if (options->cbSize == sizeof(SIPX_CALL_OPTIONS))
        {
            bEnableLocationHeader = options->sendLocation;
            bandWidth = options->bandwidthId;
        }
        else
        {
            OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                "sipxCallAccept - cbSize of SIPX_CALL_OPTIONS does not match size of structure, ignoring options");
        }
    }

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceAdd hConf=%d hLine=%d szAddress=%s contactId=%d, pDisplay=%p "
        "bEnableLocationHeader=%d bandWidth=%d",
        hConf, hLine, szAddress, contactId, pDisplay, bEnableLocationHeader, bandWidth);

    // check for and load security runtime .dlls
    if (pSecurity)
    {
        SIPX_RESULT rc = sipxConfigLoadSecurityRuntime();
        if (SIPX_RESULT_SUCCESS != rc)
        {
            return rc;  // return right away 
        }
    }

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    bool bSetFocus = false ;
    char* pLocationHeader = NULL;

    if (hConf)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_WRITE) ;
        if (pData->pInst && pSecurity)
        {
            // augment the security attributes with the instance's security parameters
            SecurityHelper securityHelper;
            // generate a random key, if one isn't there
            if (strlen(pSecurity->getSrtpKey()) == 0)
            {
                securityHelper.generateSrtpKey(*pSecurity);
            }
            if (pData->pInst->dbLocation)
            {
                securityHelper.setDbLocation(*pSecurity, pData->pInst->dbLocation);
            }
            if (pData->pInst->myCertNickname)
            {
                securityHelper.setMyCertNickname(*pSecurity, pData->pInst->myCertNickname);
            }
            if (pData->pInst->dbPassword)
            {
                securityHelper.setDbPassword(*pSecurity, pData->pInst->dbPassword);
            }
        }
        if (pData)
        {
            CONTACT_ADDRESS* pContactAddress = pData->pInst->pSipUserAgent->getContactDb().find(contactId) ;
            if (pContactAddress)
            {
                OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
                    "contactId(%d): type: %s, transport: %s, interface: %s, ip: %s:%d",
                    contactId,
                    sipxContactTypeToString((SIPX_CONTACT_TYPE) pContactAddress->eContactType),
                    sipxTransportTypeToString((SIPX_TRANSPORT_TYPE) pContactAddress->transportType),
                    pContactAddress->cInterface,
                    pContactAddress->cIpAddress,
                    pContactAddress->iPort) ;
            }

            if (pData->nCalls == 0)
            {
                /*
                 * No existings legs, create one and connect
                 */

                // Create new call
                SIPX_CALL hNewCall ;
                rc = sipxCallCreateHelper(pData->pInst, hLine, hConf, &hNewCall) ;
                if (rc == SIPX_RESULT_SUCCESS)
                {
                    // Only take focus if something doesn't already have it.
                    if (!sipxIsCallInFocus() || bTakeFocus)
                    {
                        SIPX_CALL_DATA* pCallData = sipxCallLookup(hNewCall, SIPX_LOCK_READ) ;
                        if (pCallData)
                        {
                            pCallData->pInst->pCallManager->unholdLocalTerminalConnection(pCallData->callId->data()) ;
                            bSetFocus = true ;
                            sipxCallReleaseLock(pCallData, SIPX_LOCK_READ) ;
                        }                        
                    }

                    // Get data struct for call and store callId as conf Id
                    assert(hNewCall) ;

                    SIPX_CALL_DATA* pCallData = sipxCallLookup(hNewCall, SIPX_LOCK_WRITE) ;                    
                    assert(pCallData) ;
                    if (pDisplay)
                    {
                        pCallData->display = *pDisplay;
                    }
                    if (pSecurity)
                    {
                        pCallData->security = *pSecurity;
                    }
                    pCallData->bInFocus = bSetFocus ;
                    pData->strCallId = new UtlString(pCallData->callId->data()) ;

                    // Add the call handle to the conference handle
                    pData->hCalls[pData->nCalls++] = hNewCall ;
                    *phNewCall = hNewCall ;

                    // Allocate a new session id (to replace call id for connection)
                    UtlString sessionId ;
                    pData->pInst->pCallManager->getNewSessionId(&sessionId) ;
                    *pCallData->callId = sessionId.data() ;
                    sipxCallReleaseLock(pCallData, SIPX_LOCK_WRITE) ;


                    // Notify Listeners of new call
                    SipSession session ;
                    UtlString callId ;
                    SIPX_INSTANCE_DATA* pInst ;
                    sipxCallGetCommonData(hNewCall, &pInst, &callId, NULL, NULL) ;

                    sipxFireCallEvent(pInst->pCallManager, 
                            sessionId.data(), 
                            &session, 
                            NULL, 
                            CALLSTATE_DIALTONE, 
                            CALLSTATE_CAUSE_CONFERENCE) ;   

                    // Issue connect
                    if (pInst)
                    {
                        pLocationHeader = (bEnableLocationHeader) ? pInst->szLocationHeader : NULL;
                    }


                    PtStatus status = pData->pInst->pCallManager->connect(pData->strCallId->data(),
                            szAddress, NULL, sessionId.data(), (CONTACT_ID) contactId, 
                            &pCallData->display, pSecurity ? &pCallData->security : NULL,
                            pLocationHeader, bandWidth) ;
                    if (status == PT_SUCCESS)
                    {
                        rc = SIPX_RESULT_SUCCESS ;
                    }
                    else
                    {
                        sipxFireCallEvent(pData->pInst->pCallManager, 
                                sessionId.data(),
                                &session, 
                                szAddress, 
                                CALLSTATE_DISCONNECTED, 
                                CALLSTATE_CAUSE_BAD_ADDRESS) ;
                        rc = SIPX_RESULT_BAD_ADDRESS ;
                    }
                }
            }
            else if (pData->nCalls < CONF_MAX_CONNECTIONS && 
                    pData->pInst->pCallManager->canAddConnection(pData->strCallId->data()))
            {

                // Only take focus if something doesn't already have it.
                if (!sipxIsCallInFocus() || bTakeFocus)
                {
                    pData->pInst->pCallManager->unholdLocalTerminalConnection(pData->strCallId->data()) ;
                    bSetFocus = true ;
                }

                /*
                 * Use existing call id to find call
                 */
                SIPX_INSTANCE_DATA* pInst ;
                UtlString callId ;
                UtlString lineId ;

                if (sipxCallGetCommonData(pData->hCalls[0], &pInst, &callId, NULL, &lineId))
                {
                    SIPX_CALL_DATA* pNewCallData = new SIPX_CALL_DATA ;
                    memset((void*) pNewCallData, 0, sizeof(SIPX_CALL_DATA));

                    UtlString sessionId ;
                    pData->pInst->pCallManager->getNewSessionId(&sessionId) ;

                    pNewCallData->pInst = pInst ;
                    pNewCallData->hConf = hConf ;
                    pNewCallData->callId = new UtlString(sessionId) ;
                    pNewCallData->remoteAddress = NULL ;
                    pNewCallData->hLine = hLine ;
                    pNewCallData->lineURI = new UtlString(lineId.data()) ;
                    pNewCallData->pMutex = new OsRWMutex(OsRWMutex::Q_FIFO) ;
                    pNewCallData->connectionId = -1;
                    pNewCallData->bInFocus = bSetFocus;
                    if (pSecurity)
                        pNewCallData->security = *pSecurity;

                    SIPX_CALL hNewCall = gpCallHandleMap->allocHandle(pNewCallData) ;
                    pData->hCalls[pData->nCalls++] = hNewCall ;
                    *phNewCall = hNewCall ;

                    pInst->pLock->acquire() ;
                    pInst->nCalls++ ;
                    pInst->pLock->release() ;

                    // Notify Listeners
                    SipSession session ;
                    sipxFireCallEvent(pData->pInst->pCallManager, 
                            sessionId.data(), 
                            &session, 
                            NULL, 
                            CALLSTATE_DIALTONE, 
                            CALLSTATE_CAUSE_CONFERENCE) ;

                    if (pInst)
                    {
                        pLocationHeader = (bEnableLocationHeader) ? pInst->szLocationHeader : NULL;
                    }

                    PtStatus status = pData->pInst->pCallManager->connect(
                            pData->strCallId->data(), szAddress, NULL, 
                            sessionId.data(), (CONTACT_ID) contactId,                                                                           
                            pDisplay, pSecurity ? &pNewCallData->security : NULL, 
                            pLocationHeader, bandWidth) ;
                    if (status == PT_SUCCESS)
                    {
                        rc = SIPX_RESULT_SUCCESS ;
                    }
                    else
                    {
                        sipxFireCallEvent(pData->pInst->pCallManager, 
                                sessionId.data(), 
                                &session, 
                                szAddress, 
                                CALLSTATE_DISCONNECTED, 
                                CALLSTATE_CAUSE_BAD_ADDRESS) ;
                        rc = SIPX_RESULT_BAD_ADDRESS ;
                    }
                }
                else
                {
                    rc = SIPX_RESULT_FAILURE ;
                }
            }
            else
            {
                /*
                 * Hit max connections
                 */
                rc = SIPX_RESULT_OUT_OF_RESOURCES ;
            }

            sipxConfReleaseLock(pData, SIPX_LOCK_WRITE) ;
        }
        else
        {
            rc = SIPX_RESULT_FAILURE ;
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceRemove(const SIPX_CONF hConf,
                                              const SIPX_CALL hCall)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceRemove hConf=%d hCall=%d",
        hConf, hCall);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    if (hConf && hCall)
    {
        SIPX_CONF_DATA* pConfData = sipxConfLookup(hConf, SIPX_LOCK_WRITE) ;
        UtlString callId ;
        UtlString remoteAddress ;
        SIPX_INSTANCE_DATA* pInst ;

        if (pConfData && sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
        {
            sipxRemoveCallHandleFromConf(hConf, hCall) ;
            pInst->pCallManager->dropConnection(callId.data(), remoteAddress.data()) ;

            rc = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            // Either the call or conf doesn't exist
            rc = SIPX_RESULT_FAILURE ;
        }

        sipxConfReleaseLock(pConfData, SIPX_LOCK_WRITE) ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceGetCalls(const SIPX_CONF hConf,
                                                SIPX_CALL hCalls[],
                                                const size_t iMax,
                                                size_t& nActual)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceGetCalls hConf=%d",
        hConf);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    if (hConf && iMax)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData)
        {
            // OsReadLock(*pData->pMutex) ;

            size_t idx ;
            for (idx=0; (idx<pData->nCalls) && (idx < iMax); idx++)
            {
                hCalls[idx] = pData->hCalls[idx] ;
            }
            nActual = idx ;

            rc = SIPX_RESULT_SUCCESS ;

            sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;
        }
        else
        {
            rc = SIPX_RESULT_FAILURE ;
        }
    }
    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceHold(const SIPX_CONF hConf, bool bBridging)
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceHold hConf=%d bBridging=%d",
        hConf,
        bBridging);


    if (hConf)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_WRITE) ;
        if (pData)
        {
            if (bBridging)
            {
                pData->pInst->pCallManager->holdLocalTerminalConnection(pData->strCallId->data());
                pData->confHoldState = CONF_STATE_BRIDGING_HOLD;
            }
            else
            {
                pData->pInst->pCallManager->holdAllTerminalConnections(pData->strCallId->data());
                pData->confHoldState = CONF_STATE_NON_BRIDGING_HOLD;
            }
            sipxConfReleaseLock(pData, SIPX_LOCK_WRITE) ;
            sr = SIPX_RESULT_SUCCESS;
        }
        else
        {
            sr = SIPX_RESULT_FAILURE ;
        }
    }
        
    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceUnhold(const SIPX_CONF hConf)
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceUnHold hConf=%d",
        hConf);

    if (hConf)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData)
        {
            if (pData->confHoldState == CONF_STATE_BRIDGING_HOLD)
            {
                pData->pInst->pCallManager->unholdLocalTerminalConnection(pData->strCallId->data());
                pData->confHoldState = CONF_STATE_UNHELD;
                sr = SIPX_RESULT_SUCCESS;
            }
            else if (pData->confHoldState == CONF_STATE_NON_BRIDGING_HOLD)
            {
                pData->pInst->pCallManager->unholdAllTerminalConnections(pData->strCallId->data());
                pData->confHoldState = CONF_STATE_UNHELD;
                sr = SIPX_RESULT_SUCCESS;
            }
            sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;
        }
        else
        {
            sr = SIPX_RESULT_FAILURE ;
        }
    }
    return sr;
}

SIPXTAPI_API SIPX_RESULT sipxConferencePlayAudioFileStart(const SIPX_CONF hConf,
                                                          const char* szFile,
                                                          const bool bRepeat,
                                                          const bool bLocal,
                                                          const bool bRemote)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferencePlayAudioFileStart hConf=%d File=%s bLocal=%d bRemote=%d bRepeat=%d",
        hConf, szFile, bLocal, bRemote, bRepeat);

    SIPX_RESULT sr = SIPX_RESULT_INVALID_ARGS ;
    
    if (hConf && szFile && (bLocal || bRemote))
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData && pData->pInst)
        {
            pData->pInst->pCallManager->audioPlay(pData->strCallId->data(), szFile, bRepeat, bLocal, bRemote) ;            
            sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;

            sr = SIPX_RESULT_SUCCESS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxConferencePlayAudioFileStop(const SIPX_CONF hConf)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferencePlayAudioFileStop hConf=%d", hConf);

    SIPX_RESULT sr = SIPX_RESULT_INVALID_ARGS ;

    if (hConf)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData && pData->pInst)
        {
            pData->pInst->pCallManager->audioStop(pData->strCallId->data()) ;
            sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;

            sr = SIPX_RESULT_SUCCESS ;
        }
    }

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxConferenceDestroy(SIPX_CONF hConf)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceDestroy hConf=%d",
        hConf);

    SIPX_CALL hCalls[CONF_MAX_CONNECTIONS] ;
    size_t nCalls ;
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    if (hConf)
    {
        // Get a snapshot of the calls, drop the connections, remove the conf handle,
        // and THEN whack the call -- otherwise whacking the calls will force updates
        // into SIPX_CONF_DATA structure (work that isn't needed).
        sipxConferenceGetCalls(hConf, hCalls, CONF_MAX_CONNECTIONS, nCalls) ;
        for (size_t idx=0; idx<nCalls; idx++)
        {
            sipxConferenceRemove(hConf, hCalls[idx]) ;
        }

        sipxConfFree(hConf) ;

        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConferenceGetEnergyLevels(const SIPX_CONF hConf,
                                                       int&            iInputEnergyLevel,
                                                       int&            iOutputEnergyLevel) 
{
    SIPX_RESULT sr = SIPX_RESULT_INVALID_ARGS ;

    if (hConf)
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData)
        {
            if (pData->pInst && pData->pInst->pCallManager && pData->strCallId)
            {
                CallManager* pCallManager = pData->pInst->pCallManager ;
                UtlString callId = *pData->strCallId ;            
                sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;

                if (pCallManager->getAudioEnergyLevels(callId, iInputEnergyLevel, iOutputEnergyLevel))
                {
                    sr = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    sr = SIPX_RESULT_FAILURE ;
                }
            }
            else
            {
                sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;
                sr = SIPX_RESULT_FAILURE ;                
            }            
        }
    }

    return sr ;
}

SIPXTAPI_API SIPX_RESULT sipxConferenceLimitCodecPreferences(const SIPX_CONF hConf,
                                                             const SIPX_AUDIO_BANDWIDTH_ID audioBandwidth,
                                                             const SIPX_VIDEO_BANDWIDTH_ID videoBandwidth,
                                                             const char* szVideoCodecName) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConferenceLimitCodecPreferences hCall=%d audioBandwidth=%d videoBandwidth=%d szVideoCodecName=\"%s\"", 
        hConf,
        audioBandwidth,
        videoBandwidth,
        (szVideoCodecName != NULL) ? szVideoCodecName : "") ;

    SIPX_RESULT sr = SIPX_RESULT_FAILURE;

    UtlString callId ;

    // Test bandwidth for legal values
    if (((audioBandwidth >= AUDIO_CODEC_BW_LOW && audioBandwidth <= AUDIO_CODEC_BW_HIGH) || audioBandwidth == AUDIO_CODEC_BW_DEFAULT) &&
        ((videoBandwidth >= VIDEO_CODEC_BW_LOW && videoBandwidth <= VIDEO_CODEC_BW_HIGH) || videoBandwidth == VIDEO_CODEC_BW_DEFAULT) &&
        (hConf != SIPX_CONF_NULL))
    {
        SIPX_CONF_DATA* pData = sipxConfLookup(hConf, SIPX_LOCK_READ) ;
        if (pData && pData->strCallId)
        {
            UtlString callId = *pData->strCallId ;
            sipxConfReleaseLock(pData, SIPX_LOCK_READ) ;

            pData->pInst->pCallManager->limitCodecPreferences(callId, audioBandwidth, videoBandwidth, szVideoCodecName) ;
            pData->pInst->pCallManager->silentRemoteHold(callId) ;
            pData->pInst->pCallManager->renegotiateCodecsAllTerminalConnections(callId) ;

            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


/****************************************************************************
 * Audio Related Functions
 ***************************************************************************/


static void initMicSettings(MIC_SETTING* pMicSetting)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "initMicSettings micSettings=%p",
        pMicSetting);

    pMicSetting->bInitialized = TRUE ;
    pMicSetting->bMuted = FALSE ;
    pMicSetting->iGain = GAIN_DEFAULT ;
    pMicSetting->device.remove(0) ;
}

static void initSpeakerSettings(SPEAKER_SETTING* pSpeakerSetting)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "initSpeakerSettings speakerSettings=%p",
        pSpeakerSetting);
        
    pSpeakerSetting->bInitialized = TRUE ;
    pSpeakerSetting->iVol = VOLUME_DEFAULT ;
    pSpeakerSetting->device.remove(0) ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioSetGain(const SIPX_INST hInst,
                                          const int iLevel)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioSetGain hInst=%p iLevel=%d",
        hInst, iLevel);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Validate gain is within range
        assert(iLevel >= GAIN_MIN) ;
        assert(iLevel <= GAIN_MAX) ;
        if ((iLevel >= GAIN_MIN) && (iLevel <= GAIN_MAX))
        {
            // Only process if uninitialized (first call) or the state has changed
            if (!pInst->micSetting.bInitialized || (iLevel != pInst->micSetting.iGain))
            {
                OsStatus rc = OS_SUCCESS ;

                // Lazy Init
                if (!pInst->micSetting.bInitialized)
                {
                    initMicSettings(&pInst->micSetting) ;
                    assert(pInst->micSetting.bInitialized) ;
                }

                // Record Gain
                pInst->micSetting.iGain = iLevel ;

                // Set Gain if not muted
                if (!pInst->micSetting.bMuted)
                {                    
                    int iAdjustedGain = (int) ((double)((double)iLevel / (double)GAIN_MAX) * 100.0) ;
                    rc = pInterface->setMicrophoneGain( iAdjustedGain ) ;    
                    assert(rc == OS_SUCCESS) ;                
                }
                
                sr = SIPX_RESULT_SUCCESS ;
            }
            else if (iLevel == pInst->micSetting.iGain)
            {
                sr = SIPX_RESULT_SUCCESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetGain(const SIPX_INST hInst,
                                          int& iLevel)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetGain hInst=%p",
        hInst);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Lazy init
        if (!pInst->micSetting.bInitialized)
        {
            int iGain ;
            
            initMicSettings(&pInst->micSetting) ;
            assert(pInst->micSetting.bInitialized) ;
            
            // Get Live Gain
            OsStatus status = pInterface->getMicrophoneGain(iGain) ;
            assert(status == OS_SUCCESS) ;

            int iAdjustedGain = (int) (double)((((double)iGain / 100.0)) * (double)GAIN_MAX);        
            pInst->micSetting.iGain = iAdjustedGain;          
        }

        iLevel = pInst->micSetting.iGain;
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioMute(const SIPX_INST hInst,
                                       const bool bMute)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioMute hInst=%p bMute=%d",
        hInst, bMute);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Only process if uninitialized (first call) or the state has changed
        if (!pInst->micSetting.bInitialized || bMute != pInst->micSetting.bMuted)
        {
            // Lazy Init
            if (!pInst->micSetting.bInitialized)
            {
                initMicSettings(&pInst->micSetting) ;
                assert(pInst->micSetting.bInitialized) ;
            }

            // Store setting
            pInst->micSetting.bMuted = bMute ;
            if (bMute)
            {
                // Mute gain
                OsStatus rc = pInterface->muteMicrophone(bMute) ;
                assert(rc == OS_SUCCESS);
                sr = SIPX_RESULT_SUCCESS ;
            }
            else
            {
                // UnMute mic
                OsStatus rc = pInterface->muteMicrophone(bMute) ;
                assert(rc == OS_SUCCESS);
                // Restore gain
                // convert from sipXtapi scale to 100 scale
                int iAdjustedGain = (int) (double)((((double)pInst->micSetting.iGain / (double)GAIN_MAX)) * 100.0); 
                rc = pInterface->setMicrophoneGain(iAdjustedGain);
                assert(rc == OS_SUCCESS) ;

                int iGain ;
                
                rc = pInterface->getMicrophoneGain(iGain);

                assert(iGain == pInst->micSetting.iGain);                                             
                if (rc == OS_SUCCESS)
                {
                    sr = SIPX_RESULT_SUCCESS ;
                }
            }
        }
        else if (bMute == pInst->micSetting.bMuted)
        {
            sr = SIPX_RESULT_SUCCESS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioIsMuted(const SIPX_INST hInst,
                                          bool &bMuted)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
        "sipxAudioIsMuted hInst=%p",
        hInst);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        if (!pInst->micSetting.bInitialized)
        {
            initMicSettings(&pInst->micSetting) ;
            assert(pInst->micSetting.bInitialized) ;
        }

        bMuted = pInst->micSetting.bMuted ;

        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxAudioEnableSpeaker(const SIPX_INST hInst,
                                                const SPEAKER_TYPE type)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioEnableSpeaker hInst=%p type=%d",
        hInst, type);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    OsStatus status ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;
    UtlString checkDevice ; 

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (!pInst->speakerSettings[type].bInitialized || (pInst->enabledSpeaker != type))
        {
            pInst->enabledSpeaker = type ;

            // Lazy Init
            if (!pInst->speakerSettings[type].bInitialized)
            {
                initSpeakerSettings(&pInst->speakerSettings[type]) ;
                assert(pInst->speakerSettings[type].bInitialized) ;
            }

            // Lower Volume
            status = pInterface->setSpeakerVolume(0) ;
            assert(status == OS_SUCCESS) ;

            if (status == OS_SUCCESS)
            {
                // Enable Speaker
                switch (type)
                {
                    case SPEAKER:
                    case RINGER:
                        pInterface->setSpeakerDevice(pInst->speakerSettings[type].device) ;                        
                        pInterface->getSpeakerDevice(checkDevice) ;
                        pInst->speakerSettings[type].device = checkDevice;
                        break ;                    
                    default:
                        assert(FALSE) ;
                        break ;
                }
            }

            if (status == OS_SUCCESS)
            {
                // Reset Volume
                SIPX_RESULT rc;
                rc = sipxAudioSetVolume(hInst, type, pInst->speakerSettings[type].iVol);
                assert(rc == SIPX_RESULT_SUCCESS) ;
                int iVolume ;
                rc = sipxAudioGetVolume(hInst, type, iVolume);
                assert(rc == SIPX_RESULT_SUCCESS) ;
                assert(iVolume == pInst->speakerSettings[type].iVol) ;
                if (status == OS_SUCCESS)
                {
                    sr = SIPX_RESULT_SUCCESS ;
                }
            }
        }
        else if (pInst->enabledSpeaker == type)
        {
            sr = SIPX_RESULT_SUCCESS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetEnabledSpeaker(const SIPX_INST hInst,
                                                    SPEAKER_TYPE& type)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetEnabledSpeaker hInst=%p",
        hInst);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        type = pInst->enabledSpeaker ;
        sr = SIPX_RESULT_SUCCESS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioSetVolume(const SIPX_INST hInst,
                                            const SPEAKER_TYPE type,
                                            const int iLevel)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioSetVolume hInst=%p type=%d iLevel=%d",
        hInst, type, iLevel);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(type == SPEAKER || type == RINGER) ;
    assert(iLevel >= VOLUME_MIN) ;
    assert(iLevel <= VOLUME_MAX) ;

    OsSysLog::add(FAC_LOG, PRI_ERR, "DEBUG: Testing callback\n");


    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Validate Params
        if ((type == SPEAKER || type == RINGER) && (iLevel >= VOLUME_MIN) &&
                (iLevel <= VOLUME_MAX))
        {
            // Only process if uninitialized (first call) or the state has changed
            if (!pInst->speakerSettings[type].bInitialized ||
                    pInst->speakerSettings[type].iVol != iLevel)
            {
                // Lazy Init
                if (!pInst->speakerSettings[type].bInitialized)
                {
                    initSpeakerSettings(&pInst->speakerSettings[type]) ;
                    assert(pInst->speakerSettings[type].bInitialized) ;
                }

                // Store value
                pInst->speakerSettings[type].iVol = iLevel ;
                sr = SIPX_RESULT_SUCCESS ;

                // Set value if this type is enabled
                if (pInst->enabledSpeaker == type)
                {
                    // the CpMediaInterfaceFactoryImpl always uses a scale of 0 - 100
                    OsStatus status = pInterface->setSpeakerVolume(iLevel) ;
                    assert(status == OS_SUCCESS) ;
                    int iVolume ;
                    status = pInterface->getSpeakerVolume(iVolume) ;
                    assert(status == OS_SUCCESS) ;
                    assert(iVolume == iLevel) ;
                    if (status != OS_SUCCESS)
                    {
                        sr = SIPX_RESULT_FAILURE ;
                    }
                }
            }
            else if (pInst->speakerSettings[type].iVol == iLevel)
            {
                sr = SIPX_RESULT_SUCCESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetVolume(const SIPX_INST hInst,
                                            const SPEAKER_TYPE type,
                                            int& iLevel)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetVolume hInst=%p type=%d",
        hInst, type);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(type == SPEAKER || type == RINGER) ;

    if (pInst)
    {
        // Validate Params
        if (type == SPEAKER || type == RINGER)
        {
            // Lazy Init
            if (!pInst->speakerSettings[type].bInitialized)
            {
                initSpeakerSettings(&pInst->speakerSettings[type]) ;
                assert(pInst->speakerSettings[type].bInitialized) ;
            }

            iLevel = pInst->speakerSettings[type].iVol ;
            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioSetAECMode(const SIPX_INST hInst,
                                             const SIPX_AEC_MODE mode) 
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxAudioSetAECMode hInst=%p mode=%d",
            hInst, mode);

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface)
        {
            if (pInterface->setAudioAECMode((MEDIA_AEC_MODE) mode) == OS_SUCCESS)
            {
                if (!pInst->aecSetting.bInitialized)
                {
                    pInst->aecSetting.bInitialized = true;
                }
                pInst->aecSetting.mode = mode ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
    }
    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetAECMode(const SIPX_INST hInst,
                                             SIPX_AEC_MODE&  mode)
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
            "sipxAudioGetAECMode hInst=%p",
            hInst);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
               pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface && !pInst->aecSetting.bInitialized)
        {
            MEDIA_AEC_MODE aceMode ;
            if (pInterface->getAudioAECMode(aceMode) == OS_SUCCESS)
            {
                pInst->aecSetting.bInitialized = true;
                pInst->aecSetting.mode = (SIPX_AEC_MODE) aceMode ;
                mode = (SIPX_AEC_MODE) aceMode ;

                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            mode = pInst->aecSetting.mode  ;
            sr = SIPX_RESULT_SUCCESS;
        }
    }
    return sr;
}

// For backward compatibility -- remove in time.
SIPXTAPI_API SIPX_RESULT sipAudioSetAGCMode(const SIPX_INST hInst,
                                            const bool bEnable) 
{    
    return sipxAudioSetAGCMode(hInst, bEnable) ;
} 


SIPXTAPI_API SIPX_RESULT sipxAudioSetAGCMode(const SIPX_INST hInst,
                                            const bool bEnable) 
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxAudioSetAGCMode hInst=%p enable=%d",
            hInst, bEnable);

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface)
        {
            if (pInterface->enableAGC(bEnable) == OS_SUCCESS)
            {
                if (!pInst->agcSetting.bInitialized)
                {
                    pInst->agcSetting.bInitialized = true;
                }
                pInst->agcSetting.bEnabled = bEnable ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
    }
    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetAGCMode(const SIPX_INST hInst,
                                             bool& bEnabled) 
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
            "sipxAudioGetAGCMode hInst=%p",
            hInst);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
               pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface && !pInst->agcSetting.bInitialized)
        {            
            UtlBoolean bCheck ;
            if (pInterface->isAGCEnabled(bCheck) == OS_SUCCESS)
            {
                pInst->agcSetting.bInitialized = true;
                pInst->agcSetting.bEnabled = bCheck ;
                bEnabled = bCheck ;

                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            bEnabled = pInst->agcSetting.bEnabled ;
            sr = SIPX_RESULT_SUCCESS;
        }
    }

    return sr;
}

SIPXTAPI_API SIPX_RESULT sipxAudioSetNoiseReductionMode(const SIPX_INST hInst,
                                                        const SIPX_NOISE_REDUCTION_MODE mode) 
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxAudioSetNoiseReductionMode hInst=%p mode=%d",
            hInst, mode);

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface)
        {
            if (pInterface->setAudioNoiseReductionMode((MEDIA_NOISE_REDUCTION_MODE) mode) == OS_SUCCESS)
            {
                if (!pInst->nrSetting.bInitialized)
                {
                    pInst->nrSetting.bInitialized = true;
                }
                pInst->nrSetting.mode = mode ;
                sr = SIPX_RESULT_SUCCESS;
            }
        }
    }
    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetNoiseReductionMode(const SIPX_INST hInst,
                                                        SIPX_NOISE_REDUCTION_MODE& mode) 
{
    SIPX_RESULT sr = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
            "sipxAudioGetNoiseReductionMode hInst=%p",
            hInst);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
               pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        if (pInterface && !pInst->nrSetting.bInitialized)
        {
            MEDIA_NOISE_REDUCTION_MODE nrMode ;
            if (pInterface->getAudioNoiseReductionMode(nrMode) == OS_SUCCESS)
            {
                pInst->nrSetting.bInitialized = true;
                pInst->nrSetting.mode = (SIPX_NOISE_REDUCTION_MODE) nrMode ;
                mode = (SIPX_NOISE_REDUCTION_MODE) nrMode ;

                sr = SIPX_RESULT_SUCCESS;
            }
        }
        else
        {
            mode = pInst->nrSetting.mode  ;
            sr = SIPX_RESULT_SUCCESS;
        }
    }
    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetNumInputDevices(const SIPX_INST hInst,
                                                     size_t& numDevices)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetNumInputDevices hInst=%p",
        hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        numDevices = 0 ;

        while ( (numDevices < MAX_AUDIO_DEVICES) &&
                (pInst->inputAudioDevices[numDevices] != NULL) )
        {
            numDevices++ ;
        }

        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetInputDevice(const SIPX_INST hInst,
                                                 const int index,
                                                 const char*& szDevice)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetInputDevice hInst=%p index=%d",
        hInst, index);
        
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if ((pInst) && (index >= 0) && (index < MAX_AUDIO_DEVICES))
    {
        szDevice = pInst->inputAudioDevices[index] ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetNumOutputDevices(const SIPX_INST hInst,
                                                      size_t& numDevices)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetNumOutputDevices hInst=%p",
        hInst);
        
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        numDevices = 0 ;

        while ( (numDevices < MAX_AUDIO_DEVICES) &&
                (pInst->outputAudioDevices[numDevices] != NULL) )
        {
            numDevices++ ;
        }

        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioGetOutputDevice(const SIPX_INST hInst,
                                                  const int index,
                                                  const char*& szDevice)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioGetOutputDevice hInst=%p index=%d",
        hInst, index);
        
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if ((pInst) && (index >= 0) && (index < MAX_AUDIO_DEVICES))
    {
        szDevice = pInst->outputAudioDevices[index] ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxAudioSetCallInputDevice(const SIPX_INST hInst,
                                                     const char* szDevice)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioSetCallInputDevice hInst=%p device=%s",
        hInst, szDevice);
        
    UtlString oldDevice ;
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;    
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Get existing device
        OsStatus status = pInterface->getMicrophoneDevice(oldDevice) ;
        assert(status == OS_SUCCESS) ;

        // Lazy Init
        if (!pInst->micSetting.bInitialized)
        {
            initMicSettings(&pInst->micSetting) ;
            assert(pInst->micSetting.bInitialized) ;
        }

        if (strcasecmp(szDevice, "NONE") == 0)
        {
            pInst->micSetting.device = "NONE" ;
            status = pInterface->setMicrophoneDevice(pInst->micSetting.device) ;            
            assert(status == OS_SUCCESS) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            for (int i=0; i<MAX_AUDIO_DEVICES; i++)
            {
                if (pInst->inputAudioDevices[i])
                {
                    if (strcmp(szDevice, pInst->inputAudioDevices[i]) == 0)
                    {
                        // Match
                        if (strcmp(szDevice, oldDevice) != 0)
                        {
                            pInst->micSetting.device = szDevice ;
                            status = pInterface->setMicrophoneDevice(pInst->micSetting.device) ;
                            // GIPS returns -1 on the call to set audio input device, no matter what
                            //assert(status == OS_SUCCESS) ;
                        }
                        rc = SIPX_RESULT_SUCCESS ;
                        break ;
                    }
                }
                else
                {
                    // Hit end of list, might was well jump
                    break ;
                }
            }
        }
    }

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxAudioSetRingerOutputDevice(const SIPX_INST hInst,
                                                        const char* szDevice)
{    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioSetRingerOutputDevice hInst=%p device=%s",
        hInst, szDevice);
        
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;
    UtlString oldDevice ; 

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Lazy Init settings
        if (!pInst->speakerSettings[RINGER].bInitialized)
        {
            initSpeakerSettings(&pInst->speakerSettings[RINGER]) ;
            assert(pInst->speakerSettings[RINGER].bInitialized) ;
        }

        if (strcasecmp(szDevice, "NONE") == 0)
        {
            pInst->speakerSettings[RINGER].device = "NONE" ;
            rc = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            for (int i=0; i<MAX_AUDIO_DEVICES; i++)
            {
                if (pInst->outputAudioDevices[i])
                {
                    if (strcmp(szDevice, pInst->outputAudioDevices[i]) == 0)
                    {
                        oldDevice = pInst->speakerSettings[RINGER].device ;
                        pInst->speakerSettings[RINGER].device = szDevice ;
                        rc = SIPX_RESULT_SUCCESS ;
                        break ;
                    }
                }
                else
                {
                    // Hit end of list, might was well jump
                    break ;
                }
            }
        }

        // Set the device if it changed and this is the active device group
        if ((pInst->enabledSpeaker == RINGER) && 
                (pInst->speakerSettings[RINGER].device.compareTo(oldDevice) != 0))
        {
            if (pInterface->setSpeakerDevice(pInst->speakerSettings[RINGER].device) == OS_FAILED)
            {
                rc = SIPX_RESULT_FAILURE;
            }
        }       
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxAudioSetCallOutputDevice(const SIPX_INST hInst,
                                                      const char* szDevice)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxAudioSetCallOutputDevice hInst=%p device=%s",
        hInst, szDevice);
        
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;
    UtlString oldDevice ; 

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;

        // Lazy Init settings
        if (!pInst->speakerSettings[SPEAKER].bInitialized)
        {
            initSpeakerSettings(&pInst->speakerSettings[SPEAKER]) ;
            assert(pInst->speakerSettings[SPEAKER].bInitialized) ;
        }

        if (strcasecmp(szDevice, "NONE") == 0)
        {
            pInst->speakerSettings[SPEAKER].device = "NONE" ;
            rc = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            for (int i=0; i<MAX_AUDIO_DEVICES; i++)
            {
                if (pInst->outputAudioDevices[i])
                {
                    if (strcmp(szDevice, pInst->outputAudioDevices[i]) == 0)
                    {
                        oldDevice = pInst->speakerSettings[SPEAKER].device ;
                        pInst->speakerSettings[SPEAKER].device = szDevice ;
                        rc = SIPX_RESULT_SUCCESS ;
                        break ;
                    }
                }
                else
                {
                    // Hit end of list, might was well jump
                    break ;
                }
            }
        }

        // Set the device if it changed and this is the active device group
        if ((pInst->enabledSpeaker == SPEAKER) && 
                (pInst->speakerSettings[SPEAKER].device.compareTo(oldDevice) != 0))
        {
            if (pInterface->setSpeakerDevice(pInst->speakerSettings[SPEAKER].device) == OS_FAILED)
            {
                rc = SIPX_RESULT_FAILURE;
            }

        }
    }

    return rc ;
}


/****************************************************************************
 * Line Related Functions
 ***************************************************************************/

SIPXTAPI_API SIPX_RESULT sipxLineAdd(const SIPX_INST hInst,
                                     const char* szLineUrl,
                                     SIPX_LINE* phLine,
                                     SIPX_CONTACT_ID contactId)

{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineAdd hInst=%p lineUrl=%s, phLine=%p contactId=%d",
        hInst, szLineUrl, phLine, contactId);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(szLineUrl != NULL) ;
    assert(phLine != NULL) ;


    if (pInst)
    {
        if (szLineUrl && phLine)
        {
            UtlString userId ;
            Url url(szLineUrl) ;
            Url uri = url ;
            uri.removeFieldParameters() ;
            uri.removeHeaderParameters() ;
            url.getUserId(userId) ;
            
            SipLine line(url, uri, userId) ;

            // Set the preferred contact
            Url uriPreferredContact ;
            CONTACT_ADDRESS* pContact = NULL;
            CONTACT_TYPE contactType = AUTO;
            
            pContact = pInst->pSipUserAgent->getContactDb().find(contactId);
            if (pContact)
            {
                contactType = pContact->eContactType;            
            }
            SIPX_TRANSPORT_TYPE sipx_protocol = TRANSPORT_UDP;

            UtlString lineUrl(szLineUrl);
            if (lineUrl.contains("sips:") || lineUrl.contains("transport=tls"))
            {
                sipx_protocol = TRANSPORT_TLS;
            }
            else if (lineUrl.contains("transport=tcp"))
            {
                sipx_protocol = TRANSPORT_TCP;
            }
            sipxGetContactHostPort(pInst, (SIPX_CONTACT_TYPE)contactType, uriPreferredContact, sipx_protocol) ;
            line.setPreferredContactUri(uriPreferredContact) ;

            UtlBoolean bRC = pInst->pLineManager->addLine(line, false) ;
            if (bRC)
            {
                SIPX_LINE_DATA* pData = createLineData(pInst, uri) ;
                if (pData != NULL)
                {
                    pData->contactType = (SIPX_CONTACT_TYPE) contactType ;
                    *phLine = gpLineHandleMap->allocHandle(pData) ;
                    sr = SIPX_RESULT_SUCCESS ;

                    pInst->pLineManager->setStateForLine(uri, SipLine::LINE_STATE_PROVISIONED) ;
                    sipxFireLineEvent(pInst->pRefreshManager, 
                            szLineUrl, 
                            LINESTATE_PROVISIONED, 
                            LINESTATE_PROVISIONED_NORMAL);
                }
                else
                {
                    sr = SIPX_RESULT_OUT_OF_MEMORY ;
                }
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxLineAddAlias(const SIPX_LINE hLine, const char* szLineURL) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineAddAlias hLine=%d szLineURL=%s",
        hLine, szLineURL);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    if (hLine)
    {
        SIPX_LINE_DATA* pData = sipxLineLookup(hLine, SIPX_LOCK_WRITE) ;
        if (pData)
        {
            if (pData->pLineAliases == NULL)
            {
                pData->pLineAliases = new UtlSList() ;
            }

            Url url(szLineURL) ;
            UtlString strURI;
            url.getUri(strURI) ;
            Url uri(strURI) ;
            UtlString userId ;
            url.getUserId(userId) ;
            UtlString displayName;
            url.getDisplayName(displayName);
            uri.setDisplayName(displayName);

            pData->pLineAliases->append(new UtlVoidPtr(new Url(uri))) ;
            
            sipxLineReleaseLock(pData, SIPX_LOCK_WRITE) ;

            sr = SIPX_RESULT_SUCCESS ;
        }                
    }

    return sr;
}

SIPXTAPI_API SIPX_RESULT sipxLineRegister(const SIPX_LINE hLine, const bool bRegister)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineRegister hLine=%d bRegister=%d",
        hLine, bRegister);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    if (hLine)
    {
        SIPX_LINE_DATA* pData = sipxLineLookup(hLine, SIPX_LOCK_READ) ;
        if (!pData)
            return sr;

        if (bRegister)
        {
            pData->pInst->pLineManager->enableLine(*pData->lineURI);
        }
        else
        {
            pData->pInst->pLineManager->disableLine(*pData->lineURI,
                                                    false,
                                                    pData->lineURI->toString());//  ->unRegisterUser(*pData->lineURI, false, pData->lineURI->toString());
        }
        sr = SIPX_RESULT_SUCCESS ;
        sipxLineReleaseLock(pData, SIPX_LOCK_READ) ;
    }

    return sr;
}


SIPXTAPI_API SIPX_RESULT sipxLineRemove(SIPX_LINE hLine)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineRemove hLine=%d",
        hLine);    
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;

    if (hLine)
    {
        SIPX_LINE_DATA* pData = sipxLineLookup(hLine, SIPX_LOCK_WRITE) ;
        if (pData)
        {
            pData->pInst->pLineManager->deleteLine(*pData->lineURI) ;
            sipxLineReleaseLock(pData, SIPX_LOCK_WRITE) ;
            sipxLineObjectFree(hLine) ;
            
            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_FAILURE ;
        }            
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxLineAddCredential(const SIPX_LINE hLine,                                                 
                                                const char* szUserID,
                                                const char* szPasswd,
                                                const char* szRealm)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineAddCredential hLine=%d userId=%s realm=%s",
        hLine, szUserID, szRealm);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_LINE_DATA* pData = sipxLineLookup(hLine, SIPX_LOCK_READ) ;
    if (pData)
    {
        if (szUserID && szPasswd && szRealm)
        {
            UtlBoolean rc = pData->pInst->pLineManager->addCredentialForLine(*pData->lineURI,
                    szRealm,
                    szUserID,
                    szPasswd,
                    HTTP_DIGEST_AUTHENTICATION) ;

            assert(rc) ;
            if (rc)
            {
                sr = SIPX_RESULT_SUCCESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
        sipxLineReleaseLock(pData, SIPX_LOCK_READ) ;
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}



SIPXTAPI_API SIPX_RESULT sipxLineGet(const SIPX_INST hInst,
                                     SIPX_LINE lines[],
                                     const size_t max,
                                     size_t& actual)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineGet hInst=%p",
        hInst);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;
    actual = 0 ;

    if (pInst)
    {
        SipLine* pLines = new SipLine[max] ;
        int iActual ;

        assert(pLines) ;
        if (pLines)
        {
            pInst->pLineManager->getLines(max, iActual, pLines) ;    // rc is > 0 lines (useless)
            if (iActual > 0)
            {
                actual = (size_t) iActual ;
                for (size_t i=0; i<actual; i++)
                {
                    lines[i] = sipxLineLookupHandleByURI(pLines[i].getIdentity().toString()) ;
                }
            }
            delete [] pLines ;            
            sr = SIPX_RESULT_SUCCESS ;
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxLineGetURI(const SIPX_LINE hLine,
                                        char*  szBuffer,
                                        const size_t nBuffer,
                                        size_t& nActual)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxLineGetURI hLine=%d",
        hLine);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_LINE_DATA* pData = sipxLineLookup(hLine, SIPX_LOCK_READ) ;
    if (pData)
    {
        assert(pData->lineURI != NULL) ;

        if (pData)
        {
            if (pData->lineURI)
            {
                if (szBuffer)
                {
                    strncpy(szBuffer, pData->lineURI->toString().data(), nBuffer) ;

                    // Make sure it is null terminated
                    szBuffer[nBuffer-1] = 0 ;
                    nActual = strlen(szBuffer) + 1 ;
                }
                else
                {
                    nActual = strlen(pData->lineURI->toString().data()) + 1;
                }

                sr = SIPX_RESULT_SUCCESS ;
            }
        }
        else
        {
            sr = SIPX_RESULT_INVALID_ARGS ;
        }

        sipxLineReleaseLock(pData, SIPX_LOCK_READ) ;
    }

    return sr ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetLogLevel(SIPX_LOG_LEVEL logLevel) 
{
    // Start up logger thread
    initLogger() ;

    logLevel = (logLevel == LOG_LEVEL_NONE) ? LOG_LEVEL_EMERG : logLevel;
    OsSysLog::setLoggingPriority((const enum tagOsSysLogPriority) logLevel) ;  

    return SIPX_RESULT_SUCCESS ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetLogFile(const char* szFilename)
{
    OsSysLog::setOutputFile(0, szFilename) ;

    return SIPX_RESULT_SUCCESS ;
}

/*RL*/
SIPXTAPI_API SIPX_RESULT sipxConfigEnableConsoleOutput(bool outputToConsole, FILE* outputToFile) {
	enableFileOutput(outputToFile);
	enableConsoleOutput(outputToConsole);
	return SIPX_RESULT_SUCCESS ;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetLogCallback(sipxLogCallback pCallback)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    if ( OsSysLog::setCallbackFunction(pCallback) == OS_SUCCESS )
    {
        rc = SIPX_RESULT_SUCCESS;
    }
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigEnableGIPSTracing(SIPX_INST hInst, 
                                                     bool bEnable) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigEnableGIPSTracing hInst=%p bEnable=%d", hInst, bEnable);

#ifdef VOICE_ENGINE
    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    if (pInst)
    {
        VoiceEngineFactoryImpl* pInterface = dynamic_cast<VoiceEngineFactoryImpl*>(
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation());
        if (pInterface)
        {
            pInterface->setGipsTracing(bEnable) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
    }
    else
    {
        rc = SIPX_RESULT_INVALID_ARGS ;
    }
#else
    SIPX_RESULT rc = SIPX_RESULT_NOT_IMPLEMENTED ;
#endif

    return rc ;      
}



SIPXTAPI_API SIPX_RESULT sipxConfigSetMicAudioHook(fnMicAudioHook hookProc) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetMicAudioHook hookProc=%p",
        hookProc);
//    MprFromMic::s_fnMicDataHook = hookProc ;
// TODO - call MediaInterface for hook data
    return SIPX_RESULT_SUCCESS ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetSpkrAudioHook(fnSpkrAudioHook hookProc)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetSpkrAudioHook hookProc=%p",
        hookProc);
//    MprToSpkr::s_fnToSpeakerHook = hookProc ;
// TODO - call MediaInterface for hook data

    return SIPX_RESULT_SUCCESS ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetOutboundProxy(const SIPX_INST hInst,
                                                    const char* szProxy)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetOutboundProxy hInst=%p proxy=%s",
        hInst, szProxy);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setProxyServers(szProxy) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetDnsSrvTimeouts(const int initialTimeoutInSecs,
                                                     const int retries)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetDnsSrvTimeouts initialTimeoutInSecs=%d retries=%d",
        initialTimeoutInSecs, retries);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    assert(initialTimeoutInSecs > 0) ;
    assert(retries > 0) ;

    if ((initialTimeoutInSecs > 0) && (retries > 0))
    {
        SipSrvLookup::setDnsSrvTimeouts(initialTimeoutInSecs, retries) ;
        rc = SIPX_RESULT_SUCCESS ;
    }
    /*
    else
    {
        SipSrvLookup::setOption(OptionCodeIgnoreSRV, 1);
        SipSvrLookup::setDnsSrvTimeouts(
    }
    */

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetRegisterResponseWaitSeconds(const SIPX_INST hInst,
                                                                  const int seconds)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetRegisterResponseWaitSeconds hInst=%p seconds=%d",
        hInst, seconds);
        
    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;

    if (hInst)
    {
        SIPX_INSTANCE_DATA* pData = (SIPX_INSTANCE_DATA*) hInst;
        
        if (pData->pSipUserAgent)
        {
            pData->pSipUserAgent->setRegisterResponseTimeout(seconds);
            rc = SIPX_RESULT_SUCCESS;
        }
    }
    
    return rc;
}   
SIPXTAPI_API SIPX_RESULT sipxConfigEnableRport(const SIPX_INST hInst,
                                               const bool bEnable)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableRport hInst=%p bEnable=%d",
        hInst, bEnable);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setUseRport(bEnable) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetUserAgentName(const SIPX_INST hInst,
                                                    const char* szName,
                                                    const bool bIncludePlatform)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetUserAgentName hInst=%p szName=%s",
        hInst, szName);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setIncludePlatformInUserAgentName(bIncludePlatform);
            pInst->pSipUserAgent->setUserAgentName(szName) ;
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetRegisterExpiration(const SIPX_INST hInst,
                                                      const int nRegisterExpirationSecs)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetRegisterExpiration hInst=%p seconds=%d",
        hInst, nRegisterExpirationSecs);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    assert(pInst);
    if (pInst)
    {
        assert(pInst->pRefreshManager);
        if (pInst->pRefreshManager)
        {
            pInst->pRefreshManager->setRegistryPeriod(nRegisterExpirationSecs);
            rc = SIPX_RESULT_SUCCESS;
        }
    }

    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetSubscribeExpiration(const SIPX_INST hInst,
                                                          const int nSubscribeExpirationSecs)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetSubscribeExpiration hInst=%p seconds=%d",
        hInst, nSubscribeExpirationSecs);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    assert(pInst);
    if (pInst)
    {
        assert(pInst->pRefreshManager);
        if (pInst->pRefreshManager)
        {
            pInst->pRefreshManager->setSubscribeTimeout(nSubscribeExpirationSecs);
            rc = SIPX_RESULT_SUCCESS;
        }
    }

    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetDnsSrvFailoverTimeout(const SIPX_INST hInst, const int failoverTimeoutInSecs)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetDnsSrvFailoverTimeout hInst=%p seconds=%d",
        hInst, failoverTimeoutInSecs);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;

    assert(pInst) ;
    if (pInst)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setDnsSrvTimeout(failoverTimeoutInSecs);
            rc = SIPX_RESULT_SUCCESS ;
        }
    }

    return rc ;
}



SIPXTAPI_API SIPX_RESULT sipxConfigEnableStun(const SIPX_INST hInst,
                                              const char* szServer,
                                              int iServerPort,
                                              int iKeepAliveInSecs)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableStun hInst=%p server=%s:%d keepalive=%d",
        hInst, szServer, iServerPort, iKeepAliveInSecs);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;
    assert(pInst);
    if (pInst)
    {
        // A bit of hackery; If someone calls this multiple times
        // while a STUN response is outstanding, we don't want to
        // whack the existing pNotification (cause we will likely
        // crash).  So, only create/add a new notification object
        // if the current one is NULL.  The notification object
        // is only created here and destroy on reception of the 
        // event and/or destruction of the SIPX_INST handle.
        OsNotification* pNotification = pInst->pStunNotification ;
        if (pNotification == NULL)
        {
            pNotification = new OsQueuedEvent(*pInst->pMessageObserver->getMessageQueue(), SIPXMO_NOTIFICATION_STUN) ;
            pInst->pStunNotification = pNotification ;
        }
        else 
        {
            pNotification = NULL ;
        }
      
        pInst->pCallManager->enableStun(szServer, iServerPort, iKeepAliveInSecs, pNotification) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigDisableStun(const SIPX_INST hInst)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigDisableStun hInst=%p",
        hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    assert(pInst);
    if (pInst)
    {
        pInst->pCallManager->enableStun(NULL, PORT_NONE, 0) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigEnableTurn(const SIPX_INST hInst,
                                              const char*     szServer,
                                              const int       iServerPort,
                                              const char*     szUsername,
                                              const char*     szPassword,
                                              const int       iKeepAliveInSecs) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableTurn hInst=%p server=%s:%d keepalive=%d",
        hInst, szServer, iServerPort, iKeepAliveInSecs);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;
    assert(pInst);
    if (pInst)
    {      
        pInst->pCallManager->enableTurn(szServer, iServerPort, szUsername, 
                szPassword, iKeepAliveInSecs) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;    
}


SIPXTAPI_API SIPX_RESULT sipxConfigDisableTurn(const SIPX_INST hInst) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigDisableTurn hInst=%p",
        hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;
    assert(pInst);
    if (pInst)
    {
        pInst->pCallManager->enableTurn(NULL, PORT_NONE, NULL, NULL, 0) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigEnableIce(const SIPX_INST hInst)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableICE hInst=%p", hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;
    assert(pInst);
    if (pInst)
    {      
        pInst->pCallManager->enableIce(true) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;    
}


SIPXTAPI_API SIPX_RESULT sipxConfigDisableIce(const SIPX_INST hInst) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigDisableICE hInst=%p", hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;
    assert(pInst);
    if (pInst)
    {
        pInst->pCallManager->enableIce(false) ;
        rc = SIPX_RESULT_SUCCESS ;
    }

    return rc ;    

}


SIPXTAPI_API SIPX_RESULT sipxConfigKeepAliveAdd(const SIPX_INST     hInst,
                                                SIPX_CONTACT_ID     contactId,
                                                SIPX_KEEPALIVE_TYPE type,
                                                const char*         remoteIp,
                                                int                 remotePort,
                                                int                 keepAliveSecs) 
{
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;
    UtlString localSocket = "0.0.0.0" ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigKeepAliveAdd hInst=%p type=%d target=%s:%d keepAlive=%d",
            hInst,
            type,
            remoteIp ? remoteIp : "<NULL>",
            remotePort,
            keepAliveSecs) ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    assert(pInst);
    assert(remoteIp) ;
    assert(remotePort > 0) ;
    assert(type >= SIPX_KEEPALIVE_CRLF && type <= SIPX_KEEPALIVE_SIP_PING) ;
    assert(keepAliveSecs > -2) ;

    if (pInst)
    {
        CONTACT_ADDRESS* pContact = NULL;        
        if (contactId > 0)
        {
            pContact = pInst->pSipUserAgent->getContactDb().getLocalContact(contactId);
            if (pContact)
            {
                localSocket = pContact->cIpAddress ;
            }
        }

        switch (type)
        {
            case SIPX_KEEPALIVE_CRLF:
                if (pInst->pSipUserAgent->addCrLfKeepAlive(localSocket, 
                        remoteIp, remotePort, keepAliveSecs))
                {
                    rc = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    rc = SIPX_RESULT_FAILURE ;
                }
                break ;
            case SIPX_KEEPALIVE_STUN:
                if (pInst->pSipUserAgent->addStunKeepAlive(localSocket, 
                        remoteIp, remotePort, keepAliveSecs))
                {
                    rc = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    rc = SIPX_RESULT_FAILURE ;
                }
                break ;
            case SIPX_KEEPALIVE_SIP_PING:
                rc = SIPX_RESULT_NOT_IMPLEMENTED ;
                break ;
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigKeepAliveRemove(const SIPX_INST     hInst,
                                                   SIPX_CONTACT_ID     contactId,
                                                   SIPX_KEEPALIVE_TYPE type,
                                                   const char*         remoteIp,
                                                   int                 remotePort) 
{
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;
    UtlString localSocket = "0.0.0.0" ;

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigKeepAliveRemove hInst=%p target=%s:%d",
            hInst,
            remoteIp ? remoteIp : "<NULL>",
            remotePort) ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst;

    if (pInst && remoteIp && remotePort > 0)
    {
        CONTACT_ADDRESS* pContact = NULL;        
        if (contactId > 0)
        {
            pContact = pInst->pSipUserAgent->getContactDb().getLocalContact(contactId);
            if (pContact)
            {
                localSocket = pContact->cIpAddress ;
            }
        }

        switch (type)
        {
            case SIPX_KEEPALIVE_CRLF:
                if (pInst->pSipUserAgent->removeCrLfKeepAlive(localSocket, 
                        remoteIp, remotePort))
                {
                    rc = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    rc = SIPX_RESULT_FAILURE ;
                }
                break ;
            case SIPX_KEEPALIVE_STUN:
                if (pInst->pSipUserAgent->removeStunKeepAlive(localSocket, 
                        remoteIp, remotePort))
                {
                    rc = SIPX_RESULT_SUCCESS ;
                }
                else
                {
                    rc = SIPX_RESULT_FAILURE ;
                }
                break ;
            case SIPX_KEEPALIVE_SIP_PING:
                rc = SIPX_RESULT_NOT_IMPLEMENTED ;
                break ;
        }
    }
    else
    {
        rc = SIPX_RESULT_INVALID_ARGS;
    }

    return rc ;
}

SIPXTAPI_API SIPX_RESULT sipxConfigGetVersion(char* szVersion,
											  const size_t nBuffer)
{
    SIPX_RESULT rc = SIPX_RESULT_INSUFFICIENT_BUFFER;
    size_t nLen;

    if (szVersion)
    {
        memset(szVersion, 0, nBuffer);
        // Determine length of version string 
        nLen = (strlen(SIPXTAPI_VERSION_STRING)-8) +
               strlen(SIPXTAPI_VERSION) + 
               strlen(SIPXTAPI_BUILDNUMBER) + 
               strlen(SIPXTAPI_BUILDDATE) + 4;
        if (nLen <= nBuffer)
        {
            sprintf(szVersion, SIPXTAPI_VERSION_STRING, SIPXTAPI_VERSION,
                                                        SIPXTAPI_BUILDNUMBER,
#ifdef _DEBUG
                                                        "Dbg",
#else
                                                        "Rls",
#endif
                                                        SIPXTAPI_BUILDDATE);

            rc = SIPX_RESULT_SUCCESS;
        }
    }
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigGetLocalSipUdpPort(SIPX_INST hInst, int* pPort) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetLocalSipUdpPort hInst=%p",
        hInst);
        
    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;    
    if (pInst && pPort)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            *pPort = pInst->pSipUserAgent->getUdpPort() ;
            if (portIsValid(*pPort))
            {
                rc = SIPX_RESULT_SUCCESS ;
            }
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetLocalSipTcpPort(SIPX_INST hInst, int* pPort) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetLocalSipTcpPort hInst=%p",
        hInst);

    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;    
    if (pInst && pPort)
    {
        rc = SIPX_RESULT_FAILURE ;
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            *pPort = pInst->pSipUserAgent->getTcpPort() ;
            if (portIsValid(*pPort))
            {
                rc = SIPX_RESULT_SUCCESS ;
            }
        }
    }

    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetLocalSipTlsPort(SIPX_INST hInst, int* pPort) 
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetLocalSipTlsPort hInst=%p",
        hInst);

    SIPX_RESULT rc = SIPX_RESULT_FAILURE ;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;    
    if (pInst && pPort)
    {
        assert(pInst->pSipUserAgent) ;
        if (pInst->pSipUserAgent)
        {
            *pPort = pInst->pSipUserAgent->getTlsPort() ;
            if (portIsValid(*pPort))
            {
                rc = SIPX_RESULT_SUCCESS ;
            }
        }
    }
    return rc ;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetAudioCodecPreferences(const SIPX_INST hInst, 
                                                            const SIPX_AUDIO_BANDWIDTH_ID bandWidth)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetAudioCodecPreferences hInst=%p bandWidth=%d",
            hInst, bandWidth);

    if (pInst)
    {
        int numCodecs;
        SdpCodec** codecsArray = NULL;
        UtlString codecName;
        int iRejected;

        // Check if bandwidth is legal, do not allow variable bandwidth
        if (bandWidth >= AUDIO_CODEC_BW_LOW && bandWidth <= AUDIO_CODEC_BW_HIGH)
        {
            CpMediaInterfaceFactoryImpl* pInterface = 
                    pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

            pInst->audioCodecSetting.sPreferences = "";

            if (pInterface)
            {
                int codecIndex;

                /* Unconditionally rebuild codec factory with all supported codecs. If we 
                * don't do this first then only the previously preferred codecs will be used to
                * build the new factory -> that doesn't work for changing from a lower bandwidth to
                * a higher bandwidth.
                */
                pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                              "", // No audio preferences
                                              pInst->videoCodecSetting.sPreferences, // Keep video prefs
                                              -1, // Allow all formats
                                              &iRejected);

                // Now pick preferences out of all available codecs
                pInst->pCodecFactory->getCodecs(numCodecs, codecsArray, "audio");

                OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                              "sipxConfigSetAudioCodecPreferences number of Codec = %d for hInst=%p",
                              numCodecs, hInst);
                              
                for (int i=0; i<numCodecs; i++)
                {
                    if (codecsArray[i]->getBWCost() <= bandWidth)
                    {
                        if (pInterface->getCodecNameByType(codecsArray[i]->getCodecType(), codecName) == OS_SUCCESS)
                        {
                            pInst->audioCodecSetting.sPreferences = 
                                pInst->audioCodecSetting.sPreferences + " " + codecName;
                        }
                    }
                }
                OsSysLog::add(FAC_SIPXTAPI, PRI_DEBUG,
                        "sipxConfigSetAudioCodecPreferences: %s", pInst->audioCodecSetting.sPreferences.data());

                if (pInst->audioCodecSetting.sPreferences.length() != 0)
                {
                    // Did we previously allocate a codecs array and store it in our settings?
                    if (pInst->audioCodecSetting.bInitialized)
                    {
                        // Free up the previuosly allocated codecs and the array
                        for (codecIndex = 0; codecIndex < pInst->audioCodecSetting.numCodecs; codecIndex++)
                        {
                            if (pInst->audioCodecSetting.sdpCodecArray[codecIndex])
                            {
                                delete pInst->audioCodecSetting.sdpCodecArray[codecIndex];
                                pInst->audioCodecSetting.sdpCodecArray[codecIndex] = NULL;
                            }
                        }
                        delete[] pInst->audioCodecSetting.sdpCodecArray;
                        pInst->audioCodecSetting.sdpCodecArray = NULL;
                    }
                    pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                                  pInst->audioCodecSetting.sPreferences,
                                                  pInst->videoCodecSetting.sPreferences,
                                                  -1, // Allow all formats
                                                  &iRejected);

                    // We've rebuilt the factory, so get the new count of codecs
                    pInst->pCodecFactory->getCodecs(pInst->audioCodecSetting.numCodecs,
                                                    pInst->audioCodecSetting.sdpCodecArray,
                                                    "audio");
                    pInst->audioCodecSetting.fallBack = bandWidth;
                    pInst->audioCodecSetting.codecPref = bandWidth;
                    pInst->audioCodecSetting.bInitialized = true;
                    rc = SIPX_RESULT_SUCCESS;
                }
                else
                {
                    // Resetting the codec preferences failed but we've already rebuilt the factory
                    // with all codecs - go to the fallback preferences and rebuild again but return failure
                    OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                            "sipxConfigSetAudioCodecPreferences: Setting %d failed, falling back to preference %d", 
                            bandWidth, pInst->audioCodecSetting.fallBack);
                    sipxConfigSetAudioCodecPreferences(hInst, pInst->audioCodecSetting.fallBack);
                }

                // Free up the codecs and the array
                for (codecIndex = 0; codecIndex < numCodecs; codecIndex++)
                {
                    delete codecsArray[codecIndex];
                    codecsArray[codecIndex] = NULL;
                }
                delete[] codecsArray;
                codecsArray = NULL;
            }
        }
        else
        {
            rc = SIPX_RESULT_INVALID_ARGS ;
        }
    }

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetAudioCodecByName(const SIPX_INST hInst, 
                                                       const char* szCodecName)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetAudioCodecByName hInst=%p codec=%s",
            hInst, szCodecName);

    if (pInst)
    {
        int iRejected;

        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

        pInst->audioCodecSetting.sPreferences = szCodecName;
        pInst->audioCodecSetting.sPreferences += " audio/telephone-event";

        if (pInterface)
        {
            if (pInst->audioCodecSetting.sPreferences.length() != 0)
            {
                // Did we previously allocate a codecs array and store it in our settings?
                if (pInst->audioCodecSetting.bInitialized)
                {
                    int codecIndex;

                    // Free up the previuosly allocated codecs and the array
                    for (codecIndex = 0; codecIndex < pInst->audioCodecSetting.numCodecs; codecIndex++)
                    {
                        if (pInst->audioCodecSetting.sdpCodecArray[codecIndex])
                        {
                            delete pInst->audioCodecSetting.sdpCodecArray[codecIndex];
                            pInst->audioCodecSetting.sdpCodecArray[codecIndex] = NULL;
                        }
                    }
                    delete[] pInst->audioCodecSetting.sdpCodecArray;
                    pInst->audioCodecSetting.sdpCodecArray = NULL;
                }
                pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                              pInst->audioCodecSetting.sPreferences,
                                              pInst->videoCodecSetting.sPreferences,
                                              -1, // Allow all formats
                                              &iRejected);

                // We've rebuilt the factory, so get the new count of codecs
                pInst->pCodecFactory->getCodecs(pInst->audioCodecSetting.numCodecs,
                                                pInst->audioCodecSetting.sdpCodecArray,
                                                "audio");
                if (pInst->audioCodecSetting.numCodecs > 1)
                {
                    pInst->audioCodecSetting.codecPref = AUDIO_CODEC_BW_CUSTOM;
                    rc = SIPX_RESULT_SUCCESS;
                }
                else
                {
                    // Codec setting by name failed - we have an empty (except for DTMF) codec factory.
                    // Fall back to previously set bandwidth Id
                    OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                            "sipxConfigSetAudioCodecByName: Setting %s failed, falling back to preference %d", 
                            szCodecName, pInst->audioCodecSetting.fallBack);
                    sipxConfigSetAudioCodecPreferences(hInst, pInst->audioCodecSetting.fallBack);
                }
                pInst->audioCodecSetting.bInitialized = true;

            }

        }
    }

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetAudioCodecPreferences(const SIPX_INST hInst, 
                                                            SIPX_AUDIO_BANDWIDTH_ID *pBandWidth)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    if (pInst && pInst->audioCodecSetting.bInitialized)
    {
        *pBandWidth = pInst->audioCodecSetting.codecPref;
        rc = SIPX_RESULT_SUCCESS;
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetAudioCodecPreferences hInst=%p bandWidth=%d",
        hInst, *pBandWidth);

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetNumAudioCodecs(const SIPX_INST hInst, 
                                                     int* pNumCodecs)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;    

    if (pInst && pNumCodecs)
    {
        assert(pInst->audioCodecSetting.bInitialized);

        if (pInst->audioCodecSetting.bInitialized)
        {
            *pNumCodecs = pInst->audioCodecSetting.numCodecs;
            rc = SIPX_RESULT_SUCCESS;
        }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetNumAudioCodecs hInst=%p numCodecs=%d",
        hInst, *pNumCodecs);

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetAudioCodec(const SIPX_INST hInst, 
                                                 const int index, 
                                                 SIPX_AUDIO_CODEC* pCodec)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    UtlString codecName;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    assert(pCodec);

    if (pInst && pCodec)
    {
        assert(pInst->audioCodecSetting.bInitialized);

        memset((void*)pCodec, 0, sizeof(SIPX_AUDIO_CODEC));
        if (index >= 0 && index < pInst->audioCodecSetting.numCodecs)
        {
            CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

            // If a name is found for the codec type, copy name and bandwidth cost
            if (pInterface->getCodecNameByType(pInst->audioCodecSetting.sdpCodecArray[index]->getCodecType(),
                                               codecName))
            {
                strncpy(pCodec->cName, codecName, SIPXTAPI_CODEC_NAMELEN-1);
                pCodec->iBandWidth = 
                    (SIPX_AUDIO_BANDWIDTH_ID)pInst->audioCodecSetting.sdpCodecArray[index]->getBWCost();

                rc = SIPX_RESULT_SUCCESS;
            }                   
        }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetAudioCodec hInst=%p index=%d, codec-%s",
        hInst, index, codecName.data());

    return rc;
}


#ifdef VIDEO
SIPXTAPI_API SIPX_RESULT sipxConfigGetVideoCodecPreferences(const SIPX_INST hInst, 
                                                            SIPX_VIDEO_BANDWIDTH_ID *pBandWidth)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    if (pInst && pInst->videoCodecSetting.bInitialized)
    {
        *pBandWidth = pInst->videoCodecSetting.codecPref;
        rc = SIPX_RESULT_SUCCESS;
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetVideoCodecPreferences hInst=%p bandWidth=%d",
        hInst, *pBandWidth);

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoBandwidth(const SIPX_INST hInst, 
                                                     SIPX_VIDEO_BANDWIDTH_ID bandWidth)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoBandwidth hInst=%p bandWidth=%d",
            hInst, bandWidth);

    if (pInst)
    {
        if (!pInst->videoCodecSetting.bInitialized)
        {
            pInst->pCodecFactory->getCodecs(pInst->videoCodecSetting.numCodecs,
                                            pInst->videoCodecSetting.sdpCodecArray,
                                                        "video");
            pInst->videoCodecSetting.bInitialized = true;
        }
#ifdef VIDEO
        // Check if bandwidth is legal, do not allow variable bandwidth
        if (bandWidth >= VIDEO_CODEC_BW_LOW && bandWidth <= VIDEO_CODEC_BW_HIGH)
        {
    
            CpMediaInterfaceFactoryImpl* pImpl = 
                            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
            int frameRate;
            pImpl->getVideoFrameRate(frameRate);

            pInst->videoCodecSetting.codecPref = bandWidth;
            switch (bandWidth)
            {
            case VIDEO_CODEC_BW_LOW:
                sipxConfigSetVideoParameters(hInst, 5, 10);
                break;
            case VIDEO_CODEC_BW_NORMAL:
                sipxConfigSetVideoParameters(hInst, 70, frameRate);
                break;
            case VIDEO_CODEC_BW_HIGH:
                sipxConfigSetVideoParameters(hInst, 400, frameRate);
                break;
            }
            rc = SIPX_RESULT_SUCCESS;
        }
#endif
    }
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigGetVideoCaptureDevices(const SIPX_INST hInst,
                                                          char** arrSzCaptureDevices,
                                                          int nDeviceStringLength,
                                                          int nArrayLength)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigGetVideoCaptureDevices hInst=%p",
            hInst);
            

    char *pTemp = (char*)arrSzCaptureDevices;
    memset(arrSzCaptureDevices, 0, nDeviceStringLength * nArrayLength);          
    if (pInst && pInst->pCallManager)
    {
        CpMediaInterfaceFactoryImpl* pImpl = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
        if (pImpl)
        {
            UtlSList captureDevices; 
            
            if (OS_SUCCESS == pImpl->getVideoCaptureDevices(captureDevices))
            {
                UtlSListIterator iterator(captureDevices);
                UtlString* pDevice;
                int index = 0;
                while (pDevice = (UtlString*)iterator())
                {
                    if (pDevice->length())
                    {
                        strncpy(pTemp, pDevice->data(),
                        pDevice->length() > nDeviceStringLength ? 
                            nDeviceStringLength : pDevice->length()  );
                    }
                    index++;
                    pTemp += nDeviceStringLength;
                    if (index > nArrayLength)
                    {
                        break;
                    }
                }
                rc = SIPX_RESULT_SUCCESS;
            }
        }                
    }    
    return rc;
}                                                          

SIPXTAPI_API SIPX_RESULT sipxConfigGetVideoCaptureDevice(const SIPX_INST hInst,
                                                         char* szCaptureDevice,
                                                         int nDeviceStringLength)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigGetVideoCaptureDevice hInst=%p",
            hInst);
            
    if (pInst && pInst->pCallManager)
    {
        CpMediaInterfaceFactoryImpl* pImpl = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
        if (pImpl)
        {
            UtlString captureDevice; 
            
            if (OS_SUCCESS == pImpl->getVideoCaptureDevice(captureDevice))
            {
                strncpy(szCaptureDevice, captureDevice.data(), 
                captureDevice.length() > nDeviceStringLength ? 
                    nDeviceStringLength : captureDevice.length());
                rc = SIPX_RESULT_SUCCESS;
            }
        }                
    }    
    return rc;
}            

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoCaptureDevice(const SIPX_INST hInst,
                                                         const char* szCaptureDevice)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoCaptureDevice hInst=%p device=%s",
            hInst, szCaptureDevice);
            
    if (pInst && pInst->pCallManager)
    {
        CpMediaInterfaceFactoryImpl* pImpl = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
        if (pImpl)
        {
            UtlString captureDevice(szCaptureDevice); 
            
            if (OS_SUCCESS == pImpl->setVideoCaptureDevice(captureDevice))
            {
                rc = SIPX_RESULT_SUCCESS;
            }
        }                
    }    
    return rc;
}                                                         
#endif // VIDEO                                              

SIPXTAPI_API SIPX_RESULT sipxConfigGetNumVideoCodecs(const SIPX_INST hInst, 
                                                     int* pNumCodecs)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;    

    if (pInst && pNumCodecs)
    {
        assert(pInst->videoCodecSetting.bInitialized);

        if (pInst->videoCodecSetting.bInitialized)
        {
            *pNumCodecs = pInst->videoCodecSetting.numCodecs;
            rc = SIPX_RESULT_SUCCESS;
        }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetNumVideoCodecs hInst=%p numCodecs=%d",
        hInst, *pNumCodecs);

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetVideoCodec(const SIPX_INST hInst, 
                                                 const int index, 
                                                 SIPX_VIDEO_CODEC* pCodec)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    UtlString codecName;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    assert(pCodec);

    if (pInst && pCodec)
    {
        assert(pInst->videoCodecSetting.bInitialized);

        memset((void*)pCodec, 0, sizeof(SIPX_VIDEO_CODEC));
        if (index >= 0 && index < pInst->videoCodecSetting.numCodecs)
        {
            CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

            // If a name is found for the codec type, copy name and bandwidth cost
            if (pInterface->getCodecNameByType(pInst->videoCodecSetting.sdpCodecArray[index]->getCodecType(),
                                               codecName))
            {
                strncpy(pCodec->cName, codecName, SIPXTAPI_CODEC_NAMELEN-1);
                pCodec->iBandWidth = 
                    (SIPX_VIDEO_BANDWIDTH_ID)pInst->videoCodecSetting.sdpCodecArray[index]->getBWCost();

                rc = SIPX_RESULT_SUCCESS;
            }                   
        }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigGetVideoCodec hInst=%p index=%d, codec-%s",
        hInst, index, codecName.data());

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoCodecByName(const SIPX_INST hInst, 
                                                       const char* szCodecName)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoCodecByName hInst=%p codec=%s",
            hInst, szCodecName);

    if (pInst)
    {
        int iRejected;

        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

        pInst->videoCodecSetting.sPreferences = szCodecName;

        if (pInterface)
        {
            if (pInst->videoCodecSetting.sPreferences.length() != 0)
            {
                // Did we previously allocate a codecs array and store it in our settings?
                if (pInst->videoCodecSetting.bInitialized)
                {
                    int codecIndex;

                    // Free up the previuosly allocated codecs and the array
                    for (codecIndex = 0; codecIndex < pInst->videoCodecSetting.numCodecs; codecIndex++)
                    {
                        if (pInst->videoCodecSetting.sdpCodecArray[codecIndex])
                        {
                            delete pInst->videoCodecSetting.sdpCodecArray[codecIndex];
                            pInst->videoCodecSetting.sdpCodecArray[codecIndex] = NULL;
                        }
                    }
                    delete[] pInst->videoCodecSetting.sdpCodecArray;
                    pInst->videoCodecSetting.sdpCodecArray = NULL;
                }
                pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                              pInst->audioCodecSetting.sPreferences,
                                              pInst->videoCodecSetting.sPreferences,
                                              -1, // Allow all formats
                                              &iRejected);

                // We've rebuilt the factory, so get the new count of codecs
                pInst->pCodecFactory->getCodecs(pInst->videoCodecSetting.numCodecs,
                                                pInst->videoCodecSetting.sdpCodecArray,
                                                "video");
                if (pInst->videoCodecSetting.numCodecs > 0)
                {
                    rc = SIPX_RESULT_SUCCESS;
                }
                else
                {
                    // Codec setting by name failed - we have an empty codec factory.
                    // Fall back to previously set bandwidth Id
                    OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                            "sipxConfigSetVideoCodecByName: Setting %s failed, falling back to preference %d", 
                            szCodecName, pInst->videoCodecSetting.fallBack);
                    //sipxConfigSetVideoCodecPreferences(hInst, pInst->audioCodecSetting.fallBack);
                }
                pInst->videoCodecSetting.bInitialized = true;

            }
        }
    }

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigResetVideoCodecs(const SIPX_INST hInst)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigResetVideoCodecs hInst=%p", hInst);

    if (pInst)
    {
        int iRejected;

        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

        if (pInterface)
        {
            // Did we previously allocate a codecs array and store it in our settings?
            if (pInst->videoCodecSetting.bInitialized)
            {
                int codecIndex;

                // Free up the previuosly allocated codecs and the array
                for (codecIndex = 0; codecIndex < pInst->videoCodecSetting.numCodecs; codecIndex++)
                {
                    if (pInst->videoCodecSetting.sdpCodecArray[codecIndex])
                    {
                        delete pInst->videoCodecSetting.sdpCodecArray[codecIndex];
                        pInst->videoCodecSetting.sdpCodecArray[codecIndex] = NULL;
                    }
                }
                delete[] pInst->videoCodecSetting.sdpCodecArray;
                pInst->videoCodecSetting.sdpCodecArray = NULL;
            }
            // Rebuild with all video codecs
            pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                          pInst->audioCodecSetting.sPreferences,
                                          "",
                                          -1, // Allow all formats
                                          &iRejected);

            // We've rebuilt the factory, so get the new count of codecs
            pInst->pCodecFactory->getCodecs(pInst->videoCodecSetting.numCodecs,
                                            pInst->videoCodecSetting.sdpCodecArray,
                                            "video");
            if (pInst->videoCodecSetting.numCodecs > 0)
            {
                rc = SIPX_RESULT_SUCCESS;
            }
            else
            {
                OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                    "sipxConfigResetVideoCodecs: Setting failed!!");
            }
            pInst->videoCodecSetting.bInitialized = true;
        }
    }

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoFormat(const SIPX_INST hInst,
                                                  SIPX_VIDEO_FORMAT videoFormat)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;   

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoFormat hInst=%p videoFormat=%d", hInst, videoFormat);

    if (pInst)
    {
        CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();
        if (pInterface)
        {
            int iRejected;
            // Did we previously allocate a codecs array and store it in our settings?
            if (pInst->videoCodecSetting.bInitialized)
            {
                int codecIndex;

                // Free up the previuosly allocated codecs and the array
                for (codecIndex = 0; codecIndex < pInst->videoCodecSetting.numCodecs; codecIndex++)
                {
                    if (pInst->videoCodecSetting.sdpCodecArray[codecIndex])
                    {
                        delete pInst->videoCodecSetting.sdpCodecArray[codecIndex];
                        pInst->videoCodecSetting.sdpCodecArray[codecIndex] = NULL;
                    }
                }
                delete[] pInst->videoCodecSetting.sdpCodecArray;
                pInst->videoCodecSetting.sdpCodecArray = NULL;
            }
            // Rebuild with limited video format
            pInterface->buildCodecFactory(pInst->pCodecFactory, 
                                          pInst->audioCodecSetting.sPreferences,
                                          "",
                                          videoFormat,
                                          &iRejected);

            // We've rebuilt the factory, so get the new count of codecs
            pInst->pCodecFactory->getCodecs(pInst->videoCodecSetting.numCodecs,
                                            pInst->videoCodecSetting.sdpCodecArray,
                                            "video");
            if (pInst->videoCodecSetting.numCodecs > 0)
            {
                rc = SIPX_RESULT_SUCCESS;
            }
            else
            {
                OsSysLog::add(FAC_SIPXTAPI, PRI_ERR,
                    "sipxConfigResetVideoCodecs: Setting failed!!");
            }
            pInst->videoCodecSetting.bInitialized = true;
        }
    }
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigEnableDnsSrv(const bool enable)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigEnableDnsSrv bEnable=%d",
            enable);
            
    UtlBoolean bEnable(enable);
    // The IgnoreSRV option has the opposite sense of bEnable.
    SipSrvLookup::setOption(SipSrvLookup::OptionCodeIgnoreSRV, !bEnable);
    
    return SIPX_RESULT_SUCCESS;
}

SIPXTAPI_API SIPX_RESULT sipxConfigEnableOutOfBandDTMF(const SIPX_INST hInst,
                                                       const bool bEnable)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableOutOfBandDTMF hInst=%p bEnbale=%d",
        hInst, bEnable);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         if (pInterface)
         {
             if (pInterface->enableOutOfBandDTMF(bEnable) == OS_SUCCESS)
             {
                 rc = SIPX_RESULT_SUCCESS;
             }

         }
    }
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigEnableInBandDTMF(const SIPX_INST hInst,
                                                    const bool bEnable)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableInBandDTMF hInst=%p bEnbale=%d",
        hInst, bEnable);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         if (pInterface)
         {
             if (pInterface->enableInBandDTMF(bEnable) == OS_SUCCESS)
             {

                 rc = SIPX_RESULT_SUCCESS;
             }
         }
    }
    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigIsOutOfBandDTMFEnabled(const SIPX_INST hInst,
                                                          bool& enabled)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    UtlBoolean bEnabled;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         if (pInterface && pInterface->isOutOfBandDTMFEnabled(bEnabled) == OS_SUCCESS)
         {
             enabled = (bEnabled) ? true : false;
             rc = SIPX_RESULT_SUCCESS;
         }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigIsOutOfBandDTMFEnabled hInst=%p enabled=%d",
        hInst, enabled);

    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigIsInBandDTMFEnabled(const SIPX_INST hInst,
                                                       bool& enabled)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    UtlBoolean bEnabled;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         if (pInterface && pInterface->isInBandDTMFEnabled(bEnabled) == OS_SUCCESS)
         {
             enabled = (bEnabled) ? true : false;
             rc = SIPX_RESULT_SUCCESS;
         }
    }
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigIsInBandDTMFEnabled hInst=%p enabled=%d",
        hInst, enabled);

    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigEnableRTCP(const SIPX_INST hInst,
                                              const bool bEnable) 
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableRTCP hInst=%p enable=%d",
        hInst, bEnable);

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         if (pInterface && (pInterface->enableRTCP(bEnable) == OS_SUCCESS))
         {             
             rc = SIPX_RESULT_SUCCESS;
         }
    }

    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigGetLocalContacts(const SIPX_INST hInst,
                                                    SIPX_CONTACT_ADDRESS addresses[],
                                                    size_t nMaxAddresses,
                                                    size_t& nActualAddresses) 
{
    SIPX_RESULT rc = SIPX_RESULT_INVALID_ARGS;
    UtlString address ;
    
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    assert(pInst->pSipUserAgent != NULL) ;
    nActualAddresses = 0 ;
    if (pInst && pInst->pSipUserAgent && nMaxAddresses > 0)
    {
        CONTACT_ADDRESS* contacts[MAX_IP_ADDRESSES];
        int numContacts = 0;
        pInst->pSipUserAgent->getContactAddresses(contacts, numContacts);       
        
        // translate from CONTACT_ADDRESSes to SIPX_CONTACT_ADDRESSes
        for (unsigned int i = 0; (i < (unsigned int)numContacts) && (i < nMaxAddresses); i++)
        {
            strcpy(addresses[i].cInterface, contacts[i]->cInterface);
            strcpy(addresses[i].cIpAddress, contacts[i]->cIpAddress);
            addresses[i].eContactType = (SIPX_CONTACT_TYPE)contacts[i]->eContactType;
            addresses[i].eTransportType = (SIPX_TRANSPORT_TYPE)contacts[i]->transportType ;
            addresses[i].id = contacts[i]->id;
            addresses[i].iPort = contacts[i]->iPort;
            OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
                "sipxConfigGetLocalContacts index=%d contactId=%d contactType=%s transportType=%s port=%d address=%s adapter=%s",
                i,
                addresses[i].id,
                sipxContactTypeToString(addresses[i].eContactType),
                sipxTransportTypeToString(addresses[i].eTransportType),
                addresses[i].iPort,
                addresses[i].cIpAddress,
                addresses[i].cInterface);
            nActualAddresses++ ;
        }
        rc = SIPX_RESULT_SUCCESS;
    }
    else
    {
        rc = SIPX_RESULT_FAILURE ;
    }        
    return rc;
}


SIPXTAPI_API SIPX_RESULT sipxConfigGetAllLocalNetworkIps(const char* arrAddresses[], const char* arrAddressAdapter[], int &numAddresses)
{
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;
    
    const HostAdapterAddress* utlAddresses[SIPX_MAX_IP_ADDRESSES];
    
    if (OS_SUCCESS == getAllLocalHostIps(utlAddresses, numAddresses))
    {
        rc = SIPX_RESULT_SUCCESS;
    }

    for (int i = 0; i < numAddresses; i++)
    {
        char *szAddress = NULL;
        char *szAdapter = NULL;        
        szAddress = (char*)malloc(utlAddresses[i]->mAddress.length() + 1);
        szAdapter = (char*)malloc(utlAddresses[i]->mAdapter.length() + 1);
        strcpy(szAddress, utlAddresses[i]->mAddress.data());
        strcpy(szAdapter, utlAddresses[i]->mAdapter.data());
        arrAddresses[i] = szAddress;
        arrAddressAdapter[i] = szAdapter;
        
        OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigGetAllLocalNetworkIps index=%d address=%s adapter=%s",
            i, arrAddresses[i], arrAddressAdapter[i]);
        delete utlAddresses[i];
    }
    
    return rc;
}

#ifdef VIDEO
SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoPreviewDisplay(const SIPX_INST hInst, SIPX_VIDEO_DISPLAY* const pDisplay)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoPreviewWindow hInst=%p, hDisplay=%p",
        hInst, pDisplay);
        
    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    
    CpMediaInterfaceFactoryImpl* pImpl = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
           
    pImpl->setVideoPreviewDisplay(pDisplay);
    
    return SIPX_RESULT_SUCCESS;
}

#ifdef VIDEO
SIPXTAPI_API SIPX_RESULT sipxConfigUpdatePreviewWindow(const SIPX_INST hInst, const SIPX_WINDOW_HANDLE hWnd)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigUpdatePreviewWindow hInst=%p, hWnd=%p",
        hInst, hWnd);
        
    #ifdef _WIN32
        #include <windows.h>
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint((HWND)hWnd, &ps);
		GipsVideoEngineWindows* pVideoEngine = sipxConfigGetVideoEnginePtr(hInst);
		pVideoEngine->GIPSVideo_OnPaint(hdc);
		EndPaint((HWND)hWnd, &ps);
    #endif
    return SIPX_RESULT_SUCCESS;
}
#endif

SIPXTAPI_API SIPX_RESULT sipxCallResizeWindow(const SIPX_CALL hCall, const SIPX_WINDOW_HANDLE hWnd)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigResizeWindow hCall=%d, hWnd=%p",
        hCall, hWnd);

    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;
        
    #ifdef _WIN32
        #include <windows.h>

        if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
        {
    		GipsVideoEngineWindows* pVideoEngine = sipxConfigGetVideoEnginePtr(pInst);
            if (pVideoEngine)
            {
		        pVideoEngine->GIPSVideo_OnSize((HWND)hWnd);
                sr = SIPX_RESULT_SUCCESS;
            }
        }
    #endif
    return sr;
}

SIPXTAPI_API SIPX_RESULT sipxCallUpdateVideoWindow(const SIPX_CALL hCall, const SIPX_WINDOW_HANDLE hWnd)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxCallUpdateVideoWindow hCall=%d, hWnd=%p",
        hCall, hWnd);
        
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;
    SIPX_INSTANCE_DATA* pInst ;
    UtlString callId ;
    UtlString remoteAddress ;

    if (sipxCallGetCommonData(hCall, &pInst, &callId, &remoteAddress, NULL))
    {
        // for now, just call the sipxConfigUpdatePreviewWindow - it does the same thing
        sipxConfigUpdatePreviewWindow(pInst, hWnd);
    }
    
    return SIPX_RESULT_SUCCESS;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoQuality(const SIPX_INST hInst, const SIPX_VIDEO_QUALITY_ID quality)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoQuality hInst=%p, quality=%d",
        hInst, quality);
    SIPX_RESULT sr = SIPX_RESULT_FAILURE ;

    if (quality>0 && quality<4)
    {
        SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
        
        CpMediaInterfaceFactoryImpl* pImpl = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
               
        pImpl->setVideoQuality(quality);
        sr = SIPX_RESULT_SUCCESS;
    }
    else
    {
        sr = SIPX_RESULT_INVALID_ARGS;
    }
    return sr;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoParameters(const SIPX_INST hInst,
                                                      const int bitRate,
                                                      const int frameRate)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoParameters hInst=%p, bitRate=%d, frameRate=%d",
        hInst, bitRate, frameRate);

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    int localBitrate = bitRate;

    if (localBitrate < 5)
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoParameters - Setting localBitrate to 5");
        localBitrate = 5;
    }
    else if (localBitrate > 400)
    {
        OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
            "sipxConfigSetVideoParameters - Setting localBitrate to 400");
        localBitrate = 400;
    }
    
    CpMediaInterfaceFactoryImpl* pImpl = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
            
    pImpl->setVideoParameters(localBitrate, frameRate);
  
    return SIPX_RESULT_SUCCESS;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoCpuUsage(const SIPX_INST hInst, 
                                                    const int cpuUsage)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoCpuUsage hInst=%p, cpuUsage=%d",
        hInst, cpuUsage);

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    
    CpMediaInterfaceFactoryImpl* pImpl = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
            
    pImpl->setVideoCpuValue(cpuUsage);
  
    return SIPX_RESULT_SUCCESS;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoBitrate(const SIPX_INST hInst, 
                                                   const int bitRate)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoBitrate hInst=%d, bitRate=%d",
         hInst, bitRate);

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    
    CpMediaInterfaceFactoryImpl* pImpl = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
            
    pImpl->setVideoBitrate(bitRate);
  
    return SIPX_RESULT_SUCCESS;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetVideoFramerate(const SIPX_INST hInst, 
                                                     const int frameRate)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetVideoFramerate hInst=%d, frameRate=%d",
         hInst, frameRate);

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    
    CpMediaInterfaceFactoryImpl* pImpl = 
            pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation() ;
            
    pImpl->setVideoFramerate(frameRate);
  
    return SIPX_RESULT_SUCCESS;
}
#endif // VIDEO

SIPXTAPI_API SIPX_RESULT sipxConfigSetSecurityParameters(const SIPX_INST hInst,
                                                         const char* szDbLocation,
                                                         const char* szMyCertNickname,
                                                         const char* szDbPassword)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetSecurityParameters hInst=%p, dbLocation=%s",
        hInst, szDbLocation);

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  
    if (pInst)
    {
        strncpy(pInst->dbLocation, szDbLocation, sizeof(pInst->dbLocation));
        strncpy(pInst->myCertNickname, szMyCertNickname, sizeof(pInst->myCertNickname));
        strncpy(pInst->dbPassword, szDbPassword, sizeof(pInst->dbPassword));
    }
    return SIPX_RESULT_SUCCESS;
}                                                                                                 

SIPXTAPI_API SIPX_RESULT sipxConfigEnableSipShortNames(const SIPX_INST hInst, 
                                                       const bool bEnabled)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableSipShortNames hInst=%p, bEnabled=%d",
        hInst, bEnabled);
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ; 

    if (pInst)
    {
        pInst->bShortNames = bEnabled;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setHeaderOptions(pInst->bAllowHeader, pInst->bDateHeader, pInst->bShortNames, pInst->szAcceptLanguage);
            rc = SIPX_RESULT_SUCCESS;
        }
    }    
    return rc;
}                                                       

SIPXTAPI_API SIPX_RESULT sipxConfigEnableSipDateHeader(const SIPX_INST hInst, 
                                                       const bool bEnabled)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableSipDateHeader hInst=%p, bEnabled=%d",
        hInst, bEnabled);
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
        pInst->bDateHeader = bEnabled;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setHeaderOptions(pInst->bAllowHeader, pInst->bDateHeader, pInst->bShortNames, pInst->szAcceptLanguage);
            rc = SIPX_RESULT_SUCCESS;
        }
    }    
    return rc;
}                                                       

SIPXTAPI_API SIPX_RESULT sipxConfigEnableSipAllowHeader(const SIPX_INST hInst, 
                                                       const bool bEnabled)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigEnableSipAllowHeader hInst=%p, bEnabled=%d",
        hInst, bEnabled);
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
        pInst->bAllowHeader = bEnabled;
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setHeaderOptions(pInst->bAllowHeader, pInst->bDateHeader, pInst->bShortNames, pInst->szAcceptLanguage);
            rc = SIPX_RESULT_SUCCESS;
        }
    }    
    return rc;
}                                                       

SIPXTAPI_API SIPX_RESULT sipxConfigSetSipAcceptLanguage(const SIPX_INST hInst, 
                                                        const char* szLanguage)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetSipAcceptLanguage hInst=%p, szLanguage=%s",
        hInst, szLanguage);
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
        strncpy(pInst->szAcceptLanguage, szLanguage, 16);
        if (pInst->pSipUserAgent)
        {
            pInst->pSipUserAgent->setHeaderOptions(pInst->bAllowHeader, pInst->bDateHeader, pInst->bShortNames, pInst->szAcceptLanguage);
            rc = SIPX_RESULT_SUCCESS;
        }
    }    
    return rc;
} 

SIPXTAPI_API SIPX_RESULT sipxConfigSetLocationHeader(const SIPX_INST hInst, 
                                                     const char* szHeader)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetLocationHeader hInst=%p, szHeader=%s",
        hInst, szHeader);
    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst && szHeader)
    {
        strncpy(pInst->szLocationHeader, szHeader, 255);
    }    
    return rc;
}

SIPXTAPI_API SIPX_RESULT sipxConfigSetConnectionIdleTimeout(const SIPX_INST hInst,
                                                            const int idleTimeout)
{
    OsSysLog::add(FAC_SIPXTAPI, PRI_INFO,
        "sipxConfigSetSetConnectionIdleTimeout hInst=%p, idleTimeout==%d",
        hInst, idleTimeout);

    SIPX_RESULT rc = SIPX_RESULT_FAILURE;

    SIPX_INSTANCE_DATA* pInst = (SIPX_INSTANCE_DATA*) hInst ;  

    if (pInst)
    {
         CpMediaInterfaceFactoryImpl* pInterface = 
                pInst->pCallManager->getMediaInterfaceFactory()->getFactoryImplementation();

         pInterface && pInterface->setConnectionIdleTimeout(idleTimeout);

         rc = SIPX_RESULT_SUCCESS;
    }
    return rc;
}
SIPXTAPI_API SIPX_RESULT sipxConfigSetBeginThread(uintptr_t (*func)(const char* name, void *, unsigned, unsigned (__stdcall *) (void *), void *, unsigned, unsigned *)) {
	assert(func != 0);
	osBeginThread = func;
	return SIPX_RESULT_SUCCESS;
}


