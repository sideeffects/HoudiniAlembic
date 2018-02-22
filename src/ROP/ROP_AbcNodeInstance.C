/*
 * Copyright (c) 2018
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

#include "ROP_AbcNodeInstance.h"

OObject
ROP_AbcNodeInstance::getOObject(ROP_AbcArchive &, GABC_OError &)
{
    // XXX: this should never be called
    UT_ASSERT(0);
    return OObject();
}

void
ROP_AbcNodeInstance::purgeObjects()
{
    ROP_AbcNode::purgeObjects();
    myIsValid = false;
}

void
ROP_AbcNodeInstance::update(ROP_AbcArchive &archive,
    const GABC_LayerOptions &layerOptions, GABC_OError &err)
{
    makeValid(archive, layerOptions, err);
    myBox = mySource->getBBox();
}

void
ROP_AbcNodeInstance::makeValid(ROP_AbcArchive &archive,
    const GABC_LayerOptions &layerOptions, GABC_OError &err)
{
    if(myIsValid)
	return;

    mySource->update(archive, layerOptions, err);
    myParent->getOObject(archive, err).addChildInstance(
	mySource->getOObject(archive, err), myName);
    myIsValid = true;
}
