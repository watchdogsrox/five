/////////////////////////////////////////////////////////////////////////////////
// Title	:	PtFxDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	23/11/2010
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/PtFxDrawHandler.h"

// Rage Headers

// Framework Headers

// Game Headers
#include "renderer/DrawLists/drawListMgr.h"
#include "vfx/particles/PtFxEntity.h"

RENDER_OPTIMISATIONS()

dlCmdBase* CPtFxDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* /*pParams*/)
{
	CDrawListPrototypeManager::Flush();

	CPtFxSortedEntity* pPtFxEntity = static_cast<CPtFxSortedEntity*>(pBaseEntity);

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = gDrawListMgr->GetUpdateBucket();

	if( bucket == CRenderer::RB_ALPHA && renderMode == rmStandard )
	{
		DLC( CDrawPtxEffectInst, (pPtFxEntity BANK_ONLY(, gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_DEBUG_OVERLAY))) );
	}
	// nothing needs the return value for this type atm
	return NULL;
}
