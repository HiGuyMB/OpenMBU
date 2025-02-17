//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "game/tsStatic.h"
#include "core/bitStream.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "math/mathIO.h"
#include "ts/tsShapeInstance.h"
#include "console/consoleTypes.h"
#include "game/shapeBase.h"
#include "game/shadow.h"
#include "sceneGraph/detailManager.h"
#include "sim/netConnection.h"
#include "gfx/gfxDevice.h"
#include "lightingSystem/sgLighting.h"

IMPLEMENT_CO_NETOBJECT_V1(TSStatic);


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
TSStatic::TSStatic()
{
    overrideOptions = false;

    mNetFlags.set(Ghostable | ScopeAlways);

    mTypeMask |= StaticObjectType | StaticTSObjectType |
        StaticRenderedObjectType | ShadowCasterObjectType;

    mShapeName = "";
    mShapeInstance = NULL;
    mShadow = NULL;

    mConvexList = new Convex;
}

TSStatic::~TSStatic()
{
    delete mConvexList;
    mConvexList = NULL;
    delete mShadow;
}

//--------------------------------------------------------------------------
void TSStatic::initPersistFields()
{
    Parent::initPersistFields();

    addGroup("Media");
    addField("shapeName", TypeFilename, Offset(mShapeName, TSStatic));
    endGroup("Media");

    addGroup("Lighting");
    addField("receiveSunLight", TypeBool, Offset(receiveSunLight, SceneObject));
    addField("receiveLMLighting", TypeBool, Offset(receiveLMLighting, SceneObject));
    //addField("useAdaptiveSelfIllumination", TypeBool, Offset(useAdaptiveSelfIllumination, SceneObject));
    addField("useCustomAmbientLighting", TypeBool, Offset(useCustomAmbientLighting, SceneObject));
    //addField("customAmbientSelfIllumination", TypeBool, Offset(customAmbientForSelfIllumination, SceneObject));
    addField("customAmbientLighting", TypeColorF, Offset(customAmbientLighting, SceneObject));
    addField("lightGroupName", TypeString, Offset(lightGroupName, SceneObject));
    endGroup("Lighting");
}

void TSStatic::inspectPostApply()
{
    if (isServerObject()) {
        setMaskBits(0xffffffff);
    }
}

//--------------------------------------------------------------------------
bool TSStatic::onAdd()
{
    if (!Parent::onAdd())
        return false;

    if (!mShapeName || mShapeName[0] == '\0') {
        Con::errorf("TSStatic::onAdd: no shape name!");
        return false;
    }
    mShapeHash = _StringTable::hashString(mShapeName);

    mShape = ResourceManager->load(mShapeName);

    if (bool(mShape) == false)
    {
        Con::errorf("TSStatic::onAdd: unable to load shape: %s", mShapeName);
        return false;
    }

    if (!gSPMode && isClientObject() && !mShape->preloadMaterialList() && NetConnection::filesWereDownloaded())
        return false;

    mObjBox = mShape->bounds;
    resetWorldBox();
    setRenderTransform(mObjToWorld);

    mShapeInstance = new TSShapeInstance(mShape, isClientObject() || gSPMode);

    // Scan out the collision hulls...
    U32 i;
    for (i = 0; i < mShape->details.size(); i++)
    {
        char* name = (char*)mShape->names[mShape->details[i].nameIndex];

        if (dStrstr((const char*)dStrlwr(name), "collision-"))
        {
            mCollisionDetails.push_back(i);

            // The way LOS works is that it will check to see if there is a LOS detail that matches
            // the the collision detail + 1 + MaxCollisionShapes (this variable name should change in
            // the future). If it can't find a matching LOS it will simply use the collision instead.
            // We check for any "unmatched" LOS's further down
            mLOSDetails.increment();

            char buff[128];
            dSprintf(buff, sizeof(buff), "LOS-%d", i + 1 + MaxCollisionShapes);
            U32 los = mShape->findDetail(buff);
            if (los == -1)
                mLOSDetails.last() = i;
            else
                mLOSDetails.last() = los;
        }
    }

    // Snag any "unmatched" LOS details
    for (i = 0; i < mShape->details.size(); i++)
    {
        char* name = (char*)mShape->names[mShape->details[i].nameIndex];

        if (dStrstr((const char*)dStrlwr(name), "los-"))
        {
            // See if we already have this LOS
            bool found = false;
            for (U32 j = 0; j < mLOSDetails.size(); j++)
            {
                if (mLOSDetails[j] == i)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                mLOSDetails.push_back(i);
        }
    }

    // Compute the hull accelerators (actually, just force the shape to compute them)
    for (i = 0; i < mCollisionDetails.size(); i++)
        mShapeInstance->getShape()->getAccelerator(mCollisionDetails[i]);

    addToScene();

    return true;
}


void TSStatic::onRemove()
{
    mConvexList->nukeList();

    removeFromScene();

    delete mShapeInstance;
    mShapeInstance = NULL;
    delete mShadow;
    mShadow = NULL;

    Parent::onRemove();
}



//--------------------------------------------------------------------------
bool TSStatic::prepRenderImage(SceneState* state, const U32 stateKey,
    const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
    if (isLastState(state, stateKey))
        return false;
    setLastState(state, stateKey);

    // This should be sufficient for most objects that don't manage zones, and
    //  don't need to return a specialized RenderImage...

    if (mShapeInstance && (state->isObjectRendered(this) || getCurrentClientSceneGraph()->isReflectPass()))
    {
        Point3F cameraOffset;
        getRenderTransform().getColumn(3, &cameraOffset);
        cameraOffset -= state->getCameraPosition();
        F32 dist = cameraOffset.len();
        if (dist < 0.01)
            dist = 0.01;
        F32 fogAmount = state->getHazeAndFog(dist, cameraOffset.z);
        if (fogAmount > 0.99f)
            return false;

        F32 invScale = (1.0f / getMax(getMax(mObjScale.x, mObjScale.y), mObjScale.z));
        DetailManager::selectPotentialDetails(mShapeInstance, dist, invScale);
        if (mShapeInstance->getCurrentDetail() < 0)
            return false;


        renderObject(state);
    }

    return false;
}


void TSStatic::setTransform(const MatrixF& mat)
{
    Parent::setTransform(mat);

    // Since the interior is a static object, it's render transform changes 1 to 1
    //  with it's collision transform
    setRenderTransform(mat);
}


void TSStatic::renderObject(SceneState* state)
{

    MatrixF proj = GFX->getProjectionMatrix();
    RectI viewport = GFX->getViewport();

    MatrixF world = GFX->getWorldMatrix();
    TSMesh::setCamTrans(world);
    TSMesh::setSceneState(state);
    TSMesh::setObject(this);

    GFX->pushWorldMatrix();


    getCurrentClientSceneGraph()->getLightManager()->sgSetupLights(this);

    MatrixF mat = getRenderTransform();
    mat.scale(mObjScale);
    GFX->setWorldMatrix(mat);

    mShapeInstance->animate();
    mShapeInstance->render();

    getCurrentClientSceneGraph()->getLightManager()->sgResetLights();


    GFX->popWorldMatrix();


    GFX->setProjectionMatrix(proj);
    GFX->setViewport(viewport);

}

U32 TSStatic::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
    U32 retMask = Parent::packUpdate(con, mask, stream);

    mathWrite(*stream, getTransform());
    mathWrite(*stream, getScale());
    stream->writeString(mShapeName);

    // these fields are static, so try to avoid transmit
    if (stream->writeFlag(mask & advancedStaticOptionsMask))
    {
        stream->writeFlag(receiveSunLight);
        stream->writeFlag(useAdaptiveSelfIllumination);
        stream->writeFlag(useCustomAmbientLighting);
        stream->writeFlag(customAmbientForSelfIllumination);
        stream->write(customAmbientLighting);
        stream->writeFlag(receiveLMLighting);
        if (isServerObject())
        {
            lightIds.clear();
            findLightGroup(con);

            U32 maxcount = getMin(lightIds.size(), SG_TSSTATIC_MAX_LIGHTS);
            stream->writeInt(maxcount, SG_TSSTATIC_MAX_LIGHT_SHIFT);
            for (U32 i = 0; i < maxcount; i++)
            {
                stream->writeInt(lightIds[i], NetConnection::GhostIdBitSize);
            }
        }
        else
        {
            // recording demo...
            U32 maxcount = getMin(lightIds.size(), SG_TSSTATIC_MAX_LIGHTS);
            stream->writeInt(maxcount, SG_TSSTATIC_MAX_LIGHT_SHIFT);
            for (U32 i = 0; i < maxcount; i++)
            {
                stream->writeInt(lightIds[i], NetConnection::GhostIdBitSize);
            }
        }
    }

    return retMask;
}


void TSStatic::unpackUpdate(NetConnection* con, BitStream* stream)
{
    Parent::unpackUpdate(con, stream);

    MatrixF mat;
    Point3F scale;
    mathRead(*stream, &mat);
    mathRead(*stream, &scale);
    setScale(scale);
    setTransform(mat);

    mShapeName = stream->readSTString();

    if (stream->readFlag())
    {
        receiveSunLight = stream->readFlag();
        useAdaptiveSelfIllumination = stream->readFlag();
        useCustomAmbientLighting = stream->readFlag();
        customAmbientForSelfIllumination = stream->readFlag();
        stream->read(&customAmbientLighting);
        receiveLMLighting = stream->readFlag();

        U32 count = stream->readInt(SG_TSSTATIC_MAX_LIGHT_SHIFT);
        lightIds.clear();
        for (U32 i = 0; i < count; i++)
        {
            S32 id = stream->readInt(NetConnection::GhostIdBitSize);
            lightIds.push_back(id);
        }
    }
}


//--------------------------------------------------------------------------
//----------------------------------------------------------------------------
bool TSStatic::castRay(const Point3F& start, const Point3F& end, RayInfo* info)
{
    if (mShapeInstance)
    {
        RayInfo shortest;
        shortest.t = 1e8;

        info->object = NULL;
        for (U32 i = 0; i < mLOSDetails.size(); i++)
        {
            mShapeInstance->animate(mLOSDetails[i]);
            if (mShapeInstance->castRay(start, end, info, mLOSDetails[i]))
            {
                info->object = this;
                if (info->t < shortest.t)
                    shortest = *info;
            }
        }

        if (info->object == this) {
            // Copy out the shortest time...
            *info = shortest;
            return true;
        }
    }

    return false;
}


//----------------------------------------------------------------------------
bool TSStatic::buildPolyList(AbstractPolyList* polyList, const Box3F&, const SphereF&)
{
    if (mShapeInstance) {
        bool ret = false;

        polyList->setTransform(&mObjToWorld, mObjScale);
        polyList->setObject(this);

        for (U32 i = 0; i < mCollisionDetails.size(); i++)
        {
            mShapeInstance->buildPolyList(polyList, mCollisionDetails[i]);
            ret = true;
        }

        return ret;
    }

    return false;
}


void TSStatic::buildConvex(const Box3F& box, Convex* convex)
{
    if (mShapeInstance == NULL)
        return;

    // These should really come out of a pool
    mConvexList->collectGarbage();

    Box3F realBox = box;
    mWorldToObj.mul(realBox);
    realBox.min.convolveInverse(mObjScale);
    realBox.max.convolveInverse(mObjScale);

    if (realBox.isOverlapped(getObjBox()) == false)
        return;

    for (U32 i = 0; i < mCollisionDetails.size(); i++)
    {
        // If there is no convex "accelerator" for this detail,
        // there's nothing to collide with.
        TSShape::ConvexHullAccelerator* pAccel =
            mShapeInstance->getShape()->getAccelerator(mCollisionDetails[i]);
        if (!pAccel || !pAccel->numVerts)
            continue;

        // See if this hull exists in the working set already...
        Convex* cc = 0;
        CollisionWorkingList& wl = convex->getWorkingList();
        for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
            if (itr->mConvex->getType() == TSStaticConvexType &&
                (static_cast<TSStaticConvex*>(itr->mConvex)->pStatic == this &&
                    static_cast<TSStaticConvex*>(itr->mConvex)->hullId == i)) {
                cc = itr->mConvex;
                break;
            }
        }
        if (cc)
            continue;

        // Create a new convex.
        TSStaticConvex* cp = new TSStaticConvex;
        mConvexList->registerObject(cp);
        convex->addToWorkingList(cp);
        cp->mObject = this;
        cp->pStatic = this;
        cp->hullId = i;
        cp->box = mObjBox;
        cp->findNodeTransform();
    }
}

void TSStatic::setShapeName(const char* shapeName)
{
    mShapeName = StringTable->insert(shapeName);
}

void TSStatic::setSequence(const char* sequenceName)
{
    if (mShape && mShapeInstance)
    {
        S32 seq = mShape->findSequence(sequenceName);
        if (seq >= 0)
        {
            TSThread* thread = mShapeInstance->addThread();
            mShapeInstance->setSequence(thread, seq, 0.0f);
        }
    }
}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void TSStaticConvex::findNodeTransform()
{
    S32 dl = pStatic->mCollisionDetails[hullId];

    TSShapeInstance* si = pStatic->mShapeInstance;
    TSShape* shape = si->getShape();

    const TSShape::Detail* detail = &shape->details[dl];
    S32 subs = detail->subShapeNum;
    S32 start = shape->subShapeFirstObject[subs];
    S32 end = start + shape->subShapeNumObjects[subs];

    // Find the first object that contains a mesh for this
    // detail level. There should only be one mesh per
    // collision detail level.
    for (S32 i = start; i < end; i++) {
        const TSShape::Object* obj = &shape->objects[i];
        if (obj->numMeshes && detail->objectDetailNum < obj->numMeshes) {
            nodeTransform = &si->mNodeTransforms[obj->nodeIndex];
            return;
        }
    }
    return;
}

const MatrixF& TSStaticConvex::getTransform() const
{
    // Multiply on the mesh shape offset
    // tg: Returning this static here is not really a good idea, but
    // all this Convex code needs to be re-organized.
    if (nodeTransform) {
        static MatrixF mat;
        mat.mul(mObject->getTransform(), *nodeTransform);
        return mat;
    }
    return mObject->getTransform();
}

Box3F TSStaticConvex::getBoundingBox() const
{
    return getBoundingBox(mObject->getTransform(), mObject->getScale());
}

Box3F TSStaticConvex::getBoundingBox(const MatrixF& mat, const Point3F& scale) const
{
    Box3F newBox = box;
    newBox.min.convolve(scale);
    newBox.max.convolve(scale);
    mat.mul(newBox);
    return newBox;
}

Point3F TSStaticConvex::support(const VectorF& v) const
{
    TSShape::ConvexHullAccelerator* pAccel =
        pStatic->mShapeInstance->getShape()->getAccelerator(pStatic->mCollisionDetails[hullId]);
    AssertFatal(pAccel != NULL, "Error, no accel!");

    F32 currMaxDP = mDot(pAccel->vertexList[0], v);
    U32 index = 0;
    for (U32 i = 1; i < pAccel->numVerts; i++) {
        F32 dp = mDot(pAccel->vertexList[i], v);
        if (dp > currMaxDP) {
            currMaxDP = dp;
            index = i;
        }
    }

    return pAccel->vertexList[index];
}


void TSStaticConvex::getFeatures(const MatrixF& mat, const VectorF& n, ConvexFeature* cf)
{
    cf->material = 0;
    cf->object = mObject;

    TSShape::ConvexHullAccelerator* pAccel =
        pStatic->mShapeInstance->getShape()->getAccelerator(pStatic->mCollisionDetails[hullId]);
    AssertFatal(pAccel != NULL, "Error, no accel!");

    F32 currMaxDP = mDot(pAccel->vertexList[0], n);
    U32 index = 0;
    U32 i;
    for (i = 1; i < pAccel->numVerts; i++) {
        F32 dp = mDot(pAccel->vertexList[i], n);
        if (dp > currMaxDP) {
            currMaxDP = dp;
            index = i;
        }
    }

    const U8* emitString = pAccel->emitStrings[index];
    U32 currPos = 0;
    U32 numVerts = emitString[currPos++];
    for (i = 0; i < numVerts; i++) {
        cf->mVertexList.increment();
        U32 index = emitString[currPos++];
        mat.mulP(pAccel->vertexList[index], &cf->mVertexList.last());
    }

    U32 numEdges = emitString[currPos++];
    for (i = 0; i < numEdges; i++) {
        U32 ev0 = emitString[currPos++];
        U32 ev1 = emitString[currPos++];
        cf->mEdgeList.increment();
        cf->mEdgeList.last().vertex[0] = ev0;
        cf->mEdgeList.last().vertex[1] = ev1;
    }

    U32 numFaces = emitString[currPos++];
    for (i = 0; i < numFaces; i++) {
        cf->mFaceList.increment();
        U32 plane = emitString[currPos++];
        mat.mulV(pAccel->normalList[plane], &cf->mFaceList.last().normal);
        for (U32 j = 0; j < 3; j++)
            cf->mFaceList.last().vertex[j] = emitString[currPos++];
    }
}


void TSStaticConvex::getPolyList(AbstractPolyList* list)
{
    list->setTransform(&pStatic->getTransform(), pStatic->getScale());
    list->setObject(pStatic);

    pStatic->mShapeInstance->animate(pStatic->mCollisionDetails[hullId]);
    pStatic->mShapeInstance->buildPolyList(list, pStatic->mCollisionDetails[hullId]);
}


