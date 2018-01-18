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

#include "ROP_AbcUserProperties.h"

void
ROP_AbcUserProperties::clear()
{
    for(auto it = myProps.begin(); it != myProps.end(); ++it)
	delete it->second;
    myProps.clear();
    myIsValid = false;
}

void
ROP_AbcUserProperties::update(
    OCompoundProperty &props,
    const UT_StringHolder &vals,
    const UT_StringHolder &meta,
    const ROP_AbcArchivePtr &archive)
{
    if(!myIsValid)
    {
	exportData(&props, vals, meta, archive);
	myIsValid = true;
    }

    exportData(nullptr, vals, meta, archive);
}

void
ROP_AbcUserProperties::exportData(
    OCompoundProperty *props,
    const UT_StringHolder &vals,
    const UT_StringHolder &meta,
    const ROP_AbcArchivePtr &archive)
{
    UT_AutoJSONParser vals_data(vals.buffer(), vals.length());
    UT_AutoJSONParser meta_data(meta.buffer(), meta.length());
    vals_data->setBinary(false);
    meta_data->setBinary(false);

    GABC_Util::exportUserPropertyDictionary(meta_data, vals_data, myProps,
		props, archive->getOError(), archive->getOOptions());
}
