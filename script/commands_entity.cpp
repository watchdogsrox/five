#include "script/commands_entity.h"

// Rage headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscript/scriptguid.h"
#include "phbound/boundcomposite.h"
#include "physics/constraintdistance.h"

// Game headers
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "control/trafficlights.h"
#include "control/replay/replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "game/ModelIndices.h"
#include "modelinfo/PedModelInfo.h"
#include "animation/MoveVehicle.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/Objects/Entities/NetObjVehicle.h"
#include "objects/ObjectIntelligence.h"
#include "pathserver/PathServer.h"
#include "Peds/ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "Peds/PopZones.h"
#include "physics/physics.h"
#include "physics/gtaInst.h"
#include "pickups/Pickup.h"
#include "renderer/occlusion.h"
#include "renderer/PostScan.h"
#include "scene/lod/LodMgr.h"
#include "scene/MapChange.h"
#include "scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/commands_object.h"
#include "script/commands_ped.h"
#include "script/commands_shapetest.h"
#include "script/commands_vehicle.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/wrapper.h"
#include "task/Animation/TaskAnims.h"
#include "Task/Animation/TaskCutScene.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Train.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/vehiclepopulation.h"
#include "vfx/misc/LODLightManager.h"
#include "Event/EventDamage.h"
#include "weapons/WeaponDamage.h"
#include "weapons/inventory/PedEquippedWeapon.h"
#include "weapons/components/WeaponComponent.h"
#include "weapons/weapon.h"

SCRIPT_OPTIMISATIONS()
ANIM_OPTIMISATIONS()

PARAM(setEntityHealthPrintStack, "Prints the script stack when SetEntityHealth is called");

namespace entity_commands
{
	// transport modes used in entity in area checks (only by peds)
	enum
	{
		SCRIPT_TM_ANY,
		SCRIPT_TM_ON_FOOT,
		SCRIPT_TM_IN_VEHICLE,
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// SHARED FUNCTIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////

	CEntity* GetTargetEntity(CEntity *pEntity)
	{
		CEntity* pTarget = pEntity;

		// if the entity is a ped in a vehicle, use the vehicle instead
		if (AssertVerify(pEntity) && pEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pEntity);

			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && AssertVerify(pPed->GetMyVehicle()))
			{
				pTarget = pPed->GetMyVehicle();
			}
		}

		return pTarget;
	}

	CPhysical* GetTargetEntity(CPhysical *pEntity)
	{
		return static_cast<CPhysical*>(GetTargetEntity(static_cast<CEntity*>(pEntity)));
	}

	const CEntity* GetTargetEntity(const CEntity *pEntity)
	{
		return GetTargetEntity(const_cast<CEntity*>(pEntity));
	}

	const CPhysical* GetTargetEntity(const CPhysical *pEntity)
	{
		return static_cast<CPhysical*>(GetTargetEntity(const_cast<CPhysical*>(pEntity)));
	}

	void GetEntityPosition(const CEntity *pEntity, Vector3& position)
	{
		const fwEntity* pTarget = GetTargetEntity(pEntity);

		position = Vector3(0.0f, 0.0f, 0.0f);

		if (pTarget)
		{
			position = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
		}
	}

	bool CheckTransportMode(const CEntity *pEntity, int TransportMode, const char* ASSERT_ONLY(commandName))
	{
		bool ModeOk = true;

		if (pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);

			switch (TransportMode)
			{
			case SCRIPT_TM_ANY:
				ModeOk = true;
				break;
			case SCRIPT_TM_ON_FOOT:
				ModeOk = !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );
				break;
			case SCRIPT_TM_IN_VEHICLE:
				ModeOk = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );
				break;
			default:
				scriptAssertf(0, "%s:%s - Invalid transport mode %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, TransportMode);
			}
		}

		return ModeOk;
	}

	const CGenericClipHelper* GetEntityGenericClipHelperToQuery(int EntityIndex, const char *pAnimDictName, const char *pAnimName, const char* ASSERT_ONLY(commandName), unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL )
	{
		const CGenericClipHelper* pClipHelper = NULL;	

		if (scriptVerifyf(fwAnimManager::FindSlot(pAnimDictName) != -1, "%s:%s - Unrecognised anim dictionary: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pAnimDictName))
		{
			const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, assertFlags);

			if (pEntity && pEntity->GetAnimDirector())
			{
				const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);
				pClipHelper = ((CMove &) pEntity->GetAnimDirector()->GetMove()).FindGenericClipHelper(atStringHash(pAnimDictName), atStringHash(pAnimName));

				if (pClip && pClipHelper && pClip==pClipHelper->GetClip())
				{
					return pClipHelper;
				}
			}
		}

		return NULL;
	}

	CGenericClipHelper* GetEntityGenericClipHelperToModify(int EntityIndex, const char *pAnimDictName, const char *pAnimName, const char* ASSERT_ONLY(commandName))
	{
		CGenericClipHelper* pGenericClipHelper = NULL;	

		if (scriptVerifyf(fwAnimManager::FindSlot(pAnimDictName) != -1, "%s:%s - Unrecognised anim dictionary: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pAnimDictName))
		{
			const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

			if (pEntity && pEntity->GetAnimDirector())
			{
				const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);
				pGenericClipHelper = ((CMove &) pEntity->GetAnimDirector()->GetMove()).FindGenericClipHelper(atStringHash(pAnimDictName), atStringHash(pAnimName));

				if (pClip && pGenericClipHelper && pClip==pGenericClipHelper->GetClip())
				{
					return pGenericClipHelper;
				}
			}
		}

		return NULL;
	}

	 CTaskScriptedAnimation* FindScriptedAnimTask(int EntityIndex, bool bModify, bool bPrimary, unsigned assertFlags)
	 {
		 //Const version of FindScriptedAnimTask - allows us to track these calls in the debug recorder
		 CTaskScriptedAnimation* pTask = NULL;

		 if(fwScriptGuid::IsType<CPed>(EntityIndex))
		 {
			 const CPed* pPed = bModify ? CTheScripts::GetEntityToModifyFromGUID< CPed >(EntityIndex, assertFlags) : CTheScripts::GetEntityToQueryFromGUID< CPed >(EntityIndex, assertFlags);
			 if (pPed)
			 {
				 const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
				 if (pPedIntelligence)
				 {
					 if (bPrimary)
					 {
						 pTask = static_cast<CTaskScriptedAnimation*>(pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
					 }
					 else
					 {
						 pTask = static_cast<CTaskScriptedAnimation*>(pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim());
						 if (pTask && pTask->GetTaskType()!=CTaskTypes::TASK_SCRIPTED_ANIMATION)
							 pTask = NULL;
					 }
				 }
			 }
		 }
		 else if(fwScriptGuid::IsType<CObject>(EntityIndex))
		 {
			 const CObject* pObject = bModify ? CTheScripts::GetEntityToModifyFromGUID< CObject >(EntityIndex, assertFlags) : CTheScripts::GetEntityToQueryFromGUID< CObject >(EntityIndex, assertFlags);
			 if (pObject)
			 {
				 const CObjectIntelligence* pIntelligence = pObject->GetObjectIntelligence();
				 if (pIntelligence)
				 {
					 if (bPrimary)
					 {
						 pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->FindTaskByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
					 }
					 else
					 {
						 pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
					 }
				 }
			 }
		 }
		 else if(fwScriptGuid::IsType<CVehicle>(EntityIndex))
		 {
			 const CVehicle* pVeh = bModify ? CTheScripts::GetEntityToModifyFromGUID< CVehicle >(EntityIndex, assertFlags) : CTheScripts::GetEntityToQueryFromGUID< CVehicle >(EntityIndex, assertFlags);
			 if (pVeh)
			 {
				 const CVehicleIntelligence* pIntelligence = pVeh->GetIntelligence();
				 if (pIntelligence)
				 {
					 pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION));
				 }
			 }
		 }

		 return pTask;
	 }
	 const CTaskSynchronizedScene* FindSynchronizedSceneTask(s32 EntityIndex, const char * pAnimDictName,  const char * pAnimName)
	 {
		 const CTaskSynchronizedScene* pTask = NULL;
		 if(fwScriptGuid::IsType<CPed>(EntityIndex))
		 {
			 const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID< CPed >(EntityIndex);
			 if (pPed)
			 {
				 const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
				 if (pPedIntelligence)
				 {
					 pTask = static_cast<const CTaskSynchronizedScene* >(pPedIntelligence->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
				 }
			 }
		 }
		 else if(fwScriptGuid::IsType<CObject>(EntityIndex))
		 {
			 const CObject* pObject = CTheScripts::GetEntityToQueryFromGUID< CObject >(EntityIndex);
			 if (pObject)
			 {
				 const CObjectIntelligence* pIntelligence = pObject->GetObjectIntelligence();
				 if (pIntelligence)
				 {
					 pTask = static_cast< const CTaskSynchronizedScene* >(pIntelligence->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
				 }
			 }
		 }
		 else if(fwScriptGuid::IsType<CVehicle>(EntityIndex))
		 {
			 const CVehicle* pVeh = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(EntityIndex);
			 if (pVeh)
			 {
				 const CVehicleIntelligence* pIntelligence = pVeh->GetIntelligence();
				 if (pIntelligence)
				 {
					 pTask = static_cast< const CTaskSynchronizedScene* >(pIntelligence->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SYNCHRONIZED_SCENE));
				 }
			 }
		 }

		 if (pTask)
		 {
			 if (pTask->GetClip().GetHash() == atStringHash(pAnimName) 
				&& pTask->GetDictionary().GetHash() == atStringHash(pAnimDictName))
			 {
				 return pTask;
			 }
		 }

		 return NULL;
	 }

	CEntity* GetEntityAttachParent(const CPhysical* pEntity)
	{
		CEntity* pAttachParent = static_cast<CEntity*>(pEntity->GetAttachParent());

		// network clones can be detached when they are supposed to be attached (this is the case with trailers, which are detached when
		// they are off the collideable area of the map. This is because their physics cannot be processed.) 
		if (!pAttachParent && pEntity->GetNetworkObject())
		{
			pAttachParent = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject())->GetEntityAttachedTo();
		}

		return pAttachParent;
	}

	bool IsEntityInAxisAlignedArea(Vector3& vEntityPos, Vector3& vAreaStart, Vector3& vAreaEnd, bool Do3dCheck)
	{
		bool IsInArea = false;

		if ((vEntityPos.x >= MIN(vAreaStart.x, vAreaEnd.x)) && (vEntityPos.x <= MAX(vAreaStart.x, vAreaEnd.x)) &&
			(vEntityPos.y >= MIN(vAreaStart.y, vAreaEnd.y)) && (vEntityPos.y <= MAX(vAreaStart.y, vAreaEnd.y)))
		{
			IsInArea = true;
		}

		if (Do3dCheck)
		{
			if ((vEntityPos.z < MIN(vAreaStart.z, vAreaEnd.z)) || (vEntityPos.z > MAX(vAreaStart.z, vAreaEnd.z)))
			{
				IsInArea = false;
			}
		}

		if (CScriptDebug::GetDbgFlag())
		{
			if (Do3dCheck)
			{
				CScriptDebug::DrawDebugCube(vAreaStart.x, vAreaStart.y, vAreaStart.z, vAreaEnd.x, vAreaEnd.y, vAreaEnd.z);
			}
			else
			{
				CScriptDebug::DrawDebugSquare(vAreaStart.x, vAreaStart.y, vAreaEnd.x, vAreaEnd.y);
			}
		}

		return IsInArea;
	}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// QUERYING COMMANDS
///////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool CommandDoesEntityExist(int EntityIndex)
	{
		if(EntityIndex == -1)
			return false;
		return fwScriptGuid::IsType<fwEntity>(EntityIndex) && CTheScripts::Frack2();
	}

	bool CommandDoesEntityBelongToThisScript(int EntityIndex, bool bDeadCheck)
	{
		unsigned flags = bDeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_ALL : CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, flags);

		if (pEntity)
		{
			const CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			if (pExtension && (pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler()))
			{
				return true;
			}
		}

		return false;
	}

	bool CommandDoesEntityHaveDrawable(int EntityIndex)
	{
		bool bResult = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);

		if (pEntity)
		{
			bResult = pEntity->GetDrawable() != NULL;

			if (bResult)
			{
				SCRIPT_ASSERT(pEntity->GetDrawable()->GetSkeletonData(), "DOES_ENTITY_HAVE_DRAWABLE - Entity drawable does not have skeleton data");
			}
		}

		return bResult;
	}

	bool CommandDoesEntityHavePhysics(int EntityIndex)
	{
		bool bResult = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			bResult = (pEntity->GetCurrentPhysicsInst() != 0);
		}

		return bResult;
	}

	bool CommandDoesEntityHaveSkeleton(int EntityIndex)
	{
		bool bResult = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);

		if (pEntity)
		{
			bResult = pEntity->GetSkeleton() != NULL;
		}

		return bResult;
	}

	bool CommandDoesEntityHaveAnimDirector(int EntityIndex)
	{
		bool bResult = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);

		if (pEntity)
		{
			bResult = pEntity->GetAnimDirector() != NULL;
		}

		return bResult;
	}

	bool CommandHasEntityAnimFinished(int EntityIndex, const char *pAnimDictName, const char *pAnimName, s32 animType)
	{
		if (animType & kAnimScripted)
		{
			const CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToQuery(EntityIndex, pAnimDictName, pAnimName, "HAS_ENTITY_ANIM_FINISHED");

			if (pGenericClipHelper && pGenericClipHelper->IsHeldAtEnd())
			{
				return true;
			}

			const CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, false);
			if (pTask)
			{
				CTaskScriptedAnimation::ePlaybackPriority priority;
				s32 index;
				if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
				{
					if(pTask->GetPhase(priority, (u8)index)>=1.0f)
						return true;
				}
			}

			pTask = FindScriptedAnimTask(EntityIndex, false, false);
			if (pTask)
			{
				CTaskScriptedAnimation::ePlaybackPriority priority;
				s32 index;
				if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
				{
					if (pTask->GetPhase(priority, (u8)index)>=1.0f)
						return true;
				}
			}
		}

		if (animType & kAnimSyncedScene)
		{
			const CTaskSynchronizedScene* pTask = FindSynchronizedSceneTask(EntityIndex, pAnimDictName, pAnimName);
			if (pTask)
			{
				// check the synchronized scene phase and hold last frame status to see if we're at the end
				fwSyncedSceneId sceneId = pTask->GetScene();
				if (
					fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId)
					&& fwAnimDirectorComponentSyncedScene::IsSyncedSceneHoldLastFrame(sceneId)
					&& fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(sceneId)>=1.0f
					)
				{
					return true;
				}
				
			}
		}

		if (animType & kAnimCode)
		{
			// TODO - GSALES 14/09/2012: Add support for searching the motion tree for any held at end anims
			SCRIPT_ASSERT(0, "Searching for code anims is not currently supported. See an animation programmer if you need this.");
		}

		return false;
	}

	bool CommandHasEntityBeenDamagedByAnyObject(int EntityIndex)
	{
		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			if (pEntity->HasBeenDamagedByAnyObject())
			{
				bResult = true;
			}
		}

		return bResult;
	}

	bool CommandHasEntityBeenDamagedByAnyPed(int EntityIndex)
	{
		scriptDisplayf("CommandHasEntityBeenDamagedByAnyPed  EntityIndex: %d", EntityIndex);
		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			scriptDisplayf("CommandHasEntityBeenDamagedByAnyPed  Entity ptr: 0x%p", pEntity);
			if (pEntity->HasBeenDamagedByAnyPed())
			{
				bResult = true;
			}
		}
		scriptDisplayf("CommandHasEntityBeenDamagedByAnyPed  Is Entity damaged: %s", bResult ? "TRUE": "FALSE");
		return bResult;
	}

	bool CommandHasEntityBeenDamagedByAnyVehicle(int EntityIndex)
	{
		scriptDisplayf("CommandHasEntityBeenDamagedByAnyVehicle  EntityIndex: %d", EntityIndex);

		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			scriptDisplayf("CommandHasEntityBeenDamagedByAnyVehicle  Entity ptr: 0x%p", pEntity);
			if (pEntity->HasBeenDamagedByAnyVehicle())
			{				
				bResult = true;
			}
		}
		scriptDisplayf("CommandHasEntityBeenDamagedByAnyVehicle  Is Entity damaged: %s", bResult ? "TRUE": "FALSE");
		return bResult;
	}

	bool CommandHasEntityBeenDamagedByEntity(int EntityIndex, int DamagerIndex, bool bCheckDamagerVehicle )
	{
		scriptDisplayf("CommandHasEntityBeenDamagedByEntity  EntityIndex: %d  DamagerIndex: %d  bCheckDamagerVehicle: %s", EntityIndex, DamagerIndex, bCheckDamagerVehicle ? "TRUE" :"FALSE");
		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		const CPhysical *pDamager = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(DamagerIndex);

		if (pEntity && pDamager)
		{
			if (pEntity->GetNetworkObject())
			{
				bResult = NetworkInterface::HasBeenDamagedBy(pEntity->GetNetworkObject(), pDamager);
			}			
			else 
			{
				bResult = pEntity->HasBeenDamagedByEntity(const_cast<CPhysical*>(pDamager));
			}

			if (!bResult)
			{
				const CPhysical* pTargetDamager = GetTargetEntity(pDamager);

				if (pTargetDamager != pDamager)
				{
					if (pEntity->GetNetworkObject())
					{
						bResult = NetworkInterface::HasBeenDamagedBy(pEntity->GetNetworkObject(), pTargetDamager);
					}
					else 
					{
						bResult = pEntity->HasBeenDamagedByEntity(const_cast<CPhysical*>(pDamager));
					}
				}
			}

			if (!bResult && !pEntity->GetNetworkObject() && bCheckDamagerVehicle )
			{
				const CPed* pedDamager = pDamager->GetIsTypePed() ? static_cast<const CPed*>(pDamager) : NULL;

				// if the damager is driving a vehicle then check to see if the entity was damaged by the vehicle
				if (pedDamager && pedDamager->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					CVehicle* vehicle = pedDamager->GetMyVehicle();
					scriptAssertf(vehicle, "%s:HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY - Damager is not in a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());

					if (vehicle && vehicle->IsEngineOn() && pEntity->HasBeenDamagedByEntity(vehicle) && vehicle->m_EngineSwitchedOnTime < pEntity->GetWeaponDamagedTimeByEntity(vehicle))
					{
						bResult = (pedDamager == vehicle->GetDriver());
					}
				}
			}
		}
		scriptDisplayf("CommandHasEntityBeenDamagedByEntity  Is Entity damaged: %s", bResult ? "TRUE": "FALSE");
		return bResult;
	}

	bool CommandHasEntityClearLosToEntity(int EntityIndex1, int EntityIndex2, s32 LOSFlags )
	{
		const CEntity *pEntity1 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex1);
		const CEntity *pEntity2 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex2, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool bResult = false;

		if (pEntity1 && pEntity2)
		{
			if (pEntity1->GetIsTypePed())
			{
				bResult = static_cast<const CPed*>(pEntity1)->OurPedCanSeeThisEntity(pEntity2, true, shapetest_commands::GetPhysicsFlags(LOSFlags));
			}
			else
			{
				// Want to exclude the entities which define the start and end of the probe from the test.
				static const s32 iNumExceptions = 2;
				const CEntity* ppExcludeEnts[iNumExceptions] = { pEntity1, pEntity2 };

				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(pEntity1->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pEntity2->GetTransform().GetPosition()));
				probeDesc.SetIncludeFlags(shapetest_commands::GetPhysicsFlags(LOSFlags));
				probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
				probeDesc.SetExcludeEntities(ppExcludeEnts, 2);
				
				bResult = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			}
		}

		return bResult;
	}

	bool CommandHasEntityClearLosToEntityAdjustForCover(int EntityIndex1, int EntityIndex2, s32 LOSFlags)
	{
		const CEntity *pEntity1 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex1);
		const CEntity *pEntity2 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex2, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool bResult = false;

		if (pEntity1 && pEntity2)
		{
			if (pEntity1->GetIsTypePed())
			{
				bResult = static_cast<const CPed*>(pEntity1)->OurPedCanSeeThisEntity(pEntity2, true, shapetest_commands::GetPhysicsFlags(LOSFlags), true);
			}
			else
			{
				// Want to exclude the entities which define the start and end of the probe from the test.
				static const s32 iNumExceptions = 2;
				const CEntity* ppExcludeEnts[iNumExceptions] = { pEntity1, pEntity2 };

				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(pEntity1->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pEntity2->GetTransform().GetPosition()));
				probeDesc.SetIncludeFlags(shapetest_commands::GetPhysicsFlags(LOSFlags));
				probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
				probeDesc.SetExcludeEntities(ppExcludeEnts, 2);

				bResult = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			}
		}

		return bResult;
	}

	bool CommandHasEntityClearLosToEntityInFront(int EntityIndex1, int EntityIndex2)
	{
		const CEntity *pEntity1 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex1);
		const CEntity *pEntity2 = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex2, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool bResult = false;

		if (pEntity1 && pEntity2)
		{
			if (pEntity1->GetIsTypePed())
			{
				bResult = static_cast<const CPed*>(pEntity1)->OurPedCanSeeThisEntity(pEntity2, false);
			}
			else
			{
				const Vector3 vEntity1Position = VEC3V_TO_VECTOR3(pEntity1->GetTransform().GetPosition());
				const Vector3 vEntity2Position = VEC3V_TO_VECTOR3(pEntity2->GetTransform().GetPosition());

				float losX = vEntity2Position.x - vEntity1Position.x;
				float losY = vEntity2Position.y - vEntity1Position.y;

				Vector3 ForwardVector(VEC3V_TO_VECTOR3(pEntity1->GetTransform().GetB()));
				float fDotProduct = ForwardVector.x * losX + ForwardVector.y * losY;

				if (fDotProduct >= 0.0f)
				{
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetStartAndEnd(vEntity1Position, vEntity2Position);
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE);
					probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);

					bResult = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
				}
			}
		}

		return bResult;
	}

	//Must have collision recording enabled to use this. It must be called on the frame of the collision
	bool CommandHasEntityCollidedWithAnything( int EntityIndex )
	{
		bool bResult = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			if (SCRIPT_VERIFY(pEntity->IsRecordingCollisionHistory(), "HAS_ENTITY_COLLIDED_WITH_ANYTHING - Call SET_OBJECT_RECORDS_COLLISIONS TRUE first"))
			{
				return pEntity->GetFrameCollisionHistory()->GetNumCollidedEntities() > 0;
			}
		}
		return(bResult);
	}

	int CommandGetLastMaterialHitByEntity(int EntityIndex)
	{
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{
			if(SCRIPT_VERIFY(pEntity->IsRecordingCollisionHistory(), "GET_LAST_MATERIAL_HIT_BY_ENTITY - CALL SET_OBJECT_RECORDS_COLLISIONS(true) first." ))
			{
				const CCollisionRecord* pColRecord = pEntity->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();

				if(pColRecord)
				{
					char zMaterialName[128];
					PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(pColRecord->m_OtherCollisionMaterialId), zMaterialName, 128);
					return rage::atStringHash(zMaterialName);
				}
			}
		}
		return 0;
	}

	//Normal returned points out of me
	scrVector CommandGetCollisionNormalOfLastHitForEntity(int EntityIndex)
	{
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		Vector3 vCollisionNormal(VEC3_ZERO);
		if(pEntity)
		{
			if(SCRIPT_VERIFY(pEntity->IsRecordingCollisionHistory(), "GET_COLLISION_NORMAL_OF_LAST_HIT_FOR_ENTITY - CALL SET_OBJECT_RECORDS_COLLISIONS(true) first." ))
			{
				const CCollisionRecord* pColRecord = pEntity->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();

				if(pColRecord)
				{
					vCollisionNormal = -pColRecord->m_MyCollisionNormal;
				}
			}
		}

		return vCollisionNormal;
	}

	float CommandGetEntityAnimCurrentTime(int EntityIndex, const char *pAnimDictName, const char *pAnimName)
	{
		float fResult = 0.0f;

		const CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToQuery(EntityIndex, pAnimDictName, pAnimName, "GET_ENTITY_ANIM_CURRENT_TIME",  CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pGenericClipHelper)
		{
			animDebugf3("GET_ENTITY_ANIM_CURRENT_TIME - (generic clip) %.3f: %d, %s, %s", pGenericClipHelper->GetPhase(), EntityIndex, pAnimDictName, pAnimName);
			return pGenericClipHelper->GetPhase();
		}

		const CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, false, true, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				animDebugf3("GET_ENTITY_ANIM_CURRENT_TIME - (primary task) %.3f: %d, %s, %s", pTask->GetPhase(priority, (u8)index), EntityIndex, pAnimDictName, pAnimName);
				return pTask->GetPhase(priority, (u8)index);
			}
		}

		pTask = FindScriptedAnimTask(EntityIndex, false, false , CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				animDebugf3("GET_ENTITY_ANIM_CURRENT_TIME - (secondary task) %.3f: %d, %s, %s", pTask->GetPhase(priority, (u8)index), EntityIndex, pAnimDictName, pAnimName);
				return pTask->GetPhase(priority, (u8)index);
			}
		}

		SCRIPT_ASSERT(0, "GET_ENTITY_ANIM_CURRENT_TIME - Anim is not playing on the entity");
		animDebugf3("GET_ENTITY_ANIM_CURRENT_TIME - (anim not playing) %.3f: %d, %s, %s", fResult, EntityIndex, pAnimDictName, pAnimName);
		return fResult;
	}

	float CommandGetEntityAnimTotalTime(int EntityIndex, const char *pAnimDictName, const char *pAnimName)
	{
		float fResult = 0.0f;

		const CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToQuery(EntityIndex, pAnimDictName, pAnimName, "GET_ENTITY_ANIM_TOTAL_TIME");

		if (pGenericClipHelper)
		{
			return pGenericClipHelper->GetDuration()* 1000.0f;
		}

		const CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, false);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				return pTask->GetDuration(priority, (u8)index)* 1000.0f;
			}
		}

		pTask = FindScriptedAnimTask(EntityIndex, false, false);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				return pTask->GetDuration(priority, (u8)index)* 1000.0f;
			}
		}
		
		SCRIPT_ASSERT(0, "GET_ENTITY_ANIM_TOTAL_TIME - Anim is not playing on the entity");

		return fResult;
	}

	float CommandGetAnimDuration (const char *pAnimDictName, const char *pAnimName)
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);
		if (SCRIPT_VERIFY(pClip, "GET_ANIM_DURATION - Couldn't find animation. Has it been loaded?"))
		{
			return pClip->GetDuration();
		}

		return 0.0f;
	}

	int CommandGetEntityAttachedTo(int EntityIndex)
	{
		int OtherEntityIndex = NULL_IN_SCRIPTING_LANGUAGE;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			CEntity* pAttachParent = GetEntityAttachParent(pEntity);

			if (pAttachParent)
			{
				OtherEntityIndex = CTheScripts::GetGUIDFromEntity(*pAttachParent);
			}
		}
		return OtherEntityIndex;
	}

	scrVector CommandGetEntityCoords(int EntityIndex, bool DeadCheck)
	{
		unsigned flags = DeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_ALL : CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, flags);

		Vector3 EntityPos(0.0f, 0.0f, 0.0f);

		if (pEntity)
		{
			GetEntityPosition(pEntity, EntityPos);
		}

		return EntityPos;
	}

	scrVector CommandGetEntityForwardVector(int EntityIndex)
	{
		Vector3 forwardVec = VEC3_ZERO;

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if(pEntity)
		{
			forwardVec = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetB());
		}

		return forwardVec;
	}

	float CommandGetEntityForwardX(int EntityIndex)
	{
		float ForwardX = 0.0f;

		const fwEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			const Vector3 vecForward(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetB()));
			float XComp = vecForward.x;
			float YComp = vecForward.y;
			float Length = rage::Sqrtf((XComp * XComp) + (YComp * YComp));

			ForwardX = XComp / Length;
		}

		return ForwardX;
	}

	float CommandGetEntityForwardY(int EntityIndex )
	{
		float ForwardY = 0.0f;

		const fwEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			const Vector3 vecForward(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetB()));
			float XComp = vecForward.x;
			float YComp = vecForward.y;
			float Length = rage::Sqrtf((XComp * XComp) + (YComp * YComp));

			ForwardY = YComp / Length;
		}

		return ForwardY;
	}

	float CommandGetEntityHeading(int EntityIndex)
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		float Heading = 0.0f;

		if (pEntity)
		{
			const CEntity* pTarget = GetTargetEntity(pEntity);

			if (pTarget)
			{
				Heading = ( RtoD * pTarget->GetTransform().GetHeading());

				if (Heading < 0.0f)
				{
					Heading += 360.0f;
				}
				if (Heading > 360.0f)
				{
					Heading -= 360.0f;
				}
			}
		}
		return Heading;
	}

	float CommandGetEntityHeadingFromEulers( int EntityIndex )
	{
		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		float Heading = 0.0f;

		if (pEntity)
		{
			const CEntity* pTarget = GetTargetEntity(pEntity);

			if (pTarget)
			{
				Matrix34 mat = MAT34V_TO_MATRIX34( pTarget->GetMatrix() );
				Vector3 eulers;
				mat.ToEulersYXZ( eulers );

				Heading = RtoD * eulers.z;

				if (Heading < 0.0f)
				{
					Heading += 360.0f;
				}
				if (Heading > 360.0f)
				{
					Heading -= 360.0f;
				}
			}
		}

		return Heading;
	}

	int CommandGetEntityHealth(int EntityIndex)
	{
		int health = 0;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			health = static_cast<int>(pEntity->GetHealth());
		}

		return health;
	}

	int CommandGetEntityMaxHealth(int EntityIndex)
	{
		int health = 0;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			health = static_cast<int>(pEntity->GetMaxHealth());
		}

		return health;
	}

	void CommandSetEntityMaxHealth(int EntityIndex, int health)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			gnetDebug3("SET_ENTITY_MAX_HEALTH: %s is setting %s to have max health of: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetLogName(), health);

			if (pEntity->GetNetworkObject() && health != pEntity->GetMaxHealth())
			{
				SafeCast(CNetObjPhysical, pEntity->GetNetworkObject())->SetMaxHealthAlteredByScript();
			}
			 
			pEntity->SetMaxHealth((float)health);
		}
	}

	float CommandGetEntityHeight(int EntityIndex, const scrVector &scrVecPos, bool bWorldPos, bool bWorldResult)
	{
		Vector3 vecPos = Vector3(scrVecPos);
		float fHeight = 0.0f;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			if(pEntity->GetCurrentPhysicsInst())
			{
				Vector3 vecStart = vecPos;
				Vector3 vecEnd = vecPos;

				const float fEntityZ = pEntity->GetTransform().GetPosition().GetZf();

				vecStart.z = fEntityZ + pEntity->GetBoundRadius();
				vecEnd.z = fEntityZ - pEntity->GetBoundRadius();

				if(!bWorldPos)
				{
					// Transform the start and end position vectors into world space for the shape test.
					vecStart = pEntity->TransformIntoWorldSpace(vecStart);
					vecEnd = pEntity->TransformIntoWorldSpace(vecEnd);
				}

				WorldProbe::CShapeTestFixedResults<> probeResult;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetStartAndEnd(vecStart, vecEnd);
				probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
				probeDesc.SetIncludeEntity(pEntity);

				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					Vector3 vecResult = probeResult[0].GetHitPosition();
					// Result from shape test will be in world space. If this is not desirable, transform it into
					// the local space of the vehicle before returning.
					if(!bWorldResult)
						vecResult = VEC3V_TO_VECTOR3(pEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecResult)));

					fHeight = vecResult.z;
				}
			}
		}

		return fHeight;
	}

	float CommandGetEntityHeightAboveGround(int EntityIndex)
	{
		float height = 0.0f;

		const fwEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			Vector3 pos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			const float fSecondSurfaceInterp=0.0f;
			float ground = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, pos.x, pos.y, pos.z);

			height = pos.z - ground;
		}

		return height;
	}

	void CommandGetEntityMatrix(int EntityIndex, Vector3& vFront, Vector3& vSide, Vector3& vUp, Vector3& vPos)
	{
		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			const Matrix34 matObj = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

			vFront = matObj.b;
			vSide = matObj.a;
			vUp = matObj.c;
			vPos = matObj.d;
		}

		return;
	}

	int CommandGetEntityModel(int EntityIndex)
	{
		int ModelHashKey = 0;

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(pEntity->GetModelId());
			if (SCRIPT_VERIFY(pModel, "GET_ENTITY_MODEL - couldn't find model for the entity's model index"))
			{
				ModelHashKey = (s32) pModel->GetHashKey();
			}
		}
		return ModelHashKey;
	}

	scrVector CommandGetOffsetFromEntityGivenWorldCoords(int EntityIndex, const scrVector &scrVecWorldCoors)
	{
		Vector3 LocalOffset = VEC3_ZERO;

		const fwEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			Vector3 WorldOffset = Vector3(scrVecWorldCoors);

			LocalOffset = VEC3V_TO_VECTOR3(pEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(WorldOffset)));
		}

		return LocalOffset;
	}

	scrVector CommandGetOffsetFromEntityInWorldCoords( int EntityIndex, const scrVector &scrVecOffset )
	{
		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		Vector3 WorldCoords = VEC3_ZERO;

		if (pEntity)
		{
			Vector3 LocalOffset = Vector3(scrVecOffset);
			WorldCoords = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition() + pEntity->GetTransform().Transform3x3(RCC_VEC3V(LocalOffset)));
		}
		return WorldCoords;
	}

	float CommandGetEntityPitch(int EntityIndex)
	{
		float Pitch = 0.0f;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);

		if(pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pEntity);
			
				Pitch = pPed->GetCurrentPitch();
			}
			else
			{
				Pitch = pEntity->GetTransform().GetPitch();
			}

			Pitch *= RtoD;
		}

		return Pitch;
	}

	void CommandGetEntityQuaternion(int EntityIndex, float &ReturnX, float &ReturnY, float &ReturnZ, float &ReturnW)
	{
		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			const Matrix34 matObject = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
			Quaternion quatPed;
			quatPed.FromMatrix34(matObject);
			ReturnX = quatPed.x;
			ReturnY = quatPed.y;
			ReturnZ = quatPed.z;
			ReturnW = quatPed.w;
		}
	}

	float CommandGetEntityRoll(int EntityIndex)
	{
		float roll = 0.0f;

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if(pEntity)
		{
			roll = pEntity->GetTransform().GetRoll();
			roll *= RtoD;
		}

		return roll;
	}

	Vector3 CommandGetEntityRotation(int EntityIndex, s32 RotOrder)
	{
		Vector3 vRot = VEC3_ZERO; 

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pEntity)
		{
			Matrix34 m = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
			vRot = CScriptEulers::MatrixToEulers(m, static_cast<EulerAngleOrder>(RotOrder));
			vRot *= RtoD;
		}

		return vRot;
	}

	scrVector CommandGetEntityRotationVelocity( int EntityIndex)
	{
		Vector3 RotVel = VEC3_ZERO;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			RotVel = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pEntity->GetAngVelocity())));
		}

		return RotVel; 
	}

	const char* CommandGetEntityScript(int EntityIndex, int& UNUSED_PARAM(instanceId))
	{
		const char* scriptName = NULL;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (pEntity)
		{
			const CGameScriptId* scriptId = pEntity->GetScriptThatCreatedMe();

			if (scriptId)
			{
				scriptName = scriptId->GetScriptName();
			}
		}

		return scriptName;
	}

	float CommandGetEntitySpeed(int EntityIndex)
	{
		float fSpeed = 0.0f;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			const CPhysical* pTarget = GetTargetEntity(pEntity);

			if (pTarget)
			{
				fSpeed = pTarget->GetVelocity().Mag();
			}
		}

		return fSpeed;
	}

	scrVector CommandGetEntitySpeedVector(int EntityIndex, bool bLocalResult)
	{
		Vector3 speedVec = VEC3_ZERO;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{
			speedVec = pEntity->GetVelocity();

			if(bLocalResult)
			{
				speedVec = VEC3V_TO_VECTOR3(pEntity->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(pEntity->GetVelocity())));
			}
		}
		return speedVec;
	}

	float CommandGetEntityUprightValue(int EntityIndex)
	{
		float UprightValue = 0.0f;

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			UprightValue = pEntity->GetTransform().GetC().GetZf();
		}

		return UprightValue;
	}

	scrVector CommandGetEntityVelocity(int EntityIndex)
	{
		Vector3 vVelocity = VEC3_ZERO;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pEntity)
		{
			const CPhysical* pTarget = GetTargetEntity(pEntity);

			if (pTarget)
			{
				vVelocity = pTarget->GetVelocity();
			}
		}

		return vVelocity;
	}

	int CommandGetObjectIndexFromEntityIndex(int EntityIndex)
	{
#if __ASSERT
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pEntity)
		{
			scriptAssertf(pEntity->GetIsTypeObject(), "%s: GET_OBJECT_INDEX_FROM_ENTITY_INDEX - warning, entity is not an object!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif
		return EntityIndex;
	}

/*
	scrVector CommandGetOffsetFromCoordAndHeadingInWorldCoords( const scrVector &scrVecCoors, float fHeadingDegs, const scrVector &scrOffset )
	{
		const float fHeadingRads = DtoR * fHeadingDegs;
		Vector3 vResult = scrVecCoors;
		Vector3 vRotatedOffset = scrOffset;
		vRotatedOffset.x = scrOffset.x * cosf(fHeadingRads) - scrOffset.y * sinf(fHeadingRads);
		vRotatedOffset.y = scrOffset.y * cosf(fHeadingRads) + scrOffset.x * sinf(fHeadingRads);
		vResult += vRotatedOffset;
		return vResult;
	}
*/

	int CommandGetPedIndexFromEntityIndex(int EntityIndex)
	{
#if __ASSERT
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pEntity)
		{
			scriptAssertf(pEntity->GetIsTypePed(), "%s: GET_PED_INDEX_FROM_ENTITY_INDEX - warning, entity %d is not a ped!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), EntityIndex);
		}
#endif
		return EntityIndex;
	}

	int CommandGetVehicleIndexFromEntityIndex(int EntityIndex)
	{
#if __ASSERT
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pEntity)
		{
			scriptAssertf(pEntity->GetIsTypeVehicle(), "%s: GET_VEHICLE_INDEX_FROM_ENTITY_INDEX - warning, entity %d is not a vehicle!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), EntityIndex);
		}
#endif
		return EntityIndex;
	}

	scrVector CommandGetWorldPositionOfEntityBone(int EntityIndex, int iBoneIdx)
	{
		Vector3 VecPos(VEC3_ZERO);

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);
		if(pEntity)
		{
			if(SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_WORLD_POSITION_OF_ENTITY_BONE - Object skeleton is NULL."))
			{
				if(SCRIPT_VERIFY(iBoneIdx >= 0 && iBoneIdx < pEntity->GetSkeleton()->GetBoneCount(), "GET_WORLD_POSITION_OF_ENTITY_BONE - Bone index exceeds maximum for skeleton."))
				{
					Matrix34 boneMatrix(MAT34V_TO_MATRIX34(pEntity->GetMatrix()));
					pEntity->GetSkeleton()->GetGlobalMtx(iBoneIdx, RC_MAT34V(boneMatrix));
					VecPos = boneMatrix.d;
				}
			}
		}

		return scrVector(VecPos);
	}

	int CommandGetEntityType(int EntityIndex)
	{
		// please update ENUM ENTITY_TYPE in commands_entity.sch when this enum changes
		enum eEntityType
		{
			ET_NONE = 0,
			ET_PED,
			ET_VEHICLE,
			ET_OBJECT
		};

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, 0);
		if (!pEntity)
			return ET_NONE;
		if (pEntity->GetIsTypePed())
			return ET_PED;
		if (pEntity->GetIsTypeVehicle())
			return ET_VEHICLE;
		if (pEntity->GetIsTypeObject())
			return ET_OBJECT;

		return ET_NONE;
	}

	int CommandGetEntityPopulationType(int EntityIndex)
	{
		enum ePopulationType
		{
			PT_UNKNOWN = 0,
			PT_RANDOM_PERMANENT,
			PT_RANDOM_PARKED,
			PT_RANDOM_PATROL,
			PT_RANDOM_SCENARIO,
			PT_RANDOM_AMBIENT,
			PT_PERMANENT,
			PT_MISSION,
			PT_REPLAY,
			PT_CACHE,
			PT_TOOL
		};
		ePopulationType rt = PT_UNKNOWN;
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
#if __ASSERT		
		scriptAssertf(pEntity, "GET_ENTITY_POPULATION_TYPE - couldn't find entity %d", EntityIndex);
#endif	//	__ASSERT		
		if(pEntity)
		{
			switch(pEntity->PopTypeGet())
			{
			case POPTYPE_UNKNOWN:
				rt = PT_UNKNOWN;break;
			case POPTYPE_RANDOM_PERMANENT:
				rt = PT_RANDOM_PERMANENT;break;
			case POPTYPE_RANDOM_PARKED:
				rt = PT_RANDOM_PARKED;break;
			case POPTYPE_RANDOM_PATROL:
				rt = PT_RANDOM_PATROL;break;
			case POPTYPE_RANDOM_SCENARIO:
				rt = PT_RANDOM_SCENARIO;break;
			case POPTYPE_RANDOM_AMBIENT:
				rt = PT_RANDOM_AMBIENT;break;
			case POPTYPE_PERMANENT:
				rt = PT_PERMANENT;break;
			case POPTYPE_MISSION:
				rt = PT_MISSION;break;
			case POPTYPE_REPLAY:
				rt = PT_REPLAY;break;
			case POPTYPE_CACHE:
				rt = PT_CACHE;break;
			case POPTYPE_TOOL:
				rt = PT_TOOL;break;
			default:
#if __ASSERT		
				scriptAssertf(pEntity, "GET_ENTITY_POPULATION_TYPE - unregistered population type  pEntity->PopTypeGet() == %d", pEntity->PopTypeGet());
#endif	//	__ASSERT		
				break;
			}
		}
		return rt;
	}

	bool CommandIsAnEntity(int objGuid)
	{
		fwExtensibleBase* pObj = fwScriptGuid::GetBaseFromGuid(objGuid);

		// Note: calling fwScriptGuid::FromGuid() fails an assert if it's not the type
		// we're checking for, which is undesirable here. Maybe we should make this a different
		// template function in fwScriptGuid, though.
		return pObj && pObj->GetIsClassId(CEntity::GetStaticClassId());
	}

	bool CommandIsEntityAPed(int EntityIndex)
	{
		bool bRet = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, 0);
		if (pEntity)
		{
			bRet = pEntity->GetIsTypePed();
		}

		return bRet;
	}

	bool CommandIsEntityAMissionEntity(int EntityIndex)
	{
		bool bMissionEntity = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pEntity)
		{
			bMissionEntity = pEntity->PopTypeIsMission();

#if __ASSERT
			const CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			if (bMissionEntity)
			{
				bool bIsAPlayerPed = false;
				if (pEntity->GetIsTypePed())
				{
					if ( ((CPed *)pEntity)->IsAPlayerPed() )
					{
						bIsAPlayerPed = true;
					}
				}

				bool bIsACutsceneEntity = false;
				if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE)
				{	//	Skip the assert below for mocap-created entities.
					//	I wonder if this command should even return TRUE for these entities?
					bIsACutsceneEntity = true;
				}

				if (!bIsAPlayerPed && !bIsACutsceneEntity)
				{
					scriptAssertf(pExtension, "%s: IS_ENTITY_A_MISSION_ENTITY - entity %s has pop type %s but doesn't have a script extension. Entity Index is %d. Model is %s. Entity pointer is %p", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "", CTheScripts::GetPopTypeName(pEntity->PopTypeGet()), EntityIndex, pEntity->GetModelName(), pEntity);
				}
			}
			else
			{
				scriptAssertf(!pExtension, "%s: IS_ENTITY_A_MISSION_ENTITY - entity %s has a script extension but has pop type %s. Entity Index is %d. Model is %s. Entity pointer is %p", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "", CTheScripts::GetPopTypeName(pEntity->PopTypeGet()), EntityIndex, pEntity->GetModelName(), pEntity);
			}

#endif	//	__ASSERT
		}
		return bMissionEntity;
	}

	bool CommandIsEntityAVehicle(int EntityIndex)
	{
		bool bRet = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, 0);
		if (pEntity)
		{
			bRet = pEntity->GetIsTypeVehicle();
		}

		return bRet;
	}

	bool CommandIsEntityAnObject(int EntityIndex)
	{
		bool bRet = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, 0);
		if (pEntity)
		{
			bRet = pEntity->GetIsTypeObject();
		}

		return bRet;
	}

	bool CommandIsEntityAtCoord( int EntityIndex, const scrVector &scrVecCoors, const scrVector &scrVecDimensions, bool HighlightArea, bool Do3dCheck, int TransportMode)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool IsAtEntity = false;

		if (pEntity)
		{
			if (!CheckTransportMode(pEntity, TransportMode, "IS_ENTITY_AT_COORD"))
			{
				return false;
			}

			Vector3 EntityPos;

			GetEntityPosition(pEntity, EntityPos);

			Vector3 VecCoors = Vector3 (scrVecCoors);
			Vector3 VecDimensions = Vector3 (scrVecDimensions);

			Vector3 vAreaStart = VecCoors - VecDimensions;
			Vector3 vAreaEnd = VecCoors + VecDimensions;

			IsAtEntity = IsEntityInAxisAlignedArea(EntityPos, vAreaStart, vAreaEnd, Do3dCheck);

			if (HighlightArea)
			{
				CScriptAreas::NewHighlightImportantArea(VecCoors.x, VecCoors.y, VecCoors.z, VecDimensions.x, VecDimensions.y, VecDimensions.z, Do3dCheck, CALCULATION_ENTITY_AT_COORD);
			}
#if __BANK					
			if (CScriptDebug::GetDbgFlag())
			{
				if (!Do3dCheck)
					VecDimensions.z = 0.0f;

				char text[256]; 	
				sprintf(text, "script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				grcDebugDraw::Text(VecCoors-VecDimensions, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text); 
				grcDebugDraw::Text(VecCoors+VecDimensions, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text); 
			}
#endif// __BANK
		}

		return (IsAtEntity);
	}

	bool CommandIsEntityAtEntity(int EntityIndex, int TargetIndex, const scrVector &scrVecDimensions, bool HighlightArea, bool Do3dCheck, int TransportMode)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool IsAtEntity = false;

		if (pEntity)
		{
			if (!CheckTransportMode(pEntity, TransportMode, "IS_ENTITY_AT_ENTITY"))
			{
				return false;
			}

			const CEntity *pTarget = CTheScripts::GetEntityToQueryFromGUID<CEntity>(TargetIndex);

			if (pTarget)
			{
				Vector3 EntityPos, TargetPos;

				GetEntityPosition(pEntity, EntityPos);
				GetEntityPosition(pTarget, TargetPos);

				Vector3 VecDimensions = Vector3(scrVecDimensions);
				Vector3 vAreaStart = TargetPos - VecDimensions;
				Vector3 vAreaEnd = TargetPos + VecDimensions;

				IsAtEntity = IsEntityInAxisAlignedArea(EntityPos, vAreaStart, vAreaEnd, Do3dCheck);

				if (HighlightArea)
				{
//					float ZOffsetForLocateCylinder = 0.0f;

// 					if (pTarget->GetIsTypePed())
// 					{
// 						const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
// 
// 						if (pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pTargetPed->GetMyVehicle())
// 						{
// 							ZOffsetForLocateCylinder = pTargetPed->GetMyVehicle()->GetDistanceFromCentreOfMassToBaseOfModel();
// 						}
// 						else
// 						{
// 							ZOffsetForLocateCylinder = pTargetPed->GetCapsuleInfo()->GetGroundToRootOffset();
// 						}
// 					}
// 					else
// 					{
// 						Assert(dynamic_cast<const CPhysical*>(pTarget));
// 						ZOffsetForLocateCylinder = static_cast<const CPhysical*>(pTarget)->GetDistanceFromCentreOfMassToBaseOfModel();
// 					}

//					CScriptAreas::NewHighlightImportantArea(TargetPos.x, TargetPos.y, TargetPos.z - ZOffsetForLocateCylinder, VecDimensions.x, VecDimensions.y, VecDimensions.z, Do3dCheck, CALCULATION_ENTITY_AT_ENTITY);
					CScriptAreas::NewHighlightImportantArea(TargetPos.x, TargetPos.y, TargetPos.z, VecDimensions.x, VecDimensions.y, VecDimensions.z, Do3dCheck, CALCULATION_ENTITY_AT_ENTITY);
				}

#if __BANK					
				if (CScriptDebug::GetDbgFlag())
				{
//					if (!Do3dCheck)
//						VecDimensions.z = 0.0f;

					char text[256]; 	
					sprintf(text, "script: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					grcDebugDraw::Text(TargetPos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text); 
					grcDebugDraw::Text(TargetPos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text); 
				}
#endif// __BANK

			}

		}

		return IsAtEntity;
	}

	bool CommandIsEntityAttached(int EntityIndex)
	{
		bool bAttached = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			bAttached = pEntity->GetIsAttached();

			if (!bAttached && pEntity->GetNetworkObject())
			{
				bAttached = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject())->GetIsCloneAttached();
			}
		}
		return bAttached;
	}

	bool CommandIsEntityAttachedToAnyObject(int EntityIndex)
	{
		bool bAttached = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			CEntity* pAttachParent = GetEntityAttachParent(pEntity);

			bAttached = pAttachParent && pAttachParent->GetIsTypeObject();
		}

		return bAttached;
	}

	bool CommandIsEntityAttachedToAnyPed(int EntityIndex)
	{
		bool bAttached = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			CEntity* pAttachParent = GetEntityAttachParent(pEntity);

			bAttached = pAttachParent && pAttachParent->GetIsTypePed();
		}
		return bAttached;
	}

	bool CommandIsEntityAttachedToAnyVehicle(int EntityIndex)
	{
		bool bAttached = false;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			CEntity* pAttachParent = GetEntityAttachParent(pEntity);

			bAttached = pAttachParent && pAttachParent->GetIsTypeVehicle();
		}
		return bAttached;
	}

	bool CommandIsEntityAttachedToEntity(int Entity1Index, int Entity2Index)
	{
		bool bAttached = false;

		const CPhysical* pEntity1 = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(Entity1Index, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		const CPhysical* pEntity2 = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(Entity2Index, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	
		if(pEntity1 && pEntity2)
		{
			CEntity* pAttachParent1 = GetEntityAttachParent(pEntity1);
			CEntity* pAttachParent2 = GetEntityAttachParent(pEntity2);

			bAttached = (pAttachParent1 == pEntity2) || (pAttachParent2 == pEntity1) ;
		}
		return bAttached;
	}

	bool CommandIsEntityDead(int EntityIndex, bool bIgnoreVehicleDrowningIfInvincible)
	{
		bool Result = false;

		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPhysical>(EntityIndex, 0);

		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				Result = pPed->IsFatallyInjured();
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

				bool bIsDrowning = pVehicle->m_nVehicleFlags.bIsDrowning;
				if (bIgnoreVehicleDrowningIfInvincible && pVehicle->m_nPhysicalFlags.bNotDamagedByAnything)
				{
					bIsDrowning = false;
				}

				Result = (pVehicle->GetStatus() == STATUS_WRECKED) || bIsDrowning;
			}
			else 
			{
				Result = pEntity->GetHealth() < 0.001f;
			}

#if __DEV
			pEntity->m_nDEflags.bCheckedForDead = TRUE;
#endif
		}
		else
		{
			Result = true;
		}

		return Result;
	}

	//Is the entity not touching world?
	bool CommandIsEntityInAir(int EntityIndex)
	{
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(pEntity == NULL)
			return false;

		//Special cases
		if (pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);

			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))
			{
				return true;
			}
			else if(!pPed->GetIsStanding() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KnockedUpIntoAir )
				&& pPed->GetPedIntelligence()->GetTaskActiveSimplest()
				&& pPed->GetPedIntelligence()->GetTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_FALL_OVER)
			{
				return true;
			}
			else if(!pPed->GetUsingRagdoll())
			{
				return false;
			}
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			if(!static_cast<const CVehicle*>(pEntity)->IsInAir())
			{
				return false;
			}
		}

		if (NetworkInterface::IsNetworkOpen() && pEntity->IsNetworkClone() && pEntity->GetIsTypeVehicle())
		{
			CNetObjVehicle* pNetObjVehicle = (CNetObjVehicle*) pEntity->GetNetworkObject();
			if (pNetObjVehicle)
				return pNetObjVehicle->IsRemoteInAir();
		}

		//General case (objects, vehicles and ragdolls)
		bool bHitBuilding = pEntity->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING);

		if( bHitBuilding )
		{
			//buildings in contact with > 0
			return false;
		}
		else
		{
			if( pEntity->GetFrameCollisionHistory()->IsHistoryComplete() )
			{
				//buildings in contact with == 0
				return true;
			}
			else //buildings in contact with count unknown
			{
				//const Matrix34 trans = MAT34V_TO_MATRIX34( pEntity->GetMatrix() );

				//WorldProbe::CShapeTestBoundDesc boundTestDesc;
				//boundTestDesc.SetDomainForTest(WorldProbe::TEST_IN_LEVEL);
				//boundTestDesc.SetBoundFromEntity(pEntity);
				//boundTestDesc.SetTransform( &trans );
				//boundTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				//boundTestDesc.SetContext(WorldProbe::LOS_Unspecified);
				//return !WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc);

				// The above can be REALLY FREAKING EXPENSIVE
				// At this point it seems like the evidence is pointing to us being true for InAir
				// However, NOT being in the air as a default is probably safer (in that less irreversible bad things are likely to be triggered)
				// - Additionally, falling through to here requires collision history deemed incomplete which will generally only happen for something
				//   waking from rest or running out of memory - In the first case it should be safe to assume the object was at rest touching something
				//   In the second case it again seems better to default to NOT being in the air
				//
				// Excepting that we go the opposite way and assume aircraft type vehicles are in the air by default
				if( pEntity->GetIsTypeVehicle() )
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
					if( pVehicle->GetIsAircraft() )
					{
						return true;
					}
				}
				//
				return false;
			}
		}
	}

	bool CommandIsEntityInAngledArea(int EntityIndex, const scrVector &scrVecCoors1, const scrVector & scrVecCoors2, float AreaWidth, bool HighlightArea, bool Do3dCheck, int TransportMode)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool bResult = false;

		if (pEntity)
		{
			if (!CheckTransportMode(pEntity, TransportMode, "IS_ENTITY_IN_ANGLED_AREA"))
			{
				return false;
			}

			Vector3 EntityPos;

			Vector3 vStart = Vector3 (scrVecCoors1);
			Vector3 vEnd = Vector3 (scrVecCoors2);

			GetEntityPosition(pEntity, EntityPos);

			bResult = CScriptAreas::IsPointInAngledArea(EntityPos, vStart, vEnd, AreaWidth, HighlightArea, Do3dCheck, CScriptDebug::GetDbgFlag());
		}

		return bResult;
	}

	bool CommandIsEntityInArea(int EntityIndex, const scrVector & vAreaStart, const scrVector & vAreaEnd, bool HighlightArea, bool Do3dCheck, int TransportMode)
	{
		bool IsInArea = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			if (!CheckTransportMode(pEntity, TransportMode, "IS_ENTITY_IN_AREA"))
			{
				return false;
			}

			Vector3 EntityPos;

			Vector3 vStart = Vector3 (vAreaStart);
			Vector3 vEnd = Vector3 (vAreaEnd);

			GetEntityPosition(pEntity, EntityPos);

			IsInArea = IsEntityInAxisAlignedArea(EntityPos, vStart, vEnd, Do3dCheck);

			if (HighlightArea)
			{
				CScriptAreas::NewHighlightImportantArea(vStart.x, vStart.y, vStart.z, vEnd.x, vEnd.y, vEnd.z, Do3dCheck, CALCULATION_ENTITY_IN_AREA);
			}
		}

		return IsInArea;
	}

	bool CommandIsEntityInZone(int EntityIndex, const char* pZoneLabel)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		bool bResult = false;

		if (pEntity)
		{
			Vector3 EntityPos;
			GetEntityPosition(pEntity, EntityPos);

			if (CPopZones::IsPointInPopZonesWithText(&EntityPos, pZoneLabel))
			{
				bResult = TRUE;
			}
		}

		return bResult;
	}

	bool CommandIsEntityInWater(int EntityIndex)
	{
		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			if(pEntity->IsNetworkClone() && static_cast<CNetObjPhysical *>(pEntity->GetNetworkObject())->IsNetworkObjectInWater())
			{
				bResult = true;
			}
			else if (pEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pEntity);

				// Check for ped in sinking vehicle
				if( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()) )
				{
					if( pPed->GetMyVehicle()->m_nVehicleFlags.bIsDrowning )
					{
						bResult = true;
					}
				}
				else if (pPed->GetIsSwimming())
				{
					bResult = true;
				}
				// Check for dead peds who drowned
				// Network clones might not report that they are swimming
				else if((pPed->IsDead() || pPed->IsNetworkClone() )
					&& pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
				{
					bResult = true;
				}
				// We might still have missed dying peds. Check the "is in water" flag as a fall back.
				else
				{
					bResult = pPed->GetIsInWater();
				}
			}
			else if (pEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pEntity)->GetVehicleType() == VEHICLE_TYPE_BOAT)
			{
				bResult = static_cast<const CBoat*>(pEntity)->m_BoatHandling.IsInWater();
			}
			else if (pEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pEntity)->InheritsFromAmphibiousAutomobile() )
			{
				bResult = static_cast<const CAmphibiousAutomobile*>(pEntity)->GetBoatHandling()->IsInWater();
			}
			else if (pEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pEntity)->GetVehicleType() == VEHICLE_TYPE_TRAILER)
			{
				bResult = pEntity->GetIsInWater() || pEntity->GetWasInWater();
			}
			else
			{
				bResult = pEntity->GetIsInWater();
			}
		}
#if __DEV
		TUNE_BOOL(IS_ENTITY_IN_WATER_DEBUG, false);
		TUNE_FLOAT(IS_ENTITY_IN_WATER_DEBUG_SPHERE_RADIUS, 1.0f, 0.0f, 50.0f, 0.1f);
		if(IS_ENTITY_IN_WATER_DEBUG)
		{
			Color32 colour(1.0f, 0.0f, 0.0f, 0.3f);
			if(bResult)
				colour = Color32(0.0f, 1.0f, 0.0f, 0.3f);
			grcDebugDraw::Sphere(pEntity->GetTransform().GetPosition(), IS_ENTITY_IN_WATER_DEBUG_SPHERE_RADIUS, colour);
		}
#endif // __DEV

		return bResult;
	}

	float CommandGetEntitySubmergedLevel(int EntityIndex)
	{
		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if(SCRIPT_VERIFY(pEntity, "GET_ENTITY_SUBMERGED_LEVEL - Invalid entity index."))
		{
			const CBuoyancy& rBuoyancy = pEntity->m_Buoyancy;
			if(rBuoyancy.GetBuoyancyInfoUpdatedThisFrame())
			{
				return rBuoyancy.GetSubmergedLevel();
			}
		}

		return 0.0f;
	}

	void CommandSetEntityRequiresMoreExpensiveRiverCheck(int EntityIndex, bool FlagValue)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_REQUIRES_MORE_EXPENSIVE_RIVER_CHECK - Invalid entity index."))
		{
			pEntity->m_Buoyancy.m_buoyancyFlags.bUseWidestRiverBoundTolerance = FlagValue ? 1 : 0;
		}
	}

	bool CommandIsEntityOnScreen(int EntityIndex)
	{
		bool bResult = false;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pEntity)
		{
			if (camInterface::IsSphereVisibleInGameViewport(pEntity->GetBoundCentre(), pEntity->GetBoundRadius()))
			{
				bResult = true;
			}
		}
		return bResult;
	}

	bool CommandIsEntityPlayingAnim(int EntityIndex, const char *pAnimDictName, const char *pAnimName, s32 animType = kAnimScripted | kAnimSyncedScene)
	{
		// search standard scripted anims
		if (animType & kAnimScripted)
		{
			const CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToQuery(EntityIndex, pAnimDictName, pAnimName, "IS_ENTITY_PLAYING_ANIM",  CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pGenericClipHelper)
			{
				animDebugf3("IS_ENTITY_PLAYING_ANIM - TRUE: (Generic clip) %d, %s, %s, %d", EntityIndex, pAnimDictName, pAnimName, animType);
				return true;
			}

			const CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, false, true , CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pTask && pTask->IsPlayingClip(pAnimDictName, pAnimName))
			{
				animDebugf3("IS_ENTITY_PLAYING_ANIM - TRUE: (primary task) %d, %s, %s, %d", EntityIndex, pAnimDictName, pAnimName, animType);
				return true;
			}

			pTask = FindScriptedAnimTask(EntityIndex, false, false, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pTask && pTask->IsPlayingClip(pAnimDictName, pAnimName))
			{
				animDebugf3("IS_ENTITY_PLAYING_ANIM - TRUE: (secondary task) %d, %s, %s, %d", EntityIndex, pAnimDictName, pAnimName, animType);
				return true;
			}
		}
		
		// search synced scene anims
		if (animType & kAnimSyncedScene)
		{
			const CTaskSynchronizedScene* pTaskSynchronizedScene = FindSynchronizedSceneTask(EntityIndex, pAnimDictName, pAnimName);
			if (pTaskSynchronizedScene)
			{
				animDebugf3("IS_ENTITY_PLAYING_ANIM - TRUE: (synced scene) %d, %s, %s, %d", EntityIndex, pAnimDictName, pAnimName, animType);
				return true;
			}
		}

		if (animType & kAnimCode)
		{
			// TODO - GSALES 14/09/2012: Add support for searching the motion tree for code animations
			SCRIPT_ASSERT(0, "Searching for code anims is not currently supported. See an animation programmer if you need this.");
		}

		animDebugf3("IS_ENTITY_PLAYING_ANIM - FALSE: %d, %s, %s, %d", EntityIndex, pAnimDictName, pAnimName, animType);
		return false;
	}

	bool CommandIsEntityStatic( int EntityIndex )
	{
		bool bResult = false;

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			bResult = pEntity->GetIsStatic();
		}

		return bResult;
	}

	bool CommandIsEntityTouchingEntity(int EntityIndex1, int EntityIndex2)
	{
		const CPhysical *pEntity1 = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex1);
		const CPhysical *pEntity2 = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex2);

		if (pEntity1 == NULL || pEntity2 == NULL)
		{
			return false;
		}

		const CPhysical* pTarget1 = GetTargetEntity(pEntity1);
		const CPhysical* pTarget2 = GetTargetEntity(pEntity2);

		if(pTarget1 == NULL || pTarget2 == NULL)
		{
			return false;
		}

		//Special cases
		if (pTarget1->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pTarget1);

			if (pPed->GetGroundPhysical() == pTarget2 )
			{
				return true;
			}
		}

		if (pTarget2->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pTarget2);

			if (pPed->GetGroundPhysical() == pTarget1 )
			{
				return true;
			}
		}

		// Vehicle wheels
		if (pTarget1->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pTarget1);
			int numWheels = pVehicle->GetNumWheels();

			for( int i = 0; i < numWheels; i++ )
			{
				const CWheel* pWheel = pVehicle->GetWheel( i );

				if( pWheel->GetHitPhysical() == pTarget2 ||
					pWheel->GetPrevHitPhysical() == pTarget2 )
				{
					return true;
				}
			}
		}

		if (pTarget2->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pTarget2);
			int numWheels = pVehicle->GetNumWheels();

			for( int i = 0; i < numWheels; i++ )
			{
				const CWheel* pWheel = pVehicle->GetWheel( i );

				if( pWheel->GetHitPhysical() == pTarget1 ||
					pWheel->GetPrevHitPhysical() == pTarget1 )
				{
					return true;
				}
			}
		}

		//General case
		if( pTarget1->GetFrameCollisionHistory()->FindCollisionRecord(pTarget2) != NULL )
		{
			return true;
		}
		else if( pTarget1->GetFrameCollisionHistory()->IsHistoryComplete() )
		{
			return false;
		}
		
		if( pTarget2->GetFrameCollisionHistory()->FindCollisionRecord(pTarget1) != NULL )
		{
			return true;
		}
		else if( pTarget2->GetFrameCollisionHistory()->IsHistoryComplete() )
		{
			return false;
		}

		//Neither entity has a complete collision history for the frame, so fall back on a shape test
		WorldProbe::CShapeTestBoundDesc boundTestDesc;
		const Matrix34 trans = MAT34V_TO_MATRIX34(pTarget1->GetMatrix());
		boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		boundTestDesc.SetBoundFromEntity(pTarget1);
		boundTestDesc.SetIncludeEntity(pTarget2);
		boundTestDesc.SetTransform(&trans);
		boundTestDesc.SetContext(WorldProbe::LOS_Unspecified);
		return WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc);
	}

	bool CommandIsEntityTouchingModel( int EntityIndex, int modelHashKey )
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>( EntityIndex );

		if( pEntity )
		{
			const CPhysical* pTarget	= GetTargetEntity( pEntity );
			CBaseModelInfo* pModelInfo	= CModelInfo::GetBaseModelInfoFromHashKey( (u32)modelHashKey, NULL );

			if( pTarget &&
				pModelInfo )
			{
				CCollisionHistoryIterator collisionIterator( pTarget->GetFrameCollisionHistory() );
				u32 modelNameHash = pModelInfo->GetModelNameHash();
					
				while(const CCollisionRecord* pColRecord = collisionIterator.GetNext())
				{
					CBaseModelInfo* baseModelInfo = pColRecord->m_pRegdCollisionEntity.Get()->GetBaseModelInfo();
					if( baseModelInfo && baseModelInfo->GetModelNameHash() == modelNameHash )
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool CommandIsEntityUpright(int EntityIndex, float fVerticalAngleLimit)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		bool bResult = false;

		if (pEntity)
		{
			float	zUpLimit = rage::Max(0.0f, rage::Cosf(( DtoR * fVerticalAngleLimit)));

			if (pEntity->GetTransform().GetC().GetZf() >= zUpLimit)
			{
				bResult = true;
			}
		}

		return bResult;
	}

	bool CommandIsEntityUpsideDown(int EntityIndex)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		bool bResult = false;

		if (pEntity)
		{
			if (pEntity->GetTransform().GetC().GetZf() <= UPSIDEDOWN_CAR_UPZ_THRESHOLD)
			{
				bResult = true;
			}
		}
		return bResult;
	}

	bool CommandIsEntityVisible(int EntityIndex)
	{
		bool bResult = false;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			bResult = pEntity->GetIsVisible();
		}

		return bResult;
	}

	bool CommandIsEntityVisibleToScript(int EntityIndex)
	{
		bool bResult = false;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			netObject* pNetObj = pEntity->GetIsDynamic() ? static_cast<const CDynamicEntity*>(pEntity)->GetNetworkObject() : NULL;

			if (pNetObj)
			{
				bResult = static_cast<CNetObjEntity*>(pNetObj)->IsVisibleToScript();
			}
			else
			{
				bResult = pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT);
			}
		}

		return bResult;
	}

	// This command was disabled since it was unused and a bit confusing: if we call GetIsVisibleInSomeViewportThisFrame(),
	// it may return false if the entity is within the viewport but occluded.
	//
	//bool CommandIsEntityInViewport(int EntityIndex)
	//{
	//	bool bResult = false;
	//
	//	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);
	//
	//	if (pEntity)
	//	{
	//		if(pEntity->GetIsDynamic())
	//		{
	//			bResult = static_cast<const CDynamicEntity*>(pEntity)->GetIsVisibleInSomeViewportThisFrame();
	//		}
	//		else
	//		{
	//			// This was the return value of the default implementation of the virtual
	//			// GetIsVisibleInSomeViewportThisFrame() in fwEntity:
	//			bResult = true;
	//		}
	//	}
	//
	//	return bResult;
	//
	//}

	bool CommandIsEntityOccluded(int EntityIndex)
	{
		bool bResult = false;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			spdAABB		tempBox;
			const spdAABB &aabb = pEntity->GetBoundBox( tempBox );
			bResult = !COcclusion::IsBoxVisible( aabb );
		}

		return bResult;
	}

	bool CommandWouldEntityBeOccluded(int ModelHashKey, const scrVector &scrCoords, bool ASSERT_ONLY(bAssertIfModelIsntLoaded))
	{
		fwModelId modelId;
		CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(static_cast< u32 >( ModelHashKey ), &modelId);
		
		if ( SCRIPT_VERIFY( modelId.IsValid(), "%WOULD_ENTITY_BE_OCCLUDED - this is not a valid model index" ) )
		{
			if (modelInfo->GetHasLoaded())
			{
				const spdAABB&	origAabb = modelInfo->GetBoundingBox();
				const Vec3V		origHalfExtents = origAabb.GetExtent();
				const Vec3V		newCenter = static_cast< Vec3V >( scrCoords );
				const spdAABB	newAabb( newCenter - origHalfExtents, newCenter + origHalfExtents );

				return !COcclusion::IsBoxVisible( newAabb );
			}
			else
			{
#if __ASSERT
				if (bAssertIfModelIsntLoaded)
				{
					scriptAssertf(0, "%s - WOULD_ENTITY_BE_OCCLUDED - %s model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CModelInfo::GetBaseModelInfoName(modelId));
				}
#endif
			}
		}

		return false;
	}

	bool CommandIsEntityWaitingForWorldCollision(int EntityIndex)
	{
		bool bResult = false;

		const fwEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			bResult = pEntity->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION);
		}

		return bResult;
	}

/*
	bool CommandIsPointInAngledArea(const scrVector &scrVecPoint, const scrVector &scrVecCoors1, const scrVector &scrVecCoors2, float AreaWidth, bool HighlightArea, bool Do3dCheck)
	{
		bool bResult = false;

		Vector3 VecPoint = Vector3(scrVecPoint);
		Vector3 vStart = Vector3 (scrVecCoors1);
		Vector3 vEnd = Vector3 (scrVecCoors2);

		bResult = IsPointInAngledArea(VecPoint, vStart, vEnd, AreaWidth, HighlightArea, Do3dCheck);

		return bResult;
	}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// MODIFYING COMMANDS
///////////////////////////////////////////////////////////////////////////////////////////////////////////

	void CommandApplyForceToEntity(int EntityIndex, int nApplyType, const scrVector &scrVecForce, const scrVector &scrVecOffset, int nComponent, bool bLocalForce, bool bLocalOffset, bool bScaleByMass, bool bPlayAudio, bool bScaleByTimeWarp)
	{
		Vector3 vecForce = Vector3(scrVecForce);
		Vector3 vecOffset = Vector3(scrVecOffset);

		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

		if (pEntity && pEntity->GetCurrentPhysicsInst() && pEntity->GetCurrentPhysicsInst()->IsInLevel())
		{
			Vector3 vecApplyForce = vecForce;
			Vector3 vecApplyOffset = vecOffset;

			if(bLocalForce)
				vecApplyForce = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecForce)));

			if(bLocalOffset)
			{
				if(pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetUsingRagdoll())
				{
					Matrix34 matBone;
					static_cast<CPed*>(pEntity)->GetBoneMatrixFromRagdollComponent(matBone, nComponent);
					matBone.Transform(vecOffset, vecApplyOffset);
					vecApplyOffset -= VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				}
				else
				{
					vecApplyOffset = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(RCC_VEC3V(vecOffset)));
				}
			}

			if(bScaleByMass)
			{				
				// Scale force commands by mass, torque commands by angular inertia
				switch(nApplyType)
				{
				case APPLY_TYPE_EXTERNAL_IMPULSE:
				case APPLY_TYPE_EXTERNAL_FORCE:
				case APPLY_TYPE_FORCE:
				case APPLY_TYPE_IMPULSE:
					{
						vecApplyForce.Scale(pEntity->GetMass());
					}
					break;
				case APPLY_TYPE_TORQUE:
				case APPLY_TYPE_ANGULAR_IMPULSE:
					{
						// At this point torque is in world space
						Vector3 vAngInertia = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pEntity->GetAngInertia())));
						vecApplyForce.Multiply(vAngInertia);
					}
					break;
				default:
					Assertf(false, "Unhandled apply type");
					break;
				}
			}

			int nNumComponents = 1;
			if(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE)
				nNumComponents = static_cast<phBoundComposite*>(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound())->GetNumBounds();

			if(nComponent < 0 || nComponent >= nNumComponents)
			{
				scriptAssertf(false, "%s:APPLY_FORCE_TO_ENTITY - component is outside range for this entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				nComponent = 0;
			}

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! GTAV HACK FUN PARTY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			// Script-applied forces are tuned to there being two timeslices per-frame, with one timeslice not having this force applied.
			// Over 60Hz, the game switches to running only one timeslice per-frame, which means that tuning is no longer appropriate and things will move differently.
			// Make sure that script applied forces are scaled such that when there's more or less timeslices being run than was expected, the intended effect remains.
			float fMult = (float)CPhysics::GetNumTimeSlices() / 2.0f;
			vecApplyForce *= fMult;

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! GTAV HACK FUN PARTY TURBO HD EDITION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			// If the TORNADO is being pushed from the back (As in the finale mission), zero out the traction spring delta max value so that the traction spring code in the wheels won't run.
			// At high framerates running one timeslice per-frame, the vehicle would sometimes refuse to move despite having a large force applied to it because of some shenanigans in the wheel code.
			// The wheel traction spring was providing a reasonably large window for this situation to arise. Removing this smoothing reduces this window making it much harder (impossible?) for this situation to occur!
			
			if(pEntity->GetIsTypeVehicle())
			{
				if(vecForce.x == 0 && vecForce.y > 0 && vecForce.z == 0 && vecOffset.Mag2() == 0)
				{
					CVehicle *pVeh = static_cast<CVehicle *>(pEntity);
					if(pVeh->GetModelIndex() == MI_CAR_TORNADO && pVeh->GetDriver() == NULL)
					{
						pVeh->pHandling->m_fTractionSpringDeltaMax = 0.0f;
					}
				}
			}

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			switch(nApplyType)
			{
			case APPLY_TYPE_EXTERNAL_IMPULSE:
				{
#if __ASSERT
					if(SCRIPT_VERIFY(pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) || pEntity->CheckForceInRange(vecApplyForce, 150.0f),
						"APPLY_FORCE_TO_ENTITY - force greater than recommended limits"))
#endif // __ASSERT
					{
						pEntity->ApplyExternalImpulse(vecApplyForce * (bScaleByTimeWarp ? fwTimer::GetTimeWarpActive() : 1.0f), vecApplyOffset, nComponent);
					}
				}
				break;
			case APPLY_TYPE_EXTERNAL_FORCE:
				{
					scriptAssertf(pEntity->CheckForceInRange(vecApplyForce, 150.0f), "%s:APPLY_FORCE_TO_ENTITY - force greater than recommended limits", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					pEntity->ApplyExternalForce(vecApplyForce, vecApplyOffset, nComponent);
				}
				break;
			case APPLY_TYPE_IMPULSE:
				{
					scriptAssertf(pEntity->CheckForceInRange(vecApplyForce, 20.0f), "%s:APPLY_FORCE_TO_ENTITY - impulse greater than recommended limits", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					pEntity->ApplyImpulse(vecApplyForce * (bScaleByTimeWarp ? fwTimer::GetTimeWarpActive() : 1.0f), vecApplyOffset, nComponent);
				}
				break;
			case APPLY_TYPE_FORCE:
				{
					scriptAssertf(pEntity->CheckForceInRange(vecApplyForce, 150.0f), "%s:APPLY_FORCE_TO_ENTITY - force greater than recommended limits", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					pEntity->ApplyForce(vecApplyForce, vecApplyOffset, nComponent);
				}
				break;
			case APPLY_TYPE_TORQUE:
				{
					Assertf(vecOffset.IsZero(),"Can only apply torque with a ZERO vec offset");
					pEntity->ApplyTorque(vecApplyForce);
				}
				break;
			case APPLY_TYPE_ANGULAR_IMPULSE:
				{
					Assertf(vecOffset.IsZero(),"Can only apply torque with a ZERO vec offset");
					float fTimestep = fwTimer::GetTimeStep();
					fTimestep /= (float) CPhysics::GetNumTimeSlices();
					vecApplyForce.InvScale(fTimestep);
					pEntity->ApplyTorque(vecApplyForce);
				}
				break;
			default:
				Assertf(false,"Unhandled nApplyType");
				break;

			}

			if(bPlayAudio)
			{
				if(pEntity->GetIsTypeVehicle())
				{
					(static_cast<CVehicle*>(pEntity))->GetVehicleAudioEntity()->ScriptAppliedForce(vecApplyForce);
				}
			}
		}

		if(NetworkInterface::IsGameInProgress() && pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
		{
			CNetObjPhysical* pPhysObj = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject());
			pPhysObj->RequestForceSendOfPhysicalAttachVelocityState();
		}
	}

	void CommandApplyForceToEntityCenterOfMassHelper(CPhysical *pEntity, int nApplyType, const scrVector &scrVecForce, int nComponent, bool bLocalForce, bool bScaleByMass, bool bApplyToChildren)
	{
		Vector3 vecForce = Vector3(scrVecForce);

		if (pEntity && pEntity->GetCurrentPhysicsInst() && pEntity->GetCurrentPhysicsInst()->IsInLevel())
		{
			Vector3 vecApplyForce = vecForce;

			if(bLocalForce)
				vecApplyForce = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecForce)));

			if(bScaleByMass)
			{				
				// Scale force commands by mass, torque commands by angular inertia
				switch(nApplyType)
				{
				case APPLY_TYPE_FORCE:
				case APPLY_TYPE_IMPULSE:
					{
						vecApplyForce.Scale(pEntity->GetMass());
					}
					break;
				case APPLY_TYPE_TORQUE:
				case APPLY_TYPE_ANGULAR_IMPULSE:
					{
						// At this point torque is in world space
						Vector3 vAngInertia = VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pEntity->GetAngInertia())));
						vecApplyForce.Multiply(vAngInertia);
					}
					break;
				default:
					Assertf(false, "Unhandled apply type");
					break;
				}
			}

			int nNumComponents = 1;
			if(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE)
				nNumComponents = static_cast<phBoundComposite*>(pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound())->GetNumBounds();

			if(nComponent < 0 || nComponent >= nNumComponents)
			{
				scriptAssertf(false, "%s:APPLY_FORCE_TO_ENTITY - component is outside range for this entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				nComponent = 0;
			}


			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! GTAV HACK FUN PARTY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			// Script-applied forces are tuned to there being two timeslices per-frame, with one timeslice not having this force applied.
			// Over 60Hz, the game switches to running only one timeslice per-frame, which means that tuning is no longer appropriate and things will move differently.
			// Make sure that script applied forces are scaled such that when there's more or less timeslices being run than was expected, the intended effect remains.
			float fMult = (float)CPhysics::GetNumTimeSlices() / 2.0f;
			vecApplyForce *= fMult;

			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			switch(nApplyType)
			{
			case APPLY_TYPE_IMPULSE:
				{
					scriptAssertf(pEntity->CheckForceInRange(vecApplyForce, 20.0f), "%s:APPLY_FORCE_TO_ENTITY - impulse greater than recommended limits", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					pEntity->ApplyImpulseCg(vecApplyForce);
				}
				break;
			case APPLY_TYPE_FORCE:
				{
					scriptAssertf(pEntity->CheckForceInRange(vecApplyForce, 150.0f), "%s:APPLY_FORCE_TO_ENTITY - force greater than recommended limits", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					pEntity->ApplyForceCg(vecApplyForce);
				}
				break;
			case APPLY_TYPE_TORQUE:
				{
					pEntity->ApplyTorque(vecApplyForce);
				}
				break;
			case APPLY_TYPE_ANGULAR_IMPULSE:
				{
					float fTimestep = fwTimer::GetTimeStep();
					fTimestep /= (float) CPhysics::GetNumTimeSlices();
					vecApplyForce.InvScale(fTimestep);
					pEntity->ApplyTorque(vecApplyForce);
				}
				break;
			default:
				Assertf(false,"Unhandled nApplyType");
				break;

			}
		}

		if(bApplyToChildren)
		{
			for(fwEntity* pChildEntity = pEntity->GetChildAttachment(); pChildEntity != NULL; pChildEntity = pChildEntity->GetSiblingAttachment())
			{
				CommandApplyForceToEntityCenterOfMassHelper(dynamic_cast<CPhysical*>(pChildEntity), nApplyType, scrVecForce, nComponent, bLocalForce, bScaleByMass, bApplyToChildren);
			}
		}
	}

	void CommandApplyForceToEntityCenterOfMass(int EntityIndex, int nApplyType, const scrVector &scrVecForce, int nComponent, bool bLocalForce, bool bScaleByMass, bool bApplyToChildren)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		CommandApplyForceToEntityCenterOfMassHelper(pEntity, nApplyType, scrVecForce, nComponent, bLocalForce, bScaleByMass, bApplyToChildren);
	}

	void CommandProcessEntityAttachments(int EntityIndex)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		
		if (SCRIPT_VERIFY(pEntity, "PROCESS_ENTITY_ATTACHMENTS - The entity does not exist"))
		{
			// if the entity is in a synced scene, update it's position here
			fwAnimDirectorComponentSyncedScene* pComponent = pEntity->GetAnimDirector() ? static_cast<fwAnimDirectorComponentSyncedScene*>(pEntity->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene)) : NULL;
			if (pComponent && pComponent->IsPlayingSyncedScene())
			{
				pComponent->UpdateSyncedSceneMover();
			}
			else
			{
				// if the entity has an attach parent, find the root of the attachment tree and process it immediately
				while (pEntity && pEntity->GetAttachParent())
				{
					pEntity = static_cast<CPhysical*>(pEntity->GetAttachParent());
				}

				if (pEntity)
				{
					pEntity->ProcessAllAttachments();
				}
			}
		}
	}

	void AttachEntityToEntity( CPhysical* pEntity1, CPhysical* pEntity2, s16 Entity1BoneIndex, s16 Entity2BoneIndex, const Vector3& vecOffset, const Vector3 &vecRotation, bool bDetachWhenDead, bool bDetachWhenRagdoll, bool bActiveCollision, bool bUseBasicAttachIfPed, int RotOrder, bool bAttachOffsetIsRelative, bool bMarkAsNoLongerNeededWhenDetached )
	{
		// script generally shouldn't be calling this command on a clone, but need to do this is certain cases. To avoid
		// errors we make script flag the network object before calling these commands
		if(CTheScripts::Frack() && pEntity1 && pEntity1->IsNetworkClone() && !NetworkInterface::CanScriptModifyRemoteAttachment(*pEntity1))
		{
			scriptAssertf(0, "%s : ATTACH_ENTITY_TO_ENTITY - Cannot call this on a remotely owned entity (%s) (unless NETWORK_ALLOW_REMOTE_ATTACHMENT_MODIFICATION() is called first)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity1->GetNetworkObject()->GetLogName());
			pEntity1 = 0;
		}

		if (pEntity1 && pEntity2)
		{
			u32 nBasicAttachFlags = ATTACH_FLAG_ACTIVATE_ON_DETACH;
			if (bMarkAsNoLongerNeededWhenDetached)
			{
				nBasicAttachFlags |= ATTACH_FLAG_NOT_NEEDED_ON_DETACH;
			}

			CTheScripts::Frack();
			if (SCRIPT_VERIFY(!pEntity1->GetIsTypePed() || !((CPed*)pEntity1)->GetIsAttachedInCar(), "ATTACH_ENTITY_TO_ENTITY - Cannot attach a ped already in vehicle"))
			{
				Quaternion quatRotate;
				quatRotate.Identity();
				if(vecRotation.IsNonZero())
				{
					CScriptEulers::QuaternionFromEulers(quatRotate, DtoR * vecRotation, static_cast<EulerAngleOrder>(RotOrder));
				}

				if(NetworkInterface::IsGameInProgress() && pEntity1->GetIsTypePed())
				{
					bool allowPlayerCheck = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_LOCAL_PLAYER_ATTACH_CHECK", 0x8A3D8937), true);

					CPed* ped = SafeCast(CPed, pEntity1);
					if(allowPlayerCheck && ped->IsAPlayerPed() && ped->IsNetworkClone())
					{
						bool exception = false;
						u32 objectNameHash = pEntity2->GetBaseModelInfo()->GetModelNameHash();
						if( objectNameHash == MI_ROLLER_COASTER_CAR_1.GetName().GetHash() || 
							objectNameHash == MI_ROLLER_COASTER_CAR_2.GetName().GetHash())
						{
							exception = true;
						}

						if(!exception)
						{
							CNetGamePlayer* netGamePlayer = ped->GetNetworkObject() ? ped->GetNetworkObject()->GetPlayerOwner() : nullptr;
							CNetworkTelemetry::AppendPedAttached(1, ped->IsNetworkClone(), netGamePlayer ? netGamePlayer->GetPlayerAccountId() : 0);
							scriptAssertf(0, "%s : ATTACH_ENTITY_TO_ENTITY - Cannot call this on a remote player (%s) to attach to %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity1->GetNetworkObject()->GetLogName(), pEntity2->GetLogName());
							return;
						}
					}
				}

				if (pEntity1->GetIsTypePed() && !bUseBasicAttachIfPed)
				{
					nBasicAttachFlags |= ATTACH_STATE_PED|ATTACH_FLAG_INITIAL_WARP;

					if( bDetachWhenDead )
					{
						nBasicAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_DEATH;
					}
					if( bDetachWhenRagdoll )
					{
						nBasicAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
					}
					if( bActiveCollision )
					{
						nBasicAttachFlags |= ATTACH_FLAG_COL_ON;
					}
					if( !bAttachOffsetIsRelative )
					{
						nBasicAttachFlags |= ATTACH_FLAG_OFFSET_IS_ABSOLUTE;
					}

					((CPed*)pEntity1)->AttachPedToPhysical( pEntity2, (s16)Entity2BoneIndex, nBasicAttachFlags, &vecOffset, NULL, vecRotation.z*DtoR, vecRotation.y*DtoR );
				}
				else
				{
#if __BANK
					if(NetworkInterface::IsGameInProgress())
					{
						if(pEntity2->GetIsTypeVehicle() && pEntity2->PopTypeIsMission() && pEntity1->GetIsTypeObject() && pEntity1->PopTypeIsMission())
						{
							physicsDisplayf("[ATTACH DEBUG] Attach command %s to %s.", pEntity1->GetDebugNameFromObjectID(), pEntity2->GetDebugNameFromObjectID());
						}
					}
#endif

					nBasicAttachFlags |= ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP;

					if( bActiveCollision )
					{
						nBasicAttachFlags |= ATTACH_FLAG_COL_ON;
					}
					if( !bAttachOffsetIsRelative )
					{
						nBasicAttachFlags |= ATTACH_FLAG_OFFSET_IS_ABSOLUTE;
					}

					pEntity1->AttachToPhysicalBasic(pEntity2, Entity2BoneIndex, nBasicAttachFlags, &vecOffset, &quatRotate, Entity1BoneIndex);
				}
			}
		}
	}

	void CommandAttachEntityToEntity(int Entity1Index, int Entity2Index, int Entity2BoneIndex, const scrVector &scrVecOffset, const scrVector &scrVecRotation, bool bDetachWhenDead, bool bDetachWhenRagdoll, bool bActiveCollision, bool bUseBasicAttachIfPed, int RotOrder, bool bAttachOffsetIsRelative, bool bMarkAsNoLongerNeededWhenDetached)
	{
		Vector3 vecOffset = Vector3(scrVecOffset);
		Vector3 vecRotation = Vector3(scrVecRotation);

		CPhysical* pEntity1 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity1Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
        CPhysical* pEntity2 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity2Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		AttachEntityToEntity( pEntity1, pEntity2, (s16)-1, (s16)Entity2BoneIndex, vecOffset, vecRotation, bDetachWhenDead, bDetachWhenRagdoll, bActiveCollision, bUseBasicAttachIfPed, RotOrder, bAttachOffsetIsRelative, bMarkAsNoLongerNeededWhenDetached );
	}
	
	void CommandAttachEntityBoneToEntityBone( int Entity1Index, int Entity2Index, int Entity1BoneIndex, int Entity2BoneIndex, bool bActiveCollision, bool bUseBasicAttachIfPed )
	{
		CPhysical* pEntity1 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity1Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		CPhysical* pEntity2 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity2Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pEntity1 && pEntity2 )
		{
			Vector3 vecRotation = Vector3( 0.0f, 0.0f, 0.0f );
			Vector3 vecOffset = Vector3( 0.0f, 0.0f, 0.0f );

			Matrix34 myGlobalAttachBone;
			Matrix34 parentGlobalAttachBone;
			pEntity1->GetGlobalMtx( Entity1BoneIndex, myGlobalAttachBone );	
			pEntity2->GetGlobalMtx( Entity2BoneIndex, parentGlobalAttachBone );	

			vecRotation.x = myGlobalAttachBone.GetEulers().x * RtoD;
			
			if( Dot( myGlobalAttachBone.a, parentGlobalAttachBone.a ) < 0.0f )
			{
				vecRotation.y = -( 2.0f * myGlobalAttachBone.GetEulers().y * RtoD );
				vecRotation.z = 180.0f;
			}

			AttachEntityToEntity( pEntity1, pEntity2, (s16)Entity1BoneIndex, (s16)Entity2BoneIndex, vecOffset, vecRotation, false, false, bActiveCollision, bUseBasicAttachIfPed, 0, false, false );
		}
	}

	void CommandAttachEntityBoneToEntityBoneYForward( int Entity1Index, int Entity2Index, int Entity1BoneIndex, int Entity2BoneIndex, bool bActiveCollision, bool bUseBasicAttachIfPed )
	{
		CPhysical* pEntity1 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity1Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		CPhysical* pEntity2 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity2Index, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pEntity1 && pEntity2 )
		{
			Vector3 vecRotation = Vector3( 0.0f, 0.0f, 0.0f );
			Vector3 vecOffset = Vector3( 0.0f, 0.0f, 0.0f );

			Matrix34 myGlobalAttachBone;
			Matrix34 parentGlobalAttachBone;
			pEntity1->GetGlobalMtx( Entity1BoneIndex, myGlobalAttachBone );	
			pEntity2->GetGlobalMtx( Entity2BoneIndex, parentGlobalAttachBone );	

			vecRotation.y = myGlobalAttachBone.GetEulers().y * RtoD;
			if( Dot( myGlobalAttachBone.b, parentGlobalAttachBone.b ) < 0.0f )
			{
				vecRotation.z = 180.0f;
			}

			AttachEntityToEntity( pEntity1, pEntity2, (s16)Entity1BoneIndex, (s16)Entity2BoneIndex, vecOffset, vecRotation, false, false, bActiveCollision, bUseBasicAttachIfPed, 0, false, false );
		}
	}

	void CommandAttachEntityToEntityPhysicallyOverrideInverseMass(int Entity1Index, int Entity2Index, int Entity1BoneIndex, int Entity2BoneIndex, const scrVector &scrVecEntity2Offset, const scrVector &scrVecEntity1Offset, const scrVector &scrVecRotation, float fPhysicalStrength, bool bRotationConstraint, bool bDoInitialWarp, bool bCollideWithObject, bool bAddInitialSeperation, int RotOrder, float invMassScaleA, float invMassScaleB )
	{
		Vector3 vecEntity1Offset = Vector3(scrVecEntity1Offset);
		Vector3 vecEntity2Offset = Vector3(scrVecEntity2Offset);
		Vector3 vecRotation = Vector3(scrVecRotation);

		CPhysical* pEntity1 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity1Index);
		CPhysical* pEntity2 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(Entity2Index);

		if (pEntity1 && pEntity2)
		{
			if (SCRIPT_VERIFY(!pEntity1->GetIsTypePed() || !((CPed*)pEntity1)->GetIsAttachedInCar(), "ATTACH_ENTITY_TO_ENTITY_PHYSICALLY - Cannot attach a ped already in vehicle"))
			{
				Quaternion quatRotate;
				quatRotate.Identity();
				if(vecRotation.IsNonZero())
				{
					CScriptEulers::QuaternionFromEulers(quatRotate, DtoR * vecRotation, static_cast<EulerAngleOrder>(RotOrder));
				}

				u32 nPhysicalAttachFlags = ATTACH_FLAG_POS_CONSTRAINT|ATTACH_FLAG_ACTIVATE_ON_DETACH;
				
				if (pEntity1->GetIsTypePed())
				{
					nPhysicalAttachFlags |= ATTACH_STATE_RAGDOLL;
				}
				else
				{
					nPhysicalAttachFlags |= ATTACH_STATE_PHYSICAL;
				}

				if(bRotationConstraint)
					nPhysicalAttachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;

				if(bCollideWithObject)
					nPhysicalAttachFlags |= ATTACH_FLAG_DO_PAIRED_COLL;

				if(bDoInitialWarp)
					nPhysicalAttachFlags |= ATTACH_FLAG_INITIAL_WARP;

				pEntity1->AttachToPhysicalUsingPhysics(pEntity2, (s16)Entity2BoneIndex, (s16)Entity1BoneIndex, nPhysicalAttachFlags, &vecEntity2Offset, &quatRotate, &vecEntity1Offset, fPhysicalStrength, bAddInitialSeperation, invMassScaleA, invMassScaleB );
			}
		}
	}


	void CommandAttachEntityToEntityPhysically(int Entity1Index, int Entity2Index, int Entity1BoneIndex, int Entity2BoneIndex, const scrVector &scrVecEntity2Offset, const scrVector &scrVecEntity1Offset, const scrVector &scrVecRotation, float fPhysicalStrength, bool bRotationConstraint, bool bDoInitialWarp, bool bCollideWithObject, bool bAddInitialSeperation, int RotOrder)
	{
		CommandAttachEntityToEntityPhysicallyOverrideInverseMass( Entity1Index, Entity2Index, Entity1BoneIndex, Entity2BoneIndex, scrVecEntity2Offset, scrVecEntity1Offset, scrVecRotation, fPhysicalStrength, bRotationConstraint, bDoInitialWarp, bCollideWithObject, bAddInitialSeperation, RotOrder, 1.0f, 1.0f );
	}


	void CommandAttachEntityToWorldPhysically(int EntityIndex, int EntityBoneIndex, const scrVector &scrVecWorldPosition, const scrVector & scrVecOffsetFromBone, float fPhysicalStrength, bool bFixRotation,
		float fRotMinX, float fRotMinY, float fRotMinZ, float fRotMaxX, float fRotMaxY, float fRotMaxZ)
	{
		Vector3 vecWorldPosition = Vector3(scrVecWorldPosition);
		Vector3 vecOffsetFromBone = Vector3(scrVecOffsetFromBone);
		Vector3 vecRotMinLimits = Vector3(fRotMinX, fRotMinY, fRotMinZ);
		Vector3 vecRotMaxLimits = Vector3(fRotMaxX, fRotMaxY, fRotMaxZ);

		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			if (SCRIPT_VERIFY(!pEntity->GetIsTypePed() || !((CPed*)pEntity)->GetIsAttachedInCar(), "ATTACH_ENTITY_TO_WORLD_PHYSICALLY - Cannot attach a ped already in a vehicle"))
			{
				pEntity->AttachToWorldUsingPhysics(static_cast<s16>(EntityBoneIndex), bFixRotation, vecWorldPosition, &vecOffsetFromBone, fPhysicalStrength, /* activate on detach */ true, &vecRotMinLimits, &vecRotMaxLimits);
			}
		}	
	}

	void CommandAttachEntityBoneToBonePhysically(int EntityIndex, int Bone1Index, int Bone2Index, const scrVector & scrVecBone1Offset, const scrVector & scrVecBone2Offset, float fMaxDistance)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			fragInst* frag = pEntity->GetFragInst();
			if(frag)
			{
				int component1 = frag->GetComponentFromBoneIndex(Bone1Index);
				int component2 = frag->GetComponentFromBoneIndex(Bone2Index);
				if(component1 >= 0 && component2 >= 0)
				{
					scriptAssertf(component1 != component2, "%s:ATTACH_ENTITY_BONE_TO_BONE - Trying to attach component %i to itself on entity index %i", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component1, EntityIndex);
					phConstraintDistance::Params params;
					params.instanceA = frag;
					params.instanceB = frag;
					params.componentA = static_cast<u16>(component1);
					params.componentB = static_cast<u16>(component2);
					params.constructUsingLocalPositions = true;
					params.localPosA = VECTOR3_TO_VEC3V(Vector3(scrVecBone1Offset));
					params.localPosB = VECTOR3_TO_VEC3V(Vector3(scrVecBone2Offset));
					params.maxDistance = fMaxDistance;
					params.usePushes = false;
					params.separateBias = 0.f;
					PHCONSTRAINT->Insert(params);
				}
			}
		}
	}

	// Get a bone index from bone name. Return -1 if we can't find one.
	int CommandGetEntityBoneIndexByName(int EntityIndex, const char* zBoneName)
	{
		int boneIndex = -1;

		// Hash the bone name to get bone tag.
		u16 boneId = atHash16U(zBoneName);

		const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);
		if(pEntity)
		{
			if(static_cast<const CDynamicEntity*>(pEntity)->GetSkeletonData().ConvertBoneIdToIndex(boneId, boneIndex))
			{
				return boneIndex;
			}
		}

		return -1;
	}

	void CommandClearEntityLastDamageEntity(int EntityIndex)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			if (pEntity->GetNetworkObject())
			{
				scriptDebugf1("%s:CLEAR_ENTITY_LAST_DAMAGE_ENTITY - Command called for '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject()->GetLogName());

				NetworkInterface::ClearLastDamageObject(pEntity->GetNetworkObject());
			}

			pEntity->ResetWeaponDamageInfo();
		}
	}

	void CommandDeleteEntity(int &EntityIndex)
	{
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

		if (pEntity)
		{
			CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			if (SCRIPT_VERIFY(pExtension, "DELETE_ENTITY - The entity is not a script entity") &&
				SCRIPT_VERIFY(pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler(), "DELETE_ENTITY - The entity was not created by this script"))
			{
				if (pEntity->GetIsTypePed())
				{
					ped_commands::DeleteScriptPed(static_cast<CPed*>(pEntity));
				}
				else if (pEntity->GetIsTypeVehicle())
				{
					vehicle_commands::DeleteScriptVehicle(static_cast<CVehicle*>(pEntity));
				}
				else if (pEntity->GetIsTypeObject())
				{
					object_commands::DeleteScriptObject(static_cast<CObject*>(pEntity));
				}
				else
				{
					scriptAssertf(0, "%s:DELETE_ENTITY - Unrecognised entity type", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
		}

		EntityIndex = 0;
	}

	void CommandDetachEntity(int EntityIndex, bool bApplyVelocity, bool bNoCollisionUntilClear)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

        // script generally shouldn't be calling this command on a clone, but need to do this is certain cases. To avoid
        // errors we make script flag the network object before calling these commands
        if(pEntity && pEntity->IsNetworkClone() && !NetworkInterface::CanScriptModifyRemoteAttachment(*pEntity))
        {
            scriptAssertf(0, "%s:DETACH_ENTITY - Cannot call this on a remotely owned entity (unless NETWORK_ALLOW_REMOTE_ATTACHMENT_MODIFICATION() is called first)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
            pEntity = 0;
        }

		if (pEntity)
		{
			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				if(NetworkInterface::IsInSpectatorMode() && CGameWorld::FindLocalPlayer() == pPed)
				{
#if !__FINAL
					scriptDebugf1("Script %s DETACH_ENTITY local spectator '%s' - pEntity 0x%p - bNoCollisionUntilClear %s, bApplyVelocity %s", 
						CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bNoCollisionUntilClear ? "TRUE" : "FALSE", bApplyVelocity ? "TRUE" : "FALSE");
					scriptAssertf(0, "Script %s DETACH_ENTITY local spectator '%s' - pEntity 0x%p - bNoCollisionUntilClear %s, bApplyVelocity %s", 
						CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bNoCollisionUntilClear ? "TRUE" : "FALSE", bApplyVelocity ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
#endif
					return;
				}
			}
			if (SCRIPT_VERIFY(!pEntity->GetIsTypePed() || !((CPed*)pEntity)->GetIsAttachedInCar(), "DETACH_ENTITY - Cannot detach a ped already in a vehicle"))
			{
				if(pEntity->GetIsAttached())
				{
					// Force an update of attachments as the attachments are not processed until after the script
					// So if we dont do this here the object will be placed in the correct position for the previous frame
					pEntity->ProcessAllAttachments();

					u16 nDetachFlags = DETACH_FLAG_ACTIVATE_PHYSICS;

					if (pEntity->GetIsTypePed())
					{
						// Do not let script disable collision with the ground physical, it should never be necessary. 
						// This lets us avoid exposing the different types of attachment to script
						if(bNoCollisionUntilClear && !static_cast<CPed*>(pEntity)->GetIsAttachedToGround())
						{
							nDetachFlags |= DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR;
						}
					}
					else
					{
						if(pEntity->GetIsTypeObject() && bNoCollisionUntilClear)
						{
							pEntity->SetNoCollision(pEntity->GetAttachParentForced(), NO_COLLISION_RESET_WHEN_NO_BBOX);
						}

						if(bApplyVelocity)
							nDetachFlags |= DETACH_FLAG_APPLY_VELOCITY;
					}

					pEntity->DetachFromParent(nDetachFlags);
				}
			}
		}
	}

	void CommandFreezeEntityPosition(int EntityIndex, bool FreezeFlag)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				if( !FreezeFlag &&
					CGameWorld::FindLocalPlayer() == pPed &&
					NetworkInterface::IsInSpectatorMode() )
				{
#if !__FINAL
					scriptDebugf1("Script %s FREEZE_ENTITY_POSITION On local spectator '%s' - pEntity 0x%p - FreezeFlag %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, FreezeFlag ? "TRUE" : "FALSE");
					scriptAssertf(0, "Script %s FREEZE_ENTITY_POSITION On local spectator '%s' - pEntity 0x%p - FreezeFlag %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, FreezeFlag ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
#endif
					return;
				}
			}
#if __ASSERT
			if(pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);
				scriptAssertf(!FreezeFlag || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle), "%s:FREEZE_ENTITY_POSITION - Can't freeze a ped while in a car", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
#endif //__ASSERT
			scriptAssertf(!FreezeFlag || (!pEntity->GetIsAttached() || (pEntity->GetIsTypeVehicle()&&static_cast<CVehicle*>(pEntity)->InheritsFromTrailer())),
				"%s:FREEZE_ENTITY_POSITION - Can't freeze an attached entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());

#if !__FINAL
			if(NetworkInterface::IsGameInProgress())
			{
				bool bWasFrozen = pEntity->GetIsAnyFixedFlagSet();

				if(bWasFrozen != FreezeFlag)
				{
					if(FreezeFlag)
					{
						scriptDebugf1("%s: Freezing position for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetLogName());
					}
					else
					{
						scriptDebugf1("%s: Un-freezing position for %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetLogName());
					}

					scrThread::PrePrintStackTrace();
				}
			}
#endif // !__FINAL

			if (pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

				// special case for trains, are those created via CREATE_VEHICLE
				bool bTrainScriptVehicle = pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN && ((CTrain*)pVehicle)->m_nTrainFlags.bCreatedAsScriptVehicle;

				if(pVehicle->GetVehicleType()!=VEHICLE_TYPE_TRAIN || bTrainScriptVehicle)
				{
					pVehicle->SetFixedPhysics(FreezeFlag);
				}
			}
			else if (pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);

				const bool bAlreadyFrozen = pObject->GetIsAnyFixedFlagSet();
				CBaseModelInfo * pModelInfo = pObject->GetBaseModelInfo();

				scriptAssertf(FreezeFlag || !pModelInfo->GetIsFixed(), "%s:FREEZE_ENTITY_POSITION - can't unfreeze an object that the map says is fixed", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				pObject->SetFixedPhysics(FreezeFlag);

				if(FreezeFlag != bAlreadyFrozen && (pModelInfo->GetUsesDoorPhysics() != 0))
				{
					// Will need to update the pathserver's representation of this object.
					if(pObject->IsInPathServer())
					{
						CPathServerGta::UpdateDynamicObject(pObject, true);
					}
					// Attempt to add the object, if we have failed to before
					else if(pObject->WasUnableToAddToPathServer())
					{
						CPathServerGta::MaybeAddDynamicObject(pObject);
					}
				}		
			}
			else
			{
				pEntity->SetFixedPhysics(FreezeFlag);
			}
		}
	}

	void CommandSetEntityShouldFreezeWaitingOnCollision(int EntityIndex, bool bShouldFreeze)
	{
		CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(pPhysical)
		{
			pPhysical->SetAllowFreezeWaitingOnCollision(bShouldFreeze);

			if (pPhysical->GetIsTypeVehicle())
			{
				CVehicle* veh = (CVehicle*)pPhysical;
				veh->m_nVehicleFlags.bShouldFixIfNoCollision = bShouldFreeze;
			}
		}
	}

	bool CommandIsAllowFreezeWaitingOnCollisionSet(int EntityIndex)
	{
		CPhysical *pPhysical = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(pPhysical)
		{
			return pPhysical->GetAllowFreezeWaitingOnCollision();
		}

		return false;
	}

	bool CommandPlayEntityAnim( int EntityIndex, const char* AnimName, const char* AnimDictName, float fBlendDelta, bool bLoop, bool bHoldLastFrame, bool UNUSED_PARAM(bDriveToPose), float fStartPhase, int nFlags )
	{
		bool bResult = false;

		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID< CPhysical >(EntityIndex);

		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				SCRIPT_ASSERT(0, "PLAY_ENTITY_ANIM - Not supported on peds. Use TaskPlayAnim instead");
			}
			else if (pEntity->GetIsTypeObject() || pEntity->GetIsTypeVehicle())
			{
				if (!pEntity->GetDrawable())
				{
					pEntity->CreateDrawable();
				}

				if (SCRIPT_VERIFY(pEntity->GetDrawable(), "PLAY_ENTITY_ANIM - Failed to create a drawable."))
				{
					if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "PLAY_ENTITY_ANIM - Entity does not have a skeleton data"))
					{
#if ENABLE_FRAG_OPTIMIZATION
						if (pEntity->GetIsTypeVehicle())
						{
							CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
							pVeh->GiveFragCacheEntry(true);
						}
#endif
						// Lazily create the skeleton if it doesn't exist (not for objects!)
						if (!pEntity->GetSkeleton())
						{
							pEntity->CreateSkeleton();
						}

						if (SCRIPT_VERIFY( pEntity->GetSkeleton(), "PLAY_ENTITY_ANIM - Failed to create a skeleton"))
						{
							// Lazily create the anim director if it doesn't exist (not for objects!)
							if ( !pEntity->GetAnimDirector())
							{
								pEntity->CreateAnimDirector(*pEntity->GetDrawable());
							}

							if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "PLAY_ENTITY_ANIM - Failed to create the anim director"))
							{
								// Check the animation
								if (SCRIPT_VERIFY(fwAnimManager::GetClipIfExistsByName(AnimDictName, AnimName), "PLAY_ENTITY_ANIM - Couldn't find animation. Has it been loaded?"))
								{
									if (!SCRIPT_VERIFY(fStartPhase >= 0.0f && fStartPhase <= 1.0f, "PLAY_ENTITY_ANIM - Start Phase is not in 0.0 - 1.0 range, setting it to 0.0."))
									{
										fStartPhase = 0.0f;
									}

									eScriptedAnimFlagsBitSet flags;
									flags.BitSet().SetBits(nFlags);

									if (bLoop)
										flags.BitSet().Set(AF_LOOPING);
									if (bHoldLastFrame)
										flags.BitSet().Set(AF_HOLD_LAST_FRAME);

									// TODO - Add drive to pose support!

									CTaskScriptedAnimation* pTask = rage_new CTaskScriptedAnimation(AnimDictName, AnimName, CTaskScriptedAnimation::kPriorityLow, BONEMASK_ALL, fwAnimHelpers::CalcBlendDuration(fBlendDelta), fwAnimHelpers::CalcBlendDuration(fBlendDelta), -1, flags, fStartPhase );

									if (pEntity->GetIsTypeObject())
									{
										CObject *pObject = static_cast< CObject * >(pEntity);
										pObject->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
										bResult = true;

										// We want the first anim update to happen the same frame as the script request
										if (pObject->GetObjectIntelligence())
										{
											pObject->GetObjectIntelligence()->ForcePostCameraTaskUpdate();
										}
									}

									if (pEntity->GetIsTypeVehicle())
									{
										CVehicle *pVehicle = static_cast< CVehicle * >(pEntity);
										pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_SECONDARY, pTask, VEHICLE_TASK_SECONDARY_ANIM, true);
 										bResult = true;

										// Make sure the vehicle is simulating and articulated, otherwise drive to pose wont work.
										fragInstGta *pFragInst = pVehicle->GetVehicleFragInst();
										if (pFragInst)
										{
											fragCacheEntry *pFragCacheEntry = pFragInst->GetCacheEntry();
											if (pFragCacheEntry)
											{
												// Force allocate the latched joint frags 
												pFragInst->ForceArticulatedColliderMode();

												if (pFragCacheEntry->GetHierInst()->body != NULL)
												{
													pVehicle->SetDriveMusclesToAnimation(true);
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

		return bResult;
	}

	void CommandForceEntityAiAnimUpdate( int iEntityID )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
		if (pEntity)
		{
			if (SCRIPT_VERIFY(!pEntity->GetIsTypePed(), "FORCE_ENTITY_AI_AND_ANIMATION_UPDATE should not be used on peds! Use FORCE_PED_AI_AND_ANIMATION_UPDATE instead."))
			{
				scriptDisplayf("Force post camera ai/anim update called from script: command:FORCE_ENTITY_AI_ANIMATION_UPDATE, entity: %s(%p), frame:%d", pEntity->GetModelName(), pEntity, fwTimer::GetFrameCount());

				if (pEntity->GetIsTypeObject())
				{
					CObject* pObj = static_cast<CObject*>(pEntity);
					pObj->ForcePostCameraAiUpdate(true);
					pObj->ForcePostCameraAnimUpdate(true, true);
				}
				else if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
					pVeh->ForcePostCameraAiUpdate(true);
					pVeh->ForcePostCameraAnimUpdate(true, true);
				}
			}
		}
	}

	bool CommandPlaySynchronizedEntityAnim( int iEntityID, int iSceneID, const char * pAnimName, const char * pAnimDictName, float fBlendInDelta, float fBlendOutDelta = NORMAL_BLEND_OUT_DELTA, s32 iFlags = 0 , float fMoverBlendInDelta = INSTANT_BLEND_IN_DELTA )
	{
		bool bResult = false;

		//is this a  valid scene ID
		if (SCRIPT_VERIFY(fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(iSceneID)), "PLAY_SYNCHRONIZED_ENTITY_ANIM - scene ID is invalid"))
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityID);
			if (pEntity)
			{
                // this script command is not approved for use in network scripts
                if (SCRIPT_VERIFY(!pEntity->GetNetworkObject(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - This script command is not allowed on networked entities!"))
                {
					if (SCRIPT_VERIFY(!pEntity->GetIsTypePed(), "PLAY_SYNCHRONIZED_ENTITY_ANIM should not be used on peds! Use Task synchronized scene instead."))
					{
						if (SCRIPT_VERIFY(!pEntity->GetIsAttached(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - cannot be used on an attached entity!"))
						{
							if (SCRIPT_VERIFY_TWO_STRINGS(pEntity->GetDrawable(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - Entity (%s) does not have a drawable", pEntity->GetModelName()))
							{
								if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - Entity does not have a skeleton data"))
								{
									const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);
									// Check the animation
									if (SCRIPT_VERIFYF(pClip, "PLAY_SYNCHRONIZED_ENTITY_ANIM - Couldn't find animation \"%s\\%s\". Has it been loaded?", pAnimDictName, pAnimName))
									{
#if ENABLE_FRAG_OPTIMIZATION
										if (pEntity->GetIsTypeVehicle())
										{
											CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
											pVeh->GiveFragCacheEntry(true);
										}
#endif
										// Lazily create the skeleton if it doesn't exist
										if (!pEntity->GetSkeleton())
										{
											pEntity->CreateSkeleton();
										}
										if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - Failed to create a skeleton"))
										{	
											// Lazily create the anim director if it doesn't exist
											if (!pEntity->GetAnimDirector())
											{
												pEntity->CreateAnimDirector(*pEntity->GetDrawable());
											}
											if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "PLAY_SYNCHRONIZED_ENTITY_ANIM - Failed to create an animdirector"))
											{
												// Object has to be on the process control list for the animation director to be updated
												if (!pEntity->GetIsOnSceneUpdate())
												{
													pEntity->AddToSceneUpdate();
												}

												eSyncedSceneFlagsBitSet& sceneFlags = reinterpret_cast<eSyncedSceneFlagsBitSet&>(iFlags);

												if (pEntity->GetIsTypeObject())
												{
													CObject* pObj = static_cast<CObject*>(pEntity);

													CTaskSynchronizedScene *pTask = rage_new CTaskSynchronizedScene(fwSyncedSceneId(iSceneID), atPartialStringHash(pAnimName), pAnimDictName, fBlendInDelta, fBlendOutDelta, sceneFlags, CTaskSynchronizedScene::RBF_NONE, fMoverBlendInDelta );
													pObj->SetTask(pTask);

												}
												else if (pEntity->GetIsTypeVehicle())
												{
													CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
													if(pVeh->GetVehicleType() == VEHICLE_TYPE_TRAIN)
													{
														CTrain* pTrain = static_cast<CTrain*>(pVeh);
														pTrain->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
														pTrain->m_nTrainFlags.bIsSyncedSceneControlled = true;
													}

													CTaskSynchronizedScene *pTask = rage_new CTaskSynchronizedScene(fwSyncedSceneId(iSceneID), atPartialStringHash(pAnimName), pAnimDictName, fBlendInDelta, fBlendOutDelta, sceneFlags, CTaskSynchronizedScene::RBF_NONE, fMoverBlendInDelta );
													pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(pTask, VEHICLE_TASK_SECONDARY_ANIM); 
												}

												bResult = true;
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
		return bResult;
	}

	bool CommandPlaySynchronizedMapEntityAnim( const scrVector &scrVecNewCoors, float Radius, int ObjectModelHashKey, int iSceneID, const char * pAnimName, const char * pAnimDictName, float fBlendDelta, float fBlendOutDelta = NORMAL_BLEND_OUT_DELTA, s32 flags = 0 , float fMoverBlendInDelta = INSTANT_BLEND_IN_DELTA )
	{
		Vector3 VecNewCoors = Vector3 (scrVecNewCoors);

		CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey);
		if (SCRIPT_VERIFY(pClosestObj, "PLAY_SYNCHRONIZED_MAP_ENTITY_ANIM - Could not find map object!"))
		{
			if (SCRIPT_VERIFY(pClosestObj->GetIsDynamic(), "PLAY_SYNCHRONIZED_MAP_ENTITY_ANIM - map object is not dynamic!"))
			{
				s32 iEntityID = CTheScripts::GetGUIDFromEntity((CPhysical &) *pClosestObj);

				if (SCRIPT_VERIFY(!NetworkUtils::IsNetworkClone(pClosestObj), "PLAY_SYNCHRONIZED_MAP_ENTITY_ANIM - Cannot call this command on an entity owned by another machine!"))
				{
					bool result = CommandPlaySynchronizedEntityAnim(iEntityID, iSceneID,  pAnimName,  pAnimDictName, fBlendDelta, fBlendOutDelta, flags, fMoverBlendInDelta );
#if __DEV
					if (!result)
					{
						const char *pModelName = CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetModelName();
						if (!pModelName) { pModelName = "UNKNOWN MODEL"; }
						SCRIPT_ASSERTF(false, "PLAY_SYNCHRONIZED_MAP_ENTITY_ANIM failed on model called : %s", pModelName);
					}
#endif //__DEV
					return result;
				}
			}
		}

		return false; 
	}

	void CommandStopEntityAnim(int iEntityID, const char* AnimName, const char* AnimDictName, float fBlendDelta)
	{
		scriptAssertf(fBlendDelta<0.0f, "STOP_ENTITY_ANIM - Blend out delta must be less than 0.0f");

		CTaskScriptedAnimation::ePlaybackPriority priority;

		CTaskScriptedAnimation* pTask = FindScriptedAnimTask(iEntityID, true);
		if (pTask)
		{
			s32 index;
			if (pTask->FindFirstInstanceOfClip(AnimDictName, AnimName, priority, index))
			{
				pTask->StopPlayback(priority, fwAnimHelpers::CalcBlendDuration(fBlendDelta));
				return;
			}
		}

		pTask = FindScriptedAnimTask(iEntityID, true, false);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(AnimDictName, AnimName, priority, index))
			{
				pTask->StopPlayback(priority, fwAnimHelpers::CalcBlendDuration(fBlendDelta));
				return;
			}
		}

		CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToModify(iEntityID, AnimDictName, AnimName, "STOP_ENTITY_ANIM");

		if (pGenericClipHelper)
		{
			pGenericClipHelper->BlendOutClip(fBlendDelta);
		}
	}

	bool CommandStopSynchronizedEntityAnim( int iEntityID, float fBlendOutDelta, bool UNUSED_PARAM(bActivatePhysics) )
	{
		bool bResult = false;
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "STOP_SYNCHRONIZED_ENTITY_ANIM - This script command is not allowed in network game scripts!"))
		{
			CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityID);
			if (pEntity)
			{
				if (SCRIPT_VERIFY(pEntity->GetDrawable(), "STOP_SYNCHRONIZED_ENTITY_ANIM - Entity does not have a drawable"))
				{
					if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "STOP_SYNCHRONIZED_ENTITY_ANIM - Entity does not have a skeleton data"))
					{
						if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "STOP_SYNCHRONIZED_ENTITY_ANIM - Entity has no skeleton"))
						{	
							if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "STOP_SYNCHRONIZED_ENTITY_ANIM - Entity has no animdirector"))
							{
								if (pEntity->GetIsTypeObject())
								{
									CObject* pObj = static_cast<CObject*>(pEntity);

									if (pObj->GetObjectIntelligence())
									{
										CTaskSynchronizedScene *pTask = static_cast<CTaskSynchronizedScene*>(pObj->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
										
										if (pTask)
										{
											pTask->SetBlendOutDelta(fBlendOutDelta);
											pObj->GetObjectIntelligence()->ClearTasks();
										}
									}
								}
								else if (pEntity->GetIsTypeVehicle())
								{
									CVehicle* pVeh = static_cast<CVehicle*>(pEntity);

									if (pVeh->GetIntelligence()->GetTaskManager())
									{
										CTaskSynchronizedScene *pTask = static_cast<CTaskSynchronizedScene*>(pVeh->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SYNCHRONIZED_SCENE));

										if (pTask)
										{
											// set the blend out duration
											pTask->SetBlendOutDelta(fBlendOutDelta);
											pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);

											// just set the wheel compression, but don't move the vehicle
											pVeh->PlaceOnRoadAdjust(true);
										}
									}
								}
								bResult = true;
							}
						}
					}
				}
			}
		}
		return bResult;
	}

	bool CommandStopSynchronizedMapEntityAnim( const scrVector &scrVecNewCoors, float Radius, int ObjectModelHashKey, float fBlendOutDelta  )
	{
		Vector3 VecNewCoors = Vector3 (scrVecNewCoors);

		CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey);
		if (SCRIPT_VERIFY(pClosestObj, "STOP_SYNCHRONIZED_MAP_ENTITY_ANIM - Could not find map object!"))
		{
			if (SCRIPT_VERIFY(pClosestObj->GetIsDynamic(), "STOP_SYNCHRONIZED_MAP_ENTITY_ANIM - map object is not dynamic!"))
			{
				s32 iEntityID = CTheScripts::GetGUIDFromEntity((CPhysical &) *pClosestObj);

				if (SCRIPT_VERIFY(!NetworkUtils::IsNetworkClone(pClosestObj), "STOP_SYNCHRONIZED_MAP_ENTITY_ANIM - Cannot call this command on an entity owned by another machine!"))
				{
					bool result = CommandStopSynchronizedEntityAnim(  iEntityID, fBlendOutDelta, true ); 

#if __DEV
					if (!result)
					{
						const char *pModelName = CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetModelName();
						if (!pModelName) { pModelName = "UNKNOWN MODEL"; }
						SCRIPT_ASSERTF(false, "STOP_SYNCHRONIZED_MAP_ENTITY_ANIM failed on model called : %s", pModelName);
					}
#endif //__DEV

					return result;
				}
			}
		}
		return false; 
	}

	//	returns true if an event with the specified name is currently firing in an anim on the entity
	bool CommandHasAnimEventFired(s32 EntityIndex, s32 EventHash)
	{
		bool bResult = false;

		const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);
		if (pEntity)
		{
			if (SCRIPT_VERIFY(pEntity->GetDrawable(), "HAS_ANIM_EVENT_FIRED - Entity does not have a drawable"))
			{
				if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "HAS_ANIM_EVENT_FIRED - Entity does not have a skeleton data"))
				{
					if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "HAS_ANIM_EVENT_FIRED - Entity has no skeleton"))
					{
						if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "HAS_ANIM_EVENT_FIRED - Entity has no animdirector"))
						{
							const crProperty* pProp = pEntity->GetAnimDirector()->FindScriptVisiblePropertyInMoveNetworks((u32)EventHash);
							if (pProp)
							{
								return true;
							}

							if(CutSceneManager::GetInstance())
							{
								bResult = CutSceneManager::GetInstance()->HasScriptVisibleTagPassedForEntity(pEntity, EventHash); 
							}
						}
					}
				}
			}
		}

		return bResult;
	}

	bool CommandFindAnimEventPhase(const char *pAnimDictName, const char *pAnimName, const char *pEventName, float &ReturnStartPhase, float &ReturnEndPhase)
	{
		bool bResult = false;

		strLocalIndex clipDictionarySlot = strLocalIndex(g_ClipDictionaryStore.FindSlot(pAnimDictName));
		if (SCRIPT_VERIFY(g_ClipDictionaryStore.IsValidSlot(clipDictionarySlot), "FIND_ANIM_EVENT_PHASE - Anim Dictionary does not exist"))
		{
			if (SCRIPT_VERIFY(g_ClipDictionaryStore.HasObjectLoaded(clipDictionarySlot), "FIND_ANIM_EVENT_PHASE - Anim Dictionary has not been streamed in"))
			{
				const crClipDictionary *clipDictionary = g_ClipDictionaryStore.Get(clipDictionarySlot);
				if (SCRIPT_VERIFY(clipDictionary, "FIND_ANIM_EVENT_PHASE - Anim Dictionary does not exist"))
				{
					const crClip *clip = clipDictionary->GetClip(pAnimName);
					if (SCRIPT_VERIFY(clip, "FIND_ANIM_EVENT_PHASE - Anim does not exist"))
					{
						static CClipEventTags::Key visibleToScriptKey("VisibleToScript",0xF301E135);
						static CClipEventTags::Key eventKey("Event",0xADCACCD6);

						const crTag *tag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(clip, visibleToScriptKey, eventKey, atHashString(pEventName));
						if (tag)
						{
							ReturnStartPhase = tag->GetStart();
							ReturnEndPhase = tag->GetEnd();

							bResult = true;
						}
					}
				}
			}
		}

		return bResult;
	}

	bool CommandFindAnimEventInPhaseRange(const char *pAnimDictName, const char *pAnimName, const char *pEventName, float SearchStartPhase, float SearchEndPhase, float &ReturnStartPhase, float &ReturnEndPhase)
	{
		bool bResult = false;

		strLocalIndex clipDictionarySlot = strLocalIndex(g_ClipDictionaryStore.FindSlot(pAnimDictName));
		if (SCRIPT_VERIFY(g_ClipDictionaryStore.IsValidSlot(clipDictionarySlot), "FIND_ANIM_EVENT_PHASE - Anim Dictionary does not exist"))
		{
			if (SCRIPT_VERIFY(g_ClipDictionaryStore.HasObjectLoaded(clipDictionarySlot), "FIND_ANIM_EVENT_PHASE - Anim Dictionary has not been streamed in"))
			{
				const crClipDictionary *clipDictionary = g_ClipDictionaryStore.Get(clipDictionarySlot);
				if (SCRIPT_VERIFY(clipDictionary, "FIND_ANIM_EVENT_PHASE - Anim Dictionary does not exist"))
				{
					const crClip *clip = clipDictionary->GetClip(pAnimName);
					if (SCRIPT_VERIFY(clip, "FIND_ANIM_EVENT_PHASE - Anim does not exist"))
					{
						static CClipEventTags::Key visibleToScriptKey("VisibleToScript", 0xF301E135);
						static CClipEventTags::Key eventKey("Event", 0xADCACCD6);

						const crTag *tag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(clip, visibleToScriptKey, eventKey, atHashString(pEventName), SearchStartPhase, SearchEndPhase);
						if (tag)
						{
							ReturnStartPhase = tag->GetStart();
							ReturnEndPhase = tag->GetEnd();

							bResult = true;
						}
					}
				}
			}
		}

		return bResult;
	}

	////	Returns true if an event with the specified name is currently firing in an anim on the entity, and the 
	////	event has a bool type attribute with the given name. Fills OutValue with the value of the attribute
	//bool CommandGetIntAttributeFromAnimEvent(int EntityIndex, const char* EventName, const char * AttributeName, int& OutValue)
	//{
	//	bool bResult = false;

	//	// this script command is not approved for use in network scripts
	//	if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - This script command is not allowed in network game scripts!"))
	//	{
	//		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<fwEntity>(EntityIndex);
	//		if (pEntity)
	//		{
	//			if (SCRIPT_VERIFY(pEntity->GetDrawable(), "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a drawable"))
	//			{
	//				if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a skeleton data"))
	//				{
	//					if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no skeleton"))
	//					{	
	//						if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no animdirector"))
	//						{
	//							const crProperty* pProp = pEntity->GetAnimDirector()->FindScriptVisiblePropertyInMoveNetworks(crProperty::CalcKey(EventName));
	//							if (pProp)
	//							{
	//								const crPropertyAttribute* pAttrib = pProp->GetAttribute(AttributeName);
	//								if (SCRIPT_VERIFY(pAttrib && pAttrib->GetType()==crPropertyAttribute::kTypeInt, "GET_INT_ATTRIBUTE_FROM_ANIM_EVENT - Attribute does not exist in this event, or has the wrong type"))
	//								{
	//									const crPropertyAttributeInt* pAttribTyped = static_cast<const crPropertyAttributeInt*>(pAttrib);
	//									OutValue = pAttribTyped->GetInt();
	//									bResult = true;
	//								}
	//							}
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//	return bResult;
	//}

	////	Returns true if an event with the specified name is currently firing in an anim on the entity, and the 
	////	event has a float type attribute with the given name. Fills OutValue with the value of the attribute
	//bool CommandGetFloatAttributeFromAnimEvent(int EntityIndex, const char* EventName, const char * AttributeName, float& OutValue)
	//{
	//	bool bResult = false;

	//	// this script command is not approved for use in network scripts
	//	if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - This script command is not allowed in network game scripts!"))
	//	{
	//		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<fwEntity>(EntityIndex);
	//		if (pEntity)
	//		{
	//			if (SCRIPT_VERIFY(pEntity->GetDrawable(), "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a drawable"))
	//			{
	//				if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a skeleton data"))
	//				{
	//					if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no skeleton"))
	//					{	
	//						if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no animdirector"))
	//						{
	//							const crProperty* pProp = pEntity->GetAnimDirector()->FindScriptVisiblePropertyInMoveNetworks(crProperty::CalcKey(EventName));
	//							if (pProp)
	//							{
	//								const crPropertyAttribute* pAttrib = pProp->GetAttribute(AttributeName);
	//								if (SCRIPT_VERIFY(pAttrib && pAttrib->GetType()==crPropertyAttribute::kTypeFloat, "GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT - Attribute does not exist in this event, or has the wrong type"))
	//								{
	//									const crPropertyAttributeFloat* pAttribTyped = static_cast<const crPropertyAttributeFloat*>(pAttrib);
	//									OutValue = pAttribTyped->GetFloat();
	//									bResult = true;
	//								}
	//							}
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//	return bResult;
	//}

	////	Returns true if an event with the specified name is currently firing in an anim on the entity, and the 
	////	event has a vector type attribute with the given name. Fills OutValue with the value of the attribute
	//bool CommandGetVectorAttributeFromAnimEvent(int EntityIndex, const char* EventName, const char * AttributeName, Vector3& OutValue)
	//{
	//	bool bResult = false;

	//	// this script command is not approved for use in network scripts
	//	if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - This script command is not allowed in network game scripts!"))
	//	{
	//		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<fwEntity>(EntityIndex);
	//		if (pEntity)
	//		{
	//			if (SCRIPT_VERIFY(pEntity->GetDrawable(), "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a drawable"))
	//			{
	//				if (SCRIPT_VERIFY(pEntity->GetDrawable()->GetSkeletonData(), "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - Entity does not have a skeleton data"))
	//				{
	//					if (SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no skeleton"))
	//					{	
	//						if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - Entity has no animdirector"))
	//						{
	//							const crProperty* pProp = pEntity->GetAnimDirector()->FindScriptVisiblePropertyInMoveNetworks(crProperty::CalcKey(EventName));
	//							if (pProp)
	//							{
	//								const crPropertyAttribute* pAttrib = pProp->GetAttribute(AttributeName);
	//								if (SCRIPT_VERIFY(pAttrib && pAttrib->GetType()==crPropertyAttribute::kTypeVector3, "GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT - Attribute does not exist in this event, or has the wrong type"))
	//								{
	//									const crPropertyAttributeVector3* pAttribTyped = static_cast<const crPropertyAttributeVector3*>(pAttrib);
	//									OutValue = VEC3V_TO_VECTOR3(pAttribTyped->GetVector3());
	//									bResult = true;
	//								}
	//							}
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//	return bResult;
	//}

	void CommandSetEntityAllAnimsSpeed(int /*EntityIndex*/, float /*AnimSpeed*/)
	{
		//const CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<fwEntity>(EntityIndex);

		//if (pEntity)
		//{	
		//	if (SCRIPT_VERIFY(pEntity->GetAnimDirector(), "SET_ENTITY_ALL_ANIMS_SPEED - Entity does not have an anim director"))
		//	{
		//		CGenericClipHelper* pHelper = ((CMove &) pEntity->GetAnimDirector()->GetMove()).GetGenericClipHelper();

		//		if (pHelper)
		//		{
		//			pHelper->SetRate(AnimSpeed);
		//		}
		//	}
		//}
	}

	void CommandSetEntityAnimBlendOutSpeed(int EntityIndex, const char *pAnimName, const char *pAnimDictName, float fBlendOutSpeed)
	{
		CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToModify(EntityIndex, pAnimDictName, pAnimName, "SET_ENTITY_ANIM_BLEND_OUT_SPEED");

		if (pGenericClipHelper && !pGenericClipHelper->HasNetworkExpired())
		{
			pGenericClipHelper->SetAutoBlendOutDelta(fBlendOutSpeed);
			return; 
		}

		CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, true);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				return pTask->SetBlendOutDuration(fwAnimHelpers::CalcBlendDuration(fBlendOutSpeed), priority);
			}
		}

		pTask = FindScriptedAnimTask(EntityIndex, true, false);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
			{
				return pTask->SetBlendOutDuration(fwAnimHelpers::CalcBlendDuration(fBlendOutSpeed), priority);
			}
		}
		
		SCRIPT_ASSERT(0, "SET_ENTITY_ANIM_BLEND_OUT_SPEED - Anim is not playing on the entity");
	}

	void CommandSetEntityAnimCurrentTime(int EntityIndex, const char *pAnimDictName, const char *pAnimName, float NewAnimCurrentTime)
	{
		if (SCRIPT_VERIFY((NewAnimCurrentTime >= 0.0f) && (NewAnimCurrentTime <= 1.0f), "SET_ENTITY_ANIM_CURRENT_TIME - Anim Time should be >=0.0 and <= 1.0"))
		{
			CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToModify(EntityIndex, pAnimDictName, pAnimName, "SET_ENTITY_ANIM_CURRENT_TIME");

			if (pGenericClipHelper && !pGenericClipHelper->HasNetworkExpired())
			{
				pGenericClipHelper->SetPhase(NewAnimCurrentTime);
				
				return; 
			}

			CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, true);
			if (pTask)
			{
				CTaskScriptedAnimation::ePlaybackPriority priority;
				s32 index;
				if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
				{
					pTask->SetPhase(NewAnimCurrentTime, priority, (u8)index);
					return;
				}
			}

			pTask = FindScriptedAnimTask(EntityIndex, true, false);
			if (pTask)
			{
				CTaskScriptedAnimation::ePlaybackPriority priority;
				s32 index;
				if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index))
				{
					pTask->SetPhase(NewAnimCurrentTime, priority, (u8)index);
					return;
				}
			}

			SCRIPT_ASSERT(0, "SET_ENTITY_ANIM_CURRENT_TIME - Anim is not playing on the entity");
		}
	}

	void CommandSetEntityAnimSpeed(int EntityIndex, const char *pAnimDictName, const char *pAnimName, float AnimSpeed)
	{
		CGenericClipHelper *pGenericClipHelper = GetEntityGenericClipHelperToModify(EntityIndex, pAnimDictName, pAnimName, "SET_ENTITY_ANIM_SPEED");

		if (pGenericClipHelper && !pGenericClipHelper->HasNetworkExpired())
		{
			pGenericClipHelper->SetRate(AnimSpeed);

			return; 
		}

		CTaskScriptedAnimation* pTask = FindScriptedAnimTask(EntityIndex, true);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index ))
			{
				pTask->SetRate(AnimSpeed, priority, (u8)index);
				return;
			}
		}

		pTask = FindScriptedAnimTask(EntityIndex, true, false);
		if (pTask)
		{
			CTaskScriptedAnimation::ePlaybackPriority priority;
			s32 index;
			if (pTask->FindFirstInstanceOfClip(pAnimDictName, pAnimName, priority, index ))
			{
				pTask->SetRate(AnimSpeed, priority, (u8)index);
				return;
			}
		}

		SCRIPT_ASSERT(0,"SET_ENTITY_ANIM_SPEED - Anim is not playing on the entity");
	}

	void CommandSetEntityAsMissionEntity(int EntityIndex, bool bScriptHostObject, bool bGrabFromOtherScript)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		bool bHasNetworkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity) != NULL;

		if (bScriptHostObject && bHasNetworkObject && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
		{
			if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "SET_ENTITY_AS_MISSION_ENTITY - Non-host machines cannot set entities as host mission entities"))
			{
				return;
			}
		}

		if (pEntity)
		{
			switch (pEntity->PopTypeGet())
			{
			case POPTYPE_UNKNOWN : 
			case POPTYPE_RANDOM_PARKED : 
			case POPTYPE_RANDOM_PATROL : 
			case POPTYPE_RANDOM_SCENARIO : 
			case POPTYPE_RANDOM_AMBIENT :
				{
					if (NetworkUtils::IsNetworkCloneOrMigrating(pEntity))
					{
						if (scriptVerifyf(bScriptHostObject, "%s:SET_ENTITY_AS_MISSION_ENTITY - currently only works for host objects when converting remotely", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
						{
							CNetObjPhysical* pPhysObj = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject());

							CGameScriptHandler* pHandler = CTheScripts::GetCurrentGtaScriptHandler();

							const CGameScriptObjInfo* pPendingScriptObjInfo = pPhysObj->GetPendingScriptObjInfo();

							if (pPendingScriptObjInfo)
							{
								scriptAssertf(pPendingScriptObjInfo->GetScriptId() == pHandler->GetScriptId(), "SET_ENTITY_AS_MISSION_ENTITY: %s has already been set as a script entity by %s", pEntity->GetNetworkObject()->GetLogName(), pHandler->GetLogName());
							}
							else
							{
								if (AssertVerify(pHandler->GetNetworkComponent()))
								{
									// build a new script info for this entity, this will be assigned to the entity on the other machine and registered with the 
									// handler on that machine. When we receive the update from the object, it will be properly registered with our handler.
									// We have to allocate a new id for this entity so that if the host subsequently creates any new entities, we won't get a duplicate id.
									CGameScriptObjInfo newInfo(pHandler->GetScriptId(), pHandler->AllocateNewHostObjectId(), pHandler->GetNetworkComponent()->GetHostToken());
									
									pPhysObj->SetPendingScriptObjInfo(newInfo);

									CConvertToScriptEntityEvent::Trigger(*pPhysObj);
								}
							}
						}
					}
					else if (bHasNetworkObject)
					{
						CTheScripts::RegisterEntity(pEntity, bScriptHostObject, true, false);
					}
					else
					{
						CTheScripts::RegisterEntity(pEntity, false, false, false);
					}
				}
				break;
			case POPTYPE_MISSION :	//	already a mission ped 
				{
					const CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

					if (pExtension)
					{
						if (pExtension->GetScriptHandler())
						{
							//	In a network session, there can be several frames between a script thread terminating and its script handler being destroyed. The handler has to wait for broadcast data to sync.
							//	IsTerminated() will return true during those frames. GetThreadThatCreatedMe() will return NULL because the script has already terminated.
							const bool bCreationScriptHasTerminated = pExtension->GetScriptHandler()->IsTerminated();
							const GtaThread* pCreationThread = pEntity->GetThreadThatCreatedMe();

							if (scriptVerifyf(bCreationScriptHasTerminated || pCreationThread, "%s:SET_ENTITY_AS_MISSION_ENTITY - creation thread not found", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
							{
								if (bCreationScriptHasTerminated || (pCreationThread != CTheScripts::GetCurrentGtaScriptThread()) )
								{	//	If bCreationScriptHasTerminated is true then the script that created the entity must be different from the current thread because the current thread is still running
									if (bGrabFromOtherScript)
									{
										if (!NetworkUtils::IsNetworkCloneOrMigrating(pEntity))
										{
#if !__NO_OUTPUT
											if (bCreationScriptHasTerminated)
											{
												scriptDisplayf("%s:SET_ENTITY_AS_MISSION_ENTITY - taking control of a mission entity that belonged to a script that has terminated (but its script handler hasn't terminated yet)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
											}
											else
											{
												scriptDisplayf("%s:SET_ENTITY_AS_MISSION_ENTITY - taking control of a mission entity that belonged to %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), const_cast<GtaThread*>(pCreationThread)->GetScriptName());
											}
#endif	//	!__NO_OUTPUT

											s32 oldScriptGUIDOfEntity = CTheScripts::GetGUIDFromEntity(*pEntity);

											bool bScriptObjectWasGrabbedFromTheWorld = pExtension->GetScriptObjectWasGrabbedFromTheWorld();
											bool bNetworked = pExtension->GetIsNetworked();

											CTheScripts::UnregisterEntity(pEntity, false);

											scriptAssertf(pEntity->GetExtension<CScriptEntityExtension>() == NULL, "%s:SET_ENTITY_AS_MISSION_ENTITY - Graeme - just checking if CScriptEntityExtension ever exists after calling UnregisterEntity(). I've rearranged this code slightly so that GetIsNetworked() is called on the CScriptEntityExtension before UnregisterEntity() is called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
											
											CTheScripts::RegisterEntity(pEntity, bScriptHostObject, bNetworked, false);

											if (bScriptObjectWasGrabbedFromTheWorld)
											{
												CScriptEntityExtension* pNonConstExtensionOfNewEntity = pEntity->GetExtension<CScriptEntityExtension>();

												if (scriptVerifyf(pNonConstExtensionOfNewEntity, "%s:SET_ENTITY_AS_MISSION_ENTITY - failed to find a CScriptEntityExtension for newly registered entity", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
												{
													pNonConstExtensionOfNewEntity->SetScriptObjectWasGrabbedFromTheWorld(true);

													s32 newScriptGUIDOfEntity = CTheScripts::GetGUIDFromEntity(*pEntity);

													CTheScripts::GetHiddenObjects().ReregisterInHiddenObjectMap(oldScriptGUIDOfEntity, newScriptGUIDOfEntity);
												}
											}
										}
#if __ASSERT
										else
										{
											if (pEntity->IsNetworkClone())
											{
												scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - the entity is not controlled by this machine i.e. it's a network clone", CTheScripts::GetCurrentScriptNameAndProgramCounter());
											}
											else
											{
												scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - the entity is in the process of migrating i.e. this machine doesn't have complete control of the entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
											}
										}
#endif	//	__ASSERT
									}
									else
									{
#if __ASSERT
										if (bCreationScriptHasTerminated)
										{
											scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - entity created by a script that has terminated (but its script handler hasn't terminated yet), trying to mark as mission entity by %s, but the GrabFromOtherScript parameter is FALSE", 
												CTheScripts::GetCurrentScriptNameAndProgramCounter(), CTheScripts::GetCurrentScriptName());
										}
										else
										{
											if (pCreationThread != CTheScripts::GetCurrentGtaScriptThread())
											{
												scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - entity created by %s, trying to mark as mission entity by %s, but the GrabFromOtherScript parameter is FALSE", 
													CTheScripts::GetCurrentScriptNameAndProgramCounter(), const_cast<GtaThread*>(pCreationThread)->GetScriptName(), CTheScripts::GetCurrentScriptName());
											}
										}
#endif	//	__ASSERT
									}
								}
							}
						}
#if __ASSERT
						else
						{
							if (pEntity->GetIsTypePed())
							{
								scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - ped's script extension doesn't have a script handler (%p %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity, pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "");
							}
							else
							{
								scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - non-ped entity's script extension doesn't have a script handler (%p %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity, pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "");
							}
						}
#endif	//	__ASSERT
					}
#if __ASSERT
					else
					{
						if (pEntity->GetIsTypePed())
						{
							scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - ped doesn't have a script extension.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
						}
						else
						{
							scriptAssertf(0, "%s:SET_ENTITY_AS_MISSION_ENTITY - non-ped entity doesn't have a script extension", CTheScripts::GetCurrentScriptNameAndProgramCounter());
						}
					}
#endif	//	__ASSERT
				}
				break;
			default : 
				{
#if __ASSERT
					s32 ModelHashKey = 0;
					const char *pModelName = "unknown";
					CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(pEntity->GetModelId());
					if (SCRIPT_VERIFY(pModel, "SET_ENTITY_AS_MISSION_ENTITY - couldn't find model for the entity's model index. Wanted to display the model's hash and name in an assert about unexpected entity creation type"))
					{
						ModelHashKey = (s32) pModel->GetHashKey();
						pModelName = pModel->GetModelName();
					}

					scriptAssertf(0, "SET_ENTITY_AS_MISSION_ENTITY - Unexpected entity creation type - %s (%d). Model name is %s. Model hash is %u unsigned or %d signed", CTheScripts::GetPopTypeName(pEntity->PopTypeGet()), (s32) pEntity->PopTypeGet(), pModelName, (u32) ModelHashKey, ModelHashKey );
#endif	//	__ASSERT
				}
				break;
			}
		}
	}

	void SetEntityAsNoLongerNeeded(CPhysical *pEntity, int &EntityIndex)
	{		
		if (SCRIPT_VERIFY(!pEntity->GetIsTypePed() || !((CPed*)pEntity)->IsPlayer(), "SET_ENTITY_AS_NO_LONGER_NEEDED - Cannot be called on the player"))
		{
			CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

			// don't assert if entity is not a script entity
			if (pExtension && SCRIPT_VERIFY_TWO_STRINGS(pExtension->GetScriptHandler() == CTheScripts::GetCurrentGtaScriptHandler(), "SET_ENTITY_AS_NO_LONGER_NEEDED - The entity (%s) was not created by this script",
				pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "Not Networked!"))
			{
#if !__FINAL
				if (pEntity->GetNetworkObject())
				{
					scriptDebugf1("%s: Marking %s as no longer needed (Clone or migrating:%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject()->GetLogName(), NetworkUtils::IsNetworkCloneOrMigrating(pEntity) ? "true" : "false");
					scrThread::PrePrintStackTrace();
				}
#endif // !__FINAL
				if (pEntity->IsNetworkClone() || (pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->IsPendingOwnerChange()))
				{
					CMarkAsNoLongerNeededEvent::Trigger(*pEntity->GetNetworkObject());
				}
				else
				{
					CTheScripts::UnregisterEntity(pEntity, true);
				}		
				EntityIndex = 0;
			}
		}
	}

	void CommandSetEntityAsNoLongerNeeded(int &EntityIndex)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, 0); // don't assert if no entity

		if (pEntity)
		{
			if (SCRIPT_VERIFY (!pEntity->GetIsTypePed() || !((CPed*)pEntity)->IsPlayer(),"SET_ENTITY_AS_NO_LONGER_NEEDED - Cannot be called on the player"))
			{
				SetEntityAsNoLongerNeeded(pEntity, EntityIndex );
			}
		}
	}

	void CommandSetEntityCanBeDamaged(int EntityIndex, bool CanBeDamagedFlag)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
#if !__FINAL
			if ((u32)CanBeDamagedFlag == pEntity->m_nPhysicalFlags.bNotDamagedByAnything)
			{
				scriptDebugf1("%s - SET_ENTITY_CAN_BE_DAMAGED changed to %s on %s, was previously %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CanBeDamagedFlag ? "TRUE" : "FALSE", pEntity->GetDebugName(), !pEntity->m_nPhysicalFlags.bNotDamagedByAnything ? "TRUE" : "FALSE");
				scrThread::PrePrintStackTrace();
			}
#endif //!__FINAL

			pEntity->m_nPhysicalFlags.bNotDamagedByAnything = !CanBeDamagedFlag;

			if (!CanBeDamagedFlag && pEntity->GetIsTypeVehicle())
			{
				static_cast<CVehicle*>(pEntity)->ExtinguishCarFire();
			}
		}
	}

	bool CommandGetEntityCanBeDamaged( int EntityIndex )
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pEntity )
		{
			return !pEntity->m_nPhysicalFlags.bNotDamagedByAnything;
		}
		return false;
	}

	void CommandSetEntityCanBeDamagedByRelGroup(int EntityIndex, bool bAllowDamagedByRelGroupFlag, s32 iRelGroup)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
#if !__FINAL
			if (((u32)bAllowDamagedByRelGroupFlag != pEntity->m_nPhysicalFlags.bNotDamagedByRelGroup) || ((u32)iRelGroup != pEntity->m_specificRelGroupHash))
			{
				scriptDebugf1("%s - SET_ENTITY_CAN_BE_DAMAGED_BY_RELATIONSHIP_GROUP changed to %s (group %i) on %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bAllowDamagedByRelGroupFlag ? "TRUE" : "FALSE", iRelGroup, pEntity->GetDebugName());
				scrThread::PrePrintStackTrace();
			}
#endif //!__FINAL

			if (SCRIPT_VERIFY(!pEntity->m_nPhysicalFlags.bOnlyDamagedByRelGroup, "SET_ENTITY_CAN_BE_DAMAGED_BY_RELATIONSHIP_GROUP - entity is already set to be only damaged by a relationship group - clear this first"))
			{
				pEntity->m_nPhysicalFlags.bOnlyDamagedByRelGroup = false;

				bAllowDamagedByRelGroupFlag = !bAllowDamagedByRelGroupFlag;

				if (bAllowDamagedByRelGroupFlag)
				{
					//if (SCRIPT_VERIFY(iRelGroup!=INVALID_RELATIONSHIP_GROUP, "SET_PED_CAN_BE_DAMAGED_BY_RELATIONSHIP_GROUP - Must be a valid rel group"))
					{
						pEntity->m_nPhysicalFlags.bNotDamagedByRelGroup = TRUE;
						pEntity->m_specificRelGroupHash = (u32)iRelGroup;
					}
				}
				else
				{
					pEntity->m_nPhysicalFlags.bNotDamagedByRelGroup = FALSE;
					pEntity->m_specificRelGroupHash = 0;
				}
			}
		}
	}

	void CommandSetEntityCanOnlyBeDamagedByScriptParticipants(int nEntityIndex, bool bOnlyDamagedWhenRunningScript)
	{
        if(SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_ENTITY_CAN_ONLY_BE_DAMAGED_BY_SCRIPT_PARTICIPANTS - This is not supported in single player games!"))
        {
		    CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(nEntityIndex);
			if(pEntity)
			{
#if !__FINAL
				if ((u32)bOnlyDamagedWhenRunningScript != pEntity->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript)
				{
					scriptDebugf1("%s - SET_ENTITY_CAN_ONLY_BE_DAMAGED_BY_SCRIPT_PARTICIPANTS changed to %s on %s, was previously %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bOnlyDamagedWhenRunningScript ? "TRUE" : "FALSE", pEntity->GetDebugName(), pEntity->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
				}
#endif //!__FINAL

				pEntity->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript = bOnlyDamagedWhenRunningScript;
			}
        }
	}

	void CommandSetEntityCanBeTargetedWithoutLos(int EntityIndex, bool bTargettableWithNoLos)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				static_cast<CPed*>(pEntity)->SetPedConfigFlag( CPED_CONFIG_FLAG_TargettableWithNoLos, bTargettableWithNoLos );
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				static_cast<CVehicle*>(pEntity)->m_nVehicleFlags.bTargettableWithNoLos = bTargettableWithNoLos;
			}
			else if (pEntity->GetIsTypeObject())
			{
				static_cast<CObject*>(pEntity)->m_nObjectFlags.bTargettableWithNoLos = bTargettableWithNoLos;
			}
			else
			{
				scriptAssertf(0, "Entity Type Not Supported");
			}
		}
	}

	void SetEntityCollision(int EntityIndex, bool bUseCollision, bool bKeepDisabledSimulating, bool bCompletelyDisable)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				if( bUseCollision &&
					CGameWorld::FindLocalPlayer() == pPed &&
					NetworkInterface::IsInSpectatorMode() )
				{
#if !__FINAL
					scriptDebugf1("Script %s SET_ENTITY_COLLISION On local spectator '%s' - pEntity 0x%p - bUseCollision %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bUseCollision ? "TRUE" : "FALSE");
					scriptAssertf(0, "Script %s SET_ENTITY_COLLISION On local spectator '%s' - pEntity 0x%p - bUseCollision %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bUseCollision ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
#endif
					return;
				}
			}
#if __DEV
			if(pEntity->IsCollisionEnabled() != bUseCollision)
			{
				scriptDebugf1("SET_ENTITY_COLLISION - %s - %s - '%s' - 0x%p - %s", 
					CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "Not networked!", pEntity->GetModelName(), pEntity, bUseCollision ? "TRUE" : "FALSE");
			}
#endif // __DEV
			// We are allowed to set collision on peds only if they are not in a vehicle, clearing collision is fine
			if (SCRIPT_VERIFY(!pEntity->GetIsTypePed() || ((CPed*)pEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) == false, "SET_ENTITY_COLLISION - can't set a ped's collision on when he is in a vehicle"))
			{
				// Seems like this should be AFTER the block that adds or removes physics
				// Otherwise for adding we don't do anything to actually set up flags in EnableCollision (It bails early since the entity isn't in the physics level yet)
				bUseCollision ? pEntity->EnableCollision() : pEntity->DisableCollision(NULL, bCompletelyDisable);
				if (pEntity->GetIsTypeObject() && pEntity->GetCurrentPhysicsInst())
				{
					if (bUseCollision)
					{
						pEntity->AddPhysics();
					}
					else if(!bKeepDisabledSimulating)
					{
						if(pEntity->GetCurrentPhysicsInst()->IsInLevel())
						{
							pEntity->RemovePhysics();
						}
					}
					if(NetworkInterface::IsGameInProgress() && pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
					{
						CNetObjPhysical* pPhysObj = static_cast<CNetObjPhysical*>(pEntity->GetNetworkObject());
						pPhysObj->RequestForceSendOfPhysicalAttachVelocityState();
					}
				}
				// B* 2333709, the collision flag could be changed from the above code. One case is that RemovePhysics calls DetachFromParent, which may call EnableCollision.
				// Here we call the EnableCollision or DisableCollision again just in case.
				bUseCollision ? pEntity->EnableCollision() : pEntity->DisableCollision(NULL, bCompletelyDisable);
				// Keep a record of whether script has disabled collision on this ped
				// This is to stop the ped AI physics lod from reactivating it (url:bugstar:516623)
				if(pEntity->GetIsTypePed())
				{
					((CPed*)pEntity)->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasDisabledCollision, !bUseCollision);
					if(bCompletelyDisable || bUseCollision)
					{
						((CPed*)pEntity)->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision, !bUseCollision);
					}
				}

				//bUseCollision ? pEntity->EnableCollision() : pEntity->DisableCollision();
#if !__FINAL
				if(NetworkInterface::IsGameInProgress() && pEntity->GetNetworkObject())
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SCRIPT_CHANGING_COLLISION", "%s", pEntity->GetNetworkObject()->GetLogName());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Collisions", bUseCollision ? "On" : "Off");
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Completely", bCompletelyDisable ? "True" : "False");
				}
#endif // !__FINAL
			}
		}
	}

	bool CommandGetEntityCollisionDisabled(int EntityIndex)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pEntity)
		{
			return (!pEntity->IsProtectedBaseFlagSet(fwEntity::USES_COLLISION));
		}
		return false;
	}


	void CommandSetEntityCollision(int EntityIndex, bool bUseCollision, bool bKeepDisabledSimulating)
	{
		SetEntityCollision(EntityIndex, bUseCollision, bKeepDisabledSimulating, false);
	}

	// Added a new command for this to reduce having to patch all old scripts and to reduce the chance of creating bugs by modifying CommandSetEntityCollision to completely disable collision.
	void CommandSetEntityCompletelyDisableCollision(int EntityIndex, bool bUseCollision, bool bKeepDisabledSimulating)
	{
		SetEntityCollision(EntityIndex, bUseCollision, bKeepDisabledSimulating, true);
	}

	void CommandSetEntityCoordsExt(int EntityIndex, const scrVector &scrVecNewCoors, bool DeadCheck, bool KeepTasks, bool KeepIK, bool doWarp, bool bResetPlants)
	{
		Vector3 vNewCoors(scrVecNewCoors);

		if (SCRIPT_VERIFY( ABS(vNewCoors.x) < WORLDLIMITS_XMAX && ABS(vNewCoors.y) < WORLDLIMITS_YMAX && vNewCoors.z > WORLDLIMITS_ZMIN && vNewCoors.z < WORLDLIMITS_ZMAX, "SET_ENTITY_COORDS Position out of range!" ))
		{
			unsigned flags = DeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_ALL : CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;

			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, flags);

			if (pEntity)
			{
//				Vector3 vNewCoors = Vector3 (scrVecNewCoors);
#if GTA_REPLAY
				CReplayMgr::RecordWarpedEntity(pEntity);
#endif

				if (pEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pEntity);

					int Flags = CScriptPeds::SetPedCoordFlag_WarpGang | CScriptPeds::SetPedCoordFlag_OffsetZ;

					if (KeepTasks)
						Flags |= CScriptPeds::SetPedCoordFlag_KeepTasks;

					if (KeepIK)
						Flags |= CScriptPeds::SetPedCoordFlag_KeepIK;

					CScriptPeds::SetPedCoordinates(pPed, vNewCoors.x, vNewCoors.y, vNewCoors.z, Flags, doWarp, bResetPlants);
				}
				else if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					CVehiclePopulation::SetCoordsOfScriptCar(pVehicle, vNewCoors.x, vNewCoors.y, vNewCoors.z, false, true, doWarp);
				}
				else if (pEntity->GetIsTypeObject())
				{
					CObject* pObject = static_cast<CObject*>(pEntity);
					float fAlteredZ = vNewCoors.z;

					if (fAlteredZ <= INVALID_MINIMUM_WORLD_Z)
					{
						fAlteredZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vNewCoors.x, vNewCoors.y);
					}

					u32 objectNameHash = pObject->GetBaseModelInfo()->GetModelNameHash();
					if( objectNameHash == MI_ROLLER_COASTER_CAR_1.GetName().GetHash() || 
						objectNameHash == MI_ROLLER_COASTER_CAR_2.GetName().GetHash() )
					{
						if(phInst* pInst = pObject->GetCurrentPhysicsInst())
						{
							if(pInst->IsInLevel())
							{
								PHLEVEL->SetInactiveCollidesAgainstInactive( pObject->GetCurrentPhysicsInst()->GetLevelIndex(), true );
								pObject->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);
								if(!pObject->GetCurrentPhysicsInst()->HasLastMatrix())
								{
									PHLEVEL->ReserveInstLastMatrix( pObject->GetCurrentPhysicsInst() );
								}

								Mat34V lastMatrix = pInst->GetMatrix();
								Vector3 velocity = vNewCoors - VEC3V_TO_VECTOR3( lastMatrix.GetCol3() );
								velocity *= fwTimer::GetInvTimeStep();

								if( velocity.Mag() > 50.0f )
								{
									lastMatrix.SetCol3(RCC_VEC3V(vNewCoors));
									velocity.Zero();
									doWarp = true; // script needs to set this properly
								}
								else
								{
									doWarp = false; // script needs to set this properly
								}

								CPhysics::GetSimulator()->SetLastInstanceMatrix(pInst, lastMatrix);

								CObject::ms_RollerCoasterVelocity = velocity;
							}
						}
					}

					pObject->Teleport(Vector3(vNewCoors.x, vNewCoors.y, fAlteredZ), -10.0f, false, true, true, doWarp);
					if(doWarp)
					{
						CScriptEntities::ClearSpaceForMissionEntity(pObject);
					}

					// if the pickup is set to snap to ground, switch this off as it is being manually moved
					if (pObject->m_nObjectFlags.bIsPickUp)
					{
						((CPickup*)pObject)->ClearPlaceOnGround();
					}

					//fragInst* pFragInst = pObject->GetFragInst();
					//if( pFragInst )
					//{
					//	fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
					//	if( pCacheEntry )
					//	{
					//		fragHierarchyInst* pFragHierInst = pCacheEntry->GetHierInst();
					//		environmentCloth* pCloth = pFragHierInst->envCloth;
					//		if( pFragHierInst && pCloth )
					//		{		
					//			pCloth->SetFlag( environmentCloth::CLOTH_IsSkipSimulation, true );
					//			pCloth->UpdateCloth( 0.16666667f, 1.0f );		
					//		}
					//	}
					//}
				}
				else
				{
					scriptAssertf(0, "%s:SET_ENTITY_COORDS - Unrecognised entity type", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
		}
	}

	void CommandSetEntityCoords(int EntityIndex, const scrVector &scrVecNewCoors, bool DeadCheck, bool KeepTasks, bool KeepIK, bool doWarp)
	{
		CommandSetEntityCoordsExt(EntityIndex, scrVecNewCoors, DeadCheck, KeepTasks, KeepIK, doWarp, true);
	}

	void CommandSetEntityCoordsWithoutPlantsReset(int EntityIndex, const scrVector &scrVecNewCoors, bool DeadCheck, bool KeepTasks, bool KeepIK, bool doWarp)
	{
		CommandSetEntityCoordsExt(EntityIndex, scrVecNewCoors, DeadCheck, KeepTasks, KeepIK, doWarp, false);
	}

	void CommandSetEntityCoordsNoOffset(int EntityIndex, const scrVector &scrVecNewCoors, bool KeepTasks, bool KeepIK, bool doWarp)
	{
		Vector3 vNewCoors(scrVecNewCoors);

		if (SCRIPT_VERIFY( ABS(vNewCoors.x) < WORLDLIMITS_XMAX && ABS(vNewCoors.y) < WORLDLIMITS_YMAX && vNewCoors.z > WORLDLIMITS_ZMIN && vNewCoors.z < WORLDLIMITS_ZMAX, "SET_ENTITY_COORDS_NO_OFFSET Position out of range!" ))
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

			if (pEntity)
			{
//				Vector3 vNewCoors = Vector3 (scrVecNewCoors);

				if (pEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pEntity);

					int Flags = CScriptPeds::SetPedCoordFlag_WarpGang;

					if (KeepTasks)
						Flags |= CScriptPeds::SetPedCoordFlag_KeepTasks;

					if (KeepIK)
						Flags |= CScriptPeds::SetPedCoordFlag_KeepIK;

					CScriptPeds::SetPedCoordinates(pPed, vNewCoors.x, vNewCoors.y, vNewCoors.z, Flags, doWarp);
				}
				else if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					CVehiclePopulation::SetCoordsOfScriptCar(pVehicle, vNewCoors.x, vNewCoors.y, vNewCoors.z, false, false, doWarp);
				}
				else if (pEntity->GetIsTypeObject())
				{
					CObject* pObject = static_cast<CObject*>(pEntity);
					float fAlteredZ = vNewCoors.z;

					if (fAlteredZ <= INVALID_MINIMUM_WORLD_Z)
					{
						fAlteredZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vNewCoors.x, vNewCoors.y);
					}

					pObject->Teleport(Vector3(vNewCoors.x, vNewCoors.y, fAlteredZ), -10.0f, false, true, true, doWarp);
					if(doWarp)
					{
						CScriptEntities::ClearSpaceForMissionEntity(pObject);
					}
				}
				else
				{
					scriptAssertf(0, "%s:SET_ENTITY_COORDS - Unrecognised entity type", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
		}
	}

	void CommandSetEntityDynamic( int EntityIndex, bool bIsDynamic )
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			if (bIsDynamic)
			{
				if(pEntity->GetCollider())
					pEntity->GetCollider()->GetSleep()->Reset();
				else
					pEntity->ActivatePhysics();
			}
			else
			{
				pEntity->DeActivatePhysics();
			}
		}
	}

	void CommandSetEntityHeading(int EntityIndex, float NewHeading)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			CPhysical* pTarget = GetTargetEntity(pEntity);

			if (pTarget)
			{
				if (pTarget->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pTarget);

					float fHeadingInRadians = ( DtoR * NewHeading);

					fHeadingInRadians = fwAngle::LimitRadianAngleSafe(fHeadingInRadians);

					if(SCRIPT_VERIFY(!pPed->GetUsingRagdoll(), "SET_ENTITY_HEADING - Ped is ragdolling"))
					{
						pPed->SetDesiredHeading( fHeadingInRadians );
						pPed->SetHeading(fHeadingInRadians);
					}
				}
				else
				{
					Matrix34 matOld = MAT34V_TO_MATRIX34(pTarget->GetMatrix());
					pTarget->SetHeading(DtoR * NewHeading);

                    if(pTarget->GetIsTypeVehicle())// If we're a vehicle make sure our attached trailer is in the correct orientation.
                    {
                        CVehicle * pVehicle = static_cast<CVehicle*>(pTarget);
						pVehicle->UpdateGadgetsAfterTeleport(matOld);

						// HACK
						if(!pVehicle->PopTypeIsMission())// Some scripts are using this function to move vehicles around and this removes all velocity.
						{
							pVehicle->ResetAfterTeleport();
						}


						if(pVehicle->IsInPathServer())
						{
							CPathServerGta::UpdateDynamicObject(pVehicle, true, true);
						}

// TODO: CLOTH STUFF, just re-pin verts her. I'm still investigating if this should stay here - svetli
#if 1
						fragInst* pFragInst = pVehicle->GetFragInst();
						if( pFragInst )
						{
							fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
							if( pCacheEntry )
							{
								fragHierarchyInst* pFragHierInst = pCacheEntry->GetHierInst();
								environmentCloth* pCloth = pFragHierInst->envCloth;
								if( pFragHierInst && pCloth )
								{									
#if RSG_GEN9 && USE_CLOTH_DYNAMICVERTEXBUFFERS
									clothInstance *pClothInstance = (clothInstance *)pCloth->GetClothController()->GetClothInstance();
#endif

									pCloth->SetFlag( environmentCloth::CLOTH_IsSkipSimulation, true );
									pCloth->UpdateCloth( 
#if RSG_GEN9 && USE_CLOTH_INST_NORMALS
										pClothInstance->GetClothNormalsRef(),
#endif
										0.16666667f, 
										
										1.0f 
										
#if RSG_GEN9 && USE_CLOTH_DYNAMICVERTEXBUFFERS_ONLY
										, pClothInstance->GetDynamicVertexBuffers()
#endif										
									);

#if RSG_GEN9 && USE_CLOTH_DYNAMICVERTEXBUFFERS
									pClothInstance->SetBuffersUpdated(true);
#endif
								}
							}
						}
#endif
                    }
					else if(pTarget->GetIsTypeObject())
					{
						CObject * pObject = static_cast<CObject*>(pTarget);
						if(pObject->IsInPathServer())
						{
							CPathServerGta::UpdateDynamicObject(pObject, true, true);
						}
					}
				}
			}
		}
	}

	void CommandSetEntityHealth(int EntityIndex, int NewHealth, int InstigatorIndex)
	{
		if (SCRIPT_VERIFY (NewHealth >= 0, "SET_ENTITY_HEALTH - Health must be 0 or more"))
		{
			CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
			CEntity *pInstigator = (InstigatorIndex == NULL_IN_SCRIPTING_LANGUAGE) ? NULL : CTheScripts::GetEntityToModifyFromGUID<CEntity>(InstigatorIndex,  CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

			if (pEntity)
			{
				if (pEntity->GetIsTypePed())
				{
					CPed *pPed = static_cast<CPed*>(pEntity);
					float OldHealth = pPed->GetHealth();
					if (pPed->IsPlayer())
					{
						if (SCRIPT_VERIFY(pPed->GetPlayerInfo(),"SET_ENTITY_HEALTH - GetPlayerInfo returned null for player ped."  ))
						{
							//Mark script has decreased the player health.
							if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject() && NewHealth < pPed->GetHealth() && pPed->GetWeaponDamageHash())
							{
								((CNetObjPed*)pPed->GetNetworkObject())->SetChangeNextPedHealthDataSynch(WEAPONTYPE_SCRIPT_HEALTH_CHANGE);
							}

							pPed->SetHealth((float)MIN(pPed->GetPlayerInfo()->MaxHealth, NewHealth), true );
						}
					}
					else
					{
						pPed->SetHealth((float) NewHealth, true);
						pPed->SetMaxHealth( MAX(pPed->GetMaxHealth(),(float) NewHealth) );
					}

					pPed->SetHealthLostScript(MAX(pPed->GetHealthLostScript() + (OldHealth - pPed->GetHealth()), 0.f));

					//! Generate a spoof damage event to put ped into correct dead task.
					if( (pPed->IsFatallyInjured() || pPed->ShouldBeDead()) && !pPed->GetIsDeadOrDying())
					{
						CWeaponDamage::GeneratePedDamageEvent(pInstigator, pPed, WEAPONTYPE_FALL, 0.1f, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, CPedDamageCalculator::DF_None);
					} 
				}
				else
				{
					pEntity->SetHealth((float) NewHealth);
				}

#if !__FINAL
				Displayf("SET_ENTITY_HEALTH, CommandSetEntityHealth - EntityIndex = %i, Entity = %s, NewHealth = %i, InstigatorIndex = %i, Instigator = %s", EntityIndex, pEntity->GetLogName(), NewHealth, InstigatorIndex, pInstigator ? pInstigator->GetLogName() : "NULL");
				if(PARAM_setEntityHealthPrintStack.Get())
				{
					Displayf("Script callstack below: ");
					scrThread::PrePrintStackTrace();
				}
#endif
			}
		}
	}

	void CommandSetEntityInitialVelocity( int EntityIndex, const scrVector &scrVecComponent)
	{
		Vector3 VecComponent = Vector3 (scrVecComponent);

		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			if (SCRIPT_VERIFY(pEntity->GetVelocity().IsZero(), "SET_ENTITY_INITIAL_VELOCITY - Current object speed is not zero"))
			{
				if (SCRIPT_VERIFY((VecComponent.x != 0.0f) || (VecComponent.y != 0.0f) || (VecComponent.z != 0.0f), "SET_OBJECT_INITIAL_VELOCITY - New speed can't be 0.0, 0.0, 0.0"))
				{
					pEntity->SetVelocity(VecComponent);

					scriptAssertf((rage::Abs(VecComponent.x) < DEFAULT_MAX_SPEED) && (rage::Abs(VecComponent.y) < DEFAULT_MAX_SPEED)&&(rage::Abs(VecComponent.x) < DEFAULT_MAX_SPEED), "SET_ENTITY_INITIAL_VELOCITY - Velocity component set greater than max velocity %f m/s", DEFAULT_MAX_SPEED  );
				}
			}
		}
	}

	void CommandSetEntityInvincible(int EntityIndex, bool bInvincible)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				if( !bInvincible &&
					CGameWorld::FindLocalPlayer() == pPed &&
					NetworkInterface::IsInSpectatorMode() )
				{
#if !__FINAL
					scriptDebugf1("Script %s SET_ENTITY_INVINCIBLE On local spectator '%s' - pEntity 0x%p - bInvincible %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bInvincible ? "TRUE" : "FALSE");
					scriptAssertf(0, "Script %s SET_ENTITY_INVINCIBLE On local spectator '%s' - pEntity 0x%p - bInvincible %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, bInvincible ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
#endif
					return;
				}
			}

			pEntity->m_nPhysicalFlags.bNotDamagedByAnything = bInvincible && CTheScripts::Frack(); // true;
			scriptDebugf1("Script %s SET_ENTITY_INVINCIBLE On '%s' - bInvincible %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetNetworkObject() ? pEntity->GetNetworkObject()->GetLogName() : "", bInvincible ? "TRUE" : "FALSE");
		}
	}

	void CommandSetEntityIsTargetPriority(int EntityIndex, bool bHighPriority, float fTargetableDistance)
	{
		SCRIPT_ASSERT(fTargetableDistance >= 0.0f, "SET_ENTITY_IS_TARGET_PRIORITY - Targetable Distance Should Be Positive");

		s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK;

		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, assertflags);

		if (pEntity)  
		{
			if (pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				pPed->SetIsAHighPriorityTarget( bHighPriority );
				pPed->SetLockOnTargetableDistance( fTargetableDistance );
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
				pVeh->m_nVehicleFlags.bIsATargetPriority = bHighPriority;
				pVeh->SetLockOnTargetableDistance( fTargetableDistance );
			}
			else if (pEntity->GetIsTypeObject())
			{
				CObject* pObj = static_cast<CObject*>(pEntity);
				pObj->m_nObjectFlags.bIsATargetPriority = bHighPriority;
				pObj->SetLockOnTargetableDistance( fTargetableDistance );
			}
			else 
			{
				scriptAssertf(0, "Entity Type Not Supported");
			}
		}		
	}

	void CommandSetEntityLights(int EntityIndex, bool bLightSwitch)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex);

		if (pEntity)
		{
			pEntity->m_nFlags.bLightsIgnoreDayNightSetting = bLightSwitch;
			if (bLightSwitch == true)
			{
				CLODLightManager::RegisterBrokenLightsForEntity(pEntity);
			}
			else
			{
				CLODLightManager::UnregisterBrokenLightsForEntity(pEntity);
			}
		}
	}

	void CommandSetEntityLoadCollisionFlag(int EntityIndex, bool LoadCollisionFlag, bool DoDeadCheck)
	{
		unsigned assertFlags = DoDeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_ALL : CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, assertFlags);

		if (pEntity)  
		{
			pEntity->SetDontLoadCollision(!LoadCollisionFlag);
		}		
	}

	bool CommandHasCollisionLoadedAroundEntity(int EntityIndex)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if (SCRIPT_VERIFY(pEntity, "HAS_COLLISION_LOADED_AROUND_ENTITY - invalid entity index"))
		{
			return g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pEntity->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
		}
		return false;
	}

	void CommandSetEntityMaxSpeed(int EntityIndex, float MaxSpeed)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			if(SCRIPT_VERIFY(pEntity->GetCurrentPhysicsInst(), "SET_ENTITY_MAX_SPEED - Entity doesn't have physics streamed in yet"))
			{
				if(SCRIPT_VERIFY(pEntity->GetCurrentPhysicsInst()->GetArchetype(), "SET_ENTITY_MAX_SPEED - Entity doesn't have physics archetype"))
				{		
					pEntity->SetMaxSpeed(MaxSpeed);
				}
			}
		}
	}

	void CommandSetEntityOnlyDamagedByPlayer(int EntityIndex, bool OnlyDamagedByPlayerFlag)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
#if !__FINAL
			if ((u32)OnlyDamagedByPlayerFlag != pEntity->m_nPhysicalFlags.bOnlyDamagedByPlayer)
			{
				scriptDebugf1("%s - SET_ENTITY_ONLY_DAMAGED_BY_PLAYER changed to %s on %s, was previously %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), OnlyDamagedByPlayerFlag ? "TRUE" : "FALSE", pEntity->GetDebugName(), pEntity->m_nPhysicalFlags.bOnlyDamagedByPlayer ? "TRUE" : "FALSE");
				scrThread::PrePrintStackTrace();
			}
#endif //!__FINAL

			pEntity->m_nPhysicalFlags.bOnlyDamagedByPlayer = OnlyDamagedByPlayerFlag;
		}
	}

	void CommandSetEntityOnlyDamagedByRelGroup(int EntityIndex, bool OnlyDamagedByRelGroupFlag, s32 iRelGroup)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
#if !__FINAL
			if (((u32)OnlyDamagedByRelGroupFlag != pEntity->m_nPhysicalFlags.bOnlyDamagedByRelGroup) || ((u32)iRelGroup != pEntity->m_specificRelGroupHash))
			{
				scriptDebugf1("%s - SET_ENTITY_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP set to %s (group %i) on %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), OnlyDamagedByRelGroupFlag ? "TRUE" : "FALSE", iRelGroup, pEntity->GetDebugName());
				scrThread::PrePrintStackTrace();
			}
#endif //!__FINAL

			if (SCRIPT_VERIFY(!pEntity->m_nPhysicalFlags.bNotDamagedByRelGroup, "SET_ENTITY_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP - entity is already set to be not damaged by a relationship group - clear this first"))
			{
				pEntity->m_nPhysicalFlags.bNotDamagedByRelGroup = false;

				if (OnlyDamagedByRelGroupFlag)
				{		
					//if (SCRIPT_VERIFY(iRelationshipGroup!=INVALID_RELATIONSHIP_GROUP, "SET_PED_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP - Must be a valid rel group"))
					{
						if(pEntity->GetNetworkObject())
						{
							scriptDebugf3("Setting SET_ENTITY_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP of entity: %s with relationship group: %d", pEntity->GetNetworkObject()->GetLogName(), iRelGroup);
						}
						
						pEntity->m_nPhysicalFlags.bOnlyDamagedByRelGroup = true;
						pEntity->m_specificRelGroupHash = (u32)iRelGroup;
					}
				}
				else
				{
					if(pEntity->GetNetworkObject())
					{
						scriptDebugf3("Clearing SET_ENTITY_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP of entity: %s with relationship group: %d", pEntity->GetNetworkObject()->GetLogName(), iRelGroup);
					}
	
					pEntity->m_nPhysicalFlags.bOnlyDamagedByRelGroup = false;
					pEntity->m_specificRelGroupHash = 0;
				}
			}
		}
	}

	void CommandSetEntityProofs(int EntityIndex, bool bBulletProof, bool bFlameProof, bool bExplosionProof, bool bCollisionProof, bool bMeleeWeaponProof, bool bSteamProof, bool bDontResetDamageFlagsOnCleanupMissionState, bool bSteamSmoke )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{ 
#if __DEV

#define NUMBEROFPROOFS 8

			const bool bNewProofs[NUMBEROFPROOFS] = { bBulletProof , bFlameProof , bExplosionProof , bCollisionProof , bMeleeWeaponProof , bSteamProof , bDontResetDamageFlagsOnCleanupMissionState , bSteamSmoke };

			const bool bOldProofs[NUMBEROFPROOFS] = { (bool)pEntity->m_nPhysicalFlags.bNotDamagedByBullets
					, (bool)pEntity->m_nPhysicalFlags.bNotDamagedByFlames
					, (bool)pEntity->m_nPhysicalFlags.bIgnoresExplosions
					, (bool)pEntity->m_nPhysicalFlags.bNotDamagedByCollisions
					, (bool)pEntity->m_nPhysicalFlags.bNotDamagedByMelee
					, (bool)pEntity->m_nPhysicalFlags.bNotDamagedBySteam
					, (bool)pEntity->m_bDontResetDamageFlagsOnCleanupMissionState
					, (bool)pEntity->m_nPhysicalFlags.bNotDamagedBySmoke };

			const char* proofNames[NUMBEROFPROOFS] = { "BulletProof" , "FlameProof" , "ExplosionProof" , "CollisionProof" , "MeleeWeaponProof" , "SteamProof" , "DontResetDamageFlagsOnCleanupMissionState" , "SteamSmoke" };

			bool bProofHasChanged = false;
			for (int i = 0; i <= NUMBEROFPROOFS - 1; i++)
			{
				if (bOldProofs[i] != bNewProofs[i])
				{
					bProofHasChanged = true;
					Displayf("Proof %s changed on %s by %s: from %d to %d\n", proofNames[i], pEntity->GetLogName(), CTheScripts::GetCurrentScriptName(), bOldProofs[i], bNewProofs[i]);
				}
			}
			bool bPlayer = pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->IsPlayer();
			if (bPlayer && bProofHasChanged)
			{
				Displayf("Proofs changed. Stack below");
				scrThread::PrePrintStackTrace();
			}
#endif
			pEntity->m_nPhysicalFlags.bNotDamagedByBullets		= bBulletProof;
			pEntity->m_nPhysicalFlags.bNotDamagedByFlames		= bFlameProof;
			pEntity->m_nPhysicalFlags.bIgnoresExplosions		= bExplosionProof;
			pEntity->m_nPhysicalFlags.bNotDamagedByCollisions	= bCollisionProof;
			pEntity->m_nPhysicalFlags.bNotDamagedByMelee		= bMeleeWeaponProof;
			pEntity->m_nPhysicalFlags.bNotDamagedBySteam		= bSteamProof;
			pEntity->m_bDontResetDamageFlagsOnCleanupMissionState = bDontResetDamageFlagsOnCleanupMissionState;
			pEntity->m_nPhysicalFlags.bNotDamagedBySmoke		= bSteamSmoke;
		}
	}

	bool CommandGetEntityProofs(int EntityIndex, int &bBulletProof, int &bFlameProof, int &bExplosionProof, int &bCollisionProof, int &bMeleeWeaponProof, int &bSteamProof, int &bDontResetDamageFlagsOnCleanupMissionState, int &bSteamSmoke)
	{
		const CPhysical *pEntity =  CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			
			bBulletProof = pEntity->m_nPhysicalFlags.bNotDamagedByBullets;
			bFlameProof = pEntity->m_nPhysicalFlags.bNotDamagedByFlames;
			bExplosionProof = pEntity->m_nPhysicalFlags.bIgnoresExplosions;
			bCollisionProof = pEntity->m_nPhysicalFlags.bNotDamagedByCollisions;
			bMeleeWeaponProof = pEntity->m_nPhysicalFlags.bNotDamagedByMelee;
			bSteamProof = pEntity->m_nPhysicalFlags.bNotDamagedBySteam;
			bDontResetDamageFlagsOnCleanupMissionState = pEntity->m_bDontResetDamageFlagsOnCleanupMissionState;
			bSteamSmoke = pEntity->m_nPhysicalFlags.bNotDamagedBySmoke;

			return true;
		}
		return false;
	}

	void CommandSetEntityQuaternion(int EntityIndex, float NewX, float NewY, float NewZ, float NewW)
	{
		fwEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<fwEntity>(EntityIndex);

		if (pEntity)
		{
			Vector3 TempCoors = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			Matrix34 matNew = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
			matNew.FromQuaternion(Quaternion(NewX, NewY, NewZ, NewW));
			matNew.d = TempCoors;
			pEntity->SetMatrix(matNew);
		}
	}

	void CommandSetEntityRecordsCollisions( int EntityIndex, bool bRecordsCollision )
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if (pEntity)
		{
			pEntity->SetRecordCollisionHistory(bRecordsCollision);
		}
	}

	void CommandSetEntityRotation(int EntityIndex, const scrVector &scrVecRotation, s32 RotOrder, bool DoDeadCheck)
	{
		Vector3 vRotation = Vector3 (scrVecRotation);

		unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
		if(!DoDeadCheck)
		{
			assertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
		}
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, assertFlags);

		if (pEntity)
		{
			Matrix34 mat;
			vRotation *= DtoR;
			CScriptEulers::MatrixFromEulers(mat, vRotation, static_cast<EulerAngleOrder>(RotOrder));
			
			mat.d = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			pEntity->SetMatrix(mat);

			if (pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				pVehicle->m_nVehicleFlags.bPlaceOnRoadQueued = false;
				if (pVehicle->InheritsFromBike())
				{
					// Forces bike to process physics as the 'door' ratios need to be updated
					pVehicle->m_nVehicleFlags.bDoorRatioHasChanged = true;
				}
			}

			if(pEntity->GetIsPhysical())
			{
				static_cast<CPhysical*>(pEntity)->SetAngVelocity(VEC3V_TO_VECTOR3(Vec3V(V_ZERO)));
			}
		}
	}

	void CommandSetEntityVisible(int EntityIndex, bool VisibleFlag, bool ResetRenderPhaseVisibilityMask)
	{
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pEntity)
		{
			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				if( VisibleFlag &&
					CGameWorld::FindLocalPlayer() == pPed &&
					NetworkInterface::IsInSpectatorMode())
				{
#if !__FINAL
					scriptDebugf1("Script %s SET_ENTITY_VISIBLE On local spectator - '%s' - pEntity 0x%p - VisibleFlag %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, VisibleFlag ? "TRUE" : "FALSE");
					scriptAssertf(0, "Script %s SET_ENTITY_VISIBLE On local spectator - '%s' - pEntity 0x%p - VisibleFlag %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pEntity, VisibleFlag ? "TRUE" : "FALSE");
					scrThread::PrePrintStackTrace();
#endif
					return;
				}
			}
			netObject* pNetObj = pEntity->GetIsDynamic() ? static_cast<CDynamicEntity*>(pEntity)->GetNetworkObject() : NULL;

			// we need to do something different for network objects because they may be temporarily overriding the script visibility during a cutscene
			if (pNetObj)
			{
#if __FINAL
				static_cast<CNetObjEntity*>(pNetObj)->SetIsVisible(VisibleFlag, "net");
#else
				static_cast<CNetObjEntity*>(pNetObj)->SetIsVisible(VisibleFlag, "SET_ENTITY_VISIBLE");
#endif // __FINAL
			}
			else
			{
				pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, VisibleFlag, true);
			}

			if(ResetRenderPhaseVisibilityMask)
			{
				pEntity->GetRenderPhaseVisibilityMask().SetAllFlags(VIS_PHASE_MASK_ALL);
			}

			if(NetworkInterface::IsGameInProgress() && pEntity->GetIsDynamic())
			{
#if ENABLE_NETWORK_LOGGING
				netObject* pNetObj = static_cast<CDynamicEntity*>(pEntity)->GetNetworkObject();
				if (pNetObj)
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SCRIPT_CHANGING_VISIBILITY", "%s", pNetObj->GetLogName());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Visibility", VisibleFlag ? "On" : "Off");
					NetworkInterface::GetObjectManagerLog().WriteDataValue("ResetRenderPhaseVisibilityMask", ResetRenderPhaseVisibilityMask ? "On" : "Off");
				}
#endif // ENABLE_NETWORK_LOGGING
			}
		}
	}

	void CommandSetEntityWaterReflectionFlag(int EntityIndex, bool reflectionFlag)
	{
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pEntity)
		{
			if(reflectionFlag)
			{
				pEntity->GetRenderPhaseVisibilityMask().SetFlag(VIS_PHASE_MASK_WATER_REFLECTION);
			}
			else
			{
				pEntity->GetRenderPhaseVisibilityMask().ClearFlag(VIS_PHASE_MASK_WATER_REFLECTION);
			}

			// propagate flag into descriptor
			const bool flag = pEntity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION);
			pEntity->ChangeVisibilityMask(VIS_PHASE_MASK_WATER_REFLECTION, flag, false);
		}
	}

	void CommandSetEntityMirrorReflectionFlag(int EntityIndex, bool reflectionFlag)
	{
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if(pEntity)
		{
			if(reflectionFlag)
			{
				pEntity->GetRenderPhaseVisibilityMask().SetFlag(VIS_PHASE_MASK_MIRROR_REFLECTION);
			}
			else
			{
				pEntity->GetRenderPhaseVisibilityMask().ClearFlag(VIS_PHASE_MASK_MIRROR_REFLECTION);
			}

			// propagate flag into descriptor
			const bool flag = pEntity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_MIRROR_REFLECTION);
			pEntity->ChangeVisibilityMask(VIS_PHASE_MASK_MIRROR_REFLECTION, flag, false);
		}
	}

	void CommandSetEntityMotionBlur(int EntityIndex, bool UseMotionBlur)
	{
		CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if (pEntity)
		{
			pEntity->m_nFlags.bAddtoMotionBlurMask = !UseMotionBlur;
		}
	}

	void CommandSetEntityVelocity(int EntityIndex, const scrVector & scrVecComponent)
	{
		Vector3 vComponent = Vector3 (scrVecComponent);

		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			scriptAssertf((rage::Abs(vComponent.x) < DEFAULT_MAX_SPEED),"%s,SET_ENTITY_VELOCITY: X component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_SPEED);
			scriptAssertf((rage::Abs(vComponent.y) < DEFAULT_MAX_SPEED),"%s,SET_ENTITY_VELOCITY: Y component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_SPEED);
			scriptAssertf((rage::Abs(vComponent.z) < DEFAULT_MAX_SPEED),"%s,SET_ENTITY_VELOCITY: Z component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_SPEED);
			pEntity->SetVelocity(vComponent);
		}
	}

	void CommandSetEntityAngularVelocity(int EntityIndex, const scrVector & scrVecComponent)
	{
		Vector3 vComponent = Vector3 (scrVecComponent);

		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity)
		{
			scriptAssertf((rage::Abs(vComponent.x) < DEFAULT_MAX_ANG_SPEED),"%s,SET_ENTITY_VELOCITY: X component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_ANG_SPEED);
			scriptAssertf((rage::Abs(vComponent.y) < DEFAULT_MAX_ANG_SPEED),"%s,SET_ENTITY_VELOCITY: Y component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_ANG_SPEED);
			scriptAssertf((rage::Abs(vComponent.z) < DEFAULT_MAX_ANG_SPEED),"%s,SET_ENTITY_VELOCITY: Z component of velocity vector is greater than max speed %f m/s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), DEFAULT_MAX_ANG_SPEED);
			pEntity->SetAngVelocity(vComponent);
		}
	}

	void CommandSetEntityHasGravity(int EntityIndex, bool bHasGravity)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if (pEntity && pEntity->GetCurrentPhysicsInst())
		{
			pEntity->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, !bHasGravity);
		}
	}

	void CommandSetEntityDrawableLodThresholds(int entityIndex, int highLod, int medLod, int lowLod, int vlowLod)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS | CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_DRAWABLE_LOD_THRESHOLDS - entity does not exist"))
		{
			Assertf(pEntity->GetLodData().IsOrphanHd(),
				"Trying to override drawable lod thresholds of model %s, but it is in a LOD hierarchy. That won't work", pEntity->GetModelName());

			if (pEntity->GetLodData().IsOrphanHd() && pEntity->GetDrawHandlerPtr())
			{
				const fwArchetype* pArchetype = pEntity->GetArchetype();
				Assert(pArchetype);
				rmcDrawable* drawable = pArchetype->GetDrawable();
				if (drawable)
				{
					drawable->GetLodGroup().SetLodThresh(LOD_HIGH, (float)highLod);
					drawable->GetLodGroup().SetLodThresh(LOD_MED, (float)medLod);
					drawable->GetLodGroup().SetLodThresh(LOD_LOW, (float)lowLod);
					drawable->GetLodGroup().SetLodThresh(LOD_VLOW, (float)vlowLod);
				}
			}
		}
	}

	void CommandSetEntityLodDist(int entityIndex, int lodDist)
	{
        CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS | CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_LOD_DIST - entity does not exist"))
		{
			Assertf(pEntity->GetLodData().IsOrphanHd(),
				"Trying to override lod distance of model %s to %d, but it is in a LOD hierarchy. That won't work", pEntity->GetModelName(), lodDist);

			g_LodMgr.OverrideLodDist(pEntity, (u32)lodDist);
		}
	}

	int CommandGetEntityLodDist(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS | CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (SCRIPT_VERIFY(pEntity, "GET_ENTITY_LOD_DIST - entity does not exist"))
		{
			return(pEntity->GetLodDistance());
		}

		return(-1);
	}

	static void SetWeaponAlpha(CObject *object, int alpha, bool useSmoothAlpha BANK_PARAM(bool bCalledByScript))
	{
#if __BANK
		atString ownerStr = atString( bCalledByScript ? CTheScripts::GetCurrentScriptName() : "NETCODE" );
#endif
		CWeapon *weapon = object->GetWeapon();
		if( weapon ) 
		{
			if( object->IsBaseFlagSet(fwEntity::HAS_CUTOUT) )
				alpha >>= 1; // If a weapon has cutout, we need to push the alpha down because of SSA...

			if (SCRIPT_VERIFY(CPostScan::AddAlphaOverride(object,alpha BANK_PARAM(ownerStr)), "SET_ENTITY_ALPHA Failed to add weapon to alpha override list") )
			{
				const CWeapon::Components& components = weapon->GetComponents();

				for(int i=0;i<components.GetCount();i++)
				{
					CDynamicEntity* compEntity = components[i]->GetDrawable();
					if( compEntity )
					{
						if( SCRIPT_VERIFY(CPostScan::AddAlphaOverride(compEntity,alpha BANK_PARAM(ownerStr) ), "SET_ENTITY_ALPHA Failed to add weapon components to alpha override list") )
						{
							compEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR,!useSmoothAlpha);
							compEntity->AssignBaseFlag(fwEntity::FORCED_ZERO_ALPHA, alpha==0);
						}
					}
				}

				CProjectile* projectile = CPedEquippedWeapon::GetProjectileOrdnance( object );
				if( projectile )
				{
					if( SCRIPT_VERIFY(CPostScan::AddAlphaOverride(projectile,alpha BANK_PARAM(ownerStr)), "SET_ENTITY_ALPHA Failed to add weapon projectile to alpha override list") )
					{
						projectile->AssignBaseFlag(fwEntity::USE_SCREENDOOR,!useSmoothAlpha);
						projectile->AssignBaseFlag(fwEntity::FORCED_ZERO_ALPHA, alpha==0);
					}
				}
			}
		}
	}
	
	static void ResetWeaponAlpha(CObject *object)
	{
		CWeapon *weapon = object->GetWeapon();
		if( weapon ) 
		{
			CPostScan::RemoveAlphaOverride(object);

			const CWeapon::Components& components = weapon->GetComponents();

			for(int i=0;i<components.GetCount();i++)
			{
				CDynamicEntity* compEntity = components[i]->GetDrawable();
				if( compEntity )
				{
					CPostScan::RemoveAlphaOverride(compEntity);
					compEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR,object->GetShouldUseScreenDoor());
				}
			}

			CProjectile* projectile = CPedEquippedWeapon::GetProjectileOrdnance( object );
			if( projectile )
			{
				CPostScan::RemoveAlphaOverride(projectile);
				projectile->AssignBaseFlag(fwEntity::USE_SCREENDOOR,object->GetShouldUseScreenDoor());
			}
		}
	}

	void PropagatePedAlpha(CPed *ped, int alpha, bool useSmoothAlpha BANK_PARAM(bool bCalledByScript))
	{
		CPedWeaponManager *weaponManager = ped->GetWeaponManager();
		if( weaponManager )
		{
			CPedEquippedWeapon* equippedWeapon = weaponManager->GetPedEquippedWeapon();
			if( equippedWeapon )
			{
				CObject *weaponObject = equippedWeapon->GetObject();
				if( weaponObject )
				{
					SetWeaponAlpha(weaponObject, alpha, useSmoothAlpha BANK_PARAM(bCalledByScript));
				}
			}
		}
	}
	
	void PropagatePedResetAlpha(CPed *ped)
	{
		CPedWeaponManager *weaponManager = ped->GetWeaponManager();
		if( weaponManager )
		{
			CPedEquippedWeapon* equippedWeapon = weaponManager->GetPedEquippedWeapon();
			if( equippedWeapon )
			{
				CObject *weaponObject = equippedWeapon->GetObject();
				if( weaponObject )
				{
					ResetWeaponAlpha(weaponObject);
				}
			}
		}
	}

	void CommandSetEntityAlpha(int entityIndex, int alpha, bool useSmoothAlpha)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		CommandSetEntityAlphaEntity(pEntity, alpha, useSmoothAlpha BANK_PARAM(CMD_OVERRIDE_ALPHA_BY_SCRIPT));
	}

	void CommandSetEntityAlphaEntity(CEntity* pEntity, int alpha, bool useSmoothAlpha BANK_PARAM(eCmdOverrideAlpha src) )
	{
#if __BANK
		const bool bCalledByScript = (src == CMD_OVERRIDE_ALPHA_BY_SCRIPT);
		atString ownerStr( bCalledByScript ? CTheScripts::GetCurrentScriptName() : "NETCODE" );
#endif

		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_ALPHA - entity does not exist"))
		{
			scriptAssertf(0<=alpha && alpha<=255,"%s,SET_ENTITY_ALPHA: alpha %d is outside of 0 to 255 range", CTheScripts::GetCurrentScriptNameAndProgramCounter(), alpha);
			
			if( SCRIPT_VERIFY(CPostScan::AddAlphaOverride(pEntity,alpha BANK_PARAM(ownerStr) ), "SET_ENTITY_ALPHA Too many entities with overridden alpha") )
			{
				scriptAssertf((useSmoothAlpha && !(pEntity->GetIsTypePed())) || !(useSmoothAlpha),"%s,SET_ENTITY_ALPHA: smooth alpha is not available on Peds", CTheScripts::GetCurrentScriptNameAndProgramCounter());			
				pEntity->AssignBaseFlag(fwEntity::FORCE_ALPHA,useSmoothAlpha || pEntity->GetIsTypeVehicle());
				pEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR,!useSmoothAlpha && !pEntity->GetIsTypeVehicle());
				pEntity->AssignBaseFlag(fwEntity::FORCED_ZERO_ALPHA, alpha==0);

				if( pEntity->GetIsTypeObject() )
				{
					CObject* weaponObject = (CObject*)pEntity;
					SetWeaponAlpha(weaponObject, alpha, useSmoothAlpha BANK_PARAM(bCalledByScript));
				}
				else if( pEntity->GetIsTypePed() && NetworkInterface::IsGameInProgress() )
				{
					CPed *ped = (CPed*)pEntity;
					PropagatePedAlpha(ped, alpha, useSmoothAlpha BANK_PARAM(bCalledByScript));
				}

			}
		}
	}

	int CommandGetEntityAlpha(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		return CommandGetEntityAlphaEntity(pEntity);
	}
	
	int CommandGetEntityAlphaEntity(CEntity* pEntity)
	{
		if (SCRIPT_VERIFY(pEntity, "GET_ENTITY_ALPHA - entity does not exist"))
		{
			return CPostScan::GetAlphaOverride(pEntity);
		}
		return 255;
	}

	void CommandResetEntityAlpha(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		CommandResetEntityAlphaEntity(pEntity);
	}

	void CommandResetEntityAlphaEntity(CEntity* pEntity)
	{
		if (SCRIPT_VERIFY(pEntity, "RESET_ENTITY_ALPHA - entity does not exist"))
		{
			CPostScan::RemoveAlphaOverride(pEntity);
			// Reset flags
			pEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR,pEntity->GetShouldUseScreenDoor());
			pEntity->AssignBaseFlag(fwEntity::FORCE_ALPHA,false);
			pEntity->AssignBaseFlag(fwEntity::FORCED_ZERO_ALPHA, false);

			if( pEntity->GetIsTypeObject() )
			{
				CObject* weaponObject = (CObject*)pEntity;
				ResetWeaponAlpha(weaponObject);
			}
			else if( pEntity->GetIsTypePed() && NetworkInterface::IsGameInProgress() )
			{
				CPed *ped = (CPed*)pEntity;
				PropagatePedResetAlpha(ped);
			}
		}
	}
	void CommandResetPickupEntityGlowEntity(CEntity* pEntity)
	{
		if (SCRIPT_VERIFY(pEntity, "RESET_PICKUP_ENTITY_GLOW - entity does not exist"))
		{
			entity_commands::CommandSetEntityAlphaEntity(pEntity, 255, false BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_SCRIPT));
			entity_commands::CommandResetEntityAlphaEntity(pEntity);
			if (pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);

				if (pObject->m_nObjectFlags.bIsPickUp)
				{
					CPickup* pPickup = (CPickup*)(pObject);
					pPickup->SetForceAlphaAndUseAmbientScale();
					pPickup->SetUseLightOverrideForGlow();
				}
			}
			//also ensure base alpha is 255
			pEntity->SetAlpha(255);
		}
	}
	void CommandResetPickupEntityGlow(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		CommandResetPickupEntityGlowEntity(pEntity);
	}

	void CommandSetPickupCollidesWithProjectiles( int entityIndex, bool collideWithProjectiles )
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (SCRIPT_VERIFY(pEntity, "SET_PICKUP_COLLIDES_WITH_PROJECTILES - entity does not exist"))
		{
			if (pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);

				if (pObject->m_nObjectFlags.bIsPickUp)
				{
					CPickup* pPickup = (CPickup*)(pObject);
					pPickup->SetIncludeProjectileFlag( collideWithProjectiles );
				}
			}
		}
	}

	void CommandSetEntitySortBias(int entityIndex, float bias)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_SORT_BIAS - entity does not exist"))
		{
			CPostScan::AddEntitySortBias(pEntity, bias);
		}
	}

	void CommandResetEntitySortBias(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "RESET_ENTITY_SORT_BIAS - entity does not exist"))
		{
			CPostScan::RemoveEntitySortBias(pEntity);
		}
	}

	void CommandSetEntityAlwaysPreRender(int entityIndex, bool bAlwaysPreRender)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_ALWAYS_PRERENDER - entity does not exist"))
		{
			if (bAlwaysPreRender)
			{
				gPostScan.AddToAlwaysPreRenderList(pEntity);
			}
			else
			{
				gPostScan.RemoveFromAlwaysPreRenderList(pEntity);
			}
		}
	}

	void CommandSetEntityRenderScorchedFlag(int entityIndex, bool renderscorched)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pEntity)
		{
			scriptAssertf(pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle(), "SET_ENTITY_RENDER_SCORCHED - %s called on a non ped/non vehicle entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			pEntity->m_nPhysicalFlags.bRenderScorched = renderscorched;
		}
	}
	
	void CommandSetTrafficLightOverride(int entityIndex, int tlOverride)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pEntity)
		{
			scriptAssertf(CTrafficLights::IsMITrafficLight(pEntity->GetBaseModelInfo()), "SET_ENTITY_TRAFFICLIGHT_OVERRIDE - %s called on a non trafficlight entity", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			pEntity->SetTrafficLightOverride((unsigned int)tlOverride);
		}
	}

	void CommandSetEntityInVehicle(int entityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pEntity)
		{
			pEntity->SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
		}
	}

	void CommandCreateModelSwap(const scrVector &vPos, float fRadius, s32 oldModelHash, s32 newModelHash, bool bSurviveMapReload)
	{
		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		CMapChange* pNewSwap = g_MapChangeMgr.Add((u32) oldModelHash, (u32) newModelHash, searchVolume, bSurviveMapReload, CMapChange::TYPE_SWAP, false, true);
		if (pNewSwap)
		{
			pNewSwap->SetOwner(pGtaThread->GetThreadId());
		}
	}

	void CommandRemoveModelSwap(const scrVector &vPos, float fRadius, s32 oldModelHash, s32 newModelHash, bool bLazy)
	{
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		g_MapChangeMgr.Remove(oldModelHash, newModelHash, searchVolume, CMapChange::TYPE_SWAP, bLazy);
	}

	void BaseCreateModelHide(const scrVector &vPos, float fRadius, s32 modelHash, bool bSurviveMapReload, bool bIncludeScriptObjects)
	{
		gnetDebug3("%s called CREATE_MODEL_HIDE for modelHash: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(modelHash).TryGetCStr());

		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		CMapChange* pNewSwap = g_MapChangeMgr.Add((u32) modelHash, (u32) modelHash, searchVolume, bSurviveMapReload, CMapChange::TYPE_HIDE, false, bIncludeScriptObjects);
		if (pNewSwap)
		{
			pNewSwap->SetOwner(pGtaThread->GetThreadId());
		}
	}

	void CommandCreateModelHide(const scrVector &vPos, float fRadius, s32 modelHash, bool bSurviveMapReload)
	{
		BaseCreateModelHide(vPos, fRadius, modelHash, bSurviveMapReload, true);
	}

	void CommandCreateModelHideExcludingScriptObjects(const scrVector &vPos, float fRadius, s32 modelHash, bool bSurviveMapReload)
	{
		BaseCreateModelHide(vPos, fRadius, modelHash, bSurviveMapReload, false);
	}

	void CommandRemoveModelHide(const scrVector &vPos, float fRadius, s32 modelHash, bool bLazy)
	{
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		g_MapChangeMgr.Remove(modelHash, modelHash, searchVolume, CMapChange::TYPE_HIDE, bLazy);
	}

	void CommandCreateForcedObject(const scrVector &vPos, float fRadius, s32 modelHash, bool bSurviveMapReload)
	{
		GtaThread* pGtaThread = CTheScripts::GetCurrentGtaScriptThread();
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		CMapChange* pNewSwap = g_MapChangeMgr.Add((u32) modelHash, (u32) modelHash, searchVolume, bSurviveMapReload, CMapChange::TYPE_FORCEOBJ, false, false);
		if (pNewSwap)
		{
			pNewSwap->SetOwner(pGtaThread->GetThreadId());
		}
	}

	void CommandRemoveForcedObject(const scrVector &vPos, float fRadius, s32 modelHash)
	{
		spdSphere searchVolume((Vec3V) vPos, ScalarV(fRadius));
		g_MapChangeMgr.Remove(modelHash, modelHash, searchVolume, CMapChange::TYPE_FORCEOBJ, false);
	}

	void CommandSetEntityNoCollisionEntity(int entityIndex, int secondEntityIndex, bool bReset)
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pEntity)
		{
			CPhysical* pEntity2 = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(secondEntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
			if (pEntity2)
			{
				if(NetworkInterface::IsGameInProgress() && pEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pEntity);

					if(NetworkInterface::IsInSpectatorMode() && CGameWorld::FindLocalPlayer() == pPed)
					{
#if !__FINAL
						netObject *netEntity2 = NetworkUtils::GetNetworkObjectFromEntity(pEntity2);

						scriptDebugf1("Script %s SET_ENTITY_NO_COLLISION_ENTITY between local spectator '%s' - pEntity 0x%p and pEntity2 %s - bReset %s",
							CTheScripts::GetCurrentScriptNameAndProgramCounter(),
							pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",
							pEntity,
							netEntity2?netEntity2->GetLogName():"Null net obj",
							bReset ? "TRUE" : "FALSE");

						scriptAssertf(0, "Script %s SET_ENTITY_NO_COLLISION_ENTITY between local spectator '%s' - pEntity 0x%p and pEntity2 %s - bReset %s", 
							CTheScripts::GetCurrentScriptNameAndProgramCounter(),
							pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",
							pEntity,
							netEntity2?netEntity2->GetLogName():"Null net obj",
							bReset ? "TRUE" : "FALSE");
						scrThread::PrePrintStackTrace();
#endif //!__FINAL
						return;
					}
				}
				pEntity->SetNoCollision(pEntity2, (u8) (bReset?NO_COLLISION_RESET_WHEN_NO_IMPACTS:NO_COLLISION_PERMENANT));
			}
		}
	}

	void CommandSetCanAutovaultOnEntity(int EntityIndex, bool bCanAutoVault)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (pEntity)
		{
			pEntity->SetAutoVaultDisabled(!bCanAutoVault);
		}
	}

	void CommandSetCanClimbOnEntity(int EntityIndex, bool bCanClimb)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (pEntity)
		{
			pEntity->SetClimbingDisabled(!bCanClimb);
		}
	}

	void CommandSetWaitForCollisionsBeforeProbe(int EntityIndex, bool bIsUGC)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (pEntity)
		{
			if (pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle() || pEntity->GetIsTypeObject())
			{
				CPhysical* pPhysical = smart_cast<CPhysical*>(pEntity);
				pPhysical->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(bIsUGC);
			}
			else
			{
				Assertf(false,"unexpected type of object : %s trying to modify interior probes", pEntity->GetModelName());
			}
		}
	}
	
	int CommandGetNearestPlayerToEntityHelper(int EntityIndex, int Team)
	{
		CEntity* entity = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != EntityIndex)
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if(!entity)
				return -1;
		}

		if (!entity)
			return -1;

		Vector3 entityPos	= VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
		float minDist		= FLT_MAX;
		s32 nearestPlayerIdx= -1;

		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const CNetGamePlayer *sourcePlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			if(sourcePlayer)
			{
				bool bProceed = true;

				//Evaluate Team
				if (Team != -1)
				{
					if (Team != sourcePlayer->GetTeam())
						bProceed = false;
				}

				if (bProceed)
				{
					CPed const* playerPed = sourcePlayer->GetPlayerPed();
					if(playerPed)
					{
						Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
						float dist = (playerPos - entityPos).Mag2();

						if(dist < minDist)
						{
							minDist = dist;
							nearestPlayerIdx = sourcePlayer->GetPhysicalPlayerIndex();
						}
					}
				}
			}
		}

		return nearestPlayerIdx;
	}

	int CommandGetNearestPlayerToEntity(int EntityIndex)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "ERROR : GET_NEAREST_PLAYER_TO_ENTITY : Network Games Only!"))
		{
			return CommandGetNearestPlayerToEntityHelper(EntityIndex,-1);
		}

		return -1;
	}

	int CommandGetNearestPlayerToEntityOnTeam(int EntityIndex, int Team)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "ERROR : GET_NEAREST_PLAYER_TO_ENTITY_ON_TEAM : Network Games Only!"))
		{
			return CommandGetNearestPlayerToEntityHelper(EntityIndex,Team);
		}

		return -1;
	}

	int CommandGetNearestPlayerOnLocalPlayerTutorialToEntity(int EntityIndex)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "ERROR : GET_NEAREST_PLAYER_ON_LOCAL_PLAYER_TUTORIAL_TO_ENTITY : Network Games Only!"))
		{
			CEntity* entity = NULL;
			if (NULL_IN_SCRIPTING_LANGUAGE != EntityIndex)
			{
				entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
				if(!entity)
					return -1;
			}

			if (!entity)
				return -1;

			Vector3 entityPos	= VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
			float minDist		= FLT_MAX;
			s32 nearestPlayerIdx= -1;

			unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

			CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
			Assertf(localPlayer, "GET_NEAREST_PLAYER_ON_LOCAL_PLAYER_TUTORIAL_TO_ENTITY : LocalPlayer == NULL?!");

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *sourcePlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                if(sourcePlayer)
                {
					if(!NetworkInterface::ArePlayersInDifferentTutorialSessions(*localPlayer, *sourcePlayer))
					{
						CPed const* playerPed = sourcePlayer->GetPlayerPed();
						if(playerPed)
						{
							Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
							float dist = (playerPos - entityPos).Mag2();

							if(dist < minDist)
							{
								minDist = dist;
								nearestPlayerIdx = sourcePlayer->GetPhysicalPlayerIndex();
							}
						}
					}
                }
			}

			return nearestPlayerIdx;
		}

		return -1;	
	}
	
	int CommandGetNearestParticipantToEntity(int EntityIndex)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "ERROR : GET_NEAREST_PARTICIPANT_TO_ENTITY : Network Games Only!"))
		{
			CEntity* entity = NULL;
			if (NULL_IN_SCRIPTING_LANGUAGE != EntityIndex)
			{
				entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
				if(!entity)
					return -1;
			}

			Vector3 entityPos	= VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
			float minDist		= FLT_MAX;
			s32 nearestPlayerIdx= -1;

			CGameScriptHandlerNetwork* handler = CTheScripts::GetCurrentGtaScriptHandlerNetwork();
			
			if(handler)
			{
				scriptHandlerNetComponent* netComponent = handler->GetNetworkComponent();
			
				if(netComponent)
				{
					unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
					const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

					for(unsigned index = 0; index < numPhysicalPlayers; index++)
					{
						const CNetGamePlayer *sourcePlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);				

						if(sourcePlayer && netComponent->IsPlayerAParticipant(*sourcePlayer))
						{
							CPed const* playerPed = sourcePlayer->GetPlayerPed();
							if(playerPed)
							{
								Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
								float dist = (playerPos - entityPos).Mag2();

								if(dist < minDist)
								{
									minDist = dist;
									nearestPlayerIdx = sourcePlayer->GetPhysicalPlayerIndex();
								}
							}
						}
					}				
				}
			}

			return nearestPlayerIdx;
		}

		return -1;
	}

	void CommandSetEntityNoWeaponDecals(int EntityIndex, bool bVal)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_NOWEAPONDECALS - entity does not exist"))
		{
			pEntity->SetNoWeaponDecals(bVal);
		}
	}

	bool CommandGetEntityNoWeaponDecals(int EntityIndex)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "GET_ENTITY_NOWEAPONDECALS - entity does not exist"))
		{
			return pEntity->GetNoWeaponDecals();
		}
		return false;
	}

	void CommandSetEntityUseMaxDistanceForWaterReflection(int EntityIndex, bool bVal)
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(EntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (SCRIPT_VERIFY(pEntity, "SET_ENTITY_USE_MAX_DISTANCE_FOR_WATER_REFLECTION - entity does not exist"))
		{
			pEntity->SetUseMaxDistanceForWaterReflection(bVal);
		}
	}

	scrVector CommandGetEntityBoneRotation( int entityIndex, int boneIndex )
	{
		Vector3 retVal( 0.0f, 0.0f, 0.0f );
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if( SCRIPT_VERIFY( pEntity, "GET_ENTITY_BONE_ROTATION - entity does not exist" ) )
		{
			if(SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_ENTITY_BONE_ROTATION - entity skeleton is NULL."))
			{
				if( SCRIPT_VERIFY( pEntity->GetSkeleton()->GetBoneCount() > boneIndex, "GET_ENTITY_BONE_ROTATION - invalid bone index passed in" ) )
				{
					Mat34V mat;
					pEntity->GetSkeleton()->GetGlobalMtx( boneIndex, mat );
					retVal = CScriptEulers::MatrixToEulers( MAT34V_TO_MATRIX34( mat ), EULER_YXZ );
					retVal *= RtoD;
				}
			}
		}

		return retVal;
	}

	scrVector CommandGetEntityBonePosition( int entityIndex, int boneIndex )
	{
		Vector3 retVal( 0.0f, 0.0f, 0.0f );
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if( SCRIPT_VERIFY( pEntity, "GET_ENTITY_BONE_POSITION - entity does not exist" ) )
		{
			if(SCRIPT_VERIFY(pEntity->GetSkeleton(), "GET_ENTITY_BONE_POSITION - entity skeleton is NULL."))
			{
				if( SCRIPT_VERIFY( pEntity->GetSkeleton()->GetBoneCount() > boneIndex, "GET_ENTITY_BONE_POSITION - invalid bone index passed in" ) )
				{
					Mat34V mat;
					pEntity->GetSkeleton()->GetGlobalMtx( boneIndex, mat );
					retVal = VEC3V_TO_VECTOR3( mat.d() );
				}
			}
		}

		return retVal;
	}

	scrVector CommandGetEntityBoneObjectRotation( int entityIndex, int boneIndex )
	{
		Vector3 retVal( 0.0f, 0.0f, 0.0f );
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if( SCRIPT_VERIFY( pEntity, "GET_ENTITY_BONE_ROTATION - entity does not exist" ) )
		{
			if( SCRIPT_VERIFY( pEntity->GetSkeleton()->GetBoneCount() > boneIndex, "GET_ENTITY_BONE_ROTATION - invalid bone index passed in" ) )
			{
				Mat34V mat;
				mat = pEntity->GetSkeleton()->GetObjectMtx( boneIndex );
				retVal = CScriptEulers::MatrixToEulers( MAT34V_TO_MATRIX34( mat ), EULER_YXZ );
				retVal *= RtoD;
			}
		}

		return retVal;
	}


	scrVector CommandGetEntityBoneObjectPosition( int entityIndex, int boneIndex )
	{
		Vector3 retVal( 0.0f, 0.0f, 0.0f );
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if( SCRIPT_VERIFY( pEntity, "GET_ENTITY_BONE_POSITION - entity does not exist" ) )
		{
			if( SCRIPT_VERIFY( pEntity->GetSkeleton()->GetBoneCount() > boneIndex, "GET_ENTITY_BONE_POSITION - invalid bone index passed in" ) )
			{
				Mat34V mat;
				mat = pEntity->GetSkeleton()->GetObjectMtx( boneIndex );
				retVal = VEC3V_TO_VECTOR3( mat.d() );
			}
		}

		return retVal;
	}

	int CommandGetEntityBoneCount( int entityIndex )
	{
		CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if( SCRIPT_VERIFY( pEntity, "GET_ENTITY_BONE_COUNT - entity does not exist" ) )
		{
			if( SCRIPT_VERIFY( pEntity->GetSkeleton(), "GET_ENTITY_BONE_COUNT - entity skeleton does not exist" ) )
			{
				return pEntity->GetSkeleton()->GetBoneCount();
			}
		}

		return 0;
	}

	void CommandEnableEntityBulletCollision( int entityIndex )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );

		if (pEntity)
		{
			pEntity->DisableCollision( NULL, false );
			
			if(pEntity->GetIsTypePed())
			{
				((CPed*)pEntity)->SetPedConfigFlag( CPED_CONFIG_FLAG_ScriptHasDisabledCollision, true );
				((CPed*)pEntity)->SetPedConfigFlag( CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision, false );
			}
		}
	}

	void CommandSetEntityCanOnlyDamageEntity( int entityToDamageIndex, int damagerEntityIndex )
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityToDamageIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );
		const CPhysical* pOnlyDamagedByEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>( damagerEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

		if( pEntity &&
			pOnlyDamagedByEntity )
		{
#if !__FINAL		
			scriptDebugf1("%s - SET_ENTITY_CAN_ONLY_DAMAGE_ENTITY set to %s on %s. Stack trace for invincible/invincibility issues:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pOnlyDamagedByEntity->GetDebugName(), pEntity->GetDebugName());
			scrThread::PrePrintStackTrace();
#endif //!__FINAL

			pEntity->SetCanOnlyBeDamagedByEntity( pOnlyDamagedByEntity );
		}
	}

	void CommandResetEntityCanOnlyDamageEntity( int entityToDamageIndex )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityToDamageIndex );

		if( pEntity )
		{
			pEntity->SetCanOnlyBeDamagedByEntity( NULL );
		}
	}

	void CommandSetEntityCantCauseCollisionDamageEntity( int entityToNotDamageIndex, int damagerEntityIndex )
	{
		CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityToNotDamageIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		const CPhysical* pNotDamagedByEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>( damagerEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

		if( pEntity &&
			pNotDamagedByEntity )
		{
			pEntity->SetCantBeDamagedByCollisionEntity( pNotDamagedByEntity );
		}
	}

	void CommandResetEntityCantCauseCollisionDamageEntity( int entityToNotDamageIndex )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityToNotDamageIndex );

		if( pEntity )
		{
			pEntity->SetCantBeDamagedByCollisionEntity( NULL );
		}
	}

	void CommandSetAllowMigrateToSpectator(int entityIndex, bool ignoreCanAccept)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityIndex );

		if( pEntity && pEntity->GetNetworkObject() )
		{
			CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, pEntity->GetNetworkObject());
			if(netPhysical)
			{
#if ENABLE_NETWORK_LOGGING
				netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
				NetworkLogUtils::WriteLogEvent(log, "SETTING SET_ALLOW_MIGRATE_TO_SPECTATOR", "%s", netPhysical->GetLogName());
				log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				log.WriteDataValue("Ignore", "%s", ignoreCanAccept?"TRUE":"FALSE");
#endif // ENABLE_NETWORK_LOGGING
				netPhysical->SetAllowMigrateToSpectator( ignoreCanAccept );
			}
		}
	}

	int CommandGetEntityOfTypeAttachedToEntity(int entityIndex, int modelHashKey)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>( entityIndex );

		if( pEntity )
		{
			CBaseModelInfo* pModelInfo	= CModelInfo::GetBaseModelInfoFromHashKey( (u32)modelHashKey, NULL );

			if( pModelInfo )
			{
				for( fwEntity* pChildEntity = pEntity->GetChildAttachment(); pChildEntity != NULL; pChildEntity = pChildEntity->GetSiblingAttachment() )
				{
					CBaseModelInfo* pChildModelInfo = CModelInfo::GetBaseModelInfo( pChildEntity->GetModelId() );

					if( pModelInfo == pChildModelInfo )
					{
						return CTheScripts::GetGUIDFromEntity( *pChildEntity );
					}
				}
			}
		}

		return -1;
	}

	void CommandSetPickupByCargobobDisabled(int entityIndex, bool disablePickUp )
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>( entityIndex );

		if( pEntity )
		{
			pEntity->SetPickupByCargobobDisabled( disablePickUp );
		}
	}

	void SetupScriptCommands()
	{
		// querying commands
		SCR_REGISTER_SECURE(DOES_ENTITY_EXIST,0xe5965cdf24f93a9f,		CommandDoesEntityExist);
		SCR_REGISTER_SECURE(DOES_ENTITY_BELONG_TO_THIS_SCRIPT,0x930e64cb0285caec,		CommandDoesEntityBelongToThisScript);
		SCR_REGISTER_SECURE(DOES_ENTITY_HAVE_DRAWABLE,0x8def8d80c66c34d4,		CommandDoesEntityHaveDrawable);
		SCR_REGISTER_SECURE(DOES_ENTITY_HAVE_PHYSICS,0xbda608a0a83c0961,		CommandDoesEntityHavePhysics);
		SCR_REGISTER_SECURE(DOES_ENTITY_HAVE_SKELETON,0x764eb96874effdc1,		CommandDoesEntityHaveSkeleton);
		SCR_REGISTER_SECURE(DOES_ENTITY_HAVE_ANIM_DIRECTOR,0x2158e81a6af65ea9,		CommandDoesEntityHaveAnimDirector);

		SCR_REGISTER_SECURE(HAS_ENTITY_ANIM_FINISHED,0x91243ffd31a90906,		CommandHasEntityAnimFinished);
		SCR_REGISTER_SECURE(HAS_ENTITY_BEEN_DAMAGED_BY_ANY_OBJECT,0x5d5890dba617acc7,		CommandHasEntityBeenDamagedByAnyObject);
		SCR_REGISTER_SECURE(HAS_ENTITY_BEEN_DAMAGED_BY_ANY_PED,0xe10b76698e5cf067,		CommandHasEntityBeenDamagedByAnyPed);
		SCR_REGISTER_SECURE(HAS_ENTITY_BEEN_DAMAGED_BY_ANY_VEHICLE,0x344bb7bdbab3823c,		CommandHasEntityBeenDamagedByAnyVehicle);
		SCR_REGISTER_SECURE(HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY,0x0747e45cff1f74aa,		CommandHasEntityBeenDamagedByEntity);
		SCR_REGISTER_SECURE(HAS_ENTITY_CLEAR_LOS_TO_ENTITY,0xd936dbc1d824a1be,		CommandHasEntityClearLosToEntity);
		SCR_REGISTER_SECURE(HAS_ENTITY_CLEAR_LOS_TO_ENTITY_ADJUST_FOR_COVER,0xce18aacde7f0cda1, CommandHasEntityClearLosToEntityAdjustForCover);
		SCR_REGISTER_SECURE(HAS_ENTITY_CLEAR_LOS_TO_ENTITY_IN_FRONT,0xd32ee22119fff87d,		CommandHasEntityClearLosToEntityInFront);
		SCR_REGISTER_SECURE(HAS_ENTITY_COLLIDED_WITH_ANYTHING,0x5c5b446ad691dea2,		CommandHasEntityCollidedWithAnything);
		SCR_REGISTER_SECURE(GET_LAST_MATERIAL_HIT_BY_ENTITY,0x79ee4cef9ca802b6,		CommandGetLastMaterialHitByEntity);
		SCR_REGISTER_SECURE(GET_COLLISION_NORMAL_OF_LAST_HIT_FOR_ENTITY,0xdddea5199d1ba051,		CommandGetCollisionNormalOfLastHitForEntity);
		SCR_REGISTER_SECURE(FORCE_ENTITY_AI_AND_ANIMATION_UPDATE,0xec5cb2a3f2052039,		CommandForceEntityAiAnimUpdate);

		SCR_REGISTER_SECURE(GET_ENTITY_ANIM_CURRENT_TIME,0xd01fafce275c94f3,		CommandGetEntityAnimCurrentTime);
		SCR_REGISTER_SECURE(GET_ENTITY_ANIM_TOTAL_TIME,0xc11da47ac6b1d1e2,		CommandGetEntityAnimTotalTime);
		SCR_REGISTER_SECURE(GET_ANIM_DURATION,0x32ee014e220ced07,		CommandGetAnimDuration);
		SCR_REGISTER_SECURE(GET_ENTITY_ATTACHED_TO,0x4f78f44087496a64,		CommandGetEntityAttachedTo);
		SCR_REGISTER_SECURE(GET_ENTITY_COORDS,0x6b62510f212526dc,		CommandGetEntityCoords);
		SCR_REGISTER_SECURE(GET_ENTITY_FORWARD_VECTOR,0x63bea94ab58d4e6d,		CommandGetEntityForwardVector);
		SCR_REGISTER_SECURE(GET_ENTITY_FORWARD_X,0x760b4486916d9b39,		CommandGetEntityForwardX);
		SCR_REGISTER_SECURE(GET_ENTITY_FORWARD_Y,0x675804aaed494f8a,		CommandGetEntityForwardY);
		SCR_REGISTER_SECURE(GET_ENTITY_HEADING,0x82fe2343f8bdff0b,		CommandGetEntityHeading);
		SCR_REGISTER_SECURE(GET_ENTITY_HEADING_FROM_EULERS,0xfffdd840fb251a01,		CommandGetEntityHeadingFromEulers);
		SCR_REGISTER_SECURE(GET_ENTITY_HEALTH,0x0cf63873d754b965,		CommandGetEntityHealth);
		SCR_REGISTER_SECURE(GET_ENTITY_MAX_HEALTH,0x72649caa101b626c,		CommandGetEntityMaxHealth);
		SCR_REGISTER_SECURE(SET_ENTITY_MAX_HEALTH,0x8fbb27c41710f120,		CommandSetEntityMaxHealth);
		SCR_REGISTER_SECURE(GET_ENTITY_HEIGHT,0x8a6fbb2c8f83ed39,		CommandGetEntityHeight);
		SCR_REGISTER_SECURE(GET_ENTITY_HEIGHT_ABOVE_GROUND,0x07b3d89ed0c66854,		CommandGetEntityHeightAboveGround);
		SCR_REGISTER_SECURE(GET_ENTITY_MATRIX,0xf1282bfc5af4ac08,		CommandGetEntityMatrix);
		SCR_REGISTER_SECURE(GET_ENTITY_MODEL,0x15caa6d7b11f1a7c,		CommandGetEntityModel);

		SCR_REGISTER_SECURE(GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS,0x37b78bef7c4625d4,		CommandGetOffsetFromEntityGivenWorldCoords);
		SCR_REGISTER_SECURE(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,0xcacad935c0bee14f,		CommandGetOffsetFromEntityInWorldCoords);

		SCR_REGISTER_SECURE(GET_ENTITY_PITCH,0xb8f48796849e41e3,		CommandGetEntityPitch);
		SCR_REGISTER_SECURE(GET_ENTITY_QUATERNION,0x1d11e13ece77f8dd,		CommandGetEntityQuaternion);
		SCR_REGISTER_SECURE(GET_ENTITY_ROLL,0x89d8ea6be9e3f747,		CommandGetEntityRoll);
		SCR_REGISTER_SECURE(GET_ENTITY_ROTATION,0x63a8bf5e6c2bf85c,		CommandGetEntityRotation);
		SCR_REGISTER_SECURE(GET_ENTITY_ROTATION_VELOCITY,0x511e2827d5a26942,		CommandGetEntityRotationVelocity);
		SCR_REGISTER_SECURE(GET_ENTITY_SCRIPT,0x24466a99e2719412,		CommandGetEntityScript);
		SCR_REGISTER_SECURE(GET_ENTITY_SPEED,0x207d53f9e0190691,		CommandGetEntitySpeed);
		SCR_REGISTER_SECURE(GET_ENTITY_SPEED_VECTOR,0x880984103df05f6a,		CommandGetEntitySpeedVector);
		SCR_REGISTER_SECURE(GET_ENTITY_UPRIGHT_VALUE,0x90544f2e01a178cd,		CommandGetEntityUprightValue);
		SCR_REGISTER_SECURE(GET_ENTITY_VELOCITY,0xc35fbd95659582c4,		CommandGetEntityVelocity);

		SCR_REGISTER_SECURE(GET_OBJECT_INDEX_FROM_ENTITY_INDEX,0x1aa24836ea881d77,		CommandGetObjectIndexFromEntityIndex);

//		SCR_REGISTER_SECURE(GET_OFFSET_FROM_COORD_AND_HEADING_IN_WORLD_COORDS,0x6db7fbd602c51670,		CommandGetOffsetFromCoordAndHeadingInWorldCoords);	
		SCR_REGISTER_SECURE(GET_PED_INDEX_FROM_ENTITY_INDEX,0x28aa31872a651bc7,		CommandGetPedIndexFromEntityIndex);
		SCR_REGISTER_SECURE(GET_VEHICLE_INDEX_FROM_ENTITY_INDEX,0x31eb55faeee9c0f5,		CommandGetVehicleIndexFromEntityIndex);
		SCR_REGISTER_SECURE(GET_WORLD_POSITION_OF_ENTITY_BONE,0x96eda732fd9dc0af,		CommandGetWorldPositionOfEntityBone);
		SCR_REGISTER_SECURE(GET_NEAREST_PLAYER_TO_ENTITY,0xd632326024550b66,		CommandGetNearestPlayerToEntity);
		SCR_REGISTER_SECURE(GET_NEAREST_PLAYER_TO_ENTITY_ON_TEAM,0x9b7d7680cfd3f710,		CommandGetNearestPlayerToEntityOnTeam);
		SCR_REGISTER_UNUSED(GET_NEAREST_PLAYER_ON_LOCAL_PLAYER_TUTORIAL_TO_ENTITY,0x61eee7cc2c8b7501,	CommandGetNearestPlayerOnLocalPlayerTutorialToEntity);
		SCR_REGISTER_UNUSED(GET_NEAREST_PARTICIPANT_TO_ENTITY,0x936629dedc5fc52b,		CommandGetNearestParticipantToEntity);
		SCR_REGISTER_SECURE(GET_ENTITY_TYPE,0x0fbf5ac4ba2f3447,		CommandGetEntityType);
		SCR_REGISTER_SECURE(GET_ENTITY_POPULATION_TYPE,0x349120c74cb8b967,		CommandGetEntityPopulationType);		
		SCR_REGISTER_SECURE(IS_AN_ENTITY,0x9a39b7e8a645ad2a,		CommandIsAnEntity);
		SCR_REGISTER_SECURE(IS_ENTITY_A_PED,0x9044edb8a7bf67b4,		CommandIsEntityAPed);
		SCR_REGISTER_SECURE(IS_ENTITY_A_MISSION_ENTITY,0xa6a089369417736e,		CommandIsEntityAMissionEntity);
		SCR_REGISTER_SECURE(IS_ENTITY_A_VEHICLE,0x04d9f44466cbf3ca,		CommandIsEntityAVehicle);
		SCR_REGISTER_SECURE(IS_ENTITY_AN_OBJECT,0x0086095f1731de17,		CommandIsEntityAnObject);
		SCR_REGISTER_SECURE(IS_ENTITY_AT_COORD,0x8f41785f110b0da0,		CommandIsEntityAtCoord); 
		SCR_REGISTER_SECURE(IS_ENTITY_AT_ENTITY,0x77a43afa9aaec041,		CommandIsEntityAtEntity); 
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED,0xe85f749f6d5c809e,		CommandIsEntityAttached);
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED_TO_ANY_OBJECT,0xb6c8533a238c4b1e,		CommandIsEntityAttachedToAnyObject);
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED_TO_ANY_PED,0x508cdc652f5361b5,		CommandIsEntityAttachedToAnyPed);
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED_TO_ANY_VEHICLE,0xe2ed785e8db4d3ff,		CommandIsEntityAttachedToAnyVehicle);
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED_TO_ENTITY,0x099cad293190f449,		CommandIsEntityAttachedToEntity);
		SCR_REGISTER_SECURE(IS_ENTITY_DEAD,0x55b23fe400fcea6b,		CommandIsEntityDead);
		SCR_REGISTER_SECURE(IS_ENTITY_IN_AIR,0xce282187b0a6217e,		CommandIsEntityInAir);
		SCR_REGISTER_SECURE(IS_ENTITY_IN_ANGLED_AREA,0xd132edda78ff4a51,		CommandIsEntityInAngledArea); 
		SCR_REGISTER_SECURE(IS_ENTITY_IN_AREA,0xb950b5d01e8a4868,		CommandIsEntityInArea); 
		SCR_REGISTER_SECURE(IS_ENTITY_IN_ZONE,0xa07e66961378a6b4,		CommandIsEntityInZone);
		SCR_REGISTER_SECURE(IS_ENTITY_IN_WATER,0xd8f9df94cd871327,		CommandIsEntityInWater);
		SCR_REGISTER_SECURE(GET_ENTITY_SUBMERGED_LEVEL,0x1cc29feb3b8d48e8,		CommandGetEntitySubmergedLevel);
		SCR_REGISTER_SECURE(SET_ENTITY_REQUIRES_MORE_EXPENSIVE_RIVER_CHECK,0x5337c7397bd43696,		CommandSetEntityRequiresMoreExpensiveRiverCheck);
		SCR_REGISTER_SECURE(IS_ENTITY_ON_SCREEN,0xbc7d3edf3455bc35,		CommandIsEntityOnScreen);
		SCR_REGISTER_SECURE(IS_ENTITY_PLAYING_ANIM,0xfcf559c8631abed7,		CommandIsEntityPlayingAnim);
		SCR_REGISTER_SECURE(IS_ENTITY_STATIC,0x23cb5c83d3766044,		CommandIsEntityStatic);		
		SCR_REGISTER_SECURE(IS_ENTITY_TOUCHING_ENTITY,0x72f6ae51b7e34865,		CommandIsEntityTouchingEntity);
		SCR_REGISTER_SECURE(IS_ENTITY_TOUCHING_MODEL,0x15198bc2115f8dc9,		CommandIsEntityTouchingModel);
		SCR_REGISTER_SECURE(IS_ENTITY_UPRIGHT,0xefd1745e9d77e91c,		CommandIsEntityUpright);
		SCR_REGISTER_SECURE(IS_ENTITY_UPSIDEDOWN,0x68c47642183f091a,		CommandIsEntityUpsideDown);
		SCR_REGISTER_SECURE(IS_ENTITY_VISIBLE,0x2b5f4fbaf644bec8,		CommandIsEntityVisible);
		SCR_REGISTER_SECURE(IS_ENTITY_VISIBLE_TO_SCRIPT,0x4a1ea713628c49d4,		CommandIsEntityVisibleToScript);
	//	SCR_REGISTER_UNUSED(IS_ENTITY_IN_VIEWPORT,								0x99779104,		CommandIsEntityInViewport);
		SCR_REGISTER_SECURE(IS_ENTITY_OCCLUDED,0x4d21c9fb671ed18f,		CommandIsEntityOccluded);
		SCR_REGISTER_SECURE(WOULD_ENTITY_BE_OCCLUDED,0x91194f9ab8ba4e66,		CommandWouldEntityBeOccluded);
		SCR_REGISTER_SECURE(IS_ENTITY_WAITING_FOR_WORLD_COLLISION,0x301c07463034c65f,		CommandIsEntityWaitingForWorldCollision);
	//	SCR_REGISTER_SECURE(IS_POINT_IN_ANGLED_AREA,0x5f8653e60ed2288e,		CommandIsPointInAngledArea);

		// modifying commands
		SCR_REGISTER_SECURE(APPLY_FORCE_TO_ENTITY_CENTER_OF_MASS,0x57ffaa29e06177f6,		CommandApplyForceToEntityCenterOfMass);
		SCR_REGISTER_SECURE(APPLY_FORCE_TO_ENTITY,0x3f6de6dba9a99fea,		CommandApplyForceToEntity);
		SCR_REGISTER_SECURE(ATTACH_ENTITY_TO_ENTITY,0x9a97dc6dbc7b223d,		CommandAttachEntityToEntity);
		SCR_REGISTER_SECURE(ATTACH_ENTITY_BONE_TO_ENTITY_BONE,0x93c635be37089a75,		CommandAttachEntityBoneToEntityBone);
	    SCR_REGISTER_SECURE(ATTACH_ENTITY_BONE_TO_ENTITY_BONE_Y_FORWARD,0xc2e0445c5728de31,		CommandAttachEntityBoneToEntityBoneYForward);
		SCR_REGISTER_SECURE(ATTACH_ENTITY_TO_ENTITY_PHYSICALLY,0xd1dfcb46f41425eb,		CommandAttachEntityToEntityPhysically);
		SCR_REGISTER_UNUSED(ATTACH_ENTITY_TO_ENTITY_PHYSICALLY_OVERRIDE_INVERSE_MASS,0x134d25a048863393,		CommandAttachEntityToEntityPhysicallyOverrideInverseMass);
		SCR_REGISTER_UNUSED(ATTACH_ENTITY_TO_WORLD_PHYSICALLY,0xb5efa5253ad6dc74,		CommandAttachEntityToWorldPhysically);
		SCR_REGISTER_SECURE(PROCESS_ENTITY_ATTACHMENTS,0xed905e75bf9b3a2e,		CommandProcessEntityAttachments);
		SCR_REGISTER_UNUSED(ATTACH_ENTITY_BONE_TO_BONE_PHYSICALLY,0xf6d171db025f64f0,		CommandAttachEntityBoneToBonePhysically);
		SCR_REGISTER_SECURE(GET_ENTITY_BONE_INDEX_BY_NAME,0x7f0054a115d66f44,		CommandGetEntityBoneIndexByName);
		SCR_REGISTER_SECURE(CLEAR_ENTITY_LAST_DAMAGE_ENTITY,0xe68645525d451a8b,		CommandClearEntityLastDamageEntity);
		SCR_REGISTER_SECURE(DELETE_ENTITY,0x531055a66e7a812c,		CommandDeleteEntity);
		SCR_REGISTER_SECURE(DETACH_ENTITY,0xe6451c2f193342c7,		CommandDetachEntity);
		SCR_REGISTER_SECURE(FREEZE_ENTITY_POSITION,0x5c65dddc219b3aa5,		CommandFreezeEntityPosition);
		SCR_REGISTER_SECURE(SET_ENTITY_SHOULD_FREEZE_WAITING_ON_COLLISION,0x398c962f550cf3b4,		CommandSetEntityShouldFreezeWaitingOnCollision);
		SCR_REGISTER_UNUSED(GET_IS_ALLOW_FREEZE_WAITING_ON_COLLISION_SET,0x6a1fddb971d4baa1,		CommandIsAllowFreezeWaitingOnCollisionSet);
		SCR_REGISTER_SECURE(PLAY_ENTITY_ANIM,0xf07805ffbf204ed8,		CommandPlayEntityAnim);
		SCR_REGISTER_SECURE(PLAY_SYNCHRONIZED_ENTITY_ANIM,0xaa8557afe4a4a184,		CommandPlaySynchronizedEntityAnim);
		SCR_REGISTER_SECURE(PLAY_SYNCHRONIZED_MAP_ENTITY_ANIM,0xb7dda95ccbc190a3,		CommandPlaySynchronizedMapEntityAnim);
		SCR_REGISTER_SECURE(STOP_SYNCHRONIZED_MAP_ENTITY_ANIM,0xbd673c6a813ec48e,		CommandStopSynchronizedMapEntityAnim);
		SCR_REGISTER_SECURE(STOP_ENTITY_ANIM,0xa02157241423f32e,		CommandStopEntityAnim);
		SCR_REGISTER_SECURE(STOP_SYNCHRONIZED_ENTITY_ANIM,0xa5d8ba53953077c8,		CommandStopSynchronizedEntityAnim);

		SCR_REGISTER_SECURE(HAS_ANIM_EVENT_FIRED,0xd9e56ec8ed04e45e,		CommandHasAnimEventFired);
		SCR_REGISTER_SECURE(FIND_ANIM_EVENT_PHASE,0x0af1dad2f59b2c3c,		CommandFindAnimEventPhase);
		SCR_REGISTER_UNUSED(FIND_ANIM_EVENT_IN_PHASE_RANGE,0xc0e629b38e673c59,		CommandFindAnimEventInPhaseRange);
		//SCR_REGISTER_UNUSED(GET_INT_ATTRIBUTE_FROM_ANIM_EVENT,					0xe3ae8e1a,		CommandGetIntAttributeFromAnimEvent);
		//SCR_REGISTER_UNUSED(GET_FLOAT_ATTRIBUTE_FROM_ANIM_EVENT,				0x41d0606c,		CommandGetFloatAttributeFromAnimEvent);
		//SCR_REGISTER_UNUSED(GET_VECTOR_ATTRIBUTE_FROM_ANIM_EVENT,				0xe3890102,		CommandGetVectorAttributeFromAnimEvent);

		SCR_REGISTER_UNUSED(SET_ENTITY_ALL_ANIMS_SPEED,0x74c650ed23b897c8,		CommandSetEntityAllAnimsSpeed);
		SCR_REGISTER_UNUSED(SET_ENTITY_ANIM_BLEND_OUT_SPEED,0xc610444fa4cc68f9,		CommandSetEntityAnimBlendOutSpeed);	
		SCR_REGISTER_SECURE(SET_ENTITY_ANIM_CURRENT_TIME,0x12ffbbb6191db79f,		CommandSetEntityAnimCurrentTime);
		SCR_REGISTER_SECURE(SET_ENTITY_ANIM_SPEED,0xb5cb6bc7ad4cdcce,		CommandSetEntityAnimSpeed);
		SCR_REGISTER_SECURE(SET_ENTITY_AS_MISSION_ENTITY,0x2d58a6131679d82c,		CommandSetEntityAsMissionEntity);

		SCR_REGISTER_SECURE(SET_ENTITY_AS_NO_LONGER_NEEDED,0xec548b5e95e61df6,		CommandSetEntityAsNoLongerNeeded);
		SCR_REGISTER_SECURE(SET_PED_AS_NO_LONGER_NEEDED,0xb3822db3d538c8f3,		CommandSetEntityAsNoLongerNeeded);
		SCR_REGISTER_SECURE(SET_VEHICLE_AS_NO_LONGER_NEEDED,0x3d81769bec61bff8,		CommandSetEntityAsNoLongerNeeded);
		SCR_REGISTER_SECURE(SET_OBJECT_AS_NO_LONGER_NEEDED,0x59e393b344098656,		CommandSetEntityAsNoLongerNeeded);

		SCR_REGISTER_SECURE(SET_ENTITY_CAN_BE_DAMAGED,0xfef687af74eedcc2,		CommandSetEntityCanBeDamaged);
		SCR_REGISTER_SECURE(GET_ENTITY_CAN_BE_DAMAGED,0xd8c646428b492be8,		CommandGetEntityCanBeDamaged);
		SCR_REGISTER_SECURE(SET_ENTITY_CAN_BE_DAMAGED_BY_RELATIONSHIP_GROUP,0xe701941b064f05a2,		CommandSetEntityCanBeDamagedByRelGroup);
		SCR_REGISTER_SECURE(SET_ENTITY_CAN_ONLY_BE_DAMAGED_BY_SCRIPT_PARTICIPANTS,0xd1c117aeb22f08e1,	CommandSetEntityCanOnlyBeDamagedByScriptParticipants);
		SCR_REGISTER_SECURE(SET_ENTITY_CAN_BE_TARGETED_WITHOUT_LOS,0xa93da249dfb26a26,		CommandSetEntityCanBeTargetedWithoutLos);
		SCR_REGISTER_SECURE(SET_ENTITY_COLLISION,0x959e1626cbc7d38a,		CommandSetEntityCollision);
		SCR_REGISTER_SECURE(GET_ENTITY_COLLISION_DISABLED,0x1f723890b0fbffea,		CommandGetEntityCollisionDisabled);
		SCR_REGISTER_SECURE(SET_ENTITY_COMPLETELY_DISABLE_COLLISION,0x3ff6dde9123aea35,		CommandSetEntityCompletelyDisableCollision);
		SCR_REGISTER_SECURE(SET_ENTITY_COORDS,0xc64b6e13a6a7f517,		CommandSetEntityCoords);
		SCR_REGISTER_SECURE(SET_ENTITY_COORDS_WITHOUT_PLANTS_RESET,0x3411b7feaefb2ff1,		CommandSetEntityCoordsWithoutPlantsReset);
		SCR_REGISTER_SECURE(SET_ENTITY_COORDS_NO_OFFSET,0xa539ede8da5bbc22,		CommandSetEntityCoordsNoOffset);
		SCR_REGISTER_SECURE(SET_ENTITY_DYNAMIC,0x4536ae3dabde0c44,		CommandSetEntityDynamic);		
		SCR_REGISTER_SECURE(SET_ENTITY_HEADING,0xbbf86885619695ce,		CommandSetEntityHeading);
		SCR_REGISTER_SECURE(SET_ENTITY_HEALTH,0xf160248d9083ed0c,		CommandSetEntityHealth);
		SCR_REGISTER_UNUSED(SET_ENTITY_INITIAL_VELOCITY,0x1dc52d7621b85540,		CommandSetEntityInitialVelocity);
		SCR_REGISTER_SECURE(SET_ENTITY_INVINCIBLE,0x9210f85e9cd785f1,		CommandSetEntityInvincible);
		SCR_REGISTER_SECURE(SET_ENTITY_IS_TARGET_PRIORITY,0x06af73d7e3df7782,		CommandSetEntityIsTargetPriority);
		SCR_REGISTER_SECURE(SET_ENTITY_LIGHTS,0xce8331cb5040e4b2,		CommandSetEntityLights);
		SCR_REGISTER_SECURE(SET_ENTITY_LOAD_COLLISION_FLAG,0x006574e87e6f1942,		CommandSetEntityLoadCollisionFlag);
		SCR_REGISTER_SECURE(HAS_COLLISION_LOADED_AROUND_ENTITY,0xf1e076f6cc1695aa,		CommandHasCollisionLoadedAroundEntity);
		SCR_REGISTER_SECURE(SET_ENTITY_MAX_SPEED,0x96ba5a8588a1f329,		CommandSetEntityMaxSpeed);
		SCR_REGISTER_SECURE(SET_ENTITY_ONLY_DAMAGED_BY_PLAYER,0x0407cf7efee35240,		CommandSetEntityOnlyDamagedByPlayer);
		SCR_REGISTER_SECURE(SET_ENTITY_ONLY_DAMAGED_BY_RELATIONSHIP_GROUP,0x8a8909baddfa93a6,		CommandSetEntityOnlyDamagedByRelGroup);
		SCR_REGISTER_SECURE(SET_ENTITY_PROOFS,0xbed097bb6359f09c,		CommandSetEntityProofs);
		SCR_REGISTER_SECURE(GET_ENTITY_PROOFS,0x89d0663d5c95cd4b, CommandGetEntityProofs);
		SCR_REGISTER_SECURE(SET_ENTITY_QUATERNION,0x03f4139102ba4afc,		CommandSetEntityQuaternion);
		SCR_REGISTER_SECURE(SET_ENTITY_RECORDS_COLLISIONS,0x3aa7fa9ba4f7a9e7,		CommandSetEntityRecordsCollisions);		
		SCR_REGISTER_SECURE(SET_ENTITY_ROTATION,0x8ce3d365f064f69e,		CommandSetEntityRotation);
		SCR_REGISTER_SECURE(SET_ENTITY_VISIBLE,0x1c2ed929474dc6fe,		CommandSetEntityVisible);
		SCR_REGISTER_SECURE(SET_ENTITY_WATER_REFLECTION_FLAG,0x0106728632d8bfe7,		CommandSetEntityWaterReflectionFlag);
		SCR_REGISTER_SECURE(SET_ENTITY_MIRROR_REFLECTION_FLAG,0x22fac08c55bb0816,		CommandSetEntityMirrorReflectionFlag);
		SCR_REGISTER_SECURE(SET_ENTITY_VELOCITY,0xb96633932c330b51,		CommandSetEntityVelocity);
		SCR_REGISTER_SECURE(SET_ENTITY_ANGULAR_VELOCITY,0xa086b3ae27b69b2c,		CommandSetEntityAngularVelocity);
		SCR_REGISTER_SECURE(SET_ENTITY_HAS_GRAVITY,0x823ac5a743053194,		CommandSetEntityHasGravity);
		SCR_REGISTER_UNUSED(SET_ENTITY_DRAWABLE_LOD_THRESHOLDS, 0x8e77726f2294dd5b,		CommandSetEntityDrawableLodThresholds);
		SCR_REGISTER_SECURE(SET_ENTITY_LOD_DIST,0x7a93dea53efc3cf2,		CommandSetEntityLodDist);
		SCR_REGISTER_SECURE(GET_ENTITY_LOD_DIST,0x8ac745b92a62ff20,		CommandGetEntityLodDist);
		SCR_REGISTER_SECURE(SET_ENTITY_ALPHA,0x8b9ad14c7d42cc13,		CommandSetEntityAlpha);
		SCR_REGISTER_SECURE(GET_ENTITY_ALPHA,0x84724f93d2180bcc,		CommandGetEntityAlpha);
		SCR_REGISTER_SECURE(RESET_ENTITY_ALPHA,0xeafaf9ed80db448d,		CommandResetEntityAlpha);
		SCR_REGISTER_SECURE(RESET_PICKUP_ENTITY_GLOW,0x4c9283de8fdb6a51, CommandResetPickupEntityGlow);
		SCR_REGISTER_SECURE(SET_PICKUP_COLLIDES_WITH_PROJECTILES,0x6da15de1822b4027,	CommandSetPickupCollidesWithProjectiles);
		SCR_REGISTER_SECURE(SET_ENTITY_SORT_BIAS,0xd398de5246e16a9e,	CommandSetEntitySortBias);
		SCR_REGISTER_UNUSED(RESET_ENTITY_SORT_BIAS,0x277b8c6dd1c05c07,	CommandResetEntitySortBias);
		SCR_REGISTER_SECURE(SET_ENTITY_ALWAYS_PRERENDER,0x62a01b7866de3fb4,		CommandSetEntityAlwaysPreRender);
		SCR_REGISTER_SECURE(SET_ENTITY_RENDER_SCORCHED,0x929457eb603e9f3a,		CommandSetEntityRenderScorchedFlag);
		SCR_REGISTER_SECURE(SET_ENTITY_TRAFFICLIGHT_OVERRIDE,0xf7bcac74d883d0d1,		CommandSetTrafficLightOverride);
		SCR_REGISTER_SECURE(SET_ENTITY_IS_IN_VEHICLE,0xff8a4252fbf3fbca,		CommandSetEntityInVehicle);
		SCR_REGISTER_SECURE(CREATE_MODEL_SWAP,0xb17fdb8078b01a51,		CommandCreateModelSwap);
		SCR_REGISTER_SECURE(REMOVE_MODEL_SWAP,0x7da16d38ad07a674,		CommandRemoveModelSwap);
		SCR_REGISTER_SECURE(CREATE_MODEL_HIDE,0xe87a5b1b96b0ec6f,		CommandCreateModelHide);
		SCR_REGISTER_SECURE(CREATE_MODEL_HIDE_EXCLUDING_SCRIPT_OBJECTS,0xca2a868fd7906603,		CommandCreateModelHideExcludingScriptObjects);
		SCR_REGISTER_SECURE(REMOVE_MODEL_HIDE,0xa1f6e5ef42acdb8c,		CommandRemoveModelHide);
		SCR_REGISTER_SECURE(CREATE_FORCED_OBJECT,0xea77adc78e693681,		CommandCreateForcedObject);
		SCR_REGISTER_SECURE(REMOVE_FORCED_OBJECT,0x73bda6fe727ad754,		CommandRemoveForcedObject);
		SCR_REGISTER_SECURE(SET_ENTITY_NO_COLLISION_ENTITY,0x14ad86219fe1ac08,		CommandSetEntityNoCollisionEntity);
		SCR_REGISTER_SECURE(SET_ENTITY_MOTION_BLUR,0xbf5acf94c9b79b3d,		CommandSetEntityMotionBlur);
		SCR_REGISTER_SECURE(SET_CAN_AUTO_VAULT_ON_ENTITY,0x419302b4e1d43eed,		CommandSetCanAutovaultOnEntity);
		SCR_REGISTER_SECURE(SET_CAN_CLIMB_ON_ENTITY,0x82b0e749922635bc,		CommandSetCanClimbOnEntity);
		SCR_REGISTER_SECURE(SET_WAIT_FOR_COLLISIONS_BEFORE_PROBE,0x5a3c417dfb231aa1,		CommandSetWaitForCollisionsBeforeProbe);
		SCR_REGISTER_SECURE(SET_ENTITY_NOWEAPONDECALS,0x4e02b93acfb7b3a5,CommandSetEntityNoWeaponDecals);
		SCR_REGISTER_UNUSED(GET_ENTITY_NOWEAPONDECALS,0x8a3489f8359b5526,CommandGetEntityNoWeaponDecals);
		SCR_REGISTER_SECURE(SET_ENTITY_USE_MAX_DISTANCE_FOR_WATER_REFLECTION,0xb28d5be976e5890c, CommandSetEntityUseMaxDistanceForWaterReflection);

		SCR_REGISTER_SECURE(GET_ENTITY_BONE_ROTATION,0x9c668c5b3efa1dec, CommandGetEntityBoneRotation);
		SCR_REGISTER_SECURE(GET_ENTITY_BONE_POSTION,0x851cfde0cbc150ec, CommandGetEntityBonePosition);
		SCR_REGISTER_SECURE(GET_ENTITY_BONE_OBJECT_ROTATION,0x69feba414276c40e, CommandGetEntityBoneObjectRotation);
		SCR_REGISTER_UNUSED(GET_ENTITY_BONE_OBJECT_POSTION,0x7bf851d590d0eb6c, CommandGetEntityBoneObjectPosition);

		SCR_REGISTER_SECURE(GET_ENTITY_BONE_COUNT,0x8f288b72a0b9e6ed, CommandGetEntityBoneCount);

		SCR_REGISTER_SECURE(ENABLE_ENTITY_BULLET_COLLISION,0xe14c76331d5aa34c, CommandEnableEntityBulletCollision);

		SCR_REGISTER_SECURE(SET_ENTITY_CAN_ONLY_BE_DAMAGED_BY_ENTITY,0x44ad5947dd467f0e, CommandSetEntityCanOnlyDamageEntity);
		SCR_REGISTER_UNUSED(RESET_ENTITY_CAN_ONLY_BE_DAMAGED_BY_ENTITY,0x77f4f312ada59c40, CommandResetEntityCanOnlyDamageEntity);

		SCR_REGISTER_SECURE(SET_ENTITY_CANT_CAUSE_COLLISION_DAMAGED_ENTITY,0xc3317b4bca8328b9, CommandSetEntityCantCauseCollisionDamageEntity);
		SCR_REGISTER_UNUSED(RESET_ENTITY_CANT_CAUSE_COLLISION_DAMAGED_ENTITY,0xddfc82bd082d57f6, CommandResetEntityCantCauseCollisionDamageEntity);

		SCR_REGISTER_SECURE(SET_ALLOW_MIGRATE_TO_SPECTATOR,0x64ab3102e05d0d75, CommandSetAllowMigrateToSpectator);

		SCR_REGISTER_SECURE(GET_ENTITY_OF_TYPE_ATTACHED_TO_ENTITY,0xe0ae41c323ad1f1b, CommandGetEntityOfTypeAttachedToEntity);

		SCR_REGISTER_SECURE(SET_PICK_UP_BY_CARGOBOB_DISABLED,0x60104a96124e0b12, CommandSetPickupByCargobobDisabled);
		
	}

}	//	end of namespace ped_commands
