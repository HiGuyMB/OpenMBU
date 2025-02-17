//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#include "renderInteriorMgr.h"

#include "gfx/gfxTransformSaver.h"
#include "materials/sceneData.h"
#include "sceneGraph/sceneGraph.h"
#include "materials/matInstance.h"
#include "../../game/shaders/shdrConsts.h"


//**************************************************************************
// RenderInteriorMgr
//**************************************************************************
RenderInteriorMgr::RenderInteriorMgr()
{
    mBlankShader = NULL;
}

RenderInteriorMgr::~RenderInteriorMgr()
{
    if (mBlankShader)
        delete mBlankShader;
}



//-----------------------------------------------------------------------------
// setup scenegraph data
//-----------------------------------------------------------------------------
void RenderInteriorMgr::setupSGData(RenderInst* ri, SceneGraphData& data)
{
    Parent::setupSGData(ri, data);
    ri->miscTex = NULL;
}

//-----------------------------------------------------------------------------
// addElement
//-----------------------------------------------------------------------------
void RenderInteriorMgr::addElement(RenderInst* inst)
{
    mElementList.increment();
    MainSortElem& elem = mElementList.last();
    elem.inst = inst;
    elem.key = elem.key2 = 0;

    // sort by material and matInst
    if (inst->matInst)
    {
        elem.key = (U32)inst->matInst->getMaterial();
    }

    // sort by vertex buffer
    if (inst->vertBuff)
    {
        elem.key2 = (U32)inst->vertBuff->getPointer();
    }

}

//-----------------------------------------------------------------------------
// render
//-----------------------------------------------------------------------------
void RenderInteriorMgr::render()
{
   // Early out if nothing to draw.
    if (mElementList.empty())
        return;

    PROFILE_START(RenderInteriorMgrRender);

    // Automagically save & restore our viewport and transforms.
    GFXTransformSaver saver;


    SceneGraphData sgData;

    if (getCurrentClientSceneGraph()->isReflectPass())
    {
        GFX->setCullMode(GFXCullCW);
    }
    else
    {
        GFX->setCullMode(GFXCullCCW);
    }

    if (GFX->useZPass())
        renderZpass();

    for (U32 i = 0; i < TEXTURE_STAGE_COUNT; i++)
    {
        GFX->setTextureStageAddressModeU(i, GFXAddressWrap);
        GFX->setTextureStageAddressModeV(i, GFXAddressWrap);

        GFX->setTextureStageMagFilter(i, GFXTextureFilterLinear);
        GFX->setTextureStageMinFilter(i, GFXTextureFilterLinear);
        GFX->setTextureStageMipFilter(i, GFXTextureFilterLinear);
    }

    // turn on anisotropic only on base tex stage
    //GFX->setTextureStageMaxAnisotropy( 0, 2 );
    //GFX->setTextureStageMagFilter( 0, GFXTextureFilterAnisotropic );
    //GFX->setTextureStageMinFilter( 0, GFXTextureFilterAnisotropic );


    GFX->setZWriteEnable(true);
    GFX->setZEnable(true);


    U32 binSize = mElementList.size();

    GFXTextureObject* lastLM = NULL;
    GFXTextureObject* lastLNM = NULL;

    U32 changeCount = 0;

    for (U32 j = 0; j < mElementList.size(); )
    {
        RenderInst* ri = mElementList[j].inst;

        setupSGData(ri, sgData);
        MatInstance* mat = ri->matInst;

        U32 matListEnd = j;


        while (mat->setupPass(sgData))
        {
            changeCount++;
            U32 a;
            for (a = j; a < binSize; a++)
            {
                RenderInst* passRI = mElementList[a].inst;

                // no dynamics if glowing...
                RenderPassData *passData = mat->getPass(mat->getCurPass());
                if (passData && passData->glow && passRI->dynamicLight)
                    continue;

                // commented out the reflective check as it is what causes the interior flickering for v4. MBO used debug interior rendering, but that's not ideal.
                if (newPassNeeded(mat, passRI) || (a != j))// && passRI->reflective))  // if reflective, we want to reset textures in case this piece of geometry uses different reflect texture
                {
                    lastLM = NULL;  // pointer no longer valid after setupPass() call
                    lastLNM = NULL;
                    break;
                }

                if (passRI->type == RenderInstManager::RIT_InteriorDynamicLighting || Material::isDebugLightingEnabled())
                {
                    mat->setLightingBlendFunc();

                    setupSGData(passRI, sgData);
                    sgData.matIsInited = true;
                    mat->setLightInfo(sgData);
                }

                // fill in shader constants that change per draw
                //-----------------------------------------------
                mat->setWorldXForm(*passRI->worldXform);
                mat->setObjectXForm(*passRI->objXform);
                mat->setEyePosition(*passRI->objXform, gRenderInstManager.getCamPos());
                mat->setBuffers(passRI->vertBuff, passRI->primBuff);

                // This section of code is dangerous, it overwrites the
                // lightmap values in sgData.  This could be a problem when multiple
                // render instances use the same multi-pass material.  When
                // the first pass is done, setupPass() is called again on
                // the material, but the lightmap data has been changed in
                // sgData to the lightmaps in the last renderInstance rendered.

                // This section sets the lightmap data for the current batch.
                // For the first iteration, it sets the same lightmap data,
                // however the redundancy will be caught by GFXDevice and not
                // actually sent to the card.  This is done for simplicity given
                // the possible condition mentioned above.  Better to set always
                // than to get bogged down into special case detection.
                //-------------------------------------
                bool dirty = false;

                // set the lightmaps if different
                if (passRI->lightmap && passRI->lightmap->getPointer() != lastLM)
                {
                    sgData.lightmap = *passRI->lightmap;
                    lastLM = passRI->lightmap->getPointer();
                    dirty = true;
                }
                if (passRI->normLightmap && passRI->normLightmap->getPointer() != lastLNM)
                {
                    sgData.normLightmap = *passRI->normLightmap;
                    lastLNM = passRI->normLightmap->getPointer();
                    dirty = true;
                }

                if (dirty && (passRI->type != RenderInstManager::RIT_InteriorDynamicLighting))
                {
                    mat->setLightmaps(sgData);
                }
                //-------------------------------------

                // draw it
                if (passRI->prim)
                {
                    GFXPrimitive* prim = passRI->prim;
                    GFX->drawIndexedPrimitive(prim->type, prim->minIndex, prim->numVertices,
                        prim->startIndex, prim->numPrimitives);
                }
                else
                {
                    GFX->drawPrimitive(passRI->primBuffIndex);
                }

            }

            matListEnd = a;
        }

        // force increment if none happened, otherwise go to end of batch
        j = (j == matListEnd) ? j + 1 : matListEnd;

    }

    GFX->setLightingEnable(false);

    PROFILE_END();
}

void RenderInteriorMgr::renderZpass()
{
    if (!this->mBlankShader)
    {
        mBlankShader = (ShaderData*)Sim::findObject("BlankShader");
        if (!mBlankShader)
            Con::warnf("Can't find blank shader");
    }

    if (!mBlankShader || !mBlankShader->shader)
        return;

    GFX->enableColorWrites(false, false, false, false);

    mBlankShader->shader->process();

    GFXVertexBuffer* lastVB = NULL;
    GFXPrimitiveBuffer* lastPB = NULL;

    for (S32 i = 0; i < mElementList.size(); i++)
    {
        RenderInst* inst = mElementList[i].inst;
        GFX->setVertexShaderConstF(0, (float*)inst->worldXform, 4);
        if (lastVB != inst->vertBuff->getPointer())
        {
            GFX->setVertexBuffer(*inst->vertBuff);
            lastVB = inst->vertBuff->getPointer();
        }

        if (lastPB != inst->primBuff->getPointer())
        {
            GFX->setPrimitiveBuffer(*inst->primBuff);
            lastPB = inst->primBuff->getPointer();
        }

        GFXPrimitive* prim = inst->prim;
        if (prim)
        {
            GFX->drawIndexedPrimitive(prim->type, prim->minIndex, prim->numVertices, prim->startIndex, prim->numPrimitives);
            lastVB = NULL;
            lastPB = NULL;
        } else
        {
            GFX->drawPrimitive(inst->primBuffIndex);
        }
    }

    GFX->enableColorWrites(true, true, true, true);
}

