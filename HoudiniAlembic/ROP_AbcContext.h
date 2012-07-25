/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	ROP_AbcContext.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcContext__
#define __ROP_AbcContext__

#include <OP/OP_Context.h>

/// Context for Alembic creation, containing options and frame information
class ROP_AbcContext
{
public:
    ROP_AbcContext()
	: myContext()
	, mySaveAttributes(true)
	, myPartitionAttribute()
    {
    }
    ROP_AbcContext(fpreal t)
	: myContext(t)
	, mySaveAttributes(true)
	, myPartitionAttribute()
    {
    }

    bool	 saveAttributes() const		{ return mySaveAttributes; }
    const char	*partitionAttribute() const	{ return myPartitionAttribute; }
    fpreal	 time() const			{ return myContext.getTime(); }
    OP_Context	&context() const
		 {
		     // The Houdini API expects a non-const OP_Context,
		     // however, for all intents and purposes, the context is
		     // const.
		     return (const_cast<ROP_AbcContext *>(this))->myContext;
		 }

    void	setContext(const OP_Context &c)	{ myContext = c; }
    void	setTime(fpreal t)		{ myContext.setTime(t); }
    void	setSaveAttributes(bool v)	{ mySaveAttributes = v; }
    void	setPartitionAttribute(const char *s)
		{
		    myPartitionAttribute.harden(s);
		}

private:
    OP_Context	myContext;
    UT_String	myPartitionAttribute;
    bool	mySaveAttributes;
};

#endif
