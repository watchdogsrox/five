/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AnimatedModelEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutfile/cutfobject.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutsmanager.h"
#include "cutscene/cutseventargs.h"
#include "cranimation/animation.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentfacialrig.h"

//Game Headers
#include "animation/debug/AnimDebug.h"
#include "animation/debug/AnimViewer.h"
#include "animation/EventTags.h"
#include "animation/FacialData.h"
#include "animation/MovePed.h"
#include "animation/MoveObject.h"
#include "camera/CamInterface.h"
#include "camera/Cutscene/CutsceneDirector.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "cutscene/AnimatedModelEntity.h"
#include "Cutscene/cutscene_channel.h"
#include "cutscene/CutSceneAnimManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneDefine.h"
#include "cutscene/CutsceneCustomEvents.h"
#include "event/Events.h"
#include "Network/NetworkInterface.h"
#include "Objects/ObjectIntelligence.h"
#include "Objects/Door.h"
#include "Peds/PedFactory.h"
#include "peds/ped.h"
#include "Peds/PedHelmetComponent.h"
#include "peds/PedIntelligence.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/simulator.h"
#include "physics/Archetype.h"
#include "control/record.h"
#include "scene/entities/compEntity.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/commands_object.h"
#include "script/Handlers/GameScriptEntity.h"
#include "task/motion/TaskMotionBase.h"
#include "task/system/TaskManager.h"
#include "task/System/Task.h"
#include "task/Animation/TaskAnims.h"
#include "task/Animation/TaskCutScene.h"
#include "task/system/TaskTypes.h"
#include "task/general/TaskBasic.h"
#include "vehicleAi/VehicleAILodManager.h"
#include "vehicleAI/VehicleIntelligence.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/train.h"
#include "weapons/inventory/PedInventoryLoadOut.h"

//ANIM_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////////////////////////
// Animated model entity
///////////////////////////////////////////////////////////////////////////////////////////////////

const fwMvPropertyId CCutsceneAnimatedModelEntity::ms_VisibleToScriptKey("VisibleToScript",0xF301E135);
u32 CCutsceneAnimatedModelEntity::ms_EventKey = ATSTRINGHASH("Event", 2915749078);

Mat34V identityMat(V_IDENTITY);
#if RECORDING_VERTS
namespace rage
{
	extern dbgRecords<recordCustomClothEvent> g_DbgRecordCustomClothEvents;

	EXT_PFD_DECLARE_ITEM( DebugRecords );
}
#endif

CCutsceneAnimatedModelEntity::CCutsceneAnimatedModelEntity(const cutfObject* pObject)
:cutsUniqueEntity()
, m_CollisionBound(NULL)
, m_bUpdateCollisionBound(false)
{
	m_pCutfObject = pObject; 
	
	//create a real game side object (CPed, CObject or CVehicle) not a cut scene type object which all derive from CObject
	m_bCreateGameObject = false; 

	if(pObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
	{
		const cutfModelObject* pModel = static_cast<const cutfModelObject*>(pObject); 

		if(pModel)
		{
			m_ModelNameHash = pModel->GetStreamingName(); 
			m_SceneHandleHash = pModel->GetHandle(); 
		}

		if (m_SceneHandleHash.GetHash()==0)
		{
			m_SceneHandleHash = atHashString(atFinalizeHash(pModel->GetAnimStreamingBase()));
		}
	}

	cutsceneAssert(m_ModelNameHash.GetHash()>0);

	//creation position
	m_vCreatePos = CutSceneManager::GetInstance()->GetSceneOffset(); 

	//Pointer to a game side object
	m_pEntity = NULL; 
	
	//Pointer to a registered entity that is just hidden and repositioned by the cut scene 
	m_pEntityForRepositionOnly = NULL; 

	m_pClip = NULL; 

	m_vVelocity = VEC3_ZERO; 

	m_pCutSceneTask = NULL; 

	m_HasBeenDeletedByCutscene = false; 

	m_bWasInvincible = false; 

	m_fOriginalLODMultiplier = 1.0f; 
	m_fOriginalHairScale = 0.0f;

	m_CurrentPosition = VEC3_ZERO;
	m_CurrentHeading = 0.0f;

	m_bComputeInitialVariationsForGameEntity = false;
	m_bComputeVariationsForSkipForRepositionOnlyEntity = false;

	m_bRequestRepositionOnlyEntity = false; 
	
	m_SectionAnimWasRecieved = 0; 

	m_bIsReadyForGame = false;
	m_bStoppedCalled = false;
	m_bIsRegisteredEntityScriptControlled = false;
	m_bCreatedByCutscene = false;
	m_bClearedOrSetAnimThisFrame = false; 
	m_bShouldProcessTags = false; 
	m_FrameTagsAreProcessed = 0; 
	m_OptionFlags.ClearAllFlags();
	m_StreamingOptionFlags.ClearAllFlags();

#if !__NO_OUTPUT
	m_bIsloggingEntityCalls = false; 
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutsceneAnimatedModelEntity::~CCutsceneAnimatedModelEntity()
{
	bool bDeleteRegisteredEntities = CutSceneManager::GetInstancePtr() && CutSceneManager::GetInstance()->ShouldDeleteAllRegisteredEntites();

	DeleteNonRegisteredEntities(); 

	if (m_pEntity && m_pEntity->GetPortalTracker())
	{
		m_pEntity->GetPortalTracker()->SetIsCutsceneControlled(false);
	}

	if (bDeleteRegisteredEntities)
	{
		if (m_pEntity && (!m_pEntity->GetIsTypePed() || !((CPed*)m_pEntity.Get())->IsPlayer()))
			RemoveEntityFromTheWorld(m_pEntity, true);

		if (m_pEntityForRepositionOnly && (!m_pEntityForRepositionOnly->GetIsTypePed() || !((CPed*)m_pEntityForRepositionOnly.Get())->IsPlayer()))
			RemoveEntityFromTheWorld(m_pEntityForRepositionOnly, true);
	}

	m_pEntity = NULL; 
	m_pEntityForRepositionOnly = NULL; 
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
		m_pClip = NULL; 
	}
	m_pCutSceneTask = NULL;
}

void CCutsceneAnimatedModelEntity::DeleteNonRegisteredEntities()
{
	if(RemoveEntityFromTheWorld(m_pEntity))
	{
		m_pEntity = NULL; 
	}
	
	if(RemoveEntityFromTheWorld(m_pEntityForRepositionOnly))
	{
		m_pEntityForRepositionOnly = NULL; 
	}

}


void CCutsceneAnimatedModelEntity::RegisterPlayer(cutsManager* pManager, const cutfObject* pObject)
{
	CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
	
	//Register the player
	if(pCutsManager->GetPlayerSceneId() == pObject->GetObjectId())
	{
		if(m_pEntity == NULL)	
		{
			CPed *pPed = CGameWorld::FindLocalPlayer(); 

			if(pPed)
			{
				if(pObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
				{
					pCutsManager->RegisterGameEntity(pPed, GetSceneHandleHash(), GetModelNameHash(), false, false); 

					m_bWasInvincible = pPed->m_nPhysicalFlags.bNotDamagedByAnything; 
				}
			}
		}
	}
}

void CCutsceneAnimatedModelEntity::SetVisibility(CEntity* pEnt, bool Visible)
{
	if(pEnt)
	{
#if !__NO_OUTPUT
		CPhysical* phys = NULL; 
		bool reactivateLogging = false; 
		if(pEnt->GetIsPhysical())
		{
			phys = static_cast<CPhysical*>(pEnt); 
			if(phys && m_bIsloggingEntityCalls && phys->m_LogVisibiltyCalls)
			{
				phys->m_LogVisibiltyCalls = false;
				reactivateLogging = true; 
			}
#endif //!__NO_OUTPUT			
			pEnt->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, Visible); 

#if !__NO_OUTPUT
			if(phys && m_bIsloggingEntityCalls && reactivateLogging)
			{
				phys->m_LogVisibiltyCalls = true; 
			}
		}
#endif //!__NO_OUTPUT
	}
}

void CCutsceneAnimatedModelEntity::SetUpScriptRegisteredEntity(cutsManager* UNUSED_PARAM(pManager), const cutfObject* pObject)
{
	//going to animate this version of the entity its has a valid scene handle and model hash
	if (IsRegisteredWithScript())
	{
		//the id is stored as it could be used later for creating script required entities
		m_RegisteredEntityFromScript.iSceneId = pObject->GetObjectId(); 
		
		if(IsScriptRegisteredGameEntity())
		{ 			
			if(m_RegisteredEntityFromScript.bAppearInScene == false)
			{
 				//remove our initial entity its not going to be needed
 				if(m_pEntity)
 				{
 					if(m_pEntity->GetIsTypePed())	
 					{
						SetVisibility(m_pEntity, false); 
						cutsceneModelEntityDebugf2("CCutsceneAnimatedModelEntity::SetUpScriptRegisteredEntity1: Hiding entity (%s)", m_pEntity->GetModelName());

 						//have to add to the world to safely delete, though check its not already in the world
 						if( !m_pEntity->GetIsAddedToWorld() )
 						{
 							CGameWorld::Add((CPed*)m_pEntity.Get(), CGameWorld::OUTSIDE);
 						}
 					}

#if !__NO_OUTPUT
					if(m_pEntity->GetIsPhysical() && m_bIsloggingEntityCalls)
					{
						((CPhysical*)m_pEntity.Get())->m_LogDeletions = false; 
					}
#endif
 					m_pEntity->FlagToDestroyWhenNextProcessed(); 
			
					m_pEntity = NULL;
 				} 
			}
			//animating the script given entity, not a CS version
			else if(m_RegisteredEntityFromScript.pEnt )
			{
				//remove our initial entity its not going to be needed
				if(m_pEntity)
				{
					if(m_pEntity->GetIsTypePed())	
					{
						SetVisibility(m_pEntity, false); 
						cutsceneModelEntityDebugf2("CCutsceneAnimatedModelEntity::SetUpScriptRegisteredEntity2: Hiding entity (%s)", m_pEntity->GetModelName());

						//have to add to the world to safely release
						if( !m_pEntity->GetIsAddedToWorld() )
						{
							CGameWorld::Add((CPed*)m_pEntity.Get(), CGameWorld::OUTSIDE);
						}
					}
#if !__NO_OUTPUT
					if(m_pEntity->GetIsPhysical() && m_bIsloggingEntityCalls)
					{
						((CPhysical*)m_pEntity.Get())->m_LogDeletions = false; 
					}
#endif

					m_pEntity->FlagToDestroyWhenNextProcessed(); 
					m_pEntity = NULL;
				}

				
				//check that there is no game entity already
				if(m_pEntity == NULL)	
				{
					m_pEntity = m_RegisteredEntityFromScript.pEnt;
					m_pEntity->GetPortalTracker()->SetIsCutsceneControlled(true);
					if(m_pEntity->GetIsPhysical())
					{
						CPhysical* pPhys = static_cast<CPhysical*>(m_pEntity.Get());				
						m_bWasInvincible = pPhys->m_nPhysicalFlags.bNotDamagedByAnything; 
					}
				}
			}
		}
		else
		{
			//check if its registered by scene handle only
			if(IsScriptRegisteredRepositionOnlyEntity())
			{	
				if(m_RegisteredEntityFromScript.pEnt)
				{
					//assign this reposition only, as this is a ig version of the cutscene				
					m_pEntityForRepositionOnly = m_RegisteredEntityFromScript.pEnt;
					SetRepositionOnlyEntityReadyForCutscene(); 
				}
			}
		}
	}
}

#if !__NO_OUTPUT

void CCutsceneAnimatedModelEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	CutSceneManager::CommonDebugStr(debugStr); // first write in cutscene manager debug

	sprintf(debugStr, "%sModel Entity(%p, %d, %s), G-Ent(%p, %s), RO-Ent(%p, %s)  - ", 
		debugStr, 
		pObject, 
		pObject->GetObjectId(), 
		m_SceneHandleHash.TryGetCStr(),
		GetGameEntity(),
		m_ModelNameHash.TryGetCStr(),
		GetGameRepositionOnlyEntity(),
		m_RegisteredEntityFromScript.ModelNameHash.TryGetCStr());
}

#endif // !__NO_OUTPUT

void CCutsceneAnimatedModelEntity::CreateRepositionOnlyGameEntityWhenModelLoaded(cutsManager* pManager, const cutfObject* pObject, bool bRequestVariations, bool bApplyVariations)
{
	if(m_bRequestRepositionOnlyEntity && m_pEntityForRepositionOnly == NULL)
	{
		CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 

		//set the model index to invalid and then check that we have made a request, if not the model index will remain at zero  
		s32 iModelIndex = strLocalIndex::INVALID_INDEX; 
		pCutsManager->GetAssetManager()->GetModelStreamingRequestId(pObject->GetObjectId(), m_RegisteredEntityFromScript.ModelNameHash, iModelIndex);
		if(iModelIndex != strLocalIndex::INVALID_INDEX)
		{
			if(pCutsManager->GetAssetManager()->HasRequestedModelLoaded(pObject->GetObjectId(), m_RegisteredEntityFromScript.ModelNameHash))	
			{
				cutsceneModelEntityDebugf2("CreateRepositionOnlyGameEntity - Actor - bRequestVariations(%s), bApplyVariations(%s)", bRequestVariations ? "T" : "F",  bApplyVariations ? "T" : "F");

				CreateGameEntity(strLocalIndex(iModelIndex), true, bRequestVariations, bApplyVariations); 
				SetRepositionOnlyEntityReadyForCutscene(); 
				m_RegisteredEntityFromScript.pEnt = m_pEntityForRepositionOnly; 
				pCutsManager->GetAssetManager()->RemoveStreamingRequest(pObject->GetObjectId(), true, MODEL_TYPE, m_RegisteredEntityFromScript.ModelNameHash);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void  CCutsceneAnimatedModelEntity::CreateAnimatedEntityWhenModelLoaded(cutsManager* pManager, const cutfObject* pObject, bool bRequestVariations, bool bApplyVariations)
{
	if(m_RegisteredEntityFromScript.bAppearInScene)
	{
		CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
			
		if (pCutsManager->GetAssetManager()->HasRequestedModelLoaded(pObject->GetObjectId(), m_ModelNameHash))
		{
			cutsceneModelEntityDebugf2("CreateAnimatedEntity - Actor - Model Loaded - bRequestVariations(%s), bApplyVariations(%s)", bRequestVariations ? "T" : "F",  bApplyVariations ? "T" : "F");

			//set the model index to invalid and then check that we have made a request, if not the model index will remain at zero 
			s32 iModelIndex = strLocalIndex::INVALID_INDEX; 
			
			//we are now creating our game entity just before we play an anim on it
			if(pCutsManager->GetAssetManager()->GetModelStreamingRequestId(pObject->GetObjectId(), m_ModelNameHash, iModelIndex))
			{
				m_vCreatePos = pCutsManager->GetSceneOffset(); 
				CreateGameEntity(strLocalIndex(iModelIndex), false, bRequestVariations, bApplyVariations); 		
				
				if(m_RegisteredEntityFromScript.bCreatedForScript)
				{
					if(m_RegisteredEntityFromScript.pEnt == NULL)				
					{
						if(IsScriptRegisteredGameEntity())
						{
							if(GetGameEntity())
							{
								m_RegisteredEntityFromScript.pEnt = m_pEntity;
							}
						}
						else
						{
							if(IsScriptRegisteredRepositionOnlyEntity())
							{
								if(GetGameRepositionOnlyEntity())
								{
									m_RegisteredEntityFromScript.pEnt = m_pEntityForRepositionOnly; 
								}
							}
						}
					}
				}
				m_HasBeenDeletedByCutscene = false; 
			}
		}
	}
}

CTaskCutScene* CCutsceneAnimatedModelEntity::GetCutsceneTaskForEntity()
{
	CTask* pTask = NULL; 

	if(GetGameEntity())
	{
		if(GetGameEntity()->GetIsTypeObject())
		{
			const CObject* pObject = static_cast<CObject*>(GetGameEntity()); 
			if(pObject->GetObjectIntelligence())
			{
				pTask = pObject->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE);
			}
		}
	
		if(GetGameEntity()->GetIsTypeVehicle())
		{
			const CVehicle* pVeh = static_cast<CVehicle*>(GetGameEntity()); 

			if( pVeh->GetIntelligence() &&  pVeh->GetIntelligence()->GetTaskManager())
			{
				aiTask* CurrentVehicleTask = pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetTask(VEHICLE_TASK_SECONDARY_ANIM); 

				if(CurrentVehicleTask && CurrentVehicleTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE) 
				{
					pTask = static_cast<CTask*>(CurrentVehicleTask); 
				}
			}
			
		}
		
		if(GetGameEntity()->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(GetGameEntity());	
			if(pPed->GetPedIntelligence())
			{
				pTask = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE); 
			}
		}

	}

	if(pTask)
	{
		CTaskCutScene* pCutScene = static_cast<CTaskCutScene*>(pTask);
		return pCutScene;
	}

	return NULL; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedModelEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{

	case CUTSCENE_START_OF_SCENE_EVENT:	
		{

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);
			m_ExitSceneTime = pCutSceneManager->GetTotalSeconds();

			//register the player
			if (!NetworkInterface::IsGameInProgress())
				RegisterPlayer(pManager, pObject);
			
			//possibly need to set the object matrix to be the same as the local space
			SetUpScriptRegisteredEntity(pManager, pObject);

			//create any cutscene animated entities that were loaded before the cutscene starts
			CreateAnimatedEntityWhenModelLoaded(pManager, pObject, true, true);

			//create any reposition only entities that were loaded before the cutscene starts
			//CreateRepositionOnlyGameEntityWhenModelLoaded(pManager, pObject, true, true);

			//Set the entity game ready to enter the scene 
			SetGameEntityReadyForCutscene(); 

			if (GetGameEntity() )
			{
				if (pObject)
				{
					cutsceneModelEntityDebugf2("CUTSCENE_START_OF_SCENE_EVENT");
				}

				//have a game side object
				if(GetGameEntity())
				{
					if(m_pClip)
					{
						SetVisibility(GetGameEntity(), true); 
						cutsceneModelEntityDebugf2("CUTSCENE_START_OF_SCENE_EVENT: Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());
					}
					else
					{
						SetVisibility(GetGameEntity(), false); 
						cutsceneModelEntityDebugf2("CUTSCENE_START_OF_SCENE_EVENT: Hiding GetGameEntity (%s)", GetGameEntity()->GetModelName());
					}
				}
			}
		}
		break;

	case CUTSCENE_SKIPPED_EVENT:
		{
			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);
			
			// Is a reposition only entity model needed 
			RequestRepositionOnlyEntityModelWhenNeeded(pCutSceneManager, pObject);

			if( GetGameEntity() && GetGameEntity()->GetIsTypePed() )
			{
				CPed* pPed = static_cast<CPed*>(GetGameEntity());
				if( pPed )
				{
					for( int i = 0; i < PV_MAX_COMP; ++i )
					{
						if( pPed->m_CClothController[i] )
						{
							pPed->m_CClothController[i]->SetFlag( characterClothController::enIsQueuedPinning, true );
						}
					}
				} // end if( pPed )
			}
		}
		break;
	
	case CUTSCENE_UPDATE_EVENT:
		{
			if( m_bUpdateCollisionBound )
			{
				CEntity* pEntity = GetGameEntity();
				if( pEntity )
				{
					crSkeleton* pSkel = pEntity->GetSkeleton();
					if( pSkel )
					{
						Mat34V rootBoneMat;
						pSkel->GetGlobalMtx(0, rootBoneMat);			

						Vec3V col1 = rootBoneMat.GetCol1();
						rootBoneMat.SetCol1( rootBoneMat.GetCol0() );
						rootBoneMat.SetCol0( col1 );

						Assert( m_CollisionBound );
						m_CollisionBound->SetCurrentMatrix(0, rootBoneMat);
					}
				}
			}

			if (m_bIsReadyForGame)
			{
				return;
			}

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);

			// Is a reposition only entity model needed? Check if we are n seconds before the scene ends
			if (pCutSceneManager->GetCutSceneCurrentTime() > (m_ExitSceneTime - DEFAULT_STREAMING_OFFSET))
			{
				RequestRepositionOnlyEntityModelWhenNeeded(pCutSceneManager, pObject);
			}

			// RequestRepositionOnlyEntityModel is called on skip or n seconds before the scene ends
			// Created a reposition only game entity during the cutscene (as opposed to before the cutscene or after it ends)
			CreateRepositionOnlyGameEntityWhenModelLoaded(pManager, pObject, true, true);

			// Has the associated entity (ped/vehicle/prop) been created?
			if(GetGameEntity())
			{
				m_CurrentPosition = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition()); 
				m_CurrentHeading = m_pEntity->GetTransform().GetHeading(); 
				atHashString modelHash(GetModelNameHash()); 

				// the streaming request has been removed we can now delete the object managed by this entity
				if (!pCutSceneManager->GetAssetManager()->IsModelInRequestList(pObject->GetObjectId(), modelHash))
				{
					if(!IsScriptRegisteredGameEntity() || (IsScriptRegisteredGameEntity() && m_RegisteredEntityFromScript.bDeleteBeforeEnd))	
					{	
						if(GetGameEntity()->GetIsTypeVehicle())
						{
							//set it to be ambient so it can be deleted by the population cont
							CVehicle* pVeh = static_cast<CVehicle*>(GetGameEntity());
							pVeh->PopTypeSet(POPTYPE_RANDOM_AMBIENT); 
						}

						if(GetGameEntity()->GetIsTypePed())
						{
							//set it to be ambient so it can be deleted by the population cont
							CPed* pPed = static_cast<CPed*>(GetGameEntity());
							pPed->PopTypeSet(POPTYPE_RANDOM_AMBIENT); 
						}

						cutsceneModelEntityDebugf2("CUTSCENE_UPDATE_EVENT - Deleting entity as its model is no longer in the request list.");

#if !__NO_OUTPUT
						if(GetGameEntity()->GetIsPhysical() && m_bIsloggingEntityCalls)
						{
							((CPhysical*)GetGameEntity())->m_LogDeletions = false; 
						}
#endif
						GetGameEntity()->FlagToDestroyWhenNextProcessed(); 
						GetGameEntity()->DisableCollision();
						SetVisibility(GetGameEntity(), false); 
						cutsceneModelEntityDebugf2("CUTSCENE_UPDATE_EVENT: Show GetGameEntity (%s)", GetGameEntity()->GetModelName());
						m_pEntity = NULL; 
						m_pCutSceneTask = NULL; 

						m_HasBeenDeletedByCutscene = true; 
					}
				}
			}
			else
			{
				//
				// The associated entity (ped/vehicle/prop) has not been created yet?
				//

				// Do script want us to create the associated entity (e.g. the character is dead and therefore not needed in the scene)
				if(m_RegisteredEntityFromScript.bAppearInScene)
				{
					bool bCanCreate = false; 
					
					// If the script has not "registered" the game entity we can create the ped
					if(!IsScriptRegisteredGameEntity())
					{
						bCanCreate = true; 
					}
					else
					{
						// If the script has "registered" the game entity. Script can decide if they want 
						// the cutscene to create the reposition only entity or if they want to create it themselves
						bCanCreate = m_RegisteredEntityFromScript.bCreatedForScript; 
					}
					
					// Check we are not under script control (in the exit state)
					if(!m_bIsRegisteredEntityScriptControlled)
					{
						if(cutsceneVerifyf(m_HasBeenDeletedByCutscene || bCanCreate, "The registered entity %s owned by the cutscene has been deleted whilst the scene active", pObject->GetDisplayName().c_str()))
						{
							// Created an animated actor during the cutscene (as opposed to before the cutscene or after it ends)
							CreateAnimatedEntityWhenModelLoaded( pManager, pObject, true, true);
							SetGameEntityReadyForCutscene(); 

							//have a game side object
							if(GetGameEntity())
							{
								if(m_pClip)
								{
									PlayClip(pCutSceneManager, m_pClip, pManager->GetSectionStartTime(pManager->GetCurrentSection()), m_AnimDict); 
									ComputeScriptVisibleTagsOnAnimSet(pCutSceneManager); 
									SetVisibility(GetGameEntity(), true); 
									cutsceneModelEntityDebugf2("CUTSCENE_UPDATE_EVENT: Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());
								}
								else
								{
									SetVisibility(GetGameEntity(), false); 
									cutsceneModelEntityDebugf2("CUTSCENE_UPDATE_EVENT: Hiding GetGameEntity (%s)", GetGameEntity()->GetModelName());
								}

								cutsceneModelEntityDebugf2("CUTSCENE_UPDATE_EVENT - Created entity.");
							}
						}
					}
				}
			}

			ComputeScriptVisibleTagsOnUpdate(pCutSceneManager); 
			m_bClearedOrSetAnimThisFrame = false; 
		}
		break; 

	case CUTSCENE_DICTIONARY_LOADED_EVENT:
		{
			
			if (m_ExitSceneTime==pManager->GetTotalSeconds())
			{
				const cutsDictionaryLoadedEventArgs *pDictEventArgs = static_cast<const cutsDictionaryLoadedEventArgs *>( pEventArgs );

				crClipDictionary* pDict = pDictEventArgs->GetDictionary();
				if (pDict && pObject)
				{
					if(pObject->GetAnimStreamingType() == CUTSCENE_NAMED_STREAMED_ANIMATED_OBJECT )
					{
						const cutfNamedAnimatedStreamedObject* pAnimNamedStreamed = static_cast<const cutfNamedAnimatedStreamedObject*>(pObject); 
						
						u32 animBaseHash = pAnimNamedStreamed->GetAnimStreamingBase(); 
						
						s32 iSectionIndex = pDictEventArgs->GetSection(); 

						char indexBuff[8];
						formatf(indexBuff, 8, "-%d", iSectionIndex); 
						atHashString FinalHash = atStringHash(indexBuff, animBaseHash);


						crClip* pClip = pDict->GetClip(FinalHash);
						if (pClip)
						{
							// search for the 
							const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::TagSyncBlendOut); 

							if(pTag)
							{
								CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);
								m_ExitSceneTime = pCutSceneManager->GetSectionStartTime(pDictEventArgs->GetSection()) + pClip->ConvertPhaseToTime(pTag->GetStart()); 
							}
						}
					}
				}
			}

		}
	break;

	case CUTSCENE_SET_CLIP_EVENT:
		{
			if (m_bIsReadyForGame)
			{
				return;
			}

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager); 

			// set the clip and the initial playback time
			if (pEventArgs && pEventArgs->GetType() == CUTSCENE_CLIP_EVENT_ARGS_TYPE )
			{	
				const cutsClipEventArgs *pClipEventArgs = static_cast<const cutsClipEventArgs *>( pEventArgs );
				//store the clip dictionary and anim dict we may not have an object to play an anim on yet
				const crClip* pClip = pClipEventArgs->GetClip(); 
				
				if(pClip != NULL)
				{
					pClip->AddRef(); 
				}

				if(m_pClip != NULL)
				{
					ComputeScriptVisibleTagsOnAnimClear(pCutSceneManager); 
					m_pClip->Release(); 
				}


				m_pClip = pClip; 

				m_SectionAnimWasRecieved = pCutSceneManager->GetCurrentSection(); 

				formatf(m_AnimDict, sizeof(m_AnimDict), pClipEventArgs->GetAnimDict());
				
				cutsceneAssertf(m_pClip, "CUTSCENE_SET_CLIP_EVENT has a null clip on %s, anim dict %s", 
				pObject->GetDisplayName().c_str(), pClipEventArgs->GetAnimDict());
				//Here we have a created our own cut scene object
				CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
				
				if(GetGameEntity())
				{
					PlayClip(pCutManager, pClipEventArgs->GetClip(), pManager->GetSectionStartTime(pManager->GetCurrentSection()), pClipEventArgs->GetAnimDict()); 
					ComputeScriptVisibleTagsOnAnimSet(pCutManager); 
					SetVisibility(GetGameEntity(), true); 
					cutsceneModelEntityDebugf2("SET_CLIP_EVENT: Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());
				}
				
				cutsceneModelEntityDebugf2("SET_CLIP_EVENT %s : pClip:%s", pClipEventArgs->GetAnimDict(), m_pClip ? m_pClip->GetName() : "Clip not found!");
			}
			else
			{
				cutsceneModelEntityDebugf2("NO_SET_CLIP_EVENT");
			}
		}	
		break;

	case CUTSCENE_SCENE_ORIENTATION_CHANGED_EVENT:
		{
			if (m_bIsReadyForGame)
			{
				return;
			}

			CTaskCutScene* pTask = GetCutsceneTaskForEntity(); 
			
			if(pTask)
			{
				Matrix34 SceneMat; 
				pManager->GetSceneOrientationMatrix(SceneMat);
				pTask->SetAnimOrigin(SceneMat); 
			}
		}
		break;

	case CUTSCENE_CLEAR_ANIM_EVENT:
		{
			cutsceneModelEntityDebugf2("CUTSCENE_CLEAR_ANIM_EVENT");
			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);
			bool bDuringCutscene = (pCutSceneManager->GetCutSceneCurrentTime() > pCutSceneManager->GetStartTime()) && (pCutSceneManager->GetCutSceneCurrentTime() < pCutSceneManager->GetEndTime());

			ComputeScriptVisibleTagsOnAnimClear(pCutSceneManager); 

			if(m_pClip != NULL)
			{
				m_pClip->Release(); 
				m_pClip = NULL; 
			}

			if (bDuringCutscene)
			{
				if (m_pCutSceneTask)
				{
					Matrix34 SceneMat; 
					pManager->GetSceneOrientationMatrix(SceneMat);
					m_pCutSceneTask->SetClip(m_pClip,SceneMat, 0.0f); 
				}
			}

			if (m_bIsReadyForGame)
			{
				return;
			}
		
			if(bDuringCutscene)
			{		
				// stop event has been called already and the ped is being returned to the game don't set them to be invisible.
				if(GetGameEntity() && !m_bStoppedCalled)
				{
					SetVisibility(GetGameEntity(), false); 
					cutsceneModelEntityDebugf2("CUTSCENE_CLEAR_ANIM_EVENT: Hiding GetGameEntity (%s)", GetGameEntity()->GetModelName());
				}
			}

		}
		break;

	case CUTSCENE_END_OF_SCENE_EVENT:
		{			
			m_CollisionBound = NULL;
			m_bUpdateCollisionBound = false;

			cutsceneModelEntityDebugf2("CUTSCENE_END_OF_SCENE_EVENT");

			// make sure our entity has been returned to game mode if necessary
			SetGameEntityReadyForGame(pManager);
			DeleteNonRegisteredEntities();

			// Force update the ai for the ped so we don't see the final frame twice
			if (GetGameEntity() && GetCutsceneTaskForEntity())
			{
				cutsceneModelEntityDebugf2("CUTSCENE_END_OF_SCENE_EVENT - Force AI Update");
				ForceUpdateAi(pManager);
			}
		}
		break; 
	
	case CUTSCENE_STOP_EVENT:
		{
			cutsceneModelEntityDebugf3("CUTSCENE_STOP_EVENT");
			m_bStoppedCalled = true; 
		}
		break; 
	case CUTSCENE_RESTART_EVENT:
		{
			m_pCutSceneTask = NULL; 
		}
		break; 

	case CutSceneCustomEvents::CUTSCENE_CUSTOM_CLOTH_EVENT:
		{
			//cutsceneModelEntityDebugf("CCutsceneAnimatedModelEntity::DispatchEvent: CUTSCENE_CUSTOM_CLOTH_EVENT");
			const CutSceneCustomEvents::ICutsceneCustomCustomEventArgs* pObjectVar = static_cast<const CutSceneCustomEvents::ICutsceneCustomCustomEventArgs*>(pEventArgs);
			CEntity* pEntity = GetGameEntity();
			if (	pEntity && pObjectVar 
				&& pObjectVar->GetEventType() == CCEVENT_TYPE_CUSTOM_0 
				)
			{				
				crSkeleton* pSkel = pEntity->GetSkeleton();				
				if( pSkel )
				{
					Mat34V rootBoneMat;
					pSkel->GetGlobalMtx(0, rootBoneMat);

					atHashString ModelNameHash("cs_mrs_thornhill");
					CCutsceneAnimatedModelEntity* pAnimatedModel = ((CutSceneManager*)pManager)->GetAnimatedModelEntityFromModelHash(ModelNameHash);
					CEntity* pEntity = pAnimatedModel->GetGameEntity();
					if( pEntity && pEntity->GetIsTypePed() )
					{
						CPed* pPed = static_cast<CPed*>(pEntity);
						if( pPed )
						{
							for( int i = 0; i < PV_MAX_COMP; ++i )
							{
								if( pPed->m_CClothController[i] )
								{
									phVerletCloth* pVerletCloth = pPed->m_CClothController[i]->GetCloth(0);
									Assert( pVerletCloth );
									if( !pVerletCloth->m_VirtualBound )
									{
										float fCapsuleLen = pObjectVar->GetCapsuleLength();
										float fCapsuleRad = pObjectVar->GetCapsuleRadius();								

										pVerletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
										pVerletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, true);

										pVerletCloth->CreateVirtualBound( 1, &identityMat );
										pVerletCloth->AttachVirtualBoundCapsule( fCapsuleRad, fCapsuleLen, rootBoneMat, 0 );

										m_CollisionBound = (phBoundComposite*)pVerletCloth->m_VirtualBound.ptr;

#if USE_CAPSULE_EXTRA_EXTENTS
										Assert( m_CollisionBound );
										phBound* pColBound = m_CollisionBound->GetBound(0);
										Assert( pColBound );
										static float fCapsuleHalfHeight = 0.15f;
										((phBoundCapsule*)pColBound)->SetHalfHeight( fCapsuleHalfHeight );
#endif
										m_bUpdateCollisionBound = true;
									}
								}
							}
						} // if( pPed )
					}
				} // if( pSkel )

			}
			break;
		}


	case CUTSCENE_SET_ANIMATION_EVENT:	
	case CUTSCENE_PLAY_EVENT:
	case CUTSCENE_PAUSE_EVENT:
	case CUTSCENE_UPDATE_LOADING_EVENT:
	case CUTSCENE_STEP_FORWARD_EVENT:
	case CUTSCENE_STEP_BACKWARD_EVENT:
	case CUTSCENE_FAST_FORWARD_EVENT:
	case CUTSCENE_REWIND_EVENT:

		break;
	}

//this is here to dispatch debug draw event for this entity.
#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif 

}

#if !__NO_OUTPUT
	XPARAM(cutscenecallstacklogging);
#endif //!__NO_OUTPUT

#if !__NO_OUTPUT
void CCutsceneAnimatedModelEntity::SetUpEntityForCallStackLogging()
{
	if(GetGameEntity() && GetGameEntity()->GetIsPhysical())
	{
		if(PARAM_cutscenecallstacklogging.Get() BANK_ONLY (&& (CutSceneManager::GetInstance() &&  CutSceneManager::GetInstance()->IsCallStackLogging())) )
		{
			CPhysical* phys = static_cast<CPhysical*>(GetGameEntity()); 

			phys->m_LogSetMatrixCalls = true; 
			phys->m_LogVisibiltyCalls = true; 
			phys->m_LogDeletions = true; 
			phys->m_LogVariationCalls = true;
			m_bIsloggingEntityCalls = true; 
		}
	}
};
#endif //!__NO_OUTPUT

#if !__NO_OUTPUT
void CCutsceneAnimatedModelEntity::ResetEntityForCallStackLogging()
{
	if(GetGameEntity() && GetGameEntity()->GetIsPhysical())
	{
		if(PARAM_cutscenecallstacklogging.Get() BANK_ONLY (&& (CutSceneManager::GetInstance() &&  CutSceneManager::GetInstance()->IsCallStackLogging())) )
		{
			CPhysical* phys = static_cast<CPhysical*>(GetGameEntity()); 
			phys->m_LogSetMatrixCalls = false; 
			phys->m_LogVisibiltyCalls = false;
			phys->m_LogDeletions = false; 
			phys->m_LogVariationCalls = false;
			m_bIsloggingEntityCalls = false; 
		}
	}
};
#endif //!__NO_OUTPUT

bool CCutsceneAnimatedModelEntity::HasScriptVisibleTagPassed(s32 TagHash)
{
	for(int i=0; i < SIZE_OF_SCRIPT_VISIBLE_TAG_LIST; i++)
	{
		if(m_ScriptVisibleTags[i] != 0)
		{
			if(m_ScriptVisibleTags[i] == TagHash)
			{
				return true; 
			}
		}
	}
	return false; 
}

void CCutsceneAnimatedModelEntity::ResetScriptVisibleTags()
{
	for(int i=0; i < SIZE_OF_SCRIPT_VISIBLE_TAG_LIST; i++)
	{
		m_ScriptVisibleTags[i] = 0; 
	}
}

void CCutsceneAnimatedModelEntity::FindScriptVisibleTags(const crClip* pClip, float startPhase, float endPhase)
{
	if(m_FrameTagsAreProcessed != fwTimer::GetFrameCount())
	{
		ResetScriptVisibleTags(); 
		m_FrameTagsAreProcessed = fwTimer::GetFrameCount(); 
	}
	
	startPhase = Clamp(startPhase, 0.0f, 1.0f);
	endPhase = Clamp(endPhase, 0.0f, 1.0f);

	crTagIterator it(*pClip->GetTags(), startPhase, endPhase, ms_VisibleToScriptKey); 
	int numOfTags =0; 
	while (*it)
	{
		const crTag* pTag = *it;
	
		if(pTag)
		{
			const crPropertyAttribute* pAttr = pTag->GetAttribute(ms_EventKey ); 

			if(pAttr && pAttr->GetType() == crPropertyAttribute::kTypeHashString)
			{
				const crPropertyAttributeHashString *pAttrHashString = static_cast< const crPropertyAttributeHashString * >(pAttr);

				for(int i=0; i < SIZE_OF_SCRIPT_VISIBLE_TAG_LIST; i++)
				{
					if(m_ScriptVisibleTags[i] == 0)
					{
						m_ScriptVisibleTags[i] = pAttrHashString->GetHashString().GetHash(); 
						cutsceneModelEntityDebugf3("Found Script Visible Tag: %s (%d)  in Clip: %s between phases: (s:%f , e:%f)",
							pAttrHashString->GetHashString().TryGetCStr(), pAttrHashString->GetHashString().GetHash(), m_pClip->GetName(), startPhase, endPhase); 
						break; 
					}
				}
			}
		}	
		++it;
		numOfTags++;
	}
	cutsceneAssertf(numOfTags < SIZE_OF_SCRIPT_VISIBLE_TAG_LIST, "Found more than %d script visible tags between phases: %f, %f for clip: %s ", SIZE_OF_SCRIPT_VISIBLE_TAG_LIST, startPhase, endPhase, pClip->GetName()); 
}

void CCutsceneAnimatedModelEntity::ComputeScriptVisibleTagsOnAnimClear(CutSceneManager* pCutSceneManager)
{
	if (m_pClip && m_bShouldProcessTags)
	{
		if(!pCutSceneManager->IsConcatted())
		{
			float startPhase = (pCutSceneManager->GetCutScenePreviousTime() - m_fStartTime) /  m_pClip->GetDuration(); 
			if(startPhase != 1.0f)
			{
				FindScriptVisibleTags(m_pClip, startPhase, 1.0f); 
				cutsceneModelEntityDebugf3("Non Concat - ComputeScriptVisibleTagsOnAnimClear: %s (s:%f , e:%f)", m_pClip->GetName(), startPhase, 1.0f); 
			}
		}
		else
		{
			//compute anim phases based on concat boundaries
			s32 currentConcatSection = pCutSceneManager->GetConcatSectionForTime(pCutSceneManager->GetCutSceneCurrentTime());
			s32 previousConcatSection = pCutSceneManager->GetConcatSectionForTime(pCutSceneManager->GetCutScenePreviousTime()); 

			if(currentConcatSection != previousConcatSection && ((currentConcatSection-previousConcatSection) > 1))
			{
				//compute the anim end time based concat boundary of the next concat section
				float animEndEndTime = pCutSceneManager->GetConcatSectionStartTime(previousConcatSection + 1); 

				float endPhase = (animEndEndTime - m_fStartTime) /  m_pClip->GetDuration(); 

				float startPhase = (pCutSceneManager->GetCutScenePreviousTime() - m_fStartTime) /  m_pClip->GetDuration(); 

				if(endPhase != startPhase)
				{
					FindScriptVisibleTags(m_pClip, startPhase, endPhase); 
					cutsceneModelEntityDebugf3("Concat Boundary - ComputeScriptVisibleTagsOnAnimClear: %s (s:%f , e:%f)", m_pClip->GetName(), startPhase, endPhase); 
				}
			}
			else
			{
				//search for tags to the end of the anim, still in the same concat section
				float startPhase = (pCutSceneManager->GetCutScenePreviousTime() - m_fStartTime) /  m_pClip->GetDuration(); 
				float endPhase = (pCutSceneManager->GetCutSceneCurrentTime() - m_fStartTime) /  m_pClip->GetDuration(); 
				endPhase = Clamp(endPhase, startPhase, 1.0f); 

				if(startPhase != endPhase)
				{
					FindScriptVisibleTags(m_pClip, startPhase, endPhase); 
					cutsceneModelEntityDebugf3("Concat Non-Boundary - ComputeScriptVisibleTagsOnAnimClear: %s (s:%f , e:%f)", m_pClip->GetName(), startPhase, endPhase); 
				}
			}
		}
		m_bClearedOrSetAnimThisFrame = true; 
	}
}

void CCutsceneAnimatedModelEntity::ComputeScriptVisibleTagsOnUpdate(CutSceneManager* pCutSceneManager)
{
	if (m_pClip && !m_bClearedOrSetAnimThisFrame && m_bShouldProcessTags)
	{
		float startPhase = (pCutSceneManager->GetCutScenePreviousTime() - m_fStartTime) /  m_pClip->GetDuration(); 

		float endPhase = (pCutSceneManager->GetCutSceneCurrentTime() - m_fStartTime) / m_pClip->GetDuration();

		if(endPhase != startPhase)
		{
			FindScriptVisibleTags(m_pClip, startPhase, endPhase); 
		}
	}
}

void CCutsceneAnimatedModelEntity::ComputeScriptVisibleTagsOnAnimSet(CutSceneManager* pCutSceneManager)
{
	if (m_pClip)
	{
		m_bShouldProcessTags = false; 

		const crTag* pTag = CClipEventTags::FindFirstEventTag(m_pClip, ms_VisibleToScriptKey); 

		if(pTag)
		{
			m_bShouldProcessTags = true; 
		}
		
		if(m_bShouldProcessTags)
		{
			if(!pCutSceneManager->IsConcatted())
			{
				//get the last anim phase 
				//set end phase = 1.0f we are about to get a new anim
				float endPhase = (pCutSceneManager->GetCutSceneCurrentTime() - m_fStartTime) /  m_pClip->GetDuration(); 
				if(endPhase != 0.0f)
				{
					FindScriptVisibleTags(m_pClip, 0.0f, endPhase); 
					cutsceneModelEntityDebugf3("Non Concat - ComputeScriptVisibleTagsOnAnimSet: %s (s:%f , e:%f)", m_pClip->GetName(), 0.0f, endPhase ); 
				}
			}
			else
			{
				float previousTime = pCutSceneManager->GetCutScenePreviousTime(); 

				if(pCutSceneManager->GetInternalTime() == pCutSceneManager->GetStartTime())
				{
					if(!pCutSceneManager->IsConcatSectionValidForPlayBack(0))
					{
						previousTime = -1.0f; 
					}
				}

				//Crossed a concat boundary compute the tag from the start of concat boundary to the current time
				s32 currentConcatSection = pCutSceneManager->GetConcatSectionForTime(pCutSceneManager->GetCutSceneCurrentTime());
				s32 previousConcatSection = pCutSceneManager->GetConcatSectionForTime(previousTime); 

				if(currentConcatSection != previousConcatSection && ((currentConcatSection-previousConcatSection) > 1))
				{
					//we compute the phase from this point into the anim
					float concatStartTime = pCutSceneManager->GetConcatSectionStartTime(currentConcatSection); 

					s32 currentAnimSection = pCutSceneManager->GetSectionForTime( pCutSceneManager->GetCutSceneCurrentTime() ); 
					float animSectionStartTime = pCutSceneManager->GetSectionStartTime(currentAnimSection); 
					float startPhase = 0.0f; 
					float endPhase = 0.0f; 

					//The start of the concat section is further on in time so need to calculate the phase of the anim relative to the concat section
					//because it's the concat section that defines what to play not the anim section. Other wise the anim time is in a valid part of the 
					//concat so just use a start phase of zero and whatever has progressed.
					//Showing how anim and cocnat sections can vary.
					//e.g anim section: Start:0 |		A1		end: 10 |			A2		end:20|			A3			end:30|
					//	concat section	start:0	|						end 12|			end:18|							end:30|

					if(concatStartTime >= animSectionStartTime)
					{
						startPhase = (concatStartTime - animSectionStartTime) /  m_pClip->GetDuration(); 	
					}

					endPhase = (pCutSceneManager->GetCutSceneCurrentTime() - animSectionStartTime) / m_pClip->GetDuration();

					if(endPhase != startPhase)
					{
						FindScriptVisibleTags(m_pClip, startPhase, endPhase); 
						cutsceneModelEntityDebugf3("Concat Boundary - ComputeScriptVisibleTagsOnAnimSet: %s (s:%f , e:%f)", m_pClip->GetName(), startPhase, endPhase ); 
					}
				}
				else
				{
					float startPhase = (pCutSceneManager->GetCutScenePreviousTime() - m_fStartTime) /  m_pClip->GetDuration(); 
					float endPhase = (pCutSceneManager->GetCutSceneCurrentTime() - m_fStartTime) /  m_pClip->GetDuration(); 

					startPhase = Clamp(startPhase, 0.0f, endPhase); 

					if(endPhase != startPhase)
					{
						FindScriptVisibleTags(m_pClip, startPhase, endPhase); 
					}
					cutsceneModelEntityDebugf3("Concat Non Boundary - ComputeScriptVisibleTagsOnAnimSet: %s (s:%f , e:%f)", m_pClip->GetName(), startPhase, endPhase ); 
				}
			}
		}
		
	
		m_bClearedOrSetAnimThisFrame = true; 
	}
}

//////////////////////////////////////////////////////////////////////////
void CCutsceneAnimatedModelEntity::RequestRepositionOnlyEntityModelWhenNeeded(CutSceneManager* pManager, const cutfObject* pObject)
{
	// check if we want to request a reposition only entity
	if (!IsScriptRegisteredGameEntity()						// check that script have not registered this as a game entity
		&& IsScriptRegisteredRepositionOnlyEntity()			// check that script have registered this as a reposition only entity
		&& !m_RegisteredEntityFromScript.pEnt				// check the entity has not been assigned to script already
		&& m_pEntityForRepositionOnly == NULL				// reposition entity has not been made yet
		&& m_RegisteredEntityFromScript.bCreatedForScript	// entity is to be created by the cutscene for the script
		&& !m_bRequestRepositionOnlyEntity					// request has not been made yet
		)
	{
		cutsceneModelEntityDebugf2("RequestRepositionOnlyEntityModel ------------------------------------------- ");

		// add the streaming request
		pManager->GetAssetManager()->SetModelStreamingRequest(m_RegisteredEntityFromScript.ModelNameHash, pObject->GetObjectId(), MODEL_TYPE);
		m_bRequestRepositionOnlyEntity = true; 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CCutsceneAnimatedModelEntity::SetAnimPlayBackEventTime(float fEventTime)
{
	m_fStartTime = fEventTime; 
}

void CCutsceneAnimatedModelEntity::GetMoverTrackVelocity(const crClip* pClip, float fCurrentPhase, float fLastPhase, float fTimeStep)
{
	if(m_pEntity)
	{
		if(fTimeStep > 0.0f)
		{
			if(pClip)
			{
				const float time					= pClip->ConvertPhaseToTime(fCurrentPhase);
				const float timeOnPreviousUpdate	= pClip->ConvertPhaseToTime(fLastPhase);
				bool bIsWarpFrame					= pClip->CalcBlockPassed(timeOnPreviousUpdate, time);

				if (bIsWarpFrame)
				{
					m_vVelocity.Zero();
				}
				else
				{
					m_vVelocity = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, fLastPhase, fCurrentPhase);
					m_vVelocity /= fTimeStep;
				}
				//printf("CCutsceneAnimatedModelEntity::GetMoverTrackVelocity: %s, m_vVelocity: %.2f, %.2f, %.2f, Phases: %.2f - %.2f\n", pClip->GetName(), m_vVelocity.x, m_vVelocity.y, m_vVelocity.z, fLastPhase, fCurrentPhase);
			}
			else
			{
				m_vVelocity.Zero();
			}

			cutsceneAssertf(rage::FPIsFinite(m_vVelocity.x), "Exit x velocity from cutscene is not finite" ); 
			cutsceneAssertf(rage::FPIsFinite(m_vVelocity.y), "Exit y velocity from cutscene is not finite" ); 
			cutsceneAssertf(rage::FPIsFinite(m_vVelocity.z), "Exit z velocity from cutscene is not finite" ); 

			Matrix34 mEntityMat = MAT34V_TO_MATRIX34(m_pEntity->GetMatrix()); 
			mEntityMat.Transform3x3(m_vVelocity);
		}
	}
}

void CCutsceneAnimatedModelEntity::UpdateCutSceneTaskPhase(CTask* pTask, cutsManager* pManager)
{
	CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
	
	if(ShouldUpdateCutSceneTask(pTask, pManager))
	{
		if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE)
		{
			CTaskCutScene* pCutSceneTask = static_cast<CTaskCutScene*>(pTask); 

			const crClip* pClip = pCutSceneTask->GetClip(); 

			if (pClip)
			{
				float fPhase = pCutsManager->GetPhaseUpdateAmount(pClip, GetAnimEventStartTime()); 

				pCutSceneTask->SetPhase(fPhase);

				if(pCutSceneTask->WillFinishThisPhase(fPhase))
				{
					cutsceneModelEntityDebugf1("CUTSCENE_BLEND_OUT_TAG");
					m_bStoppedCalled = true; 
				}
			}
		}
	}
}

bool CCutsceneAnimatedModelEntity::ShouldUpdateCutSceneTask(CTask* pTask, cutsManager* pManager)
{
	CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
	
	if(m_SectionAnimWasRecieved != pCutsManager->GetCurrentSection())
	{
		if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE)
		{
			CTaskCutScene* pCutSceneTask = static_cast<CTaskCutScene*>(pTask); 

			pCutSceneTask->SetPhase(1.0f); 
		}
		return false; 
	}
	return true; 
}

bool CCutsceneAnimatedModelEntity::RemoveEntityFromTheWorld(CEntity* pEntity, bool bDeleteScriptRegistered)
{
	bool ShouldRemove = false; 
	if (pEntity)
	{		
		bool CanDelete = m_RegisteredEntityFromScript.pEnt != pEntity || bDeleteScriptRegistered; 

		if(CanDelete|| m_RegisteredEntityFromScript.bDeleteBeforeEnd)
		{
			cutsceneModelEntityDebugf2("RemoveEntityFromWorld");
			if(pEntity->GetIsPhysical())
			{
				//about to delete a script registered object
				CTheScripts::UnregisterEntity((CPhysical*)pEntity, true);
			}
#if !__NO_OUTPUT			
			if(pEntity->GetIsPhysical() && m_bIsloggingEntityCalls)
			{
				((CPhysical*)pEntity)->m_LogDeletions = false; 
			}
#endif //!__NO_OUTPUT
			pEntity->FlagToDestroyWhenNextProcessed(); 
			pEntity->DisableCollision();
			SetVisibility(pEntity, false); 
			cutsceneModelEntityDebugf2("CCutsceneAnimatedModelEntity::RemoveEntityFromTheWorld: Hide entity (%s)", pEntity->GetModelName());
			ShouldRemove = true;
		}
	}
	return ShouldRemove; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if __DEV
void CCutsceneAnimatedModelEntity::DebugDraw() const
{
	Vector3 vGameEntityPosition; 
	Vector3 vPos; 
	Color32 Colour (Color_blue); 
	const CEntity* pEnt = NULL; 

	if(GetGameEntity())
	{
		pEnt = GetGameEntity(); 
	}

	if(!pEnt)
	{
		return; 
	}

	if(pEnt)
	{
		if(CutSceneManager::GetInstance()->GetDebugManager().m_bDisplayAllSkeletons)
		{
			const crSkeleton* pSkeleton = pEnt->GetSkeleton(); 
			CAnimViewer::RenderSkeleton(*pSkeleton, Color_red, Color_blue, 0.01f);
			Matrix34 m = MAT34V_TO_MATRIX34(pEnt->GetMatrix());
			grcDebugDraw::Axis(m, 0.5f, true);
		}
		
		vGameEntityPosition = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
	}
	

	//draw the scene handles
	vPos = vGameEntityPosition;
	vPos.z += 1.0f;
	grcDebugDraw::Line(vGameEntityPosition,vPos,Colour);
	char  text [128]; 
	const cutfModelObject* pModel = NULL; 
	
	if(GetCutfObject()->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
	{
		pModel = static_cast<const cutfModelObject*>(GetCutfObject()); 
	}

	if(pModel)
	{
		grcDebugDraw::Text(vPos, Colour, formatf(text,  "Display Name: %s Scene Handle: %s", GetCutfObject()->GetDisplayName().c_str(), pModel->GetHandle().GetCStr()));
	}
	else
	{
		grcDebugDraw::Text(vPos, Colour, formatf(text, "Display Name: %s", GetCutfObject()->GetDisplayName().c_str()));
	}

}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//	Animated Prop Entity 
///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAnimatedPropEntity::CCutSceneAnimatedPropEntity(const cutfObject* pObject)
:CCutsceneAnimatedModelEntity(pObject)
{
}

CCutSceneAnimatedPropEntity::~CCutSceneAnimatedPropEntity()
{
	//check that the script registered entity is being picked up by the script
	if(m_RegisteredEntityFromScript.SceneNameHash > 0 && m_RegisteredEntityFromScript.bCreatedForScript)
	{
		if(m_RegisteredEntityFromScript.pEnt != NULL)
		{
			const CScriptEntityExtension* pExtension = m_RegisteredEntityFromScript.pEnt->GetExtension<CScriptEntityExtension>();

			if(!CutSceneManager::GetInstance()->ShouldDeleteAllRegisteredEntites())
			{
				if(!IsRegisteredGameEntityUnderScriptControl())
				{
					cutsceneAssertf(pExtension, "Deleting a cutscene entity (\"%s\" %u) scene handle (\"%s\" %u) that was registered with CU_CREATE_AND_ANIMATE_NEW_SCRIPT_ENTITY use GET_ENTITY_INDEX_OF_REGISTERED_ENTITY to make sure it is pickup up by the script", 
						m_RegisteredEntityFromScript.ModelNameHash.TryGetCStr(), 
						m_RegisteredEntityFromScript.ModelNameHash.GetHash(), 
						m_RegisteredEntityFromScript.SceneNameHash.TryGetCStr(), 
						m_RegisteredEntityFromScript.SceneNameHash.GetHash());
				}
			}

			if(!pExtension)
			{
				m_RegisteredEntityFromScript.bDeleteBeforeEnd = true; 
				
				if(m_bCreatedByCutscene)
				{
					if(GetGameEntity())
					{
						GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_TEMP);
					}
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//need to implement a game version of this 
void CCutSceneAnimatedPropEntity::CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity, bool UNUSED_PARAM(bRequestVariations), bool UNUSED_PARAM(bApplyVariations))
{	
	if(GetGameEntity() == NULL)
	{
		cutsceneModelEntityDebugf2("CreateGameEntity - Prop");

		fwModelId modelId((strLocalIndex(iModelIndex)));
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		
		if(pModelInfo)
		{
			cutsceneAssertf(pModelInfo->GetIsTypeObject(), "CUTSCENE ASSET (Assign to *Default Anim Cutscene*): Exported incorrectly, %s is not a PropModel type", pModelInfo->GetModelName()); 		
			if(pModelInfo->GetIsTypeObject())
			{
				// By default objects are created at the origin and their drawables are created automatically this flag allows objects to be created without 
				// their drawable they are then moved to the scene origin and then their drawable is created
				// this matters if the drawable contains lights as these lights will only be updated to be relative to the object when the lights bounding box 
				// is on screen and not when the object is moved (FTR I think this is the real bug)
				// We don't want to make this the default behavior at this stage for fear of "fixing" things and causing double lighting etc
				bool bCreateObjectsAtSceneOrigin = CutSceneManager::GetInstance()->GetOptionFlags().IsFlagSet(CUTSCENE_CREATE_OBJECTS_AT_SCENE_ORIGIN);
				CObject* pObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_CUTSCENE, bCreateObjectsAtSceneOrigin? false : true, true, false);

				if (pObject)
				{
#if GTA_REPLAY
					CReplayMgr::RecordObject(pObject);
#endif	//GTA_REPLAY

					Matrix34 TempMat;

					TempMat.Identity();
					TempMat.d = m_vCreatePos; 

					//cant create objects on top of each other, the game complains about it.
					TempMat.d.z += 0.001f * m_pCutfObject->GetObjectId(); 

					pObject->SetMatrix(TempMat);

					if (bCreateObjectsAtSceneOrigin)
					{
						pObject->CreateDrawable();
					}

					// Add Object to world after its position has been set
		
					CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
					
					pObject->GetPortalTracker()->ScanUntilProbeTrue();
					pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));
					pObject->GetPortalTracker()->SetIsCutsceneControlled(true);
					
					if(CreateRepositionEntity)
					{
						SetRepositionOnlyEntity(pObject); 		
					}
					else
					{
						SetGameEntity(pObject);
					}
					m_bCreatedByCutscene = true; 
				}
			}
		}
	}
}

void CCutSceneAnimatedPropEntity::ForceUpdateAi(cutsManager* pManager) 
{
	if(pManager)
	{
		CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);

		if(pCutSceneManager->GetShutDownMode() != SHUTDOWN_SESSION)
		{
			CObject* pObj = GetGameEntity();
			if (pObj && pObj->GetObjectIntelligence())
			{
				pObj->GetObjectIntelligence()->Process();
			}
		}
	}
};


void CCutSceneAnimatedPropEntity::SetRepositionOnlyEntityReadyForCutscene()
{
	if(GetGameRepositionOnlyEntity())
	{
		GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = true;
		GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false); 
		cutsceneModelEntityDebugf2("CCutSceneAnimatedPropEntity::SetRepositionOnlyEntityReadyForCutscene: Hiding GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());

		NetworkInterface::CutsceneStartedOnEntity(*GetGameRepositionOnlyEntity());
	}
}

void CCutSceneAnimatedPropEntity::SetGameEntityReadyForCutscene()
{
	if(GetGameEntity())
	{

#if !__NO_OUTPUT
		SetUpEntityForCallStackLogging(); 
#endif

		GetGameEntity()->SetFixedPhysics(true); 
		GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything = true;
		GetGameEntity()->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
		GetGameEntity()->DisableCollision();

		NetworkInterface::CutsceneStartedOnEntity(*GetGameEntity());
	
		if(!m_RegisteredEntityFromScript.bCreatedForScript)
		{
			if(GetGameEntity()->IsADoor() && m_OptionFlags.IsFlagSet(CEO_UPDATE_AS_REAL_DOOR))
			{
				CDoor* door = static_cast<CDoor*>(GetGameEntity()); 
				door->SetRegisteredWithCutscene(true); 
			}
		}
	}
}

void CCutSceneAnimatedPropEntity::SetGameEntityReadyForGame(cutsManager* UNUSED_PARAM(pManager))
{
	if(!m_bIsReadyForGame)
	{	
		if(GetGameEntity())
		{
			NetworkInterface::CutsceneFinishedOnEntity(*GetGameEntity());

			if(!m_RegisteredEntityFromScript.bCreatedForScript)
			{
				if(GetGameEntity()->IsADoor() && m_OptionFlags.IsFlagSet(CEO_UPDATE_AS_REAL_DOOR))
				{
					CDoor* door = static_cast<CDoor*>(GetGameEntity()); 
					door->SetRegisteredWithCutscene(false); 
				}
			}

#if !__NO_OUTPUT
			ResetEntityForCallStackLogging();
#endif
			if(m_RegisteredEntityFromScript.bCreatedForScript || !m_RegisteredEntityFromScript.bDeleteBeforeEnd)
			{
				cutsceneModelEntityDebugf2("SetGameEntityReadyForGame - Prop");		
				GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
				GetGameEntity()->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
				GetGameEntity()->EnableCollision();
				GetGameEntity()->SetFixedPhysics(false); 
			}
			bool CanDelete = m_RegisteredEntityFromScript.pEnt != GetGameEntity(); 
			if(CanDelete|| m_RegisteredEntityFromScript.bDeleteBeforeEnd)
			{
				GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_TEMP);
			}
			else
			{
				GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
				SetVisibility(GetGameEntity(), true); 
				cutsceneModelEntityDebugf2("CCutSceneAnimatedPropEntity::SetGameEntityReadyForGame: Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());
			}
		}

		if(GetGameRepositionOnlyEntity())
		{
			cutsceneModelEntityDebugf2("SetGameEntityReadyForGame - Prop (reposition only)");		
			GetGameRepositionOnlyEntity()->SetPosition(m_CurrentPosition);
			GetGameRepositionOnlyEntity()->SetHeading(m_CurrentHeading); 
			GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
			GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true); 
			cutsceneModelEntityDebugf2("CCutSceneAnimatedPropEntity::SetGameEntityReadyForGame: Showing GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());

			if(m_bCreatedByCutscene)
			{
				GetGameRepositionOnlyEntity()->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
			}
		}

		if (m_pCutSceneTask)
		{
			m_pCutSceneTask->SetExitNextUpdate();
		}

		m_bIsReadyForGame = true;
	}
}


void CCutSceneAnimatedPropEntity::PlayClip (CutSceneManager* pCutManager, const crClip* pClip, float fEventTime, const strStreamingObjectName UNUSED_PARAM(pAnimDict))
{
	if(GetGameEntity())
	{
		if(!m_pCutSceneTask)
		{
			m_pCutSceneTask = CreateCutsceneTask(pCutManager, pClip, fEventTime); 
			GetGameEntity()->SetTask(m_pCutSceneTask); 
		}
		else
		{
			if(cutsceneVerifyf(m_pCutSceneTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE, "Trying to set the clip on a CTaskCutscene, but entity %s is not running a cutscene task", m_pCutfObject->GetDisplayName().c_str()))
			{
				Matrix34 SceneMat; 
				pCutManager->GetSceneOrientationMatrix(SceneMat);
				m_pCutSceneTask->SetClip(pClip,SceneMat, fEventTime); 
			}
		}
	}

	//store info about the which section the anim started at.
	SetAnimPlayBackEventTime(fEventTime); 
}


void CCutSceneAnimatedPropEntity::UpdateCutSceneTask(cutsManager* pManager)
{
	if(GetGameEntity())
	{
		const CObject* pObject = GetGameEntity();   
		if(pObject && pObject->GetObjectIntelligence())
		{
			CTask* pTask = pObject->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE);
			UpdateCutSceneTaskPhase(pTask, pManager); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////

CTaskCutScene* CCutsceneAnimatedModelEntity::CreateCutsceneTask(CutSceneManager* pManager, const crClip* pClip, float fEventTime)
{
	Matrix34 SceneMat(M34_IDENTITY);

	pManager->GetSceneOrientationMatrix(SceneMat); 

	float fPhase = pManager->GetPhaseUpdateAmount(pClip,fEventTime); 

	CTaskCutScene* pTask = rage_new CTaskCutScene(pClip, SceneMat, fPhase, fEventTime); 

	return pTask; 
}

/////////////////////////////////////////////////////////////////////////

void CCutSceneAnimatedPropEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{
	case CUTSCENE_PAUSE_EVENT:
	case CUTSCENE_UPDATE_EVENT:
		{
			if (iEventId==CUTSCENE_UPDATE_EVENT && m_bStoppedCalled)
			{
				CreateRepositionOnlyGameEntityWhenModelLoaded(pManager, pObject, true, true );
				SetGameEntityReadyForGame(pManager);
			}
			UpdateCutSceneTask(pManager); 
		}
		break;
	}
	CCutsceneAnimatedModelEntity::DispatchEvent( pManager,pObject, iEventId,pEventArgs); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Animated Weapon Entity
///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAnimatedWeaponEntity::CCutSceneAnimatedWeaponEntity(const cutfObject* pObject)
:CCutSceneAnimatedPropEntity(pObject)
{
	m_RegisteredGenericWeaponType = 0; 
}

CCutSceneAnimatedWeaponEntity::~CCutSceneAnimatedWeaponEntity()
{

}

u32 CCutSceneAnimatedWeaponEntity::GetCutObjectGenericWeaponType () const
{
	u32 GenericWeaponType = 0; 

	if(m_pCutfObject && m_pCutfObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
	{	
		const cutfModelObject* pModelObject = static_cast<const cutfModelObject*>(m_pCutfObject);	
	
		if(pModelObject && pModelObject->GetModelType() == CUTSCENE_WEAPON_MODEL_TYPE)
		{
			const cutfWeaponModelObject* pWeapon = static_cast<const cutfWeaponModelObject*>(pModelObject);
			
			if(pWeapon)
			{
				GenericWeaponType = pWeapon->GetGenericWeaponType(); 
			}
		}
	}
	return GenericWeaponType; 
}

bool CCutSceneAnimatedWeaponEntity::IsScriptRegisteredGameEntity()
{
	if(m_RegisteredGenericWeaponType == 0)
	{
		return CCutsceneAnimatedModelEntity::IsScriptRegisteredGameEntity(); 
	}
	else
	{
		if(m_pCutfObject)
		{
			const cutfWeaponModelObject* pWeapon = static_cast<const cutfWeaponModelObject*>(m_pCutfObject);
			if(pWeapon)
			{
				return ((m_RegisteredEntityFromScript.SceneNameHash.GetHash()>0 && (m_RegisteredEntityFromScript.SceneNameHash.GetHash() == GetSceneHandleHash().GetHash())) && pWeapon->GetGenericWeaponType() == m_RegisteredGenericWeaponType); 
			}
		}
	}
	return false; 
}

void CCutSceneAnimatedWeaponEntity::SetGameEntityReadyForCutscene()
{
	// Base
	CCutSceneAnimatedPropEntity::SetGameEntityReadyForCutscene();

	CObject* pGameObject = GetGameEntity();
	if (pGameObject && !pGameObject->m_pWeapon && pGameObject->GetBaseModelInfo() && pGameObject->GetBaseModelInfo()->GetModelType() == MI_TYPE_WEAPON)
	{
		CWeaponModelInfo* pWeaponModelInfo = static_cast<CWeaponModelInfo*>(pGameObject->GetBaseModelInfo());

		// Get the weapon info from the model hash
		const u32 uWeaponModelHash = pWeaponModelInfo->GetModelNameHash();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoFromModelNameHash<CWeaponInfo>(uWeaponModelHash ASSERT_ONLY(, true));
		if(pWeaponInfo)
		{
			CPedEquippedWeapon::SetupAsWeapon(pGameObject, pWeaponInfo, 0);
		}	
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Animated Actor Entity
///////////////////////////////////////////////////////////////////////////////////////////////////
#if __DEV
//THIS CURRENTLY DOES NOTHING TO FILTER ANY ANIMS BECAUSE THERE IS NO FILTER TO REMOVE ALL ANIM EXCEPT FACIAL
void CCutsceneAnimatedActorEntity::UpdateWithFaceViewer(CutSceneManager* pManager, CTaskCutScene* pCutSceneTask)
{
	if(!pCutSceneTask || !pManager)
	{
		return; 
	}

	const CCutSceneCameraEntity* pCam = pManager->GetCamEntity();
	//camera is pointing at ped
	if(pCam && pCam->IsCameraPointingAtThisObject(this))
	{	
		pCutSceneTask->SetCanApplyMoverTrackUpdate(false); 
		Matrix34 mSceneMat(M34_IDENTITY); 
		pManager->GetSceneOrientationMatrix(mSceneMat); 

		//pCutSceneTask->SetFilter(BONEMASK_HEADONLY); //removed because facial anims are applied by extra dofs that arent bones

		//apply the new ped pos
		mSceneMat.d.z+=1000.0f; 
		GetGameEntity()->SetMatrix(mSceneMat);
	}
	else
	{
		//if(pCutSceneTask->GetFilter().GetHash() != BONEMASK_ALL) 
		//{
		//removed because facial anims are applied by extra dofs that arent bones this filter will remove facial anims
		//	pCutSceneTask->SetFilter(BONEMASK_ALL);
			pCutSceneTask->SetCanApplyMoverTrackUpdate(true); 
		//}
	}
};
#endif

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedActorEntity::InitActor(CPed* pPed)
{
	if (pPed)
	{
		pPed->PopTypeSet(POPTYPE_MISSION);
		pPed->SetDefaultDecisionMaker();
		pPed->SetBlockingOfNonTemporaryEvents(true);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, false );
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		pPed->m_nPhysicalFlags.bNotDamagedByAnything		= true;
		//possibly add the rag doll and low lod physics commands here
		pPed->SetCharParamsBasedOnManagerType();
		
		CGameWorld::Add(pPed, CGameWorld::OUTSIDE);
		pPed->SetFixedPhysics(true);
	}
};
/////////////////////////////////////////////////////////////////////////
// store the ped variation for when a ped has to be recreated

void CCutsceneAnimatedActorEntity::StoreActorVariationData(u32 iComponent, s32 iDrawable, s32 iTexture, sActorVariationData& ActorData )
{
	cutsceneAssertf((iDrawable>-1 && iTexture>-1) || (iComponent>=PV_MAX_COMP && iDrawable>-2), "Component, Drawable or Texture is invalid (Component: %d (%s), Drawable: %d, Texture: %d)", iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), iDrawable, iTexture);

	for (int i =0; i < ActorData.iPedVarData.GetCount(); i++)
	{
		if(iComponent == ActorData.iPedVarData[i].iComponent)
		{
			ActorData.iPedVarData[i].iDrawable = iDrawable; 
			ActorData.iPedVarData[i].iTexture = iTexture; 
			return;
		}
	}

	sPedComponentVar& NewPedComponent = ActorData.iPedVarData.Grow(); 

	NewPedComponent.iComponent = iComponent; 
	NewPedComponent.iDrawable = iDrawable; 
	NewPedComponent.iTexture = iTexture; 
}

/////////////////////////////////////////////////////////////////////////
// Pass the latest setup to a newly streamed ped 

void CCutsceneAnimatedActorEntity::RestoreEntityProperties(CEntity * pEnt)
{
	if(pEnt && pEnt->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEnt); 
		for(int i =0; i < m_sCurrentActorVariationData.iPedVarData.GetCount(); i ++)
		{
			SetCurrentActorVariation(pPed, m_sCurrentActorVariationData.iPedVarData[i].iComponent, m_sCurrentActorVariationData.iPedVarData[i].iDrawable, m_sCurrentActorVariationData.iPedVarData[i].iTexture);
		}
	}
}


bool CCutsceneAnimatedActorEntity::RemoveEntityFromTheWorld(CEntity * pEnt, bool bDeleteRegistered)
{
	if(pEnt && pEnt->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEnt); 
		{
			pPed->CleanUpPreloadData(); 
		}
	}
	return CCutsceneAnimatedModelEntity::RemoveEntityFromTheWorld(pEnt, bDeleteRegistered); 
}

/////////////////////////////////////////////////////////////////////////

CCutsceneAnimatedActorEntity::CCutsceneAnimatedActorEntity(const cutfObject* pObject)
:CCutsceneAnimatedModelEntity(pObject)
{
	GetEntityVariationEvents(); 
	m_VariationStreamingIndex = 0;

	m_fVehicleLightingScalar = 0.0f;
	m_fVehicleLightingScalarTargetValue = 0.0f;
	m_fVehicleLightingScalarBlendRate = 1000.0f;
}

/////////////////////////////////////////////////////////////////////////
//create our ingame ped

void CCutsceneAnimatedActorEntity::CreateGamePed(u32 iModelIndex, bool CreateRepositionEntity)
{
	fwModelId modelId((strLocalIndex(iModelIndex)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	
	cutsceneModelEntityDebugf2("CreateGamePed - Actor");

	if(pModelInfo)
	{
		cutsceneAssertf(pModelInfo->GetModelType() == MI_TYPE_PED, "CUTSCENE ASSET (Assign to *Default Anim Cutscene*): Exported incorrectly, %s is not a PedModel type", pModelInfo->GetModelName()); 	
		if(pModelInfo->GetModelType() == MI_TYPE_PED)
		{
			Matrix34 TempMat;

			TempMat.Identity();
			TempMat.d = m_vCreatePos; 

			if(iModelIndex != strLocalIndex::INVALID_INDEX)
			{
				const CControlledByInfo localAiControl(false, false);
				
				CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pModelInfo);

				CPed* pPed = NULL;

				if (CreateRepositionEntity)
				{
					pPed = CPedFactory::GetFactory()->CreatePedFromSource(localAiControl, modelId, &TempMat, GetGameEntity(), true, false, false);
				}
				else
				{				
					if(m_sScriptActorVariationData.iPedVarData.GetCount() > 0)
					{
						// Previously we called CreatePed with bApplyDefaultVariation = true but this still generated random 
						// peds if the ped was non streamed. Now we use bApplyDefaultVariation = false in order minimise streaming 
						// bandwidth on streamed peds. Either way we need to manually set non streamed peds to default after creation
						pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, &TempMat, false, false, false); 
						if (!pPedModelInfo->GetIsStreamedGfx())
						{
							pPed->SetVarDefault();
						}					
					}
					else
					{
						pPed = CPedFactory::GetFactory()->CreatePedFromSource(localAiControl, modelId, &TempMat, GetGameRepositionOnlyEntity(), true, false, false);
					}
				}
				CPedInventoryLoadOutManager::SetDefaultLoadOut(pPed);

				

				if (Verifyf(pPed, "trying to store a null ped pointer something has gone wrong with the registration"))
				{
					if(CreateRepositionEntity)
					{
						SetRepositionOnlyEntity(pPed); 		
					}
					else
					{
						SetGameEntity(pPed);
					}
				}
			}
		}
	}
}

void CCutsceneAnimatedActorEntity::GetEntityVariationEvents()
{
	m_pVariationEvents.Reset(); 

	atArray<cutfEvent *> AllEventList;
	const cutfCutsceneFile2* pCutfile = const_cast<const CutSceneManager*>(CutSceneManager::GetInstance())->GetCutfFile(); 
	pCutfile->FindEventsForObjectIdOnly( m_pCutfObject->GetObjectId(), pCutfile->GetEventList(), AllEventList );
	
	for ( int i = 0; i < AllEventList.GetCount(); ++i )
	{
		if (AllEventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs());
			
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
			{		
				if(CutSceneManager::GetInstance()->IsConcatted())
				{
					// Are the events in a concat section that is going to be played
#if !__NO_OUTPUT
					const cutfObjectVariationEventArgs* pVariationEventFlag = static_cast<const cutfObjectVariationEventArgs *>( pEventArgs );
					int Component = pVariationEventFlag->GetComponent();
					int Drawable = pVariationEventFlag->GetDrawable(); 
					int Texture = pVariationEventFlag->GetTexture(); 
					s32 section = CutSceneManager::GetInstance()->GetConcatSectionForTime(AllEventList[i]->GetTime());
#endif
					bool bValidEventTime = CutSceneManager::GetInstance()->ValidateEventTime(AllEventList[i]->GetTime());
					if (bValidEventTime)
					{

						cutsceneModelEntityDebugf3("GetEntityVariationEvents:: Variation event has valid section/time (%d/%f) : (Component: %d (%s), Drawable: %d, Texture: %d)", section, AllEventList[i]->GetTime(), Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						m_pVariationEvents.PushAndGrow(AllEventList[i]); 
					}
					else
					{
						cutsceneModelEntityDebugf3("GetEntityVariationEvents:: Variation event has invalid section/time (%d/%f) : (Component: %d (%s), Drawable: %d, Texture: %d)", section, AllEventList[i]->GetTime(), Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture );
					}
				}
				else
				{
					m_pVariationEvents.PushAndGrow(AllEventList[i]); 
				}
				
			}
		}
	}
}

void CCutsceneAnimatedActorEntity::PreStreamVariations(CutSceneManager* pManager, float CurrentTime, float LookAhead)
{
	float StreamingOffsetBuffer = pManager->CalculateStreamingOffset(LookAhead, CurrentTime);
	float LookAheadTime = pManager->GetTime() + StreamingOffsetBuffer;

	//cutsceneModelEntityDebugf("PreStreamVariations - Current Time (%6.4f) -> Streaming time(%6.4f)", pManager->GetTime(), pManager->GetTime() + StreamingOffsetBuffer);

	for ( int i = m_VariationStreamingIndex; i < m_pVariationEvents.GetCount(); ++i )
	{
		if(m_pVariationEvents[i]->GetTime() <=  LookAheadTime)
		{					
			bool bIsValidEventTime = true;
			if (pManager->IsConcatted())
			{
				bIsValidEventTime = pManager->ValidateEventTime(m_pVariationEvents[i]->GetTime());
			}

			if ( bIsValidEventTime)
			{
				const cutfObjectVariationEventArgs *pEventArgs = static_cast<const cutfObjectVariationEventArgs *>( m_pVariationEvents[i]->GetEventArgs() );
				if(pEventArgs)
				{
					s32 Component = pEventArgs->GetComponent(); 
					s32 Drawable = pEventArgs->GetDrawable(); 
					s32 Texture = pEventArgs->GetTexture(); 
					
					if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-1))
					{	
						pManager->GetAssetManager()->SetActorVariationStreamingRequest(Component, Drawable, Texture, m_ModelNameHash, m_pCutfObject->GetObjectId()); 
					}
					else
					{
						if (Component<PV_MAX_COMP)
							cutsceneModelEntityDebugf3( "PreStreamVariations: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
					}
				}
				else
				{
					cutsceneModelEntityDebugf3( "PreStreamVariations: Ignoring invalid cutscene variation args");
				}
			}

			// Increment m_VariationStreamingIndex so we don't consider this variation again
			m_VariationStreamingIndex++; 
		}
	}
}

void CCutsceneAnimatedActorEntity::ComputeInitialVariations(atHashString& UNUSED_PARAM(modelHashString))
{
	//bool bRepositionOnlyEntity = (modelHashString == m_RegisteredEntityFromScript.ModelNameHash);
	if ( /*!bRepositionOnlyEntity &&*/ !m_bComputeInitialVariationsForGameEntity)
	{
		m_bComputeInitialVariationsForGameEntity = true;

		cutsceneModelEntityDebugf3( "ComputeInitialVariations (%s) ---------------------------------------------", m_ModelNameHash.TryGetCStr());


		// Copy all the variations set by script from the initial variations array into the current variations array
		for(int i = 0; i < m_sScriptActorVariationData.iPedVarData.GetCount(); i++ )
		{
			s32 Component = m_sScriptActorVariationData.iPedVarData[i].iComponent; 
			s32 Drawable = m_sScriptActorVariationData.iPedVarData[i].iDrawable; 
			s32 Texture = m_sScriptActorVariationData.iPedVarData[i].iTexture; 

			if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-2))
			{		
				cutsceneModelEntityDebugf3("ComputeInitialVariations - Script variation (Component: %d (%s), Drawable: %d, Texture: %d)",
					Component, 
					CPedVariationData::GetVarOrPropSlotName(Component), 
					Drawable, 
					Texture);

				StoreActorVariationData(Component, Drawable, Texture, m_sCurrentActorVariationData);
			}
		}

		// For each cutscene variation event that happened on the start frame (may or may not be 0.0 depending on if the cutscene in branched)
		for ( int i = m_VariationStreamingIndex; i < m_pVariationEvents.GetCount(); ++i )
		{
			float fEventTime = m_pVariationEvents[i]->GetTime();
			float fStartTime = CutSceneManager::GetInstance()->GetStartTime();
			if(fEventTime <= fStartTime)
			{		
				bool bIsValidEventTime = true;
				if (CutSceneManager::GetInstance()->IsConcatted())
				{
					bIsValidEventTime = CutSceneManager::GetInstance()->ValidateEventTime(fEventTime);
				}

				if (bIsValidEventTime)
				{
					bool bUseVariationFromCutsceneStartFrame = true; 
					const cutfObjectVariationEventArgs *pEventArgs = static_cast<const cutfObjectVariationEventArgs *>( m_pVariationEvents[i]->GetEventArgs() );
					if(pEventArgs)
					{
						s32 Component = pEventArgs->GetComponent(); 
						s32 Drawable = pEventArgs->GetDrawable(); 
						s32 Texture = pEventArgs->GetTexture(); 

						if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-2))
						{		
							// Favour script variation events over cutscene variation events on the first frame
							for(int j = 0; j < m_sScriptActorVariationData.iPedVarData.GetCount(); j++ )
							{
								if((u32)Component == m_sScriptActorVariationData.iPedVarData[j].iComponent)
								{
									cutsceneModelEntityDebugf3("ComputeInitialVariations - Script variation (Component: %d (%s), Drawable: %d, Texture: %d) used rather than cutscene start frame variation (Component: %d (%s), Drawable: %d, Texture: %d)",
										m_sScriptActorVariationData.iPedVarData[j].iComponent, 
										CPedVariationData::GetVarOrPropSlotName(m_sScriptActorVariationData.iPedVarData[j].iComponent), 
										m_sScriptActorVariationData.iPedVarData[j].iDrawable, 
										m_sScriptActorVariationData.iPedVarData[j].iTexture, 
										Component, 
										CPedVariationData::GetVarOrPropSlotName(Component), 
										Drawable, 
										Texture);

									bUseVariationFromCutsceneStartFrame = false; 
									break; 
								}
							}

							if(bUseVariationFromCutsceneStartFrame)
							{
								cutsceneModelEntityDebugf3("ComputeInitialVariations - Cutscene start frame variation (Component: %d (%s), Drawable: %d, Texture: %d)",
									Component, 
									CPedVariationData::GetVarOrPropSlotName(Component), 
									Drawable, 
									Texture);

								StoreActorVariationData(Component, Drawable, Texture, m_sCurrentActorVariationData);
							}
						}
						else
						{
							cutsceneModelEntityDebugf3( "ComputeInitialVariations: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						}
					}
					else
					{
						cutsceneModelEntityDebugf3( "ComputeInitialVariations: Ignoring invalid cutscene variation args");
					}
				}
				// Cutscene variations events are so increment m_VariationStreamingIndex so we don't consider this variation again
				m_VariationStreamingIndex++;
			}
		}
		
		//
		// Make sure all variation kinds have at least a default entry
		//

		// For each kind of variation component
		for(u32 i = 0; i < PV_MAX_COMP; i++)
		{
			// Search for a valid entry
			bool bFoundValidVariationEntry = false; 
			for(u32 j=0; j<m_sCurrentActorVariationData.iPedVarData.GetCount(); j++)
			{
				if(m_sCurrentActorVariationData.iPedVarData[j].iComponent == i)
				{
					bFoundValidVariationEntry = true; 
					break; 
				}
			}

			// If there is no matching entry make a new "default" entry for that kind of variation in the initial variations array
			if(!bFoundValidVariationEntry)
			{
				cutsceneModelEntityDebugf3("ComputeInitialVariations - Fallback variation (Component: %d (%s), Drawable: %d, Texture: %d)",
					i, 
					CPedVariationData::GetVarOrPropSlotName(i), 
					0, 
					0);

				StoreActorVariationData(i, 0, 0, m_sCurrentActorVariationData);
			}

			/*
				cutsceneModelEntityDebugf( "ComputeInitialVariations: m_sCurrentActorVariationData[%d] (Component: %d (%s), Drawable: %d, Texture: %d)", 
				i,
				m_sCurrentActorVariationData.iPedVarData[i].iComponent, 
				CPedVariationData::GetVarOrPropSlotName(m_sCurrentActorVariationData.iPedVarData[i].iComponent), 
				m_sCurrentActorVariationData.iPedVarData[i].iDrawable, 
				m_sCurrentActorVariationData.iPedVarData[i].iTexture);
			*/
		}

		for(int i=0; i < m_sCurrentActorVariationData.iPedVarData.GetCount(); i++)
		{
			s32 Component = m_sCurrentActorVariationData.iPedVarData[i].iComponent; 
			s32 Drawable = m_sCurrentActorVariationData.iPedVarData[i].iDrawable; 
			s32 Texture = m_sCurrentActorVariationData.iPedVarData[i].iTexture;

			if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-1))
			{		
				eStreamingRequestReturnValue streamingRequestReturnValue = CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, (u32)Drawable, (u32)Texture, m_ModelNameHash, m_pCutfObject->GetObjectId()); 
				if(Component < PV_MAX_COMP)
				{
					if ((streamingRequestReturnValue == ESRRV_INVALID_VARIATION) && (Drawable > 0 || Texture > 0))
					{
						cutsceneModelEntityDebugf3( "ComputeInitialVariations: Ignoring invalid cutscene variation request (Component: %d (%s), Drawable: %d, Texture: %d) (requesting default instead)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, 0, 0, m_ModelNameHash, m_pCutfObject->GetObjectId()); 
					}
				}
			}
			else
			{
				if (Component<PV_MAX_COMP)
					cutsceneModelEntityDebugf3( "ComputeInitialVariations: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
			}
		} 
	}
}

void CCutsceneAnimatedActorEntity::ComputeVariationsForSkip(atHashString& modelHashString)
{
	/*cutsceneModelEntityDebugf("ComputeVariationsForSkip  %s ---------------------------------------------", modelHashString.TryGetCStr());
	cutsceneModelEntityDebugf("(iSceneId(%d),SceneNameHash(%s),ModelNameHash(%s),bDeleteBeforeEnd(%d),bCreatedForScript(%d),iEnterStatePhase(%6.4f),iExitStatePhase(%6.4f),bAppearInScene(%d)",
		m_RegisteredEntityFromScript.iSceneId, 	
		m_RegisteredEntityFromScript.SceneNameHash.TryGetCStr(), 
		m_RegisteredEntityFromScript.ModelNameHash.TryGetCStr(), 
		m_RegisteredEntityFromScript.bDeleteBeforeEnd,
		m_RegisteredEntityFromScript.bCreatedForScript,
		m_RegisteredEntityFromScript.iEnterStatePhase, 
		m_RegisteredEntityFromScript.iExitStatePhase,
		m_RegisteredEntityFromScript.bAppearInScene );

	bool bNeedsVariations = false;

	static bool bMethodA = true;
	if (bMethodA)
	{
		if (!IsScriptRegisteredGameEntity()
			&& IsScriptRegisteredRepositionOnlyEntity() 
			&& !m_RegisteredEntityFromScript.pEnt				
			&& m_RegisteredEntityFromScript.bCreatedForScript )
		{
			bNeedsVariations = true;
		}

		// Only run when the repositionOnlyEntities model is loaded
		bNeedsVariations = bNeedsVariations || ((modelHashString.GetHash() == m_RegisteredEntityFromScript.ModelNameHash) && !m_bComputeVariationsForSkipForRepositionOnlyEntity);
	}
	else
	{
		atHashString modelNameHash(m_ModelNameHash);
		if (!IsScriptRegisteredGameEntity()
			&& IsScriptRegisteredRepositionOnlyEntity() 
			&& !m_RegisteredEntityFromScript.pEnt				// no script entity being animated by the scene
			&& m_RegisteredEntityFromScript.bCreatedForScript )	// reposition entity is to be created by the cutscene for the script
		{
			cutsceneModelEntityDebugf("ComputeVariationsForSkip - repositiononly = true = m_RegisteredEntityFromScript.ModelNameHash");
			bNeedsVariations = true;
			modelNameHash = m_RegisteredEntityFromScript.ModelNameHash;

		}

		if (!m_RegisteredEntityFromScript.bDeleteBeforeEnd)
		{
			bNeedsVariations = true;
			cutsceneModelEntityDebugf("ComputeVariationsForSkip - repositiononly = true = ModelNameHash");
		}
	}

	if ( bNeedsVariations && !m_bComputeVariationsForSkipForRepositionOnlyEntity )
	{
	*/

	atHashString modelNameHash(m_ModelNameHash);

	if (!IsScriptRegisteredGameEntity()
		&& IsScriptRegisteredRepositionOnlyEntity() 
		&& !m_RegisteredEntityFromScript.pEnt				// no script entity being animated by the scene
		&& m_RegisteredEntityFromScript.bCreatedForScript )	// reposition entity is to be created by script for the cutscene

	{
		modelNameHash = m_RegisteredEntityFromScript.ModelNameHash;
		cutsceneModelEntityDebugf3("ComputeVariationsForSkip - repositiononly = true");
	}
	else
	{
		cutsceneModelEntityDebugf3("ComputeVariationsForSkip - repositiononly = false");
	}

	bool bAppropriateEntity = (modelHashString.GetHash() == modelNameHash.GetHash());
	if ( bAppropriateEntity && !m_bComputeVariationsForSkipForRepositionOnlyEntity )
	{
		m_bComputeVariationsForSkipForRepositionOnlyEntity = true;

		cutsceneModelEntityDebugf3("ComputeVariationsForSkip  ---------------------------------------------");

		//
		// Any variations we have set thus far are collected in the current variations array
		//

		for(int i=0; i<m_sCurrentActorVariationData.iPedVarData.GetCount(); i++)
		{
			cutsceneModelEntityDebugf3("ComputeVariationsForSkip - PreExisting variations (Component: %d (%s), Drawable: %d, Texture: %d)",
				m_sCurrentActorVariationData.iPedVarData[i].iComponent, 
				CPedVariationData::GetVarOrPropSlotName(m_sCurrentActorVariationData.iPedVarData[i].iComponent), 
				m_sCurrentActorVariationData.iPedVarData[i].iDrawable, 
				m_sCurrentActorVariationData.iPedVarData[i].iTexture);
		}

		float fEndTime = CutSceneManager::GetInstance()->GetEndTime();
		float fStartTime = CutSceneManager::GetInstance()->GetStartTime();

		// Search through all the variation events for this ped from the current time to the end time 
		for ( int i = 0; i < m_pVariationEvents.GetCount(); ++i )
		{
			float fEventTime = m_pVariationEvents[i]->GetTime();
			if((fEventTime > fStartTime) && (fEventTime <= fEndTime))
			{		
				bool bIsValidEventTime = true;
				if (CutSceneManager::GetInstance()->IsConcatted())
				{
					bIsValidEventTime = CutSceneManager::GetInstance()->ValidateEventTime(fEventTime);
				}

				if (bIsValidEventTime)
				{
					const cutfObjectVariationEventArgs *pEventArgs = static_cast<const cutfObjectVariationEventArgs *>( m_pVariationEvents[i]->GetEventArgs() );
					if(pEventArgs)
					{
						s32 Component = pEventArgs->GetComponent(); 
						s32 Drawable = pEventArgs->GetDrawable(); 
						s32 Texture = pEventArgs->GetTexture(); 

						if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP))
						{		
							cutsceneModelEntityDebugf3("ComputeVariationsForSkip - Cutscene variation (not start frame) (Component: %d (%s), Drawable: %d, Texture: %d)",
								Component, 
								CPedVariationData::GetVarOrPropSlotName(Component), 
								Drawable, 
								Texture);

							StoreActorVariationData(Component, Drawable, Texture, m_sCurrentActorVariationData);
						}
						else
						{
							cutsceneModelEntityDebugf3( "ComputeVariationsForSkip: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						}
					}
					else
					{
						cutsceneModelEntityDebugf3( "ComputeVariationsForSkip: Ignoring invalid cutscene variation args");
					}
				}
			}
		}


		//
		// Make sure all variation kinds have at least a default entry
		//

		// For each kind of variation
		for(u32 i = 0; i < PV_MAX_COMP; i++)
		{
			// Search for a matching entry
			bool bFoundComponent = false; 
			for(int j=0; j<m_sCurrentActorVariationData.iPedVarData.GetCount(); j++)
			{
				if(m_sCurrentActorVariationData.iPedVarData[j].iComponent == i)
				{
					bFoundComponent = true; 
					break; 
				}
			}

			// If there is no matching entry make a new "default" entry for that kind of variation in the initial variations array
			if(!bFoundComponent)
			{
				cutsceneModelEntityDebugf3("ComputeVariationsForSkip - Fallback variation (Component: %d (%s), Drawable: %d, Texture: %d)",
					i, 
					CPedVariationData::GetVarOrPropSlotName(i), 
					0, 
					0);

				StoreActorVariationData(i, 0, 0, m_sCurrentActorVariationData);
			}
		}

		//
		// Request each variation in the current variation array
		//
		for(int i=0; i < m_sCurrentActorVariationData.iPedVarData.GetCount(); i++)
		{
			s32 Component = m_sCurrentActorVariationData.iPedVarData[i].iComponent; 
			s32 Drawable = m_sCurrentActorVariationData.iPedVarData[i].iDrawable; 
			s32 Texture = m_sCurrentActorVariationData.iPedVarData[i].iTexture;

			if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-1))
			{		
				eStreamingRequestReturnValue streamingRequestReturnValue = CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, (u32)Drawable, (u32)Texture, modelNameHash, m_pCutfObject->GetObjectId()); 
				if(Component < PV_MAX_COMP)
				{
					if ((streamingRequestReturnValue == ESRRV_INVALID_VARIATION) && (Drawable > 0 || Texture > 0))
					{
						cutsceneModelEntityDebugf3( "ComputeVariationsForSkip: Ignoring invalid cutscene variation request (Component: %d (%s), Drawable: %d, Texture: %d) (requesting default instead)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, 0, 0, modelNameHash, m_pCutfObject->GetObjectId()); 
					}
				}
			}
			else
			{
				if (Component<PV_MAX_COMP)
					cutsceneModelEntityDebugf3( "ComputeVariationsForSkip: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
			}
		}
	}
}

void CCutsceneAnimatedActorEntity::AddActorToWorld(CPed* pPed)
{	
	if(pPed)
	{
		InitActor(pPed);
		pPed->GetPortalTracker()->SetIsCutsceneControlled(true);

		if (IsScriptRegisteredGameEntity())
		{
			m_RegisteredEntityFromScript.pEnt = pPed; 
		}

		m_bCreatedByCutscene = true; 
	}
}

void CCutsceneAnimatedActorEntity::CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity, bool bRequestVariations, bool bApplyVariations)
{
	CPed* pPed = NULL;

	if(CreateRepositionEntity)
	{
		pPed = GetGameRepositionOnlyEntity();
	}
	else
	{
		pPed = GetGameEntity();
	}

	if(pPed == NULL)
	{
		cutsceneModelEntityDebugf2("CreateGameEntity - Actor - bRequestVariations(%s), bApplyVariations(%s)", bRequestVariations ? "T" : "F",  bApplyVariations ? "T" : "F");

		CreateGamePed(iModelIndex.Get(), CreateRepositionEntity);		

		if(CreateRepositionEntity)
		{
			pPed = GetGameRepositionOnlyEntity();
		}
		else
		{
			pPed = GetGameEntity();
		}
	}

	if(pPed && !pPed->GetIsAddedToWorld())
	{
		AddActorToWorld(pPed);
	}	

	if (pPed)
	{
		if (bRequestVariations && !IsBlockingVariationStreamingAndApplication())
		{	
			//
			// Make sure all variation kinds have at least a default entry
			//

			// For each kind of variation component
			for(u32 i = 0; i < PV_MAX_COMP; i++)
			{
				// Search for a valid entry
				bool bFoundValidVariationEntry = false; 
				for(u32 j=0; j<m_sCurrentActorVariationData.iPedVarData.GetCount(); j++)
				{
					if(m_sCurrentActorVariationData.iPedVarData[j].iComponent == i)
					{
						bFoundValidVariationEntry = true; 
						break; 
					}
				}

				// If there is no matching entry make a new "default" entry for that kind of variation in the initial variations array
				if(!bFoundValidVariationEntry)
				{
					cutsceneModelEntityDebugf2("CreateGameEntity - Actor - - Fallback variation (Component: %d (%s), Drawable: %d, Texture: %d)",
						i, 
						CPedVariationData::GetVarOrPropSlotName(i), 
						0, 
						0);

					StoreActorVariationData(i, 0, 0, m_sCurrentActorVariationData);
				}

				/*
					cutsceneModelEntityDebugf( "CreateGameEntity - Actor -: m_sCurrentActorVariationData[%d] (Component: %d (%s), Drawable: %d, Texture: %d)", 
					i,
					m_sCurrentActorVariationData.iPedVarData[i].iComponent, 
					CPedVariationData::GetVarOrPropSlotName(m_sCurrentActorVariationData.iPedVarData[i].iComponent), 
					m_sCurrentActorVariationData.iPedVarData[i].iDrawable, 
					m_sCurrentActorVariationData.iPedVarData[i].iTexture);
				*/
			}


			// Request the current variations
			for(int i=0; i < m_sCurrentActorVariationData.iPedVarData.GetCount(); i++)
			{
				s32 Component = m_sCurrentActorVariationData.iPedVarData[i].iComponent; 
				s32 Drawable = m_sCurrentActorVariationData.iPedVarData[i].iDrawable; 
				s32 Texture = m_sCurrentActorVariationData.iPedVarData[i].iTexture;

				if((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-1))
				{		
					if (pPed && pPed->GetPedModelInfo() && pPed->GetPedModelInfo()->GetIsStreamedGfx())
					{				
						atHashString modelNameHash(m_ModelNameHash);
						if (CreateRepositionEntity && m_RegisteredEntityFromScript.ModelNameHash.GetHash()>0)
						{
							modelNameHash = m_RegisteredEntityFromScript.ModelNameHash.GetHash();
						}

						// Now we have a ped pointer we can check the variation requested by the cutscene or script is valid
						if (pPed->IsVariationInRange(static_cast<ePedVarComp>(Component), Drawable, Texture))
						{
							CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, (u32)Drawable, (u32)Texture, modelNameHash, m_pCutfObject->GetObjectId()); 
						}
						else
						{
							if (Drawable > 0 || Texture > 0)
							{
								cutsceneModelEntityDebugf2( "CreateGameEntity: Ignoring out of range cutscene variation request (Component: %d (%s), Drawable: %d, Texture: %d) (requesting default instead)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
								CutSceneManager::GetInstance()->GetAssetManager()->SetActorVariationStreamingRequest((u32)Component, 0, 0, modelNameHash, m_pCutfObject->GetObjectId()); 
							}
							else
							{
								cutsceneModelEntityDebugf2( "CreateGameEntity: Ignoring out of range cutscene variation request (Component: %d (%s), Drawable: %d, Texture: %d) (no valid default)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
							}
						}
					}
				}
				else
				{
					//cutsceneModelEntityDebugf( "CCutsceneAnimatedActorEntity: Ignoring invalid cutscene variation event (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
				}
			}
		}

		if (bApplyVariations && !IsBlockingVariationStreamingAndApplication())
		{
			// Apply the current variations
			RestoreEntityProperties(pPed);
		}
	}

}

void CCutsceneAnimatedActorEntity::UpdateCutSceneTask(cutsManager* pManager)
{
	CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager);

		if(GetGameEntity())
		{
			CTask* pTask = GetGameEntity()->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE); 
			
			if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE)
			{
				CTaskCutScene* pCutSceneTask = static_cast<CTaskCutScene*>(pTask); 

				const crClip* pClip = pCutSceneTask->GetClip(); 
			
				if (pClip)
				{
					float fPhase = pCutsManager->GetPhaseUpdateAmount(pClip, GetAnimEventStartTime()); 
					
					float fLastPhase = pCutsManager->GetAnimPhaseForSection(pClip->GetDuration(),GetAnimEventStartTime(),pCutsManager->GetCutScenePreviousTime()); 
					GetMoverTrackVelocity(pClip,fPhase,fLastPhase, pCutsManager->GetCurrentCutSceneTimeStep());
					//Vector3 localEuler = pCutSceneTask->GetAnimOrigin().GetEulers();
					//printf("Rotation for %s: %7.4f, %7.4f, %7.4f \n", pClip->GetName(), localEuler.x*RtoD, localEuler.y*RtoD, localEuler.z*RtoD);
					
					//UpdateActorMotionState();
					GetGameEntity()->SetDesiredHeading(GetGameEntity()->GetTransform().GetHeading()); 
					
					UpdateCutSceneTaskPhase(pTask, pManager); 
#if __DEV
					//THIS CURRENTLY DOES NOTHING TO FILTER ANY ANIMS BECAUSE THERE IS NO FILTER TO REMOVE ALL ANIM EXCEPT FACIAL
					UpdateWithFaceViewer(pCutsManager,  pCutSceneTask);
#endif
					// Vehicle lighting - Have we crossed any CutsceneBlendToVehicleLighting tags?
					const crTags* pTags = pClip->GetTags();
					if (pTags)
					{
						for (int i=0; i < pTags->GetNumTags(); i++)
						{
							const crTag* pTag = pTags->GetTag(i);
							if (pTag)
							{
								//static CClipEventTags::Key cutsceneBlendToVehicleLightingKey("CutsceneBlendToVehicleLighting",0xB8336AF5);
								if (pTag->GetKey() == CClipEventTags::CutsceneBlendToVehicleLighting.GetHash())
								{
									// Tag phase
									const float fTagPhase = pTag->GetStart();

									// Process this tag?
									if (fTagPhase >= fLastPhase && fTagPhase <= fPhase)
									{
										const crProperty& rProperty = pTag->GetProperty();
										const crPropertyAttribute* pAttribTarget = rProperty.GetAttribute(CClipEventTags::TargetValue.GetHash());
										const crPropertyAttribute* pAttribTime = rProperty.GetAttribute(CClipEventTags::BlendTime.GetHash());

										// Target value
										if (pAttribTarget && pAttribTarget->GetType() == crPropertyAttribute::kTypeFloat)
										{
											const crPropertyAttributeFloat* pTagTargetValueFloat = static_cast<const crPropertyAttributeFloat*>(pAttribTarget);
											{
												m_fVehicleLightingScalarTargetValue = pTagTargetValueFloat->GetFloat();
												m_fVehicleLightingScalarTargetValue = Clamp(m_fVehicleLightingScalarTargetValue, 0.0f, 1.0f);
											}
										}

										// Blend time - converted to rate
										if (pAttribTime && pAttribTime->GetType() == crPropertyAttribute::kTypeFloat)
										{
											const crPropertyAttributeFloat* pTagBlendTimeFloat = static_cast<const crPropertyAttributeFloat*>(pAttribTime);
											{
												const float fBlendTime = pTagBlendTimeFloat->GetFloat();

												if (fBlendTime > VERY_SMALL_FLOAT)
												{
													// Blend up or down?
													if (m_fVehicleLightingScalarTargetValue < m_fVehicleLightingScalar)
													{
														m_fVehicleLightingScalarBlendRate = -(1.0f / fBlendTime);
													}
													else
													{
														m_fVehicleLightingScalarBlendRate = 1.0f / fBlendTime;
													}
												}
												else
												{
													m_fVehicleLightingScalarBlendRate = 1000.0f;
												}
											}
										}
									}
								}
							}
						}
					}
				}

				// Vehicle lighting - Update vehicle lighting scalar.
				if (m_fVehicleLightingScalar != m_fVehicleLightingScalarTargetValue)
				{
					Assert(m_fVehicleLightingScalarTargetValue >= 0.0f && m_fVehicleLightingScalarTargetValue <= 1.0f);

					if (m_fVehicleLightingScalarBlendRate == 1000.0f || m_fVehicleLightingScalarBlendRate == -1000.0f)
					{
						// Instant
						m_fVehicleLightingScalar = m_fVehicleLightingScalarTargetValue;
					}
					else
					{
						// Blend
						const float fDelta = pCutsManager->GetCutSceneCurrentTime() - pCutsManager->GetCutScenePreviousTime();
						m_fVehicleLightingScalar += (fDelta * m_fVehicleLightingScalarBlendRate);
						
						// Clamp
						if (m_fVehicleLightingScalarBlendRate < 0.0f && m_fVehicleLightingScalar < m_fVehicleLightingScalarTargetValue)
						{
							m_fVehicleLightingScalar = m_fVehicleLightingScalarTargetValue;
						}
						else if (m_fVehicleLightingScalarBlendRate > 0.0f && m_fVehicleLightingScalar > m_fVehicleLightingScalarTargetValue)
						{
							m_fVehicleLightingScalar = m_fVehicleLightingScalarTargetValue;
						}
					}
				}
			
				// Set the vehicle lighting scalar on the cutscene task
				Assert(m_fVehicleLightingScalar >= 0.0f && m_fVehicleLightingScalar <= 1.0f);
				pCutSceneTask->SetVehicleLightingScalar(m_fVehicleLightingScalar);
			}
		}
}

void CCutsceneAnimatedActorEntity:: ForceUpdateAi(cutsManager* pManager)
{
	if(pManager)
	{
		CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);

		 if(pCutSceneManager->GetShutDownMode() != SHUTDOWN_SESSION)
		 {
			 CPed* pPed = GetGameEntity();
			 if (pPed)
			 {
				 pPed->InstantAIUpdate();
			 }
		 }
	}
};
/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedActorEntity::SetGameEntityReadyForGame(cutsManager* pManager)
{
	if(!m_bIsReadyForGame)
	{
		cutsceneModelEntityDebugf2("SetGameEntityReadyForGame - Actor");

		if (GetGameEntity())
		{		
			NetworkInterface::CutsceneFinishedOnEntity(*GetGameEntity());
		}

		if (GetGameRepositionOnlyEntity())
		{		
			NetworkInterface::CutsceneFinishedOnEntity(*GetGameRepositionOnlyEntity());
		}

		if (GetGameEntity() && !GetGameRepositionOnlyEntity())
		{	
#if !__NO_OUTPUT
			ResetEntityForCallStackLogging(); 
#endif

			//only set entities needed for the game ready for the game
			if(m_RegisteredEntityFromScript.bCreatedForScript || !m_RegisteredEntityFromScript.bDeleteBeforeEnd)
			{
				cutsceneModelEntityDebugf2("SetGameEntityReadyForGame: Setting cutscene ped (%s) ready for game", GetGameEntity()->GetModelName());
				SetVisibility(GetGameEntity(), true); 
				cutsceneModelEntityDebugf2("CCutsceneAnimatedActorEntity::SetGameEntityReadyForGame1 : Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());

				GetGameEntity()->SetDesiredHeading(GetGameEntity()->GetTransform().GetHeading()); 		
				GetGameEntity()->SetFixedPhysics(false); 
				GetGameEntity()->EnableCollision();
				GetGameEntity()->SetUseExtractedZ(false);
				GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
				GetGameEntity()->SetVelocity(GetAnimatedVelocity());

				if (m_OptionFlags.IsFlagSet(CEO_RESET_CAPSULE_AT_END))
				{
					GetGameEntity()->ClearBound(); 
					GetGameEntity()->SetBoundPitch(0.0f); 
					GetGameEntity()->SetBoundHeading(0.0f); 
					GetGameEntity()->SetBoundOffset(VEC3_ZERO);
				}

				if(m_pCutfObject && !GetGameEntity()->GetPedAiLod().IsSafeToSwitchToFullPhysics())
				{
					cutsceneModelEntityWarningf("Scene %s has left %s where it's unsafe to turn physics on. Check that the anim is leaving the player out of collision with objects or map", pManager->GetCutsceneName(), m_pCutfObject->GetDisplayName().c_str()); 
				}
				GetGameEntity()->GetPedAiLod().ClearLowLodPhysicsFlag();

				//if the entity is created by the scene lets reset these variables
				if(m_bCreatedByCutscene)
				{
					GetGameEntity()->SetBlockingOfNonTemporaryEvents(false);
					GetGameEntity()->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, true );
				}

				CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

				//Set the matrices to be the same for the ragdoll if the scene has been skipped
				if(pCutSceneManager->WasSkipped())
				{
					if(GetGameEntity()->GetRagdollInst() && GetGameEntity()->GetRagdollInst()->IsInLevel() 
						&& GetGameEntity()->GetRagdollState()!=RAGDOLL_STATE_PHYS 
						&& GetGameEntity()->GetRagdollState()!=RAGDOLL_STATE_ANIM_DRIVEN)
					{
						GetGameEntity()->GetRagdollInst()->SetMatrix(GetGameEntity()->GetMatrix());
						PHSIM->SetLastInstanceMatrix(GetGameEntity()->GetRagdollInst(), GetGameEntity()->GetMatrix());
						GetGameEntity()->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);
						GetGameEntity()->GetRagdollInst()->PoseBoundsFromSkeleton(true, true);
					}
				}

				CPed* pPed = GetGameEntity();
				if (pPed->GetFacialData())
				{
					pPed->GetFacialData()->ResetFacialIdleAnimation(pPed);
				}

				if (!m_OptionFlags.IsFlagSet(CEO_PRESERVE_FACE_BLOOD_DAMAGE))
				{
					pPed->HideBloodDamage(kDamageZoneHead, false);
				}

				if (!m_OptionFlags.IsFlagSet(CEO_PRESERVE_BODY_BLOOD_DAMAGE))
				{
					if (m_OptionFlags.IsFlagSet(CEO_REMOVE_BODY_BLOOD_DAMAGE))
					{
						pPed->HideBloodDamage(kDamageZoneTorso, false);
						pPed->HideBloodDamage(kDamageZoneLeftArm, false);
						pPed->HideBloodDamage(kDamageZoneRightArm, false);
						pPed->HideBloodDamage(kDamageZoneLeftLeg, false);
						pPed->HideBloodDamage(kDamageZoneRightLeg, false);
					}
					else
					{
						pPed->LimitBloodDamage(kDamageZoneTorso, -1);
						pPed->LimitBloodDamage(kDamageZoneLeftArm, -1);
						pPed->LimitBloodDamage(kDamageZoneRightArm, -1);
						pPed->LimitBloodDamage(kDamageZoneLeftLeg, -1);
						pPed->LimitBloodDamage(kDamageZoneRightLeg, -1);
					}
				}
			}
			else
			{
				cutsceneModelEntityDebugf2("SetGameEntityReadyForGame: Disabling collision on cutscene ped (%s) as it is to be deleted", GetGameEntity()->GetModelName());
				//Switch off the collision flags as this entity is to be deleted 
				if(GetGameEntity()->GetCurrentPhysicsInst()->IsInLevel())
				{
					CPhysics::GetLevel()->SetInstanceIncludeFlags(GetGameEntity()->GetCurrentPhysicsInst()->GetLevelIndex(), 0);
				}
			}
		}
		else
		{
			//Switch off the collision flags as this entity is to be deleted 
			if(GetGameEntity() && GetGameEntity()->GetCurrentPhysicsInst()->IsInLevel())
			{
				cutsceneModelEntityDebugf2("SetGameEntityReadyForGame: Disabling collision on cutscene ped (%s) as it is to be deleted",  GetGameEntity()->GetModelName());
				CPhysics::GetLevel()->SetInstanceIncludeFlags(GetGameEntity()->GetCurrentPhysicsInst()->GetLevelIndex(), 0);
			}
		}

		if (GetGameRepositionOnlyEntity() && !m_RegisteredEntityFromScript.bDeleteBeforeEnd)
		{	
			cutsceneModelEntityDebugf2("SetGameEntityReadyForGame: Setting reposition ped (%s) ready for game", GetGameRepositionOnlyEntity()->GetModelName());
			GetGameRepositionOnlyEntity()->SetFixedPhysics(false); 
			GetGameRepositionOnlyEntity()->SetPosition(m_CurrentPosition); 
			GetGameRepositionOnlyEntity()->SetHeading(m_CurrentHeading); 
			GetGameRepositionOnlyEntity()->SetDesiredHeading(m_CurrentHeading); 
			GetGameRepositionOnlyEntity()->GetPortalTracker()->RequestRescanNextUpdate();
			GetGameRepositionOnlyEntity()->EnableCollision();
			GetGameRepositionOnlyEntity()->SetVelocity(GetAnimatedVelocity());

			if (m_OptionFlags.IsFlagSet(CEO_RESET_CAPSULE_AT_END))
			{
				GetGameRepositionOnlyEntity()->ClearBound(); 
				GetGameRepositionOnlyEntity()->SetBoundPitch(0.0f); 
				GetGameRepositionOnlyEntity()->SetBoundHeading(0.0f); 
				GetGameRepositionOnlyEntity()->SetBoundOffset(VEC3_ZERO);
			}

			GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
			GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true); 
			cutsceneModelEntityDebugf2("CCutsceneAnimatedActorEntity::SetGameEntityReadyForGame2: Showing GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());

			CPed* pPed = GetGameRepositionOnlyEntity();
			if (pPed->GetFacialData())
			{
				pPed->GetFacialData()->ResetFacialIdleAnimation(pPed);
			}

			if (GetGameEntity())
			{
				SetVisibility(GetGameEntity(), false); 
				cutsceneModelEntityDebugf2("CCutsceneAnimatedActorEntity::SetGameEntityReadyForGame3: Hiding GetGameEntity (%s)", GetGameEntity()->GetModelName());
			}
		}

		if (m_pCutSceneTask)
		{
			m_pCutSceneTask->SetExitNextUpdate();
		}

		m_bIsReadyForGame = true;
	}
}



/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedActorEntity::SetGameEntityReadyForCutscene()
{
	if (GetGameEntity())
	{
#if !__NO_OUTPUT
		SetUpEntityForCallStackLogging(); 
#endif

		cutsceneModelEntityDebugf2("SetGameEntityReadyForCutscene - Actor");

		m_fOriginalLODMultiplier = GetGameEntity()->GetLodMultiplier();
		GetGameEntity()->SetLodMultiplier(4.0f);

		//remove any tasks or motion on the peds before scene starts

		// PedIntelligence->FlushImmediately should be done before EquipWeapon to ensure correct prop cleanup [12/21/2012 mdawe]
		GetGameEntity()->GetPedIntelligence()->FlushImmediately(true);
		
		// Set the ped's hands to be empty [12/21/2012 mdawe]
		if(GetGameEntity()->GetWeaponManager())
		{
			GetGameEntity()->GetWeaponManager()->EquipWeapon(GetGameEntity()->GetDefaultUnarmedWeaponHash(), -1, true);
		}

		// Kill off any non-facial idle animations
		if (GetGameEntity()->GetAnimDirector())
		{
			fwAnimDirectorComponentFacialRig* pFacialRigComp = GetGameEntity()->GetAnimDirector()->GetComponentByPhase<fwAnimDirectorComponentFacialRig>(fwAnimDirectorComponent::kPhaseAll);
			if (pFacialRigComp)
			{
				pFacialRigComp->RemoveAllNonIdleFacialAnims();
			}
		}

		if (GetGameEntity()->GetIsAttachedToGround())
		{
			GetGameEntity()->DetachFromGround();
		}
		GetGameEntity()->SetPedResetFlag(CPED_RESET_FLAG_DisableGroundAttachment, true);

		GetGameEntity()->SetIsCrouching(false);
		GetGameEntity()->RemoveMovementAnims();
		GetGameEntity()->BlendOutAnyNonMovementAnims(INSTANT_BLEND_OUT_DELTA);
		GetGameEntity()->StopAllMotion(true);

		if(!m_OptionFlags.IsFlagSet(CEO_DONT_RESET_PED_CAPSULE))
		{
			GetGameEntity()->InstantResetDesiredMainMoverCapsuleDataForCutscene();
		}

		GetGameEntity()->SetFixedPhysics(true); 
		GetGameEntity()->DisableCollision();
		GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything		= true;

		// Clone damage from reposition only entity?
		if(GetGameRepositionOnlyEntity() && m_OptionFlags.IsFlagSet(CEO_CLONE_DAMAGE_TO_CS_MODEL))
		{
			GetGameEntity()->CloneDamage(GetGameRepositionOnlyEntity(), true);
		}

		if (m_OptionFlags.IsFlagSet(CEO_PRESERVE_HAIR_SCALE))
		{
			m_fOriginalHairScale = GetGameEntity()->GetTargetHairScale();
		}

		if (!m_OptionFlags.IsFlagSet(CEO_PRESERVE_FACE_BLOOD_DAMAGE))
		{
			GetGameEntity()->HideBloodDamage(kDamageZoneHead, true);
			PEDDAMAGEMANAGER.ClearDamageDecals( GetGameEntity()->GetDamageSetID(), kDamageZoneHead, ATSTRINGHASH("bruise",0xe838afb1));
		}

		if (!m_OptionFlags.IsFlagSet(CEO_PRESERVE_BODY_BLOOD_DAMAGE))
		{
			if (m_OptionFlags.IsFlagSet(CEO_REMOVE_BODY_BLOOD_DAMAGE))
			{
				GetGameEntity()->HideBloodDamage(kDamageZoneTorso, true);
				GetGameEntity()->HideBloodDamage(kDamageZoneLeftArm, true);
				GetGameEntity()->HideBloodDamage(kDamageZoneRightArm, true);
				GetGameEntity()->HideBloodDamage(kDamageZoneLeftLeg, true);
				GetGameEntity()->HideBloodDamage(kDamageZoneRightLeg, true);
			}
			else
			{
				static s32 s_maxBloodDamageForCutscene = 2;
				GetGameEntity()->LimitBloodDamage(kDamageZoneTorso, s_maxBloodDamageForCutscene);
				GetGameEntity()->LimitBloodDamage(kDamageZoneLeftArm, s_maxBloodDamageForCutscene);
				GetGameEntity()->LimitBloodDamage(kDamageZoneRightArm, s_maxBloodDamageForCutscene);
				GetGameEntity()->LimitBloodDamage(kDamageZoneLeftLeg, s_maxBloodDamageForCutscene);
				GetGameEntity()->LimitBloodDamage(kDamageZoneRightLeg, s_maxBloodDamageForCutscene);
			}
		}

		if (m_OptionFlags.IsFlagSet(CEO_IS_CASCADE_SHADOW_FOCUS_ENTITY_DURING_EXIT) && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetCamEntity())
		{
			// Register this with the camera
			CCutSceneCameraEntity* pCam = const_cast<CCutSceneCameraEntity*>(CutSceneManager::GetInstance()->GetCamEntity());
			if (pCam)
			{
				if (GetGameRepositionOnlyEntity())
				{
					pCam->SetCascadeShadowFocusEntityForSeamlessExit(GetGameRepositionOnlyEntity());
				}
				else
				{
					pCam->SetCascadeShadowFocusEntityForSeamlessExit(GetGameEntity());
				}
			}
		}

		NetworkInterface::CutsceneStartedOnEntity(*GetGameEntity());
	}
}

void CCutsceneAnimatedActorEntity::SetRepositionOnlyEntityReadyForCutscene()
{
	if(GetGameRepositionOnlyEntity())
	{
		m_fOriginalLODMultiplier = GetGameRepositionOnlyEntity()->GetLodMultiplier();
		GetGameRepositionOnlyEntity()->SetLodMultiplier(4.0f);

		if(GetGameRepositionOnlyEntity()->GetWeaponManager())
		{		
			GetGameRepositionOnlyEntity()->GetWeaponManager()->EquipWeapon(GetGameRepositionOnlyEntity()->GetDefaultUnarmedWeaponHash(), -1, true);
		}

		if(GetGameRepositionOnlyEntity()->IsNetworkClone())
		{
			GetGameRepositionOnlyEntity()->GetPedIntelligence()->FlushImmediately(false);
			GetGameRepositionOnlyEntity()->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
		}
		else
		{
			GetGameRepositionOnlyEntity()->GetPedIntelligence()->FlushImmediately(true);
		}
		
		GetGameRepositionOnlyEntity()->SetIsCrouching(false);
		GetGameRepositionOnlyEntity()->RemoveMovementAnims();
		GetGameRepositionOnlyEntity()->BlendOutAnyNonMovementAnims(INSTANT_BLEND_OUT_DELTA);
		GetGameRepositionOnlyEntity()->StopAllMotion(true);
		GetGameRepositionOnlyEntity()->DisableCollision();
		GetGameRepositionOnlyEntity()->SetFixedPhysics(true); 
		GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = true;
		GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false); 
		cutsceneModelEntityDebugf2("CCutsceneAnimatedActorEntity::SetRepositionOnlyEntityReadyForCutscene: Hiding GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());
		
		NetworkInterface::CutsceneStartedOnEntity(*GetGameRepositionOnlyEntity());
	}
}

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedActorEntity::UpdateActorMotionState()
{
	if(GetGameEntity())
	{
		GetGameEntity()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing); 
	}
}



/////////////////////////////////////////////////////////////////////////
namespace
{
	void* g_pCharClothController = NULL;
	int   g_capsuleLeftT = 0;
	int   g_capsuleRightT = 1;
	float g_customLength = -1.0f;
	float g_customRadius = -1.0f;
}

void CCutsceneAnimatedActorEntity::DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs,const float fEventTime, const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_UPDATE_EVENT:
	case CUTSCENE_PAUSE_EVENT:	
		{
			if (m_bIsReadyForGame)
			{
				return;
			}
		
			/*if ( iEventId == CUTSCENE_UPDATE_EVENT)
			{
				cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent: CUTSCENE_UPDATE_EVENT");
			}
			else
			{
				cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent: CUTSCENE_PAUSE_EVENT");
			}*/

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager);

			if (GetGameEntity())
				GetGameEntity()->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);

			if(GetGameEntity() && m_pCutSceneTask == NULL)
			{
				GetGameEntity()->SetPedResetFlag(CPED_RESET_FLAG_DisablePotentialBlastReactions, true); 
			}

			if(!IsBlockingVariationStreamingAndApplication())
			{	
				PreStreamVariations(pCutSceneManager, pManager->GetTime(), PED_VARIATION_STREAMING_OFFSET); 
			}
			
			if (iEventId==CUTSCENE_UPDATE_EVENT && m_bStoppedCalled)
			{
				CreateRepositionOnlyGameEntityWhenModelLoaded(pManager, pObject, true, true );
				SetGameEntityReadyForGame(pManager);
			}

			UpdateCutSceneTask(pManager); 
			
			//Need to prevent time slicing if being updated by the cut scene system, cant just rely on the task blocking it 
			if (GetGameEntity())
			{
				GetGameEntity()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				GetGameEntity()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
			}
		}
		break;
	
	case CUTSCENE_SET_VARIATION_EVENT:
		{
			if(IsBlockingVariationStreamingAndApplication())
			{	
				return; 
			}

			if (m_bIsReadyForGame)
			{
				//cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent: Ignoring CUTSCENE_SET_VARIATION_EVENT m_bIsReadyForGame = false");
				return;
			}

			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

			//dont set the variations from the first frame, it has already been done. 
			if((pCutSceneManager->GetCutSceneCurrentTime() > pCutSceneManager->GetStartTime()) && (fEventTime > pCutSceneManager->GetStartTime()) && !m_bIsReadyForGame )
			{
				if(!GetGameEntity())
				{
					cutsceneModelEntityWarningf("No ped entity to set a variation on %s", pObject->GetDisplayName().c_str()); 
				}
				
				//cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent - CUTSCENE_SET_VARIATION_EVENT");
				cutsceneAssertf(pEventArgs, "Cutscene content bug! CUTSCENE_SET_VARIATION_EVENT (object name=%s type=%s)(event id=%i) is corrupted and needs to be recreated, variation will not be applied!", pObject->GetDisplayName().c_str(), pObject->GetTypeName(), iEventId);
				if (pEventArgs && pEventArgs->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
				{
					CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 
					CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 

					const cutfObjectVariationEventArgs* pVariationEventFlag = static_cast<const cutfObjectVariationEventArgs *>( pEventArgs );
				
					int Component = pVariationEventFlag->GetComponent();
					int Drawable = pVariationEventFlag->GetDrawable(); 
					int Texture = pVariationEventFlag->GetTexture(); 

					if ((Component>-1 && Drawable>-1 && Texture>-1) || (Component>=PV_MAX_COMP && Drawable>-2))
					{
						cutsceneModelEntityDebugf3("CCutsceneAnimatedActorEntity::DispatchEvent: CUTSCENE_SET_VARIATION_EVENT: (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
						SetCurrentActorVariation(GetGameEntity(), Component, Drawable, Texture); 
						if (Component<PV_MAX_COMP)
							pAssetManager->RemoveVariationStreamRequest(Component, Drawable, Texture, m_ModelNameHash, pObject->GetObjectId()); 
					}
					else
					{
						cutsceneModelEntityDebugf3("CCutsceneAnimatedActorEntity::DispatchEvent: Ignoring CUTSCENE_SET_VARIATION_EVENT with invalid data: (Component: %d (%s), Drawable: %d, Texture: %d)", Component, CPedVariationData::GetVarOrPropSlotName(Component), Drawable, Texture);
					}				
				}
				else
				{
					cutsceneModelEntityDebugf3("CCutsceneAnimatedActorEntity::DispatchEvent: Ignoring CUTSCENE_SET_VARIATION_EVENT with invalid or null event arguments");
				}
			}
			else
			{
				//cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent: Ignoring CUTSCENE_SET_VARIATION_EVENT pCutSceneManager->GetCutSceneCurrentTime() != pCutSceneManager->GetStartTimeFromStartFrame() && !m_bIsReadyForGame");
			}

		}
		break;

	case CUTSCENE_SET_DUAL_CLIP_EVENT:
		{
			if (pEventArgs &&  pEventArgs->GetType() == CUTSCENE_DUAL_CLIP_EVENT_ARGS_TYPE)
			{
				if (pObject)		
				{	
					cutsceneAssertf (0,"Not supported : CUTSCENE_SET_DUAL_CLIP_EVENT %s ",pObject->GetDisplayName().c_str());
				}
			}
		}
		break;
	

	case CUTSCENE_END_OF_SCENE_EVENT:
		{
			cutsceneModelEntityDebugf3("CCutsceneAnimatedActorEntity::DispatchEvent - CUTSCENE_END_OF_SCENE_EVENT");

#if RECORDING_VERTS
			if( PFD_DebugRecords.WillDraw() )
			{
				g_DbgRecordCustomClothEvents.Reset();
			}
#endif

			if( GetGameEntity() && GetGameEntity()->GetIsTypePed() )
			{
				CPed* pPed = static_cast<CPed*>(GetGameEntity());
				if( pPed )
				{
					for( int i = 0; i < PV_MAX_COMP; ++i )
					{
						characterClothController* pCharClothController = pPed->m_CClothController[i];
						if( pCharClothController )
						{
							pCharClothController->CheckQueuedPinning();
							pCharClothController->SetPinRadiusSetThreshold( PIN_RADIUSSETS_PACKAGE_THRESHOLD );
							pCharClothController->SetFlag( characterClothController::enIsForceSkin, false);
							pCharClothController->SetFlag( characterClothController::enIsProne, false );
							pCharClothController->SetFlag( characterClothController::enIsProneFlipped, false );
							pCharClothController->SetFlag( characterClothController::enIsEndCutscene, true );
							pCharClothController->SetPackageIndex(0);							

							if( (i == PV_COMP_UPPR) && (g_pCharClothController == pCharClothController) )
							{
								characterCloth* pCharCloth = (characterCloth*)pCharClothController->GetOwner();
								Assert( pCharCloth );
								const phBoundComposite* pCustomBound = pCharCloth->GetBoundComposite();
								Assert( pCustomBound );
								if( pCustomBound )
								{
									Assert( g_customLength > 0.0f );
									Assert( g_customRadius > 0.0f );

									phBound* pBoundPart = pCustomBound->GetBound(g_capsuleLeftT);
									Assert( pBoundPart );
									Assert( pBoundPart->GetType() == phBound::CAPSULE );

									phBoundCapsule* pBoundCapsule = (phBoundCapsule*)pBoundPart;
									Assert( pBoundCapsule );
									pBoundCapsule->SetCapsuleSize( g_customRadius, g_customLength );

									pBoundPart = pCustomBound->GetBound(g_capsuleRightT);
									Assert( pBoundPart );
									Assert( pBoundPart->GetType() == phBound::CAPSULE );

									pBoundCapsule = (phBoundCapsule*)pBoundPart;
									Assert( pBoundCapsule );
									pBoundCapsule->SetCapsuleSize( g_customRadius, g_customLength );

									g_pCharClothController = NULL;
									g_customLength = -1.0f;
									g_customRadius = -1.0f;
								}
							}

							phVerletCloth* verletCloth = pCharClothController->GetCloth(0);
							Assert( verletCloth );
				
							verletCloth->DetachVirtualBound();

							verletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, false);
							verletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, false);
						} // if( pPed->m_CClothController[i] )
					} // for( int i = 0; i < PV_MAX_COMP; ++i )

					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth, false );
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePoseCharacterCloth, false );
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePackageCharacterCloth, false );
					
				} // CPed* pPed = static_cast<CPed*>(GetGameEntity());
			} // if( GetGameEntity() && GetGameEntity()->GetIsTypePed() )
		}
		break;

	case CutSceneCustomEvents::CUTSCENE_CUSTOM_CLOTH_EVENT:
		{
			//cutsceneModelEntityDebugf("CCutsceneAnimatedActorEntity::DispatchEvent: CUTSCENE_CUSTOM_CLOTH_EVENT");			
			const CutSceneCustomEvents::ICutsceneCustomCustomEventArgs* pObjectVar = static_cast<const CutSceneCustomEvents::ICutsceneCustomCustomEventArgs*>(pEventArgs); 
			if( GetGameEntity() && GetGameEntity()->GetIsTypePed() && pObjectVar )
			{
				clothDebugf1("CUTSCENE_CUSTOM_CLOTH_EVENT: eventType = %d", pObjectVar->GetEventType() );

				CPed* pPed = static_cast<CPed*>(GetGameEntity());
				if( pPed )
				{
					const int eventType = pObjectVar->GetEventType();
					AssertMsg( (eventType > 0 && eventType < CCEVENT_TYPE_MAX), "The event type value is not correct" );

					if( eventType == CCEVENT_TYPE_SET_SKIN_ON_ONCREATE )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth, true );
					}
					else if( eventType == CCEVENT_TYPE_SET_SKIN_OFF_ONCREATE )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth, false );
					}
					else if( eventType == CCEVENT_TYPE_QUEUE_POSE_ON || eventType == CCEVENT_TYPE_SET_POSE )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePoseCharacterCloth, true );
						pPed->QueueClothPoseIndex( (u8)pObjectVar->GetCapsuleLength() );
					}
					else if( eventType == CCEVENT_TYPE_QUEUE_POSE_OFF )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePoseCharacterCloth, false );
					}
					else if( eventType == CCEVENT_TYPE_SET_PACKAGE_INDEX )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePackageCharacterCloth, true );
						pPed->QueueClothPackageIndex( (u8)pObjectVar->GetCapsuleLength() );
					}
					else if( eventType == CCEVENT_TYPE_SET_PRONE_ON )
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth, true );
					}

					for( int i = 0; i < PV_MAX_COMP; ++i )
					{
						characterClothController* pCharClothController = pPed->m_CClothController[i];
						if( pCharClothController )
						{
							phVerletCloth* verletCloth = pCharClothController->GetCloth(0);
							Assert( verletCloth );

	#if RECORDING_VERTS
							if( PFD_DebugRecords.WillDraw() 
								&& eventType != CCEVENT_TYPE_ATTACH_COLLISION 
								&& eventType != CCEVENT_TYPE_SET_PACKAGE_INDEX 
								&& eventType != CCEVENT_TYPE_SET_POSE
								)
							{
								recordCustomClothEvent rec;
								rec.m_Text = "CUTSCENE_CUSTOM_CLOTH_EVENT";								
								rec.m_ClothControllerName = pPed->m_CClothController[i]->GetName();
								rec.m_EventType = eventType;
								//rec.m_Frames = 60;
								g_DbgRecordCustomClothEvents.Push( rec );
							}
	#endif


// TODO: add debug text showing warning on the screen if bound exists already ?
							if( eventType == CCEVENT_TYPE_ATTACH_COLLISION && !verletCloth->m_VirtualBound )
							{
								Vector3 vPos = VEC3_ZERO; 
								vPos.x = pObjectVar->GetPositionX();
								vPos.y = pObjectVar->GetPositionY();
								vPos.z = pObjectVar->GetPositionZ();

								Vector3 vRot = VEC3_ZERO; 
								vRot.x = pObjectVar->GetRotationX();
								vRot.y = pObjectVar->GetRotationY();
								vRot.z = pObjectVar->GetRotationZ();

								Mat34V boundMat;
								Mat34VFromEulersXYZ( boundMat, VECTOR3_TO_VEC3V(vRot) );
								boundMat.SetCol3( VECTOR3_TO_VEC3V(vPos) );

								float fCapsuleLen = pObjectVar->GetCapsuleLength();
								float fCapsuleRad = pObjectVar->GetCapsuleRadius();								

								verletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
								verletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, true);
								
								verletCloth->CreateVirtualBound( 1, &identityMat );
								verletCloth->AttachVirtualBoundCapsule( fCapsuleRad, fCapsuleLen, boundMat, 0 );

	#if RECORDING_VERTS
								if( PFD_DebugRecords.WillDraw() )
								{
									recordCustomClothEvent rec;
									rec.m_Text = "CUTSCENE_CUSTOM_CLOTH_EVENT";								
									rec.m_ClothControllerName = pPed->m_CClothController[i]->GetName();
									rec.m_EventType = eventType;
									rec.m_Position = VECTOR3_TO_VEC3V(vPos);
									rec.m_Rotation = VECTOR3_TO_VEC3V(vRot);
									rec.m_CapsuleLength = fCapsuleLen;
									rec.m_CapsuleRadius = fCapsuleRad;
									//rec.m_Frames = 60;
									g_DbgRecordCustomClothEvents.Push( rec );
								}
	#endif

							}
							else if( eventType == CCEVENT_TYPE_DETACH_COLLISION )
							{								
								verletCloth->DetachVirtualBound();

								verletCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, false);
								verletCloth->SetFlag(phVerletCloth::FLAG_IGNORE_OFFSET, false);
							}
							else if( eventType == CCEVENT_TYPE_SET_SKIN_ON )
							{
								pCharClothController->SetFlag( characterClothController::enIsForceSkin, true);
							}
							else if( eventType == CCEVENT_TYPE_SET_SKIN_OFF )
							{
								pCharClothController->SetFlag( characterClothController::enIsForceSkin, false);
							}
							else if( eventType == CCEVENT_TYPE_FORCE_PIN )
							{
								pCharClothController->SetForcePin(1);
							}
							else if( eventType == CCEVENT_TYPE_SET_PACKAGE_INDEX )
							{
								pCharClothController->SetPackageIndex( (u8)pObjectVar->GetCapsuleLength() );

								pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePackageCharacterCloth, false );
	#if RECORDING_VERTS
								if( PFD_DebugRecords.WillDraw() )
								{
									recordCustomClothEvent rec;
									rec.m_Text = "CUTSCENE_CUSTOM_CLOTH_EVENT";								
									rec.m_ClothControllerName = pPed->m_CClothController[i]->GetName();
									rec.m_EventType = eventType;
									rec.m_CapsuleLength = pObjectVar->GetCapsuleLength();
									//rec.m_Frames = 60;
									g_DbgRecordCustomClothEvents.Push( rec );
								}
	#endif

							}
							else if( eventType == CCEVENT_TYPE_SET_PRONE_ON )
							{
								pCharClothController->SetFlag( characterClothController::enIsProneFlipped, true );
								pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth, false );
							}
							else if( eventType == CCEVENT_TYPE_SET_PRONE_OFF )
							{
								pCharClothController->SetFlag( characterClothController::enIsProne, false );
								pCharClothController->SetFlag( characterClothController::enIsProneFlipped, false );
							}
							else if( eventType == CCEVENT_TYPE_SET_PIN_RADIUS_THRESHOLD )
							{
								pCharClothController->SetPinRadiusSetThreshold( pObjectVar->GetCapsuleLength() );
							}
							else if( eventType == CCEVENT_TYPE_SET_POSE )
							{
// NOTE: if we set a pose then don't skin
								pCharClothController->SetFlag( characterClothController::enIsForceSkin, false );
								pCharClothController->SetForcePin( 0 );

								characterCloth* pCharCloth = (characterCloth*)pCharClothController->GetOwner();
								Assert( pCharCloth );
								pCharCloth->SetPose( (u8)pObjectVar->GetCapsuleLength() );

								pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePoseCharacterCloth, false );

	#if RECORDING_VERTS
								if( PFD_DebugRecords.WillDraw() )
								{
									recordCustomClothEvent rec;
									rec.m_Text = "CUTSCENE_CUSTOM_CLOTH_EVENT";								
									rec.m_ClothControllerName = pPed->m_CClothController[i]->GetName();
									rec.m_EventType = eventType;
									rec.m_CapsuleLength = pObjectVar->GetCapsuleLength();									
									g_DbgRecordCustomClothEvents.Push( rec );
								}
	#endif
							}
							else if( eventType == CCEVENT_TYPE_CUSTOM_1 && i == PV_COMP_UPPR )
							{
// TODO: hacky way to increase the raduis on the collision bounds from cutscene
								characterCloth* pCharCloth = (characterCloth*)pCharClothController->GetOwner();
								Assert( pCharCloth );
								const phBoundComposite* pCustomBound = pCharCloth->GetBoundComposite();
								Assert( pCustomBound );
								if( pCustomBound )
								{
									phBound* pBoundPart = pCustomBound->GetBound(g_capsuleLeftT);
									Assert( pBoundPart );
									Assert( pBoundPart->GetType() == phBound::CAPSULE );

									phBoundCapsule* pBoundCapsule = (phBoundCapsule*)pBoundPart;
									Assert( pBoundCapsule );
									pBoundCapsule->SetCapsuleSize( pObjectVar->GetCapsuleRadius(), pObjectVar->GetCapsuleLength() );
									
									pBoundPart = pCustomBound->GetBound(g_capsuleRightT);
									Assert( pBoundPart );
									Assert( pBoundPart->GetType() == phBound::CAPSULE );

									pBoundCapsule = (phBoundCapsule*)pBoundPart;
									Assert( pBoundCapsule );
									pBoundCapsule->SetCapsuleSize( pObjectVar->GetCapsuleRadius(), pObjectVar->GetCapsuleLength() );

									Assert( !g_pCharClothController );
									g_pCharClothController = pCharClothController;
									g_customLength = pObjectVar->GetCapsuleLength();
									g_customRadius = pObjectVar->GetCapsuleRadius();
								}
							}
						}
					} // for
					
				} // if( pPed )
			} // if ( GetGameEntity ...
		}
		break;
	case CUTSCENE_STOP_EVENT:
		{
	
		}
		break;

	}

	//this is here to dispatch debug draw event for this entity.
#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif 

	CCutsceneAnimatedModelEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedActorEntity::PlayClip (CutSceneManager* pCutManager,  const crClip* pClip, float fEventTime, const strStreamingObjectName UNUSED_PARAM(pAnimDict))
{
	//Assertf(CutSceneManager::GetInstance()->GetAssetManager()->HaveAllVariationsLoaded(PED_VARIATION_TYPE), "Variations not yet loaded");
	//CutSceneManager::GetInstance()->GetAssetManager()->DumpAllVariationsLoaded(m_pCutfObject->GetObjectId());

	if(GetGameEntity())
	{
		if(!m_pCutSceneTask)
		{
			GetGameEntity()->SetPedResetFlag( CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded, true );
			
			// Make sure we don't timeslice the ai / animation this frame.
			GetGameEntity()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			GetGameEntity()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
			
			if(GetGameEntity()->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION))
			{
				GetGameEntity()->UpdateFixedWaitingForCollision(false);
			}

			if(GetGameEntity()->GetPedIntelligence()->GetTaskEventResponse())
			{
				GetGameEntity()->GetPedIntelligence()->FlushImmediately(true);
			}

			m_pCutSceneTask = CreateCutsceneTask(pCutManager, pClip, fEventTime);

			m_pCutSceneTask->SetPreserveHairScale(m_OptionFlags.IsFlagSet(CEO_PRESERVE_HAIR_SCALE));
			m_pCutSceneTask->SetInstantHairScaleUpdate(m_OptionFlags.IsFlagSet(CEO_INSTANT_HAIR_SCALE_SETUP));
			m_pCutSceneTask->SetOriginalHairScale(m_fOriginalHairScale);

            if(GetGameEntity()->IsNetworkClone())
            {
                GetGameEntity()->GetPedIntelligence()->AddLocalCloneTask(m_pCutSceneTask, PED_TASK_PRIORITY_PRIMARY);
            }
            else
            {
			    GetGameEntity()->GetPedIntelligence()->AddTaskAtPriority(m_pCutSceneTask, PED_TASK_PRIORITY_PRIMARY, TRUE); 
            }
			
			GetGameEntity()->SetFixedPhysics(false); 
		}
		else
		{
			if(cutsceneVerifyf(m_pCutSceneTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE, "Trying to set the clip on a CTaskCutscene, but entity %s is not running a cutscene task", m_pCutfObject->GetDisplayName().c_str()))
			{	
				Matrix34 SceneMat; 
				pCutManager->GetSceneOrientationMatrix(SceneMat);
				m_pCutSceneTask->SetClip(pClip,SceneMat, fEventTime); 
			}
		}
	}
#if __DEV	
	if(GetGameEntity() && m_pCutSceneTask && m_pCutSceneTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE)
	{
		//THIS CURRENTLY DOES NOTHING TO FILTER ANY ANIMS BECAUSE THERE IS NO FILTER TO REMOVE ALL ANIM EXCEPT FACIAL
		UpdateWithFaceViewer(pCutManager,  m_pCutSceneTask);
	}
#endif

	SetAnimPlayBackEventTime(fEventTime); 
}    

/////////////////////////////////////////////////////////////////////////
//Gets the position of an actors bone (used for face zoom but could use for hands etc 

bool CCutsceneAnimatedActorEntity::GetActorBonePosition(eAnimBoneTag ePedBone, Matrix34 &mMat) const
{	
	const crSkeleton* pSkel = NULL; 

	s32 nBoneIndex = -1; 
	
		if (GetGameEntity())
		{
			pSkel = GetGameEntity()->GetSkeleton();
			nBoneIndex = GetGameEntity()->GetBoneIndexFromBoneTag(ePedBone);
		}

	if (pSkel && nBoneIndex!= -1)
	{
		pSkel->GetGlobalMtx(nBoneIndex, RC_MAT34V(mMat)); 

		return true; 
	}

	return false; 
}

/////////////////////////////////////////////////////////////////////////
//set the variation on an actor

void CCutsceneAnimatedActorEntity::SetCurrentActorVariation(CPed* pPed, u32 iComponent, s32 iDrawable, s32 iTexture)
{
	if(IsBlockingVariationStreamingAndApplication())
	{	
		return; 
	}

	//setting the variation on a ped
	if (pPed && pPed->GetPedModelInfo() && pPed->GetPedModelInfo()->GetVarInfo())
	{
		if(iComponent < PV_MAX_COMP)
		{
			if(pPed->IsVariationInRange(static_cast<ePedVarComp>(iComponent),iDrawable,iTexture))
			{
				//if(!pPed->HasVariation(static_cast<ePedVarComp>(iComponent), iDrawable, iTexture))
				{
					if (pPed->GetPedModelInfo()->GetIsStreamedGfx())
					{
						// Check the variation is loaded
						eVariationStreamingStatus variationStreamingStatus = CutSceneManager::GetInstance()->GetAssetManager()->GetVariationLoadingState(iComponent,  iDrawable,  iTexture, m_ModelNameHash, m_pCutfObject->GetObjectId());
						if( variationStreamingStatus == LOADING_VARIATION)
						{								
							cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) -  Variation is still loading (Component: %d (%s), Drawable: %d, Texture: %d) ", 
								pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), iDrawable, iTexture );
						}
						else if ( variationStreamingStatus == NO_REQUEST_FOR_VARIATION )
						{
							cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Variation has no request (Component: %d (%s), Drawable: %d, Texture: %d) ", 
								pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), iDrawable, iTexture );
						}
					}

#if !__NO_OUTPUT
					bool ReactivateLogging = false; 
					if(pPed->m_LogVariationCalls)
					{
						pPed->m_LogVariationCalls = false; 
						ReactivateLogging = true; 
					}
#endif //!__NO_OUTPUT
					pPed->SetVariation(static_cast<ePedVarComp>(iComponent), iDrawable, 0, iTexture, 0, 0, true);
#if !__NO_OUTPUT
					if(ReactivateLogging)
					{
						pPed->m_LogVariationCalls = true;
					}
#endif //!__NO_OUTPUT

					cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Actual SetVariation  - (Component: %d (%s), Drawable: %d/%d, Texture: %d/%d)", 
						pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent),
						iDrawable, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(iComponent)), 
						iTexture, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(iComponent),iDrawable));

				}
				/*else
				{
					cutsceneModelEntityDebugf("SetCurrentActorVariation (%s, %p) - Already has variation - (Component: %d (%s), Drawable: %d/%d, Texture: %d/%d)", 
						pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent),
						iDrawable, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(iComponent)), 
						iTexture, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(iComponent),iDrawable));
				}*/
			}
			else
			{
				cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Variation not in range - (Component: %d (%s), Drawable: %d/%d, Texture: %d/%d) (using default instead) ", 
					pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent),
					iDrawable, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(iComponent)), 
					iTexture, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(iComponent),iDrawable));

				// Check the default variation is loaded
				eVariationStreamingStatus variationStreamingStatus = CutSceneManager::GetInstance()->GetAssetManager()->GetVariationLoadingState(iComponent,  0,  0, m_ModelNameHash, m_pCutfObject->GetObjectId());
				if( variationStreamingStatus == LOADING_VARIATION)
				{								
					cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) -  Variation is still loading (Component: %d (%s), Drawable: %d, Texture: %d) ", 
						pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), 0, 0 );
				}
				else if ( variationStreamingStatus == NO_REQUEST_FOR_VARIATION )
				{
					cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Variation has no request (Component: %d (%s), Drawable: %d, Texture: %d) ", 
						pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), 0, 0 );
				}

#if !__NO_OUTPUT
				bool ReactivateLogging = false; 
				if(pPed->m_LogVariationCalls)
				{
					pPed->m_LogVariationCalls = false; 
					ReactivateLogging = true; 
				}
#endif //!__NO_OUTPUT
				pPed->SetVariation(static_cast<ePedVarComp>(iComponent), 0, 0, 0, 0, 0, true);
#if !__NO_OUTPUT
				if(ReactivateLogging)
				{
					pPed->m_LogVariationCalls = true;
				}
#endif //!__NO_OUTPUT

				cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Actual SetVariation  - (Component: %d (%s), Drawable: %d/%d, Texture: %d/%d)", 
					pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(0),
					0, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(0)), 
					0, pPed->GetPedModelInfo()->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(0),0));
			}
		}
		else
		{
			int iPropComponent = iComponent - PV_MAX_COMP; 
			if(iDrawable != -1)
			{
				CPedPropsMgr::SetPedProp(pPed, (eAnchorPoints)iPropComponent , iDrawable, iTexture, ANCHOR_NONE, NULL, NULL);
				cutsceneModelEntityDebugf3("SetCurrentActorVariation (%s, %p) - Actual SetPedProp  - (Component: %d (%s), Drawable: %d, Texture: %d)", pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent), iDrawable, iTexture);
			}
			else
			{
				CPedPropsMgr::ClearPedProp(pPed, (eAnchorPoints)iPropComponent ); 
				if(iComponent == PV_COMP_HEAD && pPed->GetHelmetComponent())
				{
					pPed->GetHelmetComponent()->DisableHelmet(false);
				}
				cutsceneModelEntityDebugf3("SetActorCurrentVariation (%s, %p) - Actual ClearPedProp  - (Component: %d (%s))", pPed->GetModelName(), pPed, iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent));
			}
		}
	}
	else
	{
		cutsceneModelEntityDebugf3("SetCurrentActorVariation - Ped not loaded (or malformed). Failed to set variation on ped but storing the variation to apply once the ped is loaded.  - (Component: %d (%s), Drawable: %d, Texture: %d)", 
			iComponent, CPedVariationData::GetVarOrPropSlotName(iComponent),iDrawable, iTexture);
	}

	if ((iDrawable>-1 && iTexture>-1) || (iComponent>=PV_MAX_COMP && iDrawable>-2))
	{
		StoreActorVariationData( iComponent,  iDrawable, iTexture, m_sCurrentActorVariationData);
	}
}

void CCutsceneAnimatedActorEntity::SetScriptActorVariationData( u32 iComponent, s32 iDrawable, s32 iTexture)
{
	if((iDrawable>-1 && iTexture>-1) || (iComponent>=PV_MAX_COMP && iDrawable>-2))
	{
		// Store the variations set by script in their own array
		// These will be used in favour of cutscene variations on the start frame
		StoreActorVariationData(iComponent,  iDrawable, iTexture, m_sScriptActorVariationData);

		// Also copy the variations into the current array 
		// Peds not loaded before the scene dotn get ComputeInitialVariations called on them
		StoreActorVariationData(iComponent,  iDrawable, iTexture, m_sCurrentActorVariationData);
	}
}
/////////////////////////////////////////////////////////////////////////

CCutsceneAnimatedActorEntity::~CCutsceneAnimatedActorEntity()
{
	//check that the script registered entity is being picked up by the script
	if(m_RegisteredEntityFromScript.pEnt != NULL)
	{
		if(m_RegisteredEntityFromScript.pEnt->GetIsTypePed())
		{	
			CPed* pPed = (CPed*)m_RegisteredEntityFromScript.pEnt.Get(); 

			if(!pPed->IsLocalPlayer())
			{
				if(m_RegisteredEntityFromScript.SceneNameHash > 0 && m_RegisteredEntityFromScript.bCreatedForScript)
				{
					const CScriptEntityExtension* pExtension = m_RegisteredEntityFromScript.pEnt->GetExtension<CScriptEntityExtension>();
					
					if(!CutSceneManager::GetInstance()->ShouldDeleteAllRegisteredEntites())
					{
						if(!IsRegisteredGameEntityUnderScriptControl())
						{
							cutsceneAssertf(pExtension, "Deleting a cutscene entity (\"%s\" %u) scene handle (\"%s\" %u) that was registered with CU_CREATE_AND_ANIMATE_NEW_SCRIPT_ENTITY use GET_ENTITY_INDEX_OF_REGISTERED_ENTITY to make sure it is pickup up by the script", 
								m_RegisteredEntityFromScript.ModelNameHash.TryGetCStr(), 
								m_RegisteredEntityFromScript.ModelNameHash.GetHash(), 
								m_RegisteredEntityFromScript.SceneNameHash.TryGetCStr(), 
								m_RegisteredEntityFromScript.SceneNameHash.GetHash());
						}
					}
					
					if(!pExtension)
					{
						m_RegisteredEntityFromScript.bDeleteBeforeEnd = true; 
					}
				}
			}
		}
	}

	if (GetGameEntity())
	{		
		bool CanDelete = m_RegisteredEntityFromScript.pEnt != GetGameEntity(); 

		if(CanDelete|| m_RegisteredEntityFromScript.bDeleteBeforeEnd)
		{
			GetGameEntity()->PopTypeSet(POPTYPE_RANDOM_AMBIENT); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Vehicle
///////////////////////////////////////////////////////////////////////////////////////////////////

CCutsceneAnimatedVehicleEntity::CCutsceneAnimatedVehicleEntity(const cutfObject* pObject)
:CCutsceneAnimatedModelEntity(pObject)
{
	m_iAnimDict = -1; 

#if __BANK
	InitaliseVehicleExtraList(); 
#endif
}

/////////////////////////////////////////////////////////////////////////

CCutsceneAnimatedVehicleEntity::~CCutsceneAnimatedVehicleEntity()
{
	//check that the script registered entity is being picked up by the script
	if(m_RegisteredEntityFromScript.SceneNameHash > 0 && m_RegisteredEntityFromScript.bCreatedForScript)
	{
		if(m_RegisteredEntityFromScript.pEnt != NULL)
		{
			const CScriptEntityExtension* pExtension = m_RegisteredEntityFromScript.pEnt->GetExtension<CScriptEntityExtension>();

			if(!CutSceneManager::GetInstance()->ShouldDeleteAllRegisteredEntites())
			{
				if(!IsRegisteredGameEntityUnderScriptControl())
				{
					cutsceneAssertf(pExtension, "Deleting a cutscene entity (\"%s\" %u) scene handle (\"%s\" %u) that was registered with CU_CREATE_AND_ANIMATE_NEW_SCRIPT_ENTITY use GET_ENTITY_INDEX_OF_REGISTERED_ENTITY to make sure it is pickup up by the script", 
						m_RegisteredEntityFromScript.ModelNameHash.TryGetCStr(), 
						m_RegisteredEntityFromScript.ModelNameHash.GetHash(), 
						m_RegisteredEntityFromScript.SceneNameHash.TryGetCStr(), 
						m_RegisteredEntityFromScript.SceneNameHash.GetHash());
				}
			}

			if(!pExtension)
			{
				m_RegisteredEntityFromScript.bDeleteBeforeEnd = true; 
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedVehicleEntity::DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
		case CUTSCENE_UPDATE_EVENT:
		case CUTSCENE_PAUSE_EVENT:
		{
			if (iEventId==CUTSCENE_UPDATE_EVENT && m_bStoppedCalled)
			{
				CreateRepositionOnlyGameEntityWhenModelLoaded(pManager, pObject, true, true );
				SetGameEntityReadyForGame(pManager);
			}

			UpdateCutSceneTask(pManager); 

			if (GetGameEntity() && !m_bIsReadyForGame)
			{
				CVehicleAILodManager::DisableTimeslicingImmediately(*GetGameEntity());
			}
		}
		break;

		case CUTSCENE_SET_VARIATION_EVENT:
			  {
				  if (m_bIsReadyForGame)
				  {
					  return;
				  }

				  cutsceneModelEntityDebugf3("CCutsceneAnimatedVehicleEntity:CUTSCENE_SET_VARIATION_EVENT");

				  cutsceneAssertf(pEventArgs, "Cutscene content bug! CUTSCENE_SET_VARIATION_EVENT (object name=%s type=%s)(event id=%i) is corrupted and needs to be recreated, variation will not be applied!", pObject->GetDisplayName().c_str(), pObject->GetTypeName(), iEventId);
				  if (pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_VARIATION_EVENT_ARGS_TYPE)
				  {
					  const cutfVehicleVariationEventArgs* pVariationEventFlag = static_cast<const cutfVehicleVariationEventArgs *>( pEventArgs );

					  SetVariation(pVariationEventFlag->GetBodyColour() , 
						  pVariationEventFlag->GetSecondaryBodyColour(), 
						  pVariationEventFlag->GetSpecularBodyColour(),
						  pVariationEventFlag->GetWheelTrimColour(), 
						  pVariationEventFlag->GetBodyColour5(),
						  pVariationEventFlag->GetBodyColour6(),
						  pVariationEventFlag->GetLiveryId(), 
						  pVariationEventFlag->GetLivery2Id(), 
						  pVariationEventFlag->GetDirtLevel()  ); 
				  
					  cutsceneDebugf1("%s CCutsceneAnimatedVehicleEntity:CUTSCENE_SET_VARIATION_EVENT BodyColor: %d, Second BodyColor: %d, SpecBody: %d, Wheel Trim Col: %d, BodyColor5: %d, BodyColor6: %d, Livery id: %d, Livery2 id: %d, Dirt Lev: %f "
						  , pObject->GetDisplayName().c_str(), pVariationEventFlag->GetBodyColour(), 
						  pVariationEventFlag->GetSecondaryBodyColour(), 
						  pVariationEventFlag->GetSpecularBodyColour(), 
						  pVariationEventFlag->GetWheelTrimColour(),
						  pVariationEventFlag->GetBodyColour5(),
						  pVariationEventFlag->GetBodyColour6(),
						  pVariationEventFlag->GetLiveryId(), 
						  pVariationEventFlag->GetLivery2Id(), 
						  pVariationEventFlag->GetDirtLevel()); 
				  }
					
				  if (pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_EXTRA_EVENT_ARGS_TYPE)
				  {
					 const cutfVehicleExtraEventArgs* pExtraEventFlag = static_cast<const cutfVehicleExtraEventArgs *>( pEventArgs );

					if(GetGameEntity())
					{
						SetGameVehicleExtra(pExtraEventFlag->GetBoneIdList()); 
					}
				  }
			  }
			break; 

		case CutSceneCustomEvents::CUTSCENE_SET_VEHICLE_DAMAGE_EVENT:
			{
				if (m_bIsReadyForGame)
				{
					return;
				}

				 cutsceneDebugf1("%s CCutsceneAnimatedVehicleEntity:CUTSCENE_SET_VEHICLE_DAMAGE_EVENT", pObject->GetDisplayName().c_str()); 
				if (pEventArgs && pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
				{
					const cutfNameEventArgs* pObjectVar = static_cast<const cutfNameEventArgs*>(pEventArgs); 

					Vector3 vDamage = VEC3_ZERO; 
					float fDamage = 0.0f; 
					float fDeform = 0.0f; 

					if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.x"))
					{
						vDamage.x = pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.x")->FindFloatValue();  
					}
					if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.y"))
					{
						vDamage.y = pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.y")->FindFloatValue();  
					}
					
					if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.z"))
					{
						vDamage.z = pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.z")->FindFloatValue();  
					}
					
					if(pObjectVar->GetAttributeList().FindAttributeAnyCase("fDamage"))
					{
						fDamage = pObjectVar->GetAttributeList().FindAttributeAnyCase("fDamage")->FindFloatValue(); 
					}
					 
					if(pObjectVar->GetAttributeList().FindAttributeAnyCase("fDeform"))
					{
						fDeform = pObjectVar->GetAttributeList().FindAttributeAnyCase("fDeform")->FindFloatValue(); 
					}

					//cutsceneDebugf1("%s CCutsceneAnimatedVehicleEntity:CUTSCENE_SET_VEHICLE_DAMAGE_EVENT pos: x:%f, y:%f, z:%f, ", pObject->GetDisplayName(), )
					SetVehicleDamage(vDamage,fDamage,fDeform, true);
				  }
			  }
			break; 
	}
	CCutsceneAnimatedModelEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
}

void CCutsceneAnimatedVehicleEntity::ForceUpdateAi(cutsManager* pManager)
{
	if(pManager)
	{
		CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*> (pManager);

		if(pCutSceneManager->GetShutDownMode() != SHUTDOWN_SESSION)
		{
			CVehicle* pVeh = GetGameEntity();
			if (pVeh && pVeh->GetIntelligence() && pVeh->GetIntelligence()->GetTaskManager() && GetCutsceneTaskForEntity())
			{
				// TODO - Update the vehicle a.i. properly if we find we need to cross blend between vehicle anims, etc
				// For now lets just get rid of the cutscene task.
				pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedVehicleEntity::PlayClip (CutSceneManager* pCutManager, const crClip* pClip, float fEventTime,  const strStreamingObjectName UNUSED_PARAM(pAnimDict))
{
	if (GetGameEntity())
	{	
		if(!m_pCutSceneTask)
		{
			// Ready the vehicle for animation
			GetGameEntity()->SetFixedPhysics(true);
			GetGameEntity()->m_nVehicleFlags.bAnimateWheels = true; 
			GetGameEntity()->m_nVehicleFlags.bAnimatePropellers = true; 
			GetGameEntity()->m_nVehicleFlags.bAnimateJoints = true;
			CVehicleAILodManager::DisableTimeslicingImmediately(*GetGameEntity());
			GetGameEntity()->InitAnimLazy();
		
			GetGameEntity()->SetDriveMusclesToAnimation(true);
			m_pCutSceneTask = CreateCutsceneTask(pCutManager, pClip, fEventTime); 
			GetGameEntity()->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(m_pCutSceneTask, VEHICLE_TASK_SECONDARY_ANIM); 
		}
		else
		{
			if(cutsceneVerifyf(m_pCutSceneTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE, "Trying to set the clip on a CTaskCutscene, but entity %s is not running a cutscene task", m_pCutfObject->GetDisplayName().c_str()))
			{
				Matrix34 SceneMat; 
				pCutManager->GetSceneOrientationMatrix(SceneMat);
				m_pCutSceneTask->SetClip(pClip,SceneMat, fEventTime); 
			}
		}
	}

	SetAnimPlayBackEventTime(fEventTime);
}

/////////////////////////////////////////////////////////////////////////
//Restore a vehicle to a previous setting

void CCutsceneAnimatedVehicleEntity::RestoreEntityProperties(CEntity* UNUSED_PARAM(pEnt))
{
	SetVariation(m_sVehicleData.iBodyColour, m_sVehicleData.iSecondColour, m_sVehicleData.iSpecColour, m_sVehicleData.iWheelColour, m_sVehicleData.iBodyColour5, m_sVehicleData.iBodyColour6,
				m_sVehicleData.iLiveryId, m_sVehicleData.iLivery2Id, m_sVehicleData.fDirt);

	if(m_sVehicleData.HiddenBones.GetCount() > 0 )
	{
		if(GetGameEntity())
		{
			SetGameVehicleExtra(m_sVehicleData.HiddenBones); 
		}
	}

}

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedVehicleEntity::SetVariation(int iBodyColour, int iSecondColour, int iSpecColour, int iWheelColour, int iBodyColour5, int iBodyColour6, int LiveryId, int Livery2Id, float fDirt)
{
	//store the variation for later use if we delete and need to recreate the vehicle
	m_sVehicleData.iBodyColour = iBodyColour; 
	m_sVehicleData.iSecondColour = iSecondColour; 
	m_sVehicleData.iSpecColour = iSpecColour; 
	m_sVehicleData.iWheelColour = iWheelColour; 
	m_sVehicleData.iBodyColour5 = iBodyColour5;
	m_sVehicleData.iBodyColour6 = iBodyColour6;
	m_sVehicleData.iLiveryId = LiveryId;
	m_sVehicleData.iLivery2Id = Livery2Id;
	m_sVehicleData.fDirt = fDirt; 

	if(GetGameEntity())
	{
		GetGameEntity()->SetBodyColour1( static_cast<u8>(iBodyColour) );
		GetGameEntity()->SetBodyColour2( static_cast<u8>(iSecondColour) ); 
		GetGameEntity()->SetBodyColour3( static_cast<u8>(iSpecColour));
		GetGameEntity()->SetBodyColour4( static_cast<u8>(iWheelColour) ); 
		GetGameEntity()->SetBodyColour5( static_cast<u8>(iBodyColour5) ); 
		GetGameEntity()->SetBodyColour6( static_cast<u8>(iBodyColour6) ); 
		GetGameEntity()->SetLiveryId(LiveryId); 
		GetGameEntity()->SetLivery2Id(Livery2Id); 
		GetGameEntity()->SetBodyDirtLevel(fDirt); 
		GetGameEntity()->UpdateBodyColourRemapping(); 
	}
}

/////////////////////////////////////////////////////////////////////////
//Game side vehicles have a hierarchy that describes various components, need to map our bone ids to its hierachy

void CCutsceneAnimatedVehicleEntity::SetGameVehicleExtra(const atArray<s32>& Bones) 
{
	//store the variation for later use if we delete and need to recreate the vehicle for streaming.
	m_sVehicleData.HiddenBones.Reset();
	for (int i =0; i < Bones.GetCount(); i++)
	{
		m_sVehicleData.HiddenBones.PushAndGrow(Bones[i]);
	}

	//m_sVehicleData.HiddenBones.CopyFrom(Bones, Bones.GetCount()); 
	
	//have reserved the first array to be 0 if extras have been reactivated
	if(Bones.GetCount()> 0 && Bones[0] != 0)
	{	
		//Reactivate any previous turned off bones
		for(int i=0; i < 10; i ++)
		{
			int VehicleHeirachy = i + VEH_EXTRA_1; 
			bool bReactivateExtra = true; 

			if (!GetGameEntity()->GetIsExtraOn(((eHierarchyId)VehicleHeirachy))) //extra is off do we need ot turn it off or is it on the list. 
			{
				//look through our list of bones 
				for(int i = 0; i < Bones.GetCount(); i++ )
				{
					s32 iMappedNewBoneId = (Bones[i] - VEHICLE_HEIRACHY_MODIFIER) + (VEH_EXTRA_1 - 1); 
					//if its on our list already, leave it.
					if(VehicleHeirachy == iMappedNewBoneId  )
					{
						bReactivateExtra = false; 
						break;
					}
				}
				
				if (bReactivateExtra)
				{
					GetGameEntity()->TurnOffExtra(((eHierarchyId)VehicleHeirachy), false ); 
				}
			}
		}
		
		//Turn off any bones on our list
		for(int i = 0; i < Bones.GetCount(); i++ )
		{
			s32 iBoneId = Bones[i]; //store the bone id

			//remap to the heirchy- iboneids are u16hash of the bone name, starting at extra_1
			//VEHICLE_HEIRACHY_MODIFIER is the hash of extra_0 so we subtract to get 1, 2, 3 etc
			//and add to a zeroed VEH_EXTRA_X 
			int iMappedNewBoneId = (iBoneId - VEHICLE_HEIRACHY_MODIFIER) + (VEH_EXTRA_1 - 1); 
			if (GetGameEntity()->GetIsExtraOn(((eHierarchyId)iMappedNewBoneId)))
			{	
				GetGameEntity()->TurnOffExtra(((eHierarchyId)iMappedNewBoneId), true );
			}
		}
	}
	else
	{
		//Careful: reactivating vehicle extras actually repairs the whole vehicle
		for(int i=0; i < 10; i ++)
		{
			int VehicleHeirachy = i + VEH_EXTRA_1; 
			if (!GetGameEntity()->GetIsExtraOn(((eHierarchyId)VehicleHeirachy))) 
			{	
				GetGameEntity()->TurnOffExtra(((eHierarchyId)VehicleHeirachy), false);
			}
		}
	}
};

void CCutsceneAnimatedVehicleEntity::SetVehicleDamage(const Vector3& scrVecDamage, float damageAmount, float deformationScale, bool localDamage)
{
	CVehicle *pVehicle = GetGameEntity();

	if (pVehicle)
	{
		const Vector3 passedPos(scrVecDamage);
		Vector3 impactPos = passedPos;
		if( localDamage )
		{
			impactPos = pVehicle->TransformIntoWorldSpace(impactPos);
		}

		Vector3 impactImpulse = -passedPos;
		impactImpulse.Normalize();

		Vector3 normalizedImpulse = impactImpulse;

		impactImpulse *= damageAmount;
		if( localDamage )
		{
			impactImpulse = VEC3V_TO_VECTOR3(pVehicle->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(impactImpulse)));
		}

		// Deformation
		pVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact(impactImpulse*deformationScale, impactPos, NULL);

		// Damage
		pVehicle->GetVehicleDamage()->ApplyDamage(NULL,DAMAGE_TYPE_UNKNOWN, 0, damageAmount, impactPos, normalizedImpulse, -normalizedImpulse, 0);
	}
}

/////////////////////////////////////////////////////////////////////////

void CCutsceneAnimatedVehicleEntity::CreateGameEntity(strLocalIndex iModelIndex, bool CreateRepositionEntity, bool UNUSED_PARAM(bRequestVariations), bool UNUSED_PARAM(bApplyVariations))
{
	if(GetGameEntity() == NULL)
	{
		cutsceneModelEntityDebugf2("CreateGameEntity - Vehicle");

		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(iModelIndex));
		if(pModelInfo)
		{
			cutsceneAssertf(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "CUTSCENE ASSET (Assign to *Default Anim Cutscene*): Exported incorrectly, %s is not a VehicleModel type", pModelInfo->GetModelName()); 	
			if(pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				Matrix34 TempMat;

				TempMat.Identity();
				TempMat.d = m_vCreatePos; 

				const CControlledByInfo localAiControl(false, false);
				fwModelId modelId(iModelIndex);
				CVehicle* pVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_CUTSCENE, POPTYPE_MISSION, &TempMat, false);
				
				if (Verifyf(pVehicle, "trying to store a null ped pointer something has gone wrong with the registration"))
				{
					//store the vehicle pointer
					if(CreateRepositionEntity)
					{
						SetRepositionOnlyEntity(pVehicle);
					}
					else
					{
						SetGameEntity(pVehicle); 
					}

					if(pVehicle)
					{
						pVehicle->SetIsAbandoned();
						pVehicle->m_nPhysicalFlags.bNotDamagedByAnything = true;
					
						CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);
						pVehicle->SetFixedPhysics(true); 
						pVehicle->GetPortalTracker()->RequestRescanNextUpdate();

						pVehicle->GetPortalTracker()->SetIsCutsceneControlled(true);

						pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;

						if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
						{
							((CTrain*)pVehicle)->m_nTrainFlags.bIsCutsceneControlled = true;
						}
						m_bCreatedByCutscene = true; 

						//if (bApplyVariations)
						//{
							// Apply the current variations
							//RestoreEntityProperties(pVehicle);
						//}
					}
				}
			}
		}
	}
}

void CCutsceneAnimatedVehicleEntity::HideVehicleFragmentComponent()
{
	if(GetCutfObject() && GetGameEntity())
	{
		if(m_pCutfObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
		{
			const cutfModelObject* pModel = static_cast<const cutfModelObject*>(m_pCutfObject); 

			if(pModel->GetModelType() == CUTSCENE_VEHICLE_MODEL_TYPE)
			{
				const cutfVehicleModelObject* pModelObject = static_cast<const cutfVehicleModelObject*>(m_pCutfObject); 

				if(pModelObject->GetRemoveBoneNameList().GetCount() > 0)
				{
					for (int i = 0; i < pModelObject->GetRemoveBoneNameList().GetCount(); i++)
					{
						const crBoneData* pBoneData = GetGameEntity()->GetSkeletonData().FindBoneData(pModelObject->GetRemoveBoneNameList()[i].c_str()); 
						
						if(pBoneData)
						{
							int BoneIndex = pBoneData->GetIndex(); 

							CVehicleModelInfo* pModel = GetGameEntity()->GetVehicleModelInfo(); 

							if(pModel)
							{
								CVehicleStructure* pVehicleStruct = pModel->GetStructure(); 

								if(pVehicleStruct)
								{
									for(int j=0; j < VEH_NUM_NODES; j++)
									{
										if (pVehicleStruct->m_nBoneIndices[j] == BoneIndex)
										{
											eHierarchyId HeirarchyId = (eHierarchyId)j; 

											if(GetGameEntity()->HasFragmentComponent(HeirarchyId))
											{
												GetGameEntity()->RemoveFragmentComponent(HeirarchyId);
												cutsceneModelEntityDebugf2("HideVehicleFragmentComponent: Removing component %s from vehicle", pModelObject->GetRemoveBoneNameList()[i].c_str());
											}
											else
											{
												cutsceneModelEntityDebugf2("HideVehicleFragmentComponent: Couldn't remove component %s, it may have already been removed", pModelObject->GetRemoveBoneNameList()[i].c_str());
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}	
	}

}

void CCutsceneAnimatedVehicleEntity::SetGameEntityReadyForCutscene()
{
	if(GetGameEntity())
	{
#if !__NO_OUTPUT
		SetUpEntityForCallStackLogging(); 
#endif

		GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_CUTSCENE); 
		GetGameEntity()->SetFixedPhysics(true);
		GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything = true;
		GetGameEntity()->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
		GetGameEntity()->DisableCollision();
		GetGameEntity()->ResetSuspension(); 
		GetGameEntity()->SetForceHd(true); 
		CVehicleRecordingMgr::StopPlaybackRecordedCar(GetGameEntity());
		HideVehicleFragmentComponent(); 

		if (m_OptionFlags.IsFlagSet(CEO_IS_CASCADE_SHADOW_FOCUS_ENTITY_DURING_EXIT) && CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetCamEntity())
		{
			// Register this with the camera
			CCutSceneCameraEntity* pCam = const_cast<CCutSceneCameraEntity*>(CutSceneManager::GetInstance()->GetCamEntity());
			if (pCam)
			{
				pCam->SetCascadeShadowFocusEntityForSeamlessExit(GetGameEntity());
			}
		}

		NetworkInterface::CutsceneStartedOnEntity(*GetGameEntity());
	}
}

void CCutsceneAnimatedVehicleEntity::SetRepositionOnlyEntityReadyForCutscene()
{
	if(GetGameRepositionOnlyEntity())
	{
		cutsceneModelEntityDebugf2("SetRepositionOnlyEntityReadyForCutscene - Vehicle");
		GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = true;
		GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false); 
		cutsceneModelEntityDebugf2("CCutsceneAnimatedVehicleEntity::SetRepositionOnlyEntityReadyForCutscene: Hiding GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());

		NetworkInterface::CutsceneStartedOnEntity(*GetGameRepositionOnlyEntity());
	}
}

void CCutsceneAnimatedVehicleEntity::SetGameEntityReadyForGame(cutsManager* UNUSED_PARAM(pManager))
{
	if(!m_bIsReadyForGame)
	{
		if(GetGameEntity())
		{
			NetworkInterface::CutsceneFinishedOnEntity(*GetGameEntity());

#if !__NO_OUTPUT
			ResetEntityForCallStackLogging();
#endif

			GetGameEntity()->SetLodMultiplier(m_fOriginalLODMultiplier);	

			cutsceneModelEntityDebugf2("CCutsceneAnimatedVehicleEntity::SetGameEntityReadyForGame - Vehicle");
			if (GetGameEntity()->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				((CTrain*)GetGameEntity())->m_nTrainFlags.bIsCutsceneControlled = false;
			}

			bool canEnableCollision = true;
			if(NetworkInterface::IsGameInProgress() && GetGameEntity()->GetNetworkObject())
			{
				canEnableCollision = !SafeCast(CNetObjPhysical, GetGameEntity()->GetNetworkObject())->GetKeepCollisionDisabledAfterAnimScene();
			}

			if(canEnableCollision)
			{
				GetGameEntity()->EnableCollision();
			}

			GetGameEntity()->SetForceHd(false); 
			GetGameEntity()->SetFixedPhysics(false);
			GetGameEntity()->m_nVehicleFlags.bAnimateWheels = false; 
			GetGameEntity()->m_nVehicleFlags.bAnimatePropellers = false; 
			GetGameEntity()->m_nVehicleFlags.bAnimateJoints = false; 
			GetGameEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
			GetGameEntity()->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
			GetGameEntity()->SetDriveMusclesToAnimation(false);

			if(!GetGameEntity()->m_nVehicleFlags.bUseCutsceneWheelCompression)
			{
				GetGameEntity()->PlaceOnRoadAdjust();
			}

			if(GetGameEntity()->InheritsFromTrailer())
			{
				//Need to enforce constraint limits on trailers to avoid pops
				GetGameEntity()->ActivatePhysics();
			}

			bool CanDelete = m_RegisteredEntityFromScript.pEnt != GetGameEntity(); 

			if(CanDelete|| m_RegisteredEntityFromScript.bDeleteBeforeEnd)
			{
				GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_TEMP);
				GetGameEntity()->PopTypeSet(POPTYPE_RANDOM_AMBIENT); 
			}
			else
			{
				GetGameEntity()->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
				SetVisibility(GetGameEntity(), true); 
				cutsceneModelEntityDebugf3("CCutsceneAnimatedVehicleEntity::SetGameEntityReadyForGame1: Showing GetGameEntity (%s)", GetGameEntity()->GetModelName());
			}
		}
		
		if(GetGameRepositionOnlyEntity())
		{
			cutsceneModelEntityDebugf2("SetGameEntityReadyForGame - Vehicle (reposition only");
			GetGameRepositionOnlyEntity()->SetLodMultiplier(m_fOriginalLODMultiplier);	
			GetGameRepositionOnlyEntity()->SetHeading(m_CurrentHeading); 
			GetGameRepositionOnlyEntity()->SetPosition(m_CurrentPosition); 
			GetGameRepositionOnlyEntity()->SetFixedPhysics(false);
			GetGameRepositionOnlyEntity()->m_nPhysicalFlags.bNotDamagedByAnything = m_bWasInvincible;
			GetGameRepositionOnlyEntity()->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true); 
			cutsceneModelEntityDebugf2("CCutsceneAnimatedVehicleEntity::SetGameEntityReadyForGame2: Showing GetGameRepositionOnlyEntity (%s)", GetGameRepositionOnlyEntity()->GetModelName());
			GetGameRepositionOnlyEntity()->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
		}

		if (m_pCutSceneTask)
		{
			m_pCutSceneTask->SetExitNextUpdate();
		}

		m_bIsReadyForGame = true;
	}

}

void CCutsceneAnimatedVehicleEntity::SetupSuspension()
{
	for(int i = 0; i < GetGameEntity()->GetNumWheels(); i++)
	{
		CWheel *pWheel = GetGameEntity()->GetWheel(i);
		pWheel->UpdateSuspension(NULL, false);
	}

	if(GetGameEntity()->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsInactive(GetGameEntity()->GetCurrentPhysicsInst()->GetLevelIndex()))
	{
		fragInstGta * pVehicleFragInst = GetGameEntity()->GetVehicleFragInst();
		if(pVehicleFragInst && pVehicleFragInst->GetCacheEntry() && pVehicleFragInst->GetCacheEntry()->GetHierInst() &&
		   pVehicleFragInst->GetCacheEntry()->GetHierInst()->articulatedCollider)
		{
			GetGameEntity()->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetColliderMatrixFromInstance();
		}
	}
	GetGameEntity()->GetVehicleFragInst()->SyncSkeletonToArticulatedBody(true);
}

void CCutsceneAnimatedVehicleEntity::UpdateCutSceneTask(cutsManager* pManager)
{
	if(GetGameEntity() && GetGameEntity()->GetIntelligence())
	{
		aiTask* CurrentVehicleTask = GetGameEntity()->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetTask(VEHICLE_TASK_SECONDARY_ANIM); 

		if(CurrentVehicleTask && CurrentVehicleTask->GetTaskType() == CTaskTypes::TASK_CUTSCENE) 
		{
			CTask* pTask = static_cast<CTask*>(CurrentVehicleTask);  
			UpdateCutSceneTaskPhase(pTask, pManager);

			// Update vehicle velocity
			if (!m_bStoppedCalled)
			{
				CTaskCutScene* pCutSceneTask = static_cast<CTaskCutScene*>(pTask); 
				const crClip* pClip = pCutSceneTask->GetClip(); 

				if (pClip)
				{
					CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager);

					float fPhase = pCutsManager->GetPhaseUpdateAmount(pClip, GetAnimEventStartTime());
					float fLastPhase = pCutsManager->GetAnimPhaseForSection(pClip->GetDuration(),GetAnimEventStartTime(), pCutsManager->GetCutScenePreviousTime()); 

					const float time					= pClip->ConvertPhaseToTime(fPhase);
					const float timeOnPreviousUpdate	= pClip->ConvertPhaseToTime(fLastPhase);
					bool bIsWarpFrame					= pClip->CalcBlockPassed(timeOnPreviousUpdate, time);

					const float fCurrentTimeStep = pCutsManager->GetCurrentCutSceneTimeStep();

					if (bIsWarpFrame || fCurrentTimeStep <= 0.0001f)
					{
						GetGameEntity()->SetVelocity(VEC3_ZERO);
					}
					else
					{
						Vector3 vehVelocity(VEC3_ZERO);
						Vector3 vehDistance = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, fLastPhase, fPhase);
						vehVelocity = vehDistance/fCurrentTimeStep;

						pCutSceneTask->GetAnimOrigin().Transform3x3(vehVelocity);

#if __ASSERT
						if (vehVelocity.Mag2() > 62500.0f)
						{
							cutsceneAssertf(0, "Cutscene vehicle (%s) velocity is > 250 ms-1. Moved distance (%.2f, %.2f, %.2f) meters in %6.4f seconds\n", GetGameEntity()->GetModelName(), vehDistance.x, vehDistance.y, vehDistance.z, fCurrentTimeStep);
						}
#endif //__ASSERT

						GetGameEntity()->SetVelocity( vehVelocity );
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////
//Allow for manual adding and subtraction of bones

#if __BANK
void CCutsceneAnimatedVehicleEntity::SetBoneToHide(s32 Boneid, bool bHide)
{
	for(int i=0; i < m_HiddenBones.GetCount(); i++)
	{
		if(m_HiddenBones[i] == Boneid )
		{
			if(!bHide)
			{
				m_HiddenBones.Delete(i);
			}
			return; 
		}
	}

	m_HiddenBones.PushAndGrow(Boneid); 
}

/////////////////////////////////////////////////////////////////////////
//When debugging a real vehicle find the extras that are already switched off.

void CCutsceneAnimatedVehicleEntity::InitaliseVehicleExtraList()
{
	if(GetGameEntity())
	{
		for(int i=0; i < 10; i ++)
		{
			int VehicleHeirachy = i + VEH_EXTRA_1; 
			if (!GetGameEntity()->GetIsExtraOn(((eHierarchyId)VehicleHeirachy))) 
			{	
				m_HiddenBones.PushAndGrow((VEHICLE_HEIRACHY_MODIFIER+1) + i ); 
			}
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// Rayfire
///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneRayFireEntity::CCutSceneRayFireEntity(const cutfObject* pObject)
: cutsUniqueEntity(pObject)
{
	m_fEventTime = 0.0f; 
	m_iRayFireId = -1; 
}

CCutSceneRayFireEntity::~CCutSceneRayFireEntity()
{
	if (m_iRayFireId>-1)
	{
		CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(m_iRayFireId);

		if (pCompEntity)
		{
			pCompEntity->SetIsCutsceneDriven(false);
		}
	}

	m_iRayFireId = -1; 
}

void CCutSceneRayFireEntity::DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs), const float fTime, const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
		
		}
		break; 
	
	case CUTSCENE_PAUSE_EVENT:
	case CUTSCENE_UPDATE_EVENT:
		{
			if(m_iRayFireId > -1 )
			{
				CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(m_iRayFireId); 
				
				if(pCompEntity && pCompEntity->GetPrimaryAnimatedObject())
				{
					if(object_commands::CommandGetCompositeEntityState(m_iRayFireId) == CE_STATE_ANIMATING)
					{
						CObject* pObj = pCompEntity->GetPrimaryAnimatedObject();
						
						if(cutsceneVerifyf(pObj, "CUTSCENE_UPDATE_EVENT: Component entity failed to create the animated rayfire object: %s", pObj->GetModelName()))
						{
							if(cutsceneVerifyf(pObj->GetAnimDirector(), "CUTSCENE_UPDATE_EVENT: Rayfire object %s has failed to create an anim director", pObj->GetModelName()))
							{
								CMoveObject& move = pObj->GetMoveObject();

								const crClip* pClip = move.GetClipHelper().GetClip();

								CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 
								
								if(pClip)
								{
									float fPhase = pCutSceneManager->GetAnimPhaseForSection(pClip->GetDuration(), m_fEventTime, pCutSceneManager->GetCutSceneCurrentTime() ); 
									pCompEntity->SetIsCutsceneDriven(true);
									move.GetClipHelper().SetRate(0.0f);
									move.GetClipHelper().SetPhase(fPhase);
									for(u32 i=1; i<pCompEntity->GetChildObjects().GetCount(); i++){
										if (pCompEntity->GetChild(i)){
											CMoveObject& move = pCompEntity->GetChild(i)->GetMoveObject();
											move.GetClipHelper().SetRate(0.0f);
											move.GetClipHelper().SetPhase(fPhase);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		break;
	case CUTSCENE_SET_ANIM_EVENT:
		{
			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

			CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 

			s32 iIndex = pAssetManager->GetGameIndex(pObject->GetObjectId()); 

			if(iIndex > -1)	
			{
				if(object_commands::CommandGetCompositeEntityState(iIndex) != 0 )
				{
					if(object_commands::CommandGetCompositeEntityState(iIndex) == CE_STATE_PRIMED)
					{
						object_commands::CommandSetCompositeEntityState(iIndex, CE_STATE_START_ANIM); 
						
						CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iIndex); 
						
						//calling update to make sure we create the anim director
						pCompEntity->Update(); 
							
						if(pCompEntity && pCompEntity->GetPrimaryAnimatedObject())
						{
							CObject* pObj = pCompEntity->GetPrimaryAnimatedObject();
							if(cutsceneVerifyf(pObj, "CUTSCENE_SET_ANIM_EVENT: Component entity failed to create the animated rayfire object: %s", pObj->GetModelName()))
							{
								if(cutsceneVerifyf(pObj->GetAnimDirector(), "CUTSCENE_SET_ANIM_EVENT: Rayfire object %s has failed to create an anim director", pObj->GetModelName()))
								{
									CMoveObject& move = pObj->GetMoveObject();

									const crClip* pClip = move.GetClipHelper().GetClip();

									CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 
									m_fEventTime = fTime; 

									if(pClip)
									{
										//m_HaveAnimToUpdate = true; 
										float fPhase = pCutSceneManager->GetAnimPhaseForSection(pClip->GetDuration(), m_fEventTime, pCutSceneManager->GetCutSceneCurrentTime() ); 
										move.GetClipHelper().SetRate(0.0f);
										move.GetClipHelper().SetPhase(fPhase);
										pCompEntity->SetIsCutsceneDriven(true);

										for(u32 i=1; i<pCompEntity->GetChildObjects().GetCount(); i++){
											if (pCompEntity->GetChild(i)){
												CMoveObject& move = pCompEntity->GetChild(i)->GetMoveObject();
												move.GetClipHelper().SetRate(0.0f);
												move.GetClipHelper().SetPhase(fPhase);
											}
										}
									}

									m_iRayFireId = pAssetManager->GetGameIndex(pObject->GetObjectId());
								}
							}
						}
					}
				}
			}
		}
		break;	 
	}

}
