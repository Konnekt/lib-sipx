//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////
// Author: Dan Petrie (dpetrie AT SIPez DOT com)

#ifndef _SipDialogMgr_h_
#define _SipDialogMgr_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES

#include <os/OsDefs.h>
#include <os/OsMutex.h>
#include <utl/UtlString.h>
#include <utl/UtlHashBag.h>

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// FORWARD DECLARATIONS
class SipMessage;
class SipDialog;

// TYPEDEFS

//! Class for refreshing SIP subscriptions and registrations
/*! This is currently verified for SUBSCRIPTIONS ONLY.
 *  This class is intended to replace the SipRefreshMgr.
 *
 * \par 
 */
class SipDialogMgr
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

    //! Default Dialog constructor
    SipDialogMgr();


    //! Destructor
    virtual
    ~SipDialogMgr();


/* ============================ MANIPULATORS ============================== */

    //! Create a new dialog for the given SIP message
    UtlBoolean createDialog(const SipMessage& message, 
                            UtlBoolean messageIsFromLocalSide,
                            const char* dialogHandle = NULL);

    //! Update the dialog information for the given message
    /*! If a dialog matches this message update the dialog information
     *  otherwise if this message is part of an established dialog and 
     *  matches an early dialog change the dialog to established and 
     *  update the dialog information.
     */
    UtlBoolean updateDialog(const SipMessage& message, 
                            const char* dialogHandle = NULL);

    //! Delete the dialog for the given dialog handle
    UtlBoolean deleteDialog(const char* dialogHandle);


    //! Get the dialog related fields and set them in the given request
    /*! Increments the dialogs local Cseq as well.
     */
    UtlBoolean setNextLocalTransactionInfo(SipMessage& request,
                                           const char* method = NULL,
                                           const char* dialogHandle = NULL);

    /* ============================ ACCESSORS ================================= */

    //! Get the early dialog handle for the given established dialog handle
    /*! This works even if the SipDialog is an early dialog that has not yet 
     *  been updated to be an established dialog. */
    UtlBoolean getEarlyDialogHandleFor(const char* establishedDialogHandle, 
                                       UtlString& earlyDialogHandle);

    //! Get the established dialog for the given early dialog
    UtlBoolean getEstablishedDialogHandleFor(const char* earlyDialogHandle,
                                             UtlString& establishedDialogHandle);

    //! Get a count of the SipDialogs
    int countDialogs() const;

    //! Get dump string of dialogs
    int toString(UtlString& dumpString);

/* ============================ INQUIRY =================================== */

    //! Is there an early dialog that matches this early dialogHandle
    /*! If earlyDialogHandle is not an early dialog, no matches are
     * considered to exist.
     */
    UtlBoolean earlyDialogExists(const char* earlyDialogHandle);

    //! Is there an early dialog that matches this established dialogHandle
    /*! If establishedDialogHandle is not an established dialog, no matches are
     * considered to exist.
     */
    UtlBoolean earlyDialogExistsFor(const char* establishedDialogHandle);

    //! Is there a dialog that matches this dialogHandle
    /*! If the dialog handle is an early dialog, it will only match
     *  early dialogs.  If the dialog handle is an established dialog
     *  it will only match established dialogs.
     */
    UtlBoolean dialogExists(const char* dialogHandle);

    //! Checks to see if the given message matches the last local transaction
    UtlBoolean isLastLocalTransaction(const SipMessage& message, 
                                      const char* dialogHandle = NULL);

    //! Check if the message is part of a new remote transaction
    /*! The cseq of the message is greater than the last known cseq
     *  of the remote side of the dialog
     */
    UtlBoolean isNewRemoteTransaction(const SipMessage& sipMessage);

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
    //! Copy constructor NOT ALLOWED
    SipDialogMgr(const SipDialogMgr& rSipDialogMgr);

    //! Assignment operator NOT ALLOWED
    SipDialogMgr& operator=(const SipDialogMgr& rhs);

    //! Find a dialog that matches, optionally look for an early dialog if exact match does not exist
    /*! Checks tags in both directions
     */
    SipDialog* findDialog(UtlString& dialogHandle,
                          UtlBoolean ifHandleEstablishedFindEarlyDialog,
                          UtlBoolean ifHandleEarlyFindEstablishedDialog);

    //! Find a dialog that matches, optionally look for an early dialog if exact match does not exist
    /*! Checks tags in both directions
     */
    SipDialog* findDialog(UtlString& callId,
                          UtlString& localTag,
                          UtlString& remoteTag,
                          UtlBoolean ifHandleEstablishedFindEarlyDialog,
                          UtlBoolean ifHandleEarlyFindEstablishedDialog);

    //! lock for single thread use
    void lock();

    //! unlock for use
    void unlock();

    OsMutex mDialogMgrMutex;
    UtlHashBag mDialogs; 
};

/* ============================ INLINE METHODS ============================ */

#endif  // _SipDialogMgr_h_
