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

// APPLICATION INCLUDES
#include "stdwx.h"
#include "VideoPanel.h"
#include "sipXezPhoneSettings.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
// MACROS
BEGIN_EVENT_TABLE(VideoPanel, wxPanel)
END_EVENT_TABLE()

// Constructor
VideoPanel::VideoPanel(wxWindow* parent, const wxPoint& pos, const wxSize& size) :
   wxPanel(parent, IDR_VIDEO_PANEL, pos, size, wxTAB_TRAVERSAL, "VideoPanel")
{
    wxColor* pPanelColor = & (sipXezPhoneSettings::getInstance().getBackgroundColor());
    SetBackgroundColour(*pPanelColor);



    mpPreviewWindow = new VideoWindow(this,  wxPoint(2, 3), wxSize(320, 240));
    
    mpVideoWindow = new PreviewWindow(this,  wxPoint(2, 249), wxSize(320, 240));
    
}


// Destructor
VideoPanel::~VideoPanel()
{
}

void VideoPanel::UpdateBackground(wxColor color)
{
    SetBackgroundColour(color);
}
