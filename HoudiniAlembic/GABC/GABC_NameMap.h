/*
 * Copyright (c) 2013
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

#ifndef __GABC_NameMap__
#define __GABC_NameMap__

#include "GABC_API.h"
#include "GABC_Types.h"
#include <SYS/SYS_AtomicInt.h>
#include <UT/UT_SymbolTable.h>
#include <GA/GA_SharedLoadData.h>

class UT_JSONWriter;
class UT_JSONParser;

namespace GABC_NAMESPACE
{
/// Map to translate from Alembic attribute names to Houdini names
class GABC_API GABC_NameMap
{
public:
    typedef UT_SymbolMap<UT_String>	MapType;

    class LoadContainer : public GA_SharedLoadData
    {
    public:
	LoadContainer() {}
	virtual ~LoadContainer() {}
	GABC_NameMapPtr	myNameMap;
    };

     GABC_NameMap();
    ~GABC_NameMap();

    /// Number of entries in the map
    exint	entries() const	{ return myMap.entries(); }

    /// Compare equality
    bool	isEqual(const GABC_NameMap &src) const;

    /// @{
    /// Equality operator
    bool	operator==(const GABC_NameMap &src) const
		    { return isEqual(src); }
    bool	operator!=(const GABC_NameMap &src) const
		    { return !isEqual(src); }
    /// @}

    /// Get the name mapping.  If the name isn't mapped, the original name
    /// will be returned.
    /// If the attribute should be skipped, a NULL pointer will be returned.
    const char	*getName(const char *name) const;
    const char	*getName(const std::string &name) const
			{ return getName(name.c_str()); }

    /// Add a translation from the abcName to the houdini attribute name
    void		 addMap(const char *abcName, const char *houdiniName);

    /// Avoid adding an attribute of the given name.  This is done by 
    void		 skip(const char *abcName)
			    { addMap(abcName, NULL); }

    /// @{
    /// JSON I/O
    bool	save(UT_JSONWriter &w) const;
    static bool	load(GABC_NameMapPtr &map, UT_JSONParser &p);
    /// @}

    /// @{
    /// Reference counting
    void	incref()	{ myRefCount.add(1); }
    void	decref()
		{
		    if (!myRefCount.add(-1))
			delete this;
		}
    /// @}

private:
    MapType		myMap;
    SYS_AtomicInt32	myRefCount;
};

static inline void intrusive_ptr_add_ref(GABC_NameMap *m) { m->incref(); }
static inline void intrusive_ptr_release(GABC_NameMap *m) { m->decref(); }
}

#endif
