#if __BANK

#include "AnimSceneEntityInitialisers.h"

#include "AnimSceneEntityInitialisers_parser.h"


void CAnimScenePedInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	//add the ped widgets here:
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_pedSelector.AddWidgets(pBank, "Model");
	m_pedSelector.Select("ig_genstorymale");
	m_animEventInitialiser.AddWidgets(pGroup, pEditor);
}

void CAnimScenePedInitialiser::RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	m_animEventInitialiser.RemoveWidgets(pGroup, pEditor);
}

void CAnimSceneObjectInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_objectSelector.AddWidgets(pBank, "Model");
	m_animEventInitialiser.AddWidgets(pGroup, pEditor);
}

void CAnimSceneObjectInitialiser::RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	m_objectSelector.RemoveWidgets();
	m_animEventInitialiser.RemoveWidgets(pGroup, pEditor);
}

void CAnimSceneVehicleInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);
	m_vehicleSelector.AddWidgets(pBank, "Model");
	m_animEventInitialiser.AddWidgets(pGroup, pEditor);
}

void CAnimSceneVehicleInitialiser::RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	m_vehicleSelector.RemoveWidgets();
	m_animEventInitialiser.RemoveWidgets(pGroup, pEditor);
}

void CAnimSceneBooleanInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* UNUSED_PARAM(pEditor))
{
	pGroup->AddToggle("Default value", &m_bDefault);
}

void CAnimSceneCameraInitialiser::AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	m_animEventInitialiser.AddWidgets(pGroup, pEditor);
}

void CAnimSceneCameraInitialiser::RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor)
{
	m_animEventInitialiser.RemoveWidgets(pGroup, pEditor);
}

#endif // __BANK