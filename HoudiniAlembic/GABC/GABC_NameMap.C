/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	477 Richmond Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
 *
 * NAME:	GABC_GEOPrim.C ( GEO Library, C++)
 *
 * COMMENTS:	Class for Alembic primitives.
 */

#include "GABC_NameMap.h"
#include "GABC_Util.h"
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

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
