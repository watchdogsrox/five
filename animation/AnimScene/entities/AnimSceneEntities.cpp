#include "AnimSceneEntities.h"

#include "AnimSceneEntities_parser.h"

// rage includes
#include "cutfile\cutfevent.h" // required for AnimatedModelEntity below
#include "grcore/debugdraw.h"

// framework includes
#include "fwdebug/picker.h"

// game includes
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneEntityIterator.h"
#include "animation/AnimScene/entities/AnimSceneEntityInitialisers.h"
#include "cutscene/AnimatedModelEntity.h" // required for weapon helper functions
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#if __BANK
#include "Peds/pedpopulation.h"
#endif
#include "Peds/PlayerInfo.h"
#include "Objects/object.h"
#include "Objects/objectpopulation.h"
#include "streaming/streaming.h"
#include "vehicles/vehicle.h"
#include "vehicles/VehicleFactory.h"

ANIM_OPTIMISATIONS()

#if __BANK
//////////////////////////////////////////////////////////////////////////
// CAnimScenePed

void CAnimScenePed::Shutdown(CAnimScene* UNUSED_PARAM(pScene))
{
	if (m_widgets->m_usingDebugModel && m_pPed)
	{
		m_pPed->FlagToDestroyWhenNextProcessed();
	}
}

void CAnimScenePed::InitForEditor(CAnimSceneEntityInitialiser* pInitialiser)
{
	CAnimScenePedInitialiser* pInit = CAnimSceneEntityInitialiser::Cast<CAnimScenePedInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimScene* pScene = pInit->m_pEditor->GetScene();
		// create and register the ped
		SetDebugPed(pInit->m_pedSelector.CreatePed(pScene->GetSceneOrigin(), POPTYPE_TOOL, false));

		pScene->SceneEditorAddEntity(GetId(), this);

		if (pInit->m_animEventInitialiser.m_clipSelector.HasSelection())
		{
			pInit->m_animEventInitialiser.m_pEntity = this;
			pInit->m_pEditor->CreateEventAndAddToCurrentScene("CAnimScenePlayAnimEvent", &pInit->m_animEventInitialiser, pScene->FindSectionForTime(pInit->m_pEditor->GetCurrentTime()));
		}
	}
}

void CAnimScenePed::SetDebugPed(CPed* pPed)
{
	if (pPed)
	{	
		m_debugPedName = pPed->GetModelName();
		SetPed(pPed);
		m_widgets->m_usingDebugModel = true;
	}
}

void CAnimScenePed::SetLeadInClipSet(atHashString playbackList, fwMvClipSetId clipSet)
{
	CAnimSceneLeadInData* pData= FindLeadIn(playbackList.GetHash(), false);

	if (pData)
	{
		if (clipSet.GetHash()==CLIP_SET_ID_INVALID)
		{
			// remove the lead in data
			RemoveLeadIn(playbackList.GetHash());
			pData = NULL;
		}
		else
		{
			pData->m_clipSet.Set(clipSet);
		}
	}
	else
	{
		if (clipSet.GetHash()!=CLIP_SET_ID_INVALID)
		{
			CAnimSceneLeadInData& data = m_leadIns.Grow();
			data.m_clipSet.Set(clipSet);
			data.m_playbackList = playbackList;
		}
	}
}
#endif //__BANK

void CAnimScenePed::SetPed(CPed* pPed)
{ 
#if __BANK
	if (m_widgets->m_usingDebugModel)
	{
		if (m_pPed)
			m_pPed->FlagToDestroyWhenNextProcessed();
		m_widgets->m_usingDebugModel = false;
	}
	if (pPed)
	{
		m_widgets->m_modelSelector.Select(pPed->GetModelName());
	}
	animDebugf2("%s CAnimScenePed::SetPed - %s - %s(%p)", CAnimSceneManager::GetSceneDescription(GetScene(), 0).c_str(), m_Id.GetCStr(), pPed ? pPed->GetModelName() : "null", pPed);
#endif //__BANK
	m_pPed = pPed;
}


#if __BANK

// add the ped selector widgets so we can pick the ped to use
void CAnimScenePed::OnPostAddWidgets(bkBank& bank)
{
	bank.AddToggle("Optional entity", &m_optional, NullCB, "This entity is optional in the scene, and will be silently ignored if not registered by the user");
	bank.AddButton("Register selected ped", datCallback(MFA(CAnimScenePed::RegisterSelectedPed), (datBase*)this));
	bank.AddButton("Unregister ped", datCallback(MFA(CAnimScenePed::UnregisterPed), (datBase*)this));
	m_widgets->m_modelSelector.AddWidgets(&bank, "Pick debug ped", datCallback(MFA(CAnimScenePed::OnModelSelected), (datBase*)this));
	if (m_pPed)
	{
		m_widgets->m_modelSelector.Select(m_pPed->GetModelName());
	}
	else if (m_debugPedName.GetHash()!=0)
	{
		m_widgets->m_modelSelector.Select(m_debugPedName.GetCStr());
	}
	m_widgets->m_modelSelector.SetRegisteredEntity(NULL);
}

void CAnimScenePed::RegisterSelectedPed()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt && pEnt->GetIsTypePed())
	{
		SetPed(static_cast<CPed*>(pEnt));
		if(g_AnimSceneManager)
		{
			g_AnimSceneManager->OnGameEntityRegister(); 
		}
	}
	else if (FindPlayerPed())
	{
		SetPed(FindPlayerPed());
		if(g_AnimSceneManager)
		{
			g_AnimSceneManager->OnGameEntityRegister(); 
		}
	}
}

void CAnimScenePed::UnregisterPed()
{
	if (GetPed(false))
	{
		if (m_widgets->m_usingDebugModel)
		{
			m_pPed->FlagToDestroyWhenNextProcessed();
			m_widgets->m_usingDebugModel = false;
		}
		SetPed(NULL);
	}
}

void CAnimScenePed::OnModelSelected()
{
	m_debugPedName = m_widgets->m_modelSelector.GetSelectedModelName();
	
	if (m_pPed && strcmp(m_pPed->GetModelName(), m_debugPedName.GetCStr())!=0)
	{
		UnregisterPed();
	}
}
#if __DEV
void CAnimScenePed::DebugRender(CAnimScene* UNUSED_PARAM(pScene), debugPlayback::OnScreenOutput& mainOutput)
{
	if (m_pPed && m_Id.TryGetCStr())
	{
		// render the entity name at the ped centre
		grcDebugDraw::Text(m_pPed->GetMatrix().d(), Color_yellow, 0, 0, m_Id.GetCStr());
	}

	mainOutput.AddLine("%s: %s(%p) (debug model %s)", GetDebugString().c_str(), m_pPed==NULL ? "No ped registered" : m_pPed->GetDebugName(), m_pPed.Get(), m_debugPedName.TryGetCStr() ? m_debugPedName.GetCStr() : "none" );
}
#endif //__DEV

void CAnimScenePed::CreateDebugPedIfRequired()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		/* Can only create entities on the main thread! */
		return;
	}

	if (!IsInEditor())
	{
		/* only ever create peds when in a scene owned by the anim scene editor*/
		return;
	}

	// If the pool is full, and couldn't make any space, bail
	if (CPed::GetPool()->GetNoOfFreeSpaces() <= 0)
	{		
		return;
	}

	//if the ped is null, and there is a debug model selected
	//make the debug ped before passing it back.
	if (!m_pPed && m_debugPedName.IsNotNull())
	{
		m_widgets->m_modelSelector.Select(m_debugPedName.GetCStr());
		fwModelId modelId = m_widgets->m_modelSelector.GetSelectedModelId();

		if (modelId.IsValid())
		{
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
			}

			// Create a new listener using the selected model
			const CControlledByInfo localAiControl(false, false);
			m_pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, NULL, true, true, false, false);

			// Listener should be a tool char
			m_pPed->PopTypeSet(POPTYPE_TOOL);
			m_pPed->SetDefaultDecisionMaker();
			m_pPed->SetCharParamsBasedOnManagerType();
			m_pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

			m_widgets->m_usingDebugModel = true;
		}
	}
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// CAnimVehicle

void CAnimSceneVehicle::SetVehicle(CVehicle* pVeh)
{ 
#if __BANK
	if (m_widgets->m_usingDebugModel)
	{
		if (m_pVeh)
			m_pVeh->FlagToDestroyWhenNextProcessed();
		m_widgets->m_usingDebugModel = false;
	}
	if (pVeh)
	{
		m_widgets->m_modelSelector.Select(pVeh->GetModelName());
	}
	animDebugf2("%s CAnimSceneVehicle::SetVehicle - %s - %s(%p)", CAnimSceneManager::GetSceneDescription(GetScene(), 0).c_str(), m_Id.GetCStr(), pVeh ? pVeh->GetModelName() : "null", pVeh);
#endif //__BANK
	m_pVeh = pVeh;
}

#if __BANK


void CAnimSceneVehicle::Shutdown(CAnimScene* UNUSED_PARAM(pScene))
{
	if (m_widgets->m_usingDebugModel && m_pVeh)
	{
		m_pVeh->FlagToDestroyWhenNextProcessed();
	}
}

void CAnimSceneVehicle::InitForEditor(CAnimSceneEntityInitialiser* pInitialiser)
{
	CAnimSceneVehicleInitialiser* pInit = CAnimSceneEntityInitialiser::Cast<CAnimSceneVehicleInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimScene* pScene = pInit->m_pEditor->GetScene();
		// create and register the vehicle
		SetDebugVehicle(pInit->m_vehicleSelector.CreateVehicle(pScene->GetSceneOrigin(), POPTYPE_TOOL));

		pScene->SceneEditorAddEntity(GetId(), this);

		if (pInit->m_animEventInitialiser.m_clipSelector.HasSelection())
		{
			pInit->m_animEventInitialiser.m_pEntity = this;
			pInit->m_pEditor->CreateEventAndAddToCurrentScene("CAnimScenePlayAnimEvent", &pInit->m_animEventInitialiser, pScene->FindSectionForTime(pInit->m_pEditor->GetCurrentTime()));
		}
	}
}

// add the ped selector widgets so we can pick the ped to use
void CAnimSceneVehicle::OnPostAddWidgets(bkBank& bank)
{
	bank.AddToggle("Optional entity", &m_optional, NullCB, "This entity is optional in the scene, and will be silently ignored if not registered by the user");
	bank.AddButton("Register selected vehicle", datCallback(MFA(CAnimSceneVehicle::RegisterSelectedVehicle), (datBase*)this));
	m_widgets->m_modelSelector.AddWidgets(&bank, "Pick vehicle", datCallback(MFA(CAnimSceneVehicle::OnModelSelected), (datBase*)this));
	if (m_pVeh)
	{
		m_widgets->m_modelSelector.Select(m_pVeh->GetModelName());
	}
	else if (m_debugModelName.GetHash()!=0)
	{
		m_widgets->m_modelSelector.Select(m_debugModelName.GetCStr());
	}
}

void CAnimSceneVehicle::RegisterSelectedVehicle()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt && pEnt->GetIsTypeVehicle())
	{
		SetVehicle(static_cast<CVehicle*>(pEnt));
	}
}

void CAnimSceneVehicle::OnModelSelected()
{

	m_debugModelName = m_widgets->m_modelSelector.GetSelectedObjectName();

	if (m_pVeh && strcmp(m_pVeh->GetModelName(), m_debugModelName.GetCStr())!=0)
	{
		UnregisterVehicle();
	}

	if (m_widgets->m_modelSelector.HasSelection() && m_widgets->m_modelSelector.StreamSelected())
	{
		Matrix34 tempMat;
		tempMat.Identity();
		CVehicle *pVehicle = CVehicleFactory::GetFactory()->Create(m_widgets->m_modelSelector.GetSelectedModelId(), ENTITY_OWNEDBY_DEBUG, POPTYPE_MISSION, &tempMat);
		if( pVehicle)
		{		
			CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);
			pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
			pVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
			SetVehicle(pVehicle);
		}
	}
}
#if __DEV
void CAnimSceneVehicle::DebugRender(CAnimScene* UNUSED_PARAM(pScene), debugPlayback::OnScreenOutput& mainOutput)
{
	if (m_pVeh && m_Id.TryGetCStr())
	{
		// render the entity name at the ped centre
		grcDebugDraw::Text(m_pVeh->GetMatrix().d(), Color_yellow, 0, 0, m_Id.GetCStr());
	}

	mainOutput.AddLine("%s: %s(%p) (debug model %s)", GetDebugString().c_str(), m_pVeh==NULL ? "No vehicle registered" : m_pVeh->GetDebugName(), m_pVeh.Get(), m_debugModelName.TryGetCStr() ? m_debugModelName.GetCStr() : "none" );
}
#endif //__DEV

void CAnimSceneVehicle::CreateDebugVehIfRequired()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		/* Can only create vehicles on the main thread! */
		return;
	}

	if (!IsInEditor())
	{
		/* only ever create entities when in a scene owned by the anim scene editor*/
		return;
	}

	//if the ped is null, and there is a debug model selected
	//make the debug ped before passing it back.
	if (!GetVehicle(false) && m_debugModelName.IsNotNull())
	{
		m_widgets->m_modelSelector.Select(m_debugModelName.GetCStr());
		fwModelId modelId = m_widgets->m_modelSelector.GetSelectedModelId();

		if (modelId.IsValid())
		{
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				CStreaming::LoadAllRequestedObjects(true);
			}

			// Create a new listener using the selected model
			Matrix34 tempMat;
			tempMat.Identity();
			CVehicle *pVehicle = CVehicleFactory::GetFactory()->Create(m_widgets->m_modelSelector.GetSelectedModelId(), ENTITY_OWNEDBY_DEBUG, POPTYPE_TOOL, &tempMat);
			if( pVehicle)
			{		
				CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);
				pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
				pVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
				SetVehicle(pVehicle);
			}

			m_widgets->m_usingDebugModel = true;
		}
	}
}

void CAnimSceneVehicle::UnregisterVehicle()
{
	if (GetVehicle(false))
	{
		if (m_widgets->m_usingDebugModel)
		{
			m_pVeh->FlagToDestroyWhenNextProcessed();
			m_widgets->m_usingDebugModel = false;
		}
		SetVehicle(NULL);
	}
}

void CAnimSceneVehicle::SetDebugVehicle(CVehicle* pVeh)
{
	if (pVeh)
	{	
		m_debugModelName = pVeh->GetModelName();
		SetVehicle(pVeh);
		m_widgets->m_usingDebugModel = true;
	}
}

#endif //__BANK


//////////////////////////////////////////////////////////////////////////
// CAnimSceneObject

void CAnimSceneObject::SetObject(CObject* pObj, bool BANK_ONLY(bSelect /* = true */))
{ 
#if __BANK
	if (m_widgets->m_usingDebugModel)
	{
		if (m_data->m_pObj)
			m_data->m_pObj->FlagToDestroyWhenNextProcessed();
		m_widgets->m_usingDebugModel = false;
	}
	if (pObj && bSelect)
	{
		m_widgets->m_modelSelector.Select(pObj->GetModelName());
	}
	animDebugf2("%s CAnimSceneObject::SetObject - %s - %s(%p)", CAnimSceneManager::GetSceneDescription(GetScene(), 0).c_str(), m_Id.GetCStr(), pObj ? pObj->GetModelName() : "null", pObj);
#endif //__BANK
	m_data->m_pObj = pObj;
}

void CAnimSceneObject::OnStartOfScene()
{
	if (m_data->m_pObj && m_data->m_pObj->GetWeapon())
	{
		// TODO - Commented out as we don't have the FindOwningPed function in this code base
		// Integrate this over.
		// if this is a weapon, and the owner is in the scene
		// make this entity dependent on the owners entity
// 		CPed* pOwningPed = CCutSceneAnimatedWeaponEntity::FindOwningPed(m_data->m_pObj);
// 
// 		if (pOwningPed)
// 		{
// 			AnimSceneEntityTypeFilter filter;
// 			filter.Set(ANIM_SCENE_ENTITY_PED, true);
// 			CAnimSceneEntityIterator it (GetScene(), filter);
// 			it.begin();
// 
// 			while (*it)
// 			{
// 				CAnimScenePed* pPedEnt = static_cast<CAnimScenePed*>(*it);
// 				if (pPedEnt->GetPed(BANK_ONLY(false))==pOwningPed)
// 				{
// 					SetParentEntityId(pPedEnt->GetId());
// 					return;
// 				}
// 				++it;
// 			}
// 		}
	}
}

void CAnimSceneObject::OnOtherEntityRemovedFromSceneEarly(CAnimSceneEntity::Id entityId)
{
	if (entityId==m_data->m_parentEntityId)
	{
		// if our parent has been removed, remove us too
		GetScene()->RemoveEntityFromScene(GetId());
	}
}

#if __BANK

void CAnimSceneObject::Shutdown(CAnimScene* UNUSED_PARAM(pScene))
{
	if (m_widgets->m_usingDebugModel && m_data->m_pObj)
	{
		m_data->m_pObj->FlagToDestroyWhenNextProcessed();
	}
}

void CAnimSceneObject::InitForEditor(CAnimSceneEntityInitialiser* pInitialiser)
{
	CAnimSceneObjectInitialiser* pInit = CAnimSceneEntityInitialiser::Cast<CAnimSceneObjectInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimScene* pScene = pInit->m_pEditor->GetScene();
		// create and register the ped
		SetDebugObject(pInit->m_objectSelector.CreateObject(pScene->GetSceneOrigin()));

		pScene->SceneEditorAddEntity(GetId(), this);

		if (pInit->m_animEventInitialiser.m_clipSelector.HasSelection())
		{
			pInit->m_animEventInitialiser.m_pEntity = this;
			pInit->m_pEditor->CreateEventAndAddToCurrentScene("CAnimScenePlayAnimEvent", &pInit->m_animEventInitialiser, pScene->FindSectionForTime(pInit->m_pEditor->GetCurrentTime()));
		}
	}
}

void CAnimSceneObject::OnPostAddWidgets(bkBank& bank)
{
	bank.AddToggle("Optional entity", &m_optional, NullCB, "This entity is optional in the scene, and will be silently ignored if not registered by the user");
	bank.AddButton("Register selected object", datCallback(MFA(CAnimSceneObject::RegisterSelectedObject), (datBase*)this));
	m_widgets->m_modelSelector.AddWidgets(&bank, "Pick object", datCallback(MFA(CAnimSceneObject::OnModelSelected), (datBase*)this));
	if (m_data->m_pObj)
	{
		m_widgets->m_modelSelector.Select(m_data->m_pObj->GetModelName());
	}
	else if (m_debugModelName.GetHash()!=0)
	{
		m_widgets->m_modelSelector.Select(m_debugModelName.GetCStr());
	}
}

void CAnimSceneObject::RegisterSelectedObject()
{
	CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEnt && pEnt->GetIsTypeObject())
	{
		SetObject(static_cast<CObject*>(pEnt));
	}
}

void CAnimSceneObject::OnModelSelected()
{
	m_debugModelName = m_widgets->m_modelSelector.GetSelectedObjectName();

	if (m_data->m_pObj && strcmp(m_data->m_pObj->GetModelName(), m_debugModelName.GetCStr())!=0)
	{
		UnregisterObject();
	}

	if (m_widgets->m_modelSelector.HasSelection() && m_widgets->m_modelSelector.StreamSelected())
	{
		CObject *pObject = CObjectPopulation::CreateObject(m_widgets->m_modelSelector.GetSelectedModelId(), ENTITY_OWNEDBY_SCRIPT, true, true);

		if (pObject)
		{
			// Add Object to world after its position has been set
			CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
			pObject->GetPortalTracker()->RequestRescanNextUpdate();
			pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

			pObject->SetupMissionState();

			if (pObject->GetDrawable())
			{
				// lazily create a skeleton if one doesn't exist
				if (!pObject->GetSkeleton())
				{
					pObject->CreateSkeleton();
				}

				// Lazily create the animBlender if it doesn't exist
				if (!pObject->GetAnimDirector() && pObject->GetSkeleton())
				{
					pObject->CreateAnimDirector(*pObject->GetDrawable());
				}

				// make sure the object is on the process control list
				pObject->AddToSceneUpdate();
			}

			SetObject(pObject, false);
		}
	}
}
#if __DEV
void CAnimSceneObject::DebugRender(CAnimScene* UNUSED_PARAM(pScene), debugPlayback::OnScreenOutput& mainOutput)
{
	if (m_data->m_pObj && m_Id.TryGetCStr())
	{
		// render the entity name at the ped centre
		grcDebugDraw::Text(m_data->m_pObj->GetMatrix().d(), Color_yellow, 0, 0, m_Id.GetCStr());
	}

	mainOutput.AddLine("%s: %s(%p) (debug model %s)", GetDebugString().c_str(), m_data->m_pObj==NULL ? "No object registered" : m_data->m_pObj->GetDebugName(), m_data->m_pObj.Get(), m_debugModelName.TryGetCStr() ? m_debugModelName.GetCStr() : "none" );
}
#endif //__DEV

void CAnimSceneObject::CreateDebugObjIfRequired()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		/* Can only create objects on the main thread! */
		return;
	}

	if (!IsInEditor())
	{
		/* only ever create entities when in a scene owned by the anim scene editor*/
		return;
	}

	//if the ped is null, and there is a debug model selected
	//make the debug ped before passing it back.
	if (!GetObject(false) && m_debugModelName.IsNotNull())
	{
		m_widgets->m_modelSelector.Select(m_debugModelName.GetCStr());
		
		if (m_widgets->m_modelSelector.GetSelectedIndex()>-1 && m_widgets->m_modelSelector.StreamSelected())
		{
			fwModelId modelId = m_widgets->m_modelSelector.GetSelectedModelId();

			CObject *pObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_DEBUG, true, true);

			if (pObject)
			{
				// Add Object to world after its position has been set
				CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
				pObject->GetPortalTracker()->RequestRescanNextUpdate();
				pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

				pObject->SetupMissionState();

				if (pObject->GetDrawable())
				{
					// lazily create a skeleton if one doesn't exist
					if (!pObject->GetSkeleton())
					{
						pObject->CreateSkeleton();
					}

					// Lazily create the animBlender if it doesn't exist
					if (!pObject->GetAnimDirector() && pObject->GetSkeleton())
					{
						pObject->CreateAnimDirector(*pObject->GetDrawable());
					}

					// make sure the object is on the process control list
					pObject->AddToSceneUpdate();
				}

				SetObject(pObject);
			}

			m_widgets->m_usingDebugModel = true;
		}
	}
}

void CAnimSceneObject::UnregisterObject()
{
	if (GetObject(false))
	{
		if (m_widgets->m_usingDebugModel)
		{
			m_data->m_pObj->FlagToDestroyWhenNextProcessed();
			m_widgets->m_usingDebugModel = false;
		}
		SetObject(NULL);
	}
}

void CAnimSceneObject::SetDebugObject(CObject* pObj)
{
	if (pObj)
	{	
		m_debugModelName = pObj->GetModelName();
		SetObject(pObj);
		m_widgets->m_usingDebugModel = true;
	}
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// CAnimSceneBoolean

#if __BANK

void CAnimSceneBoolean::InitForEditor(CAnimSceneEntityInitialiser* pInitialiser)
{
	CAnimSceneBooleanInitialiser* pInit = CAnimSceneEntityInitialiser::Cast<CAnimSceneBooleanInitialiser>(pInitialiser, this);
	if (pInit)
	{
		SetValue(pInit->m_bDefault);
	}
}

atString CAnimSceneBoolean::GetDebugString()
{
	atString debugString = CAnimSceneEntity::GetDebugString();
	if (GetValue())
	{
		debugString+=" - TRUE";
	}
	else
	{
		debugString+=" - FALSE";
	}
	return debugString;
}

//////////////////////////////////////////////////////////////////////////
// CAnimSceneCamera

void CAnimSceneCamera::InitForEditor(CAnimSceneEntityInitialiser* pInitialiser)
{
	CAnimSceneCameraInitialiser* pInit = CAnimSceneEntityInitialiser::Cast<CAnimSceneCameraInitialiser>(pInitialiser, this);
	if (pInit)
	{
		if (pInit->m_animEventInitialiser.m_clipSelector.HasSelection())
		{
			CAnimScene* pScene = pInit->m_pEditor->GetScene();
			pScene->SceneEditorAddEntity(GetId(), this);
			pInit->m_animEventInitialiser.m_pEntity = this;
			pInit->m_pEditor->CreateEventAndAddToCurrentScene("CAnimScenePlayCameraAnimEvent", &pInit->m_animEventInitialiser, pScene->FindSectionForTime(pInit->m_pEditor->GetCurrentTime()));
		}
	}
}

#endif // __BANK
