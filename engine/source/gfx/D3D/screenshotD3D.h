//-----------------------------------------------------------------------------
// Torque Shader Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _SCREENSHOTD3D_H_
#define _SCREENSHOTD3D_H_

#include "gfx/screenshot.h"

//**************************************************************************
// D3D implementation of screenshot
//**************************************************************************
class ScreenShotD3D : public ScreenShot
{

public:

    /// captures the back buffer
    virtual void captureStandard();

    // captures a custom size - test
    virtual void captureCustom();

};


#endif  // _SCREENSHOTD3D_H_
