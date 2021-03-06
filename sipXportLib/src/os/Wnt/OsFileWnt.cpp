//
// Copyright (C) 2004, 2005 Pingtel Corp.
// 
//
// $$
////////////////////////////////////////////////////////////////////////
//////


// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "os/Wnt/OsFileSystemWnt.h"
#include "os/Wnt/OsFileWnt.h"
#include "os/Wnt/OsPathWnt.h"
#include "os/Wnt/OsFileInfoWnt.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
const unsigned long CopyBufLen = 32768;

// STATIC VARIABLE INITIALIZATIONS

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
OsFileWnt::OsFileWnt(const OsPathBase& filename) :
OsFileBase(filename)
{
}

// Copy constructor
OsFileWnt::OsFileWnt(const OsFileWnt& rOsFileWnt) :
OsFileBase(rOsFileWnt)
{
    OsPathWnt path;
    rOsFileWnt.getFileName(path);
    mFilename = path;
    mOsFileHandle = rOsFileWnt.mOsFileHandle;
}

// Destructor
OsFileWnt::~OsFileWnt()
{
    if (mOsFileHandle)
        close(); //call our close
}

/* ============================ MANIPULATORS ============================== */




OsStatus OsFileWnt::setLength(unsigned long newLength)
{
    OsStatus stat = OS_SUCCESS;
    
    return stat;
}



OsStatus OsFileWnt::fileunlock()
{
    OsStatus retval = OS_SUCCESS;

    return retval;
}


OsStatus OsFileWnt::filelock(const int mode)
{
    OsStatus retval = OS_SUCCESS;

    return retval;
}

OsStatus OsFileWnt::getFileInfo(OsFileInfoBase& fileinfo) const
{
    OsStatus ret = OS_INVALID;

    struct stat stats;
    if (stat(mFilename,&stats) == 0)
    {
        ret = OS_SUCCESS;
        if (stats.st_mode & _S_IWRITE)
            fileinfo.mbIsReadOnly = FALSE;
        else
            fileinfo.mbIsReadOnly = TRUE;

        OsTime createTime(stats.st_ctime,0);
        fileinfo.mCreateTime = createTime;

        OsTime modifiedTime(stats.st_ctime,0);
        fileinfo.mCreateTime = modifiedTime;
        
        fileinfo.mSize = stats.st_size;
    }



    return ret;

}
/* ============================ ACCESSORS ================================= */





/* ============================ INQUIRY =================================== */

UtlBoolean OsFileWnt::isReadonly() const
{
    UtlBoolean retval = FALSE;
    
    OsFileInfoWnt info;
    getFileInfo(info);
    
    return info.mbIsReadOnly;
}





/* //////////////////////////// PROTECTED ///////////////////////////////// */
// Assignment operator
OsFileWnt& 
OsFileWnt::operator=(const OsFileWnt& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */



