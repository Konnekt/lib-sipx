//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _CALLBACKS_H
#define _CALLBACKS_H

extern SIPX_CALL g_hAutoAnswerCallbackCall ;
extern SIPX_CALL g_hAutoAnswerCallbackCallOther ;
extern SIPX_LINE g_hAutoAnswerCallbackLine ;
extern SIPX_CALL g_hAutoAnswerCallbackCall2 ;
extern SIPX_CALL g_hAutoAnswerCallbackCall2Other ;
extern SIPX_LINE g_hAutoAnswerCallbackLine2 ;
extern SIPX_CALL g_hAutoAnswerHangupCallbackCall ;
extern SIPX_LINE g_hAutoAnswerHangupCallbackLine ;
extern SIPX_CALL g_hAutoRejectCallbackCall ;
extern SIPX_LINE g_hAutoRejectCallbackLine ;
extern SIPX_CALL g_hAutoRedirectCallbackCall ;
extern SIPX_LINE g_hAutoRedirectCallbackLine ;


void resetAutoAnswerCallback() ;

bool AutoAnswerCallback(SIPX_EVENT_CATEGORY category, 
                        void* pInfo, 
                        void* pUserData) ;

void resetAutoAnswerCallback2() ;

bool AutoAnswerCallback2(SIPX_EVENT_CATEGORY category, 
                         void* pInfo, 
                         void* pUserData) ;


bool AutoAnswerHangupCallback(SIPX_EVENT_CATEGORY category, 
                              void* pInfo, 
                              void* pUserData) ;

bool AutoRejectCallback(SIPX_EVENT_CATEGORY category, 
                        void* pInfo, 
                        void* pUserData) ;

bool AutoRedirectCallback(SIPX_EVENT_CATEGORY category, 
                        void* pInfo, 
                        void* pUserData) ;

bool UniversalEventValidatorCallback(SIPX_EVENT_CATEGORY category,
                                     void* pInfo,
                                     void* pUserData) ;

#endif
