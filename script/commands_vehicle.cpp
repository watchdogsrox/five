// Rage headers
#include "script/wrapper.h"
#include "phbound/boundcomposite.h"
#include "phcore/phmath.h"
#include "fwanimation/animmanager.h"
#include "fwmaths/angle.h"
#include "fwmaths/Vector.h"
#include "fwscene/world/WorldLimits.h"

// Framework headers
#include "fwdecorator/decoratorExtension.h"

// Game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "audio/heliaudioentity.h"
#include "audio/planeaudioentity.h"
#include "audio/caraudioentity.h"
#include "camera/CamInterface.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "control/garages.h"
#include "control/record.h"
#include "control/trafficlights.h"
#include "game/ModelIndices.h"
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "network/events/NetworkEventTypes.h"
#include "network/General/NetworkVehicleModelDoorLockTable.h"
#include "network/Network.h"
#include "pathserver/PathServer.h"
#include "peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/pedpopulation.h"
#include "peds/popcycle.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "physics/breakable.h"
#include "physics/gtaInst.h"
#include "physics/Physics.h"
#include "physics/Tunable.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/OcclusionQueries.h"
#include "scene/world/gameWorld.h"
#include "script/array.h"
#include "script/commands_ped.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/Script.h"
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/script_shapetests.h"
#include "script/commands_shapetest.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "shaders/CustomShaderEffectProp.h"
#include "streaming/populationstreaming.h"
#include "Task/Animation/TaskCutScene.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Combat/Cover/Cover.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "VehicleAi/Task/TaskVehicleGoToPlane.h"
#include "vehicleAi/roadspeedzone.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicles/Automobile.h"
#include "vehicles/Bike.h"
#include "vehicles/Boat.h"
#include "vehicles/cargen.h"
#include "vehicles/Heli.h"
#include "vehicles/Planes.h"
#include "vehicles/train.h"
#include "vehicles/Vehicle.h"
#include "vehicles/vehicleDamage.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/Vehiclepopulation.h"
#include "vehicles/buses.h"
#include "vehicles/planes.h"
#include "Vehicles/Submarine.h"
#include "vehicles/trailer.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/metadata/vehiclescriptresource.h"
#include "vehicles/metadata/vehiclelayoutinfo.h"
#include "vehicles/metadata/vehicleseatinfo.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/DistantLights.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "cutscene/AnimatedModelEntity.h"
#include "network/NetworkInterface.h"

SCRIPT_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

namespace vehicle_commands
{
	int CommandCreateVehicle(int VehicleModelHashKey, const scrVector & scrVecCoors, float fHeading, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bIgnoreGroundCheck)
	{
		Vector3 VecCoors =  Vector3 (scrVecCoors);
		int NewVehicleIndex = 0;

		// non-networked vehicles should always be treated as client objects
		if (!bRegisterAsNetworkObject) 
			bScriptHostObject = false;

		if(!CVehiclePopulation::CanGenerateMissionCar())
		{
#if __BANK
			CVehiclePopulation::DumpVehiclePoolToLog();
#endif
			SCRIPT_ASSERT(CVehiclePopulation::CanGenerateMissionCar(), "CREATE_VEHICLE - Too many mission vehicles. See log for details");
			return NewVehicleIndex;
		}

		if (SCRIPT_VERIFY(!CNetwork::IsGameInProgress() || !bRegisterAsNetworkObject || CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "CREATE_VEHICLE - Non-networked scripts that are safe to run during the network game cannot create networked entities"))
		{
			if (SCRIPT_VERIFY((rage::Abs(VecCoors.x) <WORLDLIMITS_XMAX && rage::Abs(VecCoors.y) < WORLDLIMITS_YMAX && rage::Abs(VecCoors.z) < WORLDLIMITS_ZMAX), "CREATE_VEHICLE - Coordinates out of range"))
			{
				if (bScriptHostObject && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
				{
					if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_VEHICLE - Non-host machines cannot create host vehicles"))
					{
						return NewVehicleIndex;
					}
				}

				fwModelId VehicleModelId;
				CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value

				if( scriptVerifyf( VehicleModelId.IsValid(), "%s:CREATE_VEHICLE - this is not a valid model index", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
					scriptVerifyf(CModelInfo::GetBaseModelInfo(VehicleModelId), "%s:CREATE_VEHICLE - Specified model isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
					scriptVerifyf(CModelInfo::GetBaseModelInfo(VehicleModelId)->GetHasLoaded(), "%s:CREATE_VEHICLE - Specified model isn't loaded!", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
					scriptVerifyf(CModelInfo::GetBaseModelInfo(VehicleModelId)->GetModelType() == MI_TYPE_VEHICLE, "%s:CREATE_VEHICLE - Specified model isn't actually a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
				{
#if !__NO_OUTPUT
                    bool bNonNetworkedDueToCutscene = false;
#endif // !__NO_OUTPUT

					// don't network vehicles created during a cutscene
					if (!NetworkInterface::AreCutsceneEntitiesNetworked() && CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene())
					{
						if (scriptVerifyf(NetworkInterface::IsInMPCutscene(), "Script cutscene flag still set when not in a cutscene"))
						{
#if !__NO_OUTPUT
                            if(bRegisterAsNetworkObject)
                            {
                                bNonNetworkedDueToCutscene = true;
                            }
#endif // !__NO_OUTPUT
							bRegisterAsNetworkObject = false;
						}
						else
						{
							CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetInMPCutscene(false);
						}
					}

					CVehicle* pNewVehicle = CVehiclePopulation::CreateCarForScript(VehicleModelId.GetModelIndex(), VecCoors, fHeading, bRegisterAsNetworkObject, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), bIgnoreGroundCheck);	//	second last parameter (bAddToMissionCleanup) used to be CTheScripts::GetCurrentGtaScriptThread()->IsThisAMissionScript

					if(scriptVerifyf(pNewVehicle,"%s:CREATE_VEHICLE Failed to create vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						NewVehicleIndex = CTheScripts::GetGUIDFromEntity(*pNewVehicle);
		
						CTheScripts::RegisterEntity(pNewVehicle, bScriptHostObject, bRegisterAsNetworkObject);

#if !__NO_OUTPUT
                        if(NetworkInterface::IsGameInProgress() && !bRegisterAsNetworkObject)
                        {
                            scriptDebugf1("%s: Creating non-networked vehicle %p (None networked due to cutscene: %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pNewVehicle, bNonNetworkedDueToCutscene ? "TRUE" : "FALSE");
                            NOTFINAL_ONLY(scrThread::PrePrintStackTrace();)
                        }
#endif // !__NO_OUTPUT
					}
				}
			}
		}
		return NewVehicleIndex;
	}

	void DeleteScriptVehicle(CVehicle *pVehicle)
	{
		if (scriptVerifyf(pVehicle, "DeleteScriptVehicle - vehicle doesn't exist"))
		{
			pVehicle->m_nVehicleFlags.bDontStoreAsPersistent = true;

			// remove all drivers and passengers from the vehicle being deleted
			CNetObjVehicle *vehicleNetObj = static_cast<CNetObjVehicle *>(pVehicle->GetNetworkObject());

			if (vehicleNetObj)
			{
				vehicleNetObj->EvictAllPeds();
			}
#if __DEV
			scriptAssertf(!pVehicle->ContainsPlayer(), "DeleteScriptVehicle - Vehicle \"%s\" is being deleted whilst it contains a player.", pVehicle->GetModelName());
#endif
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
		}
	}

	void CommandDeleteVehicle(int &VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

		if (pVehicle)
		{
#if __ASSERT
			if(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN)
			{
				CTrain * pTrain = (CTrain*)pVehicle;
				if(pTrain->m_nTrainFlags.bEngine && pTrain->GetLinkedToBackward())
				{
					Assertf(false, "This script has called DELETE_VEHICLE on a train with attached carriages.  They will not be removed.  You should call DELETE_MISSION_TRAIN instead.");
				}
			}
#endif

			CScriptEntityExtension* pExtension = pVehicle->GetExtension<CScriptEntityExtension>();

#if __ASSERT
			Assertf(pExtension, "%s: DELETE_VEHICLE - The vehicle \'%s\' is not a script entity", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetBaseModelInfo()->GetModelName());
#endif

			if (pExtension &&
				SCRIPT_VERIFY(pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler(), "DELETE_VEHICLE - The vehicle was not created by this script"))
			{
				DeleteScriptVehicle(pVehicle);
			}
		}

		VehicleIndex = 0;
	}

	void CommandSetVehicleInCutscene(int VehicleIndex, bool bShow)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_IN_CUTSCENE - Vehicle has to be a misison vehicle"))
			{
				pVehicle->m_nVehicleFlags.bHideInCutscene = !bShow;
			}
		}
	}

	void CommandSetVehicleAllowHomingMissleLockon(int VehicleIndex, bool bAllowHomingMissleLockon, bool bIgnoreMisisonVehCheck)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(bIgnoreMisisonVehCheck || pVehicle->PopTypeIsMission(), "SET_VEHICLE_ALLOW_HOMING_MISSLE_LOCKON - Vehicle has to be a mission vehicle"))
			{
				pVehicle->m_nVehicleFlags.bAllowHomingMissleLockon = bAllowHomingMissleLockon;
			}
		}
	}

	void CommandSetVehicleDisableCollisionUponCreation(int VehicleIndex, bool disableCollision)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());

			if(netObjVehicle)
			{
				gnetDebug1("Calling SET_VEHICLE_DISABLE_COLLISION_UPON_CREATION on %s with disableCollision = %s", netObjVehicle->GetLogName(), disableCollision ? "True" : "False");
				netObjVehicle->SetDisableCollisionUponCreation(disableCollision);
			}
		}
	}

	void CommandSetVehicleAllowHomingMissleLockonSynced(int VehicleIndex, bool bAllowHomingMissleLockon, bool bIgnoreMisisonVehCheck)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(bIgnoreMisisonVehCheck || pVehicle->PopTypeIsMission(), "SET_VEHICLE_ALLOW_HOMING_MISSLE_LOCKON_SYNCED - Vehicle has to be a mission vehicle"))
			{
				pVehicle->m_bAllowHomingMissleLockOnSynced = bAllowHomingMissleLockon;
			}
		}
	}

	void CommandSetVehicleAllowNoPassengersLockon(int VehicleIndex, bool bAllowLockon)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_ALLOW_NO_PASSENGERS_LOCKON - Vehicle has to be a mission vehicle"))
			{
				pVehicle->m_nVehicleFlags.bAllowNoPassengersLockOn = bAllowLockon;
			}
		}
	}

	int CommandGetVehicleHomingLockOnState(int VehicleIndex)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			return pVehicle->GetHomingLockOnState();
		}

		return CEntity::HLOnS_NONE;
	}

	int CommandGetVehicleHomingLockedOntoState(int VehicleIndex)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			return pVehicle->GetHomingLockedOntoState();
		}

		return CEntity::HLOnS_NONE;
	}

	void CommandSetVehicleHomingLockedOntoState(int VehicleIndex, int LockOnState)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			pVehicle->UpdateHomingLockedOntoState((CEntity::eHomingLockOnState)LockOnState);
		
			NetworkInterface::SetTargetLockOn(pVehicle, LockOnState);
		}
	}

	void CommandRemoveVehicleFromParkedVehiclesBudget(int VehicleIndex, bool bTakeCarOutOfParkedCarsBudget)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_IN_CUTSCENE - Vehicle has to be a misison vehicle"))
			{
				CVehiclePopulation::UpdateVehCount(pVehicle, CVehiclePopulation::SUB);
				pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation = bTakeCarOutOfParkedCarsBudget;
				CVehiclePopulation::UpdateVehCount(pVehicle, CVehiclePopulation::ADD);
			}
		}
	}

	bool CommandIsVehicleModel(int VehicleIndex, int VehicleModelHashKey)
	{
		const CVehicle *pVehicle;
		bool 	LatestCmpFlagResult = false;

		fwModelId VehicleModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
	
		if (SCRIPT_VERIFY(VehicleModelId.IsValid(), "IS_VEHICLE_MODEL - this is not a valid model index"))
		{
			pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

			if (pVehicle)
			{
				if (pVehicle->GetModelIndex() == VehicleModelId.GetModelIndex())
				{
					LatestCmpFlagResult = true;
				}
			}
		}

		return LatestCmpFlagResult;
	}

	bool CommandDoesScriptVehicleGeneratorExist(int VehicleGeneratorIndex)
	{
		CCarGenerator *pCurrCarGen = CTheCarGenerators::GetCarGeneratorSafe(VehicleGeneratorIndex);
		return (pCurrCarGen != NULL && pCurrCarGen->IsOwnedByScript());
	}

	int CommandCreateScriptVehicleGenerator(const scrVector & scrVecNewCoors, float Heading, float MaxLength, float MaxWidth, int VehicleModelHashKey, int Remap1, int Remap2, int Remap3, int Remap4, bool HighPriorityFlag, int UNUSED_PARAM(ChanceOfVehicleAlarm), int ChanceOfLocked, bool PreventEntryIfNotQualified, bool CanBeStolen, int livery)
	{
		Vector3 vNewCoors = Vector3(scrVecNewCoors);
		float fHeadingInRadians = DtoR * Heading;
		float FowardX = -1.0f*rage::Sinf(fHeadingInRadians)*MaxLength;
		float FowardY = rage::Cosf(fHeadingInRadians)*MaxLength;

		u32 flags = HighPriorityFlag ? CARGEN_FORCESPAWN : 0;
		flags |= PreventEntryIfNotQualified ? CARGEN_PREVENT_ENTRY : 0;
		return CTheCarGenerators::CreateCarGenerator(vNewCoors.x, vNewCoors.y, vNewCoors.z, FowardX, FowardY, MaxWidth,
			VehicleModelHashKey, (s16) Remap1, (s16) Remap2, (s16) Remap3, (s16) Remap4, (s16)/*Remap5*/-1, (s16)/*Remap6*/-1, flags, 0, true, CREATION_RULE_ALL, CARGEN_SCENARIO_NONE, atHashString(), (s8)livery, (s8)/*livery2*/-1, ChanceOfLocked == 0, CanBeStolen);
	}


	void CommandDeleteScriptVehicleGenerator(int VehicleGeneratorIndex)
	{
		CCarGenerator *pCurrCarGen = CTheCarGenerators::GetCarGeneratorSafe(VehicleGeneratorIndex);

		if (SCRIPT_VERIFY(pCurrCarGen, "DELETE_SCRIPT_VEHICLE_GENERATOR - Vehicle generator doesn't exist"))
		{
			if(scriptVerifyf(pCurrCarGen->IsUsed(), "%s: DELETE_SCRIPT_VEHICLE_GENERATOR - Vehicle generator %d already destroyed", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleGeneratorIndex))
			{
				if(scriptVerifyf(pCurrCarGen->IsOwnedByScript(), "DELETE_SCRIPT_VEHICLE_GENERATOR - Trying to destroy car generator %d, which is not script-created.", VehicleGeneratorIndex))
				{
					CTheCarGenerators::DestroyCarGeneratorByIndex(VehicleGeneratorIndex, false);
				}
			}
		}
	}


	void CommandSetScriptVehicleGenerator(int VehicleGeneratorIndex, int NumberOfVehiclesToGenerate)
	{
		CCarGenerator *CurrCarGen = CTheCarGenerators::GetCarGeneratorSafe(VehicleGeneratorIndex);

		if (SCRIPT_VERIFY(CurrCarGen, "SET_SCRIPT_VEHICLE_GENERATOR - Vehicle generator doesn't exist"))
		{
			if (NumberOfVehiclesToGenerate == 0)
			{
				CurrCarGen->SwitchOff();
			}
			else if (NumberOfVehiclesToGenerate > 100)
			{
				CurrCarGen->SwitchOn();
			}
			else
			{
				CurrCarGen->GenerateMultipleVehs((u16) NumberOfVehiclesToGenerate);
			}
		}
	}

	void CommandSetAllVehicleGeneratorsActiveInArea(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool bActive, bool bSyncOverNetwork)
	{
		Vector3 vNewMinCoors;
		Vector3 vNewMaxCoors;
        vNewMinCoors.x = rage::Min(scrVecMinCoors.x, scrVecMaxCoors.x);
        vNewMinCoors.y = rage::Min(scrVecMinCoors.y, scrVecMaxCoors.y);
        vNewMinCoors.z = rage::Min(scrVecMinCoors.z, scrVecMaxCoors.z);
        vNewMaxCoors.x = rage::Max(scrVecMinCoors.x, scrVecMaxCoors.x);
        vNewMaxCoors.y = rage::Max(scrVecMinCoors.y, scrVecMaxCoors.y);
        vNewMaxCoors.z = rage::Max(scrVecMinCoors.z, scrVecMaxCoors.z);

		CTheCarGenerators::SetCarGeneratorsActiveInArea(vNewMinCoors, vNewMaxCoors, bActive);

        if(NetworkInterface::IsGameInProgress() && bSyncOverNetwork)
        {
            if(bActive)
            {
#if !__FINAL
                scriptDebugf1("%s: Switching car generators on over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vNewMinCoors.x, vNewMinCoors.y, vNewMinCoors.z, vNewMaxCoors.x, vNewMaxCoors.y, vNewMaxCoors.z);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
                NetworkInterface::SwitchCarGeneratorsOnOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vNewMinCoors, vNewMaxCoors);
            }
            else
            {
#if !__FINAL
                scriptDebugf1("%s: Switching car generators off over network: (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vNewMinCoors.x, vNewMinCoors.y, vNewMinCoors.z, vNewMaxCoors.x, vNewMaxCoors.y, vNewMaxCoors.z);
                scrThread::PrePrintStackTrace();
#endif // !__FINAL
                NetworkInterface::SwitchCarGeneratorsOffOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vNewMinCoors, vNewMaxCoors);
            }
        }
	}

	void CommandSetAllVehicleGeneratorsActive()
	{
		CTheCarGenerators::SetAllCarGeneratorsBackToActive();
	}

	void CommandSetVehGenSpawnLockedThisFrame()
	{
		CTheCarGenerators::bScriptForcingAllCarsLockedThisFrame = true;
	}

	void CommandSetAllLowPriorityVehicleGeneratorsActive(bool bAllow)
	{
		bAllow = !bAllow;

		CTheCarGenerators::bScriptDisabledCarGenerators = bAllow;
	}

	void CommandSetVehGenSpawnLockedPercentage(int percentage)
	{
		CTheCarGenerators::m_CarGenScriptLockedPercentage = float(percentage) / 100.0f;
	}

	void CommandNetworkSetAllLowPriorityVehicleGeneratorsWithHeliActive(bool bAllow)
	{
		bAllow = !bAllow;
		
		CTheCarGenerators::bScriptDisabledCarGeneratorsWithHeli = bAllow;
	}

	void CommandSetVehicleGeneratorAreaOfInterest(const scrVector & position, float radius)
	{
		CTheCarGenerators::m_CarGenAreaOfInterestPosition = Vector3(position);
		CTheCarGenerators::m_CarGenAreaOfInterestRadius = radius;
	}

	void CommandClearVehicleGeneratorAreaOfInterest()
	{
		CTheCarGenerators::m_CarGenAreaOfInterestRadius = -1.f;
	}

	void CommandSetVehicleUseCutsceneWheelCompression(int VehicleIndex, bool bUseCutsceneWheelCompression, bool bAnimateWheels, bool bAnimateJoints)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			bool bInACutscene = false;

			if (pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetTaskManager())
			{
				CTaskCutScene* pCutsceneVehicleTask = static_cast<CTaskCutScene*>(pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_CUTSCENE));
				if (pCutsceneVehicleTask)
				{
					bInACutscene = true;
					if (pCutsceneVehicleTask->GetExitNextUpdate())
					{
						bInACutscene = false;
					}
				}
			}

			pVehicle->m_nVehicleFlags.bUseCutsceneWheelCompression = bUseCutsceneWheelCompression;

			if(bInACutscene)
			{
				pVehicle->m_nVehicleFlags.bAnimateWheels = bAnimateWheels;
				pVehicle->m_nVehicleFlags.bAnimateJoints = bAnimateJoints;
			}
		}
	}

	bool CommandSetVehicleOnGroundProperly(int VehicleIndex, float hightSampleRangeUp )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	
		if (pVehicle)
		{
			pVehicle->SetOverridenPlaceOnRoadHeight(hightSampleRangeUp);

			bool ret = pVehicle->PlaceOnRoadAdjust( false, hightSampleRangeUp );

			if( pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR )
			{
				CAutomobile* pAutomobile = static_cast< CAutomobile* >( pVehicle );
				pAutomobile->ResetHydraulics();
			}
			if (!ret)
			{
				pVehicle->SetIsFixedWaitingForCollision(true);
			}

			return ret;
		}
		return false;
	}

	void CommandSetVehicleHasMutedSirens(int VehicleIndex, bool bMute)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			pVehicle->SetScriptMutedSirens( bMute );
		}
	}

	bool CommandIsVehicleStuckOnRoof(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool LatestCmpFlagResult = false;

		if (pVehicle)
		{
			if (CScriptCars::GetUpsideDownCars().HasCarBeenUpsideDownForAWhile(VehicleIndex))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}


	void CommandAddVehicleUpsidedownCheck(int VehicleIndex)
	{
		CVehicle *	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			CScriptCars::GetUpsideDownCars().AddCarToCheck(VehicleIndex);
		}
	}

	void CommandRemoveVehicleUpsidedownCheck(int VehicleIndex)
	{
		CScriptCars::GetUpsideDownCars().RemoveCarFromCheck(VehicleIndex);
	}

	bool CommandIsVehicleStopped(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		bool LatestCmpFlagResult = false;

		if (pVehicle)
		{
			if (CScriptCars::IsVehicleStopped(pVehicle))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}

	int CommandGetFVehicleNumberOfPassengers(int VehicleIndex, bool bIncludeDriversSeat = false, bool bIncludeDeadPeds = true)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			return pVehicle->GetSeatManager()->CountPedsInSeats(bIncludeDriversSeat, bIncludeDeadPeds);
		}
		return 0;
	}


	int CommandGetMaxNumberOfPassengers(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex,  CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pVehicle)
		{
			return pVehicle->GetSeatManager()->GetMaxSeats() - 1;
		}
		return 0;
	}

	int CommandGetVehicleModelNumberOfSeats(s32 VehicleModelHashKey)
	{
		CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_NUMBER_OF_SEATS - failed to find model info") &&
			scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_NUMBER_OF_SEATS - model is not a vehicle"))
		{
			CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
			
			// If model seat data is in memory, return the validated number of seats
			if (pVehicleModelInfo->m_data)
			{
				if (const CModelSeatInfo* pModelSeatInfo = pVehicleModelInfo->GetModelSeatInfo())
				{
					return pModelSeatInfo->GetNumSeats();
				}
			}
			
			// Otherwise, fall back to number of seats in layout (or an override if the layout/seats don't match)
			if (pVehicleModelInfo->GetNumSeatsOverride() != -1)
			{
				return pVehicleModelInfo->GetNumSeatsOverride();
			}
			else if (const CVehicleLayoutInfo* pVehicleLayout = pVehicleModelInfo->GetVehicleLayoutInfo())
			{
				return pVehicleLayout->GetNumSeats();
			}
		}

		return 0;
	}

	bool CommandIsTurretSeat(int VehicleIndex, int DestNumber)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex,  CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pVehicle)
		{
			s32 iSeatIndex = DestNumber+1;
			if (scriptVerifyf(pVehicle->IsSeatIndexValid(iSeatIndex), "Seat %s isn't a valid enum for this vehicle", CTaskVehicleFSM::GetScriptVehicleSeatEnumString(DestNumber)))
			{
				return pVehicle->IsTurretSeat(iSeatIndex);
			}
		}
		return false;
	}

	bool CommandDoesVehicleAllowRappel(int VehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pVehicle && pVehicle->GetVehicleModelInfo())
		{
			return pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ALLOWS_RAPPEL);
		}
		return false;
	}

	bool CommandDoesVehicleHaveRearSeatActivities(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex,  CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pVehicle && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES))
		{
			return true;
		}
		return false;
	}

	bool CommandIsSeatWarpOnly(int VehicleIndex, int DestNumber)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex,  CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		
		if (pVehicle)
		{
			const CVehicleLayoutInfo* pLayout = pVehicle->GetLayoutInfo();
			if (pLayout)
			{
				// Layout set to warp in and out
				if (pLayout->GetWarpInAndOut())
				{
					return true;
				}

				s32 iSeatIndex = DestNumber+1;
				if (scriptVerifyf(pVehicle->IsSeatIndexValid(iSeatIndex), "Seat %i isn't a valid enum for this vehicle", DestNumber))
				{
					// Should have a valid entry point to potentially animate into the vehicle from
					CSeatAccessor::SeatAccessType sa = pVehicle->GetSeatAccessType(iSeatIndex);
					if (sa != CSeatAccessor::eSeatAccessTypeInvalid)
					{
						// Warp in to all other multiple seat access seats
						if (sa == CSeatAccessor::eSeatAccessTypeMultiple)
						{
							// Multiple access seats are warp only (unless its a boat, since we have an animated solution for that)
							if (pVehicle->InheritsFromBoat())
							{
								return false;
							}
							return true;
						}
					}
					else
					{
						// Invalid access, need to warp
						return true;
					}
				}
			}
		}
		return false;
	}

	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
	void CommandSetVehicleDensityMultiplier(float DensityMultiplier)
	{
		Displayf("SET_VEHICLE_DENSITY_MULTIPLIER set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetRandomVehDensityMultiplier( DensityMultiplier );
        CVehiclePopulation::SetParkedCarDensityMultiplier( DensityMultiplier );
	}
	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
    void CommandSetRandomVehicleDensityMultiplier(float DensityMultiplier)
	{
		Displayf("SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetRandomVehDensityMultiplier( DensityMultiplier );
	}
	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
    void CommandSetParkedVehicleDensityMultiplier(float DensityMultiplier)
	{
		Displayf("SET_PARKED_VEHICLE_DENSITY_MULTIPLIER set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
        CVehiclePopulation::SetParkedCarDensityMultiplier( DensityMultiplier );
	}
	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
	void CommandSetAmbientVehicleRangeMultiplier(float RangeMultiplier)
	{
		Displayf("SET_AMBIENT_VEHICLE_RANGE_MULTIPLIER set to %f by %s\n", RangeMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptedRangeMultiplier( RangeMultiplier );
	}

	void CommandSetVehicleDensityMultiplierThisFrame(float DensityMultiplier)
	{
//		Displayf("SET_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptRandomVehDensityMultiplierThisFrame( DensityMultiplier );
		CVehiclePopulation::SetScriptParkedCarDensityMultiplierThisFrame( DensityMultiplier );
	}
	void CommandSetRandomVehicleDensityMultiplierThisFrame(float DensityMultiplier)
	{
//		Displayf("SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptRandomVehDensityMultiplierThisFrame( DensityMultiplier );
	}
	void CommandSetParkedVehicleDensityMultiplierThisFrame(float DensityMultiplier)
	{
//		Displayf("SET_PARKED_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME set to %f by %s\n", DensityMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptParkedCarDensityMultiplierThisFrame( DensityMultiplier );
	}
	void CommandSetDisableRandomTrainsThisFrame(bool disable)
	{
		//		Displayf("SET_DISABLE_RANDOM_TRAINS_THIS_FRAME set to %d by %s\n", disable, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptDisableRandomTrainsThisFrame( disable );
	}
	void CommandSetAmbientVehicleRangeMultiplierThisFrame(float RangeMultiplier)
	{
//		Displayf("SET_AMBIENT_VEHICLE_RANGE_MULTIPLIER_THIS_FRAME set to %f by %s\n", RangeMultiplier, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CVehiclePopulation::SetScriptedRangeMultiplierThisFrame( RangeMultiplier );
	}
	void CommandDesertedStreetMultiplierLocked(bool locked)
	{
		CVehiclePopulation::SetDesertedStreetMultiplierLocked(locked);
	}

	void CommandSetFarDrawVehicles(bool bVal)
	{
		CVehicleFactory::GetFactory()->SetFarDrawAllVehicles(bVal);
	}

	void CommandSetNumberOfParkedVehicles(int NewNumberOfParkedCars)
	{
		CVehiclePopulation::ms_overrideNumberOfParkedCars = NewNumberOfParkedCars;
	}

	void CommandSetVehicleDoorsLocked(int VehicleIndex, int NewLockState)
	{
		scriptAssertf(NewLockState != CARLOCK_NONE, "VEHICLELOCK_NONE doesn't do anything, please set this to VEHICLELOCK_UNLOCKED");
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			pVehicle->SetCarDoorLocks((eCarLockState) NewLockState);
		}
	}

	void CommandSetVehicleIndividualDoorsLocked(int VehicleIndex, int DoorID, int NewLockState)
	{
		scriptAssertf(NewLockState != CARLOCK_NONE, "VEHICLELOCK_NONE doesn't do anything, please set this to VEHICLELOCK_UNLOCKED");
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle)
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorID));
			pVehicle->SetCarDoorLocks(CARLOCK_PARTIALLY);
			pVehicle->SetDoorLockStateFromDoorID(nDoorId, (eCarLockState) NewLockState);
		}
	}

	void CommandSetVehicleDoorsLockedForPlayer(int VehicleIndex, int PlayerIndex, bool bLock)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pVehicle)
		{
            if(NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
            {
                CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());

                if(netObjVehicle)
                {
					vehicleDisplayf("SET_VEHICLE_DOORS_LOCKED_FOR_PLAYER called on %s VehicleIndex %d PlayerIndex %d bLock %s", netObjVehicle->GetLogName(), VehicleIndex, PlayerIndex, bLock ? "TRUE" : "FALSE");

                    PlayerFlags newPlayerLocks = netObjVehicle->HasPendingPlayerLocks() ? netObjVehicle->GetPendingPlayerLocks() : netObjVehicle->GetPlayerLocks();

                    if (gnetVerify(PlayerIndex != INVALID_PLAYER_INDEX))
	                {
		                if (bLock)
		                {
			                newPlayerLocks |= (1<<PlayerIndex);
		                }
		                else
		                {
			                newPlayerLocks &= ~(1<<PlayerIndex);
		                }
	                }
#if __BANK
					if ((newPlayerLocks != netObjVehicle->GetPendingPlayerLocks()) || (newPlayerLocks != netObjVehicle->GetPlayerLocks()))
					{
						vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_PLAYER [%s][%p]: Current player locks - %i, Set player locks to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetPlayerLocks(), newPlayerLocks);
						scrThread::PrePrintStackTrace();
					}
#endif
                    netObjVehicle->SetPendingPlayerLocks(newPlayerLocks);
                }
            }
            else
            {
			    if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_LOCKED_FOR_PLAYER - This script command can only be called on a networked vehicle"))
			    {
#if __BANK
					vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_PLAYER [%s][%p]: Locking for player - %i, Lock set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, PlayerIndex, bLock);
					scrThread::PrePrintStackTrace();
#endif
				    static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->LockForPlayer(static_cast<PhysicalPlayerIndex>(PlayerIndex), bLock);
			    }
            }
		}
	}

	bool CommandGetVehicleDoorsLockedForPlayer(int VehicleIndex, int PlayerIndex)
	{
		bool bLocked = false;

		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "GET_VEHICLE_DOORS_LOCKED_FOR_PLAYER - This script command can only be called on a networked vehicle"))
			{
				bLocked = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->IsLockedForPlayer(static_cast<PhysicalPlayerIndex>(PlayerIndex));
			}
		}

		return bLocked;
	}

	void CommandSetVehicleDoorsLockedForAllPlayers(int VehicleIndex, bool bLock)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pVehicle)
		{
			CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());

			if (netObjVehicle)
			{
				if (NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
				{

					PlayerFlags newPlayerLocks = bLock ? ~0 : 0;
#if __BANK
					if ((newPlayerLocks != netObjVehicle->GetPendingPlayerLocks()) || (newPlayerLocks != netObjVehicle->GetPlayerLocks()))
					{
						vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_ALL_PLAYERS [%s][%p]: Current player locks - %i, Set player locks to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetPlayerLocks(), newPlayerLocks);
						scrThread::PrePrintStackTrace();
					}
#endif
					netObjVehicle->SetPendingPlayerLocks(newPlayerLocks);
				}
				else
				{
					if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_LOCKED_FOR_ALL_PLAYERS - This script command can only be called on a networked vehicle"))
					{
#if __BANK
						if ((netObjVehicle->GetPlayerLocks() == 0 && bLock) || (netObjVehicle->GetPlayerLocks() != 0 && !bLock))
						{
							vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_ALL_PLAYERS [%s][%p]: Vehicle lock for all players used to be - %i  lock is now set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetPlayerLocks(), bLock);
							scrThread::PrePrintStackTrace();
						}
#endif
						netObjVehicle->LockForAllPlayers(bLock);
					}
				}
			}
		}
	}

	void CommandSetVehicleDoorsLockedForNonScriptPlayers(int VehicleIndex, bool bLock)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_LOCKED_FOR_NON_SCRIPT_PLAYERS - This script command can only be called on a networked vehicle"))
			{

				CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
				if (netObjVehicle)
				{
#if __BANK
					if (netObjVehicle->IsLockedForNonScriptPlayers() != bLock)
					{
						vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_NON_SCRIPT_PLAYERS [%s][%p]: Lock for non script players used to be - %i  lock is now set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->IsLockedForNonScriptPlayers(), bLock);
						scrThread::PrePrintStackTrace();
					}
#endif
					netObjVehicle->LockForNonScriptPlayers(bLock);
				}
			}
		}
	}

    void CommandSetVehicleDoorsLockedForTeam(int VehicleIndex, int TeamIndex, bool bLock)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
            if (SCRIPT_VERIFY(TeamIndex >= 0 && TeamIndex < MAX_NUM_TEAMS, "SET_VEHICLE_DOORS_LOCKED_FOR_TEAM - Invalid team specified!"))
            {
			    if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_LOCKED_FOR_TEAM - This script command can only be called on a networked vehicle"))
			    {
					CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
					if (netObjVehicle)
					{
#if __BANK
						if ((netObjVehicle->IsLockedForTeam(u8(TeamIndex)) == 0 && bLock) || (netObjVehicle->IsLockedForTeam(u8(TeamIndex)) != 0 && !bLock))
						{
							vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_TEAM [%s][%p]: Lock for team used to be - %i  lock is now set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetTeamLocks(), bLock);
							scrThread::PrePrintStackTrace();
						}
#endif
						netObjVehicle->LockForTeam(static_cast<u8>(TeamIndex), bLock);
					}
			    }
            }
		}
	}

	bool CommandGetVehicleDoorsLockedForTeam(int VehicleIndex, int TeamIndex)
	{
		bool bLocked = false;

		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
            if (SCRIPT_VERIFY(TeamIndex >= 0 && TeamIndex < MAX_NUM_TEAMS, "GET_VEHICLE_DOORS_LOCKED_FOR_TEAM - Invalid team specified!"))
            {
			    if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "GET_VEHICLE_DOORS_LOCKED_FOR_TEAM - This script command can only be called on a networked vehicle"))
			    {
				    bLocked = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->IsLockedForTeam(static_cast<u8>(TeamIndex));
			    }
            }
		}

		return bLocked;
	}

	void CommandSetVehicleDoorsLockedForAllTeams(int VehicleIndex, bool bLock)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_LOCKED_FOR_ALL_TEAMS - This script command can only be called on a networked vehicle"))
			{
				CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
				if (netObjVehicle)
				{
#if __BANK
					if ((netObjVehicle->GetTeamLocks() == 0 && bLock) || (netObjVehicle->GetTeamLocks() != 0 && !bLock))
					{
						vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_ALL_TEAMS [%s][%p]: Lock for all teams used to be - %i  lock is now set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetTeamLocks(), bLock);
						scrThread::PrePrintStackTrace();
					}
#endif
					netObjVehicle->LockForAllTeams(bLock);
				}
			}
		}
	}

    void CommandSetVehicleDoorsTeamLockOverrideForPlayer(int VehicleIndex, int PlayerIndex, bool bOverride)
    {
        CNetGamePlayer *player = CTheScripts::FindNetworkPlayer(PlayerIndex);
		
		if (player)
		{
            CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		    if (pVehicle)
		    {
			    if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_DOORS_TEAM_LOCK_OVERRIDE_FOR_PLAYER - This script command can only be called on a networked vehicle"))
			    {
					CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());
					if (netObjVehicle)
					{
#if __BANK
						vehicleDebugf1("SET_VEHICLE_DOORS_LOCKED_FOR_ALL_TEAMS [%s][%p]: Teamlock overrides used to be - %i  Teamlock overrides is now set to - %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, netObjVehicle->GetTeamLockOverrides(), bOverride);
						scrThread::PrePrintStackTrace();
#endif
						netObjVehicle->SetTeamLockOverrideForPlayer(static_cast<PhysicalPlayerIndex>(PlayerIndex), bOverride);
					}
                }
            }
        }
    }

	void CommandSetDontTerminateOnMissionAchieved(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetTaskManager())
			{
				CTaskVehicleMissionBase* pBaseMission = pVehicle->GetIntelligence()->GetActiveTask();
				if (pBaseMission)
				{
					pBaseMission->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
				}
			}
		}
	}

	void CommandSetEnterLockedForSpecialEditionVehicles(bool bCanEnter)
	{
		CVehicle::SetCanEnterLockedForSpecialEditionVehicles(bCanEnter);
	}

	void CommandSetVehicleModelDoorsLockedForPlayer(int const PlayerIndex, int ModelHashKey, bool Lock)
	{
		if(SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_VEHICLE_MODEL_DOORS_LOCKED_FOR_PLAYER - This script command can only be called for network games"))
		{
			Assertf(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() == PlayerIndex, "ERROR : Trying to set locking information for a remote player?!");
			
			fwModelId ModelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) ModelHashKey, &ModelId);
			Assert(ModelId.IsValid());

			//---

			// are we trying to lock / unlock an instance and we've already done it?
			if(CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelRegistered(PlayerIndex, ModelId.GetModelIndex()))
			{
				if(CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelLocked(PlayerIndex, ModelId.GetModelIndex()) == Lock)
				{
					return;
				}
			}

			//---

			int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByModelId(PlayerIndex, ModelId.GetModelIndex());
			if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
			{
				// we need to send a network event to modify an entry rather than add/remove it....
				CModifyVehicleLockWorldStateDataEvent::Trigger(PlayerIndex, -1, ModelId.GetModelIndex(), Lock);

				// modify it locally...
				CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(PlayerIndex, ModelId.GetModelIndex(), Lock);
			}
			else
			{
				int index = CNetworkVehicleModelDoorLockedTableManager::SetModelIdPlayerLock(PlayerIndex, ModelId.GetModelIndex(), Lock);

				// Add over network....is a network game in progress and are we the script host?
				if(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork())
				{
#if !__FINAL
                    CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(PlayerIndex));
                    scriptDebugf1("%s: Locking vehicle model for player:%s Model:%s, Locked:%s",
                        CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                        player ? player->GetLogName() : "Unknown",
                        CModelInfo::GetBaseModelInfoName(ModelId),
                        Lock ? "true" : "false");
                    scrThread::PrePrintStackTrace();
#endif // !__FINAL
					NetworkInterface::SetModelHashPlayerLock_OverNetwork
					(
						CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
						PlayerIndex,
						ModelId.GetModelIndex(),
						Lock,
						index
					);
				}
			}
		}
	}

	void CommandSetVehicleModelInstanceDoorsLockedForPlayer(int const PlayerIndex, int VehicleIndex, bool Lock)
	{
		if(SCRIPT_VERIFY(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "SET_VEHICLE_MODEL_INSTANCE_DOORS_LOCKED_FOR_PLAYER - This script command can only be called for network games"))
		{
			Assertf(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() == PlayerIndex, "ERROR : Trying to set locking information for a remote player?!");

			const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
			Assert(pVehicle);

			if( pVehicle )
			{
				if (SCRIPT_VERIFY(pVehicle->GetNetworkObject(), "SET_VEHICLE_MODEL_INSTANCE_DOORS_LOCKED_FOR_PLAYER - This script command can only be called on a networked vehicle"))
				{
					//---

					// are we trying to lock / unlock an instance and we've already done it?
					if(CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceRegistered(PlayerIndex, VehicleIndex))
					{
						if(CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceLocked(PlayerIndex, VehicleIndex) == Lock)
						{
							return;
						}
					}

					//---

					int existing_index = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLockIndexByVehicleInstance(PlayerIndex, VehicleIndex);
					if(existing_index != CNetworkVehicleModelDoorLockedTableManager::INVALID_PLAYER_INFO_INDEX)
					{
						// we need to send a network event to modify an entry rather than add/remove it....
						CModifyVehicleLockWorldStateDataEvent::Trigger(PlayerIndex, VehicleIndex, fwModelId::MI_INVALID, Lock);

						// modify it locally...
						CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(PlayerIndex, VehicleIndex, Lock);
					}
					else
					{
						int index = CNetworkVehicleModelDoorLockedTableManager::SetModelInstancePlayerLock(PlayerIndex, VehicleIndex, Lock);

#if !__FINAL
                        CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(PlayerIndex));
                        scriptDebugf1("%s: Locking vehicle instance for player:%s vehicle:%s, Locked:%s",
                            CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                            player ? player->GetLogName() : "Unknown",
                            pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "Unknown",
                            Lock ? "true" : "false");
                        scrThread::PrePrintStackTrace();
#endif // !__FINAL
						NetworkInterface::SetModelInstancePlayerLock_OverNetwork
						(
							CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
							PlayerIndex,
							VehicleIndex,
							Lock,
							index
						);
					}
				}
			}
		}
	}

	void CommandClearVehicleModelDoorsLockedForPlayer(int const PlayerIndex)
	{
		if(SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "CLEAR_VEHICLE_MODEL_DOORS_LOCKED_FOR_PLAYER - This script command can only be called for network games"))
		{
			Assertf(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() == PlayerIndex, "ERROR : Trying to set locking information for a remote player?!");

			if(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork())
			{
				u32 numHashLocks	= CNetworkVehicleModelDoorLockedTableManager::GetNumModelHashPlayerLocks();
				u32 numTotalLocks	= CNetworkVehicleModelDoorLockedTableManager::GetNumPlayerLocks();

				u32 count = 0;

				for(u32 lock = 0; lock < numTotalLocks; ++lock)
				{
					CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo const& lockInfo = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLock(lock);

					if((int)lockInfo.GetPlayerIndex() == PlayerIndex)
					{
						if(lockInfo.GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelId)
						{
#if !__FINAL
                            CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(PlayerIndex));
                            scriptDebugf1("%s: Clearing vehicle model lock for player:%s Model:%s",
                                CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                                player ? player->GetLogName() : "Unknown",
                                CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(lockInfo.GetModelHash()))));
                            scrThread::PrePrintStackTrace();
#endif // !__FINAL
							NetworkInterface::ClearModelHashPlayerLockForPlayer_OverNetwork
							(
								CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
								PlayerIndex,
								lockInfo.GetModelHash(),
								lockInfo.GetLocked(),
								lock
							);

							++count;
						}
					}
				}

				Assert(count == numHashLocks);
				UNUSED_VAR(count);
				UNUSED_VAR(numHashLocks);
			}

			CNetworkVehicleModelDoorLockedTableManager::ClearAllModelIdPlayerLocksForPlayer(PlayerIndex);
		}
	}

	void CommandClearVehicleModelInstanceDoorsLockedForPlayer(int const PlayerIndex)
	{
		if(SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "CLEAR_VEHICLE_MODEL_INSTANCE_DOORS_LOCKED_FOR_PLAYER - This script command can only be called for network games"))
		{
			Assertf(NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() == PlayerIndex, "ERROR : Trying to set locking information for a remote player?!");

			if(NetworkInterface::IsGameInProgress() && CTheScripts::GetCurrentGtaScriptHandlerNetwork())
			{
				u32 numInstanceLocks	= CNetworkVehicleModelDoorLockedTableManager::GetNumVehicleInstancePlayerLocks();
				u32 numTotalLocks		= CNetworkVehicleModelDoorLockedTableManager::GetNumPlayerLocks();

				u32 count = 0;

				for(u32 lockIdx = 0; lockIdx < numTotalLocks; ++lockIdx)
				{
					CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo const& lockInfo = CNetworkVehicleModelDoorLockedTableManager::GetPlayerLock(lockIdx);

					if((int)lockInfo.GetPlayerIndex() == PlayerIndex)
					{
						if(lockInfo.GetLockType() == CNetworkVehicleModelDoorLockedTableManager::PlayerLockInfo::LockType_ModelInstance)
						{
#if !__FINAL
                            const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(lockInfo.GetVehicleIndex());
                            CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(PlayerIndex));
                            scriptDebugf1("%s: Clearing vehicle lock for player:%s vehicle:%s",
                                CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                                player ? player->GetLogName() : "Unknown",
                                (pVehicle && pVehicle->GetNetworkObject()) ? pVehicle->GetNetworkObject()->GetLogName() : "Unknown");
                            scrThread::PrePrintStackTrace();
#endif // !__FINAL
							NetworkInterface::ClearModelInstancePlayerLockForPlayer_OverNetwork
							(
								CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
								PlayerIndex,
								lockInfo.GetVehicleIndex(),
								lockInfo.GetLocked(),
								lockIdx
							);

							++count;
						}
					}
				}

				Assert(count == numInstanceLocks);
				UNUSED_VAR(count);
				UNUSED_VAR(numInstanceLocks);

				CNetworkVehicleModelDoorLockedTableManager::ClearAllModelInstancePlayerLocksForPlayer(PlayerIndex);
			}				
		}
	}

	void CommandSetVehicleDoorOpenForPathfinder(int iVehicle, int iDoor, bool bOpen)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicle);
		if (pVehicle)
		{
			if (iDoor == SC_DOOR_INVALID)
			{
				for (int i = 0; i < pVehicle->GetNumDoors(); ++i)
				{
					pVehicle->GetDoor(i)->SetForceOpenForNavigationInfinite(bOpen);
				}
			}
			else
			{
				eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(iDoor));
				CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
				if(SCRIPT_VERIFY((pDoor), "SET_VEHICLE_DOOR_OPEN_OPEN_FPR_PATHFINDER - Door not found"))
				{
					pDoor->SetForceOpenForNavigationInfinite(bOpen);
				}
			}

			if (pVehicle->WasUnableToAddToPathServer())
			{
				CPathServerGta::MaybeAddDynamicObject(pVehicle);
			}

			if (pVehicle->IsInPathServer())
			{
				CPathServerGta::UpdateDynamicObject(pVehicle, true, true);
				pVehicle->UpdateDoorsForNavigation();
			}
		}
	}

	void CommandExplodeVehicle(int VehicleIndex, bool bAddExplosion, bool bKeepDamageEntity)
	{
		if(!SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "EXPLODE_VEHICLE - This script command is not allowed in mulriplayer player game scripts, use NETWORK_EXPLODE_VEHICLE!"))
			return;

		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			CEntity *pInflictor = NULL;

			if(bKeepDamageEntity)
			{
				pInflictor = pVehicle->GetWeaponDamageEntity();
			}

			pVehicle->BlowUpCar( pInflictor, false, bAddExplosion, false );
		}
	}

	void CommandSetVehicleFullThrottleTimer(int VehicleIndex, int durationMs) {
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		
		if (pVehicle) {
			pVehicle->SetFullThrottleTime(durationMs);
		}
	}

	void CommandSetVehicleOutOfControl(int VehicleIndex, bool bKillPedsInVehicle, bool bExplodeOnNextImpact)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			CPlayerInfo* playerInfo = CGameWorld::GetMainPlayerInfo();
			if(playerInfo && bKillPedsInVehicle)
			{
				CPed* playerPed = playerInfo->GetPlayerPed();
				pVehicle->KillPedsInVehicle(playerPed, WEAPONTYPE_BLEEDING);
			}
			
			pVehicle->SetIsOutOfControl();
			pVehicle->m_nVehicleFlags.bExplodesOnNextImpact = bExplodeOnNextImpact;
		}
	}

	void CommandSetVehicleTimedExplosion(int nVehicleIndex, int nCulpritIndex, int nTimeFromNow)
	{
		if(!SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_VEHICLE_TIMED_EXPLOSION - This script command is not allowed in single player game scripts!"))
			return; 

		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
		if(pVehicle)
		{
			CEntity* pCulprit = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nCulpritIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			ObjectId nCulpritID = NetworkUtils::GetObjectIDFromGameObject(pCulprit);
			unsigned nExplosionTime = NetworkInterface::GetNetworkTime() + nTimeFromNow;
			static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->SetTimedExplosion(nCulpritID, nExplosionTime);
		}
	}

	void CommandAddVehiclePhoneExplosiveDevice(int nVehicleIndex)
	{
		if(!SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "ADD_VEHICLE_PHONE_EXPLOSIVE_DEVICE - This script command is not allowed in single player game scripts!"))
			return; 

		CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
		if(pMyPlayer)
		{
			CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
			if(pVehicle)
			{
				ObjectId nVehicleID = NetworkUtils::GetObjectIDFromGameObject(pVehicle);
				pMyPlayer->SetPhoneExplosiveVehicleID(nVehicleID);
			}
		}
	}

	void CommandClearVehiclePhoneExplosiveDevice()
	{
		if(!SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "CLEAR_VEHICLE_PHONE_EXPLOSIVE_DEVICE - This script command is not allowed in single player game scripts!"))
			return; 

		CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
		if(pMyPlayer)
			pMyPlayer->ClearPhoneExplosiveVehicleID();
	}

	bool CommandHasVehiclePhoneExplosiveDevice()
	{
		if(!SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "HAS_VEHICLE_PHONE_EXPLOSIVE_DEVICE - This script command is not allowed in single player game scripts!"))
			return false; 

		CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
		if(pMyPlayer)
		{
			if(pMyPlayer->GetPhoneExplosiveVehicleID() != NETWORK_INVALID_OBJECT_ID)
			{
				netObject* pObject = NetworkInterface::GetNetworkObject(pMyPlayer->GetPhoneExplosiveVehicleID());
				if(pObject && pObject->GetEntity() && pObject->GetEntity()->GetIsTypeVehicle())
					return true;
			}
		}

		// no local player
		return false;
	}

	void CommandDetonateVehiclePhoneExplosiveDevice()
	{
		if(!SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "DETONATE_VEHICLE_PHONE_EXPLOSIVE_DEVICE - This script command is not allowed in single player game scripts!"))
			return; 

		CNetGamePlayer* pMyPlayer = NetworkInterface::GetLocalPlayer();
		if(pMyPlayer)
		{
			if(gnetVerifyf(pMyPlayer->GetPhoneExplosiveVehicleID() != NETWORK_INVALID_OBJECT_ID, "DETONATE_VEHICLE_PHONE_EXPLOSIVE_DEVICE - Player has not set a phone explosive!"))
			{
				// apply locally if we still know about the vehicle
				netObject* pObject = NetworkInterface::GetNetworkObject(pMyPlayer->GetPhoneExplosiveVehicleID());
				if(pObject && 
				   gnetVerifyf(pObject->GetEntity(), "DETONATE_VEHICLE_PHONE_EXPLOSIVE_DEVICE - %s has NULL entity", pObject->GetLogName()) && 
				   gnetVerifyf(pObject->GetEntity()->GetIsTypeVehicle(), "DETONATE_VEHICLE_PHONE_EXPLOSIVE_DEVICE - %s is not a vehicle", pObject->GetLogName()))
				{
					CNetObjVehicle* pNetVehicle = SafeCast(CNetObjVehicle, pObject);
					if(pNetVehicle)
					{
						CPed* pCulprit = pMyPlayer->GetPlayerPed();
						pNetVehicle->DetonatePhoneExplosive(pCulprit ? pCulprit->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID);
					}
				}

				// and tell everyone else
				CRequestPhoneExplosionEvent::Trigger(pMyPlayer->GetPhoneExplosiveVehicleID());
				pMyPlayer->ClearPhoneExplosiveVehicleID();
			}
		}
	}

	bool CommandHaveVehicleRearDoorsBeenBlownOpenByStickybomb(int nVehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle)
		{
			 return pVehicle->m_nVehicleFlags.bRearDoorsHaveBeenBlownOffByStickybomb;
		}
		return false;
	}

	void CommandSetTaxiLights(int VehicleIndex, bool TaxiLightsFlag)
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_TAXI_LIGHTS - This script command is not allowed in network game scripts!"))
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				if (pVehicle->InheritsFromAutomobile())
				{
					if (TaxiLightsFlag)
					{
						((CAutomobile*) pVehicle)->SetTaxiLight(TRUE);
					}
					else
					{
						((CAutomobile*) pVehicle)->SetTaxiLight(FALSE);
					}
				}
				else
				{
					scriptAssertf(0, "%s:SET_TAXI_LIGHTS - vehicle must be an automobile", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
		}
	}

	bool CommandIsTaxiLightOn(int VehicleIndex)
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "IS_TAXI_LIGHT_ON - This script command is not allowed in network game scripts!"))
		{
			const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				if (SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "IS_TAXI_LIGHT_ON - Vehicle must be a car"))
				{
					if (((CAutomobile*) pVehicle)->m_nAutomobileFlags.bTaxiLight)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool CommandIsVehicleInGarageArea(const char *pGarageName, int VehicleIndex)
	{
		s32 GarageIndex = CGarages::FindGarageIndex((char *) pGarageName);;
		
		if (SCRIPT_VERIFY(GarageIndex>= 0, "IS_VEHICLE_IN_GARAGE_AREA - Garage doesn't exist"))
		{
			const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				return CGarages::IsThisCarWithinGarageArea(GarageIndex, pVehicle);
			}
		}
		return false;
	}

	void CommandSetVehicleColour(int VehicleIndex, int Colour1, int Colour2)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (SCRIPT_VERIFY((Colour1 >= 0 && Colour2 >= 0), "SET_VEHICLE_COLOURS - Colours must be >= 0"))
			{
				const s32 MaxColours = CVehicleModelInfo::GetVehicleColours()->GetColorCount();
				if (SCRIPT_VERIFY(Colour1 < MaxColours, "SET_VEHICLE_COLOURS - First colour is too large"))
				{
					if (SCRIPT_VERIFY(Colour2 < MaxColours, "SET_VEHICLE_COLOURS - Second colour is too large"))
					{
						pVehicle->SetBodyColour1((u8) Colour1);
						pVehicle->SetBodyColour2((u8) Colour2);
						pVehicle->UpdateBodyColourRemapping();		// let shaders know, that body colours changed
					}
				}
			}
		}
	}

	void CommandSetVehiclePrimaryColour(int VehicleIndex, int Red, int Green, int Blue)
	{
		//SET_VEHICLE_CUSTOM_PRIMARY_COLOUR
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);

			const Color32 col(Red,Green,Blue);
			pShader->SetCustomPrimaryColor(col);
		}
	}

	void CommandGetVehiclePrimaryColour(int VehicleIndex, int& Red, int& Green, int& Blue)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);

			Color32 col = pShader->GetCustomPrimaryColor();
			Red = col.GetRed();
			Green = col.GetGreen();
			Blue = col.GetBlue();
		}
	}

	void CommandClearVehiclePrimaryColour(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			pShader->ClearCustomPrimaryColor();
		}
	}

	bool CommandGetIsVehiclePrimaryColourCustom(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);

			return pShader->GetIsPrimaryColorCustom();
		}

		return false;
	}
	
	void CommandSetVehicleSecondaryColour(int VehicleIndex, int Red, int Green, int Blue)
	{
		//SET_VEHICLE_CUSTOM_SECONDARY_COLOUR
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			
			const Color32 col(Red,Green,Blue);
			pShader->SetCustomSecondaryColor(col);
		}
	}

	void CommandGetVehicleSecondaryColour(int VehicleIndex, int& Red, int& Green, int& Blue)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);

			Color32 col = pShader->GetCustomSecondaryColor();
			Red = col.GetRed();
			Green = col.GetGreen();
			Blue = col.GetBlue();
		}
	}

	void CommandClearVehicleSecondaryColour(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			pShader->ClearCustomSecondaryColor();
		}
	}

	bool CommandGetIsVehicleSecondaryColourCustom(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			Assert(pVehicle->GetDrawHandlerPtr() != NULL);
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);

			return pShader->GetIsSecondaryColorCustom();
		}

		return false;
	}

	void CommandSetVehicleEnvEffScale(s32 VehicleIndex, float envEffScale)
	{
		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			if(pShader)
			{
				pShader->SetEnvEffScale(envEffScale);
			}
		}
	}

	float CommandGetVehicleEnvEffScale(s32 VehicleIndex)
	{
		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			if(pShader)
			{
				return pShader->GetEnvEffScale();
			}
		}

		return(0.0f);
	}

	void CommandSetVehicleFullBeam(int VehicleIndex, bool bOn)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (bOn)
				pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn = TRUE;
			else
				pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn = FALSE;
		}
	}

	void CommandSetVehicleIsRacing(int VehicleIndex, bool bRacing)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (bRacing)
			{
				pVehicle->m_nVehicleFlags.bIsRacing = true;
			}
			else
			{
				pVehicle->m_nVehicleFlags.bIsRacing = false;
			}
		}
	}

	void CommandSetCanResprayVehicle(int VehicleIndex, bool CanChangeColourFlag)
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_CAN_RESPRAY_CAR - This script command is not allowed in network game scripts!"))
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				if (CanChangeColourFlag)
					pVehicle->m_nVehicleFlags.bShouldNotChangeColour = FALSE;
				else
					pVehicle->m_nVehicleFlags.bShouldNotChangeColour = TRUE;
			}
		}
	}

	void CommandSetGoonBossVehicle(int VehicleIndex, bool EnableSpecialEnterFunc)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_GOON_BOSS_VEHICLE - This script command is allowed in network game scripts only!"))
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				if (EnableSpecialEnterFunc)
					pVehicle->m_bSpecialEnterExit = TRUE;
				else
					pVehicle->m_bSpecialEnterExit = FALSE;
			}
		}
	}

	void CommandSetOpenRearDoorsOnExplosion(int VehicleIndex, bool OpenRearDoorsOnExplosion)
	{
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_OPEN_REAR_DOORS_ON_EXPLOSION - This script command is allowed in network game scripts only!"))
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{
				if (!OpenRearDoorsOnExplosion)
					pVehicle->m_bDontOpenRearDoorsOnExplosion = TRUE;
				else
					pVehicle->m_bDontOpenRearDoorsOnExplosion = FALSE;
			}
		}
	}

	void CommandSetGarageType(const char *pGarageName, int NewGarageType)
	{
		s32 GarageIndex = CGarages::FindGarageIndex((char *) pGarageName);

		if (GarageIndex >= 0)
		{
			CGarages::ChangeGarageType(GarageIndex, (u8) NewGarageType);
		}
	}

	void CommandSetGarageLeaveCameraAlone(const char *pGarageName, bool NewFlag)
	{
		s32 GarageIndex = CGarages::FindGarageIndex((char *) pGarageName);

		if (GarageIndex >= 0)
		{
			CGarages::aGarages[GarageIndex].Flags.bLeaveCameraAlone = NewFlag;
		}
	}

	void CommandForceSubmarineSurfaceMode(int VehicleIndex, bool ForceSurfaceMode)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if(SCRIPT_VERIFY( pSubHandling, "FORCE_SUBMARINE_SURFACE_MODE - Can only call this command for a submarine"))
			{
				pSubHandling->SetForceSurfaceMode(ForceSurfaceMode);
			}
		}
	}

	void CommandForceSubmarineNeutralBuoyancy(int VehicleIndex, int iTimeMS)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if (SCRIPT_VERIFY(pSubHandling, "FORCE_SUBMARINE_NEURTAL_BUOYANCY - Can only call this command for a submarine"))
			{
				pSubHandling->ForceNeutralBuoyancy(iTimeMS);
			}
		}
	}

	void CommandSetSubmarineCrushDepths(int VehicleIndex, bool EnableCrushDamage, float VisibleDamageDepth, float AirLeakDepth, float CrushDepth)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if(SCRIPT_VERIFY( pSubHandling, "SET_SUBMARINE_CRUSH_DEPTHS - Can only call this command for a submarine"))
			{
				pSubHandling->SetCrushDepths(EnableCrushDamage, VisibleDamageDepth, AirLeakDepth, CrushDepth);
			}
		}
	}

	bool CommandGetSubmarineIsUnderCrushDepth(int VehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if(SCRIPT_VERIFY( pSubHandling, "GET_SUBMARINE_IS_UNDER_CRUSH_DEPTH - Can only call this command for a submarine"))
			{
				return pSubHandling->IsUnderCrushDepth(pVehicle);
			}
		}
		return false;
	}

	bool CommandGetSubmarineIsUnderDesignDepth(int VehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if(SCRIPT_VERIFY( pSubHandling, "GET_SUBMARINE_IS_UNDER_DESIGN_DEPTH - Can only call this command for a submarine"))
			{
				return pSubHandling->IsUnderDesignDepth(pVehicle);
			}
		}
		return false;
	}

	int CommandGetSubmarineNumberOfAirLeaks(int VehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

			if(SCRIPT_VERIFY( pSubHandling, "GET_SUBMARINE_NUMBER_OF_AIR_LEAKS - Can only call this command for a submarine"))
			{
				return pSubHandling->GetNumberOfAirLeaks();
			}
		}
		return 0;
	}

	void CommandSetBoatIgnoreLandProbes(int VehicleIndex, bool bIgnoreProbes)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			pVehicle->GetIntelligence()->m_bSetBoatIgnoreLandProbes = bIgnoreProbes;
		}
	}

	void CommandSetVehicleSteerForBuildings(int VehicleIndex, bool bSteerForBuildings)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			pVehicle->GetIntelligence()->m_bSteerForBuildings = bSteerForBuildings;
		}
	}

	void CommandSetVehicleCausesSwerving(int VehicleIndex, bool bCauseSwerving)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			pVehicle->GetIntelligence()->m_bDontSwerveForUs = !bCauseSwerving;
		}
	}

	void CommandSetIgnorePlanesSmallPitchChange(int VehicleIndex, bool bIgnorePitching)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			pVehicle->GetIntelligence()->m_bIgnorePlanesSmallPitchChange = bIgnorePitching;
		}
	}

	void CommandSetFleeingVehiclesUseSwitchedOffNodes(bool bCanUseSwitchedOff)
	{
		CVehicleIntelligence::ms_bReallyPreventSwitchedOffFleeing = !bCanUseSwitchedOff;
	}

	void CommandSetBoatAnchor(int VehicleIndex, bool AnchoredFlag)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if(SCRIPT_VERIFY(CAnchorHelper::IsVehicleAnchorable(pVehicle), "SET_BOAT_ANCHOR - Can only call this command for a boat, submarine, amphibious vehicles or seaplane"))
			{
#if !__FINAL
				if(NetworkInterface::IsGameInProgress())
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SCRIPT_SET_BOAT_ANCHOR", "%s", 
						pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "Non-networked vehicle");
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Anchor", AnchoredFlag ? "On" : "Off");
				}
#endif
				if(pVehicle->InheritsFromBoat())
				{
					static_cast<CBoat*>(pVehicle)->GetAnchorHelper().Anchor(AnchoredFlag);
				}
				else if(pVehicle->GetSubHandling())
				{
					CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();
					pSubHandling->GetAnchorHelper().Anchor(AnchoredFlag);
				}
				else if(pVehicle->InheritsFromPlane())
				{
					if(CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CPlane*>(pVehicle)->GetExtension<CSeaPlaneExtension>())
					{
						pSeaPlaneExtension->GetAnchorHelper().Anchor(AnchoredFlag);
					}
				}
				else if( pVehicle->InheritsFromHeli() )
				{
					if( CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CHeli*>( pVehicle )->GetExtension<CSeaPlaneExtension>() )
					{
						pSeaPlaneExtension->GetAnchorHelper().Anchor( AnchoredFlag );
					}
				}
				else if(pVehicle->InheritsFromAmphibiousAutomobile())
				{
					CAmphibiousAutomobile* pAmphibiousVeh = static_cast<CAmphibiousAutomobile*>(pVehicle);

					pAmphibiousVeh->GetAnchorHelper().Anchor(AnchoredFlag);
				}
			}
		}
	}

	bool CommandCanAnchorBoatHere(int VehicleIndex)
	{
		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(CAnchorHelper::IsVehicleAnchorable(pVehicle), "CAN_ANCHOR_BOAT_HERE - Can only call this command for a boat, submarine, amphibious vehicles or seaplane"))
			{
				const CAnchorHelper* pAnchorHelper = CAnchorHelper::GetAnchorHelperPtr(pVehicle);
				if(AssertVerify(pAnchorHelper))
				{
					return pAnchorHelper->CanAnchorHere();
				}
			}
		}

		return false;
	}

	bool CommandCanAnchorBoatHereIgnorePlayersStandingOnVehicle(int VehicleIndex)
	{
		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(CAnchorHelper::IsVehicleAnchorable(pVehicle), "CAN_ANCHOR_BOAT_HERE - Can only call this command for a boat, submarine, amphibious vehicles or seaplane"))
			{
				const CAnchorHelper* pAnchorHelper = CAnchorHelper::GetAnchorHelperPtr(pVehicle);
				if(AssertVerify(pAnchorHelper))
				{
					return pAnchorHelper->CanAnchorHere( true );
				}
			}
		}

		return false;
	}

	void CommandSetBoatRemainsAnchoredWhilePlayerIsDriver(int VehicleIndex, bool ForcePlayerBoatAnchorFlag)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(CAnchorHelper::IsVehicleAnchorable(pVehicle), "SET_BOAT_REMAINS_ANCHORED_WHILE_PLAYER_IS_DRIVER - Can only call this command for a boat, submarine, amphibious vehicles or seaplane"))
			{
				if(pVehicle->InheritsFromBoat())
				{
					static_cast<CBoat*>(pVehicle)->GetAnchorHelper().ForcePlayerBoatToRemainAnchored(ForcePlayerBoatAnchorFlag);
				}
				else if(pVehicle->GetSubHandling())
				{
					CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();
					if( pSubHandling )
					{
						pSubHandling->GetAnchorHelper().ForcePlayerBoatToRemainAnchored(ForcePlayerBoatAnchorFlag);
					}
				}
				else if(pVehicle->InheritsFromPlane())
				{
					if(CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CPlane*>(pVehicle)->GetExtension<CSeaPlaneExtension>())
					{
						pSeaPlaneExtension->GetAnchorHelper().ForcePlayerBoatToRemainAnchored(ForcePlayerBoatAnchorFlag);
					}
				}
				else if( pVehicle->InheritsFromHeli() )
				{
					if( CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CHeli*>( pVehicle )->GetExtension<CSeaPlaneExtension>() )
					{
						pSeaPlaneExtension->GetAnchorHelper().ForcePlayerBoatToRemainAnchored( ForcePlayerBoatAnchorFlag );
					}
				}
				else if(pVehicle->InheritsFromAmphibiousAutomobile())
				{
					CAmphibiousAutomobile* pAmphibiousVeh = static_cast<CAmphibiousAutomobile*>(pVehicle);

					pAmphibiousVeh->GetAnchorHelper().ForcePlayerBoatToRemainAnchored(ForcePlayerBoatAnchorFlag);
				}
			}
		}
	}

	void CommandForceLowLodAnchorMode(int VehicleIndex, bool ForceLowLodAnchorModeAnchorFlag)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromBoat(), "SET_FORCE_LOW_LOD_ANCHOR_MODE - Can only call this command for a boat."))
			{
				if(pVehicle->InheritsFromBoat())
				{
					static_cast<CBoat*>(pVehicle)->GetAnchorHelper().ForceLowLodModeAlways(ForceLowLodAnchorModeAnchorFlag);
				}
			}
		}
	}

	void CommandSetBoatLowLodAnchorDistance(int VehicleIndex, float fLodDistance)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromBoat(), "SET_BOAT_LOW_LOD_ANCHOR_DISTANCE - Can only call this command for a boat"))
			{
				static_cast<CBoat*>(pVehicle)->m_BoatHandling.SetLowLodBuoyancyDistance(fLodDistance);
			}
		}
	}

	bool CommandIsBoatAnchored(int VehicleIndex)
	{
		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(CAnchorHelper::IsVehicleAnchorable(pVehicle), "IS_BOAT_ANCHORED - Can only call this command for a boat, submarine, amphibious vehicles or seaplane"))
			{
				const CAnchorHelper* pAnchorHelper = CAnchorHelper::GetAnchorHelperPtr(pVehicle);
				if(AssertVerify(pAnchorHelper))
				{
					return pAnchorHelper->IsAnchored();
				}
			}
		}

		return false;
	}

	void CommandSetBoatSinksWhenWrecked(int VehicleIndex, bool bShouldSink)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromBoat(), "SET_BOAT_SINKS_WHEN_WRECKED - Can only call this command for a boat."))
			{
				if(bShouldSink)
				{
					static_cast<CBoat*>(pVehicle)->m_BoatHandling.SetWreckedAction( BWA_SINK );
				}
				else
				{
					static_cast<CBoat*>(pVehicle)->m_BoatHandling.SetWreckedAction( BWA_FLOAT );
				}
			}
		}
	}

	void CommandSetBoatWrecked(int VehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromBoat(), "SET_BOAT_WRECKED - Can only call this command for a boat."))
			{
				pVehicle->SetIsWrecked();
			}
		}
	}

	void CommandOpenGarage(const char *pGarageName)
	{
		s32 GarageIndex;

		GarageIndex = CGarages::FindGarageIndex((char*) pGarageName);
		if (GarageIndex >= 0)
		{
			CGarages::aGarages[GarageIndex].OpenThisGarage();
		}
	}

	void CommandCloseGarage(const char *pGarageName)
	{
		s32 GarageIndex;

		GarageIndex = CGarages::FindGarageIndex((char*) pGarageName);
		if (GarageIndex >= 0)
		{
			CGarages::aGarages[GarageIndex].CloseThisGarage();
		}
	}

	void CommandSetVehicleSiren(int VehicleIndex, bool SirenFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
			{
				if (SirenFlag)
				{
					((CAutomobile *)pVehicle)->TurnSirenOn(true,false);
				}
				else
				{
					((CAutomobile *)pVehicle)->TurnSirenOn(false,false);
				}
			}
		}

	s32 CommandGetVehicleSirenHealth(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if( pVehicle->m_nVehicleFlags.bHasSiren )
			{
				return pVehicle->GetVehicleDamage()->GetSirenHealth();
			}
		}
		return -1;
	}
	
	bool CommandIsVehicleSirenOn(int VehicleIndex)
	{
		// this script command is not approved for use in network scripts
		bool bSirenIsOn = false;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (((CAutomobile *)pVehicle)->m_nVehicleFlags.GetIsSirenOn())
			{
				bSirenIsOn = true;
			}
		}
		return bSirenIsOn;
	}

	bool CommandIsVehicleSirenAudioOn(int VehicleIndex)
	{
		// this script command is not approved for use in network scripts
		bool bSirenIsOn = false;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (((CAutomobile *)pVehicle)->m_nVehicleFlags.GetIsSirenAudioOn())
			{
				bSirenIsOn = true;
			}
		}
		return bSirenIsOn;
	}
	float CommandGetVehiclePretendOccupantsConversionRange()
	{
		return CVehicleAILodManager::ms_makePedsFromPretendOccupantsRangeOnScreen;
	}

	void CommandSetVehicleTurnsToFaceCoord(int VehicleIndex, float PointAtX, float PointAtY)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		Vector3 TempCoors, Diff;
		float NewHeading;

		if (pVehicle)
		{
				TempCoors = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				Diff = TempCoors - (Vector3(PointAtX, PointAtY, 0.0f));
				NewHeading = fwAngle::GetATanOfXY(Diff.x, Diff.y);
				NewHeading += ( DtoR * 90.0f);
				if (NewHeading > (( DtoR * 360.0f)))
				{
					NewHeading -= ( DtoR * 360.0f);
				}
		}
	}

	void CommandSetVehicleStrong(int VehicleIndex, bool StrongFlag)
	{
		// this script command is not approved for use in network scripts
		CVehicle *	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
			{
				if (StrongFlag)
				{
					pVehicle->m_nVehicleFlags.bTakeLessDamage = TRUE;
				}
				else
				{
					pVehicle->m_nVehicleFlags.bTakeLessDamage = FALSE;
				}
			}
		}

	bool CommandIsGarageOpen(const char *pGarageName)
	{
		s32 GarageIndex;

		GarageIndex = CGarages::FindGarageIndex((char*) pGarageName);
		if (GarageIndex >= 0)
		{
			return (CGarages::IsGarageOpen(GarageIndex));
		}
		return false;
	}

	bool CommandIsGarageClosed(const char *pGarageName)
	{
		s32 GarageIndex;

		GarageIndex = CGarages::FindGarageIndex((char*) pGarageName);
		if (GarageIndex >= 0)
		{
			return (CGarages::IsGarageClosed(GarageIndex));
		}
		return false;
	}

	void CommandRemoveVehicleStuckCheck(int VehicleIndex)
	{
		CScriptCars::GetStuckCars().RemoveCarFromCheck(VehicleIndex);
	}

	void CommandGetVehicleColours(int VehicleIndex, int &ReturnColour1, int &ReturnColour2)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			ReturnColour1 = pVehicle->GetBodyColour1();
			ReturnColour2 = pVehicle->GetBodyColour2();
		}
	}

	void CommandSetAllVehiclesCanBeDamaged(bool AllVehiclesCanBeDamagedFlag)
	{
		if (AllVehiclesCanBeDamagedFlag)
		{
			CGameWorld::SetAllCarsCanBeDamaged(TRUE);
		}
		else
		{
			CGameWorld::SetAllCarsCanBeDamaged(FALSE);
			CGameWorld::ExtinguishAllCarFiresInArea(CGameWorld::FindLocalPlayerCoors(), 4000.0f);
		}
	}

	bool CommandIsVehiclePassengerSeatFree(int VehicleIndex, int PassengerSeatNumber)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		bool LatestCmpFlagResult = false;
		s32 iSeat = PassengerSeatNumber + 1;

		if (pVehicle)
		{
			if (iSeat < pVehicle->GetSeatManager()->GetMaxSeats())
			{
				if (pVehicle->GetSeatManager()->GetPedInSeat(PassengerSeatNumber) == (CPed*)NULL)
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsVehicleSeatFree(int VehicleIndex, int DestNumber, bool ConsiderPedUsingSeat)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool LatestCmpFlagResult = false;
		s32 seat = DestNumber+1;
		
		if (pVehicle)
		{
			if( seat == -1 )
			{
				// BAD_SEAT_USE
				for (s32 i = 1; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++ )
				{
					CPed* pPedInOrUsingSeat = ConsiderPedUsingSeat ? CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, i) : pVehicle->GetSeatManager()->GetPedInSeat(i);
					if (pPedInOrUsingSeat == NULL)
					{
						LatestCmpFlagResult = true;
					}
				}
			}
			else if(pVehicle->IsSeatIndexValid(seat))
			{
				CPed* pPedInOrUsingSeat = ConsiderPedUsingSeat ? CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat) : pVehicle->GetSeatManager()->GetPedInSeat(seat);
				if (pPedInOrUsingSeat == NULL)
				{
					LatestCmpFlagResult = true;
				}
			}
		}
	
		return LatestCmpFlagResult;
	}

	int CommandGetPedInVehicleSeat(int VehicleIndex, int DestNumber, bool ConsiderPedUsingSeat)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		CPed *pPed = NULL;
		s32 seat = DestNumber+1;
		int ReturnPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
		
		if(pVehicle)
		{
			if (scriptVerifyf(seat < MAX_VEHICLE_SEATS, "GET_PED_IN_VEHICLE_SEAT - seat is %u, should be less than %u", DestNumber, MAX_VEHICLE_SEATS))
			{
				if (seat >= 0)
				{
					pPed = ConsiderPedUsingSeat ? CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat) : pVehicle->GetSeatManager()->GetPedInSeat(seat);
					if (pPed == NULL)
					{
						// Check if the vehicle is using pretend occupants.
						if (!pVehicle->IsNetworkClone() && !(pVehicle->GetNetworkObject() && pVehicle->GetNetworkObject()->IsPendingOwnerChange()))
						{
							if(pVehicle->IsUsingPretendOccupants())
							{
								// Try to make the occupants.
								if(pVehicle->TryToMakePedsFromPretendOccupants())
								{
									// We created the occupants, now check if the
									// vehicle was given a driver.
									CPed* pPedInOrUsingSeat = ConsiderPedUsingSeat ? CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat) : pVehicle->GetSeatManager()->GetPedInSeat(seat);
									if (pPedInOrUsingSeat)
									{
										pPed = pPedInOrUsingSeat;
									}
								}
							}
						}
					}	
				}	
				else
				{
					scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "GET_PED_IN_VEHICLE_SEAT - VS_ANY_PASSENGER is not valid, must use a seat specific enum  ");
				}	
			}


			if (pPed)
			{
				ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
	#if __DEV
				pPed->m_nDEflags.bCheckedForDead = TRUE;
	#endif
				
			}
		}

		return ReturnPedIndex;
	}

	int CommandGetLastPedInVehicleSeat(int VehicleIndex, int DestNumber)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		CPed *pPed = NULL;
		s32 seat = DestNumber+1;
		int ReturnPedIndex = NULL_IN_SCRIPTING_LANGUAGE;

		if(pVehicle)
		{
			if (seat >= 0)
			{
				pPed = pVehicle->GetSeatManager()->GetLastPedInSeat(seat);
			}	
			else
			{
				scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "GET_LAST_PED_IN_VEHICLE_SEAT - VS_ANY_PASSENGER is not valid, must use a seat specific enum  ");
			}	

			if (pPed)
			{
				ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
#if __DEV
				pPed->m_nDEflags.bCheckedForDead = TRUE;
#endif
			}
		}

		return ReturnPedIndex;
	}

	void CommandGetPedInVehiclePassengerSeat(int VehicleIndex, int PassengerSeatNumber, int &ReturnPedIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		s32 iSeat = PassengerSeatNumber + 1;

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(iSeat < pVehicle->GetSeatManager()->GetMaxSeats(), "GET_PED_IN_VEHICLE_PASSENGER_SEAT - Vehicle doesn't have enough seats")&&
				SCRIPT_VERIFY(pVehicle->GetSeatManager()->GetPedInSeat(iSeat), "GET_PED_IN_VEHICLE_PASSENGER_SEAT - Specified seat is not occupied"))
			{
				ReturnPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
				CPed *pPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
				if (pPed)
				{
					ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
#if __DEV
					pPed->m_nDEflags.bCheckedForDead = TRUE;
#endif
				}
			}
		}
	}

	bool CommandGetVehicleLightsState(int iVehicleIndex, int & bLightsOn, int & bFullBeam)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);
		if(pVehicle)
		{
			bLightsOn = pVehicle->m_nVehicleFlags.bLightsOn;
			bFullBeam = pVehicle->m_nVehicleFlags.bHeadlightsFullBeamOn;

			return true;
		}
		bLightsOn = 0;
		bFullBeam = 0;
		return false;
	}

	int CommandGetDriverOfVehicle(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		int ReturnPedIndex = 0;

		if(pVehicle)
		{
			if (pVehicle->GetDriver())
			{
				ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pVehicle->GetDriver());
			}
			else
			{
				// Check if the vehicle is using pretend occupants, if so
				// then make real occupants and then hand back the driver.

				// Init the return ped index to a bad value, just
				// to make the code below cleaner.
				ReturnPedIndex = NULL_IN_SCRIPTING_LANGUAGE;

				// Check if the vehicle is using pretend occupants.
				if (!pVehicle->IsNetworkClone() && !(pVehicle->GetNetworkObject() && pVehicle->GetNetworkObject()->IsPendingOwnerChange()))
				{
				if(pVehicle->IsUsingPretendOccupants())
				{
					// Try to make the occupants.
					if(pVehicle->TryToMakePedsFromPretendOccupants())
					{
						// We created the occupants, now check if the
						// vehicle was given a driver.
						if (pVehicle->GetDriver())
						{
								ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pVehicle->GetDriver());
							}
						}
					}
				}
			}
		}
	
		return ReturnPedIndex;
	}

	bool CommandIsVehicleTyreBurst(int VehicleIndex, int WheelNumber, bool bIsBurstToRim)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		bool LatestCmpFlagResult = false;

		if(pVehicle)
		{
			if(SCRIPT_VERIFY((pVehicle->InheritsFromBike()) || (pVehicle->InheritsFromAutomobile()), "IS_VEHICLE_TYRE_BURST - Vehicle is not a vehicle or bike"))
			{
				eHierarchyId nWheelId = CWheel::GetScriptWheelId(eScriptWheelList(WheelNumber));

				if(pVehicle->GetWheelFromId(nWheelId))
				{
					if(bIsBurstToRim && pVehicle->GetWheelFromId(nWheelId)->GetTyreHealth() == 0.0f)
					{
						LatestCmpFlagResult = true;
					}
					else if(!bIsBurstToRim && pVehicle->GetWheelFromId(nWheelId)->GetTyreHealth() < TYRE_HEALTH_DEFAULT)
					{
						LatestCmpFlagResult = true;
					}
				}
			}
		}
	
		return LatestCmpFlagResult;
	}

	void CommandSetVehicleForwardSpeed(int VehicleIndex, float VehicleSpeed)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
#if __DEV
			if(pVehicle && pVehicle->GetCurrentPhysicsInst() && pVehicle->GetCurrentPhysicsInst()->GetArchetype())
			{
				scriptAssertf((rage::Abs(VehicleSpeed) < pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed()),
					"SET_VEHICLE_FORWARD_SPEED - Cannot set the forward speed greater than archetype max speed %f m/s have you adjusted max speed with SET_ENTITY_MAX_SPEED?",  pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed());
			}
#endif

			scriptAssertf((rage::Abs(VehicleSpeed) < DEFAULT_MAX_SPEED), "SET_VEHICLE_FORWARD_SPEED - Cannot set the forward speed greater than %f m/s", DEFAULT_MAX_SPEED);

			if(pVehicle->GetIsFixedUntilCollisionFlagSet())
			{
				pVehicle->SetSpeedToRestoreAfterFix(VehicleSpeed);
				pVehicle->m_nVehicleFlags.bRestoreVelAfterCollLoads = true;
			}
			else
			{
				pVehicle->SetForwardSpeed(VehicleSpeed);
			}
		}
	}

	void CommandSetVehicleForwardSpeedXY(int VehicleIndex, float VehicleSpeed)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
#if __DEV
			if(pVehicle && pVehicle->GetCurrentPhysicsInst() && pVehicle->GetCurrentPhysicsInst()->GetArchetype())
			{
				scriptAssertf((rage::Abs(VehicleSpeed) < pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed()),
					"SET_VEHICLE_FORWARD_SPEED_XY - Cannot set the forward speed greater than archetype max speed %f m/s have you adjusted max speed with SET_ENTITY_MAX_SPEED?",  pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed());
			}
#endif

			scriptAssertf((rage::Abs(VehicleSpeed) < DEFAULT_MAX_SPEED), "SET_VEHICLE_FORWARD_SPEED_XY - Cannot set the forward speed greater than %f m/s", DEFAULT_MAX_SPEED);

			if(pVehicle->GetIsFixedUntilCollisionFlagSet())
			{
				pVehicle->SetSpeedToRestoreAfterFix(VehicleSpeed);
				pVehicle->m_nVehicleFlags.bRestoreVelAfterCollLoads = true;
			}
			else
			{
				pVehicle->SetForwardSpeedXY(VehicleSpeed);
			}
		}
	}

	void CommandSetVehicleDisableHeightmapAvoidance(int VehicleIndex, bool bDisableHeightmapAvoidance )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if ( SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_DISABLE_HEIGHT_MAP_AVOIDANCE called with invalid vehicle") )
			{
				pVehicle->m_nVehicleFlags.bDisableHeightMapAvoidance = bDisableHeightmapAvoidance;
			}
		}
	}

	void CommandSetBoatDisableAvoidance(int VehicleIndex, bool bDisableBoatAvoidance )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if ( SCRIPT_VERIFY(pVehicle, "SET_BOAT_DISABLE_AVOIDANCE called with invalid vehicle") )
			{
				if ( SCRIPT_VERIFY(pVehicle->InheritsFromBoat(), "SET_BOAT_DISABLE_AVOIDANCE called with a non boat") )
				{
					if ( pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetBoatAvoidanceHelper() )
					{
						pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetEnabled(!bDisableBoatAvoidance);
					}
				}				
			}
		}
	}

	bool CommandIsHeliLandingAreaBlocked(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if ( SCRIPT_VERIFY(pVehicle, "IS_HELI_LANDING_AREA_BLOCKED called with invalid vehicle") )
			{
				if ( SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "IS_HELI_LANDING_AREA_BLOCKED called on a non helicopter") )
				{
					CHeli* pHeli = static_cast<CHeli*>(pVehicle);
					return pHeli->GetHeliIntelligence()->IsLandingAreaBlocked();
				}
			}
		}
		return false;
	}

	void CommandSetShortSlowdownDistanceForLanding(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if ( SCRIPT_VERIFY(pVehicle, "SET_SHORT_SLOWDOWN_FOR_LANDING called with invalid vehicle") )
			{
				if ( SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_SHORT_SLOWDOWN_FOR_LANDING called on a non helicopter") )
				{
					aiTask* pTask = pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_LAND);
					if ( SCRIPT_VERIFY(pTask, "SET_SHORT_SLOWDOWN_FOR_LANDING not running landing task") )
					{
						CTaskVehicleLand* pLandtask = static_cast<CTaskVehicleLand*>(pTask);
						pLandtask->SetSlowDownDistance(10.0f);
					}
				}
			}
		}
	}

	void CommandSetHeliTurbulenceScalar(int VehicleIndex, float in_Scalar )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if ( SCRIPT_VERIFY(pVehicle, "SET_HELI_TURBULENCE_SCALAR called with invalid vehicle") )
			{
				if ( SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_TURBULENCE_SCALAR called on a non helicopter") )
				{
					CHeli* pHeli = static_cast<CHeli*>(pVehicle);
					pHeli->SetTurbulenceScalar(in_Scalar);
				}
			}
		}
	}

	void CommandSetVehicleAsConvoyVehicle(int VehicleIndex, bool ConvoyFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if (ConvoyFlag)
			{
				pVehicle->m_nVehicleFlags.bPartOfConvoy = true;
			}
			else
			{
				pVehicle->m_nVehicleFlags.bPartOfConvoy = false;
			}
		}
	}

	void CommandSetVehicleControlToPlayer(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if (pVehicle->GetDriver() &&
				pVehicle->GetDriver()->IsPlayer())
			{
				pVehicle->SetStatus(STATUS_PLAYER);
			}
		}
	}

	void CommandSetCarBootOpen(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "SET_CAR_BOOT_OPEN - Can only call this command for automobiles"))
			{
		
				CCarDoor* pDoor = pVehicle->GetDoorFromId(VEH_BOOT);
				if(pDoor)
				{
					pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET, pVehicle);
				}
			}
		}
	}

	void CommandSetVehicleTyreBurst(int VehicleIndex, int WheelNumber, bool bInstantBurst, float fDamage)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(((pVehicle->InheritsFromBike()) || (pVehicle->InheritsFromAutomobile())), "BURST_VEHICLE_TYRE - Vehicle is not a car or bike"))
			{
				eHierarchyId nWheelId = CWheel::GetScriptWheelId(eScriptWheelList(WheelNumber));

				if(CWheel *pWheel = pVehicle->GetWheelFromId(nWheelId))
				{
					pWheel->ApplyTyreDamage(NULL, fDamage, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), DAMAGE_TYPE_BULLET, 0);

					if(bInstantBurst)
					{
						pWheel->DoBurstWheelToRim();
					}
				}
			}
		}
	}

	void CommandSetVehicleDoorsShut(int VehicleIndex, bool ShutInstantly)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		
		if(pVehicle)
		{
			for(int i=0; i<pVehicle->GetNumDoors(); i++)
			{
                if(ShutInstantly)
                { 
                    pVehicle->GetDoor(i)->SetShutAndLatched(pVehicle, false);
					pVehicle->GetDoor(i)->AudioUpdate();
                }
                else
                { 
                    pVehicle->GetDoor(i)->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::WILL_LOCK_DRIVEN, pVehicle);
                }
			}
		}
	}

	// Remember to update commands_vehicle.sch if you change these flags
	// Please leave as numbers so its easy to compare with the flags in commands_vehicle.sch
	// i.e dont change 4 to (1<<2)
	enum eScriptCarSearchFlags
	{
		VEHICLE_SEARCH_FLAG_RETURN_LAW_ENFORCER_VEHICLES					= 1,
		VEHICLE_SEARCH_FLAG_RETURN_MISSION_VEHICLES							= 2,
		VEHICLE_SEARCH_FLAG_RETURN_RANDOM_VEHICLES							= 4,	//	always true in the past
		VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_GROUP_MEMBERS		= 8,	//	always false in the past
		VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_A_PLAYER				= 16,	//	always false in the past
		VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_A_DEAD_OR_DYING_PED	= 32,	//	always false in the past
		VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_WITH_PEDS_ENTERING_OR_EXITING	= 64,	//	always false in the past
		VEHICLE_SEARCH_FLAG_DO_NETWORK_CHECKS								= 128,	//	always true in the past - but try with false
		VEHICLE_SEARCH_FLAG_CHECK_VEHICLE_OCCUPANTS_STATES					= 256,	//	always true in the past
		VEHICLE_SEARCH_FLAG_CHECK_INTERESTING_VEHICLES						= 512,	//	
		VEHICLE_SEARCH_FLAG_RETURN_LAW_ENFORCER_VEHICLES_ONLY				= 1024,
		VEHICLE_SEARCH_FLAG_ALLOW_VEHICLE_OCCUPANTS_TO_BE_PERFORMING_A_SCRIPTED_TASK = 2048,
		VEHICLE_SEARCH_FLAG_RETURN_HELICOPTORS_ONLY							= 4096,
		VEHICLE_SEARCH_FLAG_RETURN_BOATS_ONLY								= 8192,
		VEHICLE_SEARCH_FLAG_RETURN_PLANES_ONLY								= 16384,
		VEHICLE_SEARCH_FLAG_ALLOW_LAW_ENFORCER_VEHICLES_WITH_WANTED_LEVEL	= 32768,
		VEHICLE_SEARCH_FLAG_ALLOW_VEHICLE_OCCUPANTS_TO_BE_PERFORMING_A_NON_DEFAULT_TASK = 65536,
		VEHICLE_SEARCH_FLAG_ALLOW_TRAILERS									= 131072,
		VEHICLE_SEARCH_FLAG_ALLOW_BLIMPS									= 262144,
		VEHICLE_SEARCH_FLAG_ALLOW_SUBMARINES								= 524288
	};

	bool CanVehicleBeGrabbedByScript(CVehicle *pVehicle, u32 ModelIndexToMatch, int flags)
	{
		bool bReturnLawEnforcerCars						= (flags & VEHICLE_SEARCH_FLAG_RETURN_LAW_ENFORCER_VEHICLES) ? true : false;
		bool bReturnMissionCars							= (flags & VEHICLE_SEARCH_FLAG_RETURN_MISSION_VEHICLES) ? true : false;
		bool bReturnRandomCars							= (flags & VEHICLE_SEARCH_FLAG_RETURN_RANDOM_VEHICLES) ? true : false;
		bool bReturnCarsContainingGroupMembers			= (flags & VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_GROUP_MEMBERS) ? true : false;
		bool bReturnCarsContainingAPlayer				= (flags & VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_A_PLAYER) ? true : false;
		bool bReturnCarsContainingADeadOrDyingPed		= (flags & VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_CONTAINING_A_DEAD_OR_DYING_PED) ? true : false;
		bool bReturnCarsWithPedsEnteringOrExiting		= (flags & VEHICLE_SEARCH_FLAG_RETURN_VEHICLES_WITH_PEDS_ENTERING_OR_EXITING) ? true : false;
		bool bDoVehicleNetworkChecks					= (flags & VEHICLE_SEARCH_FLAG_DO_NETWORK_CHECKS) ? true : false;
		bool bCheckVehicleOccupantsStates				= (flags & VEHICLE_SEARCH_FLAG_CHECK_VEHICLE_OCCUPANTS_STATES) ? true : false;
		bool bCheckInterestingVehicles					= (flags & VEHICLE_SEARCH_FLAG_CHECK_INTERESTING_VEHICLES) ? true : false;
		bool bReturnLawEnforcerCarsOnly					= (flags & VEHICLE_SEARCH_FLAG_RETURN_LAW_ENFORCER_VEHICLES_ONLY) ? true : false;
		bool bAllowOccupantsWithScriptedTasks			= (flags & VEHICLE_SEARCH_FLAG_ALLOW_VEHICLE_OCCUPANTS_TO_BE_PERFORMING_A_SCRIPTED_TASK) ? true : false;
		bool bAllowOccupantsWithNonDefaultTask			= (flags & VEHICLE_SEARCH_FLAG_ALLOW_VEHICLE_OCCUPANTS_TO_BE_PERFORMING_A_NON_DEFAULT_TASK) ? true : false;
		bool bReturnHelicoptorsOnly						= (flags & VEHICLE_SEARCH_FLAG_RETURN_HELICOPTORS_ONLY) ? true : false;
		bool bReturnBoatsOnly							= (flags & VEHICLE_SEARCH_FLAG_RETURN_BOATS_ONLY) ? true : false;
		bool bReturnPlanesOnly							= (flags & VEHICLE_SEARCH_FLAG_RETURN_PLANES_ONLY) ? true : false;
		bool bAllowLawEnforcementVehsWithWantedLevel	= (flags & VEHICLE_SEARCH_FLAG_ALLOW_LAW_ENFORCER_VEHICLES_WITH_WANTED_LEVEL) ? true : false;
		bool bAllowTrailers								= (flags & VEHICLE_SEARCH_FLAG_ALLOW_TRAILERS) ? true : false;
		bool bAllowBlimps								= (flags & VEHICLE_SEARCH_FLAG_ALLOW_BLIMPS) ? true : false;
		bool bAllowSubmarines							= (flags & VEHICLE_SEARCH_FLAG_ALLOW_SUBMARINES) ? true : false;

		VehicleType eVehicleType = pVehicle->GetVehicleType();
		bool isAllowedVehicleType = eVehicleType == VEHICLE_TYPE_CAR || eVehicleType == VEHICLE_TYPE_SUBMARINECAR || eVehicleType == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE ||
									eVehicleType == VEHICLE_TYPE_BIKE || eVehicleType == VEHICLE_TYPE_QUADBIKE || eVehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE || eVehicleType == VEHICLE_TYPE_BICYCLE; 

		if (bReturnHelicoptorsOnly || bReturnBoatsOnly || bReturnPlanesOnly)
		{
			if (!((bReturnHelicoptorsOnly && eVehicleType == VEHICLE_TYPE_HELI) ||
				(bReturnBoatsOnly		  && eVehicleType == VEHICLE_TYPE_BOAT) ||
				(bReturnPlanesOnly		  && eVehicleType == VEHICLE_TYPE_PLANE)))
				return false;
		}
		else if (!isAllowedVehicleType)
		{
			if( (!bAllowTrailers || eVehicleType != VEHICLE_TYPE_TRAILER) &&
				(!bAllowBlimps || eVehicleType != VEHICLE_TYPE_BLIMP) &&
				(!bAllowSubmarines || eVehicleType != VEHICLE_TYPE_SUBMARINE))
			{
				return false;
			}
		}

		if (pVehicle->IsLawEnforcementVehicle())
		{
			if ( (!bReturnLawEnforcerCars) && (!CModelInfo::IsValidModelInfo(ModelIndexToMatch)) )		// Only do the law enforcer test if the model isn't specified. This allows the designers to grab police cars.
			{
				return false;
			}
		}
		else
		{
			if (bReturnLawEnforcerCarsOnly)
			{
				return false;
			}
		}

		if ((pVehicle->GetModelIndex() != ModelIndexToMatch) && (CModelInfo::IsValidModelInfo(ModelIndexToMatch)))
		{
			return false;
		}

		if (!pVehicle->CanBeDeletedSpecial(!bReturnMissionCars, bDoVehicleNetworkChecks, !bReturnCarsWithPedsEnteringOrExiting, bCheckVehicleOccupantsStates, bCheckInterestingVehicles, bAllowLawEnforcementVehsWithWantedLevel, bReturnCarsContainingAPlayer ))
		{
			return false;
		}

		//	Reset this before checking any of the peds although peds in vehicles are probably never running a scenario
		CTheScripts::SetScenarioPedsCanBeGrabbedByScript(false);

		for (s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
		{
			if (pVehicle->GetSeatManager()->GetPedInSeat(iSeat))
			{
				if (ped_commands::CanPedBeGrabbedByScript(pVehicle->GetSeatManager()->GetPedInSeat(iSeat), bReturnRandomCars, bReturnMissionCars, !bReturnCarsContainingGroupMembers, false, bReturnCarsContainingAPlayer, bReturnCarsContainingADeadOrDyingPed, bAllowOccupantsWithScriptedTasks, bAllowOccupantsWithNonDefaultTask, -1) == false)
				{
					return false;
				}
			}
		}

		return true;
	}

	int CommandGetRandomVehicleOfTypeInArea(float MinX, float MinY, float MaxX, float MaxY, int VehicleModelHashKey)
	{
		s32 CarPoolIndex, RandomCarIndex = NULL_IN_SCRIPTING_LANGUAGE;
		CVehicle *pVehicle;

		fwModelId VehicleModelId;
		//	if (VehicleModelHashKey	==	?	hash key can't be negative? How to specify that the vehicle model doesn't matter?
		if (VehicleModelHashKey	!= DUMMY_ENTRY_IN_MODEL_ENUM_FOR_SCRIPTING_LANGUAGE)
		{
			CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
			if (!SCRIPT_VERIFY(VehicleModelId.IsValid(), "GET_RANDOM_VEHICLE_OF_TYPE_IN_AREA_NO_SAVE - this is not a valid model index"))
			{
				return RandomCarIndex;
			}
		}

		CarPoolIndex = (s32) CVehicle::GetPool()->GetSize();
		
		while ((CarPoolIndex--) && (RandomCarIndex == NULL_IN_SCRIPTING_LANGUAGE))
		{
			pVehicle = CVehicle::GetPool()->GetSlot(CarPoolIndex);
			if (pVehicle)
			{
				//			if (CanVehicleBeGrabbedByScript(pVehicle, VehicleModelIndex, true, false, true, true, true))
				if (CanVehicleBeGrabbedByScript(pVehicle, VehicleModelId.GetModelIndex(), VEHICLE_SEARCH_FLAG_RETURN_RANDOM_VEHICLES | VEHICLE_SEARCH_FLAG_CHECK_VEHICLE_OCCUPANTS_STATES))
				{
					if (pVehicle->IsWithinArea(MinX, MinY, MaxX, MaxY))
					{
						RandomCarIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
					}
				}
			}
		}

		return RandomCarIndex;
	}


	
	void CommandSetDriftTyres(int VehicleIndex, bool DriftTyresFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->m_bDriftTyres = DriftTyresFlag;
		}
	}


	bool CommandGetDriftTyresSet(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			return pVehicle->m_bDriftTyres;
		}
		return false;
	}

	void CommandSetVehicleTyresCanBurst(int VehicleIndex, bool CanBurstTyresFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if (CanBurstTyresFlag)
			{
				if( !( pVehicle->pHandling->mFlags & MF_NO_WHEEL_BURST ) )
				{
					pVehicle->m_nVehicleFlags.bTyresDontBurst = false;
				}
			}
			else
			{
				pVehicle->m_nVehicleFlags.bTyresDontBurst = true;
			}
		}
	}

	bool CommandGetVehicleTyresCanBurst(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			return !pVehicle->m_nVehicleFlags.bTyresDontBurst;
		}
		return false;
	}

	void CommandSetVehicleTyresCanBurstToRim(int VehicleIndex, bool CanBurstTyresToRimFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			pVehicle->m_bTyresDontBurstToRim = !CanBurstTyresToRimFlag;
		}
	}

	bool CommandGetVehicleTyresCanBurstToRim(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			return !pVehicle->m_bTyresDontBurstToRim;
		}
		return false;
	}

	void CommandSetVehicleWheelsCanBreak(int VehicleIndex, bool CanBreakWheelsFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bWheelsDontBreak = !CanBreakWheelsFlag;
		}
	}

	bool CommandGetVehicleWheelsCanBreak(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			return !pVehicle->m_nVehicleFlags.bWheelsDontBreak;
		}
		return false;
	}

	int CommandGetVehicleRecordingId(int FileNumber, const char* pRecordingName)
	{
		return CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, pRecordingName);
	}

	bool CommandStartRecordingVehicle(int DEV_ONLY(VehicleIndex) , int DEV_ONLY(FileNumber), const char* DEV_ONLY(pRecordingName), bool DEV_ONLY(allowOverwrite) )
	{
#if __DEV
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			return CVehicleRecordingMgr::StartRecordingCar(pVehicle, FileNumber, (pRecordingName ? pRecordingName : ""), allowOverwrite);
		}
#endif
		return false;
	}

	bool CommandStartRecordingVehicleTransitionFromPlayback(int DEV_ONLY(VehicleIndex) , int DEV_ONLY(FileNumber), const char* DEV_ONLY(pRecordingName), bool DEV_ONLY(allowOverwrite) )
	{
#if __DEV
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			return CVehicleRecordingMgr::StartRecordingCarTransitionFromPlayback(pVehicle, FileNumber, (pRecordingName ? pRecordingName : ""), allowOverwrite);
		}
#endif
		return false;
	}

	void CommandStopRecordingAllVehicles()
	{
#if !__FINAL
		CVehicleRecordingMgr::StopRecordingCar();
#endif
	}

	void CommandStartPlaybackRecordedVehicle(int VehicleIndex, int FileNumber, const char* pRecordingName, bool DoPlaceOnRoadAdjustment)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	
		if(pVehicle)
		{
			int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, (pRecordingName ? pRecordingName : ""));
			if(scriptVerifyf(index != -1, "START_PLAYBACK_RECORDED_VEHICLE: %s%d vehicle recording doesn't exist", pRecordingName, FileNumber))
			{
				CVehicleRecordingMgr::StartPlaybackRecordedCar(pVehicle, index);

				if(DoPlaceOnRoadAdjustment && !pVehicle->GetIsAttached() && !pVehicle->GetAttachedTrailer())
				{
					pVehicle->PlaceOnRoadAdjust();
				}

				CPed* pDriver = pVehicle->GetDriver();
				if (!pDriver || (pDriver && !pDriver->IsPlayer()))
				{
					pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
					pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();

					if (pDriver)
					{
						pDriver->GetPedIntelligence()->AddTaskDefault(rage_new CTaskInVehicleBasic( pVehicle, false ));
					}
				}
			}
		}
	}

	void CommandStartPlaybackRecordedVehicleWithFlags(int VehicleIndex, int FileNumber, const char* pRecordingName, int flags, s32 delayUntilRevertingBack, int iDrivingFlags)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, (pRecordingName ? pRecordingName : ""));
			if( delayUntilRevertingBack != 0 )
			{
				delayUntilRevertingBack = rage::Max((u32)500, ((u32)delayUntilRevertingBack));
			}
			if(scriptVerifyf(index != -1, "START_PLAYBACK_RECORDED_VEHICLE_WITH_FLAGS: %s%d vehicle recording doesn't exist", pRecordingName, FileNumber))
			{
				CVehicleRecordingMgr::StartPlaybackRecordedCar(pVehicle, index, false, 10, iDrivingFlags, false, -1, 0, 0, flags, delayUntilRevertingBack);

				if(!pVehicle->GetIsAttached() && !pVehicle->GetAttachedTrailer())
				{
					pVehicle->PlaceOnRoadAdjust();
				}

				CPed* pDriver = pVehicle->GetDriver();
				if (!pDriver || (pDriver && !pDriver->IsPlayer()))
				{
					pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
					pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();

					if (pDriver)
					{
						pDriver->GetPedIntelligence()->AddTaskDefault(rage_new CTaskInVehicleBasic( pVehicle, false ));
					}
				}
			}
		}
	}	

	void CommandForcePlaybackRecordedVehicleUpdate(int VehicleIndex, bool bDoPlaceOnRoadAdjustment)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::ForcePlaybackRecordedCarUpdate(pVehicle);

			if(bDoPlaceOnRoadAdjustment && !pVehicle->GetIsAttached() && !pVehicle->GetAttachedTrailer())
			{
				pVehicle->PlaceOnRoadAdjust();
			}

			// It appears that we need to update the vehicle position in the spatial array,
			// otherwise an assert may fail in the entity scanning code.
			if(pVehicle->GetSpatialArrayNode().IsInserted())
			{
				pVehicle->UpdateInSpatialArray();
			}
		}
	}

	void CommandStopPlaybackRecordedVehicle(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pVehicle)
		{
			CVehicleRecordingMgr::StopPlaybackRecordedCar(pVehicle);
		}
	}

	void CommandPausePlaybackRecordedVehicle(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::PausePlaybackRecordedCar(pVehicle);
		}
	}

	void CommandUnpausePlaybackRecordedVehicle(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::UnpausePlaybackRecordedCar(pVehicle);
		}
	}


	bool CommandIsPlaybackGoingOnForVehicle(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pVehicle)
		{
			return (CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVehicle));
		}
		return false;
	}

	bool CommandIsPlaybackUsingAIGoingOnForVehicle(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			s32 recording = CVehicleRecordingMgr::GetPlaybackSlot(pVehicle);
			if (recording != -1)
			{
				return (CVehicleRecordingMgr::GetUseCarAI(recording));
			}
		}
		return false;
	}

	int CommandGetCurrentPlaybackForVehicle(int VehicleIndex)
	{
		const CVehicle	*pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			int playback = CVehicleRecordingMgr::GetPlaybackIdForCar(pVehicle);
			if(scriptVerifyf(playback != -1, "%s:GET_CURRENT_PLAYBACK_FOR_VEHICLE - Vehicle isn't actually playing a recording", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return playback;
			}
		}
		return -1;
	}

	void CommandSetShouldLerpFromAiToFullRecording(int VehicleIndex, bool Interp)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bShouldLerpFromAiToFullRecording = Interp;
		}
	}

#if !__FINAL
	const char* CommandGetVehicleRecordingName(int recordingIndex)
	{
		return CVehicleRecordingMgr::GetRecordingName(recordingIndex);
	}
#endif

	void CommandSetVehicleDoorOpen(int VehicleIndex, int DoorNumber, bool bSwingFree, bool bInstant)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(SCRIPT_VERIFY((pDoor), "SET_VEHICLE_DOOR_OPEN - Door not found"))
			{
				if(bInstant)
				{
#if __BANK					
					if(CCarDoor::sm_debugDoorPhysics)
					{
						Printf("CommandSetVehicleDoorOpen - [%s] Script setting vehicle door open instant.\n", pVehicle->GetDebugName());
					}
#endif
					pDoor->ClearFlag(CCarDoor::DRIVEN_MASK);
					pDoor->SetFlag(CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::SET_TO_FORCE_OPEN);
				}
				else
				{
					// Clear flags before setting them, avoid any flag conflicts
					pDoor->ClearFlag(CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_SWINGING|CCarDoor::WILL_LOCK_DRIVEN|CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
					pDoor->SetTargetDoorOpenRatio(1.0f, bSwingFree ? (CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_SWINGING) : (CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL), pVehicle);
				}

				audVehicleAudioEntity* vehicleAudioEntity = pVehicle->GetVehicleAudioEntity();

				if(vehicleAudioEntity)
				{
					vehicleAudioEntity->OnScriptVehicleDoorOpen();
				}
			}
		}
	}

	void CommandSetVehicleDoorAutoLock(int VehicleIndex, int DoorNumber, bool bAutoLock)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(SCRIPT_VERIFY((pDoor), "SET_VEHICLE_DOOR_AUTO_LOCK - Door not found"))
			{
				if(bAutoLock)
				{
					pDoor->SetFlag(CCarDoor::WILL_LOCK_SWINGING);
				}
				else
				{
					pDoor->ClearFlag(CCarDoor::WILL_LOCK_SWINGING);
				}
			}
		}
	}

	void CommandRemoveVehicleWindow(int VehicleIndex, int WindowNumber)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile()), "REMOVE_VEHICLE_WINDOW - Vehicle is not an automobile"))
			{
				eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));

				pVehicle->RemoveFragmentComponent(nWindowId);
				pVehicle->SetWindowComponentDirty(nWindowId);
			}
		}
	}

	void CommandSmashVehicleWindow(int VehicleIndex, int WindowNumber)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile()), "SMASH_VEHICLE_WINDOW - Vehicle is not an automobile"))
			{
				eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));
				// TODO -- should we not just call pVehicle->SmashWindow(nWindowId) here?

				s32 boneIndex = pVehicle->GetBoneIndex(nWindowId);
				if (boneIndex>-1)
				{
					s32 iWindowComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex);
					if(iWindowComponent != -1)
					{
						const Vec3V vSmashNormal = pVehicle->GetWindowNormalForSmash(nWindowId);
						g_vehicleGlassMan.SmashCollision(pVehicle, iWindowComponent, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW, vSmashNormal);
						pVehicle->SetWindowComponentDirty(nWindowId);
					}
				}
			}
		}
	}

	void CommandFixVehicleWindow(int VehicleIndex, int WindowNumber)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile()), "FIX_VEHICLE_WINDOW - Vehicle is not an automobile"))
			{
				eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));
				pVehicle->FixWindow(nWindowId);
			}
		}
	}

	void CommandPopOutVehicleWindscreen(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY((pVehicle->GetBoneIndex(VEH_WINDSCREEN)!=-1), "%s:POP_OUT_VEHICLE_WINDSCREEN - vehicle must have a windscreen"))
			{
				pVehicle->GetVehicleDamage()->PopOutWindScreen();
			}
		}
	}

    void CommandPopOffVehicleRoof(int VehicleIndex)
    {
        Vector3 vecImpulse(0.0f, 0.0f, 5.0f);

        CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
        if (pVehicle)
        {
            if (SCRIPT_VERIFY((pVehicle->GetBoneIndex(VEH_ROOF)!=-1), "%s:POP_OFF_VEHICLE_ROOF - vehicle must have a roof"))
            {
                pVehicle->GetVehicleDamage()->PopOffRoof(vecImpulse);
            }
        }
    }

    void CommandPopOffVehicleRoofWithImpulse(int VehicleIndex, const scrVector & scrVecImpulse)
    {
        Vector3 vecImpulse = Vector3(scrVecImpulse);

        CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
        if (pVehicle)
        {
            if (SCRIPT_VERIFY((pVehicle->GetBoneIndex(VEH_ROOF)!=-1), "%s:POP_OFF_VEHICLE_ROOF_WITH_IMPULSE - vehicle must have a roof"))
            {
                pVehicle->GetVehicleDamage()->PopOffRoof(vecImpulse);
            }
        }
    }

	void CommandSetActualVehicleLightsOff(bool value)
	{
		CVehicle::ForceActualVehicleLightsOff(value);
	}

	void CommandSetVehicleLights(int VehicleIndex, int VehicleLightSetting)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(VehicleLightSetting == SET_CAR_LIGHTS_OFF || VehicleLightSetting == SET_CAR_LIGHTS_ON)
			{
				pVehicle->m_nVehicleFlags.bLightsOn = (VehicleLightSetting == SET_CAR_LIGHTS_ON) ? true : false;
				if(VehicleLightSetting == SET_CAR_LIGHTS_ON)
				{
					//Need to turn the engine on for the lights in case no one is in this vehicle
					pVehicle->m_nVehicleFlags.bEngineOn = true;
				}
			}
			
			if(SCRIPT_VERIFY((VehicleLightSetting >= NO_CAR_LIGHT_OVERRIDE && VehicleLightSetting <= SET_CAR_LIGHTS_OFF), "SET_VEHICLE_LIGHTS - Second parameter should be 0, 1 or 2"))
			{
				pVehicle->m_OverrideLights = VehicleLightSetting;
			}
		}
	}

	void CommandSetVehicleUsePlayerLightSettings(int vehicleindex, bool state)
	{
        CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleindex);
        if (pVehicle)
        {
			pVehicle->m_usePlayerLightSettings = state;
		}
	}

	void CommandSetVehicleHeadlightShadows(int VehicleIndex, int flags)
	{
		CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY((flags >= NO_HEADLIGHT_SHADOWS && flags <= HEADLIGHTS_CAST_FULL_SHADOWS), "SET_VEHICLE_HEADLIGHT_SHADOWS - Second parameter should be 0, 1, 2 or 3"))
			{
				pVehicle->m_HeadlightShadows = flags;
			}
		}
	}

	void CommandSetVehicleLightMultiplier(int VehicleIndex, float value)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			pVehicle->m_HeadlightMultiplier = value;
		}
	}

	void CommandAttachVehicleToTrailer(int iVehicleIndex, int iTrailerIndex, float fInvMass)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
		CVehicle* pVehicleTrailer = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
		if(pVehicle && pVehicleTrailer)
		{
			if(SCRIPT_VERIFY(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER, "ATTACH_VEHICLE_TO_TRAILER - Cannot call this script function on a vehicle that is not a trailer"))
			{
                if(SCRIPT_VERIFY(pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAILER, "ATTACH_VEHICLE_TO_TRAILER - Cannot call this script function with two trailers"))
                {
				    if(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER)
				    {
					    CTrailer *pTrailer = static_cast<CTrailer*>(pVehicleTrailer);

						if(fInvMass < 0.01f || fInvMass > 2.0f)//catch bad values for inverse mass.
						{
							fInvMass = 1.0f;
						}
					    pTrailer->AttachToParentVehicle(pVehicle, TRUE, fInvMass);
				    }
                }
			}
		}
	}

	void CommandSetTrailerInverseMassScale(int iTrailerIndex, float fInvMass)
	{
		CVehicle* pVehicleTrailer = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
		if(pVehicleTrailer)
		{
			if(SCRIPT_VERIFY(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER, "SET_TRAILER_INVERSE_MASS_SCALE - Cannot call this script function on a vehicle that is not a trailer"))
			{
				if(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER)
				{
					CTrailer *pTrailer = static_cast<CTrailer*>(pVehicleTrailer);

					if(fInvMass < 0.01f || fInvMass > 2.0f)//catch bad values for inverse mass.
					{
						fInvMass = 1.0f;
					}
					pTrailer->SetInverseMassScale(fInvMass);
				}
			}
		}
	}

	void CommandSetTrailerLegsRaised(int iTrailerIndex)
	{
		CVehicle* pVehicleTrailer = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
		if(pVehicleTrailer)
		{
			if(SCRIPT_VERIFY(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER, "SET_TRAILER_LEGS_RAISED - Cannot call this script function on a vehicle that is not a trailer"))
			{
				if(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER)
				{
					CTrailer *pTrailer = static_cast<CTrailer*>(pVehicleTrailer);
					pTrailer->RaiseTrailerLegsInstantly();
				}
			}
		}
	}

	void CommandSetTrailerLegsLowered(int iTrailerIndex)
	{
		CVehicle* pVehicleTrailer = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
		if(pVehicleTrailer)
		{
			if(SCRIPT_VERIFY(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER, "SET_TRAILER_LEGS_RAISED - Cannot call this script function on a vehicle that is not a trailer"))
			{
				if(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER)
				{
					CTrailer *pTrailer = static_cast<CTrailer*>(pVehicleTrailer);
					pTrailer->LowerTrailerLegsInstantly();
				}
			}
		}
	}

    void CommandStabiliseEntityAttachedToHelicopter(int iVehicleIndex, int iEntityIndex, float fSpringDistance)
    {
        CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
        
        if(pVehicle)
        {
            if(SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI, "STABILISE_ENTITY_ATTACHED_TO_HELI - Cannot call this script function on a vehicle that is not a heli"))
            {   
                CHeli *pHeli = static_cast<CHeli*>(pVehicle);
                
                if(iEntityIndex > 0)
                {
                    CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityIndex);
                    pHeli->AttachAntiSwayToEntity(pEntity, fSpringDistance);
                }
                else
                {
                    pHeli->AttachAntiSwayToEntity(NULL, fSpringDistance);
                }
                
            }
        }
    }

	void CommandSetEntityForHelicopterToDoHeightChecksFor(int iVehicleIndex, int iEntityIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI, "SET_ENTITY_FOR_HELICOPTER_TO_DO_HEIGHT_CHECKS_FOR - Cannot call this script function on a vehicle that is not a heli"))
			{   
				CHeli *pHeli = static_cast<CHeli*>(pVehicle);

				if(iEntityIndex >=0)
				{
					CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(iEntityIndex);
					pHeli->GetHeliIntelligence()->SetExtraEntityToDoHeightChecksFor(pEntity);
				}
				else
				{
					pHeli->GetHeliIntelligence()->SetExtraEntityToDoHeightChecksFor(NULL);
				}
			}
		}
	}

	void CommandClearEntityForHelicopterToDoHeightChecksFor(int iVehicleIndex)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI, "CLEAR_ENTITY_FOR_HELICOPTER_TO_DO_HEIGHT_CHECKS_FOR - Cannot call this script function on a vehicle that is not a heli"))
			{   
				CHeli *pHeli = static_cast<CHeli*>(pVehicle);
				pHeli->GetHeliIntelligence()->SetExtraEntityToDoHeightChecksFor(NULL);
			}
		}
	}

    void CommandAttachVehicleOnToTrailer(int iVehicleIndex, int iTrailerIndex, const scrVector & scrVecEntity1Offset, const scrVector & scrVecEntity2Offset, const scrVector & scrVecRotation, float fPhysicalStrength)
    {
        CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
        CVehicle* pVehicleTrailer = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
        
        Vector3 vecRotation = Vector3(scrVecRotation);
        Vector3 vecEntity1Offset = Vector3(scrVecEntity1Offset);
        Vector3 vecEntity2Offset = Vector3(scrVecEntity2Offset);

        if(pVehicle && pVehicleTrailer)
        {
            if(SCRIPT_VERIFY(pVehicleTrailer->GetVehicleType() == VEHICLE_TYPE_TRAILER, "ATTACH_VEHICLE_ON_TO_TRAILER - Cannot call this script function on a vehicle that is not a trailer"))
            {
                if(SCRIPT_VERIFY(pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAILER, "ATTACH_VEHICLE_ON_TO_TRAILER - Cannot call this script function with two trailers"))
                {
                    Quaternion quatRotate;
                    quatRotate.Identity();
                    if(vecRotation.IsNonZero())
                    {
                        CScriptEulers::QuaternionFromEulers(quatRotate, DtoR * vecRotation);
                    }

                    u32 nPhysicalAttachFlags = ATTACH_FLAG_POS_CONSTRAINT|ATTACH_FLAG_ACTIVATE_ON_DETACH;

                    nPhysicalAttachFlags |= ATTACH_STATE_PHYSICAL;
                    nPhysicalAttachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;
                    nPhysicalAttachFlags |= ATTACH_FLAG_INITIAL_WARP;

                    dev_float invMassScaleA = 1.0f;
                    dev_float invMassScaleB = 0.1f;

					pVehicle->SetParentTrailerAttachStrength(fPhysicalStrength);
					pVehicle->SetParentTrailerEntity1Offset(vecEntity1Offset);
					pVehicle->SetParentTrailerEntity2Offset(vecEntity2Offset);
					pVehicle->SetParentTrailerEntityRotation(quatRotate);

                    pVehicle->AttachToPhysicalUsingPhysics(pVehicleTrailer, 0, 0, nPhysicalAttachFlags, &vecEntity2Offset, &quatRotate, &vecEntity1Offset, fPhysicalStrength, false, invMassScaleA, invMassScaleB);
                }
            }
        }
    }

    void CommandDetachVehicleFromTrailer(int iVehicleIndex)
    {
        if (NULL_IN_SCRIPTING_LANGUAGE != iVehicleIndex)
        {
            CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
            if(pVehicle)
            {
                DEV_ONLY(bool bTrailerAttachPointFound = false);
                //loop through gadgets until we found the trailer attach gadget
                for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
                {
                    CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

                    if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
                    {
                        CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);
                        pTrailerAttachPoint->DetachTrailer(pVehicle);
                        DEV_ONLY(bTrailerAttachPointFound = true);
                    }
                }

                SCRIPT_ASSERT(bTrailerAttachPointFound == TRUE, "DETACH_VEHICLE_FROM_TRAILER - Trailer attach point gadget not found");
            }
        }

    }

    // Deprecated
    void CommandDisableTrailerBreakingFromVehicle(int UNUSED_PARAM(iTrailerIndex), bool UNUSED_PARAM(bIndestructableTrailerConstraint))
    {
        AssertMsg(0, "DISABLE_TRAILER_BREAKING_FROM_VEHICLE - This command is being removed. Please remove any calls to this from your script, Thanks.");
/*
        CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTrailerIndex);
        if(pVehicle)
        {
            if(SCRIPT_VERIFY((pVehicle->InheritsFromTrailer()), "DISABLE_TRAILER_BREAKING_FROM_VEHICLE - vehicle must be a trailer"))
            {
                CTrailer *pTrailer = static_cast<CTrailer*>(pVehicle);
                pTrailer->SetTrailerConstraintIndestructible(bIndestructableTrailerConstraint);
            }
        }
*/
    }

    bool CommandIsVehicleAttachedToTrailer(int iVehicleIndex)
    {
        const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);
        if(pVehicle)
        {
            //loop through gadgets until we found the trailer attach gadget
            for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
            {
                CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

                if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
                {
                    CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);
                    if(pTrailerAttachPoint->GetAttachedTrailer(pVehicle))
					{
                        return true;
					}
                    else if(pVehicle->HasDummyAttachmentChildren())
					{
						// Dummy trailers are restricted to dummy-child slot 0.
						CVehicle * pDummyChild = pVehicle->GetDummyAttachmentChild(0);
						if(pDummyChild && pDummyChild->InheritsFromTrailer())
						{
							return true;
						}
					}
					return false;
                }
            }
        }

        return false;
    }
    
	void CommandSetVehicleTyreFixed(int VehicleIndex, int WheelNumber)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			eHierarchyId nWheelId = CWheel::GetScriptWheelId(eScriptWheelList(WheelNumber));

			if(pVehicle->GetWheelFromId(nWheelId))
			{
				pVehicle->GetWheelFromId(nWheelId)->ResetDamage();

				CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
				Assert(pShader);
				pShader->SetEnableTyreDeform(false);
			}
		}
	}

	void CommandSetVehicleNumberPlateText(int VehicleIndex, const char *licencePlateTxt_Max8Chars)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			pShader->SetLicensePlateText(licencePlateTxt_Max8Chars);

			gnetDebug1("%s: SetLicensePlateText done for [%d] [%s]",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleIndex, licencePlateTxt_Max8Chars ? licencePlateTxt_Max8Chars : "");
		}
#if !__NO_OUTPUT
		else
		{
			gnetDebug1("%s: SetLicensePlateText not done for [%d] [%s]",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleIndex, licencePlateTxt_Max8Chars ? licencePlateTxt_Max8Chars : "");
		}
#endif //!__NO_OUTPUT
	}

	const char* CommandGetVehicleNumberPlateText(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			return pShader->GetLicensePlateText();
		}
		return NULL;
	}

	const char* CommandGetVehicleNumberPlateCharacter(const char *pNumberPlateText, s32 CharacterIndex)
	{
		static char ReturnString[2];
		ReturnString[0] = '\0';
		ReturnString[1] = '\0';

		if (scriptVerifyf(pNumberPlateText, "%s:GET_VEHICLE_NUMBER_PLATE_CHARACTER - string is a NULL pointer", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerifyf(CharacterIndex >= 0, "%s:GET_VEHICLE_NUMBER_PLATE_CHARACTER - CharacterIndex is negative", pNumberPlateText))
			{
				if (scriptVerifyf(CharacterIndex < strlen(pNumberPlateText), "%s:GET_VEHICLE_NUMBER_PLATE_CHARACTER - CharacterIndex is beyond the end of the string", pNumberPlateText))
				{
					ReturnString[0] = pNumberPlateText[CharacterIndex];
				}
			}
		}

		return ReturnString;
	}

	int CommandGetNumberOfVehicleNumberPlates()
	{
		return CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData().GetTextureArray().size();
	}

	void CommandSetVehicleNumberPlateTextIndex(int VehicleIndex, s32 index)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			pShader->SetLicensePlateTexIndex(index);
		}
	}

	int CommandGetVehicleNumberPlateTextIndex(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
			Assert(pShader);
			return pShader->GetLicensePlateTexIndex();
		}

		return -1;
	}

	bool CommandIsRecordingGoingOnForVehicle(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
#if !__FINAL
			if (CVehicleRecordingMgr::IsCarBeingRecorded(pVehicle))
			{
				return true;
			}
#endif
		}
		return false;
	}

	void CommandSkipToEndAndStopPlaybackRecordedVehicle(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::SkipToEndAndStopPlaybackRecordedCar(pVehicle);
		}
	}

	void CommandSetRandomTrains(bool RandomTrainsFlag)
	{
		if (RandomTrainsFlag)
		{
			CTrain::DisableRandomTrains(false);
		}
		else
		{
			CTrain::DisableRandomTrains(true);
		}
	}

	void CommandSwitchTrainTrack(int iTrackIndex, bool bSwitchOn)
	{
		CTrain::SwitchTrainTrackFromScript(iTrackIndex, bSwitchOn, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}

	void CommandSetTrainTrackSpawnFrequency(int iTrackIndex, s32 iTimeMS)
	{
		CTrain::SetTrainTrackSpawnFrequencyFromScript(iTrackIndex, iTimeMS, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}

	int CommandCreateMissionTrain(int trainConfigurationIndex, const scrVector & scrVecNewCoors, bool directionFlag, bool bRegisterAsNetworkObject=false, bool bScriptHostObject=false)
	{
		CTrain *pTrainEngine;
		int NewTrainEngineIndex = 0;

		if(!bRegisterAsNetworkObject)
		{
			bScriptHostObject = false;
		}

		//if (SCRIPT_VERIFY(CStreaming::HasObjectLoaded(gTrainConfigs[CarriageConfiguration].ModelIndexes[0], CModelInfo::GetStreaming ModuleId()), "CREATE_MISSION_TRAIN was called but train model (subway?) wasn't streamed in."))
		{
			s8 trackIndex = 0;
			s32 trackNode = CTrain::FindClosestTrackNode(Vector3(scrVecNewCoors), trackIndex);
			CTrain::CreateTrain(trackIndex, trackNode, directionFlag, static_cast<u8>(trainConfigurationIndex), &pTrainEngine, NULL, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
			NewTrainEngineIndex = CTheScripts::GetGUIDFromEntity(*pTrainEngine);

			CTrain * pTrainComponent = pTrainEngine;
			while(pTrainComponent)
			{
				CTheScripts::RegisterEntity(pTrainComponent, bScriptHostObject, bRegisterAsNetworkObject);
				pTrainComponent = pTrainComponent->GetLinkedToBackward();
			}
#if __DEV
			pTrainEngine->m_nDEflags.bCheckedForDead = TRUE;
#endif
// END DEBUG!!
		}
		return NewTrainEngineIndex;
	}
	int CommandCreateMissionTrainByName(int configurationName, const scrVector & scrVecNewCoors, bool directionFlag)
	{
		int configIdx = CTrain::GetTrainConfigIndex((u32)configurationName);
		if(Verifyf(configIdx!=-1,"No train with the given name found"))
		{
			return CommandCreateMissionTrain(configIdx,scrVecNewCoors, directionFlag);			
		}
		else
		{
			return -1;
		}
	}

	void CommandSetMissionTrainsAsNoLongerNeeded(int UNUSED_PARAM(iReleaseTrainFlags))
	{
		scriptAssertf(0, "%s : SET_MISSION_TRAINS_AS_NO_LONGER_NEEDED is no longer supported", CTheScripts::GetCurrentScriptNameAndProgramCounter());
//		CTrain::ReleaseMissionTrains( CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), iReleaseTrainFlags );
	}

	void CommandAllowTrainToBeRemovedByPopulation(int VehicleIndex)
	{
		CVehicle *pVehicle;
		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pVehicle && scriptVerifyf(pVehicle->InheritsFromTrain(), "ALLOW_TRAIN_TO_BE_REMOVED_BY_POPULATION - %s called on vehicle: %s[0x%p] that is not a train", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle ? pVehicle->GetLogName() : "Missing", pVehicle))
		{
			gnetDebug1("ALLOW_TRAIN_TO_BE_REMOVED_BY_POPULATION: %s[0x%p] called from: %s", pVehicle->GetLogName(), pVehicle, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			CTrain* train = static_cast<CTrain*>(pVehicle);
			train->m_nTrainFlags.bAllowRemovalByPopulation = true;
		}
	}

	void CommandDeleteAllTrains()
	{
		CTrain::RemoveAllTrains();
	}

	void CommandSetTrainSpeed(int VehicleIndex, float NewTrainSpeed)
	{
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if(pVehicle)
			{
				if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "SET_TRAIN_SPEED - Vehicle is not a train"))
				{
					CTrain::SetCarriageSpeed(CTrain::FindEngine((CTrain*)pVehicle), NewTrainSpeed);
				}
			}
		}
	}

	void CommandSetTrainCruiseSpeed(int VehicleIndex, float NewTrainCruiseSpeed)
	{
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
			if(pVehicle)
			{
				if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "SET_TRAIN_CRUISE_SPEED - Vehicle is not a train"))
				{
					CTrain::SetCarriageCruiseSpeed(CTrain::FindEngine((CTrain*)pVehicle), NewTrainCruiseSpeed);
				}
			}
		}
	}

	int CommandGetTrainCaboose(int TrainIndex)
	{
		int ReturnCabooseIndex = 0;

		const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(TrainIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "GET_TRAIN_CABOOSE - Vehicle is not a train"))
			{
				CVehicle* pTrainCaboose = CTrain::FindCaboose((CTrain*)pVehicle);
				if(pTrainCaboose)
				{
					ReturnCabooseIndex = CTheScripts::GetGUIDFromEntity(*pTrainCaboose);
#if __DEV
					pTrainCaboose->m_nDEflags.bCheckedForDead = TRUE;
#endif
				}
			}
		}
	
		return ReturnCabooseIndex;
	}



	void CommandSetTrainStopsForStations(int VehicleIndex, bool bStops)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "SET_TRAIN_STOPS_FOR_STATIONS - Vehicle is not a train"))
			{
				((CTrain*)pVehicle)->SetTrainStopsForStations(bStops);
			}
		}
	}

	void CommandSetTrainIsStoppedAtStation(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "SET_TRAIN_IS_STOPPED_AT_STATION - Vehicle is not a train"))
			{
				((CTrain*)pVehicle)->SetTrainIsStoppedAtStation();
			}
		}
	}

	void CommandTrainLeaveStation(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "TRAIN_LEAVE_STATION - Vehicle is not a train"))
			{
				((CTrain*)pVehicle)->SetTrainToLeaveStation();
			}
		}
	}

	int CommandGetNextStationForTrain(int VehicleIndex)
	{
		int retVal = 0;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "TRAIN_GET_NEXT_STATION - Vehicle is not a train"))
			{
				retVal = ((CTrain*)pVehicle)->GetNextStation();
				scriptAssertf(retVal >= 0, "%s:TRAIN_GET_NEXT_STATION - Could not find the next station",CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			}
		}

		return retVal;
	}

	int CommandGetCurrentStationForTrain(int VehicleIndex)
	{
		int retVal = 0;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "TRAIN_GET_CURRENT_STATION - Vehicle is not a train"))
			{
				retVal = ((CTrain*)pVehicle)->GetCurrentStation();
				scriptAssertf(retVal >= 0, "%s:TRAIN_GET_NEXT_STATION - Could not find the current station",CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			}
		}

		return retVal;
	}

	int CommandGetTimeTilNextStation(int VehicleIndex)
	{
		int retVal = 0;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "GET_TIME_TIL_NEXT_STATION - Vehicle is not a train"))
			{
				retVal = ((CTrain*)pVehicle)->GetTimeTilNextStation();
					scriptAssertf(retVal >= 0, "%s:GET_TIME_TIL_NEXT_STATION - Could not find the time to the next station",CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			}
		}

		return retVal;
	}

	void CommandSetRenderTrainAsDerailed(int VehicleIndex, bool bVal)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "SET_RENDER_TRAIN_AS_DERAILED - Vehicle is not a train"))
			{
				CTrain *pLoop = (CTrain*)pVehicle;
				while (pLoop)
				{
					pLoop->m_nTrainFlags.bRenderAsDerailed = bVal;
					pLoop = pLoop->GetLinkedToForward();
				}
				pLoop = (CTrain*)pVehicle;
				while (pLoop)
				{
					pLoop->m_nTrainFlags.bRenderAsDerailed = bVal;
					pLoop = pLoop->GetLinkedToBackward();
				}
			}

		}
	}

	const char *CommandGetStationName(int VehicleIndex, int StationIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN,  "GET_TRAIN_STATION_NAME - Vehicle is not a train"))
			{
				return ((CTrain*)pVehicle)->GetStationName(StationIndex);
			}
		}

		return NULL;
	}

	void CommandSetRandomBoats(bool RandomBoatsFlag)
	{
		CVehiclePopulation::ms_bAllowRandomBoats = RandomBoatsFlag;
	}

	void CommandSetRandomBoatsMP(bool RandomBoatsFlag)
	{
		CVehiclePopulation::ms_bAllowRandomBoatsMP = RandomBoatsFlag;
	}

	void CommandSetGarbageTrucks(bool Flag)
	{
		CVehiclePopulation::ms_bAllowGarbageTrucks = Flag;
	}

	bool CommandDoesVehicleHaveStuckVehicleCheck(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		bool LatestCmpFlagResult = false;
		
		if(pVehicle)
		{
			if(CScriptCars::GetStuckCars().IsCarInStuckCarArray(VehicleIndex) ||
			   CScriptCars::GetUpsideDownCars().IsCarInUpsideDownCarArray(VehicleIndex))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandSetPlaybackSpeed(int VehicleIndex, float PlaybackSpeed)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	
		if(pVehicle)
		{
			CVehicleRecordingMgr::SetPlaybackSpeed(pVehicle, PlaybackSpeed);
		}
	}

	void CommandStartPlaybackRecordedVehicleUsingAi(int VehicleIndex, int FileNumber, const char* pRecordingName, float fSpeed, int iDrivingFlags)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, (pRecordingName ? pRecordingName : ""));
			if(scriptVerifyf(index != -1, "START_PLAYBACK_RECORDED_VEHICLE_USING_AI: %s%d vehicle recording doesnt exist", pRecordingName, FileNumber))
			{
				CVehicleRecordingMgr::StartPlaybackRecordedCar(pVehicle, index, true, fSpeed,iDrivingFlags);

				CPed* pDriver = pVehicle->GetDriver();
				if (!pDriver || (pDriver && !pDriver->IsPlayer()))
				{
					pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
					pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();

					if (pDriver)
					{
						pDriver->GetPedIntelligence()->AddTaskDefault(rage_new CTaskInVehicleBasic( pVehicle, false ));
					}
				}
			}
		}
	}

	void CommandSkipTimeInPlaybackRecordedVehicle(int VehicleIndex, float SkipTime)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::SkipTimeForwardInRecording(pVehicle, SkipTime);
		}
	}

	void CommandSetAdditionalRotationForRecordedVehiclePlayback(int VehicleIndex, const scrVector & scrVecRot, int RotOrder)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			const Vec3V eulerDegrees = VECTOR3_TO_VEC3V(Vector3(scrVecRot));
			const Vec3V axisAngleRotation = ComputeAxisAnglePrecise(QuatVFromEulers(Scale(eulerDegrees,ScalarV(V_TO_RADIANS)),static_cast<EulerAngleOrder>(RotOrder)));
			CVehicleRecordingMgr::SetAdditionalRotationForRecordedVehicle(pVehicle, axisAngleRotation);
		}
	}

	void CommandSetPositionOffsetForRecordedVehiclePlayback(int VehicleIndex, const scrVector & scrVecOffset)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::SetLocalPositionOffsetForRecordedVehicle(pVehicle, VECTOR3_TO_VEC3V(Vector3(scrVecOffset)));
		}
	}

	void CommandSetGlobalPositionOffsetForRecordedVehiclePlayback(int VehicleIndex, const scrVector & scrVecOffset)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::SetGlobalPositionOffsetForRecordedVehicle(pVehicle, VECTOR3_TO_VEC3V(Vector3(scrVecOffset)));
		}
	}

	float CommandGetPositionInRecording(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
#if __ASSERT
			scriptAssertf(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVehicle), "%s: GET_POSITION_IN_RECORDING - vehicle not using a car recording, call IS_PLAYBACK_GOING_ON_FOR_VEHICLE first", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __ASSERT
			return CVehicleRecordingMgr::FindPositionInRecording(pVehicle);
		}
		return 0.0f;
	}

	float CommandGetTimePositionInRecording(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pVehicle)
		{
#if __ASSERT
			scriptAssertf(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVehicle), "%s: GET_TIME_POSITION_IN_RECORDING - vehicle not using a car recording, call IS_PLAYBACK_GOING_ON_FOR_VEHICLE first", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __ASSERT
			return CVehicleRecordingMgr::FindTimePositionInRecording(pVehicle);
		}
		return 0.0f;
	}

	float CommandGetTimePositionInRecordedRecording(int DEV_ONLY(VehicleIndex))
	{
#if __DEV
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			return CVehicleRecordingMgr::FindTimePositionInRecordedRecording(pVehicle);
		}
		return 0.0f;
#else
		return 0.0f;
#endif
	}

	void CommandExplodeVehicleInCutscene(int VehicleIndex, bool bAddExplosion)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->SetCanBeDamaged(TRUE);
			pVehicle->BlowUpCar( NULL, true, bAddExplosion, false );
		}
	}

	void CommandAddVehicleStuckCheckWithWarp(int VehicleIndex, float MinimumMoveDistance, int CheckFrequency, bool WarpIfStuckFlag, bool WarpIfUpsideDownFlag, bool WarpIfInWaterFlag, int WarpMethod)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(SCRIPT_VERIFY(WarpMethod != 0, "ADD_STUCK_VEHICLE_CHECK_WITH_WARP - can't check 0 closest car nodes - last parameter must be -1 or greater than 0"))
			{
				bool bStuckFlag, bUpsideDownFlag, bInWaterFlag;

				bStuckFlag = false;
				if (WarpIfStuckFlag)
				{
					bStuckFlag = true;
				}
				bUpsideDownFlag = false;
				if (WarpIfUpsideDownFlag)
				{
					bUpsideDownFlag = true;
				}
				bInWaterFlag = false;
				if (WarpIfInWaterFlag)
				{
					bInWaterFlag = true;
				}
				
				CScriptCars::GetStuckCars().AddCarToCheck(VehicleIndex, MinimumMoveDistance, CheckFrequency, true, bStuckFlag, bUpsideDownFlag, bInWaterFlag, (s8) WarpMethod);
			}
		}
	}

	void CommandSetVehicleModelIsSuppressed(int VehicleModelHashKey, bool IsSuppressed)
	{
		fwModelId VehicleModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value

		if (SCRIPT_VERIFY ( VehicleModelId.IsValid(), "SET_VEHICLE_MODEL_IS_SUPPRESSED - this is not a valid model index"))
		{
			bool changedSomething = false;
			if (IsSuppressed)
			{
				changedSomething = CScriptCars::GetSuppressedCarModels().AddToSuppressedModelArray(VehicleModelId.GetModelIndex(), CTheScripts::GetCurrentScriptName());
			}
			else
			{
				changedSomething = CScriptCars::GetSuppressedCarModels().RemoveFromSuppressedModelArray(VehicleModelId.GetModelIndex());
			}
			if(changedSomething)
			{
				CScenarioManager::CarGensMayNeedToBeUpdated();
			}
		}
	}

	enum eBumperCheck
	{
		NO_BUMPER_CHECK,
		FRONT_BUMPER_CHECK,
		BACK_BUMPER_CHECK
	};

	int BaseGetRandomCarInSphereNoSave(const Vector3 &CentreVec, float Radius, int VehicleModelHashKey, int SearchVehicleFlags, eBumperCheck BumperCheck, int VehicleToBeIgnored)
	{
		Vector3 TempVec, VecDiff;
		s32 CarPoolIndex, RandomCarIndex = 0;
		float ClosestDistance, Distance;
		CVehicle *pVehicle;
		const CVehicle *pVehicleToBeIgnored = NULL;

		fwModelId VehicleModelId;
		//	if (VehicleModelHashKey	==	?	hash key can't be negative? How to specify that the vehicle model doesn't matter?
		if (VehicleModelHashKey	!= DUMMY_ENTRY_IN_MODEL_ENUM_FOR_SCRIPTING_LANGUAGE)
		{
			CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
			if (!SCRIPT_VERIFY ( VehicleModelId.IsValid(), "GET_RANDOM_VEHICLE_IN_SPHERE - this is not a valid model index"))
			{
				RandomCarIndex = 0;
			}
		}

		if (VehicleToBeIgnored)
		{
			pVehicleToBeIgnored = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleToBeIgnored, 0);
		}

		CarPoolIndex = (s32) CVehicle::GetPool()->GetSize();
		RandomCarIndex = NULL_IN_SCRIPTING_LANGUAGE;
		ClosestDistance = 9999.9f;
		while (CarPoolIndex--)
		{
			pVehicle = CVehicle::GetPool()->GetSlot(CarPoolIndex);
			if (pVehicle && pVehicle != pVehicleToBeIgnored)
			{
				if (CanVehicleBeGrabbedByScript(pVehicle, VehicleModelId.GetModelIndex(), SearchVehicleFlags))
				{
					switch (BumperCheck)
					{
						case NO_BUMPER_CHECK :
							TempVec = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
							break;

						case FRONT_BUMPER_CHECK :
							TempVec = pVehicle->TransformIntoWorldSpace(Vector3(0.0f, pVehicle->GetBoundingBoxMax().y, 0.0f));
							break;

						case BACK_BUMPER_CHECK :
							TempVec = pVehicle->TransformIntoWorldSpace(Vector3(0.0f, pVehicle->GetBoundingBoxMin().y, 0.0f));
							break;
					}

					VecDiff = TempVec - CentreVec;
					Distance = VecDiff.Mag();
					if (Distance < Radius)
					{
						if (Distance < ClosestDistance)
						{
							RandomCarIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
							ClosestDistance = Distance;
						}
					}
				}
			}
		}
		if (RandomCarIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(RandomCarIndex, 0);
			scriptAssertf(pVehicle, "%s:GET_RANDOM_VEHICLE_IN_SPHERE - Failed to get a vehicle from the pool (Ask a coder)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __DEV
			if (pVehicle)
			{
			pVehicle->m_nDEflags.bCheckedForDead = TRUE;
			}
#endif
		}
		
		return RandomCarIndex;

	}

	int CommandGetRandomVehicleInSphere(const scrVector & scrVecCentre, float Radius, int VehicleModelHashKey, int SearchFlags)
	{
		Vector3 vCentre = Vector3(scrVecCentre);
		s32 ReturnVehicleIndex;

		ReturnVehicleIndex = BaseGetRandomCarInSphereNoSave(vCentre, Radius, VehicleModelHashKey, SearchFlags, NO_BUMPER_CHECK, 0);

		return ReturnVehicleIndex;
	}
	
	int CommandGetRandomVehicleFrontBumperInSphere(const scrVector & scrVecCentre, float Radius, int VehicleModelHashKey, int SearchFlags, int VehicleToBeIgnored)
	{
		Vector3 vCentre = Vector3(scrVecCentre);
		s32 ReturnVehicleIndex = BaseGetRandomCarInSphereNoSave(vCentre, Radius, VehicleModelHashKey, SearchFlags, FRONT_BUMPER_CHECK, VehicleToBeIgnored);

		return ReturnVehicleIndex;
	}

	int CommandGetRandomVehicleBackBumperInSphere(const scrVector & scrVecCentre, float Radius, int VehicleModelHashKey, int SearchFlags, int VehicleToBeIgnored)
	{
		Vector3 vCentre = Vector3(scrVecCentre);
		s32 ReturnVehicleIndex = BaseGetRandomCarInSphereNoSave(vCentre, Radius, VehicleModelHashKey, SearchFlags, BACK_BUMPER_CHECK, VehicleToBeIgnored);
		
		return ReturnVehicleIndex;
	}

	int CommandGetClosestVehicle(const scrVector & scrVecCentre, float Radius, int VehicleModelHashKey, int SearchFlags)
	{
		Vector3 TempVec, CentreVec, VecDiff;
		s32 CarPoolIndex, RandomCarIndex = 0;
		CVehicle *pVehicle;

		fwModelId VehicleModelId;
		if (VehicleModelHashKey	!= DUMMY_ENTRY_IN_MODEL_ENUM_FOR_SCRIPTING_LANGUAGE)
		{
			CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
			if (!SCRIPT_VERIFY ( VehicleModelId.IsValid(), "GET_CLOSEST_VEHICLE - this is not a valid model index"))
			{
				return 	RandomCarIndex;
			}
		}

		CentreVec = Vector3(scrVecCentre);

		CarPoolIndex = (s32) CVehicle::GetPool()->GetSize();
		float ClosestDistanceSqaured = Radius*Radius;
		while (CarPoolIndex--)
		{
			pVehicle = CVehicle::GetPool()->GetSlot(CarPoolIndex);
			if (pVehicle)
			{
				float DistanceSquared = DistSquared(pVehicle->GetTransform().GetPosition(),RCC_VEC3V(CentreVec)).Getf();
				if(DistanceSquared < ClosestDistanceSqaured)
				{
					if (CanVehicleBeGrabbedByScript(pVehicle, VehicleModelId.GetModelIndex(), SearchFlags))
					{
						RandomCarIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
						ClosestDistanceSqaured = DistanceSquared;
					}
				}
			}
		}
		if (RandomCarIndex != NULL_IN_SCRIPTING_LANGUAGE)
		{
			pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(RandomCarIndex, 0);
			scriptAssertf(pVehicle, "%s:GET_CLOSEST_VEHICLE - Failed to get a vehicle from the pool (Ask a coder)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __DEV
			if (pVehicle)
			{
			pVehicle->m_nDEflags.bCheckedForDead = TRUE;
			}
#endif
		}
		return RandomCarIndex;
	}

	void CommandStopRecordingVehicle(int DEV_ONLY(VehicleIndex) )
	{
#if __DEV
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			CVehicleRecordingMgr::StopRecordingCar(pVehicle);
		}
#endif
	}

	int CommandGetTrainCarriage(int TrainEngineIndex, int CarriageNumber)
	{
		const CTrain *pTrainEngine;
		CTrain *pTrainCarriage;
		int ReturnTrainCarriageIndex = NULL_IN_SCRIPTING_LANGUAGE;

		pTrainEngine = (CTrain *) CTheScripts::GetEntityToQueryFromGUID<CVehicle>(TrainEngineIndex);
		if(pTrainEngine)
		{
			pTrainCarriage = CTrain::FindCarriage(pTrainEngine, (u8) CarriageNumber);
			if (pTrainCarriage)
			{
				ReturnTrainCarriageIndex = CTheScripts::GetGUIDFromEntity(*pTrainCarriage);
	#if __DEV
				pTrainCarriage->m_nDEflags.bCheckedForDead = TRUE;
	#endif
			}
		}
		return ReturnTrainCarriageIndex;
	}

	bool CommandIsMissionTrain(int TrainIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(TrainIndex);
		if(pVehicle)
		{
			if(pVehicle->InheritsFromTrain())
			{
				const CTrain* pTrain = (const CTrain*) pVehicle;
				if(pTrain->GetIsMissionTrain())
				{
					return true;
				}
			}
		}
		return false;
	}

// 	void CommandSetHeliSpeedCheat(int HeliIndex, int SpeedBoostValue)
// 	{
// 		// this script command is not approved for use in network scripts
// 		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "ACTIVATE_HELI_SPEED_CHEAT - This script command is not allowed in network game scripts!"))
// 		{
// 			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(HeliIndex);
// 			if (pVehicle)
// 			{
// 				if (SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_SPEED_CHEAT - Can only call this command for helis"))
// 				{
// 					pVehicle->GetIntelligence()->SpeedCheat = (s8) SpeedBoostValue;
// 				}
// 			}
// 		}
// 	}

	void CommandDeleteMissionTrain(int &TrainIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(TrainIndex, 0);

		if (pVehicle)
		{
			if (SCRIPT_VERIFYF(pVehicle->InheritsFromTrain(), "DELETE_MISSION_TRAIN - Can only call this command for trains. It was called on: %s", pVehicle->GetLogName()))
			{
				CTrain * pTrain = static_cast<CTrain*>(pVehicle);
				
				if (SCRIPT_VERIFY(pTrain->GetMissionScriptId()!=THREAD_INVALID || (NetworkInterface::IsGameInProgress() && pTrain->GetScriptThatCreatedMe()), "DELETE_MISSION_TRAIN - this is not a mission train (was it released into ambient population?)"))
				{
					bool isScriptValid = pTrain->GetMissionScriptId()!=THREAD_INVALID;
					if(!isScriptValid && NetworkInterface::IsGameInProgress())
					{
						isScriptValid = CTheScripts::GetCurrentGtaScriptThread()->m_Handler->GetScriptId() == *pTrain->GetScriptThatCreatedMe();
					}

					if (SCRIPT_VERIFY(isScriptValid, "DELETE_MISSION_TRAIN - script is trying to delete a train which it did not create"))
					{
						CTrain::RemoveOneMissionTrain(*CTheScripts::GetCurrentGtaScriptThread(), pTrain);
					}
				}
			}
		}

		TrainIndex = 0;
	}

	void CommandSetMissionTrainAsNoLongerNeeded(int &TrainIndex, int iReleaseTrainFlags)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(TrainIndex, 0);

		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->InheritsFromTrain(), "SET_MISSION_TRAIN_AS_NO_LONGER_NEEDED - Can only call this command for trains"))
			{
				CTrain * pTrain = static_cast<CTrain*>(pVehicle);

				if (SCRIPT_VERIFY(pTrain->GetMissionScriptId()!=THREAD_INVALID || (NetworkInterface::IsGameInProgress() && pTrain->GetScriptThatCreatedMe()), "SET_MISSION_TRAIN_AS_NO_LONGER_NEEDED - this is not a mission train (was it already released into ambient population?)"))
				{
					bool isScriptValid = pTrain->GetMissionScriptId()!=THREAD_INVALID;
					if(!isScriptValid && NetworkInterface::IsGameInProgress())
					{
						isScriptValid = CTheScripts::GetCurrentGtaScriptThread()->m_Handler->GetScriptId() == *pTrain->GetScriptThatCreatedMe();
					}

					if (SCRIPT_VERIFY(isScriptValid, "SET_MISSION_TRAIN_AS_NO_LONGER_NEEDED - script is trying to mark as no longer needed a train which it did not create"))
					{
						CTrain::ReleaseOneMissionTrain(*CTheScripts::GetCurrentGtaScriptThread(), pTrain, iReleaseTrainFlags);
					}
				}
			}
		}

		TrainIndex = 0;
	}

	void CommandRequestVehicleAsset(int VehicleModelHashKey, int iVehicleRequestFlags)
	{
		if( scriptVerifyf(VehicleModelHashKey != 0, "REQUEST_VEHICLE_ASSET - %s Vehicle type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey) &&
			scriptVerifyf(iVehicleRequestFlags != 0, "REQUEST_VEHICLE_ASSET - %s Vehicle request flags was 0", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
		{
			fwModelId VehicleModelId;
			CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);
			if (scriptVerifyf(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "REQUEST_VEHICLE_ASSET - %s Vehicle type [%d] does not exist in data or isn't a Vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey))
			{
				CScriptResource_Vehicle_Asset vehicleAsset(VehicleModelHashKey, iVehicleRequestFlags);
				CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(vehicleAsset);
			}
		}
	}

	bool CommandHasVehicleAssetLoaded(int VehicleModelHashKey)
	{
		if(scriptVerifyf(VehicleModelHashKey != 0, "HAS_VEHICLE_ASSET_LOADED - %s Vehicle type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey))
		{
			fwModelId VehicleModelId;
			CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);
			if (scriptVerifyf(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "REQUEST_VEHICLE_ASSET - %s Vehicle type [%d] does not exist in data or isn't a Vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey))
			{
				if(CTheScripts::GetCurrentGtaScriptHandler())
				{
					scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_ASSET, VehicleModelHashKey);
					if(scriptVerifyf(pScriptResource, "HAS_VEHICLE_ASSET_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), static_cast<CVehicleModelInfo*>(pBaseModelInfo)->GetGameName()))
					{
						if(scriptVerifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_ASSET, "HAS_VEHICLE_ASSET_LOADED - %s incorrect type of resource %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptResource->GetType()))
						{
							CScriptResource_Vehicle_Asset* pVehicleAsset = static_cast<CScriptResource_Vehicle_Asset*>(pScriptResource);
							CVehicleScriptResource* pVehicleResource = CVehicleScriptResourceManager::GetResource(pVehicleAsset->GetReference());
							if(scriptVerifyf(pVehicleResource, "HAS_VEHICLE_ASSET_LOADED - %s NULL resource", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
							{
								return pVehicleResource->GetIsStreamedIn();
							}
						}
					}
				}
			}
		}

		return false;
	}

	void CommandRemoveVehicleAsset(int VehicleModelHashKey)
	{
		if(scriptVerifyf(VehicleModelHashKey != 0, "REMOVE_VEHICLE_ASSET - %s Vehicle type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey))
		{
			fwModelId VehicleModelId;
			CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);
			if (scriptVerifyf(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "REMOVE_VEHICLE_ASSET - %s Vehicle type [%d] does not exist in data or isn't a Vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter(), VehicleModelHashKey))
			{
				CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_ASSET, VehicleModelHashKey);
			}
		}
	}

	void CommandRequestVehicleRecording(int FileNumber, const char* pRecordingName)
	{
		int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, pRecordingName);
		if(scriptVerifyf(index != -1, "REQUEST_VEHICLE_RECORDING - vehicle recording with number %d and name %s doesn't exist", FileNumber, pRecordingName))
		{
			u32 extraFlags = STRFLAG_FORCE_LOAD;
			CScriptResource_VehicleRecording veh_recording(index, extraFlags);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(veh_recording);
		}
	}

	bool CommandHasVehicleRecordingBeenLoaded(int FileNumber, const char* pRecordingName)
	{
		int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, pRecordingName);
		bool LatestCmpFlagResult = false;

		if(scriptVerifyf(index != -1, "HAS_VEHICLE_RECORDING_BEEN_LOADED - vehicle recording with number %d and name %s doesn't exist", FileNumber, pRecordingName))
		{
			if (CTheScripts::GetCurrentGtaScriptHandler())
			{
				scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_RECORDING, index), "HAS_VEHICLE_RECORDING_BEEN_LOADED - %s has not requested vehicle recording with number %d and name %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), FileNumber, pRecordingName);
			}

			if (CVehicleRecordingMgr::HasRecordingFileBeenLoaded(index))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}
	
	void CommandRemoveVehicleRecording(int FileNumber, const char* pRecordingName)
	{
		int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber, pRecordingName);

		if (scriptVerifyf(index != -1, "REMOVE_VEHICLE_RECORDING - couldn't find vehicle recording with number %d and name %s", FileNumber, pRecordingName))
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_RECORDING, index);
		}
	}

	bool CommandDoesVehicleRecordingFileExist(int DEV_ONLY(FileNumber), const char* DEV_ONLY(pRecordingName))
	{
#if __DEV
		bool LatestCmpFlagResult = false;
		if (SCRIPT_VERIFY((FileNumber > 0) && (FileNumber < 4000), "DOES_VEHICLE_RECORDING_EXIST - File number should be between 1 and 3999"))
		{
			if (CVehicleRecordingMgr::DoesRecordingFileExist(FileNumber, (pRecordingName ? pRecordingName : "")))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
#else
		return true;
#endif
	}

	bool CommandDoesVehicleRecordingFileExistInCdImage(int DEV_ONLY(FileNumber), const char* DEV_ONLY(pRecordingName))
	{
#if __DEV
		bool LatestCmpFlagResult = false;
		if (SCRIPT_VERIFY((FileNumber > 0) && (FileNumber < 4000), "DOES_VEHICLE_RECORDING_FILE_EXIST_IN_CDIMAGE - File number should be between 1 and 3999"))
		{
			if (CVehicleRecordingMgr::FindIndexWithFileNameNumber(FileNumber,(pRecordingName ? pRecordingName : "")) != -1)
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
#else
		return true;
#endif
	}

	void CommandDisplayPlaybackRecordedVehicle(int DEV_ONLY(VehicleIndex), int DEV_ONLY(DisplayMode))
	{
#if __DEV
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			CVehicleRecordingMgr::DisplayPlaybackRecordedCar(pVehicle, DisplayMode);
		}
#endif
	}

	void CommandSetMissionTrainCoords(int TrainIndex, const scrVector & scrVecNewCoors)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(TrainIndex);
		Vector3 TempCoors;
		if (pVehicle)
		{
			if (SCRIPT_VERIFY(pVehicle->InheritsFromTrain(), "SET_MISSION_TRAIN_COORDS - Can only call this command for trains"))
			{
				TempCoors = Vector3(scrVecNewCoors);
				CTrain::SetNewTrainPosition( (CTrain *)pVehicle, TempCoors);
			}
		}
	}

	void CommandBringVehicleToHalt(int nVehicleID, float fStoppingDistance, int nTimeToStopFor, bool bControlVerticalVelocity)
	{
		if(SCRIPT_VERIFY((nTimeToStopFor == -1) || (nTimeToStopFor > 0) ,"BRING_VEHICLE_TO_HALT - time must be -1 for infinite or > 0 otherwise"))
		{
			CVehicle *pVehicle = 0;
			CTask* pTask;
			{
				if(SCRIPT_VERIFY(nVehicleID!=NULL_IN_SCRIPTING_LANGUAGE, "BRING_VEHICLE_TO_HALT - Null vehicle ID"))
				{
					pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);
					if(!pVehicle)
						return;

					pTask = rage_new CTaskBringVehicleToHalt(fStoppingDistance, nTimeToStopFor, bControlVerticalVelocity);
					Assert(pTask);
					if(pTask)
					{
						pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_SECONDARY, pTask, VEHICLE_TASK_SECONDARY_ANIM, true);
					}
				}
			}
		}
	}

	void CommandStopBringingVehicleToHalt(int nVehicleID)
	{
		if(SCRIPT_VERIFY(nVehicleID!=NULL_IN_SCRIPTING_LANGUAGE, "STOP_BRINGING_VEHICLE_TO_HALT - Null vehicle ID"))
		{
			CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);
			if(!pVehicle)
				return;

			const aiTask* pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_SECONDARY);
			if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_BRING_VEHICLE_TO_HALT)
			{
				pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
			}
		}
	}

	bool CommandIsVehicleBeingBroughtToHalt(int iVehicleIndex)
	{
		if(SCRIPT_VERIFY(iVehicleIndex != NULL_IN_SCRIPTING_LANGUAGE, "IS_VEHICLE_BEING_BROUGHT_TO_HALT - The vehicle index is invalid."))
		{
			const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);
			if(SCRIPT_VERIFY(pVehicle, "IS_VEHICLE_BEING_BROUGHT_TO_HALT - The vehicle could not be found."))
			{
				const aiTask* pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_SECONDARY);
				if(pTask && (pTask->GetTaskType() == CTaskTypes::TASK_BRING_VEHICLE_TO_HALT))
				{
					return true;
				}
			}
		}

		return false;
	}

	void CommandRaiseForkliftForks(int nVehicleId)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"RAISE_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
						pForks->SetDesiredAcceleration(1.0f);
						pForks->SetDesiredHeight(1.25f);
						pForks->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = 0.0872666f; // About 5 degrees.
						// Forklift needs to be awake to drive the forks to the desired pose.
						pForklift->ActivatePhysics();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandLowerForkliftForks(int nVehicleId)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"LOWER_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
						pForks->SetDesiredAcceleration(-1.0f);
						pForks->SetDesiredHeight(0.0f);
						pForks->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = 0.0f;
						// Forklift needs to be awake to drive the forks to the desired pose.
						pForklift->ActivatePhysics();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandSetForkliftForkHeight(int nVehicleId, float fForkHeightRatio)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"SET_FORKLIFT_FORK_HEIGHT - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);

						if(SCRIPT_VERIFY(fForkHeightRatio>=0.0f && fForkHeightRatio<=1.0f, "SET_FORKLIFT_FORK_HEIGHT - Height must be in range [0.0, 1.0]"))
						{
							// Compute an absolute joint position from the ratio provided by script.
							float fForkHeight = fForkHeightRatio*1.25f;
							float fCarriageAngle = fForkHeightRatio*0.0872666f;

							pForks->SetDesiredHeight(fForkHeight);
							pForks->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = fCarriageAngle;
							// Forklift needs to be awake to drive the forks to the desired pose.
							pForklift->ActivatePhysics();
						}
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandLockForkliftForks(int nVehicleId)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"LOCK_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
						pForks->LockForks();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	bool CommandIsEntityAttachedToForkliftForks(int nVehicleId, int nEntityIndex)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"IS_ENTITY_ATTACHED_TO_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(SCRIPT_VERIFY(pEntity, "IS_ENTITY_ATTACHED_TO_FORKLIFT_FORKS - no entity found with given entity index"))
				{
					// See if we have a set of forks.
					for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
					{
						CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

						if(pVehicleGadget->GetType() == VGT_FORKS)
						{
							CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
							atArray<CPhysical*> const& attachedObjects = pForks->GetListOfAttachedObjects();

							// Search through the list of attached objects for the entity we are interested in.
							for(int obj = 0; obj < attachedObjects.GetCount(); ++obj)
							{
								if(attachedObjects[obj]==pEntity)
									return true;
							}
						}
					} // Loop over vehicle gadgets.
				}
			}
		}

		return false;
	}

	bool CommandIsAnyEntityAttachedToForkliftForks(int nVehicleId)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"IS_ANY_ENTITY_ATTACHED_TO_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
						atArray<CPhysical*> const& attachedObjects = pForks->GetListOfAttachedObjects();

						// Inform the caller if anything is attached to the forks.
						return attachedObjects.GetCount() > 0;
					}
				} // Loop over vehicle gadgets.
			}
		}

		return false;
	}

	void CommandAttachPalletToForkliftForks(int nVehicleId, int nEntityIndex)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"ATTACH_PALLET_TO_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(pEntity)
				{
					if(scriptVerifyf(pEntity->GetModelIndex()==MI_FORKLIFT_PALLET,
						"ATTACH_PALLET_TO_FORKLIFT_FORKS - Can only attach pallet prop (%s) directly", MI_FORKLIFT_PALLET.GetName().GetCStr()))
					{
						// See if we have a set of forks.
						for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
						{
							CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

							if(pVehicleGadget->GetType() == VGT_FORKS)
							{
								CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
								if(SCRIPT_VERIFY(pForks->GetListOfAttachedObjects().GetCount()==0,
									"ATTACH_PALLET_TO_FORKLIFT_FORKS - objects already attached to forks; detach them first."))
								{
									// Figure out the best position to attach the pallet.
									int nBoneIndex = pForklift->GetBoneIndex(VEH_FORKS_ATTACH);
									if(SCRIPT_VERIFY(nBoneIndex > -1, "ATTACH_PALLET_TO_FORKLIFT_FORKS - couldn't find attach bone."))
									{
										Matrix34 matAttachPoint;
										pForklift->GetGlobalMtx(nBoneIndex, matAttachPoint);
										matAttachPoint.d.z -= 0.1f;

										// Warp the pallet to the best position on the forks.
										pEntity->SetMatrix(matAttachPoint, true, true, true);

										// Force the forklift to attach the pallet to its forks.
										pForks->AttachPalletInstanceToForks(pEntity);
									}
								}
							}
						} // Loop over vehicle gadgets.
					}
				}
			}
		}
	}

	void CommandDetachPalletFromForkliftForks(int nVehicleId)
	{
		CVehicle* pForklift = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pForklift)
		{
			if(SCRIPT_VERIFY(pForklift->GetModelIndex()==MI_CAR_FORKLIFT,"DETACH_PALLET_FROM_FORKLIFT_FORKS - Should only be called on forklift model"))
			{
				// See if we have a set of forks.
				for(int i = 0; i < pForklift->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pForklift->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_FORKS)
					{
						CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);
						if(SCRIPT_VERIFY(pForks->GetListOfAttachedObjects().GetCount()>0,
							"DETACH_PALLET_FROM_FORKLIFT_FORKS - no objects attached to forks; check first!"))
						{
							// Force the forklift to detach the pallet currently attached to its forks.
							pForks->DetachPalletFromForks();
							// Force an update of the FSM so that the detachment happens immediately.
							pForks->ProcessControl(pForklift);
						}
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandRaiseHandlerFrame(int nVehicleId)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"RAISE_HANDLER_FRAME - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
						pFrame->SetDesiredAcceleration(1.0f);
						pFrame->SetDesiredHeight(5.6f);
						//pFrame->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = 0.0872666f; // About 5 degrees.
						// Handler needs to be awake to drive the frame to the desired pose.
						pHandler->ActivatePhysics();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandLowerHandlerFrame(int nVehicleId)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"LOWER_HANDLER_FRAME - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
						pFrame->SetDesiredAcceleration(-1.0f);

						if(pFrame->IsContainerAttached())
							pFrame->SetDesiredHeight(0.81564861f);
						else
							pFrame->SetDesiredHeight(0.0f);
						//pFrame->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = 0.0f;
						// Handler needs to be awake to drive the frame to the desired pose.
						pHandler->ActivatePhysics();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandSetHandlerFrameHeight(int nVehicleId, float fFrameHeightRatio)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"SET_HANDLER_FRAME_HEIGHT - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);

						if(SCRIPT_VERIFY(fFrameHeightRatio>=0.0f && fFrameHeightRatio<=1.0f, "SET_HANDLER_FRAME_HEIGHT - Height must be in range [0.0, 1.0]"))
						{
							// Compute an absolute joint position from the ratio provided by script.
							float fFrameHeight = fFrameHeightRatio*5.6f;
							//float fCarriageAngle = fFrameHeightRatio*0.0872666f;

							pFrame->SetDesiredHeight(fFrameHeight);
							//pFrame->GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = fCarriageAngle;
							// Handler needs to be awake to drive the frame to the desired pose.
							pHandler->ActivatePhysics();
						}
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandLockHandlerFrame(int nVehicleId)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"LOCK_HANDLER_FRAME - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
						pFrame->LockFrame();
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	bool CommandIsEntityAttachedToHandlerFrame(int nVehicleId, int nEntityIndex)
	{
		// Check for a specific container being attached to the handler.

		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"IS_ENTITY_ATTACHED_TO_HANDLER_FRAME - Should only be called on handler model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(SCRIPT_VERIFY(pEntity, "IS_ENTITY_ATTACHED_TO_HANDLER_FRAME - no entity found with given entity index")
					&& scriptVerifyf(pEntity->GetModelIndex()==MI_HANDLER_CONTAINER,
					"FIND_HANDLER_VEHICLE_CONTAINER_IS_ATTACHED_TO - Can only attach container prop (%s) directly",
					MI_HANDLER_CONTAINER.GetName().GetCStr()))
				{
					// See if we have a frame on this handler instance.
					for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
					{
						CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

						if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
						{
							CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);

							// Is this the container we are looking for?
							if(pFrame->GetAttachedObject()==pEntity)
							{
								return true;
							}
						}
					} // Loop over vehicle gadgets.
				}
			}
		}

		return false;
	}

	bool CommandIsAnyEntityAttachedToHandlerFrame(int nVehicleId)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"IS_ANY_ENTITY_ATTACHED_TO_HANDLER_FRAME - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
						//atArray<CPhysical*> const& attachedObjects = pFrame->GetListOfAttachedObjects();

						// Inform the caller if anything is attached to the frame.
						//return attachedObjects.GetCount() > 0;
						return pFrame->IsContainerAttached();
					}
				} // Loop over vehicle gadgets.
			}
		}

		return false;
	}

	int CommandFindHandlerVehicleContainerIsAttachedTo(int nEntityIndex)
	{
		// Find any handler vehicle to which this container is attached.
		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(nEntityIndex);
		if(pEntity)
		{
			if(scriptVerifyf(pEntity->GetModelIndex()==MI_HANDLER_CONTAINER,
				"FIND_HANDLER_VEHICLE_CONTAINER_IS_ATTACHED_TO - Can only attach container prop (%s) directly", MI_HANDLER_CONTAINER.GetName().GetCStr()))
			{
				for(s32 i=0; i < CVehicle::GetPool()->GetSize(); ++i)
				{
					CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(i);

					if(pVehicle && pVehicle->GetModelIndex()==MI_CAR_HANDLER)
					{
						// See if we have a frame on this handler instance.
						for(int j = 0; j < pVehicle->GetNumberOfVehicleGadgets(); ++j)
						{
							CVehicleGadget* pVehicleGadget = pVehicle->GetVehicleGadget(j);

							if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
							{
								CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
								if(pFrame->GetAttachedObject()==pEntity)
								{
									return CTheScripts::GetGUIDFromEntity(*pVehicle);
								}
							}
						} // Loop over vehicle gadgets.
					}
				}
			}
		}

		return NULL_IN_SCRIPTING_LANGUAGE;
	}

	bool CommandIsHandlerFrameLinedUpWithContainer(int nVehicleId, int nEntityIndex)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"IS_HANDLER_FRAME_LINED_UP_WITH_CONTAINER - Should only be called on handler model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(pEntity)
				{
					if(scriptVerifyf(pEntity->GetModelIndex()==MI_HANDLER_CONTAINER,
						"IS_HANDLER_FRAME_LINED_UP_WITH_CONTAINER - Can only attach container prop (%s) directly", MI_HANDLER_CONTAINER.GetName().GetCStr()))
					{
						// See if we have a frame on this handler instance.
						for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
						{
							CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

							if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
							{
								CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
								if(SCRIPT_VERIFY(!pFrame->IsContainerAttached(),
									"IS_HANDLER_FRAME_LINED_UP_WITH_CONTAINER - Handler already has a container attached."))
								{
									return pFrame->TestForGoodAttachmentOrientation(pHandler, pEntity);
								}
							}
						} // Loop over vehicle gadgets.
					}
				}
			}
		}

		return false;
	}

	void CommandAttachContainerToHandlerFrame(int nVehicleId, int nEntityIndex)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"ATTACH_CONTAINER_TO_HANDLER_FRAME - Should only be called on handler model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(pEntity)
				{
					if(scriptVerifyf(pEntity->GetModelIndex()==MI_HANDLER_CONTAINER,
						"ATTACH_CONTAINER_TO_HANDLER_FRAME - Can only attach container prop (%s) directly", MI_HANDLER_CONTAINER.GetName().GetCStr()))
					{
						// See if we have a frame on this handler instance.
						for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
						{
							CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

							if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
							{
								CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
								if(SCRIPT_VERIFY(!pFrame->IsContainerAttached(),
									"ATTACH_CONTAINER_TO_HANDLER_FRAME - objects already attached to frame; detach them first."))
								{
									// Figure out the best position to attach the container.
									int nBoneIndex = pHandler->GetBoneIndex(VEH_HANDLER_FRAME_2);
									if(SCRIPT_VERIFY(nBoneIndex > -1, "ATTACH_CONTAINER_TO_HANDLER_FRAME - couldn't find attach bone."))
									{
										// Force the Handler to attach the container to its frame.
										pFrame->AttachContainerInstanceToFrame(pEntity);
									}
								}
							}
						} // Loop over vehicle gadgets.
					}
				}
			}
		}
	}

	void CommandAttachContainerToHandlerFrameWhenLinedUp(int nVehicleId, int nEntityIndex)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"ATTACH_CONTAINER_TO_HANDLER_FRAME_WHEN_LINED_UP - Should only be called on handler model"))
			{
				CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex);
				if(pEntity)
				{
					if(scriptVerifyf(pEntity->GetModelIndex()==MI_HANDLER_CONTAINER,
						"ATTACH_CONTAINER_TO_HANDLER_FRAME_WHEN_LINED_UP - Can only attach container prop (%s) directly", MI_HANDLER_CONTAINER.GetName().GetCStr()))
					{
						// See if we have a frame on this handler instance.
						for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
						{
							CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

							if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
							{
								CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
								if(SCRIPT_VERIFY(!pFrame->IsContainerAttached(),
									"ATTACH_CONTAINER_TO_HANDLER_FRAME_WHEN_LINED_UP - objects already attached to frame; detach them first."))
								{
									// Figure out the best position to attach the container.
									int nBoneIndex = pHandler->GetBoneIndex(VEH_HANDLER_FRAME_2);
									if(SCRIPT_VERIFY(nBoneIndex > -1, "ATTACH_CONTAINER_TO_HANDLER_FRAME_WHEN_LINED_UP - couldn't find attach bone."))
									{
										// Force the Handler to attach the container to its frame.
										pFrame->AttachContainerInstanceToFrameWhenLinedUp(pEntity);
									}
								}
							}
						} // Loop over vehicle gadgets.
					}
				}
			}
		}
	}

	void CommandDetachContainerFromHandlerFrame(int nVehicleId)
	{
		CVehicle* pHandler = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleId);

		if(pHandler)
		{
			if(SCRIPT_VERIFY(pHandler->GetModelIndex()==MI_CAR_HANDLER,"DETACH_CONTAINER_FROM_HANDLER_FRAME - Should only be called on handler model"))
			{
				// See if we have a frame on this handler instance.
				for(int i = 0; i < pHandler->GetNumberOfVehicleGadgets(); ++i)
				{
					CVehicleGadget* pVehicleGadget = pHandler->GetVehicleGadget(i);

					if(pVehicleGadget->GetType() == VGT_HANDLER_FRAME)
					{
						CVehicleGadgetHandlerFrame* pFrame = static_cast<CVehicleGadgetHandlerFrame*>(pVehicleGadget);
						if(SCRIPT_VERIFY(pFrame->IsContainerAttached(),
							"DETACH_CONTAINER_FROM_HANDLER_FRAME - no objects attached to frame; check first!"))
						{
							// Force the Handler to detach the container currently attached to its frame.
							pFrame->DetachContainerFromFrame();
							// Force an update of the FSM so that the detachment happens immediately.
							pFrame->ProcessControl(pHandler);
						}
					}
				} // Loop over vehicle gadgets.
			}
		}
	}

	void CommandLowerConvertibleRoof(int nVehicleID, bool bInstantlyLowerRoof)
    {
        CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);

        if(pVehicle)
        {
            scriptAssertf(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation(), "%s:LOWER_CONVERTIBLE_ROOF - %s is not a convertible", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetModelName());
            pVehicle->LowerConvertibleRoof(bInstantlyLowerRoof);
        }
    }

    void CommandRaiseConvertibleRoof(int nVehicleID, bool bInstantlyRaiseRoof)
    {
        CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);

        if(pVehicle)
        {
           scriptAssertf(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation(), "%s:RAISE_CONVERTIBLE_ROOF - %s is not a convertible", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetModelName());
           pVehicle->RaiseConvertibleRoof(bInstantlyRaiseRoof);
        }
    }

    s32 CommandGetConvertibleRoofState(int nVehicleID)
    {
        const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(nVehicleID);

        if(pVehicle)
        {
           return pVehicle->GetConvertibleRoofState();
        }

        return CTaskVehicleConvertibleRoof::STATE_RAISED;
    }

    bool CommandIsVehicleAConvertible(int iVehicleIndex, bool bCheckRoofExtras)
    {
		bool bIsConvertible = false;
        const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);
        if(pVehicle)
        {
            if( pVehicle->DoesVehicleHaveAConvertibleRoofAnimation() )
            {
                bIsConvertible = true;
            }
			else if(bCheckRoofExtras)
			{
				bIsConvertible = pVehicle->CarHasRoof(true);
			}
        }

        return bIsConvertible;
    }

	void CommandTransformToSubmarine(int nVehicleID, bool bInstantlyTransform )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);

		if( pVehicle )
		{
			pVehicle->TransformToSubmarine( bInstantlyTransform );
		}
	}

	void CommandTransformToCar(int nVehicleID, bool bInstantlyTransform )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID);

		if( pVehicle )
		{
			pVehicle->TransformToCar( bInstantlyTransform );
		}
	}

	bool CommandGetIsInSubmarineMode( int nVehicleID )
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pVehicle )
		{
			return pVehicle->IsInSubmarineMode();
		}

		return false;
	}

	bool CommandIsThisModelABoat(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetIsBoat())
		{
			LatestCmpFlagResult = true;
		}

		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAJetski(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetIsBoat() 
			&& ((CVehicleModelInfo*)pModelInfo)->GetIsJetSki())
		{
			LatestCmpFlagResult = true;
		}

		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAPlane(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAHeli(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_HELI)
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelACar(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
		&& (((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_CAR || 
			((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
			((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR))
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}


	bool CommandIsThisModelATrain(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
		&& ((CVehicleModelInfo*)pModelInfo)->GetIsTrain())
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelABike(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if (pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
		&& ( ((CVehicleModelInfo*)pModelInfo)->GetIsBike()) )
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelABicycle(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if (pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
		&& ( ((CVehicleModelInfo*)pModelInfo)->GetVehicleType() == VEHICLE_TYPE_BICYCLE) )
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAQuadBike(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if (pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
		&& (((CVehicleModelInfo*)pModelInfo)->GetIsQuadBike() || ((CVehicleModelInfo*)pModelInfo)->GetIsAmphibiousQuad()))
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAnAmphibiousCar(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsThisModelAnAmphibiousQuadBike(int VehicleModelHashKey)
	{
		bool LatestCmpFlagResult = false;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

		if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE
			&& ((CVehicleModelInfo*)pModelInfo)->GetIsAmphibiousQuad())
		{
			LatestCmpFlagResult = true;
		}
		return LatestCmpFlagResult;
	}

	void SetHeliBladesSpeed(int HeliIndex, float fSpeedRatio)
	{
		fSpeedRatio = Clamp(fSpeedRatio, 0.0f, 1.0f);
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(HeliIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if(pVehicle)
		{
			if (SCRIPT_VERIFYF(pVehicle->GetIsRotaryAircraft() || pVehicle->InheritsFromPlane(), 
				"SET_HELI_BLADES_FULL_SPEED/SET_HELI_BLADES_SPEED - Vehicle %s type %i is not an aircraft", pVehicle->GetDebugName(), pVehicle->GetVehicleType()))
			{
				if(pVehicle->GetIsRotaryAircraft())
				{
					((CRotaryWingAircraft*) pVehicle)->SetMainRotorSpeed(MAX_ROT_SPEED_HELI_BLADES * fSpeedRatio);

					if(pVehicle->InheritsFromAutogyro())
					{
						static_cast<CAutogyro*>(pVehicle)->SetPropellerSpeed(fSpeedRatio);
					}
				}
				else if(pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
				{
					((CPlane*) pVehicle)->SetEngineSpeed(CPlane::ms_fMaxEngineSpeed * fSpeedRatio);
				}

				if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
				{
					static_cast<audHeliAudioEntity*>(pVehicle->GetVehicleAudioEntity())->SetHeliShouldSkipStartup(true);
				}	
				else if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
				{
					static_cast<audPlaneAudioEntity*>(pVehicle->GetVehicleAudioEntity())->ResetEngineSpeedSmoother();
				}
			}
		}
	}

	void CommandSetHeliBladesFullSpeed(int HeliIndex)
	{
		SetHeliBladesSpeed(HeliIndex, 1.0f);
	}

	void CommandForceSubThrottleForTime(int SubIndex, float in_Throttle, float in_Seconds)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(SubIndex);
		if(pVehicle)
		{
			CSubmarineHandling* subHandling = pVehicle->GetSubHandling();
			if (SCRIPT_VERIFY(subHandling, "FORCE_SUB_THROTTLE_FOR_TIME - Vehicle is not an submarine"))
			{
				subHandling->ForceThrottleForTime(Clamp(in_Throttle, 0.0f, 1.0f), in_Seconds);
			}
		}
	}

	void CommandSetHeliBladesSpeed(int HeliIndex, float fSpeedRatio)
	{
		SetHeliBladesSpeed(HeliIndex, fSpeedRatio);
	}

	void CommandSetVehicleCanBeTargetted(int VehicleIndex, bool CanBeTargettedFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			if (CanBeTargettedFlag)
			{
				pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted = true;
			}
			else
			{
				pVehicle->m_nVehicleFlags.bVehicleCanBeTargetted = false;
			}
		}
	}

	void CommandSetDontAllowPlayerToEnterVehicleIfLockedForPlayer(int VehicleIndex, bool DontAllowPlayerToEnter)
	{
		// this script command is not approved for use in single player scripts
		if (SCRIPT_VERIFY(NetworkInterface::IsGameInProgress(), "SET_DONT_ALLOW_PLAYER_TO_ENTER_VEHICLE_IF_LOCKED_FOR_PLAYER - This script command is not allowed in single player game scripts!"))
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pVehicle)
			{
                if(NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
                {
                    CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, pVehicle->GetNetworkObject());

                    if(netObjVehicle)
                    {
                        netObjVehicle->SetPendingDontTryToEnterIfLockedForPlayer(DontAllowPlayerToEnter);
                    }
                }
                else
                {
				    if (DontAllowPlayerToEnter)
				    {
					    pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = true;
				    }
				    else
				    {
					    pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = false;
				    }
                }
			}
		}
	}

	void CommandSetVehicleCanBeVisiblyDamaged(int VehicleIndex, bool VisibleDamageFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bCanBeVisiblyDamaged = (u8)VisibleDamageFlag;
		}
	}

	void CommandSetVehicleHasUnbreakableLights(int VehicleIndex, bool flag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bUnbreakableLights = (u8)flag;
		}
	}

    void CommandSetVehicleRespectsLocksWhenHasDriver(int VehicleIndex, bool flag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bRespectLocksWhenHasDriver = (u8)flag;
		}
	}

	void CommandSetCanEjectPassengersIfLocked(int VehicleIndex, bool flag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pVehicle)
		{
			pVehicle->SetCanEjectPassengersIfLocked(flag);
		}
	}

	float CommandGetVehicleDirtLevel(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			return pVehicle->GetBodyDirtLevel();
		}
		return 0.0f;
	}


	void CommandSetVehicleDirtLevel(int VehicleIndex, float DirtLevel)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (SCRIPT_VERIFY((DirtLevel >= 0.0f) && (DirtLevel <= 15.0f), "SET_VEHICLE_DIRT_LEVEL - Dirt level should be between 0.0 and 15.0"))
			{
				pVehicle->SetBodyDirtLevel(DirtLevel);
			}
		}
	}

	bool CommandDoesVehicleHaveDamageDecals(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			u32 decalTypeFlags = (1<<DECALTYPE_BANG) | 
								 (1<<DECALTYPE_SCUFF) | 
								 (1<<DECALTYPE_WEAPON_IMPACT) | 
								 (1<<DECALTYPE_WEAPON_IMPACT_SHOTGUN) | 
								 (1<<DECALTYPE_WEAPON_IMPACT_SMASHGROUP) | 
								 (1<<DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN) | 
								 (1<<DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE) | 
								 (1<<DECALTYPE_WEAPON_IMPACT_VEHICLE);

		 	decalBucket* pBucket = g_decalMan.FindFirstBucket(decalTypeFlags, pVehicle);
			if (pBucket)
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsVehicleDoorFullyOpen(int VehicleIndex, int DoorNumber)
	{
		bool LatestCmpFlagResult = false;

		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			const CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(pDoor)
			{
				LatestCmpFlagResult = pDoor->GetIsFullyOpen();
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandSetFreebiesInVehicle(int VehicleIndex, bool ContainsFreebiesFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (ContainsFreebiesFlag)
			{
				pVehicle->m_nVehicleFlags.bFreebies = true;
			}
			else
			{
				pVehicle->m_nVehicleFlags.bFreebies = false;
			}
		}
	}

	void CommandSetRocketLauncherFreebieInHeli(bool /*bFlag*/)
	{
#if 0 // JG
		CPickups::m_bRocketLauncherFreebieInHeli = bFlag;
#endif // 0
	}

	void CommandSetVehicleEngineOn(int VehicleIndex, bool EngineOnFlag, bool bNoDelay, bool bOnlyStartWithPlayerInput)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if(!bNoDelay)
			{
				// We do this to allow for cutscenes to shut down heli engines without having to animate them, but only if noDelay flag is false
				pVehicle->SetAnimatePropellers(false); 
			}
			if (EngineOnFlag)
			{
				scriptAssertf(!pVehicle->m_nVehicleFlags.bNotDriveable, "Vehicle has been set to not be driveable");
				pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
				pVehicle->SwitchEngineOn(bNoDelay);
				pVehicle->m_bOnlyStartVehicleEngineOnThrottle = false;
				if(pVehicle->InheritsFromPlane())
				{
					((CPlane *)pVehicle)->ResetEngineTurnedOffByScript();
				}
			}
			else
			{
				pVehicle->SwitchEngineOff();
				pVehicle->m_bOnlyStartVehicleEngineOnThrottle = bOnlyStartWithPlayerInput;
				if(pVehicle->InheritsFromPlane())
				{
					((CPlane *)pVehicle)->TurnEngineOffFromScript();
				}
			}
		}
	}

	void CommandSetVehicleUnDriveable(int VehicleIndex, bool UndriveableFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			if (UndriveableFlag)
			{
				pVehicle->m_nVehicleFlags.bNotDriveable = true;
				pVehicle->SwitchEngineOff();
			}
			else
			{
				pVehicle->m_nVehicleFlags.bNotDriveable = false;
			}
		}
	}

	void CommandSetFreeResprays(bool FreeRespraysFlag)
	{
		CGarages::RespraysAreFree = FreeRespraysFlag;
	}

	void CommandSetVehicleProvidesCover(int VehicleIndex, bool ProvidesCoverFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bDoesProvideCover = ProvidesCoverFlag;
		}
	}

// This must be matching with ENUM DOOR_DAMAGE in commands_vehicle.sch
enum DOOR_DAMAGE_SCRIPT_STATE {
	DT_DOOR_INTACT = 0,
	DT_DOOR_SWINGING_FREE,              //  1
	DT_DOOR_BASHED,                     //  2
	DT_DOOR_BASHED_AND_SWINGING_FREE,   //  3
	DT_DOOR_MISSING,                    //  4
	DT_DOOR_NO_RESET					//  5
};

void CommandSetVehicleDoorControl(int VehicleIndex, int DoorNumber, int DoorStatus, float AngleRatio)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "SET_VEHICLE_DOOR_CONTROL - vehicle must have door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(pDoor)
			{
				if(DoorStatus == DT_DOOR_INTACT && !pDoor->GetIsIntact(pVehicle))
				{
					pDoor->Fix(pVehicle);
				}

				if(pDoor->GetIsIntact(pVehicle))
				{
#if __BANK					
					if(CCarDoor::sm_debugDoorPhysics)
					{
						Printf("CommandSetVehicleDoorControl - [%s] Script setting door target ratio: %f .\n", pVehicle->GetDebugName(), AngleRatio);
					}
#endif

					if( DoorStatus == DT_DOOR_NO_RESET )
					{
						pDoor->SetTargetDoorOpenRatio(AngleRatio, CCarDoor::DRIVEN_NORESET | CCarDoor::DRIVEN_SPECIAL, pVehicle);
					}
					else
					{
						pDoor->SetTargetDoorOpenRatio(AngleRatio, CCarDoor::DRIVEN_AUTORESET | CCarDoor::DRIVEN_SPECIAL, pVehicle);
					}
				}
			}
		}
	}
}

void CommandSetVehicleDoorLatched(int VehicleIndex, int DoorNumber, bool bLatched, bool bWillAutoLatch, bool bApplyForceForDoorClosed)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "SET_VEHICLE_DOOR_LATCHED - vehicle must have door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
				// If the door is to be latched, drive it shut regardless of current status
				if(bLatched)
				{
					pDoor->SetShutAndLatched(pVehicle, bApplyForceForDoorClosed);
				}
				// Else we want to unlatch it
				else
				{			
					// If we are going to autolatch it then make sure the locking flags are set, otherwise un set them
					if(bWillAutoLatch)
					{
						pDoor->SetFlag(CCarDoor::WILL_LOCK_SWINGING);
					}
					else
					{
						pDoor->ClearFlag(CCarDoor::WILL_LOCK_SWINGING);
					}
					// Don't change its current angle, but free it so it can swing to other angles using DRIVEN_AUTORESET
					// DRIVEN_AUTORESET means the door wont be driven to specified angle forever
					pDoor->SetTargetDoorOpenRatio(pDoor->GetTargetDoorRatio(),CCarDoor::DRIVEN_AUTORESET, pVehicle);
				}
			}
		}
	}
}

void CommandSetConvertibleRoofLatchState(int VehicleIndex, bool bLatch)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (pVehicle)
    {
        if (SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "SET_CONVERTIBLE_ROOF_LATCH_STATE - must be an automobile"))
        {
            if(bLatch)
            {
                CTaskVehicleConvertibleRoof::LatchRoof(pVehicle);
            }
            else
            {
                CTaskVehicleConvertibleRoof::UnLatchRoof(pVehicle);
            }
        }
    }
}

float CommandGetVehicleModelEstimatedMaxSpeed(s32 VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_ESTIMATED_MAX_SPEED - failed to find model info") &&
		scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_ESTIMATED_MAX_SPEED - model is not a vehicle"))
	{
		CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
		{
			pVehicleModelInfo->CalculateMaxFlatVelocityEstimate(pHandling);
			return pHandling->m_fEstimatedMaxFlatVel;
		}
	}

	return 0.0f;
}

//--------------Vehicle Index Versions - Includes mods-----------------------------//

float CommandGetVehicleEstimatedMaxSpeed(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
    if (pVehicle)
    {
		if( pVehicle->pHandling && 
			pVehicle->pHandling->GetFlyingHandlingData() )
		{
			static float sfMaxSpeedModifier = 0.15f;
			float estimatedMaxSpeed = CommandGetVehicleModelEstimatedMaxSpeed( pVehicle->GetModelNameHash() );
			estimatedMaxSpeed += estimatedMaxSpeed * ( pVehicle->GetVariationInstance().GetEngineModifier() * 0.01f ) * sfMaxSpeedModifier;
			return estimatedMaxSpeed;
		}
		else
		{
			return pVehicle->m_Transmission.GetDriveMaxFlatVelocity();
		}
    }

    return 0.0f;
}

float CommandGetVehicleMaxBraking(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		if (pVehicle->pHandling && pVehicle->pHandling->GetFlyingHandlingData())
		{
			return pVehicle->pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY) * pVehicle->pHandling->m_fEstimatedMaxFlatVel * 0.01f;
		}

		return pVehicle->GetBrakeForce( true );
	} 

	return 0.0f;
}

float CommandGetVehicleMaxTraction(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle && pVehicle->pHandling)
	{
		float fTractionMult = 1.0f;

		if(pVehicle)
		{
			if( CWheel::ms_bUseExtraWheelDownForce &&				
				pVehicle->pHandling->m_fDownforceModifier > 0.0f )
			{
                if( pVehicle->pHandling->GetCarHandlingData() &&
                    pVehicle->pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
                {
                    fTractionMult += CWheel::ms_fDownforceMult * pVehicle->pHandling->m_fDownforceModifier * ( ( pVehicle->GetDownforceModifierFront() + pVehicle->GetDownforceModifierRear() ) * 0.5f );
                }
				else if(pVehicle->GetVariationInstance().HasSpoiler() &&
                    !pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_SPOILER_MOD_DOESNT_INCREASE_GRIP ) )
				{
					fTractionMult += (CWheel::ms_fDownforceMultSpoiler - CWheel::ms_fDownforceMult) * pVehicle->pHandling->m_fDownforceModifier;
				}
				else
				{
					fTractionMult += CWheel::ms_fDownforceMult * pVehicle->pHandling->m_fDownforceModifier;
				}
			}
			else
			{
				if(pVehicle->GetVariationInstance().HasSpoiler() &&
                    !pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_SPOILER_MOD_DOESNT_INCREASE_GRIP ) )
				{
					fTractionMult += (CWheel::ms_fDownforceMultSpoiler - CWheel::ms_fDownforceMult);
				}
			}

			if(pVehicle->m_bDriftTyres)
			{
				// NOTE: fTractionMult can end up here to some very high value for some cars
				// the point is to show drift tyres reduce traction. Value is only for UI purposes
				if( fTractionMult > 1.0f )
					fTractionMult = CWheel::GetReduceGripOnRimMultRear();
				else
					fTractionMult *= CWheel::GetReduceGripOnRimMultRear();
			}
		}

		return pVehicle->pHandling->m_fTractionCurveMax * fTractionMult;
	}

	return 0.0f;
}

static dev_float sfGearBoxAccelerationMult = 0.08f;// This is a guesstimate of the performance increase but should do the job.
float CommandGetVehicleAcceleration(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		if(pVehicle->pHandling && pVehicle->pHandling->GetFlyingHandlingData())
		{
			return pVehicle->pHandling->GetFlyingHandlingData()->m_fThrust * pVehicle->GetGravityForWheellIntegrator();
		}
		else
		{
			return (pVehicle->m_Transmission.GetDriveForce() + 
					(pVehicle->GetVariationInstance().IsToggleModOn(VMT_TURBO) ? CTransmission::GetTurboPowerModifier(pVehicle) : 0.0f)) *
					(1.0f + (sfGearBoxAccelerationMult * (pVehicle->GetVariationInstance().GetGearboxModifier()/100.0f)));
		}
	}

	return 0.0f;
}

//--------------Vehicle Model Hash Key Versions-----------------------------//

float CommandGetVehicleModelMaxTraction(s32 VehicleModelHashKey)
{
    CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

    if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_MAX_TRACTION - failed to find model info") &&
        scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_MAX_TRACTION - model is not a vehicle"))
    {
        CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
        if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
        {
            float maxTraction = pHandling->m_fTractionCurveMax;
            if( pHandling->GetCarHandlingData() &&
                pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
            {
                maxTraction += CWheel::ms_fDownforceMult * pHandling->m_fDownforceModifier * 2.0f;
            }

            return maxTraction;
        }
    }

    return 0.0f;
}

float CommandGetFlyingVehicleModelAgility(s32 VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_FLYING_VEHICLE_MODEL_AGILITY - failed to find model info") &&
		scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_MAX_TRACTION - model is not a vehicle"))
	{
		CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
		{
			if(pHandling->GetFlyingHandlingData())
			{
				//add yaw pitch and roll together and average them
				return (((-pHandling->GetFlyingHandlingData()->m_fYawMult)) + 
					(pHandling->GetFlyingHandlingData()->m_fRollMult) + 
					(pHandling->GetFlyingHandlingData()->m_fPitchMult)) * 
					pHandling->m_fEstimatedMaxFlatVel * (-GRAVITY) / 3.0f;
			}
		}
	}

	return 0.0f;
}

float CommandGetBoatVehicleModelAgility(s32 VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_BOAT_VEHICLE_MODEL_AGILITY - failed to find model info") &&
		scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_BOAT_VEHICLE_MODEL_AGILITY - model is not a vehicle"))
	{
		CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
		{
			if(pHandling->GetBoatHandlingData())
			{
				return pHandling->GetBoatHandlingData()->m_vecMoveResistance.GetXf();
			}
		}
	}

	return 0.0f;
}

float CommandGetVehicleModelAcceleration(s32 VehicleModelHashKey)
{
    CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

    if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_ACCELERATION - failed to find model info") &&
        scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_ACCELERATION - model is not a vehicle"))
    {
        CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
        if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
        {
			if(pHandling->GetFlyingHandlingData())
			{
				return pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY);
			}
			else
			{
				return pHandling->m_fInitialDriveForce;
			}
        }
    }

    return 0.0f;
}

float CommandGetVehicleModelAccelerationMaxMods(s32 VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_ACCELERATION_MAX_MODS - failed to find model info") &&
		scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_ACCELERATION_MAX_MODS- model is not a vehicle"))
	{
		CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
		{
			if(pHandling->GetFlyingHandlingData())
			{
				return pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY);
			}
			else
			{
				return pHandling->m_fInitialDriveForce + CVehicle::ms_fEngineVarianceMaxModifier + CTransmission::GetMaxTurboPowerModifier(pHandling) + sfGearBoxAccelerationMult;
			}
		}
	}

	return 0.0f;
}

float CommandGetVehicleModelMaxBraking(s32 VehicleModelHashKey)
{
    CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

    if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_MAX_BRAKING - failed to find model info") &&
        scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_MAX_BRAKING - model is not a vehicle"))
    {
        CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
        if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
        {
			if(pHandling->GetFlyingHandlingData())
			{
				return pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY) * pHandling->m_fEstimatedMaxFlatVel * 0.01f;
			}
			else
			{
	            return pHandling->m_fBrakeForce;
			}
        }
    }

    return 0.0f;
}

float CommandGetVehicleModelMaxBrakingMaxMods(s32 VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_VEHICLE_MODEL_MAX_BRAKING_MAX_MODS - failed to find model info") &&
		scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_VEHICLE_MODEL_MAX_BRAKING_MAX_MODS - model is not a vehicle"))
	{
		CVehicleModelInfo *pVehicleModelInfo = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (CHandlingData *pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId()))
		{
			if(pHandling->GetFlyingHandlingData())
			{
				return pHandling->GetFlyingHandlingData()->m_fThrust * (-GRAVITY) * pHandling->m_fEstimatedMaxFlatVel * 0.01f;
			}
			else
			{
				return pHandling->m_fBrakeForce + CVehicle::ms_fBrakeVarianceMaxModifier;
			}
		}
	}

	return 0.0f;
}

//--------------Vehicle Class Versions-----------------------------//

float CommandGetVehicleClassEstimatedMaxSpeed(int VehicleClass)
{
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo((eVehicleClass)VehicleClass);
	if (SCRIPT_VERIFY(pVehicleClassInfo, "GET_VEHICLE_CLASS_ESTIMATED_MAX_SPEED - no vehicle class info found"))
	{
		return pVehicleClassInfo->m_fMaxSpeedEstimate;
	}
	return 0.0f;
}

float CommandGetVehicleClassMaxTraction(int VehicleClass)
{
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo((eVehicleClass)VehicleClass);
	if (SCRIPT_VERIFY(pVehicleClassInfo, "GET_VEHICLE_CLASS_MAX_TRACTION - no vehicle class info found"))
	{
		return pVehicleClassInfo->m_fMaxTraction;
	}
	return 0.0f;
}

float CommandGetVehicleClassMaxAgility(int VehicleClass)
{
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo((eVehicleClass)VehicleClass);
	if (SCRIPT_VERIFY(pVehicleClassInfo, "GET_VEHICLE_CLASS_MAX_AGILITY - no vehicle class info found"))
	{
		return pVehicleClassInfo->m_fMaxAgility;
	}
	return 0.0f;
}

float CommandGetVehicleClassMaxAcceleration(int VehicleClass)
{
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo((eVehicleClass)VehicleClass);
	if (SCRIPT_VERIFY(pVehicleClassInfo, "GET_VEHICLE_CLASS_MAX_ACCELERATION - no vehicle class info found"))
	{
		return pVehicleClassInfo->m_fMaxAcceleration;
	}
	return 0.0f;
}

float CommandGetVehicleClassMaxBraking(int VehicleClass)
{
	VehicleClassInfo *pVehicleClassInfo = CVehicleModelInfo::GetVehicleClassInfo((eVehicleClass)VehicleClass);
	if (SCRIPT_VERIFY(pVehicleClassInfo, "GET_VEHICLE_CLASS_MAX_BRAKING - no vehicle class info found"))
	{
		return pVehicleClassInfo->m_fMaxBraking;
	}
	return 0.0f;
}

//--------------------------------------------------------------------------//

float CommandGetVehicleDoorTargetRatio(int VehicleIndex, int DoorNumber)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN || pVehicle->InheritsFromSubmarine()), "GET_DOOR_TARGET_ANGLE_RATIO - vehicle must have door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));
			const CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if (pDoor)
			{
				return pDoor->GetTargetDoorRatio();
			}
		}
	}
	return 0.0f;
}

float CommandGetVehicleDoorAngleRatio(int VehicleIndex, int DoorNumber)
{
	float ReturnDoorAngle = 0.0f;

	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN || pVehicle->InheritsFromSubmarine()), "GET_DOOR_ANGLE_RATIO - vehicle must have door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			const CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(pDoor)
			{
				ReturnDoorAngle = pDoor->GetDoorRatio();
			}
			else
			{
				ReturnDoorAngle = 0.0f;
			}
		}
	}
	return ReturnDoorAngle;
}

int CommandGetPedUsingVehicleDoor(int VehicleIndex, int DoorNumber)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	int ReturnPedIndex = NULL_IN_SCRIPTING_LANGUAGE;

	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "SET_VEHICLE_DOOR_LATCHED - vehicle must have door"))
		{
			// Get script requested door
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));
			const CCarDoor* pRequestedDoor = pVehicle->GetDoorFromId(nDoorId);

			const CModelSeatInfo* pModelSeatinfo = pVehicle->GetVehicleModelInfo() ? pVehicle->GetVehicleModelInfo()->GetModelSeatInfo() : NULL;
			if (pModelSeatinfo)
			{
				// Loop through all entry points on vehicle
				const s32 iNumEntryPoints = pModelSeatinfo->GetNumberEntryExitPoints();
				for (s32 i = 0; i < iNumEntryPoints; i++)
				{
					// Get door for entry point, see if it's a match
					const CCarDoor* pEntryPointDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, i);
					if (pRequestedDoor == pEntryPointDoor)
					{
						// Check if the door for this entry point is reserved
						CComponentReservation* pComponentReservation = const_cast<CVehicle*>(pVehicle)->GetComponentReservationMgr()->GetDoorReservation(i);
						if (pComponentReservation)
						{
							CPed* pPed = pComponentReservation->GetPedUsingComponent();
							if (pPed)
							{
								ReturnPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
								break;
							}
						}
					}
				}
			}
		}
	}
	return ReturnPedIndex;
}

void CommandSetVehicleDoorShut(int VehicleIndex, int DoorNumber, bool ShutInstantly)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromSubmarine() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "SET_VEHICLE_DOOR_SHUT - vehicle must have door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
                if(ShutInstantly)
                {	
                    pDoor->SetShutAndLatched(pVehicle, false);
                }
                else
                {
                    pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::WILL_LOCK_DRIVEN, pVehicle);
                }

				audVehicleAudioEntity* vehicleAudioEntity = pVehicle->GetVehicleAudioEntity();

				if(vehicleAudioEntity)
				{
					vehicleAudioEntity->OnScriptVehicleDoorShut();
				}
			}
		}
	}
}

void CommandSetVehicleDoorBroken(int VehicleIndex, int DoorNumber, bool bDissapear)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "%s:SET_VEHICLE_DOOR_BROKEN - vehicle must have door"))
		{

			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));
			CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
			fragInst* pFragInst = pVehicle->GetFragInst();

			if(pFragInst && pDoor && pDoor->GetIsIntact(pVehicle))
			{
				if(bDissapear || !pVehicle->CarPartsCanBreakOff())
				{
					if(pDoor->GetFragChild() > 0)
						pFragInst->DeleteAbove(pDoor->GetFragChild());
				}
				else
				{
					if(pDoor->GetIsLatched(pVehicle))
					{
						pDoor->BreakLatch(pVehicle);
					}
					Assertf(pVehicle->GetFragInst() == NULL || !pVehicle->GetFragInst()->IsBreakingDisabled(), "This command will have no effect with breaking disabled. Call SET_VEHICLE_CAN_BREAK first");
					pDoor->BreakOff(pVehicle);
				}
			}
		}
	}
}

void CommandSetVehicleUsesLargeRearRamp(int VehicleIndex, bool bUse)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromPlane()), "%s:SET_VEHICLE_USES_LARGE_REAR_RAMP - vehicle must be a plane"))
		{
			pVehicle->m_nVehicleFlags.bUsesLargeRearRamp = bUse;
		}
	}
}

void CommandSetVehicleWingBroken(int VehicleIndex, bool rightWing, bool bDissapear)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (pVehicle)
    {
        if (SCRIPT_VERIFY((pVehicle->InheritsFromPlane()), "%s:SET_VEHICLE_WING_BROKEN - vehicle must have wings"))
		{
			Assertf(pVehicle->GetFragInst() == NULL || !pVehicle->GetFragInst()->IsBreakingDisabled(), "This command will have no effect with breaking disabled. Call SET_VEHICLE_CAN_BREAK first");
			CPlane *pPlane = (CPlane *)pVehicle;
			CAircraftDamage::eAircraftSection eSection = rightWing ? CAircraftDamage::WING_R : CAircraftDamage::WING_L;
			pPlane->GetAircraftDamage().SetSectionHealth(eSection, 0.0f);
			pPlane->GetAircraftDamage().BreakOffSection(pPlane, eSection, bDissapear);
        }
    }
}

void CommandSetVehicleRudderBroken(int VehicleIndex, bool bDissapear)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromPlane()), "%s:SET_VEHICLE_RUDDER_BROKEN - vehicle must have rudder"))
		{
			Assertf(pVehicle->GetFragInst() == NULL || !pVehicle->GetFragInst()->IsBreakingDisabled(), "This command will have no effect with breaking disabled. Call SET_VEHICLE_CAN_BREAK first");
			CPlane *pPlane = (CPlane *)pVehicle;
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::RUDDER, 0.0f);
			pPlane->GetAircraftDamage().BreakOffSection(pPlane, CAircraftDamage::RUDDER, bDissapear);
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::RUDDER_2, 0.0f);
			pPlane->GetAircraftDamage().BreakOffSection(pPlane, CAircraftDamage::RUDDER_2, bDissapear);
		}
	}
}

void CommandSetVehicleTailBroken(int VehicleIndex, bool bDissapear)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		Assertf(pVehicle->GetFragInst() == NULL || !pVehicle->GetFragInst()->IsBreakingDisabled(), "This command will have no effect with breaking disabled. Call SET_VEHICLE_CAN_BREAK first");
		if (pVehicle->InheritsFromPlane())
		{
			CPlane *pPlane = (CPlane *)pVehicle;
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::TAIL, 0.0f);
			pPlane->GetAircraftDamage().BreakOffSection(pPlane, CAircraftDamage::TAIL, bDissapear);
		}
		else if(pVehicle->InheritsFromHeli())
		{
			CHeli *pHeli = (CHeli *)pVehicle;
			pHeli->SetCanBreakOffTailBoom(true);
			pHeli->BreakOffTailBoom();
		}
		else
		{
			SCRIPT_ASSERT(false, "%s:SET_VEHICLE_TAIL_BROKEN - vehicle must have tail");
		}
	}
}

void CommandSetVehicleCanBreak(int VehicleIndex, bool bBreaking)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN), "SET_VEHICLE_CAN_BREAK - vehicle must have door"))
		{
			fragInstGta* pFragInst = static_cast<fragInstGta*>(pVehicle->GetFragInst());
			if(pFragInst)
			{
				pFragInst->SetDisableBreakable(!bBreaking);
			}
		}
	}
}

void CommandSetRemovalRadiusAndNumberForCarjackingMission(int iNumVehicles, float fRemovalRadius)
{
	CVehiclePopulation::ms_iNumVehiclesInRangeForAggressiveRemovalCarjackMission = iNumVehicles;
	CVehiclePopulation::ms_iRadiusForAggressiveRemoalCarjackMission	= fRemovalRadius;
}

void CommandSetRemoveAggressivelyForCarJackingMission(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->GetIntelligence()->m_bRemoveAggressivelyForCarjackingMission = true;
	}
}

void CommandSetSwerveAroundPlayerVehicleForRiotVanMission(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->GetIntelligence()->m_bSwerveAroundPlayerVehicleForRiotVanMission = true;
	}
}

bool CommandDoesVehicleHaveRoof(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	bool bReturnValue = false;

	if (pVehicle)
	{
		if (pVehicle->CarHasRoof())
		{
			bReturnValue = true;
		}
	}

	return bReturnValue;
}

bool CommandIsBigVehicle(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return ((bool) pVehicle->m_nVehicleFlags.bIsBig);
	}
	return false;
}

int CommandGetNumberOfVehicleModelColours(s32 VehicleModelHashKey)
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if (scriptVerifyf(pModelInfo, "GET_NUMBER_OF_VEHICLE_MODEL_COLOURS - failed to find model info"))
	{
		if (scriptVerifyf(pModelInfo->GetModelType()==MI_TYPE_VEHICLE, "GET_NUMBER_OF_VEHICLE_MODEL_COLOURS - model is not a vehicle"))
		{
			return ((CVehicleModelInfo*) pModelInfo)->GetNumPossibleColours();
		}
	}

	return 0;
}

int CommandGetNumberOfVehicleColours(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
		if (SCRIPT_VERIFY(pModelInfo, "GET_NUM_VEHICLE_COLOURS - ModelInfo doesn't exist"))
		{
			if (SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "GET_NUM_VEHICLE_COLOURS - ModelInfo isn't a vehicle"))
			{
				return pModelInfo->GetNumPossibleColours();
			}
		}
	}
	return 0;
}

void CommandSetVehicleColourCombination(int VehicleIndex, int ColourCombination)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
		if (SCRIPT_VERIFY(pModelInfo, "SET_VEHICLE_COLOUR_COMBINATION - ModelInfo doesn't exist"))
		{
			if (SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "SET_VEHICLE_COLOUR_COMBINATION - ModelInfo isn't a vehicle"))
			{

				u8	col1 = 0;
				u8	col2 = 0;
				u8	col3 = 0;
				u8	col4 = 0;
				u8	col5 = 0;
				u8	col6 = 0;

				if (ColourCombination < 0 || ColourCombination >= pModelInfo->GetNumPossibleColours())
				{
#if __ASSERT
					char ErrorMsg[256];
					sprintf(ErrorMsg, "SET_VEHICLE_COLOUR_COMBINATION- This vehicle doesn't have that many colours");
					scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ErrorMsg);
#endif
				} 
				else
				{
					col1 = pModelInfo->GetPossibleColours(0, ColourCombination);
					col2 = pModelInfo->GetPossibleColours(1, ColourCombination);
					col3 = pModelInfo->GetPossibleColours(2, ColourCombination);
					col4 = pModelInfo->GetPossibleColours(3, ColourCombination);
					col5 = pModelInfo->GetPossibleColours(4, ColourCombination);
					col6 = pModelInfo->GetPossibleColours(5, ColourCombination);
				}

				pVehicle->SetBodyColour1(col1);
				pVehicle->SetBodyColour2(col2);
				pVehicle->SetBodyColour3(col3);
				pVehicle->SetBodyColour4(col4);
				pVehicle->SetBodyColour5(col5);
				pVehicle->SetBodyColour6(col6);

				pVehicle->UpdateBodyColourRemapping();		// let shaders know, that body colours changed
			}
		}
	}
}

int CommandGetVehicleColourCombination(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
		if (SCRIPT_VERIFY(pModelInfo, "GET_VEHICLE_COLOUR_COMBINATION - ModelInfo doesn't exist"))
		{
			if (SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "GET_VEHICLE_COLOUR_COMBINATION - ModelInfo isn't a vehicle"))
			{

				u8 col1 = pVehicle->GetBodyColour1();
				u8 col2 = pVehicle->GetBodyColour2();
				u8 col3 = pVehicle->GetBodyColour3();
				u8 col4 = pVehicle->GetBodyColour4();
				u8 col5 = pVehicle->GetBodyColour5();
				u8 col6 = pVehicle->GetBodyColour6();

				for (s32 i = 0; i < pModelInfo->GetNumPossibleColours(); ++i)
				{
					if (pModelInfo->GetPossibleColours(0, i) == col1 &&
						pModelInfo->GetPossibleColours(1, i) == col2 &&
						pModelInfo->GetPossibleColours(2, i) == col3 &&
						pModelInfo->GetPossibleColours(3, i) == col4 &&
						pModelInfo->GetPossibleColours(4, i) == col5 &&
						pModelInfo->GetPossibleColours(5, i) == col6 )
					{
						return i;
					}
				}
			}
		}
	}

	return -1;
}


void CommandSetVehicleXenonLightColorIdx(int VehicleIndex, int xenonLightColorIdx)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
	#if __ASSERT
		scriptAssertf((xenonLightColorIdx<CVehicleModelInfo::GetVehicleColours()->m_XenonLightColors.GetCount()) || (xenonLightColorIdx==0xFF), "SET_VEHICLE_XENON_LIGHT_COLOR_INDEX: light index %d is too big (max: %d),", xenonLightColorIdx, CVehicleModelInfo::GetVehicleColours()->m_XenonLightColors.GetCount()-1);
	#endif

		pVehicle->SetXenonLightColor((u8)xenonLightColorIdx);
	}
}

int CommandGetVehicleXenonLightColorIdx(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return pVehicle->GetXenonLightColor();
	}

	return 0xff; // invalid
}

void CommandSetPlaybackToUseAi(int VehicleIndex, int iDrivingFlags)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
#if __ASSERT
		scriptAssertf(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVehicle), "%s: SET_PLAYBACK_TO_USE_AI - vehicle not using a car recording, call IS_PLAYBACK_GOING_ON_FOR_VEHICLE first", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(pVehicle->GetDriver(), "%s: SET_PLAYBACK_TO_USE_AI. car doesn't have a driver", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __ASSERT


		CVehicleRecordingMgr::ChangeCarPlaybackToUseAI(pVehicle, 0, iDrivingFlags);
	}
}

void CommandSetPlaybackToUseAiTryToRevertBackLater(int VehicleIndex, s32 delayUntilRevertingBack, int iDrivingFlags, bool bSnapToPositionIfNotVisible)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
#if __ASSERT
		scriptAssertf(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVehicle), "%s: SET_PLAYBACK_TO_USE_AI_TRY_TO_REVERT_BACK_LATER - vehicle not using a car recording, call IS_PLAYBACK_GOING_ON_FOR_VEHICLE first", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(pVehicle->GetDriver(), "%s: SET_PLAYBACK_TO_USE_AI_TRY_TO_REVERT_BACK_LATER. car doesn't have a driver", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __ASSERT
		CVehicleRecordingMgr::ChangeCarPlaybackToUseAI(pVehicle, rage::Max((u32)1, ((u32)delayUntilRevertingBack)), iDrivingFlags, bSnapToPositionIfNotVisible);
	}
}

scrVector CommandGetPositionOfVehicleRecordingIdAtTime(int index, float time)
{
	Vector3 retVal;

	CVehicleRecordingMgr::GetPositionOfCarRecordingAtTime(index, time, retVal);

	return retVal;
}

scrVector CommandGetPositionOfVehicleRecordingAtTime(int iRecordingNumber, float time, const char* pRecordingName)
{
	int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(iRecordingNumber, pRecordingName);
	if(scriptVerifyf(index != -1, "GET_POSITION_OF_VEHICLE_RECORDING_AT_TIME: %s%d vehicle recording doesn't exist", pRecordingName, iRecordingNumber))
	{
		return CommandGetPositionOfVehicleRecordingIdAtTime(index, time);
	}
	return scrVector();
}

scrVector CommandGetRotationOfVehicleRecordingIdAtTime(int index, float time)
{
	Vector3 retVal;

	CVehicleRecordingMgr::GetRotationOfCarRecordingAtTime(index, time, retVal);

	return retVal;
}

scrVector CommandGetRotationOfVehicleRecordingAtTime(int iRecordingNumber, float time, const char* pRecordingName)
{
	int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(iRecordingNumber, pRecordingName);
	if(scriptVerifyf(index != -1, "GET_ROTATION_OF_VEHICLE_RECORDING_AT_TIME: %s%d vehicle recording doesn't exist", pRecordingName, iRecordingNumber))
	{
		return CommandGetRotationOfVehicleRecordingIdAtTime(index, time);
	}
	return scrVector();
}

float CommandGetTotalDurationOfVehicleRecordingId(int index)
{
	return CVehicleRecordingMgr::GetTotalDurationOfCarRecording(index);
}

float CommandGetTotalDurationOfVehicleRecording(int iRecordingNumber, const char* pRecordingName)
{
	int index = CVehicleRecordingMgr::FindIndexWithFileNameNumber(iRecordingNumber, pRecordingName);
	if(scriptVerifyf(index != -1, "GET_TOTAL_DURATION_OF_VEHICLE_RECORDING. Cannot find recording #: %d name: %s - it is likely not streamed in", iRecordingNumber, pRecordingName))
	{
		return CommandGetTotalDurationOfVehicleRecordingId(index);
	}

	return 0.0f;
}


void CommandSetVehicleIsConsideredByPlayer(int VehicleIndex, bool ConsideredByPlayerFlag)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_IS_CONSIDERED_BY_PLAYER - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle->m_nVehicleFlags.bConsideredByPlayer = ConsideredByPlayerFlag;
	}
}

void CommandSetVehicleWillForceOtherVehiclesToStop(int VehicleIndex, bool ForceOtherVehiclesToStop)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_WILL_FORCE_OTHER_VEHICLES_TO_STOP - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = ForceOtherVehiclesToStop;
	}
}

void CommandSetVehicleActAsIfHasSirenOn(int VehicleIndex, bool bActAsIfSirenOn)
{
	CVehicle* pVehicle =CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_ACT_AS_IF_HAS_SIREN_ON - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle->m_nVehicleFlags.bActAsIfHasSirenOn = bActAsIfSirenOn;
	}
}

void CommandSetVehicleWillTellOthersToHurry(int VehicleIndex, bool bTellOthersToHurry)
{
	CVehicle* pVehicle =CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_WILL_TELL_OTHERS_TO_HURRY - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle->m_nVehicleFlags.bTellOthersToHurry = bTellOthersToHurry;
	}
}

void CommandSetVehicleUseMoreRestrictiveSpawnChecks(int VehicleIndex, bool UseMoreRestrictiveSpawnChecks)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_USE_MORE_RESTRICTIVE_SPAWN_CHECKS - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle->m_nVehicleFlags.bOtherCarsShouldCheckTrajectoryBeforeSpawning = UseMoreRestrictiveSpawnChecks;
	}
}

void CommandSetVehicleMayBeUsedByGoToPointAnyMeans(int iVehicleIndex, bool bState)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_MAY_BE_USED_BY_GOTO_POINT_ANY_MEANS - Invalid Vehicle - VehicleIndex not found."))
	{
		if(SCRIPT_VERIFY(!IsPopTypeRandomNonPermanent(pVehicle->PopTypeGet()), "SET_VEHICLE_MAY_BE_USED_BY_GOTO_POINT_ANY_MEANS - vehicle must be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans = bState;
		}
	}
}

void CommandGetRandomVehicleModelInMemory(bool UNUSED_PARAM(bOnlyPickNormalVehiclesFlag), int& UNUSED_PARAM(ReturnVehicleModelHashKey), int &UNUSED_PARAM(ReturnVehicleClass))
{
	/*
//	s32 VehicleModelIndex = gPopStreaming.GetAppropriateLoadedCars().PickRandomModel();

	CLoadedModelGroup	SelectableCars;
	SelectableCars.Copy(&gPopStreaming.GetAppropriateLoadedCars());

	SelectableCars.RemoveBoats();

	if (bOnlyPickNormalVehiclesFlag)
	{
		SelectableCars.RemoveTaxiVehs();
		SelectableCars.RemoveEmergencyServices();
		SelectableCars.RemoveBigVehicles();
	}

	u32 VehicleModelIndex = SelectableCars.PickRandomCarModel();

	if(fwModelId::MI_INVALID == VehicleModelIndex)
	{
//		ScriptParams[1] = -1;
		ReturnVehicleModelHashKey = -1;	//	Could a hash key of -1 be valid? What should be returned here?
		ReturnVehicleClass = -1;
	}
	else
	{
		CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(fwModelId(VehicleModelIndex));
		if (SCRIPT_VERIFY(pModel, "GET_RANDOM_VEHICLE_MODEL_IN_MEMORY - couldn't find model for the vehicle's model index"))
		{
			ReturnVehicleModelHashKey = (int) pModel->GetHashKey();

			ReturnVehicleClass = 0; //((CVehicleModelInfo *) pModel)->GetVehicleList();
		}
	}*/
}

int CommandGetCurrentBasicPoliceVehicleModel()
{
	scriptAssertf(0, "GET_CURRENT_BASIC_POLICE_VEHICLE_MODEL - No police vehicles loaded, returning invalid index!");
	return -1;
}

int CommandGetCurrentPoliceVehicleModel()
{
	scriptAssertf(0, "GET_CURRENT_POLICE_VEHICLE_MODEL - This command needs reimplementing if still used!");
	return -1;
}

int CommandGetCurrentTaxiVehicleModel()
{
	Assert(MI_CAR_TAXI_CURRENT_1 != fwModelId::MI_INVALID);
	return CModelInfo::GetBaseModelInfo(MI_CAR_TAXI_CURRENT_1)->GetHashKey();
}

int CommandGetVehicleDoorLockStatus(int VehicleIndex)
{
	int ReturnLockState = 0;
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		ReturnLockState = pVehicle->GetCarDoorLocks();
	}
	return ReturnLockState;
}

int CommandGetVehicleIndividualDoorLockStatus(int VehicleIndex, int DoorID)
{
	int ReturnLockState = 0;
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorID));
		const CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);
		if(SCRIPT_VERIFY((pDoor), "GET_VEHICLE_INDIVIDUAL_DOOR_LOCK_STATUS - Door not found"))
		{
			ReturnLockState = pDoor->m_eDoorLockState;
		}
	}
	return ReturnLockState;
}

bool CommandIsVehicleDoorDamaged(int VehicleIndex, int DoorNumber)
{
	bool LatestCmpFlagResult = false;

		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		if (SCRIPT_VERIFY((pVehicle->InheritsFromAutomobile()), "IS_VEHICLE_DOOR_DAMAGED - vehicle must have a door"))
		{
			eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

			if(pVehicle && pVehicle->GetDoorFromId(nDoorId))
			{
				LatestCmpFlagResult = !pVehicle->GetDoorFromId(nDoorId)->GetIsIntact(pVehicle);
			}
		}
	}

	return LatestCmpFlagResult;
}


void CommandSetDoorAllowedToBeBrokenOff(int VehicleIndex, int DoorNumber, bool DoorAllowedToBeBrokenOff)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (SCRIPT_VERIFY(pVehicle, "SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF - vehicle does not exist"))
    {
        eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));

        if(pVehicle && pVehicle->GetDoorFromId(nDoorId))
        {
            CCarDoor* pCarDoor = pVehicle->GetDoorFromId(nDoorId);
            if(pCarDoor)
            {
                pCarDoor->SetDoorAllowedToBeBrokenOff(DoorAllowedToBeBrokenOff,pVehicle);
            }
        }
    }
}

bool CommandIsVehicleBumperBouncing(int VehicleIndex, bool front)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		eHierarchyId bumper = VEH_BUMPER_R;
		if (front)
			bumper = VEH_BUMPER_F;

		// check if bumper is bouncing
		const CVehicleDamage* dmg = pVehicle->GetVehicleDamage();
		if (dmg)
		{
			for (u32 i = 0; i < MAX_BOUNCING_PANELS; ++i)
			{
				const CBouncingPanel* panel = dmg->GetBouncingPanel(i);
				if (!panel)
					continue;

				// bumper is bouncing
				if (panel->m_nComponentIndex == bumper)
					return true;
			}
		}
	}

	return false;
}

bool CommandIsVehicleBumperBrokenOff(int VehicleIndex, bool front)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		const eHierarchyId bumper = front ? VEH_BUMPER_F : VEH_BUMPER_R;
		const fragInst* pFragInst = pVehicle->GetVehicleFragInst();
		if (pFragInst && pVehicle->GetVehicleModelInfo())
		{
			for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
			{
				const int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());
				const int nBumperIndex = pVehicle->GetVehicleModelInfo()->GetBoneIndex(bumper);

				if (boneIndex > -1 && nBumperIndex > -1 && boneIndex == nBumperIndex && pFragInst->GetChildBroken(nChild))
				{
					return true;
			}
		}
	}
	}

	return false;
}


bool CommandIsCopVehicleInArea3d(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors)
{
	int pedCount;
	int vehicleCount;
	Vector3 vMinCoors( scrVecMinCoors );
	Vector3 vMaxCoors( scrVecMaxCoors );
	return CPedPopulation::GetCopsInArea(vMinCoors, vMaxCoors, NULL, 0, pedCount, NULL, 1, vehicleCount );
}


bool CommandIsEntryPointForSeatClear(int PedIndex, int VehicleIndex, int DestNumber, bool CheckSide, bool LeftSide)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	s32 seat = DestNumber+1;

	if(pVehicle)
	{
		if (scriptVerifyf(seat < MAX_VEHICLE_SEATS, "IS_ENTRY_POINT_FOR_SEAT_CLEAR - seat is %u, should be less than %u", DestNumber, MAX_VEHICLE_SEATS))
		{
			if (seat >= 0)
			{
				s32 iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(seat, pVehicle, SA_directAccessSeat, CheckSide, LeftSide);
				if (pVehicle->IsEntryPointIndexValid(iEntryPointIndex))
				{
					if (pVehicle->InheritsFromBike() && pVehicle->IsSeatIndexValid(seat))
					{
						if (CTaskVehicleFSM::IsOpenSeatIsBlocked(*pVehicle, seat))
						{
							return false;
						}
					}
					return CVehicle::IsEntryPointClearForPed(*pVehicle, *pPed, iEntryPointIndex);
				}
			}	
			else
			{
				scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "IS_ENTRY_POINT_FOR_SEAT_CLEAR - VS_ANY_PASSENGER is not valid, must use a seat specific enum  ");
			}	
		}
	}

	return false;
}

int CommandGetNumberOfEntryPoints(int VehicleIndex)
{
	int iNumPoints = -1;
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	if (pVehicle)
	{
		iNumPoints = pVehicle->GetNumberEntryExitPoints();
	}

	return iNumPoints;
}

Vector3 CommandGetEntryPointPosition(int VehicleIndex, int iEntryPointIndex)
{
	Vector3 vPosition(Vector3::ZeroType);
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	if (pVehicle)
	{
		const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);
		if (scriptVerifyf(pEntryPoint, "GET_ENTRY_POINT_POSITION: Invalid entry point for index %d.", iEntryPointIndex))
		{
			pEntryPoint->GetEntryPointPosition(pVehicle, NULL, vPosition);
		}
	}

	return vPosition;
}

bool CommandCanShuffleSeat(int VehicleIndex, int DestNumber)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	s32 seat = DestNumber+1;
	if(pVehicle)
	{
		if (scriptVerifyf(seat < MAX_VEHICLE_SEATS, "CAN_SHUFFLE_SEAT - seat is %u, should be less than %u", DestNumber, MAX_VEHICLE_SEATS))
		{
			if (seat >= 0)
			{
				s32 iEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(seat, pVehicle);
				return CTaskInVehicleSeatShuffle::CanShuffleSeat(pVehicle, seat, iEntryPoint);
			}	
			else
			{
				scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "CAN_SHUFFLE_SEAT - VS_ANY_PASSENGER is not valid, must use a seat specific enum  ");
			}	
		}
	}

	return false;
}

void CommandSetTrainForcedToSlowDown(int TrainIndex, bool SlowDownFlag)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(TrainIndex);
		if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN, "SET_TRAIN_FORCED_TO_SLOW_DOWN - Vehicle NOT A train"))
		{
				((CTrain*)pVehicle)->m_nTrainFlags.bIsForcedToSlowDown = SlowDownFlag;
			}
		}
	}

bool CommandIsVehicleOnAllWheels(int VehicleIndex)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	bool LatestCmpFlagResult = false;
		if (pVehicle)
	{
		if(pVehicle->GetNumContactWheels() == pVehicle->GetNumWheels())
		{
			LatestCmpFlagResult = true;
		}
	}
	return LatestCmpFlagResult;
}

int CommandGetVehicleModelValue(int VehicleModelHashKey)
{
	fwModelId VehicleModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
	if (SCRIPT_VERIFY(VehicleModelId.IsValid(), "GET_VEHICLE_MODEL_VALUE - this is not a valid model index"))
	{
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(VehicleModelId);
		s32 nHandlingIndex = pModelInfo->GetHandlingId();
		CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(nHandlingIndex);
		return pHandling->m_nMonetaryValue;
	}
	return 0;
}

bool CommandGetTrainDirection(int TrainIndex)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(TrainIndex);
		if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN, "GET_TRAIN_DIRECTION - Vehicle NOT A train"))
		{
			return(((CTrain *)pVehicle)->m_nTrainFlags.bDirectionTrackForward);
		}
	}
	return false;
}

void CommandSkipToNextAllowedStation(int TrainIndex)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(TrainIndex);
		if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN, "SKIP_TO_NEXT_ALLOWED_STATION - Vehicle NOT A train"))
		{
			CTrain::SkipToNextAllowedStation((CTrain*)pVehicle);
		}
	}
}

void CommandSetVehicleExtraColours(int VehicleIndex, int ExtraColour1, int ExtraColour2)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
		{
			const s32 MaxColours = CVehicleModelInfo::GetVehicleColours()->GetColorCount();
			if (SCRIPT_VERIFY((ExtraColour1 >= 0 && ExtraColour1 < MaxColours), "SET_VEHICLE_EXTRA_COLOURS - Colour 1 must be >= 0")&&
				SCRIPT_VERIFY((ExtraColour2 >= 0 && ExtraColour2 < MaxColours), "SET_VEHICLE_EXTRA_COLOURS - Colour 2 must be >= 0"))
			{
				pVehicle->SetBodyColour3((u8) ExtraColour1);
				pVehicle->SetBodyColour4((u8) ExtraColour2);
				pVehicle->UpdateBodyColourRemapping();			// let shaders know, that body colours changed
			}
		}
	}

void CommandGetVehicleExtraColours(int VehicleIndex, int &ReturnExtraVehicleColour1, int &ReturnExtraVehicleColour2)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		// Removing this assert as it seems inconsistent with the rest of the Get/Set vehicle colour commands
		//if (SCRIPT_VERIFY(!pVehicle->InheritsFromBoat(), "GET_EXTRA_VEHICLE_COLOURS - Trying to get the colour of a boat"))
		{
			ReturnExtraVehicleColour1 = pVehicle->GetBodyColour3();
			ReturnExtraVehicleColour2 = pVehicle->GetBodyColour4();
		}
	}
}

void CommandSetVehicleExtraColour5(int VehicleIndex, int ExtraColour5)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		const s32 MaxColours = CVehicleModelInfo::GetVehicleColours()->GetColorCount();
		if (SCRIPT_VERIFY((ExtraColour5 >= 0 && ExtraColour5 < MaxColours), "SET_VEHICLE_EXTRA_COLOUR_5 - Colour 5 must be >= 0"))
		{
			pVehicle->SetBodyColour5((u8) ExtraColour5);
			pVehicle->UpdateBodyColourRemapping();			// let shaders know, that body colours changed
		}
	}
}

void CommandGetVehicleExtraColour5(int VehicleIndex, int &ReturnExtraVehicleColour5)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		// Removing this assert as it seems inconsistent with the rest of the Get/Set vehicle colour commands
		//if (SCRIPT_VERIFY(!pVehicle->InheritsFromBoat(), "GET_EXTRA_VEHICLE_COLOURS - Trying to get the colour of a boat"))
		{
			ReturnExtraVehicleColour5 = pVehicle->GetBodyColour5();
		}
	}
}

void CommandSetVehicleExtraColour6(int VehicleIndex, int ExtraColour6)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		const s32 MaxColours = CVehicleModelInfo::GetVehicleColours()->GetColorCount();
		if (SCRIPT_VERIFY((ExtraColour6 >= 0 && ExtraColour6 < MaxColours), "SET_VEHICLE_EXTRA_COLOUR_6 - Colour 6 must be >= 0"))
		{
			pVehicle->SetBodyColour6((u8) ExtraColour6);
			pVehicle->UpdateBodyColourRemapping();			// let shaders know, that body colours changed
		}
	}
}

void CommandGetVehicleExtraColour6(int VehicleIndex, int &ReturnExtraVehicleColour6)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		// Removing this assert as it seems inconsistent with the rest of the Get/Set vehicle colour commands
		//if (SCRIPT_VERIFY(!pVehicle->InheritsFromBoat(), "GET_EXTRA_VEHICLE_COLOURS - Trying to get the colour of a boat"))
		{
			ReturnExtraVehicleColour6 = pVehicle->GetBodyColour6();
		}
	}
}

void CommandSetRespraysActive(bool RespraysFlag)
{
	RespraysFlag = !RespraysFlag;
	CGarages::NoResprays = RespraysFlag;
	CGarages::AllRespraysCloseOrOpen(!CGarages::NoResprays);
}

void CommandStopAllGarageActivity()
{
	CGarages::AllRespraysCloseOrOpen(true);
}

bool CommandHasVehicleBeenResprayed(int VehicleIndex)
{
	bool LatestCmpFlagResult = false;
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		if(pVehicle->m_nVehicleFlags.bHasBeenResprayed)
		{
			LatestCmpFlagResult = true;
			pVehicle->m_nVehicleFlags.bHasBeenResprayed = false;
		}
	}
	return LatestCmpFlagResult;
}

bool CommandIsPayNSprayActive()
{
	return CGarages::IsPaynSprayActive();
}

bool CommandHasResprayHappened()
{
	bool bResult = CGarages::ResprayHappened;
	CGarages::ResprayHappened = false;
	return bResult;
}

void CommandSetVehicleFixed(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->Fix();
		pVehicle->SetHealth(CREATED_VEHICLE_HEALTH);
	}
}

void CommandSetVehicleDeformationFixed(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->FixVehicleDeformation();
	}
}

void CommandSetVehicleCanEngineMissFire(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_CAN_ENGINE_MISSFIRE - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanEngineMissFire = bVal;
		}
	}
}

void CommandSetVehicleCanLeakOil(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_CAN_LEAK_OIL - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanLeakOil = bVal;
            pVehicle->m_nVehicleFlags.bCanPlayerCarLeakOil = bVal;
		}
	}
}

void CommandSetVehicleCanLeakPetrol(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_CAN_LEAK_PETROL - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanLeakPetrol = bVal;
            pVehicle->m_nVehicleFlags.bCanPlayerCarLeakPetrol = bVal;
		}
	}
}

void CommandDisablePetrolTankFires(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_DISABLE_PETROL_TANK_FIRES - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bDisablePetrolTankFires = bVal;
		}
	}
}

void CommandDisablePetrolTankDamage(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_DISABLE_PETROL_TANK_DAMAGE - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bDisablePetrolTankDamage = bVal;
		}
	}
}

void CommandDisableEngineFires(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_DISABLE_ENGINE_FIRES - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bDisableEngineFires = bVal;
		}
	}
}

void CommandSetVehicleLimitSpeedWhenPlayerInactive(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_LIMIT_SPEED_WHEN_PLAYER_INACTIVE - Vehicle has to be a mission vehicle"))
		{
			if (bVal!=pVehicle->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive)
			{
				// log when this is changed (design request)
				Displayf("SET_VEHICLE_LIMIT_SPEED_WHEN_PLAYER_INACTIVE set to %s by %s\n", bVal ? "true" : "false", CTheScripts::GetCurrentScriptName());
			}
			pVehicle->m_nVehicleFlags.bLimitSpeedWhenPlayerInactive = bVal;
		}
	}
}

void CommandSetVehicleStopInstantlyWhenPlayerInactive(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (bVal!=pVehicle->m_nVehicleFlags.bStopInstantlyWhenPlayerInactive)
		{
			// log when this is changed (design request)
			Displayf("SET_VEHICLE_STOP_INSTANTLY_WHEN_PLAYER_INACTIVE set to %s by %s\n", bVal ? "true" : "false", CTheScripts::GetCurrentScriptName());
		}
		pVehicle->m_nVehicleFlags.bStopInstantlyWhenPlayerInactive = bVal;
	}
}

void CommandDisablePretendOccupants(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_DISABLE_PRETEND_OCCUPANTS - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bDisablePretendOccupants = bVal;
		}
	}
}

void CommandSetMadDrivers(bool bNewVal)
{
	CVehiclePopulation::ms_bMadDrivers = bNewVal;
}

void CommandRemoveVehiclesFromGeneratorsInArea(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors, bool isNetworked)
{
	Vector3 vMinCoors = Vector3(scrVecMinCoors);
	Vector3 vMaxCoors = Vector3(scrVecMaxCoors);

	CTheCarGenerators::RemoveCarsFromGeneratorsInArea( vMinCoors.x, vMinCoors.y, vMinCoors.z, vMaxCoors.x, vMaxCoors.y, vMaxCoors.z);

	if(isNetworked)
	{
		CClearRectangleAreaEvent::Trigger(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), &vMinCoors, &vMaxCoors);
	}
}

void CommandSetVehicleSteerBias(int VehicleIndex, float Bias)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_fSteerInputBias = Bias;
	}

}

void CommandSetVehiclePitchBias(int VehicleIndex, float Bias)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle && pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast< CPlane* >( pVehicle );
		pPlane->m_fPitchInputBias = Bias;
	}
}

void CommandSetVehicleRollBias(int VehicleIndex, float Bias)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle && pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast< CPlane* >( pVehicle );
		pPlane->m_fRollInputBias = Bias;
	}
}

bool CommandHasVehicleStoppedBecauseOfLight(int VehicleIndex)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		return pVehicle->HasCarStoppedBecauseOfLight();
	}
	return false;
}

bool CommandIsVehicleExtraTurnedOn(int iVehicleID, int nExtra)
{
	// Check the vehicle
		const CVehicle *pVehicle= CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleID);
		if (pVehicle)
	{
		nExtra += VEH_EXTRA_1 - 1;
		if (SCRIPT_VERIFY(nExtra >= VEH_EXTRA_1 && nExtra <= VEH_LAST_EXTRA, "IS_VEHICLE_EXTRA_TURNED_ON - Extra is outside range"))
		{
			return pVehicle->GetIsExtraOn((eHierarchyId)nExtra);
		}
	}
	return false;
}

void CommandTurnOffVehicleExtra(int iVehicleID, int nExtra, bool bTurnOff)
{
	// Check the vehicle
	CVehicle *pVehicle= CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleID);

	if (pVehicle)
	{
		nExtra += VEH_EXTRA_1 - 1;
		if (SCRIPT_VERIFY(nExtra >= VEH_EXTRA_1 && nExtra <= VEH_LAST_EXTRA, "TURN_OFF_VEHICLE_EXTRA - Extra is outside range"))
		{
			pVehicle->TurnOffExtra((eHierarchyId)nExtra, bTurnOff);

			// GTAV - B*1882897 
			// If the roof has been removed, get rid of the front windows too
			if( bTurnOff && pVehicle->GetVehicleType()==VEHICLE_TYPE_CAR && !pVehicle->CarHasRoof() )
			{
				pVehicle->DeleteWindow(VEH_WINDOW_LF);
				pVehicle->DeleteWindow(VEH_WINDOW_RF);
			}
		}
	}
}

bool CommandDoesExtraExist(int iVehicleID, int nExtra)
{
	// Check the vehicle
		const CVehicle *pVehicle= CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleID);
		if (pVehicle)
	{
		nExtra += VEH_EXTRA_1 - 1;
		if (SCRIPT_VERIFY(nExtra >= VEH_EXTRA_1 && nExtra <= VEH_LAST_EXTRA, "TURN_OFF_VEHICLE_EXTRA - Extra is outside range"))
		{
			return(pVehicle->GetDoesExtraExist((eHierarchyId)nExtra));
		}
	}

	return(false);
}

bool CommandIsExtraBrokenOff(int iVehicleID, int nExtra)
{
	// Check the vehicle
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleID);
	if (pVehicle)
	{
		nExtra += VEH_EXTRA_1 - 1;
		if (SCRIPT_VERIFY(nExtra >= VEH_EXTRA_1 && nExtra <= VEH_LAST_EXTRA, "IS_EXTRA_BROKEN_OFF - Extra is outside range"))
		{
			if (pVehicle->GetDoesExtraExist((eHierarchyId)nExtra))
			{
				int iChild = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( (eHierarchyId)nExtra ));
				return pVehicle->GetVehicleFragInst() && iChild > -1 && pVehicle->GetVehicleFragInst()->GetChildBroken(iChild);
			}
		}
	}

	return(false);
}


void CommandSetConvertibleRoof(int iVehicleID, bool bTurnOn)
{
	// Check the vehicle
		CVehicle *pVehicle= CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleID);
#if __ASSERT
	if(pVehicle)
		scriptAssertf(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_CONVERTIBLE), "%s:SET_CONVERTIBLE_ROOF - Vehicle is not a convertible", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif

		if (pVehicle)
	{
		if(bTurnOn)
		{
			pVehicle->InitExtraFlags(EXTRAS_SET_RAIN);
		}
		else
		{
			pVehicle->InitExtraFlags(EXTRAS_SET_CONV_OPEN);
		}

		// fix resets all the fragment bits, setting up all the fragExtra's at the same time.
		pVehicle->Fix();
	}
}

void CommandSetGangVehicle(int iVehicleID, bool bTurnOn)
{
	// Check the vehicle
		CVehicle *pVehicle= CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleID);
#if __ASSERT
	if(pVehicle)
		scriptAssertf(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_GANG), "%s:SET_GANG_VEHICLE - Vehicle is not a gang vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
		if (pVehicle)
	{
		if(bTurnOn)
			pVehicle->InitExtraFlags(EXTRAS_SET_GANG);
		else
			pVehicle->InitExtraFlags(0);

		// fix resets all the fragment bits, setting up all the fragExtra's at the same time.
		pVehicle->Fix();
	}
}


bool CommandIsVehicleStoppedAtTrafficLights(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		if (pVehicle->GetVelocity().Mag2() < 0.5f * 0.5f)
		{
			return pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS;
		}
	}
	return false;
}


void CommandSetVehicleDamage(int VehicleIndex, const scrVector & scrVecDamage, float damageAmount, float deformationScale, bool localDamage)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

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


void CommandSetVehicleOccupantsTakeExplosiveDamage(int VehicleIndex, bool bVehicleOccupantsTakeExplosiveDamage)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->SetVehicleOccupantsTakeExplosiveDamage(bVehicleOccupantsTakeExplosiveDamage);
	}
}

float CommandGetVehicleEngineHealth(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return pVehicle->GetVehicleDamage()->GetEngineHealth();
	}
	return 1000.0f;
}

void CommandSetVehicleEngineHealth(int VehicleIndex, float fHealth)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		gnetDebug2("CommandSetVehicleEngineHealth: setting engine health for %s to be %f", pVehicle->GetLogName(), fHealth);
		pVehicle->m_Transmission.SetEngineHealth(fHealth);

		if(pVehicle->InheritsFromPlane())
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			float fPlaneEngineHealth = Clamp(fHealth, 0.0f, pPlane->GetAircraftDamage().GetSectionMaxHealth(CAircraftDamage::ENGINE_L));
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_L, fPlaneEngineHealth);
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_R, fPlaneEngineHealth);
		}

		if (pVehicle->InheritsFromHeli())
		{
			CNetObjHeli* netObjHeli = static_cast<CNetObjHeli*>(pVehicle->GetNetworkObject());
			if (netObjHeli && fHealth > pVehicle->m_Transmission.GetEngineHealthMax())
			{
				gnetDebug1("CommandSetVehicleEngineHealth: setting custom engine health for a heli at %f", fHealth);
				netObjHeli->SetIsCustomHealthHeli(true);							
			}
		}
	}
}

void CommandSetPlaneEngineHealth(int VehicleIndex, float fHealth)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if(pVehicle->InheritsFromPlane())
		{
			gnetDebug2("CommandSetPlaneEngineHealth: setting plane engine health for %s to be %f", pVehicle->GetLogName(), fHealth);
			pVehicle->m_Transmission.SetEngineHealth(fHealth);

			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_L, fHealth);
			pPlane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_R, fHealth);
		}		
	}
}


float CommandGetVehiclePetrolTankHealth(int VehicleIndex)
{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		return pVehicle->GetVehicleDamage()->GetPetrolTankHealth();
	}
	return 1000.0f;
}

void CommandSetVehiclePetrolTankHealth(int VehicleIndex, float fHealth)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	
	if (pVehicle)
	{
		gnetDebug2("CommandSetVehiclePetrolTankHealth: setting petrol tank health for %s to be %f", pVehicle->GetLogName(), fHealth);
		pVehicle->GetVehicleDamage()->SetPetrolTankHealth(fHealth);

		if (pVehicle->InheritsFromHeli())
		{
			CNetObjHeli* netObjHeli = static_cast<CNetObjHeli*>(pVehicle->GetNetworkObject());
			if (netObjHeli && fHealth > pVehicle->GetVehicleDamage()->GetPetrolTankHealth())
			{
				gnetDebug1("CommandSetVehiclePetrolTankHealth: setting custom petrol tank health for a heli at %f", fHealth);
				netObjHeli->SetIsCustomHealthHeli(true);							
			}
		}
	}
}

float CommandGetVehicleBodyHealth(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return pVehicle->GetVehicleDamage()->GetBodyHealth();
	}
	return 1000.0f;
}

void CommandSetVehicleBodyHealth(int VehicleIndex, float fHealth)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		gnetDebug2("CommandSetVehicleBodyHealth: setting vehicle body health for %s to be %f", pVehicle->GetLogName(), fHealth);
		pVehicle->GetVehicleDamage()->SetBodyHealth(fHealth);

		if (pVehicle->InheritsFromHeli())
		{
			CNetObjHeli* netObjHeli = static_cast<CNetObjHeli*>(pVehicle->GetNetworkObject());
			if (netObjHeli && fHealth > pVehicle->GetVehicleDamage()->GetBodyHealthMax())
			{
				gnetDebug1("CommandSetVehicleBodyHealth: setting custom body health for a heli at %f", fHealth);
				netObjHeli->SetIsCustomHealthHeli(true);							
			}
		}
	}
}

bool CommandIsVehicleStuckTimerUp(int VehicleIndex, int nStuckType, int nRequiredTime)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	
    if(pVehicle)
	{
		if(nRequiredTime > BIT(16) - 1)
			nRequiredTime = BIT(16) - 1;

		return pVehicle->GetVehicleDamage()->GetIsStuck(nStuckType, (u16)nRequiredTime);
	
    }
	return false;
}

void CommandResetVehicleStuckTimer(int VehicleIndex, int nStuckType)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->GetVehicleDamage()->ResetStuckTimer(nStuckType);
	}
}

bool CommandIsVehDriveable(int VehicleIndex, bool bCheckFire)
{
#if RSG_GEN9 && __BANK
	if ( VehicleIndex != 0 )
	{
		CPhysical* pPhysical = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPhysical>(VehicleIndex, 0);
		if (pPhysical && !(rage_dynamic_cast<CVehicle*>(pPhysical)))
		{
			vehicleDisplayf("[CommandIsVehDriveable] Physical : %s  is not vehicle", pPhysical->GetModelName());
		}
	}
#endif // RSG_GEN9 && __BANK

	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CVehicle>(VehicleIndex, 0);
	if(pVehicle)
	{
#if __DEV
		if (pVehicle->GetStatus() != STATUS_WRECKED)
		{
			pVehicle->m_nDEflags.bCheckedForDead = TRUE;
		}
#endif

		if (bCheckFire && pVehicle->IsOnFire()) 
		{
			return false;
		}

		if( pVehicle->GetVehicleDamage()->GetIsDriveable( true, true, true ) )
		{
			return pVehicle->m_nVehicleFlags.bIsThisADriveableCar;
		}
	}

	return false;
}


void CommandSetVehicleHasBeenOwnedByPlayer(int VehicleIndex, bool bNewVal)
{
	CVehicle *pVehicle;

		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = bNewVal;
		}
	}

void CommandSetVehicleNeedsToBeHotwired(int VehicleIndex, bool bNewVal)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = bNewVal;
		}
	}

void CommandSetVehicleBlipThrottleRandomly(int VehicleIndex, bool bNewVal)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
	{
		// TODO: chat to network guys about this
		pVehicle->m_nVehicleFlags.bBlipThrottleRandomly = bNewVal;
	}
}


bool CommandGetIsVehicleUsingPretendOccupants(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		return pVehicle->IsUsingPretendOccupants();
	}

	return false;
}

bool CommandGetIsVehicleStoppedForDoor(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		return pVehicle->m_nVehicleFlags.bWasStoppedForDoor;
	}

	return false;
}

void CommandSetPoliceFocusWillTrackVehicle(int VehicleIndex, bool bNewVal)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bPoliceFocusWillTrackCar = bNewVal;
	}
}

void CommandSoundCarHorn(int VehicleIndex, int TimeToSoundHorn,int HornTypeHash, bool IsMusicalHorn)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		audVehicleAudioEntity *vehAudEntity = pVehicle->GetVehicleAudioEntity();
		if(vehAudEntity)
		{
			if(pVehicle->IsHornOn())
			{
				vehAudEntity->StopHorn();
				pVehicle->ProcessSirenAndHorn(true);
			}
			vehAudEntity->SetHornType(HornTypeHash);
			f32 timeInSeconds = (f32)TimeToSoundHorn/1000.f;
			if(IsMusicalHorn)
			{
				timeInSeconds = -1.f;
			}
			vehAudEntity->PlayVehicleHorn(timeInSeconds, false, true);
		}
	}
}
void CommandVehicleInCarModShop(int VehicleIndex,bool InCarModShop)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		audVehicleAudioEntity *vehAudEntity = pVehicle->GetVehicleAudioEntity();
		if(vehAudEntity)
		{
			vehAudEntity->SetIsInCarModShop(InCarModShop);
		}
	}
}

void CommandSetVehicleHasStrongAxles(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bMissionVehToughAxles = bSet;
	}
}

void CommandSetVehicleAlarm(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetCarAlarm(bSet);
	}
}

void CommandStartVehicleAlarm(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->TriggerCarAlarm();
	}
}

bool CommandIsVehicleAlarmActivated(int VehicleIndex)
{
	bool bIsAlarmActivated = false;
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pVehicle)
	{
		bIsAlarmActivated = pVehicle->IsAlarmActivated();
	}

	return bIsAlarmActivated;
}

void CommandSetVehicleInteriorLight(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetInteriorLight(bSet);
	}
}

void CommandSetVehicleForceInteriorLight(int VehicleIndex, bool bForce)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->SetForceInteriorLight(bForce);
	}
}

const char* CommandGetDisplayNameFromVehicleModel(int VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if(SCRIPT_VERIFYF(pModelInfo, "GET_DISPLAY_NAME_FROM_VEHICLE_MODEL - not a valid model hash [%u]", (u32)VehicleModelHashKey))
	{
		if(SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "GET_DISPLAY_NAME_FROM_VEHICLE_MODEL - Specified model isn't actually a vehicle"))
		{
			return ((CVehicleModelInfo*) pModelInfo)->GetGameName();
		}
	}

	return "CARNOTFOUND";
}

const char* CommandGetMakeNameFromVehicleModel(int VehicleModelHashKey)
{
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if(SCRIPT_VERIFYF(pModelInfo, "GET_MAKE_NAME_FROM_VEHICLE_MODEL - not a valid model hash [%u]", (u32)VehicleModelHashKey))
	{
		if(SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_VEHICLE, "GET_MAKE_NAME_FROM_VEHICLE_MODEL - Specified model isn't actually a vehicle"))
		{
			return ((CVehicleModelInfo*) pModelInfo)->GetVehicleMakeName();
		}
	}

	return "CARNOTFOUND";
}

u32 CommandGetVehicleLayoutHash(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (SCRIPT_VERIFY(pVehicle, "GET_VEHICLE_LAYOUT_HASH - Vehicle index is invalid"))
	{
		return pVehicle->GetLayoutInfo()->GetName().GetHash();
	}
	return 0;
}

u32 CommandGetInVehicleClipSetHashForSeat(int VehicleIndex, int DestNumber)
{
	s32 seat = DestNumber+1;
	if (SCRIPT_VERIFYF(seat >= 0, "GET_IN_VEHICLE_CLIPSET_HASH_FOR_SEAT, Invalid seat (%i) passed in", DestNumber))
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK | CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (SCRIPT_VERIFY(pVehicle, "GET_VEHICLE_LAYOUT_HASH - Vehicle index is invalid"))
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(seat);
			if (pSeatAnimInfo)
			{
				return pSeatAnimInfo->GetSeatClipSetId().GetHash();
			}
		}
	}
	return 0;
}

scrVector CommandGetVehicleDeformationAtPos(int VehicleIndex, const scrVector & scrVecPos)
{
	Vec3V vecResult(V_ZERO);

	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(pVehicle && pVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			void* pTexLock = pVehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
			if(pTexLock)
			{
				Vec3V vecPos(scrVecPos);

				vecResult = pVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pTexLock, vecPos);

				pVehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
			}
		}
	}
	return VEC3V_TO_VECTOR3(vecResult);
}


void CommandSetVehicleLivery(int VehicleIndex, int livery)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(SCRIPT_VERIFY((livery >= -1), "SET_VEHICLE_LIVERY - Livery must be >= -1"))
		{
#if __ASSERT
			CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
			s32 liveryCount = pModelInfo->GetLiveriesCount();
			if ((livery > liveryCount))
			{
				char ErrorMsg[256];
				sprintf(ErrorMsg, "SET_VEHICLE_LIVERY - Livery for this vehicle must be less than %d", liveryCount);
				scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ErrorMsg);
			}
#endif
			pVehicle->SetLiveryId(livery);
			pVehicle->UpdateBodyColourRemapping();		// let shaders know, that body colours changed
		}
	}
}

void CommandSetVehicleLivery2(int VehicleIndex, int livery2)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(SCRIPT_VERIFY((livery2 >= -1), "SET_VEHICLE_LIVERY2 - Livery must be >= -1"))
		{
#if __ASSERT
			CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
			s32 livery2Count = pModelInfo->GetLiveries2Count();
			if ((livery2 > livery2Count))
			{
				char ErrorMsg[256];
				sprintf(ErrorMsg, "SET_VEHICLE_LIVERY2 - Livery for this vehicle must be less than %d", livery2Count);
				scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ErrorMsg);
			}
#endif
			pVehicle->SetLivery2Id(livery2);
			pVehicle->UpdateBodyColourRemapping();		// let shaders know, that body colours changed
		}
	}
}

int CommandGetVehicleLivery(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		return pVehicle->GetLiveryId();
	}

	return -1;
}

int CommandGetVehicleLivery2(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		return pVehicle->GetLivery2Id();
	}

	return -1;
}

int CommandGetVehicleLiveryCount(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		return pVehicle->GetVehicleModelInfo()->GetLiveriesCount();
	}

	return 0;
}

int CommandGetVehicleLivery2Count(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		return pVehicle->GetVehicleModelInfo()->GetLiveries2Count();
	}

	return 0;
}

bool CommandIsVehicleWindowIntact(int VehicleIndex, int WindowNumber)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));
		if(pVehicle->HasWindowToSmash(nWindowId))
		{
			// Window exists and can be smashed
			return true;
		}
		else
		{
			// Either window doesn't exist or it has been smashed
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool CommandAreAllVehicleWindowsIntact(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(pVehicle->HasAnyWindowsToSmash())
		{
			// Windows exists and can be smashed
			return true;
		}
		else
		{
			// Either window doesn't exist or it has been smashed
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool CommandAreAnyVehicleSeatsFree(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle, "ARE_ANY_VEHICLE_SEATS_FREE - vehicle index isn't valid"))
	{
		return pVehicle->GetSeatManager()->CountFreeSeats() > 0 ? true : false;
	}
	return false;
}

void CommandResetVehicleWheels(int VehicleIndex, bool bExtend)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		for(int i=0; i<pVehicle->GetNumWheels(); i++)
			pVehicle->GetWheel(i)->Reset(bExtend);
	}
}

void CommandRollDownWindows(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		s32 iNumEntryPoints = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints();

		for (s32 iEntryPoint = 0; iEntryPoint < iNumEntryPoints; ++iEntryPoint)
		{
			if (pVehicle->IsEntryIndexValid(iEntryPoint))
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPoint);
				Assertf(pEntryPointInfo, "Vehicle is missing entry point");

				eHierarchyId window = pEntryPointInfo->GetWindowId();
				if (window != VEH_INVALID_ID)
				{
					pVehicle->RolldownWindow(window);
				}
			}
		}
	}
}

void CommandRollDownWindow(int VehicleIndex, int WindowNumber)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));
		if(nWindowId != VEH_INVALID_ID)
		{
			pVehicle->RolldownWindow(nWindowId);
		}
	}
}

void CommandRollUpWindow(int VehicleIndex, int WindowNumber)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		eHierarchyId nWindowId = CCarDoor::GetScriptWindowId(eScriptWindowList(WindowNumber));
		if(nWindowId != VEH_INVALID_ID)
		{
			pVehicle->RollWindowUp(nWindowId);
		}
	}
}

bool CommandIsHeliPartBroken(int VehicleIndex, bool bCheckMainRotor, bool bCheckRearRotor, bool bCheckTailBoom)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "IS_HELI_PART_BROKEN -  Vehicle is not a helicopter!"))
		{
				const CHeli* pHeli = static_cast<const CHeli*>(pVehicle);

			if(bCheckMainRotor && pHeli->GetIsMainRotorBroken())
			{
				return true;
			}

			if(bCheckRearRotor && pHeli->GetIsRearRotorBroken())
			{
				return true;
			}

			if(bCheckTailBoom && pHeli->GetIsTailBoomBroken())
			{
				return true;
			}
		}
	}

	return false;
}

float CommandGetHeliMainRotorHealth( int VehicleIndex )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( VehicleIndex );
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "GET_HELI_MAIN_ROTOR_HEALTH -  Vehicle is not a helicopter!"))
		{
			CHeli* pHeli = (CHeli *)pVehicle;
			return pHeli->GetMainRotorHealth();
		}
	}

	return 1000.0f;
}


float CommandGetHeliTailRotorHealth( int VehicleIndex )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( VehicleIndex );
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "GET_HELI_TAIL_ROTOR_HEALTH -  Vehicle is not a helicopter!"))
		{
			CHeli* pHeli = (CHeli *)pVehicle;
			return pHeli->GetRearRotorHealth();
		}
	}

	return 1000.0f;
}


void CommandSetHeliMainRotorHealth( int VehicleIndex, float fRotorHealth )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( VehicleIndex );
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_MAIN_ROTOR_HEALTH -  Vehicle is not a helicopter!"))
		{
			gnetDebug2("CommandSetHeliMainRotorHealth: setting main rotor health for %s to be %f", pVehicle->GetLogName(), fRotorHealth);
			CHeli* pHeli = (CHeli *)pVehicle;
			pHeli->SetMainRotorHealth(fRotorHealth);
		}
	}
}

void CommandSetHeliTailRotorHealth( int VehicleIndex, float fRotorHealth )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( VehicleIndex );
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_TAIL_ROTOR_HEALTH -  Vehicle is not a helicopter!"))
		{
			gnetDebug2("CommandSetHeliTailRotorHealth: setting tail rotor health for %s to be %f", pVehicle->GetLogName(), fRotorHealth);
			CHeli* pHeli = (CHeli *)pVehicle;
			pHeli->SetRearRotorHealth(fRotorHealth);
		}
	}
}

float CommandGetHeliTailBoomHealth( int VehicleIndex )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( VehicleIndex );
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "GET_HELI_TAIL_BOOM_HEALTH -  Vehicle is not a helicopter!"))
		{
			CHeli* pHeli = (CHeli *)pVehicle;
			return pHeli->GetTailBoomHealth();
		}
	}

	return 1000.0f;
}


void CommandSetHeliTailBoomCanBreakOff(int VehicleIndex, bool bCanBreakOff)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_TAIL_BOOM_CAN_BREAK_OFF -  Vehicle is not a helicopter!"))
		{
			CHeli* pHeli = (CHeli *)pVehicle;

			pHeli->SetCanBreakOffTailBoom(bCanBreakOff);
		}
	}
}

void CommandAllVehiclesHighLOD(bool UNUSED_PARAM(bForceHighLOD)){
	Assertf(false,"This isn't supported at the moment. Is it really necessary? - JW");
//	CBreakable::SetForceHighLOD(bForceHighLOD);
}

int CommandGetNearestCableCar(const scrVector & scrVecNewCoors, float Radius)
{
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();	
	float bestDistSoFar = Radius;
	CVehicle *pRetVeh = NULL;
	int ReturnVehicleIndex = NULL_IN_SCRIPTING_LANGUAGE;

	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);

		if( pVehicle && pVehicle->GetModelIndex()==MI_CAR_CABLECAR )
		{
			scriptAssertf(pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN, "%s:GET_NEAREST_CABLE_CAR - Vehicle Must be of type train", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			float	dist = (Vector3(scrVecNewCoors) - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) ).Mag();

			if (dist < bestDistSoFar)
			{
				bestDistSoFar = dist;
				pRetVeh = pVehicle;
			}
		}
	}
	if (pRetVeh)
	{
		ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*pRetVeh);
	}
	
	return ReturnVehicleIndex;
}

#if !__FINAL
void CommandSetVehicleNameDebug(int iVehicleIndex, const char* strName)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, 0);

	if(SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_NAME_DEBUG - Vehicle doesn't exist"))
	{
		pVehicle->SetDebugName(strName);
	}
}
#else
void CommandSetVehicleNameDebug(int UNUSED_PARAM(iVehicleIndex), const char* UNUSED_PARAM(strName))
{

}
#endif

void CommandSetVehicleExplodesOnHighExplosionDamage(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage = bSet;
	}
}

void CommandSetVehicleExplodesOnExplosionDamageAtZeroBodyHealth(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth(bSet);
	}
}

void CommandSetAllowVehicleExplodesOnContact(int VehicleIndex, bool bSet)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bDisableExplodeOnContact = !bSet;
	}
}

void CommandSetVehicleDisableTowing(int VehicleIndex, bool bDisable)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bDisableTowing = bDisable;
	}
}


void CommandDisablePlaneAileron(int VehicleIndex, bool bLeftSide, bool bDisable)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "DISABLE_PLANE_AILERON Vehicle is not a plane"))
        {
            CPlane* pPlane = static_cast<CPlane*>(pVehicle);

            pPlane->DisableAileron(bLeftSide, bDisable);
        }
    }
}


bool CommandGetIsVehicleEngineRunning(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

    if(SCRIPT_VERIFY(pVehicle, "GET_IS_VEHICLE_ENGINE_RUNNING Vehicle not found"))
    {
       return pVehicle->IsEngineOn();
    }

    return false;
}

bool CommandGetVehicleHasLandingGear(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(pVehicle->InheritsFromPlane() || (pVehicle->InheritsFromHeli() && static_cast<const CHeli*>(pVehicle)->HasLandingGear()))
		{
			return true;
		}
	}
	return false;
}

void CommandControlLandingGear(int VehicleIndex, int nCommand)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane() || ( pVehicle->InheritsFromHeli() && static_cast< CHeli* >( pVehicle )->HasLandingGear() ), "CONTROL_LANDING_GEAR Vehicle is not a plane or a heli with landing gear"))
		{
			CLandingGear* pLandingGear = 0;
			if( pVehicle->InheritsFromPlane() )
			{
				pLandingGear = &static_cast<CPlane*>(pVehicle)->GetLandingGear();
			}
			else
			{
				pLandingGear = &static_cast<CHeli*>(pVehicle)->GetLandingGear();
			}

			pLandingGear->ControlLandingGear( pVehicle,(CLandingGear::eCommands) nCommand );
		}
	}
}

s32 CommandGetLandingGearState(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane() || ( pVehicle->InheritsFromHeli() && static_cast< const CHeli* >( pVehicle )->HasLandingGear() ), "GET_LANDING_GEAR_STATE Vehicle is not a plane or a Heli with landing gear"))
		{
			const CLandingGear* pLandingGear = 0;
			if( pVehicle->InheritsFromPlane() )
			{
				pLandingGear = &( static_cast<const CPlane*>(pVehicle)->GetLandingGear() );
			}
			else
			{
				pLandingGear = &( static_cast<const CHeli*>(pVehicle)->GetLandingGear() );
			}

			return pLandingGear->GetPublicState();
		}
	}

	// Default
	return CLandingGear::STATE_LOCKED_DOWN;
}

bool CommandIsAnyVehicleNearPoint(const scrVector & scrVecPoint, float radius)
{
	Vec3V vPoint = VECTOR3_TO_VEC3V(Vector3 (scrVecPoint));
	ScalarV radiusSqr = ScalarVFromF32(radius*radius);

	for(s32 i=0; i<CVehicle::GetPool()->GetSize(); i++)
	{
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(i);

		if (pVehicle)
		{
			if (IsLessThanOrEqualAll(DistSquared(pVehicle->GetTransform().GetPosition(), vPoint), radiusSqr))
			{
				return true;
			}
		}
	}

	return false;
}

void CommandRequestVehicleHighDetailModel(int iVehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex); 

	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bRequestDrawHD = true;
	}
} 

void CommandRemoveVehicleHighDetailModel(int iVehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex); 

	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bRequestDrawHD = false;
	}
}

bool CommandIsVehicleHighDetail(int iVehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex); 

	if (pVehicle)
	{
		return pVehicle->GetIsCurrentlyHD();
	}

	return false;
}

void CommandSetVehicleDiggerArmPosition(int VehicleIndex, float ArmPosition)
{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if (pVehicle)
	{
		DEV_ONLY(bool bDiggerArmFound = false);
		//loop through gadgets until we found the digger arm
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_DIGGER_ARM)
			{
				CVehicleGadgetDiggerArm *pDiggerArm = static_cast<CVehicleGadgetDiggerArm*>(pVehicleGadget);
				pDiggerArm->SetDesiredPitch( ArmPosition );
				DEV_ONLY(bDiggerArmFound = true);
			}
		}

		SCRIPT_ASSERT(bDiggerArmFound == TRUE, "SET_VEHICLE_DIGGER_ARM_POSITION - Digger arm gadget not found");
	}
}

void CommandSetVehicleTowTruckArmPosition(int VehicleIndex, float ArmPosition)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (pVehicle)
    {
        DEV_ONLY(bool bTowArmFound = false);
        //loop through gadgets until we found the tow truck arm
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
            {
                CVehicleGadgetTowArm *pDiggerArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
                pDiggerArm->SetDesiredPitchRatio(pVehicle, ArmPosition );
                DEV_ONLY(bTowArmFound = true);
            }
        }

        SCRIPT_ASSERT(bTowArmFound == TRUE, "SET_VEHICLE_TOW_TRUCK_ARM_POSITION - Tow truck arm gadget not found");
    }
}

#if __DEV
bool
#else // __DEV
void
#endif // __DEV
SnapVehiclePartToPositionHelper(eVehicleGadgetType PartToSnap, int VehicleIndex, float PartPosition, bool SnapToPosition)
{
	bool bPartFound = false;
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if(pVehicle)
    {
		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if(!pVehicle->GetVehicleWeaponMgr())
		{
			pVehicle->CreateVehicleWeaponMgr();
		}

        // Loop through gadgets until we found the part to snap.
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget && pVehicleGadget->GetType() == PartToSnap)
            {
                CVehicleGadgetDiggerArm *pDiggerArm = static_cast<CVehicleGadgetDiggerArm*>(pVehicleGadget);
                pDiggerArm->SetDesiredPitch(PartPosition);
				pDiggerArm->SetSnapToPosition(SnapToPosition);
				pDiggerArm->SetDefaultRatioOverride(true);
                bPartFound = true;
            }
        }

		if(!bPartFound && pVehicle->GetVehicleWeaponMgr())
		{
			for(int nTurret = 0; nTurret < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); nTurret++)
			{
				CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret(nTurret);

				if(pTurret && pTurret->GetType() == PartToSnap)
				{
					Matrix34 matTurrent;
					matTurrent.Identity();
					matTurrent.RotateFullZ(PartPosition);
					Vector3 vHeading = YAXIS;
					matTurrent.Transform3x3(vHeading);
					pTurret->AimAtLocalDir(vHeading, pVehicle, false, SnapToPosition);

					if(SnapToPosition)
					{
						if(pTurret->IsPhysicalTurret())
						{
							CTurretPhysical *pPhyscialTurret = (CTurretPhysical *)pTurret;

							float fAngleOut;
							if(pPhyscialTurret->GetBaseFragChild() > VEH_INVALID_ID)
							{
								pPhyscialTurret->SnapComponentToAngle(pVehicle->GetVehicleFragInst(), 
									pPhyscialTurret->GetBaseFragChild(), 
									PartPosition, 
									fAngleOut);
							}

							if(pPhyscialTurret->GetBarrelFragChild() > VEH_INVALID_ID)
							{
								pPhyscialTurret->SnapComponentToAngle(pVehicle->GetVehicleFragInst(), 
									pPhyscialTurret->GetBarrelFragChild(), 
									PartPosition, 
									fAngleOut);
							}
						}
					}
					bPartFound = true;
				}
			}
		}
    }

	DEV_ONLY(return bPartFound);
}

void CommandSetVehicleBulldozerArmPosition(int VehicleIndex, float ArmPosition, bool SnapToPosition)
{
	DEV_ONLY(bool bBulldozerArmFound =) SnapVehiclePartToPositionHelper(VGT_DIGGER_ARM, VehicleIndex, ArmPosition, SnapToPosition);
	SCRIPT_ASSERT(bBulldozerArmFound == TRUE, "SET_VEHICLE_BULLDOZER_ARM_POSITION - Bulldozer arm gadget not found");
}

void CommandSetVehicleTankTurretPosition(int VehicleIndex, float TurretPosition, bool SnapToPosition)
{
	DEV_ONLY(bool bTankTurretFound =) SnapVehiclePartToPositionHelper(VGT_TURRET_PHYSICAL, VehicleIndex, TurretPosition, SnapToPosition);
	SCRIPT_ASSERT(bTankTurretFound == TRUE, "SET_VEHICLE_TANK_TURRET_POSITION - Tank turret gadget not found");
}

void CommandSetVehicleTurretTarget( int VehicleIndex, int turretIndex, const scrVector& TargetPosition, bool SnapToPosition )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	Vector3 worldTargetPos = Vector3( TargetPosition );

	if(pVehicle)
	{
		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if(!pVehicle->GetVehicleWeaponMgr())
		{
			pVehicle->CreateVehicleWeaponMgr();
		}

		// Loop through gadgets until we found the part to snap.
		for(int nTurret = 0; nTurret < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); nTurret++)
		{
			if( turretIndex == -1 ||
				nTurret == turretIndex )
			{
				CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret(nTurret);

				if( pTurret )
				{
					pTurret->AimAtWorldPos( worldTargetPos, pVehicle, false, SnapToPosition );
				}
			}
		}
	}
}

void CommandSetVehicleTankStationary(int VehicleIndex, bool bStationary)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->GetIntelligence()->m_bStationaryInTank = bStationary;
	}
}

void CommandSetVehicleTurretSpeedThisFrame(int VehicleIndex, float TurretSpeed)
{
	DEV_ONLY(bool bTurretFound = false;) 
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	
	// create the vehicle weapon manager for the case there is no driver in the vehicle
	if(!pVehicle->GetVehicleWeaponMgr())
	{
		pVehicle->CreateVehicleWeaponMgr();
	}
	if (pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		for(int i = 0; i < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); i++)
		{
			CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret(i);
			if(pTurret && (pTurret->GetType() == VGT_TURRET_PHYSICAL ||  pTurret->GetType() == VGT_TURRET))
			{
				pTurret->SetTurretSpeedOverride(TurretSpeed);
				DEV_ONLY(bTurretFound = true;)
			}
		}
	}
	SCRIPT_ASSERT(bTurretFound == TRUE, "SET_VEHICLE_TURRET_SPEED - Turret gadget not found");
}


void CommandDisableTurretMovementThisFrame( int VehicleIndex )
{
	DEV_ONLY(bool bTurretFound = false;) 
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	
	// create the vehicle weapon manager for the case there is no driver in the vehicle
	if(!pVehicle->GetVehicleWeaponMgr())
	{
		pVehicle->CreateVehicleWeaponMgr();
	}
	if (pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		for(int i = 0; i < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); i++)
		{
			CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret(i);
			if(pTurret && (pTurret->GetType() == VGT_TURRET_PHYSICAL ||  pTurret->GetType() == VGT_TURRET))
			{
				pTurret->GetFlags().SetFlag( CTurret::TF_DisableMovement );
				DEV_ONLY(bTurretFound = true;)
			}
		}
	}
	SCRIPT_ASSERT(bTurretFound == TRUE, "DISABLE_VEHICLE_TURRET_MOVEMENT - Turret gadget not found");
}

void CommandSetVerticalFlightNozzlePosition(int VehicleIndex, float NozzleRatio)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (pVehicle)
    {
        if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_VEHICLE_FLIGHT_NOZZLE_POSITION Vehicle is not a plane"))
        {
            CPlane *pPlane = static_cast<CPlane*>(pVehicle);
            if(SCRIPT_VERIFY(pPlane->GetVerticalFlightModeAvaliable(), "SET_VEHICLE_FLIGHT_NOZZLE_POSITION Vehicle does not have vertical flight mode"))
            {
                pPlane->SetDesiredVerticalFlightModeRatio(NozzleRatio);
            }
        }
    }
}

void CommandSetVerticalFlightNozzlePositionImmediate(int VehicleIndex, float NozzleRatio)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_VEHICLE_FLIGHT_NOZZLE_POSITION_IMMEDIATE Vehicle is not a plane"))
		{
			CPlane *pPlane = static_cast<CPlane*>(pVehicle);
			if(SCRIPT_VERIFY(pPlane->GetVerticalFlightModeAvaliable(), "SET_VEHICLE_FLIGHT_NOZZLE_POSITION_IMMEDIATE Vehicle does not have vertical flight mode"))
			{
				pPlane->SetVerticalFlightModeRatio(NozzleRatio);
			}
		}
	}
}

float CommandGetVerticalFlightNozzlePosition(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "GET_VEHICLE_FLIGHT_NOZZLE_POSITION Vehicle is not a plane"))
		{
			const CPlane *pPlane = static_cast<const CPlane*>(pVehicle);
			return pPlane->GetVerticalFlightModeRatio();
		}
	}
	return 0.0f;
}

void CommandSetDisableVerticalFlightModeTransition(int VehicleIndex, bool DisableTransition)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_DISABLE_VERTICAL_FLIGHT_MODE_TRANSITION Vehicle is not a plane"))
		{
			CPlane *pPlane = static_cast<CPlane*>(pVehicle);
			if(SCRIPT_VERIFY(pPlane->GetVerticalFlightModeAvaliable(), "SET_DISABLE_VERTICAL_FLIGHT_MODE_TRANSITION Vehicle does not have vertical flight mode"))
			{
				pPlane->SetDisableVerticalFlightModeTransition(DisableTransition);
			}
		}
	}
}

void CommandAttachVehicleToTowTruck(int iTowTruckIndex, int iVehicleIndex, int iVehicleBoneIndex, const scrVector & scrVecAttachPointOffset)
{
    CVehicle* pTowTruck = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTowTruckIndex);
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    Vector3 vPoint =  Vector3 (scrVecAttachPointOffset);

    if(pTowTruck && pVehicle)
    {
        DEV_ONLY(bool bTowArmFound = false);
        //loop through gadgets until we found the tow truck arm
        for(int i = 0; i < pTowTruck->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pTowTruck->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
            {
				vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CommandAttachVehicleToTowTruck - attaching vehicle to tow truck.");
                CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
                pTowArm->AttachVehicleToTowArm( pTowTruck, pVehicle, static_cast<s16>(iVehicleBoneIndex), vPoint);
                DEV_ONLY(bTowArmFound = true);
            }
        }

        SCRIPT_ASSERT(bTowArmFound == TRUE, "SET_VEHICLE_TOW_TRUCK_ARM_POSITION - Tow truck arm gadget not found");
    }
}

void CommandSetVehicleBurnout(int iVehicleIndex, bool bEnableBurnout)
{
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    if(pVehicle)
    {
        if (pVehicle->InheritsFromAutomobile())
        {
            CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
            pAutomobile->EnableBurnoutMode(bEnableBurnout);
        }
    }
}


bool CommandIsVehicleInBurnout(int iVehicleIndex)
{
	bool bInBurnout = false;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pVehicle)
	{
		if (pVehicle->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			bInBurnout = pAutomobile->IsInBurnout();
		}
	}

	return bInBurnout;
}


void CommandSetVehicleReduceGrip(int iVehicleIndex, bool bEnableReduceGrip)
{
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    if(pVehicle)
    {
        if (pVehicle->InheritsFromAutomobile())
        {
            CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
            pAutomobile->EnableReduceGripMode(bEnableReduceGrip);
        }
    }
}


void CommandSetVehicleReduceGripLevel(int iVehicleIndex, int reducedGripLevel)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			if( scriptVerifyf( reducedGripLevel >= 0 && reducedGripLevel < 4, "SET_VEHICLE_REDUCE_GRIP_LEVEL: passing in invalid grip level range is ( 0 - 3 ): %d", reducedGripLevel ) )
			{
				CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
				pAutomobile->SetReduceGripLevel( reducedGripLevel );
			}
		}
	}
}

void CommandSetReducedSuspensionForce(int iVehicleIndex, bool reduceSuspensionForce)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			pAutomobile->SetReducedSuspensionForce( reduceSuspensionForce );
		}
	}
}

bool CommandGetIsReducedSuspensionForceSet(int iVehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

	if( pVehicle )
	{
		if (pVehicle)
		{
			//loop through the wheels and see if any wheels have reduced suspension force
			for(s32 index = 0; index < pVehicle->GetNumWheels(); index++)
			{
				CWheel *wheel = pVehicle->GetWheel(index);
				if(wheel && wheel->GetReducedSuspensionForce())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CommandDetachVehicleFromTowTruck(int iTowTruckIndex, int iVehicleIndex)
{
    CVehicle* pTowTruck = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTowTruckIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
    if(pTowTruck && pVehicle)
    {
        DEV_ONLY(bool bTowArmFound = false);
        //loop through gadgets until we found the tow truck arm
        for(int i = 0; i < pTowTruck->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pTowTruck->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
            {
				vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CommandDetachVehicleFromTowTruck - Detaching entity for script %s.", CTheScripts::GetCurrentScriptName());

                CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
                pTowArm->DetachVehicle( pTowTruck );
                DEV_ONLY(bTowArmFound = true);
            }
        }

        SCRIPT_ASSERT(bTowArmFound == TRUE, "DETACH_VEHICLE_FROM_TOW_TRUCK - Tow truck arm gadget not found");
    }
}

bool CommandDetachVehicleFromAnyTowTruck(int iVehicleIndex)
{
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    bool bVehiclewasAttachedToTowTruck = false;
    if(pVehicle)
    {
        CVehicle::Pool* pool = CVehicle::GetPool();
        s32 i = (s32) pool->GetSize();	
        while(i--)
        {
            CVehicle* pPossibleTowVehicle = pool->GetSlot(i);
            if(pPossibleTowVehicle)
            {	
                //loop through gadgets until we found the tow truck arm
                for(int i = 0; i < pPossibleTowVehicle->GetNumberOfVehicleGadgets(); i++)
                {
                    CVehicleGadget *pVehicleGadget = pPossibleTowVehicle->GetVehicleGadget(i);

                    if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
                    {
                        CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
                        if( pTowArm->GetAttachedVehicle( ) == pVehicle )
                        {
							vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CommandDetachEntityFromAnyTowtruck - Detaching entity for script %s.", CTheScripts::GetCurrentScriptName());
                            pTowArm->DetachVehicle(pPossibleTowVehicle);
                            bVehiclewasAttachedToTowTruck = true;
                        }
                    }
                }
            }
        }
    }

    return bVehiclewasAttachedToTowTruck;
}


bool CommandIsVehicleAttachedToTowTruck(int iTowTruckIndex, int iVehicleIndex)
{
    CVehicle* pTowTruck = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iTowTruckIndex);
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    if(pTowTruck && pVehicle)
    {
        DEV_ONLY(bool bTowArmFound = false);
        //loop through gadgets until we found the tow truck arm
        for(int i = 0; i < pTowTruck->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pTowTruck->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
            {
                CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);
                if( pTowArm->GetAttachedVehicle( ) == pVehicle )
                {
                    return true;
                }

                DEV_ONLY(bTowArmFound = true);
            }
        }
        SCRIPT_ASSERT(bTowArmFound == TRUE, "IS_VEHICLE_ATTACHED_TO_TOW_TRUCK - Tow truck arm gadget not found");
    }

    return false;
}

int CommandGetEntityAttachedToTowTruck(int iTowTruckIndex)
{
    int OtherEntityIndex = NULL_IN_SCRIPTING_LANGUAGE;
    const CVehicle* pTowTruck = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iTowTruckIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if(pTowTruck)
    {
        DEV_ONLY(bool bTowArmFound = false);
        //loop through gadgets until we found the tow truck arm
        for(int i = 0; i < pTowTruck->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pTowTruck->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_TOW_TRUCK_ARM)
            {
                CVehicleGadgetTowArm *pTowArm = static_cast<CVehicleGadgetTowArm*>(pVehicleGadget);

                const CVehicle* pVeh = pTowArm->GetAttachedVehicle( );
                if (pVeh)
                {
                    OtherEntityIndex = CTheScripts::GetGUIDFromEntity(const_cast<CVehicle&>(*pVeh));
                }

                DEV_ONLY(bTowArmFound = true);
            }
        }
        SCRIPT_ASSERT(bTowArmFound == TRUE, "GET_ENTITY_ATTACHED_TO_TOW_TRUCK - Tow truck arm gadget not found");
    }

    return OtherEntityIndex;
}

void CommandDetachVehicleFromCargobob(int iCargobobIndex, int iVehicleIndex)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob() && pVehicle)
	{
		DEV_ONLY(bool bPickUpRopeFound = false);
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				vehicleDisplayf("[PICKUP ROPE DEBUG] CommandDetachVehicleFromCargobob - Detaching entity for script %s. Vehicle: %s", CTheScripts::GetCurrentScriptName(), pCargobob->GetNetworkObject() ? pCargobob->GetNetworkObject()->GetLogName() : pCargobob->GetModelName() );
				pPickUpRope->DetachEntity( pCargobob );
				DEV_ONLY(bPickUpRopeFound = true);
			}
		}

		SCRIPT_ASSERT(bPickUpRopeFound == TRUE, "DETACH_VEHICLE_FROM_CARGOBOB - Pick up rope gadget not found");
	}
}

void CommandDetachEntityFromCargobob(int iCargobobIndex, int iEntityIndex)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityIndex);

	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob() && pEntity)
	{
		DEV_ONLY(bool bPickUpRopeFound = false);
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				vehicleDisplayf("[PICKUP ROPE DEBUG] CommandDetachEntityFromCargobob - Detaching entity for script %s. Vehicle: %s", CTheScripts::GetCurrentScriptName(), pCargobob->GetNetworkObject() ? pCargobob->GetNetworkObject()->GetLogName() : pCargobob->GetModelName() );
				pPickUpRope->DetachEntity( pCargobob );
				DEV_ONLY(bPickUpRopeFound = true);
			}
		}

		SCRIPT_ASSERT(bPickUpRopeFound == TRUE, "DETACH_ENTITY_FROM_CARGOBOB - Pick up rope gadget not found");
	}
}

bool CommandDetachVehicleFromAnyCargobob(int iVehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

	bool bVehiclewasAttachedToCargobob = false;
	if(pVehicle)
	{
		CVehicle::Pool* pool = CVehicle::GetPool();
		s32 i = (s32) pool->GetSize();	
		while(i--)
		{
			CVehicle* pPossibleCargobob = pool->GetSlot(i);
			if(pPossibleCargobob && pPossibleCargobob->InheritsFromHeli() && ((CHeli *) pPossibleCargobob)->GetIsCargobob())
			{	
				//loop through gadgets until we found the pick up rope
				for(int i = 0; i < pPossibleCargobob->GetNumberOfVehicleGadgets(); i++)
				{
					CVehicleGadget *pVehicleGadget = pPossibleCargobob->GetVehicleGadget(i);

					if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
					{
						CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
						if( pPickUpRope->GetAttachedVehicle( ) == pVehicle )
						{
							if( scriptVerifyf( !pPickUpRope->GetForceNoVehicleDetach(), "[PICKUP ROPE DEBUG] CommandDetachVehicleFromAnyCargobob - trying to detach a vehicle that has been forced to not detach from script" ) )
							{
								pPickUpRope->SetForceNoVehicleDetach( false );
							}
							vehicleDisplayf("[PICKUP ROPE DEBUG] CommandDetachVehicleFromAnyCargobob - Detaching entity for script %s. Vehicle: %s", CTheScripts::GetCurrentScriptName(), pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : pVehicle->GetModelName() );
							pPickUpRope->DetachEntity(pPossibleCargobob);
							bVehiclewasAttachedToCargobob = true;
						}
					}
				}
			}
		}
	}

	return bVehiclewasAttachedToCargobob;
}


bool CommandIsVehicleAttachedToCargobob(int iCargobobIndex, int iVehicleIndex)
{
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iCargobobIndex);
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);

	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob() && pVehicle)
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				if( pPickUpRope->GetAttachedVehicle( ) == pVehicle )
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CommandIsEntityAttachedToCargobob(int iCargobobIndex, int entityIndex)
{
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iCargobobIndex);
	const CEntity* pEntity =  CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob() && pEntity)
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				if( pPickUpRope->GetAttachedEntity( ) == pEntity )
				{
					return true;
				}
			}
		}
	}
	return false;
}

int CommandGetVehcileAttachedToCargobob(int iCargobobIndex)
{
	int OtherEntityIndex = NULL_IN_SCRIPTING_LANGUAGE;
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iCargobobIndex);

	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);

				const CVehicle* pVeh = pPickUpRope->GetAttachedVehicle( );
				if (pVeh)
				{
					OtherEntityIndex = CTheScripts::GetGUIDFromEntity(const_cast<CVehicle&>(*pVeh));
				}
			}
		}
	}

	return OtherEntityIndex;
}

int CommandGetEntityAttachedToCargobob(int iCargobobIndex)
{
	int OtherEntityIndex = NULL_IN_SCRIPTING_LANGUAGE;
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iCargobobIndex);

	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);

				const CEntity* pEntity = pPickUpRope->GetAttachedEntity();
				if (pEntity)
				{
					OtherEntityIndex = CTheScripts::GetGUIDFromEntity(const_cast<CEntity&>(*pEntity));
				}
			}
		}
	}

	return OtherEntityIndex;
}

void AttachPhysicalToCargobob( CVehicle* pCargobob, CPhysical *pPhysical, s16 iboneIndex, Vector3 vPoint )
{
	if( pPhysical &&
	    pCargobob && 
	    pCargobob->InheritsFromHeli() && 
	    ( (CHeli*)pCargobob )->GetIsCargobob() )
	{
		DEV_ONLY(bool bPickUpRopeFound = false);
		
		CVehicleGadgetPickUpRope::ComputeIdealAttachmentOffset( pPhysical, vPoint );
		
			//loop through gadgets until we found the pick up rope
			for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
			{
				CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

				if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
				{
					CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
					pPickUpRope->AttachEntityToPickUpProp( pCargobob, pPhysical, iboneIndex, vPoint );
					DEV_ONLY(bPickUpRopeFound = true);
				}
			}

		SCRIPT_ASSERT( bPickUpRopeFound == TRUE, "ATTACH_TO_CARGOBOB - Pick up rope gadget not found" );
	}
}

void CommandAttachVehicleToCargobob( int iCargobobIndex, int iVehicleIndex, int iVehicleBoneIndex, const scrVector & scrVecAttachPointOffset )
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

	if( pVehicle && 
		SCRIPT_VERIFY( !NetworkUtils::IsNetworkCloneOrMigrating( pVehicle ), "ATTACH_VEHICLE_TO_CARGOBOB - The vehicle is not owned by this machine" ) )
	{
		AttachPhysicalToCargobob( pCargobob, pVehicle, static_cast<s16>(iVehicleBoneIndex), (Vector3)scrVecAttachPointOffset );
	}
}

void CommandAttachEntityToCargobob(int iCargobobIndex, int iEntityIndex, int iEntityBoneIndex, const scrVector & scrVecAttachPointOffset )
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	CEntity*  pEntity	= CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityIndex);

	if( pEntity &&
		SCRIPT_VERIFY( pEntity->GetIsPhysical(), "ATTACH_ENTITY_TO_CARGOBOB - entity is not physical" ) &&
		SCRIPT_VERIFY( !NetworkUtils::IsNetworkCloneOrMigrating( pEntity ), "ATTACH_ENTITY_TO_CARGOBOB - The entity is not owned by this machine" ) )
	{
		CPhysical *pPhysical = static_cast<CPhysical*>( pEntity ); 
		AttachPhysicalToCargobob( pCargobob, pPhysical, static_cast< s16 >( iEntityBoneIndex ), (Vector3)scrVecAttachPointOffset );
	}
}

void CommandCargobobForceNoVehicleDetach( int iCargobobIndex, bool bNoDetach )
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

	if( pCargobob && 
		pCargobob->InheritsFromHeli() && 
		( (CHeli*)pCargobob )->GetIsCargobob() )
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				pPickUpRope->SetForceNoVehicleDetach( bNoDetach );

				vehicleDisplayf("[PICKUP ROPE DEBUG] CommandCargobobForceNoVehicleDetach - %s %d", pCargobob->GetLogName(), (int)bNoDetach );
			}
		}
	}
}

void CommandCargobobSetExcludeFromPickupEntity( int iCargobobIndex, int iEntityIndex )
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);

	if( pCargobob && 
		pCargobob->InheritsFromHeli() && 
		( (CHeli*)pCargobob )->GetIsCargobob() )
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				CPhysical *pPhysical = NULL; 

				if( iEntityIndex >= 0 )
				{
					CEntity*  pEntity	= CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
					pPhysical = static_cast<CPhysical*>( pEntity ); 
				}

				pPickUpRope->SetExcludeEntity( pPhysical );
			}
		}
	}
}

bool CommandCanCargobobPickUpEntity( int iCargobobIndex, int iEntityIndex )
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	bool bCanBePickedUp	= false;

	if( pCargobob && 
		pCargobob->InheritsFromHeli() && 
		( (CHeli*)pCargobob )->GetIsCargobob() )
	{
		bCanBePickedUp	= true;

		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				CEntity*  pEntity		= CTheScripts::GetEntityToModifyFromGUID<CEntity>(iEntityIndex);
				
				bool bIsRootBrokenOff	= pEntity && pEntity->GetFragInst() && pEntity->GetFragInst()->GetCacheEntry() && pEntity->GetFragInst()->GetCacheEntry()->IsGroupBroken(0);
				bool bIsMoveable		= pEntity && pEntity->GetFragInst() && pEntity->GetFragInst()->GetTypePhysics() && pEntity->GetFragInst()->GetTypePhysics()->GetMinMoveForce() == 0.0f;

				if(pEntity && pEntity->GetIsTypeObject() && !pEntity->GetIsAnyFixedFlagSet() && !pEntity->GetIsFixedRoot() && !bIsRootBrokenOff && bIsMoveable && !((CObject *)pEntity)->m_nObjectFlags.bIsPickUp)
				{
					CObject *pObject = static_cast<CObject*>(pEntity);

					const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pObject->GetBaseModelInfo()->GetModelNameHash());
					bool bCanBePickedUp = pTuning == NULL || pTuning->GetCanBePickedUpByCargobob();

					if( bCanBePickedUp )
					{
						bCanBePickedUp = pPickUpRope->GetExcludeEntity() != static_cast<CPhysical*>( pEntity );
					}
				}
			}
		}
	}
	return bCanBePickedUp;
}

scrVector CommandGetAttachedPickUpHookPosition(int iVehicleIndex)
{
	// Return zero if the prop hasn't loaded yet.
	Vector3 retVal(0.0f, 0.0f, 0.0f);

	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleIndex);

	if(pVehicle)
	{
		DEV_ONLY(bool bPickUpRopeFound = false);

		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
				const CEntity* pHookEntity = pPickUpRope->GetPropObject();
				if(pHookEntity)
				{
					retVal = VEC3V_TO_VECTOR3(pHookEntity->GetTransform().GetPosition());					
				}	

				DEV_ONLY(bPickUpRopeFound = true);
			}
		}

		SCRIPT_ASSERT(bPickUpRopeFound == TRUE, "GET_ATTACHED_HOOK_POSITION - Pick up rope gadget not found");
	}

	return retVal;
}

bool CommandDoesCargobobHavePickUpRope(int iCargobobIndex)
{
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE))
			{
				return true;
			}
		}
	}
	return false;
}

void CommandCreatePickUpRopeForCargobob(int iCargobobIndex, s32 nPickupType = PICKUP_HOOK)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		bool HasRopeAndHook = false;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				HasRopeAndHook = true;
				break;
			}
		}

		if(!HasRopeAndHook)
		{
			((CHeli *)pCargobob)->SetPickupRopeType((ePickupRopeGadgetType)nPickupType);
			((CHeli *)pCargobob)->AddPickupRope();
		}
	}
}

void CommandRemovePickUpRopeForCargobob(int iCargobobIndex)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		((CHeli *)pCargobob)->RemoveRopesAndHook();
	}
}

void CommandSetPickUpRopeLengthForCargobob(int iCargobobIndex, float fDetachedRopeLength, float fAttachedRopeLength, bool bSetRopeLengthInstantly)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget == NULL)
		{
			pVehicleGadget = ((CHeli *)pCargobob)->AddPickupRope();
		}

		if(pVehicleGadget)
		{
			((CVehicleGadgetPickUpRope *)pVehicleGadget)->SetRopeLength(fDetachedRopeLength, fAttachedRopeLength, bSetRopeLengthInstantly);
		}
	}
}

void CommandSetPickUpRopeLengthWithoutCreatingRopeForCargobob(int iCargobobIndex, float fDetachedRopeLength, float fAttachedRopeLength)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			((CVehicleGadgetPickUpRope *)pVehicleGadget)->SetRopeLength(fDetachedRopeLength, fAttachedRopeLength, false);
		}
		else
		{
			static_cast< CHeli* >( pCargobob )->SetInitialRopeLength( fDetachedRopeLength );
		}
	}
}


void CommandSetCargobobPickupRopeDampingMultiplier(int iCargobobIndex, float fMultiplier)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iCargobobIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && ((CHeli *) pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			((CVehicleGadgetPickUpRope *)pVehicleGadget)->m_fDampingMultiplier = fMultiplier;
		}
	}
}

///// CARGOBOB MAGNET COMMANDS

void CommandSetCargobobPickupRopeType(int nVehicleIndex, int nType)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		static_cast<CHeli *>(pCargobob)->SetPickupRopeType((ePickupRopeGadgetType)nType);
	}
}

bool CommandDoesCargobobHaveMagnet(int nVehicleIndex)
{
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<const CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				return true;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}		
	}

	return false;
}


void CommandSetCargobobMagnetActive(int nVehicleIndex, bool bActive)
{
	Displayf("[MAGNET DEBUG] CommandSetCargobobMagnetActive - bActive: %i", bActive);

	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			if(static_cast<CVehicleGadgetPickUpRopeWithMagnet*>(pVehicleGadget)->GetIsActive() != bActive)
				static_cast<CVehicleGadgetPickUpRopeWithMagnet*>(pVehicleGadget)->Trigger();
		}
	}
}


int CommandGetCargobobMagnetTargetEntity(int nVehicleIndex)
{
	const CVehicle* pCargobob = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<const CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			CPhysical *pTarget = static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->GetTargetEntity();
			if(pTarget)
				return CTheScripts::GetGUIDFromEntity(*pTarget);
		}
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}


void CommandSetCargobobMagnetStrength(int nVehicleIndex, float fMagnetStrength)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fMagnetStrength = fMagnetStrength;
		}
	}
}

void CommandSetCargobobMagnetFalloff(int nVehicleIndex, float fMagnetFalloff)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fMagnetFalloff = fMagnetFalloff;
		}
	}
}

void CommandSetCargobobMagnetReducedStrength(int nVehicleIndex, float fMagnetStrength)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fReducedMagnetStrength = fMagnetStrength;
		}
	}
}

void CommandSetCargobobMagnetReducedFalloff(int nVehicleIndex, float fMagnetFalloff)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fReducedMagnetFalloff = fMagnetFalloff;
		}
	}
}

void CommandSetCargobobMagnetPullStrength(int nVehicleIndex, float fMagnetPullStrength)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fMagnetPullStrength = fMagnetPullStrength;
		}
	}
}

void CommandSetCargobobMagnetPullRopeLength(int nVehicleIndex, float fMagnetPullRopeLength)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fMagnetPullRopeLength = fMagnetPullRopeLength;
		}
	}
}

void CommandSetCargobobMagnetSetTargetedMode(int nVehicleIndex, int nTargetEntityIndex)
{
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			CPhysical *pTargetEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(nTargetEntityIndex);		
			if(pTargetEntity)
			{
				Displayf("[MAGNET DEBUG] CommandSetCargobobMagnetSetTargetedMode - pTargetEntity: %s", pTargetEntity->GetNetworkObject() ? pTargetEntity->GetNetworkObject()->GetLogName() : pTargetEntity->GetDebugName());
				static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->SetTargetedMode(pTargetEntity);
			}
		}
	}
}

void CommandSetCargobobMagnetSetAmbientMode(int nVehicleIndex, bool bAffectsVehicles, bool bAffectsObjects)
{
		Displayf("[MAGNET DEBUG] CommandSetCargobobMagnetSetAmbientMode");
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->SetAmbientMode(bAffectsVehicles, bAffectsObjects);
		}
	}
}

void CommandSetCargobobMagnetEnsurePickupEntityUpright(int nVehicleIndex, bool bPickupEntityUpright)
{
	Displayf("[MAGNET DEBUG] CommandSetCargobobMagnetEnsurePickupEntityUpright bPickupEntityUpright: %i", bPickupEntityUpright);
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			static_cast<CVehicleGadgetPickUpRope *>(pVehicleGadget)->m_bEnsurePickupEntityUpright = bPickupEntityUpright;
		}
	}
}


void CommandSetCargobobMagnetEffectRadius(int nVehicleIndex, float fRadius)
{
	Displayf("[MAGNET DEBUG] CommandSetCargobobMagnetEffectRadius %f", fRadius);
	CVehicle* pCargobob = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(nVehicleIndex);
	if(pCargobob && pCargobob->InheritsFromHeli() && static_cast<CHeli *>(pCargobob)->GetIsCargobob())
	{
		CVehicleGadget *pVehicleGadget = NULL;

		for(int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++)
		{
			pVehicleGadget = pCargobob->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				break;
			}
			else
			{
				pVehicleGadget = NULL;
			}
		}

		if(pVehicleGadget)
		{
			if(SCRIPT_VERIFY(fRadius > 0, "SET_CARGOBOB_PICKUP_MAGNET_EFFECT_RADIUS - Radius must be > 0"))
			{
				static_cast<CVehicleGadgetPickUpRopeWithMagnet *>(pVehicleGadget)->m_fMagnetEffectRadius = fRadius;
			}			
		}
	}
}

///// CARGOBOB MAGNET COMMANDS END

void CommandSetVehicleAutomaticallyAttaches(int iVehicleIndex, bool bAutomaticallyAttach, bool bScansWithNonPlayerDriver)
{
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);

    if(pVehicle) 
    {
        pVehicle->m_nVehicleFlags.bAutomaticallyAttaches = bAutomaticallyAttach;
		pVehicle->m_nVehicleFlags.bScansWithNonPlayerDriver = bScansWithNonPlayerDriver;
    }
}

bool CommandGenerateVehicleCreationPosFromPaths(const Vector3& targetPos, Vector3& vResult, Vector3& vResultLinkDir, float DesiredHeadingDegrees, float DesiredHeadingToleranceDegrees, float MinCreationDistance, bool bIncludeSwitchedOffNodes, bool bNoWater, bool bAllowAgainstTraffic)
{
	return CVehiclePopulation::ScriptGenerateVehCreationPosFromPaths(targetPos, DesiredHeadingDegrees, DesiredHeadingToleranceDegrees, MinCreationDistance, bIncludeSwitchedOffNodes, bNoWater, bAllowAgainstTraffic, vResult, vResultLinkDir);
}


void CommandSetVehicleIndicatorLights(int VehicleIndex, bool bLeft, bool bOn)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if( bLeft )
		{
			pVehicle->m_nVehicleFlags.bLeftIndicator = bOn;
		}
		else
		{
			pVehicle->m_nVehicleFlags.bRightIndicator = bOn;
		}
	}
}

void CommandSetVehicleBrakeLights(int VehicleIndex, bool bOn)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		if( bOn	)
		{
			pVehicle->m_nVehicleFlags.bForceBrakeLightOn = true;
		}
		else
		{
			pVehicle->m_nVehicleFlags.bSuppressBrakeLight = true;
		}
	}
}

#if RSG_GEN9
void CommandSetVehicleTailLights(int VehicleIndex, bool bOn)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		scriptDebugf1("%s: SET_VEHICLE_TAIL_LIGHTS: Setting tail lights to %s for vehicle: %s this frame" , 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), bOn ? "On" : "Off", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : pVehicle->GetDebugName());

		if (bOn)
		{
			pVehicle->m_nVehicleFlags.bForceTailLightsOn = true;
		}
		else
		{
			pVehicle->m_nVehicleFlags.bSuppressTailLights = true;
		}
	}
}
#endif // RSG_GEN9

void CommandSetVehicleHandbrake(int VehicleIndex, bool bOn)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetHandBrake(bOn);
		pVehicle->m_nVehicleFlags.bScriptSetHandbrake = bOn;

		scriptDebugf1("%s: SET_VEHICLE_HANDBRAKE: Setting handbrake %s for vehicle: %s" , 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), bOn ? "On" : "Off", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : pVehicle->GetDebugName());
		NOTFINAL_ONLY(scrThread::PrePrintStackTrace();)
	}
}

void CommandSetVehicleBrake(int VehicleIndex, bool fBrakeAmount)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetBrake(fBrakeAmount);
	}
}

void CommandInstantlyFillVehiclePopulation()
{
	CVehiclePopulation::InstantFillPopulation();
}

bool CommandHasInstantFillVehiclePopulationFinished()
{
	return CVehiclePopulation::HasInstantFillPopulationFinished();
}

void CommandNetworkAllowEmptyCrowdingVehiclesRemoval(bool bEnableThisFrame)
{
	CVehiclePopulation::SetScriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame(bEnableThisFrame);
}

void CommandNetworkCapEmptyCrowdingVehiclesRemoval(int capNum)
{
	CVehiclePopulation::SetScriptCapEmptyCrowdingPlayersInNetGamesThisFrame(capNum);
}

bool CommandGetVehicleTrailerVehicle(int VehicleIndex, int &TrailerIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		// See if we have a connected vehicle that is a trailer
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
			{
				CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);
				CTrailer	*pTrailer = pTrailerAttachPoint->GetAttachedTrailer(pVehicle);
				if(pTrailer)
				{
					// Get it's index
					TrailerIndex = CTheScripts::GetGUIDFromEntity(*pTrailer);
					return true;
				}
			}
		}
	}
	return false;
}

void CommandSetTrailerAttachmentEnabled(int VehicleIndex, bool bEnabled)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle && scriptVerifyf( pVehicle->InheritsFromTrailer(), "SET_TRAILER_ATTACHMENT_ENABLED - vehicle is not a trailer!") )
	{
		CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
		pTrailer->SetTrailerAttachmentEnabled(bEnabled);
	}
}

void CommandOpenBombBayDoors(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a bomb bay
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOMBBAY)
            {
                CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);
                pBombBay->OpenDoors(pVehicle);
            }
        }
    }
}

void CommandCloseBombBayDoors(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a bomb bay
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOMBBAY)
            {
                CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);
                pBombBay->CloseDoors(pVehicle);
            }
        }
    }
}

bool CommandGetAreBombBayDoorsOpen(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		// See if we have a bomb bay
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_BOMBBAY)
			{
				CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);
				return pBombBay->GetOpen();
			}
		}
	}
	return false;
}


void CommandSwingOutBoatBoom(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                pBoatBoom->SwingOutBoatBoom(pVehicle);
            }
        }
    }
}

void CommandSwingInBoatBoom(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                pBoatBoom->SwingInBoatBoom(pVehicle);
            }
        }
    }
}

void CommandSwingBoatBoomToRatio(int VehicleIndex, float fTargetRatio)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                pBoatBoom->SwingToRatio(pVehicle, fTargetRatio);
            }
        }
    }
}

void CommandSwingBoatBoomFreely(int VehicleIndex, bool bSwingFreely)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                pBoatBoom->SetSwingFreely(pVehicle, bSwingFreely);
            }
        }
    }
}


void CommandAllowBoatBoomToAnimate(int VehicleIndex, bool bAllowBoomToAnimate)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                pBoatBoom->SetAllowBoomToAnimate(pVehicle, bAllowBoomToAnimate);
            }
        }
    }
}



float CommandGetBoatBoomPositionRatio(int VehicleIndex)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

    if(pVehicle)
    {
        // See if we have a boat boom
        for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
        {
            CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

            if(pVehicleGadget->GetType() == VGT_BOATBOOM)
            {
                CVehicleGadgetBoatBoom *pBoatBoom = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);
                return pBoatBoom->GetPositionRatio();
            }
        }
    }

    SCRIPT_ASSERT(0, "Vehicle doesn't have a boat boom");
    return 0.0f;
}

int CommandAddRoadNodeSpeedZone(const scrVector & center, float radius, float maxSpeed, bool bAllowAffectMissionVehs)
{
	scrDisplayf("Script %s adding speed zone", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	Vector3 VecCenter =  Vector3(center);
	CScriptResource_SpeedZone speedZone(VecCenter, radius, maxSpeed, bAllowAffectMissionVehs);
	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(speedZone);
}

bool CommandRemoveRoadNodeSpeedZone(int index)
{
	bool retVal = CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(index, false, true, CGameScriptResource::SCRIPT_RESOURCE_SPEED_ZONE);

	scrDisplayf("Script %s removing speed zone", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	if (!retVal)
	{
		scrWarningf("REMOVE_ROAD_NODE_SPEED_ZONE--tried to delete a speed zone that doesn't exist.  Doing nothing instead.");
	}
	return retVal;
}

bool CommandIsVehicleSearchLightOn(int VehicleIndex)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
	
		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if(!pWeaponMgr)
		{
			pVehicle->CreateVehicleWeaponMgr();
			pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		}

		if(scriptVerifyf(pWeaponMgr, "Vehicle has no weapon manager:%s",pVehicle->GetModelName()))
		{
			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon)
				{
					if(pWeapon->GetType() == VGT_SEARCHLIGHT)
					{
						CSearchLight* pSearchLight = static_cast<CSearchLight*>(pWeapon);
						return pSearchLight->GetLightOn() && !pSearchLight->GetIsDamaged();
					}
				}
			}
		}
	}

	SCRIPT_ASSERT(0, "Vehicle doesn't have a searchlight");
	return false;
}

void CommandSetVehicleSearchLight(int VehicleIndex, bool On, bool AlwaysOn)
{
	CVehicle *pVehicle;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();

		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if(!pWeaponMgr)
		{
			pVehicle->CreateVehicleWeaponMgr();
			pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		}

		if(SCRIPT_VERIFY(pWeaponMgr, "Vehicle has no weapon manager"))
		{

			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon)
				{
					if(pWeapon->GetType() == VGT_SEARCHLIGHT)
					{
						CSearchLight* pSearchLight = static_cast<CSearchLight*>(pWeapon);
						
						// if we are trying to turn this light on, but it is damaged, don't allow it
						if(pSearchLight->GetIsDamaged())
						{
							On = false;
							AlwaysOn = false;
						}
						
						pSearchLight->SetLightOn(On);
						pSearchLight->SetForceLightOn(AlwaysOn);
						return;
					}
				}
			}
		}
	}

	SCRIPT_ASSERT(0, "Vehicle doesn't have a searchlight");
}

bool CommandDoesVehicleHaveSearchLight(int VehicleIndex)
{
	CVehicle *pVehicle;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();

		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if (!pWeaponMgr)
		{
			pVehicle->CreateVehicleWeaponMgr();
			pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		}

		if (SCRIPT_VERIFY(pWeaponMgr, "Vehicle has no weapon manager"))
		{
			for (int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if (pWeapon)
				{
					if (pWeapon->GetType() == VGT_SEARCHLIGHT)
					{
						return true;
					}
				}
			}
		}
	}

	return false;

}

int CommandGetNumModKits(int VehicleIndex)
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		CVehicleModelInfo* pMi = pVehicle->GetVehicleModelInfo();
		return pMi->GetNumModKits();
	}

	return 0;
}

void CommandSetVehicleModKit(int VehicleIndex, int kitIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleModelInfo* pMi = pVehicle->GetVehicleModelInfo();
		if (SCRIPT_VERIFY(kitIndex >= 0 && kitIndex < pMi->GetNumModKits(), "Invalid kit index passed to SET_VEHICLE_MOD_KIT"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			// we store the kit index into the global array in the variation
			Assert(pMi->GetModKit(kitIndex) < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount());
			variation.SetKitIndex(pMi->GetModKit(kitIndex));
		}
	}
}

int CommandGetVehicleModKit(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		return variation.GetKitIndex();
	}

	return INVALID_VEHICLE_KIT_INDEX;
}

eModKitType CommandGetVehicleModKitType(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleModelInfo* vmi = pVehicle->GetVehicleModelInfo();
		if (SCRIPT_VERIFY(vmi, "No model info for specified vehicle!"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has no mod kit set!"))
			{
				return variation.GetKit()->GetType();
			}
		}
	}

	return MKT_STANDARD;
}

eVehicleWheelType CommandGetVehicleWheelType(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
        return pVehicle->GetVariationInstance().GetWheelType(pVehicle);
	}

	return VWT_SPORT;
}

void CommandSetVehicleWheelType(int VehicleIndex, int type)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
        eVehicleWheelType wheelType = VWT_INVALID;
        if (SCRIPT_VERIFY(type >= -1 && type < VWT_MAX, "Invalid vehicle wheel type passed in to SET_VEHICLE_WHEEL_TYPE"))
            wheelType = (eVehicleWheelType)type;

        if (SCRIPT_VERIFY(wheelType != VWT_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || 
			pVehicle->IsReverseTrike() ||
			pVehicle->IsTrike(), "Can't set bike wheels on non bike vehicle"))
		{
            pVehicle->GetVariationInstance().SetWheelType(wheelType);
		}
	}
}

int CommandGetNumModColors(int colorType, bool base)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return 0;

    switch (colorType)
    {
        case VCT_METALLIC:
            return CVehicleModelInfo::GetVehicleModColors()->m_metallic.GetCount();
            break;
        case VCT_CLASSIC:
            return CVehicleModelInfo::GetVehicleModColors()->m_classic.GetCount();
            break;
        case VCT_PEARLESCENT:
            if (base)
                return CVehicleModelInfo::GetVehicleModColors()->m_pearlescent.m_baseCols.GetCount();
            else
                return CVehicleModelInfo::GetVehicleModColors()->m_pearlescent.m_specCols.GetCount();
            break;
        case VCT_MATTE:
            return CVehicleModelInfo::GetVehicleModColors()->m_matte.GetCount();
            break;
        case VCT_METALS:
            return CVehicleModelInfo::GetVehicleModColors()->m_metals.GetCount();
            break;
        case VCT_CHROME:
            return CVehicleModelInfo::GetVehicleModColors()->m_chrome.GetCount();
            break;
		case VCT_CHAMELEON:
			return CVehicleModelInfo::GetVehicleModColorsGen9() ? CVehicleModelInfo::GetVehicleModColorsGen9()->m_chameleon.GetCount() : 0;
        case VCT_NONE:
        default:
            Assertf(false, "Invalid color type (%d) passed to GET_NUM_MOD_COLORS", colorType);
    };

    return 0;
}

void CommandSetVehicleModColor1(int vehicle, int colorType, int baseColIndex, int specColIndex)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return;

    if (colorType < 0 || colorType >= VCT_MAX)
    {
        Assertf(false, "Invalid color type (%d) passed to SET_VEHICLE_MOD_COLOR_1", colorType);
        return;
    }

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        variation.SetModColor1((eVehicleColorType)colorType, baseColIndex, specColIndex);
        pVehicle->UpdateBodyColourRemapping();
	}
}

void CommandSetVehicleModColor2(int vehicle, int colorType, int baseColIndex)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return;

    if (colorType < 0 || colorType >= VCT_MAX)
    {
        Assertf(false, "Invalid color type (%d) passed to SET_VEHICLE_MOD_COLOR_2", colorType);
        return;
    }

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        variation.SetModColor2((eVehicleColorType)colorType, baseColIndex);
        pVehicle->UpdateBodyColourRemapping();
	}
}

void CommandGetVehicleModColor1(int vehicle, int& colorType, int& baseColIndex, int& specColIndex)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has no mod kit set!"))
        {
			eVehicleColorType type;
            variation.GetModColor1(type, baseColIndex, specColIndex);
			colorType = (int)type;
        }
	}
}

void CommandGetVehicleModColor2(int vehicle, int& colorType, int& baseColIndex)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has no mod kit set!"))
        {
			eVehicleColorType type;
            variation.GetModColor2(type, baseColIndex);
			colorType = (int)type;
        }
	}
}

const char* CommandGetVehicleModColor1Name(int vehicle, bool spec)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return NULL;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has no mod kit set!"))
        {
            return variation.GetModColor1Name(spec);
        }
	}

    return NULL;
}

const char* CommandGetVehicleModColor2Name(int vehicle)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return NULL;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicle);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has no mod kit set!"))
        {
            return variation.GetModColor2Name();
        }
	}

    return NULL;
}

bool CommandHaveVehicleModsStreamedIn(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		return variation.HaveModsStreamedIn(pVehicle);
	}
	return true;
}

void CommandRemoveVehicleModKit(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		variation.SetKitIndex(INVALID_VEHICLE_KIT_INDEX);
	}
}

bool CommandIsVehicleModGen9Exclusive(int VehicleIndex, int modSlot, int modIndex)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		const CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		if (variation.IsGen9ExclusiveVehicleMod((eVehicleModType)modSlot, (u8)modIndex, pVehicle))
		{
			return true;
		}
	}

	return false;
}

void CommandSetVehicleMod(int VehicleIndex, int modSlot, int modIndex, bool variation)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((modSlot >= 0 && modSlot < VMT_TOGGLE_MODS) || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS, "Invalid mod slot passed to SET_VEHICLE_MOD"))
		{
			CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			var.SetModIndexForType((eVehicleModType)modSlot, (u8)modIndex, pVehicle, variation, (u32)streamingFlags);
		}
	}
}

int CommandGetVehicleMod(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((modSlot >= 0 && modSlot < VMT_TOGGLE_MODS) || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS, "Invalid mod slot passed to GET_VEHICLE_MOD"))
		{
			bool bVariation = false;
			CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
			const u8 modIdx = var.GetModIndexForType((eVehicleModType)modSlot, pVehicle, bVariation);
			return (modIdx == INVALID_MOD) ? -1 : (int)modIdx;
		}
	}
	return -1;
}

int CommandGetVehicleModVariation(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((modSlot >= 0 && modSlot < VMT_TOGGLE_MODS) || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS, "Invalid mod slot passed to GET_VEHICLE_MOD_VARIATION"))
		{
			bool bVariation = false;
			CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
			var.GetModIndexForType((eVehicleModType)modSlot, pVehicle, bVariation);
			return bVariation ? 1 : 0;
		}
	}
	return 0;
}

int CommandGetNumVehicleMods(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((modSlot >= 0 && modSlot < VMT_TOGGLE_MODS) || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS, "Invalid mod slot passed to GET_NUM_VEHICLE_MODS"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			return (int)variation.GetNumModsForType((eVehicleModType)modSlot, pVehicle);
		}
	}
	return 0;
}

void CommandRemoveVehicleMod(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY((modSlot >= 0 && modSlot < VMT_TOGGLE_MODS) || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS, "Invalid mod slot passed to REMOVE_VEHICLE_MOD"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			variation.SetModIndexForType((eVehicleModType)modSlot, INVALID_MOD, pVehicle, false, (u32)streamingFlags);
		}
	}
}

void CommandToggleVehicleMod(int VehicleIndex, int modSlot, bool toggleOn)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS, "Invalid mod slot passed to TOGGLE_VEHICLE_MOD"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			variation.ToggleMod((eVehicleModType)modSlot, toggleOn);
		}
	}
}

bool CommandIsToggleModOn(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS, "Invalid mod slot passed to IS_TOGGLE_MOD_ON"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			return variation.IsToggleModOn((eVehicleModType)modSlot);
		}
	}
	return false;
}

const char* CommandGetModTextLabel(int VehicleIndex, int modSlot, int modIndex)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (modSlot >= 0 && (modSlot < VMT_RENDERABLE || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
            return variation.GetModShopLabelForType((eVehicleModType)modSlot, modIndex, pVehicle);
		}
	}

	return NULL;
}

const char* CommandGetModSlotName(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (modSlot < VMT_RENDERABLE)
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
            if (variation.GetKit())
            {
                return variation.GetKit()->GetModSlotName((eVehicleModType)modSlot);
            }
		}
	}

	return NULL;
}

const char* CommandGetLiveryName(int VehicleIndex, int livery)
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (livery >= 0)
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (variation.GetKit())
			{
				return variation.GetKit()->GetLiveryName((u32)livery);
			}
		}
	}

	return NULL;
}

const char* CommandGetLivery2Name(int VehicleIndex, int livery2)
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		if (livery2 >= 0)
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (variation.GetKit())
			{
				return variation.GetKit()->GetLivery2Name((u32)livery2);
			}
		}
	}

	return NULL;
}

u32 CommandGetVehicleModModifierValue(int VehicleIndex, int modSlot, int modIndex)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		const u32 modelNameHash = pVehicle->GetModelNameHash();
		if (SCRIPT_VERIFY((modSlot >= VMT_STAT_MODS && modSlot < VMT_TOGGLE_MODS) || ( modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && !(pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))), "Invalid mod slot passed to GET_VEHICLE_MOD_MODIFIER_VALUE"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has nod mod kit set!"))
			{
				return variation.GetModifierForType((eVehicleModType)modSlot, modIndex);
			}
		}
	}
	return 0;
}

u32 CommandGetVehicleModIdentifierHash(int VehicleIndex, int modSlot, int modIndex)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if (pVehicle)
	{
		const u32 modelNameHash = pVehicle->GetModelNameHash();
		if (SCRIPT_VERIFY((modSlot >= VMT_STAT_MODS && modSlot < VMT_TOGGLE_MODS) || ( modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && !(pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))), "Invalid mod slot passed to GET_VEHICLE_MOD_IDENTIFIER_HASH"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (SCRIPT_VERIFY(variation.GetKit(), "Specified vehicle has nod mod kit set!"))
			{
				return variation.GetIdentifierHashForType((eVehicleModType)modSlot, modIndex);
			}
		}
	}
	return 0;
}

int CommandGetCameraPositionForMod(int VehicleIndex, int modSlot)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(modSlot < VMT_RENDERABLE, "Invalid mod slot passed to GET_CAMERA_POSITION_FOR_MOD"))
		{
			bool dummy = false;
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			const u8 modIdx = variation.GetModIndexForType((eVehicleModType)modSlot, pVehicle, dummy);
			if (modIdx != INVALID_MOD && modIdx < CVehicleModelInfo::GetVehicleColours()->m_Kits[variation.GetKitIndex()].GetVisibleMods().GetCount())
			{
				return CVehicleModelInfo::GetVehicleColours()->m_Kits[variation.GetKitIndex()].GetVisibleMods()[modIdx].GetCameraPos();
			}
		}
	}
	return VMCP_DEFAULT;
}

void CommandPreloadVehicleMod(int VehicleIndex, int modSlot, int modIndex)
{
	modSlot = ConvertVehicleModTypeFromScript((eVehicleModType_Script)modSlot);
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(modSlot >= 0 && (modSlot < VMT_RENDERABLE || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS || modSlot == VMT_HYDRAULICS), "Invalid mod slot passed to PRELOAD_VEHICLE_MOD"))
		{
			CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			variation.SetPreloadModForType((eVehicleModType)modSlot, (u8)modIndex, pVehicle);
		}
	}
}

bool CommandHasPreloadModsFinished(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        return variation.HasPreloadModsFinished();
	}

    return true;
}

void CommandReleasePreloadMods(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
        CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
        variation.CleanUpPreloadMods(pVehicle);
	}
}

bool CommandCanVehicleBeModded(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		return !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CANNOT_BE_MODDED);
	}
	return false;
}

void CommandSetVehicleTyreSmokeColor(int VehicleIndex, int red, int green, int blue)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		variation.SetSmokeColorR((u8)red);
		variation.SetSmokeColorG((u8)green);
		variation.SetSmokeColorB((u8)blue);
	}
}

void CommandGetVehicleTyreSmokeColor(int VehicleIndex, int& red, int& green, int& blue)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		const CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		red = (int)variation.GetSmokeColorR();
		green = (int)variation.GetSmokeColorG();
		blue = (int)variation.GetSmokeColorB();
	}
}

void CommandSetVehicleWindowTint(int VehicleIndex, int colorIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		scriptDebugf1("SET_VEHICLE_WINDOW_TINT(%d, %d) called for vehicle %p", VehicleIndex, colorIndex, pVehicle);
		CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
		variation.SetWindowTint((u8)colorIndex);
	}
}

int CommandGetVehicleWindowTint(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		CVehicleVariationInstance const& variation = pVehicle->GetVariationInstance();
		scriptDebugf1("GET_VEHICLE_WINDOW_TINT(%d): Vehicle %p has tint %d", VehicleIndex, pVehicle, variation.GetWindowTint());
		if (variation.GetWindowTint() != 0xff)
			return variation.GetWindowTint();
	}
	return -1;
}

int CommandGetNumVehicleWindowTints()
{
	return CVehicleModelInfo::GetVehicleColours()->m_WindowColors.GetCount();
}

void CommandGetVehicleColor(int VehicleIndex, int &Red, int &Green, int &Blue)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		Red = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1()).GetRed();
		Green = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1()).GetGreen();
		Blue = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1()).GetBlue();
	}
}

s32 CommandGetVehicleColoursWhichCanBeSet(s32 VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	s32 ColourBitField = 0;

	if (pVehicle)
	{
		CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
		if (SCRIPT_VERIFY(pShader, "GET_VEHICLE_COLOURS_WHICH_CAN_BE_SET - failed to get vehicle shader"))
		{
			if(pShader->HasBodyColor1())
			{	// vehicle supports bodycolor1
				ColourBitField |= 0x01;
			}

			if(pShader->HasBodyColor2())
			{	// vehicle supports bodycolor2
				ColourBitField |= 0x02;
			}

			if(pShader->HasBodyColor3())
			{
				ColourBitField |= 0x04;
			}

			if(pShader->HasBodyColor4())
			{
				ColourBitField |= 0x08;
			}

			if(pShader->HasBodyColor5())
			{
				ColourBitField |= 0x10;
			}
		}
	}

	return ColourBitField;
}

int CommandGetVehicleSourceOfDestruction(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		CEntity* pSource = pVehicle->GetSourceOfDestruction();
		if(pSource)
		{
			return CTheScripts::GetGUIDFromEntity(*pSource);
		}
	}
	
	return NULL_IN_SCRIPTING_LANGUAGE;
}

u32 CommandGetVehicleCauseOfDestruction(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		return pVehicle->GetCauseOfDestruction();
	}

	return 0;
}

u32 CommandGetVehicleTimeOfDestruction(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		return pVehicle->GetTimeOfDestruction();
	}

	return 0;
}

void CommandOverridePlaneDamageThreshold(int VehicleIndex, float val)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_Transmission.SetPlaneDamageThresholdOverride(val);
	}
}

bool CommandGetIsLeftHeadLightDamaged(int VehicleIndex)
{
	bool result = false;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if( !pVehicle->GetVehicleDrawHandler().GetVariationInstance().IsBoneTurnedOff( pVehicle, pVehicle->GetBoneIndex( VEH_HEADLIGHT_L ) ) )
		{
			result = pVehicle->IsLeftHeadLightDamaged();
		}
	}

	return result;
}

bool CommandGetIsRightHeadLightDamaged(int VehicleIndex)
{
	bool result = false;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if( !pVehicle->GetVehicleDrawHandler().GetVariationInstance().IsBoneTurnedOff( pVehicle, pVehicle->GetBoneIndex( VEH_HEADLIGHT_R ) ) )
		{
			result = pVehicle->IsRightHeadLightDamaged();
		}
	}

	return result;
}


bool CommandGetBothVehicleHeadlightsDamaged(int VehicleIndex)
{
    CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
    if(pVehicle)
    {
        return pVehicle->AreBothHeadLightsDamaged();
    }

    return false;
}

int CommandGetNumberOfDamagedHeadlights(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		return pVehicle->GetNumberOfDamagedHeadlights();
	}

	return 0;
}



void CommandModifyVehicleTopSpeed(int VehicleIndex, float fPercentChange)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		CVehicleFactory::ModifyVehicleTopSpeed(pVehicle, fPercentChange);

        if( fPercentChange == 0.0f )
        {
            CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
            if( variation.GetKit() )
            {
                for( s32 i = VMT_STAT_MODS; i < VMT_MISC_MODS; ++i )
                {
                    variation.SetModIndex( (eVehicleModType)i, variation.GetModIndex( (eVehicleModType)i ), pVehicle, false, 0 );
                }
            }
        }
	}
}

void CommandSetVehicleMaxSpeed( int VehicleIndex, float maxSpeed )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->SetScriptMaxSpeed( maxSpeed );

		scriptDebugf1("SET_VEHICLE_MAX_SPEED: %s [%p], ModelName: %s  MaxSpeed: %f", pVehicle->GetLogName(), pVehicle, pVehicle->GetModelName(), maxSpeed);
	}
}

//	Default setting is false
void CommandSetVehicleStaysFrozenWhenCleanedUp(int VehicleIndex, bool bStaysFrozen)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if (bStaysFrozen)
		{
			pVehicle->m_nVehicleFlags.bUnfreezeWhenCleaningUp = false;
		}
		else
		{
			pVehicle->m_nVehicleFlags.bUnfreezeWhenCleaningUp = true;
		}
	}
}


void CommandSetVehicleActAsIfHighSpeedForFragSmashing(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bActAsIfHighSpeedForFragSmashing = bVal;
	}
}

void CommandSetPedsCanFallOffThisVehicleFromLargeFallDamage(int VehicleIndex, bool bVal, float fVelThreshold)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		SCRIPT_VERIFY(pVehicle->InheritsFromBike()||pVehicle->InheritsFromQuadBike()||pVehicle->InheritsFromAmphibiousQuadBike(), "SET_PEDS_CAN_FALL_OFF_THIS_VEHICLE_FROM_LARGE_FALL_DAMAGE only makes sense on a bike or quad.");
		pVehicle->SetAllowKnockOffVehicleForLargeVertVel(bVal);
		if(fVelThreshold >= 0.0f)
		{
			pVehicle->SetVelocityThresholdBeforeKnockOffVehicle(fVelThreshold);
		}
	}
}

void CommandSetEnemyPedsForceFallOffThisVehicle(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bForceEnemyPedsFalloff = bVal;
	}
}

void CommandSetCarBlowsUpWithNoWheels(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bBlowUpWhenMissingAllWheels = bVal;
	}
}

int CommandAddVehicleCombatAngledAvoidanceArea(const scrVector & vStart, const scrVector & vEnd, float fWidth)
{
	CScriptResource_VehicleCombatAvoidanceArea vehCombatAvoidArea(VECTOR3_TO_VEC3V((Vector3)vStart), VECTOR3_TO_VEC3V((Vector3)vEnd), fWidth);
	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(vehCombatAvoidArea);
}

int CommandAddVehicleCombatSphereAvoidanceArea(const scrVector & vCenter, float fRadius)
{
	CScriptResource_VehicleCombatAvoidanceArea vehCombatAvoidArea(VECTOR3_TO_VEC3V((Vector3)vCenter), fRadius);
	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(vehCombatAvoidArea);
}

void CommandRemoveVehicleCombatAvoidanceArea(int iIndex)
{
	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA, iIndex);
}

void CommandClearVehicleCombatAvoidanceAreas()
{
	CVehicleCombatAvoidanceArea::Clear();
}

bool CommandIsAnyPedRappellingFromHeli(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle && pVehicle->InheritsFromHeli())
	{
		for (s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++ )
		{
			const CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
			if ( pPed &&
				 (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling) || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL)) )
			{
				return true;
			}
			
			const CPed* pLastPed = pVehicle->GetSeatManager()->GetLastPedInSeat(i);
			if ( pLastPed && pLastPed != pPed && pLastPed->GetMyVehicle() == pVehicle &&
				(pLastPed->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling) || pLastPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL)) )
			{
				return true;
			}
		}

		return static_cast<const CHeli*>(pVehicle)->AreRopesAttachedToNonHeliEntity();
	}

	return false;
}

void CommandSetVehicleCheatPowerIncrease(int VehicleIndex, float fIncrease)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->SetCheatPowerIncrease(fIncrease);
	}
}

void CommandSetVehicleInfluencesWantedLevel(int VehicleIndex, bool bInfluenceWantedLevel)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bInfluenceWantedLevel = bInfluenceWantedLevel;
	}
}

void CommandSetVehicleIsWanted(int VehicleIndex, bool bIsWanted)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bWanted = bIsWanted;
	}
}

void CommandSetVehicleUseAlternateHandling(int VehicleIndex, bool bUseAlternateHandling)
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
    if (pVehicle)
    {
        pVehicle->m_nVehicleFlags.bUseAlternateHandling = bUseAlternateHandling;
        if(pVehicle->m_nVehicleFlags.bUseAlternateHandling)
        {
            pVehicle->SetCheatFlag(VEH_SET_POWER1);
            pVehicle->SetCheatFlag(VEH_SET_GRIP1);
        }
        else
        {
            pVehicle->ClearCheatFlag(VEH_SET_POWER1);
            pVehicle->ClearCheatFlag(VEH_SET_GRIP1);
        }

    }
}

bool CommandGetBikersFeedOffGround(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "GET_BIKERS_FEED_OFF_GROUND - failed to get vehicle"))
	{
		if(SCRIPT_VERIFY(pVehicle->GetDriver(), "GET_BIKERS_FEED_OFF_GROUND - failed to get driver"))
		{
			return CTaskMotionInAutomobile::AreFeetOffGround(fwTimer::GetTimeStep(), *pVehicle, *pVehicle->GetDriver());
		}
	}
	return false;
}

void CommandSetBikeOnStand(int VehicleIndex, float SteerAngle, float LeanAngle)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle && pVehicle->InheritsFromBike(), "SET_BIKE_ON_STAND - not a valid bike"))
	{
		static_cast<CBike*>(pVehicle)->SetBikeOnSideStand(true, true, SteerAngle, LeanAngle);
	}
}

void CommandSetVehicleNotStealableAmbiently(int VehicleIndex, bool bStealable)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_NOT_STEALABLE_AMBIENTLY - failed to get vehicle"))
	{
		pVehicle->m_nVehicleFlags.bStealable = bStealable;
	}
}


void CommandLockDoorsWhenNoLongerNeeded(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "LOCK_DOORS_WHEN_NO_LONGER_NEEDED - failed to get vehicle") &&
		SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "LOCK_DOORS_WHEN_NO_LONGER_NEEDED - the vehicle is not a mission vehicle"))
	{
		pVehicle->m_nVehicleFlags.bLockDoorsOnCleanup = true;
	}
}

void CommandSetLastDrivenVehicle(int VehicleIndex)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_LAST_DRIVEN_VEHICLE - failed to get vehicle"))
	{
#if __BANK
		scriptDebugf1("SET_LAST_DRIVEN_VEHICLE: %s [%p], index %i", AILogging::GetDynamicEntityNameSafe(veh), veh, VehicleIndex);
#endif
		CVehiclePopulation::SetInterestingVehicle(veh);
	}
}

void ClearLastDrivenVehicle()
{
	CVehiclePopulation::ClearInterestingVehicles();
}

int CommandGetLastDrivenVehicle()
{
	int index = -1;
	CVehicle* veh = CVehiclePopulation::GetInterestingVehicle();
	if(veh)
	{
		index = CTheScripts::GetGUIDFromEntity(*veh);
	}

	return index;
}

void CommandSetVehicleHasBeenDrivenFlag(int VehicleIndex, bool b)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_HAS_BEEN_DRIVEN_FLAG - failed to get vehicle"))
	{
		veh->m_nVehicleFlags.bHasBeenDriven = b;
	}
}

void CommandSetTaskVehicleGotoPlaneMinHeightAboveTerrain(int VehicleIndex, int iMinHeightAboveTerrain)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(veh && veh->InheritsFromPlane(), "SET_TASK_VEHICLE_GOTO_PLANE_MIN_HEIGHT_ABOVE_TERRAIN - failed to get pPlane"))
	{
		CPlane* pPlane = static_cast<CPlane*>(veh);
		if(NetworkUtils::IsNetworkCloneOrMigrating(pPlane))
		{
			CScriptEntityStateChangeEvent::CSettingOfPlaneMinHeightAboveTerrainParameters parameters(iMinHeightAboveTerrain);
			CScriptEntityStateChangeEvent::Trigger(pPlane->GetNetworkObject(), parameters);					
		}
		else
		{
			CTaskVehicleMissionBase* pVehicleTask = pPlane->GetIntelligence()->GetActiveTask();
			if(pVehicleTask)
			{
				if(pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_PLANE)
				{
					CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane *>(pVehicleTask);
					pTask->SetMinHeightAboveTerrain(iMinHeightAboveTerrain);
				}
			}
		}
	}
}

void CommandSetVehicleLodMultiplier(int VehicleIndex, float multiplier)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_LOD_MULTIPLIER - failed to get vehicle"))
	{
		veh->SetLodMultiplier(multiplier);
	}
}

void CommandSetVehicleLodClamp(int VehicleIndex, int clamp)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_LOD_CLAMP - failed to get vehicle"))
	{
		SCRIPT_ASSERT(clamp >= -1 && clamp <= 255, "Invalid vehicle lod clamp set");
		veh->SetClampedRenderLod((s8)clamp);
	}
}

void CommandSetVehicleCanSaveInGarage(int VehicleIndex, bool canSave)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_CAN_SAVE_IN_GARAGE - failed to get vehicle"))
	{
		veh->SetCanSaveInGarage(canSave);
	}
}

int CommnadGetVehicleNumOfBrokenOffParts(int VehicleIndex)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "GET_VEHICLE_NUM_OF_BROKEN_OFF_PARTS - failed to get vehicle"))
	{
		return veh->GetVehicleDamage()->GetNumOfBrokenOffParts();
	}
	return 0;
}

int CommnadGetVehicleNumOfBrokenLoosenParts(int VehicleIndex)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "GET_VEHICLE_NUM_OF_BROKEN_LOOSEN_PARTS - failed to get vehicle"))
	{
		return veh->GetVehicleDamage()->GetNumOfBrokenLoosenParts();
	}
	return 0;
}

void CommandSetForceVehicleEngineDamageByBullet(int VehicleIndex, bool bForceEngineDamage)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_FORCE_VEHICLE_ENGINE_DAMAGE_BY_BULLET - failed to get vehicle"))
	{
		veh->m_nVehicleFlags.bForceEngineDamageByBullet = bForceEngineDamage;
	}
}

void CommandSetVehicleCanGenerateEngineShockingEvents(int VehicleIndex, bool bAllowedToGenerateEvents)
{
	CVehicle* pVeh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVeh, "SET_VEHICLE_GENERATES_ENGINE_SHOCKING_EVENTS - failed to get vehicle!"))
	{
		pVeh->SetShouldGenerateEngineShockingEvents(bAllowedToGenerateEvents);
	}
}

void CommandSetVehicleCustomPathNodeStreamingRadius(int VehicleIndex, float fRadius)
{
	CVehicle* pVeh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVeh, "SET_VEHICLE_CUSTOM_PATH_NODE_STREAMING_RADIUS - failed to get vehicle!"))
	{
		pVeh->GetIntelligence()->SetCustomPathNodeStreamingRadius(fRadius);
	}
}

void CommandCopyVehicleDamage(int srcVehicleIndex, int dstVehicleIndex)
{
	CVehicle* srcVeh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(srcVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	CVehicle* dstVeh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(dstVehicleIndex);
	if( SCRIPT_VERIFY(srcVeh, "COPY_VEHICLE_DAMAGES - failed to get source vehicle") &&
		SCRIPT_VERIFY(dstVeh, "COPY_VEHICLE_DAMAGES - failed to get destination vehicle") )
	{
		CVehicleDamage::Copy(srcVeh, dstVeh);
	}
}

void CommandDisableVehicleExplosionBreakOffParts()
{
	CVehicleDamage::DisableVehicleExplosionBreakOffParts();
}

void CommandLightsCutoffDistanceTweak(float dist)
{
	CVehicle::SetLightsCutoffDistanceTweak(dist);
}

void CommandSetVehicleShootAtTarget(int pedIndex, int targetEntityIndex, const scrVector & scrVecTargetCoors)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
	if(SCRIPT_VERIFY(pPed, "SET_VEHICLE_SHOOT_AT_TARGET - failed to get ped!"))
	{
		CPedWeaponManager* pPedWeaponMgr = pPed->GetWeaponManager();
		if(pPedWeaponMgr)
		{
			CVehicleWeapon* pVehicleWeapon = pPedWeaponMgr->GetEquippedVehicleWeapon();

			if(SCRIPT_VERIFY(pVehicleWeapon, "SET_VEHICLE_SHOOT_AT_TARGET - the ped does not have a vehicle weapon! Call SET_CURRENT_PED_VEHICLE_WEAPON first."))
			{
				CEntity* pEntity = NULL;
				if (NULL_IN_SCRIPTING_LANGUAGE != targetEntityIndex)
				{
					pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(targetEntityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
				}

				Vector3 vTargetCoors = Vector3(scrVecTargetCoors); 
				CVehicle* pVehicle = pPed->GetMyVehicle();
				if(SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_SHOOT_AT_TARGET - failed to get vehicle!"))
				{
					if((pEntity || !vTargetCoors.IsZero()) && pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
					{
						CHeli *pHeli = static_cast<CHeli*>(pVehicle);
						Vector3 vTargetPosition = pEntity ? VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) : vTargetCoors;
						pHeli->SetHoverModeDesiredTarget(vTargetPosition);
					}
				}

				pVehicleWeapon->Fire(pPed, pPed->GetMyVehicle(), vTargetCoors, pEntity);
			}
		}
	}
}

bool CommandGetVehicleLockOnTarget(int vehicleIndex, int& iLockOnEntity)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (veh)
	{
		CEntity *pLockOnEntity = veh->GetLockOnTarget();
		if(pLockOnEntity)
		{
			iLockOnEntity = CTheScripts::GetGUIDFromEntity(*(static_cast<CPhysical*>(pLockOnEntity)));
			return true;
		}
	}

	return false;
}

void CommandSetForceHdVehicle(int vehicleIndex, bool val)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (veh)
	{
		veh->SetScriptForceHd(val);
	}
}

eVehiclePlateType CommandGetVehiclePlateType(int vehicleIndex)
{
	const CVehicle* veh = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(vehicleIndex);
	if (veh)
	{
		return veh->GetVehicleModelInfo()->GetVehiclePlateType();
	}

	return VPT_FRONT_AND_BACK_PLATES;
}

void CommandTrackVehicleVisibility(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if (pVehicle)
	{
		pVehicle->SetUseOcclusionQuery(true);
	}
}

bool CommandIsVehicleVisible(int VehicleIndex)
{
	bool result = false;
	
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		scriptAssertf(pVehicle->GetUseOcclusionQuery(), "%s:IS_VEHICLE_VISIBLE - Vehicle visibility isn't tracked", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		fwDrawData* drawData = pVehicle->GetDrawHandlerPtr();
		if( drawData )
		{
			u32 queryId = drawData->GetOcclusionQueryId();
			
			if( queryId )
			{
				int pixelCount = OcclusionQueries::OQGetQueryResult(queryId);
				result = pixelCount > 10; // 10 pixels should be enough for everybody.
			}
		}
	}
	
	return result;
}



void CommandSetVehicleGravity(int VehicleIndex, bool bEnableGravity)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		if(bEnableGravity)
		{
			pVehicle->SetGravityForWheellIntegrator(-GRAVITY);
		}
		else
		{
			pVehicle->SetGravityForWheellIntegrator(0.0f);
		}
		
	}
}

void CommandSetEnableVehicleSlipstreaming(bool bEnableSlipstreaming)
{
	CVehicle::sm_bSlipstreamingEnabled = bEnableSlipstreaming;
}

void CommandSetVehicleSlipstreamingShouldTimeOut(bool bShouldTimeout)
{
	CVehicle::ms_bSlipstreamApplyMaxTimeBeforeBeingDisabled = bShouldTimeout;
}

float CommandGetVehicleCurrentTimeInSlipStream(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		return pVehicle->GetTimeInSlipStream();
	}

	return 0.0f;
}

bool CommandIsVehicleProducingSlipStream(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		return pVehicle->m_bProducingSlipStreamForOther;
	}

	return false;
}

void CommandSetVehicleInactiveDuringPlayback(int VehicleIndex, bool bForceInactive)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bForceInactiveDuringPlayback = bForceInactive;
		if(bForceInactive && pVehicle->GetCollider() == NULL && !pVehicle->m_nVehicleFlags.bIsDeactivatedByPlayback)
		{
			// If the vehicle is already deactivated make sure it runs the same code other deactivated-by-playback vehicles usually run
			//   when deactivated. 
			pVehicle->m_nVehicleFlags.bIsDeactivatedByPlayback = true;
			pVehicle->HandleDeactivateForPlayback();
		}
	}
}

void CommandSetVehicleActiveDuringPlayback(int VehicleIndex, bool bForceActive)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bForceActiveDuringPlayback = bForceActive;
	}
}

bool CommandIsVehicleSprayable(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
        CVehicleModelInfo* vmi = pVehicle->GetVehicleModelInfo();
        if (vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_NO_RESPRAY))
            return false;

        return true;
	}

    return false;
}

void CommandSetVehicleEngineCanDegrade(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_ENGINE_CAN_DEGRADE - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanEngineDegrade = bVal;
			pVehicle->m_nVehicleFlags.bCanPlayerAircraftEngineDegrade = bVal;
		}
	}
}

void CommandDisableVehicleDynamicAmbientScales(int VehicleIndex, int iNaturalScale, int iArtificialScale)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetUseDynamicAmbientScale(false);
		pVehicle->SetNaturalAmbientScale(iNaturalScale);
		pVehicle->SetArtificialAmbientScale(iArtificialScale);
	}
}

void CommandEnableVehicleDynamicAmbientScales(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{
		pVehicle->SetUseDynamicAmbientScale(true);
	}
}

bool CommandIsLandingGearIntact(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(SCRIPT_VERIFY(pVehicle, "IS_PLANE_LANDING_GEAR_INTACT - failed to find plane!"))
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane() || ( pVehicle->InheritsFromHeli() && static_cast< CHeli* >( pVehicle )->HasLandingGear() ), "IS_PLANE_LANDING_GEAR_INTACT - vehicle is not a plane!"))
		{
			return ((CPlane *)pVehicle)->GetIsLandingGearintact();
		}
	}
	return false;
}

bool CommnadArePlanePropellersIntact(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle, "ARE_PLANE_PROPELLERS_INTACT - failed to find plane!"))
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "ARE_PLANE_PROPELLERS_INTACT - vehicle is not a plane!"))
		{
			CPlane *pPlane = (CPlane *)pVehicle;
			for(int i = 0; i < pPlane->GetNumPropellors(); i++)
			{
				if(pPlane->GetPropellerHealth(i) <= 0.0f)
				{
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

void CommmandSetPlanePropellerHealth( int VehicleIndex, float health )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( SCRIPT_VERIFY( pVehicle, "SET_PLANE_PROPELLER_HEALTH - failed to find plane!" ) )
	{
		if( SCRIPT_VERIFY( pVehicle->InheritsFromPlane(), "SET_PLANE_PROPELLER_HEALTH - vehicle is not a plane!" ) )
		{
			CPlane *pPlane = (CPlane *)pVehicle;
			for( int i = 0; i < pPlane->GetNumPropellors(); i++ )
			{
				pPlane->SetPropellerHealth( i, health );
			}
		}
	}
}

bool CommandArePlaneControlPanelsIntact(int VehicleIndex, bool bCheckForZeroHealth)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle, "ARE_PLANE_CONTROL_PANELS_INTACT - failed to find plane!"))
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "ARE_PLANE_CONTROL_PANELS_INTACT - vehicle is not a plane!"))
		{
			return ((CPlane *)pVehicle)->AreControlPanelsIntact(bCheckForZeroHealth);
		}
	}
	return false;
}

void CommandSetVehicleCanDeformWheels(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_CAN_DEFORM_WHEELS - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanDeformWheels = bVal;
		}
	}
}

bool CommandIsVehicleStolen(int VehicleIndex)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "IS_VEHICLE_STOLEN - The vehicle is invalid."))
	{
		return (pVehicle->m_nVehicleFlags.bIsStolen);
	}

	return false;
}

void CommandSetVehicleIsStolen(int VehicleIndex, bool bVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bIsStolen = bVal;
	}
}

void CommandSetPlaneTurbulenceMulitiplier(int VehicleIndex, float fVal)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_PLANE_TURBULENCE_MULTIPLIER - Vehicle has to be a mission vehicle"))
		{
			if (SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_PLANE_TURBULENCE_MULTIPLIER - Vehicle has to be a plane"))
			{
				((CPlane *)pVehicle)->m_fScriptTurbulenceMult = Clamp(fVal, 0.0f, 1.0f);
			}
		}
	}
}

void CommandAllowAmbientVehiclesToAvoidAdverseConditions(bool /*bAllow*/)
{
	//CTaskVehicleCruiseNew::ms_bAllowAvoidAdverseConditions = bAllow;
}

bool CommandAreWingsOfPlaneIntact(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle, "ARE_WINGS_OF_PLANE_INTACT - failed to find plane!"))
	{
		if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "ARE_WINGS_OF_PLANE_INTACT - vehicle is not a plane!"))
		{
			return !((CPlane *)pVehicle)->GetAircraftDamage().HasSectionBrokenOff(((CPlane *)pVehicle), CAircraftDamage::WING_L)
				&& !((CPlane *)pVehicle)->GetAircraftDamage().HasSectionBrokenOff(((CPlane *)pVehicle), CAircraftDamage::WING_R);
		}
	}
	return false;
}

bool CommandDoesVehicleHaveWeapons(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle, "DOES_VEHICLE_HAVE_WEAPONS - failed to find vehicle!"))
	{
		// Can't simply check if m_pVehicleWeaponMgr is NULL since we only create vehicle weapons when there is a driver.
		CHandlingData* pHandling = NULL;
		pHandling = CHandlingDataMgr::GetHandlingData(pVehicle->GetVehicleModelInfo()->GetHandlingId());
		if(pHandling == NULL)
		{
			return false;
		}
		
		//B*1749538 - Return false if we don't have a vehicle weapon manager
		const CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if (pWeaponMgr == NULL)
		{
			return false;
		}

		CVehicleWeaponHandlingData* pWeaponHandlingData = pHandling->GetWeaponHandlingData();
		if(pWeaponHandlingData != NULL)
		{
			return true;
		}
	}

	return false;
}

void CommandDisableVehicleWeapon(bool bDisable,  int TypeOfWeapon, int VehicleIndex, int PedIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(SCRIPT_VERIFY(pVehicle, "DISABLE_VEHICLE_WEAPON - failed to find vehicle!"))
	{
		CVehicleWeaponMgr* pVehicleWeaponMgr = pVehicle->GetVehicleWeaponMgr();

		// create the vehicle weapon manager for the case there is no driver in the vehicle
		if(!pVehicleWeaponMgr)
		{
			pVehicle->CreateVehicleWeaponMgr();
			pVehicleWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		}

		if(scriptVerifyf(pVehicleWeaponMgr, "DISABLE_VEHICLE_WEAPON - Vehicle has no weapon manager:%s",pVehicle->GetModelName()))
		{
			s32 weaponindex = pVehicleWeaponMgr->GetVehicleWeaponIndexByHash((u32)TypeOfWeapon);
			if(weaponindex != -1)
			{
				if(PedIndex != NULL_IN_SCRIPTING_LANGUAGE)
				{
					CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
					if (pPed->GetMyVehicle() == pVehicle)
					{
						s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
						if (pVehicle->IsSeatIndexValid(iSeatIndex))
						{
							atArray<CVehicleWeapon*> weapons;
							bool bChanged = false;
							pVehicleWeaponMgr->GetWeaponsForSeat(iSeatIndex, weapons);
							for(int i= 0; i < weapons.GetCount(); ++i)
							{
								if(weapons[i]->GetHash() == (u32)TypeOfWeapon)
								{
									weapons[i]->SetEnabled(pVehicle,!bDisable);
									bChanged = true;
								}
							}

							if(bDisable && bChanged)
							{
								CPedWeaponManager *pPedWeaponMgr = pPed->GetWeaponManager();
								if (pPedWeaponMgr && pPedWeaponMgr->GetSelectedWeaponHash() == (u32)TypeOfWeapon)
								{
									pPedWeaponMgr->GetWeaponSelector()->SelectWeapon(pPed,SET_NEXT);
								}
							}
						}		
					}
				}
			}
		}
	}
}

bool CommandIsVehicleWeaponDisabled(int TypeOfWeapon, int VehicleIndex, int PedIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(SCRIPT_VERIFY(pVehicle, "IS_VEHICLE_WEAPON_DISABLED - Failed to find vehicle"))
	{
		CVehicleWeaponMgr* pVehicleWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if(SCRIPT_VERIFYF(pVehicleWeaponMgr, "IS_VEHICLE_WEAPON_DISABLED - Vehicle %s has no weapon manager",pVehicle->GetModelName()))
		{
			s32 iWeaponIndex = pVehicleWeaponMgr->GetVehicleWeaponIndexByHash((u32)TypeOfWeapon);
			if(SCRIPT_VERIFYF(iWeaponIndex != -1, "IS_VEHICLE_WEAPON_DISABLED - Could not find weapon hash %u on this vehicle", (u32)TypeOfWeapon))
			{
				if(PedIndex != NULL_IN_SCRIPTING_LANGUAGE)
				{
					CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
					if (pPed->GetMyVehicle() == pVehicle)
					{
						s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
						if (pVehicle->IsSeatIndexValid(iSeatIndex))
						{
							atArray<CVehicleWeapon*> weapons;
							pVehicleWeaponMgr->GetWeaponsForSeat(iSeatIndex, weapons);
							for(int i= 0; i < weapons.GetCount(); ++i)
							{
								if(weapons[i]->GetHash() == (u32)TypeOfWeapon)
								{
									return !weapons[i]->GetIsEnabled();
								}
							}
						}
					}
				}
			}
		}
	}

	return true;
}

void CommandSetVehicleUsedForPilotSchool(int VehicleIndex, bool bIsUsedForPilotSchool)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_USED_FOR_PILOT_SCHOOL - failed to get vehicle"))
	{
		veh->m_nVehicleFlags.bUsedForPilotSchool = bIsUsedForPilotSchool;
	}
}

void CommandSetVehicleActiveForPedNavigation(int VehicleIndex, bool bActive)
{
	CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(veh, "SET_VEHICLE_ACTIVE_FOR_PED_NAVIGATION - failed to get vehicle"))
	{
		veh->m_nVehicleFlags.bActiveForPedNavigation = bActive;
		if (veh->IsInPathServer())
		{
			CPathServerGta::UpdateDynamicObject(veh, true);
		}
	}
}

eVehicleClass CommandGetVehicleClass(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "GET_VEHICLE_CLASS - failed to find vehicle!"))
	{
		return pVehicle->GetVehicleModelInfo()->GetVehicleClass();
	}

	return VC_COMPACT;
}

eVehicleClass CommandGetVehicleClassFromName(int VehicleModelHashKey)
{
	fwModelId VehicleModelId;
	CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromHashKey((u32)VehicleModelHashKey, &VehicleModelId);
	if (VehicleModelId.IsValid() && vmi)
	{
		return vmi->GetVehicleClass();
	}

	return VC_COMPACT;
}

void CommandSetPlayersLastVehicle(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "SET_PLAYERS_LAST_VEHICLE - failed to find vehicle!"))
	{
#if __BANK
		scriptDebugf1("SET_PLAYERS_LAST_VEHICLE: %s [%p], index %i", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, VehicleIndex);
#endif
		CVehiclePopulation::RegisterInterestingVehicle(pVehicle);
	}
}

void CommandSetVehicleCanBeUsedByFleeingPeds(int VehicleIndex, bool bCanBeUsed)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_CAN_BE_USED_BY_FLEEING_PEDS - Vehicle has to be a mission vehicle"))
		{
			pVehicle->m_nVehicleFlags.bCanBeUsedByFleeingPeds = bCanBeUsed;
		}
	}
}

void CommandSetVehicleFrictionOverride(int VehicleIndex, float fFrictionOverride)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_fFrictionOverride = fFrictionOverride;
	}
}

void CommandSetAircraftPilotSkillNoiseScalar(int VehicleIndex, float in_Scalar )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		if ( SCRIPT_VERIFY(pVehicle, "SET_AIRCRAFT_PILOT_SKILL_NOISE_SCALAR called with invalid vehicle") )
		{
			if ( pVehicle->InheritsFromHeli() )
			{
				CHeli* pHeli = static_cast<CHeli*>(pVehicle);
				pHeli->SetPilotNoiseScalar(Clamp(in_Scalar, 0.0f, 1.0f));
			}
			else if ( pVehicle->InheritsFromPlane() )
			{
				CPlane *pPlane = static_cast<CPlane*>(pVehicle);
				pPlane->m_fScriptPilotSkillNoiseScalar = Clamp(in_Scalar, 0.0f, 1.0f);
			}
			else
			{
				SCRIPT_ASSERT(false, "SET_AIRCRAFT_PILOT_SKILL_NOISE_SCALAR called on a non aircraft");
			}
		}
	}
}

void CommandSetVehicleDropsMoneyWhenBlownUp(int VehicleIndex, bool bDropsMoney)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (pVehicle->InheritsFromAutomobile())
		{
			((CAutomobile*)pVehicle)->m_nAutomobileFlags.bDropsMoneyWhenBlownUp = bDropsMoney;
		}
		else
		{
			scriptAssertf(0, "%s:SET_VEHICLE_DROPS_MONEY_WHEN_BLOWN_UP - vehicle must be an automobile", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandSetVehicleKeepEngineOnWhenAbandoned(int VehicleIndex, bool bKeepEngineOn)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_KEEP_ENGINE_ON_WHEN_ABANDONED called with invalid vehicle"))
	{
		pVehicle->m_nVehicleFlags.bKeepEngineOnWhenAbandoned = bKeepEngineOn;
	}
}

void CommandSetVehicleHandlingOverride(int VehicleIndex, int HandlingOverrideHash)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_HANDLING_OVERRIDE called with invalid vehicle"))
	{
		pVehicle->GetIntelligence()->m_nHandlingOverrideHash = (u32)HandlingOverrideHash;
	}
}

void CommandSetVehicleImpatienceTimer(int VehicleIndex, int iImpatienceTimer)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_IMPATIENCE_TIMER called with invalid vehicle"))
	{
		pVehicle->GetIntelligence()->SetImpatienceTimerOverride(iImpatienceTimer);
	}
}

void CommandSetVehicleExtendedRemovalRange(int VehicleIndex, int removalRange)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_EXTENDED_REMOVAL_RANGE called with invalid vehicle"))
	{
		pVehicle->m_ExtendedRemovalRange = (s16)removalRange;
	}
}

void CommandSetVehicleSteeringBiasScalar(int VehicleIndex, float fScalar)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if(SCRIPT_VERIFY(pVehicle && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetIsAutomobile(), "SET_VEHICLE_STEERING_BIAS_SCALAR called on invalid vehicle"))
	{
		((CAutomobile *) pVehicle)->SetSteeringBiasScalar(fScalar);
	}
}

void CommandSetHeliControlLaggingRateScalar(int VehicleIndex, float in_Scalar )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "SET_HELI_CONTROL_LAGGING_RATE_SCALAR called with invalid vehicle"))
	{
		if ( SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_CONTROL_LAGGING_RATE_SCALAR called with invalid vehicle") )
		{
			CHeli* pHeli = static_cast<CHeli*>(pVehicle);
			pHeli->SetControlLaggingRateMulti(Clamp(in_Scalar, 0.0f, 1.0f));
		}
	}
}

void CommandSetVehicleWheelsCanBreakOffWhenBlowUp(int VehicleIndex, bool bCanWheelsBreakoff)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_WHEELS_CAN_BREAK_OFF_WHEN_BLOW_UP called with invalid vehicle"))
	{
		pVehicle->m_nVehicleFlags.bCanBreakOffWheelsWhenBlowUp = bCanWheelsBreakoff;
	}
}

void CommandSetVehicleCeilingHeight(int VehicleIndex, float fCeilingHeight)
{
	CVehicle::ms_fFlightCeilingScripted = Clamp(fCeilingHeight, 0.0f, CVehicle::FLIGHT_CEILING_PLANE);
	CVehicle *pVehicle;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_CEILING_HEIGHT called with invalid vehicle"))
	{
		pVehicle->m_nVehicleFlags.bUseScriptedCeilingHeight = true;
	}
}

void CommandSetVehicleNoExplosionDamageFromDriver(int VehicleIndex, bool bNoDamage)
{
	CVehicle *pVehicle;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_NO_EXPLOSION_DAMAGE_FROM_DRIVER called with invalid vehicle"))
	{
		pVehicle->SetNoDamageFromExplosionsOwnedByDriver(bNoDamage);
	}
}

void CommandClearVehicleRouteHistory(int VehicleIndex)
{
	CVehicle *pVehicle;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFY(pVehicle, "CLEAR_VEHICLE_ROUTE_HISTORY called with invalid vehicle"))
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();
	}
}

int CommandDoesVehicleExistWithDecorator(const char *label)
{
    CVehicle::Pool *pool = CVehicle::GetPool();

    if(pool)
    {
        atHashWithStringBank decKey(label);

        s32 i = (s32) pool->GetSize();

	    while(i--)
	    {
		    CVehicle *pVehicle = pool->GetSlot(i);

            if(pVehicle)
            {
                if(fwDecoratorExtension::ExistsOn(*pVehicle, decKey))
                {
                    return CTheScripts::GetGUIDFromEntity(const_cast<CVehicle&>(*pVehicle));
                }
            }
        }
    }

    return NULL_IN_SCRIPTING_LANGUAGE;
}

void CommandSetPlanePropellersBreakOffOnContact(int VehicleIndex, bool bBreakOff)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_PLANE_PROPELLERS_BREAK_OFF_ON_CONTACT - Vehicle has to be a mission vehicle"))
		{
			if (SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_PLANE_PROPELLERS_BREAK_OFF_ON_CONTACT - Vehicle has to be a plane"))
			{
				((CPlane *)pVehicle)->SetBreakOffPropellerOnContact(bBreakOff);
			}
		}
	}
}

void CommandSetBrokenPartsAffectHandling( int VehicleIndex, bool bIgnoreBrokenParts )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if( pVehicle )
	{
		if( SCRIPT_VERIFY( pVehicle->InheritsFromPlane(), "SET_VEHICLE_BROKEN_PARTS_DONT_AFFECT_AI_HANDLING - Vehicle has to be a plane" ) )
		{
			((CPlane *)pVehicle)->GetAircraftDamage().SetIgnoreBrokenOffPartsForAIHandling( bIgnoreBrokenParts );
		}
	}
}

void CommandDisableIndividualPlanePropeller(int VehicleIndex, int PropellerIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "DISABLE_INDIVIDUAL_PLANE_PROPELLER - Vehicle has to be a plane"))
		{
			((CPlane *)pVehicle)->EnableIndividualPropeller(PropellerIndex, false);
		}
	}
}

void CommandSetVehicleAIUseExclusiveSeat(int VehicleIndex, bool CanUseExclusiveSeats)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_AI_CAN_USE_EXCLUSIVE_SEATS called with invalid vehicle"))
	{
		pVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats = CanUseExclusiveSeats;
	}
}

void CommandSetVehicleExclusiveDriver(int VehicleIndex, int PedIndex, int ExclusiveDriverIndex)
{
	if (!scriptVerifyf(ExclusiveDriverIndex >= 0 && ExclusiveDriverIndex < CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS, "ExclusiveDriverIndex %i is not valid, max = %i", ExclusiveDriverIndex, CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS))
	{
		return;
	}

	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, 0);

	if(pVehicle)
	{
#if __BANK
		if (NetworkInterface::IsGameInProgress())
		{
			const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			const CNetObjVehicle* pNetObjVeh = static_cast<const CNetObjVehicle*>(pVehicle->GetNetworkObject());
			const bool bIsScriptVeh = (pNetObjVeh && pNetObjVeh->GetScriptObjInfo()) ? true : false;
			const char* szScript = bIsScriptVeh ? pNetObjVeh->GetScriptObjInfo()->GetScriptId().GetLogName() : "Code";
			CAILogManager::GetLog().Log("[ExclusiveDriver] - Machine with local player %s is calling SET_VEHICLE_EXCLUSIVE_DRIVER on vehicle %s (%p), exclusive driver %s, index %i, script %s\n", AILogging::GetDynamicEntityNameSafe(pLocalPlayer), AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle, AILogging::GetDynamicEntityNameSafe(pPed), ExclusiveDriverIndex, szScript);
		}
#endif // __BANK
        if(NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
        {
            ObjectId exclusiveDriverID = NETWORK_INVALID_OBJECT_ID;

            if(pPed && pPed->GetNetworkObject())
            {
                exclusiveDriverID = pPed->GetNetworkObject()->GetObjectID();
            }

            CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver parameters(exclusiveDriverID, (u32)ExclusiveDriverIndex);
            CScriptEntityStateChangeEvent::Trigger(pVehicle->GetNetworkObject(), parameters);
        }
        else
        {
		    pVehicle->SetExclusiveDriverPed(pPed, ExclusiveDriverIndex);
        }
	}
}

bool CommandIsPedExclusiveDriverOfVehicle(int VehicleIndex, int PedIndex, int& ExclusiveDriverIndex)
{
	ExclusiveDriverIndex = -1;

	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (!pVehicle)
		return false;

	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (!pPed)
		return false;

	for (s32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
	{
		if (pPed == pVehicle->GetExclusiveDriverPed(i))
		{
			ExclusiveDriverIndex = i;
			return true;
		}
	}
	return false;
}

void CommandSetVehicleForceAfterburner( int VehicleIndex, bool boAfterburnerOn )
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->PopTypeIsMission(), "SET_VEHICLE_FORCE_AFTERBURNER - Vehicle has to be a misison vehicle"))
		{
			pVehicle->m_nVehicleFlags.bForceAfterburnerVFX = boAfterburnerOn;
		}
	}
}

void CommandSetDontProcessVehicleGlass( int VehicleIndex, bool val )
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_bDontProcessVehicleGlass = val;
	}
}

void CommandSetDisableWantedConesResponse(int VehicleIndex, bool val)
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		Displayf("SET_DISABLE_WANTED_CONES_RESPONSE: DisableWantedConesResponse is being set from %d to %d\n", pVehicle->m_bDisableWantedConesResponse, val);
		pVehicle->m_bDisableWantedConesResponse = val;
	}
}

void CommandSetUseDesiredZCruiseSpeedForLanding( int VehicleIndex, bool val )
{
	CVehicle *pVehicle;

	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFYF(pVehicle,"SET_USE_DESIRED_Z_CRUISE_SPEED_FOR_LANDING - Could not find a vehicle with provided index %i",VehicleIndex))
	{
		if (SCRIPT_VERIFY(pVehicle->InheritsFromHeli(),"SET_USE_DESIRED_Z_CRUISE_SPEED_FOR_LANDING - target vehicle is not a helicopter! This native only works for helis."))
		{
			pVehicle->m_bUseDesiredZCruiseSpeedForLanding = val;
		}
	}
}

void CommandOverrideArriveDistanceForVehiclePersuitAttack( int VehicleIndex, float fDist )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (SCRIPT_VERIFYF(pVehicle,"SET_ARRIVE_DISTANCE_OVERRIDE_FOR_VEHICLE_PERSUIT_ATTACK - Could not find a vehicle with provided index %i",VehicleIndex))
	{
		pVehicle->SetOverrideArriveDistForVehPersuitAttack(fDist);
	}
}

void CommandSetVehicleReadyForCleanup(int VehicleIndex)
{
	CVehicle *pVehicle = NULL;
	pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bReadyForCleanup = true;
	}
}

void CommandSetDistantCarsEnabled(bool enabled)
{
#if ENABLE_DISTANT_CARS
    g_distantLights.SetDistantCarsEnabled(enabled);
#else
    (void)enabled;
#endif
}

float CommandGetFakeSuspensionLoweringAmount( int VehicleIndex )
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);
	if( pVehicle && pVehicle->InheritsFromAutomobile() )
	{
		CAutomobile* pAutomobile = static_cast< CAutomobile* >( pVehicle );

		return pAutomobile->GetFakeSuspensionLoweringAmount();
	}
	return -1.0f;
}

void CommandSetVehicleNeonColour(int VehicleIndex, int Red, int Green, int Blue)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		const Color32 col(Red,Green,Blue);
		pVehicle->SetNeonColour(col);
	}
}

void CommandSetVehicleNeonIndexColour(int VehicleIndex, int NColour)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		CVehicleModelInfoVarGlobal* pVehColours = CVehicleModelInfo::GetVehicleColours();
		Assert(pVehColours);

		const s32 MaxColours = pVehColours->GetColorCount();

		if(SCRIPT_VERIFY((NColour >= 0), "SET_VEHICLE_NEON_INDEX_COLOUR - Colour must be >= 0"))
		{
			if(SCRIPT_VERIFY(NColour < MaxColours, "SET_VEHICLE_NEON_INDEX_COLOUR - Colour is too large"))
			{
				Color32 col = pVehColours->GetVehicleColour(NColour);
				pVehicle->SetNeonColour(col);
			}
		}
	}
}

void CommandGetVehicleNeonColour(int VehicleIndex, int& Red, int& Green, int& Blue)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		Color32 col = pVehicle->GetNeonColour();
		Red = col.GetRed();
		Green = col.GetGreen();
		Blue = col.GetBlue();
	}
}

void CommandSetConsumePetrol(bool value)
{
	CVehicle::SetConsumePetrol(value);
}

bool CommandGetConsumePetrol()
{
	return CVehicle::GetConsumePetrol();
}

void CommandGetVehiclePetrolStats(s32 VehicleIndex, float& maxPetrol, float& currPetrol)
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);

	if (SCRIPT_VERIFYF(pVehicle, "GET_VEHICLE_PETROL_STATS - Vehicle is NULL VehicleIndex = %i", VehicleIndex))
	{
		maxPetrol = pVehicle->pHandling->m_fPetrolTankVolume;
		currPetrol = pVehicle->GetVehicleDamage()->GetPetrolTankLevel();
	}
}

void CommandSetPetrolConsumptionRate(s32 VehicleIndex, float newRate)
{
	s32 assertflags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, assertflags);

	if (SCRIPT_VERIFYF(pVehicle, "SET_PETROL_CONSUMPTION_RATE - Vehicle is NULL VehicleIndex = %i", VehicleIndex))
	{
		pVehicle->pHandling->m_fPetrolConsumptionRate = newRate;
	}
}

float CommandGetVehicleHealthPercentage(s32 VehicleIndex, float maxEngineHealth, float maxPetrolTankHealth, float maxHealth, float maxMainRotorHealth, float maxRearRotorHealth, float maxTailBoomHealth)
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if (SCRIPT_VERIFYF(pVehicle, "GET_VEHICLE_HEALTH_PERCENTAGE - Vehicle is NULL VehicleIndex = %i", VehicleIndex))
	{
		return pVehicle->GetVehicleDamage()->GetVehicleHealthPercentage( pVehicle, maxEngineHealth, maxPetrolTankHealth, maxHealth, maxMainRotorHealth, maxRearRotorHealth, maxTailBoomHealth );
	}

	return 0;
}

bool CommandIsMercVehicle(int iVehicleID)
{
	const CVehicle * pVehicle= CTheScripts::GetEntityToQueryFromGUID<CVehicle>(iVehicleID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	return pVehicle && pVehicle->m_nVehicleFlags.bMercVeh;
}

void CommandSetKERSAllowed( int iVehicleIndex, bool bKERSAllowed )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_Transmission.SetKERSAllowed( pVehicle, bKERSAllowed );
	}	
}

bool CommandGetVehicleHasKers( int iVehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if (pVehicle)
	{
		if(pVehicle->pHandling && pVehicle->pHandling->hFlags & HF_HAS_KERS)
			return true;
	}
	return false;
}

bool CommandIsVehicleAsleep( int iVehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( iVehicleIndex );
	if( SCRIPT_VERIFYF( pVehicle, "IS_VEHICLE_ASLEEP - Vehicle is NULL VehicleIndex = %i", iVehicleIndex ) )
	{
		return pVehicle->m_nVehicleFlags.bIsAsleep;
	}	
	return true;
}

void CommandSetPlaneResistToExplosion(int iVehicleIndex, bool bResistToExplosion)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "SET_PLANE_RESIST_TO_EXPLOSION - Vehicle has to be a plane"))
		{
			pVehicle->m_nVehicleFlags.bPlaneResistToExplosion = bResistToExplosion;
		}
	}
}

void CommandSetHeliResistToExplosion(int iVehicleIndex, bool bResistToExplosion)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if (pVehicle)
	{
		if (SCRIPT_VERIFY(pVehicle->InheritsFromHeli(), "SET_HELI_RESIST_TO_EXPLOSION - Vehicle has to be a plane"))
		{
			pVehicle->m_nVehicleFlags.bPlaneResistToExplosion = bResistToExplosion;
		}
	}
}

void SetDisableExtraTrickForces( bool bDisableExtraTrickForces )
{
	CBike::SetDisableExtraTrickForces( bDisableExtraTrickForces );
}

void CommandSetVehicleNeonEnabled(int VehicleIndex, int Index, bool Enabled)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		switch(Index)
		{
		case 0: pVehicle->SetNeonLOn(Enabled); break;
		case 1: pVehicle->SetNeonROn(Enabled); break;
		case 2: pVehicle->SetNeonFOn(Enabled); break;
		case 3: pVehicle->SetNeonBOn(Enabled); break;
		default: scriptErrorf("SET_VEHICLE_NEON_COLOUR - Invalid neon"); break;
		}
	}
}

bool CommandGetVehicleNeonEnabled(int VehicleIndex, int Index)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		switch(Index)
		{
		case 0: return pVehicle->IsNeonLOn();
		case 1: return pVehicle->IsNeonROn();
		case 2: return pVehicle->IsNeonFOn();
		case 3: return pVehicle->IsNeonBOn();
		default: scriptErrorf("SET_VEHICLE_NEON_COLOUR - Invalid neon"); break;
		}
	}
	return false;
}

void CommandSetAmbientVehicleNeonEnabled(bool Enabled)
{
	CVehiclePopulation::SetAmbientVehicleNeonsEnabled(Enabled);
}

bool CommandGetAmbientVehicleNeonEnabled()
{
	return CVehiclePopulation::GetAmbientVehicleNeonsEnabled();
}

void CommandSuppressNeonsOnVehicle(int VehicleIndex, bool bShouldBeSuppressed)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		pVehicle->SetNeonSuppressionState(bShouldBeSuppressed);
	}
}

bool CommandAreNeonsOnVehicleSuppressed(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return pVehicle->GetNeonSuppressionState();
	}
	return false;
}

void CommandSetDisableSuperDummy(int VehicleIndex, bool Disabled)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bDisableSuperDummy = Disabled;
	}
}

void CommandRequestDials(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if (pVehicle)
	{
		pVehicle->RequestDials(true);
	}

}

void CommandGetSize(int VehicleIndex, Vector3 &outSizeBoundsMin, Vector3 &outSizeBoundsMax)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

	if(pVehicle)
	{		
		pVehicle->GetApproximateSize(outSizeBoundsMin, outSizeBoundsMax);
	}
}

void CommandSetCarHighSpeedBumpSeverityMultiplier(float fMultiplier)
{
	CAutomobile::ms_fBumpSeverityMulti = fMultiplier;
}

int CommandGetNumberOfVehicleDoors(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if(pVehicle)
	{		
		return pVehicle->GetNumDoors();
	}
	return 0;
}

void CommandSetHydraulicsControl(int iVehicleIndex, bool bRaise)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if(pVehicle)
	{
		if(pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->HasHydraulicSuspension())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			for(int i = 0; i < pVehicle->GetNumWheels(); i++)
			{
				if(CWheel *pWheel = pVehicle->GetWheel(i))
				{
					if(bRaise)
					{
						pWheel->SetSuspensionRaiseAmount(pAutomobile->m_fHydraulicsUpperBound);
					}
					else
					{
						pWheel->SetSuspensionRaiseAmount(0.0f);
					}
					pWheel->GetConfigFlags().SetFlag(WCF_UPDATE_SUSPENSION);
				}

				pVehicle->ActivatePhysics();
				pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised = bRaise;
			}
		}
	}
}

void CommandSetHydraulicSuspensionRaiseFactor( int iVehicleIndex, int iWheelIndex, float fRaiseFactor )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);

			if( pAutomobile->HasHydraulicSuspension() )
			{
				eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)iWheelIndex );

				if( CWheel *pWheel = pVehicle->GetWheelFromId( wheelId ) )
				{
					float m_fHydraulicsT = ( pAutomobile->m_fHydraulicsUpperBound * fRaiseFactor );
					pWheel->SetSuspensionRaiseAmount( m_fHydraulicsT );
					pWheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );

					pVehicle->ActivatePhysics();
					pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised = false;

					if( fRaiseFactor != 0.0f )
					{
						pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics = true;
					}
					pAutomobile->SetTimeHyrdaulicsModified(fwTimer::GetTimeInMilliseconds());
				}
			}
		}
	}
}

float CommandGetHydraulicSuspensionRaiseFactor( int iVehicleIndex, int iWheelIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);

			if( pAutomobile->HasHydraulicSuspension() )
			{
				eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)iWheelIndex );
				CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

				if( pWheel &&
					pWheel->GetSuspensionLength() != 0.0f &&
					pAutomobile->m_fHydraulicsUpperBound != 0.0f )
				{
					float currentRaiseAmount = pWheel->GetSuspensionRaiseAmount();
					currentRaiseAmount /= pAutomobile->m_fHydraulicsUpperBound;
					return currentRaiseAmount;
				}
			}
		}
	}
	return 0.0f;
}

void CommandSetCanUseHydraulics( int iVehicleIndex, bool bCanUse )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);

			const u32 modelNameHash = pAutomobile->GetModelNameHash();

			if( bCanUse &&
				(modelNameHash!=MID_TORNADO6) && (modelNameHash!=MID_IMPALER2) && (modelNameHash!=MID_IMPALER4) && (modelNameHash!=MID_PEYOTE2))
			{
			    float hydraulicsModifier = (pVehicle->GetVariationInstance().GetHydraulicsModifier()/100.0f);

			    if(hydraulicsModifier > 0.0f)
  			    {
				    float upperBound = CVehicle::ms_fHydraulicVarianceUpperBoundMinModifier + ( ( CVehicle::ms_fHydraulicVarianceUpperBoundMaxModifier - CVehicle::ms_fHydraulicVarianceUpperBoundMinModifier ) * hydraulicsModifier );
				    float lowerBound = CVehicle::ms_fHydraulicVarianceLowerBoundMinModifier + ( ( CVehicle::ms_fHydraulicVarianceLowerBoundMaxModifier - CVehicle::ms_fHydraulicVarianceLowerBoundMinModifier ) * hydraulicsModifier );
				    float rate = CVehicle::ms_fHydraulicRateMin + ( ( CVehicle::ms_fHydraulicRateMax - CVehicle::ms_fHydraulicRateMin ) * hydraulicsModifier );

				    pAutomobile->SetHydraulicsBounds( upperBound, -lowerBound );
				    pAutomobile->SetHydraulicsRate( rate );
			    }
			}
			else
			{
				pAutomobile->SetHydraulicsBounds( 0.0f, 0.0f );
			}
		}
	}
}

enum WHEEL_HYDRAULIC_SCRIPT_STATE {
	HS_WHEEL_FREE = 0,
	HS_WHEEL_LOCKED,
	HS_WHEEL_BOUNCE,

	HS_ALL_FREE,
	HS_ALL_LOCK_UP,
	HS_ALL_LOCK_DOWN,
	HS_ALL_BOUNCE
};

void CommandSetHydraulicVehicleState( int iVehicleIndex, int state )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);

			if( pAutomobile->HasHydraulicSuspension() )
			{
				eWheelHydraulicState wheelState = WHS_IDLE;
				float raiseRate		= 1.0f;
				float raiseAmount	= 0.0f;

				for( int i = 0; i < pVehicle->GetNumWheels(); i++ )
				{
					if( CWheel *pWheel = pVehicle->GetWheel( i ) )
					{
						switch( (WHEEL_HYDRAULIC_SCRIPT_STATE)state )
						{
							case HS_ALL_FREE:
							{
								wheelState = WHS_FREE;
								break;
							}
							case HS_ALL_LOCK_UP:
							{
								wheelState	= WHS_LOCK_UP_ALL;
								raiseAmount = pAutomobile->m_fHydraulicsUpperBound;
								break;
							}
							case HS_ALL_LOCK_DOWN:
							{
								raiseAmount = pAutomobile->m_fHydraulicsLowerBound;
								wheelState	= WHS_LOCK_DOWN_ALL;
								break;
							}
							case HS_ALL_BOUNCE:
							{
								raiseAmount	= pAutomobile->m_fHydraulicsUpperBound;
								wheelState	= WHS_BOUNCE_ALL;
								break;
							}
							default:
								scriptAssertf( false, "SET_HYDRAULIC_VEHICLE_STATE - Setting an invalid vehicle state." );
						}

						pWheel->SetSuspensionHydraulicTargetState( wheelState );
						pWheel->SetSuspensionTargetRaiseAmount( raiseAmount, raiseRate );
					}
				}
				pVehicle->ActivatePhysics();
				pAutomobile->SetTimeHyrdaulicsModified( fwTimer::GetTimeInMilliseconds() );
			}
		}
	}
}

void CommandSetHydraulicWheelState( int iVehicleIndex, int iWheelIndex, int state, float raiseAmount, float raiseSpeed )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if( pVehicle )
	{
		if( pVehicle->InheritsFromAutomobile() )
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);

			if( pAutomobile->HasHydraulicSuspension() )
			{
				eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)iWheelIndex );

				if( CWheel *pWheel = pVehicle->GetWheelFromId( wheelId ) )
				{
					eWheelHydraulicState wheelState = WHS_IDLE;

					switch( (WHEEL_HYDRAULIC_SCRIPT_STATE)state )
					{
						case HS_WHEEL_FREE:
						{
							wheelState = WHS_FREE;
							break;
						}
						case HS_WHEEL_LOCKED:
						{
							wheelState = WHS_LOCK;
							break;
						}
						case HS_WHEEL_BOUNCE:
						{
							wheelState = WHS_BOUNCE;
							break;
						}
						default:
							scriptAssertf( false, "SET_HYDRAULIC_WHEEL_STATE - Setting an invalid wheel state." );
					}

					float hydraulicsT = raiseAmount;
					
					if( hydraulicsT > 0.0f )
					{
						hydraulicsT *= pAutomobile->m_fHydraulicsUpperBound;
					}
					else
					{
						hydraulicsT *= -pAutomobile->m_fHydraulicsLowerBound;
					}

					pWheel->SetSuspensionHydraulicTargetState( wheelState );
					pWheel->SetSuspensionTargetRaiseAmount( hydraulicsT, raiseSpeed );
					
					pVehicle->ActivatePhysics();
					pAutomobile->SetTimeHyrdaulicsModified( fwTimer::GetTimeInMilliseconds() );
					pVehicle->GetVehicleAudioEntity()->OnScriptSetHydraulicWheelState();
				}
			}
		}
	}
}


void CommandSetCanAdjustGroundClearance(int iVehicleIndex, bool bCanAdjust)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bAllowBoundsToBeRaised = bCanAdjust;
	}	
}

bool CommandHasVehiclePetrolTankBeenSetOnFireByEntity(int vehicleIndex, int damagerIndex)
{
	bool result = false;

	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	const CPhysical* pDamager = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(damagerIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	if (pVehicle && pVehicle->GetVehicleDamage() && pDamager)
	{
		const CEntity* pEntity = pVehicle->GetVehicleDamage()->GetEntityThatSetUsOnFire();
		if (pEntity && pEntity == pDamager)
		{
			scriptDebugf1("%s: HAS_VEHICLE_PETROLTANK_SET_ON_FIRE_BY_ENTITY - Vehicle '%s' Petrol Tank has been set on fire by entity '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetLogName(), pDamager->GetLogName());
			result = true;
		}		
	}

	return result;
}

void CommandClearVehiclePetrolTankFireCulprit(int vehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	if (pVehicle && pVehicle->GetVehicleDamage())
	{
		const CEntity* pEntity = pVehicle->GetVehicleDamage()->GetEntityThatSetUsOnFire();
		if (pEntity)
		{
			scriptDebugf1("%s: CLEAR_VEHICLE_PETROLTANK_FIRE_CULPRIT - Clear Vehicle '%s' Petrol Tank FIRE CULPRIT '%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pVehicle->GetLogName(), pEntity->GetLogName());
			pVehicle->GetVehicleDamage()->ClearPetrolTankFireCulprit();
		}
	}
}

void CommandVehicleSetBobbleHeadVelocity( const scrVector& scrVelocity )
{
	Vec3V vVelocity( scrVelocity );
	CVehicle::SetBobbleHeadVelocity( vVelocity );
}

bool CommandVehicleGetIsDummy( int vehicleIndex )
{
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>( vehicleIndex );
	return ( pVehicle && pVehicle->IsDummy() );
}


bool CommandTestVehicleCoords( int vehicleIndex, const scrVector& scrVecNewCoors, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies, bool bUseBoundingBox )
{
	Vector3 vNewCoors( scrVecNewCoors );

	if( SCRIPT_VERIFY( ABS( vNewCoors.x ) < WORLDLIMITS_XMAX && ABS( vNewCoors.y ) < WORLDLIMITS_YMAX, "SET_VEHICLE_COORDS Position out of range!" ) )
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( vehicleIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS );
		if( pVehicle )
		{
			float Range = pVehicle->GetBoundRadius();
			s32 NumberOfCollisions = 0;

		//	Check for collisions with vehicles, peds, objects and dummies
			CEntity *pCollidingEnts[16];
			CGameWorld::FindObjectsKindaColliding( vNewCoors, Range, bUseBoundingBox, FALSE, &NumberOfCollisions, 16, pCollidingEnts, bCheckBuildings, bCheckVehicles, bCheckPeds, bCheckObjects, bCheckDummies);

			if( NumberOfCollisions == 0 ||
				( NumberOfCollisions == 1 &&
				  pCollidingEnts[ 0 ] == pVehicle ) )
			{
				return true;
			}
		}
	}
	return false;
}

void CommandSetVehicleDamageScale(int VehicleIndex, float damageScale)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->GetVehicleDamage()->SetScriptDamageScale( damageScale );
	}
}

void CommandSetVehicleWeaponDamageScale(int VehicleIndex, float damageScale)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->GetVehicleDamage()->SetScriptWeaponDamageScale( damageScale );
	}
}

void CommandSetDisableDamageWithPickedUpEntity( int VehicleIndex, bool disableDamage )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );

	if (pVehicle)
	{
		pVehicle->GetVehicleDamage()->SetDisableDamageWithPickedUpEntity( disableDamage );
	}
}

void CommandSetVehicleUsesMPPlayerDamageMultiplier(int VehicleIndex, bool bUseMultiplier)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		pVehicle->SetUseMPDamageMultiplierForPlayerVehicle(bUseMultiplier);
	}
}

void CommandSetBikeEasyToLand( int VehicleIndex, bool easyToLand )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle )
	{
		pVehicle->SetEasyToLand( easyToLand );
	}
}

void CommandSetBeastVehicle( int VehicleIndex, bool beastVehicle )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle )
	{
		pVehicle->SetBeastVehicle( beastVehicle );
	}
}

void CommandSetInvertVehicleControls( int VehicleIndex, bool invertControls )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle )
	{
		pVehicle->m_bInvertControls = invertControls;
	}
}

void CommandSetSpeedBoostEffectDisabled( bool disableEffect )
{
	CVehicle::ms_DisableSpeedBoostEffect = disableEffect;
}

void CommandSetSlowDownEffectDisabled( bool disableEffect )
{
	CVehicle::ms_DisableSlowDownEffect = disableEffect;
}

void CommandSetFormationLeader( int VehicleIndex, const scrVector& scrVecOffset, float radius )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		Vector3 offset = Vector3( scrVecOffset );
		CVehicle::SetFormationLeader( pVehicle, offset, radius );
	}
	else
	{
		CVehicle::ResetFormationLeader();
	}
}

void CommandResetFormationLeader()
{
	CVehicle::ResetFormationLeader();
}


bool CommandGetIsBoatCapsized( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle->InheritsFromBoat() )
	{
		CBoat* pBoat = static_cast< CBoat* >( pVehicle );
		if( pBoat->m_BoatHandling.GetCapsizedTimer() > 0.0f)
		{
			return true;
		}
	}
	return false;
}

void CommandSetRammingScoopOrRamp( int VehicleIndex, bool allowScoopOrRamp )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		pVehicle->SetRampOrRammingAllowed( allowScoopOrRamp );
	}
}


void CommandSetScriptRampImpulseScale( int VehicleIndex, float impulseScale )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle )
	{
		pVehicle->SetScriptRammingImpulseScale( impulseScale );
	}
}

bool CommandGetIsDoorValid( int VehicleIndex, int DoorNumber )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle)
	{
		eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));
		CCarDoor* pDoor = pVehicle->GetDoorFromId(nDoorId);

		return pDoor != NULL;
	}

	return false;
}

void CommandSetScriptRocketBoostRechargeTime( int VehicleIndex, float rechargeTime)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		Assertf(pVehicle->HasRocketBoost(), "Can not change rocket boost recharge time on vehicle with no rocket boost!");
		Assertf(rechargeTime > 0.0f, "Can not set rocket boost recharge time to a non-positive number!");

		float fNewRechargeRate = pVehicle->GetRocketBoostCapacity() / rechargeTime;

		pVehicle->SetRocketBoostRechargeRate(fNewRechargeRate);
	}
}

bool CommandGetHasRocketBoost( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		return pVehicle->HasRocketBoost();
	}

	return false;
}

bool CommandIsRocketBoostActive( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && SCRIPT_VERIFY(pVehicle->HasRocketBoost(), "IS_ROCKET_BOOST_ACTIVE - called on a vehicle that does not have a rocket boost!") )
	{
		return pVehicle->IsRocketBoosting();
	}

	return false;
}

void CommandSetRocketBoostActive( int VehicleIndex, bool ShouldBeActive )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && SCRIPT_VERIFY(pVehicle->HasRocketBoost(), "IS_ROCKET_BOOST_ACTIVE - called on a vehicle that does not have a rocket boost!") )
	{
		if(ShouldBeActive && pVehicle->GetRocketBoostRemaining() == pVehicle->GetRocketBoostCapacity())
		{
			pVehicle->SetRocketBoosting(true);
		}
		else if(!ShouldBeActive)
		{
			pVehicle->SetRocketBoosting(false);
		}
	}
}

void CommandSetRocketBoostFill(int VehicleIndex, float amountToFill)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle && SCRIPT_VERIFY(pVehicle->HasRocketBoost(), "SET_ROCKET_BOOST_FILL - called on a vehicle that does not have a rocket boost!") )
	{
		float amountToFillClamped = rage::Clamp(amountToFill, 0.0f, 1.0f);
		pVehicle->SetRocketBoostRemaining(amountToFillClamped * pVehicle->GetRocketBoostCapacity());
	}
}

void CommandSetGliderActive(int VehicleIndex, bool bActive)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if( pVehicle && SCRIPT_VERIFY(pVehicle->HasGlider(), "SET_GLIDER_ACTIVE - called on a vehicle that does not have a glider!") )
	{
		if(bActive)
		{
			pVehicle->StartGliding();
		}
		else
		{
			pVehicle->FinishGliding();
		}
	}
}

bool CommandGetIsWheelsRetracted( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		if( SCRIPT_VERIFY( pVehicle->InheritsFromAmphibiousQuadBike(), "GET_IS_WHEELS_RETRACTED - called on a vehicle that does not have retractable wheels!" ) )
		{
			// We say that wheels are retracted if they are not fully out ( so we count wheels in transition as being retracted )
			return !( static_cast<CAmphibiousQuadBike*>( pVehicle )->IsWheelsFullyOut() );
		}
	}

	return false;
}

bool CommandGetHasRetractableWheels( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		return pVehicle->InheritsFromAmphibiousQuadBike();
	}

	return false;
}

void CommandsSetWheelsExtendedInstantly( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		if( SCRIPT_VERIFY( pVehicle->InheritsFromAmphibiousQuadBike(), "SET_WHEELS_EXTENDED_INSTANTLY - called on a vehicle that does not have retractable wheels!" ) )
		{
			static_cast<CAmphibiousQuadBike*>(pVehicle)->ForceWheelsOut();
		}
	}
}

void CommandsSetWheelsRetractedInstantly( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		if( SCRIPT_VERIFY( pVehicle->InheritsFromAmphibiousQuadBike(), "SET_WHEELS_RETRACTED_INSTANTLY - called on a vehicle that does not have retractable wheels!" ) )
		{
			static_cast<CAmphibiousQuadBike*>(pVehicle)->ForceWheelsIn();
		}
	}
}

bool CommandGetCarHasJump( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		return pVehicle->HasJump();
	}

	return false;
}

void CommandUseHigherCarJump( int VehicleIndex, bool useHigherJump )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		if( SCRIPT_VERIFY( pVehicle->HasJump(), "USE_HIGHER_CAR_JUMP - called on a vehicle that does not have a jump!" ) )
		{
			pVehicle->SetDoHigherJump(useHigherJump);
		}
	}
}

void CommandSetClearWaitingOnCollisionOnPlayerEnter( int VehicleIndex, bool ClearWatingOnCollision )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle )
	{
		pVehicle->SetClearWaitingOnCollisionOnPlayerEnter( ClearWatingOnCollision );
	}
}

void CommandSetVehicleWeaponRestrictedAmmo( int VehicleIndex, int VehicleWeaponIndex, int AmmoCount )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && SCRIPT_VERIFYF(VehicleWeaponIndex >= -1 && VehicleWeaponIndex < MAX_NUM_VEHICLE_WEAPONS, "SET_VEHICLE_WEAPON_RESTRICTED_AMMO - Vehicle weapon index %i invalid for vehicle %s", VehicleWeaponIndex, pVehicle->GetDebugName()) )
	{
		if (SCRIPT_VERIFYF(AmmoCount >= -1, "SET_VEHICLE_WEAPON_RESTRICTED_AMMO - Ammo value %i invalid for vehicle %s", AmmoCount, pVehicle->GetDebugName()))
		{
			pVehicle->SetRestrictedAmmoCount(VehicleWeaponIndex, AmmoCount);
		}
	}
}

int CommandGetVehicleWeaponRestrictedAmmo( int VehicleIndex, int VehicleWeaponIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && SCRIPT_VERIFYF(VehicleWeaponIndex >= -1 && VehicleWeaponIndex < MAX_NUM_VEHICLE_WEAPONS, "GET_VEHICLE_WEAPON_RESTRICTED_AMMO - Vehicle weapon index %i invalid for vehicle %s", VehicleWeaponIndex, pVehicle->GetDebugName()) )
	{
		return pVehicle->GetRestrictedAmmoCount(VehicleWeaponIndex);
	}
	return -1;
}

bool CommandGetVehicleHasParachute( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && pVehicle->InheritsFromAutomobile() )
	{
		return pVehicle->HasParachute();
	}

	return false;
}

bool CommandGetVehicleCanDeployParachute( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && pVehicle->HasParachute(), "GET_VEHICLE_CAN_DEPLOY_PARACHUTE - Vehicle does not have a parachute!") )
	{
		return static_cast<CAutomobile*>(pVehicle)->CanDeployParachute();
	}

	return false;
}

void CommandVehicleStartParachuting( int VehicleIndex, bool AllowPlayerCancelParachuting )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && 
		SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && 
		pVehicle->HasParachute() && 
		static_cast<CAutomobile*>(pVehicle)->CanDeployParachute(), "VEHICLE_START_PARACHUTING - Vehicle does not have a parachute or is not in a valid state to deploy the parachute!") )
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);

		pCar->StartParachuting();

		pCar->SetCanPlayerDetachParachute(AllowPlayerCancelParachuting);
	}
}

bool CommandIsVehicleParachuteDeployed( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pVehicle && SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && pVehicle->HasParachute(), "IS_VEHICLE_PARACHUTE_DEPLOYED - Vehicle does not have a parachute, please check GET_VEHICLE_HAS_PARACHUTE first"))
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);
		return pCar->IsParachuteDeployed();
	}

	return false;
}

void CommandVehicleSetRampAndRammingCarsTakeDamage( int VehicleIndex, bool TakeDamage )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && 
		SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && 
		( pVehicle->HasRamp() ||
		  pVehicle->HasRammingScoop() ), "VEHICLE_SET_RAMP_AND_RAMMING_CARS_TAKE_DAMAGE - vehicle either doesn't have a ramp or it doesn't have a ramming scoop ") )
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);
		pCar->m_disableRampCarImpactDamage = !TakeDamage;
	}
}


void CommandVehicleSetEnableRampCarSideImpulse( int VehicleIndex, bool ApplySideImpulses )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && 
		SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && 
		pVehicle->HasRamp(), "VEHICLE_SET_ENABLE_RAMP_CAR_SIDE_IMPULSE - vehicle doesn't have a ramp") )
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);
		pCar->m_applyRampCarSideImpulse = ApplySideImpulses;
	}
}

void CommandVehicleSetNormaliseRampCarVerticalVelocity( int VehicleIndex, bool Normalise )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if( pVehicle && 
		SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() && 
		pVehicle->HasRamp(), "VEHICLE_SET_ENABLE_NORMALISE_RAMP_CAR_VERTICAL_VELOCTIY - vehicle doesn't have a ramp") )
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(pVehicle);
		pCar->m_normaliseRampHitVelocity = Normalise;
	}
}

void CommandVehicleSetJetWashForceEnabled( bool enable )
{
	CVehicle::sm_bJetWashEnabled = enable;
}

void CommandSetVehicleWeaponCanTargetObjects(int VehicleIndex, bool CanTargetObjects)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(SCRIPT_VERIFY(pVehicle, "SET_VEHICLE_WEAPON_CAN_TARGET_OBJECTS - Failed to find vehicle!"))
	{
		pVehicle->SetHomingCanLockOnToObjects(CanTargetObjects);
	}
}

void CommandSetUseBoostButtonForWheelRetract( bool UseBoostbutton )
{
	CVehicle::SetUseBoostButtonForWheelRetract( UseBoostbutton );
}


void CommandUpdateSlipperyBoxLimits( int VehicleIndex, float limitModifier )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if( SCRIPT_VERIFY( pVehicle && pVehicle->InheritsFromAutomobile(), "UPDATE_SLIPPERY_BOX_LIMITS - Failed to find automobile!" ) )
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		limitModifier = Clamp( limitModifier, 0.01f, 1.0f );
		pAutomobile->SlipperyBoxUpdateLimits( limitModifier );
	}
}
	 
void CommandBreakOffSlipperyBox( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if( SCRIPT_VERIFY( pVehicle && pVehicle->InheritsFromAutomobile(), "BREAK_OFF_SLIPPERY_BOX - Failed to find automobile!" ) )
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		pAutomobile->SlipperyBoxBreakOff();
	}
}

void CommandSetVehicleParachuteModelOverride( int VehicleIndex, int ModelNameHash)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if( pVehicle && pVehicle->InheritsFromAutomobile()) 
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		if(pAutomobile->HasParachute() )
		{
			pAutomobile->SetParachuteModelOverride( (u32)ModelNameHash );
		}
	}
}

//
//
//
//
static CCustomShaderEffectProp* GetCsePropFromObject(CObject *pObj)
{
	fwCustomShaderEffect *fwCSE = pObj->GetDrawHandler().GetShaderEffect();
	if(fwCSE && fwCSE->GetType()==CSE_PROP)
	{
		return static_cast<CCustomShaderEffectProp*>(fwCSE);
	}

	return NULL;
}

void CommandSetVehicleParachuteModelTintIndex(int VehicleIndex, int TintIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle && pVehicle->InheritsFromAutomobile())
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		pAutomobile->SetParachuteTintIndex((u8)TintIndex);

		// still try to dynamically set tint index (if parachute object already created):
		CObject *pParachuteObj = (CObject*)pAutomobile->GetParachuteObject();
		if(pParachuteObj)
		{
			CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(pParachuteObj);
			if(pPropCSE)
			{
				pPropCSE->SelectTintPalette((u8)TintIndex, pParachuteObj);
			}
		}
	}
}

int CommandGetVehicleParachuteModelTintIndex(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if(pVehicle && pVehicle->InheritsFromAutomobile())
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		CObject *pParachuteObj = (CObject*)pAutomobile->GetParachuteObject();
		if(pParachuteObj)
		{
			CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(pParachuteObj);
			if(pPropCSE)
			{
				return pPropCSE->GetMaxTintPalette();
			}
		}
	}

	return 0;
}

int CommandSetOverrideExtendableSideRatio( int UNUSED_PARAM( VehicleIndex ), bool UNUSED_PARAM( OverrideExtending ) )
{
	//CTrailer *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CTrailer >( VehicleIndex );

	//if( pVehicle )
	//{
	//	pVehicle->SetOverrideExendableRatio( OverrideExtending );
	//}

	return 0;
}

int CommandSetExtendableSideTargetRatio( int UNUSED_PARAM( VehicleIndex ), float UNUSED_PARAM( TargetRatio ) )
{
	//CTrailer *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CTrailer >( VehicleIndex );

	//if( pVehicle )
	//{
	//	pVehicle->SetExendableTargetRatio( TargetRatio );
	//}

	return 0;
}

int CommandSetExtendableSideRatio( int UNUSED_PARAM( VehicleIndex ), float UNUSED_PARAM( TargetRatio ) )
{
	//CTrailer *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CTrailer >( VehicleIndex );

	//if( pVehicle )
	//{
	//	pVehicle->SetExendableRatio( TargetRatio );
	//}

	return 0;
}

int CommandGetAllVehicles(scrArrayBaseAddr& vehicleArray)
{
	int count = 0;
	{
		scrValue* pAddress = scrArray::GetElements<scrValue>(vehicleArray);
		int size = scrArray::GetCount(vehicleArray);

		CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
		int numVehiclesInPool = (int) pVehiclePool->GetSize();

		// search for the next dummy vehicle
		for (int i=0; i<numVehiclesInPool; i++)
		{
			CVehicle* pVehicle = pVehiclePool->GetSlot(i);
			if (pVehicle != NULL && size > 0)
			{
				// some vehicles are in a funny state that can cause errors later - ignore these (url:bugstar:3635323)
				const CScriptEntityExtension* pExtension = pVehicle->GetExtension<CScriptEntityExtension>();
				if (!((pVehicle->PopTypeGet() == POPTYPE_MISSION) && (pExtension == NULL)))
				{
					pAddress->Int = fwScriptGuid::CreateGuid(*pVehicle);
					pAddress++;
					count++;
					size--;
				}
			}
		}
	}
	return count;
}

void CommandSetCargobobExtraPickupRangeAllowance( int iVehicleIndex, float extraAllowance )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if( pVehicle && pVehicle->InheritsFromHeli() && ( static_cast<CHeli*>( pVehicle )->GetIsCargobob() ) )
	{
		//loop through gadgets until we found the pick up rope
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			if( pVehicleGadget && ( pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET ) )
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast< CVehicleGadgetPickUpRope* >( pVehicleGadget );
				pPickUpRope->SetExtraPickupBoundAllowance( extraAllowance );
			}
		}
	}
}

void CommandSetOverrideVehicleDoorTorque( int VehicleIndex, int doorId, bool overrideTorque )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );
	if( pVehicle )
	{
		eHierarchyId nDoorId = CCarDoor::GetScriptDoorId( eScriptDoorList( doorId ) );

		CCarDoor* pDoor = pVehicle->GetDoorFromId( nDoorId );
		if(SCRIPT_VERIFY((pDoor), "SET_OVERRIDE_VEHICLE_DOOR_TORQUE - Door not found"))
		{
			pDoor->OverrideDoorTorque( overrideTorque );
		}
	}
}

void CommandSetWheelieEnabled( int VehicleIndex, bool isEnabled )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );
	if( pVehicle )
	{
		pVehicle->SetWheeliesEnabled(isEnabled);
	}
}

void CommandSetDisableHeliExplodeFromBodyDamage( int VehicleIndex, bool disableExplode )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );
	if( pVehicle &&
		pVehicle->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast< CHeli* >( pVehicle );
		pHeli->SetDisableExplodeFromBodyDamage( disableExplode );
	}
	if( pVehicle &&
		pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast< CPlane* >( pVehicle );
		pPlane->SetDisableExplodeFromBodyDamage( disableExplode );
	}
}

void CommandSetDisableExplodeFromBodyDamageOnCollision( int VehicleIndex, bool disableExplode )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );
	if( pVehicle &&
		pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast< CPlane* >( pVehicle );
		pPlane->SetDisableExplodeFromBodyDamageOnCollision( disableExplode );
	}
}

void CommandSetTrailerCanAttachToLocalPlayer( int VehicleIndex, bool canAttach )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromTrailer() )
	{
		CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
		pTrailer->SetLocalPlayerCanAttach( canAttach );
	}
}

void CommandSetShouldResetTurreatInScriptedCameras( int VehicleIndex, bool shouldReset )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromTrailer() )
	{
		CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
		pTrailer->m_bDontResetTurretHeadingInScriptedCamera = !shouldReset;
	}
}

void CommandSetGroundEffectReducesDrag( bool reduceDrag )
{
	CPlane::ms_ReduceDragWithGroundEffect = reduceDrag;
}

void CommandSetDisableMapCollision( int VehicleIndex )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		if(const phInst* pInst = pVehicle->GetCurrentPhysicsInst())
		{
			if(pInst->IsInLevel())
			{
				u32 includeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags(pInst->GetLevelIndex());
				includeFlags = includeFlags & ~ArchetypeFlags::GTA_MAP_TYPE_VEHICLE;

				CPhysics::GetLevel()->SetInstanceIncludeFlags( pInst->GetLevelIndex(), includeFlags );
			}
		}
	}
}

void CommandSetDisablePedStandOnTop( int VehicleIndex, bool DisableStandOn )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		pVehicle->m_bDisablePlayerCanStandOnTop = DisableStandOn;
	}
}

void SetVehicleDamageScales( int VehicleIndex, float bodyScale, float petrolTankScale, float engineScale, float collisionDamageWithMap )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		pVehicle->GetVehicleDamage()->SetDamageScales( bodyScale, petrolTankScale, engineScale );
		pVehicle->GetVehicleDamage()->SetCollisionWithMapDamageScale( collisionDamageWithMap );
	}
}

void SetHeliDamageScales( int VehicleIndex, float mainRotorScale, float rearRotorScale, float tailBoomScale )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast<CHeli*>(pVehicle);
		pHeli->GetVehicleDamage()->SetHeliDamageScales( mainRotorScale, rearRotorScale, tailBoomScale );
	}
}

void SetPlaneSectionDamageScale( int VehicleIndex, int section, float damageScale )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetAircraftDamage().SetSectionDamageScale( section, damageScale );
	}
}

void SetPlaneLandingGearSectionDamageScale( int VehicleIndex, int section, float damageScale )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetLandingGearDamage().SetSectionDamageScale( section, damageScale );
	}
}

void SetPlaneSetControllingSectionShouldBreakOffWhenHitByExplosion( int VehicleIndex, bool shouldBreakOff )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromPlane() )
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetAircraftDamage().SetControlSectionsBreakOffFromExplosions( shouldBreakOff );
	}
}


void SetHeliCanPickUpObjectsThatHavePickupDisabled( int VehicleIndex, bool canPickUp )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle && pVehicle->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast<CHeli*>(pVehicle);
		pHeli->SetCanPickupEntitiesThatHavePickupDisabled( canPickUp );
	}
}

void CommandSetVehicleBombAmmo( int VehicleIndex, int AmmoCount )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pVehicle)
	{
		if (SCRIPT_VERIFYF(AmmoCount >= 0 && AmmoCount <= 1000, "SET_VEHICLE_BOMB_AMMO - Ammo value %i invalid for vehicle %s (min 0, max 1000)", AmmoCount, pVehicle->GetDebugName()))
		{
			pVehicle->SetBombAmmoCount(AmmoCount);
		}
	}
}

int CommandGetVehicleBombAmmo( int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pVehicle)
	{
		return pVehicle->GetBombAmmoCount();
	}
	return -1;
}

void CommandSetVehicleCountermeasureAmmo( int VehicleIndex, int AmmoCount )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pVehicle)
	{
		if (SCRIPT_VERIFYF(AmmoCount >= 0 && AmmoCount <= 1000, "SET_VEHICLE_COUNTERMEASURE_AMMO - Ammo value %i invalid for vehicle %s (min 0, max 1000)", AmmoCount, pVehicle->GetDebugName()))
		{
			pVehicle->SetCountermeasureAmmoCount(AmmoCount);
		}
	}
}

int CommandGetVehicleCountermeasureAmmo( int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pVehicle)
	{
		return pVehicle->GetCountermeasureAmmoCount();
	}
	return -1;
}

void CommandSetHeliCombatOffset(int VehicleIndex, const scrVector & scrVecOffset)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if (pVehicle)
	{
		if (pVehicle->InheritsFromHeli())
		{
			CHeli* pHeli = static_cast<CHeli*>(pVehicle);
			pHeli->GetHeliIntelligence()->bHasCombatOffset = true;
			pHeli->GetHeliIntelligence()->vCombatOffset = Vector3(scrVecOffset);
		}
	}
}

bool Command_GetCanVehicleBePlacedHere( int VehicleIndex, const scrVector& VehiclePos, const scrVector& VehicleRotation, int RotOrder, s32 LOSFlags )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if( pVehicle )
	{
		Vector3 vecRotation( VehicleRotation );

		Matrix34 trans;
		if( vecRotation.IsNonZero() )
		{
			CScriptEulers::MatrixFromEulers( trans, DtoR * vecRotation, static_cast<EulerAngleOrder>( RotOrder ) );
		}
		else
		{
			trans.Identity();
		}

		// Translate the box to the desired world position.
		trans.d = Vector3( VehiclePos );

		const int iBoneIndexChassisLowLod = pVehicle->GetBoneIndex(VEH_CHASSIS_LOWLOD);
		if( iBoneIndexChassisLowLod != -1 )
		{
			WorldProbe::CShapeTestBoundDesc boundTestDesc;

			int iComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndexChassisLowLod);
			if(iComponent > -1)
			{
				const phBoundComposite* pBoundCompOrig = static_cast<const phBoundComposite*>( pVehicle->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds() );
				boundTestDesc.SetBound( pBoundCompOrig->GetBound( iComponent ) );

				boundTestDesc.SetTransform( &trans );
				boundTestDesc.SetExcludeEntity( pVehicle );
				boundTestDesc.SetIncludeFlags( shapetest_commands::GetPhysicsFlags( LOSFlags ) );
				boundTestDesc.SetTypeFlags( ArchetypeFlags::GTA_VEHICLE_TYPE );
				
				return !WorldProbe::GetShapeTestManager()->SubmitTest( boundTestDesc );
			}
		}
		else
		{
			// if we don't have a low lod chassis just create a bounding box
			Vector3 vBoundBoxMax = pVehicle->GetBoundingBoxMax();
			Vector3 vBoundBoxMin = pVehicle->GetBoundingBoxMin();

			phBoundBox* pBoxBound = rage_new phBoundBox( vBoundBoxMax - vBoundBoxMin );
			
			// Average the bounding box extents to find the centre of the bound.
			Vector3 vBoundCentre = vBoundBoxMax + vBoundBoxMin;
			vBoundCentre.Scale( 0.5f );
			vBoundCentre += trans.d;
			trans.d = vBoundCentre;

			WorldProbe::CShapeTestBoundDesc boxDesc;
			boxDesc.SetBound( pBoxBound );
			boxDesc.SetTransform( &trans );
			boxDesc.SetIncludeFlags( shapetest_commands::GetPhysicsFlags( LOSFlags ) );
			boxDesc.SetTypeFlags( ArchetypeFlags::GTA_SCRIPT_TEST );
			boxDesc.SetExcludeEntity( pVehicle );
			boxDesc.SetContext( WorldProbe::EScript );

			return !WorldProbe::GetShapeTestManager()->SubmitTest( boxDesc );
		}
	}

	return false;
}

void ComandSetDisableAutomaticCrashTask(int vehicleIndex, bool bDisable)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_DISABLE_AUTOMATIC_CRASH_TASK - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->GetIsHeli() || pVehicle->InheritsFromPlane(),"SET_DISABLE_AUTOMATIC_CRASH_TASK - vehicle %s [%p] is not a plane or heli. This native works only for those types!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
			if (pVehicle->GetIsHeli())
			{
				((CHeli*)pVehicle)->SetDisableAutomaticCrashTask(bDisable);
			}
			else if (pVehicle->InheritsFromPlane())
			{
				((CPlane*)pVehicle)->SetDisableAutomaticCrashTask(bDisable);
			}
		}
	}
}



void ComandSetSpecialFlightModeRatio(int vehicleIndex, float ratio )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_SPECIAL_FLIGHT_MODE_RATIO - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->pHandling->GetSpecialFlightHandlingData(),"SET_SPECIAL_FLIGHT_MODE_RATIO - vehicle %s [%p] does not have a special flight mode!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
			pVehicle->SetSpecialFlightModeRatio( ratio );
		}
	}
}

void CommandSetSpecialFlightModeTargetRatio(int vehicleIndex, float ratio )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->pHandling->GetSpecialFlightHandlingData(),"SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO - vehicle %s [%p] does not have a special flight mode!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
#if !__FINAL
			if(NetworkInterface::IsGameInProgress())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO", "%s", pVehicle->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Ratio", "%.2f", ratio);
			}
#endif
			pVehicle->SetSpecialFlightModeTargetRatio( ratio );
		}
	}
}

void CommandSetSpecialFlightModeAllowed(int vehicleIndex, bool allowSpecialFlight )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_SPECIAL_FLIGHT_MODE_ALLOWED - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->pHandling->GetSpecialFlightHandlingData(),"SET_SPECIAL_FLIGHT_MODE_ALLOWED - vehicle %s [%p] does not have a special flight mode!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
#if !__FINAL
			if(NetworkInterface::IsGameInProgress())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SET_SPECIAL_FLIGHT_MODE_ALLOWED", "%s", pVehicle->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Allow", allowSpecialFlight ? "TRUE" : "FALSE");
			}
#endif
			pVehicle->SetSpecialFlightModeAllowed( allowSpecialFlight );
		}
	}
}

void CommandSetDisableHoverModeFlight(int vehicleIndex, bool allowHoverModeFlight )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_DISABLE_HOVER_MODE_FLIGHT - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->pHandling->GetSpecialFlightHandlingData(),"SET_DISABLE_HOVER_MODE_FLIGHT - vehicle %s [%p] does not have a special flight mode!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
			pVehicle->SetDisableHoverModeFlight( allowHoverModeFlight );
		}
	}
}

bool CommandGetDisableHoverModeFlight(int vehicleIndex )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"GET_DISABLE_HOVER_MODE_FLIGHT - couldn't find vehicle with index %i", vehicleIndex))
	{
		if (scriptVerifyf(pVehicle->pHandling->GetSpecialFlightHandlingData(),"GET_DISABLE_HOVER_MODE_FLIGHT - vehicle %s [%p] does not have a special flight mode!", AILogging::GetDynamicEntityNameSafe(pVehicle), pVehicle))
		{
			return pVehicle->GetDisableHoverModeFlight();
		}
	}
	return false;
}

void CommandSetDeployOutriggers(int vehicleIndex, bool deploy )
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (scriptVerifyf(pVehicle,"SET_DEPLOY_OUTRIGGERS - couldn't find vehicle with index %i", vehicleIndex))
	{
		pVehicle->SetDeployOutriggers( deploy );
	}
}

bool CommandGetOutriggersDeployed(int vehicleIndex)
{	
    const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(vehicleIndex);
    if (scriptVerifyf(pVehicle,"GET_OUTRIGGERS_DEPLOYED - couldn't find vehicle with index %i", vehicleIndex))
    {
        return pVehicle->GetAreOutriggersFullyDeployed();
    }
    return false;
}


scrVector CommandFindSpawnCoordinatesForHeli(int TargetPedIndex)
{
	Vector3 v3Result(Vector3::ZeroType);

	const CPed* pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetPedIndex);
	if(SCRIPT_VERIFY(pTargetPed, "FIND_SPAWN_COORDINATES_FOR_HELI - failed to get ped!"))
	{
		v3Result = CHeli::FindSpawnCoordinatesForHeli(pTargetPed, true);
	}

	return v3Result;
}

void CommandSetDeployFoldingWings( int vehicleIndex, bool deploy, bool instant )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if( scriptVerifyf( pVehicle,"SET_DEPLOY_FOLDING_WINGS - couldn't find vehicle with index %i", vehicleIndex ) )
	{
		pVehicle->DeployFoldingWings(deploy, instant);
	}
}

bool GetAreFoldingWingsDeployed( int vehicleIndex )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if( scriptVerifyf( pVehicle,"ARE_FOLDING_WINGS_DEPLOYED - couldn't find vehicle with index %i", vehicleIndex ) )
	{
		return pVehicle->GetAreFoldingWingsDeployed();
	}

	return true;
}

void SetCanRollAndYawWhenCrashing( int vehicleIndex, bool allowRollAndYaw )
{
    CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
    if( scriptVerifyf( pVehicle && pVehicle->InheritsFromPlane(),"SET_CAN_ROLL_AND_YAW_WHEN_CRASHING - couldn't find plane with index %i", vehicleIndex ) )
    {
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
        pPlane->SetAllowRollAndYawWhenCrashing( allowRollAndYaw );
    }
}

void SetDipStraightDownWhenCrashing( int vehicleIndex, bool dipDownOnCrash )
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if( scriptVerifyf( pVehicle && pVehicle->InheritsFromPlane(),"SET_DIP_STRAIGHT_DOWN_WHEN_CRASHING_PLANE - couldn't find plane with index %i", vehicleIndex ) )
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->SetDipStraightDownWhenCrashing( dipDownOnCrash );
	}
}

void CommandSetTurretHidden( int VehicleIndex, int TurretIndex, bool HideTurret )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

    // create the vehicle weapon manager for the case there is no driver in the vehicle
    if(!pVehicle->GetVehicleWeaponMgr())
    {
        pVehicle->CreateVehicleWeaponMgr();
    }
    if (pVehicle && pVehicle->GetVehicleWeaponMgr())
    {
        if( TurretIndex < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets() )
        {
            CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret( TurretIndex );
            if( pTurret )
            {
                if( HideTurret )
                {
                    pTurret->GetFlags().SetFlag( CTurret::TF_Hide );
                }
                else 
                {
                    pTurret->GetFlags().ClearFlag( CTurret::TF_Hide );       
                }
            }
        }
    }
}

void CommandSetHoverModeWingRatio( int VehicleIndex, float WingRatio )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

    if( pVehicle )
    {
        pVehicle->SetTargetGravityScale( WingRatio );
        pVehicle->SetHoverModeWingRatio( WingRatio );
    }
}

void CommandDisableTurretMovement( int VehicleIndex, int TurretIndex )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

    // create the vehicle weapon manager for the case there is no driver in the vehicle
    if(!pVehicle->GetVehicleWeaponMgr())
    {
        pVehicle->CreateVehicleWeaponMgr();
    }
    if (pVehicle && pVehicle->GetVehicleWeaponMgr())
    {
        if( TurretIndex < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets() )
        {
            CTurret *pTurret = pVehicle->GetVehicleWeaponMgr()->GetTurret( TurretIndex );
            if( pTurret )
            {
                pTurret->GetFlags().SetFlag( CTurret::TF_DisableMovement );
            }
        }
    }
}

void CommandForceFixLinkMatrices( int VehicleIndex )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( pVehicle )
	{
		pVehicle->SetFixLinkMatrices();
	}
	
}

void CommandSetTransformRateForAnimation( int VehicleIndex, float TransformRate )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		pVehicle->SetTransformRateForCurrentAnimation( TransformRate  );
	}
}

void CommandSetTransformToSubmarineUsesAlternateInput( int VehicleIndex, bool UseAlternateInput )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		aiTask *pTask = pVehicle->GetIntelligence()->GetTaskManager()->GetTree( VEHICLE_TASK_TREE_SECONDARY )->GetActiveTask();
		if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE )
		{
			CTaskVehicleTransformToSubmarine* pTransformTask = static_cast<CTaskVehicleTransformToSubmarine*>( pTask );
			if( pTransformTask )
			{
				pTransformTask->SetUseAlternateInput( UseAlternateInput );
			}
		}
	}
}

float CommandGetVehicleMass( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		return pVehicle->GetMass();
	}

	return 0.0f;
}

float CommandGetVehicleWeaponDamageMult( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		return pVehicle->pHandling->m_fWeaponDamageMult;
	}

	return 0.0f;
}

float CommandGetVehicleCollisionDamageMult( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		return pVehicle->pHandling->m_fCollisionDamageMult;
	}

	return 0.0f;
}

bool CommandGetVehicleHasReducedStickyBombDamage( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle &&
		pVehicle->GetVehicleModelInfo() )
	{
		return pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DAMPEN_STICKBOMB_DAMAGE );
	}

	return false;
}

bool CommandGetVehicleHasCappedExplosionDamage( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle &&
		pVehicle->GetVehicleModelInfo() )
	{
		return pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_CAPPED_EXPLOSION_DAMAGE );
	}

	return false;
}

bool CommandGetVehicleHasBulletProofGlass( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle &&
		pVehicle->GetVehicleModelInfo() )
	{
		return pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS );
	}

	return false;
}

bool CommandGetVehicleHasBulletResistantGlass(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle &&
		pVehicle->GetVehicleModelInfo())
	{
		return pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLET_RESISTANT_GLASS);
	}

	return false;
}

void CommandSetVehicleCombatMode(bool UNUSED_PARAM(InCombatMode))
{
}

void CommandSetVehicleDetonationMode(bool UNUSED_PARAM(InDetonationMode))
{
}

void CommandSetVehicleShuntOnStick(bool UNUSED_PARAM(ShuntOnStick))
{
}

bool CommandGetIsVehicleShunting(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		return pVehicle->GetSideShuntForce() != 0.0f;
	}

	return false;
}

bool CommandHasVehicleBeenHitByShunt(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		return pVehicle->GetHitBySideShunt() && pVehicle->GetSideShuntForce() == 0.0f;
	}

	return false;
}

int CommandGetLastVehicleHitByShunt(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle &&
		pVehicle->GetLastSideShuntVehicle())
	{
		return CTheScripts::GetGUIDFromEntity(*pVehicle->GetLastSideShuntVehicle());
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}

bool CommandHasVehicleBeenHitByWeaponBlade(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		return pVehicle->GetHitByWeaponBlade();
	}

	return false;
}

void CommandDisableCarExplosionsDamage(bool disableDamage)
{
	CVehicle::sm_bDisableExplosionDamage = disableDamage;
}

void CommandSetOverrideNitrousLevel(int VehicleIndex, bool overrideBoostLevel, float durationModOverride, float powerModOverride, float rechargeRateModOverride, bool disableDefaultSound = false)
{	
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		vehicleDebugf1("CommandSetOverrideNitrousLevel: %d, %d, %f, %f, %f, %d", VehicleIndex, overrideBoostLevel ? 1 : 0, durationModOverride, powerModOverride, rechargeRateModOverride, disableDefaultSound);
		pVehicle->SetOverrideNitrousLevel(overrideBoostLevel, durationModOverride, powerModOverride, rechargeRateModOverride, disableDefaultSound);
	}
	else
	{
		vehicleDebugf1("CommandSetOverrideNitrousLevel: Vehicle index invalid: %d", VehicleIndex);
	}
}

void CommandFullyChargeNitrous(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		vehicleDebugf1("CommandFullyChargeNitrous: Full charge nitrous");
		pVehicle->FullyChargeNitrous();
	}
	else
	{
		vehicleDebugf1("CommandFullyChargeNitrous: Vehicle index invalid: %d", VehicleIndex);
	}
}

void CommandClearNitrous(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		vehicleDebugf1("CommandClearNitrous: Clearing nitrous");
		pVehicle->ClearNitrous();
	}
	else
	{
		vehicleDebugf1("CommandClearNitrous: Vehicle index invalid: %d", VehicleIndex);
	}
}

void CommandSetNitrousIsActive(int VehicleIndex, bool isActive)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle)
	{
		vehicleDebugf1("CommandSetNitrousActive: Setting Nitrous to %d", isActive ? 1 : 0);
		pVehicle->SetNitrousActive(isActive);
	}
	else
	{
		vehicleDebugf1("CommandSetNitrousActive: Vehicle index invalid: %d", VehicleIndex);
	}
}

void CommandSetIncreaseWheelCrushDamage( int VehicleIndex, bool increaseDamage )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle )
	{
		pVehicle->SetIncreaseWheelCrushDamage( increaseDamage );
	}
}

bool CommandGetIsVehicleRemoteControlCar(int VehicleIndex)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >(VehicleIndex);

	if (pVehicle && pVehicle->pHandling && pVehicle->pHandling->mFlags & MF_IS_RC)
	{
		return true;
	}

	return false;
}


void CommandSetDisableWeaponBladeForces( bool disableForce )
{
	CVehicle::sm_bDisableWeaponBladeForces = disableForce;
}

void CommandSetUseDoubleClickForCarJump( bool useDoubleClick )
{
	CVehicle::sm_bDoubleClickForJump = useDoubleClick;
}

bool CommandGetDoesVehicleHaveTombstone( int VehicleIndex )
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID< CVehicle >( VehicleIndex );

	if( pVehicle->HasTombstone() )
	{
		return true;
	}
	return false;
}

void CommandHideTombstone( int VehicleIndex, bool HideTombstone )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID< CVehicle >( VehicleIndex );
	pVehicle->HideTombstone( HideTombstone );
}

void CommandApplyEMPEffect(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "APPLY_EMP_EFFECT - Vehicle not found"))
	{
		pVehicle->ApplyExplosionEffectEMP();
	}
}

bool CommandGetIsVehicleDisabledByEMP(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "GET_IS_VEHICLE_DISABLED_BY_EMP - Vehicle not found"))
	{
		return pVehicle->GetExplosionEffectEMP();
	}

	return false;
}

void CommandApplySlickEffect(int VehicleIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "APPLY_SLICK_EFFECT - Vehicle not found"))
	{
		pVehicle->ApplyExplosionEffectSlick();
	}
}

bool CommandGetIsVehicleAffectedBySlick(int VehicleIndex)
{
	CVehicle const* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "GET_IS_VEHICLE_AFFECTED_BY_SLICK - Vehicle not found"))
	{
		return pVehicle->GetExplosionEffectSlick();
	}

	return false;
}

void CommandApplySteeringBias(int VehicleIndex, float SteeringBias, float Lifetime)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (SCRIPT_VERIFY(pVehicle, "APPLY_STEERING_BIAS - Vehicle not found"))
	{
		CAutomobile* pAutomobile = dynamic_cast<CAutomobile*>(pVehicle);
		if (pAutomobile)
		{
			pAutomobile->ApplySteeringBias(SteeringBias, Lifetime);
		}
		else
		{
			vehicleDebugf1("APPLY_STEERING_BIAS - sterring bias not applied as this vehicle index (%d) is not an automobile", VehicleIndex);
		}
	}
}

void CommandSetDisableRetractingWeaponBlades( bool DisableRetracting )
{
	CWeaponBlade::ms_bDisableRetractingWeaponBlades = DisableRetracting;
}

float CommandGetTyreHealth( int VehicleIndex, int WheelNumber )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            return pWheel->GetTyreHealth();
        }
    }
    return 0.0f;
}

void CommandSetTyreHealth( int VehicleIndex, int WheelNumber, float TyreHealth )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            pWheel->SetTyreHealth( TyreHealth );
        }
    }
}

float CommandGetTyreWearRate( int VehicleIndex, int WheelNumber )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            return pWheel->GetTyreWearRate();
        }
    }
    return 0.0f;
}

void CommandSetTyreWearRate( int VehicleIndex, int WheelNumber, float WearRate )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            pWheel->SetTyreWearRate( WearRate );
        }
    }
}

void CommandSetTyreWearRateScale( int VehicleIndex, int WheelNumber, float WearRateScale )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            pWheel->SetWearRateScale( WearRateScale );
        }
    }
}

void CommandMaxGripDifferenceDueToWearRate( int VehicleIndex, int WheelNumber, float gripDiff )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        eHierarchyId wheelId = CWheel::GetScriptWheelId( (eScriptWheelList)WheelNumber );
        CWheel *pWheel = pVehicle->GetWheelFromId( wheelId );

        if( pWheel )
        {
            pWheel->SetMaxGripDiffFromWearRate( gripDiff );
        }
    }
}

void CommandResetDownforceScales( int UNUSED_PARAM( VehicleIndex ) )
{
//    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
//    if( pVehicle )
//    {
//        pVehicle->m_fDownforceModifierFront = 1.0f;
//        pVehicle->m_fDownforceModifierRear = 1.0f;
//    }
}

void CommandModifyDownforce( int UNUSED_PARAM( VehicleIndex ), float UNUSED_PARAM( FrontChange ), float UNUSED_PARAM( RearChange ) )
{
    //CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    //if( pVehicle )
    //{
    //    pVehicle->m_fDownforceModifierFront = Clamp( pVehicle->m_fDownforceModifierFront + FrontChange, 0.0f, 2.0f );
    //    pVehicle->m_fDownforceModifierRear = Clamp( pVehicle->m_fDownforceModifierFront + RearChange, 0.0f, 2.0f );
    //}
}

void CommandTiggerPumppedUpJump( int VehicleIndex, float HeightScale )
{
    CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle )
    {
        pVehicle->TriggerPumpedUpJump( HeightScale );
    }
}

void CommandSetAircraftIngoreHeightMapOptimisation( int VehicleIndex, bool IgnoreHeightMap )
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>( VehicleIndex );
    if( pVehicle && pVehicle->InheritsFromAutomobile() )
    {
        ((CAutomobile*)pVehicle)->m_bIgnoreWorldHeightMapForWheelProbes = IgnoreHeightMap;
    }
}


void CommandNetworkUseHighPrecisionTrainBlending(int networkID, bool useHighPrecision)
{
	if (scriptVerifyf(NetworkInterface::IsGameInProgress(), "%s:NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING - Network game not running", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
		scriptVerifyf(networkID != INVALID_SCRIPT_OBJECT_ID, "%s:NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING - Invalid network ID supplied!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPhysical *physical = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptEntity(static_cast<ScriptObjectId>(networkID));

		if (scriptVerifyf(physical, "%s:NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING - no entity with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(scriptVerifyf(physical->GetNetworkObject(), "%s:NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING - specific entity is not networked!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if (scriptVerifyf(physical->GetIsTypeVehicle(), "%s:NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING - Only supported for trains!", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					CNetObjTrain *netObjTrain = SafeCast(CNetObjTrain, physical->GetNetworkObject());

					netObjTrain->SetUseHighPrecisionBlending(useHighPrecision);
				}
			}
		}
	}
}

void CommandSetOverrideVehGroup( const char* vehGroupOverride, int overridePercentage )
{
	if(scriptVerifyf(CPopCycle::HasValidCurrentPopAllocation(), "SET_OVERRIDE_VEH_GROUP - Current Pop Allocation is not valid, no override applied" ))
	{
		u32 vehGroup;
		if(scriptVerifyf(CPopCycle::GetPopGroups().FindVehGroupFromNameHash(atHashValue(vehGroupOverride), vehGroup), "SET_OVERRIDE_VEH_GROUP - Could not find veh group from given name"))
		{
			(const_cast<CPopSchedule*>(&CPopCycle::GetCurrentPopSchedule()))->SetOverrideVehGroup(vehGroup, (u32)overridePercentage);
		}
	}
}

void CommandSetCheckForEnoughRoomToFitPed(int iVehicleIndex, bool bCheckForEnoughRoomToFitPed)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex);
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bCheckForEnoughRoomToFitPed = bCheckForEnoughRoomToFitPed;
	}
}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(CREATE_VEHICLE,0xba715a7beba5a1f9, CommandCreateVehicle);
		SCR_REGISTER_SECURE(DELETE_VEHICLE,0xd59cc8123fd1a789, CommandDeleteVehicle);
		SCR_REGISTER_UNUSED(REMOVE_VEHICLE_FROM_PARKED_VEHICLES_BUDGET,0x2bae2b907287f34c, CommandRemoveVehicleFromParkedVehiclesBudget);
		SCR_REGISTER_UNUSED(SET_VEHICLE_IN_CUTSCENE,0x90497b5e4fa16480, CommandSetVehicleInCutscene);
		SCR_REGISTER_SECURE(SET_VEHICLE_ALLOW_HOMING_MISSLE_LOCKON,0x4793e369e98dc1b3, CommandSetVehicleAllowHomingMissleLockon);
		SCR_REGISTER_SECURE(SET_VEHICLE_ALLOW_HOMING_MISSLE_LOCKON_SYNCED,0x52ed1ceb870c0a42, CommandSetVehicleAllowHomingMissleLockonSynced);
		SCR_REGISTER_SECURE(SET_VEHICLE_ALLOW_NO_PASSENGERS_LOCKON,0x31048e31eaaf3aba, CommandSetVehicleAllowNoPassengersLockon);
		SCR_REGISTER_SECURE(GET_VEHICLE_HOMING_LOCKON_STATE,0x09878227cf5b2063, CommandGetVehicleHomingLockOnState);
		SCR_REGISTER_SECURE(GET_VEHICLE_HOMING_LOCKEDONTO_STATE,0x9035dfa99b4e0838, CommandGetVehicleHomingLockedOntoState);
		SCR_REGISTER_SECURE(SET_VEHICLE_HOMING_LOCKEDONTO_STATE,0x8955d96a20ff3811, CommandSetVehicleHomingLockedOntoState);
		SCR_REGISTER_SECURE(IS_VEHICLE_MODEL,0x7010991fda59d3f2, CommandIsVehicleModel);
		SCR_REGISTER_SECURE(DOES_SCRIPT_VEHICLE_GENERATOR_EXIST,0xd23a6eb80c5ca596, CommandDoesScriptVehicleGeneratorExist);
		SCR_REGISTER_SECURE(CREATE_SCRIPT_VEHICLE_GENERATOR,0x375bb75b2409befc, CommandCreateScriptVehicleGenerator);
		SCR_REGISTER_SECURE(DELETE_SCRIPT_VEHICLE_GENERATOR,0xfe29ec7e7bcc0e15, CommandDeleteScriptVehicleGenerator);
		SCR_REGISTER_SECURE(SET_SCRIPT_VEHICLE_GENERATOR,0xe8eeb9e60d338c50, CommandSetScriptVehicleGenerator);
		SCR_REGISTER_SECURE(SET_ALL_VEHICLE_GENERATORS_ACTIVE_IN_AREA,0x84a65e2e9cfb6a77, CommandSetAllVehicleGeneratorsActiveInArea);
		SCR_REGISTER_SECURE(SET_ALL_VEHICLE_GENERATORS_ACTIVE,0x7b8dc5701d211549, CommandSetAllVehicleGeneratorsActive);
		SCR_REGISTER_SECURE(SET_ALL_LOW_PRIORITY_VEHICLE_GENERATORS_ACTIVE,0x8c3444d417673c8a, CommandSetAllLowPriorityVehicleGeneratorsActive);
		SCR_REGISTER_UNUSED(SET_VEH_GEN_SPAWN_LOCKED_THIS_FRAME,0x03b9f94cb412deb3, CommandSetVehGenSpawnLockedThisFrame);	
		SCR_REGISTER_UNUSED(SET_VEH_GEN_LOCKED_PERCENTAGE,0x4619793e335d1135, CommandSetVehGenSpawnLockedPercentage);	
		SCR_REGISTER_UNUSED(NETWORK_SET_ALL_LOW_PRIORITY_VEHICLE_GENERATORS_WITH_HELI_ACTIVE,0x8634ecf8afeb90f6, CommandNetworkSetAllLowPriorityVehicleGeneratorsWithHeliActive);
		SCR_REGISTER_SECURE(SET_VEHICLE_GENERATOR_AREA_OF_INTEREST,0x289aed7c360b6f6c, CommandSetVehicleGeneratorAreaOfInterest);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_GENERATOR_AREA_OF_INTEREST,0x2da7b08cec309602, CommandClearVehicleGeneratorAreaOfInterest);
		SCR_REGISTER_SECURE(SET_VEHICLE_ON_GROUND_PROPERLY,0x9f65b3bafa302a65, CommandSetVehicleOnGroundProperly);
		SCR_REGISTER_SECURE(SET_VEHICLE_USE_CUTSCENE_WHEEL_COMPRESSION,0xf9639b5c765fed1f, CommandSetVehicleUseCutsceneWheelCompression);
		SCR_REGISTER_SECURE(IS_VEHICLE_STUCK_ON_ROOF,0x66aa8bc9ca1be26c, CommandIsVehicleStuckOnRoof);
		SCR_REGISTER_SECURE(ADD_VEHICLE_UPSIDEDOWN_CHECK,0x66e36c5e6e28930c, CommandAddVehicleUpsidedownCheck);
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_UPSIDEDOWN_CHECK,0xf5644c53558c678d, CommandRemoveVehicleUpsidedownCheck);
		SCR_REGISTER_SECURE(IS_VEHICLE_STOPPED,0xa7a8e89ea6c5e222, CommandIsVehicleStopped);
		SCR_REGISTER_SECURE(GET_VEHICLE_NUMBER_OF_PASSENGERS,0x5eb186df27ffcf1a, CommandGetFVehicleNumberOfPassengers);
		SCR_REGISTER_SECURE(GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS,0xadd893bd545be5fc, CommandGetMaxNumberOfPassengers);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_NUMBER_OF_SEATS,0xd664875c4bdb14b0, CommandGetVehicleModelNumberOfSeats);
		SCR_REGISTER_SECURE(IS_SEAT_WARP_ONLY,0xe632c45f7e4013b9, CommandIsSeatWarpOnly);
		SCR_REGISTER_SECURE(IS_TURRET_SEAT,0x38b74904557f43cc, CommandIsTurretSeat);
		SCR_REGISTER_SECURE(DOES_VEHICLE_ALLOW_RAPPEL,0x18cee6d8a7493206, CommandDoesVehicleAllowRappel);
		SCR_REGISTER_UNUSED(DOES_VEHICLE_HAVE_REAR_SEAT_ACTIVITIES,0x1cbdd0f7fdea8fe3, CommandDoesVehicleHaveRearSeatActivities);

		SCR_REGISTER_UNUSED(SET_CONSUME_PETROL,0xf941a60db2cca5bb, CommandSetConsumePetrol);
		SCR_REGISTER_UNUSED(GET_CONSUME_PETROL,0x0808853f711bc981, CommandGetConsumePetrol);
		SCR_REGISTER_UNUSED(GET_VEHICLE_PETROL_STATS,0x6b6914c292aaadc0, CommandGetVehiclePetrolStats);
		SCR_REGISTER_UNUSED(SET_PETROL_CONSUMPTION_RATE,0x9f9f53e9757720db, CommandSetPetrolConsumptionRate);

		SCR_REGISTER_UNUSED(SET_VEHICLE_DENSITY_MULTIPLIER, 0x8b289f79, CommandSetVehicleDensityMultiplier);
        SCR_REGISTER_UNUSED(SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER, 0x876a0bce, CommandSetRandomVehicleDensityMultiplier);
        SCR_REGISTER_UNUSED(SET_PARKED_VEHICLE_DENSITY_MULTIPLIER, 0x98e5c8a7, CommandSetParkedVehicleDensityMultiplier);
		SCR_REGISTER_UNUSED(SET_AMBIENT_VEHICLE_RANGE_MULTIPLIER, 0x3b6197bb, CommandSetAmbientVehicleRangeMultiplier);
		SCR_REGISTER_SECURE(SET_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME,0x6b81df75fa671549, CommandSetVehicleDensityMultiplierThisFrame);
		SCR_REGISTER_SECURE(SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME,0xbb35b4e1d532f811, CommandSetRandomVehicleDensityMultiplierThisFrame);
		SCR_REGISTER_SECURE(SET_PARKED_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME,0x3c92b0470b23a5d2, CommandSetParkedVehicleDensityMultiplierThisFrame);
		SCR_REGISTER_SECURE(SET_DISABLE_RANDOM_TRAINS_THIS_FRAME,0x83d185533a7a8e2e, CommandSetDisableRandomTrainsThisFrame);
		SCR_REGISTER_SECURE(SET_AMBIENT_VEHICLE_RANGE_MULTIPLIER_THIS_FRAME,0xf02e66a4059dc629, CommandSetAmbientVehicleRangeMultiplierThisFrame);
		SCR_REGISTER_UNUSED(SET_DESERTED_STREET_MULTIPLIER_LOCKED,0x4eecda2a46438b49, CommandDesertedStreetMultiplierLocked);

		
		SCR_REGISTER_SECURE(SET_FAR_DRAW_VEHICLES,0x9b2661f051d4de62, CommandSetFarDrawVehicles);
		SCR_REGISTER_SECURE(SET_NUMBER_OF_PARKED_VEHICLES,0x7fb3477c478f833e, CommandSetNumberOfParkedVehicles);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED,0x3b2646b62e7bbe11, CommandSetVehicleDoorsLocked);
		SCR_REGISTER_SECURE(SET_VEHICLE_INDIVIDUAL_DOORS_LOCKED,0xea390390606de844, CommandSetVehicleIndividualDoorsLocked);
		SCR_REGISTER_SECURE(SET_VEHICLE_HAS_MUTED_SIRENS,0xaf026bfb427d2c6f, CommandSetVehicleHasMutedSirens);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED_FOR_PLAYER,0xceefce6c0d7566c9, CommandSetVehicleDoorsLockedForPlayer);
		SCR_REGISTER_SECURE(GET_VEHICLE_DOORS_LOCKED_FOR_PLAYER,0xe63219d598ed6478, CommandGetVehicleDoorsLockedForPlayer);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED_FOR_ALL_PLAYERS,0x2706c6faa25ab1b7, CommandSetVehicleDoorsLockedForAllPlayers);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED_FOR_NON_SCRIPT_PLAYERS,0x484ccfb803fcfd80, CommandSetVehicleDoorsLockedForNonScriptPlayers);
        SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED_FOR_TEAM,0x00810c7d84492ae4, CommandSetVehicleDoorsLockedForTeam);
		SCR_REGISTER_UNUSED(GET_VEHICLE_DOORS_LOCKED_FOR_TEAM,0x20fa5b4ddfd395e0, CommandGetVehicleDoorsLockedForTeam);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_LOCKED_FOR_ALL_TEAMS,0xdb3d9d1ce4f105fb, CommandSetVehicleDoorsLockedForAllTeams);
        SCR_REGISTER_UNUSED(SET_VEHICLE_DOORS_TEAM_LOCK_OVERRIDE_FOR_PLAYER,0x2bfc202fd3859038, CommandSetVehicleDoorsTeamLockOverrideForPlayer);

		SCR_REGISTER_UNUSED(SET_ENTER_LOCKED_FOR_SPECIAL_EDITION_VEHICLES,0x0b71df77491c6940, CommandSetEnterLockedForSpecialEditionVehicles);
		SCR_REGISTER_SECURE(SET_VEHICLE_DONT_TERMINATE_TASK_WHEN_ACHIEVED,0x9c2afd6cc05195ef, CommandSetDontTerminateOnMissionAchieved);
		
		SCR_REGISTER_UNUSED(SET_VEHICLE_MODEL_DOORS_LOCKED_FOR_PLAYER,0x15d2ec556ea8dfb7, CommandSetVehicleModelDoorsLockedForPlayer);
		SCR_REGISTER_UNUSED(SET_VEHICLE_MODEL_INSTANCE_DOORS_LOCKED_FOR_PLAYER,0x923a8a885d782623, CommandSetVehicleModelInstanceDoorsLockedForPlayer);
		SCR_REGISTER_UNUSED(CLEAR_VEHICLE_MODEL_DOORS_LOCKED_FOR_PLAYER,0x71f98db115d00c5f, CommandClearVehicleModelDoorsLockedForPlayer);
		SCR_REGISTER_UNUSED(CLEAR_VEHICLE_MODEL_INSTANCE_DOORS_LOCKED_FOR_PLAYER,0x066d46dd818c3ef2, CommandClearVehicleModelInstanceDoorsLockedForPlayer);

		SCR_REGISTER_UNUSED(SET_VEHICLE_DOOR_OPEN_FOR_PATHFINDER,0xa88e6b0bfa86c1b9, CommandSetVehicleDoorOpenForPathfinder);

		SCR_REGISTER_SECURE(EXPLODE_VEHICLE,0x207617ce1796d28e, CommandExplodeVehicle);
		SCR_REGISTER_UNUSED(SET_VEHICLE_FULL_THROTTLE_TIMER,0x8af5a56952b85be2, CommandSetVehicleFullThrottleTimer);
		SCR_REGISTER_SECURE(SET_VEHICLE_OUT_OF_CONTROL,0x15c98c9826ee029a, CommandSetVehicleOutOfControl);
		SCR_REGISTER_SECURE(SET_VEHICLE_TIMED_EXPLOSION,0xc91d36e05e1b8246, CommandSetVehicleTimedExplosion);
		SCR_REGISTER_SECURE(ADD_VEHICLE_PHONE_EXPLOSIVE_DEVICE,0xdc7a3584f47a942c, CommandAddVehiclePhoneExplosiveDevice);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_PHONE_EXPLOSIVE_DEVICE,0x62a0df6cf911efd1, CommandClearVehiclePhoneExplosiveDevice);
		SCR_REGISTER_SECURE(HAS_VEHICLE_PHONE_EXPLOSIVE_DEVICE,0x9a31ea3ceae94e4a, CommandHasVehiclePhoneExplosiveDevice);
		SCR_REGISTER_SECURE(DETONATE_VEHICLE_PHONE_EXPLOSIVE_DEVICE,0x4e9b81412bfbab75, CommandDetonateVehiclePhoneExplosiveDevice);
		SCR_REGISTER_UNUSED(HAVE_VEHICLE_REAR_DOORS_BEEN_BLOWN_OPEN_BY_STICKYBOMB,0x55102167e76f0bee, CommandHaveVehicleRearDoorsBeenBlownOpenByStickybomb);
		SCR_REGISTER_SECURE(SET_TAXI_LIGHTS,0x8861c13bcb159f69, CommandSetTaxiLights);
		SCR_REGISTER_SECURE(IS_TAXI_LIGHT_ON,0x9d4f3b95ed8fcb0c, CommandIsTaxiLightOn);
		SCR_REGISTER_SECURE(IS_VEHICLE_IN_GARAGE_AREA,0xbef939cd3af0b8f0, CommandIsVehicleInGarageArea);
		SCR_REGISTER_SECURE(SET_VEHICLE_COLOURS,0x58dafdeb2f46a5ea, CommandSetVehicleColour);
		SCR_REGISTER_SECURE(SET_VEHICLE_FULLBEAM,0x7eeabc35fcfa08b7, CommandSetVehicleFullBeam);
		SCR_REGISTER_SECURE(SET_VEHICLE_IS_RACING,0xe4a910d49c232c19, CommandSetVehicleIsRacing);
		SCR_REGISTER_SECURE(SET_VEHICLE_CUSTOM_PRIMARY_COLOUR,0x401f3880d64cb7d9, CommandSetVehiclePrimaryColour);
		SCR_REGISTER_SECURE(GET_VEHICLE_CUSTOM_PRIMARY_COLOUR,0x3d8bb85a33e13d09, CommandGetVehiclePrimaryColour);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR,0x812006f6c16bbea2, CommandClearVehiclePrimaryColour);
		SCR_REGISTER_SECURE(GET_IS_VEHICLE_PRIMARY_COLOUR_CUSTOM,0xa4429f0196864bb8, CommandGetIsVehiclePrimaryColourCustom);
		SCR_REGISTER_SECURE(SET_VEHICLE_CUSTOM_SECONDARY_COLOUR,0x4a498f8f5db5d526, CommandSetVehicleSecondaryColour);
		SCR_REGISTER_SECURE(GET_VEHICLE_CUSTOM_SECONDARY_COLOUR,0xfb61e9a85a29dda7, CommandGetVehicleSecondaryColour);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR,0xd27cad777bb60ae5, CommandClearVehicleSecondaryColour);
		SCR_REGISTER_SECURE(GET_IS_VEHICLE_SECONDARY_COLOUR_CUSTOM,0x21db5182c34b6c0e, CommandGetIsVehicleSecondaryColourCustom);
		SCR_REGISTER_SECURE(SET_VEHICLE_ENVEFF_SCALE,0x6df92db1542a3e0d, CommandSetVehicleEnvEffScale);
		SCR_REGISTER_SECURE(GET_VEHICLE_ENVEFF_SCALE,0x09566e2739f599fa, CommandGetVehicleEnvEffScale);
		SCR_REGISTER_SECURE(SET_CAN_RESPRAY_VEHICLE,0x65abd7fbffdbb638, CommandSetCanResprayVehicle);
		SCR_REGISTER_SECURE(SET_GOON_BOSS_VEHICLE,0x04f8a1fbe3e7bf6b, CommandSetGoonBossVehicle);
		SCR_REGISTER_SECURE(SET_OPEN_REAR_DOORS_ON_EXPLOSION,0x1f68e53a00b640e2, CommandSetOpenRearDoorsOnExplosion);
		SCR_REGISTER_UNUSED(SET_GARAGE_TYPE,0xb0d68498f687a960, CommandSetGarageType);
		SCR_REGISTER_UNUSED(SET_GARAGE_LEAVE_CAMERA_ALONE,0x6786dfd085f5db0d, CommandSetGarageLeaveCameraAlone);
		SCR_REGISTER_SECURE(FORCE_SUBMARINE_SURFACE_MODE,0xa216bc5d0f68ced4, CommandForceSubmarineSurfaceMode);
		SCR_REGISTER_SECURE(FORCE_SUBMARINE_NEURTAL_BUOYANCY,0x3a6032fb27feb48c, CommandForceSubmarineNeutralBuoyancy);
		SCR_REGISTER_SECURE(SET_SUBMARINE_CRUSH_DEPTHS,0x7cd91126b58f25ed, CommandSetSubmarineCrushDepths);
		SCR_REGISTER_UNUSED(GET_SUBMARINE_IS_UNDER_CRUSH_DEPTH,0xbda170f210c62871, CommandGetSubmarineIsUnderCrushDepth);
		SCR_REGISTER_SECURE(GET_SUBMARINE_IS_UNDER_DESIGN_DEPTH,0xc1b954f29c44e286, CommandGetSubmarineIsUnderDesignDepth);
		SCR_REGISTER_SECURE(GET_SUBMARINE_NUMBER_OF_AIR_LEAKS,0x532b2915c613363f, CommandGetSubmarineNumberOfAirLeaks);
		SCR_REGISTER_SECURE(SET_BOAT_IGNORE_LAND_PROBES,0x7f44d71e8e7ad640, CommandSetBoatIgnoreLandProbes);
		SCR_REGISTER_SECURE(SET_BOAT_ANCHOR,0xae2662f6a912f71e, CommandSetBoatAnchor);
		SCR_REGISTER_SECURE(CAN_ANCHOR_BOAT_HERE,0x59901c7e30dcb902, CommandCanAnchorBoatHere);
		SCR_REGISTER_SECURE(CAN_ANCHOR_BOAT_HERE_IGNORE_PLAYERS,0x97a5f80399e1beb1, CommandCanAnchorBoatHereIgnorePlayersStandingOnVehicle);
		SCR_REGISTER_SECURE(SET_BOAT_REMAINS_ANCHORED_WHILE_PLAYER_IS_DRIVER,0x577f790cb611bd49, CommandSetBoatRemainsAnchoredWhilePlayerIsDriver);
		SCR_REGISTER_SECURE(SET_FORCE_LOW_LOD_ANCHOR_MODE,0x860e5b23064dfcbb, CommandForceLowLodAnchorMode);
		SCR_REGISTER_SECURE(SET_BOAT_LOW_LOD_ANCHOR_DISTANCE,0x9f5508fed14c9e13, CommandSetBoatLowLodAnchorDistance);
		SCR_REGISTER_SECURE(IS_BOAT_ANCHORED,0xc8bbe1bc9cd6658c, CommandIsBoatAnchored);
		SCR_REGISTER_SECURE(SET_BOAT_SINKS_WHEN_WRECKED,0x033cf61c91c35914, CommandSetBoatSinksWhenWrecked);
		SCR_REGISTER_SECURE(SET_BOAT_WRECKED,0xef08e9e3afd3fb8e, CommandSetBoatWrecked);
		SCR_REGISTER_UNUSED(OPEN_GARAGE,0x3e6826cc6f518225, CommandOpenGarage);
		SCR_REGISTER_UNUSED(CLOSE_GARAGE,0x92686ea75bc16ee4, CommandCloseGarage);
		SCR_REGISTER_SECURE(SET_VEHICLE_SIREN,0xbf0ddddacd98c554, CommandSetVehicleSiren);
		SCR_REGISTER_UNUSED(GET_VEHICLE_SIREN_HEALTH,0x1043281456646348, CommandGetVehicleSirenHealth);
		SCR_REGISTER_SECURE(IS_VEHICLE_SIREN_ON,0xe3f9ed416083bae0, CommandIsVehicleSirenOn);
		SCR_REGISTER_SECURE(IS_VEHICLE_SIREN_AUDIO_ON,0xfd37e16f7768e05a, CommandIsVehicleSirenAudioOn);
		SCR_REGISTER_UNUSED(SET_VEHICLE_TURNS_TO_FACE_COORD,0xd8121ce97bd336f9, CommandSetVehicleTurnsToFaceCoord);
		SCR_REGISTER_SECURE(SET_VEHICLE_STRONG,0xff74dec3f62d3654, CommandSetVehicleStrong);
		SCR_REGISTER_UNUSED(IS_GARAGE_OPEN,0xd28e53d9795c7e9b, CommandIsGarageOpen);
		SCR_REGISTER_UNUSED(IS_GARAGE_CLOSED,0x005cd51f07fbe04c, CommandIsGarageClosed);
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_STUCK_CHECK,0x64859f2b7f1e9459, CommandRemoveVehicleStuckCheck);
		SCR_REGISTER_SECURE(GET_VEHICLE_COLOURS,0xdca20dba52b32eec, CommandGetVehicleColours);
		SCR_REGISTER_UNUSED(SET_ALL_VEHICLES_CAN_BE_DAMAGED,0x2f93d1a4b5592913, CommandSetAllVehiclesCanBeDamaged);
		SCR_REGISTER_SECURE(IS_VEHICLE_SEAT_FREE,0x7158135695fae89d, CommandIsVehicleSeatFree);
		SCR_REGISTER_SECURE(GET_PED_IN_VEHICLE_SEAT,0xab793ea186ab8daa, CommandGetPedInVehicleSeat);
		SCR_REGISTER_SECURE(GET_LAST_PED_IN_VEHICLE_SEAT,0x48512ac2c4218836, CommandGetLastPedInVehicleSeat);
		SCR_REGISTER_SECURE(GET_VEHICLE_LIGHTS_STATE,0x9a899383ad37fc41, CommandGetVehicleLightsState);
		SCR_REGISTER_SECURE(IS_VEHICLE_TYRE_BURST,0xb4c63f21d1dc6c23, CommandIsVehicleTyreBurst);
		SCR_REGISTER_SECURE(SET_VEHICLE_FORWARD_SPEED,0xb2d5451cc5606248, CommandSetVehicleForwardSpeed);
		SCR_REGISTER_SECURE(SET_VEHICLE_FORWARD_SPEED_XY,0xde738043797837a5, CommandSetVehicleForwardSpeedXY);
		SCR_REGISTER_SECURE(BRING_VEHICLE_TO_HALT,0xd28e3800883ded83, CommandBringVehicleToHalt);
		SCR_REGISTER_SECURE(SET_VEHICLE_STEER_FOR_BUILDINGS,0x8ede85651d0232a2, CommandSetVehicleSteerForBuildings);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAUSES_SWERVING,0x91830eecd92c6800, CommandSetVehicleCausesSwerving);
		SCR_REGISTER_SECURE(SET_IGNORE_PLANES_SMALL_PITCH_CHANGE,0xca3b69c048341635, CommandSetIgnorePlanesSmallPitchChange);
		SCR_REGISTER_SECURE(STOP_BRINGING_VEHICLE_TO_HALT,0x7bd360f9bdf6e098, CommandStopBringingVehicleToHalt);
		SCR_REGISTER_SECURE(IS_VEHICLE_BEING_BROUGHT_TO_HALT,0x14ac6820e96ac452, CommandIsVehicleBeingBroughtToHalt);
		SCR_REGISTER_UNUSED(LOWER_FORKLIFT_FORKS,0x2aaf406915a80983, CommandLowerForkliftForks);
		SCR_REGISTER_UNUSED(RAISE_FORKLIFT_FORKS,0x63581dbc79f6d1a5, CommandRaiseForkliftForks);
		SCR_REGISTER_SECURE(SET_FORKLIFT_FORK_HEIGHT,0x191fe5efd6bc0329, CommandSetForkliftForkHeight);
		SCR_REGISTER_UNUSED(LOCK_FORKLIFT_FORKS,0x5e98610343786a43, CommandLockForkliftForks);
		SCR_REGISTER_UNUSED(IS_ENTITY_ATTACHED_TO_FORKLIFT_FORKS,0x84484da67bcbf778, CommandIsEntityAttachedToForkliftForks);
		SCR_REGISTER_UNUSED(IS_ANY_ENTITY_ATTACHED_TO_FORKLIFT_FORKS,0xd42e522cba88f0bd, CommandIsAnyEntityAttachedToForkliftForks);
		SCR_REGISTER_UNUSED(ATTACH_PALLET_TO_FORKLIFT_FORKS,0xbbc1611503773a0b, CommandAttachPalletToForkliftForks);
		SCR_REGISTER_UNUSED(DETACH_PALLET_FROM_FORKLIFT_FORKS,0x8334d0808a48cd35, CommandDetachPalletFromForkliftForks);
		SCR_REGISTER_UNUSED(LOWER_HANDLER_FRAME,0x31a3ce65c2b365f3, CommandLowerHandlerFrame);
		SCR_REGISTER_UNUSED(RAISE_HANDLER_FRAME,0xe8e0c37a369d84bb, CommandRaiseHandlerFrame);
		SCR_REGISTER_UNUSED(SET_HANDLER_FRAME_HEIGHT,0x8c28cdb1ca6f46ce, CommandSetHandlerFrameHeight);
		SCR_REGISTER_UNUSED(LOCK_HANDLER_FRAME,0x5521b58680ab752e, CommandLockForkliftForks);
		SCR_REGISTER_SECURE(IS_ENTITY_ATTACHED_TO_HANDLER_FRAME,0xc27ac1adfd86ac9d, CommandIsEntityAttachedToHandlerFrame);
		SCR_REGISTER_SECURE(IS_ANY_ENTITY_ATTACHED_TO_HANDLER_FRAME,0x70e571bf95be7243, CommandIsAnyEntityAttachedToHandlerFrame);
		SCR_REGISTER_SECURE(FIND_HANDLER_VEHICLE_CONTAINER_IS_ATTACHED_TO,0xdc86a1752a8ada54, CommandFindHandlerVehicleContainerIsAttachedTo);
		SCR_REGISTER_SECURE(IS_HANDLER_FRAME_LINED_UP_WITH_CONTAINER,0x36d170c816c17270, CommandIsHandlerFrameLinedUpWithContainer);
		SCR_REGISTER_UNUSED(ATTACH_CONTAINER_TO_HANDLER_FRAME,0x664cb1654b43ee76, CommandAttachContainerToHandlerFrame);
		SCR_REGISTER_SECURE(ATTACH_CONTAINER_TO_HANDLER_FRAME_WHEN_LINED_UP,0x618335ab4b1ad654, CommandAttachContainerToHandlerFrameWhenLinedUp);
		SCR_REGISTER_SECURE(DETACH_CONTAINER_FROM_HANDLER_FRAME,0xc58cd2901ebceef1, CommandDetachContainerFromHandlerFrame);
		SCR_REGISTER_SECURE(SET_VEHICLE_DISABLE_HEIGHT_MAP_AVOIDANCE,0xdfbc290c98373d69, CommandSetVehicleDisableHeightmapAvoidance);
		SCR_REGISTER_SECURE(SET_BOAT_DISABLE_AVOIDANCE,0x21fe8b68c655751b, CommandSetBoatDisableAvoidance);
		SCR_REGISTER_SECURE(IS_HELI_LANDING_AREA_BLOCKED,0x03af66bae091dee2, CommandIsHeliLandingAreaBlocked);
		SCR_REGISTER_SECURE(SET_SHORT_SLOWDOWN_FOR_LANDING,0x2e1294a4fa6aeb0f, CommandSetShortSlowdownDistanceForLanding);
		SCR_REGISTER_SECURE(SET_HELI_TURBULENCE_SCALAR,0x0eb48250b8f38fcf, CommandSetHeliTurbulenceScalar);
		SCR_REGISTER_UNUSED(SET_VEHICLE_AS_CONVOY_VEHICLE,0x9f8d3bca1e36ecbf, CommandSetVehicleAsConvoyVehicle);
		SCR_REGISTER_UNUSED(SET_VEHICLE_CONTROL_TO_PLAYER,0xe0a0c63800fbe7c8, CommandSetVehicleControlToPlayer);
		SCR_REGISTER_SECURE(SET_CAR_BOOT_OPEN,0xa4fcb26551d216b8, CommandSetCarBootOpen);
		SCR_REGISTER_SECURE(SET_VEHICLE_TYRE_BURST,0x7517a117ea38d570, CommandSetVehicleTyreBurst);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOORS_SHUT,0x0e97fae15bee6379, CommandSetVehicleDoorsShut);
		SCR_REGISTER_UNUSED(GET_RANDOM_VEHICLE_OF_TYPE_IN_AREA,0x5b533b41117fefce, CommandGetRandomVehicleOfTypeInArea);
		SCR_REGISTER_SECURE(SET_VEHICLE_TYRES_CAN_BURST,0xafe1b52f0de91595, CommandSetVehicleTyresCanBurst);
		SCR_REGISTER_SECURE(GET_VEHICLE_TYRES_CAN_BURST,0xc8e55aeb1e2b047f, CommandGetVehicleTyresCanBurst);
		SCR_REGISTER_UNUSED(SET_VEHICLE_TYRES_CAN_BURST_TO_RIM,0x5e6ddf43008276cc, CommandSetVehicleTyresCanBurstToRim);
		SCR_REGISTER_UNUSED(GET_VEHICLE_TYRES_CAN_BURST_TO_RIM,0x1e6652013b0d5cf9, CommandGetVehicleTyresCanBurstToRim);
		SCR_REGISTER_SECURE(SET_VEHICLE_WHEELS_CAN_BREAK,0xf5a71ed585ae4eac, CommandSetVehicleWheelsCanBreak);
		SCR_REGISTER_UNUSED(GET_VEHICLE_WHEELS_CAN_BREAK,0xd376c327e507f32c, CommandGetVehicleWheelsCanBreak);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_OPEN,0x2476be9932670cad, CommandSetVehicleDoorOpen);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_AUTO_LOCK,0xb14ac5c9d3b8d524, CommandSetVehicleDoorAutoLock);
		SCR_REGISTER_SECURE(SET_FLEEING_VEHICLES_USE_SWITCHED_OFF_NODES,0xb0998f6290b6ce9c, CommandSetFleeingVehiclesUseSwitchedOffNodes);

		SCR_REGISTER_SECURE(REMOVE_VEHICLE_WINDOW,0xf6fba55b09506b23, CommandRemoveVehicleWindow);
		SCR_REGISTER_SECURE(ROLL_DOWN_WINDOWS,0xabaf363c566079cf, CommandRollDownWindows);
		SCR_REGISTER_SECURE(ROLL_DOWN_WINDOW,0xa664b3d039ffded1, CommandRollDownWindow);
		SCR_REGISTER_SECURE(ROLL_UP_WINDOW,0x16dbb2e2df4aeedd, CommandRollUpWindow);
		SCR_REGISTER_SECURE(SMASH_VEHICLE_WINDOW,0xbdba586f627a444a, CommandSmashVehicleWindow);
		SCR_REGISTER_SECURE(FIX_VEHICLE_WINDOW,0xd30ba8c994da4a59, CommandFixVehicleWindow);
		SCR_REGISTER_SECURE(POP_OUT_VEHICLE_WINDSCREEN,0xfc9192010268ed99, CommandPopOutVehicleWindscreen);
        SCR_REGISTER_UNUSED(POP_OFF_VEHICLE_ROOF,0xf0ced02b540a165b, CommandPopOffVehicleRoof);
        SCR_REGISTER_SECURE(POP_OFF_VEHICLE_ROOF_WITH_IMPULSE,0x26d7c8d88cb890cf, CommandPopOffVehicleRoofWithImpulse);
		SCR_REGISTER_UNUSED(SET_ACTUAL_VEHICLE_LIGHTS_OFF,0x79506f3cc4a3e135, CommandSetActualVehicleLightsOff );
		SCR_REGISTER_SECURE(SET_VEHICLE_LIGHTS,0xe5283b3a324d3fb0, CommandSetVehicleLights);
		SCR_REGISTER_SECURE(SET_VEHICLE_USE_PLAYER_LIGHT_SETTINGS,0x447ee3dfaa41270f, CommandSetVehicleUsePlayerLightSettings);
		SCR_REGISTER_SECURE(SET_VEHICLE_HEADLIGHT_SHADOWS,0xfc402b6af164b125, CommandSetVehicleHeadlightShadows);	
		SCR_REGISTER_SECURE(SET_VEHICLE_ALARM,0x909ff2867c7e0cae, CommandSetVehicleAlarm);
		SCR_REGISTER_SECURE(START_VEHICLE_ALARM,0x32c1b93fd1667ddb, CommandStartVehicleAlarm);
		SCR_REGISTER_SECURE(IS_VEHICLE_ALARM_ACTIVATED,0xaa2961a9653c5853, CommandIsVehicleAlarmActivated);
		SCR_REGISTER_SECURE(SET_VEHICLE_INTERIORLIGHT,0xebdc1fad110777e3, CommandSetVehicleInteriorLight);
		SCR_REGISTER_SECURE(SET_VEHICLE_FORCE_INTERIORLIGHT,0xa29fa0e1023d5c9d, CommandSetVehicleForceInteriorLight);
		SCR_REGISTER_SECURE(SET_VEHICLE_LIGHT_MULTIPLIER,0x26f7481668f849a8, CommandSetVehicleLightMultiplier);
		SCR_REGISTER_SECURE(ATTACH_VEHICLE_TO_TRAILER,0xfe1d9d8bff8d8ea6, CommandAttachVehicleToTrailer);
        SCR_REGISTER_SECURE(ATTACH_VEHICLE_ON_TO_TRAILER,0x0584f9efa7ee3834, CommandAttachVehicleOnToTrailer);
        SCR_REGISTER_SECURE(STABILISE_ENTITY_ATTACHED_TO_HELI,0xd0c0c836fb290d0d, CommandStabiliseEntityAttachedToHelicopter);
		SCR_REGISTER_UNUSED(SET_ENTITY_FOR_HELICOPTER_TO_DO_HEIGHT_CHECKS_FOR,0xf79c37c15e566532, CommandSetEntityForHelicopterToDoHeightChecksFor);
		SCR_REGISTER_UNUSED(CLEAR_ENTITY_FOR_HELICOPTER_TO_DO_HEIGHT_CHECKS_FOR,0x112d87c1ebdce6e1, CommandClearEntityForHelicopterToDoHeightChecksFor);
        SCR_REGISTER_SECURE(DETACH_VEHICLE_FROM_TRAILER,0x6c2ff30b05b4d198,	CommandDetachVehicleFromTrailer);
        SCR_REGISTER_UNUSED(DISABLE_TRAILER_BREAKING_FROM_VEHICLE,0xfad25982f0299b43,	CommandDisableTrailerBreakingFromVehicle);
        SCR_REGISTER_SECURE(IS_VEHICLE_ATTACHED_TO_TRAILER,0x817b926b53c0589a,	CommandIsVehicleAttachedToTrailer);
		SCR_REGISTER_SECURE(SET_TRAILER_INVERSE_MASS_SCALE,0x6f8aba9f3a136b93,	CommandSetTrailerInverseMassScale);
		SCR_REGISTER_SECURE(SET_TRAILER_LEGS_RAISED,0xeec059ac0da1daed, CommandSetTrailerLegsRaised);
		SCR_REGISTER_SECURE(SET_TRAILER_LEGS_LOWERED,0x4976f71e5a906e42, CommandSetTrailerLegsLowered);
		SCR_REGISTER_SECURE(SET_VEHICLE_TYRE_FIXED,0xf66e9c612e9bcd20, CommandSetVehicleTyreFixed);
		SCR_REGISTER_SECURE(SET_VEHICLE_NUMBER_PLATE_TEXT,0x2c2f2f6bee7ac7e7, CommandSetVehicleNumberPlateText);
		SCR_REGISTER_SECURE(GET_VEHICLE_NUMBER_PLATE_TEXT,0x80d16db58890b7bc, CommandGetVehicleNumberPlateText);
		SCR_REGISTER_UNUSED(GET_VEHICLE_NUMBER_PLATE_CHARACTER,0x77de26a07abbc76e, CommandGetVehicleNumberPlateCharacter);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_VEHICLE_NUMBER_PLATES,0xacf41d856b7ccb39, CommandGetNumberOfVehicleNumberPlates);
		SCR_REGISTER_SECURE(SET_VEHICLE_NUMBER_PLATE_TEXT_INDEX,0xe56b65c6b89cfcf2, CommandSetVehicleNumberPlateTextIndex);
		SCR_REGISTER_SECURE(GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX,0xc264f708491d88d7, CommandGetVehicleNumberPlateTextIndex);
		SCR_REGISTER_SECURE(SET_RANDOM_TRAINS,0x50cf518fb6f3bbde, CommandSetRandomTrains);
		SCR_REGISTER_SECURE(CREATE_MISSION_TRAIN,0x27a58935528fc12f, CommandCreateMissionTrain);
		SCR_REGISTER_UNUSED(CREATE_MISSION_TRAIN_BY_NAME,0xa8ab1e935fb305ac, CommandCreateMissionTrainByName);
		SCR_REGISTER_SECURE(SWITCH_TRAIN_TRACK,0x8aa588abcb86e05d, CommandSwitchTrainTrack);
		SCR_REGISTER_SECURE(SET_TRAIN_TRACK_SPAWN_FREQUENCY,0xf2b3b5bdf87008c0, CommandSetTrainTrackSpawnFrequency);
		SCR_REGISTER_UNUSED(SET_MISSION_TRAINS_AS_NO_LONGER_NEEDED, 0x22beb86e, CommandSetMissionTrainsAsNoLongerNeeded);
		SCR_REGISTER_SECURE(ALLOW_TRAIN_TO_BE_REMOVED_BY_POPULATION,0x0ff532f1f7d2bc7f, CommandAllowTrainToBeRemovedByPopulation);
		SCR_REGISTER_SECURE(DELETE_ALL_TRAINS,0x4e012c90db4d2b14, CommandDeleteAllTrains);
		SCR_REGISTER_SECURE(SET_TRAIN_SPEED,0x95f65274c3ab868e, CommandSetTrainSpeed);
		SCR_REGISTER_SECURE(SET_TRAIN_CRUISE_SPEED,0x2fb817738fffdc38, CommandSetTrainCruiseSpeed);
		SCR_REGISTER_UNUSED(GET_TRAIN_CABOOSE,0xee34e3ad7ce9079b, CommandGetTrainCaboose);
		SCR_REGISTER_UNUSED(SET_TRAIN_STOPS_FOR_STATIONS,0x2e79d025b3f29270, CommandSetTrainStopsForStations);
		SCR_REGISTER_UNUSED(SET_TRAIN_IS_STOPPED_AT_STATION,0x724c93286b44db54, CommandSetTrainIsStoppedAtStation);
		SCR_REGISTER_UNUSED(SET_TRAIN_LEAVES_STATION,0x543ed7a84f4d1470, CommandTrainLeaveStation);
		SCR_REGISTER_SECURE(SET_RANDOM_BOATS,0x5b336ff89a289cb3, CommandSetRandomBoats);
		SCR_REGISTER_SECURE(SET_RANDOM_BOATS_MP,0xf2c4c91339d8e00b, CommandSetRandomBoatsMP);
		SCR_REGISTER_SECURE(SET_GARBAGE_TRUCKS,0x63e14b0ea0bbf0ff, CommandSetGarbageTrucks);
		SCR_REGISTER_SECURE(DOES_VEHICLE_HAVE_STUCK_VEHICLE_CHECK,0x79a822779cc6508a, CommandDoesVehicleHaveStuckVehicleCheck);
		// vehicle recordings - recording
		SCR_REGISTER_UNUSED(START_RECORDING_VEHICLE,0xe6ffbfe41d721f6b, CommandStartRecordingVehicle);
		SCR_REGISTER_UNUSED(STOP_RECORDING_ALL_VEHICLES,0xf9ea4c6112f6b65f, CommandStopRecordingAllVehicles);
		SCR_REGISTER_UNUSED(STOP_RECORDING_VEHICLE,0x225617d3b93a2f4c, CommandStopRecordingVehicle);
		SCR_REGISTER_UNUSED(DOES_VEHICLE_RECORDING_FILE_EXIST,0xc3bead5b5b693b13, CommandDoesVehicleRecordingFileExist);
		SCR_REGISTER_UNUSED(DOES_VEHICLE_RECORDING_FILE_EXIST_IN_CDIMAGE,0x2f2d48e78494f912, CommandDoesVehicleRecordingFileExistInCdImage);
		// vehicle recordings - playback
		SCR_REGISTER_SECURE(GET_VEHICLE_RECORDING_ID,0x395075b91017d334, CommandGetVehicleRecordingId);
		SCR_REGISTER_SECURE(REQUEST_VEHICLE_RECORDING,0xb018baa4ed9aa217, CommandRequestVehicleRecording);
		SCR_REGISTER_SECURE(HAS_VEHICLE_RECORDING_BEEN_LOADED,0xf777caa43f4b281a, CommandHasVehicleRecordingBeenLoaded);
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_RECORDING,0xa72858c94e70296c, CommandRemoveVehicleRecording);
		SCR_REGISTER_SECURE(GET_POSITION_OF_VEHICLE_RECORDING_ID_AT_TIME,0xad6ef15ccc57aa73, CommandGetPositionOfVehicleRecordingIdAtTime);
		SCR_REGISTER_SECURE(GET_POSITION_OF_VEHICLE_RECORDING_AT_TIME,0xa53596fa06de5ecb, CommandGetPositionOfVehicleRecordingAtTime);
		SCR_REGISTER_SECURE(GET_ROTATION_OF_VEHICLE_RECORDING_ID_AT_TIME,0xa978194a1e680f26, CommandGetRotationOfVehicleRecordingIdAtTime);
		SCR_REGISTER_SECURE(GET_ROTATION_OF_VEHICLE_RECORDING_AT_TIME,0xabe8e3132c9d7251, CommandGetRotationOfVehicleRecordingAtTime);
		SCR_REGISTER_SECURE(GET_TOTAL_DURATION_OF_VEHICLE_RECORDING_ID,0x48612b013e0ccba2, CommandGetTotalDurationOfVehicleRecordingId);
		SCR_REGISTER_SECURE(GET_TOTAL_DURATION_OF_VEHICLE_RECORDING,0x733908dbfa08cead, CommandGetTotalDurationOfVehicleRecording);
		SCR_REGISTER_SECURE(GET_POSITION_IN_RECORDING,0x21f08de9d609bdc9, CommandGetPositionInRecording);
		SCR_REGISTER_SECURE(GET_TIME_POSITION_IN_RECORDING,0xfe9c4864a7c4cedb, CommandGetTimePositionInRecording);
		SCR_REGISTER_UNUSED(GET_TIME_POSITION_IN_RECORDED_RECORDING,0x3f69b0d80e2fb186, CommandGetTimePositionInRecordedRecording);
		SCR_REGISTER_UNUSED(START_RECORDING_VEHICLE_TRANSITION_FROM_PLAYBACK,0x29c629a71e2d754c, CommandStartRecordingVehicleTransitionFromPlayback);
		SCR_REGISTER_SECURE(START_PLAYBACK_RECORDED_VEHICLE,0x2f4db11a0db3685a, CommandStartPlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(START_PLAYBACK_RECORDED_VEHICLE_WITH_FLAGS,0x0d1d3bf940d4c760, CommandStartPlaybackRecordedVehicleWithFlags);
		SCR_REGISTER_SECURE(FORCE_PLAYBACK_RECORDED_VEHICLE_UPDATE,0x92ee3ed9242dcb76, CommandForcePlaybackRecordedVehicleUpdate);
		SCR_REGISTER_SECURE(STOP_PLAYBACK_RECORDED_VEHICLE,0x90f8f7d270431fae, CommandStopPlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(PAUSE_PLAYBACK_RECORDED_VEHICLE,0x9b24ca8f49e61f9e, CommandPausePlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(UNPAUSE_PLAYBACK_RECORDED_VEHICLE,0x4f918d011b31cb9b, CommandUnpausePlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(IS_PLAYBACK_GOING_ON_FOR_VEHICLE,0xe896049ad1887ff0, CommandIsPlaybackGoingOnForVehicle);
		SCR_REGISTER_SECURE(IS_PLAYBACK_USING_AI_GOING_ON_FOR_VEHICLE,0x8f33d7496432068d, CommandIsPlaybackUsingAIGoingOnForVehicle);
		SCR_REGISTER_SECURE(GET_CURRENT_PLAYBACK_FOR_VEHICLE,0xfd0eae8e8428f6a3, CommandGetCurrentPlaybackForVehicle);
		SCR_REGISTER_UNUSED(IS_RECORDING_GOING_ON_FOR_VEHICLE,0xf2efd7d71b8d5853, CommandIsRecordingGoingOnForVehicle);
		SCR_REGISTER_SECURE(SKIP_TO_END_AND_STOP_PLAYBACK_RECORDED_VEHICLE,0x5b8ecad4a9e4355d, CommandSkipToEndAndStopPlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(SET_PLAYBACK_SPEED,0xffe9ff137bafdfd8, CommandSetPlaybackSpeed);
		SCR_REGISTER_SECURE(START_PLAYBACK_RECORDED_VEHICLE_USING_AI,0x1b70646075a0f088, CommandStartPlaybackRecordedVehicleUsingAi);
		SCR_REGISTER_SECURE(SKIP_TIME_IN_PLAYBACK_RECORDED_VEHICLE,0x4030f85138a50df6, CommandSkipTimeInPlaybackRecordedVehicle);
		SCR_REGISTER_UNUSED(DISPLAY_PLAYBACK_RECORDED_VEHICLE,0x0c7be48cb23783ac, CommandDisplayPlaybackRecordedVehicle);
		SCR_REGISTER_SECURE(SET_PLAYBACK_TO_USE_AI,0xf4e68ebd83914967, CommandSetPlaybackToUseAi);
		SCR_REGISTER_SECURE(SET_PLAYBACK_TO_USE_AI_TRY_TO_REVERT_BACK_LATER,0x40d396d9ecf69add, CommandSetPlaybackToUseAiTryToRevertBackLater);
		SCR_REGISTER_SECURE(SET_ADDITIONAL_ROTATION_FOR_RECORDED_VEHICLE_PLAYBACK,0x99a7dd7fc16db91b, CommandSetAdditionalRotationForRecordedVehiclePlayback);
		SCR_REGISTER_SECURE(SET_POSITION_OFFSET_FOR_RECORDED_VEHICLE_PLAYBACK,0xe98f26077277cec7, CommandSetPositionOffsetForRecordedVehiclePlayback);
		SCR_REGISTER_SECURE(SET_GLOBAL_POSITION_OFFSET_FOR_RECORDED_VEHICLE_PLAYBACK,0xeee959888040c97b, CommandSetGlobalPositionOffsetForRecordedVehiclePlayback);		
		SCR_REGISTER_SECURE(SET_SHOULD_LERP_FROM_AI_TO_FULL_RECORDING,0x3bbbbdbfc1da3fa5, CommandSetShouldLerpFromAiToFullRecording);
#if !__FINAL
		SCR_REGISTER_UNUSED(GET_VEHICLE_RECORDING_NAME,0xf340551847ff5988, CommandGetVehicleRecordingName);
#endif
		SCR_REGISTER_SECURE(EXPLODE_VEHICLE_IN_CUTSCENE,0x6931de5ef0f6ed65, CommandExplodeVehicleInCutscene);
		SCR_REGISTER_SECURE(ADD_VEHICLE_STUCK_CHECK_WITH_WARP,0x4ccfba59b410315d, CommandAddVehicleStuckCheckWithWarp);
		SCR_REGISTER_SECURE(SET_VEHICLE_MODEL_IS_SUPPRESSED,0x7cd708deb04f8474, CommandSetVehicleModelIsSuppressed);
		SCR_REGISTER_SECURE(GET_RANDOM_VEHICLE_IN_SPHERE,0x8aa54390e994ce1b, CommandGetRandomVehicleInSphere);
		SCR_REGISTER_SECURE(GET_RANDOM_VEHICLE_FRONT_BUMPER_IN_SPHERE,0x106c0716304470c2, CommandGetRandomVehicleFrontBumperInSphere);
		SCR_REGISTER_SECURE(GET_RANDOM_VEHICLE_BACK_BUMPER_IN_SPHERE,0xc3048e3e178b269b, CommandGetRandomVehicleBackBumperInSphere);
		SCR_REGISTER_SECURE(GET_CLOSEST_VEHICLE,0xfe15feb341652241, CommandGetClosestVehicle);
		SCR_REGISTER_SECURE(GET_TRAIN_CARRIAGE,0xc1ee596291035d51, CommandGetTrainCarriage);
		SCR_REGISTER_SECURE(IS_MISSION_TRAIN,0x91e77d03171a5783, CommandIsMissionTrain);
		//SCR_REGISTER_UNUSED(SET_HELI_SPEED_CHEAT,0x1ebd3021a7fb35a2, CommandSetHeliSpeedCheat);
		SCR_REGISTER_SECURE(DELETE_MISSION_TRAIN,0xf49d9d19b56dc6d4, CommandDeleteMissionTrain);
		SCR_REGISTER_SECURE(SET_MISSION_TRAIN_AS_NO_LONGER_NEEDED,0x419230303abc3b0b, CommandSetMissionTrainAsNoLongerNeeded);
		SCR_REGISTER_SECURE(SET_MISSION_TRAIN_COORDS,0xcc2da9dc117955cc, CommandSetMissionTrainCoords);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_BOAT,0xd2b5b1c8fc84fe0f, CommandIsThisModelABoat);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_JETSKI,0x5c921200ca5cbf8e, CommandIsThisModelAJetski);		
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_PLANE,0xd6f7d32a98e07f93, CommandIsThisModelAPlane);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_HELI,0x4262b4dceadc2695, CommandIsThisModelAHeli);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_CAR,0xcb09a834f0c86fbb, CommandIsThisModelACar);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_TRAIN,0xf4ad75b2d964d8ab, CommandIsThisModelATrain);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_BIKE,0x6f0299ed3ceb4e5d, CommandIsThisModelABike);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_BICYCLE,0x532c99faf70c652b, CommandIsThisModelABicycle);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_A_QUADBIKE,0xcd8c08fa7d84c27e, CommandIsThisModelAQuadBike);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_AN_AMPHIBIOUS_CAR,0x49305e8836792d92, CommandIsThisModelAnAmphibiousCar);
		SCR_REGISTER_SECURE(IS_THIS_MODEL_AN_AMPHIBIOUS_QUADBIKE,0x2ac65a9c213b5bc8, CommandIsThisModelAnAmphibiousQuadBike);
		SCR_REGISTER_SECURE(SET_HELI_BLADES_FULL_SPEED,0xbaab64584d161c4d, CommandSetHeliBladesFullSpeed);
		SCR_REGISTER_SECURE(SET_HELI_BLADES_SPEED,0x4394f7f852a22b94, CommandSetHeliBladesSpeed);
		SCR_REGISTER_SECURE(FORCE_SUB_THROTTLE_FOR_TIME,0x86c7e0a8ed5445d8, CommandForceSubThrottleForTime);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_BE_TARGETTED,0xabbfd0819c89c140, CommandSetVehicleCanBeTargetted);
		SCR_REGISTER_SECURE(SET_DONT_ALLOW_PLAYER_TO_ENTER_VEHICLE_IF_LOCKED_FOR_PLAYER,0x28c1aa9789ab5554, CommandSetDontAllowPlayerToEnterVehicleIfLockedForPlayer);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_BE_VISIBLY_DAMAGED,0xe0af417cbbb366f3, CommandSetVehicleCanBeVisiblyDamaged);
		SCR_REGISTER_SECURE(SET_VEHICLE_HAS_UNBREAKABLE_LIGHTS,0x436bfe83f6ae0d30, CommandSetVehicleHasUnbreakableLights);
        SCR_REGISTER_SECURE(SET_VEHICLE_RESPECTS_LOCKS_WHEN_HAS_DRIVER,0x0cd9b67a70aad552, CommandSetVehicleRespectsLocksWhenHasDriver);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_EJECT_PASSENGERS_IF_LOCKED,0x4694f96d00a3b8eb, CommandSetCanEjectPassengersIfLocked);
		SCR_REGISTER_SECURE(GET_VEHICLE_DIRT_LEVEL,0xd3ff3a3c2d524a18, CommandGetVehicleDirtLevel);
		SCR_REGISTER_SECURE(SET_VEHICLE_DIRT_LEVEL,0x2ec6505629d78cce, CommandSetVehicleDirtLevel);
		SCR_REGISTER_SECURE(GET_DOES_VEHICLE_HAVE_DAMAGE_DECALS,0xd37b43eb5fb2e301, CommandDoesVehicleHaveDamageDecals);
		SCR_REGISTER_SECURE(IS_VEHICLE_DOOR_FULLY_OPEN,0x9e2309ac10d3abe6, CommandIsVehicleDoorFullyOpen);
		SCR_REGISTER_UNUSED(SET_FREEBIES_IN_VEHICLE,0xee1c2510e0379120, CommandSetFreebiesInVehicle);
		SCR_REGISTER_UNUSED(SET_ROCKET_LAUNCHER_FREEBIE_IN_HELI,0x2a02a7339eb52073, CommandSetRocketLauncherFreebieInHeli);
		SCR_REGISTER_SECURE(SET_VEHICLE_ENGINE_ON,0x371d594409a68a18, CommandSetVehicleEngineOn);
		SCR_REGISTER_SECURE(SET_VEHICLE_UNDRIVEABLE,0x7570fb331dfe5904, CommandSetVehicleUnDriveable);
		SCR_REGISTER_UNUSED(SET_FREE_RESPRAYS,0x4df623dceca9bfc6, CommandSetFreeResprays);
		SCR_REGISTER_SECURE(SET_VEHICLE_PROVIDES_COVER,0xaa207a327a73d480, CommandSetVehicleProvidesCover);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_CONTROL,0x59492c1b3db11bde, CommandSetVehicleDoorControl);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_LATCHED,0x3707f224fa0050ea, CommandSetVehicleDoorLatched);
		SCR_REGISTER_SECURE(GET_VEHICLE_DOOR_ANGLE_RATIO,0x7df569d9e616d55e, CommandGetVehicleDoorAngleRatio);
		SCR_REGISTER_UNUSED(GET_VEHICLE_DOOR_TARGET_RATIO, 0xf63231a9bb567dd2, CommandGetVehicleDoorTargetRatio);
		SCR_REGISTER_SECURE(GET_PED_USING_VEHICLE_DOOR,0xbc76e5204e0d5382, CommandGetPedUsingVehicleDoor);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_SHUT,0xcaa02c32ec0c2462, CommandSetVehicleDoorShut);
		SCR_REGISTER_SECURE(SET_VEHICLE_DOOR_BROKEN,0xf2a3bd88ac59d1b0, CommandSetVehicleDoorBroken);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_BREAK,0x5bb40fec6faa468d, CommandSetVehicleCanBreak);
		SCR_REGISTER_SECURE(DOES_VEHICLE_HAVE_ROOF,0xbcf71e2c4b3df832, CommandDoesVehicleHaveRoof);
		SCR_REGISTER_SECURE(SET_VEHICLE_REMOVE_AGGRESSIVE_CARJACK_MISSION,0x1a7b12c9ee8ed1d4, CommandSetRemoveAggressivelyForCarJackingMission);
		SCR_REGISTER_SECURE(SET_VEHICLE_AVOID_PLAYER_VEHICLE_RIOT_VAN_MISSION,0xe216632002dcc397, CommandSetSwerveAroundPlayerVehicleForRiotVanMission);	
		SCR_REGISTER_SECURE(SET_CARJACK_MISSION_REMOVAL_PARAMETERS,0x1379dc7e5c1a210c, CommandSetRemovalRadiusAndNumberForCarjackingMission);
		SCR_REGISTER_SECURE(IS_BIG_VEHICLE,0xa94e098b1788a730, CommandIsBigVehicle);
		SCR_REGISTER_UNUSED(GET_NUMBER_OF_VEHICLE_MODEL_COLOURS,0x109bef77799d08dc, CommandGetNumberOfVehicleModelColours);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_VEHICLE_COLOURS,0x76910b06be582258, CommandGetNumberOfVehicleColours);
		SCR_REGISTER_SECURE(SET_VEHICLE_COLOUR_COMBINATION,0xd584204572df0440, CommandSetVehicleColourCombination);
		SCR_REGISTER_SECURE(GET_VEHICLE_COLOUR_COMBINATION,0xe0095ca086337962, CommandGetVehicleColourCombination);
		SCR_REGISTER_SECURE(SET_VEHICLE_XENON_LIGHT_COLOR_INDEX,0x300bfaa8e2a36a8e, CommandSetVehicleXenonLightColorIdx);
		SCR_REGISTER_SECURE(GET_VEHICLE_XENON_LIGHT_COLOR_INDEX,0xdb29312237512d99, CommandGetVehicleXenonLightColorIdx);
		SCR_REGISTER_SECURE(SET_VEHICLE_IS_CONSIDERED_BY_PLAYER,0xf838539c61c30896, CommandSetVehicleIsConsideredByPlayer);
		SCR_REGISTER_SECURE(SET_VEHICLE_WILL_FORCE_OTHER_VEHICLES_TO_STOP,0x7928629159f456d1, CommandSetVehicleWillForceOtherVehiclesToStop);
		SCR_REGISTER_SECURE(SET_VEHICLE_ACT_AS_IF_HAS_SIREN_ON,0xa0525d286052cdd0, CommandSetVehicleActAsIfHasSirenOn);
		SCR_REGISTER_SECURE(SET_VEHICLE_USE_MORE_RESTRICTIVE_SPAWN_CHECKS,0x22aae0eeac98904b, CommandSetVehicleUseMoreRestrictiveSpawnChecks);
		SCR_REGISTER_SECURE(SET_VEHICLE_MAY_BE_USED_BY_GOTO_POINT_ANY_MEANS,0x60e9b6f76bd38b78, CommandSetVehicleMayBeUsedByGoToPointAnyMeans);
		SCR_REGISTER_SECURE(GET_RANDOM_VEHICLE_MODEL_IN_MEMORY,0x20c186dc005af163, CommandGetRandomVehicleModelInMemory);
		SCR_REGISTER_UNUSED(GET_CURRENT_BASIC_POLICE_VEHICLE_MODEL,0x750529756f666a1b, CommandGetCurrentBasicPoliceVehicleModel);
		SCR_REGISTER_UNUSED(GET_CURRENT_POLICE_VEHICLE_MODEL,0x78463883a202d38c, CommandGetCurrentPoliceVehicleModel);
		SCR_REGISTER_UNUSED(GET_CURRENT_TAXI_VEHICLE_MODEL,0xa7097cc1850fb867, CommandGetCurrentTaxiVehicleModel);
		SCR_REGISTER_SECURE(GET_VEHICLE_DOOR_LOCK_STATUS,0xa09765d2ade13cae, CommandGetVehicleDoorLockStatus);
		SCR_REGISTER_SECURE(GET_VEHICLE_INDIVIDUAL_DOOR_LOCK_STATUS,0x2aa76deb8bb593a3, CommandGetVehicleIndividualDoorLockStatus);
		SCR_REGISTER_SECURE(IS_VEHICLE_DOOR_DAMAGED,0x2abe742a1b0bf1a6, CommandIsVehicleDoorDamaged);
        SCR_REGISTER_SECURE(SET_DOOR_ALLOWED_TO_BE_BROKEN_OFF,0x6d60abba76db955a, CommandSetDoorAllowedToBeBrokenOff);
        SCR_REGISTER_SECURE(IS_VEHICLE_BUMPER_BOUNCING,0xb27089b11e5dbcad, CommandIsVehicleBumperBouncing);
        SCR_REGISTER_SECURE(IS_VEHICLE_BUMPER_BROKEN_OFF,0xc9d76a6a7441f5a4, CommandIsVehicleBumperBrokenOff);
		SCR_REGISTER_SECURE(IS_COP_VEHICLE_IN_AREA_3D,0x0c9b32757ae0b2bf, CommandIsCopVehicleInArea3d);
		SCR_REGISTER_UNUSED(SET_TRAIN_FORCED_TO_SLOW_DOWN,0x636cf3c77c65eb2b, CommandSetTrainForcedToSlowDown);
		SCR_REGISTER_SECURE(IS_VEHICLE_ON_ALL_WHEELS,0x78006d212fd7243e, CommandIsVehicleOnAllWheels);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_VALUE,0xe0bdb19a85d41c89, CommandGetVehicleModelValue);
		SCR_REGISTER_SECURE(GET_VEHICLE_LAYOUT_HASH,0x816a3adb189bd123, CommandGetVehicleLayoutHash);
		SCR_REGISTER_SECURE(GET_IN_VEHICLE_CLIPSET_HASH_FOR_SEAT,0x5f7d161e0e6a21b6, CommandGetInVehicleClipSetHashForSeat);
		SCR_REGISTER_UNUSED(GET_TRAIN_DIRECTION,0x2b97e440e8bbe9e0, CommandGetTrainDirection);
		SCR_REGISTER_UNUSED(SKIP_TO_NEXT_ALLOWED_STATION,0xfef29191da0385f0, CommandSkipToNextAllowedStation);
		SCR_REGISTER_UNUSED(GET_NEXT_STATION_FOR_TRAIN,0x5aaf6c2280e43a27, CommandGetNextStationForTrain);
		SCR_REGISTER_UNUSED(GET_CURRENT_STATION_FOR_TRAIN,0x6642ecb9ee1c3c5e, CommandGetCurrentStationForTrain);
		SCR_REGISTER_UNUSED(GET_TIME_TIL_NEXT_STATION,0x07e509a579d5264c, CommandGetTimeTilNextStation);
		SCR_REGISTER_SECURE(SET_RENDER_TRAIN_AS_DERAILED,0x67196474beffda91, CommandSetRenderTrainAsDerailed);
		SCR_REGISTER_UNUSED(GET_STATION_NAME,0x211b95dc549c845b, CommandGetStationName);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXTRA_COLOURS,0x952b5201cc721090, CommandSetVehicleExtraColours);
		SCR_REGISTER_SECURE(GET_VEHICLE_EXTRA_COLOURS,0x356c37f1aa6ea3aa, CommandGetVehicleExtraColours);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXTRA_COLOUR_5,0x63140c89ad7622ef, CommandSetVehicleExtraColour5);
		SCR_REGISTER_SECURE(GET_VEHICLE_EXTRA_COLOUR_5,0x1e4d0d5de3802bc2, CommandGetVehicleExtraColour5);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXTRA_COLOUR_6,0xce01344f3cabba9d, CommandSetVehicleExtraColour6);
		SCR_REGISTER_SECURE(GET_VEHICLE_EXTRA_COLOUR_6,0x04ce0bf11e7d9de4, CommandGetVehicleExtraColour6);
		SCR_REGISTER_UNUSED(SET_RESPRAYS,0x852392e6504088d8, CommandSetRespraysActive);
		SCR_REGISTER_SECURE(STOP_ALL_GARAGE_ACTIVITY,0x973a0b294c1dbd4f, CommandStopAllGarageActivity);
		SCR_REGISTER_UNUSED(HAS_VEHICLE_BEEN_RESPRAYED,0x408b36fa2f9168db, CommandHasVehicleBeenResprayed);
		SCR_REGISTER_UNUSED(IS_PAY_N_SPRAY_ACTIVE,0x342c2aab6df25120, CommandIsPayNSprayActive);
		SCR_REGISTER_UNUSED(HAS_RESPRAY_HAPPENED,0xcc7d76244c734830, CommandHasResprayHappened);
		SCR_REGISTER_SECURE(SET_VEHICLE_FIXED,0x02443408423266f9, CommandSetVehicleFixed);
		SCR_REGISTER_SECURE(SET_VEHICLE_DEFORMATION_FIXED,0x5dacc304f20af7e6, CommandSetVehicleDeformationFixed);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_ENGINE_MISSFIRE,0x4f71024e0c6500dd, CommandSetVehicleCanEngineMissFire);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_LEAK_OIL,0x076b41121bfb1c77, CommandSetVehicleCanLeakOil);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_LEAK_PETROL,0xf80f852786cb8bb0, CommandSetVehicleCanLeakPetrol);
		SCR_REGISTER_SECURE(SET_DISABLE_VEHICLE_PETROL_TANK_FIRES,0xdc41f053565f1481, CommandDisablePetrolTankFires);
		SCR_REGISTER_SECURE(SET_DISABLE_VEHICLE_PETROL_TANK_DAMAGE,0xcdd48501873ff1bc, CommandDisablePetrolTankDamage);
		SCR_REGISTER_SECURE(SET_DISABLE_VEHICLE_ENGINE_FIRES,0x86a3d2835b8f8645, CommandDisableEngineFires);
		SCR_REGISTER_SECURE(SET_VEHICLE_LIMIT_SPEED_WHEN_PLAYER_INACTIVE,0x9874270e3081a06a, CommandSetVehicleLimitSpeedWhenPlayerInactive);
		SCR_REGISTER_SECURE(SET_VEHICLE_STOP_INSTANTLY_WHEN_PLAYER_INACTIVE,0xeae08e51369cea78, CommandSetVehicleStopInstantlyWhenPlayerInactive);
		SCR_REGISTER_SECURE(SET_DISABLE_PRETEND_OCCUPANTS,0x11647d34b93a72eb, CommandDisablePretendOccupants);
		SCR_REGISTER_UNUSED(SET_MAD_DRIVERS,0xfb24b9dc688170ee, CommandSetMadDrivers);
		SCR_REGISTER_SECURE(REMOVE_VEHICLES_FROM_GENERATORS_IN_AREA,0x06b8fba220c6488c, CommandRemoveVehiclesFromGeneratorsInArea);
		SCR_REGISTER_SECURE(SET_VEHICLE_STEER_BIAS,0x77451b49db5a200a, CommandSetVehicleSteerBias);
		SCR_REGISTER_UNUSED(SET_VEHICLE_PITCH_BIAS,0xc66b05488813fdb0, CommandSetVehiclePitchBias);
		SCR_REGISTER_UNUSED(SET_VEHICLE_ROLL_BIAS,0x0849fb557e3ce3bb, CommandSetVehicleRollBias);
		SCR_REGISTER_UNUSED(HAS_VEHICLE_STOPPED_BECAUSE_OF_LIGHT,0x2601fedae60b7d23, CommandHasVehicleStoppedBecauseOfLight);
		SCR_REGISTER_SECURE(IS_VEHICLE_EXTRA_TURNED_ON,0xc49329a4e58d025b, CommandIsVehicleExtraTurnedOn);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXTRA,0x45a6ea15139a0c54, CommandTurnOffVehicleExtra);
		SCR_REGISTER_SECURE(DOES_EXTRA_EXIST,0x2c52b3cc96343f32, CommandDoesExtraExist);
		SCR_REGISTER_SECURE(IS_EXTRA_BROKEN_OFF,0x390a3d0aa379ba97, CommandIsExtraBrokenOff);
		SCR_REGISTER_SECURE(SET_CONVERTIBLE_ROOF,0xc5b25a5d478730a6, CommandSetConvertibleRoof);
        SCR_REGISTER_SECURE(LOWER_CONVERTIBLE_ROOF,0x47b16f33dd2893cf, CommandLowerConvertibleRoof);
        SCR_REGISTER_SECURE(RAISE_CONVERTIBLE_ROOF,0xa36a3ab3d2350ccd, CommandRaiseConvertibleRoof);
        SCR_REGISTER_SECURE(GET_CONVERTIBLE_ROOF_STATE,0x0c2181485e5fb7c2, CommandGetConvertibleRoofState);
        SCR_REGISTER_SECURE(IS_VEHICLE_A_CONVERTIBLE,0x351e83123efc0a15, CommandIsVehicleAConvertible);
		SCR_REGISTER_SECURE(TRANSFORM_TO_SUBMARINE,0x457a7eb3bab235b6, CommandTransformToSubmarine);
		SCR_REGISTER_SECURE(TRANSFORM_TO_CAR,0xe1202d8d620aa25e, CommandTransformToCar);
		SCR_REGISTER_SECURE(IS_VEHICLE_IN_SUBMARINE_MODE,0xd01f664cef70d0b0, CommandGetIsInSubmarineMode);
		SCR_REGISTER_UNUSED(SET_GANG_VEHICLE, 0x924c8dbe, CommandSetGangVehicle);
		SCR_REGISTER_SECURE(IS_VEHICLE_STOPPED_AT_TRAFFIC_LIGHTS,0x7f25db35334461e0, CommandIsVehicleStoppedAtTrafficLights);
		SCR_REGISTER_SECURE(SET_VEHICLE_DAMAGE,0xbdb27a4f81702f96,CommandSetVehicleDamage);
		SCR_REGISTER_SECURE(SET_VEHICLE_OCCUPANTS_TAKE_EXPLOSIVE_DAMAGE,0x079a535265838a8a,CommandSetVehicleOccupantsTakeExplosiveDamage);
		SCR_REGISTER_SECURE(GET_VEHICLE_ENGINE_HEALTH,0x20e1198e4672f72d, CommandGetVehicleEngineHealth);
		SCR_REGISTER_SECURE(SET_VEHICLE_ENGINE_HEALTH,0x2ee80cd0eaeb9b5b, CommandSetVehicleEngineHealth);
		SCR_REGISTER_SECURE(SET_PLANE_ENGINE_HEALTH,0x9740b00ff15d67e4, CommandSetPlaneEngineHealth);
		SCR_REGISTER_SECURE(GET_VEHICLE_PETROL_TANK_HEALTH,0xda70f2c4f36b81cb, CommandGetVehiclePetrolTankHealth);
		SCR_REGISTER_SECURE(SET_VEHICLE_PETROL_TANK_HEALTH,0x448ad51b3157c957, CommandSetVehiclePetrolTankHealth);
		SCR_REGISTER_SECURE(IS_VEHICLE_STUCK_TIMER_UP,0xf798dcf92c74b531, CommandIsVehicleStuckTimerUp);
		SCR_REGISTER_SECURE(RESET_VEHICLE_STUCK_TIMER,0xd8948f63b2ebc20e, CommandResetVehicleStuckTimer);
		SCR_REGISTER_SECURE(IS_VEHICLE_DRIVEABLE,0x55a0c756c4a8220c, CommandIsVehDriveable);
		SCR_REGISTER_SECURE(SET_VEHICLE_HAS_BEEN_OWNED_BY_PLAYER,0x84934728f6567814, CommandSetVehicleHasBeenOwnedByPlayer);
		SCR_REGISTER_SECURE(SET_VEHICLE_NEEDS_TO_BE_HOTWIRED,0x0bae77e76ebd33d2, CommandSetVehicleNeedsToBeHotwired);
		SCR_REGISTER_SECURE(SET_VEHICLE_BLIP_THROTTLE_RANDOMLY,0x9830d9c399b40156, CommandSetVehicleBlipThrottleRandomly);
		SCR_REGISTER_UNUSED(GET_IS_VEHICLE_USING_PRETEND_OCCUPANTS,0xb37d86b310a38b43, CommandGetIsVehicleUsingPretendOccupants);
		SCR_REGISTER_UNUSED(GET_IS_VEHICLE_STOPPED_FOR_DOOR,0x4fe30f9f6b4fa7e0, CommandGetIsVehicleStoppedForDoor);
		SCR_REGISTER_SECURE(SET_POLICE_FOCUS_WILL_TRACK_VEHICLE,0x01640baf2d975118, CommandSetPoliceFocusWillTrackVehicle);
		SCR_REGISTER_SECURE(START_VEHICLE_HORN,0x8024c4a99a470a8e, CommandSoundCarHorn);
		SCR_REGISTER_SECURE(SET_VEHICLE_IN_CAR_MOD_SHOP,0xe78d2b33c23f2020, CommandVehicleInCarModShop);
		SCR_REGISTER_SECURE(SET_VEHICLE_HAS_STRONG_AXLES,0x05eab38f5ac8da1d, CommandSetVehicleHasStrongAxles);
		SCR_REGISTER_SECURE(GET_DISPLAY_NAME_FROM_VEHICLE_MODEL,0x01e2429b82e280e6, CommandGetDisplayNameFromVehicleModel);
		SCR_REGISTER_SECURE(GET_MAKE_NAME_FROM_VEHICLE_MODEL, 0xf7af4f159ff99f97, CommandGetMakeNameFromVehicleModel);
		SCR_REGISTER_SECURE(GET_VEHICLE_DEFORMATION_AT_POS,0x279f4d7315daeac0, CommandGetVehicleDeformationAtPos);
		SCR_REGISTER_SECURE(SET_VEHICLE_LIVERY,0xe1f8dbcf9a6e205e,CommandSetVehicleLivery);
		SCR_REGISTER_SECURE(GET_VEHICLE_LIVERY,0x3a5087062a6dbaef,CommandGetVehicleLivery);
		SCR_REGISTER_SECURE(GET_VEHICLE_LIVERY_COUNT,0x8f0bc830ffcf7f2a,CommandGetVehicleLiveryCount);
		SCR_REGISTER_SECURE(SET_VEHICLE_LIVERY2,0x9e56f3b30f9945a4,CommandSetVehicleLivery2);
		SCR_REGISTER_SECURE(GET_VEHICLE_LIVERY2,0xca76a695ac878584,CommandGetVehicleLivery2);
		SCR_REGISTER_SECURE(GET_VEHICLE_LIVERY2_COUNT,0x5e17421c6ddf16af,CommandGetVehicleLivery2Count);
		SCR_REGISTER_SECURE(IS_VEHICLE_WINDOW_INTACT,0x49e701a18d345b0e,CommandIsVehicleWindowIntact);
		SCR_REGISTER_SECURE(ARE_ALL_VEHICLE_WINDOWS_INTACT,0x9ca0cb41f81c959c,CommandAreAllVehicleWindowsIntact);
		SCR_REGISTER_SECURE(ARE_ANY_VEHICLE_SEATS_FREE,0x10a4f356f2dcea52,CommandAreAnyVehicleSeatsFree);

		SCR_REGISTER_SECURE(RESET_VEHICLE_WHEELS,0x672374e428bf31db, CommandResetVehicleWheels);
		SCR_REGISTER_UNUSED(SET_ALL_VEHICLES_HIGH_LOD,0x17de2ac78e60565f, CommandAllVehiclesHighLOD);
		SCR_REGISTER_SECURE(IS_HELI_PART_BROKEN,0x266d8e87c6f0c244, CommandIsHeliPartBroken);
		SCR_REGISTER_SECURE(GET_HELI_MAIN_ROTOR_HEALTH,0xc3c4ab879e734270, CommandGetHeliMainRotorHealth);
		SCR_REGISTER_SECURE(GET_HELI_TAIL_ROTOR_HEALTH,0x714854dc0f74197f, CommandGetHeliTailRotorHealth);
		SCR_REGISTER_SECURE(GET_HELI_TAIL_BOOM_HEALTH,0xc564f95537ff5300, CommandGetHeliTailBoomHealth);
		SCR_REGISTER_SECURE(SET_HELI_MAIN_ROTOR_HEALTH,0xe95f6c594f509d09, CommandSetHeliMainRotorHealth);
		SCR_REGISTER_SECURE(SET_HELI_TAIL_ROTOR_HEALTH,0xa20a708fc9d21460, CommandSetHeliTailRotorHealth);
		SCR_REGISTER_SECURE(SET_HELI_TAIL_BOOM_CAN_BREAK_OFF,0x392ada18c88106d8, CommandSetHeliTailBoomCanBreakOff);
		SCR_REGISTER_UNUSED(GET_NEAREST_CABLE_CAR,0x5ca45074df8d5cc5, CommandGetNearestCableCar);
		SCR_REGISTER_SECURE(SET_VEHICLE_NAME_DEBUG,0x892981d1dd897c93, CommandSetVehicleNameDebug);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXPLODES_ON_HIGH_EXPLOSION_DAMAGE,0x1af469c927341a18, CommandSetVehicleExplodesOnHighExplosionDamage);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXPLODES_ON_EXPLOSION_DAMAGE_AT_ZERO_BODY_HEALTH,0xc6672a1a9fbe00a1, CommandSetVehicleExplodesOnExplosionDamageAtZeroBodyHealth);
		SCR_REGISTER_SECURE(SET_ALLOW_VEHICLE_EXPLODES_ON_CONTACT,0x18fef16687c831e3, CommandSetAllowVehicleExplodesOnContact);
		SCR_REGISTER_SECURE(SET_VEHICLE_DISABLE_TOWING,0x1b3814e11b085f5e, CommandSetVehicleDisableTowing);
		SCR_REGISTER_SECURE(GET_VEHICLE_HAS_LANDING_GEAR,0x03377f11051bc47f, CommandGetVehicleHasLandingGear);
		SCR_REGISTER_SECURE(CONTROL_LANDING_GEAR,0x3739deed4a756e69, CommandControlLandingGear);
		SCR_REGISTER_SECURE(GET_LANDING_GEAR_STATE,0xc56def5c90606858, CommandGetLandingGearState);
		SCR_REGISTER_SECURE(IS_ANY_VEHICLE_NEAR_POINT,0x34faa537d539192c, CommandIsAnyVehicleNearPoint);
		SCR_REGISTER_SECURE(REQUEST_VEHICLE_HIGH_DETAIL_MODEL,0x6b137db2c9c0c754,CommandRequestVehicleHighDetailModel); 
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_HIGH_DETAIL_MODEL,0x6ba431f131a11a0e,CommandRemoveVehicleHighDetailModel); 
		SCR_REGISTER_SECURE(IS_VEHICLE_HIGH_DETAIL,0x89c8af90aef28755,CommandIsVehicleHighDetail); 
		SCR_REGISTER_SECURE(REQUEST_VEHICLE_ASSET,0x6b9a9289f29f4166,CommandRequestVehicleAsset); 
		SCR_REGISTER_SECURE(HAS_VEHICLE_ASSET_LOADED,0x08a9bdfe90d7cfe2,CommandHasVehicleAssetLoaded); 
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_ASSET,0x72dbc5d8e674e2ed,CommandRemoveVehicleAsset); 
		SCR_REGISTER_UNUSED(GET_VEHICLE_PRETEND_OCCUPANTS_CONVERSION_RANGE,0xcf8ae4549d9580a9, CommandGetVehiclePretendOccupantsConversionRange); 
		SCR_REGISTER_UNUSED(SET_VEHICLE_DIGGER_ARM_POSITION,0xe69be11860e4723e, CommandSetVehicleDiggerArmPosition);
        SCR_REGISTER_SECURE(SET_VEHICLE_TOW_TRUCK_ARM_POSITION,0x71aaa22c257241e2, CommandSetVehicleTowTruckArmPosition);
        SCR_REGISTER_SECURE(ATTACH_VEHICLE_TO_TOW_TRUCK,0xad0c529621854370, CommandAttachVehicleToTowTruck);
        SCR_REGISTER_SECURE(DETACH_VEHICLE_FROM_TOW_TRUCK,0x25780288ab87ba6f, CommandDetachVehicleFromTowTruck);
        SCR_REGISTER_SECURE(DETACH_VEHICLE_FROM_ANY_TOW_TRUCK,0x3c5e540babd9e8aa, CommandDetachVehicleFromAnyTowTruck);
        SCR_REGISTER_SECURE(IS_VEHICLE_ATTACHED_TO_TOW_TRUCK,0x671cb92f637fdb33, CommandIsVehicleAttachedToTowTruck);
        SCR_REGISTER_SECURE(GET_ENTITY_ATTACHED_TO_TOW_TRUCK,0x5dff6b0d4f63c800, CommandGetEntityAttachedToTowTruck);
        SCR_REGISTER_SECURE(SET_VEHICLE_AUTOMATICALLY_ATTACHES,0x2eabff5efcb7e6cf, CommandSetVehicleAutomaticallyAttaches);
        SCR_REGISTER_SECURE(SET_VEHICLE_BULLDOZER_ARM_POSITION,0xf2d70f64da5b8c27, CommandSetVehicleBulldozerArmPosition);
		SCR_REGISTER_SECURE(SET_VEHICLE_TANK_TURRET_POSITION,0xb551d01ea2292b47, CommandSetVehicleTankTurretPosition);
		SCR_REGISTER_SECURE(SET_VEHICLE_TURRET_TARGET,0x8f43872427e3508a, CommandSetVehicleTurretTarget);
		SCR_REGISTER_SECURE(SET_VEHICLE_TANK_STATIONARY,0x537574ee39db9104, CommandSetVehicleTankStationary);
		SCR_REGISTER_SECURE(SET_VEHICLE_TURRET_SPEED_THIS_FRAME,0xff85d4893258e0be, CommandSetVehicleTurretSpeedThisFrame);
		SCR_REGISTER_SECURE(DISABLE_VEHICLE_TURRET_MOVEMENT_THIS_FRAME,0x6fdbcebfe9de2bd2, CommandDisableTurretMovementThisFrame);
        SCR_REGISTER_SECURE(SET_VEHICLE_FLIGHT_NOZZLE_POSITION,0x606f297da0a55aff, CommandSetVerticalFlightNozzlePosition);
		SCR_REGISTER_SECURE(SET_VEHICLE_FLIGHT_NOZZLE_POSITION_IMMEDIATE,0x01c0a017e00bcf82, CommandSetVerticalFlightNozzlePositionImmediate);
		SCR_REGISTER_SECURE(GET_VEHICLE_FLIGHT_NOZZLE_POSITION,0x5c729d34ad6cb405, CommandGetVerticalFlightNozzlePosition);
		SCR_REGISTER_SECURE(SET_DISABLE_VERTICAL_FLIGHT_MODE_TRANSITION,0xc66e93f49a6a7aa3, CommandSetDisableVerticalFlightModeTransition);
		SCR_REGISTER_SECURE(GENERATE_VEHICLE_CREATION_POS_FROM_PATHS,0xae32ae15890aa504, CommandGenerateVehicleCreationPosFromPaths);
        SCR_REGISTER_SECURE(SET_VEHICLE_BURNOUT,0xdaca4dfbfd1bec8a, CommandSetVehicleBurnout);
		SCR_REGISTER_SECURE(IS_VEHICLE_IN_BURNOUT,0x6aca3842dd7f2536, CommandIsVehicleInBurnout);
        SCR_REGISTER_SECURE(SET_VEHICLE_REDUCE_GRIP,0x03e7ca3dc742fa72, CommandSetVehicleReduceGrip);
		SCR_REGISTER_SECURE(SET_VEHICLE_REDUCE_GRIP_LEVEL,0xbc1cdf8c3ef5fa95, CommandSetVehicleReduceGripLevel);
		SCR_REGISTER_SECURE(SET_VEHICLE_INDICATOR_LIGHTS,0xffcd7c1bd59ed815, CommandSetVehicleIndicatorLights);
		SCR_REGISTER_SECURE(SET_VEHICLE_BRAKE_LIGHTS,0xa3e3fc2853e2107b, CommandSetVehicleBrakeLights);
#if RSG_GEN9		
		SCR_REGISTER_UNUSED(SET_VEHICLE_TAIL_LIGHTS,0x130d281128da6a9e, CommandSetVehicleTailLights);
#endif		
		SCR_REGISTER_SECURE(SET_VEHICLE_HANDBRAKE,0x399cfa9720ccdf92, CommandSetVehicleHandbrake);
		SCR_REGISTER_SECURE(SET_VEHICLE_BRAKE,0xfea9d72485628c49, CommandSetVehicleBrake);
		SCR_REGISTER_SECURE(INSTANTLY_FILL_VEHICLE_POPULATION,0xc545abad8d25a872, CommandInstantlyFillVehiclePopulation);
		SCR_REGISTER_SECURE(HAS_INSTANT_FILL_VEHICLE_POPULATION_FINISHED,0xc13e0cba57f5b9d6, CommandHasInstantFillVehiclePopulationFinished);
		SCR_REGISTER_SECURE(NETWORK_ENABLE_EMPTY_CROWDING_VEHICLES_REMOVAL,0x4a65f9005ec6e45f, CommandNetworkAllowEmptyCrowdingVehiclesRemoval);
		SCR_REGISTER_SECURE(NETWORK_CAP_EMPTY_CROWDING_VEHICLES_REMOVAL,0xe6f7627bbb394bf9, CommandNetworkCapEmptyCrowdingVehiclesRemoval);
		SCR_REGISTER_SECURE(GET_VEHICLE_TRAILER_VEHICLE,0xcd52e82290c8c97a, CommandGetVehicleTrailerVehicle);
		SCR_REGISTER_SECURE(SET_VEHICLE_USES_LARGE_REAR_RAMP,0xea6971de99dd06ce, CommandSetVehicleUsesLargeRearRamp);
		SCR_REGISTER_UNUSED(SET_VEHICLE_WING_BROKEN,0x90da3d6f0b3b993a, CommandSetVehicleWingBroken);
		SCR_REGISTER_SECURE(SET_VEHICLE_RUDDER_BROKEN,0x4241f426fda9a558, CommandSetVehicleRudderBroken);
		SCR_REGISTER_UNUSED(SET_VEHICLE_TAIL_BROKEN,0xfe7e1f62c34516bd, CommandSetVehicleTailBroken);
        SCR_REGISTER_SECURE(SET_CONVERTIBLE_ROOF_LATCH_STATE,0xb7ca8f6b9d629efc, CommandSetConvertibleRoofLatchState);
        SCR_REGISTER_SECURE(GET_VEHICLE_ESTIMATED_MAX_SPEED,0x0bea3e6c636cae2b, CommandGetVehicleEstimatedMaxSpeed);
		SCR_REGISTER_SECURE(GET_VEHICLE_MAX_BRAKING,0x0d50b7306353e678, CommandGetVehicleMaxBraking);
		SCR_REGISTER_SECURE(GET_VEHICLE_MAX_TRACTION,0x1bb1047bf054186c, CommandGetVehicleMaxTraction);
		SCR_REGISTER_SECURE(GET_VEHICLE_ACCELERATION,0x243d924feea17838, CommandGetVehicleAcceleration);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_ESTIMATED_MAX_SPEED,0x21adaa2a197d4d0f, CommandGetVehicleModelEstimatedMaxSpeed);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_MAX_BRAKING,0x218c5b0472f0b2b3, CommandGetVehicleModelMaxBraking);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_MAX_BRAKING_MAX_MODS,0xbeeb8f910a7bfade, CommandGetVehicleModelMaxBrakingMaxMods);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_MAX_TRACTION,0x049ca97a39eaa3b8, CommandGetVehicleModelMaxTraction);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_ACCELERATION,0xdca867e594b1c1f4, CommandGetVehicleModelAcceleration);
		SCR_REGISTER_SECURE(GET_VEHICLE_MODEL_ACCELERATION_MAX_MODS,0x2d5bf7969a651e1f, CommandGetVehicleModelAccelerationMaxMods);
		SCR_REGISTER_SECURE(GET_FLYING_VEHICLE_MODEL_AGILITY,0x58153c894737bf15, CommandGetFlyingVehicleModelAgility);
		SCR_REGISTER_SECURE(GET_BOAT_VEHICLE_MODEL_AGILITY,0x353a3619cd479b9b, CommandGetBoatVehicleModelAgility);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_ESTIMATED_MAX_SPEED,0x629583b4ee13ccd4, CommandGetVehicleClassEstimatedMaxSpeed);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_MAX_TRACTION,0x6d4756fd96e33a30, CommandGetVehicleClassMaxTraction);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_MAX_AGILITY,0x15a6b87b67f505a3, CommandGetVehicleClassMaxAgility);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_MAX_ACCELERATION,0x0ddceafa6835ab9a, CommandGetVehicleClassMaxAcceleration);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_MAX_BRAKING,0xe61ae55777273d7c, CommandGetVehicleClassMaxBraking);
		SCR_REGISTER_SECURE(ADD_ROAD_NODE_SPEED_ZONE,0xbf1778fe3ac12e6e, CommandAddRoadNodeSpeedZone);
		SCR_REGISTER_SECURE(REMOVE_ROAD_NODE_SPEED_ZONE,0x11240d265de9105f, CommandRemoveRoadNodeSpeedZone);
        SCR_REGISTER_SECURE(OPEN_BOMB_BAY_DOORS,0x374a31568f76efaf, CommandOpenBombBayDoors);
        SCR_REGISTER_SECURE(CLOSE_BOMB_BAY_DOORS,0x93a61d7cd6f5537d, CommandCloseBombBayDoors);
		SCR_REGISTER_SECURE(GET_ARE_BOMB_BAY_DOORS_OPEN,0x2f3c82da3a15cd5e, CommandGetAreBombBayDoorsOpen);
		SCR_REGISTER_SECURE(IS_VEHICLE_SEARCHLIGHT_ON,0x09113f944fb63ed2, CommandIsVehicleSearchLightOn);
		SCR_REGISTER_SECURE(SET_VEHICLE_SEARCHLIGHT,0xa132a7a6a765a696, CommandSetVehicleSearchLight);
		SCR_REGISTER_SECURE(DOES_VEHICLE_HAVE_SEARCHLIGHT,0xcd1df129e7e656d9, CommandDoesVehicleHaveSearchLight);
		SCR_REGISTER_SECURE(IS_ENTRY_POINT_FOR_SEAT_CLEAR,0x33168c7a83d47cc0, CommandIsEntryPointForSeatClear);
		SCR_REGISTER_UNUSED(GET_NUM_ENTRY_POINTS,0xf4a2f4525f67b076, CommandGetNumberOfEntryPoints);
		SCR_REGISTER_SECURE(GET_ENTRY_POINT_POSITION,0x6cd0b79c6a87ba2d, CommandGetEntryPointPosition);
		SCR_REGISTER_SECURE(CAN_SHUFFLE_SEAT,0xcc63baf56c17834c, CommandCanShuffleSeat);
		SCR_REGISTER_SECURE(GET_NUM_MOD_KITS,0x03ac3319d1b50189, CommandGetNumModKits);
		SCR_REGISTER_SECURE(SET_VEHICLE_MOD_KIT,0x3c1bce3438ecbfc0, CommandSetVehicleModKit);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD_KIT,0x741835e0ef89491d, CommandGetVehicleModKit);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD_KIT_TYPE,0xf3f5d16790870112, CommandGetVehicleModKitType);
		SCR_REGISTER_SECURE(GET_VEHICLE_WHEEL_TYPE,0xd9b8afe0dd1ad635, CommandGetVehicleWheelType);
		SCR_REGISTER_SECURE(SET_VEHICLE_WHEEL_TYPE,0xe19d99912c7661c6, CommandSetVehicleWheelType);
		SCR_REGISTER_SECURE(GET_NUM_MOD_COLORS,0xed439e3cac3259f9, CommandGetNumModColors);
        SCR_REGISTER_SECURE(SET_VEHICLE_MOD_COLOR_1,0xf8359e159a574462, CommandSetVehicleModColor1);
        SCR_REGISTER_SECURE(SET_VEHICLE_MOD_COLOR_2,0x774ad997bb23cd05, CommandSetVehicleModColor2);
        SCR_REGISTER_SECURE(GET_VEHICLE_MOD_COLOR_1,0x0595a3cf1ba9c95c, CommandGetVehicleModColor1);
        SCR_REGISTER_SECURE(GET_VEHICLE_MOD_COLOR_2,0xb0800355bab891dd, CommandGetVehicleModColor2);
        SCR_REGISTER_SECURE(GET_VEHICLE_MOD_COLOR_1_NAME,0xf2e12eefb9affe62, CommandGetVehicleModColor1Name);
        SCR_REGISTER_SECURE(GET_VEHICLE_MOD_COLOR_2_NAME,0xbe5387f9b686eab0, CommandGetVehicleModColor2Name);
		SCR_REGISTER_UNUSED(REMOVE_VEHICLE_MOD_KIT,0xee0098dcb7b9c9ef, CommandRemoveVehicleModKit);
		SCR_REGISTER_SECURE(HAVE_VEHICLE_MODS_STREAMED_IN,0xe9e8be4cf922470d, CommandHaveVehicleModsStreamedIn);
		SCR_REGISTER_SECURE(IS_VEHICLE_MOD_GEN9_EXCLUSIVE, 0x00834eac4a96e010, CommandIsVehicleModGen9Exclusive);
		SCR_REGISTER_SECURE(SET_VEHICLE_MOD,0x0061b7c9351b41ed, CommandSetVehicleMod);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD,0x0dd0bc46c4cfd6ad, CommandGetVehicleMod);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD_VARIATION,0xccee7d38fc5d9fd6, CommandGetVehicleModVariation);
		SCR_REGISTER_SECURE(GET_NUM_VEHICLE_MODS,0x6b83f5ae0478286f, CommandGetNumVehicleMods);
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_MOD,0x0cf999545677298a, CommandRemoveVehicleMod);
		SCR_REGISTER_SECURE(TOGGLE_VEHICLE_MOD,0x6c6188d0f8ab739a, CommandToggleVehicleMod);
		SCR_REGISTER_SECURE(IS_TOGGLE_MOD_ON,0x75b2c853cb0b3047, CommandIsToggleModOn);
		SCR_REGISTER_SECURE(GET_MOD_TEXT_LABEL,0xac1327f86ac1314a, CommandGetModTextLabel);
		SCR_REGISTER_SECURE(GET_MOD_SLOT_NAME,0xfa9c34b5d0539761, CommandGetModSlotName);
		SCR_REGISTER_SECURE(GET_LIVERY_NAME,0x66b61200529ab970, CommandGetLiveryName);
		SCR_REGISTER_UNUSED(GET_LIVERY2_NAME,0xf9852988c8f04cbf, CommandGetLivery2Name);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD_MODIFIER_VALUE,0xd90d01d41407f76c, CommandGetVehicleModModifierValue);
		SCR_REGISTER_SECURE(GET_VEHICLE_MOD_IDENTIFIER_HASH,0x3e54ce58c7eb63b4, CommandGetVehicleModIdentifierHash);
		SCR_REGISTER_UNUSED(GET_CAMERA_POSITION_FOR_MOD,0xd1587801d4cf2472, CommandGetCameraPositionForMod);
        SCR_REGISTER_SECURE(PRELOAD_VEHICLE_MOD,0x7dd405d4677e064c, CommandPreloadVehicleMod);
        SCR_REGISTER_SECURE(HAS_PRELOAD_MODS_FINISHED,0x09d3ef67fa2e1e4b, CommandHasPreloadModsFinished);
        SCR_REGISTER_SECURE(RELEASE_PRELOAD_MODS,0x781a6ec21b72b52e, CommandReleasePreloadMods);
		SCR_REGISTER_UNUSED(CAN_VEHICLE_BE_MODDED,0xe0cc93e11fa599c5, CommandCanVehicleBeModded);
		SCR_REGISTER_SECURE(SET_VEHICLE_TYRE_SMOKE_COLOR,0xfef0c9a733c7b609, CommandSetVehicleTyreSmokeColor);
		SCR_REGISTER_SECURE(GET_VEHICLE_TYRE_SMOKE_COLOR,0x2ee88eb83af61b18, CommandGetVehicleTyreSmokeColor);
		SCR_REGISTER_SECURE(SET_VEHICLE_WINDOW_TINT,0x2eb7787881686751, CommandSetVehicleWindowTint);
		SCR_REGISTER_SECURE(GET_VEHICLE_WINDOW_TINT,0xf9d204d7598c0239, CommandGetVehicleWindowTint);
		SCR_REGISTER_SECURE(GET_NUM_VEHICLE_WINDOW_TINTS,0x847f16ba535b65c8, CommandGetNumVehicleWindowTints);
		SCR_REGISTER_SECURE(GET_VEHICLE_COLOR,0xc320571daaedfaab, CommandGetVehicleColor);
		SCR_REGISTER_SECURE(GET_VEHICLE_COLOURS_WHICH_CAN_BE_SET,0xe4a58617db957843, CommandGetVehicleColoursWhichCanBeSet);
		SCR_REGISTER_UNUSED(GET_VEHICLE_SOURCE_OF_DESTRUCTION,0x2a08bfc960e3ebd4, CommandGetVehicleSourceOfDestruction);
		SCR_REGISTER_SECURE(GET_VEHICLE_CAUSE_OF_DESTRUCTION,0xbca4e9d520d10b01, CommandGetVehicleCauseOfDestruction);
		SCR_REGISTER_UNUSED(GET_VEHICLE_TIME_OF_DESTRUCTION,0xf7d3de9d4188b1e0, CommandGetVehicleTimeOfDestruction);
		SCR_REGISTER_SECURE(OVERRIDE_PLANE_DAMAGE_THREHSOLD,0xb6d3cf89988e9305, CommandOverridePlaneDamageThreshold);
		SCR_REGISTER_SECURE(GET_IS_LEFT_VEHICLE_HEADLIGHT_DAMAGED,0x957511ea92cf8139, CommandGetIsLeftHeadLightDamaged);
		SCR_REGISTER_SECURE(GET_IS_RIGHT_VEHICLE_HEADLIGHT_DAMAGED,0x2dfaf27126d5d66a, CommandGetIsRightHeadLightDamaged);
        SCR_REGISTER_SECURE(GET_BOTH_VEHICLE_HEADLIGHTS_DAMAGED,0x5761f497677d0402, CommandGetBothVehicleHeadlightsDamaged);
		SCR_REGISTER_UNUSED(GET_NUMBER_OF_DAMAGED_VEHICLE_HEADLIGHTS,0x844c0858290e8275, CommandGetNumberOfDamagedHeadlights);
		SCR_REGISTER_SECURE(MODIFY_VEHICLE_TOP_SPEED,0xd356c8928e61c3f6, CommandModifyVehicleTopSpeed);
		SCR_REGISTER_SECURE(SET_VEHICLE_MAX_SPEED,0x9611b039ccc9132f, CommandSetVehicleMaxSpeed);
		SCR_REGISTER_SECURE(SET_VEHICLE_STAYS_FROZEN_WHEN_CLEANED_UP,0xd3bdaed335d2db3e, CommandSetVehicleStaysFrozenWhenCleanedUp);
		SCR_REGISTER_SECURE(SET_VEHICLE_ACT_AS_IF_HIGH_SPEED_FOR_FRAG_SMASHING,0x3edc5ce75ab1b296, CommandSetVehicleActAsIfHighSpeedForFragSmashing);
		SCR_REGISTER_SECURE(SET_PEDS_CAN_FALL_OFF_THIS_VEHICLE_FROM_LARGE_FALL_DAMAGE,0x6d8e0ccf51d7c022, CommandSetPedsCanFallOffThisVehicleFromLargeFallDamage);
		SCR_REGISTER_UNUSED(SET_ENEMY_PEDS_FORCE_FALL_OFF_THIS_VEHICLE,0xc2125f1d79401426, CommandSetEnemyPedsForceFallOffThisVehicle);
		SCR_REGISTER_UNUSED(SET_CAR_BLOWS_UP_WHEN_MISSING_ALL_WHEELS,0xb2ee455af6505db7, CommandSetCarBlowsUpWithNoWheels);
		SCR_REGISTER_SECURE(ADD_VEHICLE_COMBAT_ANGLED_AVOIDANCE_AREA,0x6d025f93a22a9d4f, CommandAddVehicleCombatAngledAvoidanceArea);
		SCR_REGISTER_UNUSED(ADD_VEHICLE_COMBAT_SPHERE_AVOIDANCE_AREA,0x9c4ee7d7129a411f, CommandAddVehicleCombatSphereAvoidanceArea);
		SCR_REGISTER_SECURE(REMOVE_VEHICLE_COMBAT_AVOIDANCE_AREA,0xdf86fbfcd72e39e9, CommandRemoveVehicleCombatAvoidanceArea);
		SCR_REGISTER_UNUSED(CLEAR_VEHICLE_COMBAT_AVOIDANCE_AREAS,0x3ef3f0ba14f3add4, CommandClearVehicleCombatAvoidanceAreas);
		SCR_REGISTER_SECURE(IS_ANY_PED_RAPPELLING_FROM_HELI,0xa9bb7a0a20b07b48, CommandIsAnyPedRappellingFromHeli);
		SCR_REGISTER_SECURE(SET_VEHICLE_CHEAT_POWER_INCREASE,0x8f7d5ed5832ac0aa, CommandSetVehicleCheatPowerIncrease);
		SCR_REGISTER_SECURE(SET_VEHICLE_INFLUENCES_WANTED_LEVEL,0x67e2c3dcb85cced2, CommandSetVehicleInfluencesWantedLevel);
		SCR_REGISTER_SECURE(SET_VEHICLE_IS_WANTED,0x5c7a358e1c48def4, CommandSetVehicleIsWanted);
        SCR_REGISTER_UNUSED(SWING_OUT_BOAT_BOOM,0x0c8e58638d9e3a59, CommandSwingOutBoatBoom);
        SCR_REGISTER_UNUSED(SWING_IN_BOAT_BOOM,0x8f5d42a1e0060075, CommandSwingInBoatBoom);
        SCR_REGISTER_SECURE(SWING_BOAT_BOOM_TO_RATIO,0xfb72e02a4b6d025b, CommandSwingBoatBoomToRatio);
        SCR_REGISTER_SECURE(SWING_BOAT_BOOM_FREELY,0xe96af53282be9e05, CommandAllowBoatBoomToAnimate);
        SCR_REGISTER_SECURE(ALLOW_BOAT_BOOM_TO_ANIMATE,0x0269717aa3500f6b, CommandAllowBoatBoomToAnimate);
        SCR_REGISTER_SECURE(GET_BOAT_BOOM_POSITION_RATIO,0x48ce4aa48303ad3a, CommandGetBoatBoomPositionRatio);
        SCR_REGISTER_SECURE(DISABLE_PLANE_AILERON,0x6c98742dfe00c9ad, CommandDisablePlaneAileron);
        SCR_REGISTER_SECURE(GET_IS_VEHICLE_ENGINE_RUNNING,0xa11c4a6609ca663a, CommandGetIsVehicleEngineRunning);
		SCR_REGISTER_SECURE(SET_VEHICLE_USE_ALTERNATE_HANDLING,0xfdca79f3a2039825, CommandSetVehicleUseAlternateHandling);
		SCR_REGISTER_UNUSED(GET_BIKERS_FEED_OFF_GROUND,0xb2ccec9990e461e2, CommandGetBikersFeedOffGround);
		SCR_REGISTER_SECURE(SET_BIKE_ON_STAND,0x003aa43765a7e9bf, CommandSetBikeOnStand);
		SCR_REGISTER_SECURE(SET_VEHICLE_NOT_STEALABLE_AMBIENTLY,0xfa37094e2df03e75, CommandSetVehicleNotStealableAmbiently);
		SCR_REGISTER_SECURE(LOCK_DOORS_WHEN_NO_LONGER_NEEDED,0x64c030791cea9715, CommandLockDoorsWhenNoLongerNeeded);
		SCR_REGISTER_SECURE(SET_LAST_DRIVEN_VEHICLE,0x7a75dc202144accf, CommandSetLastDrivenVehicle);
		SCR_REGISTER_SECURE(GET_LAST_DRIVEN_VEHICLE,0x78291c96efbbdc75, CommandGetLastDrivenVehicle);
		SCR_REGISTER_SECURE(CLEAR_LAST_DRIVEN_VEHICLE,0xb6e0e11dc88e5013, ClearLastDrivenVehicle);
		SCR_REGISTER_SECURE(SET_VEHICLE_HAS_BEEN_DRIVEN_FLAG,0xddee81f50a761f49, CommandSetVehicleHasBeenDrivenFlag);
		SCR_REGISTER_SECURE(SET_TASK_VEHICLE_GOTO_PLANE_MIN_HEIGHT_ABOVE_TERRAIN,0x1195689fecae45c7, CommandSetTaskVehicleGotoPlaneMinHeightAboveTerrain);
		SCR_REGISTER_SECURE(SET_VEHICLE_LOD_MULTIPLIER,0x09042d5723106b97, CommandSetVehicleLodMultiplier);
		SCR_REGISTER_UNUSED(SET_VEHICLE_LOD_CLAMP,0x55027d576b3ab452, CommandSetVehicleLodClamp);
        SCR_REGISTER_SECURE(SET_VEHICLE_CAN_SAVE_IN_GARAGE,0x0fcb0a306ff51eca, CommandSetVehicleCanSaveInGarage);
		SCR_REGISTER_SECURE(GET_VEHICLE_NUM_OF_BROKEN_OFF_PARTS,0x2ad597af2ba2c3ba, CommnadGetVehicleNumOfBrokenOffParts);
		SCR_REGISTER_SECURE(GET_VEHICLE_NUM_OF_BROKEN_LOOSEN_PARTS,0xce3ff89646c9c806, CommnadGetVehicleNumOfBrokenLoosenParts);
		SCR_REGISTER_SECURE(SET_FORCE_VEHICLE_ENGINE_DAMAGE_BY_BULLET,0x8753d1a4e2a2cfc1, CommandSetForceVehicleEngineDamageByBullet);
		SCR_REGISTER_SECURE(SET_VEHICLE_GENERATES_ENGINE_SHOCKING_EVENTS,0x2b1869f1a1d940f8, CommandSetVehicleCanGenerateEngineShockingEvents);
		SCR_REGISTER_SECURE(COPY_VEHICLE_DAMAGES,0xd9734ad3534a452f, CommandCopyVehicleDamage);
		SCR_REGISTER_SECURE(DISABLE_VEHICLE_EXPLOSION_BREAK_OFF_PARTS,0x6635bfd11849c0a5, CommandDisableVehicleExplosionBreakOffParts);
		SCR_REGISTER_SECURE(SET_LIGHTS_CUTOFF_DISTANCE_TWEAK,0xd3c7df69c235a6d3, CommandLightsCutoffDistanceTweak);
		SCR_REGISTER_SECURE(SET_VEHICLE_SHOOT_AT_TARGET,0x91909f6b2950d5a4, CommandSetVehicleShootAtTarget);
		SCR_REGISTER_SECURE(GET_VEHICLE_LOCK_ON_TARGET,0xa774c52677e02d95,	CommandGetVehicleLockOnTarget);
		SCR_REGISTER_SECURE(SET_FORCE_HD_VEHICLE,0x44b7e38f69d1e462, CommandSetForceHdVehicle);
		SCR_REGISTER_SECURE(SET_VEHICLE_CUSTOM_PATH_NODE_STREAMING_RADIUS,0xe5972da7f32861af, CommandSetVehicleCustomPathNodeStreamingRadius);
		SCR_REGISTER_SECURE(GET_VEHICLE_PLATE_TYPE,0x0634fde853d3cb3b, CommandGetVehiclePlateType);
		SCR_REGISTER_SECURE(TRACK_VEHICLE_VISIBILITY,0xe0a93e5adbed87de, CommandTrackVehicleVisibility				);
		SCR_REGISTER_SECURE(IS_VEHICLE_VISIBLE,0xe390839fc176621d, CommandIsVehicleVisible						);
		SCR_REGISTER_SECURE(SET_VEHICLE_GRAVITY,0x63e35ce7a9e5aed2, CommandSetVehicleGravity						);
		SCR_REGISTER_SECURE(SET_ENABLE_VEHICLE_SLIPSTREAMING,0xed7c8628ea95c8c9, CommandSetEnableVehicleSlipstreaming);
		SCR_REGISTER_SECURE(SET_VEHICLE_SLIPSTREAMING_SHOULD_TIME_OUT,0xe80599445b597dc2, CommandSetVehicleSlipstreamingShouldTimeOut);
		SCR_REGISTER_SECURE(GET_VEHICLE_CURRENT_TIME_IN_SLIP_STREAM,0x5921e530f215af43, CommandGetVehicleCurrentTimeInSlipStream);
		SCR_REGISTER_SECURE(IS_VEHICLE_PRODUCING_SLIP_STREAM,0xd775397e28c2d359, CommandIsVehicleProducingSlipStream);
		SCR_REGISTER_SECURE(SET_VEHICLE_INACTIVE_DURING_PLAYBACK,0x61a0ce40cdd0cf3e, CommandSetVehicleInactiveDuringPlayback);
		SCR_REGISTER_SECURE(SET_VEHICLE_ACTIVE_DURING_PLAYBACK,0x6ae2ded7bd7b93b8, CommandSetVehicleActiveDuringPlayback);
		SCR_REGISTER_SECURE(IS_VEHICLE_SPRAYABLE,0xcdec1eee2f3b81f4, CommandIsVehicleSprayable);
		SCR_REGISTER_SECURE(SET_VEHICLE_ENGINE_CAN_DEGRADE,0x67829e7e051ca476, CommandSetVehicleEngineCanDegrade);
		SCR_REGISTER_SECURE(DISABLE_VEHCILE_DYNAMIC_AMBIENT_SCALES,0xf4a87fa177976c03, CommandDisableVehicleDynamicAmbientScales);
		SCR_REGISTER_SECURE(ENABLE_VEHICLE_DYNAMIC_AMBIENT_SCALES,0x52a9803bafa30601, CommandEnableVehicleDynamicAmbientScales);
		SCR_REGISTER_SECURE(IS_PLANE_LANDING_GEAR_INTACT,0x333a68465eab12b7, CommandIsLandingGearIntact);
		SCR_REGISTER_SECURE(ARE_PLANE_PROPELLERS_INTACT,0x386b922b324dadee, CommnadArePlanePropellersIntact);
		SCR_REGISTER_SECURE(SET_PLANE_PROPELLER_HEALTH,0x5db16eb519ae4f6d, CommmandSetPlanePropellerHealth);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_DEFORM_WHEELS,0x2ab75bba1381e289, CommandSetVehicleCanDeformWheels);
		SCR_REGISTER_SECURE(IS_VEHICLE_STOLEN,0x8b56dbbe237edf1f, CommandIsVehicleStolen);
		SCR_REGISTER_SECURE(SET_VEHICLE_IS_STOLEN,0x4fa753674d81bf2e, CommandSetVehicleIsStolen);
		SCR_REGISTER_SECURE(SET_PLANE_TURBULENCE_MULTIPLIER,0xfcf9858c931e0022, CommandSetPlaneTurbulenceMulitiplier);
		SCR_REGISTER_SECURE(ARE_WINGS_OF_PLANE_INTACT,0xb5d888782d450a38, CommandAreWingsOfPlaneIntact);
		SCR_REGISTER_SECURE(ALLOW_AMBIENT_VEHICLES_TO_AVOID_ADVERSE_CONDITIONS,0x4cdbb300386d729e, CommandAllowAmbientVehiclesToAvoidAdverseConditions);
		
		SCR_REGISTER_SECURE(DETACH_VEHICLE_FROM_CARGOBOB,0x752b2d45ee2ff015, CommandDetachVehicleFromCargobob);
		SCR_REGISTER_SECURE(DETACH_VEHICLE_FROM_ANY_CARGOBOB,0xb27b37a448cca2fd, CommandDetachVehicleFromAnyCargobob);
		SCR_REGISTER_SECURE(DETACH_ENTITY_FROM_CARGOBOB,0x5f9ffbd75039cf11, CommandDetachEntityFromCargobob);
		SCR_REGISTER_SECURE(IS_VEHICLE_ATTACHED_TO_CARGOBOB,0xc25fbab58dc1031d, CommandIsVehicleAttachedToCargobob);
		SCR_REGISTER_UNUSED(IS_ENTITY_ATTACHED_TO_CARGOBOB,0x495d570cd2eec42b, CommandIsEntityAttachedToCargobob);
		SCR_REGISTER_SECURE(GET_VEHICLE_ATTACHED_TO_CARGOBOB,0xb5c8c9a4385f1f4b, CommandGetVehcileAttachedToCargobob);
		SCR_REGISTER_SECURE(GET_ENTITY_ATTACHED_TO_CARGOBOB,0x96df8c50a64d66a8, CommandGetEntityAttachedToCargobob);
		SCR_REGISTER_SECURE(ATTACH_VEHICLE_TO_CARGOBOB,0xd9af6077705e4e5c, CommandAttachVehicleToCargobob);
		SCR_REGISTER_SECURE(ATTACH_ENTITY_TO_CARGOBOB,0x8820194facff8f5b, CommandAttachEntityToCargobob);
		SCR_REGISTER_SECURE(SET_CARGOBOB_FORCE_DONT_DETACH_VEHICLE,0x8c7f8ce503fca873, CommandCargobobForceNoVehicleDetach);
		SCR_REGISTER_SECURE(SET_CARGOBOB_EXCLUDE_FROM_PICKUP_ENTITY,0xc6f3ddf3187e4494, CommandCargobobSetExcludeFromPickupEntity);
		SCR_REGISTER_SECURE(CAN_CARGOBOB_PICK_UP_ENTITY,0x4291d19bdeb13f01, CommandCanCargobobPickUpEntity);
		SCR_REGISTER_SECURE(GET_ATTACHED_PICK_UP_HOOK_POSITION,0x7e2eacab40c87020, CommandGetAttachedPickUpHookPosition);
		SCR_REGISTER_SECURE(DOES_CARGOBOB_HAVE_PICK_UP_ROPE,0xe420a4190e3e41aa, CommandDoesCargobobHavePickUpRope);
		SCR_REGISTER_SECURE(CREATE_PICK_UP_ROPE_FOR_CARGOBOB,0xd4113ee2bf196c99, CommandCreatePickUpRopeForCargobob);
		SCR_REGISTER_SECURE(REMOVE_PICK_UP_ROPE_FOR_CARGOBOB,0x12bef18a46d6ab20, CommandRemovePickUpRopeForCargobob);
		SCR_REGISTER_SECURE(SET_PICKUP_ROPE_LENGTH_FOR_CARGOBOB,0xceb7f0703415589d, CommandSetPickUpRopeLengthForCargobob);
		SCR_REGISTER_SECURE(SET_PICKUP_ROPE_LENGTH_WITHOUT_CREATING_ROPE_FOR_CARGOBOB,0x4c545c35d0b0655b, CommandSetPickUpRopeLengthWithoutCreatingRopeForCargobob);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_ROPE_DAMPING_MULTIPLIER,0x03d8deff992b8869, CommandSetCargobobPickupRopeDampingMultiplier);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_ROPE_TYPE,0x2270c69ffec1fc93, CommandSetCargobobPickupRopeType);
		SCR_REGISTER_SECURE(DOES_CARGOBOB_HAVE_PICKUP_MAGNET,0xe9fbc1941336531b, CommandDoesCargobobHaveMagnet);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_ACTIVE,0x4733e81aa7724c96, CommandSetCargobobMagnetActive);
		SCR_REGISTER_UNUSED(GET_CARGOBOB_PICKUP_MAGNET_TARGET_ENTITY,0xf73603492540f71d, CommandGetCargobobMagnetTargetEntity);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_STRENGTH,0xb158ef3c34e943b3, CommandSetCargobobMagnetStrength);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_FALLOFF,0xbe43b65ce2f5e213, CommandSetCargobobMagnetFalloff);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_REDUCED_STRENGTH,0xdd6b1358a060226e, CommandSetCargobobMagnetReducedStrength);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_REDUCED_FALLOFF,0xb196a05e5f7f0973, CommandSetCargobobMagnetReducedFalloff);		
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_PULL_STRENGTH,0xea199d281ab3bdab, CommandSetCargobobMagnetPullStrength);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_PULL_ROPE_LENGTH,0x7eab90e2985f7b78, CommandSetCargobobMagnetPullRopeLength);		
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_SET_TARGETED_MODE,0xefd6209466add5a7, CommandSetCargobobMagnetSetTargetedMode);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_SET_AMBIENT_MODE,0x817b28edf0a1fc78, CommandSetCargobobMagnetSetAmbientMode);
		SCR_REGISTER_SECURE(SET_CARGOBOB_PICKUP_MAGNET_ENSURE_PICKUP_ENTITY_UPRIGHT,0xfb5f118c8166d363, CommandSetCargobobMagnetEnsurePickupEntityUpright);
		SCR_REGISTER_UNUSED(SET_CARGOBOB_PICKUP_MAGNET_EFFECT_RADIUS,0x05d6ee9c7eb75e5b, CommandSetCargobobMagnetEffectRadius);		

		SCR_REGISTER_SECURE(DOES_VEHICLE_HAVE_WEAPONS,0x35cef967158ab23d, CommandDoesVehicleHaveWeapons);
		SCR_REGISTER_SECURE(SET_VEHICLE_WILL_TELL_OTHERS_TO_HURRY,0x11db11e02c3d15b8, CommandSetVehicleWillTellOthersToHurry);
		SCR_REGISTER_SECURE(DISABLE_VEHICLE_WEAPON,0x314a88deaa6f4ac8, CommandDisableVehicleWeapon);
		SCR_REGISTER_SECURE(IS_VEHICLE_WEAPON_DISABLED,0xca3044631d47dc92, CommandIsVehicleWeaponDisabled);
		SCR_REGISTER_SECURE(SET_VEHICLE_USED_FOR_PILOT_SCHOOL,0xdf38220db6f7f01b, CommandSetVehicleUsedForPilotSchool);
		SCR_REGISTER_SECURE(SET_VEHICLE_ACTIVE_FOR_PED_NAVIGATION,0x9b63dbe83030b207, CommandSetVehicleActiveForPedNavigation);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS,0xac23c79ca02f144e, CommandGetVehicleClass);
		SCR_REGISTER_SECURE(GET_VEHICLE_CLASS_FROM_NAME,0x3dec1c3f21ae4d70, CommandGetVehicleClassFromName);
		SCR_REGISTER_SECURE(SET_PLAYERS_LAST_VEHICLE,0x583aa2db1278b578, CommandSetPlayersLastVehicle);
		SCR_REGISTER_SECURE(SET_VEHICLE_CAN_BE_USED_BY_FLEEING_PEDS,0x34d9c57cc5ce87ea, CommandSetVehicleCanBeUsedByFleeingPeds);
		SCR_REGISTER_SECURE(SET_AIRCRAFT_PILOT_SKILL_NOISE_SCALAR,0x52ef8b7f3d0a9ea8, CommandSetAircraftPilotSkillNoiseScalar);
		SCR_REGISTER_SECURE(SET_VEHICLE_DROPS_MONEY_WHEN_BLOWN_UP,0xa2b9ae794c45034b, CommandSetVehicleDropsMoneyWhenBlownUp);
		SCR_REGISTER_SECURE(SET_VEHICLE_KEEP_ENGINE_ON_WHEN_ABANDONED,0x63fb8fb0b9df030f, CommandSetVehicleKeepEngineOnWhenAbandoned);
		SCR_REGISTER_SECURE(SET_VEHICLE_IMPATIENCE_TIMER,0x84c04424d477a997, CommandSetVehicleImpatienceTimer);
		SCR_REGISTER_SECURE(SET_VEHICLE_HANDLING_OVERRIDE,0xab92ea451f06cf1b, CommandSetVehicleHandlingOverride);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXTENDED_REMOVAL_RANGE,0x22553d3793725f7b, CommandSetVehicleExtendedRemovalRange);
		SCR_REGISTER_SECURE(SET_VEHICLE_STEERING_BIAS_SCALAR,0xa0727ea79f309246, CommandSetVehicleSteeringBiasScalar);
		SCR_REGISTER_SECURE(SET_HELI_CONTROL_LAGGING_RATE_SCALAR,0x316a18d1ac7b9ef4, CommandSetHeliControlLaggingRateScalar);
		SCR_REGISTER_SECURE(SET_VEHICLE_FRICTION_OVERRIDE,0xe1ae83af2dcc6161, CommandSetVehicleFrictionOverride);
		SCR_REGISTER_SECURE(SET_VEHICLE_WHEELS_CAN_BREAK_OFF_WHEN_BLOW_UP,0x1a63025eb01d5de9, CommandSetVehicleWheelsCanBreakOffWhenBlowUp);
		SCR_REGISTER_SECURE(ARE_PLANE_CONTROL_PANELS_INTACT,0x77a329ae2223ddbc, CommandArePlaneControlPanelsIntact);
		SCR_REGISTER_SECURE(SET_VEHICLE_CEILING_HEIGHT,0x04bf67760881def1, CommandSetVehicleCeilingHeight);
		SCR_REGISTER_SECURE(SET_VEHICLE_NO_EXPLOSION_DAMAGE_FROM_DRIVER,0xddadb1f8557015d7,CommandSetVehicleNoExplosionDamageFromDriver);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_ROUTE_HISTORY,0x603c50e35615a4c7, CommandClearVehicleRouteHistory);
		SCR_REGISTER_SECURE(DOES_VEHICLE_EXIST_WITH_DECORATOR,0x5c12ab3d0be16d1a, CommandDoesVehicleExistWithDecorator);
		SCR_REGISTER_UNUSED(SET_PLANE_PROPELLERS_BREAK_OFF_ON_CONTACT, 0x9A025503, CommandSetPlanePropellersBreakOffOnContact);
		SCR_REGISTER_SECURE(SET_VEHICLE_AI_CAN_USE_EXCLUSIVE_SEATS,0xc7050f5ac2f8481e, CommandSetVehicleAIUseExclusiveSeat);
		SCR_REGISTER_SECURE(SET_VEHICLE_EXCLUSIVE_DRIVER,0x948c8d759cb6bcfe, CommandSetVehicleExclusiveDriver);
		SCR_REGISTER_SECURE(IS_PED_EXCLUSIVE_DRIVER_OF_VEHICLE,0x7525e83a62ae29dd, CommandIsPedExclusiveDriverOfVehicle);
		SCR_REGISTER_SECURE(DISABLE_INDIVIDUAL_PLANE_PROPELLER,0x54c298c51182b8ac, CommandDisableIndividualPlanePropeller);
		SCR_REGISTER_SECURE(SET_VEHICLE_FORCE_AFTERBURNER,0xbf5b39c84ee2aea5, CommandSetVehicleForceAfterburner);
		SCR_REGISTER_SECURE(SET_DONT_PROCESS_VEHICLE_GLASS,0x077fdc1a202b9273, CommandSetDontProcessVehicleGlass);
		SCR_REGISTER_SECURE(SET_DISABLE_WANTED_CONES_RESPONSE,0xdf14057b445c4cb0, CommandSetDisableWantedConesResponse);
		SCR_REGISTER_SECURE(SET_USE_DESIRED_Z_CRUISE_SPEED_FOR_LANDING,0x9e172f1a73aacd6e, CommandSetUseDesiredZCruiseSpeedForLanding);
		SCR_REGISTER_SECURE(SET_ARRIVE_DISTANCE_OVERRIDE_FOR_VEHICLE_PERSUIT_ATTACK,0x059021d4ac5801f4, CommandOverrideArriveDistanceForVehiclePersuitAttack);
		SCR_REGISTER_SECURE(SET_VEHICLE_READY_FOR_CLEANUP,0x2261db27aa9058b4, CommandSetVehicleReadyForCleanup);
		SCR_REGISTER_SECURE(SET_DISTANT_CARS_ENABLED,0x21e9cc24def14473, CommandSetDistantCarsEnabled);
		SCR_REGISTER_SECURE(SET_VEHICLE_NEON_COLOUR,0x74fcade93b81fd35, CommandSetVehicleNeonColour);
		SCR_REGISTER_SECURE(SET_VEHICLE_NEON_INDEX_COLOUR,0xa8b24796d7f2734c, CommandSetVehicleNeonIndexColour);
		SCR_REGISTER_SECURE(GET_VEHICLE_NEON_COLOUR,0xedb21c30f1baf83a, CommandGetVehicleNeonColour);
		SCR_REGISTER_SECURE(SET_VEHICLE_NEON_ENABLED,0xbe69e921963a86b7, CommandSetVehicleNeonEnabled);
		SCR_REGISTER_SECURE(GET_VEHICLE_NEON_ENABLED,0x6ca60a8ee52231d2, CommandGetVehicleNeonEnabled);
		SCR_REGISTER_SECURE(SET_AMBIENT_VEHICLE_NEON_ENABLED,0xab92ed80a05c5258, CommandSetAmbientVehicleNeonEnabled);
		SCR_REGISTER_UNUSED(GET_AMBIENT_VEHICLE_NEON_ENABLED,0xa7be15040bd6b772, CommandGetAmbientVehicleNeonEnabled);
		SCR_REGISTER_SECURE(SUPPRESS_NEONS_ON_VEHICLE,0x5f5a7d81d3817e5e, CommandSuppressNeonsOnVehicle);
		SCR_REGISTER_UNUSED(ARE_NEONS_ON_VEHICLE_SUPPRESSED,0x56a5051d46b63553, CommandAreNeonsOnVehicleSuppressed);
		SCR_REGISTER_SECURE(SET_DISABLE_SUPERDUMMY,0x085065fb04601e74, CommandSetDisableSuperDummy);	
		SCR_REGISTER_SECURE(REQUEST_VEHICLE_DIAL,0xa5b263a8703e8d01, CommandRequestDials);			
		SCR_REGISTER_SECURE(GET_VEHICLE_BODY_HEALTH,0x29bc8ced19e45cc0, CommandGetVehicleBodyHealth);
		SCR_REGISTER_SECURE(SET_VEHICLE_BODY_HEALTH,0x8789481e090bf20c, CommandSetVehicleBodyHealth);
		SCR_REGISTER_SECURE(GET_VEHICLE_SIZE,0x9225ae5215730c05, CommandGetSize);
		SCR_REGISTER_SECURE(GET_FAKE_SUSPENSION_LOWERING_AMOUNT,0x382b07f45aacd5bc, CommandGetFakeSuspensionLoweringAmount);
		SCR_REGISTER_SECURE(SET_CAR_HIGH_SPEED_BUMP_SEVERITY_MULTIPLIER,0x1f7f46e954008f0c, CommandSetCarHighSpeedBumpSeverityMultiplier);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_VEHICLE_DOORS,0x6c3517e064234913, CommandGetNumberOfVehicleDoors);
		SCR_REGISTER_SECURE(SET_HYDRAULICS_CONTROL,0x8c00b426d81e26ba, CommandSetHydraulicsControl);
		SCR_REGISTER_SECURE(SET_CAN_ADJUST_GROUND_CLEARANCE,0xdfb0bf9106595870, CommandSetCanAdjustGroundClearance);
		SCR_REGISTER_SECURE(GET_VEHICLE_HEALTH_PERCENTAGE,0xf3778f4a893d38e2, CommandGetVehicleHealthPercentage);
		SCR_REGISTER_SECURE(GET_VEHICLE_IS_MERCENARY,0xec74d8986996234a, CommandIsMercVehicle);
		SCR_REGISTER_SECURE(SET_VEHICLE_BROKEN_PARTS_DONT_AFFECT_AI_HANDLING,0x3c5b5e6395329033, CommandSetBrokenPartsAffectHandling);
		SCR_REGISTER_SECURE(SET_VEHICLE_KERS_ALLOWED,0x14544d8fd0820f1f, CommandSetKERSAllowed);
		SCR_REGISTER_SECURE(GET_VEHICLE_HAS_KERS,0xa70b4ea2ad8d6806, CommandGetVehicleHasKers);
		SCR_REGISTER_UNUSED(IS_VEHICLE_ASLEEP,0x81552018d55d9f95, CommandIsVehicleAsleep);
		SCR_REGISTER_SECURE(SET_PLANE_RESIST_TO_EXPLOSION,0x2e1d24379c889b21, CommandSetPlaneResistToExplosion);
		SCR_REGISTER_SECURE(SET_HELI_RESIST_TO_EXPLOSION,0x8074cc1886802912, CommandSetHeliResistToExplosion);
		SCR_REGISTER_SECURE(SET_DISABLE_BMX_EXTRA_TRICK_FORCES,0xb2ffdfebf8f94f36, SetDisableExtraTrickForces);
		SCR_REGISTER_SECURE(SET_HYDRAULIC_SUSPENSION_RAISE_FACTOR,0x6587552e6995c1a3, CommandSetHydraulicSuspensionRaiseFactor);
		SCR_REGISTER_SECURE(GET_HYDRAULIC_SUSPENSION_RAISE_FACTOR,0x4d64f9676fc057fb, CommandGetHydraulicSuspensionRaiseFactor);
		SCR_REGISTER_SECURE(SET_CAN_USE_HYDRAULICS,0xe538576974378cb9, CommandSetCanUseHydraulics);
		SCR_REGISTER_SECURE(SET_HYDRAULIC_VEHICLE_STATE,0xac5c50178f2a2aea, CommandSetHydraulicVehicleState);
		SCR_REGISTER_SECURE(SET_HYDRAULIC_WHEEL_STATE,0x76e092faf2ed7475, CommandSetHydraulicWheelState);
		SCR_REGISTER_SECURE(HAS_VEHICLE_PETROLTANK_SET_ON_FIRE_BY_ENTITY,0xb0f79929922e2b71, CommandHasVehiclePetrolTankBeenSetOnFireByEntity);
		SCR_REGISTER_SECURE(CLEAR_VEHICLE_PETROLTANK_FIRE_CULPRIT,0xc99d918a7123b872, CommandClearVehiclePetrolTankFireCulprit);
		SCR_REGISTER_SECURE(SET_VEHICLE_BOBBLEHEAD_VELOCITY,0xed180afe4c1d3f53, CommandVehicleSetBobbleHeadVelocity);
		SCR_REGISTER_SECURE(GET_VEHICLE_IS_DUMMY,0x3eb9c70ed5b40f55, CommandVehicleGetIsDummy);
		SCR_REGISTER_UNUSED(TEST_VEHICLE_COORDS,0xa239619e6d3e7fc3, CommandTestVehicleCoords);

		SCR_REGISTER_SECURE(SET_VEHICLE_DAMAGE_SCALE,0x083d28d7fa474397, CommandSetVehicleDamageScale);
		SCR_REGISTER_SECURE(SET_VEHICLE_WEAPON_DAMAGE_SCALE,0x727aa8128642805a, CommandSetVehicleWeaponDamageScale);
		
		SCR_REGISTER_SECURE(SET_DISABLE_DAMAGE_WITH_PICKED_UP_ENTITY,0x2dc5c2b3129a25fd, CommandSetDisableDamageWithPickedUpEntity);
		SCR_REGISTER_SECURE(SET_VEHICLE_USES_MP_PLAYER_DAMAGE_MULTIPLIER,0x252c1729e923ecd6, CommandSetVehicleUsesMPPlayerDamageMultiplier);	

		SCR_REGISTER_SECURE(SET_BIKE_EASY_TO_LAND,0xcca20449c6ef3927, CommandSetBikeEasyToLand);	
		SCR_REGISTER_UNUSED(SET_BEAST_VEHICLE,0x9e1755d045373ace, CommandSetBeastVehicle);	
		SCR_REGISTER_SECURE(SET_INVERT_VEHICLE_CONTROLS,0xa556cfd86100d12a, CommandSetInvertVehicleControls);	

		SCR_REGISTER_SECURE(SET_SPEED_BOOST_EFFECT_DISABLED,0x2f58745217388192, CommandSetSpeedBoostEffectDisabled);	
		SCR_REGISTER_SECURE(SET_SLOW_DOWN_EFFECT_DISABLED,0xe3c83ff4290fbcd4, CommandSetSlowDownEffectDisabled);	

		SCR_REGISTER_SECURE(SET_FORMATION_LEADER,0x14ef323c3c234bfa, CommandSetFormationLeader);	
		SCR_REGISTER_SECURE(RESET_FORMATION_LEADER,0x76bd01f3fb33767b, CommandResetFormationLeader);	

		SCR_REGISTER_SECURE(GET_IS_BOAT_CAPSIZED,0xee0ec71c8a5c5b6a, CommandGetIsBoatCapsized);	

		SCR_REGISTER_SECURE(SET_ALLOW_RAMMING_SOOP_OR_RAMP,0xd045d56d95a3d704, CommandSetRammingScoopOrRamp);	
		
		SCR_REGISTER_SECURE(SET_SCRIPT_RAMP_IMPULSE_SCALE,0x33d2ad0783fd0d43, CommandSetScriptRampImpulseScale);	

		SCR_REGISTER_SECURE(GET_IS_DOOR_VALID,0xf9b6a519028cd38c, CommandGetIsDoorValid);	
		
		SCR_REGISTER_SECURE(SET_SCRIPT_ROCKET_BOOST_RECHARGE_TIME,0xddecedbe203e1057, CommandSetScriptRocketBoostRechargeTime);

		SCR_REGISTER_SECURE(GET_HAS_ROCKET_BOOST,0x4545af649bd80692, CommandGetHasRocketBoost);

		SCR_REGISTER_SECURE(IS_ROCKET_BOOST_ACTIVE,0xd377cbcd474fdcfa, CommandIsRocketBoostActive);

		SCR_REGISTER_SECURE(SET_ROCKET_BOOST_ACTIVE,0x416220c999f84215, CommandSetRocketBoostActive);

		SCR_REGISTER_SECURE(GET_HAS_RETRACTABLE_WHEELS,0xd4d74c1f667f85bf, CommandGetHasRetractableWheels);

		SCR_REGISTER_SECURE(GET_IS_WHEELS_RETRACTED,0xc3e9d5f4a9806ea3, CommandGetIsWheelsRetracted);

		SCR_REGISTER_SECURE(SET_WHEELS_EXTENDED_INSTANTLY,0x077cc59992f9f853, CommandsSetWheelsExtendedInstantly);
		SCR_REGISTER_SECURE(SET_WHEELS_RETRACTED_INSTANTLY,0xed10425ca9977820, CommandsSetWheelsRetractedInstantly);

		SCR_REGISTER_SECURE(GET_CAR_HAS_JUMP,0xe14cac0fa19d3f5d, CommandGetCarHasJump);

		SCR_REGISTER_SECURE(SET_USE_HIGHER_CAR_JUMP,0xfc787cf19ed487e6, CommandUseHigherCarJump);
		SCR_REGISTER_SECURE(SET_CLEAR_FREEZE_WAITING_ON_COLLISION_ONCE_PLAYER_ENTERS,0x41ec6688527c1c31, CommandSetClearWaitingOnCollisionOnPlayerEnter);

		SCR_REGISTER_SECURE(SET_VEHICLE_WEAPON_RESTRICTED_AMMO,0x97181d540a6bce59, CommandSetVehicleWeaponRestrictedAmmo);
		SCR_REGISTER_SECURE(GET_VEHICLE_WEAPON_RESTRICTED_AMMO,0x684bcc70f361bdc0, CommandGetVehicleWeaponRestrictedAmmo);

		SCR_REGISTER_SECURE(GET_VEHICLE_HAS_PARACHUTE,0x26ecb449f872b70c, CommandGetVehicleHasParachute);
		SCR_REGISTER_SECURE(GET_VEHICLE_CAN_DEPLOY_PARACHUTE,0x20a739ee22e77125, CommandGetVehicleCanDeployParachute);
		SCR_REGISTER_SECURE(VEHICLE_START_PARACHUTING,0xbcea7dc770ce00a9, CommandVehicleStartParachuting);
		SCR_REGISTER_SECURE(IS_VEHICLE_PARACHUTE_DEPLOYED,0xc47b53107bd90e9f, CommandIsVehicleParachuteDeployed);
		
		SCR_REGISTER_SECURE(VEHICLE_SET_RAMP_AND_RAMMING_CARS_TAKE_DAMAGE,0xcd98c8cea8a43820, CommandVehicleSetRampAndRammingCarsTakeDamage);

		SCR_REGISTER_SECURE(VEHICLE_SET_ENABLE_RAMP_CAR_SIDE_IMPULSE,0xf5e968755c418569, CommandVehicleSetEnableRampCarSideImpulse);

		SCR_REGISTER_SECURE(VEHICLE_SET_ENABLE_NORMALISE_RAMP_CAR_VERTICAL_VELOCTIY,0x82262ff616167463, CommandVehicleSetNormaliseRampCarVerticalVelocity);

		SCR_REGISTER_SECURE(VEHICLE_SET_JET_WASH_FORCE_ENABLED,0x1435be4ef74af35f, CommandVehicleSetJetWashForceEnabled);
		
		SCR_REGISTER_SECURE(SET_VEHICLE_WEAPON_CAN_TARGET_OBJECTS,0xbfead8b9e5c655f1, CommandSetVehicleWeaponCanTargetObjects);		

		SCR_REGISTER_SECURE(SET_VEHICLE_USE_BOOST_BUTTON_FOR_WHEEL_RETRACT,0x0b1375f23b0d70ee, CommandSetUseBoostButtonForWheelRetract);		

		SCR_REGISTER_UNUSED(VEHICLE_UPDATE_SLIPPERY_BOX_LIMITS,0x0cc3480a05e78d19, CommandUpdateSlipperyBoxLimits);		
		SCR_REGISTER_UNUSED(VEHICLE_BREAK_OFF_SLIPPERY_BOX,0x10c32cb4ba5d57ac, CommandBreakOffSlipperyBox);		

		SCR_REGISTER_SECURE(VEHICLE_SET_PARACHUTE_MODEL_OVERRIDE,0x37b31991b0b879d6, CommandSetVehicleParachuteModelOverride);		
		SCR_REGISTER_SECURE(VEHICLE_SET_PARACHUTE_MODEL_TINT_INDEX,0x56f9d6840204745e, CommandSetVehicleParachuteModelTintIndex);		
		SCR_REGISTER_UNUSED(VEHICLE_GET_PARACHUTE_MODEL_TINT_INDEX,0xfc299f794576b6f7, CommandGetVehicleParachuteModelTintIndex);		

		SCR_REGISTER_SECURE(VEHICLE_SET_OVERRIDE_EXTENABLE_SIDE_RATIO,0x64cf64209aff3ddc, CommandSetOverrideExtendableSideRatio);	
		SCR_REGISTER_SECURE(VEHICLE_SET_EXTENABLE_SIDE_TARGET_RATIO,0x858ee2d5cfb29775, CommandSetExtendableSideTargetRatio);	
		SCR_REGISTER_SECURE(VEHICLE_SET_OVERRIDE_SIDE_RATIO,0xb6ab27964aadb068, CommandSetExtendableSideRatio);	
		
		SCR_REGISTER_SECURE(GET_ALL_VEHICLES,0xa0547b659f4e7b59, CommandGetAllVehicles);

		SCR_REGISTER_SECURE(SET_CARGOBOB_EXTA_PICKUP_RANGE, 0x72beccf4b829522e, CommandSetCargobobExtraPickupRangeAllowance);

		SCR_REGISTER_SECURE(SET_OVERRIDE_VEHICLE_DOOR_TORQUE, 0x66e3aaface2d1eb8, CommandSetOverrideVehicleDoorTorque);

		SCR_REGISTER_SECURE(SET_WHEELIE_ENABLED, 0x1312ddd8385aee4e, CommandSetWheelieEnabled);
		SCR_REGISTER_SECURE(SET_DISABLE_HELI_EXPLODE_FROM_BODY_DAMAGE, 0xedbc8405b3895cc9, CommandSetDisableHeliExplodeFromBodyDamage);
		SCR_REGISTER_SECURE(SET_DISABLE_EXPLODE_FROM_BODY_DAMAGE_ON_COLLISION, 0x26e13d440e7f6064, CommandSetDisableExplodeFromBodyDamageOnCollision);

		SCR_REGISTER_SECURE(SET_TRAILER_ATTACHMENT_ENABLED, 0x2fa2494b47fdd009, CommandSetTrailerAttachmentEnabled);

		SCR_REGISTER_SECURE(SET_ROCKET_BOOST_FILL, 0xfeb2dded3509562e, CommandSetRocketBoostFill);
		SCR_REGISTER_SECURE(SET_GLIDER_ACTIVE, 0x544996c0081abdeb, CommandSetGliderActive);

		SCR_REGISTER_UNUSED(SET_TRAILER_CAN_ATTACH_TO_LOCAL_PLAYER, 0xd9c768ab47122dbd, CommandSetTrailerCanAttachToLocalPlayer);
		
		SCR_REGISTER_SECURE(SET_SHOULD_RESET_TURRET_IN_SCRIPTED_CAMERAS, 0x78ceee41f49f421f, CommandSetShouldResetTurreatInScriptedCameras);
		SCR_REGISTER_SECURE(SET_VEHICLE_DISABLE_COLLISION_UPON_CREATION, 0xaf60e6a2936f982a, CommandSetVehicleDisableCollisionUponCreation);

		SCR_REGISTER_SECURE(SET_GROUND_EFFECT_REDUCES_DRAG, 0x430a7631a84c9be7, CommandSetGroundEffectReducesDrag);
		
		SCR_REGISTER_SECURE(SET_DISABLE_MAP_COLLISION, 0x75627043c6aa90ad, CommandSetDisableMapCollision);

		SCR_REGISTER_SECURE(SET_DISABLE_PED_STAND_ON_TOP, 0x8235f1bead557629, CommandSetDisablePedStandOnTop);

		SCR_REGISTER_SECURE(SET_VEHICLE_DAMAGE_SCALES, 0x9640e30a7f395e4b, SetVehicleDamageScales);
		SCR_REGISTER_UNUSED(SET_HELI_DAMAGE_SCALES, 0xbd1aa4ad082b71ad, SetHeliDamageScales);
		SCR_REGISTER_SECURE(SET_PLANE_SECTION_DAMAGE_SCALE, 0x0bbb9a7a8ffe931b, SetPlaneSectionDamageScale);
		SCR_REGISTER_UNUSED(SET_PLANE_LANDING_GEAR_SECTION_DAMAGE_SCALE, 0x46472312b16b12b9, SetPlaneLandingGearSectionDamageScale);
		SCR_REGISTER_UNUSED(SET_PLANE_CONTROL_SECTIONS_SHOULD_BREAK_OFF_FROM_EXPLOSIONS, 0xdd8a2d3337f04196, SetPlaneSetControllingSectionShouldBreakOffWhenHitByExplosion);

		SCR_REGISTER_SECURE(SET_HELI_CAN_PICKUP_ENTITY_THAT_HAS_PICK_UP_DISABLED, 0x94a68da412c4007d, SetHeliCanPickUpObjectsThatHavePickupDisabled);
		
		SCR_REGISTER_SECURE(SET_VEHICLE_BOMB_AMMO,0xfcdc8bee4424aa2a, CommandSetVehicleBombAmmo);
		SCR_REGISTER_SECURE(GET_VEHICLE_BOMB_AMMO,0x10dc0a99c89a6570, CommandGetVehicleBombAmmo);
		SCR_REGISTER_SECURE(SET_VEHICLE_COUNTERMEASURE_AMMO,0xc5f03cf322cb836d, CommandSetVehicleCountermeasureAmmo);
		SCR_REGISTER_SECURE(GET_VEHICLE_COUNTERMEASURE_AMMO,0x0771bf1c8feeba14, CommandGetVehicleCountermeasureAmmo);
		SCR_REGISTER_SECURE(SET_HELI_COMBAT_OFFSET,0x3520bcbc9e81f010, CommandSetHeliCombatOffset);

		SCR_REGISTER_SECURE(GET_CAN_VEHICLE_BE_PLACED_HERE,0x0235225edaf1a078, Command_GetCanVehicleBePlacedHere);

		SCR_REGISTER_SECURE(SET_DISABLE_AUTOMATIC_CRASH_TASK,0x2e8969970014be4c, ComandSetDisableAutomaticCrashTask);

		SCR_REGISTER_SECURE(SET_SPECIAL_FLIGHT_MODE_RATIO,0x13ed1d6bcdec5ab5, ComandSetSpecialFlightModeRatio);
		SCR_REGISTER_SECURE(SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO,0x16c2c89df3a1e544, CommandSetSpecialFlightModeTargetRatio);
		SCR_REGISTER_SECURE(SET_SPECIAL_FLIGHT_MODE_ALLOWED,0xe133aec3b69a735e, CommandSetSpecialFlightModeAllowed);
		SCR_REGISTER_SECURE(SET_DISABLE_HOVER_MODE_FLIGHT,0x702dc2f046dfec2b, CommandSetDisableHoverModeFlight);
		SCR_REGISTER_UNUSED(GET_DISABLE_HOVER_MODE_FLIGHT,0x6d204c41ddabfeb1, CommandGetDisableHoverModeFlight);
		SCR_REGISTER_UNUSED(SET_DEPLOY_OUTRIGGERS,0x6fb6f7a63cba0c1e, CommandSetDeployOutriggers);
        SCR_REGISTER_SECURE(GET_OUTRIGGERS_DEPLOYED,0xf4a878e80ee5f492, CommandGetOutriggersDeployed);
        
		
		SCR_REGISTER_SECURE(FIND_SPAWN_COORDINATES_FOR_HELI,0x22d47eddb803bb38, CommandFindSpawnCoordinatesForHeli);

		SCR_REGISTER_SECURE(SET_DEPLOY_FOLDING_WINGS,0x3bc0b31eec98d938, CommandSetDeployFoldingWings);
		SCR_REGISTER_SECURE(ARE_FOLDING_WINGS_DEPLOYED,0x70578c38f82a5d0d, GetAreFoldingWingsDeployed);

        SCR_REGISTER_UNUSED(SET_CAN_ROLL_AND_YAW_WHEN_CRASHING,0x5f8633735bff16d4, SetCanRollAndYawWhenCrashing);
		SCR_REGISTER_SECURE(SET_DIP_STRAIGHT_DOWN_WHEN_CRASHING_PLANE,0x7c82a3bd8b91d269, SetDipStraightDownWhenCrashing);

        SCR_REGISTER_SECURE(SET_TURRET_HIDDEN,0x57a6afe7707c81b0, CommandSetTurretHidden);
        
		SCR_REGISTER_SECURE(SET_HOVER_MODE_WING_RATIO,0xf216ab299663b813, CommandSetHoverModeWingRatio);	 
        SCR_REGISTER_SECURE(SET_DISABLE_TURRET_MOVEMENT,0x133203029bc9528b, CommandDisableTurretMovement);	 
        
		SCR_REGISTER_SECURE( SET_FORCE_FIX_LINK_MATRICES,0xb4ac87d9bcb5dcf7, CommandForceFixLinkMatrices );

		SCR_REGISTER_SECURE( SET_TRANSFORM_RATE_FOR_ANIMATION,0xb03c1c7ae70e10c1, CommandSetTransformRateForAnimation );
		SCR_REGISTER_SECURE( SET_TRANSFORM_TO_SUBMARINE_USES_ALTERNATE_INPUT,0x24ca47b99c8a71ba, CommandSetTransformToSubmarineUsesAlternateInput );

		SCR_REGISTER_UNUSED( GET_VEHICLE_MASS,0xc5b159460bc9432f, CommandGetVehicleMass );
		SCR_REGISTER_UNUSED( GET_VEHICLE_WEAPON_DAMAGE_MULT,0x58fd505c1846e7b9, CommandGetVehicleWeaponDamageMult );
		SCR_REGISTER_UNUSED( GET_VEHICLE_COLLISION_DAMAGE_MULT,0x640e1fe2aeba3dd8, CommandGetVehicleCollisionDamageMult );
		SCR_REGISTER_UNUSED( GET_VEHICLE_HAS_RECDUCED_STICKY_BOMB_DAMAGE,0x73a4c751d3f57ecb, CommandGetVehicleHasReducedStickyBombDamage );
		SCR_REGISTER_UNUSED( GET_VEHICLE_HAS_CAPPED_EXPLOSION_DAMAGE,0xf8b27c5f710dc472, CommandGetVehicleHasCappedExplosionDamage );
		SCR_REGISTER_UNUSED( GET_VEHICLE_HAS_BULLET_PROOF_GLASS,0x439355586b075ffc, CommandGetVehicleHasBulletProofGlass );
		SCR_REGISTER_UNUSED( GET_VEHICLE_HAS_BULLET_RESISTANT_GLASS,0xfcc9c35259cacb4c, CommandGetVehicleHasBulletResistantGlass );

		SCR_REGISTER_SECURE( SET_VEHICLE_COMBAT_MODE,0x49ff67ce965a9ca0, CommandSetVehicleCombatMode );
		SCR_REGISTER_SECURE( SET_VEHICLE_DETONATION_MODE,0xa9dc91773c63c079, CommandSetVehicleDetonationMode ); 
		SCR_REGISTER_SECURE( SET_VEHICLE_SHUNT_ON_STICK,0x74dd43b047cfb3d6, CommandSetVehicleShuntOnStick );
		

		SCR_REGISTER_SECURE( GET_IS_VEHICLE_SHUNTING,0x43c1cd8b0eeee9a6, CommandGetIsVehicleShunting );
		SCR_REGISTER_SECURE( GET_HAS_VEHICLE_BEEN_HIT_BY_SHUNT,0xd75da3b776068a54, CommandHasVehicleBeenHitByShunt );
		SCR_REGISTER_SECURE( GET_LAST_SHUNT_VEHICLE,0xf8226abcdca5135e, CommandGetLastVehicleHitByShunt );
		SCR_REGISTER_UNUSED( GET_HAS_VEHICLE_BEEN_HIT_BY_WEAPON_BLADE,0x51fc9159dd5406b4, CommandHasVehicleBeenHitByWeaponBlade );
		SCR_REGISTER_SECURE( SET_DISABLE_VEHICLE_EXPLOSIONS_DAMAGE,0x623e6f86f7ea1f6f, CommandDisableCarExplosionsDamage );
		SCR_REGISTER_SECURE( SET_OVERRIDE_NITROUS_LEVEL,0xb5a0c4e6b09e2d4b, CommandSetOverrideNitrousLevel );
		SCR_REGISTER_UNUSED( FULLY_CHARGE_NITROUS,0x6ddbf5429c7f5c5e, CommandFullyChargeNitrous );
		SCR_REGISTER_UNUSED( CLEAR_NITROUS,0x2e45008db64c5ec6, CommandClearNitrous);
		SCR_REGISTER_UNUSED( SET_NITROUS_IS_ACTIVE,0x595e3eac7749eb53, CommandSetNitrousIsActive);
		SCR_REGISTER_SECURE( SET_INCREASE_WHEEL_CRUSH_DAMAGE,0x9d34acb83840c018, CommandSetIncreaseWheelCrushDamage );
		SCR_REGISTER_UNUSED( GET_IS_VEHICLE_REMOTE_CONTROL_CAR,0x29bd828f1d9818fb, CommandGetIsVehicleRemoteControlCar );
		SCR_REGISTER_SECURE( SET_DISABLE_WEAPON_BLADE_FORCES,0x034d9fe280c12eab, CommandSetDisableWeaponBladeForces );
		SCR_REGISTER_SECURE( SET_USE_DOUBLE_CLICK_FOR_CAR_JUMP,0xcc8d70b64262a044, CommandSetUseDoubleClickForCarJump );
		SCR_REGISTER_SECURE( GET_DOES_VEHICLE_HAVE_TOMBSTONE,0x45fc89e52779f5f9, CommandGetDoesVehicleHaveTombstone );		
        SCR_REGISTER_SECURE( HIDE_TOMBSTONE,0x688b84e91c8e1689, CommandHideTombstone );	
		SCR_REGISTER_UNUSED( APPLY_EMP_EFFECT,0x2f5af73c0a0f592d, CommandApplyEMPEffect );
		SCR_REGISTER_SECURE( GET_IS_VEHICLE_DISABLED_BY_EMP,0x0e532a4d2c420fec, CommandGetIsVehicleDisabledByEMP );
		SCR_REGISTER_UNUSED( APPLY_SLICK_EFFECT,0x356c5c43098434cc, CommandApplySlickEffect );
		SCR_REGISTER_UNUSED(GET_IS_VEHICLE_AFFECTED_BY_SLICK,0x0eede983da60f96c, CommandGetIsVehicleAffectedBySlick);
		SCR_REGISTER_UNUSED( APPLY_STEERING_BIAS,0xdc84574923a2878d, CommandApplySteeringBias );
		
		SCR_REGISTER_SECURE( SET_DISABLE_RETRACTING_WEAPON_BLADES,0xdd10f223864dc3b5, CommandSetDisableRetractingWeaponBlades );
        SCR_REGISTER_SECURE( GET_TYRE_HEALTH,0xba19dc35cdba15c8, CommandGetTyreHealth );
        SCR_REGISTER_SECURE( SET_TYRE_HEALTH,0x3173e2e58afffeab, CommandSetTyreHealth );
        SCR_REGISTER_SECURE( GET_TYRE_WEAR_RATE,0x53c70b6e9a5beae7, CommandGetTyreWearRate );
        SCR_REGISTER_SECURE( SET_TYRE_WEAR_RATE,0xf9370de1f7aff056, CommandSetTyreWearRate );
        SCR_REGISTER_SECURE( SET_TYRE_WEAR_RATE_SCALE,0xb4710f96caa92500, CommandSetTyreWearRateScale );
        SCR_REGISTER_SECURE( SET_TYRE_MAXIMUM_GRIP_DIFFERENCE_DUE_TO_WEAR_RATE,0xe7a2de5ed8c510f0, CommandMaxGripDifferenceDueToWearRate );
        SCR_REGISTER_UNUSED( RESET_DOWNFORCE,0x4344baa92193eb7e, CommandResetDownforceScales );
        SCR_REGISTER_UNUSED( MODIFY_DOWNFORCE,0x35c884f7a1651cf2, CommandModifyDownforce );

        SCR_REGISTER_UNUSED( TRIGGER_PUMPED_UP_JUMP,0x982a8921c2ea2874, CommandTiggerPumppedUpJump );
        SCR_REGISTER_SECURE( SET_AIRCRAFT_IGNORE_HIGHTMAP_OPTIMISATION,0x6399d7d66fe1c361, CommandSetAircraftIngoreHeightMapOptimisation );
       
        SCR_REGISTER_SECURE( SET_REDUCED_SUSPENSION_FORCE,0x321dc7de46bf01c3, CommandSetReducedSuspensionForce );
		SCR_REGISTER_UNUSED( GET_IS_REDUCED_SUSPENSION_FORCE_SET,0x4d3afacf60081506, CommandGetIsReducedSuspensionForceSet );

		SCR_REGISTER_SECURE( SET_DRIFT_TYRES,0x8c5dd298059d8bf8, CommandSetDriftTyres );
		SCR_REGISTER_SECURE( GET_DRIFT_TYRES_SET,0xbb34ca7bce1af9a1, CommandGetDriftTyresSet );

		SCR_REGISTER_SECURE(NETWORK_USE_HIGH_PRECISION_TRAIN_BLENDING,0xf3beb7beccdbf0b3, CommandNetworkUseHighPrecisionTrainBlending);

		SCR_REGISTER_UNUSED( SET_OVERRIDE_VEH_GROUP,0x8b0c0b9e68564483, CommandSetOverrideVehGroup );

		SCR_REGISTER_SECURE( SET_CHECK_FOR_ENOUGH_ROOM_FOR_PED,0xef9d388f8d377f44, CommandSetCheckForEnoughRoomToFitPed );
	}
}	//	end of namespace vehicle_commands

