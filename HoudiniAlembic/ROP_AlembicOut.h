#ifndef __ROP_AlembicOut__
#define __ROP_AlembicOut__

#include <ROP/ROP_Node.h>
#include <ROP/ROP_Error.h>

class GABC_OError;
class ROP_AbcContext;
class ROP_AbcArchive;

class ROP_AlembicOut : public ROP_Node
{
public:
    ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~ROP_AlembicOut();

    virtual int			startRender(int nframes, fpreal s, fpreal e);
    virtual ROP_RENDER_CODE	renderFrame(fpreal time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE	endRender();
    virtual bool		updateParmsFlags();
    virtual void	getDescriptiveParmName(UT_String &str) const
			{ str = "filename"; }

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
    void	ROOT(UT_String &str, fpreal time)
		    { evalString(str, "root", 0, time); }
    void	OBJECTS(UT_String &str, fpreal time)
		    { evalString(str, "objects", 0, time); }
    bool	COLLAPSE(fpreal time)
		    { return evalInt("collapse", 0, time) != 0; }
    bool	SAVE_ATTRIBUTES(fpreal time)
		    { return evalInt("save_attributes", 0, time) != 0; }
    bool	FULL_BOUNDS(fpreal time)
		    { return evalInt("full_bounds", 0, time) != 0; }
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

    ROP_AbcArchive	*myArchive;
    ROP_AbcContext	*myContext;
    GABC_OError		*myError;
    fpreal		 myEndTime;
    int			 myVerbose;
    bool		 myFirstFrame;
};

#endif

