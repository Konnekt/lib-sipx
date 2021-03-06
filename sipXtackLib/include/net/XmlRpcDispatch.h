// 
// 
// Copyright (C) 2005 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// $$
//////////////////////////////////////////////////////////////////////////////

#ifndef _XMLRPCDISPATCH_H_
#define _XMLRPCDISPATCH_H_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include <os/OsBSem.h>
#include <xmlparser/tinyxml.h>
#include "net/HttpService.h"
#include "net/HttpServer.h"
#include "net/XmlRpcMethod.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS

// FORWARD DECLARATIONS

// Private class to contain XmlRpcMethod and user data for each methodName
class XmlRpcMethodContainer : public UtlContainable
{
public:
   XmlRpcMethodContainer();

   virtual ~XmlRpcMethodContainer();

   virtual UtlContainableType getContainableType() const;

   virtual unsigned int hash() const;

   int compareTo(const UtlContainable *b) const;
   
   void setData(XmlRpcMethod::Get* method, void* userData);
   
   void getData(XmlRpcMethod::Get*& method, void*& userData);

private:

   void* mpUserData;
   XmlRpcMethod::Get* mpMethod;
    
   //! DISALLOWED accidental copying
   XmlRpcMethodContainer(const XmlRpcMethodContainer& rXmlRpcMethodContainer);
   XmlRpcMethodContainer& operator=(const XmlRpcMethodContainer& rhs);
};

/**
 * A XmlRpcDispatch is a object that monitors the incoming
 * XML-RPC requests, parses XmlRpcRequest messages, invokes the correct
 * XmlRpcMethod calls, and sends back the corresponding XmlRpcResponse responses.
 * If the correspnding method does not exit, it will send back a 404 response.
 * Otherwise, it will always send back a 200 OK response with XmlRpcResponse
 * content.
 * 
 * For each XML-RPC server, it needs to instantiate a XmlRpcDispatch object first,
 * and then register each service method using addMethod() or remove the method
 * using removeMethod().
 */

class XmlRpcDispatch : public HttpService
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:
   static const char* DEFAULT_URL_PATH;

/* ============================ CREATORS ================================== */

   /// Create a dispatch object.
   XmlRpcDispatch(int httpServerPort,           ///< port number for HttpServer
                  bool isSecureServer,          ///< option for HTTP or HTTPS
                  const char* uriPath = DEFAULT_URL_PATH          ///< uri path
                  );

   /// Destructor.
   virtual ~XmlRpcDispatch();

/* ============================ MANIPULATORS ============================== */

   /// Handler for XML-RPC requests
   void processRequest(const HttpRequestContext& requestContext,
                       const HttpMessage& request,
                       HttpMessage*& response );

/* ============================ ACCESSORS ================================= */

   /// Add a method to the RPC dispatch
   void addMethod(const char* methodName, XmlRpcMethod::Get* method, void* userData = NULL);

   /// Remove a method from the RPC dispatch by name
   void removeMethod(const char* methodName);

   /// Return the HTTP server that services RPC requests
   HttpServer* getHttpServer();
   
/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

   friend class XmlRpcTest;
   
   /// Parse the XML-RPC request
   bool parseXmlRpcRequest(UtlString& requestContent,
                           XmlRpcMethodContainer*& method,
                           UtlSList& params,
                           XmlRpcResponse& response);

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

   /// Parse a value in the XML-RPC request
   bool parseValue(TiXmlNode* valueNode, int index, UtlSList& params);

   /// Parse an array in the XML-RPC request
   bool parseArray(TiXmlNode* valueNode, UtlSList*& array);

   /// Parse a struct in the XML-RPC request
   bool parseStruct(TiXmlNode* valueNode, UtlHashMap*& memebers);

   /// Clean up the memory in a struct
   void cleanUp(UtlHashMap* members);
   
   /// Clean up the memory in an array
   void cleanUp(UtlSList* array);
   
   /// Http server for handling the HTTP POST request  
   HttpServer* mpHttpServer;
   
   /// hash map for holding all registered XML-RPC methods
   UtlHashMap  mMethods;
   
   /// reader/writer lock for synchronization
   OsBSem mLock;

   /// Disabled copy constructor
   XmlRpcDispatch(const XmlRpcDispatch& rXmlRpcDispatch);

   /// Disabled assignment operator
   XmlRpcDispatch& operator=(const XmlRpcDispatch& rhs);

};

/* ============================ INLINE METHODS ============================ */

#endif  // _XMLRPCDISPATCH_H_


