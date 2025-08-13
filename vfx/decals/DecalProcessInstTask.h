//
// vfx/decals/DecalProcessInstTask.h
//
// Copyright (C) 2012-2013 Rockstar Games.  All Rights Reserved.
//


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// include task template first
#include "vfx/decal/decalprocessinsttaskbase.h"

// includes (rage)

// includes (framework)

// includes (game)
#include "basetypes.h"  // this is usual brought in via precompiled headers, for spu code, need this first out of all the game headers
#include "vfx/decals/DecalAsyncTaskDesc.h"
#include "vfx/decals/DecalCallbacks.h"


///////////////////////////////////////////////////////////////////////////////
//  INCLUDED CODE
///////////////////////////////////////////////////////////////////////////////

// includes (rage)

// includes (framework)

// includes (game)
#include "vfx/decals/DecalCallbacks.cpp"


///////////////////////////////////////////////////////////////////////////////
//  GLOBALS
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  DecalProcessInstGameInit
///////////////////////////////////////////////////////////////////////////////

void DecalProcessInstGameInit(decalAsyncTaskDescBase *tdBase, const fwEntity *pEntity)
{
	CDecalAsyncTaskDesc *const td = static_cast<CDecalAsyncTaskDesc*>(tdBase);
	CDecalCallbacks::SpuGlobalInit();
	CDecalCallbacks *const callbacks = static_cast<CDecalCallbacks*>(g_pDecalCallbacks);

	callbacks->SpuTaskInit(td, pEntity);

#	if DECAL_MANAGER_ENABLE_PROCESSBOUND || DECAL_MANAGER_ENABLE_PROCESSBREAKABLEGLASS || DECAL_MANAGER_ENABLE_PROCESSDRAWABLESNONSKINNED || DECAL_MANAGER_ENABLE_PROCESSDRAWABLESSKINNED
		callbacks->GlobalInit(td->materialArray);
		decalAssert(td->debugFwArchetype == pEntity->GetArchetype());
#	endif
}
