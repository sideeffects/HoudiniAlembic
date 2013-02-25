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
 * NAME:	GABC_Visibility.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_Visibility__
#define __GABC_Visibility__

#include "GABC_API.h"
#include "GABC_ChannelCache.h"

class GABC_IObject;

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
    GABC_VisibilityCache(const GABC_IObject &obj, bool check_parent)
	: myVisible(GABC_VISIBLE_DEFER)
	, myCache(NULL)
    {
	// Initialize to the object's state (possibly deferred)
	set(obj, check_parent);
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

    /// Set visibility based on the object's value.  If the object doesn't have
    /// visibility defined, this will set the type to GABC_VISIBLE_DEFER and the
    /// method will return false.  If @c check_parent is set, the tree will be
    /// walked to compute the inherited visibility.
    bool	set(const GABC_IObject &obj, bool check_parent);

private:
    GABC_ChannelCache	*myCache;
    GABC_VisibilityType	 myVisible;
};

#endif
