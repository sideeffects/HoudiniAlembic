/*
 * Copyright (c) 2017
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

#ifndef __ROP_AbcUserProperties__
#define __ROP_AbcUserProperties__

#include "ROP_AbcNode.h"

#include <Alembic/Abc/All.h>
#include <GABC/GABC_Util.h>

typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

typedef GABC_NAMESPACE::GABC_Util GABC_Util;
typedef GABC_Util::PropertyMap PropertyMap;

/// utility class to simplify writing user properties
class ROP_AbcUserProperties
{
public:
    ROP_AbcUserProperties() : mySampleCount(0) {}
    ~ROP_AbcUserProperties() { clear(); }

    /// releases all references to user property data
    void clear();
    /// set a new user property sample, creating the user properties if needed
    void update(OCompoundProperty &props,
		const UT_StringHolder &vals, const UT_StringHolder &meta,
		const ROP_AbcArchivePtr &archive);

private:
    void exportData(OCompoundProperty *props,
		    const UT_StringHolder &vals, const UT_StringHolder &meta,
		    const ROP_AbcArchivePtr &archive);

    PropertyMap myProps;
    exint mySampleCount;
};

#endif
