// 
// 
// Copyright (C) 2005 SIPez LLC.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// Copyright (C) 2004 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2004 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// $$
//////////////////////////////////////////////////////////////////////////////

// Author: Dan Petrie (dpetrie AT SIPez DOT com)

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include <net/SmimeBody.h>
#include <net/HttpMessage.h>
#include <os/OsSysLog.h>

//#define ENABLE_OPENSSL_SMIME
#ifdef ENABLE_OPENSSL_SMIME
#   include <openssl/bio.h>
#   include <openssl/pem.h>
#   include <openssl/crypto.h>
#   include <openssl/err.h>
#   include <openssl/pkcs12.h>
#endif

//#define ENABLE_NSS_SMIME
#ifdef ENABLE_NSS_SMIME
#   include <nspr.h>
#   include <nss.h>
#   include <ssl.h>
#   include <cms.h>
#   include <pk11func.h>
#   include <p12.h>
#   include <base64.h>
    extern "C" CERTCertificate *
    __CERT_DecodeDERCertificate(SECItem *derSignedCert, PRBool copyDER, char *nickname);
#endif

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
#ifdef ENABLE_OPENSSL_SMIME
static void dumpOpensslError()
{
    BIO* bioErrorBuf = BIO_new(BIO_s_mem());
    ERR_print_errors(bioErrorBuf);
    BUF_MEM *bioErrorMem;
    BIO_get_mem_ptr(bioErrorBuf, &bioErrorMem);
    if(bioErrorMem)
    {
        UtlString errorString;
        errorString.append(bioErrorMem->data, bioErrorMem->length);
        printf("OPENSSL Error:\n%s\n", errorString.data());
    }
}
#endif
/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Default Constructor
SmimeBody::SmimeBody()
{
    mContentEncoding = SMIME_ENODING_BINARY;
    append(CONTENT_SMIME_PKCS7);
    mClassType = SMIME_BODY_CLASS;
    mpDecryptedBody = NULL;
}
// Constructor
SmimeBody::SmimeBody(const char* bytes, 
                     int length,
                     const char* smimeEncodingType)
{
    bodyLength = length;
    mBody.append(bytes, length);

    // Remove to ake sure base class did not already set it
    remove(0);
    append(CONTENT_SMIME_PKCS7);

    mClassType = SMIME_BODY_CLASS;
    mContentEncoding = SMIME_ENODING_UNKNOWN;
    if(smimeEncodingType)
    {
        UtlString sMimeEncodingString(smimeEncodingType);
        sMimeEncodingString.toUpper();

        if(sMimeEncodingString.compareTo(HTTP_CONTENT_TRANSFER_ENCODING_BINARY, UtlString::ignoreCase) == 0)
        {
            mContentEncoding = SMIME_ENODING_BINARY;
        }
        else if(sMimeEncodingString.compareTo(HTTP_CONTENT_TRANSFER_ENCODING_BASE64, UtlString::ignoreCase) == 0)
        {
            mContentEncoding = SMIME_ENODING_BASE64;
        }
        else
        {
            // TODO: We could probably put a hack in here to heuristically
            // determine if the encoding is base64 or not based upon the
            // byte values.
            OsSysLog::add(FAC_SIP, PRI_ERR,
                "Invalid transport encoding for S/MIME content");
        }
    }

    mpDecryptedBody = NULL;
}

// Copy constructor
SmimeBody::SmimeBody(const SmimeBody& rSmimeBody)
{
    // Copy the base class stuff
    this->HttpBody::operator=((const HttpBody&)rSmimeBody);

    mpDecryptedBody = NULL;
    if(rSmimeBody.mpDecryptedBody)
    {
        mpDecryptedBody = HttpBody::copyBody(*(rSmimeBody.mpDecryptedBody));
    }

    mClassType = SMIME_BODY_CLASS;

    // Remove to ake sure base class did not already set it
    remove(0);
    append(CONTENT_SMIME_PKCS7);
    mContentEncoding = rSmimeBody.mContentEncoding;
}

// Destructor
SmimeBody::~SmimeBody()
{
   if(mpDecryptedBody)
   {
       delete mpDecryptedBody;
       mpDecryptedBody = NULL;
   }
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
SmimeBody&
SmimeBody::operator=(const SmimeBody& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   // Copy the parent
   *((HttpBody*)this) = rhs;

   // Set the class type just to play it safe
   mClassType = SMIME_BODY_CLASS;

   // Remove the decrypted body if one is attached,
   // in case we copy over it
   if(mpDecryptedBody)
   {
       delete mpDecryptedBody;
       mpDecryptedBody = NULL;
   }

   // If the source has a body copy it
   if(rhs.mpDecryptedBody)
   {
       mpDecryptedBody = HttpBody::copyBody(*(rhs.mpDecryptedBody));
   }

   mContentEncoding = rhs.mContentEncoding;
   return *this;
}

UtlBoolean SmimeBody::decrypt(const char* derPkcs12,
                              int derPkcs12Length,
                              const char* pkcs12Password)
{
    UtlBoolean decryptionSucceeded = FALSE;

    UtlString decryptedData;

#ifdef ENABLE_OPENSSL_SMIME
    decryptionSucceeded = 
        opensslSmimeDecrypt(derPkcs12,
                            derPkcs12Length,
                            pkcs12Password,
                            (mContentEncoding == SMIME_ENODING_BASE64),
                            mBody.data(),
                            mBody.length(),
                            decryptedData);
#elif ENABLE_NSS_SMIME
    OsSysLog::add(FAC_SIP, PRI_ERR, "NSS S/MIME decrypt not implemented");
#endif

    // Decryption succeeded, so create a HttpBody for the result
    if(decryptionSucceeded && 
        decryptedData.length() > 0)
    {
        HttpBody* newDecryptedBody = NULL;
        // Need to read the headers before the real body to see
        // what the content type of the decrypted body is
        UtlDList bodyHeaders;
        int parsedBytes = 
            HttpMessage::parseHeaders(decryptedData.data(), 
                                      decryptedData.length(),
                                      bodyHeaders);

        UtlString contentTypeName(HTTP_CONTENT_TYPE_FIELD);
        NameValuePair* contentType = 
            (NameValuePair*) bodyHeaders.find(&contentTypeName);
        UtlString contentEncodingName(HTTP_CONTENT_TRANSFER_ENCODING_FIELD);
        NameValuePair* contentEncoding = 
            (NameValuePair*) bodyHeaders.find(&contentEncodingName);
        
        const char* realBodyStart = decryptedData.data() + parsedBytes;
        int realBodyLength = decryptedData.length() - parsedBytes;

        newDecryptedBody = 
            HttpBody::createBody(realBodyStart,
                                 realBodyLength,
                                 contentType ? contentType->getValue() : NULL,
                                 contentEncoding ? contentEncoding->getValue() : NULL);

        bodyHeaders.destroyAll();

        // If one already exists, delete it.  This should not typically
        // be the case.  Infact it might make sense to make this method
        // a no-op if a decrypted body already exists
        if(mpDecryptedBody)
        {
            delete mpDecryptedBody;
            mpDecryptedBody = NULL;
        }

        mpDecryptedBody = newDecryptedBody;
    }

    return(decryptionSucceeded);
}

UtlBoolean SmimeBody::encrypt(HttpBody* bodyToEncrypt,
                              int numRecipients,
                              const char* derPublicKeyCerts[],
                              int derPubliceKeyCertLengths[])
{
    UtlBoolean encryptionSucceeded = FALSE;

    // Clean up an residual decrypted body or encrypted body content.
    // Should typically not be any.
    if(mpDecryptedBody)
    {
        delete mpDecryptedBody;
        mpDecryptedBody = NULL;
    }
    mBody.remove(0);


    if(bodyToEncrypt)
    {
        UtlString dataToEncrypt;
        UtlString contentType = bodyToEncrypt->getContentType();

        // Add the content-type and content-encoding headers to
        // the body to be decrypted so that when the otherside
        // decrypts this body, it can tell what the content is
        dataToEncrypt ="Content-Type: ";
        dataToEncrypt.append(contentType);
        dataToEncrypt.append(END_OF_LINE_DELIMITOR);
        dataToEncrypt.append("Content-Transfer-Encoding: binary");
        dataToEncrypt.append(END_OF_LINE_DELIMITOR);
        dataToEncrypt.append(END_OF_LINE_DELIMITOR);

        // Append the real body content
        const char* dataPtr;
        int dataLength;
        bodyToEncrypt->getBytes(&dataPtr, &dataLength);
        dataToEncrypt.append(dataPtr, dataLength);

        // Attach the decrypted version of the body for
        // future reference.
        mpDecryptedBody = bodyToEncrypt;

        // We almost always want to use binary for SIP as it is 
        // much more efficient than base64.
        UtlBoolean encryptedDataInBase64Format = FALSE;

#ifdef ENABLE_OPENSSL_SMIME
        encryptionSucceeded = 
            opensslSmimeEncrypt(numRecipients,
                                derPublicKeyCerts,
                                derPubliceKeyCertLengths,
                                dataToEncrypt.data(),
                                dataToEncrypt.length(),
                                encryptedDataInBase64Format,
                                mBody);
#elif ENABLE_NSS_SMIME

        encryptionSucceeded = 
            nssSmimeEncrypt(numRecipients,
                            derPublicKeyCerts,
                            derPubliceKeyCertLengths,
                            dataToEncrypt.data(),
                            dataToEncrypt.length(),
                            encryptedDataInBase64Format,
                            mBody);
#endif

        // There should always be content if encryption succeeds
        if(encryptionSucceeded &&
           mBody.length() <= 0)
        {
            encryptionSucceeded = FALSE;
            OsSysLog::add(FAC_SIP, PRI_ERR,
                "SmimeBody::encrypt no encrypted content");
        }
    }

    bodyLength = mBody.length(); 
    return(encryptionSucceeded);
}

#ifdef ENABLE_NSS_SMIME
static void nssOutToUtlString(void *sink, const char *data, unsigned long dataLength)
{
    printf("nssOutToUtlString recieved %d bytes\n", dataLength);
    UtlString* outputSink = (UtlString*) sink;
    outputSink->append(data, dataLength);
}

#endif

UtlBoolean SmimeBody::nssSmimeEncrypt(int numResipientCerts,
                           const char* derPublicKeyCerts[], 
                           int derPublicKeyCertLengths[],
                           const char* dataToEncrypt,
                           int dataToEncryptLength,
                           UtlBoolean encryptedDataInBase64Format,
                           UtlString& encryptedData)
{
    UtlBoolean encryptionSucceeded = FALSE;
    encryptedData.remove(0);

#ifdef ENABLE_NSS_SMIME

    // nickname can be NULL as we are not putting this in a database
    char *nickname = NULL;
    // copyDER = true so we copy the DER format cert passed in so memory
    // is internally alloc'd and freed
    PRBool copyDER = true;

    // Create an envelope or container for the encrypted data
    SECOidTag algorithm = SEC_OID_DES_EDE3_CBC; // or  SEC_OID_AES_128_CBC
    // Should be able to get the key size from the cert somehow
    int keysize = 1024;
    NSSCMSMessage* cmsMessage = NSS_CMSMessage_Create(NULL);
    NSSCMSEnvelopedData* myEnvelope = 
        NSS_CMSEnvelopedData_Create(cmsMessage, algorithm, keysize);

    // Do the following for each recipient if there is more than one.
    // For each recipient:
    for(int certIndex = 0; certIndex < numResipientCerts; certIndex++)
    {
        // Convert the DER to a NSS CERT
        SECItem derFormatCertItem;
        SECItem* derFormatCertItemPtr = &derFormatCertItem;
        derFormatCertItem.data = (unsigned char*) derPublicKeyCerts[certIndex];
        derFormatCertItem.len = derPublicKeyCertLengths[certIndex];
        CERTCertificate* myCertFromDer = NULL;
        myCertFromDer = __CERT_DecodeDERCertificate(&derFormatCertItem, 
                                                   copyDER, 
                                                   nickname);

        // Add just the recipient Subject key Id, if it exists to the envelope
        // This is the minimal information needed to identify which recipient
        // the the symetric/session key is encrypted for
        NSSCMSRecipientInfo* recipientInfo = NULL; 

        // Add the full set of recipient information including 
        // the Cert. issuer location and org. info.
        recipientInfo = 
            NSS_CMSRecipientInfo_Create(cmsMessage, myCertFromDer);

        if(recipientInfo)
        {
            if(NSS_CMSEnvelopedData_AddRecipient(myEnvelope , recipientInfo) != 
                SECSuccess)
            {
                NSS_CMSEnvelopedData_Destroy(myEnvelope);
                myEnvelope = NULL;
                NSS_CMSRecipientInfo_Destroy(recipientInfo);
            }
        }
        // No recipientInfo
        else
        {
            NSS_CMSEnvelopedData_Destroy(myEnvelope);
            myEnvelope = NULL;
        }

    } //end for each recipient

    // Get the content out of the envelop
    NSSCMSContentInfo* envelopContentInfo =
       NSS_CMSEnvelopedData_GetContentInfo(myEnvelope);    

    //TODO: why are we copying or setting the content pointer from the envelope into the msg????????
    if (NSS_CMSContentInfo_SetContent_Data(cmsMessage, 
                                           envelopContentInfo, 
                                           NULL, 
                                           PR_FALSE) != 
        SECSuccess)
    {
        // release cmsg and other stuff
        NSS_CMSEnvelopedData_Destroy(myEnvelope);
        myEnvelope = NULL;
    }

    //TODO: why are we copying or setting the content pointer from the message and 
    // putting it back into the msg????????
    NSSCMSContentInfo* messageContentInfo = 
        NSS_CMSMessage_GetContentInfo(cmsMessage);
    if(NSS_CMSContentInfo_SetContent_EnvelopedData(cmsMessage, 
                                                   messageContentInfo, 
                                                   myEnvelope) != 
       SECSuccess)
    {
        // release cmsg and other stuff
        NSS_CMSEnvelopedData_Destroy(myEnvelope);
        myEnvelope = NULL;
    }

    if(cmsMessage)
    {
        // Create an encoder  and context to do the encryption.
        // The encodedItem will stort the encoded result
        //SECItem encodedItem;
        //encodedItem.data = NULL;
        //encodedItem.len = 0;
        //SECITEM_AllocItem(NULL, &encodedItem, 0);
        printf("start encoder\n");
        NSSCMSEncoderContext* encoderContext = 
            NSS_CMSEncoder_Start(cmsMessage, nssOutToUtlString, &encryptedData, NULL, 
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        // Add encrypted content
        printf("update encoder\n");
        NSS_CMSEncoder_Update(encoderContext, dataToEncrypt, dataToEncryptLength);

        // Finished encrypting
        printf("finish encoder\n");
        NSS_CMSEncoder_Finish(encoderContext);

        myEnvelope = NULL;

        if(encryptedData.length() > 0)
        {
            encryptionSucceeded = TRUE;
        }

        // Clean up the message memory, the envelop gets cleaned up
        // with the message
        NSS_CMSMessage_Destroy(cmsMessage);
        cmsMessage = NULL;
    }

#else
    OsSysLog::add(FAC_SIP, PRI_ERR, "SmimeBody::nssSmimeEncrypt invoked with ENABLE_NSS_SMIME not defined");
#endif

    return(encryptionSucceeded);
}

UtlBoolean SmimeBody::nssSmimeDecrypt(const char* derPkcs12,
                               int derPkcs12Length,
                               const char* pkcs12Password,
                               UtlBoolean dataIsInBase64Format,
                               const char* dataToDecrypt,
                               int dataToDecryptLength,
                               UtlString& decryptedData)
{
    UtlBoolean decryptSucceeded = FALSE;
    decryptedData.remove(0);

#ifdef ENABLE_NSS_SMIME
    OsSysLog::add(FAC_SIP, PRI_ERR, "SmimeBody::nssSmimeDecrypt not implemented");

    ////// BEGIN WARNING: THIS CODE HAS NOT BEEN TESTED AT ALL ///////

    // allocate a temporaty slot in the database
    PK11SlotInfo *slot = PK11_GetInternalKeySlot();
    PRBool swapUnicode = PR_FALSE;
    SEC_PKCS12DecoderContext *p12Decoder = NULL;

    // Need to put the pkcs12 password into a SECItem
    SECItem passwordItem;
    passwordItem.data = (unsigned char*) pkcs12Password;
    passwordItem.len = strlen(pkcs12Password);
    SECItem uniPasswordItem;
    uniPasswordItem.data = NULL;
    uniPasswordItem.len = 0;

#ifdef IS_LITTLE_ENDIAN
    swapUnicode = PR_TRUE;
#endif

    // Allocate a temporary internal slot
    slot = PK11_GetInternalSlot();
    if(slot == NULL)
    {
        OsSysLog::add(FAC_SIP, PRI_ERR, "unable to use slot in NSS dataqbase for S/MIME decryption");
    }
    else
    {
        // Do UNICODE conversion of password based upon the platform
        // (not sure this is even neccessary in our application).  I do not
        // know how we would get unicode passwords
        if(0) //P12U_UnicodeConversion(NULL, &uniPasswordItem, passwordItem, PR_TRUE,
			  //    swapUnicode) != SECSuccess)
        {
            OsSysLog::add(FAC_SIP, PRI_ERR, 
                "NSS Unicode conversion failed for PKCS12 object for S/MIME decryption");
        }
        else
        {
            // Initialze the decoder for the PKCS12 container for the private key
            p12Decoder = SEC_PKCS12DecoderStart(&passwordItem, slot, NULL,
				    NULL, NULL, NULL, NULL, NULL);
            if(!p12Decoder) 
            {
                OsSysLog::add(FAC_SIP, PRI_ERR,
                    "failed to initialize PKCS12 decoder to extract private key for S/MIME decryption");
            }
            else
            {
                // Add the PKCS12 data to the decoder
                if(SEC_PKCS12DecoderUpdate(p12Decoder, 
                                           (unsigned char *) derPkcs12, 
                                           derPkcs12Length) != SECSuccess ||
                   // Validate the decoded PKCS12
                   SEC_PKCS12DecoderVerify(p12Decoder) != SECSuccess)

                {
                    OsSysLog::add(FAC_SIP, PRI_ERR,
                        "unable to decrypt PKCS12 for S/MIME decryption. Perhaps invalid PKCS12 or PKCS12 password");
                }
                else
                {
                    // Import the private key and certificate from the 
                    // decoded PKCS12 into the database
                    if(SEC_PKCS12DecoderImportBags(p12Decoder) != SECSuccess)
                    {
                        OsSysLog::add(FAC_SIP, PRI_ERR,
                            "failed to import private key and certificate into NSS database");
                    }
                    else
                    {
                        // Put the S/MIME data in a SECItem
                        SECItem dataToDecodeItem;
                        dataToDecodeItem.data = (unsigned char *) dataToDecrypt;
                        dataToDecodeItem.len = dataToDecryptLength;

                        if(dataIsInBase64Format)
                        {
                            // TODO:
                            // Use some NSS util. to convert base64 to binary
                            OsSysLog::add(FAC_SIP, PRI_ERR,
                                "NSS decrypt of base64 S/MIME message not implemented");
                        }
                        else
                        {
                            // Decode the S/MIME blob
                            NSSCMSMessage *cmsMessage = 
                                NSS_CMSMessage_CreateFromDER(&dataToDecodeItem,
                                                             nssOutToUtlString, 
                                                             &decryptedData,
                                                             NULL, NULL,
                                                             NULL, NULL);


                            if(cmsMessage &&
                               decryptedData.length() > 0)
                            {
                                decryptSucceeded = TRUE;
                            }

                            // TODO:
                            // Remove the temporary private key from the 
                            // database using the slot handle
                        }
                    }

                }
            }
        }
    }

    // Clean up
    if(p12Decoder) 
    {
	    SEC_PKCS12DecoderFinish(p12Decoder);
    }
    if(uniPasswordItem.data) 
    {
	    SECITEM_ZfreeItem(&uniPasswordItem, PR_FALSE);
    }

    ////// END WARNING   /////
#else
    OsSysLog::add(FAC_SIP, PRI_ERR, "SmimeBody::nssSmimeDecrypt invoked with ENABLE_NSS_SMIME not defined");
#endif

    return(decryptSucceeded);
}

UtlBoolean SmimeBody::opensslSmimeEncrypt(int numRecipients,
                                          const char* derPublicKeyCerts[],
                                          int derPublicKeyCertLengths[],
                                          const char* dataToEncrypt,
                                          int dataToEncryptLength,
                                          UtlBoolean encryptedDataInBase64Format,
                                          UtlString& encryptedData)
{
    UtlBoolean encryptSucceeded = FALSE;
    encryptedData.remove(0);

#ifdef ENABLE_OPENSSL_SMIME

    // Convert the DER format Certs. to a stack of X509
    STACK_OF(X509)* recipientX509Certs = sk_X509_new_null();
    for(int recipIndex = 0; recipIndex < numRecipients; recipIndex++)
    {
        X509* oneX509Cert = d2i_X509(NULL, (unsigned char**)&derPublicKeyCerts[recipIndex], 
                                   derPublicKeyCertLengths[recipIndex]);
        if(oneX509Cert)
        {
            sk_X509_push(recipientX509Certs, oneX509Cert);
        }
        else
        {
            printf("Invalid DER format cert. Cannot convert to X509 OpenSSL (DER length: %d)\n",
                derPublicKeyCertLengths[recipIndex]);
        }
    }

    // Put the data to be encryped into a BIO that can be used as input to
    // the PKCS7 encoder
    BIO* dataToEncryptBio = BIO_new_mem_buf((void*)dataToEncrypt,
                                            dataToEncryptLength);

    //  Use the X509 certs to create a PKCS7 encoder
    int encodeOpts = 0;
    if(encryptedDataInBase64Format)
    {
        PKCS7_BINARY;  // binary format, not base64 
    }

    const EVP_CIPHER* cipher = EVP_des_ede3_cbc();
    PKCS7* pkcs7Encoder = PKCS7_encrypt(recipientX509Certs, dataToEncryptBio, cipher, encodeOpts);

    BIO* encryptedDataBio = BIO_new(BIO_s_mem());
    if(pkcs7Encoder && encryptedDataBio)
    {
        // Get the raw binary encrypted data out of the encoder
        i2d_PKCS7_bio(encryptedDataBio, pkcs7Encoder);

        BUF_MEM *encryptedDataBioMem = NULL;
        BIO_get_mem_ptr(encryptedDataBio, &encryptedDataBioMem);

        if(encryptedDataBioMem)
        {
            encryptSucceeded = TRUE;
            encryptedData.append(encryptedDataBioMem->data,
                                 encryptedDataBioMem->length);

            BIO_free(encryptedDataBio);
            encryptedDataBio = NULL;
        }
    }
    else
    {
        if(pkcs7Encoder == NULL)
        {
            OsSysLog::add(FAC_SIP, PRI_WARNING,
                "failed to create openssl PKCS12 encoder");
        }
        if(encryptedDataBio)
        {
            OsSysLog::add(FAC_SIP, PRI_ERR,
                "Failed to allocate openssl BIO");
        }
    }

    if(pkcs7Encoder)
    {
        // TODO: free up PKCS7
        pkcs7Encoder = NULL;
    }
    if(dataToEncryptBio)
    {
        BIO_free(dataToEncryptBio);
        dataToEncryptBio = NULL;
    }
    // Free up the cert stack
    sk_X509_free(recipientX509Certs);
#endif

    return(encryptSucceeded);
}

UtlBoolean SmimeBody::opensslSmimeDecrypt(const char* derPkcs12,
                               int derPkcs12Length,
                               const char* pkcs12Password,
                               UtlBoolean dataIsInBase64Format,
                               const char* dataToDecrypt,
                               int dataToDecryptLength,
                               UtlString& decryptedData)
{
    UtlBoolean decryptSucceeded = FALSE;
    decryptedData.remove(0);

#ifdef ENABLE_OPENSSL_SMIME

    EVP_PKEY* privateKey = NULL;
    X509* publicKeyCert = NULL;

    //  Create the PKCS12 which contains both cert. and private key
    // from the DER format
    BIO* pkcs12Bio = BIO_new_mem_buf((void*)derPkcs12,
                                            derPkcs12Length);
    PKCS12 *pkcs12 = d2i_PKCS12_bio(pkcs12Bio, NULL);

    // The PKCS12 contains both the private key and the cert. which
    // are protected by symmetric encryption using the given password.
	PKCS12_parse(pkcs12, pkcs12Password, &privateKey, &publicKeyCert, NULL);
	PKCS12_free(pkcs12);
	pkcs12 = NULL;

    if(privateKey == NULL)
    {
        dumpOpensslError();
        OsSysLog::add(FAC_SIP, PRI_ERR, "PKCS12 or PKCS12 password invalid or does not contain private key for S/MIME decrypt operation");
    }
    if(publicKeyCert)
    {
        dumpOpensslError();
        OsSysLog::add(FAC_SIP, PRI_ERR, "PKCS12 or PKCS12 password invalid or does not contain certificate and public key for S/MIME decrypt operation");
    }

    // Create a memory BIO to put the body into
    BIO* encryptedBodyBioBuf = BIO_new_mem_buf((void*)dataToDecrypt,
                                                dataToDecryptLength);

    // Create the pkcs7 structure
    // The clearTextSignatureBio only gets set if it
    // is provided.
    BIO* decryptedBodyBioBuf = BIO_new(BIO_s_mem());
    PKCS7* pkcs7 = NULL;

    if(dataIsInBase64Format)  // base64
    {
        BIO* clearTextSignatureBio = NULL;
        pkcs7 = SMIME_read_PKCS7(encryptedBodyBioBuf,
                                 &clearTextSignatureBio);
        if(clearTextSignatureBio)
        {
            BIO_free(clearTextSignatureBio);
        }
    }
    else // binary
    {
        pkcs7 = d2i_PKCS7_bio(encryptedBodyBioBuf, 0);
    }

    if(pkcs7 == NULL)
    {
        // Unable to initialize PKCS7
        OsSysLog::add(FAC_SIP, PRI_ERR, 
            "Unable to create OpenSSL PKCS7 context for S/MIME decrypt operation\n");
    }

    if(pkcs7 && privateKey && publicKeyCert && decryptedBodyBioBuf)
    {
        // Decrypt the pkcs7 structure into a memory BIO
        int decryptOk = PKCS7_decrypt(pkcs7, 
                                      privateKey,
                                      publicKeyCert, 
                                      decryptedBodyBioBuf, 
                                      0);

        // Unable to decrypt
        if(!decryptOk)
        {
            OsSysLog::add(FAC_SIP, PRI_ERR,
                "Unable to decrypt S/MIME message using OpenSSL\n");
        }

        else
        {
            // Get the data from the decrypted BIO
            BUF_MEM *bioMemoryStructure;
            BIO_get_mem_ptr(decryptedBodyBioBuf, &bioMemoryStructure);
            if (bioMemoryStructure)
            {
                decryptedData.append(bioMemoryStructure->data,
                                     bioMemoryStructure->length);
                decryptSucceeded = TRUE;
            }
        }
    }

    // Free up the BIOs
    if(encryptedBodyBioBuf) 
    {
        BIO_free(encryptedBodyBioBuf);
        encryptedBodyBioBuf = NULL;
    }
    if(decryptedBodyBioBuf) 
    {
        BIO_free(decryptedBodyBioBuf);
        decryptedBodyBioBuf;
    }

#else
    OsSysLog::add(FAC_SIP, PRI_ERR, "SmimeBody::opensslSmimeDecrypt invoked with ENABLE_OPENSSL_SMIME not defined");
#endif

    return(decryptSucceeded);
}

UtlBoolean SmimeBody::convertPemToDer(UtlString& pemData,
                                      UtlString& derData)
{
    UtlBoolean conversionSucceeded = FALSE;
    derData.remove(0);

#ifdef ENABLE_OPENSSL_SMIME
    // Convert PEM to openssl X509
    BIO* certPemBioBuf = BIO_new_mem_buf((void*)pemData.data(),
                                      pemData.length());
    if(certPemBioBuf)
    {
        X509* x509 = PEM_read_bio_X509(certPemBioBuf, NULL, NULL, NULL);
        BIO* certDerBio = BIO_new(BIO_s_mem());

        if(x509 && certDerBio)
        {
            // Convert the X509 to a DER buffer
            i2d_X509_bio(certDerBio, x509);

            BUF_MEM* certDerBioMem = NULL;
            BIO_get_mem_ptr(certDerBio, &certDerBioMem);
            if(certDerBioMem && certDerBioMem->data && certDerBioMem->length > 0)
            {
                derData.append(certDerBioMem->data, certDerBioMem->length);
            }
            BIO_free(certDerBio);
            certDerBio = NULL;
            X509_free(x509);
            x509 = NULL;
        }
        BIO_free(certPemBioBuf);
        certPemBioBuf = NULL;
    }

#elif ENABLE_NSS_SMIME
    // Code from NSS secutil.c

    char* body = NULL;
    char* pemDataPtr = (char*) pemData.data();

	/* check for headers and trailers and remove them */
	if ((body = strstr(pemDataPtr, "-----BEGIN")) != NULL) {
	    char *trailer = NULL;
	    pemData = body;
	    body = PORT_Strchr(body, '\n');
	    if (!body)
		body = PORT_Strchr(pemDataPtr, '\r'); /* maybe this is a MAC file */
	    if (body)
		trailer = strstr(++body, "-----END");
	    if (trailer != NULL) {
		*trailer = '\0';
	    } else {
		OsSysLog::add(FAC_SIP, PRI_ERR, 
            "input has header but no trailer\n");
	    }
	} else {
	    body = pemDataPtr;
	}
     
	/* Convert to binary */
    SECItem derItem;
    derItem.data = NULL;
    derItem.len = 0;
	if(ATOB_ConvertAsciiToItem(&derItem, body))
    {
        OsSysLog::add(FAC_SIP, PRI_ERR, 
            "error converting PEM base64 data to binary");
    }
    else
    {
        derData.append(((char*)derItem.data), derItem.len);
        conversionSucceeded = TRUE;
    }
#else
    OsSysLog::add(FAC_SIP, PRI_ERR, 
        "SmimeBody::convertPemToDer implemented with NSS and OpenSSL disabled");
#endif

    return(conversionSucceeded);
}

/* ============================ ACCESSORS ================================= */

const HttpBody* SmimeBody::getDecyptedBody() const
{
    return(mpDecryptedBody);
}

/* ============================ INQUIRY =================================== */

UtlBoolean SmimeBody::isDecrypted() const
{
    return(mpDecryptedBody != NULL);
}

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ TESTING =================================== */

/* ============================ FUNCTIONS ================================= */

