#if __BANK

#include "AnimSceneEventInitialisers.h"
#include "AnimSceneEventInitialisers_Parser.h"

#include "animation/AnimScene/AnimSceneEditor.h"
#include "animation/animscene/AnimSceneEvent.h"
#include "animation/debug/AnimDebug.h"


#if __ASSERT
const char * CAnimSceneEventInitialiser::GetStructureName(const CAnimSceneEvent* pEvt)
{
	return pEvt->parser_GetStructure()->GetName();
}
#endif //_ASSERT

void CAnimSceneEquipWeaponEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_entitySelector.AddWidgets(*pBank, pEditor->GetScene(),NULL);
}

void CAnimSceneCreatePedEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor*)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_pedSelector.AddWidgets(pBank, "Select ped model");
}

void CAnimSceneCreateObjectEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor*)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_objectSelector.AddWidgets(pBank, "Select object model");
}

void CAnimSceneCreateObjectEventInitialiser::RemoveWidgets(bkGroup* , CAnimSceneEditor*)
{
	m_objectSelector.RemoveWidgets();
}

void CAnimSceneCreateVehicleEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor*)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_vehicleSelector.AddWidgets(pBank, "Select ped model");
}

void CAnimSceneCreateVehicleEventInitialiser::RemoveWidgets(bkGroup* , CAnimSceneEditor*)
{
	m_vehicleSelector.RemoveWidgets();
}

void CAnimScenePlaySceneEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor*)
{
	m_sceneSelector.AddWidgets(CDebugClipSelector::FindBank(pGroup), "Select anim scene");
}

void CAnimScenePlaySceneEventInitialiser::RemoveWidgets(bkGroup* , CAnimSceneEditor*)
{
	m_sceneSelector.RemoveWidgets();
}


void CAnimSceneInternalLoopEventInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor*)
{
	pGroup->AddToggle("Break out immediately", &m_bBreakOutImmediately);
}

#endif //__BANK
