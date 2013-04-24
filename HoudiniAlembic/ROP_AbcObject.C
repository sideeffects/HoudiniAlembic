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

#include "ROP_AbcObject.h"
#include <UT/UT_StackBuffer.h>
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_ParallelUtil.h>
#include "ROP_AbcContext.h"

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "ROP_AbcObject")

using namespace GABC_NAMESPACE;

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
    : myParent(NULL)
    , myKids()
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
ROP_AbcObject::checkDuplicateName(const char *name) const
{
    return myKidNames.count(name) > 0;
}

void
ROP_AbcObject::fakeParent(ROP_AbcObject *kid)
{
    std::string	name = kid->getName();
    if (checkDuplicateName(name.c_str()))
    {
	UT_WorkBuffer	storage;
	const char	*newname;

	for (int i = 0; i < 100000; ++i)
	{
	    newname = renameChild(name.c_str(), kid, storage, i);
	    if (!checkDuplicateName(newname))
	    {
		kid->setName(newname);
		myKidNames.insert(newname);
		return;
	    }
	}
	UT_ASSERT(0 && "Renaming child failed");
    }
    myKidNames.insert(name);
}

bool
ROP_AbcObject::addChild(const char *name, ROP_AbcObject *kid, bool rename)
{
    if (checkDuplicateName(name))
    {
	if (rename)
	{
	    UT_WorkBuffer	 storage;
	    const char		*newname;

	    for (int i = 0; i < 100000; ++i)
	    {
		newname = renameChild(name, kid, storage, i);
		if (!checkDuplicateName(newname))
		{
		    kid->setName(newname);
		    myKids[newname] = kid;
		    myKidNames.insert(newname);
		    return true;
		}
	    }
	}
	return false;
    }
    kid->setName(name);
    kid->myParent = this;
    myKids[name] = kid;
    myKidNames.insert(kid->getName());	// Share the std::string data
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
