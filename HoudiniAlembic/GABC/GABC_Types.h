/*
 * Copyright (c) 2015
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

#ifndef __GABC_Types__
#define __GABC_Types__

#include "GABC_API.h"
#include <UT/UT_IntrusivePtr.h>
#include <GEO/GEO_PackedTypes.h>

namespace GABC_NAMESPACE
{

enum GABC_NodeType
{
    GABC_UNKNOWN=-1,
    GABC_XFORM,
    GABC_POLYMESH,
    GABC_SUBD,
    GABC_CAMERA,
    GABC_FACESET,
    GABC_CURVES,
    GABC_POINTS,
    GABC_NUPATCH,
    GABC_LIGHT,		// Added in Alembic1.1
    GABC_MATERIAL,	// Added in Alembic1.1

    GABC_NUM_NODE_TYPES	// Sentinal
};

class GABC_IArchive;
typedef UT_IntrusivePtr<GABC_IArchive>	GABC_IArchivePtr;

GABC_API extern const char	*GABCnodeType(GABC_NodeType type);
GABC_API extern GABC_NodeType	 GABCnodeType(const char *type);
}

#endif
