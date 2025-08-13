#include "AnimSceneEvents.h"

#include "AnimSceneEvents_parser.h"

// game includes
#include "animation/AnimScene/AnimSceneEntityTypes.h"
#include "animation/AnimScene/entities/AnimSceneEntities.h"

#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "script/script.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vehicles/VehicleFactory.h"

//////////////////////////////////////////////////////////////////////////
// CAnimSceneForceMotionStateEvent - force the motion state on a ped. Used for walking blend outs, etc

void CAnimSceneForceMotionStateEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresEnter );

	// Initialise Helpers
	m_ped.OnInit(pScene);
}
void CAnimSceneForceMotionStateEvent::OnShutdown(CAnimScene* UNUSED_PARAM(pScene))
{
	// any cleanup needed here?
}

void CAnimSceneForceMotionStateEvent::OnEnter(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAnim)
	{
		// get the entity
		CAnimScenePed* pScenePed = m_ped.GetEntity<CAnimScenePed>(ANIM_SCENE_ENTITY_PED);

		if (pScenePed && pScenePed->GetPed())
		{
			// force the motion state
			pScenePed->GetPed()->ForceMotionStateThisFrame(m_motionState, m_restartState);
		}
	}
}

#if __BANK

EXTERN_PARSER_ENUM(CPedMotionStates__eMotionState);

extern const char* parser_CPedMotionStates__eMotionState_Strings[];
extern ::rage::parEnumListEntry parser_CPedMotionStates__eMotionState_Values[];

//////////////////////////////////////////////////////////////////////////

void CAnimSceneForceMotionStateEvent::OnPostAddWidgets(bkBank& bank)
{
	// set the selection value correctly
	for (s32 i=0; i<CPedMotionStates::eMotionState_NUM_ENUMS; i++)
	{
		if (parser_CPedMotionStates__eMotionState_Values[i].m_Value==m_motionState)
		{
			m_widgets->m_selection=i;
			break;
		}
	}

	bank.AddCombo("Motion state:", &m_widgets->m_selection, CPedMotionStates::eMotionState_NUM_ENUMS, &parser_CPedMotionStates__eMotionState_Strings[0], datCallback(MFA(CAnimSceneForceMotionStateEvent::OnWidgetChanged),this));
}
//////////////////////////////////////////////////////////////////////////
void CAnimSceneForceMotionStateEvent::OnWidgetChanged()
{
	m_motionState = (CPedMotionStates::eMotionState)parser_CPedMotionStates__eMotionState_Values[m_widgets->m_selection].m_Value;
}

bool CAnimSceneForceMotionStateEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
 	CAnimSceneEvent::InitForEditor(pInitialiser);
 	m_ped.SetEntity(pInitialiser->m_pEntity);
	return true;
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// CAnimSceneCreatePedEvent

void CAnimSceneCreatePedEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresEnter | kRequiresExit );
	m_data->m_modelId = CModelInfo::GetModelIdFromName(strStreamingObjectName(m_modelName.GetCStr()));

	// Initialise Helpers
	m_entity.OnInit(pScene);
}

bool CAnimSceneCreatePedEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	// only request the model if one has not already been assigned
	CAnimScenePed* pPedEnt = m_entity.GetEntity<CAnimScenePed>(ANIM_SCENE_ENTITY_PED);
	if (m_data->m_modelId.IsValid() && pPedEnt && pPedEnt->GetPed(BANK_ONLY(false))==NULL)
	{
		return m_data->m_modelRequest.Request(strLocalIndex(m_data->m_modelId.GetModelIndex()), CModelInfo::GetStreamingModule()->GetStreamingModuleId());
	}
	else
	{
		return true;
	}
}
void CAnimSceneCreatePedEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	// create the ped and assign to the entity
	if (phase==kAnimSceneUpdatePreAnim)
	//if (phase==kAnimSceneUpdatePostMovement)
	{
		CAnimScenePed* pPedEnt = m_entity.GetEntity<CAnimScenePed>(ANIM_SCENE_ENTITY_PED);
		if (m_data->m_modelId.IsValid() && pPedEnt && pPedEnt->GetPed(BANK_ONLY(false))==NULL)
		{
			const CControlledByInfo localAiControl(false, false);
			Matrix34 mat = MAT34V_TO_MATRIX34(pScene->GetSceneOrigin());
			m_data->m_pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, m_data->m_modelId, &mat, true, false, false, false);
			pPedEnt->SetPed(m_data->m_pPed);
		}
	}
}
void CAnimSceneCreatePedEvent::OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAnim || phase==kAnimSceneShuttingDown)
	{
		if (m_data->m_pPed)
		{
			//about to delete a script registered object
			CTheScripts::UnregisterEntity(m_data->m_pPed, true);
			m_data->m_pPed->FlagToDestroyWhenNextProcessed();
			m_data->m_pPed->DisableCollision();
			m_data->m_pPed->SetIsVisibleForModule( SETISVISIBLE_MODULE_CUTSCENE, false );
		}
	}
}

#if __BANK

bool CAnimSceneCreatePedEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimSceneCreatePedEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimSceneCreatePedEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
 		CAnimSceneEvent::InitForEditor(pInitialiser);
 		m_entity.SetEntity(pInitialiser->m_pEntity);
 		m_modelName = pInit->m_pedSelector.GetSelectedModelName();
	}
	return true;
}

#endif //__BANK


//////////////////////////////////////////////////////////////////////////
// CAnimSceneCreateObjectEvent

void CAnimSceneCreateObjectEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresEnter | kRequiresExit );
	m_data->m_modelId = CModelInfo::GetModelIdFromName(strStreamingObjectName(m_modelName.GetCStr()));

	// Initialise Helpers
	m_entity.OnInit(pScene);
}

bool CAnimSceneCreateObjectEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	// only request the model if one has not already been assigned
	CAnimSceneObject* pObjEnt = m_entity.GetEntity<CAnimSceneObject>(ANIM_SCENE_ENTITY_OBJECT);
	if (m_data->m_modelId.IsValid() && pObjEnt && pObjEnt->GetObject()==NULL)
	{
		return m_data->m_modelRequest.Request(strLocalIndex(m_data->m_modelId.GetModelIndex()), CModelInfo::GetStreamingModule()->GetStreamingModuleId());
	}
	else
	{
		return true;
	}
}
void CAnimSceneCreateObjectEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	// create the ped and assign to the entity
	if (phase==kAnimSceneUpdatePreAnim)
	//if (phase==kAnimSceneUpdatePostMovement)
	{
		CAnimSceneObject* pObjEnt = m_entity.GetEntity<CAnimSceneObject>(ANIM_SCENE_ENTITY_OBJECT);
		if (m_data->m_modelId.IsValid() && pObjEnt && pObjEnt->GetObject()==NULL)
		{
			const CControlledByInfo localAiControl(false, false);
			Matrix34 mat = MAT34V_TO_MATRIX34(pScene->GetSceneOrigin());
			m_data->m_pObject = CObjectPopulation::CreateObject(m_data->m_modelId, ENTITY_OWNEDBY_CUTSCENE, true, true, false);
			m_data->m_pObject->SetMatrix(mat);
			// Add Object to world after its position has been set
			CGameWorld::Add(m_data->m_pObject, CGameWorld::OUTSIDE );

			m_data->m_pObject->GetPortalTracker()->ScanUntilProbeTrue();
			m_data->m_pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_data->m_pObject->GetTransform().GetPosition()));
			m_data->m_pObject->GetPortalTracker()->SetIsCutsceneControlled(true);

			pObjEnt->SetObject(m_data->m_pObject);
		}
	}
}
void CAnimSceneCreateObjectEvent::OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAnim || phase==kAnimSceneShuttingDown)
	{
		if (m_data->m_pObject)
		{
			//about to delete a script registered object
			CTheScripts::UnregisterEntity(m_data->m_pObject, true);
			m_data->m_pObject->FlagToDestroyWhenNextProcessed();
			m_data->m_pObject->DisableCollision();
			m_data->m_pObject->SetIsVisibleForModule( SETISVISIBLE_MODULE_CUTSCENE, false );
		}
	}
}

#if __BANK

bool CAnimSceneCreateObjectEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimSceneCreateObjectEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimSceneCreateObjectEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimSceneEvent::InitForEditor(pInitialiser);
		m_entity.SetEntity(pInitialiser->m_pEntity);
		m_modelName = pInit->m_objectSelector.GetSelectedObjectName();
	}
	return true;
}

#endif //__BANK


//////////////////////////////////////////////////////////////////////////
// CAnimSceneCreateVehicleEvent

void CAnimSceneCreateVehicleEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresEnter | kRequiresExit );
	m_data->m_modelId = CModelInfo::GetModelIdFromName(strStreamingObjectName(m_modelName.GetCStr()));

	// Initialise Helpers
	m_entity.OnInit(pScene);
}

bool CAnimSceneCreateVehicleEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	// only request the model if one has not already been assigned
	CAnimSceneVehicle* pVehEnt = m_entity.GetEntity<CAnimSceneVehicle>(ANIM_SCENE_ENTITY_VEHICLE);
	if (m_data->m_modelId.IsValid() && pVehEnt && pVehEnt->GetVehicle()==NULL)
	{
		return m_data->m_modelRequest.Request(strLocalIndex(m_data->m_modelId.GetModelIndex()), CModelInfo::GetStreamingModule()->GetStreamingModuleId());
	}
	else
	{
		return true;
	}
}
void CAnimSceneCreateVehicleEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	// create the ped and assign to the entity
	if (phase==kAnimSceneUpdatePreAnim)
	//if (phase==kAnimSceneUpdatePostMovement)
	{
		CAnimSceneVehicle* pVehEnt = m_entity.GetEntity<CAnimSceneVehicle>(ANIM_SCENE_ENTITY_VEHICLE);
		if (m_data->m_modelId.IsValid() && pVehEnt && pVehEnt->GetVehicle()==NULL)
		{
			const CControlledByInfo localAiControl(false, false);
			Matrix34 mat = MAT34V_TO_MATRIX34(pScene->GetSceneOrigin());
			m_data->m_pVehicle = CVehicleFactory::GetFactory()->Create(m_data->m_modelId, ENTITY_OWNEDBY_CUTSCENE, POPTYPE_MISSION, &mat, false);

			CGameWorld::Add(m_data->m_pVehicle, CGameWorld::OUTSIDE);
			m_data->m_pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
			m_data->m_pVehicle->GetPortalTracker()->SetIsCutsceneControlled(true);
			m_data->m_pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
			if (m_data->m_pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				((CTrain*)m_data->m_pVehicle.Get())->m_nTrainFlags.bIsCutsceneControlled = true;
			}

			pVehEnt->SetVehicle(m_data->m_pVehicle);
		}
	}
}
void CAnimSceneCreateVehicleEvent::OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAnim || phase==kAnimSceneShuttingDown)
	{
		if (m_data->m_pVehicle)
		{
			//about to delete a script registered object
			CTheScripts::UnregisterEntity(m_data->m_pVehicle, true);
			m_data->m_pVehicle->FlagToDestroyWhenNextProcessed();
			m_data->m_pVehicle->DisableCollision();
			m_data->m_pVehicle->SetIsVisibleForModule( SETISVISIBLE_MODULE_CUTSCENE, false );
		}
	}
}

#if __BANK

bool CAnimSceneCreateVehicleEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimSceneCreateVehicleEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimSceneCreateVehicleEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimSceneEvent::InitForEditor(pInitialiser);
		m_entity.SetEntity(pInitialiser->m_pEntity);
		m_modelName = pInit->m_vehicleSelector.GetSelectedObjectName();
	}
	return true;
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// CAnimScenePlaySceneEvent

void CAnimScenePlaySceneEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresUpdate | kRequiresExit );
	if (m_bLoopScene)
	{
		bool bEntityFound = m_breakOutTrigger.GetEntity<CAnimSceneBoolean>(ANIM_SCENE_BOOLEAN) != NULL;
		animAssertf(bEntityFound, "No breakout condition found. intenral scene looping will be disabled!");
		if (!bEntityFound)
		{
			m_bLoopScene = false;
		}
	}

	// Initialise Helpers
	m_trigger.OnInit(pScene);
	m_breakOutTrigger.OnInit(pScene);
}

bool CAnimScenePlaySceneEvent::OnUpdatePreload(CAnimScene* pScene)
{
	// create the new scene and start loading it (if necessary)
	return ProcessLoadScene(pScene);
}

void CAnimScenePlaySceneEvent::OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	// If the scene has been started, update it here

	// create the ped and assign to the entity
	if (phase==kAnimSceneUpdatePreAnim)
	{
		if (ProcessLoadScene(pScene))
		{
			if (!m_data->m_bSceneTriggered)
			{
				CAnimSceneBoolean* pTriggerEnt = m_trigger.GetEntity<CAnimSceneBoolean>(ANIM_SCENE_BOOLEAN);
				if (m_data->m_pScene && (!pTriggerEnt || pTriggerEnt->GetValue()==true))
				{
					// Copy any missing entities from the child scene to the master one.
					m_data->m_pScene->CopyEntities(pScene);
					m_data->m_pScene->Start();
					m_data->m_bSceneTriggered = true;
					if (m_bLoopScene)
					{
						pScene->SetInternalLooping(GetStart(), GetEnd());
					}
				}
			}
			else
			{
				if (m_data->m_pScene)
				{
					m_data->m_pScene->SetPaused(pScene->GetPaused());
					m_data->m_pScene->SetRate(pScene->GetRate());
				}

				// check if the breakout condition has been hit
				if (m_bLoopScene)
				{
					CAnimSceneBoolean* pTriggerEnt = m_breakOutTrigger.GetEntity<CAnimSceneBoolean>(ANIM_SCENE_BOOLEAN);
					if ( !pTriggerEnt || pTriggerEnt->GetValue()==true)
					{
						pScene->StopInternalLooping();

						if (m_bBreakOutImmediately)
						{
							pScene->Skip(GetEnd()+SMALL_FLOAT);
						}
					}
				}
			}
		}
	}
}

void CAnimScenePlaySceneEvent::OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi || phase==kAnimSceneShuttingDown)
	{
		if (m_data->m_pScene)
		{
			pScene->DestroyChildScene(m_data->m_pScene);
			m_data->m_pScene = NULL;
			m_data->m_bSceneTriggered = false;
		}
	}
}

bool CAnimScenePlaySceneEvent::ProcessLoadScene(CAnimScene* pScene)
{
	if (m_sceneName.GetHash()!=0)
	{
		// if we haven't created the new scene instance, make it now
		if (!m_data->m_pScene)
		{
			m_data->m_pScene = pScene->CreateChildScene(m_sceneName, NULL, GetStart());

			if (m_data->m_pScene) 
			{
				m_data->m_pScene->BeginLoad();
				return m_data->m_pScene->IsLoaded();
			}
		}
		else
		{
			return m_data->m_pScene->IsLoaded();
		}
	}

	return false;
}

#if __BANK

bool CAnimScenePlaySceneEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimScenePlaySceneEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimScenePlaySceneEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimSceneEvent::InitForEditor(pInitialiser);
		m_sceneName = pInit->m_sceneSelector.GetSelectedName();
		// TODO - get the scene duration and use it to set the event length
		SetEnd(GetStart()+1.0f);
	}
	return true;
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// CAnimSceneInternalLoopEvent

void CAnimSceneInternalLoopEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresEnter | kRequiresUpdate );
	// Initialise Helpers
	m_breakOutTrigger.OnInit(pScene);

	bool bEntityFound = m_breakOutTrigger.GetEntity<CAnimSceneBoolean>(ANIM_SCENE_BOOLEAN) != NULL;
	animAssertf(bEntityFound, "No breakout condition found. intenral scene looping will be disabled!");
	if (!bEntityFound)
	{
		m_bDisable = true;
	}
}

void CAnimSceneInternalLoopEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	// create the ped and assign to the entity
	if (phase==kAnimSceneUpdatePreAnim && !m_bDisable)
	{
		pScene->SetInternalLooping(GetStart(), GetEnd());
	}
}

void CAnimSceneInternalLoopEvent::OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi && !m_bDisable)
	{
		CAnimSceneBoolean* pTriggerEnt = m_breakOutTrigger.GetEntity<CAnimSceneBoolean>(ANIM_SCENE_BOOLEAN);
		if ( !pTriggerEnt || pTriggerEnt->GetValue()==true)
		{
			pScene->StopInternalLooping();

			if (m_bBreakOutImmediately)
			{
				pScene->Skip(GetEnd()+SMALL_FLOAT);
			}
		}
	}
}

#if __BANK

bool CAnimSceneInternalLoopEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimSceneInternalLoopEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimSceneInternalLoopEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
		SetEnd(GetStart()+1.0f);
		CAnimSceneEvent::InitForEditor(pInitialiser);
		m_breakOutTrigger.SetEntity(pInitialiser->m_pEntity);
		m_bBreakOutImmediately = pInit->m_bBreakOutImmediately;
	}
	return true;
}
#endif //__BANK