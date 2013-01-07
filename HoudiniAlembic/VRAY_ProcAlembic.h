//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Side Effects Software Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#ifndef __VRAY_ProcAlembic__
#define __VRAY_ProcAlembic__

#include "VRAY_Procedural.h"
#include <UT/UT_Array.h>

class GU_Detail;

class VRAY_ProcAlembic : public VRAY_Procedural
{
public:
    VRAY_ProcAlembic();
    virtual ~VRAY_ProcAlembic();

    static VRAY_Procedural	*create(const char *);
    static VRAY_ProceduralArg	 theArgs[];

    virtual const char	*getClassName();

    virtual int		initialize(const UT_BoundingBox *box);
    virtual void	getBoundingBox(UT_BoundingBox &box);
    virtual void	render();
    virtual bool	isThreadSafe() const	{ return true; }

private:
    const UT_Array<GU_Detail *>	&getDetailList() const
    {
	return myLoadDetails.entries() ? myLoadDetails : myConstDetails;
    }

    UT_Array<GU_Detail *>	 myLoadDetails;
    UT_Array<GU_Detail *>	 myConstDetails;
    fpreal			 myPreBlur, myPostBlur;
    bool			 myNonAlembic;
};

#endif
