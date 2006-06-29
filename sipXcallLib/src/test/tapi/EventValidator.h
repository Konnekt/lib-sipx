//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _EVENTVALIDATOR_H /* [ */
#define _EVENTVALIDATOR_H

#include "tapi/sipXtapi.h"
#include "tapi/sipXtapiEvents.h"
#include "tapi/sipXtapiInternal.h"
#include "os/OsBSem.h"
#include "os/OsMutex.h"
#include "os/OsLock.h"
#include "utl/UtlSList.h"

#define DEFAULT_TIMEOUT         -1
#define MAX_EVENT_CATEGORIES    16  // room for growth
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

typedef enum 
{
    EVENT_VALIDATOR_ERROR_NONE,
    EVENT_VALIDATOR_ERROR_TIMEOUT,    
    EVENT_VALIDATOR_ERROR_MISMATCH,
    EVENT_VALIDATOR_ERROR_UNEXPECTED
} EVENT_VALIDATOR_ERROR_TYPE ;

class EventValidator
{
protected:
    bool m_filterCategories[MAX_EVENT_CATEGORIES] ;
    int  m_iDefaultTimeoutInSecs ;
    bool m_bIgnoreMessages ;
    int  m_iMaxLookAhead ;

    UtlSList   m_unprocessedEvents ;
    UtlSList   m_processedEvents ;
    UtlString* m_pUnfoundEvent ;
    UtlString  m_title ;

    OsBSem   m_semUnprocessed ;
    OsMutex  m_mutLists ;
    EVENT_VALIDATOR_ERROR_TYPE m_eErrorType ;

public:
    EventValidator(const char* szTitle = "Unknown") ;

    ~EventValidator() ;

    void ignoreEventCategory(SIPX_EVENT_CATEGORY category) ;
    bool isIgnoredCateogry(SIPX_EVENT_CATEGORY category) ;    
    void ignoreMessages() ;
    bool isMessageIgnored() ;
    void setDefaultTimeout(int iTimeoutInSecs) ;
    void setMaxLookhead(int iMaxLookAhead) ;
    void reset() ;
    const char* getTitle();

    bool waitForCallEvent(SIPX_LINE hLine, 
                          SIPX_CALL hCall,
                          SIPX_CALLSTATE_EVENT event,
                          SIPX_CALLSTATE_CAUSE cause,
                          bool bStrictOrderMatch = true,
                          int iTimeoutInSecs = DEFAULT_TIMEOUT) ;

    bool waitUntilCallEvent(SIPX_LINE hLine,
                            SIPX_CALL hCall,
                            SIPX_CALLSTATE_EVENT event,
                            SIPX_CALLSTATE_CAUSE cause,
                            int iTimeoutInSecs = DEFAULT_TIMEOUT) ;

    bool waitForMessage(SIPX_LINE hLine, 
                        const char* szMsg,
                        bool bStrictOrderMatch = true, 
                        int iTimeoutInSecs = DEFAULT_TIMEOUT) ;

    bool waitForLineEvent(SIPX_LINE hLine, 
                          SIPX_LINESTATE_EVENT event, 
                          SIPX_LINESTATE_CAUSE cause,
                          bool bStrictOrderMatch = true, 
                          int iTimeoutInSecs = DEFAULT_TIMEOUT) ;


    bool waitForInfoStatusEvent(SIPX_INFO hInfo, 
                                int status, 
                                int responseCode, 
                                const char* szResponseText,
                                bool bStrictOrderMatch = true, 
                                int iTimeoutInSecs = DEFAULT_TIMEOUT) ;

    bool waitForInfoEvent(SIPX_CALL hCall,
                          SIPX_LINE hLine,
                          const char* szFromURL,
                          const char* szUserAgent,
                          const char* szContentType,
                          const char* szContent,
                          int nContentLength,
                          bool bStrictOrderMatch = true, 
                          int iTimeoutInSecs = DEFAULT_TIMEOUT) ;


    bool waitForConfigEvent(SIPX_CONFIG_EVENT event,
                            bool bStrictOrderMatch = true, 
                            int iTimeoutInSecs = DEFAULT_TIMEOUT) ;

    bool waitForSubStatusEvent(SIPX_SUBSCRIPTION_STATE state, 
                            SIPX_SUBSCRIPTION_CAUSE cause, 
                            bool bStrictOrderMatch = true, 
                            int iTimeoutInSecs = DEFAULT_TIMEOUT) ; 
                            
    bool waitForNotifyEvent(SIPX_NOTIFY_INFO* pInfo, 
                            bool bStrictOrderMatch = true, 
                            int iTimeoutInSecs = DEFAULT_TIMEOUT) ; 

    bool waitForSecurityEvent(SIPX_SECURITY_EVENT event,
                              SIPX_SECURITY_CAUSE cause,
                              bool bStrictOrderMatch = true,
                              int iTimeoutInSecs = DEFAULT_TIMEOUT);

    bool waitForMediaEvent(SIPX_MEDIA_EVENT event,
                           SIPX_MEDIA_CAUSE cause,
                           SIPX_MEDIA_TYPE  type,
                           bool bStrictOrderMatch = true,
                           int iTimeoutInSecs = DEFAULT_TIMEOUT);

    bool hasUnprocessedEvents() ;

    bool validateNoWaitingEvent() ;

    void report() ;

    void addEvent(SIPX_EVENT_CATEGORY category, void* pInfo) ;

    void addMessage(SIPX_LINE hLine, const char* szMsg) ;

    void addMarker(const char* szMarkerText) ;

protected:
    UtlString* allocCallStateEntry(SIPX_CALL hCall,
                                   SIPX_LINE hLine,
                                   SIPX_CALLSTATE_EVENT event,
                                   SIPX_CALLSTATE_CAUSE cause) ;
    UtlString* allocLineStateEntry(SIPX_LINE hLine,
                                   SIPX_LINESTATE_EVENT event,
                                   SIPX_LINESTATE_CAUSE cause) ;

    UtlString* allocMessageEvent(SIPX_LINE hLine, 
                                 const char* szMessage) ;

    UtlString* allocInfoStatusEvent(SIPX_INFO hInfo, 
                                    int status, 
                                    int responseCode, 
                                    const char* szResponseText)  ;

    UtlString* allocInfoEvent(SIPX_CALL hCall, 
                              SIPX_LINE hLine, 
                              const char* szFromURL, 
                              const char* szUserAgent,
                              const char* szContentType,
                              const char* szContent,
                              int nContentLength) ;

    UtlString* allocConfigEvent(SIPX_CONFIG_EVENT hEvent) ;
    UtlString* allocSecurityEvent(SIPX_SECURITY_EVENT hEvent,
                                  SIPX_SECURITY_CAUSE cause) ;
    UtlString* allocMediaEvent(SIPX_MEDIA_EVENT hEvent,
                                  SIPX_MEDIA_CAUSE cause,
                                  SIPX_MEDIA_TYPE type) ;
    UtlString* allocNotifyEvent(SIPX_NOTIFY_INFO* pInfo); 
    UtlString* allocSubStatusEvent(SIPX_SUBSCRIPTION_STATE state, 
                                    SIPX_SUBSCRIPTION_CAUSE cause); 
                                  
    bool findEvent(const char* szEvent, int nMaxLookAhead, int &nActualLookAhead) ;


    bool waitForEvent(const char* szEvent, bool bStrictOrderMatch, int iTimeoutInSecs) ;
    
    bool waitUntilEvent(const char* szEvent, int iTimeoutInSecs);
    



} ;


#endif /* _EVENTVALIDATOR_H ] */
