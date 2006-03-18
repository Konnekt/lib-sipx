//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _OsMulticastSocket_h_
#define _OsMulticastSocket_h_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include <os/OsSocket.h>

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

//: Implements UDP version of OsSocket
// This class provides the implementation of the UDP datagram
// based socket class which may be instantiated.

class OsMulticastSocket : public OsSocket
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

   OsMulticastSocket(int multicastPort = PORT_DEFAULT,
                     const char* multicastHostName = NULL,
                     int localHostPort = PORT_DEFAULT,
                     const char* localHostName = NULL);

  virtual
   ~OsMulticastSocket();
     //:Destructor


/* ============================ MANIPULATORS ============================== */

   virtual UtlBoolean reconnect();
   //: Sets up the connection again, assuming the connection failed

   void joinMulticast(int multicastPort, const char* multicastHostName);

   virtual int read(char* buffer, int bufferLength);
   //: Blocking read from the socket
   // Read bytes into the buffer from the socket up to a maximum of
   // bufferLength bytes.  This method will block until there is
   // something to read from the socket.
   //! param: buffer - Place to put bytes read from the socket.
   //! param: bufferLength - The maximum number of bytes buffer will hold.
   //! returns: The number of bytes actually read.

/* ============================ ACCESSORS ================================= */
   virtual int getIpProtocol() const;
   //: Returns the protocol type of this socket


/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

   OsMulticastSocket(const OsMulticastSocket& rOsMulticastSocket);
     //:Disable copy constructor

   OsMulticastSocket& operator=(const OsMulticastSocket& rhs);
     //:Assignment operator

};

/* ============================ INLINE METHODS ============================ */

#endif  // _OsMulticastSocket_h_
