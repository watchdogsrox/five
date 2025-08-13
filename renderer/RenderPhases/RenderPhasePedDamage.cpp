//
// filename:	RenderPhasePedDamage.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhasePedDamage.h"

// RAGE headers
#include "profile/timebars.h"

// Game headers
#include "Peds/rendering/PedDamage.h"   
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/DrawLists/DrawListMgr.h"

#include "Peds/rendering/PedOverlay.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals ----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//--------------------------------------------------------------------


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CRenderPhasePedDamageUpdate::CRenderPhasePedDamageUpdate(CViewport *pViewport)
	: CRenderPhase(pViewport, "Ped Damage", DL_RENDERPHASE_PED_DAMAGE_GEN, 0.0f, true)
{
}


void CRenderPhasePedDamageUpdate::BuildDrawList() 
{ 
	// TODO: Move this out
	DrawListPrologue();

	CPedDamageManager::AddToDrawList(true,false);

	// TODO: Move this out
	DrawListEpilogue();
}

void CRenderPhasePedDamageUpdate::UpdateViewport() 
{
}


void CRenderPhasePedDamageUpdate::BuildRenderList() 
{
	PEDDECORATIONMGR.Update();
	CPedDamageManager::StartRenderList();
}

