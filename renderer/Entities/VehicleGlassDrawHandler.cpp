/////////////////////////////////////////////////////////////////////////////////
// Title	:	PtFxDrawHandler.cpp
// Author	:	Anoop Thomas
// Started	:	08/08/2014
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/VehicleGlassDrawHandler.h"

// Rage Headers

// Framework Headers

// Game Headers
#include "renderer/DrawLists/drawListMgr.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"

RENDER_OPTIMISATIONS()

dlCmdBase* CVehicleGlassDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* /*pParams*/)
{
	CDrawListPrototypeManager::Flush();

	CVehicleGlassComponentEntity* pVehicleGlassComponentEntity = static_cast<CVehicleGlassComponentEntity*>(pBaseEntity);

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = gDrawListMgr->GetUpdateBucket();

	if( bucket == CRenderer::RB_ALPHA && renderMode == rmStandard )
	{
		DLC( CDrawVehicleGlassComponent, (pVehicleGlassComponentEntity BANK_ONLY(, gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_DEBUG_OVERLAY))) );
	}
	// nothing needs the return value for this type atm
	return NULL;
}
