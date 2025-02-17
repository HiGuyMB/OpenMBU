//-----------------------------------------------------------------------------
// Torque Shader Engine
// 
// Copyright (c) 2003 GarageGames.Com
//-----------------------------------------------------------------------------

#ifndef _GFXCUBEMAP_H_
#define _GFXCUBEMAP_H_

#include "gfx/gfxTextureHandle.h"
#include "gfx/gfxDevice.h"

//**************************************************************************
// Cube map
//**************************************************************************
class GFXCubemap
{
    friend class GFXDevice;
private:
    // should only be called by GFXDevice
    virtual void setToTexUnit(U32 tuNum) = 0;

public:
    virtual void initStatic(GFXTexHandle* faces) = 0;
    virtual void initDynamic(U32 texSize) = 0;


    // pos is the position to generate cubemap from
    virtual void updateDynamic(Point3F& pos) = 0;

    virtual ~GFXCubemap() {}
};




#endif // GFXCUBEMAP
