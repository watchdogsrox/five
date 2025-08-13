/////////////////////////////////////////////////////////////////////////////////
// Title	:	CutSceneDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/CutSceneDrawHandler.h"

// Rage Headers

// Framework Headers

// Game Headers
#include "cutscene/cutscene_channel.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/rendering/PedVariationStream.h"
#include "scene/world/GameWorld.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "shaders\CustomShaderEffectPed.h"

RENDER_OPTIMISATIONS()

dlCmdBase* CCutSceneVehicleDrawHandler::AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( CutSceneVehicleAddToDrawList );

	CDrawListPrototypeManager::Flush();

	dlCmdBase* pDrawCommand = CDynamicEntityDrawHandler::AddToDrawList(pEntity, pParams); //rsTODO: Can I be more specific about which draw function to use? Always the fragment one?
	if(pDrawCommand)
	{
		Assert(pDrawCommand->GetInstructionIdStatic() == DC_DrawFrag);
		((CDrawFragDC*)pDrawCommand)->SetFlag(CDrawFragDC::CAR_PLUS_WHEELS);
	}
	return NULL;
}

