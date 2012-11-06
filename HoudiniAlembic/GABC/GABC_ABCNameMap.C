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

#include "GABC_ABCNameMap.h"
#include "GABC_Util.h"
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

GEO_ABCNameMap::GEO_ABCNameMap()
    : myMap()
    , myRefCount(0)
{
}

GEO_ABCNameMap::~GEO_ABCNameMap()
{
}

bool
GEO_ABCNameMap::isEqual(const GEO_ABCNameMap &src) const
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
GEO_ABCNameMap::getName(const char *name) const
{
    UT_String	entry;
    if (myMap.findSymbol(name, &entry))
    {
	return (const char *)entry;
    }
    return name;
}

void
GEO_ABCNameMap::addMap(const char *abcName, const char *houdiniName)
{
    UT_String	&entry = myMap[abcName];
    if (UTisstring(houdiniName))
	entry.harden(houdiniName);	// Replace existing entry
    else
	entry = NULL;
}

bool
GEO_ABCNameMap::save(UT_JSONWriter &w) const
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
GEO_ABCNameMap::load(GEO_ABCNameMapPtr &result, UT_JSONParser &p)
{
    GEO_ABCNameMap	*map = new GEO_ABCNameMap();
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
