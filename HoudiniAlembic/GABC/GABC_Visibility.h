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

#ifndef __GABC_Visibility__
#define __GABC_Visibility__

#include "GABC_API.h"
#include "GABC_ChannelCache.h"

namespace GABC_NAMESPACE
{
enum GABC_VisibilityType
{
    GABC_VISIBLE_DEFER		= -1,
    GABC_VISIBLE_HIDDEN		= 0,
    GABC_VISIBLE_VISIBLE	= 1
};

class GABC_API GABC_VisibilityCache
{
public:
    GABC_VisibilityCache()
	: myVisible(GABC_VISIBLE_DEFER)
	, myCache(NULL)
    {
    }
    GABC_VisibilityCache(GABC_VisibilityType vtype,
			const GABC_ChannelCache *cache)
	: myVisible(vtype)
	, myCache(NULL)
    {
	if (cache)
	    set(vtype, cache);
    }
    GABC_VisibilityCache(const GABC_VisibilityCache &src)
	: myVisible(src.myVisible)
	, myCache(NULL)
    {
	*this = src;
    }
    ~GABC_VisibilityCache()
    {
	clear();
    }

    GABC_VisibilityCache	&operator=(const GABC_VisibilityCache &src)
    {
	if (this != &src)
	{
	    clear();
	    myVisible = src.myVisible;
	    myCache = src.myCache ? new GABC_ChannelCache(*src.myCache) : NULL;
	}
	return *this;
    }

    int64       getMemoryUsage(bool inclusive) const;

    void	clear();
    bool	valid() const	{ return myVisible != GABC_VISIBLE_DEFER; }
    bool	animated() const	{ return myCache != NULL; }

    bool	visible() const	{ return myVisible != GABC_VISIBLE_HIDDEN; }
    void	update(fpreal t)
		{
		    if (myCache)
		    {
			exint	sample = myCache->getSample(t);
			if (myCache->data()->getI32(sample) != 0)
			    myVisible = GABC_VISIBLE_VISIBLE;
			else
			    myVisible = GABC_VISIBLE_HIDDEN;
		    }
		}

    const GABC_ChannelCache	*cache() const	{ return myCache; }

    // Set visibility to a specific value (whether animated or static)
    void	set(GABC_VisibilityType vtype, const GABC_ChannelCache *vcache=NULL);

private:
    GABC_ChannelCache	*myCache;
    GABC_VisibilityType	 myVisible;
};
}

#endif
