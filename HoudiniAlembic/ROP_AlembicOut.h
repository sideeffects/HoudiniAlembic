/*
 * Copyright (c) 2014
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

#ifndef __ROP_AlembicOut__
#define __ROP_AlembicOut__

#include <ROP/ROP_Node.h>
#include <ROP/ROP_Error.h>
#include <GABC/GABC_OError.h>

class ROP_AbcContext;
class ROP_AbcArchive;

class ROP_AlembicOut : public ROP_Node
{
public:
    typedef GABC_NAMESPACE::GABC_OError	GABC_OError;
    ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~ROP_AlembicOut();

    virtual int			startRender(int nframes, fpreal s, fpreal e);
    virtual ROP_RENDER_CODE	renderFrame(fpreal time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE	endRender();
    virtual bool		updateParmsFlags();
    virtual void	getDescriptiveParmName(UT_String &str) const
			{ str = "filename"; }

    /// We need to hint to the merge ROP that we can't be called on frame at a
    /// time.
    virtual void	buildRenderDependencies(
				const ROP_RenderDepParms &p);

    void	abcError(const char *message)
		{
		    addError(ROP_MESSAGE, message);
		}
    void	abcWarning(const char *message)
		{
		    addWarning(ROP_MESSAGE, message);
		}
    void	abcInfo(int verbose, const char *message)
		{
		    if (verbose < myVerbose)
			addMessage(ROP_MESSAGE, message);
		}

protected:
    void	close();
    bool	filterNode(OP_Node *obj, fpreal now);

    void	FILENAME(UT_String &str, fpreal time)
		    { getOutputOverrideEx(str, time, "filename", "mkpath"); }
    void	FORMAT(UT_String &str, fpreal time)
		    { evalString(str, "format", 0, time); }
    void	ROOT(UT_String &str, fpreal time)
		    { evalString(str, "root", 0, time); }
    void	OBJECTS(UT_String &str, fpreal time)
		    { evalString(str, "objects", 0, time); }
    int		COLLAPSE(fpreal time);
    bool	USE_INSTANCING(fpreal time)
		    { return evalInt("use_instancing", 0, time) != 0; }
    bool	SAVE_HIDDEN(fpreal time)
		    { return evalInt("save_hidden", 0, time) != 0; }
    bool	SAVE_ATTRIBUTES(fpreal time)
		    { return evalInt("save_attributes", 0, time) != 0; }
    bool	DISPLAYSOP(fpreal time)
		    { return evalInt("displaysop", 0, time) != 0; }
    bool	FULL_BOUNDS(fpreal time)
		    { return evalInt("full_bounds", 0, time) != 0; }
    bool	BUILD_HIERARCHY_FROM_PATH(fpreal time)
		    { return evalInt("build_from_path", 0, time) != 0; }
    void        PATH_ATTRIBUTE(UT_String &sval, fpreal time)
                    { evalString(sval, "path_attrib", 0, time); }
    void        PACKED_ABC_PRIORITY(UT_String &sval, fpreal time)
                    { evalString(sval, "packed_priority", 0, time); }
    void	FACESET_MODE(UT_String &sval, fpreal time)
		    { evalString(sval, "facesets", 0, time); }
    void	PARTITION_MODE(UT_String &sval, fpreal time)
		    { evalString(sval, "partition_mode", 0, time); }
    void	PARTITION_ATTRIBUTE(UT_String &str, fpreal time)
		    { evalString(str, "partition_attribute", 0, time); }
    void	SUBDGROUP(UT_String &str, fpreal time)
		    { evalString(str, "subdgroup", 0, time); }
    int		VERBOSE(fpreal time)
		    { return evalInt("verbose", 0, time); }
    bool	MOTIONBLUR(fpreal time)
		    { return evalInt("motionBlur", 0, time) != 0; }
    fpreal	SHUTTEROPEN(fpreal time)
		    { return evalFloat("shutter", 0, time); }
    fpreal	SHUTTERCLOSE(fpreal time)
		    { return evalFloat("shutter", 1, time); }
    int		SAMPLES(fpreal time)
		    { return evalInt("samples", 0, time); }
    bool	INITSIM(fpreal time)
		    { return evalInt("initsim", 0, time) != 0; }
    bool	RENDER_FULL_RANGE()
		    { return evalInt("render_full_range", 0, 0.0) != 0; }

    ROP_AbcArchive	*myArchive;
    ROP_AbcContext	*myContext;
    GABC_OError		*myError;
    fpreal		 myEndTime;
    int			 myVerbose;
    bool		 myFirstFrame;
};

#endif

