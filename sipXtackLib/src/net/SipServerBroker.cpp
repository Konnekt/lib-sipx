// 
// Copyright (C) 2005 Pingtel Corp.
// 
// $$
//////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES
#include "os/OsDefs.h"
#include <assert.h>
#include <string.h>
#include "os/OsTask.h"
#include "os/OsPtrMsg.h"

// APPLICATION INCLUDES
#include "os/OsDefs.h"
#include "os/OsTask.h"
#include "os/OsServerTask.h"
#include "os/OsConnectionSocket.h"
#include "os/OsEvent.h"
#include "os/OsNotification.h"
#include "net/SipServerBroker.h"
#include "os/OsServerSocket.h"
#include "os/OsEventMsg.h"
#include "net/SipTcpServer.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

SipServerBroker::SipServerBroker(OsServerTask* pTask, OsServerSocket* pSocket) :
    OsTask("SipServerBroker - %d"),
    mpSocket(pSocket),
    mpOwnerTask(pTask)
{
    start();
}

int SipServerBroker::run(void *pNotUsed)
{
    
    OsConnectionSocket* clientSocket = NULL;

    while(mpSocket && !isShuttingDown() &&
        mpSocket->isOk())
    {
        clientSocket = mpSocket->accept();   

        // post a message, containing the the client socket,
        // to the owner
        if(clientSocket)
        {
            OsPtrMsg ptrMsg(OsMsg::OS_EVENT, SipTcpServer::SIP_SERVER_BROKER_NOTIFY, (void*)clientSocket);
            mpOwnerTask->postMessage(ptrMsg);
        }
    }
    return 0;
}

// Destructor
SipServerBroker::~SipServerBroker()
{
    if (mpSocket)
    {
        mpSocket->close();
    }
    waitUntilShutDown();
    delete mpSocket;
    mpSocket = NULL;

}

/************************************************************************/

/* ============================ FUNCTIONS ================================= */

