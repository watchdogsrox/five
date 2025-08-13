//
// renderer/Entities/EntityBatchDrawHandler.cpp : batched entity list draw handler
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _ENTITYBATCHDRAWHANDLER_H_INCLUDED_
#define _ENTITYBATCHDRAWHANDLER_H_INCLUDED_

#include "EntityDrawHandler.h"
#include "scene/EntityBatch_Def.h"

namespace rage {
	class bkBank;
} // namespace rage

class CEntityBatchDrawHandler : public CEntityDrawHandler
{
public:
	CEntityBatchDrawHandler(CEntity *pEntity, rmcDrawable *pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {}

	dlCmdBase *AddToDrawList(fwEntity *pEntity, fwDrawDataAddParams *pParams);

#if __BANK
	static void RenderDebug();
	static void AddWidgets(bkBank &bank);
	static void SetGrassBatchEnabled(bool bEnabled);
#endif
};

class CGrassBatchDrawHandler : public CEntityDrawHandler
{
public:
	CGrassBatchDrawHandler(CEntity *pEntity, rmcDrawable *pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {}

	dlCmdBase *AddToDrawList(fwEntity *pEntity, fwDrawDataAddParams *pParams);

	static void DrawInstancedGrass(rmcDrawable &drawable, GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::GrassCSParams &params, const grcVertexBuffer *vb), u32 bucketMask, int lodIdx = 0, u32 startInstance = 0);
	GRASS_BATCH_CS_CULLING_ONLY(static void DispatchComputeShader(rmcDrawable &drawable, EBStatic::GrassCSParams &params));
	GRASS_BATCH_CS_CULLING_ONLY(WIN32PC_ONLY(static void CopyStructureCount(EBStatic::GrassCSParams &params)));
};

#endif //_ENTITYBATCHDRAWHANDLER_H_INCLUDED_
