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
 * NAME:	ROP_AbcObject.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcObject.h"
#include <UT/UT_StackBuffer.h>
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_ParallelUtil.h>
#include "ROP_AbcContext.h"

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "ROP_AbcObject")

namespace
{
typedef Alembic::Abc::OObject		OObject;

static void
extractKids(ROP_AbcObject **array, ROP_AbcObject::ChildContainer &kids)
{
    int		i = 0;
    for (ROP_AbcObject::ChildContainer::const_iterator it = kids.begin();
	    it != kids.end(); ++it, ++i)
    {
	array[i] = it->second;
    }
}

class processTask
{
public:
    processTask(ROP_AbcObject **list, const OObject *parent,
	    GABC_OError &err, const ROP_AbcContext &context)
	: myParent(parent)
	, myList(list)
	, myError(err)
	, myContext(context)
	, myOk(true)
    {
	myBox.makeInvalid();
    }
    processTask(const processTask &t, UT_Split)
	: myParent(t.myParent)
	, myList(t.myList)
	, myError(t.myError)
	, myContext(t.myContext)
	, myOk(t.myOk)
    {
	myBox.makeInvalid();
    }

    bool			 ok() const	{ return myOk; }
    const UT_BoundingBox	&box() const	{ return myBox; }

    void operator	()(const UT_BlockedRange<exint> &range)
    {
	for (exint i = range.begin(); i != range.end() && myOk; ++i)
	{
	    UT_BoundingBox	box;
	    if (myParent)
	    {
		// First frame
		myOk = myList[i]->start(*myParent, myError, myContext, box);
	    }
	    else if (myList[i]->timeDependent())
	    {
		// Save if any kids are time dependent
		myOk = myList[i]->update(myError, myContext, box);
	    }
	    else if (myContext.fullBounds())
	    {
		myOk = myList[i]->getLastBounds(box);
	    }
	    if (myOk && myContext.fullBounds())
		myBox.enlargeBounds(box);
	}
    }

    void	join(const processTask &task)
		{
		    if (ok() && task.ok())
		    {
			if (myContext.fullBounds())
			    myBox.enlargeBounds(task.myBox);
		    }
		    else
			myOk = false;
		}

private:
    const OObject		 *myParent;
    ROP_AbcObject		**myList;
    GABC_OError			 &myError;
    const ROP_AbcContext	 &myContext;
    UT_BoundingBox		  myBox;
    bool			  myOk;
};
}

ROP_AbcObject::ROP_AbcObject()
    : myKids()
    , myName()
    , myTimeDependentKids(false)
{
    UT_INC_COUNTER(theCount)
}

ROP_AbcObject::~ROP_AbcObject()
{
    UT_DEC_COUNTER(theCount)
    deleteChildren();
}

void
ROP_AbcObject::deleteChildren()
{
    for (ChildContainer::const_iterator it = myKids.begin();
	it != myKids.end(); ++it)
    {
	delete it->second;
    }
    myKids = ChildContainer();
}

void
ROP_AbcObject::updateTimeDependentKids()
{
    myTimeDependentKids = false;
    for (ChildContainer::const_iterator it = myKids.begin();
	    it != myKids.end(); ++it)
    {
	if (it->second->timeDependent())
	{
	    myTimeDependentKids = true;
	    break;
	}
    }
}

bool
ROP_AbcObject::addChild(const char *name, ROP_AbcObject *kid, bool rename)
{
    if (myKids.count(name) > 0)
    {
	if (rename)
	{
	    UT_WorkBuffer	storage;
	    const char	*newname;

	    for (int i = 0; i < 10000; ++i)
	    {
		newname = renameChild(name, kid, storage, i);
		if (myKids.count(newname) == 0)
		{
		    kid->setName(newname);
		    myKids[newname] = kid;
		    return true;
		}
	    }
	}
	return false;
    }
    kid->setName(name);
    myKids[name] = kid;
    return true;
}

const char *
ROP_AbcObject::renameChild(const char *base, ROP_AbcObject *kid,
	UT_WorkBuffer &storage, int retries) const
{
    storage.sprintf("%s%d", base, retries);
    return storage.buffer();
}

bool
ROP_AbcObject::startChildren(const OObject &parent,
	GABC_OError &err,
	const ROP_AbcContext &context,
	UT_BoundingBox &kidbox)
{
    UT_StackBuffer<ROP_AbcObject *>	kids(myKids.size());
    extractKids(kids, myKids);
    processTask		task(kids, &parent, err, context);
    //UTparallelReduce(UT_BlockedRange<exint>(0, myKids.size()), task, 2, 8);
    UTserialReduce(UT_BlockedRange<exint>(0, myKids.size()), task);
    kidbox = task.box();
    return task.ok();
}

bool
ROP_AbcObject::updateChildren(GABC_OError &err,
				const ROP_AbcContext &context,
				UT_BoundingBox &kidbox)
{
    UT_StackBuffer<ROP_AbcObject *>	kids(myKids.size());
    extractKids(kids, myKids);
    processTask		task(kids, NULL, err, context);
    //UTparallelReduce(UT_BlockedRange<exint>(0, myKids.size()), task, 2, 8);
    UTserialReduce(UT_BlockedRange<exint>(0, myKids.size()), task);
    kidbox = task.box();
    return task.ok();
}
