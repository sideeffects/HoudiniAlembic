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

#include "GABC_NameMap.h"
#include "GABC_IArchive.h"
#include "GABC_Util.h"
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

using namespace GABC_NAMESPACE;

GABC_NameMap::GABC_NameMap()
    : myMap()
    , myRefCount(0)
{
}

GABC_NameMap::~GABC_NameMap()
{
}

bool
GABC_NameMap::isEqual(const GABC_NameMap &src) const
{
    if (myMap.entries() != src.myMap.entries())
	return false;
    for (MapType::iterator it = myMap.begin(); !it.atEnd(); ++it)
    {
	UT_String	item;
	if (!src.myMap.findSymbol(it.name(), &item))
	    return false;
	if (item != it.thing())
	    return false;
    }
    return true;
}

const char *
GABC_NameMap::getName(const char *name) const
{
    UT_String	entry;
    if (myMap.findSymbol(name, &entry))
    {
	return (const char *)entry;
    }
    return name;
}

void
GABC_NameMap::addMap(const char *abcName, const char *houdiniName)
{
    UT_String	&entry = myMap[abcName];
    if (UTisstring(houdiniName))
	entry.harden(houdiniName);	// Replace existing entry
    else
	entry = NULL;
}

bool
GABC_NameMap::save(UT_JSONWriter &w) const
{
    bool	ok = true;
    ok = w.jsonBeginMap();
    for (MapType::iterator it = myMap.begin(); ok && !it.atEnd(); ++it)
    {
	const UT_String &str = it.thing();
	const char	*value = str.isstring() ? (const char *)str : "";
	ok = ok && w.jsonKeyToken(it.name());
	ok = ok && w.jsonStringToken(value);
    }
    return ok && w.jsonEndMap();
}

bool
GABC_NameMap::load(GABC_NameMapPtr &result, UT_JSONParser &p)
{
    GABC_NameMap	*map = new GABC_NameMap();
    bool		 ok = true;
    UT_WorkBuffer	 key, value;
    for (UT_JSONParser::iterator it = p.beginMap(); ok && !it.atEnd(); ++it)
    {
	ok = ok && it.getKey(key);
	ok = ok && p.parseString(value);
	map->addMap(key.buffer(), value.buffer());
    }
    if (!ok || map->entries() == 0)
	delete map;
    else
	result = map;
    return ok;
}
