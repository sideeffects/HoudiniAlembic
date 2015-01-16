/*
 * Copyright (c) 2015
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 */

#include "ROP_AbcContext.h"
#include "ROP_AbcGTInstance.h"
#include "ROP_AbcGTShape.h"
#include <GABC/GABC_OGTAbc.h>
#include <GABC/GABC_OGTGeometry.h>
#include <GABC/GABC_OXform.h>
#include <GABC/GABC_PackedImpl.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GU/GU_PrimPacked.h>

using namespace GABC_NAMESPACE;

namespace {
    typedef GABC_OXform     OXform;

    static UT_Matrix4D
    getTransform(const GT_PrimitiveHandle &prim)
    {
        GT_TransformHandle      transform;
        UT_Matrix4D             mat;

        transform = UTverify_cast<const GT_GEOPrimPacked *>(prim.get())
                ->getInstanceTransform();

        if (transform)
        {
            transform->getMatrix(mat);
        }
        else
        {
            mat.identity();
        }

        return mat;
    }

    static const GABC_PackedImpl *
    getPackedImpl(const GT_PrimitiveHandle &prim)
    {
        if (prim->getPrimitiveType() == GT_GEO_PACKED)
        {
            const GT_GEOPrimPacked *gt = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());
            const GU_PrimPacked    *gu = gt->getPrim();

            if (gu->getTypeId() == GABC_PackedImpl::typeId())
            {
                return UTverify_cast<const GABC_PackedImpl *>(gu->implementation());

            }
        }
	return NULL;
    }

    static GABC_NodeType
    getNodeType(const GT_PrimitiveHandle &prim)
    {
	const GABC_PackedImpl  *gabc = getPackedImpl(prim);
	return gabc ? gabc->object().nodeType() : GABC_UNKNOWN;
    }

    static exint
    getNumSamples(OXform *xform)
    {
        return xform->getSchema().getNumSamples();
    }
}

ROP_AbcGTShape::ROP_AbcGTShape(const std::string &name,
        const char *const path,
        InverseMap *const inv_map,
        GeoSet *const shape_set,
        XformMap *const xform_map,
        const ShapeType type,
        bool geo_lock)
    : myInverseMap(inv_map)
    , myGeoSet(shape_set)
    , myType(type)
    , myXformMap(xform_map)
    , myName(name)
    , myElapsedFrames(0)
    , myPrimType(GT_PRIM_UNDEFINED)
    , myGeoLock(geo_lock)
{
    // Split the path (if we're using one) into its transform components.
    if (path)
    {
        myPath = UT_String(path);
        myPath.tokenize(myTokens, '/');
    }

    myObj.myVoidPtr = NULL;
}

ROP_AbcGTShape::~ROP_AbcGTShape()
{
    clear();
}

void
ROP_AbcGTShape::clear()
{
    switch (myType)
    {
        case INSTANCE:
            if (myObj.myInstance)
            {
                delete myObj.myInstance;
            }
            break;

        case GEOMETRY:
            if (myObj.myShape)
            {
                delete myObj.myShape;
            }
            break;

        case ALEMBIC:
            if (myObj.myAlembic)
            {
                delete myObj.myAlembic;
            }
            break;

        default:
            UT_ASSERT(0 && "GTShape declared without type");
    }

    myObj.myVoidPtr = NULL;
}

bool
ROP_AbcGTShape::isPrimitiveSupported(const GT_PrimitiveHandle &prim)
{
    return GABC_OGTGeometry::isPrimitiveSupported(prim);
}

bool
ROP_AbcGTShape::firstFrame(const GT_PrimitiveHandle &prim,
        const OObject &parent,
        const ObjectVisibility vis,
        const ROP_AbcContext &ctx,
        GABC_OError &err,
        bool calc_inverse,
        const bool subd_mode,
        const bool add_unused_pts)
{
    const OObject		*pobj = &parent;
    OXform			*xform = NULL;
    const UT_Matrix4D		*stored_mat = NULL;
    UT_Matrix4D			 inverse_mat(1.0);
    UT_Matrix4D			 packed_mat(1.0);
    InverseMap::iterator	 i_it;
    XformMap::iterator		 x_it;
    std::string			 partial_path = "";
    fpreal			 time;
    int				 numt = myTokens.entries();
    int				 numx = numt - 1;
    const char			*current_token;
    bool			 is_xform = (getNodeType(prim) == GABC_XFORM);
    bool			 abc_insert_xform = false;

    clear();
    ++myElapsedFrames;

    // If we're using a path:
    if (numt)
    {
        // If 'calc_inverse' is true, we compute the inverse of the ancestor
        // transformations as we go, to find the relative transform of an
        // object.
        //
        // We never do this for deforming geometry. For packed Alembics,
        // whether we do it or not depends on whether this is the first
        // packed Alembic we've encountered for this path and whether we
        // should preserve it's transform. For transforms, we always do it.
        if (!calc_inverse && is_xform)
        {
            calc_inverse = true;
        }

        // Go through the tokens, building up the path.
        for (int i = 0; i < numx; ++i)
        {
            current_token = myTokens.getArg(i);
            partial_path.append("/");
            partial_path.append(current_token);

            x_it = myXformMap->find(partial_path);
            if (x_it == myXformMap->end())
            {
                // We cannot have a transform have the same name as a
                // a shape: it would cause problems as we have to make
                // sure that the transform always comes first, so that
                // future paths that use it are still valid. Easier to
                // simply not allow it.
                if (myGeoSet->count(partial_path))
                {
                    err.error("Transform and geometry have same path %s.",
                            partial_path.c_str());
                    return false;
                }

                xform = new OXform(*pobj, current_token, ctx.timeSampling());
                myXformMap->insert(XformMapInsert(partial_path, xform));
            }
            else
            {
                xform = x_it->second;

                // Only compute the inverse if we find an existing transform.
                // Otherwise the transform matrix is just the identity matrix.
                if (calc_inverse)
                {
                    UT_ASSERT(myType == ALEMBIC);

                    stored_mat = xform->getSchema().getNextXform();
                    if (stored_mat)
                    {
                        stored_mat->invert(inverse_mat);
                        packed_mat = inverse_mat * packed_mat;
                    }
                }
            }

            // If the transform we're currently processing hasn't been
            // updated for this frame, AND it's either a) not the direct
            // parent to the object we own, or b) the object we own is deforming
            // geometry, THEN write the transform data for
            // the current frame.
            //
            // Partitions are sorted into order first by which ones
            // contain packed Alembics, then by the length of the path
            // (/a/b is shorter than /a/b/c), then alphabetically.
            //
            // CASE A:  If the OXform we're currently processing is not the
            //          direct parent, then we're at least one step removed from
            //          it (our object has path /a/b/c and we're
            //          currently looking at transform a). Due to the sorting
            //          method, this means that there are no more paths that
            //          use the current transform as a direct parent, so we can
            //          finalize it's sample.
            //
            // CASE B:  Due to the sorting order, the OXform we're currently
            //          processing must be the parent (otherwise case a would be
            //          triggered instead) and there must be no packed Alembics
            //          under the same path (if there were, we would have
            //          already encountered them and the first one should have
            //          called finalize on the OXform). No packed Alembics means
            //          nothing will mess with the OXform sample, so finalize.
            //
            if ((getNumSamples(xform) < myElapsedFrames)
                    &&  ((i < numx - 1) || (myType != ALEMBIC)))
            {
                xform->getSchema().finalize();
            }

            pobj = xform;
        }

        current_token = myTokens.getArg(numx);

        // If we've computed the inverse of the ancestor transforms:
        if (calc_inverse)
        {
            // Use it to get the relative transform for the packed Alembic
            // we're processing.
            packed_mat = packed_mat * getTransform(prim);

            // If we're processing a transform:
            if (is_xform) {
                // Create the transform object and set its transform.
                xform = new OXform(*pobj, myName, ctx.timeSampling());
                xform->getSchema().setMatrix(packed_mat);
            }
            // If we're processing an Alembic shape, it has a parent
            // transform, and this shape is the first one encountered
            // under this parent xform:
            else if (xform && (getNumSamples(xform) < myElapsedFrames))
            {
                // Merge the computed transform with the parent transform.
                stored_mat = xform->getSchema().getNextXform();
                if (stored_mat)
                {
                    packed_mat = packed_mat * (*stored_mat);
                }
                xform->getSchema().setMatrix(packed_mat);
                xform->getSchema().finalize();

                packed_mat.invert(inverse_mat);
                myInverseMap->insert(InverseMapInsert(partial_path,
                        inverse_mat));
            }
            // If we're processing an Alembic shape and we're prioritizing
            // transforms over hierarchy:
            //
            // NOTE: We can end up here if either the current shape is not the
            //       first packed Alembic under the parent transform, or there
            //       is no parent transform.
            else if (ctx.packedAlembicPriority()
                    == ROP_AbcContext::PRIORITY_TRANSFORM)
            {
                UT_ASSERT(!xform || (getNumSamples(xform) == myElapsedFrames));

                // The path we'll use to store this extra transform in the map
                std::string     extra_xform_path = partial_path;
                extra_xform_path.append(current_token);
                extra_xform_path.append("/");
                extra_xform_path.append(myName);

                // If a parent transform exists, we'll need to fetch the
                // inverse of the transformation we merged into it to compute
                // the relative transform.
                if (xform)
                {
                    i_it = myInverseMap->find(partial_path);
                    if (i_it == myInverseMap->end())
                    {
                        UT_ASSERT(0 && "Not first packed Alembic under parent,"
                                " but no inverse transform stored.");
                        return false;
                    }
                    packed_mat = packed_mat * i_it->second;
                }

                // Need to create a buffer transform even if subsequent
                // packed Alembics have the same transformation. This is because
                // they may not have the same transformation in future frames,
                // and it will be too late to create a buffer transform
                // since the shape will already be parented to the wrong
                // transform.
                xform = new OXform(*pobj, myName, ctx.timeSampling());
                xform->getSchema().setMatrix(packed_mat);
                xform->getSchema().finalize();

                myXformMap->insert(XformMapInsert(extra_xform_path, xform));
                pobj = xform;

                // Currently this warning is posted anytime multiple packed
                // Alembics are stored under the same path. We COULD make it
                // so that it's only printed when the multiple Alembics have
                // different transforms, but then we'd have to calculate the
                // inverse transform on all packed Alembics all the time.
                err.warning("Placing packed Alembic under path %s/%s/%s to"
                        " preserve transform.",
                        partial_path.c_str(),
                        myName.c_str(),
                        myName.c_str());
            }
            // If we're processing an Alembic shape and we're prioritizing
            // hierarchy over transforms:
            else
            {
                // This warning also posted whenever multiple packed Alembics
                // stored under the same path
                err.warning("Ignoring transform for packed Alembic %s/%s to"
                        " preserve hierarchy.",
                        partial_path.c_str(),
                        myName.c_str());
            }
        }
        // If we did not compute the inverse of the ancestor transforms and
        // we're processing deforming geometry:
        else if (myType != ALEMBIC)
        {
            i_it = myInverseMap->find(partial_path);

            if (i_it != myInverseMap->end())
            {
                prim->setPrimitiveTransform(GT_TransformHandle(
                        new GT_Transform(&(i_it->second), 1)));
            }
        }
        // If we did not compute the inverse of the ancestor transforms and
        // we're processing a packed Alembic (This only happens when
        // prioritizing hierarchy over transforms):
        else
        {
            err.warning("Ignoring transform for packed Alembic %s/%s to"
                    " preserve hierarchy.",
                    partial_path.c_str(),
                    myName.c_str());
        }

        // Complete the path
        partial_path.append("/");
        partial_path.append(current_token);

        if (is_xform)
        {
            // If processing a transform, check that there is no shape or
            // existing transform with the same name.
            //
            // We can't have multiple transforms with the same path;
            // one will have to have its name/path changed and won't
            // be used by any shapes, so there's no point.
            if (myGeoSet->count(partial_path))
            {
                err.error("Transform and geometry have same path %s.",
                        partial_path.c_str());
                delete xform;
                return false;
            }

            x_it = myXformMap->find(partial_path);
            if (x_it != myXformMap->end())
            {
                err.error("Multiple transforms have path %s.",
                        partial_path.c_str());
                delete xform;
                return false;
            }

            myXformMap->insert(XformMapInsert(partial_path, xform));
        }
        else
        {
            // If processing a shape, check that no transform exists with
            // this path.
            x_it = myXformMap->find(partial_path);
            if (x_it != myXformMap->end())
            {
                err.error("Transform and geometry have same path %s.",
                        partial_path.c_str());
                return false;
            }
            myGeoSet->insert(partial_path);
        }
    }
    else if (myType == ALEMBIC)
    {
	// We need to insert a transform to capture the transform on the packed
	// prim.
	abc_insert_xform = true;
    }

    myPrimType = prim->getPrimitiveType();

    //
    //  CREATE OBJ AND SAMPLE FIRST FRAME
    //

    switch (myType)
    {
        case INSTANCE:
            myObj.myInstance = new ROP_AbcGTInstance(myName, myGeoLock);
            return myObj.myInstance->first(*pobj,
                    err,
                    ctx,
                    prim,
                    subd_mode,
                    add_unused_pts,
                    vis);

        case GEOMETRY:
            myObj.myShape = new GABC_OGTGeometry(myName);
            return myObj.myShape->start(prim, *pobj, ctx, err, vis);

        case ALEMBIC:
            // Need to know at what time to sample the input Alembic archive.
            time = ctx.cookTime()
                + ctx.timeSampling()->getTimeSamplingType().getTimePerCycle();

            myObj.myAlembic = new GABC_OGTAbc(myName);

            if (is_xform)
            {
                // Transforms use their own function; they don't need to
                // have a new OObject made since they have one already, and
                // we don't need to check their type (we already know it).
                return myObj.myAlembic->startXform(prim,
                        xform,
                        time,
                        ctx,
                        err,
                        vis);
            }
            else
            {
                return myObj.myAlembic->start(prim, *pobj, time,
			ctx, err, vis, abc_insert_xform);
            }

        default:
            UT_ASSERT(0 && "Unknown type, how did this happen?");
            break;
    }

    return false;
}

bool
ROP_AbcGTShape::nextFrame(const GT_PrimitiveHandle &prim,
	const ROP_AbcContext &ctx,
	GABC_OError &err,
	bool calc_inverse)
{
    OXform                 *xform = NULL;
    const UT_Matrix4D      *stored_mat = NULL;
    UT_Matrix4D             inverse_mat;
    UT_Matrix4D             packed_mat;
    InverseMap::iterator    i_it;
    XformMap::iterator      x_it;
    std::string             partial_path = "";
    fpreal                  time;
    int                     numt = myTokens.entries();
    int                     numx = numt - 1;
    const char             *current_token;
    bool                    is_xform = (getNodeType(prim) == GABC_XFORM);

    ++myElapsedFrames;
    packed_mat.identity();

    // Same thing as when creating the first frame, but this time don't need
    // to make new transforms. They should already exist.
    if (numt)
    {
        if (!calc_inverse && is_xform)
        {
            calc_inverse = true;
        }

        for (int i = 0; i < numx; ++i)
        {
            current_token = myTokens.getArg(i);
            partial_path.append("/");
            partial_path.append(current_token);

            x_it = myXformMap->find(partial_path);
            if (x_it == myXformMap->end())
            {
                UT_ASSERT(0 && "Missing transform!");
                return false;
            }

            xform = x_it->second;

            if (calc_inverse)
            {
                UT_ASSERT(myType == ALEMBIC);

                stored_mat = xform->getSchema().getNextXform();
                if (stored_mat)
                {
                    stored_mat->invert(inverse_mat);
                    packed_mat = inverse_mat * packed_mat;
                }
            }

            UT_ASSERT(getNumSamples(xform) >= (myElapsedFrames - 1));

            if ((getNumSamples(xform) < myElapsedFrames)
                    &&  ((i < numx - 1) || (myType != ALEMBIC)))
            {
                xform->getSchema().finalize();
            }
        }

        current_token = myTokens.getArg(numx);

        if (calc_inverse)
        {
            packed_mat = packed_mat * getTransform(prim);

            if (is_xform) {
                // DO NOTHING
            }
            // First packed Alembic, regardless of prioritization
            else if (xform && (getNumSamples(xform) < myElapsedFrames))
            {
                stored_mat = xform->getSchema().getNextXform();
                if (stored_mat)
                {
                    packed_mat = packed_mat * (*stored_mat);
                }
                xform->getSchema().setMatrix(packed_mat);
                xform->getSchema().finalize();

                packed_mat.invert(inverse_mat);
                myInverseMap->insert(InverseMapInsert(partial_path,
                        inverse_mat));
            }
            // Subsequent packed Alembic, prioritizing transform
            //
            // A new transform is created even if the packed Alembic has
            // the same transform as the previous one, because there's no
            // garauntee it will in future frames.
            else if (ctx.packedAlembicPriority()
                    == ROP_AbcContext::PRIORITY_TRANSFORM)
            {
                UT_ASSERT(!xform || (getNumSamples(xform) == myElapsedFrames));

                std::string     extra_xform_path = partial_path;
                extra_xform_path.append(current_token);
                extra_xform_path.append("/");
                extra_xform_path.append(myName);

                if (xform)
                {
                    i_it = myInverseMap->find(partial_path);
                    if (i_it == myInverseMap->end())
                    {
                        UT_ASSERT(0 && "Not first packed Alembic under parent,"
                                " but no inverse transform stored.");
                        return false;
                    }
                    packed_mat = packed_mat * i_it->second;
                }

                x_it = myXformMap->find(extra_xform_path);
                if (x_it == myXformMap->end())
                {
                    UT_ASSERT(0 && "Missing transform!");
                    return false;
                }
                xform = x_it->second;
                xform->getSchema().setMatrix(packed_mat);
                xform->getSchema().finalize();
            }
        }
        else if (myType != ALEMBIC)
        {
            i_it = myInverseMap->find(partial_path);

            if (i_it != myInverseMap->end())
            {
                prim->setPrimitiveTransform(GT_TransformHandle(
                        new GT_Transform(&(i_it->second), 1)));
            }
        }

        // If we're processing a transform, we need to update its matrix.
        if (is_xform)
        {
            partial_path.append("/");
            partial_path.append(current_token);

            x_it = myXformMap->find(partial_path);
            if (x_it == myXformMap->end())
            {
                UT_ASSERT(0 && "Missing transform!");
                return false;
            }
            xform = x_it->second;
            xform->getSchema().setMatrix(packed_mat);
        }
    }

    switch (myType)
    {
        case INSTANCE:
            return myObj.myInstance->update(prim, ctx, err);

        case GEOMETRY:
            if (myGeoLock)
            {
                return myObj.myShape->updateFromPrevious(err,
                        Alembic::AbcGeom::kVisibilityDeferred);
            }
            else
            {
                return myObj.myShape->update(prim, ctx, err);
            }

        case ALEMBIC:
            if (myGeoLock)
            {
                return myObj.myAlembic->updateFromPrevious(err,
                        Alembic::AbcGeom::kVisibilityDeferred);
            }
            else
            {
                time = ctx.cookTime() +
                        ctx.timeSampling()->getTimeSamplingType().getTimePerCycle();
                return myObj.myAlembic->update(prim, time, ctx, err);
            }

        default:
            UT_ASSERT(0 && "Unknown type, how did this happen?");
    }

    return false;
}

bool
ROP_AbcGTShape::nextFrameFromPrevious(GABC_OError &err,
        ObjectVisibility vis,
        exint frames)
{
    OXform                 *xform = NULL;
    XformMap::iterator      x_it;
    std::string             partial_path = "";
    int                     numt = myTokens.entries();
    int                     numx = numt - 1;
    const char             *current_token;

    if (frames < 0)
    {
        UT_ASSERT(0 && "Attempted to update less than 0 frames.");
        return false;
    }

    if (!frames)
    {
        return true;
    }

    myElapsedFrames += frames;

    if (numt)
    {
        for (int i = 0; i < numx; ++i)
        {
            current_token = myTokens.getArg(i);
            partial_path.append("/");
            partial_path.append(current_token);

            x_it = myXformMap->find(partial_path);
            if (x_it == myXformMap->end())
            {
                UT_ASSERT(0 && "Missing transform!");
                return false;
            }

            xform = x_it->second;
            if (getNumSamples(xform) < myElapsedFrames)
            {
                for (exint i = 0; i < frames; ++i)
                {
                    xform->getSchema().setFromPrevious();
                }

                UT_ASSERT(getNumSamples(xform) == myElapsedFrames);
            }
        }

        // Check for the extra transforms that subsequent Alembics have
        // when we prioritize transforms over hierarchy.
        if (myType == ALEMBIC)
        {
            partial_path.append("/");
            partial_path.append(myTokens.getArg(numx));
            partial_path.append("/");
            partial_path.append(myName);

            x_it = myXformMap->find(partial_path);
            if (x_it != myXformMap->end())
            {
                x_it->second->getSchema().setFromPrevious();
            }
        }
    }

    switch (myType)
    {
        case INSTANCE:
            return myObj.myInstance->updateFromPrevious(err,
                    myPrimType,
                    vis,
                    frames);

        case GEOMETRY:
            return myObj.myShape->updateFromPrevious(err, vis, frames);

        case ALEMBIC:
            return myObj.myAlembic->updateFromPrevious(err, vis, frames);

        default:
            UT_ASSERT(0 && "Unknown type, how did this happen?");
    }

    return false;
}

int
ROP_AbcGTShape::getPrimitiveType() const
{
    return myPrimType;
}

bool
ROP_AbcGTShape::start(const OObject &, GABC_OError &,
	const ROP_AbcContext &, UT_BoundingBox &)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcGTShape::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcGTShape::selfTimeDependent() const
{
    return false;	// Only parent knows
}

bool
ROP_AbcGTShape::getLastBounds(UT_BoundingBox &) const
{
    return false;
}

Alembic::Abc::OObject
ROP_AbcGTShape::getOObject() const
{
    switch (myType)
    {
        case INSTANCE:
            return myObj.myInstance->getOObject();

        case GEOMETRY:
            return myObj.myShape->getOObject();

        case ALEMBIC:
            UT_ASSERT(0 && "Exported geometry will be incorrect");
            break;

        default:
            UT_ASSERT(0 && "Unknown type, how did this happen?");
    }

    return Alembic::Abc::OObject();
}

void
ROP_AbcGTShape::dump(int indent) const
{
    printf("%*sROP_AbcGTShape = {\n", indent, "");
    indent += 2;
    printf("%*sType := '%s'\n", indent, "", shapeType(myType));
    printf("%*sPath := '%s'\n", indent, "", myPath.isstring() ? myPath.buffer() : "null");
    printf("%*sName := '%s'\n", indent, "", myName.c_str());
    printf("%*sFrames := %d\n", indent, "", (int)myElapsedFrames);
    printf("%*sPrimType := %d\n", indent, "", myPrimType);
    switch (myType)
    {
	case GEOMETRY:
	    myObj.myShape->dump(indent);
	    break;
	case INSTANCE:
	    myObj.myInstance->dump(indent);
	    break;
	case ALEMBIC:
	    myObj.myAlembic->dump(indent);
	    break;
    }

    printf("%*s}\n", indent-2, "");
}
