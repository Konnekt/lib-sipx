//------------------------------------------------------------------------------
// Header generated by wxDesigner from file: sipXezPhone.wdr
// Do not modify this file, all changes will be lost!
//------------------------------------------------------------------------------

#ifndef __WDR_sipXezPhone_H__
#define __WDR_sipXezPhone_H__

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "sipXezPhone_wdr.h"
#endif

// Include wxWindows' headers

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/toolbar.h>

// Declare window functions

#define ID_TEXT 10000
#define ID_IDENTITY_CTRL 6002
#define ID_REALM_CTRL 6001
#define ID_USERNAME_CTRL 6003
#define ID_PASSWORD_CTRL 6004
#define ID_PROXY_SERVER_CTRL 6005
#define ID_ENABLE_RPORT_CTRL 6006
#define ID_STUN_SERVER_CTRL 6007
#define ID_ENABLE_AUTO_ANSWER 6018
wxSizer *sipXezPhoneSettingsDlgFunc( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_STATICBITMAP 10001
wxSizer *sipXezPhoneAboutDlgFunc( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

// Declare menubar functions

// Declare toolbar functions

// Declare bitmap functions

wxBitmap sipXezPhoneBitmapsFunc( size_t index );

#endif

// End of generated file
