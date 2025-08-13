#include "script/commands_ped.h"

// Rage headers
#include "crmetadata/properties.h"
#include "crmetadata/property.h"
#include "fragmentnm/messageparams.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "math/angmath.h"
#include "script/array.h"
#include "script/ScriptMetadata.h"
#include "script/ScriptMetadataManager.h"
#include "script/thread.h"
#include "script/wrapper.h"
#include "system/TimeMgr.h"
#include "fwmaths/angle.h"
#include "fwscene/world/WorldLimits.h"
#include "phbound/boundcomposite.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fragmentnm/nm_channel.h "

// Framework headers
#include "fwdecorator/decoratorExtension.h"
#include "fwnet/netobject.h"

// Game headers
#include "ai/BlockingBounds.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "animation/AnimBones.h"
#include "animation/Animdefines.h"
#include "animation/AnimManager.h"
#include "animation/FacialData.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "control/Gen9ExclusiveAssets.h"
#include "debug/MarketingTools.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "frontend/MiniMap.h"
#include "Game/Localisation.h"
#include "game/zones.h"
#include "ik/IkConfig.h"
#include "ik/IkManager.h"
#include "ik/IkRequest.h"
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/WeaponModelInfo.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkSynchronisedScene.h"
#include "network/Network.h"
#include "pedgroup/formation.h"
#include "pedgroup/pedgroup.h"
#include "Peds/Horse/horseComponent.h"
#include "peds/ped.h"
#include "peds/PedCapsule.h"
#include "peds/PedFactory.h"
#include "peds/PedFlagsMeta.h"
#include "peds/PedGeometryAnalyser.h"
#include "peds/PedHelmetComponent.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "peds/pedpopulation.h"
#include "peds/PedScriptResource.h"
#include "peds/RelationshipEnums.h"
#include "peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedDamage.h"
#include "peds/rendering/PedOverlay.h"
#include "peds/rendering/PedHeadshotManager.h"
#include "peds/PedWeapons/PedTargeting.h"
#include "peds/Ped.h"
#include "peds/ped_channel.h"
#include "peds/Popcycle.h"
#include "peds/popzones.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/RagdollConstraints.h"
#include "pickups/PickupManager.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "renderer/MeshBlendManager.h"
#include "scene/world/GameWorld.h"
#include "scene/EntityIterator.h"
#include "scene/Physical.h"
#include "script/commands_graphics.h"
#include "script/commands_task.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "script/script_itemsets.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/populationstreaming.h"
#include "task/Scenario/info/ScenarioActionCondition.h"
#include "task/Combat/TaskCombat.h"
#include "task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Helpers/PropManagementHelper.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBindPose.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMScriptControl.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Weapons/TaskBomb.h"
#include "vfx/Systems/VfxWeapon.h"
#include "weapons/Weapon.h"
#include "weapons/WeaponDamage.h"
#include "weapons/WeaponTypes.h"
#include "vehicleAi/driverpersonality.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "control/replay/ReplayExtensions.h"
#include "Task/Movement/Jumping/TaskJump.h"

// network includes
#include "network/arrays/NetworkArrayHandlerTypes.h"
#include "network/arrays/NetworkArrayMgr.h"
#include "network/NetworkInterface.h"
#include "Network/General/NetworkSynchronisedScene.h"

AI_MOTION_OPTIMISATIONS()
AI_SCENARIO_OPTIMISATIONS()
SCRIPT_OPTIMISATIONS()
PED_OPTIMISATIONS()

struct scrPedHeadBlendData
{
	scrValue m_head0;
	scrValue m_head1;
	scrValue m_head2;
	scrValue m_tex0;
	scrValue m_tex1;
	scrValue m_tex2;
	scrValue m_headBlend;
	scrValue m_texBlend;
	scrValue m_varBlend;
	scrValue m_isParent;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrPedHeadBlendData);

EXTERN_PARSER_ENUM(ePedConfigFlags);

namespace ped_commands
{
	int GetActualGroupId(int UniqueGroupID)
	{
		return CTheScripts::GetCurrentGtaScriptHandler()->GetResourceReferenceFromId(UniqueGroupID);
	}

	bool ThisIsAValidRandomPed(u32 PedType, s32 CivilianFlag, s32 GangFlag, s32 CriminalFlag)
	{
		switch (PedType)
		{
		case PEDTYPE_CIVMALE :
		case PEDTYPE_CIVFEMALE :
		case PEDTYPE_ANIMAL :
			if (CivilianFlag)
			{
				return true;
			}
			break;

		case PEDTYPE_GANG_ALBANIAN :
		case PEDTYPE_GANG_BIKER_1 :
		case PEDTYPE_GANG_BIKER_2 :
		case PEDTYPE_GANG_ITALIAN :
		case PEDTYPE_GANG_RUSSIAN :
		case PEDTYPE_GANG_RUSSIAN_2 :
		case PEDTYPE_GANG_IRISH :
		case PEDTYPE_GANG_JAMAICAN :
		case PEDTYPE_GANG_AFRICAN_AMERICAN :
		case PEDTYPE_GANG_KOREAN :
		case PEDTYPE_GANG_CHINESE_JAPANESE :
		case PEDTYPE_GANG_PUERTO_RICAN :
			if (GangFlag)
			{
				return true;
			}
			break;

		case PEDTYPE_CRIMINAL :
		case PEDTYPE_PROSTITUTE :
			if (CriminalFlag)
			{
				return true;
			}
			break;
		}

		return false;
	}

	int CommandCreatePed(int UNUSED_PARAM(PedType), int PedModelHashKey, const scrVector & scrVecCoors, float fHeading, bool bRegisterAsNetworkObject, bool bScriptHostObject)
	{
		CPed *pPed;
		Vector3 TempCoors = Vector3(scrVecCoors);
		int NewPedIndex = 0;
		Matrix34 TempMat;
		fwModelId PedModelId;
		float fSetHeading;

		// non-networked peds should always be treated as client objects
		if (!bRegisterAsNetworkObject) 
			bScriptHostObject = false;

		if (SCRIPT_VERIFY( ABS(TempCoors.x) < WORLDLIMITS_XMAX && ABS(TempCoors.y) < WORLDLIMITS_YMAX, "CREATE_PED Position out of range!" ) &&
			SCRIPT_VERIFY( !CNetwork::IsNetworkOpen() || !bRegisterAsNetworkObject || CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "CREATE_PED - Non-networked scripts that are safe to run during the network game cannot create networked entities"))
		{
			if (bScriptHostObject && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
			{
				if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_PED - Non-host machines cannot create host peds"))
				{
					return NewPedIndex;
				}
			}

			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) PedModelHashKey, &PedModelId);		//	ignores return value
			
			if (SCRIPT_VERIFY(pModelInfo, "CREATE_PED - this is not a valid model index"))
			{
				if (SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_PED, "CREATE_PED - not a ped model"))
				{
					if (SCRIPT_VERIFY(pModelInfo->GetDrawable() || pModelInfo->GetFragType(), "CREATE_PED - Ped model is not loaded"))
					{
						CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pModelInfo);

						// want to pass matrix into PedFactory
						if (TempCoors.z <= INVALID_MINIMUM_WORLD_Z)
						{
							TempCoors.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, TempCoors.x, TempCoors.y);
						}

						const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(pPedModelInfo->GetPedCapsuleHash().GetHash());
						SCRIPT_ASSERT(pCapsuleInfo, "CREATE_PED - Missing capsule info");
						TempCoors.z += pCapsuleInfo->GetGroundToRootOffset();

						fSetHeading = ( DtoR * fHeading);
						fSetHeading = fwAngle::LimitRadianAngleSafe(fSetHeading);

						TempMat.Identity();
						TempMat.RotateFullZ(fSetHeading);
						TempMat.d = TempCoors;

#if !__NO_OUTPUT
						bool bNonNetworkedDueToCutscene = false;
#endif // !__NO_OUTPUT
						// don't network peds created during a cutscene
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

						// use pedfactory to create the ped:
						const CControlledByInfo localAiControl(false, false);
						pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, PedModelId, &TempMat, true, bRegisterAsNetworkObject, true);

						if (SCRIPT_VERIFY(pPed, "CREATE_PED - Couldn't create a new ped"))
						{
                            if (!pPed->GetPedModelInfo()->GetIsStreamedGfx())
								pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);

							pPed->SetDesiredHeading( fSetHeading );
							pPed->SetHeading( fSetHeading );
							pPed->SetLodDistance(CPed::ms_uMissionPedLodDistance);

							CScriptEntities::ClearSpaceForMissionEntity(pPed);
							pPed->ActivatePhysics();

							CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

							pPed->GetPortalTracker()->RequestRescanNextUpdate();
							pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

							NewPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);

							CTheScripts::RegisterEntity(pPed, bScriptHostObject, bRegisterAsNetworkObject);

#if !__NO_OUTPUT
							if(NetworkInterface::IsGameInProgress() && !bRegisterAsNetworkObject)
							{
								scriptDebugf1("%s: Creating non-networked ped %p (None networked due to cutscene: %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed, bNonNetworkedDueToCutscene ? "TRUE" : "FALSE");
								NOTFINAL_ONLY(scrThread::PrePrintStackTrace();)
							}
#endif // !__NO_OUTPUT
						}
					}
				}
			}
		}
		return NewPedIndex;
	}


	int CommandClonePedAlt(int PedIndex, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bLinkBlends, bool bCloneCompressedDamage)
	{
		int NewPedIndex = 0;

		// don't network peds created during a cutscene
		if (!NetworkInterface::AreCutsceneEntitiesNetworked() && CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene())
			bRegisterAsNetworkObject = false;

		const CPed* ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		CPed* newPed = CPedFactory::GetFactory()->ClonePed(ped, bRegisterAsNetworkObject, bLinkBlends, bCloneCompressedDamage);

		Displayf("[HEAD BLEND] Cloned ped %p to %p %s", ped, newPed, bLinkBlends ? "(linked)" : "");

		if (newPed)
		{
			CScriptEntities::ClearSpaceForMissionEntity(newPed);
			NewPedIndex = CTheScripts::GetGUIDFromEntity(*newPed);
			CTheScripts::RegisterEntity(newPed, bScriptHostObject, bRegisterAsNetworkObject);

#if !__FINAL
			scriptDebugf1("CLONE_PED(%d): Dumping cloned ped variation (ID %d):", PedIndex, NewPedIndex);
			for(int iComponent=0; iComponent < PV_MAX_COMP; iComponent++)
			{
				u32 iDrawableId = CPedVariationPack::GetCompVar(newPed, static_cast<ePedVarComp>(iComponent));
				u8 TextureVarId = CPedVariationPack::GetTexVar(newPed, static_cast<ePedVarComp>(iComponent));
				u8 Palette = CPedVariationPack::GetPaletteVar(newPed, static_cast<ePedVarComp>(iComponent));
				scriptDebugf1("Component %d: %s_%03d_%c (Palette %d)", iComponent, varSlotNames[iComponent], iDrawableId, 'a' + TextureVarId, Palette);
			}
#endif // !__FINAL
		}

		return NewPedIndex;
	}

	int CommandClonePed(int PedIndex, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bLinkBlends)
	{
		return CommandClonePedAlt(PedIndex, bRegisterAsNetworkObject, bScriptHostObject, bLinkBlends, true);
	}

	void CommandClonePedToTargetAlt(int PedIndexCloneSource, int PedIndexCloneTarget, bool bCloneCompressedDamage)
	{
		const CPed* source = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndexCloneSource, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		CPed* target = const_cast<CPed*>(CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndexCloneTarget, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK));

		Displayf("[HEAD BLEND] Cloned ped %p to target %p", source, target);
#if !__FINAL
		if(NetworkInterface::IsGameInProgress() && source->IsPlayer() && source->GetNetworkObject())
		{
			scriptDebugf1("[HEAD BLEND] Cloned Network player %s: %s [ %p ]", source->GetDebugName(),source->IsNetworkClone()?"CLONE":"LOCAL", source);
			scrThread::PrePrintStackTrace();
		}
#endif
		if (scriptVerifyf(source, "Source ped couldn't be found!"))
		{
			if (scriptVerifyf(target, "Target ped couldn't be found!"))
			{
				CPedFactory::GetFactory()->ClonePedToTarget(source, target, bCloneCompressedDamage);
				target->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
				target->SetHairScaleLerp(false);

#if GTA_REPLAY
				ReplayPedExtension::ClonePedDecorationData(source, target);
#endif // GTA_REPLAY

#if !__FINAL
				scriptDebugf1("CLONE_PED_TO_TARGET(%d, %d): Dumping cloned ped variation:", PedIndexCloneSource, PedIndexCloneTarget);
				for(int iComponent=0; iComponent < PV_MAX_COMP; iComponent++)
				{
					u32 iDrawableId = CPedVariationPack::GetCompVar(target, static_cast<ePedVarComp>(iComponent));
					u8 TextureVarId = CPedVariationPack::GetTexVar(target, static_cast<ePedVarComp>(iComponent));
					u8 Palette = CPedVariationPack::GetPaletteVar(target, static_cast<ePedVarComp>(iComponent));
					scriptDebugf1("Component %d: %s_%03d_%c (Palette %d)", iComponent, varSlotNames[iComponent], iDrawableId, 'a' + TextureVarId, Palette);
				}
#endif // !__FINAL
			}
		}
	}

	void CommandClonePedToTarget(int PedIndexCloneSource, int PedIndexCloneTarget)
	{
		CommandClonePedToTargetAlt(PedIndexCloneSource, PedIndexCloneTarget, true);
	}

	void DeleteScriptPed(CPed *pPed)
	{
		if (scriptVerifyf(pPed, "DeleteScriptPed - ped doesn't exist"))
		{
			CPedFactory::GetFactory()->DestroyPed(pPed);
		}
	}

	void CommandDeletePed(int &PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

		if (pPed)
		{
			CScriptEntityExtension* pExtension = pPed->GetExtension<CScriptEntityExtension>();

			if (scriptVerifyf(pExtension, "%s: DELETE_PED - The ped is not a script entity (no extension, pop type %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CTheScripts::GetPopTypeName(pPed->PopTypeGet()) ))
			{
				if (scriptVerifyf(pExtension->GetScriptHandler(), "%s: DELETE_PED - The script ped (pop type %s) has a script extension but that extension doesn't have a script handler", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CTheScripts::GetPopTypeName(pPed->PopTypeGet()) ))
				{
					if (scriptVerifyf(pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler(), "%s: DELETE_PED - The ped (pop type %s) was not created by this script. It was created by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CTheScripts::GetPopTypeName(pPed->PopTypeGet()), pExtension->GetScriptHandler()->GetLogName()))
					{
						DeleteScriptPed(pPed);
					}
				}
			}
		}

		PedIndex = 0;
	}

	bool CommandIsPedInVehicle(int PedIndex, int VehicleIndex, bool ConsiderEnterExitAsInVehicle)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			const CVehicle *pVehicle= CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
			
			if (pVehicle)
			{
				if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					if 	(pVehicle == pPed->GetMyVehicle())
					{
						LatestCmpFlagResult = true;
					}
				}

				if ( ConsiderEnterExitAsInVehicle )
				{
					if ( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
					{
						const s32 iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
						if (iState >= CTaskEnterVehicle::State_EnterSeat || iState == CTaskEnterVehicle::State_ClimbUp)
						{
							CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX);
							aiAssert(dynamic_cast<CClonedEnterVehicleInfo*>(pTaskInfo));
							CClonedEnterVehicleInfo* pVehInfo = static_cast<CClonedEnterVehicleInfo*>(pTaskInfo);
							if (pVehicle == pVehInfo->GetVehicle())
							{
								LatestCmpFlagResult = true;
							}
						}
					}
					else if ( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) )
					{
						const s32 iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);
						if (iState <= CTaskExitVehicle::State_ExitSeat)
						{
							CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_EXIT_VEHICLE, PED_TASK_PRIORITY_MAX);
							aiAssert(dynamic_cast<CClonedExitVehicleInfo*>(pTaskInfo));
							CClonedExitVehicleInfo* pVehInfo = static_cast<CClonedExitVehicleInfo*>(pTaskInfo);
							if (pVehicle == pVehInfo->GetVehicle())
							{
								LatestCmpFlagResult = true;
							}
						}
					}
				}
			}
		}
		return (LatestCmpFlagResult);
	}


	bool CommandIsPedInModel(int PedIndex, int VehicleModelHashKey)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
            scriptAssertf(!NetworkInterface::IsGameInProgress() || !NetworkInterface::IsRemotePlayerInNonClonedVehicle(pPed), "%s:IS_PED_IN_MODEL - Calling this function for a remote player in a vehicle that has not been cloned on this machine!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if (SCRIPT_VERIFY(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "IS_PED_IN_MODEL - Ped is not in a vehicle"))
			{
				fwModelId VehicleModelId;
				CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
				if (SCRIPT_VERIFY(VehicleModelId.IsValid(), "IS_PED_IN_MODEL - this is not a valid model index"))
				{
					CVehicle *pVehicle = pPed->GetMyVehicle();

					if (pVehicle && (pVehicle->GetModelIndex() == VehicleModelId.GetModelIndex()))
					{
						LatestCmpFlagResult = TRUE;
					}
				}
			}
		}
		return(LatestCmpFlagResult);
	}

	bool CommandIsPedInAnyVehicle(int PedIndex, bool ConsiderEnteringAsInVehicle)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			if ( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()) )
			{
				LatestCmpFlagResult = true;
			}

			if ( ConsiderEnteringAsInVehicle )
			{
				if ( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
				{
					const s32 iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
					if (iState >= CTaskEnterVehicle::State_EnterSeat || iState == CTaskEnterVehicle::State_ClimbUp)
					{
						LatestCmpFlagResult = true;
					}
				}
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandClearCoverPoint(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFYF(pPed, "Can't find a PED with this index %i", iPedIndex))
		{
			if (pPed->GetCoverPoint())
			{
				pPed->ReleaseCoverPoint();
			}
		}
	}

	void CommandSetPedVehicleForcedSeatUsage(int iPedIndex, int iVehicleIndex, int iSlot, int iFlags, int DestNumber = -2)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (!pPed)
			return;

		if (!SCRIPT_VERIFYF(iSlot >= 0 && iSlot < CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS, "Slot %i is invalid, should be within range [0,%i]", iSlot, CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS-1))
			return;

		CVehicle* pVehicle = iVehicleIndex == NULL_IN_SCRIPTING_LANGUAGE ? NULL : CTheScripts::GetEntityToModifyFromGUID<CVehicle>(iVehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		const s32 iSeatIndex = DestNumber + 1;
		AI_LOG_WITH_ARGS("Script %s calling SetPedVehicleForcedSeatUsage, PedIndex [%i], Vehicle [%s] with index [%i], iSlot [%i], iFlags [%i], DestNumber [%i], SeatIndex [%i]", CTheScripts::GetCurrentScriptName(), iPedIndex, AILogging::GetDynamicEntityNameSafe(pVehicle), iVehicleIndex, iSlot, iFlags, DestNumber, iSeatIndex);
		scriptAssertf(iSeatIndex >= -1, "DestNumber %i is invalid, should be >= -2 to generate a valid vehicle seat index", DestNumber);

#if __BANK
		if (const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig())
		{
			if (iSeatIndex >= 0 && pVehicleEntryConfig->GetData(iSlot).HasAForcingFlagSet())
			{
				AI_LOG_WITH_ARGS("Script %s is setting conflicting forced seat usage to vehicle %s, due to both seat index [%i] and flags [%i] being set", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pVehicle), iSeatIndex, iFlags);
			}

			CSyncedPedVehicleEntryScriptConfigData temp((u8)iFlags, pVehicle, iSeatIndex);
			if (temp != pVehicleEntryConfig->GetData(iSlot))
			{
				AI_LOG_WITH_ARGS("Script %s is setting forced vehicle seat usage to vehicle %s with index %i on ped %s, flags %i, slot %i, seatindex %i\n", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pVehicle), iVehicleIndex, AILogging::GetDynamicEntityNameSafe(pPed), iFlags, iSlot, iSeatIndex);
			}
		}
#endif // __BANK

		// If no vehicle specified and a flag has been passed in, set the for any vehicle flag
		if (!pVehicle && (iFlags != 0))
		{
			iFlags |= CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle;
		}

		AI_LOG_WITH_ARGS("Script %s setting forced seat usage for ped %s, vehicle %s, iSlot [%i], iFlags [%i], DestNumber [%i]", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pVehicle), iSlot, iFlags, DestNumber);			
		pPed->SetForcedSeatUsage(iSlot, iFlags, pVehicle, iSeatIndex);

		if (pPed->GetVehicleEntryConfig() && pPed->GetVehicleEntryConfig()->HasDefaultState())
		{
			AI_LOG_WITH_ARGS("Script %s deleting vehicle entry config for ped %s due to default state vehicle %s, iSlot [%i], iFlags [%i], DestNumber [%i]", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pVehicle), iSlot, iFlags, DestNumber);
			pPed->DeleteVehicleEntryConfig();
		}

#if __BANK
		if (const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig())
		{
			if (!pVehicleEntryConfig->ValidateData())
			{
				pVehicleEntryConfig->SpewDataToTTY();
				scriptAssertf(0, "Script %s has setup ped %s with conflicting data, it has been spewed to the tty", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pPed));
			}
		}
#endif // __BANK
	}

	void CommandClearAllPedVehicleForcedSeatUsage(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (!pPed)
			return;

		if (!pPed->GetVehicleEntryConfig())
			return;

		AI_LOG_WITH_ARGS("Script %s has cleared forced vehicle seat usage on ped %s", CTheScripts::GetCurrentScriptName(), AILogging::GetDynamicEntityNameSafe(pPed));
		pPed->ClearForcedSeatUsage();
	}

	int CommandGetVehicleAndFlagsForPedsForcedSeatUsage(int iPedIndex, int iSlot, int& iFlags)
	{
		iFlags = 0;

		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (!pPed)
			return NULL_IN_SCRIPTING_LANGUAGE;

		if (!SCRIPT_VERIFYF(iSlot >= 0 && iSlot < CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS, "Slot %i is invalid, should be within range [0,%i]", iSlot, CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS-1))
			return NULL_IN_SCRIPTING_LANGUAGE;
		
		const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig();
		if (!pVehicleEntryConfig)
			return NULL_IN_SCRIPTING_LANGUAGE;

		const CVehicle* pVeh = pVehicleEntryConfig->GetVehicle(iSlot);
		if (!pVeh)
			return NULL_IN_SCRIPTING_LANGUAGE;

		const int iVehIndex = CTheScripts::GetGUIDFromEntity(*const_cast<CVehicle*>(pVeh));
		iFlags = pVehicleEntryConfig->GetFlags(iSlot);
		return iVehIndex;
	}
	
	void CommandForcePedToUseSpecifiedSeats(int UNUSED_PARAM(iPedIndex), int UNUSED_PARAM(iVehicleIndex), bool UNUSED_PARAM(bForcePedToUseSeats))
	{
		scriptAssertf(0, "Script : (%s) FORCE_PED_TO_USE_SPECIFIED_SEATS has been deprecated, please switch over to using SET_PED_VEHICLE_FORCED_SEAT_USAGE", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	bool CommandIsPedReloading(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, 0);
		if (pPed)
		{
			return pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading);
		}

		return false;
	}

	bool CommandIsPedThrowingGrenadeWhileAimingGun(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, 0);
		if (pPed)
		{
			return pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming);
		}

		return false;
	}

	bool CommandIsPedAimingFromCover(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, 0);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
			{
				if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_AimIntro ||
					pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_Aim)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool CommandIsPedAPlayer(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			if ( pPed->IsAPlayerPed() )
			{
				LatestCmpFlagResult = true;
			}
		}

		return LatestCmpFlagResult;
	}

	//	Is the char injuerd and playing the injured anim on the ground
	bool CommandIsPedInjured(int PedIndex)
	{
		bool bReturnValue = false;
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPed>(PedIndex, 0);
		if (pPed)
		{
			if (pPed->IsInjured())
			{
				bReturnValue = true;
			}
			else
			{
				bReturnValue = false;
#if __DEV
				pPed->m_nDEflags.bCheckedForDead = true;
#endif
			}
		}
		else
		{
			bReturnValue = true;
		}

		return bReturnValue;
	}

	//	Is the char hurt and playing the injured anim on the ground
	bool CommandIsPedHurt(int PedIndex)
	{
		bool bReturnValue = false;
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPed>(PedIndex, 0);
		if (pPed)
		{
			if (pPed->HasHurtStarted())
			{
				bReturnValue = true;
			}
			else
			{
				bReturnValue = false;
#if __DEV
				pPed->m_nDEflags.bCheckedForDead = true;
#endif
			}
		}
		else
		{
			bReturnValue = true;
		}

		return bReturnValue;
	}

	//	Is the char fatally injured and playing the injured anim on the ground
	bool CommandIsPedFatallyInjured(int PedIndex)
	{
		bool bReturnValue = false;
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPed>(PedIndex, 0);

		if (pPed)
		{
			if (pPed->IsFatallyInjured())
			{
				bReturnValue = true;
			}
			else
			{
				bReturnValue = false;
#if __DEV
				pPed->m_nDEflags.bCheckedForDead = true;
#endif
			}
		}
		else
		{
			bReturnValue = true;
		}

		return bReturnValue;
	}

	//	Is the char dead
	bool CommandIsPedDeadOrDying(int PedIndex, bool CheckMeleeDeathFlags)
	{
		bool bReturnValue = false;
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID_NoTracking<CPed>(PedIndex, 0);

		if (pPed)
		{
			if (pPed->GetIsDeadOrDying() || (CheckMeleeDeathFlags && pPed->IsDeadByMelee()))
			{
				bReturnValue = true;
			}
			else
			{
				bReturnValue = false;
#if __DEV
				pPed->m_nDEflags.bCheckedForDead = true;
#endif
			}
		}
		else
		{
			bReturnValue = true;
		}

		return bReturnValue;
	}

	//	Dead check that matches what audio conversation system checks (to avoid asserts)
	bool CommandIsConversationPedDead(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			return pPed->IsDead();
		}

		return false;
	}

	void CommandSetPedDesiredHeading(int PedIndex, float fDesiredHeading)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (SCRIPT_VERIFY(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "SET_PED_DESIRED_HEADING - Ped cannot be in a vehicle"))
			{
				float fDesiredHeadingRad = fwAngle::LimitRadianAngleSafe(fDesiredHeading * DtoR);

				pPed->SetDesiredHeading( fDesiredHeadingRad );
			}
		}
	}

	void CommandSetPedHeadingAndPitch(int PedIndex, float fHeadingDeg, float fPitchDeg)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			if (SCRIPT_VERIFY(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "SET_PED_HEADING_AND_PITCH - Ped cannot be in a vehicle"))
			{
				float fHeadingRad = fwAngle::LimitRadianAngleSafe(DtoR * fHeadingDeg);
				float fPitchRad = fwAngle::LimitRadianAngleForPitchSafe(DtoR * fPitchDeg);

				pPed->SetHeadingAndPitch(fHeadingRad,fPitchRad);
			}
		}
	}

	void CommandSetPedPitch(int PedIndex, float fPitchDeg)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			if (SCRIPT_VERIFY(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "SET_PED_HEADING_AND_PITCH - Ped cannot be in a vehicle"))
			{
				// Check the heading and pitch
				Assertf(fPitchDeg >= -180.0f && fPitchDeg <= 180.0f,"SET_PED_PITCH Invalid heading %f must be in range -180 to +180",fPitchDeg);

				float fPitchRad = fwAngle::LimitRadianAngleForPitch(DtoR * fPitchDeg);

				pPed->SetPitch(fPitchRad);
			}
		}
	}

	void CommandForceAllHeadingValuesToAlign(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (SCRIPT_VERIFY(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "FORCE_ALL_HEADING_VALUES_TO_ALIGN - Ped cannot be in a vehicle"))
			{
				pPed->SetHeading(pPed->GetTransform().GetHeading());
				pPed->SetDesiredHeading(pPed->GetTransform().GetHeading());

				if(pPed->GetMotionData())
				{
					pPed->GetMotionData()->SetCurrentHeading(pPed->GetTransform().GetHeading());
					pPed->GetMotionData()->SetDesiredHeading(pPed->GetTransform().GetHeading());
				}
			}
		}
	}

	void CommandSetPedMinGroundTimeForStunGun(int PedIndex, int minTime)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			pPed->SetMinOnGroundTimeForStunGun(minTime);
		}
	}

	bool CommandIsPedFacingPed(int PedIndex1, int PedIndex2, float Degrees)
	{
		const CPed *pPed1 = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex1);
		const CPed *pPed2 = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex2);
		
		if (pPed1 && pPed2)
		{
			const float fRadians = ( DtoR * Degrees);
			const float fHeading = fwAngle::LimitRadianAngle(pPed1->GetCurrentHeading());
			const Vector3 vPed1Position = VEC3V_TO_VECTOR3(pPed1->GetTransform().GetPosition());
			const Vector3 vPed2Position = VEC3V_TO_VECTOR3(pPed2->GetTransform().GetPosition());
			float fHeading1To2 = fwAngle::GetRadianAngleBetweenPoints(vPed2Position.x, vPed2Position.y, vPed1Position.x, vPed1Position.y);
			fHeading1To2 = fwAngle::LimitRadianAngle(fHeading1To2);

			const float fDelta = SubtractAngleShorter(fHeading, fHeading1To2);
			return (Abs(fDelta) <= fRadians);
		}
		return false;
	}

	bool CommandIsPedInMeleeCombat(int PedIndex)
	{
		bool bIsPedInMeleeCombat = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence())
			{
				CTaskMelee* pTaskComplexMelee = pPed->GetPedIntelligence()->GetTaskMelee();
				if(pTaskComplexMelee)
				{
					bIsPedInMeleeCombat = true;
				}
			}
		}

		return bIsPedInMeleeCombat;
	}

	bool CommandIsPedDoingGunDisArm(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence())
			{
				CTaskMelee* pTaskMelee = pPed->GetPedIntelligence()->GetTaskMelee();
				if (pTaskMelee)
				{
					return pTaskMelee->IsPedDoingGunDisArm();
				}
			}
		}

		return false;
	}

	int CommandCreatePedInsideVehicle(int VehicleIndex, int UNUSED_PARAM(PedType), int PedModelHashKey, int PassengerNumber, bool bRegisterAsNetworkObject, bool bScriptHostObject)
	{
		CPed *pPed;
		int NewPedIndex = 0;

		s32 seat = PassengerNumber+1;
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		fwModelId PedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) PedModelHashKey, &PedModelId);		//	ignores return value
		if (SCRIPT_VERIFY(PedModelId.IsValid(), "CREATE_PED_INSIDE_VEHICLE - this is not a valid model index")) 
		{
			if (pVehicle)
			{
				if (seat >= 0)
				{
					if (!SCRIPT_VERIFYF(seat < pVehicle->GetSeatManager()->GetMaxSeats(), "CREATE_PED_INSIDE_VEHICLE - Specified seat (%i) is larger than the number of seats in car model %s (%i)", seat, pVehicle->GetModelName(), pVehicle->GetSeatManager()->GetMaxSeats() ))
						return NewPedIndex; 	
				
					if (!SCRIPT_VERIFYF(!pVehicle->GetSeatManager()->GetPedInSeat(seat), "CREATE_PED_INSIDE_VEHICLE -  Specified seat (%i) is already occupied", seat))
						return NewPedIndex;
				}
			}
			else
			{
				return NewPedIndex; 
			}
		}
		else
		{
			return NewPedIndex; 
		}
		
		CBaseModelInfo* pBMI = CModelInfo::GetBaseModelInfo(PedModelId);
		if (!SCRIPT_VERIFY(pBMI, "CREATE_PED_INSIDE_VEHICLE - Ped doesn't exist"))
			return NewPedIndex;

		if (!SCRIPT_VERIFY(pBMI->GetHasLoaded(), "CREATE_PED_INSIDE_VEHICLE - Ped to go into vehicle has not loaded"))
			return NewPedIndex;

		if (!SCRIPT_VERIFY( !CNetwork::IsNetworkOpen() || !bRegisterAsNetworkObject || CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "CREATE_PED - Non-networked scripts that are safe to run during the network game cannot create networked entities"))
			return NewPedIndex;

		// don't network peds created during a cutscene
		if (!NetworkInterface::AreCutsceneEntitiesNetworked() && CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene())
			bRegisterAsNetworkObject = false;

		// use pedfactory to create the ped:
		const CControlledByInfo localAiControl(false, false);
		Matrix34 m = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
		pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, PedModelId, &m, true, bRegisterAsNetworkObject, true);

		if (SCRIPT_VERIFY (pPed,"CREATE_PED_INSIDE_VEHICLE - Couldn't create a new ped"))
		{
			if (!pPed->GetPedModelInfo()->GetIsStreamedGfx())
				pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
			CGameWorld::Add(pPed, CGameWorld::OUTSIDE );		

			NewPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);

			CTheScripts::RegisterEntity(pPed, bScriptHostObject, bRegisterAsNetworkObject);

			{
#if __ASSERT
				if(seat!=-1)	
				{
					scriptAssertf((seat < pVehicle->GetSeatManager()->GetMaxSeats()), "%s:CREATE_PED_INSIDE_VEHICLE - Seat number is too large", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					scriptAssertf(!pVehicle->GetSeatManager()->GetPedInSeat(seat), "%s:CREATE_PED_INSIDE_VEHICLE - Vehicle already has a passenger in the specified seat", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
				else
				{
					scriptAssertf( (pVehicle->GetSeatManager()->CountPedsInSeats() < pVehicle->GetSeatManager()->GetMaxSeats()), "%s:CREATE_PED_INSIDE_VEHICLE - Vehicle has no free passenger seats", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
#endif // __ASSERT

				if (!scriptVerifyf(pVehicle->IsSeatIndexValid(seat), "CREATE_PED_INSIDE_VEHICLE - specified seat (%i) is not valid on this car model", PassengerNumber))
					return NewPedIndex; 	

				pPed->SetPedInVehicle(pVehicle, seat, CPed::PVF_IfForcedTestVehConversionCollision|CPed::PVF_Warp);
			}

		}
		return NewPedIndex;
	}

	void CommandSetPedMovementClipSet ( int PedIndex, const char* szClipSetName, float fBlendDuration )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_MOVEMENT_CLIPSET - couldn't find clip set"))
			{
				if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_MOVEMENT_CLIPSET - clips set is not loaded yet:"))
				{
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
					
					if (SCRIPT_VERIFY(pTask, "SET_PED_MOVEMENT_CLIPSET - Ped motion task is not running."))
					{
						pTask->SetOnFootClipSet(clipSet, fBlendDuration);
					}
				}
			}
		}
	}

	void CommandResetPedMovementClipSet ( int PedIndex, float fBlendDuration )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPrimaryMotionTask())
		{
			pPed->GetPrimaryMotionTask()->ResetOnFootClipSet(true, fBlendDuration);
		}
	}

	void CommandSetPedStrafeClipSet ( int PedIndex, const char* szClipSetName )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_STRAFE_CLIPSET - couldn't find clip set"))
			{
				if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_STRAFE_CLIPSET - Clip set is not loaded yet:"))
				{
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

					if (SCRIPT_VERIFY(pTask, "SET_PED_STRAFE_CLIPSET - Ped motion task is not running."))
					{
						pTask->SetOverrideStrafingClipSet(clipSet);
					}
				}
			}
		}
	}

	void CommandResetPedStrafeClipSet ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPrimaryMotionTask())
		{
			pPed->GetPrimaryMotionTask()->ResetStrafeClipSet();
		}
	}

	//void CommandSetPedInjuredStrafeClipSet ( int PedIndex, const char* szClipSetName )
	//{
	//	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	//	if (pPed)
	//	{
	//		fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

	//		//look for an clip set with this name
	//		if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
	//		{
	//			clipSet.SetFromString(szClipSetName);
	//		}

	//		if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_INJURED_STRAFE_CLIPSET - couldn't find clip set"))
	//		{
	//			if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_INJURED_STRAFE_CLIPSET - Clip set is not loaded yet:"))
	//			{
	//				CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

	//				if (SCRIPT_VERIFY(pTask, "SET_PED_INJURED_STRAFE_CLIPSET - Ped motion task is not running."))
	//				{
	//					pTask->SetDefaultInjuredStrafingClipSet(clipSet);
	//				}
	//			}
	//		}
	//	}
	//}

	void CommandSetPedWeaponMovementClipSet ( int PedIndex, const char* szClipSetName )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_WEAPON_MOVEMENT_CLIPSET - couldn't find clip set"))
			{
				if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_WEAPON_MOVEMENT_CLIPSET - Clip set is not loaded yet:"))
				{
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

					if (SCRIPT_VERIFY(pTask!=NULL, "SET_PED_WEAPON_MOVEMENT_CLIPSET - Ped motion task is not running."))
					{
						pTask->SetOverrideWeaponClipSet(clipSet);
					}
				}
			}
		}
	}

	void CommandResetPedWeaponMovementClipSet ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPrimaryMotionTask())
		{
			pPed->GetPrimaryMotionTask()->ClearOverrideWeaponClipSet();
		}
	}

	void CommandSetPedDriveByClipSetOverride ( int PedIndex, const char* szClipSetName )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_DRIVE_BY_CLIPSET_OVERRIDE - couldn't find clip set"))
			{
				if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_DRIVE_BY_CLIPSET_OVERRIDE - Clip set is not loaded yet:"))
				{
					CPedMotionData* pPedMotionData = pPed->GetMotionData();

					if (SCRIPT_VERIFY(pPedMotionData!=NULL, "SET_PED_DRIVE_BY_CLIPSET_OVERRIDE - Ped has no motiondata."))
					{
						pPedMotionData->SetOverrideDriveByClipSet(clipSet);
					}
				}
			}
		}
	}
	void CommandClearPedDriveByClipSetOverride ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CPedMotionData* pPedMotionData = pPed->GetMotionData();
			if (SCRIPT_VERIFY(pPedMotionData!=NULL, "CLEAR_PED_DRIVE_BY_CLIPSET_OVERRIDE - Ped has no motiondata."))
			{
				pPedMotionData->ResetOverrideDriveByClipSet();
			}
		}
	}

	void CommandSetPedMotionInCoverClipSetOverride ( int PedIndex, const char* szClipSetName )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE - couldn't find clip set"))
			{
				if (SCRIPT_VERIFYF(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE - Clip set %s is not loaded yet:", clipSet.GetCStr()))
				{
					CPedMotionData* pPedMotionData = pPed->GetMotionData();

					if (SCRIPT_VERIFY(pPedMotionData!=NULL, "SET_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE - Ped has no motiondata."))
					{
						pPedMotionData->SetOverrideMotionInCoverClipSet(clipSet);
					}
				}
			}
		}
	}

	void CommandClearPedMotionInCoverClipSetOverride ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			CPedMotionData* pPedMotionData = pPed->GetMotionData();
			if (SCRIPT_VERIFY(pPedMotionData!=NULL, "CLEAR_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE - Ped has no motiondata."))
			{
				pPedMotionData->ResetOverrideMotionInCoverClipSet();
			}
		}
	}

	void CommandSetPedFallUpperBodyClipSetOverride ( int PedIndex, const char* szClipSetName )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;

			//look for an clip set with this name
			if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipSetName)))
			{
				clipSet.SetFromString(szClipSetName);
			}

			if (SCRIPT_VERIFY(clipSet.GetHash(), "SET_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE - couldn't find clip set"))
			{
				if (SCRIPT_VERIFY(fwClipSetManager::IsStreamedIn_DEPRECATED(clipSet), "SET_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE - Clip set is not loaded yet:"))
				{
					CPedMotionData* pPedMotionData = pPed->GetMotionData();

					if (SCRIPT_VERIFY(pPedMotionData!=NULL, "SET_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE - Ped has no motiondata."))
					{
						pPedMotionData->SetOverrideFallUpperBodyClipSet(clipSet);
					}
				}
			}
		}
	}

	void CommandClearPedFallUpperBodyClipSetOverride ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CPedMotionData* pPedMotionData = pPed->GetMotionData();
			if (SCRIPT_VERIFY(pPedMotionData!=NULL, "CLEAR_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE - Ped has no motiondata."))
			{
				pPedMotionData->ResetOverrideFallUpperBodyClipSet();
			}
		}
	}

	void CommandSetPedInVehicleContext ( int PedIndex, int OverrideHash )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			const CInVehicleOverrideInfo* pOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(OverrideHash);
			if (scriptVerifyf(pOverrideInfo, "SET_PED_IN_VEHICLE_CONTEXT - Couldn't find context for hash %u", OverrideHash))
			{
				CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

				if (SCRIPT_VERIFY(pTask, "SET_PED_IN_VEHICLE_CONTEXT - Ped motion task is not running."))
				{
					pTask->SetInVehicleContextHash((u32)OverrideHash, true);
				}
			}
		}
	}

	void CommandResetPedInVehicleContext ( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPrimaryMotionTask())
		{
			pPed->GetPrimaryMotionTask()->ResetInVehicleContextHash(true);
		}
	}

	// START OF DEPRECATED
	void CommandSetPedAnimGroup ( int, const char* )
	{
		SCRIPT_ASSERT(0, "SET_PED_ANIM_GROUP - This script command is no longer supported. Please remove it from your script. See SET_PED_MOVEMENT_CLIPSET to change the movement animations.");
	}
	void CommandResetPedAnimGroup ( int )
	{
		SCRIPT_ASSERT(0, "RESET_PED_ANIM_GROUP - This script command is no longer supported. Please remove it from your script. See RESET_PED_MOVEMENT_CLIPSET to change the movement animations.");
	}
	void CommandSetPedAnimGroupTwoHanded ( int, const char* )
	{
		SCRIPT_ASSERT(0, "SET_PED_ANIM_GROUP_TWO_HANDED - This script command is no longer supported. Please remove it from your script. See SET_PED_WEAPON_MOVEMENT_CLIPSET to change the weapon carrying movement animations.");
	}
	void CommandResetPedAnimGroupTwoHanded ( int )
	{
		SCRIPT_ASSERT(0, "RESET_PED_ANIM_GROUP_TWO_HANDED - This script command is no longer supported. Please remove it from your script. See RESET_PED_WEAPON_MOVEMENT_CLIPSET to change the weapon carrying movement animations.");
	}
	void CommandSetPedAnimGroupStrafe ( int, const char* )
	{
		SCRIPT_ASSERT(0, "SET_PED_ANIM_GROUP_STRAFE - This script command is no longer supported. Please remove it from your script. See SET_PED_STRAFE_CLIPSET to change the strafing movement animations.");
	}
	void CommandResetPedAnimGroupStrafe ( int )
	{
		SCRIPT_ASSERT(0, "RESET_PED_ANIM_GROUP_STRAFE - This script command is no longer supported. Please remove it from your script. See RESET_PED_STRAFE_CLIPSET to change the strafing movement animations.");
	}
	void CommandForceWeaponMovementAnimGroup ( int, const char* )
	{
		SCRIPT_ASSERT(0, "FORCE_WEAPON_MOVEMENT_ANIM_GROUP - This script command is no longer supported. Please remove it from your script. See SET_PED_WEAPON_MOVEMENT_CLIPSET to change the weapon carrying movement animations.");
	}
	void CommandResetWeaponMovementAnimGroup ( int )
	{
		SCRIPT_ASSERT(0, "RESET_WEAPON_MOVEMENT_ANIM_GROUP - This script command is no longer supported. Please remove it from your script. See RESET_PED_WEAPON_MOVEMENT_CLIPSET to change the weapon carrying movement animations.");
	}
	// END OF DEPRECATED 
	
	bool CommandIsScriptedScenarioPedUsingConditionalAnim(int PedIndex, const char * pAnimDictName, const char * pAnimClipName)
	{
		// I'd rather we use hashes instead of strings...but anims are usually passed as strings from script and this is how design wants it...
		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (scriptVerifyf(pPed, "Invalid PED_INDEX passed to command IS_SCRIPTED_SCENARIO_PED_USING_CONDITIONAL_ANIM.  %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pAnimDictName && pAnimClipName, "NULL string passed to IS_SCRIPTED_SCENARIO_PED_USING_CONDITIONAL_ANIM.  %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CTaskAmbientClips * pTaskAmbientClips = static_cast<CTaskAmbientClips *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pTaskAmbientClips && pTaskAmbientClips->GetBaseClip())
			{
				atHashString clipDict, clip;
				bool bFound = fwClipSetManager::GetClipDictionaryNameAndClipName(pTaskAmbientClips->GetBaseClip()->GetClipSet(), pTaskAmbientClips->GetBaseClip()->GetClipId(), clipDict, clip, false);
				return (bFound && clipDict == atHashString(pAnimDictName) && clip == atHashString(pAnimClipName));
			}
		}
		return false;
	}

	void CommandSetPedAlternateWalkAnim( int PedIndex, const char * animDictName, const char * animName, float blendDelta, bool looped )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			atHashWithStringNotFinal dict(animDictName);
			if (SCRIPT_VERIFY(fwAnimManager::FindSlotFromHashKey(dict.GetHash()).Get()>-1, "SET_PED_ALTERNATE_WALK_ANIM - couldn't find anim dictionary!"))
			{
				atHashWithStringNotFinal anim(animName);
				if (SCRIPT_VERIFY(anim.GetHash(), "SET_PED_ALTERNATE_WALK_ANIM - No anim name given"))
				{
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
					if(scriptVerifyf(pTask && (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED || pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED_LOW_LOD), "%s:SET_PED_ALTERNATE_WALK_ANIM - Primary motion task [%s] is not TASK_MOTION_PED (or TASK_MOTION_PED_LOW_LOD). Is this an animal or mounted ped [%s]?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTask ? pTask->GetName().c_str() : "null", pPed->GetModelName()))
					{
						CTaskMotionPed* pTaskPed = static_cast<CTaskMotionPed*>(pTask);
						pTaskPed->SetAlternateClip(CTaskMotionBase::ACT_Walk, CTaskMotionBase::sAlternateClip(dict, anim, fwAnimHelpers::CalcBlendDuration(blendDelta)), looped);
					}
				}
			}
		}
	}

	void CommandClearPedAlternateWalkAnim( int PedIndex, float blendDelta )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
			if(scriptVerifyf(pTask && (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED || pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED_LOW_LOD), "%s:CLEAR_PED_ALTERNATE_WALK_ANIM - Primary motion task [%s] is not TASK_MOTION_PED (or TASK_MOTION_PED_LOW_LOD). Is this an animal or mounted ped [%s]?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTask ? pTask->GetName().c_str() : "null", pPed->GetModelName()))
			{
				CTaskMotionPed* pTaskPed = static_cast<CTaskMotionPed*>(pTask);
				pTaskPed->StopAlternateClip( CTaskMotionBase::ACT_Walk, fwAnimHelpers::CalcBlendDuration(blendDelta) );
			}
		}
	}

	void CommandSetPedAlternateMovementAnim( int PedIndex, int AlternateAnimType, const char * animDictName, const char * animName, float blendDelta, bool looped )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			atHashWithStringNotFinal dict(animDictName);
			if (SCRIPT_VERIFY(fwAnimManager::FindSlotFromHashKey(dict.GetHash()).Get()>-1, "SET_PED_ALTERNATE_MOVEMENT_ANIM - couldn't find anim dictionary!"))
			{
				atHashWithStringNotFinal anim(animName);
				if (SCRIPT_VERIFY(anim.GetHash(), "SET_PED_ALTERNATE_MOVEMENT_ANIM - No anim name given"))
				{
					CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
					if(scriptVerifyf(pTask && (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED || pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED_LOW_LOD), "%s:SET_PED_ALTERNATE_MOVEMENT_ANIM - Primary motion task [%s] is not TASK_MOTION_PED (or TASK_MOTION_PED_LOW_LOD). Is this an animal or mounted ped [%s]?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTask ? pTask->GetName().c_str() : "null", pPed->GetModelName()))
					{
						pTask->SetAlternateClip(static_cast<CTaskMotionBase::AlternateClipType>(AlternateAnimType), CTaskMotionBase::sAlternateClip(dict, anim, fwAnimHelpers::CalcBlendDuration(blendDelta)), looped);
					}
				}
			}
		}
	}

	void CommandClearPedAlternateMovementAnim( int PedIndex, int AlternateAnimType, float blendDelta )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if (pPed)
		{
			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
			if(scriptVerifyf(pTask && (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED || pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED_LOW_LOD), "%s:CLEAR_PED_ALTERNATE_MOVEMENT_ANIM - Primary motion task [%s] is not TASK_MOTION_PED (or TASK_MOTION_PED_LOW_LOD). Is this an animal or mounted ped [%s]?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTask ? pTask->GetName().c_str() : "null", pPed->GetModelName()))
			{
				CTaskMotionPed* pTaskPed = static_cast<CTaskMotionPed*>(pTask);
				pTaskPed->StopAlternateClip(static_cast<CTaskMotionBase::AlternateClipType>(AlternateAnimType), fwAnimHelpers::CalcBlendDuration(blendDelta));
			}
		}
	}

	void CommandSetPedGestureGroup( int PedIndex, const char* szGestureGroupName )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_GESTURE_GROUP - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				fwMvClipSetId	groupId = fwClipSetManager::GetClipSetId(szGestureGroupName);
					
				if(SCRIPT_VERIFY(groupId != CLIP_SET_ID_INVALID, "SET_PED_GESTURE_GROUP - Couldn't find Anim Group Name"))
				{	
					pPed->SetGestureClipSet(groupId);
				}
			}
		}
	}

	void CommandBlendOutSynchronisedMovementAnims( int UNUSED_PARAM(PedIndex) )
	{
		scriptAssertf(0, "%s:%s - this command is no longer supported. Please remove it from your script.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scrThread::GetCurrentCmdName());
	}

	bool CommandIsPedStopped(int PedIndex)
	{
		const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		bool LatestCmpFlagResult = false;
		
		if (SCRIPT_VERIFY(pPed, "IS_PED_STOPPED - Ped doesn't exist"))
		{
			if (CScriptPeds::IsPedStopped(pPed))
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedShootingInArea(int PedIndex, const scrVector & scrVec1, const scrVector & scrVec2, bool HighlightArea, bool Do3dCheck)
	{
		bool LatestCmpFlagResult = false;
		Vector3 Vec1 = Vector3 (scrVec1); 
		Vector3 Vec2f = Vector3 (scrVec2); 

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetPedResetFlag( CPED_RESET_FLAG_FiringWeapon ))
			{
				if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					if (pPed->GetMyVehicle())
					{
						if (Do3dCheck)	
						{
							if (pPed->GetMyVehicle()->IsWithinArea(Vec1.x, Vec1.y, Vec1.z, Vec2f.x, Vec2f.y, Vec2f.z))
							{
								LatestCmpFlagResult = TRUE;
							}
						}
						else
						{
							if (pPed->GetMyVehicle()->IsWithinArea(Vec1.x, Vec1.y, Vec2f.x, Vec2f.y))
							{
								LatestCmpFlagResult = TRUE;
							}
						}
					}
				}
				else
				{
					if (Do3dCheck)	
					{
						if (pPed->IsWithinArea(Vec1.x, Vec1.y, Vec1.z, Vec2f.x, Vec2f.y, Vec2f.z))
						{
							LatestCmpFlagResult = TRUE;
						}
					}
					else
					{
						if (pPed->IsWithinArea(Vec1.x, Vec1.y, Vec2f.x, Vec2f.y))
						{
							LatestCmpFlagResult = TRUE;
						}
					}
				}
			}

			if (HighlightArea)
			{
				if (Do3dCheck)
				{
				CScriptAreas::NewHighlightImportantArea(Vec1.x, Vec1.y, Vec1.z, Vec2f.x, Vec2f.y, Vec2f.z, true, CALCULATION_THIS_PED_SHOOTING_IN_AREA);
				}
				else
				{
				CScriptAreas::NewHighlightImportantArea(Vec1.x, Vec1.y, INVALID_MINIMUM_WORLD_Z, Vec2f.x, Vec2f.y, INVALID_MINIMUM_WORLD_Z, false, CALCULATION_THIS_PED_SHOOTING_IN_AREA);
				}
			}
			if (CScriptDebug::GetDbgFlag())
			{
				if (Do3dCheck)
				{
					CScriptDebug::DrawDebugCube(Vec1.x, Vec1.y, Vec1.z, Vec2f.x, Vec2f.y, Vec2f.z);
				}
				else
				{
					CScriptDebug::DrawDebugSquare(Vec1.x, Vec1.y, Vec2f.x, Vec2f.y);
				}	
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsAnyPedShootingInArea(const scrVector & vMin, const scrVector & vMax, bool HighlightArea, bool Do3dCheck)
	{
		bool LatestCmpFlagResult = FALSE;

		CEntityIterator entityIterator( IteratePeds );
		CPed* pPed = (CPed*)entityIterator.GetNext();

		while( pPed && !LatestCmpFlagResult )
		{
			if (pPed->GetPedResetFlag( CPED_RESET_FLAG_FiringWeapon ))
			{
				if (Do3dCheck)
				{				
					if (pPed->IsWithinArea(vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z))
					{
						LatestCmpFlagResult = TRUE;
					}
				}
				else
				{
					if (pPed->IsWithinArea(vMin.x, vMin.y, vMax.x, vMax.y))
					{
						LatestCmpFlagResult = TRUE;
					}
				
				}
			}	
			pPed = (CPed*)entityIterator.GetNext();
		}

		if (HighlightArea)
		{
			if (Do3dCheck)
			{
				CScriptAreas::NewHighlightImportantArea(vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z, true, CALCULATION_THIS_PED_SHOOTING_IN_AREA);
			}
			else
			{
				CScriptAreas::NewHighlightImportantArea(vMin.x, vMin.y, INVALID_MINIMUM_WORLD_Z, vMax.x, vMax.y, INVALID_MINIMUM_WORLD_Z, false, CALCULATION_THIS_PED_SHOOTING_IN_AREA);
			}
		}


		if (CScriptDebug::GetDbgFlag())
		{
			if (Do3dCheck)
			{
				CScriptDebug::DrawDebugCube(vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z);
			}
			else
			{
				CScriptDebug::DrawDebugSquare(vMin.x, vMin.y, vMax.x, vMax.y);
			}	

		}

		return LatestCmpFlagResult;
	}


	bool CommandIsPedShooting(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			if (pPed->GetPedResetFlag( CPED_RESET_FLAG_FiringWeapon ))
			{
				LatestCmpFlagResult = true;
			}
		}

		return LatestCmpFlagResult;
	}


	void CommandSetPedAccuracy(int PedIndex, int NewAccuracy)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
		    if (SCRIPT_VERIFY(((NewAccuracy >= 0) && (NewAccuracy <= 100)), "SET_PED_ACCURACY - Shooting accuracy must be between 0 and 100"))
            {
				if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_ACCURACY - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			    {
					pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponAccuracy, ((float) NewAccuracy)/100.0f);
					if (NewAccuracy == 100)
					{
						pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_PerfectAccuracy);
					}
					else
					{
						pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_PerfectAccuracy);
					}
			    }
            }
		}
	}

	int CommandGetPedAccuracy(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( scriptVerifyf(pPed, "%s:GET_PED_ACCURACY - Ped doesn't exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()) )
		{
			return (int)(100.0f * pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponAccuracy));
		}

		// by default return sentinel value indicating error
		return -1;
	}

	void CommandSetAmbientLawPedAccuracyModifier(float fNewAccuracy)
	{
		if (scriptVerifyf(fNewAccuracy >= 0.0f && fNewAccuracy <= 1.0f, "SET_AMBIENT_LAW_PED_ACCURACY_MODIFIER - trying to set modifier (%.2f) outside valid ranges [0.0;1.0]",fNewAccuracy))
		{
			CPedAccuracy::fMPAmbientLawPedAccuracyModifier = fNewAccuracy;
		}
	}

	bool CommandIsPedModel(int PedIndex, int PedModelHashKey)
	{
		bool LatestCmpFlagResult = false;

		fwModelId PedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) PedModelHashKey, &PedModelId);		//	ignores return value
	
		if (SCRIPT_VERIFY(PedModelId.IsValid(), "IS_PED_MODEL - this is not a valid model index"))
		{
			const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		
			if (pPed)
			{
				if (pPed->GetModelIndex() == PedModelId.GetModelIndex())
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}


	void CommandExplodePedHead(int PedIndex, int nWeaponUsedHash)
	{
		// this script command is not approved for use in network scripts

		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "EXPLODE_PED_HEAD - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				CPedDamageCalculator damageCalculator(NULL, 1000.0f, nWeaponUsedHash, 0, false);
				// set damage to be applied to head
				damageCalculator.SetComponentFromPedBoneTag(pPed, BONETAG_HEAD);

				CEventDamage event(NULL, fwTimer::GetTimeInMilliseconds(), nWeaponUsedHash);
				damageCalculator.ApplyDamageAndComputeResponse(pPed, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);

				if(event.AffectsPed(pPed))
					pPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}

	void CommandRemovePedElegantly(int &PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, 0); // don't assert if no ped
		if (pPed)
		{
			if (SCRIPT_VERIFY(!pPed->IsPlayer(), "REMOVE_PED_ELEGANTLY - Cannot call this command on the player"))
			{				
				if (pPed->PopTypeIsMission())
				{
					CScriptEntityExtension* pExtension = pPed->GetExtension<CScriptEntityExtension>();

					if (SCRIPT_VERIFY(pExtension, "REMOVE_PED_ELEGANTLY - The ped is not a script ped") &&
						SCRIPT_VERIFY(pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler(), "REMOVE_PED_ELEGANTLY - The ped was not created by this script"))
					{
						if ( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()) )
						{
							CPedFactory::GetFactory()->DestroyPed(pPed);							
						}
						else
						{
							CTheScripts::UnregisterEntity(pPed, true);
						}
					}
				}
				PedIndex = 0;
			}
		}
	}


	void CommandAddArmourToPed(int PedIndex, int ArmourToAdd)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{
			pPed->SetArmour( pPed->GetArmour() + ArmourToAdd );
			if (pPed->IsPlayer())
			{
				if (AssertVerify(pPed->GetPlayerInfo()))
				{
					pPed->SetArmour( MIN(pPed->GetArmour(), pPed->GetPlayerInfo()->MaxArmour) );
				}
			}
			pPed->SetArmour( MAX(pPed->GetArmour(), 0) );
		}
	}

	void CommandSetPedArmour(int PedIndex, int Armour)
	{
		Assert(Armour >= 0);
		
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->SetArmour((float)Armour);
			if (pPed->IsPlayer())
			{
				if (AssertVerify(pPed->GetPlayerInfo()))
				{
					pPed->SetArmour( MIN(pPed->GetArmour(), pPed->GetPlayerInfo()->MaxArmour) );
				}
			}
			pPed->SetArmour( MAX(pPed->GetArmour(), 0) );
		}
	}

	int CommandGetPedEndurance(int iPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if(SCRIPT_VERIFY(pPed, "GET_PED_ENDURANCE - CPed not found"))
		{
			return static_cast<int>(pPed->GetEndurance());
		}

		return 0;
	}

	int CommandGetPedMaxEndurance(int iPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if(SCRIPT_VERIFY(pPed, "GET_PED_MAX_ENDURANCE - CPed not found"))
		{
			return static_cast<int>(pPed->GetMaxEndurance());
		}

		return 0;
	}

	void CommandSetPedEndurance(int iPedIndex, int iNewEndurance)
	{
		if(SCRIPT_VERIFY(iNewEndurance >= 0, "SET_PED_ENDURANCE - Endurance must be 0 or more"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);

			if(SCRIPT_VERIFY(pPed, "SET_PED_ENDURANCE - CPed not found"))
			{
				pPed->SetEndurance((float)iNewEndurance);
			}
		}
	}

	void CommandSetPedMaxEndurance(int iPedIndex, int iNewEndurance)
	{
		if (SCRIPT_VERIFY(iNewEndurance >= 0, "SET_PED_MAX_ENDURANCE - Endurance must be 0 or more"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);

			if (SCRIPT_VERIFY(pPed, "SET_PED_MAX_ENDURANCE - CPed not found"))
			{
				if (pPed->GetNetworkObject() && iNewEndurance != pPed->GetMaxEndurance())
				{
					SafeCast(CNetObjPed, pPed->GetNetworkObject())->SetMaxEnduranceAlteredByScript();
				}

				pPed->SetMaxEndurance((float)iNewEndurance);
			}
		}
	}

	void CommandSetEnableHandCuffs(int FirstPedIndex, bool enable)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed, enable );

			if (pPed->GetRagdollConstraintData())
				pPed->GetRagdollConstraintData()->EnableHandCuffs(pPed, enable);
		}
	}

	bool CommandGetEnableHandCuffs(int FirstPedIndex)
	{
		bool rv = false;
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);		

		if (pPed)
		{
			rv = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed);
		}
		return rv;
	}

	void CommandSetEnableBoundAnkles(int FirstPedIndex, bool enable)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsAnkleCuffed, enable );
	
			if (pPed->GetRagdollConstraintData())
				pPed->GetRagdollConstraintData()->EnableBoundAnkles(pPed, enable);
		}
	}

	bool CommandGetEnableBoundAnkles(int FirstPedIndex)
	{
		bool rv = false;
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);		
		if (pPed)
		{
			rv = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAnkleCuffed);
		}
		return rv;
	}

	void CommandSetEnableScuba(int FirstPedIndex, bool enable)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsScuba, enable );
		}
	}

	void CommandSetCanAttackFriendly(int FirstPedIndex, bool enable, bool canTarget)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly, enable );

			if(canTarget)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowLockonToFriendlyPlayers, enable );
			}
		}
	}

	int CommandGetPedAlertness(int FirstPedIndex)
	{
		const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(FirstPedIndex);

		if (pPed)
		{
			return static_cast<int>(pPed->GetPedIntelligence()->GetAlertnessState());
		}
		return -1;
	}

	void CommandSetPedAlertness(int FirstPedIndex, int iAlertnessState)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			pPed->GetPedIntelligence()->SetAlertnessState(static_cast<CPedIntelligence::AlertnessState>(iAlertnessState));
		}
	}

	void CreateRandomPed(const Vector3 &TempCoors_ , u32 &NewPedIndex, bool bMustBeMale, bool bMustBeFemale)
	{
		Vector3 TempCoors = TempCoors_;
		u32 NewPedModelIndex; //used to be an u32 is weird things happen return to how it was.
		CPed *pPed;
		Gender gender = bMustBeMale ? GENDER_MALE : (bMustBeFemale ? GENDER_FEMALE : GENDER_DONTCARE);

		// Old, with presumably mismatched parameters:
		//	NewPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(gender, MEMBERSHIP_DONT_CARE, CLIP_SET_ID_INVALID, NULL, 0, false, false, NULL, true);
		CPedPopulation::ChooseCivilianPedModelIndexArgs args;
		args.m_RequiredGender = gender;
		args.m_IgnoreMaxUsage = true;
		NewPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);

		if (SCRIPT_VERIFY(NewPedModelIndex != fwModelId::MI_INVALID, "CREATE_RANDOM_PED - Couldn't create a new ped - no suitable models loaded") &&
			SCRIPT_VERIFY(!CTheScripts::GetCurrentGtaScriptThread()->IsANetworkThread(), "CREATE_RANDOM_PED - Currently not supported for network scripts"))
		{
			if (TempCoors.z <= INVALID_MINIMUM_WORLD_Z)
			{
				TempCoors.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, TempCoors.x, TempCoors.y);
			}

			fwModelId newPedModelId((strLocalIndex(NewPedModelIndex)));
			const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(((CPedModelInfo*)CModelInfo::GetBaseModelInfo(newPedModelId))->GetPedCapsuleHash().GetHash());
			SCRIPT_ASSERT(pCapsuleInfo, "CREATE_RANDOM_PED - Missing capsule info");
			TempCoors.z += pCapsuleInfo->GetGroundToRootOffset();

			Matrix34 tempMat;
			tempMat.Identity();
			tempMat.d = TempCoors;

			const CControlledByInfo localAiControl(false, false);
			pPed =  CPedFactory::GetFactory()->CreatePed(localAiControl, newPedModelId, &tempMat, false, true, true);

			if (SCRIPT_VERIFY(pPed, "CreateRandomPed - Couldn't create a new ped - probably too many peds already"))
			{
				pPed->SetVarRandom(pPed->IsPlayer(), PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL); // I assume a random ped should have a random variation...
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);

				// This is done so that if we get a non-human ped-type we are offsetting their z position properly
				TempCoors.z -= pCapsuleInfo->GetGroundToRootOffset();
				TempCoors.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();

				pPed->SetPosition(TempCoors);
				pPed->SetHeading(0.0f);
				CScriptEntities::ClearSpaceForMissionEntity(pPed);
				pPed->ActivatePhysics();

				CGameWorld::Add(pPed, CGameWorld::OUTSIDE );	

				NewPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);

				CTheScripts::RegisterEntity(pPed);
			}
		}
	}

	enum {
		DONT_CARE_PED_SEX,
		MALE_PED,
		FEMALE_PED
	};
	bool CommandCanCreateRandomPed(int iPedType)
	{
		Gender gender;

		switch (iPedType)
		{
			case DONT_CARE_PED_SEX:
				gender = GENDER_DONTCARE;
				break;
			case MALE_PED:
				gender = GENDER_MALE;
				break;
			case FEMALE_PED:
				gender = GENDER_FEMALE;
				break;
			default:
				scriptAssertf (0, "%s:CAN_CREATE_RANDOM_PED - Ped type is incorrect, use the correct enum", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return false;
		}

			// Note: this used to be
			//	fwModelId NewPedModelIndex(CPedPopulation::ChooseCivilianPedModelIndex(gender, MEMBERSHIP_DONT_CARE, CLIP_SET_ID_INVALID, NULL, 0, false, false, NULL, true));
			// but the parameters appeared to have been mismatched a bit (pointers/booleans don't match up).
			// Presumably it's bIgnoreMaxUsage that should have been true.

			CPedPopulation::ChooseCivilianPedModelIndexArgs args;
			args.m_RequiredGender = gender;
			args.m_IgnoreMaxUsage = true;
			fwModelId NewPedModelIndex((strLocalIndex(CPedPopulation::ChooseCivilianPedModelIndex(args))));

			if (!NewPedModelIndex.IsValid())
			{
				return false;
			}

			// if we are in a network game we have to check whether we can register another ped first
			if(NetworkInterface::IsGameInProgress())
			{
				bool canRegisterPed = NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_PED, true);

				if(!canRegisterPed)
				{
					return false;
				}
			}

			return true;
	}

	u32 CommandCreateRandomPed(const scrVector & scrVecNewCoors)
	{
		u32 NewPedIndex = NULL_IN_SCRIPTING_LANGUAGE;

		Vector3 vNewCoors = Vector3(scrVecNewCoors); 
		
		CreateRandomPed(vNewCoors, NewPedIndex, false, false);

		return NewPedIndex;
	}

	u32 CommandCreateRandomMalePed(const scrVector & scrVecNewCoors)
	{
		u32 NewPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
		
		Vector3 vNewCoors = Vector3(scrVecNewCoors); 

		CreateRandomPed(vNewCoors, NewPedIndex, true, false);
	
		return NewPedIndex;
	}

	u32 CommandCreateRandomFemalePed(const scrVector & scrVecNewCoors)
	{
		u32 NewPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
		
		Vector3 vNewCoors = Vector3(scrVecNewCoors); 

		CreateRandomPed(vNewCoors, NewPedIndex, false, true);

		return NewPedIndex;
	}

	u32 CommandCreateRandomPedAsDriver(int VehicleIndex, bool ignoreDriverRestriction)
	{
		u32 NewPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
		
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle && 
			SCRIPT_VERIFY(!CTheScripts::GetCurrentGtaScriptThread()->IsANetworkThread(), "CREATE_RANDOM_PED_AS_DRIVER - Currently not supported for network scripts"))
		{				
			if (SCRIPT_VERIFY(!pVehicle->GetDriver(), "CREATE_RANDOM_PED_AS_DRIVER - Vehicle already has a driver"))
			{	
				if (SCRIPT_VERIFY(gPopStreaming.IsFallbackPedAvailable(), "CREATE_RANDOM_PED_AS_DRIVER - No ped available to be used. Test first with CAN_CREATE_RANDOM_DRIVER"))
				{
					CPed *pPed = CPedPopulation::AddPedInCar(pVehicle, true, 0, false, false, true, ignoreDriverRestriction);
					
					if (SCRIPT_VERIFY(pPed, "CREATE_RANDOM_PED_AS_DRIVER - Couldn't create a new ped - call CAN_CREATE_RANDOM_PED first"))
					{
						pPed->SetVarRandom(pPed->IsPlayer(), PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL); // I assume a random ped should have a random variation...
						pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);

						pPed->GetPedIntelligence()->AddTaskAtPriority(0, PED_TASK_PRIORITY_PRIMARY);
						scriptAssertf(pPed->GetPedIntelligence()->GetTaskDefault(),"CREATE_RANDOM_PED_AS_DRIVER - Ped does not have a default task (Ask a coder)" );

						CTheScripts::RegisterEntity(pPed);

						NewPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
					}
				}
			}
		}

		return NewPedIndex;
	}

	bool CommandCanCreateRandomDriver()
	{
		// if we are in a network game we have to check whether we can register another ped first
		if(NetworkInterface::IsGameInProgress())
		{
			bool canRegisterPed = NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_PED, true);

			if(!canRegisterPed)
			{
				return false;
			}
		}

		return gPopStreaming.IsFallbackPedAvailable();
	}

	bool CommandCanCreateRandomBikeRider()
	{
		// if we are in a network game we have to check whether we can register another ped first
		if(NetworkInterface::IsGameInProgress())
		{
			bool canRegisterPed = NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_PED, true);

			if(!canRegisterPed)
			{
				return false;
			}
		}

		return gPopStreaming.IsFallbackPedAvailable(true);
	}

	void CommandSetPedMoveAnimSpeedMultiplier(int PedIndex, float UNUSED_PARAM(fMultiplier))
	{
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_MOVE_ANIM_SPEED_MULTIPLIER - This script command is not allowed in network game scripts"))
		{	
			CPed * pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{	
				animAssertf(0, "SetMoveAnimSpeedMultiplyer not implemented! See an animation programmer!");
			}
		}
	}

	void CommandSetPedMoveAnimsBlendOut(int PedIndex)
	{
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_MOVE_ANIMS_BLEND_OUT - This script command is not allowed in network game scripts"))
		{	
			CPed * pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{	
				pPed->GetMotionData()->StopAllMotion(false);
			}
		}
	}

	void CommandSetPedCanBeDraggedOut(int PedIndex, bool bAllowDragOut)
	{
		CPed *pPed= CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{
			bAllowDragOut = !bAllowDragOut;
			
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar, bAllowDragOut  );
		}
	}

	void CommandSetAllowHurtCombatForAllMissionPeds(bool bEnable)
	{
		CPed::SetAllowHurtForAllMissionPeds(bEnable);
	}

	bool CommandIsPedMale(int PedIndex)
	{
		bool LatestCmpFlagResult = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{	
			scriptAssertf(!pPed->IsPlayer(), "%s:IS_PED_MALE - Being called on a player! Please use '!IS_PLAYER_FEMALE' instead!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if (!CPedType::IsFemaleType(pPed->GetPedType()))
			{
				LatestCmpFlagResult = TRUE;
			}
		}

		return LatestCmpFlagResult;
	}

	bool CommandIsPedHuman(int PedIndex)
	{
		bool bPedHuman = true;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pPed && pPed->GetPedModelInfo())
		{	
			bPedHuman = pPed->GetPedModelInfo()->GetPersonalitySettings().GetIsHuman();
		}

		return bPedHuman;
	}

	int CommandGetVehiclePedIsIn(int PedIndex, bool ConsiderEnteringAsInVehicle)
	{
		int ReturnVehicleIndex = 0;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
            scriptAssertf(!NetworkInterface::IsGameInProgress() || !NetworkInterface::IsRemotePlayerInNonClonedVehicle(pPed) || ConsiderEnteringAsInVehicle , "%s:GET_VEHICLE_PED_IS_IN - Calling this function for a remote player in a vehicle that has not been cloned on this machine!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			CVehicle *pVehicle = pPed->GetMyVehicle();

			if (ConsiderEnteringAsInVehicle && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)) // Matching logic behaviour of IS_PED_IN_ANY_VEHICLE
			{
				const s32 iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
				if ((iState < CTaskEnterVehicle::State_EnterSeat) && (iState != CTaskEnterVehicle::State_ClimbUp))
				{
					scriptAssertf(0, "GET_VEHICLE_PED_IS_IN - Considering enter task, but not in correct state (>= State_EnterSeat || State_ClimbUp) (state %d)", iState);
					return ReturnVehicleIndex;
				}

				CClonedEnterVehicleInfo* pEnterVehicleTaskInfo = static_cast<CClonedEnterVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
				if (!pEnterVehicleTaskInfo)
				{
					return ReturnVehicleIndex;
				}

				if (!pEnterVehicleTaskInfo->GetTarget() || !pEnterVehicleTaskInfo->GetTarget()->GetIsTypeVehicle())
				{
					return ReturnVehicleIndex;
				}

				pVehicle = static_cast<CVehicle*>(pEnterVehicleTaskInfo->GetTarget());
			}

			if (!pVehicle)
			{
				return ReturnVehicleIndex;
			}

#if __DEV
			pVehicle->m_nDEflags.bCheckedForDead = TRUE;
#endif
			ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
			
		}
		return ReturnVehicleIndex;
	}

	void CommandResetPedLastVehicle(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Expected valid ped index in RESET_PED_LAST_VEHICLE"))
		{
			if (SCRIPT_VERIFY(!pPed->GetIsInVehicle(), "Cannot call RESET_PED_LAST_VEHICLE if ped is inside a vehicle."))
			{
				pPed->SetMyVehicle(NULL);
			}
		}
	}

	void CommandResetPedLastVehicleOnExit(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Expected valid ped index in PROC_RESET_PED_LAST_VEHICLE_ON_EXIT"))
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ResetLastVehicleOnVehicleExit, true);
		}
	}

	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
	void CommandSetPedDensityMultiplier(float DensityMult)
	{
		scriptDebugf1("SET_PED_DENSITY_MULTIPLIER set to %f by %s", DensityMult, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CPedPopulation::SetAmbientPedDensityMultiplier(DensityMult);
	}
	// NOTE : SOON TO BE REMOVED (url:bugstar:933592)
	void CommandSetScenarioPedDensityMultiplier(float fInteriorDensityMult, float fExteriorDensityMult)
	{
		scriptDebugf1("SET_SCENARIO_PED_DENSITY_MULTIPLIER set to %f/%f by %s", fInteriorDensityMult, fExteriorDensityMult, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CPedPopulation::SetScenarioPedDensityMultipliers(fInteriorDensityMult, fExteriorDensityMult);
	}

	void CommandSetPedDensityMultiplierThisFrame(float DensityMult)
	{
		//scriptDebugf1("SET_PED_DENSITY_MULTIPLIER_THIS_FRAME set to %f by %s", DensityMult, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CPedPopulation::SetScriptAmbientPedDensityMultiplierThisFrame(DensityMult);
	}

	void CommandSetScenarioPedDensityMultiplierThisFrame(float fInteriorDensityMult, float fExteriorDensityMult)
	{
		//scriptDebugf1("SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME set to %f/%f by %s", fInteriorDensityMult, fExteriorDensityMult, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CPedPopulation::SetScriptScenarioPedDensityMultipliersThisFrame(fInteriorDensityMult, fExteriorDensityMult);
	}

	void CommandSuppressAmbientPedAggressiveCleanupThisFrame()
	{
		CPedPopulation::SetSuppressAmbientPedAggressiveCleanupThisFrame();
	}

	void CommandSetScriptedConversionCoordThisFrame(const scrVector & centre)
	{
		CPedPopulation::ScriptedConversionCentreSetThisFrame(centre);
	}

	void CommandSetScriptedConversionCoord(const scrVector & UNUSED_PARAM(centre))
	{
		Assertf(false, "SET_SCRIPTED_CONVERSION_COORD is deprecated, use SET_SCRIPTED_CONVERSION_COORD_THIS_FRAME instead");
		//CPedPopulation::ScriptedConversionCentreSet(centre);
	}

	void CommandClearScriptedConversionCoord()
	{
		Assertf(false, "CLEAR_SCRIPTED_CONVERSION_COORD is deprecated, use SET_SCRIPTED_CONVERSION_COORD_THIS_FRAME instead");
		//CPedPopulation::ScriptedConversionCentreClear();
	}

	void CommandSetPedNonRemovalArea(const scrVector & min, const scrVector & max)
	{
		Vector3 vMin(min.x, min.y, min.z);
		Vector3 vMax(max.x, max.y, max.z);
		CPedPopulation::PedNonRemovalAreaSet(vMin, vMax);
	}

	void CommandClearPedNonRemovalArea()
	{
		CPedPopulation::PedNonRemovalAreaClear();
	}

	void CommandSetPedNonCreationArea(const scrVector & min, const scrVector & max)
	{
		Vector3 vMin(min.x, min.y, min.z);
		Vector3 vMax(max.x, max.y, max.z);
		CPedPopulation::PedNonCreationAreaSet(vMin, vMax);
	}

	void CommandClearPedNonCreationArea()
	{
		CPedPopulation::PedNonCreationAreaClear();
	}

	void CommandInstantlyFillPedPopulation()
	{
		CPedPopulation::InstantFillPopulation();
	}

	void CommandSetPedMoney(int PedIndex, int MoneyToSet)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
			{
				pPed->SetMoneyCarried( (u16) MoneyToSet );
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_MoneyHasBeenGivenByScript, true );
			}
		}

	int CommandGetPedMoney(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			return pPed->GetMoneyCarried();
		}
		return 0;
	}

	void CommandSetMoneyCarriedByAllNewPeds(int amount)
	{
		CPed::ms_MoneyCarriedByAllNewPeds = amount;
	}

	void CommandSetHealthSnacksCarriedByAllNewPeds(float probability, int amount)
	{
		if (scriptVerifyf(probability >= 0.0f && probability < 1.01f, "SET_HEALTH_SNACKED_CARRIED_BY_ALL_NEW_PEDS: Probability must be between 0 and 1"))
		{
			CPed::ms_ProbabilityPedsWillDropHealthSnacks = probability;
			CPed::ms_HealthInSnackCarriedByAllNewPeds = amount;
		}
	}

	void CommandSetAmbientPedsDropMoney(bool pedsDropMoney)
	{
		CPed::SetRandomPedsDropMoney(pedsDropMoney);
	}

#if __BANK
	REGISTER_TUNE_GROUP_BOOL( bLogEventBlockingNativeCalls, false );
#endif
	void CommandSetBlockingOfNonTemporaryEventsForAmbientPedsThisFrame(bool block)
	{
#if __BANK
		INSTANTIATE_TUNE_GROUP_BOOL( SCRIPT_COMMANDS, bLogEventBlockingNativeCalls );
		if (bLogEventBlockingNativeCalls)
		{
			scriptDisplayf("SET_BLOCKING_OF_NON_TEMPORARY_EVENTS_FOR_AMBIENT_PEDS_THIS_FRAME - called with bBlock = %s", AILogging::GetBooleanAsString(block));
			scrThread::PrePrintStackTrace();
		}
#endif

		CPed::SetRandomPedsBlockingNonTempEventsThisFrame(block);
	}

	void CommandSetPedOntoMount(int PedIndex, int MountPedIndex, int DestNumber)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{	
			CPed *pMount = CTheScripts::GetEntityToModifyFromGUID<CPed>(MountPedIndex);

			if (pMount)
			{
				s32 iSeat = -1;
				if (DestNumber >= -1)
				{
					iSeat = DestNumber + 1;
				}
				
				pPed->SetPedOnMount(pMount);
			}			
		}
	}

	void CommandRemovePedFromMount(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			pPed->SetPedOffMount();				
		}
	}

	int CommandGetMount(int PedIndex)
	{
		int mountIndex = 0;
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			CPed* pMount = pPed->GetMyMount();
			if (pMount)
				mountIndex = CTheScripts::GetGUIDFromEntity(*pMount);			
		}
		return mountIndex;
	}

	bool CommandIsPedOnMount(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			return pPed->GetMyMount() != NULL;				
		}
		return false;
	}

#if ENABLE_HORSE

	void CommandSetMaxMountSpeedWhileAiming(int PedIndex, float Speed)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			if (scriptVerifyf(pPed->GetHorseComponent(), "I don't think this is a horse: SET_MAX_MOUNT_SPEED_WHILE_AIMING"))
			{
				pPed->GetHorseComponent()->SetMaxSpeedWhileAiming(Speed);
			}					
		}
	}

	void CommandSetMountAgitationScale(int PedIndex, float Scale)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			if (scriptVerifyf(pPed->GetHorseComponent(), "I don't think this is a horse: SET_MOUNT_AGITATION_SCALE"))
			{
				pPed->GetHorseComponent()->ApplyAgitationScale(Scale);
			}					
		}
	}	

#else

	void CommandSetMaxMountSpeedWhileAiming(int UNUSED_PARAM(PedIndex), float UNUSED_PARAM(Speed))
	{
	}

	void CommandSetMountAgitationScale(int UNUSED_PARAM(PedIndex), float UNUSED_PARAM(Scale))
	{
	}

#endif

	bool CommandIsPedUsingLowriderAlternateClipset(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans);
		}

		return false;
	}

	bool CommandIsPedOnVehicle(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetGroundPhysical())
		{
			return pPed->GetGroundPhysical()->GetIsTypeVehicle();
		}

		return false;
	}

	bool CommandIsPedOnSpecificVehicle(int PedIndex, int VehicleIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			CVehicle *pOtherVehicle = static_cast<CVehicle*>(pPed->GetGroundPhysical());

			if (pVehicle == pOtherVehicle)
			{
				return true;
			}
		}

		return false;
	}

	void CommandSetPedIntoVehicle(int PedIndex, int VehicleIndex, int DestNumber)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{	
#if __ASSERT
			if (pPed->IsBaseFlagSet(fwEntity::IS_FIXED))
			{
				scriptAssertf(0, "Script : (%s) has called SET_PED_INTO_VEHICLE on ped who is frozen: %s - 0x%p", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetDebugName(), pPed);
			}
#endif

			// Clear the vehicle that may have been cached for the single player BJ synced scene, required because of 
			// pPed->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_Warp); in CommandTaskSynchronizedScene that removes the player from 
			// the vehicle when getting a BJ in first person view.
			if(pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->SetCachedBJVehicle(NULL);
			}
			
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		    if (pVehicle)
			{
				scriptAssertf(!pVehicle->GetNetworkObject() || !pVehicle->GetNetworkObject()->HasBeenCloned(), "Script : (%s) SET_PED_INTO_VEHICLE is not safe to be called on vehicles that have been cloned! Use TASK_ENTER_VEHICLE with the warp flag set instead!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				s32 iSeat = -1;
				if (DestNumber >= -1)
				{
					iSeat = DestNumber + 1;
				}
				
				// If -2 is passed in, we should put the ped into any passenger seat
				bool bAnyPassenger = false;
				if( DestNumber == -2 )
				{
					bAnyPassenger = true;
				}

				if (!scriptVerifyf(bAnyPassenger || pVehicle->IsSeatIndexValid(iSeat), "Invalid seat index (%i) passed in", DestNumber))
				{
					return;
				}

				if (!bAnyPassenger)
				{
					CPed* pOccupier = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
					if( pPed != pOccupier && !scriptVerifyf(!pOccupier, "Seat (%i) is already occupied by ped %s, call GET_PED_IN_VEHICLE_SEAT to check if there is a ped inside. Please remove the ped in the seat before calling this command", DestNumber, pOccupier->GetDebugName() ? pOccupier->GetDebugName() : "NULL"))
					{
						return;
					}
				}

			    // Remove ped from vehicle
			    pPed->SetPedOutOfVehicle();

			    // update interior state from the vehicle being warped into
			    if (pPed->m_nFlags.bInMloRoom || pPed->GetIsRetainedByInteriorProxy() )
                {
				    CGameWorld::Remove(pPed);
				    CGameWorld::Add(pPed, pVehicle->GetInteriorLocation());
			    }

				    pPed->GetPedIntelligence()->FlushImmediately();
				
				if (!bAnyPassenger)
				{
					pPed->SetPedInVehicle(pVehicle,iSeat,CPed::PVF_Warp);
				}
				else
				{
					// If -2 is passed in, we should put the ped into any passenger seat
					for (s32 seatIndex=1; seatIndex<pVehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
					{
						if (!pVehicle->GetSeatManager()->GetPedInSeat(seatIndex))
						{
							pPed->SetPedInVehicle(pVehicle,seatIndex,CPed::PVF_Warp);
							break;
						}
					}
				}

			    scriptAssertf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "%s:SET_PED_INTO_VEHICLE - Failed to add ped to vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			    if (iSeat>-1) // only check the seat number if the script asked for a specific seat
			    {
				    scriptAssertf(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) == iSeat, "%s:SET_PED_INTO_VEHICLE_AS_PASSENGER - Failed to add ped to vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			    }
			}
		}
    }

	void CommandSetPedAllowVehiclesOverride(int PedIndex, bool bOnOff)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowPedInVehiclesOverrideTaskFlags, bOnOff );
		}
	}


	void CommandSetPedSuffersCriticalHits(int PedIndex, bool SuffersCriticalHitsFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{	
			if (SuffersCriticalHitsFlag)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits, FALSE );
			}
			else
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits, TRUE );
			}
		}
	}

	void CommandSetPedUpperBodyDamageOnly(int PedIndex, bool EnableUpperBodyDamageOnly)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly, EnableUpperBodyDamageOnly );
		}
	}

	bool CommandIsPedSittingInVehicle(int PedIndex, int VehicleIndex)
	{
		bool LatestCmpFlagResult = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	
		if (pPed)
		{
			const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
			if (pVehicle)
			{	
				if ((pPed->GetMyVehicle() == pVehicle)&&(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )))
				{
					if ((pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) == false)
						&& (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) == false ))
					{
						LatestCmpFlagResult = true;
					}
				
				}
			}
		}
		return LatestCmpFlagResult;
	}


	bool CommandIsPedSittingInAnyVehicle(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
            if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				scriptAssertf(pPed->GetMyVehicle(), "%s:IS_PED_SITTING_IN_ANY_VEHICLE - Ped is not in a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				if ( (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE) == NULL)
					&& (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE) == NULL) )
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}


	bool CommandIsPedOnFoot(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{	
			LatestCmpFlagResult = true;

			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				LatestCmpFlagResult = false;
			}
			else
			{
				if ( (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
					|| (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) ) )
				{
					LatestCmpFlagResult = false;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	

	bool CommandIsPedOnAnyBike(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
			{
				if (pPed->GetMyVehicle()->InheritsFromBike())
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedPlantingC4(int PedIndex)
	{
		Assertf( 0, "NATIVE NO LONGER SUPPORTED! Please use new native IS_PED_PLANTING_BOMB" );
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{	
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BOMB);
			if (pTask)
			{
				s32 iTaskState = pTask->GetState();
				if (iTaskState == CTaskBomb::State_SlideAndPlant)
				{
					return true;
				}
			}

		}

		return false;
	}

	bool CommandIsPedPlantingBomb(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{	
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BOMB);
			if (pTask)
			{
				s32 iTaskState = pTask->GetState();
				if (iTaskState == CTaskBomb::State_SlideAndPlant)
				{
					return true;
				}
			}

		}

		return false;
	}


	scrVector CommandGetDeadPedPickupCoords(int PedIndex, float minDist, float maxDist)
	{
		Vector3 VecNewCoors; 
		VecNewCoors.Zero();

		const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pPed)
		{
			Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
			if (pMyVehicle)
			{
				pedPos = VEC3V_TO_VECTOR3(pMyVehicle->GetTransform().GetPosition());
			}

			CPickupManager::CPickupPositionInfo pos( pedPos );
			pos.m_MinDist = minDist;
			pos.m_MaxDist = maxDist;
			CPickupManager::CalculateDroppedPickupPosition(pos, true);
#if __ASSERT
			// Don't check this in MP. We could create pickups in MP without collision around.
			if(!NetworkInterface::IsGameInProgress())
			{
				// url:bugstar:231786 
				bool bGroundLoaded = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(pedPos), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
				// Try a raw probe mimicking the PickupManager
				WorldProbe::CShapeTestProbeDesc probeDesc;
				WorldProbe::CShapeTestFixedResults<> probeResult;
				bool ReturnValue;
				ReturnValue = FALSE;
				probeDesc.SetStartAndEnd(Vector3(pedPos.x, pedPos.y, 1500.0f), Vector3(pedPos.x, pedPos.y, -1500.0f));
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				probeDesc.SetContext(WorldProbe::LOS_Unspecified);
				ReturnValue = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
				float testZ;
				if (ReturnValue)
				{
					testZ = probeResult[0].GetHitPosition().z;
				}
				else
				{
					testZ = 0.0f;
				}
				bool bAssertOkay = (pos.m_bOnGround && bGroundLoaded) ? true : false;
				scriptAssertf( bAssertOkay, "GET_DEAD_PED_PICKUP_COORDS: Ped '%s' died nowhere near ground [%c] or "
					"collision wasn't streaming in [%c].  Leaving co-ords near where passed in.  Original ped pos [%.2f, %.2f, %.2f] - "
					"returned pos [%.2f, %.2f, %.2f].  Independent probe z@+1500.0f returned %c to hitting at height %.2f\n",
					pPed->GetDebugName(), (pos.m_bOnGround) ? 'F' : 'T', (bGroundLoaded) ? 'F' : 'T', pedPos.x, pedPos.y, pedPos.z,pos.m_Pos.x,pos.m_Pos.y,pos.m_Pos.z,
					(ReturnValue) ? 'T' : 'F', testZ );
			}
#endif // __ASSERT

			VecNewCoors = pos.m_Pos;
		}

		return VecNewCoors;
	}


	bool CommandIsPedInAnyBoat(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if ((pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()))
			{
				if (pPed->GetMyVehicle()->InheritsFromBoat())
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

    bool CommandIsPedInAnySub(int PedIndex)
    {
        bool LatestCmpFlagResult = false;

        const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

        if (pPed)
        {
            if ((pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()))
            {
                if (pPed->GetMyVehicle()->InheritsFromSubmarine())
                {
                    LatestCmpFlagResult = true;
                }
            }
        }
        return LatestCmpFlagResult;
    }


	bool CommandIsPedInAnyHeli(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if ((pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()))
			{
				if (pPed->GetMyVehicle()->InheritsFromHeli() )
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}


	bool CommandIsPedInAnyPlane(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if ((pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()))
			{
				if (pPed->GetMyVehicle()->InheritsFromPlane())
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedInFlyingVehicle(int PedIndex)
	{
		bool LatestCmpFlagResult = false;;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if ((pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))&&(pPed->GetMyVehicle()))
			{	
				if ( (pPed->GetMyVehicle()->InheritsFromPlane())
					|| pPed->GetMyVehicle()->GetIsRotaryAircraft() )
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandSetPedDiesInWater(int PedIndex, bool DrownsInWaterFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, DrownsInWaterFlag );
#if DEBUG_DRAW
			pPed->m_bDrownDisabledByScript=!DrownsInWaterFlag;
#endif
		}
	}

	bool CommandGetPedDiesInWater(int PedIndex)
	{
		if (const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DrownsInWater);
		}

		return false;
	}

	void CommandSetPedDiesInstantlyInWater(int PedIndex, bool bDiesWhenSwimming)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming, bDiesWhenSwimming );
		}
	}

	void CommandSetPedDiesInSinkingVehicle(int PedIndex, bool bDrownsInVeh)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle, bDrownsInVeh );
			}

	}

	void CommandSetPedDiesWhenInjured(int PedIndex, bool bDies)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress() || !pPed->IsAPlayerPed(), "SET_PED_DIES_WHEN_INJURED -  This script command is not supported for players in network game scripts"))
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceDieIfInjured, bDies );
			}
		}
	}

	void CommandSetPedEnableWeaponBlocking(int PedIndex, bool bEnableWeaponBlocking)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (SCRIPT_VERIFY(!pPed->IsAPlayerPed(), "SET_PED_ENABLE_WEAPON_BLOCKING -  This script command is not used for players"))
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_EnableWeaponBlocking, bEnableWeaponBlocking );
			}
		}
	}	

	int CommandGetPedArmour(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			return (s32)pPed->GetArmour();
		}
		return 0;
	}

	void CommandSetPedStayInVehicleWhenJacked(int PedIndex, bool StayInCarFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_StayInCarOnJack, StayInCarFlag );
		}
	}

	void CommandSetPedCanBeShotInVehicle(int PedIndex, bool CanBeShotInVehicleFlag)
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_CAN_BE_SHOT_IN_VEHICLE -  This script command is not allowed in network game scripts"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanBeShotInVehicle, CanBeShotInVehicleFlag );
			}
		}
	}

	//	Return true if ReturnBoneTag contains a valid result
	bool GetPedLastDamageBone(int PedIndex, int &ReturnBoneTag)
	{
	    ReturnBoneTag = (int) BONETAG_ROOT;

	    const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

	    if (pPed)
	    {
		    int nHitComponent = pPed->GetWeaponDamageComponent();
		    //	still not sure whether invalid result is represented by 0 or -1
		    if (nHitComponent > 0)
		    {
				ReturnBoneTag = (int) pPed->GetBoneTagFromRagdollComponent(nHitComponent);
			    return true;
		    }
	    }
  
		return false;
	}
	
	void CommandSetScriptControlledRagdollFlag(int FirstPedIndex)
	{
		CPed *pModifiedPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pModifiedPed)
		{	
			pModifiedPed->SetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll, true );
		}
	}

	void CommandSetSuppressLowLODRagdollSwitchUponDeath(int FirstPedIndex)
	{
		CPed *pModifiedPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(FirstPedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pModifiedPed)
		{	
			pModifiedPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SuppressLowLODRagdollSwitchWhenCorpseSettles, true );
		}
	}

	void SetAiWeaponDamageModifier(float fModifier)
	{
		CPedDamageCalculator::SetAiWeaponDamageModifier(fModifier);
	}

	void ResetAiWeaponDamageModifier()
	{
		CPedDamageCalculator::ResetAiWeaponDamageModifier();
	}

	void SetAiMeleeWeaponDamageModifier(float fModifier)
	{
		CPedDamageCalculator::SetAiMeleeWeaponDamageModifier(fModifier);
	}

	void ResetAiMeleeWeaponDamageModifier()
	{
		CPedDamageCalculator::ResetAiMeleeWeaponDamageModifier();
	}

	void ClearPedLastDamageBone(int PedIndex)
	{
        if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "CLEAR_PED_LAST_DAMAGE_BONE -  This script command is not allowed in network game scripts"))
        {
		    CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
    		
		    if (pPed)
		    {
			    pPed->SetWeaponDamageComponent(-1);
		    }
        }
	}

	void CommandSetTreatAsAmbientPedForDriverLockOn(int PedIndex, bool bTreatAsAmbientPed)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TreatAsAmbientPedForDriverLockOn, bTreatAsAmbientPed );
		}
	}

	void CommandSetPedCanBeTargetted(int PedIndex, bool bAllowTargettedFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed, !bAllowTargettedFlag );
		}
	}

	void CommandSetPedCanBeTargettedByTeam(int PedIndex, int Team, bool bAllowTargettedFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);

		if (pPed &&
			SCRIPT_VERIFY(pPed->GetNetworkObject(), "SET_PED_CAN_BE_TARGETTED_BY_TEAM - Can only be used during a network game") &&
			SCRIPT_VERIFY(Team >=0 && Team < MAX_NUM_TEAMS, "SET_PED_CAN_BE_TARGETTED_BY_TEAM - Invalid team") )
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			if (pPedObj)
			{
				pPedObj->SetIsTargettableByTeam(Team, bAllowTargettedFlag);
			}
		}
	}

	void CommandSetPedCanBeTargettedByPlayer(int PedIndex, int Player, bool bAllowTargettedFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

		if (pPed 
			&& SCRIPT_VERIFY(pPed->GetNetworkObject(), "SET_PED_CAN_BE_TARGETTED_BY_PLAYER - Can only be used during a network game")
			&& SCRIPT_VERIFY(Player >= 0 && Player < MAX_NUM_PHYSICAL_PLAYERS, "SET_PED_CAN_BE_TARGETTED_BY_PLAYER - Invalid Player"))
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());

			if (pPedObj)
			{
				pPedObj->SetIsTargettableByPlayer(Player, bAllowTargettedFlag);
			}
		}
	}

	void CommandAllowLockOnToPedIfFriendly(int PedIndex, bool bLockOn)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly, bLockOn );
		}
	}

	void CommandSetUseCameraHeadingForDesiredDirectionLockOnTest(int PedIndex, bool bUseCameraHeading)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseCameraHeadingForDesiredDirectionLockOnTest, bUseCameraHeading );
		}
	}

	bool CommandIsPedInAnyPoliceVehicle(int PedIndex)
	{
		bool LatestCmpFlagResult = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	
		if (pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
			{
				if ( (pPed->GetMyVehicle()->IsLawEnforcementVehicle()) )
				{
					LatestCmpFlagResult = true;
				}
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandForcePedToOpenParachute(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);

			if (pTask)
			{
				static_cast<CTaskParachute*>(pTask)->ForcePedToOpenChute();
			}
		}
	}

	bool CommandIsPedFalling(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed && pPed->GetPedIntelligence()->IsPedFalling())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedJumping(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence()->IsPedJumping())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedDoingABeastJump(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskJump* pTaskJump = (CTaskJump*)( pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP) );
			if(pTaskJump && pTaskJump->GetIsDoingSuperJump() && pTaskJump->GetIsDoingBeastJump())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedLanding(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence()->IsPedLanding())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedClimbing(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence()->IsPedClimbing())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedVaulting(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence()->IsPedVaulting())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedDiving(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence()->IsPedDiving())
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedJumpingOutOfVehicle(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskExitVehicleSeat* pExitVehicleTask = static_cast<CTaskExitVehicleSeat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
			if (pExitVehicleTask)
			{
				return pExitVehicleTask->GetState() == CTaskExitVehicleSeat::State_JumpOutOfSeat;
			}
			else if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT))
			{
				if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT) == CTaskExitVehicleSeat::State_JumpOutOfSeat)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool CommandIsPedOpeningDoor(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed && pPed->GetPedIntelligence())
		{
			CTaskOpenDoor* pOpenDoorTask = static_cast<CTaskOpenDoor*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_OPEN_DOOR));

			if (pOpenDoorTask)
			{
				return true;
			}

			pOpenDoorTask = static_cast<CTaskOpenDoor*>(pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_OPEN_DOOR));

			if (pOpenDoorTask)
			{
				return true;
			}
		}

		return false;
	}

	bool CommandIsPedInParachuteFreefall(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CQueriableInterface* pQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
			if(pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
			{
				switch(pQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_PARACHUTE))
				{
					case CTaskParachute::State_TransitionToSkydive:
					case CTaskParachute::State_Skydiving:
					{
						LatestCmpFlagResult = true;
						break;
					}
					default:
					{
						break;
					}
				}
			}
		}

		return LatestCmpFlagResult;
	}

	// Return value maps to PED_PARACHUTE_STATE defined in commands_ped.sch. This doesn't map to any code-side
	// enums because of differences in states between CTaskParachute and CTaskParachuteNM.
	//	PPS_INVALID		= -1,	// Ped is not in a valid Parachuting state
	//	PPS_SKYDIVING	= 0,
	//	PPS_DEPLOYING	= 1,
	//	PPS_PARACHUTING	= 2,
	//	PPS_LANDING		= 3
	int CommandGetPedParachuteState(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE) )
			{
				int iState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_PARACHUTE);
				switch (iState)
				{
				case CTaskParachute::State_Start:
				case CTaskParachute::State_WaitingForBaseToStream:
				case CTaskParachute::State_BlendFromNM:
				case CTaskParachute::State_WaitingForMoveNetworkToStream:
				case CTaskParachute::State_TransitionToSkydive:
				case CTaskParachute::State_WaitingForSkydiveToStream:
				case CTaskParachute::State_Skydiving:
					return 0;
				case CTaskParachute::State_Deploying:
					return 1;
				case CTaskParachute::State_Parachuting:
					return 2;
				case CTaskParachute::State_Landing:
				case CTaskParachute::State_CrashLand:
					return 3;
				default:
					return -1;
				}
			}
		}
		return -1;
	}

	int CommandGetPedParachuteLandingType(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskParachute* pTask = static_cast<CTaskParachute *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
			if(pTask)
			{
				return (int)pTask->GetLandingType();
			}
		}

		return -1;
	}

	void CommandSetPedParachuteTintIndex(int pedIndex, int tintIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			pPed->GetPedIntelligence()->SetTintIndexForParachute(tintIndex);
		}	
	}

	void CommandGetPedParachuteTintIndex(int pedIndex, int& tintIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			tintIndex = (int)pPed->GetPedIntelligence()->GetTintIndexForParachute();
		}	
	}

	void CommandSetPedReserveParachuteTintIndex(int pedIndex, int tintIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			pPed->GetPedIntelligence()->SetTintIndexForReserveParachute(tintIndex);
		}	
	}

	void CommandGetPedReserveParachuteTintIndex(int pedIndex, int& tintIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			tintIndex = (int)pPed->GetPedIntelligence()->GetTintIndexForReserveParachute();
		}	
	}

	void CommandSetPedParachutePackTintIndex(int pedIndex, int tintIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			pPed->GetPedIntelligence()->SetTintIndexForParachutePack((u32)tintIndex);
		}	
	}

	void CommandGetPedParachutePackTintIndex(int pedIndex, int& tintIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			tintIndex = (int)pPed->GetPedIntelligence()->GetTintIndexForParachutePack();
		}	
	}

	int CommandCreateParachuteBagObject(int pedIndex, bool bAttach, bool bClearVariation)
	{
		int iPropIndex = 0;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			CObject *pProp = CTaskParachute::CreateParachuteBagObject(pPed, bAttach, bClearVariation);
			if(pProp)
			{
				iPropIndex = CTheScripts::GetGUIDFromEntity(*pProp);
			}
		}

		return iPropIndex;
	}

	void CommandGiveJetpack(int pedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			//Ensure the inventory is valid.
			CPedInventory* pPedInventory = pPed->GetInventory();
			if(pPedInventory)
			{
				//! Give ped a jetpack.
				pPedInventory->AddWeapon(GADGETTYPE_JETPACK);
			}
		}
	}

	void CommandRemoveJetpack(int pedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			//Ensure the inventory is valid.
			CPedInventory* pPedInventory = pPed->GetInventory();
			if(pPedInventory)
			{
				//! Remove jetpack.
				pPedInventory->RemoveWeapon(GADGETTYPE_JETPACK);
			}
		}
	}

	void CommandEquipJetpack(int pedIndex, bool bEquip)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_EquipJetpack, bEquip);
		}
	}

	bool CommandIsJetpackEquipped(int pedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			return pPed->GetHasJetpackEquipped();
		}
		return false;
	}

	bool CommandIsUsingJetpack(int pedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(pPed)
		{
			return pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack);
		}
		return false;
	}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	void CommandRequestRagdollBoundsUpdate(int PedIndex, int framesToUpdateBounds)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->RequestRagdollBoundsUpdate((u16) framesToUpdateBounds);
		}
	}
#else
	void CommandRequestRagdollBoundsUpdate(int UNUSED_PARAM(PedIndex), int UNUSED_PARAM(framesToUpdateBounds))
	{
		scriptWarningf("CommandRequestRagdollBoundsUpdate - It's currently not necessary to call this function, since LAZY_RAGDOLL_BOUNDS_UPDATE is disabled.");
	}
#endif

	void CommandSetPedAoBlobRendering(int PedIndex, bool val)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetAoBlobRendering(val);
		}
	}

	bool CommandIsPedSheltered(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetIsShelteredFromRain();
		}

		return false;
	}

	bool CommandIsPedAoBlobRenderingEnabled(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetAoBlobRendering();
		}
		return true;
	}

	void CommandSetPedDucking(int PedIndex, bool bCrouching)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetIsCrouching(bCrouching);
		}
	}

	bool CommandIsPedDucking(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetIsCrouching();
		}
		return false;
	}

	bool CommandIsPedInAnyTaxi(int PedIndex)
	{
		bool LatestCmpFlagResult = false;
		
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if ( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (pPed->GetMyVehicle()) )
			{
				CVehicle *pVehicle = pPed->GetMyVehicle();

				if (pVehicle && CVehicle::IsTaxiModelId(pVehicle->GetModelId()))
				{
					LatestCmpFlagResult = true;
				}

			}
		}
		return LatestCmpFlagResult;
	}

	////////////////////////////////////
	// Ped perception commands

	void CommandSetPedSeeingRange(int PedIndex, float SeeingRange)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetPedPerception().SetSeeingRange(SeeingRange);
		}
	}

	void CommandSetPedHearingRange(int PedIndex, float HearingRange)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetPedPerception().SetHearingRange(HearingRange);
		}
	}

	void CommandSetPedIdentificationRange(int PedIndex, float IdRange)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetPedPerception().SetIdentificationRange(IdRange);
		}
	}

	void CommandSetPedHighlyPerceptive(int PedIndex, bool bIsHighlyPerceptive)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetPedPerception().SetIsHighlyPerceptive(bIsHighlyPerceptive);
		}
	}
	
	void CommandSetCopPerceptionOverrides( float fSeeingRange, float fSeeingRangePeripheral, float fHearingRange, float fMinAzimuthAngle, float fMaxAzimuthAngle, float fCentreOfGazeMaxAngle, float fRearViewSeeingRange)
	{
		// Override values set on cop peds upon creation ped perception constructor.
		CPedPerception::ms_bCopOverridesSet = true;
		CPedPerception::ms_fCopSeeingRangeOverride = fSeeingRange;
		CPedPerception::ms_fCopSeeingRangePeripheralOverride = fSeeingRangePeripheral;
		CPedPerception::ms_fCopHearingRangeOverride = fHearingRange;
		CPedPerception::ms_fCopVisualFieldMinAzimuthAngleOverride = fMinAzimuthAngle;
		CPedPerception::ms_fCopVisualFieldMaxAzimuthAngleOverride = fMaxAzimuthAngle;
		CPedPerception::ms_fCopCentreOfGazeMaxAngleOverride = fCentreOfGazeMaxAngle;
		CPedPerception::ms_fCopRearViewRangeOverride = fRearViewSeeingRange;	
	}

	//////////////////////////////////////////////
	// Ped perception field of view interface

	void CommandSetPedVisualFieldMinAngle(int iPedIndex, float fFOV)
	{
		Assert(fFOV >= -180.0f && fFOV <= 0.0f);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMinAzimuthAngle(fFOV);
	}
	void CommandSetPedVisualFieldMaxAngle(int iPedIndex, float fFOV)
	{
		Assert(fFOV >= 0.0f && fFOV <= 180.0f);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMaxAzimuthAngle(fFOV);
	}
	void CommandSetPedVisualFieldMinElevationAngle(int iPedIndex, float fTheta)
	{
		Assert(fTheta >= -180.0f && fTheta <= 0.0f);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMinElevationAngle(fTheta);
	}
	void CommandSetPedVisualFieldMaxElevationAngle(int iPedIndex, float fTheta)
	{
		Assert(fTheta >= 0.0f && fTheta <= 180.0f);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMaxElevationAngle(fTheta);
	}
	void CommandSetPedVisualFieldPeripheralRange(int iPedIndex, float fRange)
	{
		Assert(fRange >= 0.0f && fRange <= 9999.0f);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetSeeingRangePeripheral(fRange);
	}
	void CommandSetPedVisualFieldCenterAngle(int iPedIndex, float fFOV)
	{
		scriptAssertf(fFOV >= 0.0f && fFOV <= 90.0f, "fFoV: %f", fFOV);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			pPed->GetPedIntelligence()->GetPedPerception().SetCentreOfGazeMaxAngle(fFOV);
	}

	//

	float CommandGetPedVisualFieldMinAngle(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMinAzimuthAngle();
		return 0.0;
	}
	float CommandGetPedVisualFieldMaxAngle(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMaxAzimuthAngle();
		return 0.0f;
	}
	float CommandGetPedVisualFieldMinElevationAngle(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMinElevationAngle();
		return 0.0f;
	}
	float CommandGetPedVisualFieldMaxElevationAngle(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMaxElevationAngle();
		return 0.0f;
	}
	float CommandGetPedVisualFieldPeripheralRange(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRangePeripheral();
		return 0.0f;
	}
	float CommandGetPedVisualFieldCenterAngle(int iPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Ped doesn't exist"))
			return pPed->GetPedIntelligence()->GetPedPerception().GetCentreOfGazeMaxAngle();
		return 0.0f;
	}

	///////////////////////////////////////////////////


	void CommandSetPedInjuredOnGroundBehaviour(int PedIndex, float duration)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().ForceInjuredOnGroundBehaviour(duration);
		}
	}

	void CommandClearPedInjuredOnGroundBehaviour(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().ClearForcedInjuredOnGroundBehaviour();
		}
	}

	void CommandDisablePedInjuredOnGroundBehaviour(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().DisableInjuredOnGroundBehaviour();
		}
	}

	void CommandClearDisablePedInjuredOnGroundBehaviour(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().ClearDisableInjuredOnGroundBehaviour();
		}
	}

	void CommandSetPedStealthMovement(int PedIndex, bool bStealth, const char* movementModeOverrideName)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetMotionData()->SetUsingStealth(bStealth);

			u32 nOverrideHash = (bStealth && movementModeOverrideName) ? atStringHash(movementModeOverrideName) : 0;
			pPed->SetMovementModeOverrideHash(nOverrideHash);

			// Update the movement mode variables
			pPed->UpdateMovementMode();
		}
	}

	bool CommandGetPedStealthMovement(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			return pPed->GetMotionData()->GetUsingStealth();
		}
		return false;
	}

	void GetAnimInitialOffsetPositionAndRotation(const crClip* pClip, const scrVector & rootPos, const scrVector & rootRot, scrVector & vOutPos, scrVector & vOutRot, float phase, EulerAngleOrder RotationOrder)
	{
		Matrix34 rootMatrix(M34_IDENTITY);
		rootMatrix.d.x=rootPos.x;
		rootMatrix.d.y=rootPos.y;
		rootMatrix.d.z=rootPos.z;

		Vector3 radRot(DtoR*rootRot.x, DtoR*rootRot.y, DtoR*rootRot.z);

		CScriptEulers::MatrixFromEulers(rootMatrix, radRot, RotationOrder);

		radRot.Zero();

		if(pClip)
		{
			fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, rootMatrix);
		}

		if (phase>0.0f)
		{
			//have to get the mover track offset for the given phase as well:
			Matrix34 deltaMatrix(M34_IDENTITY);

			// Get the calculated mover track offset from phase 0.0 to the new phase
			if(pClip)
			{
				fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, phase, deltaMatrix);
			}

			// Transform by the mover offset
			rootMatrix.DotFromLeft(deltaMatrix);
		}

		vOutPos.x = rootMatrix.d.x;
		vOutPos.y = rootMatrix.d.y;
		vOutPos.z = rootMatrix.d.z;

		radRot = CScriptEulers::MatrixToEulers(rootMatrix, RotationOrder);

		vOutRot.x = RtoD*radRot.x;
		vOutRot.y = RtoD*radRot.y;
		vOutRot.z = RtoD*radRot.z;
	}

	scrVector CommandGetAnimInitialOffsetPosition(const char *pAnimDictName, const char *pAnimName, const scrVector & rootPos, const scrVector & rootRot, float phase, int RotOrder)
	{
		scrVector posVec(VEC3_ZERO);
		scrVector rotVec(VEC3_ZERO);

		// load the position from the clip and return it
		const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);

		scriptAssertf(phase>=0.0f && phase<=1.0f,"phase %f not in zero to one",phase);
		phase = Clamp(phase, 0.0f, 1.0f);

		if (scriptVerifyf(pClip, "Check that the dictionary: %s is loaded and the anim: %s is contained with the dictionary",pAnimDictName ,pAnimName ))
		{
			GetAnimInitialOffsetPositionAndRotation(pClip, rootPos, rootRot, posVec, rotVec, phase, static_cast<EulerAngleOrder>(RotOrder));
		}	

		return posVec;
	}

	scrVector CommandGetAnimInitialOffsetRotation(const char *pAnimDictName, const char *pAnimName, const scrVector & rootPos, const scrVector & rootRot, float phase, int RotOrder)
	{
		scrVector posVec(VEC3_ZERO);
		scrVector rotVec(VEC3_ZERO);

		// load the position from the clip and return it
		const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);

		scriptAssertf(phase>=0.0f && phase<=1.0f,"phase %f not in zero to one",phase);
		phase = Clamp(phase, 0.0f, 1.0f);

		if (scriptVerifyf(pClip, "Check that the dictionary: %s is loaded and the anim: %s is contained with the dictionary",pAnimDictName ,pAnimName ))
		{
			GetAnimInitialOffsetPositionAndRotation(pClip, rootPos, rootRot, posVec, rotVec, phase, static_cast<EulerAngleOrder>(RotOrder));
		}	

		return rotVec;
	}

	int CommandCreateGroup(int UNUSED_PARAM(DefaultTaskType))
	{
		CScriptResource_PedGroup pedGroup(CTheScripts::GetCurrentGtaScriptHandler());

		return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(pedGroup);
	}

	void CommandSetPedAsGroupLeader(int PedID, int UniqueGroupID)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedID);

		if (pPed)
		{
			int GroupIndex = GetActualGroupId(UniqueGroupID);

			if (SCRIPT_VERIFY((GroupIndex >= 0) && (GroupIndex < CPedGroups::MAX_NUM_GROUPS), "SET_PED_GROUP_LEADER - Group id out of range"))
			{
				if (SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].IsActive(), "SET_PED_AS_GROUP_LEADER - Group isn't active"))
				{
					if (SCRIPT_VERIFY(0==CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->GetLeader(), "SET_PED_AS_GROUP_LEADER - Group already has a leader"))
					{
						if (SCRIPT_VERIFY(!pPed->IsPlayer(), "SET_PED_AS_GROUP_LEADER - player already has a group, use GET_PLAYER_GROUP to find it"))
						{
							CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->SetLeader(pPed);
							CPedGroups::ms_groups[GroupIndex].Process();
						}
					}
				}
			}
		}
	}

	void CommandSetPedAsGroupMember(int PedID, int UniqueGroupID)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		int GroupIndex = GetActualGroupId(UniqueGroupID);

		if (!pPed)
		{
			return;
		}

		scriptAssertf(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillScanForDeadPeds), "CA_WILL_SCAN_FOR_DEAD_PEDS is not compatible with ped being in a group, please remove ped from group before setting this, ped 0x%p", pPed);

		if ((SCRIPT_VERIFY((GroupIndex >= 0) && (GroupIndex < CPedGroups::MAX_NUM_GROUPS), "SET_PED_AS_GROUP_MEMBER - Group id out of range"))&&
			(SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].IsActive(), "SET_PED_AS_GROUP_MEMBER - Group isn't active"))&&
			(SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->GetLeader(), "SET_PED_AS_GROUP_MEMBER - Group doesn't have leader"))&&
			(SCRIPT_VERIFY(0==pPed->GetPedsGroup(), "SET_PED_AS_GROUP_MEMBER - Ped already in a group"))&&
			(SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->CountMembersExcludingLeader()< CPedGroupMembership::MAX_NUM_MEMBERS-1, "SET_GROUP_MEMBER - group full")))
		{
			if (pPed->GetPedsGroup() && pPed->GetNetworkObject())
			{
				scriptAssertf(0, "Trying to add %s to group %d, but he is already a member of group %d", pPed->GetNetworkObject()->GetLogName(), GroupIndex, pPed->GetPedsGroup()->GetGroupIndex());
			}
			if(CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->CountMembersExcludingLeader() >= CPedGroupMembership::MAX_NUM_MEMBERS-1
				&& CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->GetLeader()
				&& CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->GetLeader()->IsPlayer())
			{
				CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->RemoveNFollowers(1);
			}
			CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->AddFollower(pPed);
			CPedGroups::ms_groups[GroupIndex].Process();

			// prevent the ped migrating for a while (so that the ped group gets time to update and sync)
			if (pPed->GetNetworkObject() && !NetworkUtils::IsNetworkCloneOrMigrating(pPed))
			{
				const int STOP_MIGRATION_TIME = 1000;
				NetworkInterface::DelayMigrationForTime(pPed, STOP_MIGRATION_TIME);
			}
		}
	}

	void CommandSetPedCanTeleportToGroupLeader(int PedID, int UniqueGroupID, bool bEnabled)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		int GroupIndex = GetActualGroupId(UniqueGroupID);

		if (SCRIPT_VERIFY(pPed, "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER called with invalid ped"))
		{
			if ( (SCRIPT_VERIFY((GroupIndex >= 0) && (GroupIndex < CPedGroups::MAX_NUM_GROUPS), "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER - Group id out of range")) &&
				(SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].IsActive(), "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER - Group isn't active")) &&
				(SCRIPT_VERIFY(CPedGroups::ms_groups[GroupIndex].GetGroupMembership()->GetLeader(), "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER - Group doesn't have leader")) &&
				(SCRIPT_VERIFY(pPed->GetPedsGroup(), "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER - Ped not in group")) &&
				(SCRIPT_VERIFY(pPed->GetPedsGroup()->GetGroupIndex() == GroupIndex, "SET_PED_CAN_TELEPORT_TO_GROUP_LEADER - Ped not in supplied group")) 
				)
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_TeleportIfCantReachPlayer, bEnabled);
			}
		}
	}

	void CommandRemoveGroup(int UniqueGroupID)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);

		if (iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS)
		{
			if ( (CPedGroups::ms_groups[iGroupID].IsActive())
				&& (CPedGroups::ms_groups[iGroupID].GetGroupMembership()->GetLeader())
				&& (CPedGroups::ms_groups[iGroupID].GetGroupMembership()->GetLeader()->IsPlayer()) )
			{
				CPedGroups::RemoveAllFollowersFromGroup(iGroupID);
			}
			else
			{
				CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(UniqueGroupID, false, true, CGameScriptResource::SCRIPT_RESOURCE_PEDGROUP);
			}
		}
	}

	void CommandRemovePedFromGroup(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		
		if (pPed && SCRIPT_VERIFY(!pPed->IsLocalPlayer(), "Shouldn't be removing player ped from group"))
		{
#if !__NO_OUTPUT
			if (pPed->GetUsingRagdoll())
				scriptErrorf("Calling REMOVE_PED_FROM_GROUP on ped '%s' while he's in ragdoll! This will abort the ragdoll task.", pPed->GetModelName());
#endif //!__NO_OUTPUT
			CPedGroup *pPedGroup = pPed->GetPedsGroup();
			if (pPedGroup)
			{
				if (pPedGroup->IsLocallyControlled())
				{
					// Actually OK to remove the leader, it should appoint another one in the process
					pPedGroup->GetGroupMembership()->RemoveMember(pPed);
					pPedGroup->Process();
				}
				else
				{
					CRemovePedFromPedGroupEvent::Trigger(*pPed);
				}
			}
		}
	}


	bool CommandIsPedGroupMember(int PedIndex, int UniqueGroupID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			if (!CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueGroupID))
			{
				return false;
			}

			const int iGroupID = GetActualGroupId(UniqueGroupID);
			if ((SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "IS_PED_GROUP_MEMBER - Group index is out of range"))&&
				(SCRIPT_VERIFY(CPedGroups::ms_groups[iGroupID].IsActive(), "IS_PED_GROUP_MEMBER - group isn't active")))
			{
				if (iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS)
				{
					if ( CPedGroups::ms_groups[iGroupID].IsActive() && CPedGroups::ms_groups[iGroupID].GetGroupMembership()->IsMember(pPed))
					{
						LatestCmpFlagResult = true;
					}
				}
			}
		}
		return LatestCmpFlagResult;
	}


	bool CommandIsPedGroupLeader(int PedIndex, int UniqueGroupID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		bool LatestCmpFlagResult = false;

		if (pPed)
		{
			const int iGroupID = GetActualGroupId(UniqueGroupID);
			if ((SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "IS_PED_GROUP_LEADER - Group index is out of range"))&&
				(SCRIPT_VERIFY(CPedGroups::ms_groups[iGroupID].IsActive(), "IS_PED_GROUP_LEADER - group isn't active")))
			{

				if (iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS)
				{
					if (CPedGroups::ms_groups[iGroupID].GetGroupMembership()->IsLeader(pPed))
					{
						LatestCmpFlagResult = true;
					}
				}
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedHangingOnToVehicle(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (SCRIPT_VERIFY(pPed, "IS_PED_HANGING_ON_TO_VEHICLE - Ped does not exist") && pPed->GetIsInVehicle())
		{
			const CVehicle& rVeh = *pPed->GetMyVehicle();
			const s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(pPed);
			if (rVeh.IsSeatIndexValid(iSeatIndex))
			{
				const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(iSeatIndex);
				if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
				{
					return true;
				}
			}
		}
		return false;
	}

	void CommandSetGroupSeparationRange(int UniqueGroupID, float SeparationRange)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);

		if (SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "SET_GROUP_SEPARATION_RANGE - Group index is out of range"))
		{
			CPedGroup* pPedGroup = &CPedGroups::ms_groups[iGroupID];

			if (SCRIPT_VERIFY(pPedGroup->IsActive(), "SET_GROUP_SEPARATION_RANGE - group isn't active"))
			{
				if (!pPedGroup->IsPlayerGroup() && NetworkInterface::IsGameInProgress())
				{
					if (!SCRIPT_VERIFY(!NetworkInterface::GetArrayManager().GetPedGroupsArrayHandler()->IsElementRemotelyArbitrated(pPedGroup->GetNetArrayHandlerIndex()), "SET_GROUP_SEPARATION_RANGE - Group is not controlled by this machine"))
					{
						return;
					}
				}

				pPedGroup->GetGroupMembership()->SetMaxSeparation(SeparationRange);
			}
		}
	}

	// Returns true if the ped is currently in combat, defined by whether it has a combat task active or in the list and only
	// interrupted by a temporary event
	bool CommandIsPedInCombat(int PedIndex, int TargetPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetPedIndex, 0);
		
		if (pPed)
		{
			if (pTargetPed)
			{
				// If task combat is the active task with the target ped, then return true
				if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pTargetPed)
				{
					return true;
				}
			}
			else
			{
				// If task combat is the active task, then return true
				if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT ) )
				{
					return true;
				}
			}
		}

		// No combat task present that can will be in control in the near future
		return false;
	}

	// Return the index of the target ped with which the ped is in combat with. Allow for an optional LoS check to be enforced
	int CommandGetPedTargetFromCombatPed(int PedIndex, bool ForceLoSCheck)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{
				CEntity* pTargetPed = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();

				if(pTargetPed)
				{ 
					if (ForceLoSCheck)
					{
						CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(false);
						if (pPedTargetting && pPedTargetting->GetLosStatus(pTargetPed) == Los_clear)
						{
							return CTheScripts::GetGUIDFromEntity(*pTargetPed);
						}
					}
					else
					{
						return CTheScripts::GetGUIDFromEntity(*pTargetPed);						
					}
				}
			}
		}

		return NULL_IN_SCRIPTING_LANGUAGE;
	}
	   
	// Returns true if the PedIndex ped can see the target using the the existing target info (will not wake up the targeting system)
	bool CommandCanPedInCombatSeeTarget(int PedIndex, int TargetPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetPedIndex);

		// Verify both peds exist
		if (pPed && pTargetPed)
		{
			// Get the targeting without waking it up then check if our los is clear to the target
			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
			if( pPedTargetting && pPedTargetting->GetLosStatus(pTargetPed) == Los_clear )
			{
				return true;
			}
		}
		
		return false;
	}

	// Returns true if the ped is basically lying on the ground.
	bool CommandIsPedProne(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->IsProne();
		}

		return false;
	}

	// Returns true if the ped is currently investigating, defined by whether it has a investigation task active or in the list and only
	// interrupted by a temporary event
	bool CommandIsPedInvestigating(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			// If task combat is the active task, then return true
			if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_INVESTIGATE ) )
			{
				return true;
			}
		}
		// No investigation task present that can will be in control in the near future
		return false;
	}

	bool CommandIsPedDoingDriveby(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
			{
				// If task driveby is the active task, then return true
				if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_GUN))
				{
					return true;
				}		
			}

		// No driveby task present that can will be in control in the near future
		return false;
	}	

	bool CommandIsPedJacking(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
			{
				CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX);
				aiAssert(dynamic_cast<CClonedEnterVehicleInfo*>(pTaskInfo));
				CClonedEnterVehicleInfo* pVehInfo = static_cast<CClonedEnterVehicleInfo*>(pTaskInfo);
				if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::HasJackedAPed))
				{
					return true;
				}
			}
		}	
		return false;
	}

	bool CommandIsPedBeingJacked(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
			{
				CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_EXIT_VEHICLE, PED_TASK_PRIORITY_MAX);
				aiAssert(dynamic_cast<CClonedExitVehicleInfo*>(pTaskInfo));
				CClonedExitVehicleInfo* pVehInfo = static_cast<CClonedExitVehicleInfo*>(pTaskInfo);
				if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
				{
					return true;
				}
			}

		}
		return false;
	}

	bool CommandIsPedBeingStunned(int PedIndex, int WeaponType)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			const CWeaponInfo* pWeaponInfo = WeaponType != 0 ? CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType) : NULL;

			if ((pWeaponInfo == NULL || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC) && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_ELECTROCUTE))
			{
				return TRUE;
			}

			if (pPed->GetPedIntelligence()->GetQueriableInterface()-> IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_SHOT))
			{
				const CClonedNMShotInfo* pTaskInfo =  static_cast<CClonedNMShotInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_NM_SHOT, PED_TASK_PRIORITY_MAX, false));
				if (pTaskInfo && pTaskInfo->GetWeaponHash() != 0)
				{
					const CWeaponInfo* pTaskWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pTaskInfo->GetWeaponHash());						
					if ((pWeaponInfo == NULL || (u32)WeaponType == pTaskWeaponInfo->GetHash()) && (pTaskWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER || pTaskWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC))
					{
						return TRUE;
					}
				}
			}

			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC))
			{
				if (pWeaponInfo == NULL)
				{
					return TRUE;
				}

				const CClonedDamageElectricInfo* pTaskInfo =  static_cast<CClonedDamageElectricInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_DAMAGE_ELECTRIC, PED_TASK_PRIORITY_MAX, false));
				if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER && pTaskInfo->GetRagdollType() == RAGDOLL_TRIGGER_RUBBERBULLET)
				{
					return TRUE;
				}
				else if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC && pTaskInfo->GetRagdollType() == RAGDOLL_TRIGGER_ELECTRIC)
				{
					return TRUE;
				}
			}
		}

		return false;
	}

	int CommandGetPedsJacker(int PedBeingJacked)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedBeingJacked);

		if (pPed)
		{
			CPed* pJackingPed = NULL;

			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
			{
				CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_EXIT_VEHICLE, PED_TASK_PRIORITY_MAX);
				aiAssert(dynamic_cast<CClonedExitVehicleInfo*>(pTaskInfo));
				CClonedExitVehicleInfo* pVehInfo = static_cast<CClonedExitVehicleInfo*>(pTaskInfo);
				if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
				{
					pJackingPed = pVehInfo->GetPedsJacker();
				}
			}

			if (pJackingPed)
			{
				return CTheScripts::GetGUIDFromEntity(*pJackingPed);
			}

		}
		return NULL_IN_SCRIPTING_LANGUAGE;
	}

	int CommandGetJackTarget(int PedJacking)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedJacking);

		if (pPed)
		{
			CPed* pJackedPed = NULL;
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
			{
				CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX);
				aiAssert(dynamic_cast<CClonedEnterVehicleInfo*>(pTaskInfo));
				CClonedEnterVehicleInfo* pVehInfo = static_cast<CClonedEnterVehicleInfo*>(pTaskInfo);
				if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::HasJackedAPed))
				{
					pJackedPed = pVehInfo->GetJackedPed();
				}
			}

			if (pJackedPed)
			{
				return CTheScripts::GetGUIDFromEntity(*pJackedPed);
			}
		}
		return NULL_IN_SCRIPTING_LANGUAGE;
	}

	bool CommandIsPedFleeing(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE ) ||
				pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_AND_FLEE ) ||
				pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE ) ||
				pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SCENARIO_FLEE ) )
			{
				return true;
			}
		}
		return false;
	}

	// Returns true if the ped is in cover
	bool CommandIsPedInCover(int PedIndex, bool bLimitToPedsInCoverThatAreIdle )
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(NetworkInterface::IsGameInProgress())
			{
				CQueriableInterface *p_Interface = pPed->GetPedIntelligence()->GetQueriableInterface();
				if( p_Interface->IsTaskCurrentlyRunning( CTaskTypes::TASK_IN_COVER ) )
				{
					if ( bLimitToPedsInCoverThatAreIdle )
					{
						s32 getState = p_Interface->GetStateForTaskType( CTaskTypes::TASK_IN_COVER );
						if ( !CTaskInCover::IsClassifiedAsIdle(getState))
						{
							return false;
						}
					}
					return true;
				}
			}
			else
			{
				CTaskInCover* pCoverTask =  static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				if(pCoverTask && pCoverTask->GetState() > CTaskInCover::State_Start)
				{
					if ( bLimitToPedsInCoverThatAreIdle )
					{
						if ( !CTaskInCover::IsClassifiedAsIdle(pCoverTask->GetState()))
						{
							// This ped is not seen as in cover.
							return false;
						}
					}
					return true;
				}
			}
		}
		return false;
	}

	// Returns true if the ped is in cover facing left
	bool CommandIsPedInCoverFacingLeft(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->IsNetworkClone())
			{
				CQueriableInterface *p_Interface = pPed->GetPedIntelligence()->GetQueriableInterface();
				if( p_Interface->IsTaskCurrentlyRunning( CTaskTypes::TASK_IN_COVER ) )
				{
					CClonedCoverInfo* pCoverInfo = static_cast<CClonedCoverInfo*>(p_Interface->GetTaskInfoForTaskType( CTaskTypes::TASK_IN_COVER, PED_TASK_PRIORITY_MAX ));
					if (pCoverInfo && (pCoverInfo->GetFlags() & CTaskCover::CF_FacingLeft))
					{
						return true;
					}
				}
			}
			else
			{
				CTaskInCover* pCoverTask =  static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				if(pCoverTask && pCoverTask->IsFacingLeft())
				{
					return true;
				}
			}
		}
		return false;
	}

	// Returns true if the ped is in high cover, false in low cover (or not in cover)
	bool CommandIsPedInHighCover(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->IsNetworkClone())
			{
				CQueriableInterface *p_Interface = pPed->GetPedIntelligence()->GetQueriableInterface();
				if( p_Interface->IsTaskCurrentlyRunning( CTaskTypes::TASK_IN_COVER ) )
				{
					CClonedCoverInfo* pCoverInfo = static_cast<CClonedCoverInfo*>(p_Interface->GetTaskInfoForTaskType( CTaskTypes::TASK_IN_COVER, PED_TASK_PRIORITY_MAX ));
					SCRIPT_ASSERT(pCoverInfo, "No cover task found during IS_PED_IN_HIGH_COVER, please check ped with IS_PED_IN_COVER first.");
					if (pCoverInfo && (pCoverInfo->GetFlags() & CTaskCover::CF_TooHighCoverPoint))
					{
						return true;
					}
				}
			}
			else
			{
				CTaskInCover* pCoverTask =  static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				SCRIPT_ASSERT(pCoverTask, "No cover task found during IS_PED_IN_HIGH_COVER, please check ped with IS_PED_IN_COVER first.");
				if(pCoverTask && pCoverTask->IsCoverPointTooHigh())
				{
					return true;
				}
			}
		}
		return false;
	}

	// Returns true if the ped is sliding into cover
	bool CommandIsPedGoingIntoCover(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(NetworkInterface::IsGameInProgress())
			{
				if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_COVER) )
				{
					return true;
				}
			}
			else
			{
				if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_COVER))
				{
					return true;
				}
			}
		}
		return false;
	}


	// Sets a char as pinned down (NOTE will only work if the ped is in combat
	void CommandSetPedPinnedDown(int PedIndex, bool bPinned, int iTime )
	{
		// this script command is not approved for use in network scripts
		float fTime = ( ( float )iTime ) / 1000.0f;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->GetPedIntelligence()->ForcePedPinnedDown( bPinned, fTime );
		}
	}

	void CommandGivePedPickupObject(int PedIndex, int ObjectIndex, bool UseProceduralAttach = false)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
			if (pObj)
			{
				pObj->m_nObjectFlags.bAmbientProp = !UseProceduralAttach;
#if 0 // CS
				pPed->GetInventory().PickupWeaponObject(pPed, pObj);
#endif // 0
			}
		}
	}

	int CommandGetSeatPedIsTryingToEnter(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<CClonedEnterVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
				if (pClonedEnterVehicleInfo)
				{
					return pClonedEnterVehicleInfo->GetSeat() - 1;
				}
			}
			else
			{
				CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if(pEnterVehicleTask)
				{
					return pEnterVehicleTask->GetTargetSeat() - 1;
				}
			}
		}

#if __ASSERT
		scriptWarningf("%s:GET_SEAT_PED_IS_TRYING_TO_ENTER - Ped %s, 0x%p wasn't running an enter vehicle task", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed ? pPed->GetDebugName() : "NULL", pPed);
#endif
		return -3;	// Valid VEHICLE_SEAT enums start at -2 (see generic.sch)
	}
	
	int CommandGetVehiclePedIsTryingToEnter(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<CClonedEnterVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
				if (pClonedEnterVehicleInfo)
				{
					CVehicle* pVehicle = (CVehicle*)(pClonedEnterVehicleInfo->GetTarget());
					int iVehicleIndex	= pVehicle ? CTheScripts::GetGUIDFromEntity(*pVehicle) : 0;
					return iVehicleIndex;
				}
			}
		}

		return 0;
	}

	int CommandGetPedSourceOfDeath(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			CEntity* pSource = pPed->GetSourceOfDeath();
			if(pSource)
			{
				return CTheScripts::GetGUIDFromEntity(*pSource);
			}
		}
		
		return 0;
	}
	
	u32 CommandGetPedCauseOfDeath(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if (pPed)
		{
			return pPed->GetCauseOfDeath();
		}

		return 0;
	}
	
	u32 CommandGetPedTimeOfDeath(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			return pPed->GetTimeOfDeath();
		}

		return 0;
	}

	int CommandCountPedsInCombatWithTarget(int TargetIndex)
	{
		const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetIndex);
		if(!pTargetPed)
		{
			return 0;
		}

		return CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*pTargetPed);
	}

	int CommandCountPedsInCombatWithTargetWithinRadius(int TargetIndex, const scrVector & scrVecSearchCenter, float fSearchRadius)
	{
		const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetIndex);
		if(!pTargetPed)
		{
			return 0;
		}

		Vec3V vSearchCenter(scrVecSearchCenter);
		ScalarV scSearchDistSq = LoadScalar32IntoScalarV(fSearchRadius);
		scSearchDistSq = (scSearchDistSq * scSearchDistSq);
		return CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*pTargetPed, 0, &vSearchCenter, &scSearchDistSq);
	}

	void CommandSetRelationshipBetweenGroups(int iRelationshipType, int iRelationshipGroup, int iOtherRelationshipGroup)
	{
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup(iRelationshipGroup);
		CRelationshipGroup* pRelGroup2 = CRelationshipManager::FindRelationshipGroup(iOtherRelationshipGroup);
		scriptAssertf(pRelGroup, "%s:SET_RELATIONSHIP_BETWEEN_GROUPS: Unknown group1", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(pRelGroup2, "%s:SET_RELATIONSHIP_BETWEEN_GROUPS: Unknown group2", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if( pRelGroup && pRelGroup2 )
		{
			if(!scriptVerifyf(pRelGroup != CRelationshipManager::s_pNoRelationshipGroup && pRelGroup2 != CRelationshipManager::s_pNoRelationshipGroup, 
				"SET_RELATIONSHIP_BETWEEN_GROUPS: Cannot change the NoRelationship group"))
			{
				return;
			}

			pRelGroup->SetRelationship((eRelationType) iRelationshipType, pRelGroup2);
		}

	}

	void CommandClearRelationshipBetweenGroups(int UNUSED_PARAM(iRelationshipType), int iRelationshipGroup, int iOtherRelationshipGroup)
	{
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroup);
		CRelationshipGroup* pRelGroup2 = CRelationshipManager::FindRelationshipGroup((u32)iOtherRelationshipGroup);
		scriptAssertf(pRelGroup, "%s:CLEAR_RELATIONSHIP_BETWEEN_GROUPS: Unknown group1", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(pRelGroup2, "%s:CLEAR_RELATIONSHIP_BETWEEN_GROUPS: Unknown group2", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if( pRelGroup && pRelGroup2 )
		{
			pRelGroup->ClearRelationship(pRelGroup2);
		}
	}

	void CommandAddRelationshipGroup(const char* szGroupName, int& iGroupHash)
	{
		CScriptResource_RelGroup relGroup(szGroupName);

		iGroupHash = (int)CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(relGroup);
	}

	void CommandRemoveRelationshipGroup(int iRelationshipGroup)
	{
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroup);
		if(pRelGroup)
		{
			if(scriptVerifyf(pRelGroup != CRelationshipManager::s_pPlayerGroup && pRelGroup != CRelationshipManager::s_pCopGroup &&
				pRelGroup != CRelationshipManager::s_pCivmaleGroup && pRelGroup != CRelationshipManager::s_pCivfemaleGroup && pRelGroup != CRelationshipManager::s_pArmyGroup &&
				pRelGroup != CRelationshipManager::s_pNoRelationshipGroup && pRelGroup != CRelationshipManager::s_pSecurityGuardGroup,
				"REMOVE_RELATIONSHIP_GROUP: Can't remove default relationship group: %s.", atHashWithStringNotFinal(pRelGroup->GetName()).TryGetCStr()))
			{
				CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RELATIONSHIP_GROUP, iRelationshipGroup);

				// Make sure the conditions here match the conditions in CScriptResource_RelGroup::Destroy()
				CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroup);
				if(pRelGroup && pRelGroup->GetType() == RT_mission)
				{
					CRelationshipManager::RemoveRelationshipGroup(pRelGroup);
				}
			}
		}
	}

	bool CommandDoesRelationshipGroupExist(int iRelGroup)
	{
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelGroup);
		if (pRelGroup)
		{
			return true;
		}
		return false;
	}

	void SetRelationshipGroupBlipForPed(CPed *pPed, bool bAddBlip)
	{
		if (pPed)
		{
			CMiniMapBlip *pExistingBlip = CMiniMap::GetBlipAttachedToEntity(pPed, BLIP_TYPE_CHAR, BLIP_FLAG_CREATED_FOR_RELATIONSHIP_GROUP_PED);

			if (bAddBlip)
			{
				if (pExistingBlip == NULL)
				{
					//	When the script terminates, will we have to remove the blips from any non-mission peds
					//	with this relationship group?
					CEntityPoolIndexForBlip PedPoolIndex(pPed, BLIP_TYPE_CHAR);
					s32 blipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CHAR, PedPoolIndex, BLIP_DISPLAY_BOTH, NULL);

					if (CMiniMap::IsBlipIdInUse(blipId))
					{
						CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, blipId, false);

						CMiniMapBlip *pNewBlip = CMiniMap::GetBlip(blipId);
						if (scriptVerifyf(pNewBlip, "SetRelationshipGroupBlipForPed - failed to get pointer to newly-created blip"))
						{
							CMiniMap::SetFlag(pNewBlip, BLIP_FLAG_CREATED_FOR_RELATIONSHIP_GROUP_PED);
						}
					}
				}
			}
			else
			{	//	Remove blip
				if (pExistingBlip)
				{
					CMiniMap::RemoveBlip(pExistingBlip);
				}
			}
		}
	}

	void CommandSetPedRelationshipGroupDefaultHash( int PedIndex, int iRelationshipGroup )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if( pPed )
		{
			CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroup);

			if (SCRIPT_VERIFY(pGroup, "SET_PED_RELATIONSHIP_GROUP_DEFAULT_HASH - Cannot find group"))
			{
				pPed->GetPedIntelligence()->SetRelationshipGroupDefault(pGroup);
			}
		}
	}

	void CommandSetPedRelationshipGroupHash( int PedIndex, int iRelationshipGroup )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroup);

			if (scriptVerifyf(pGroup, "SET_PED_RELATIONSHIP_GROUP_HASH - Cannot find group %i", iRelationshipGroup))
			{
				scriptAssertf(!pPed->IsAPlayerPed() || CNetwork::IsGameInProgress() || CScriptHud::bUsingMissionCreator || (pGroup == CRelationshipManager::s_pPlayerGroup), "SET_PED_RELATIONSHIP_GROUP_DEFAULT_HASH - Can't change the players relationship group!");
				if (scriptVerifyf(pGroup->GetType() == RT_mission, "%s:SET_PED_RELATIONSHIP_GROUP_HASH(%s) - Cant assign ped to randomly created group", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pGroup->GetName().GetCStr()))
				{
					if(NetworkUtils::IsNetworkCloneOrMigrating(pPed))
					{
                        if(static_cast<int>(pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash()) != iRelationshipGroup)
                        {
						    CScriptEntityStateChangeEvent::CSettingOfPedRelationshipGroupHashParameters parameters((u32)iRelationshipGroup);
						    CScriptEntityStateChangeEvent::Trigger(pPed->GetNetworkObject(), parameters);
#if __BANK
							scriptDisplayf("%s: Frame: %i, SET_PED_RELATIONSHIP_GROUP_HASH(%s) is trying to set relationship group on network clone (or migrating ped): Ped %s %p, PedIndex - %i", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetFrameCount(), pGroup->GetName().GetCStr(),  pPed->GetDebugName(), pPed, PedIndex);
#endif //__BANK
                        }
					}
					else
					{
#if __DEV
						CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(false);
						bool bWasFriendlyWithAnyTarget = pPedTargetting && pPedTargetting->IsFriendlyWithAnyTarget();
#endif
						pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);

						if (pGroup->GetShouldBlipPeds())
						{
							SetRelationshipGroupBlipForPed(pPed, true);
						}
						else
						{
							SetRelationshipGroupBlipForPed(pPed, false);
						}

#if __BANK
						scriptDisplayf("%s: Frame: %i, SET_PED_RELATIONSHIP_GROUP_HASH(%s) is trying to set relationship group on local Ped %s %p, PedIndex - %i, ShouldBlipPeds - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetFrameCount(), pGroup->GetName().GetCStr(),  pPed->GetDebugName(), pPed, PedIndex, pGroup->GetShouldBlipPeds() ? "true" : "false");
#endif //__BANK

#if __DEV
						bool bNowFriendlyWithAnyTarget = pPedTargetting && pPedTargetting->IsFriendlyWithAnyTarget();
						scriptAssertf( bWasFriendlyWithAnyTarget || !bNowFriendlyWithAnyTarget, "%s: SET_RELATIONSHIP_GROUP: setting a char to like chars its already fighting, please use clear char tasks!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
					}
				}
			}
#if __DEV
			else
			{
				CRelationshipManager::PrintRelationshipGroups();
			}
#endif
		}
	}
	void CommandSetPedInformRespectedFriends(int PedIndex, float Distance, int MaximumNumber)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			u8 uMaxNumber = scriptVerifyf(MaximumNumber <= CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE, "CommandSetPedInformRespectedFriends: MaximumNumber too high.") ? static_cast<u8>(MaximumNumber) : CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE;
			pPed->GetPedIntelligence()->SetInformRespectedFriends(Distance, uMaxNumber);
		}
	}
	
	int CommandGetRelationshipBetweenPeds(int FirstPedIndex, int SecondPedIndex )
	{
		const CPed *pFirstPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(FirstPedIndex);
		const CPed *pSecondPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(SecondPedIndex);

		if (pFirstPed && pSecondPed)
		{
			CRelationshipGroup* pFirstRelGrp =  pFirstPed->GetPedIntelligence()->GetRelationshipGroup(); 
			int secondRelGrpIndex = pSecondPed->GetPedIntelligence()->GetRelationshipGroupIndex();
			
			return pFirstRelGrp->GetRelationship(secondRelGrpIndex); 
		}

		return ACQUAINTANCE_TYPE_INVALID; 
	}

	int CommandGetPedRelationshipGroupDefaultHash(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if(pPed)
		{
			return (int)pPed->GetPedIntelligence()->GetRelationshipGroupDefault()->GetName().GetHash(); 
		}
		return 0; 
	}

	int CommandGetPedRelationshipGroupHash(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		
		if(pPed)
		{
			return (int)pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash(); 
		}
		return 0; 
	}
	
	int CommandGetRelationshipBetweenGroups(int iRelationshipGroupA,int iRelationshipGroupB)
	{
		CRelationshipGroup* pGroupA = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroupA);
		CRelationshipGroup* pGroupB = CRelationshipManager::FindRelationshipGroup((u32)iRelationshipGroupB);
		
		if (SCRIPT_VERIFY(pGroupA, "GET_RELATIONSHIP_BETWEEN_GROUPS - Cannot find the first group"))
		{
			if (SCRIPT_VERIFY(pGroupB, "GET_RELATIONSHIP_BETWEEN_GROUPS - Cannot find the second group"))
			{
				return pGroupA->GetRelationship(pGroupB->GetIndex()); 
			}	
			
		}
		return ACQUAINTANCE_TYPE_INVALID; 
	}

	void CommandSetRelationshipGroupAffectsWantedLevel(int iRelGroup, bool bShouldAffectWantedLevel)
	{
		if(!bShouldAffectWantedLevel)
		{
			CScriptResource_SetRelGroupDontAffectWantedLevel relGroupOverride(iRelGroup);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(relGroupOverride);
		}
		else
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL, iRelGroup);
		}
	}

	void CommandSetBlipPedsInRelationshipGroup(int iRelGroup, bool bShouldPedsInRelGroupBeBlipped)
	{
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelGroup);
		if (SCRIPT_VERIFY(pRelGroup, "SET_BLIP_PEDS_IN_RELATIONSHIP_GROUP - Cannot find the relationship group"))
		{
			pRelGroup->SetShouldBlipPeds(bShouldPedsInRelGroupBeBlipped);

			u32 IndexOfGroupThatHasJustBeenModified = pRelGroup->GetIndex();

			// Loop through entire ped pool - fwPool<CPed>::GetSlot() already ignores cached peds
			CPed::Pool* pPedPool = CPed::GetPool();
			const s32 pedPoolSize = pPedPool->GetSize();
			for(s32 i=0; i < pedPoolSize; i++)
			{
				CPed *pPedToCheck = pPedPool->GetSlot(i);
				if (pPedToCheck && !pPedToCheck->IsDead() && !pPedToCheck->IsInjured() && pPedToCheck->GetPedIntelligence())
				{
					const CRelationshipGroup* pRelGroupForThisPed = pPedToCheck->GetPedIntelligence()->GetRelationshipGroup();
					if (pRelGroupForThisPed->GetIndex() == IndexOfGroupThatHasJustBeenModified)
					{
						SetRelationshipGroupBlipForPed(pPedToCheck, bShouldPedsInRelGroupBeBlipped);
					}
				}
			}
		}
	}

	void CommandTellGroupPedsInAreaToAttack(int TargetPedIndex, const scrVector & scrVectorCenterCoords, float radius, int RelationshipGroup)
	{
		const CPed* pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetPedIndex);
		CRelationshipGroup* pAttackingGroup = CRelationshipManager::FindRelationshipGroup((u32)RelationshipGroup);
		if (scriptVerifyf(pTargetPed, "TELL_RELATED_PEDS_IN_AREA_TO_ATTACK - expected a valid target ped index!\n") &&  
			scriptVerifyf(pAttackingGroup, "TELL_RELATED_PEDS_IN_AREA_TO_ATTACK - a valid relationship group hash must be provided!\n"))
		{
			Vec3V vCenter(scrVectorCenterCoords);
			CPed* pRelatedPed = NULL;

			// Loop through all peds
			CPed::Pool* pPedPool = CPed::GetPool();
			for(s32 i=0; i < pPedPool->GetSize(); i++) 
			{
				pRelatedPed = pPedPool->GetSlot(i);
				//check if it is a valid ped
				if (pRelatedPed && !pRelatedPed->IsDead() && !pRelatedPed->IsInjured() &&
					pRelatedPed->GetPedIntelligence() && pRelatedPed != pTargetPed)
				{
					//check the relationship group to see if it matches the parameter
					CRelationshipGroup* pOtherRelationshipGroup = pRelatedPed->GetPedIntelligence()->GetRelationshipGroup();
					if (scriptVerifyf(pOtherRelationshipGroup, "TELL_RELATED_PEDS_IN_AREA_TO_ATTACK - expected a valid relationship group for other ped!\n")
						&& pOtherRelationshipGroup == pAttackingGroup)
					{
						Vec3V vOther = pRelatedPed->GetTransformPtr()->GetPosition();
						//check the distance to the target ped
						if (IsLessThanAll(DistSquared(vOther, vCenter), ScalarVFromF32(rage::square(radius))))
						{
							//tell the ped to start attacking...will be ignored if a higher priority event takes place at the same time
							CEventShoutTargetPosition event(pRelatedPed, const_cast<CPed*>(pTargetPed));
							pRelatedPed->GetPedIntelligence()->AddEvent(event);
						}
					}
				}
			} //end for each ped
		}
	}

	bool CommandIsPedRespondingToEvent(int PedIndex, int EventType)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			return (pPed->GetPedIntelligence()->IsRespondingToEvent(EventType));
		}
		return false;
	}

	int CommandGetPedIndexFromFiredEvent(int PedIndex, int EventType)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CEvent* pEvent = pPed->GetPedIntelligence()->GetEventOfType(EventType);
			if (pEvent && pEvent->GetSourcePed())
			{
				return CTheScripts::GetGUIDFromEntity(*pEvent->GetSourcePed());
			}
		}

		return NULL_IN_SCRIPTING_LANGUAGE;
	}

	bool CommandGetPosFromFiredEvent(int PedIndex, int EventType, Vector3& OutPos)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CEvent* pEvent = pPed->GetPedIntelligence()->GetEventOfType(EventType);
			if (pEvent)
			{
				OutPos = pEvent->GetSourcePos();
				return true;
			}
		}
		OutPos = VEC3_ZERO;
		return false;
	}

	void CommandSetPedCanBeTargetedWithoutLos(int PedIndex, bool bTargettableWithNoLos)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TargettableWithNoLos, bTargettableWithNoLos );
		}
	}

	void CommandSetPedFiringPattern(int PedIndex, int FiringPatternHash)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (scriptVerifyf(!NetworkInterface::IsGameInProgress() || !pPed->IsAPlayerPed(), "SET_PED_FIRING_PATTERN -  This script command is not supported for players in network game scripts"))
			{
				const CFiringPatternInfo* pFiringPatternInfo = FiringPatternHash != 0 ? CFiringPatternInfoManager::GetInfo((u32)FiringPatternHash) : NULL;
				if(scriptVerifyf(pFiringPatternInfo, "SET_PED_FIRING_PATTERN - A valid firing pattern must be provided"))
				{
					pPed->GetPedIntelligence()->GetCombatBehaviour().SetFiringPattern((u32)FiringPatternHash);
				}
			}
		}
	}

	void CommandSetPedShootRate(int PedIndex, int ShootRate)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress() || !pPed->IsAPlayerPed(), "SET_PED_SHOOT_RATE -  This script command is not supported for players in network game scripts"))
			{
				if(SCRIPT_VERIFY(ShootRate > 0, "SET_PED_SHOOT_RATE - Shoot Rate should be greater than 0"))
				{
					if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_SHOOT_RATE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						pPed->GetPedIntelligence()->GetCombatBehaviour().SetShootRateModifier(static_cast<float>(ShootRate)/100.0f);
					}
				}
			}
		}
	}


	void CommandSetCombatFloat(int PedIndex, int combatAttribute, float fNewValue)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{
			if(SCRIPT_VERIFY(combatAttribute >= 0 && combatAttribute < kMaxCombatFloats, "SET_COMBAT_FLOAT - must pass in a valid combat attribute"))
			{
				pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat((CombatFloats)combatAttribute, fNewValue);
			}
		}
	}

	float CommandGetCombatFloat(int PedIndex, int combatAttribute)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{
			if(SCRIPT_VERIFY(combatAttribute >= 0 && combatAttribute < kMaxCombatFloats, "GET_COMBAT_FLOAT - must pass in a valid combat attribute"))
			{
				return pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat((CombatFloats)combatAttribute);
			}
		}

		return 0.0f;
	}

	void CommandGetGroupSize(int UniqueGroupID, int &ReturnHasLeader, int &ReturnNumberOfFollowers)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);
		if (SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "GET_GROUP_SIZE - Group index is out of range"))
		{
			if (SCRIPT_VERIFY(CPedGroups::ms_groups[iGroupID].IsActive(), "GET_GROUP_SIZE - group isn't active"))
			{
				ReturnHasLeader = (CPedGroups::ms_groups[iGroupID].GetGroupMembership()->GetLeader() ? 1 : 0);
				ReturnNumberOfFollowers = CPedGroups::ms_groups[iGroupID].GetGroupMembership()->CountMembersExcludingLeader();
			}
		}
		else
		{
			ReturnHasLeader = 0;
			ReturnNumberOfFollowers = 0;
		}
	}


	bool CommandDoesGroupExist(int UniqueGroupID)
	{
		bool LatestCmpFlagResult = false;

		//	If we called GetActualGroupId and the group didn't exist then we'd 
		//	get a "Script resource with given id does not exist" assert in scriptHandler::GetResourceReferenceFromId()
		//	Instead just check if a resource with the given unique ID exists
		if (CTheScripts::GetCurrentGtaScriptHandler() && CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueGroupID))
		{
//			const int iGroupID = GetActualGroupId(UniqueGroupID);
//			if ( (iGroupID >= 0) && (iGroupID < CPedGroups::MAX_NUM_GROUPS) )
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}


	int CommandGetPedGroupIndex(int PedIndex)
	{
		int ReturnGroupIndex = -1;

		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedGroup * pGroup = pPed->GetPedsGroup();

			if(pGroup)
			{
				int iGroupIndex = pGroup->GetGroupIndex();
				if(SCRIPT_VERIFY(iGroupIndex >= 0 && iGroupIndex < CPedGroups::MAX_NUM_GROUPS, "GET_PED_GROUP_INDEX - Group index out of range"))
				{
					scriptResource* pResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PEDGROUP, iGroupIndex);

					if (!pResource)
					{
						// we reference ped groups via an id assigned to a group when it is created by the script. Permanent groups are a special case in that they already
						// exist. Therefore we still need to register them with the resource management system so we have an id we can use. 
						if (CPedGroups::ms_groups[iGroupIndex].PopTypeGet() == POPTYPE_PERMANENT)
						{
							CScriptResource_PedGroup pedGroup((u32)iGroupIndex);
							return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(pedGroup);
						}
						else
						{
							SCRIPT_ASSERT(0, "GET_PED_GROUP_INDEX - The ped's group has not been registered with this script as a script resource ");
						}
					}
					else
					{
						return pResource->GetId();
					}
				}
			}
			else
			{
				ReturnGroupIndex = -1;
			}
		}

		return ReturnGroupIndex;
	}


	bool CommandIsPedInGroup(int PedIndex)
	{
		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedGroup * pGroup = pPed->GetPedsGroup();
			return (pGroup != NULL);
		}
		return false;
	}

	int CommandGetPlayerPedIsFollowing(int PedIndex)
	{
		int playerIndex = -1;

		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedGroup* pGroup = pPed->GetPedsGroup();
			CPed* pGroupLeader = pGroup ? pGroup->GetGroupMembership()->GetLeader() : NULL;

			if (pGroupLeader && pGroupLeader->IsAPlayerPed())
			{
				if (pGroupLeader->GetNetworkObject() && pGroupLeader->GetNetworkObject()->GetPlayerOwner())
				{
					playerIndex = pGroupLeader->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
				}
				else
				{
					playerIndex = 0; // the local player in SP
				}
			}
		}

		return playerIndex;
	}

	void CommandSetGroupFormation(int UniqueGroupID, int FormationType)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);
		if(SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "SET_GROUP_FORMATION - Group doesn't exist"))
		{
			CPedGroup * pGroup = &CPedGroups::ms_groups[iGroupID];

			if(SCRIPT_VERIFY(pGroup, "SET_GROUP_FORMATION - Group doesn't exist"))
			{
				if (SCRIPT_VERIFY(pGroup->IsActive(), "SET_GROUP_FORMATION - Group is not active"))
				{
					// Script formations need to be mapped to code formations
					int iActualFormationType = MapScriptableFormationToPedFormation(FormationType);
					if(SCRIPT_VERIFY(iActualFormationType >= 0, "SET_GROUP_FORMATION - Unknown formation type specified"))
					{
						pGroup->SetFormation(iActualFormationType);
					}
				}
			}
		}
	}

	void CommandSetGroupFormationSpacing(int UniqueGroupID, float FormationSpacing, float AdjustSpeedMinDist, float AdjustSpeedMaxDist)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);
		if(SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "SET_GROUP_FORMATION_SPACING - Group doesn't exist"))
		{
			CPedGroup * pGroup = &CPedGroups::ms_groups[iGroupID];
			if(SCRIPT_VERIFY(pGroup, "SET_GROUP_FORMATION_SPACING - Group doesn't exist"))
			{			
				if(SCRIPT_VERIFY(pGroup->IsActive(), "SET_GROUP_FORMATION_SPACING - Group is not active")&&
					SCRIPT_VERIFY(pGroup->GetFormation(), "SET_GROUP_FORMATION_SPACING - Group not a valid formations"))
				{
					Assertf(pGroup->GetFormation()->GetScriptModified()==THREAD_INVALID || pGroup->GetFormation()->GetScriptModified()==CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(), "This ped group's formation is already being set by script %u", pGroup->GetFormation()->GetScriptModified() );

					pGroup->GetFormation()->SetSpacing( FormationSpacing, AdjustSpeedMinDist, AdjustSpeedMaxDist, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() );
				}
			}
		}
	}

	void CommandResetGroupFormationDefaultSpacing(int UniqueGroupID)
	{
		const int iGroupID = GetActualGroupId(UniqueGroupID);
		if(SCRIPT_VERIFY(iGroupID >= 0 && iGroupID < CPedGroups::MAX_NUM_GROUPS, "RESET_GROUP_FORMATION_DEFAULT_SPACING - Group doesn't exist"))
		{
			CPedGroup * pGroup = &CPedGroups::ms_groups[iGroupID];
			if(SCRIPT_VERIFY(pGroup, "RESET_GROUP_FORMATION_DEFAULT_SPACING - Group doesn't exist"))
			{			
				if(SCRIPT_VERIFY(pGroup->IsActive(), "RESET_GROUP_FORMATION_DEFAULT_SPACING - Group is not active")&&
					SCRIPT_VERIFY(pGroup->GetFormation(), "RESET_GROUP_FORMATION_DEFAULT_SPACING - Group not a valid formations"))
				{
					pGroup->GetFormation()->SetDefaultSpacing();
					pGroup->GetFormation()->ResetScriptModified();
				}
			}
		}
	}

	int CommandGetVehiclePedIsEntering(int PedIndex)
	{
		CVehicle *pVehicle = NULL;
		int ReturnVehicleIndex = 0;

		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{
			scriptAssertf(!NetworkInterface::IsGameInProgress() || !NetworkInterface::IsRemotePlayerInNonClonedVehicle(pPed), "%s: GET_VEHICLE_PED_IS_ENTERING - Calling this function for a remote player in a vehicle that has not been cloned on this machine!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				pVehicle = (CVehicle*)pPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
			}
			else
			{
				CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if(pEnterVehicleTask)
				{
					pVehicle = pEnterVehicleTask->GetVehicle();
				}
			}

			if (pVehicle)
			{
#if __DEV
				pVehicle->m_nDEflags.bCheckedForDead = TRUE;
#endif
				ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
			}
			else
			{
				ReturnVehicleIndex = NULL_IN_SCRIPTING_LANGUAGE;
			}
		}

		return ReturnVehicleIndex;
	}

	int CommandGetVehiclePedIsUsing(int PedIndex)
	{
		CVehicle *pVehicle = NULL;
		int ReturnVehicleIndex = 0;

		const CPed * pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
            scriptAssertf(!NetworkInterface::IsGameInProgress() || !NetworkInterface::IsRemotePlayerInNonClonedVehicle(pPed), "%s: GET_VEHICLE_PED_IS_USING - Calling this function for a remote player in a vehicle that has not been cloned on this machine!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
			{
				pVehicle = pPed->GetMyVehicle();
				if (pVehicle)
				{
#if __DEV
					pVehicle->m_nDEflags.bCheckedForDead = TRUE;
#endif
					ReturnVehicleIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);
				}
				else
				{
					ReturnVehicleIndex = NULL_IN_SCRIPTING_LANGUAGE;
				}
			}
			else
			{
				ReturnVehicleIndex = CommandGetVehiclePedIsEntering(PedIndex);
			}
			
		}

		return ReturnVehicleIndex;
	}

	void CommandSetPedGravity(int PedIndex, bool canBeAffectedByGravity)
	{
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_GRAVITY - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				if (canBeAffectedByGravity)
				{
					// Are we extracting z already?
					if (pPed->GetUseExtractedZ())
					{
						pPed->SetUseExtractedZ(false);
					}
				}
				else
				{
					// Are we not extracting z already?
					if (!pPed->GetUseExtractedZ())
					{
						pPed->SetUseExtractedZ(true);
					}
				}
			}
		}
	}

	void CommandSetSwimUnderSurface(int PedIndex, bool bUnder)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, bUnder);
		}
	}

	void CommandSetPedDamage(int PedIndex, int Damage, bool DamageArmourFlag, int InstigatorIndex)
	{
		float FloatDamage = (float) Damage;

		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		CEntity* pInstigator = (InstigatorIndex == NULL_IN_SCRIPTING_LANGUAGE) ? NULL : CTheScripts::GetEntityToModifyFromGUID<CEntity>(InstigatorIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if(pPed)
		{
#if !__FINAL
			if( pPed->IsDebugInvincible() )
				return;
#endif 		

			if (DamageArmourFlag)
			{
				if(FloatDamage <= pPed->GetArmour())
				{
					pPed->SetArmour( pPed->GetArmour() - FloatDamage );
					FloatDamage = 0.0f;
				}
				else
				{
					FloatDamage -= pPed->GetArmour();
					pPed->SetArmour(0.0f);
				}
			}
			pPed->ChangeHealth( -FloatDamage );

			if( pPed->IsInjured() )
			{
				CWeaponDamage::GeneratePedDamageEvent(pInstigator, pPed, WEAPONTYPE_FALL, 0.1f, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, CPedDamageCalculator::DF_None);
			}
		}
	}

	u32 CommandGetTimePedDamagedByWeapon(int PedIndex, int WeaponType)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			const CWeaponInfo* pWeaponInfo = WeaponType != 0 ? CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType) : NULL;
			if (pWeaponInfo)
			{
				return pPed->GetWeaponDamagedTimeByHash(pWeaponInfo->GetHash());
			}
		}
		return 0;
	}

	void CommandSetPedAllowedToDuck(int PedIndex, bool AllowedToCrouchFlag)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_ALLOWED_TO_DUCK - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				if (AllowedToCrouchFlag)
				{
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NotAllowedToCrouch, false );
				}
				else
				{
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NotAllowedToCrouch, true );
				}
			}
		}
	}

	void CommandSetPedNeverLeavesGroup(int PedIndex, bool NeverLeavesGroupFlag)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "COMMAND_SET_PED_NEVER_LEAVES_GROUP - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverLeavesGroup, NeverLeavesGroupFlag );
			}
		}	
	}

	int CommandGetPedType(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	
		if(pPed)
		{
			return pPed->GetPedType();
		}
		return 0;
	}

	void CommandSetPedAsCop(int PedIndex, bool bSetRelationshipGroup)
	{
		// Grab the ped and set their ped type, decision maker and relationship group
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedPopulation::RemovePedFromPopulationCount(pPed);
			pPed->SetPedType(PEDTYPE_COP);
			CPedPopulation::AddPedToPopulationCount(pPed);

			pPed->SetDefaultDecisionMaker();

			if(bSetRelationshipGroup)
			{
				pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
			}
		}
	}

	void CommandSetHealthPendingLastDamageEventOverrideFlag(bool overrideValue)
	{
		if (overrideValue) scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: SET_PED_HEALTH_PENDING_LAST_DAMAGE_EVENT_OVERRIDE_FLAG - This command can only be called with overrideValue=true during a network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (CNetObjPed::ms_playerMaxHealthPendingLastDamageEventOverride != overrideValue)
		{
			scriptDebugf1("%s: SET_PED_HEALTH_PENDING_LAST_DAMAGE_EVENT_OVERRIDE_FLAG - Setting flag to %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), overrideValue ? "true" : "false");
			CNetObjPed::ms_playerMaxHealthPendingLastDamageEventOverride = overrideValue;
		}
	}

	void CommandSetPedMaxHealth(int PedIndex, int MaxHealth)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
        {
            scriptAssertf(!NetworkInterface::IsGameInProgress() || pPed->IsLocalPlayer(), "%s: SET_PED_MAX_HEALTH - This command can only be called on the local player in a network game!", CTheScripts::GetCurrentScriptNameAndProgramCounter());

            pPed->SetMaxHealth( (float) MaxHealth);

            if(pPed->IsLocalPlayer())
            {
                if (SCRIPT_VERIFY(pPed->GetPlayerInfo(),"SET_PED_MAX_HEALTH - GetPlayerInfo returned null for player ped."  ))
                {
                    pPed->GetPlayerInfo()->MaxHealth = static_cast<u16>(MaxHealth);
                }
            }
		}
    }

	int CommandGetPedMaxHealth(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return (int)pPed->GetMaxHealth();
		}
		else
		{
			return 0;
		}
	}

	void CommandSetPedMaxTimeInWater(int PedIndex, float fMaxTimeInSeconds)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if(fMaxTimeInSeconds < 0.0f)
			{
				if(pPed->IsPlayer())
				{
					fMaxTimeInSeconds = PLAYER_MAX_TIME_IN_WATER;
				}
				else
				{
					fMaxTimeInSeconds = PED_MAX_TIME_IN_WATER;
				}
			}
			pPed->SetMaxTimeInWater(fMaxTimeInSeconds);
		}
	}

	void CommandSetPedMaxTimeUnderwater(int PedIndex, float fMaxTimeInSeconds)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetMaxTimeUnderwater(fMaxTimeInSeconds);
		}
	}

	void CommandSetGroupPedDucksWhenAimedAt(int PedIndex, bool b)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "COMMAND_SET_PED_MAX_TIME_UNDERWATER - This script command is not allowed in network game scripts!"))
		{	
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockGroupPedAimedAtResponse, !b );
			}
		}
	}

	void SetPedCanBeKnockedOffVehicle(CPed *pPed, int CanBeKnockedOffFlag)
	{
		if (pPed)
		{
			pPed->m_PedConfigFlags.SetCantBeKnockedOffVehicle( CanBeKnockedOffFlag );

			// if ped is already on a bike, need to set/clear the Auto-detach on ragdoll flag
			if(pPed->GetIsAttachedInCar() && pPed->GetAttachParent() && ((CPhysical *) pPed->GetAttachParent())->GetIsTypeVehicle()
				&& (((CVehicle*)pPed->GetAttachParent())->GetVehicleModelInfo()->GetCanPassengersBeKnockedOff() ))
			{
				if(pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle() == KNOCKOFFVEHICLE_NEVER)
					pPed->SetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL, false);
				else
					pPed->SetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL, true);

			}
		}
	}

	void CommandSetCorpseRagdollFriction(int PedIndex, float friction)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			pPed->SetCorpseRagdollFriction(friction);
	}

	void CommandSetCorpseSleepTolerances(int UNUSED_PARAM(PedIndex), float UNUSED_PARAM(velTolerance), float UNUSED_PARAM(internalVelTolerance))
	{
		Assertf(0, "SET_CORPSE_SLEEP_TOLERANCES is currently not supported now that default ragdolls sleep tolerances are so loose.  You can remove this call from script.");

		//if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS))
		//{
		//	if (SCRIPT_VERIFY(pPed->IsDead(), "SET_CORPSE_SLEEP_TOLERANCES - The ped isn't (officially) dead yet, input tolerances won't be used"))
		//	{
		//		CTaskDyingDead* pDeadTask = (CTaskDyingDead*) pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD);
		//		if (SCRIPT_VERIFY(pDeadTask, "SET_CORPSE_SLEEP_TOLERANCES - The ped has not started the dead task yet, input tolerances won't be used"))
		//		{
		//			pDeadTask->SetVelTolerance(velTolerance);
		//			pDeadTask->SetInternalVelTolerance(internalVelTolerance);
		//			pDeadTask->InitSleep(pPed);
		//		}
		//	}
		//}
	}

	void CommandSetCorpseSleepsAlone(int UNUSED_PARAM(PedIndex), bool UNUSED_PARAM(set))
	{
		Assertf(0, "SET_CORPSE_SLEEPS_ALONE is currently not supported.  You can remove this call from script.");

		//if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS))
		//{
		//	if (SCRIPT_VERIFY(pPed->IsFatallyInjured() && pPed->GetRagdollInst(), "SET_CORPSE_SLEEPS_ALONE - The ped isn't (officially) dead yet, input value won't be set"))
		//	{
		//		pPed->GetRagdollInst()->SetInstFlag(phInst::FLAG_SLEEPS_ALONE, set);
		//	}
		//}
	}

	void CommandSetPedCanBeKnockedOffVehicle(int PedIndex, int CanBeKnockedOffFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if(pPed)
		{
            if(SCRIPT_VERIFY((!NetworkInterface::IsGameInProgress() || !pPed->IsNetworkClone()), "SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE - Can only be called on a local ped"))
            {
			    if (SCRIPT_VERIFY((CanBeKnockedOffFlag >=0) && (CanBeKnockedOffFlag <=KNOCKOFFVEHICLE_HARD), "SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE - Invalid Knockoff Type"))
			    {
				    SetPedCanBeKnockedOffVehicle(pPed, CanBeKnockedOffFlag);
			    }
            }
		}
	}

	void CommandDeprecatedCommandWarningSetPedCanBeKnockedOffBike(int UNUSED_PARAM(PedIndex), int UNUSED_PARAM(CanBeKnockedOffFlag))
	{
		SCRIPT_ASSERT(false, "SET_PED_CAN_BE_KNOCKED_OFF_BIKE - This command is deprecated; use SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE instead.");
	}
	void CommandDeprecatedCommandWarningCanKnockPedOffBike(int UNUSED_PARAM(PedIndex))
	{
		SCRIPT_ASSERT(false, "CAN_KNOCK_PED_OFF_BIKE - This command is deprecated; use CAN_KNOCK_PED_OFF_VEHICLE instead.");
	}
	void CommandDeprecatedCommandWarningKnockPedOffBike(int UNUSED_PARAM(PedIndex))
	{
		SCRIPT_ASSERT(false, "KNOCK_PED_OFF_BIKE - This command is deprecated; use KNOCK_PED_OFF_VEHICLE instead.");
	}

	// shared code for the two switch to ragdoll commands
	bool SwitchToRagdollCode(CPed* pPed, int MaxTime, bool bForceScriptControl, CTask* pTaskNM)
	{
		bool bReturn = false;
		if SCRIPT_VERIFY (MaxTime < MAX_UINT16, "SWITCH_PED_TO_RAGDOLL - Max time must be less than 65535, use 0 for infinite time")
		{
			if(pPed)
			{
				if (SCRIPT_VERIFY(pPed->GetRagdollState()!=RAGDOLL_STATE_ANIM_LOCKED, "SWITCH_PED_TO_RAGDOLL - Cannot activate the ragdoll because it has been locked. Has script called SET_PED_CAN_RAGDOLL(FALSE)?"))
				{
					// force the setting of this flag, or it might not happen fast enough to stop the ped getting killed
					pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll, bForceScriptControl );

					// if this ped's never been PreRendered yet, force it to happen now so that the ragdoll can then be used
					if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_PRESETUP)
					{
						pPed->ForceRagdollPreSetup();
					}

					if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM)
					{
						if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_SCRIPT))
						{
							task_commands::UpdatePedRagdollBoundsForScriptActivation(pPed);					

							CEventSwitch2NM event(MaxTime, pTaskNM, true);
							pTaskNM = NULL;
							pPed->SwitchToRagdoll(event);
							bReturn = true;
						}
					}
					else if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS && pTaskNM
						&& (pTaskNM->GetTaskType()==CTaskTypes::TASK_NM_SCRIPT_CONTROL || pTaskNM->GetTaskType()==CTaskTypes::TASK_NM_FALL_DOWN))
					{
						CEventSwitch2NM event(MaxTime, pTaskNM, true);
						pTaskNM = NULL;

						pPed->GetPedIntelligence()->AddEvent(event);

						if(pPed->GetRagdollInst()->GetNMAgentID() != -1)
						{
							ART::MessageParams msg;
							pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);
						}

						bReturn = true;
					}

#if !__NO_OUTPUT
					if(!pPed->GetUsingRagdoll())
					{
						// warning if this command didn't work, and try to print possible reasons it may not have
						scriptWarningf("%s SWITCH_PED_TO_RAGDOLL %s Ragdoll failed to activate RS:%d, Fixed:%d, FixWC:%d, Att:%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), pPed->GetRagdollState(), pPed->GetIsAnyFixedFlagSet(), pPed->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION), pPed->GetIsAttached());
					}
#endif
				}
			}

			if(pTaskNM)
				delete pTaskNM;
		}
		return bReturn;
	}
	
	bool CommandCanKnockPedOffVehicle(int PedIndex)
	{
		bool bCanUseRagdoll = false;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed != NULL && SCRIPT_VERIFY(!pPed->IsInjured(), "CAN_KNOCK_PED_OFF_VEHICLE - Check ped is not injured this frame") && 
			pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) &&
			pPed->GetMyVehicle() != NULL && pPed->GetMyVehicle()->GetVehicleModelInfo() != NULL && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetCanPassengersBeKnockedOff() &&
			pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle() != KNOCKOFFVEHICLE_NEVER &&
			CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_SCRIPT_VEHICLE_FALLOUT, pPed->GetMyVehicle(), 1000.0f))
		{
			bCanUseRagdoll = true;
		}

		return bCanUseRagdoll;
	}

	void CommandKnockPedOffVehicle(int PedIndex)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "KNOCK_PED_OFF_VEHICLE - This script command is not allowed in network game scripts!"))
		{	
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				if	(
					SCRIPT_VERIFY(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) == TRUE, "KNOCK_PED_OFF_VEHICLE - Ped is not in a vehicle")&&
					SCRIPT_VERIFY(pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetCanPassengersBeKnockedOff(), "KNOCK_PED_OFF_VEHICLE - Ped is not on a vehicle that he can be knocked off (bike or quad)")&&
					SCRIPT_VERIFY(pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle()!=KNOCKOFFVEHICLE_NEVER, "KNOCK_PED_OFF_VEHICLE - Ped is flagged as KNOCKOFFVEHICLE_NEVER")&&
					SCRIPT_VERIFY(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_SCRIPT_VEHICLE_FALLOUT, pPed->GetMyVehicle(), 1000.0f), "KNOCK_PED_OFF_VEHICLE - the ped cannot ragdoll at this time. Use CAN_PED_RAGDOLL to check if it's safe to ragdoll the ped.")
					)
				{
					// AUDIO, i've just been thrown off my bike audio
					pPed->NewSay("OVER_HANDLEBARS");

					nmDebugf2("[%u] CommandKnockPedOffVehicle:%s(%p) Added through windscreen task (ped knocked from vehicle)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);

					Mat34V lastMat = PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst());

					Mat34V differenceMat;
					UnTransformOrtho(differenceMat, pPed->GetRagdollInst()->GetMatrix(), lastMat);
					CVehicle* pVehicle = pPed->GetMyVehicle();

					// do an explicit attachment update here to
					// make sure the ped is in the correct position
					if (pPed->GetIsAttached())
					{
						pPed->ProcessAllAttachments();
					}

					// Ensure that we maintain the same difference between current/last matrices after processing attachments since we don't want to mess up
					// the initial velocity of the ragdoll
					Transform(lastMat, pPed->GetRagdollInst()->GetMatrix(), differenceMat);
					PHSIM->SetLastInstanceMatrix(pPed->GetRagdollInst(), lastMat);

					pPed->SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_IgnoreSafetyPositionCheck);

					CTask* pTaskRoll = rage_new CTaskNMThroughWindscreen(2000, 30000, false, pVehicle);

					SwitchToRagdollCode(pPed, 30000, false, pTaskRoll);
				}
			}
		}
	}

	void CommandSetCharCoordsNoGang(int PedIndex, const scrVector & scrVecNewCoors)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_COORDS_NO_GANG - This script command is not allowed in network game scripts!"))
		{	
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				Vector3 vNewCoors = Vector3 (scrVecNewCoors);
				CScriptPeds::SetPedCoordinates(pPed, vNewCoors.x, vNewCoors.y, vNewCoors.z, CScriptPeds::SetPedCoordFlag_OffsetZ);	//	false);
			}
		}
	}

	int CommandGetPedAsGroupMember(int UniqueGroupID, int MemberNumber)
	{
		s32 nPedID = NULL_IN_SCRIPTING_LANGUAGE;
		s32 nGroupID = GetActualGroupId(UniqueGroupID);

		if(SCRIPT_VERIFY(nGroupID>=0 && nGroupID<CPedGroups::MAX_NUM_GROUPS, "GET_PED_AS_GROUP_MEMBER - Group ID is out of ranges"))
		{	
			if(SCRIPT_VERIFY(MemberNumber>=0 && MemberNumber<CPedGroupMembership::LEADER, "GET_PED_AS_GROUP_MEMBER - Not a valid member of the group"))
			{	
				CPedGroup& pedGroup=CPedGroups::ms_groups[nGroupID];
				CPed* pMember=pedGroup.GetGroupMembership()->GetMember(MemberNumber);
				
				if(pMember)
				{
					nPedID = CTheScripts::GetGUIDFromEntity(*pMember);
					scriptAssertf(nPedID>=0,"GET_PED_AS_GROUP_MEMBER - Did not find a valid group member - see a coder");
				}
			}
		}
		return nPedID;
	}

	int CommandGetPedAsGroupLeader(int UniqueGroupID)
	{
		s32 nPedID = NULL_IN_SCRIPTING_LANGUAGE;
		s32 nGroupID = GetActualGroupId(UniqueGroupID);

		if(SCRIPT_VERIFY(nGroupID>=0 && nGroupID<CPedGroups::MAX_NUM_GROUPS, "GET_PED_AS_GROUP_LEADER - Group ID is out of ranges"))
		{	
			CPedGroup& pedGroup=CPedGroups::ms_groups[nGroupID];
			CPed* pMember=pedGroup.GetGroupMembership()->GetMember(CPedGroupMembership::LEADER);
			
			if(pMember)
			{
				nPedID = CTheScripts::GetGUIDFromEntity(*pMember);
				scriptAssertf(nPedID>=0,"GET_PED_AS_GROUP_LEADER - Did not find the group leader - see a coder");
			}
		}
		return nPedID;
	}

	void CommandSetPedKeepTask(int PedIndex, bool KeepTasksFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepTasksAfterCleanUp, KeepTasksFlag );
		}
	}

	void CommandSetPedAllowMinorReactionsAsMissionPed(int PedIndex, bool AllowReactions)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowMinorReactionsAsMissionPed, AllowReactions );
		}
	}

	bool CommandIsPedSwimming(int PedIndex)
	{
		bool LatestCmpFlagResult = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (pPed->GetPedIntelligence()->IsPedSwimming())
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedSwimmingUnderWater(int PedIndex)
	{
		bool bIsSwimmingUnderWater = false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
			if(pMotionTask)
			{
				bIsSwimmingUnderWater = pMotionTask->IsUnderWater();
			}
		}

		return bIsSwimmingUnderWater;
	}

	void CommandSetCharCoordsKeepVehicle(int PedIndex, const scrVector & scrVecCoors)
	{
		Vector3 vNewCoors = Vector3 (scrVecCoors);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			int Flags = CScriptPeds::SetPedCoordFlag_WarpGang | CScriptPeds::SetPedCoordFlag_OffsetZ | CScriptPeds::SetPedCoordFlag_WarpCar;
	
			CScriptPeds::SetPedCoordinates(pPed, vNewCoors.x, vNewCoors.y, vNewCoors.z, Flags);	
		}
	}

	void CommandSetPedDiesInVehicle(int PedIndex, bool DieInCarFlag)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_DIES_IN_VEHICLE - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceDieInCar, DieInCarFlag );
			}
		}
	}
	void CommandSetCreateRandomCops(bool CreateRandomCopsFlag)
	{
		CPedPopulation::SetAllowCreateRandomCops(CreateRandomCopsFlag);
		CPedPopulation::SetAllowCreateRandomCopsOnScenarios(CreateRandomCopsFlag);
	}

	void CommandSetCreateRandomCopsNotOnScenarios(bool createRandomCops)
	{
		CPedPopulation::SetAllowCreateRandomCops(createRandomCops);
	}

	void CommandSetCreateRandomCopsOnScenarios(bool createRandomCops)
	{
		CPedPopulation::SetAllowCreateRandomCopsOnScenarios(createRandomCops);
	}

	bool CommandCanCreateRandomCops()
	{
		return CPedPopulation::GetAllowCreateRandomCops();
	}


	void CommandSetPedInCutscene(int PedIndex, bool bShow)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HideInCutscene, !bShow );
		}
	}


	void CommandSetPedAsEnemy(int PedIndex, bool bEnemy) 
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedIsEnemyToPlayer, bEnemy );
		}
	}



	void CommandSetPedCanSmashGlass(int PedIndex, bool CanSmashGlassFlagRagdoll, bool CanSmashGlassFlagWeapon)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_CollideWithGlassRagdoll, CanSmashGlassFlagRagdoll );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_CollideWithGlassWeapon, CanSmashGlassFlagWeapon );
		}
	}

	bool CommandIsPedInAnyTrain(int PedIndex)
	{
		bool LatestCmpFlagResult= false;

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
			{
				if (pPed->GetMyVehicle()->InheritsFromTrain())
				{
					LatestCmpFlagResult = true;
				}
			}
			if (!LatestCmpFlagResult)
			{
				LatestCmpFlagResult = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain);
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedGettingInToAVehicle(int PedIndex)
	{
		bool LatestCmpFlagResult = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				if( pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE ) >= CTaskEnterVehicle::State_Align)
				{
					if( pPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE) )
					{
						LatestCmpFlagResult = true;
					}
				}
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandIsPedTryingToEnterALockedVehicle(int PedIndex) 
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		bool LatestCmpFlagResult = false;
		if(pPed)
		{

			if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE) &&
				pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE) == CTaskOpenVehicleDoorFromOutside::State_TryLockedDoor)
			{
				LatestCmpFlagResult = true;
			}
		}
		return LatestCmpFlagResult;
	}

	bool CommandHasPedClearLosToEntityInFront(int FirstPedIndex, int EntityIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(FirstPedIndex);
		bool LatestCmpFlagResult = false;
		if(pPed)
		{
			const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

			if(pEntity)
			{
				LatestCmpFlagResult = pPed->OurPedCanSeeThisEntity(pEntity, false);
			}
		}
		return LatestCmpFlagResult;
	}

	void CommandSetPedGetOutUpsideDownVehicle(int PedIndex, bool GetOutFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle, GetOutFlag );
		}
	}

	void CommandSetPedCanRemainOnBoatAfterMissionEnds(int PedIndex, bool RemainOnBoatFlag)
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_CAN_REMAIN_ON_BOAT_AFTER_MISSION_ENDS - This script command is not allowed in network game scripts!"))
		{	
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillRemainOnBoatAfterMissionEnds, RemainOnBoatFlag );
			}
		}
	}

	int CommandGetPedDrawableVariation( int PedIndex, int ComponentID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			return (CPedVariationPack::GetCompVar(pPed, static_cast<ePedVarComp>(ComponentID) ) );
		}
		return -1; //this is an invalid id
	}

	int CommandGetNumberOfPedPropDrawableVariations(s32 PedIndex, int AnchorPoint)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>((CModelInfo::GetBaseModelInfo(pPed->GetModelId())));
			if(SCRIPT_VERIFY(pModelInfo, "GET_NUMBER_OF_PED_PROP_DRAWABLE_VARIATIONS - Couldn't find Model Info for this ped"))
			{	
				CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
				if(SCRIPT_VERIFY(pVarInfo, "GET_NUMBER_OF_PED_PROP_DRAWABLE_VARIATIONS - Couldn't find Var Info for this ped's model"))
				{	
					return (pVarInfo->GetMaxNumProps(static_cast<eAnchorPoints>(AnchorPoint)));
				}
			}
		}
		return 0;
	}

	int CommandGetNumberOfPedDrawableVariations(s32 PedIndex, int ComponentID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>((CModelInfo::GetBaseModelInfo(pPed->GetModelId())));
			if(SCRIPT_VERIFY(pModelInfo, "GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS - Couldn't find Model Info for this ped"))
			{	
				CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
				if(SCRIPT_VERIFY(pVarInfo, "GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS - Couldn't find Var Info for this ped's model"))
				{	
					return (pVarInfo->GetMaxNumDrawbls(static_cast<ePedVarComp>(ComponentID)));
				}
			}
		}
		return 0;
	}

	int CommandGetPedTextureVariation(int PedIndex, int ComponentID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			return (CPedVariationPack::GetTexVar(pPed, static_cast<ePedVarComp>(ComponentID) ) );
		}
		return -1;
	}

	int CommandGetNumberOfPedTextureVariations(int PedIndex, int ComponentID, int DrawableID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>((CModelInfo::GetBaseModelInfo(pPed->GetModelId())));
			
			if(SCRIPT_VERIFY(pModelInfo, "GET_NUMBER_OF_PED_TEXTURE_VARIATIONS - Couldn't find Model Info for this ped"))
			{
				CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
				
				if(SCRIPT_VERIFY(pVarInfo, "GET_NUMBER_OF_PED_TEXTURE_VARIATIONS - Couldn't find Var Info for this ped's model"))
				{
					return (pVarInfo->GetMaxNumTex(static_cast<ePedVarComp>(ComponentID), DrawableID));
				}
			}
		}
		return 0;
	}

	int CommandGetNumberOfPedPropTextureVariations(int PedIndex, int AnchorPoint, int PropId)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>((CModelInfo::GetBaseModelInfo(pPed->GetModelId())));

			if(SCRIPT_VERIFY(pModelInfo, "GET_NUMBER_OF_PED_PROP_TEXTURE_VARIATIONS - Couldn't find Model Info for this ped"))
			{
				CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();

				if(SCRIPT_VERIFY(pVarInfo, "GET_NUMBER_OF_PED_PROP_TEXTURE_VARIATIONS - Couldn't find Var Info for this ped's model"))
				{
					return (pVarInfo->GetMaxNumPropsTex(static_cast<eAnchorPoints>(AnchorPoint), PropId));
				}
			}
		}
		return 0;
	}

	int CommandGetPedPaletteVariation( int PedIndex, int ComponentID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			return (CPedVariationPack::GetPaletteVar(pPed, static_cast<ePedVarComp>(ComponentID) ) );
		}
		return 0;
	}

	bool CommandGetPedHasPaletteVariations(int PedIndex, int ComponentID, int DrawableID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if(pPed)
		{	
			CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>((CModelInfo::GetBaseModelInfo(pPed->GetModelId())));
			
			if(SCRIPT_VERIFY(pModelInfo, "GET_PED_HAS_PALETTE_VARIATIONS - Couldn't find Model Info for this ped"))
			{	
				CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
				if(SCRIPT_VERIFY(pVarInfo, "GET_PED_HAS_PALETTE_VARIATIONS - Couldn't find Var Info for this ped's model"))
				{
					return (pVarInfo->HasPaletteTextures(static_cast<ePedVarComp>(ComponentID), DrawableID));
				}
			}
		}
		return false;
	}


	bool CommandGetMpOutfitDataFromMetadata(int& outfitData, int& outfitDataRequest)
	{
		//request struct
		scrValue* pRequestBuffer = reinterpret_cast<scrValue*>(&outfitDataRequest);
		bool bMale = pRequestBuffer->Int != 0;
		pRequestBuffer++;
		int requestedOutfit = pRequestBuffer->Int;

		Displayf("GetMpOutfitData %d %s", requestedOutfit, bMale? "male" : "female");

		CMPOutfitsData* outfitFound = NULL;
		if(!CScriptMetadataManager::GetMPOutfitData(&outfitFound, requestedOutfit, bMale))
		{
			Assertf(0, "Outfit %d %s cannot be found", requestedOutfit, bMale? "male" : "female");
			return false;
		}

		//buffer to write back to
		u8* pOutfitBuffer = reinterpret_cast<u8*>(&outfitData);

		// skip array size prefix
		pOutfitBuffer += sizeof(scrValue);
		for(int i = 0; i < outfitFound->m_ComponentDrawables.GetCount(); i++)
		{
			memcpy(pOutfitBuffer, &outfitFound->m_ComponentDrawables[i], sizeof(int));
			pOutfitBuffer += sizeof(scrValue);
		}

		pOutfitBuffer += sizeof(scrValue);
		for(int i = 0; i < outfitFound->m_ComponentTextures.GetCount(); i++)
		{
			memcpy(pOutfitBuffer, &outfitFound->m_ComponentTextures[i], sizeof(int));
			pOutfitBuffer += sizeof(scrValue);
		}

		pOutfitBuffer += sizeof(scrValue);
		for(int i = 0; i < outfitFound->m_PropIndices.GetCount(); i++)
		{
			memcpy(pOutfitBuffer, &outfitFound->m_PropIndices[i], sizeof(int));
			pOutfitBuffer += sizeof(scrValue);
		}

		pOutfitBuffer += sizeof(scrValue);
		for(int i = 0; i < outfitFound->m_PropTextures.GetCount(); i++)
		{
			memcpy(pOutfitBuffer, &outfitFound->m_PropTextures[i], sizeof(int));
			pOutfitBuffer += sizeof(scrValue);
		}

		pOutfitBuffer += sizeof(scrValue);
		for(int i = 0; i < outfitFound->m_TattooHashes.GetCount(); i++)
		{
			memcpy(pOutfitBuffer, &outfitFound->m_TattooHashes[i], sizeof(int));
			pOutfitBuffer += sizeof(scrValue);
		}

		return true;
	}

	int CommandGetFmMaleShopPedApparelItemIndex(int nameHash)
	{
		return CScriptMetadataManager::GetMPApparelMale(nameHash);
	}

	int CommandGetFmFemaleShopPedApparelItemIndex(int nameHash)
	{
		return CScriptMetadataManager::GetMPApparelFemale(nameHash);
	}
	
	bool CommandIsPedComponentVariationValid(int PedIndex, int ComponentID, int DrawableID, int TextureID)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return pPed->IsVariationValid(static_cast<ePedVarComp>(ComponentID), DrawableID, TextureID);
		}

		return false;
	}

	bool CommandIsPedDrawableGen9Exclusive(int PedIndex, int ComponentID, int DrawableID)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (CGen9ExclusiveAssets::IsGen9ExclusivePedDrawable(pPed, static_cast<ePedVarComp>(ComponentID), DrawableID))
			{
				return true;
			}
		}

		return false;
	}

	void CommandSetPedComponentVariation( int PedIndex, int ComponentID, int DrawableID, int TextureID, int PaletteID)
	{
		scriptDebugf1("CommandSetPedComponentVariation(%d, %s, %d, %d, %d)", PedIndex, (unsigned)ComponentID < PV_MAX_COMP ? varSlotNames[ComponentID] : "<INVALID>",
			DrawableID, TextureID, PaletteID);

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if(pPed)
		{
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			pPed->SetVariation(static_cast<ePedVarComp>(ComponentID), DrawableID, 0, TextureID, PaletteID, (u32)streamingFlags, false);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
		}
	}

	void CommandSetPedRandomComponentVariation(int PedIndex, int race)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, ePVRaceType(race), (u32)streamingFlags);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
			pedDebugf3("SET_PED_RANDOM_COMPONENT_VARIATION -  modelname(%s)", pPed->GetModelName());
		}
	}

	void CommandSetPedRandomProps(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			float freq0 = 0.6f;
			float freq1 = 0.3f;
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			CPedPropsMgr::SetPedPropsRandomly(pPed, freq0, freq1, PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, 0, 0, (u32)streamingFlags);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
		}
	}

	void CommandSetPedDefaultComponentVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		
		if(pPed)
		{
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			pPed->SetVarDefault((u32)streamingFlags);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
			pedDebugf3("SET_PED_DEFAULT_COMPONENT_VARIATION - modelname(%s)", pPed->GetModelName());
		}
	}

	void CommandSetPedBlendFromParents(int targetPedIndex, int parentPed1, int parentPed2, float geomBlend, float texBlend)
	{
		CPed* targetPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(targetPedIndex);
		CPed* parent1 = CTheScripts::GetEntityToModifyFromGUID<CPed>(parentPed1);
		CPed* parent2 = CTheScripts::GetEntityToModifyFromGUID<CPed>(parentPed2);

		if (SCRIPT_VERIFY(targetPed && parent1 && parent2, "One or more invalid peds passed in to SET_PED_BLEND_FROM_PARENTS"))
		{
			pedDisplayf("[HEAD BLEND] Set blend on ped %p from parents %p %p", targetPed, parent1, parent2);

			geomBlend = rage::Clamp(geomBlend, 0.f, 1.f);

			if (texBlend < 0.f)
				texBlend = geomBlend;
			texBlend = rage::Clamp(texBlend, 0.f, 1.f);

			targetPed->RequestBlendFromParents(parent1, parent2, geomBlend, texBlend);
		}
	}

	void CommandUpdatePedChildBlend(int PedIndex)
	{
		CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (SCRIPT_VERIFY(ped, "Invalid ped passed in to UPDATE_PED_CHILD_BLEND"))
		{
			pedDisplayf("[HEAD BLEND] Update child blend on ped %p", ped);
			ped->UpdateChildBlend();
		}
	}

	void CommandSetNewParentBlendValues(int PedIndex, float geomBlend, float texBlend)
	{
		CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (SCRIPT_VERIFY(ped, "Invalid ped passed in to SET_NEW_PARENT_BLEND_VALUES"))
		{
			pedDisplayf("[HEAD BLEND] Set new parent blend values on ped %p", ped);
			if (texBlend < 0.f)
                texBlend = geomBlend;
			ped->SetNewHeadBlendValues(geomBlend, texBlend, 0.f);
		}
	}

	void CommandSetPedHeadBlendData(int PedIndex, int head0, int head1, int head2, int tex0, int tex1, int tex2, float headBlend, float texBlend, float varBlend, bool parent)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

#if !__FINAL
		if ((Channel_ped.FileLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_ped.TtyLevel >= DIAG_SEVERITY_DISPLAY))
		{
	        pedDisplayf("[HEAD BLEND] SET_PED_HEAD_BLEND_DATA called on ped index %d (%p): head(%d, %d, %d) tex(%d, %d, %d) headblend %f, texBlend %f, varBlend %f, parent %s",
				PedIndex, pPed, head0, head1, head2, tex0, tex1, tex2, headBlend, texBlend, varBlend, parent?"true":"false");
	        scrThread::PrePrintStackTrace();
		}
#endif

		if(pPed)
		{
			CPedHeadBlendData data;
			data.m_head0 = (u8) head0;
			data.m_head1 = (u8) head1;
			data.m_head2 = (u8) head2;
			data.m_tex0  = (u8) tex0;
			data.m_tex1  = (u8) tex1;
			data.m_tex2  = (u8) tex2;
			data.m_headBlend = headBlend;
			data.m_texBlend = texBlend;
			data.m_varBlend = varBlend;
			data.m_isParent = parent;
			pPed->SetHeadBlendData(data);
		}
	}

	bool CommandGetHeadBlendData(int PedIndex, scrPedHeadBlendData* targetBlendData)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			const CPedHeadBlendData* currBlendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

			if (currBlendData)
			{
				targetBlendData->m_head0.Uns = currBlendData->m_head0;
				targetBlendData->m_head1.Uns = currBlendData->m_head1;
				targetBlendData->m_head2.Uns = currBlendData->m_head2;
				targetBlendData->m_tex0.Uns = currBlendData->m_tex0;
				targetBlendData->m_tex1.Uns = currBlendData->m_tex1;
				targetBlendData->m_tex2.Uns = currBlendData->m_tex2;
				targetBlendData->m_headBlend.Float = currBlendData->m_headBlend;
				targetBlendData->m_texBlend.Float = currBlendData->m_texBlend;
				targetBlendData->m_varBlend.Float = currBlendData->m_varBlend;
				targetBlendData->m_isParent.Int = (int)currBlendData->m_isParent;

				return true;
			}
		}

		return false;
	}

	void CommandUpdatePedHeadBlendData(int PedIndex, float headBlend, float texBlend, float varBlend)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			Displayf("[HEAD BLEND] Update blend values on ped %p", pPed);
			pPed->SetNewHeadBlendValues(headBlend, texBlend, varBlend);
		}
	}

    int CommandGetNumEyeColors()
	{
        return MESHBLENDMANAGER.GetNumEyeColors();
	}

    void CommandSetHeadBlendEyeColor(int PedIndex, int colorIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			Displayf("[HEAD BLEND] Set blend eye colors on ped %p", pPed);
			pPed->SetHeadBlendEyeColor(colorIndex);
		}
	}

	int CommandGetHeadBlendEyeColor(int PedIndex)
	{
		int retVal = -1;

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			Displayf("[HEAD BLEND] Get blend eye colors on ped %p", pPed);
			retVal = pPed->GetHeadBlendEyeColor();
		}

		return(retVal);
	}

	void CommandSetPedHeadOverlay(int PedIndex, int slot, int tex, float blend)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed && Verifyf(slot > -1 && slot < HOS_MAX, "Invalid slot %d passed to SET_PED_HEAD_OVERLAY. Needs to be [0, %d)", slot, HOS_MAX))
		{
			Displayf("[HEAD BLEND] Set head overlay on ped %p (Slot %d, tex %d, blend %f)", pPed, slot, tex, blend);
			if (tex < 0)
				tex = 255;

			pPed->SetHeadOverlay(static_cast<eHeadOverlaySlots>(slot), tex, blend, blend);
		}
	}

	void CommandUpdatePedHeadOverlayBlend(int PedIndex, int slot, float blend)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed && Verifyf(slot > -1 && slot < HOS_MAX, "Invalid slot %d passed to SET_PED_HEAD_OVERLAY. Needs to be [0, %d)", slot, HOS_MAX))
		{
			Displayf("[HEAD BLEND] Update head overlay on ped %p", pPed);

			pPed->UpdateHeadOverlayBlend(static_cast<eHeadOverlaySlots>(slot), blend, blend);
		}
	}

	int CommandGetPedHeadOverlay(int PedIndex, int slot)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed && Verifyf(slot > -1 && slot < HOS_MAX, "Invalid slot %d passed to GET_PED_HEAD_OVERLAY. Needs to be [0, %d)", slot, HOS_MAX))
		{
			return pPed->GetHeadOverlay(static_cast<eHeadOverlaySlots>(slot));
		}
		return -1;
	}

	int CommandGetPedHeadOverlayNum(int slot)
	{
		int count = 0;
		eHeadOverlaySlots eSlot = static_cast<eHeadOverlaySlots>(slot);
		switch (eSlot)
		{
		case HOS_BLEMISHES:
			count = CPedVariationStream::GetFacialOverlays().m_blemishes.GetCount();
			break;
		case HOS_FACIAL_HAIR:
			count = CPedVariationStream::GetFacialOverlays().m_facialHair.GetCount();
			break;
		case HOS_EYEBROW:
			count = CPedVariationStream::GetFacialOverlays().m_eyebrow.GetCount();
			break;
		case HOS_AGING:
			count = CPedVariationStream::GetFacialOverlays().m_aging.GetCount();
			break;
		case HOS_MAKEUP:
			count = CPedVariationStream::GetFacialOverlays().m_makeup.GetCount();
			break;
		case HOS_BLUSHER:
			count = CPedVariationStream::GetFacialOverlays().m_blusher.GetCount();
			break;
		case HOS_DAMAGE:
			count = CPedVariationStream::GetFacialOverlays().m_damage.GetCount();
			break;
		case HOS_BASE_DETAIL:
			count = CPedVariationStream::GetFacialOverlays().m_baseDetail.GetCount();
			break;
		case HOS_SKIN_DETAIL_1:
			count = CPedVariationStream::GetFacialOverlays().m_skinDetail1.GetCount();
			break;
		case HOS_SKIN_DETAIL_2:
			count = CPedVariationStream::GetFacialOverlays().m_skinDetail2.GetCount();
			break;
		case HOS_BODY_1:
			count = CPedVariationStream::GetFacialOverlays().m_bodyOverlay1.GetCount();
			break;
		case HOS_BODY_2:
			count = CPedVariationStream::GetFacialOverlays().m_bodyOverlay2.GetCount();
			break;
		case HOS_BODY_3:
			count = CPedVariationStream::GetFacialOverlays().m_bodyOverlay3.GetCount();
			break;
		default:
			count = 0;
		}

		return count;
	}

	void CommandSetPedHeadOverlayTint(int PedIndex, int slot, int rampType, int tint, int tint2)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed && Verifyf(slot > -1 && slot < HOS_MAX, "Invalid slot %d passed to SET_PED_HEAD_OVERLAY_TINT. Needs to be [0, %d)", slot, HOS_MAX))
		{
			Displayf("[HEAD BLEND] Set head overlay tint on ped %d (%p): slot %d ramp %d tint1 %d tint2 %d", PedIndex, pPed, slot, rampType, tint, tint2);
			pPed->SetHeadOverlayTintIndex(static_cast<eHeadOverlaySlots>(slot), (eRampType)rampType, (u32)tint, (u32)tint2);
		}
	}

	void CommandSetPedHairTint(int PedIndex, int tint, int tint2)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			Displayf("[HEAD BLEND] Set hair tint on ped %d (%p): tint1 %d tint2 %d", PedIndex, pPed, tint, tint2);
			pPed->SetHairTintIndex((u32)tint, (u32)tint2);
		}
	}

	bool CommandGetPedHairTint(int PedIndex, int& outTint, int& outTint2)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			return pPed->GetHairTintIndex((u32&)outTint, (u32&)outTint2);
		}

		return false;
	}

	int CommandGetNumPedHairTints()
	{
		return MESHBLENDMANAGER.GetRampCount(RT_HAIR);
	}

	int CommandGetNumPedMakeupTints()
	{
		return MESHBLENDMANAGER.GetRampCount(RT_MAKEUP);
	}

	void CommandGetPedHairTintColor(int tint, int& red, int& green, int& blue)
	{
		Color32 col = MESHBLENDMANAGER.GetRampColor(RT_HAIR, tint);
		red = col.GetRed();
		green = col.GetGreen();
		blue = col.GetBlue();
	}

	void CommandGetPedMakeupTintColor(int tint, int& red, int& green, int& blue)
	{
		Color32 col = MESHBLENDMANAGER.GetRampColor(RT_MAKEUP, tint);
		red = col.GetRed();
		green = col.GetGreen();
		blue = col.GetBlue();
	}

	bool CommandIsPedHairTintForCreator(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_creatorHairColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_creatorHairColors[i].m_primaryColor == (u8)tint)
				return true;

		return false;
	}

	int CommandGetDefaultSecondaryTintForCreator(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return tint;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_creatorHairColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_creatorHairColors[i].m_primaryColor == (u8)tint)
				return (int)CPedVariationStream::GetMpTintData().m_creatorHairColors[i].m_defaultSecondaryColor;

		return tint;
	}

	bool CommandIsPedAccsTintForCreator(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_creatorAccsColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_creatorAccsColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedLipstickTintForCreator(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_creatorLipstickColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_creatorLipstickColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedBlushTintForCreator(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_creatorBlushColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_creatorBlushColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedHairTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberHairColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberHairColors[i].m_primaryColor == (u8)tint)
				return true;

		return false;
	}

	int CommandGetDefaultSecondaryTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return tint;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberHairColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberHairColors[i].m_primaryColor == (u8)tint)
				return (int)CPedVariationStream::GetMpTintData().m_barberHairColors[i].m_defaultSecondaryColor;

		return tint;
	}

	bool CommandIsPedAccsTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberAccsColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberAccsColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedLipstickTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberLipstickColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberLipstickColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedBlushTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberBlushColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberBlushColors[i] == (u8)tint)
				return true;

		return false;
	}

	bool CommandIsPedBlushFacepaintTintForBarber(int tint)
	{
		if (tint < 0 || tint >= MAX_RAMP_COLORS)
			return false;

		const s32 numTints = CPedVariationStream::GetMpTintData().m_barberBlushFacepaintColors.GetCount();
		for (s32 i = 0; i < numTints; ++i)
			if (CPedVariationStream::GetMpTintData().m_barberBlushFacepaintColors[i] == (u8)tint)
				return true;

		return false;
	}

	// FA: TODO: this is the same code as in CPedVariationStream::RequestStreamPedFilesInternal and it should
	// really be combined into a nice utility function. Not right when we ship though...
	u32 GetAssetHash(CPedModelInfo* pmi, u32 hairDrawableId)
	{
		CPedVariationInfoCollection* pVarInfo = pmi->GetVarInfo();
		if (!pVarInfo)
			return 0;

		u32 pedFolderHash = atPartialStringHash(pmi->GetStreamFolder());
		u32 pedFolderWithDlcHash = atPartialStringHash("_", pedFolderHash);
		pedFolderHash = atPartialStringHash("/", pedFolderHash);

		const char* dlcName = pVarInfo->GetDlcNameFromDrawableIdx(PV_COMP_HAIR, hairDrawableId);
		hairDrawableId = pVarInfo->GetDlcDrawableIdx(PV_COMP_HAIR, hairDrawableId);

		u32 drawableHash = pedFolderHash;
		if (dlcName)
		{
			drawableHash = atPartialStringHash(dlcName, pedFolderWithDlcHash);
			drawableHash = atPartialStringHash("/", drawableHash);
		}
		drawableHash = atPartialStringHash(varSlotNames[PV_COMP_HAIR], drawableHash);
		drawableHash = atPartialStringHash("_", drawableHash);
		char drawblIdxStr[4] = {0};
		drawableHash = atPartialStringHash(CPedVariationStream::CustomIToA3(hairDrawableId, drawblIdxStr), drawableHash);
		drawableHash = atPartialStringHash("_r", drawableHash);
		drawableHash = atFinalizeHash(drawableHash);

		return drawableHash;
	}

	const MpTintAssetData* GetHairTintAssetData(int pedModelHash, int hairDrawableId)
	{
		fwModelId pedModelId;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32)pedModelHash, &pedModelId);

		if (SCRIPT_VERIFY(pModelInfo, "HAIR_STYLE_HAS_SECONDARY_HAIR_COLOR - this is not a valid model index"))
		{
			if (SCRIPT_VERIFY(pModelInfo->GetModelType() == MI_TYPE_PED, "HAIR_STYLE_HAS_SECONDARY_HAIR_COLOR - not a ped model"))
			{
				CPedModelInfo* pmi = (CPedModelInfo*)pModelInfo;
				u32 hash = GetAssetHash(pmi, hairDrawableId);

				const s32 numAssets = CPedVariationStream::GetMpTintData().m_assetData.GetCount();
				for (s32 i = 0; i < numAssets; ++i)
				{
					if (CPedVariationStream::GetMpTintData().m_assetData[i].m_assetName.GetHash() != hash)
						continue;

					return &CPedVariationStream::GetMpTintData().m_assetData[i];
				}
			}
		}

		return NULL;
	}

	bool CommandHairStyleHasSecondaryHairColor(int pedModelHash, int hairDrawableId)
	{
		const MpTintAssetData* tintData = GetHairTintAssetData(pedModelHash, hairDrawableId);
		if (!tintData)
			return false;

		return tintData->m_secondaryHairColor;
	}

	bool CommandHairStyleHasSecondaryAccsColor(int pedModelHash, int hairDrawableId)
	{
		const MpTintAssetData* tintData = GetHairTintAssetData(pedModelHash, hairDrawableId);
		if (!tintData)
			return false;

		return tintData->m_secondaryAccsColor;
	}

	int CommandGetTintIndexForLastGenHairTexture(int pedModelHash, int hairDrawableId, int hairTextureId)
	{
		const MpTintAssetData* tintData = GetHairTintAssetData(pedModelHash, hairDrawableId);

		if (tintData)
		{
			const s32 numMappings = tintData->m_lastGenMapping.GetCount();
			for (s32 i = 0; i < numMappings; ++i)
			{
				if (tintData->m_lastGenMapping[i].m_texIndex == (u8)hairTextureId)
				{
					scriptDebugf2("GET_TINT_INDEX_FOR_LAST_GEN_HAIR_TEXTURE: Ped model hash 0x%08x, drawable %d, texture %d = palette row %d",
						pedModelHash, hairDrawableId, hairTextureId, tintData->m_lastGenMapping[i].m_paletteRow);
					return tintData->m_lastGenMapping[i].m_paletteRow;
				}
			}
		}
		scriptDebugf1("GET_TINT_INDEX_FOR_LAST_GEN_HAIR_TEXTURE: Tint data not found for ped model hash 0x%08x, drawable %d, texture %d",
			pedModelHash, hairDrawableId, hairTextureId);
		return 0;
	}

	void CommandSetPedMicroMorph(int PedIndex, int morphType, float blend)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed && Verifyf(morphType > -1 && morphType < MMT_MAX, "Invalid micro morph type %d passed to SET_PED_MICRO_MORPH. Needs to be [0, %d]", morphType, MMT_MAX))
		{
			Displayf("[HEAD BLEND] Setting %.2f on slot %d, ped index %d (%p)", blend, morphType, PedIndex, pPed);
			pPed->MicroMorph(static_cast<eMicroMorphType>(morphType), blend);
		}
	}

	bool CommandHasPedHeadBlendFinished(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);

		if(pPed)
		{
			return pPed->HasHeadBlendFinished();
		}

		return true;
	}
    
	void CommandFinalizeHeadBlend(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		scriptDebugf1("CommandFinalizeHeadBlend(%d) = ped %p", PedIndex, pPed);

#if !__FINAL
        Displayf("[HEAD BLEND] FINALIZE_HEAD_BLEND called on ped index %d (%p)", PedIndex, pPed);
        scrThread::PrePrintStackTrace();
#endif

		if(pPed)
		{
			pPed->FinalizeHeadBlend();
		}
	}

    void CommandSetHeadBlendPaletteColor(int PedIndex, int red, int green, int blue, int colorIndex)
    {
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
        {
            if (SCRIPT_VERIFY((red >= 0 && red <= 255), "SET_HEAD_BLEND_PALETTE_COLOR - Red colour component invalid! [0, 255]") &&
                SCRIPT_VERIFY((green >= 0 && green <= 255), "SET_HEAD_BLEND_PALETTE_COLOR - Red colour component invalid! [0, 255]") &&
                SCRIPT_VERIFY((blue >= 0 && blue <= 255), "SET_HEAD_BLEND_PALETTE_COLOR - Red colour component invalid! [0, 255]"))
            {	
				Displayf("[HEAD BLEND] Set head blend palette color on ped %p", pPed);
				if (colorIndex < 0 || colorIndex >= MAX_PALETTE_COLORS)
					colorIndex = 0;

                pPed->SetHeadBlendPaletteColor((u8)red, (u8)green, (u8)blue, (u8)colorIndex);
            }
        }
    }

    void CommandDisableHeadBlendPaletteColor(int PedIndex)
    {
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
        {
            pPed->SetHeadBlendPaletteColorDisabled();
        }
    }

	enum eHeadBlendHeadType
	{
		HBHT_MALE = 0,
		HBHT_FEMALE,
		HBHT_UNIQUE_MALE,
		HBHT_UNIQUE_FEMALE
	};

	int CommandGetPedHeadBlendFirstIndex(int type)
	{
		switch (type)
		{
		case HBHT_MALE:
			return 0;
		case HBHT_FEMALE:
			return CPedVariationStream::GetSkinTones().m_males.GetCount() / 2;
		case HBHT_UNIQUE_MALE:
			return (CPedVariationStream::GetSkinTones().m_males.GetCount() +
				   CPedVariationStream::GetSkinTones().m_females.GetCount()) / 2;
		case HBHT_UNIQUE_FEMALE:
			return (CPedVariationStream::GetSkinTones().m_males.GetCount() +
				   CPedVariationStream::GetSkinTones().m_females.GetCount() +
				   CPedVariationStream::GetSkinTones().m_uniqueMales.GetCount()) / 2;
		default:
			break;
		}
		return 0;
	}

	int CommandGetPedHeadBlendNumHeads(int type)
	{
		switch (type)
		{
		case HBHT_MALE:
			return CPedVariationStream::GetSkinTones().m_males.GetCount() / 2;
		case HBHT_FEMALE:
			return CPedVariationStream::GetSkinTones().m_females.GetCount() / 2;
		case HBHT_UNIQUE_MALE:
			return CPedVariationStream::GetSkinTones().m_uniqueMales.GetCount() / 2;
		case HBHT_UNIQUE_FEMALE:
			return CPedVariationStream::GetSkinTones().m_uniqueFemales.GetCount() / 2;
		default:
			break;
		}
		return 0;
	}

	int CommandSetPedPreloadVariationData(int PedIndex, int ComponentID, int DrawableID, int TextureID)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
			return (int)pPed->SetPreloadData((ePedVarComp)ComponentID, DrawableID, TextureID, (u32)streamingFlags);
		}
        return 0;
	}

	bool CommandHasPedPreloadVariationDataFinished(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			return pPed->HasPreloadDataFinished();
		}

		return true;
	}

	bool CommandHasPedPreloadVariationDataHandleFinished(int PedIndex, int handle)
	{
        if (handle <= 0)
            return true;

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return pPed->HasPreloadDataFinished(handle);
		}

		return true;
	}

	void CommandReleasePedComponentPreloadVariationData(int UNUSED_PARAM(pedIndex), int UNUSED_PARAM(componentId), int UNUSED_PARAM(drawableId), int UNUSED_PARAM(textureId))
	{
        SCRIPT_ASSERT(false, "This command is deprecated! Please use RELEASE_PED_PRELOAD_VARIATION_DATA_HANDLE instead.");
	}

	void CommandReleasePedPreloadVariationData(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			pPed->CleanUpPreloadData();
		}
	}

	void CommandReleasePedPreloadVariationDataHandle(int PedIndex, int handle)
	{
        if (handle <= 0)
            return;

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->CleanUpPreloadData(handle);
		}
	}

	int CommandSetPedPreloadPropData(int pedIndex, int anchor, int propId, int texId)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (pPed)
		{
			s32 streamingFlags = 0;
			CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
            return (int)CPedPropsMgr::SetPreloadProp(pPed, (eAnchorPoints)anchor, propId, texId, (u32)streamingFlags);
		}
        return 0;
	}

	bool CommandHasPedPreloadPropDataFinished(int pedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (pPed)
		{
			return CPedPropsMgr::HasPreloadPropsFinished(pPed);
		}

		return true;
	}

	bool CommandHasPedPreloadPropDataHandleFinished(int pedIndex, int handle)
	{
        if (handle <= 0)
            return true;

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (pPed)
		{
			return CPedPropsMgr::HasPreloadPropsFinished(pPed, (u32)handle);
		}

		return true;
	}

	void CommandReleasePedPreloadPropData(int pedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (pPed)
		{
            CPedPropsMgr::CleanUpPreloadProps(pPed);
		}
	}

	void CommandReleasePedPreloadPropDataHandle(int pedIndex, int handle)
	{
        if (handle <= 0)
            return;

		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (pPed)
		{
			CPedPropsMgr::CleanUpPreloadPropsHandle(pPed, handle);
		}
	}

	int CommandGetPedPropIndex( int PedIndex, int Position)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{	
			return (CPedPropsMgr::GetPedPropIdx(pPed, (eAnchorPoints) Position));
		}
		return -1;
	}

	void CommandSetPedPropIndex( int PedIndex, int Position, int NewPropIndex, int NewTextIndex, bool UNUSED_PARAM(SyncWithBlend))
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if(SCRIPT_VERIFY(NewPropIndex > -1, "SET_PED_PROP_INDEX - NewPropIndex must be >-1, to clear props use CLEAR_PED_PROP"))
			{
				if(scriptVerifyf(CPedPropsMgr::IsPropValid(pPed, (eAnchorPoints)Position, NewPropIndex, NewTextIndex), "%s: SET_PED_PROP_INDEX - Invalid Prop. anchor: %d prop: %d tex: %d ('%s')", CTheScripts::GetCurrentScriptNameAndProgramCounter(), Position, NewPropIndex, NewTextIndex, pPed->GetModelName()))
				{
					s32 streamingFlags = 0;
					CTheScripts::PrioritizeRequestsFromMissionScript(streamingFlags);
					CPedPropsMgr::SetPedProp(pPed, (eAnchorPoints)Position, NewPropIndex, NewTextIndex, ANCHOR_NONE, NULL, NULL, (u32)streamingFlags);
					pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);

					pedDisplayf("SET_PED_PROP -ped[%s][%p], prop:%i, tex:%i, Pos:%i, Script %s ", pPed->GetModelName(), pPed, NewPropIndex, NewTextIndex, Position, CTheScripts::GetCurrentScriptNameAndProgramCounter());

					if((eAnchorPoints)Position == ANCHOR_HEAD)
					{
						pPed->SetHairScaleLerp(false);
					}
				}
			}
		}
	}

	void CommandKnockOffPedProp( int PedIndex, bool bDamaged, bool bHats, bool bGlasses, bool bHelmets)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedPropsMgr::KnockOffProps(pPed, bDamaged, bHats, bGlasses, bHelmets);		
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
		}
	}
	
	int CommandGetPropTextureIndex(int PedIndex, int Position)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{
			return CPedPropsMgr::GetPedPropTexIdx(pPed,(eAnchorPoints)Position); 
		}
		return -1; 
	}

	void CommandClearPedProp( int PedIndex, int Position)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedPropsMgr::ClearPedProp(pPed, (eAnchorPoints) Position);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);

			if((eAnchorPoints)Position == ANCHOR_HEAD)
			{
				pPed->SetHairScaleLerp(false);
			}
		}
	}

	void CommandClearAllPedProps( int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CPedPropsMgr::ClearAllPedProps(pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraProcessExternallyDrivenDOFs, true);
			pPed->SetHairScaleLerp(false);
		}
	}

	void CommandDropAmbientProp(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			// Try to use the prop management helper, if possible.
			CPropManagementHelper* pPropManagementHelper = pPed->GetPedIntelligence()->GetActivePropManagementHelper();
			if (pPropManagementHelper)
			{
				pPropManagementHelper->DropProp(*pPed, true);
			}
			else if (pPed->GetWeaponManager())
			{
				// Otherwise use the weapon helper.
				pPed->GetWeaponManager()->DropWeapon(OBJECTTYPE_OBJECT, true);
			}
		}
	}

	void CommandSetPedParachutePackVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskParachute::SetParachutePackVariationForPed(*pPed);
		}
	}

	void CommandClearPedParachutePackVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskParachute::ClearParachutePackVariationForPed(*pPed);
		}
	}

	void CommandSetPedScubaGearVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskMotionSwimming::SetScubaGearVariationForPed(*pPed);
		}
	}

	void CommandClearPedScubaGearVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskMotionSwimming::ClearScubaGearVariationForPed(*pPed);
		}
	}

	bool CommandIsUsingScubaGearVariation(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return CTaskMotionSwimming::IsScubaGearVariationActiveForPed(*pPed);
		}

		return false;
	}

	void CommandSetPedRagdollBoundsSize( int PedIndex, int newSize )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed && pPed->GetRagdollInst())
		{
			pPed->GetRagdollInst()->SetNMAssetSize(newSize);
		}
	}

	void CommandSetPedBoundsOrientation( int PedIndex, float fBoundPitch, float fBoundHeading, const scrVector & vScrVecBoundOffset )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			Vector3 vBoundOffset(vScrVecBoundOffset);

			pPed->SetBoundPitch(fBoundPitch);
			pPed->SetBoundHeading(fBoundHeading);
			pPed->SetBoundOffset(vBoundOffset);
		}
	}

	void CommandSetBlockingOfNonTemporaryEvents( int PedIndex, bool bBlock )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if(pPed)
		{
			if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress() || !pPed->IsAPlayerPed(), "SET_BLOCKING_OF_NON_TEMPORARY_EVENTS -  This script command is not supported for players in network game scripts"))
			{
#if __BANK
				INSTANTIATE_TUNE_GROUP_BOOL( SCRIPT_COMMANDS, bLogEventBlockingNativeCalls );
				if (bLogEventBlockingNativeCalls)
				{
					scriptDisplayf("SET_BLOCKING_OF_NON_TEMPORARY_EVENTS - called with bBlock = %s for %s[%p]", AILogging::GetBooleanAsString(bBlock), AILogging::GetDynamicEntityNameSafe(pPed), pPed);
					scrThread::PrePrintStackTrace();
				}
#endif
                if(NetworkUtils::IsNetworkCloneOrMigrating(pPed))
                {
                    CScriptEntityStateChangeEvent::CBlockingOfNonTemporaryEventsParameters parameters(bBlock);
                    CScriptEntityStateChangeEvent::Trigger(pPed->GetNetworkObject(), parameters);
                }
                else if (pPed->GetBlockingOfNonTemporaryEvents() != bBlock)
                {
				    pPed->SetBlockingOfNonTemporaryEvents( bBlock );

#if ENABLE_NETWORK_LOGGING
                    if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
                    {
                        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "SET_BLOCK_NON_TEMP_EVENTS", pPed->GetNetworkObject()->GetLogName());
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Value", bBlock ? "TRUE" : "FALSE");
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Network Time", "%d", NetworkInterface::GetNetworkTime());
                    }
#endif // ENABLE_NETWORK_LOGGING
                }
			}
		}
	}

	void CommandRegisterTarget( int PedIndex, int TargetIndex )
	{
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "REGISTER_TARGET - This script command is not allowed in network game scripts!"))
		{	
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{	
				const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetIndex);
				if(pTargetPed)
				{	
					pPed->GetPedIntelligence()->RegisterThreat( pTargetPed, true );

					// Make sure our targeting stays alive for a few seconds, but don't force it because the previous call should have already
					CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
					if(pPedTargeting)
					{
						pPedTargeting->SetInUse();
					}
				}
			}
		}
	}

	void CommandRegisterHatedTargetsInArea( int PedIndex, const scrVector & scrVecCentreCoors, float Radius )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{	
			Vector3 vCentre(scrVecCentreCoors);

			// Loop through all peds and add those in range that are hated
			CPed::Pool *PedPool = CPed::GetPool();
			s32 i=PedPool->GetSize();
			CPed* pTargetPed = NULL;
			while(i--)
			{
				pTargetPed = PedPool->GetSlot(i);
				if( pTargetPed &&
					!pTargetPed->IsDead() && // Not dead
					( !pTargetPed->IsInjured() || pPed->WillAttackInjuredPeds() ) &&
					pPed->GetPedIntelligence()->IsThreatenedBy( *pTargetPed ) &&
					vCentre.Dist2(VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition())) < rage::square(Radius))
				{
					pPed->GetPedIntelligence()->RegisterThreat( pTargetPed, true );
				}
			}
		}
	}

	void CommandRegisterHatedTargetsAroundPed( int PedIndex, float Radius )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed)
		{	
			scrVector scrVecCoors = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			CommandRegisterHatedTargetsInArea( PedIndex, scrVecCoors, Radius );
		}
	}




	static CEntity *pClosestPedEntity = NULL;
	static float ClosestPedDistance = 9999.9f;
	static bool bScanForRandomPeds = false;
	static bool bScanForMissionPeds = false;

	bool CanPedBeGrabbedByScript(const CEntity *pPedEntity, bool bReturnRandomPeds, bool bReturnMissionPeds, bool bCheckIfThePedIsInAGroup, bool bCheckIfThePedIsInAVehicle, bool bReturnPlayerPeds, bool bReturnDeadOrDyingPeds, bool bReturnPedsWithScriptedTasks, bool bReturnPedsWithAnyNonDefaultTask, int iExclusionPedType)
	{
		CPed *pPed = NULL;

		if (pPedEntity == NULL)
		{
			scriptAssertf(0, "CanPedBeGrabbedByScript - entity is NULL");
			return false;
		}

		if (pPedEntity->GetIsTypePed())
		{
			pPed = (CPed *)pPedEntity;
		}
		else
		{
			scriptAssertf(0, "CanPedBeGrabbedByScript - entity is not a ped");
			return false;
		}

		if(pPed->GetPedType() == iExclusionPedType)
		{
			return false;
		}

		if (pPed->GetIsInPopulationCache())
		{
			return false;
		}

		if (!bReturnPlayerPeds)
		{
			if (pPed->IsPlayer())
			{
				return false;
			}
		}

		if (bCheckIfThePedIsInAGroup)
		{
			if (pPed->GetPedsGroup() && !pPed->IsPlayer()/*bugstar:80160, ignore the player in the group check*/)
			{
				return false;
			}
		}

		if (!bReturnDeadOrDyingPeds)
		{
			if(	(pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD)) || (pPed->IsDead()))
			{
				return false;
			}
		}


		if (!CTheScripts::GetScenarioPedsCanBeGrabbedByScript())
		{
			if (pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO)
			{
				return false;
			}

			if (pPed->GetScenarioType(*pPed) != Scenario_Invalid)
			{
				return false;
			}
		}

		if ( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain )) || (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad )) )
		{
			return false;
		}

		if (bCheckIfThePedIsInAVehicle)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				return false;
			}
		}

		if (!bReturnMissionPeds)
		{
			if (pPed->PopTypeIsMission() )
			{
				return false;
			}
		}

		if (!bReturnRandomPeds)
		{
			if (pPed->PopTypeIsRandom() )
			{
				return false;
			}
		}

		if (pPed->GetPedIntelligence()->GetTaskActive() != pPed->GetPedIntelligence()->GetTaskDefault())
		{
			if (pPed->GetPedIntelligence()->GetTaskActive() == pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY))
			{	//	Phil said that PrimaryTask is only used for scripted tasks
				if (!bReturnPedsWithScriptedTasks && !bReturnPedsWithAnyNonDefaultTask)
				{
					return false;
				}
			}
			else
			{	//	Any other non-default task
				if (!bReturnPedsWithAnyNonDefaultTask)
				{
					return false;
				}
			}
		}

		return true;
	}

	int CommandGetRandomPedAtCoord(const scrVector & scrVecCentre, const scrVector & scrVecOffset, int iExclusionPedType)
	{
		Vector3 vec = Vector3 (scrVecCentre);
		Vector3 vec2 = Vector3 (scrVecOffset);
		
		Vector3 vecTemp = vec;
		vecTemp -= vec2;
		vec2 += vec;
		vec = vecTemp;
		
		s32 PedPoolIndex = CPed::GetPool()->GetSize();
		s32 RandomPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
		while (PedPoolIndex--)
		{
			CPed *pPed = CPed::GetPool()->GetSlot(PedPoolIndex);
			if(pPed)
			{
				if (CanPedBeGrabbedByScript(pPed, true, false, true, true, false, false, false, false, iExclusionPedType))
				{
					Vector3 PedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					if(	(PedPos.x >= vec.x) && (PedPos.x <= vec2.x) &&
						(PedPos.y >= vec.y) && (PedPos.y <= vec2.y) &&
						(PedPos.z >= vec.z) && (PedPos.z <= vec2.z))
					{
						RandomPedIndex = CTheScripts::GetGUIDFromEntity(*pPed);
						break;
					}
				}
			}
		}

		//	The scan has finished so reset the scan variables
		CTheScripts::SetScenarioPedsCanBeGrabbedByScript(false);

		if (RandomPedIndex != NULL_IN_SCRIPTING_LANGUAGE)	//	>= 0)
		{
#if __DEV
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(RandomPedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
			
			if (pPed)
			pPed->m_nDEflags.bCheckedForDead = TRUE;
#endif
		}
		return RandomPedIndex;
	}


	struct PedIsCloserData
	{
		bool		bReturnPedsWithScriptedTasks;
		bool		bCheckPlayerPeds;
		Vector3 *	pVecCentre;
		int			iExclusionPedType;
	};

	static bool CheckIfPedEntityIsCloser(CEntity* pPedEntity, Vector3 *pCentre, bool bCheckPlayerPeds, bool bReturnPedsWithScriptedTasks, int iExclusionPedType)
	{
		if (CanPedBeGrabbedByScript(pPedEntity, bScanForRandomPeds, bScanForMissionPeds, true, true, bCheckPlayerPeds, false, bReturnPedsWithScriptedTasks, false, iExclusionPedType))
		{
			Vector3 vecPos = VEC3V_TO_VECTOR3(pPedEntity->GetTransform().GetPosition());

			Vector3 vecDiff = vecPos - *pCentre;
			float Distance = vecDiff.Mag();

			if (Distance < ClosestPedDistance)
			{
				pClosestPedEntity = pPedEntity;
				ClosestPedDistance = Distance;
			}
		}
		return true;
	}

	static bool CheckIfPedIsCloser(void* pItem, void* data)
	{
		Assert(data);

		PedIsCloserData * pPedIsCloserData = (PedIsCloserData*)data;

		Assert(pPedIsCloserData->pVecCentre);

		Vector3 *pCentre = pPedIsCloserData->pVecCentre;

		CPed* pPed = static_cast<CPed*>(pItem);
		return CheckIfPedEntityIsCloser(pPed, pCentre, pPedIsCloserData->bCheckPlayerPeds, pPedIsCloserData->bReturnPedsWithScriptedTasks, pPedIsCloserData->iExclusionPedType);
	}

	bool CommandGetClosestPed(const scrVector & scrVecCentre, float Range, bool bScanRandomPeds, bool bScanMissionPeds, int &ClosestPedIndex, bool bCheckPlayerPeds, bool bReturnPedsWithScriptedTasks, int iExclusionPedType)
	{
		bScanForRandomPeds = bScanRandomPeds;
		bScanForMissionPeds = bScanMissionPeds;

		pClosestPedEntity = NULL;
		ClosestPedDistance = Range;

		Vector3 vecCentre(scrVecCentre);

		PedIsCloserData pedIsCloserData;
		pedIsCloserData.bReturnPedsWithScriptedTasks	= bReturnPedsWithScriptedTasks;
		pedIsCloserData.bCheckPlayerPeds				= bCheckPlayerPeds;
		pedIsCloserData.pVecCentre						= &vecCentre;
		pedIsCloserData.iExclusionPedType				= iExclusionPedType;

		CPed *pClosestRealPed = NULL;
		CPed::GetPool()->ForAll(CheckIfPedIsCloser, (void *)&pedIsCloserData);
		pClosestRealPed = (CPed *)pClosestPedEntity;

		//	The scan has finished so reset the scan variables
		CTheScripts::SetScenarioPedsCanBeGrabbedByScript(false);

		if (pClosestRealPed)
		{
			ClosestPedIndex = CTheScripts::GetGUIDFromEntity(*pClosestRealPed);
			return true;
		}
		else
		{
			ClosestPedIndex = NULL_IN_SCRIPTING_LANGUAGE;
			return false;
		}
	}

	void CommandSetScenarioPedsCanBeReturnedByNextCommand(bool bAllowScenarioPedsToBeGrabbed)
	{
		if (bAllowScenarioPedsToBeGrabbed)
		{
			CTheScripts::SetScenarioPedsCanBeGrabbedByScript(true);
		}
		else
		{
			CTheScripts::SetScenarioPedsCanBeGrabbedByScript(false);
		}
	}

	bool CommandGetCanPedBeGrabbedByScript(int PedIndex, bool bReturnRandomPeds, bool bReturnMissionPeds, bool bCheckIfThePedIsInAGroup, bool bCheckIfThePedIsInAVehicle, bool bReturnPlayerPeds, bool bReturnDeadOrDyingPeds, bool bReturnPedsWithScriptedTasks, int iExclusionPedType)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return CanPedBeGrabbedByScript(pPed, bReturnRandomPeds, bReturnMissionPeds, bCheckIfThePedIsInAGroup, bCheckIfThePedIsInAVehicle, bReturnPlayerPeds, bReturnDeadOrDyingPeds, bReturnPedsWithScriptedTasks, false, iExclusionPedType);
		}

		return false;
	}

	float CommandGetDriverRacingModifier(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedIntelligence()->GetDriverRacingModifier();
		}

		return 0.0f;
	}

	void CommandSetDriverRacingModifier(int PedIndex, float fRacingModifier)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && SCRIPT_VERIFY(fRacingModifier >= 0.0f && fRacingModifier <= 1.0f, "SET_DRIVER_RACING_MODIFIER - Must be between 0.0 and 1.0!"))
		{
			pPed->GetPedIntelligence()->SetDriverRacingModifier(fRacingModifier);
		}
	}

	void CommandSetDriverAbility(int PedIndex, float fAbility)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && SCRIPT_VERIFY(fAbility >= 0.0f && fAbility <= 1.0f, "SET_DRIVER_ABILITY - Must be between 0.0 and 1.0!"))
		{
			pPed->GetPedIntelligence()->SetDriverAbilityOverride(fAbility);
		}
	}

	void CommandSetDriverAggressiveness(int PedIndex, float fAggressiveness)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && SCRIPT_VERIFY(fAggressiveness >= 0.0f && fAggressiveness <= 1.0f, "SET_DRIVER_AGGRESSIVENESS - Must be between 0.0 and 1.0!"))
		{
			pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(fAggressiveness);
		}
	}

	float CommandGetDriverAbility(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetDriverAbilityOverride() >= 0.0f)
			{
				return pPed->GetPedIntelligence()->GetDriverAbilityOverride();
			}
			else
			{
				return CDriverPersonality::FindDriverAbility(pPed, pPed->GetMyVehicle());
			}
		}

		return 0.0f;
	}

	float CommandGetDriverAggressiveness(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetPedIntelligence()->GetDriverAggressivenessOverride() >= 0.0f)
			{
				return pPed->GetPedIntelligence()->GetDriverAggressivenessOverride();
			}
			else
			{
				return CDriverPersonality::FindDriverAggressiveness(pPed, pPed->GetMyVehicle());
			}
		}

		return 0.0f;
	}

	bool CommandCanPedRagdoll(int PedIndex)
	{
		bool bCanUseRagdoll = false;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed != NULL && SCRIPT_VERIFY(!pPed->IsInjured(), "CAN_PED_RAGDOLL - Check ped is not injured this frame") && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_SCRIPT))
		{
			bCanUseRagdoll = true;
		}

		return bCanUseRagdoll;
	}

	bool CommandSetPedToRagdoll(int PedIndex, int MinTime, int MaxTime, int AddTask, bool UNUSED_PARAM(bAbortIfInjured), bool UNUSED_PARAM(bAbortIfDead), bool bForceScriptControl)
	{
		bool bSwitchToRagdoll = false;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
			{
				if (SCRIPT_VERIFY(!pPed->IsInjured(), "SET_PED_TO_RAGDOLL - Check ped is not injured this frame")) 
				{
					CTask* pTaskNM = NULL;
					if(AddTask==0)
						pTaskNM = rage_new CTaskNMRelax(MinTime, MaxTime);
					else if(AddTask==1)
					{
						if (!SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_TO_RAGDOLL - TASK_NM_SCRIPT ragdoll type not supported in MP"))
						{
							return false;
						}

						pTaskNM = rage_new CTaskNMScriptControl(MaxTime, /*bAbortIfInjured, bAbortIfDead,*/ bForceScriptControl);
					}
					else
					{
						nmDebugf2("[%u] Adding nm balance task:%s(%p) CommandSetPedToRagdoll.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
						pTaskNM = rage_new CTaskNMBalance(MinTime, MaxTime, NULL, 0);
						static_cast<CTaskNMBalance*>(pTaskNM)->EvaluateAndSetType(pPed);
					}
					
					bSwitchToRagdoll = SwitchToRagdollCode(pPed, MaxTime, bForceScriptControl, pTaskNM);
				}
			}
		return bSwitchToRagdoll;
	}

	bool CommandSetPedToRagdollWithFall(int PedIndex, int MinTime, int MaxTime, int nFallType, const scrVector & scrVecDirection, float fGroundHeight, const scrVector & scrVecGrab1, const scrVector & scrVecGrab2)
	{
		bool bSwitchToRagdoll = false;	
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
			{
				if (SCRIPT_VERIFY(!pPed->IsInjured(), "SET_PED_TO_RAGDOLL_WITH_FALL - Check ped is not injured this frame")) 
				{
					Vector3 vecGrab1(scrVecGrab1);
					Vector3 vecGrab2(scrVecGrab2);
					CGrabHelper* pGrabHelper = NULL;
					if(vecGrab1.IsNonZero() && vecGrab2.IsNonZero())
					{

					}

					Vector3 vecDirn(scrVecDirection);
					if (SCRIPT_VERIFY((vecDirn.Mag2() < 1.5f) && (vecDirn.Mag2() > 0.5f), "SET_PED_TO_RAGDOLL_WITH_FALL - Direction should be a normalised (length 1.0) vector")) 
					{
						CTaskNMFallDown* pTaskFall = rage_new CTaskNMFallDown(MinTime, MaxTime, CTaskNMFallDown::eNMFallType(nFallType), vecDirn, fGroundHeight, NULL, pGrabHelper, VEC3_ZERO, true);

						bool bForceScriptControl = false;
						if(nFallType > CTaskNMFallDown::TYPE_DIE_TYPES)
							bForceScriptControl = true;

						if(pGrabHelper)
							delete pGrabHelper;

						bSwitchToRagdoll = SwitchToRagdollCode(pPed, MaxTime, bForceScriptControl, pTaskFall);
					}
				}
			}
		return bSwitchToRagdoll;
	}

	void CommandSetPedRagdollOnCollision(int PedIndex, bool RagdollOnCollision)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed != NULL && SCRIPT_VERIFY(!pPed->IsInjured(), "SET_PED_RAGDOLL_ON_COLLISION - Check ped is not injured this frame")) 
		{
			// Set the ragdoll to activate on collision with geometry
			pPed->SetActivateRagdollOnCollision(RagdollOnCollision);
		}
	}

	void CommandSetPedToAnimated(int PedIndex, bool ForceSwitch)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
				if(pPed && pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
				{
					CTask *pActiveTask = pPed->GetPedIntelligence()->GetTaskActiveSimplest();
					CTaskNMBehaviour* pTaskNM = pActiveTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pActiveTask : NULL;

					if(ForceSwitch)
					{
						pPed->SwitchToAnimated();
					}
					else if(pTaskNM)
					{
						pTaskNM->ForceTimeout();
					}
				}
			}
		}

	bool CommandIsPedRagdoll(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if(pPed && pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				return true;
			}
		}
		return false;
	}

	bool CommandIsPedRunningRagdollTask(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL))
			{
				return true;
			}

			return pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_NM_CONTROL)!=NULL;
		}
		return false;
	}

	void CommandSetPedRagdollForceFall(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed != NULL)
		{
			CTask* pActiveTask = pPed->GetPedIntelligence()->GetTaskActiveSimplest();
			if (pActiveTask != NULL && pActiveTask->IsNMBehaviourTask())
			{
				smart_cast<CTaskNMBehaviour*>(pActiveTask)->ForceFallOver();
			}
		}
	}

	bool CommandIsPedRunningMeleeTask(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		SCRIPT_ASSERT(pPed, "IS_PED_RUNNING_MELEE_TASK - You must specify the ped!");
		if(pPed)
		{
			bool bMeleeTaskRunning = pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning);
			return bMeleeTaskRunning;
		}
		return false;
	}

	bool CommandIsPedRunningMobilePhoneTask(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (CTaskMobilePhone::IsRunningMobilePhoneTask(*pPed) )
			{
				return true;
			}
		}
		return false;
	}

	bool CommandIsMobilePhoneToPedEar(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskMobilePhone* pMobilePhoneTask = static_cast<CTaskMobilePhone*>(pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE));
			if(!pMobilePhoneTask)
			{
				 pMobilePhoneTask = static_cast<CTaskMobilePhone*>(pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_MOBILE_PHONE));
			}
			
			if(pMobilePhoneTask && pMobilePhoneTask->GetState() == CTaskMobilePhone::State_EarLoop)
			{
				return true;
			}
		}
		return false;
	}

	void CommandResetPedRagdollTimer(int PedIndex)
	{
		// Used to reset the start time of a Natural Motion task. For specific scenarios where timeout (and return to animation) must
		// be avoided, call this every frame.

		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			CTaskNMControl* pNmTask = NULL;
			if(pPed && pPed->GetPedIntelligence())
			{
				pNmTask = static_cast<CTaskNMControl*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
			}

			if(SCRIPT_VERIFY(pNmTask, "RESET_PED_RAGDOLL_TIMER - Ped is not running an nm control task!"))
			{
				pNmTask->ResetStartTime();
			}
		}
	}

	void CommandSetPedCanRagdoll(int PedIndex, bool bUnlock)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
		    if(bUnlock && pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED)
		    {
			    pPed->SetRagdollState(RAGDOLL_STATE_ANIM, true);
		    }
		    else if(bUnlock==false)
		    {
			    if(pPed->GetRagdollState() > RAGDOLL_STATE_ANIM)
			    {
				    pPed->SwitchToAnimated();
			    }

			    pPed->SetRagdollState(RAGDOLL_STATE_ANIM_LOCKED);
		    }
 		}
	}
	
	void CommandSetRagdollBlockingFlags(int PedIndex, s32 flags)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			if(NetworkUtils::IsNetworkCloneOrMigrating(pPed))
			{
				CScriptEntityStateChangeEvent::CSetPedRagdollBlockFlagParameters parameters(flags);
				CScriptEntityStateChangeEvent::Trigger(pPed->GetNetworkObject(), parameters);
			}
			else
			{
				pPed->ApplyRagdollBlockingFlags(reinterpret_cast<eRagdollBlockingFlagsBitSet&>(flags));

#if ENABLE_NETWORK_LOGGING
                    if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
                    {
                        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "SET_RAGDOLL_BLOCKING_FLAGS", pPed->GetNetworkObject()->GetLogName());
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Ragdoll blocking flags", "%d", flags);
                        NetworkInterface::GetObjectManagerLog().WriteDataValue("Network Time", "%d", NetworkInterface::GetNetworkTime());
                    }
#endif // ENABLE_NETWORK_LOGGING
			}
		}
	}

	void CommandClearRagdollBlockingFlags(int PedIndex, s32 flags)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->ClearRagdollBlockingFlags(reinterpret_cast<eRagdollBlockingFlagsBitSet&>(flags));
		}
	}

	void SetPedDefensiveIfNeeded( CPed* pPed )
	{
		if(pPed)
		{
			// If our ped is set as will advance movement then change it to defensive
			CCombatBehaviour& pedCombatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
			if(pedCombatBehaviour.GetCombatMovement() == CCombatData::CM_WillAdvance)
			{
				pedCombatBehaviour.SetCombatMovement(CCombatData::CM_Defensive);
			}
		}
	}

	void CommandSetPedAngledDefensiveArea( int PedIndex, const scrVector & scrVec1, const scrVector & scrVec2, float fRectangleWidth, bool bUseCenterAsGoToPosition, bool bApplyToSecondaryDefensiveArea )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			Vector3 v1(scrVec1);
			Vector3 v2(scrVec2);

			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			pDefensiveArea->Set( v1 ,v2, fRectangleWidth, NULL, false );
			pDefensiveArea->SetUseCenterAsGoToPosition(bUseCenterAsGoToPosition);

			SetPedDefensiveIfNeeded(pPed);
		}
	}

	void CommandSetPedSphereDefensiveArea( int PedIndex, const scrVector & scrVecCenter, float fRadius, bool bUseCenterAsGoToPosition, bool bApplyToSecondaryDefensiveArea )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			Vector3 v1(scrVecCenter);

			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			pDefensiveArea->SetAsSphere( v1, fRadius, NULL );
			pDefensiveArea->SetUseCenterAsGoToPosition(bUseCenterAsGoToPosition);

			SetPedDefensiveIfNeeded(pPed);
		}
	}

	void SetDefensiveSphereAttachedToEntity( CPed* pPed, const CEntity* pEntity, const scrVector & scrVecCenter, float fRadius, bool bApplyToSecondaryDefensiveArea )
	{
		if(pPed && pEntity)
		{
			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			pDefensiveArea->SetAsSphere( scrVecCenter, fRadius, pEntity );

			SetPedDefensiveIfNeeded(pPed);
		}
	}

	void CommandSetPedDefensiveSphereAttachedToPed( int PedIndex, int OtherPedIndex, const scrVector & scrVecCenter, float fRadius, bool bApplyToSecondaryDefensiveArea )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_DEFENSIVE_SPHERE_ATTACHED_TO_PED - This script command is not allowed in network game scripts!"))
		{		
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				const CPed *pOtherPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(OtherPedIndex);
				if (pOtherPed)
				{
					SetDefensiveSphereAttachedToEntity(pPed, pOtherPed, scrVecCenter, fRadius, bApplyToSecondaryDefensiveArea);
				}
			}
		}
	}

	void CommandSetPedDefensiveSphereAttachedToVehicle( int PedIndex, int VehicleIndex, const scrVector & scrVecCenter, float fRadius, bool bApplyToSecondaryDefensiveArea )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_DEFENSIVE_SPHERE_ATTACHED_TO_VEHICLE - This script command is not allowed in network game scripts!"))
		{		
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
				if (pVehicle)
				{
					SetDefensiveSphereAttachedToEntity(pPed, pVehicle, scrVecCenter, fRadius, bApplyToSecondaryDefensiveArea);
				}
			}
		}
	}

	void CommandSetPedDefensiveAreaAttachedToPed( int PedIndex, int OtherPedIndex, const scrVector & scrVec1, const scrVector & scrVec2, float fWidth, bool bOrientateWithPed, bool bApplyToSecondaryDefensiveArea )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_DEFENSIVE_AREA_ATTACHED_TO_PED - This script command is not allowed in network game scripts!"))
		{		
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				const CPed *pOtherPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(OtherPedIndex);
				if (pOtherPed)
				{
					Vector3 v1(scrVec1);
					Vector3 v2(scrVec2);

					CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
					CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
					pDefensiveArea->Set( v1 ,v2, fWidth, pOtherPed, bOrientateWithPed );

					SetPedDefensiveIfNeeded(pPed);
				}
			}
		}
	}

	void CommandSetPedDefensiveAreaDirection( int PedIndex, const scrVector & scvDefendFromPos, bool bApplyToSecondaryDefensiveArea )
	{
		Vector3 vDefendFromPos = Vector3(scvDefendFromPos);
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			pDefensiveArea->SetDefaultCoverFromPosition( vDefendFromPos );

			SetPedDefensiveIfNeeded(pPed);
		}
	}

	void CommandRemovePedDefensiveArea( int PedIndex, bool bApplyToSecondaryDefensiveArea )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(true) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			pDefensiveArea->Reset();
		}
	}

	scrVector CommantGetPedDefensiveAreaPosition( int PedIndex, bool bApplyToSecondaryDefensiveArea )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		Vector3 vDefensiveAreaPosition(VEC3_ZERO);

		if(pPed)
		{
			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bApplyToSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(false) : pDefensiveAreaManager->GetPrimaryDefensiveArea();
			if(scriptVerifyf(pDefensiveArea && pDefensiveArea->IsActive(), "%s:GET_PED_DEFENSIVE_AREA_POSITION: Ped does not have an active defensive area", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pDefensiveArea->GetCentre(vDefensiveAreaPosition);
			}
		}

		return vDefensiveAreaPosition;
	}

	bool CommandIsPedDefensiveAreaActive( int PedIndex, bool bCheckSecondaryDefensiveArea )
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			CDefensiveAreaManager* pDefensiveAreaManager = pPed->GetPedIntelligence()->GetDefensiveAreaManager();
			CDefensiveArea* pDefensiveArea = bCheckSecondaryDefensiveArea ? pDefensiveAreaManager->GetSecondaryDefensiveArea(false) : pDefensiveAreaManager->GetPrimaryDefensiveArea();

			return pDefensiveArea && pDefensiveArea->IsActive();
		}

		return false;
	}

	void CommandSetPedPreferredCoverSet( int PedIndex, int ItemSetIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
#if __ASSERT
			// Check the item set to ensure only cover points have been added to the set
			CItemSet* pItemSet = CTheScripts::GetItemSetToModifyFromGUID(ItemSetIndex);
			if (pItemSet)
			{
				for (s32 i=0; i<pItemSet->GetSetSize(); i++)
				{
					const fwExtensibleBase* pExtBase = pItemSet->GetEntity(i);
					if (!scriptVerifyf(pExtBase && pExtBase->GetIsClassId(CScriptedCoverPoint::GetStaticClassId()), "Item %i is NOT a coverpoint- SET_PED_PREFERRED_COVER_SET failed", i))
					{
						return;
					}
				}
			}
#endif // __ASSERT
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetPreferredCoverGuid( ItemSetIndex );
		}
	}

	void CommandRemovePedPreferredCoverSet( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetPreferredCoverGuid( -1 );
		}
	}

	void CommandReviveInjuredPed(int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY(pPed->IsInjured(), "REVIVE_INJURED_PED - Ped not injured")&&
				SCRIPT_VERIFY(!pPed->IsDead(), "REVIVE_INJURED_PED - Can't revive dead peds using this method"))
			{
				pPed->SetHealth(pPed->GetMaxHealth());
			}
		}
	}

	void CommandResurrectPed (int iPedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if ( pPed->IsDead() )
			{
				const bool bTriggerNetworkRespawnEvent = false;
				const bool bDoTeleport = false;
				pPed->Resurrect(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), pPed->GetTransform().GetHeading(), bTriggerNetworkRespawnEvent, bDoTeleport);
			}
			else
			{
				scriptWarningf("RESURRECT_PED called on a ped that isn't dead, skipping resurrect call!");
			}
		}
	}

#if !__FINAL
	void CommandDebugSetPedName(int iPedIndex, const char* sPedName)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
			{
				strncpy( pPed->m_debugPedName, sPedName, 15 );
				pPed->m_debugPedName[15] = '\0';
			}
		}
#else
	void CommandDebugSetPedName(int UNUSED_PARAM(iPedIndex) , const char* UNUSED_PARAM(sPedName) )
	{
	}
#endif

	scrVector CommandGetPedExtractedDisplacement(int iPedIndex,  bool bWorldSpace)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		Vector3 vDisplacement;
		vDisplacement.Zero();

		if (pPed)
		{
			float timeStep = fwTimer::GetTimeStep();
			vDisplacement = pPed->GetAnimatedVelocity() * timeStep;

			if (bWorldSpace)
			{
				vDisplacement = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDisplacement)));
			}
		}
		return vDisplacement;
	}

	void CommandDetachPedFromWithinVehicle(int PedIndex, bool bNoCollisionUntilClear)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
			{
				// This command is explicitly removing the ped from the vehicle, so block the assert
				ASSERT_ONLY(CPed::ASSERT_IF_DETACHED_FROM_VEHICLE = false;)
				if(pPed->GetIsAttached())
				{
					u16 nDetachFlags = DETACH_FLAG_ACTIVATE_PHYSICS;
					if(bNoCollisionUntilClear)
						nDetachFlags |= DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR;

					pPed->DetachFromParent(nDetachFlags);
				}
				ASSERT_ONLY(CPed::ASSERT_IF_DETACHED_FROM_VEHICLE = true;)
			}
		}

	void CommandSetHeadingLimitForAttachedPed(int PedIndex, float fDefaultHeading, float fHeadingLimit)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
			{
				pPed->SetAttachHeading(fDefaultHeading);
				pPed->SetAttachHeadingLimit(fHeadingLimit);
			}
		}

	void CommandSetAttachedPedRotation(int PedIndex, const scrVector & scrVecRotation, int RotOrder)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
			{
				Vector3 vecRotation = Vector3(scrVecRotation);
				vecRotation.x = DtoR * vecRotation.x; 	
				vecRotation.y = DtoR * vecRotation.y;
				vecRotation.z = DtoR * vecRotation.z;

				Quaternion quatRotate;
				quatRotate.Identity();
				if(vecRotation.IsNonZero())
				{
					CScriptEulers::QuaternionFromEulers(quatRotate, vecRotation, static_cast<EulerAngleOrder>(RotOrder));
				}

				pPed->SetAttachQuat(quatRotate);
				pPed->SetAttachHeading(0.0f);
				pPed->SetAttachHeadingLimit(0.0f);

				pPed->SetAttachState(ATTACH_STATE_PED_WITH_ROT);
			}
		}

	bool CommandIsPedHeadingTowardsPosition(int PedIndex, const scrVector & scrVecPosition, float fDegreesDelta)
	{
		Vector3 vPosition(scrVecPosition);
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			float fCurrentHeading = fwAngle::LimitRadianAngleSafe( pPed->GetCurrentHeading() );
			float fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetDesiredHeading() );

			Vector3 vToTarget = vPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			float fHeadingToTarget = fwAngle::GetRadianAngleBetweenPoints(vToTarget.x, vToTarget.y, 0.0f, 0.0f);
			fHeadingToTarget = fwAngle::LimitRadianAngleSafe( fHeadingToTarget );

			float fRadsDelta = fDegreesDelta * DtoR;
			if(Abs(SubtractAngleShorter(fCurrentHeading, fHeadingToTarget)) < fRadsDelta &&
				Abs(SubtractAngleShorter(fDesiredHeading, fHeadingToTarget)) < fRadsDelta)
			{
				return true;
			}
		}
		return false;
	}

	void CommandResetPedVisibleDamage(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->ClearDamage();
		}
	}

	// convert old style type to type and name hash
	u32 ConvertTypeToName(int type, bool & isStab)
	{
		isStab = false;
		u32 bloodNameHash;
		switch(type)
		{
			case 0:
				bloodNameHash = ATSTRINGHASH("BulletSmall",0x8a6a0ed5);
				break;
			case 1:
				bloodNameHash = ATSTRINGHASH("BulletLarge",0x75e891e4);
				break;
			case 2:
				bloodNameHash = ATSTRINGHASH("ShotgunSmall",0xbdf00aa);
				break;
			case 3:
				bloodNameHash = ATSTRINGHASH("ShotgunLarge",0x68f7998b);
				break;
			case 4:
				bloodNameHash = ATSTRINGHASH("stab",0x7744e5b7);
				isStab = true;
				break;
			default:
				Errorf("%d is an unsupport old style blood type",type);
				bloodNameHash = 0;
				break;
		}
		return bloodNameHash;
	}

	// DEPRICATED: please use  CommandApplyPedBlood() or CommandApplyPedStab()
/*
	void CommandApplyPedBloodDamage(int PedIndex, int component, const scrVector & scrVecPos, const scrVector & scrVecDir, int type)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed )
		{
			scriptAssertf(component>=0 || component<RAGDOLL_NUM_COMPONENTS, "%s: APPLY_PED_BLOOD_DAMAGE - component (%d) is not a valid ped component index", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component);
			scriptAssertf(type>=0 && type<kNumDamageTypes, "%s: APPLY_PED_BLOOD_DAMAGE - type (%d) is not a valid ped blood damage type. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);

			Vector3 pos = Vector3 (scrVecPos);
			Vector3 dir = Vector3 (scrVecDir);
			
			// convert old style type to type and name hash
			bool isStab = false;
			u32 bloodTypeHash = ConvertTypeToName(type, isStab);

			if (isStab)
				PEDDAMAGEMANAGER.AddPedStab(pPed,(u16)component, pos, dir, bloodTypeHash);
			else
				PEDDAMAGEMANAGER.AddPedBlood(pPed, (u16)component, pos, bloodTypeHash);
		}
	}
*/

	// DEPRICATED: please use  CommandApplyPedBloodByZone() or CommandApplyPedStabByZone()
	void CommandApplyPedBloodDamageByZone(int PedIndex, int zone, float u, float v, int type)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed )
		{
			scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_BLOOD_DAMAGE_BY_ZONE - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
			scriptAssertf(type>=0 && type<kNumDamageTypes, "%s: APPLY_PED_BLOOD_DAMAGE_BY_ZONE - type (%d) is not a valid ped blood damage type. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);

			Vector2 uv(u,v);

			// convert old style type to type and name hash
			bool isStab = false;
			u32 bloodTypeHash = ConvertTypeToName(type, isStab);

			if (isStab)
				PEDDAMAGEMANAGER.AddPedStab(pPed, (ePedDamageZones)zone, uv, uv, bloodTypeHash);
			else
				PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)zone, uv, bloodTypeHash);
		}
	}

	void CommandApplyPedBlood(int PedIndex, int component, const scrVector & scrVecPos, const char * bloodName)
	{
		scriptAssertf(component>=0 || component<RAGDOLL_NUM_COMPONENTS, "%s: APPLY_PED_BLOOD - component (%d) is not a valid ped component index", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDAMAGEMANAGER.AddPedBlood(pPed, (u16)component, Vector3(scrVecPos), atHashWithStringBank(bloodName));
	}

	
	void CommandApplyPedBloodByZone(int PedIndex, int zone, float u, float v, const char * bloodName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_BLOOD_BY_ZONE - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex) )
			PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)zone, Vector2(u,v), atHashWithStringBank(bloodName));
	}

	void CommandApplyPedBloodSpecific(int PedIndex, int zone, float u, float v, float rotation, float scale, int forceFrame, float preAge, const char * bloodName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_BLOOD_SPECIFIC - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex) )
			PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)zone, Vector2(u,v), rotation, scale, atHashWithStringBank(bloodName), false, preAge, forceFrame);
	}

	void CommandApplyPedStab(int PedIndex, int component, const scrVector & scrVecPos, const scrVector & scrVecDir, const char * bloodName)
	{
		scriptAssertf(component>=0 || component<RAGDOLL_NUM_COMPONENTS, "%s: APPLY_PED_STAB - component (%d) is not a valid ped component index", CTheScripts::GetCurrentScriptNameAndProgramCounter(), component);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex) )
			PEDDAMAGEMANAGER.AddPedStab(pPed, (u16)component, Vector3(scrVecPos), Vector3(scrVecDir), atHashWithStringBank(bloodName));
	}

	void CommandApplyPedStabByZone(int PedIndex, int zone, float u, float v, float u2, float v2, const char * bloodName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_STAB_BY_ZONE - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex) )
			PEDDAMAGEMANAGER.AddPedStab(pPed, (ePedDamageZones)zone, Vector2(u,v), Vector2(u2,v2), atHashWithStringBank(bloodName)); 
	}

	// Depricated, please use CommandApplyPedDamageDecal
	void CommandApplyPedScar(int PedIndex, int zone, float u, float v, float rotation, float scale, const char * scarName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_SCAR - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDAMAGEMANAGER.AddPedScar(pPed, (ePedDamageZones)zone, Vector2(u,v), rotation, scale, atHashWithStringBank(scarName));
	}

	// Depricated, please use CommandApplyPedDamageDecal
	void CommandApplyPedDirt(int PedIndex, int zone, float u, float v, float rotation, float scale, float alpha, bool fadeIn, const char * dirtName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_DIRT - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDAMAGEMANAGER.AddPedDirt(pPed, (ePedDamageZones)zone, Vector2(u,v), rotation, scale, atHashWithStringBank(dirtName), fadeIn, alpha);
	}

	void CommandApplyPedDamageDecal(int PedIndex, int zone, float u, float v, float rotation, float scale, float alpha, int forceFrame, bool fadeIn, const char * dirtName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_DAMAGE_DECAL - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDAMAGEMANAGER.AddPedDamageDecal(pPed, (ePedDamageZones)zone, Vector2(u,v), rotation, scale, atHashWithStringBank(dirtName), fadeIn, forceFrame, alpha);
	}

	void CommandApplyPedDamagePack(int PedIndex, const char * packName, float preAge, float alpha)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDAMAGEMANAGER.AddPedDamagePack(pPed, packName, preAge, alpha);
	}
	
	void CommandApplyPedDecoration(int PedIndex, int zone, float u, float v, float rotation, float scale, const char *pTextureDictionaryName, const char *pTextureName, bool addToBumpMap)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: APPLY_PED_DECORATION - zone (%d) is not a valid ped decoration zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			atHashString nullHash;
			PEDDAMAGEMANAGER.AddPedDecoration(pPed, nullHash, nullHash, (ePedDamageZones)zone, Vector2(u,v), rotation, Vector2(scale,scale), false, pTextureDictionaryName, pTextureName, addToBumpMap);
		}
	}

	void CommandAddPedDecoration(int PedIndex, const char* pCollectionName, const char* pPresetName)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDECORATIONMGR.AddPedDecoration(pPed, atHashString(pCollectionName), atHashString(pPresetName));
	}
	
	void CommandAddPedDecorationWithAlpha(int PedIndex, const char* pCollectionName, const char* pPresetName, float alpha)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDECORATIONMGR.AddPedDecoration(pPed, atHashString(pCollectionName), atHashString(pPresetName), EmblemDescriptor::INVALID_EMBLEM_ID, alpha);
	}

	void CommandAddPedDecorationFromHashes(int PedIndex, int CollectionNameHash, int PresetNameHash)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pedDebugf1("ADD_PED_DECORATION_FROM_HASHES called for ped %d (%p), CollectionNameHash=0x%08x(%s), PresetNameHash=0x%08x(%s)",
				PedIndex, pPed, CollectionNameHash, atHashString(CollectionNameHash).TryGetCStr(), PresetNameHash, atHashString(PresetNameHash).TryGetCStr());
			PEDDECORATIONMGR.AddPedDecoration(pPed, atHashString(CollectionNameHash), atHashString(PresetNameHash));
		}
	}

	void CommandAddPedDecorationFromHashesWithAlpha(int PedIndex, int CollectionNameHash, int PresetNameHash, float alpha)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDECORATIONMGR.AddPedDecoration(pPed, atHashString(CollectionNameHash), atHashString(PresetNameHash), EmblemDescriptor::INVALID_EMBLEM_ID, alpha);
	}

	void CommandAddPedDecorationFromHashesInCorona(int PedIndex, int CollectionNameHash, int PresetNameHash)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			PEDDAMAGEMANAGER.ForceDecorationsOnNewDamageSet(true);
			PEDDECORATIONMGR.AddPedDecoration(pPed, atHashString(CollectionNameHash), atHashString(PresetNameHash));
			PEDDAMAGEMANAGER.ForceDecorationsOnNewDamageSet(false);
		}
	}

	int CommandGetPedDecorationIndex(const char* pCollectionName, const char* pPresetName)
	{
		int idx = PEDDECORATIONMGR.GetPedDecorationIndex(atHashString(pCollectionName), atHashString(pPresetName));
		return idx;
	}

	int CommandGetPedDecorationZone(const char* pCollectionName, const char* pPresetName)
	{
		int zone = (int)PEDDECORATIONMGR.GetPedDecorationZone(atHashString(pCollectionName), atHashString(pPresetName));
		return zone;
	}

	int CommandGetPedDecorationZoneFromHashes(int CollectionNameHash, int PresetNameHash)
	{
		const int zone = (int)PEDDECORATIONMGR.GetPedDecorationZone(atHashString(CollectionNameHash), atHashString(PresetNameHash));
		return zone;
	}

	void CommandAddPedDecorationByIndex(int PedIndex, int collectionIndex, const char* pPresetName)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			PEDDECORATIONMGR.AddPedDecorationByIndex(pPed, collectionIndex, atHashString(pPresetName));
	}

	int CommandGetPedDecorationIndexByIndex(int collectionIndex, const char* pPresetName)
	{
		int idx = PEDDECORATIONMGR.GetPedDecorationIndexByIndex(collectionIndex, atHashString(pPresetName));
		return idx;
	}

	int CommandGetPedDecorationZoneByIndex(int collectionIndex, const char* pPresetName)
	{
		int zone = (int)PEDDECORATIONMGR.GetPedDecorationZoneByIndex(collectionIndex, atHashString(pPresetName));
		return zone;
	}

    int CommandGetNumPedDecorationCollections()
    {
		return PEDDECORATIONMGR.GetNumCollections();
    }

	void CommandClearPedBloodDamage(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			pPed->ClearDamage();
	}

	void CommandClearPedBloodDamageByZone(int PedIndex, int zone)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones, "%s: CLEAR_PED_BLOOD_DAMAGE_BY_ZONE - zone (%d) is not a valid ped damage zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);

		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			pPed->ClearDamage(zone);
	}

	void CommandClearPedDamageDecalByZone(int PedIndex, int zone, const char * damageDecalName)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumZones || zone == kDamageZoneInvalid, "%s: CLEAR_PED_DAMAGE_DECAL_BY_ZONE - zone (%d) is not a valid ped damage zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			PEDDAMAGEMANAGER.ClearDamageDecals(pPed->GetDamageSetID(), (ePedDamageZones)zone, atHashWithStringBank(damageDecalName));
		}
	}

	void CommandSetPedBloodDamageClearInfo(int PedIndex, float redIntensity, float alphaIntensity)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pPed->SetBloodDamageClearInfo(redIntensity, alphaIntensity);
		}
	}

	void CommandHidePedBloodDamageByZone(int PedIndex, int zone, bool enable)
	{
		scriptAssertf(zone>=0 && zone<kDamageZoneNumBloodZones || zone == kDamageZoneInvalid, "%s: HIDE_PED_BLOOD_DAMAGE_BY_ZONE - zone (%d) is not a valid ped damage blood zone. ", CTheScripts::GetCurrentScriptNameAndProgramCounter(), zone);

		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			pPed->HideBloodDamage(zone, enable);
	}
	
	enum ePedDamageStateScript
	{
		kInProgress = 0,
		kNoDecorations,
		kReady,
	};
	int CommandGetPedDecorationsState(int PedIndex)
	{
		ePedDamageStateScript dmgState = kNoDecorations;

		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			const CPedDamageSet* pDamageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed);
			const CCompressedPedDamageSet* pCompressedPedDamageSet = PEDDAMAGEMANAGER.GetCompressedDamageSet(pPed);
			if (pCompressedPedDamageSet && pCompressedPedDamageSet->HasDecorations())
			{
				dmgState = (pCompressedPedDamageSet->GetTexture() ? kReady : kInProgress);
				pedDebugf1("GET_PED_DECORATIONS_STATE(%d / %p) has compressed ped damage set; returning %d", PedIndex, pPed, dmgState);
			}
			else if (pDamageSet && pDamageSet->HasDecorations())
			{
				dmgState = (pDamageSet->IsWaitingForTexture() ? kInProgress : kReady);
				pedDebugf1("GET_PED_DECORATIONS_STATE(%d / %p) has uncompressed ped damage set; returning %d", PedIndex, pPed, dmgState);
			}
		}

		pedDebugf1("GET_PED_DECORATIONS_STATE(%d) has no decorations, or ped index is invalid; returning %d", PedIndex, dmgState);
		return (int)dmgState;
	}

	void CommandMarkPedDecorationAsClonedFromLocalPlayer(int PedIndex, bool bIsLocalPedClone)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			CPedDamageSet* pDamageSet = const_cast<CPedDamageSet*>(PEDDAMAGEMANAGER.GetDamageSet(pPed));
			if (pDamageSet && pDamageSet->HasDecorations())
			{
				pDamageSet->SetWasClonedFromLocalPlayer(bIsLocalPedClone);
			}
		}
	}

	void CommandClearPedWetness(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pPed->ClearWetClothing();
			pPed->SetTimeSincePedInWater(60.0f);
			if (pPed->GetPedVfx())
			{
				pPed->GetPedVfx()->ResetOutOfWaterBoneTimers();
			}
		}
	}

	void CommandSetPedWetnessHeight(int PedIndex, float height)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES))
		{
			pPed->SetWetClothingHeight(height);
			pPed->SetWetClothingLevel(1.0f);
		}
	}

	void CommandSetPedWetnessEnabledThisFrame(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			CPed::AddPedThatCanGetWetThisFrame(pPed);
		}
	}

	void CommandSetPedWetness(int PedIndex, float wetLevel)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pPed->SetWetClothingLevel(wetLevel);
		}
	}

	void CommandClearPedEnvDirt(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pPed->SetDirtScale(0.0f);
			pPed->SetCustomDirtScale(0.0f);
		}
	}

	void CommandSetPedSweat(int PedIndex, float amount)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			pPed->SetSweatScale(amount);
		}
	}

	void CommandClearPedDecorations(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
			pPed->ClearDecorations();
	}

	void CommandClearPedDecorationsLeaveScars(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex != kInvalidPedDamageSet) 
				PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex, false, true);  // leave the only the scars and bruises
			
			damageIndex = pPed->GetCompressedDamageSetID();
			if (damageIndex != kInvalidPedDamageSet) 
				PEDDAMAGEMANAGER.ClearCompressedPedDecorations(damageIndex);	// all compressed decorations are included in this group

#if GTA_REPLAY
			if(pPed && ReplayPedExtension::HasExtension(pPed))
			{
				ReplayPedExtension::ClearPedDecorationData(pPed);
			}
#endif //GTA_REPLAY
		}
	}

	void CommandClearPedScriptDecorations(int PedIndex)
	{
		if(CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex))
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex != kInvalidPedDamageSet) 
				PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex, true, true);  // leave non-streamed decorations and the scars and bruises
			
#if GTA_REPLAY
			if(pPed && ReplayPedExtension::HasExtension(pPed))
			{
				ReplayPedExtension::ClearPedDecorationData(pPed);
			}
#endif //GTA_REPLAY
		}
	}

	bool CommandWasPedSkeletonUpdated(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

		if (pPed)
		{
			return true;
		}

		return false;
	}

	scrVector CommandGetPedBoneCoords(int PedIndex, int iPedBoneTag, const scrVector & scriptBoneOffset)
	{
		Vector3 vecResult;
		vecResult.Zero();

		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	
		if (pPed)
		{
			Vector3 vecOffset(scriptBoneOffset);
	
			Matrix34 boneMat;
			pPed->GetBoneMatrix(boneMat, (eAnimBoneTag)iPedBoneTag);

			if(vecOffset.IsNonZero())
				boneMat.Transform(vecOffset, vecResult);
			else
				vecResult = boneMat.d;
		}

		return vecResult;
	}



	void ToggleNMBindposeTask(int DEV_ONLY(nPedIndex), bool DEV_ONLY(bDoBlendFromNM))
	{
#if __DEV
		// Check that the bind pose task is running on the focus ped and signal it to
		// toggle between animation and NM controlled bind-pose.

		CPed* pFocusPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(nPedIndex);

		if (pFocusPed)
		{
		CTaskBindPose *pBindPoseTask = dynamic_cast<CTaskBindPose*>(pFocusPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_BIND_POSE));
		if(!taskVerifyf(pBindPoseTask, "TASK_BIND_POSE not started"))
		{
			return;
		}

		pBindPoseTask->m_bDoBlendFromNM = bDoBlendFromNM;

		pBindPoseTask->ToggleNmBindPose();
		}
#endif
	}

	void CommandNMHeadLook(int PedIndex, const scrVector & vLookPos)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetRagdollInst()->GetNMAgentID() >= 0)
		{
			ART::MessageParams msg;
			msg.addBool(NMSTR_PARAM(NM_START), true);
			msg.addBool(NMSTR_PARAM(NM_HEADLOOK_ALWAYS_LOOK), true); 
			msg.addBool(NMSTR_PARAM(NM_HEADLOOK_ALWAYS_EYES_HORIZONTAL), false); 
			msg.addVector3(NMSTR_PARAM(NM_HEADLOOK_POS), vLookPos.x, vLookPos.y, vLookPos.z);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_HEADLOOK_MSG), &msg);	
		}
	}

	static ART::MessageParams *pScriptArtMessageParams = NULL;
	static int nMessageName = -1;

	void CommandCreateNMMessage(int StartParam, int nMessageType)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams==NULL, "CREATE_NM_MESSAGE - Message already exists"))
		{
			nMessageName = nMessageType;

			scriptAssertf(CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageType), "%s: CREATE_NM_MESSAGE - Message enum (%d) is not a message type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nMessageType);

			pScriptArtMessageParams = rage_new ART::MessageParams;
			if(StartParam >= 0)
				pScriptArtMessageParams->addBool(NMSTR_PARAM(NM_START), StartParam != 0);
		}
	}

	void CommandGivePedNMMessage(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY(pPed->GetUsingRagdoll(), "GIVE_PED_NM_MESSAGE - Ped is not using ragdoll")&&
				SCRIPT_VERIFY(!pPed->IsInjured(), "GIVE_PED_NM_MESSAGE - Check ped is not injured this frame"))
			{
				if (SCRIPT_VERIFY(pScriptArtMessageParams, "GIVE_PED_NM_MESSAGE - Message does not exist")&&
					SCRIPT_VERIFY(nMessageName!=-1, "GIVE_PED_NM_MESSAGE - Message type not set")&&
					SCRIPT_VERIFY(pPed->GetRagdollInst()->GetNMAgentID() != -1, "GIVE_PED_NM_MESSAGE - Ped isn't using a NM ragdoll"))
				{
					bool bIsRagdollControlledByScript = false;
					if(pPed->GetPedIntelligence()->GetTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_NM_SCRIPT_CONTROL)
					{
						bIsRagdollControlledByScript = true;
					}
					else
					{
						const s32 iNumEvents = pPed->GetPedIntelligence()->CountEventGroupEvents();
						for( s32 i = 0; i < iNumEvents; i++ )
						{
							CEvent* pEvent = pPed->GetPedIntelligence()->GetEventByIndex(i);
							if( pEvent && pEvent->GetEventType() == EVENT_SWITCH_2_NM_TASK )
							{
								CEventSwitch2NM* pEventSwitch = static_cast<CEventSwitch2NM*>(pEvent);
								if( pEventSwitch->GetTaskNM() && pEventSwitch->GetTaskNM()->GetTaskType()==CTaskTypes::TASK_NM_SCRIPT_CONTROL)
									bIsRagdollControlledByScript = true;
							}
						}
					}

					// ragdoll needs to be controlled by a CTaskNMScriptControl task for it to be valid for the script to send behaviour messages
					scriptAssertf(bIsRagdollControlledByScript, "%s:GIVE_PED_NM_MESSAGE - Ragdoll isn't being controlled by a NM_SCRIPT_CONTROL task", CTheScripts::GetCurrentScriptNameAndProgramCounter());

					if(pScriptArtMessageParams && pPed && pPed->GetRagdollInst()->GetNMAgentID() != -1 && bIsRagdollControlledByScript)
					{
						pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG((eNMStrings)nMessageName), pScriptArtMessageParams);
					}

					// delete this message now we've used it.
					if(pScriptArtMessageParams)
						delete pScriptArtMessageParams;
					pScriptArtMessageParams = NULL;
					nMessageName = -1;
				}
			}
		}
	}


	void CommandSetNMMessageFloat(int nMessageParam, float fValue)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_FLOAT - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_FLOAT - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_FLOAT - Message param is not valid for this message type"))
				{
					pScriptArtMessageParams->addFloat(NMSTR_PARAM((eNMStrings)nMessageParam), fValue);
				}
			}
		}
	}

	void CommandSetNMMessageInt(int nMessageParam, int nValue)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_INT - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_INT - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_INT - Message param is not valid for this message type"))
				{
					pScriptArtMessageParams->addInt(NMSTR_PARAM((eNMStrings)nMessageParam), nValue);
				}
			}
		}
	}

	void CommandSetNMMessageBool(int nMessageParam, bool bValue)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_BOOL - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_BOOL - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_BOOL - Message param is not valid for this message type"))
				{
					pScriptArtMessageParams->addBool(NMSTR_PARAM((eNMStrings)nMessageParam), bValue);
				}
			}
		}
	}

	void CommandSetNMMessageVec3(int nMessageParam, const scrVector & vParam)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_VEC3 - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_VEC3 - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_VEC3 - Message param is not valid for this message type"))
				{
					pScriptArtMessageParams->addVector3(NMSTR_PARAM((eNMStrings)nMessageParam), vParam.x, vParam.y, vParam.z);
				}
			}
		}
	}

	void CommandSetNMMessageInstanceIndex(int nMessageParam, int PedIndex, int CarIndex, int ObjectIndex)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_INSTANCE_INDEX - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_INSTANCE_INDEX - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_INSTANCE_INDEX - Message param is not valid for this message type"))
				{
					CEntity* pEntity = NULL;
					if(PedIndex!=NULL_IN_SCRIPTING_LANGUAGE)
					{
						CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
						if (pPed)
						{
							pEntity = pPed;
						}
						else
						{
							return;
						}
					}

					if(CarIndex!=NULL_IN_SCRIPTING_LANGUAGE)
					{
						if (SCRIPT_VERIFY(pEntity==NULL, "SET_NM_MESSAGE_INSTANCE_INDEX - Can't specify a vehicle and a ped"))
						{
							CVehicle* pCar = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(CarIndex);
							if (pCar)
							{
								pEntity = pCar;
							}
							else
							{
								return;
							}
						}
					}

					if(ObjectIndex!=NULL_IN_SCRIPTING_LANGUAGE)
					{
						if (SCRIPT_VERIFY(pEntity==NULL, "SET_NM_MESSAGE_INSTANCE_INDEX - Can't specify an object as well as a vehicle or ped"))
						{
							CObject* pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
							if (pObj)
							{
								pEntity = pObj;
							}
							else
							{
								return;
							}
						}
					}
					
					if (SCRIPT_VERIFY(pEntity, "SET_NM_MESSAGE_INSTANCE_INDEX - No entity was specified"))
					{
						if (SCRIPT_VERIFY(pEntity->GetCurrentPhysicsInst(), "SET_NM_MESSAGE_INSTANCE_INDEX - entity has no physics")&&
							SCRIPT_VERIFY(pEntity->GetCurrentPhysicsInst()->IsInLevel(), "SET_NM_MESSAGE_INSTANCE_INDEX - entity is not in physics level"))
						{
							pScriptArtMessageParams->addInt(NMSTR_PARAM((eNMStrings)nMessageParam), pEntity->GetCurrentPhysicsInst()->GetLevelIndex());
						}
					}
				}
			}
		}
	}

	void CommandSetNMMessageString(int nMessageParam, const char *pStringValue)
	{
		if (SCRIPT_VERIFY(pScriptArtMessageParams, "SET_NM_MESSAGE_STRING - Message does not exist"))
		{
			if (SCRIPT_VERIFY(!CTaskNMBehaviour::IsMessageString((eNMStrings)nMessageParam), "SET_NM_MESSAGE_STRING - Message enum is not a param type"))
			{
				if (SCRIPT_VERIFY(CTaskNMBehaviour::IsParamForThisMessage((eNMStrings)nMessageName, (eNMStrings)nMessageParam), "SET_NM_MESSAGE_STRING - Message param is not valid for this message type"))
				{
					pScriptArtMessageParams->addString(NMSTR_PARAM((eNMStrings)nMessageParam), pStringValue);
				}
			}
		}
	}

	void CommandSetPedNMAnimPose(int PedIndex, const char *pAnimDictName, const char *pAnimName, float fPhase)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		
		if (pPed)
		{
			if (SCRIPT_VERIFY(pPed->GetUsingRagdoll(), "SET_NM_ANIM_POSE - Ped is not using ragdoll"))
			{
				const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pAnimDictName, pAnimName);
				
				if (SCRIPT_VERIFY(pClip, "SET_PED_NM_ANIM_POSE - Anim doesn't exist"))
				{
					pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, fPhase);
				}
			}
		}
	}

	enum eScriptNMFeedbackType
	{
		NM_FB_SUCCESS = 0,
		NM_FB_FAILURE,
		NM_FB_FINISH
	};

	bool CommandGetPedNMFeedback(int PedIndex, int nMessageFeedback, int nFeedbackType)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY(pPed->GetUsingRagdoll(), "GET_PED_NM_FEEDBACK - Ped is not using ragdoll")&&
				SCRIPT_VERIFY(pPed->GetRagdollInst()->GetNMAgentID() != -1, "GET_PED_NM_FEEDBACK - Ped isn't using a NM ragdoll"))
			{

				CTaskNMScriptControl* pNMScriptTask = NULL;
				if(pPed && pPed->GetPedIntelligence()->GetTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_NM_SCRIPT_CONTROL)
					pNMScriptTask = (CTaskNMScriptControl*)pPed->GetPedIntelligence()->GetTaskActiveSimplest();

				// too messy to deal with task not having started yet, so lets not bother with this assert
				if(pNMScriptTask)
				{
					if(nFeedbackType==NM_FB_SUCCESS)
					{
						return pNMScriptTask->HasSucceeded(eNMStrings(nMessageFeedback));
					}
					else if(nFeedbackType==NM_FB_FAILURE)
					{
						return pNMScriptTask->HasFailed(eNMStrings(nMessageFeedback));
					}
					else
					{
						return pNMScriptTask->HasFinished(eNMStrings(nMessageFeedback));
					}
				}
			}
		}
		return false;
	}

	// Add a cover blocking area
	int CommandAddScenarioSpawningBlockingArea( const scrVector & vStart, const scrVector & vEnd, bool bNetwork, bool bCancelActive, bool bBlockPeds, bool bBlockVehicles )
	{
		if(!SCRIPT_VERIFY(bBlockPeds || bBlockVehicles, "ADD_SCENARIO_BLOCKING_AREA - doesn't block peds and doesn't block vehicles, no area will be created."))
		{
			return CScriptResource_ScenarioBlockingArea::INVALID_REFERENCE;
		}

		Vector3 vStartV3 = vStart;
		Vector3 vEndV3 = vEnd;
		CScriptResource_ScenarioBlockingArea blockingArea(vStartV3, vEndV3, bCancelActive, bBlockPeds, bBlockVehicles);
		int blockingAreaID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(blockingArea);

#if !__FINAL
		scriptDebugf1("%s: Adding Scenario Blocking Area Locally: ID:%d (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) CA: %s, Block Peds: %s Block Vehicles: %s",
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),
			blockingAreaID,
			vStartV3.x, vStartV3.y, vStartV3.z,
			vEndV3.x, vEndV3.y, vEndV3.z,
			bCancelActive ? "true" :  "false",
			bBlockPeds ? "true" :  "false",
			bBlockVehicles ? "true" :  "false");
		scrThread::PrePrintStackTrace();
#endif // !__FINAL

        if(NetworkInterface::IsGameInProgress() && bNetwork)
        {
#if !__FINAL
            scriptDebugf1("%s: Adding Scenario Blocking Area Over Network: ID:%d (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f) CA: %s, Block Peds: %s Block Vehicles: %s",
                CTheScripts::GetCurrentScriptNameAndProgramCounter(),
                blockingAreaID,
                vStartV3.x, vStartV3.y, vStartV3.z,
                vEndV3.x, vEndV3.y, vEndV3.z,
                bCancelActive ? "true" :  "false",
                bBlockPeds ? "true" :  "false",
                bBlockVehicles ? "true" :  "false");
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            NetworkInterface::AddScenarioBlockingAreaOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), vStartV3, vEndV3, blockingAreaID, bCancelActive, bBlockPeds, bBlockVehicles);
        }

        return blockingAreaID;
	}

	bool CommandDoesScenarioBlockingAreaExists(const scrVector & vStart, const scrVector & vEnd)
	{
		Vector3 vStartV3 = vStart;
		Vector3 vEndV3 = vEnd;

		bool result = CScenarioBlockingAreas::IsScenarioBlockingAreaExists(vStartV3, vEndV3);
#if !__FINAL
		scriptDebugf1("DOES_SCENARIO_BLOCKING_AREA_EXISTS for start: (%.2f, %.2f, %.2f) end:  (%.2f, %.2f, %.2f) returns: %s",
			vStartV3.x, vStartV3.y, vStartV3.z,
			vEndV3.x, vEndV3.y, vEndV3.z,
			result ? "TRUE":"FALSE");
#endif

		return result;
	}

	// Add a scripted cover point
	void CommandRemoveScenarioSpawningBlockingAreas()
	{
		// Deprecated...
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveAllScriptResourcesOfType( CGameScriptResource::SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA );
	}

	//INFO:
	//PARAM NOTES:
	// PURPOSE: Removes a scenario blocking area by SCENARIO_BLOCKING_INDEX
	void CommandRemoveScenarioBlockingArea( int scenarioBlockingIndex, bool bNetwork )
	{
        if(NetworkInterface::IsGameInProgress() && bNetwork)
        {
#if !__FINAL
            scriptDebugf1("%s: Removing Scenario Blocking Area: ID:%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scenarioBlockingIndex);
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            NetworkInterface::RemoveScenarioBlockingAreaOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), scenarioBlockingIndex);
        }

		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource( CGameScriptResource::SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA, scenarioBlockingIndex );
	}

	// Add a scripted cover point
	void CommandSetScenarioPedsSpawnInSphereArea( const scrVector & svCenter, float fRadius, s32 iMaxPeds )
	{
		Vector3 vCenter = svCenter;
		if (SCRIPT_VERIFY(iMaxPeds <= 30, "SET_SCENARIO_PEDS_SPAWN_IN_SPHERE_AREA - spawning more than 30 peds might be dodgy!"))
		{
			const bool allowDeepInteriors = true;
			CPedPopulation::GenerateScenarioPedsInSpecificArea(vCenter, allowDeepInteriors, fRadius, iMaxPeds, CPedPopulation::SF_ignoreSpawnTimes|CPedPopulation::SF_forceSpawn);
		}
	}

	bool CommandIsPedUsingScenario(int iPedIndex, const char* szScenario)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			s32 type = SCENARIOINFOMGR.GetScenarioIndex(atStringHash(szScenario), true, true);

			if (type!=Scenario_Invalid)
			{
				const int usingRealScenarioType = pPed->GetScenarioType(*pPed);
				if(SCENARIOINFOMGR.IsVirtualIndex(type))
				{
					// It's a virtual type, so loop over all the real possibilities and see if there is a match.
					// This doesn't check if the actual scenario point is of the proper virtual type, but it
					// may be good enough.
					const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(type);
					for(int i = 0; i < numRealTypes; i++)
					{
						if(SCENARIOINFOMGR.GetRealScenarioType(type, i) == usingRealScenarioType)
						{
							return true;
						}
					}
					return false;
				}
				else
				{
					return usingRealScenarioType == type;
				}
			}
			else
			{
				scriptAssertf(type!=Scenario_Invalid, "IS_PED_USING_SCENARIO: Unknown scenario type (%s)", szScenario );
			}
		}
		return false;
	}


	bool CommandIsPedUsingAnyScenario(int iPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			return pPed->GetScenarioType(*pPed, true) != Scenario_Invalid;
		}
		return false;
	}

	CTaskUseScenario* HelperFindTaskScenario(int iPedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			return pTask;
		}
		return NULL;
	}
	
	bool CommandSetPedPanicExitScenario(int iPedIndex, const scrVector & vAmbientPosition)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iPedIndex);

		if (!scriptVerifyf(pPed, "Expected a valid ped!"))
		{
			return false;
		}

		if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO))
		{
			CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTask)
			{
				pTask->ScriptForcePanicExit(VECTOR3_TO_VEC3V((Vector3)vAmbientPosition), true);
				return true;
			}

		}
		else if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COWER_SCENARIO))
		{
			CTaskCowerScenario* pTask = static_cast<CTaskCowerScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COWER_SCENARIO));
			if (pTask)
			{
				pTask->TriggerPanicExit();
				return true;
			}
		}
		scriptAssertf(pPed->GetScenarioType(*pPed, true) != Scenario_Invalid, "SET_PED_PANIC_EXIT_SCENARIO - The ped is not using a scenario.");
		return false;
	}

	void CommandToggleScenarioPedCowerInPlace(int iPedIndex, bool bStart)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (SCRIPT_VERIFY(pPed, "Invalid ped specified in TOGGLE_SCENARIO_PED_COWER_IN_PLACE"))
		{
			CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (SCRIPT_VERIFY(pTask, "Ped in TOGGLE_SCENARIO_PED_COWER_IN_PLACE is not using a scenario."))
			{
				if (bStart)
				{
					pTask->TriggerScriptedCowerInPlace();
				}
				else
				{
					pTask->TriggerStopScriptedCowerInPlace();
				}
			}
		}
	}

	bool CommandTriggerPedScenarioPanicExitToFlee(int iPedIndex, const scrVector & vAmbientPosition)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_PANICEXITTOFLEE - The ped is not using a scenario."))
			{   
				CEventScenarioForceAction e;
				CAITarget target(vAmbientPosition);
				e.SetSourcePos(vAmbientPosition);
				e.SetForceActionType(eScenarioActionFlee);
				fwMvClipSetId reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(vAmbientPosition, *pPed);
				fwMvClipSetId reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS;
				fwFlags8 uConfigFlags = 0;
				uConfigFlags.SetFlag(CTaskReactAndFlee::CF_DisableReact);
				uConfigFlags.SetFlag(CTaskReactAndFlee::CF_AcquireExistingMoveTask);
				CTaskReactAndFlee* pTask = rage_new CTaskReactAndFlee(target, reactClipSetId, reactToFleeClipSetId, uConfigFlags); 
				e.SetTask(pTask);
				pPed->GetPedIntelligence()->AddEvent(e);
				return true;
			}				
		}
		return false;
	}

	bool CommandTriggerPedScenarioPanicExitToCombat(int iPedIndex, int iTargetIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_PANICEXITTOCOMBAT - The ped is not using a scenario."))
			{   
				const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iTargetIndex);
				if (pTargetPed)
				{
					CEventScenarioForceAction e;
					CAITarget target(pTargetPed);
					Vector3 position;
					target.GetPosition(position);
					e.SetSourcePos(position);
					e.SetForceActionType(eScenarioActionCombatExit);
					e.SetTask(rage_new CTaskThreatResponse(pTargetPed));
					pPed->GetPedIntelligence()->AddEvent(e);
					return true;
				}
			}
		}
		return false;
	}

	bool CommandTriggerPedScenarioCowardThenResume(int iPedIndex, int iTargetIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_COWARDTHENRESUME - The ped is not using a scenario."))
			{   
				const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iTargetIndex);
				if (pTargetPed)
				{
					CEventScenarioForceAction e;
					CAITarget target(pTargetPed);
					Vector3 position;
					target.GetPosition(position);
					e.SetSourcePos(position);
					e.SetForceActionType(eScenarioActionTempCowardReaction);
					e.SetTask(rage_new CTaskThreatResponse(pTargetPed));
					pPed->GetPedIntelligence()->AddEvent(e);
					return true;
				}
			}
		}
		return false;
	}

	bool CommandTriggerPedScenarioCowardThenExit(int iPedIndex, const scrVector & vAmbientPosition)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_PANICEXITTOFLEE - The ped is not using a scenario."))
			{   
				CEventScenarioForceAction e;
				CAITarget target(vAmbientPosition);
				e.SetSourcePos(vAmbientPosition);
				e.SetForceActionType(eScenarioActionCowardExit);
				e.SetTask(rage_new CTaskSmartFlee(target));
				pPed->GetPedIntelligence()->AddEvent(e);
				return true;
			}				
		}
		return false;
	}

	bool CommandTriggerPedScenarioAggroThenExit(int iPedIndex, int iTargetIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_AGGROTHENEXIT - The ped is not using a scenario."))
			{   
				const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iTargetIndex);
				if (pTargetPed)
				{
					CEventScenarioForceAction e;
					CAITarget target(pTargetPed);
					Vector3 position;
					target.GetPosition(position);
					e.SetSourcePos(position);
					e.SetForceActionType(eScenarioActionThreatResponseExit);
					e.SetTask(rage_new CTaskThreatResponse(pTargetPed));
					pPed->GetPedIntelligence()->AddEvent(e);
					return true;
				}
			}	
		}
		return false;
	}


	bool CommandTriggerPedScenarioShockAnimation(int iPedIndex, int iTargetIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_SHOCKANIMATION - The ped is not using a scenario."))
			{ 
				const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(iTargetIndex);
				if (pTargetPed)
				{
					CEventScenarioForceAction e;
					CAITarget target(pTargetPed);
					Vector3 position;
					e.SetSourcePos(position);
					e.SetForceActionType(eScenarioActionShockReaction);
					e.SetTask(rage_new CTaskThreatResponse(pTargetPed));
					pPed->GetPedIntelligence()->AddEvent(e);
					return true;
				}
			}	
		}
		return false;
	}

	bool CommandTriggerPedScenarioHeadLook(int iPedIndex, const scrVector & vAmbientPosition)
	{
		const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTask = HelperFindTaskScenario(iPedIndex);
			if (SCRIPT_VERIFY(pTask, "TRIGGER_PED_SCENARIO_HEADLOOK - The ped is not using a scenario."))
			{   
				CEventScenarioForceAction e;
				e.SetSourcePos(vAmbientPosition);
				e.SetForceActionType(eScenarioActionHeadTrack);
				pPed->GetPedIntelligence()->AddEvent(e);
				return true;
			}	
		}
		return false;
	}

	bool CommandSetPedShouldPlayDirectedNormalScenarioExitOnNextCommand(int iPedIndex, const scrVector & vReactionDirection)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario::ClearScriptedTriggerFlags(*pPed);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayDirectedNormalScenarioExitOnNextScriptCommand, true);
			CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTaskScenario)
			{
				pTaskScenario->SetAmbientPosition(vReactionDirection);
			}

			return true;
		}

		// There was no ped.
		return false;
	}

	void CommandSetPedShouldPlayNormalScenarioExitOnNextCommand(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario::ClearScriptedTriggerFlags(*pPed);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayNormalScenarioExitOnNextScriptCommand, true);
		}
	}

	void CommandSetPedShouldPlayImmediateScenarioExitOnNextCommand(int iPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario::ClearScriptedTriggerFlags(*pPed);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayImmediateScenarioExitOnNextScriptCommand, true);
		}
	}

	bool CommandSetPedShouldPlayFleeScenarioExitOnNextCommand(int iPedIndex, const scrVector & vReactionDirection)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTaskScenario)
			{
				CTaskUseScenario::ClearScriptedTriggerFlags(*pPed);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayFleeScenarioExitOnNextScriptCommand, true);
				pTaskScenario->SetAmbientPosition(vReactionDirection);
				// Position was set, return true.
				return true;
			}
			else
			{
				// Ped is not in a scenario.
				return false;
			}
		}

		// There was no ped.
		return false;
	}

	void CommandSetPedIgnoresScenarioExitCollisionChecks(int iPedIndex, bool bIgnore)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioExitProbeChecks, bIgnore);
		}
	}

	void CommandSetPedIgnoresScenarioNavChecks(int iPedIndex, bool bIgnore)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_NeverDoScenarioNavChecks, bIgnore);
		}
	}

	void CommandSetPedProbesForScenarioExitsInOneFrame(int iPedIndex, bool bOneFrame)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceSynchronousScenarioExitChecking, bOneFrame);
		}
	}

	bool CommandIsPedGesturing(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(pPed->IsPlayingAGestureAnim())
			{
				return true;
			}
		}
		return false;
	}

	void CommandSetPedCanHeadIk(int PedIndex, bool bAllowHeadIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			//Temp fix
			if (!bAllowHeadIK)
				pPed->SetHeadIkBlocked();
		}
	}

	void CommandSetPedCanPlayGestureAnims(int PedIndex, bool bAllowGestureAnims)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetGestureAnimsBlockedFromScript(!bAllowGestureAnims);
			pPed->SetGestureAnimsAllowed(bAllowGestureAnims);
		}
	}

	enum ePedVisemeFlags
	{
		PVF_Default = 0, //0
		PVF_AllowVisemeAnimsAudio = 1, // 1<<0
	};

	void CommandSetPedCanPlayVisemeAnims(int PedIndex, bool bAllowVisemeAnims, s32 flags = PVF_Default)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			// If anims are allowed audio is allowed, otherwise look at the flag
			bool bAllowVisemeAnimsAudio = bAllowVisemeAnims || (flags & PVF_AllowVisemeAnimsAudio) != 0;

			//Temp fix
			bool bBlockVisemeAnims = !bAllowVisemeAnims;
			bool bBlockVisemeAnimsAudio = !bAllowVisemeAnimsAudio;

			pPed->SetVisemeAnimsBlocked(bBlockVisemeAnims);
			pPed->SetVisemeAnimsAudioBlocked(bBlockVisemeAnimsAudio);
		}
	}

	void CommandSetPedIgnoredByAutoOpenDoors(int PedIndex, bool bIgnored)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (scriptVerifyf(pPed, "Invalid ped %d", PedIndex))
		{
			pPed->SetIgnoredByAutoOpenDoors(bIgnored);
		}
	}

	void CommandSetPedCanPlayAmbientAnims(int PedIndex, bool bBlockAmbientAnims)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			pPed->SetAmbientAnimsBlocked(!bBlockAmbientAnims);
		}
	}

	void CommandSetPedCanPlayAmbientBaseAnims(int PedIndex, bool bBlockAmbientBaseAnims)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			pPed->SetAmbientIdleAndBaseAnimsBlocked(!bBlockAmbientBaseAnims);
		}
	}

	void CommandTriggerIdleAnimationOnPed(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pTaskAmbientClips)
			{
				pTaskAmbientClips->ForcePickNewIdleAnim();
			}
		}
	}

	void CommandSetFacialIdleAnim(int PedIndex, const char * pFacialClip)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetFacialData())
		{
			fwMvClipId facialIdleClip(pFacialClip);
			pPed->GetFacialData()->SetFacialIdleClipID(pPed, facialIdleClip);
		}
	}

	void CommandResetFacialIdleAnim(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetFacialData())
		{
			pPed->GetFacialData()->ResetFacialIdleAnimation(pPed);
		}
	}

	void CommandPlayFacialAnim(int PedIndex, const char * pFacialClip, const char * pFacialDictName)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (scriptVerifyf(pPed, "PLAY_FACIAL_ANIM - Invalid ped %d", PedIndex))
		{
			if (pPed->GetFacialData())
			{
				if (pFacialDictName)
				{
					const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pFacialDictName, pFacialClip);
					if (scriptVerifyf(pClip, "PLAY_FACIAL_ANIM - Clip or Dictionary not found. Check that dictionary: %s is loaded and anim: %s is contained within the dictionary", pFacialDictName, pFacialClip))
					{
						// Play using specified anim dictionary
						pPed->GetFacialData()->PlayFacialAnimByClip(pPed, pClip);
					}
				}
				else
				{
					// Play with standard facial clipset
					fwMvClipId facialClip(pFacialClip);
					pPed->GetFacialData()->PlayFacialAnim(pPed, facialClip);
				}
			}
		}
	}

	void CommandSetFacialClipset(int PedIndex, const char * pFacialClipsetName)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (scriptVerifyf(pPed, "SET_FACIAL_CLIPSET - Invalid ped %d", PedIndex))
		{
			if (pPed->GetFacialData())
			{
				if (scriptVerifyf(pFacialClipsetName, "SET_FACIAL_CLIPSET - Null clipset name passed in"))
				{
					fwMvClipSetId facialClipset(pFacialClipsetName);
					if (scriptVerifyf(fwClipSetManager::GetClipSet(facialClipset), "SET_FACIAL_CLIPSET - Could not find facial clipset"))
					{
						pPed->GetFacialData()->SetFacialClipSet(pPed, facialClipset);
					}
				}
			}
		}
	}

	void CommandClearFacialClipset(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (scriptVerifyf(pPed, "Invalid ped %d", PedIndex))
		{
			if (pPed->GetFacialData())
			{
				pPed->GetFacialData()->SetFacialClipsetToDefault(pPed);
			}
		}
	}

	void CommandSetFacialIdleAnimOverride(int PedIndex, const char * pOverrideIdleClipName, const char * pOverrideIdleClipDictName)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (scriptVerifyf(pPed, "SET_FACIAL_IDLE_ANIM_OVERRIDE - Invalid ped %d", PedIndex))
		{
			if(!NetworkUtils::IsNetworkCloneOrMigrating(pPed))
			{
				if (scriptVerifyf(pOverrideIdleClipName, "SET_FACIAL_IDLE_ANIM_OVERRIDE - Null clip name for ped %d", PedIndex))
				{
					if (pPed->GetFacialData())
					{
						// Play the override
						atHashString clipName(pOverrideIdleClipName);
						atHashString clipDictName;
						if(pOverrideIdleClipDictName)
						{
							clipDictName.SetFromString(pOverrideIdleClipName);
						}

						pPed->GetFacialData()->SetFacialIdleAnimOverride(pPed, clipName.GetHash(), clipDictName.GetHash(), pOverrideIdleClipName, pOverrideIdleClipDictName);

#if ENABLE_NETWORK_LOGGING
						if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
						{
							NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "SET_FACIAL_IDLE_ANIM_OVERRIDE", pPed->GetNetworkObject()->GetLogName());
							NetworkInterface::GetObjectManagerLog().WriteDataValue("Clear", "FALSE");
							NetworkInterface::GetObjectManagerLog().WriteDataValue("ClipName", "%s", pOverrideIdleClipName ? pOverrideIdleClipName : "None");
							NetworkInterface::GetObjectManagerLog().WriteDataValue("ClipDictName", "%s", pOverrideIdleClipDictName ? pOverrideIdleClipDictName : "None");
						}
#endif // ENABLE_NETWORK_LOGGING
					}
				}
			}
			else
			{
				if (pPed->GetFacialData())
				{
					atHashString clipName(pOverrideIdleClipName);
					atHashString clipDictName(pOverrideIdleClipDictName);
					CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride parameters(false, clipName.GetHash(), clipDictName.GetHash());
					CScriptEntityStateChangeEvent::Trigger(pPed->GetNetworkObject(), parameters);
				}
			}
		}
	}
	
	void CommandClearFacialIdleAnimOverride(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if (scriptVerifyf(pPed, "SET_FACIAL_IDLE_ANIM_OVERRIDE - Invalid ped %d", PedIndex))
		{
			if (pPed->GetFacialData())
			{
				if(!NetworkUtils::IsNetworkCloneOrMigrating(pPed))
				{

					// Play the override
					pPed->GetFacialData()->ClearFacialIdleAnimOverride(pPed);

#if ENABLE_NETWORK_LOGGING
					if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
					{
						NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), pPed->GetNetworkObject()->GetPhysicalPlayerIndex(), "SET_FACIAL_IDLE_ANIM_OVERRIDE", pPed->GetNetworkObject()->GetLogName());
						NetworkInterface::GetObjectManagerLog().WriteDataValue("Clear", "TRUE");
					}
#endif // ENABLE_NETWORK_LOGGING
				}
				else
				{
					CScriptEntityStateChangeEvent::CSetPedFacialIdleAnimOverride parameters(true, 0, 0);
					CScriptEntityStateChangeEvent::Trigger(pPed->GetNetworkObject(), parameters);
				}
			}
		}
	}

	void CommandSetPedCanArmIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_ARM_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableArmSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_ARM_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanBodyRecoilIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_BODY_RECOIL_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableBodyRecoilSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_BODY_RECOIL_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanHeadIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_HEAD_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableHeadSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_HEAD_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanLegIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_LEG_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableLegSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_LEG_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanTorsoIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableTorsoSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanTorsoReactIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_REACT_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableTorsoReactSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_REACT_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanTorsoVehicleIK(int PedIndex, bool bEnableIK)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_VEHICLE_IK(pPed %p %s, bEnableIK %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bEnableIK ? "TRUE" : "FALSE");

			pPed->SetDisableTorsoVehicleSolver(!bEnableIK);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_TORSO_VEHICLE_IK(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bEnableIK ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedCanUseAutoConversationLookAt(int PedIndex, bool bAllowAutoConversationLookAts)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex,CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			ikDebugf3("%u SET_PED_CAN_USE_AUTO_CONVERSATION_LOOK_AT(pPed %p %s, bAllowAutoConversationLookAts %s)",
				fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), bAllowAutoConversationLookAts ? "TRUE" : "FALSE");

			pPed->SetAutoConversationLookAts(bAllowAutoConversationLookAts);
		}
		else
		{
			ikDebugf3("%u SET_PED_CAN_USE_AUTO_CONVERSATION_LOOK_AT(PedIndex %i, bEnableIK %s) Could not find ped!\n%s",
				fwTimer::GetFrameCount(), PedIndex, bAllowAutoConversationLookAts ? "TRUE" : "FALSE",
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	bool CommandIsPedHeadtrackingPed(int FirstPedIndex, int SecondPedIndex)
	{
		bool bReturnValue = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(FirstPedIndex);
		if (pPed)
		{
			if (pPed->GetIkManager().GetLookAtEntity())
			{
				if (SecondPedIndex == NULL_IN_SCRIPTING_LANGUAGE)
				{	//	return true if first ped is looking at any ped
					if (pPed->GetIkManager().GetLookAtEntity()->GetIsTypePed())
					{
						bReturnValue = true;
					}
				}
				else
				{
					const CPed *pSecondPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(SecondPedIndex);
					if (pSecondPed)
					{
						if (pPed->GetIkManager().GetLookAtEntity() == pSecondPed)
						{
							bReturnValue = true;
						}
					}
				}
			}
		}

		return bReturnValue;
	}

	bool CommandIsPedHeadtrackingEntity(int PedIndex, int EntityIndex)
	{
		bool bReturnValue = false;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetIkManager().GetLookAtEntity())
			{
				if (EntityIndex == NULL_IN_SCRIPTING_LANGUAGE)
				{	
					//	return true if first ped is looking at any entity
					if (pPed->GetIkManager().GetLookAtEntity() )
					{
						bReturnValue = true;
					}
				}
				else
				{
					const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(EntityIndex);
					if (pEntity)
					{
						if (pPed->GetIkManager().GetLookAtEntity() == pEntity)
						{
							bReturnValue = true;
						}
					}
				}
			}
		}

		return bReturnValue;
	}

	void CommandSetPedPrimaryLookAt( int PedIndex, int PrimaryLookAtPedIndex )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_PRIMARY_LOOKAT - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				const CPed *pPrimaryLookAtPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PrimaryLookAtPedIndex);
				if (pPrimaryLookAtPed)
				{
					ikDebugf3("%u SET_PED_PRIMARY_LOOK_AT(pPed %p %s, pPrimaryLookAtPed %p %s)",
						fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), pPrimaryLookAtPed, pPrimaryLookAtPed->GetModelName());

					// We don't check the PrimaryLookAtPedIndex is alive as we may want to look at a dead ped
					pPed->SetPrimaryLookAt(pPrimaryLookAtPed);
				}
				else
				{
					ikDebugf3("%u SET_PED_PRIMARY_LOOK_AT(pPed %p %s, PrimaryLookAtPedIndex %i) Could not find primary look at ped!\n%s",
						fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), PrimaryLookAtPedIndex,
						CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
			else
			{
				ikDebugf3("%u SET_PED_PRIMARY_LOOK_AT(PedIndex %i, PrimaryLookAtPedIndex %i) Could not find ped!\n%s",
					fwTimer::GetFrameCount(), PedIndex, PrimaryLookAtPedIndex,
					CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		else
		{
			ikDebugf3("%u SET_PED_PRIMARY_LOOK_AT(PedIndex %i, PrimaryLookAtPedIndex %i) Network game is not in progress!\n%s",
				fwTimer::GetFrameCount(), PedIndex, PrimaryLookAtPedIndex,
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedSecondaryLookAt( int PedIndex, int SecondaryLookAtPedIndex )
	{
		// this script command is not approved for use in network scripts
		if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "SET_PED_SECONDARY_LOOKAT - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				const CPed *pSecondaryLookAtPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(SecondaryLookAtPedIndex);
				if (pSecondaryLookAtPed)
				{
					ikDebugf3("%u SET_PED_SECONDARY_LOOK_AT(pPed %p %s, pSecondaryLookAtPed %p %s)",
						fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), pSecondaryLookAtPed, pSecondaryLookAtPed->GetModelName());

					// We don't check the SecondaryLookAtPedIndex is alive as we may want to look at a dead ped
					pPed->SetSecondaryLookAt(pSecondaryLookAtPed);
				}
				else
				{
					ikDebugf3("%u SET_PED_SECONDARY_LOOK_AT(pPed %p %s, SecondaryLookAtPedIndex %i) Could not find secondary look at ped!\n%s",
						fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), SecondaryLookAtPedIndex,
						CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
			else
			{
				ikDebugf3("%u SET_PED_SECONDARY_LOOK_AT(PedIndex %i, SecondaryLookAtPedIndex %i) Could not find ped!\n%s",
					fwTimer::GetFrameCount(), PedIndex, SecondaryLookAtPedIndex,
					CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		else
		{
			ikDebugf3("%u SET_PED_SECONDARY_LOOK_AT(PedIndex %i, SecondaryLookAtPedIndex %i) Network game is not in progress!\n%s",
				fwTimer::GetFrameCount(), PedIndex, SecondaryLookAtPedIndex,
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandSetPedClothProne(int PedIndex, bool bTrue)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			pPed->SetClothIsProne( bTrue );
		}
	}

	void CommandSetPedClothPinFrames(int PedIndex, int pinFrames)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			pPed->SetClothForcePin( (u8)pinFrames );
		}
	}

	void CommandSetPedClothPose(int PedIndex, int poseIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			pPed->SetClothPoseIndex( (u8)poseIndex );
		}
	}

	void CommandSetPedClothPackageIndex(int PedIndex, int packageIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			pPed->SetClothPackageIndex( (u8)packageIndex, 0 );
		}
	}

	void CommandQueuePedClothPose(int PedIndex, int poseIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			pPed->QueueClothPoseIndex( (u8)poseIndex );
		}
	}


	void CommandSetPedConfigFlag(int PedIndex, int Flag, bool bTrue)
	{
#if __ASSERT
		switch (Flag)
		{
		case CPED_CONFIG_FLAG_IsDuckingInVehicle:
			{
				const char* name = PARSER_ENUM(ePedConfigFlags).NameFromValueUnsafe(Flag);
				scriptAssertf(0,"CommandSetPedConfigFlag Flag[%d][%s] is not meant to be set from script",Flag,name);
				return;
			}
		default: 
			break;
		}
#endif // __ASSERT

		CPed* pPed = NULL;

		int AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
		switch (Flag)
		{
			case CPED_CONFIG_FLAG_DontInfluenceWantedLevel :	// Exception it is not synched in the network game and needs to be set on clones.
			case CPED_CONFIG_FLAG_DontBlipNotSynced :	// Exception it is not synched in the network game and needs to be set on clones.
				AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
				break;

#if ENABLE_DRUNK
			case CPED_CONFIG_FLAG_IsDrunk:							// Need to be able to clear drunk even on dead peds or they keep it when they revive
#endif // ENABLE_DRUNK
			case CPED_CONFIG_FLAG_ForceDirectEntry :
			case CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat :
			case CPED_CONFIG_FLAG_WillFlyThroughWindscreen :
			case CPED_CONFIG_FLAG_DisableLockonToRandomPeds :
			case CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown :
			case CPED_CONFIG_FLAG_DisableVehicleCombat :
			case CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning:
			case CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations:
				AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
				break;
		}

		pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, AssertFlags);

		if (pPed)
		{
			if (!SCRIPT_VERIFY(Flag>=0 && Flag < ePedConfigFlags_NUM_ENUMS, "SET_PED_CONFIG_FLAG - Invalid flag index!"))
			{
				return;
			}

			// Some flags may create conflicts in certain situations. With this
			// guard we'll make sure that the flag gets set only if it's safe to
			// do so
			bool bShouldSetFlag = true;

			if( Flag == CPED_CONFIG_FLAG_ForceSkinCharacterCloth )
			{
				pPed->SetClothForceSkin(bTrue);
			}
#if __DEV
			if (Flag == CPED_CONFIG_FLAG_WillFlyThroughWindscreen)
			{
				scriptDisplayf("Ped %s set (%s) fly through windscreen at frame %i, via script %s", pPed->GetDebugName() ? pPed->GetDebugName() : "NULL", bTrue ? "to" : "to not", fwTimer::GetFrameCount(), CTheScripts::GetCurrentScriptNameAndProgramCounter());
				scrThread::PrePrintStackTrace();
			}
#endif // __DEV

			// Don't bother setting the flag if the value is already the same as the desired value
			if (bTrue == pPed->GetPedConfigFlag((ePedConfigFlags)Flag))
			{
				return;
			}

#if !__NO_OUTPUT

			bool bIsPlayer = pPed->IsPlayer();
			bool bFlagNetworkSafe = CPedScriptGameStateDataNode::IsScriptPedConfigFlagSynchronized(Flag) ||
									(Flag == CPED_CONFIG_FLAG_TreatAsPlayerDuringTargeting) ||
									(Flag == CPED_CONFIG_FLAG_DontBlipNotSynced)			||
									(Flag == CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) ||
									(Flag == CPED_CONFIG_FLAG_PlayerIsWeird && bIsPlayer) || 
									(Flag == CPED_CONFIG_FLAG_SwatHeliSpawnWithinLastSpottedLocation && bIsPlayer) ||
									(Flag == CPED_CONFIG_FLAG_DisableStartEngine && bIsPlayer) ||
									(Flag == CPED_CONFIG_FLAG_DisableTakeOffScubaGear && bIsPlayer) ||
									(Flag == CPED_CONFIG_FLAG_LeaveEngineOnWhenExitingVehicles && bIsPlayer) || 
									(Flag == CPED_CONFIG_FLAG_ScriptForceNoTimesliceIntelligenceUpdate) ||
									(Flag == CPED_CONFIG_FLAG_ForcedAim && bIsPlayer);

			if (NetworkInterface::IsGameInProgress() && !bFlagNetworkSafe)
			{
				const char* name = PARSER_ENUM(ePedConfigFlags).NameFromValueUnsafe(Flag);

#if __ASSERT
				Assertf(0,"CommandSetPedConfigFlag Flag[%d][%s] not synchronized but set through script. Create issue for Default Network Code. CPedScriptGameStateDataNode/NetObjPed/ScriptPedFlags needs modification to synchronize.",Flag,name);
#if RAGE_MINIMAL_ASSERTS
				//Supplement the log if we have minimal asserts enabled as the assert above will just assert with a 0 without any good information
				Errorf("CommandSetPedConfigFlag Flag[%d][%s] not synchronized but set through script. Create issue for Default Network Code. CPedScriptGameStateDataNode/NetObjPed/ScriptPedFlags needs modification to synchronize.",Flag,name);
#endif
#else
				Warningf("CommandSetPedConfigFlag Flag[%d][%s] not synchronized but set through script. Create issue for Default Network Code. CPedScriptGameStateDataNode/NetObjPed/ScriptPedFlags needs modification to synchronize.",Flag,name);
#endif
			}
#endif

			if (bShouldSetFlag)
				pPed->SetPedConfigFlag( (ePedConfigFlags)Flag, bTrue );
		}
	}

	void CommandSetPedResetFlag(int PedIndex, int Flag, bool bTrue)
	{
		int AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
		switch (Flag)
		{
            case CPED_RESET_FLAG_CannotBeTargeted:
			case CPED_RESET_FLAG_DontRaiseFistsWhenLockedOn:
			case CPED_RESET_FLAG_AllowHomingMissileLockOnInVehicle:
			case CPED_RESET_FLAG_ScriptDisableSecondaryAnimationTasks:
            case CPED_RESET_FLAG_DisableActionMode:
            case CPED_RESET_FLAG_DisableMeleeHitReactions:
			case CPED_RESET_FLAG_DisableInVehicleActions:
			case CPED_RESET_FLAG_PreventAllMeleeTakedowns:
			case CPED_RESET_FLAG_PreventAllStealthKills:
				AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
				break;
		}

		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, AssertFlags);
		if (pPed)
		{
			if (SCRIPT_VERIFY(Flag>=0 && Flag < ePedResetFlags_NUM_ENUMS, "SET_PED_RESET_FLAG - Invalid flag index!"))
				pPed->SetPedResetFlag( (ePedResetFlags)Flag, bTrue );
		}
	}

	bool CommandGetPedConfigFlag(int PedIndex, int Flag, bool DoDeadCheck)
	{
		bool result = false;

		unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
		if(!DoDeadCheck)
		{
			assertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
		}
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, assertFlags);
		if (pPed)
		{
			if (SCRIPT_VERIFY(Flag>=0 && Flag < ePedConfigFlags_NUM_ENUMS, "GET_PED_CONFIG_FLAG - Invalid flag index!"))
			{
				result = pPed->GetPedConfigFlag( (ePedConfigFlags)Flag );
			#if FPS_MODE_SUPPORTED
				if( (ePedConfigFlags)Flag == CPED_CONFIG_FLAG_IsAimingGun &&
					pPed->IsLocalPlayer() &&
					camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() &&
					camInterface::GetGameplayDirector().IsFirstPersonModeAllowed() )
				{
					const CControl* pControl = pPed->GetControlFromPlayer();
					if( pControl )
					{
						// If in first person camera, only consider player aiming if left or right trigger is held.
						result &= ( pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown() );
					}
				}
			#endif
			}
		}

		return result;
	}

	bool CommandGetPedResetFlag(int PedIndex, int Flag)
	{
		int AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
		switch(Flag)
		{
			case (CPED_RESET_FLAG_BlockRemotePlayerRecording):
			case (CPED_RESET_FLAG_IsSeatShuffling):
				AssertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
				break;
			default:
				break;
		}

		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, AssertFlags);
		if (pPed)
		{
			if (SCRIPT_VERIFY(Flag>=0 && Flag < ePedResetFlags_NUM_ENUMS, "GET_PED_RESET_FLAG - Invalid flag index!"))
				return pPed->GetPedResetFlag( (ePedResetFlags)Flag );
		}
		return false;
	}

	void CommandSetPedCanFlyThroughWindscreen(int PedIndex, bool bTrue)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			scriptAssertf(0, "This script command is deprecated, use SET_PED_CONFIG_FLAG(PedIndex, PCF_WillFlyThroughWindscreen, Val) instead");
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillFlyThroughWindscreen, bTrue );
		}
	}

	void CommandSetPedGroupMemberPassengerIndex(int PedIndex, int iPassengerIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY(iPassengerIndex > -1, "Passenger index should be greater than VS_DRIVER"))
			{
				pPed->m_PedConfigFlags.SetPassengerIndexToUseInAGroup(iPassengerIndex + 1);
#if __DEV
				scriptDisplayf("%s: Frame: %i, Ped %s %p set to prefer seat %i", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fwTimer::GetFrameCount(), pPed->GetDebugName(), pPed, iPassengerIndex);
#endif // __DEV
			}
		}
	}


	void CommandSetPedCanEvasiveDive(int PedIndex, bool bAllow)
	{
	
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			//this is a temp fix whilst because the command gets changed to positive fix after bDisableEvasiveDive changes
			bAllow = !bAllow;

			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableEvasiveDives, bAllow );
		}
	}

	bool CommandIsPedEvasiveDiving(int PedIndex, int& EntityDivingFromIndex)
	{
		EntityDivingFromIndex = NULL_IN_SCRIPTING_LANGUAGE;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CTaskComplexEvasiveStep* pEvasiveStepTask = static_cast<CTaskComplexEvasiveStep*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP));
			if (pEvasiveStepTask)
			{
				if (pEvasiveStepTask->GetTargetEntity())
				{
					EntityDivingFromIndex = CTheScripts::GetGUIDFromEntity(*pEvasiveStepTask->GetTargetEntity());
				}
				return true;
			}
		}
		return false;
	}

	void CommandSetPedGeneratesDeadBodyEvents(int PedIndex, bool bTrue)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedGeneratesDeadBodyEvents, bTrue );
			if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PedGeneratesDeadBodyEvents ) )
			{
				CEventShockingDeadBody ev(*pPed);
				CShockingEventsManager::Add(ev);
			}
		}
	}

	void CommandBlockPedFromGeneratingDeadBodyEventsWhenDead(int PedIndex, bool bBlock)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BlockDeadBodyShockingEventsWhenDead, bBlock);
		}
	}

	void CommandSetPedCanOnlyAttackWantedPlayer(int PedIndex, bool bTrue)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontAttackPlayerWithoutWantedLevel, bTrue );
		}
	}

	bool CommandIsCopPedInArea3d(const scrVector & scrVecMinCoors, const scrVector & scrVecMaxCoors)
	{
		int pedCount;
		int vehicleCount;
		Vector3 vMinCoors( scrVecMinCoors );
		Vector3 vMaxCoors( scrVecMaxCoors );
		return CPedPopulation::GetCopsInArea(vMinCoors, vMaxCoors, NULL, 1, pedCount, NULL, 0, vehicleCount );
	}

	void CommandSetPedShootsAtCoord(int PedIndex, const scrVector & scrVecTarget, bool bSetPerfectAccuracy)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_SHOOTS_AT_COORD - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				if (SCRIPT_VERIFY(pPed->GetWeaponManager()->GetEquippedWeapon(), "SET_PED_SHOOTS_AT_COORD - Ped doesn't have a useable weapon")&&
					SCRIPT_VERIFY(pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsGun(), "SET_PED_SHOOTS_AT_COORD - Ped's weapon isn't a gun"))
				{

					Vector3 vecStart, vecEnd, vecTarget;
					vecTarget =  Vector3(scrVecTarget);

					CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
					if( pEquippedWeapon && pWeaponObject )
					{
						Matrix34 matWeapon;
						if(pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ) != -1)
						{
							pWeaponObject->GetGlobalMtx( pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ), matWeapon );
						}
						else
						{
							pWeaponObject->GetMatrixCopy( matWeapon );
						}

						// Check to see if the target vector is valid
						bool bvecTargetIsZero = vecTarget.IsZero();
						if( !bvecTargetIsZero )
						{
							// calculate the END of the firing vector using the specific position
							pEquippedWeapon->CalcFireVecFromPos(pPed, matWeapon, vecStart, vecEnd, vecTarget);

							// fire the weapon
							CWeapon::sFireParams params( pPed, matWeapon, &vecStart, &vecTarget );
							params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
							if(bSetPerfectAccuracy)
								params.iFireFlags.SetFlag( CWeapon::FF_SetPerfectAccuracy );
							pEquippedWeapon->Fire( params );
						}
						// Otherwise just shoot down the forward vector of the weapon
						else
						{
							CWeapon::sFireParams params( pPed, matWeapon, NULL, NULL );
							params.iFireFlags.SetFlag( CWeapon::FF_ForceBulletTrace );
							if(bSetPerfectAccuracy)
								params.iFireFlags.SetFlag( CWeapon::FF_SetPerfectAccuracy );
							pEquippedWeapon->Fire( params );
						}

						if (NetworkInterface::IsGameInProgress() && !pPed->IsNetworkClone())
						{
							CNetworkSpecialFireEquippedWeaponEvent::Trigger(pPed, bvecTargetIsZero, vecStart, vecTarget, bSetPerfectAccuracy);
						}

						// eject the gunshell
						// MN/CS - commenting out as this should now get done through the normal bullet path
						//pEquippedWeapon->DoWeaponGunshell(pPed);
					}
				}
			}
		}
	}

	void CommandSetPedModelIsSuppressed(int PedModelHashKey, bool SurpressModel)
	{
		fwModelId PedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) PedModelHashKey, &PedModelId);		//	ignores return value
		if(SCRIPT_VERIFY (PedModelId.IsValid(), "SET_PED_MODEL_IS_SUPPRESSED - this is not a valid model index"))
		{
			if (SurpressModel)
			{
				CScriptPeds::GetSuppressedPedModels().AddToSuppressedModelArray(PedModelId.GetModelIndex(), CTheScripts::GetCurrentScriptName());
			}
			else
			{
				CScriptPeds::GetSuppressedPedModels().RemoveFromSuppressedModelArray(PedModelId.GetModelIndex());
			}
		}
	}

	void CommandStopAnyPedModelBeingSuppressed()
	{
		CScriptPeds::GetSuppressedPedModels().ClearAllSuppressedModels();
	}
	
	void CommandSetPedModelIsRestricted(int iPedModelHashKey, bool bRestrictModel)
	{
		fwModelId PedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(static_cast<u32>(iPedModelHashKey), &PedModelId);		//	ignores return value
		if (scriptVerifyf(PedModelId.IsValid(), "SET_PED_MODEL_IS_RESTRICTED - this is not a valid model index"))
		{
			if (bRestrictModel)
			{
				CScriptPeds::GetRestrictedPedModels().AddToRestrictedModelArray(PedModelId.GetModelIndex(), CTheScripts::GetCurrentScriptName());
			}
			else
			{
				CScriptPeds::GetRestrictedPedModels().RemoveFromRestrictedModelArray(PedModelId.GetModelIndex());
			}
		}
	}

	void CommandStopAnyPedModelBeingRestricted()
	{
		CScriptPeds::GetRestrictedPedModels().ClearAllRestrictedModels();
	}

	void CommandSetPedCanBeTargetedWhenInjured(int PedIndex, bool bAllowTargetWhenInjured)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetTargetWhenInjuredAllowed(bAllowTargetWhenInjured);
		}
	}

	void CommandSetPedCanRagdollFromPlayerPedImpact(int PedIndex, bool bAllowActivateRagdollFromPlayerPedImpact)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			//temp fix for whilst the command is negative
			bAllowActivateRagdollFromPlayerPedImpact = !bAllowActivateRagdollFromPlayerPedImpact;

			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact, bAllowActivateRagdollFromPlayerPedImpact );
		}
	}

	void CommandSetPedHelmetFlag(int PedIndex, s32 HelmetPropFlag)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if( pPed->GetHelmetComponent() )
			{
				pPed->GetHelmetComponent()->SetPropFlag((ePedCompFlags)HelmetPropFlag);
			}
		}
	}

	void CommandSetPedHelmetPropIndex(int PedIndex, s32 OverwriteHelmetPropId, bool bIncludeBicycles)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if( pPed->GetHelmetComponent() )
			{
				pPed->GetHelmetComponent()->SetHelmetPropIndex(OverwriteHelmetPropId, bIncludeBicycles);
				pPed->GetHelmetComponent()->SetCurrentHelmetSupportsVisors(false);
				pPed->GetHelmetComponent()->SetVisorPropIndices(false, false, -1, -1);
			}
		}
	}

	void CommandSetPedHelmetVisorPropIndices(int PedIndex, bool bVisorUp, s32 HelmetVisorUpPropId, s32 HelmetVisorDownPropId)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if( pPed->GetHelmetComponent() )
			{
				pPed->GetHelmetComponent()->SetVisorPropIndices(true, bVisorUp, HelmetVisorUpPropId, HelmetVisorDownPropId);
			}
		}
	}

	bool CommandGetIsPedHelmetVisorUp(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if( pPed->GetHelmetComponent() && pPed->GetHelmetComponent()->IsHelmetEnabled() && pPed->GetHelmetComponent()->GetCurrentHelmetSupportsVisors())
			{
				return pPed->GetHelmetComponent()->GetIsVisorUp();
			}
		}
		return false;
	}

	void CommandSetPedHelmetTextureIndex(int PedIndex, s32 OverwriteHelmetTexId)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if( pPed->GetHelmetComponent() )
			{
				pPed->GetHelmetComponent()->SetHelmetTexIndex(OverwriteHelmetTexId);
			}
		}
	}

	bool CommandIsPedWearingHelmet(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if ( pPed->GetHelmetComponent() )
			{
				if( pPed->GetHelmetComponent()->IsHelmetEnabled() )
				{
					return true;
				}
			}
		}
		return false;
	}

	void CommandGivePedHelmetWithOptions(int PedIndex, bool DontTakeOffHelmet, s32 HelmetPropFlag, s32 OverwriteHelmetTexId)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pedDisplayf("GIVE_PED_HELMET - ped[%s][%p] HelmetPropFlag:%i OverwriteHelmetTexId:%i, Script %s ", pPed->GetModelName(),pPed, HelmetPropFlag, OverwriteHelmetTexId, CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if( pPed->GetHelmetComponent() )
			{
				pPed->GetHelmetComponent()->SetPropFlag((ePedCompFlags)HelmetPropFlag);
				
				if(OverwriteHelmetTexId != -1)
				{
					if(SCRIPT_VERIFY(  pPed->GetHelmetComponent()->HasHelmetOfType() && pPed->GetHelmetComponent()->IsHelmetTexValid(pPed, OverwriteHelmetTexId, (ePedCompFlags)HelmetPropFlag),"Invalid Helmet tex index") )
					{
						pPed->GetHelmetComponent()->SetHelmetTexIndex(OverwriteHelmetTexId, true);
					}
				}

				if(  pPed->GetHelmetComponent()->EnableHelmet() )
				{
					// If the put on helmet task is currently running - quit it
					CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_PUT_ON_HELMET );
					if( pTask )
					{
						pTask->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL );
					}

					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet, DontTakeOffHelmet );
				}
			}
		}
	}

	void CommandGivePedHelmet(int PedIndex, bool DontTakeOffHelmet, s32 HelmetPropFlag, s32 OverwriteHelmetTexId)
	{
		CommandGivePedHelmetWithOptions(PedIndex, DontTakeOffHelmet, HelmetPropFlag, OverwriteHelmetTexId);
	}

	void CommandRemovePedHelmet(int PedIndex, bool ForceRemove)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pedDisplayf("REMOVE_PED_HELMET - ped[%s][%p], forceremove:%i , Script %s ", pPed->GetModelName(), pPed, ForceRemove, CTheScripts::GetCurrentScriptNameAndProgramCounter());

			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet, false );

			// B*2028988: Pop the helmet off if taking a helmet off in a vehicle as we don't have proper in-vehicle take-off anims (getting a lot of clipping otherwise).
			if (!ForceRemove && pPed->GetIsInVehicle())
			{
				ForceRemove = true;
			}

			if(ForceRemove && pPed->GetHelmetComponent())
			{
				pPed->GetHelmetComponent()->DisableHelmet();
			}
			else
			{
				CTask* pTask = rage_new CTaskTakeOffHelmet();
				CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask, true);
				pPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}

	void CommandPedTakeOffHelmet(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_RemoveHelmet, true);
		}
	}

	bool CommandIsPedTakingOffHelmet(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return pPed->GetPedResetFlag(CPED_RESET_FLAG_IsRemovingHelmet);
		}
		return false;
	}

	void CommandSetPedHelmet(int PedIndex, bool Enable)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseHelmet, Enable );
		}
	}

	void CommandClearPedStoredHatProp(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed && pPed->GetHelmetComponent())
		{
			pPed->GetHelmetComponent()->SetStoredHatIndices(-1, -1);
		}
	}

	int CommandGetPedHelmetStoredHatPropIndex(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed &&  pPed->GetHelmetComponent())
		{
			return pPed->GetHelmetComponent()->GetStoredHatPropIndex();
		}
		return -1;
	}

	int CommandGetPedHelmetStoredHatTexIndex(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed &&  pPed->GetHelmetComponent())
		{
			return pPed->GetHelmetComponent()->GetStoredHatTexIndex();
		}
		return -1;
	}

	bool CommandIsCurrentHeadPropAHelmet(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if(CPedPropsMgr::GetPedPropIdx(pPed, ANCHOR_HEAD) != -1 &&
			  (CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*pPed,PV_FLAG_DEFAULT_HELMET) || 
			   CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*pPed,PV_FLAG_RANDOM_HELMET) || 
			   CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*pPed,PV_FLAG_PILOT_HELMET) || 
			   CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*pPed,PV_FLAG_SCRIPT_HELMET)))
			{
				return true;
			}
			
		}
		return false;
	}

	void CommandAddVehicleHelmetAssociation(int iVehicleIndex, int iHelmetPropIndex)
	{
		if (scriptVerifyf(iVehicleIndex != 0, "ADD_VEHICLE_HELMET_ASSOCIATION: Invalid vehicle index: %d", iVehicleIndex))
		{
			if (scriptVerifyf(iHelmetPropIndex != -1, "ADD_VEHICLE_HELMET_ASSOCIATION: Invalid helmet prop index: %d", iHelmetPropIndex))
			{
				CPedHelmetComponent::AddVehicleHelmetAssociation((u32)iVehicleIndex, iHelmetPropIndex);
			}
		}
	}

	void CommandClearVehicleHelmetAssociation(int iVehicleIndex)
	{
		if (scriptVerifyf(iVehicleIndex != 0, "CLEAR_VEHICLE_HELMET_ASSOCIATION: Invalid vehicle index: %d", iVehicleIndex))
		{
			CPedHelmetComponent::ClearVehicleHelmetAssociation((u32)iVehicleIndex);
		}
	}

	void CommandClearAllVehicleHelmetAssociations()
	{
		CPedHelmetComponent::ClearAllVehicleHelmetAssociations();
	}

	void CommandSetChargeGoalPosOverride(int PedIndex, const scrVector & scrPos)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
			if( pPedIntelligence )
			{
				Vec3V vPos = VECTOR3_TO_VEC3V((Vector3)scrPos);
				pPedIntelligence->SetChargeGoalPositionOverride(vPos);
			}
		}
	}

	void CommandSetPedToLoadCover(int PedIndex, bool Force)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedLoadCover, Force );
		}
	}


	void CommandSetPedCanCowerInCover(int PedIndex, bool bAllowCower)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			//temp fix to make command positive
			bAllowCower	=	!bAllowCower;
			
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockCoweringInCover, bAllowCower );
		}
	}

	void CommandSetPedCanPeekInCover(int PedIndex, bool bAllowPeek)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			//temp fix for making command positive
			bAllowPeek	=	!bAllowPeek;
			
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockPeekingInCover, bAllowPeek );
		}
	}

	void SetAllowDummyCoversions(bool allow)
	{
		CPedPopulation::SetAllowDummyConversions(allow);
	}

		void CommandSetPedPlaysHeadOnHornAnimWhenDiesInVehicle(int UNUSED_PARAM(PedIndex), bool UNUSED_PARAM(AlwaysUseHeadOnHornAnim))
	{
		Displayf("CommandSetPedPlaysHeadOnHornAnimWhenDiesInVehicle is deprecated, the only animation available is the one that makes the ped lie his head on top of the steering wheel. This call will have no effect");
		/*CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			// Head on horn is the only animation now, there's no point on doing this anymore
			//pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AlwaysUseHeadOnHornAnimWhenDeadInCar, AlwaysUseHeadOnHornAnim );
		}*/
	}

	void CommandSetPedLegIkMode(int PedIndex, int nPedLegIkMode)
	{
		if(SCRIPT_VERIFY((nPedLegIkMode < LEG_IK_MODE_NUM && nPedLegIkMode >= LEG_IK_MODE_OFF), "SET_PED_ENABLE_LEG_IK - not a valid IK value"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if (pPed)
			{
				ikDebugf3("%u SET_PED_LEG_IK_MODE(pPed %p %s, nPedLegIkMode %i)",
					fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), nPedLegIkMode);

				pPed->m_PedConfigFlags.SetPedLegIkMode( nPedLegIkMode );

#if ENABLE_NETWORK_LOGGING
				if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
				{
					const char *IkMode = "Unknown";

					switch(nPedLegIkMode)
					{
					case LEG_IK_MODE_OFF:
						IkMode = "OFF";
						break;
					case LEG_IK_MODE_PARTIAL:
						IkMode = "PARTIAL";
						break;
					case LEG_IK_MODE_FULL:
						IkMode = "FULL";
						break;
					case LEG_IK_MODE_FULL_MELEE:
						IkMode = "FULL_MELEE";
						break;
					default:
						break;
					}

					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "SCRIPT_CHANGING_IK", "");
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Ped", pPed->GetNetworkObject()->GetLogName());
					NetworkInterface::GetObjectManagerLog().WriteDataValue("IK Mode", IkMode);
				}
#endif // ENABLE_NETWORK_LOGGING
			}
			else
			{
				ikDebugf3("%u SET_PED_LEG_IK_MODE(PedIndex %i, nPedLegIkMode %i) Could not find ped!\n%s",
					fwTimer::GetFrameCount(), PedIndex, nPedLegIkMode,
					CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		else
		{
			ikDebugf3("%u SET_PED_LEG_IK_MODE(PedIndex %i, nPedLegIkMode %i) Invalid leg IK mode!\n%s",
				fwTimer::GetFrameCount(), PedIndex, nPedLegIkMode,
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandGivePedFakeNetworkName(int PedIndex, const char *name, int red, int green, int blue, int alpha)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY(name, "GIVE_PED_FAKE_NETWORK_NAME - Invalid name specified"))
			{		
				if (SCRIPT_VERIFY(strlen(name) < 32, "GIVE_PED_FAKE_NETWORK_NAME - Name specified is too long"))
				{		
					if (SCRIPT_VERIFY((red >= 0 && red <= 255), "GIVE_PED_FAKE_NETWORK_NAME - Red colour component invalid!")&&
						SCRIPT_VERIFY((green >= 0 && green <= 255), "GIVE_PED_FAKE_NETWORK_NAME - Red colour component invalid!")&&
						SCRIPT_VERIFY((blue >= 0 && blue <= 255), "GIVE_PED_FAKE_NETWORK_NAME - Red colour component invalid!")&&
						SCRIPT_VERIFY((alpha >= 0 && alpha <= 255), "GIVE_PED_FAKE_NETWORK_NAME - Red colour component invalid!"))
					{	
                        //AddFakeName() no longer in NetworkInterface
						//NetworkInterface::AddFakeName(pPed, name, Color32(red, green, blue, alpha));
					}
				}
			}
		}
	}

	void CommandRemoveFakeNetworkNameFromPed(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
            //RemoveFakeName() no longer in NetworkInterface
			//NetworkInterface::RemoveFakeName(pPed);
		}
	}

	void CommandSetPedMotionBlur(int index, bool bmotionblur)
	{
		CPed *pPed;
		pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(index);
		if (pPed)
		{
			if (bmotionblur==true)
				pPed->m_nFlags.bAddtoMotionBlurMask = false; //disable mask - enable motionblur
			else
				pPed->m_nFlags.bAddtoMotionBlurMask = true; //enable mask - disable motionblur
		}
	}

	void CommandSetPedCanSwitchWeapon(int PedIndex, bool bAllowSwitch)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			bAllowSwitch = !bAllowSwitch;
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockWeaponSwitching, bAllowSwitch );
		}
	}

	void CommandSetPedClimbAnimRate(int PedIndex, float fRate)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->m_PedConfigFlags.SetClimbRateOverride( fRate );
		}
	}

	void CommandSetLadderClimbInputState(int PedIndex, int iInputState)
	{
		if (SCRIPT_VERIFY(iInputState >= CTaskGoToAndClimbLadder::InputState_Nothing && iInputState < CTaskGoToAndClimbLadder::InputState_Max, "SET_LADDER_CLIMB_INPUT_STATE - Invalid Climb Mode"))
		{
			CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if (pPed)
			{
				CTaskClimbLadder* pLadderTask = pPed->GetPedIntelligence()->GetTaskClimbLadder();
				if (SCRIPT_VERIFY(pLadderTask, "SET_LADDER_CLIMB_INPUT_STATE - Ped is not climbing a ladder"))
				{
					pLadderTask->SetInputStateFromAI((CTaskGoToAndClimbLadder::InputState)iInputState);
				}
			}
		}
	}

	bool CommandIsPedInSphereAreaOfAnyEnemyPeds( int PedIndex, const scrVector & scrPos, float fRange )
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		Vec3V vPos = VECTOR3_TO_VEC3V((Vector3)scrPos);
		
		if (pPed)
		{
			CEntityIterator entityIterator( IteratePeds, pPed, &vPos, fRange );
			CPed* pOtherPed = (CPed*)entityIterator.GetNext();

			while( pOtherPed )
			{
				if (pOtherPed->GetPedIntelligence()->IsThreatenedBy(*pPed))
				{
					return true;
				}
				pOtherPed = (CPed*)entityIterator.GetNext();
			}
		}
		return false;
	}

	void CommandStopPedWeaponFiringWhenDropped(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact, true );
			weaponDebugf1("STOP_PED_WEAPON_FIRING_WHEN_DROPPED [0x%p][%d]", pPed, PedIndex);
#if !__FINAL
			scrThread::PrePrintStackTrace();
#endif
		}
	}

	void CommandStopPedDoingFallOffTestsWhenShot(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DissableAutoFallOffTests, true );
		}
	}

	void CommandSetScriptedAnimSeatOffset(int PedIndex, float fDistance)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->m_PedResetFlags.SetScriptedScaleBetweenSeatsDefaultDistance( fDistance );
		}
	}

	void CommandSetPedSkiing(int UNUSED_PARAM(PedIndex), bool UNUSED_PARAM(bSkiing))
	{
		scriptAssertf(0, "%s:SET_PED_SKIING - Function is deprecated - use Weapon functions to achieve same result", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	bool CommandIsPedSkiing(int PedIndex)
	{
		bool bIsPedSkiing = false;

		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			bIsPedSkiing = pPed->GetIsSkiing();
		}
		return bIsPedSkiing;
	}

	void CommandSetPedCombatMovement (int PedIndex, int CombatMovementType)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		CCombatData::Movement CombatMovement = CCombatData::Movement (CombatMovementType); 

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CombatMovement);
		}
	
	}

	int CommandGetPedCombatMovement (int PedIndex )
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		CCombatData::Movement CombatMovement;
		int MovementType = 0;

		if (pPed)
		{
			CombatMovement = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement();
			MovementType  = CCombatData::Movement (CombatMovement);
		}
		
		return MovementType;
	}

	void CommandSetPedCombatAbility (int PedIndex, int CombatAbility)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		CCombatData::Ability ComAbility = CCombatData::Ability (CombatAbility);

		if (pPed)
		{
#if __ASSERT
			if (CombatAbility == CCombatData::BF_WillScanForDeadPeds && pPed->GetPedsGroup())
			{
				scriptAssertf(0, "CA_WILL_SCAN_FOR_DEAD_PEDS is not compatible with ped being in a group, please unset this before adding ped to group, ped 0x%p", pPed);
			}
#endif // __ASSERT

			CCombatBehaviour& pedCombatBehavior = pPed->GetPedIntelligence()->GetCombatBehaviour();
			aiAssertf(CombatAbility != CCombatData::BF_AlwaysFight || pedCombatBehavior.IsFlagSet(CCombatData::BF_AlwaysFlee) == false, "Setting AlwaysFight when AlwaysFlee is already set!");
			aiAssertf(CombatAbility != CCombatData::BF_AlwaysFlee || pedCombatBehavior.IsFlagSet(CCombatData::BF_AlwaysFight) == false, "Setting AlwaysFlee when AlwaysFight is already set!");

			pedCombatBehavior.SetCombatAbility(ComAbility);
		}
	}

	int CommandGetPedCombatAbility (int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		CCombatData::Ability ComAbility;
		int CombatAbility = 0;

		if (pPed)
		{
			ComAbility = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatAbility();
			CombatAbility = CCombatData::Ability (ComAbility);
		}

		return CombatAbility;
	}

	void CommandSetPedCombatRange (int PedIndex, int CombatRangeType)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		CCombatData::Range ComRange = CCombatData::Range(CombatRangeType);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetAttackRange(ComRange);
		}
	}

	int CommandGetPedCombatRange (int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		CCombatData::Range ComRange;
		int CombatRangeType = 0;

		if (pPed)
		{
			ComRange = pPed->GetPedIntelligence()->GetCombatBehaviour().GetAttackRange();
			CombatRangeType = CCombatData::Range (ComRange);
		}
	
		return CombatRangeType;
	}

	void CommandSetPedCombatAttributes (int PedIndex, int CombatFlag, bool bActive)
	{
		scriptAssertf(CombatFlag >= 0 && CombatFlag < CCombatData::MAX_COMBAT_FLAGS, "Invalid combat attribute passed in have attributes been ORd together?");

		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (bActive)
			{		
				pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag((CCombatData::BehaviourFlags)CombatFlag);
			}
			else
			{
				pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag((CCombatData::BehaviourFlags)CombatFlag);
			}
		}
	}

	void CommandSetPedCombatInfo(int PedIndex, int CombatInfoHash)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().InitFromCombatData(CombatInfoHash);
		}
	}

	void CommandSetPedTargetLossResponse(int PedIndex, int TargetLossResponseFlag)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse((CCombatData::TargetLossResponse)TargetLossResponseFlag);
		}
	}

	void CommandGetPedFleeAttributes (int PedIndex, int FleeFlag, bool bActive)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (bActive)
			{
				pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().SetFlag(FleeFlag);
			}
			else
			{
				pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().ClearFlag(FleeFlag);
			}	
		}
	}

	void CommandSetPedCowerHash(int PedIndex, const char* szCowerHash)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			s32 type = CScenarioManager::GetScenarioTypeFromName(szCowerHash);

			if (scriptVerifyf(type!=Scenario_Invalid, "SET_PED_COWER_HASH: Unknown cower scenario type (%s)", szCowerHash ))
			{
				pPed->GetPedIntelligence()->GetFleeBehaviour().SetStationaryReactHash(szCowerHash);	
			}
		}
	}

	bool CommandIsPedPeformingMeleeAction(int nPedIndex)
	{
		bool bIsPedIsPerformingMeleeAction = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(nPedIndex);
		if (pPed)
		{
			if(pPed->GetPedIntelligence())
			{
				CTaskMeleeActionResult* pTaskMeleeActionResult = pPed->GetPedIntelligence()->GetTaskMeleeActionResult();
				if( pTaskMeleeActionResult )
					bIsPedIsPerformingMeleeAction = true;
			}
		}

		return bIsPedIsPerformingMeleeAction;
	}

	bool CommandIsPedPerformingStealthKill(int PedIndex)
	{
		bool bIsPedPerformingStealthKill = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( pPed && pPed->GetPedIntelligence() )
		{
			CTask* pBaseTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT );
			if( pBaseTask )
			{
				CTaskMeleeActionResult* pMeleeActionTask = static_cast<CTaskMeleeActionResult*>(pBaseTask);
				const CActionResult* pActionResult = pMeleeActionTask ? pMeleeActionTask->GetActionResult() : NULL;
				if( pActionResult )
					bIsPedPerformingStealthKill = pActionResult->GetIsAStealthKill() || pActionResult->GetIsATakedown();
			}
		}

		return bIsPedPerformingStealthKill;
	}

	bool CommandIsPedPerformingABlock(int PedIndex)
	{
		bool bIsPedPerformingABlock = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( pPed && pPed->GetPedIntelligence() )
		{
			CTask* pBaseTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT );
			if( pBaseTask )
			{
				CTaskMeleeActionResult* pMeleeActionTask = static_cast<CTaskMeleeActionResult*>(pBaseTask);
				const CActionResult* pActionResult = pMeleeActionTask ? pMeleeActionTask->GetActionResult() : NULL;
				if( pActionResult )
					bIsPedPerformingABlock = pActionResult->GetIsABlock();
			}
		}

		return bIsPedPerformingABlock;
	}

	bool CommandIsPedPerformingACounterAttack(int PedIndex)
	{
		bool bIsPedPerformingACounterAttack = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( pPed && pPed->GetPedIntelligence() )
		{
			CTask* pBaseTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT );
			if( pBaseTask )
			{
				CTaskMeleeActionResult* pMeleeActionTask = static_cast<CTaskMeleeActionResult*>(pBaseTask);
				const CActionResult* pActionResult = pMeleeActionTask ? pMeleeActionTask->GetActionResult() : NULL;
				if( pActionResult )
					bIsPedPerformingACounterAttack = pActionResult->GetIsACounterAttack();
			}
		}

		return bIsPedPerformingACounterAttack;
	}

	bool CommandIsPedBeingStealthKilled(int PedIndex)
	{
		bool bIsPedBeingStealthKilled = false;
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( pPed )
		{
			// We can slightly cheat here by querying the stealth kill config flag and testing against ped death
			bIsPedBeingStealthKilled = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) && !pPed->IsDead();
		}

		return bIsPedBeingStealthKilled;
	}

	int CommandGetMeleeTargetForPed(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if( pPed && pPed->GetPedIntelligence() )
		{
			CTask* pBaseTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT );
			if( pBaseTask )
			{
				CTaskMeleeActionResult* pMeleeActionTask = static_cast<CTaskMeleeActionResult*>(pBaseTask);
				if(pMeleeActionTask && pMeleeActionTask->GetTargetEntity() && pMeleeActionTask->GetTargetEntity()->GetIsTypePed())
				{
					CPed* pTargetPed = static_cast<CPed*>(pMeleeActionTask->GetTargetEntity());
					return CTheScripts::GetGUIDFromEntity(*pTargetPed);
				}
			}
		}

		return 0;
	}

	void CommandGetPedStealthAttributes(int PedIndex, int StealthFlag, bool bActive)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			if (bActive)
			{
				pPed->GetPedIntelligence()->GetPedStealth().GetFlags().SetFlag(StealthFlag);
			}
			else
			{
				pPed->GetPedIntelligence()->GetPedStealth().GetFlags().ClearFlag(StealthFlag);
			}	
		}
	}

	bool CommandWasPedKilledByStealth(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ))
				return true;
		}
		return false;
	}

	bool CommandWasPedKilledByTakedown(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ))
				return true;
		}
		return false;
	}

	bool CommandWasPedKnockedout(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ))
				return true;
		}
		return false;
	}

	void CommandSetPedSteersAroundDeadBodies(int PedIndex, bool bState)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteerAroundDeadBodies, bState );
		}
	}
	bool CommandGetPedSteersAroundDeadBodies(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteerAroundDeadBodies );
		}
		return false;
	}
	void CommandSetPedSteersAroundPeds(int PedIndex, bool bState)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, bState );
		}
	}
	bool CommandGetPedSteersAroundPeds(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds );
		}
		return false;
	}
	void CommandSetPedSteersAroundObjects(int PedIndex, bool bState)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects, bState );
		}
	}
	bool CommandGetPedSteersAroundObjects(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects );
		}
		return false;
	}
	void CommandSetPedSteersAroundVehicles(int PedIndex, bool bState)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles, bState );
		}
	}
	bool CommandGetPedSteersAroundVehicles(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles );
		}
		return false;
	}

	void CommandSetPedIsAvoidedByOthers(int PedIndex, bool bState)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignored_by_All, !bState );
		}
	}
	bool CommandGetPedIsAvoidedByOthers(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignored_by_All );
		}
		return false;
	}
	void CommandPedSetIncreasedAvoidanceRadius(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_IncreasedAvoidanceRadius, true );
		}
	}
	void CommandPedSetBlocksPathingWhenDead(int PedIndex, bool b)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlocksPathingWhenDead, b );
		}
	}

	void CommandPedSetNoTimeDelayBeforeShot(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_NoTimeDelayBeforeShot, true );
		}
	}

	void CommandSetPedIgnoredMaterialCollisionWeapon(s32 WeaponType)
	{
		CPed::SetIgnoredMaterialCollisionWeaponHash((u32)WeaponType);
	}

	void CommandSetPedNeverFallOffSkis(int PedIndex, bool bFallOffSkis)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverFallOffSkis, bFallOffSkis );
		}
	}

	bool CommandIsAnyPedNearPoint(const scrVector & scrVecPoint, float radius)
	{
		Vector3 vPoint =  Vector3 (scrVecPoint);
		float radiusSqr = radius*radius;

		for(s32 i=0; i<CPed::GetPool()->GetSize(); i++)
		{
			CPed *pPed = CPed::GetPool()->GetSlot(i);

			if (pPed)
			{
				Vector3 v = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vPoint;

				if (v.Mag2() <= radiusSqr)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool CommandIsAnyHostilePedNearPoint(int PedIndex, const scrVector & scrVecPoint, float radius)
	{
		CPed* pPedToAgainst = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPedToAgainst)
		{
			Vec3V vPoint =  VECTOR3_TO_VEC3V(Vector3(scrVecPoint));

			CEntityIterator entityIterator( IteratePeds, pPedToAgainst, &vPoint, radius);
			CPed* pPed = (CPed*)(entityIterator.GetNext());

			while(pPed)
			{
				if( !pPed->GetIsDeadOrDying() && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->IsThreatenedBy(*pPedToAgainst))
				{
					return true;
				}

				pPed = (CPed*)(entityIterator.GetNext());
			}
		}
		return false;
	}

	void CommandSetPedCanPlayInCarIdles(int PedIndex, bool bOnOff)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetCanPlayInCarIdles(bOnOff);
		}
	}

	void CommandForcePedAIAndAnimationUpdate(int PedIndex, bool ForceAIPreCameraUpdate, bool ForceZeroTimeStep)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (ForceAIPreCameraUpdate)
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePreCameraAIUpdate, true );
			}
			else
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
			}
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
			if (ForceZeroTimeStep)
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );
			}

			// make sure we update the 
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAiAnimUpdateIfFirstPerson, true);
#if __DEV
			scriptDisplayf("Force post camera ai/anim update called from script: command:FORCE_PED_AI_ANIMATION_UPDATE, entity: %s(%p), frame:%d", pPed->GetModelName(), pPed, fwTimer::GetFrameCount());
			SCRIPT_ASSERT(pPed->m_PedResetFlags.GetForceUpdateCalledLastFrame() != 2, "FORCE_PED_AI_AND_ANIMATION_UPDATE (or FORCE_PED_MOTION_STATE): Called twice in one frame! This command is very expensive and must be called sparingly!");
			SCRIPT_ASSERT(pPed->m_PedResetFlags.GetForceUpdateCalledLastFrame() != 1, "FORCE_PED_AI_AND_ANIMATION_UPDATE (or FORCE_PED_MOTION_STATE): Called on subsequent frames! This command is very expensive and must be called sparingly!");
			pPed->m_PedResetFlags.SetForceUpdateCalledLastFrame( 2 );
#endif //__DEV
		}
	}
	
	//Get the age of a ped.
	int CommandGetPedAge(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetAge(); 
		}
		else
		{
			return -1; 
		}
	}
	
	//Make a request for a high detail model
	void CommandRequestPedHighDetailModel(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			Displayf("Requesting high detail ped (this doesn't do anything - see a coder)"); 
		}
	}

	//Release high detail model. 
	void CommandRemovePedHighDetailModel(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{

		}
	}


	void CommandRequestPedVisibilityTracking(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if( pPed )
		{
			pPed->SetUseOcclusionQuery(true);
		}
	}

	void CommandRequestPedsVehicleVisibilityTracking(int PedIndex, bool b)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if( pPed )
		{
			pPed->SetUseVehicleBoundingBox(b);
		}
	}

	void CommandRequestPedsUseRestrictedVehicleVisibilityTracking(int PedIndex, bool b)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if( pPed )
		{
			pPed->SetUseRestrictedVehicleBoundingBox(b);
		}
	}

	void CommandRequestPedsHnSBoundingBox(int PedIndex, bool b)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if( pPed )
		{
			pPed->SetUseHnSBoundingBox(b);
		}
	}

	bool CommandIsTrackedPedVisible(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			scriptAssertf(pPed->GetUseOcclusionQuery(), "%s:IS_TRACKED_PED_VISIBLE - Requesting visibility for an un-tracked ped.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return pPed->IsPedVisible();
		}
		
		return false;
	}

	int CommandGetTrackedPedPixelCount(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			scriptAssertf(pPed->GetUseOcclusionQuery(), "%s:GET_TRACKED_PED_PIXELCOUNT - Requesting pixel count for an un-tracked ped.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return pPed->GetPedPixelCount();
		}
		
		return -1;
	}

	bool CommandIsPedTracked(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (pPed)
		{
			return pPed->GetUseOcclusionQuery();
		}

		return false;
	}

	bool HasPedReceivedEvent(int PedIndex, int EventType)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			const s32 iNumEvents = pPed->GetPedIntelligence()->CountEventGroupEvents();
			for(s32 i = 0; i < iNumEvents; i++ )
			{
				CEvent* pEvent = pPed->GetPedIntelligence()->GetEventByIndex(i);
				if( pEvent &&
					( ( EventType == EVENT_INVALID ) || ( pEvent->GetEventType() == EventType ) ) )
				{
					return true;
				}
			}
		}
		return false;
	}

	bool CommandCanPedSeeHatedPed(int PedIndex, int OtherPedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			scriptAssertf(pPed->GetBlockingOfNonTemporaryEvents(), "CAN_PED_SEE_HATED_PED should be used with blocking on non temporary events on for the first ped (%s)", pPed->GetDebugName());  
			pPed->SetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed, true);
			const CPed* pOtherPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(OtherPedIndex);
			if (SCRIPT_VERIFY (pOtherPed, "CAN_PED_SEE_HATED_PED - Other ped doesn't exist"))
			{
				const s32 iNumEvents = pPed->GetPedIntelligence()->CountEventGroupEvents();
				for(s32 i = 0; i < iNumEvents; i++ )
				{
					CEvent* pEvent = pPed->GetPedIntelligence()->GetEventByIndex(i);
					if( pEvent && pEvent->GetEventType() == EVENT_ACQUAINTANCE_PED_HATE )
					{
						scriptAssertf(dynamic_cast<CEventAcquaintancePedHate*>(pEvent), "Event of wrong type!");
						CEventAcquaintancePedHate* pEventHate = static_cast<CEventAcquaintancePedHate*>(pEvent);
						if( pEventHate->GetAcquaintancePed() == pOtherPed )
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	bool CommandCanPedShuffleToOrFromTurretSeat(int PedIndex, int& TargetShuffleSeat)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (SCRIPT_VERIFY(pPed, "CAN_PED_SHUFFLE_TO_OR_FROM_TURRET_SEAT - Ped doesn't exist"))
		{
			if (pPed->GetIsInVehicle())
			{
				if (CTaskVehicleFSM::CanPedShuffleToOrFromTurretSeat(pPed->GetMyVehicle(), pPed, TargetShuffleSeat))
				{
					// Script seat indices are one of of sync with code, see core\common\native\generic.sch
					TargetShuffleSeat -= 1;
					return true;
				}
			}
		}
		return false;
	}

	bool CommandCanPedShuffleToOrFromExtraSeat(int PedIndex, int& TargetShuffleSeat)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (SCRIPT_VERIFY(pPed, "CAN_PED_SHUFFLE_TO_OR_FROM_EXTRA_SEAT - Ped doesn't exist"))
		{
			if (pPed->GetIsInVehicle())
			{
				if (CTaskVehicleFSM::CanPedShuffleToOrFromExtraSeat(pPed->GetMyVehicle(), pPed, TargetShuffleSeat))
				{
					// Script seat indices are one of of sync with code, see core\common\native\generic.sch
					TargetShuffleSeat -= 1;
					return true;
				}
			}
		}
		return false;
	}

	int CommandGetPedBoneIndex(int PedIndex, int boneTag)
	{
		int boneIndex = -1;

		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			boneIndex = (int)pPed->GetBoneIndexFromBoneTag((eAnimBoneTag)boneTag);
		}

		return boneIndex;
	}

	int CommandGetPedRagdollBoneIndex(int UNUSED_PARAM(PedIndex), int boneTag)
	{
		// just converts a type of PED_RAGDOLL_COMPONENTS to an int
		return boneTag;
	}


	void CommandSetPedStubble(int PedIndex, float growth)
	{
		CPed* phairyPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (phairyPed)
		{
			phairyPed->SetStubbleGrowth(growth);
		}
	}

	//
	//
	// handy helper function to extract ped's CSE:
	//
	static CCustomShaderEffectPed* GetShaderEffectExt(fwDrawData& drawData)
	{
		CPedDrawHandler *pPedDrawHandler = (CPedDrawHandler*)&drawData;
		
		fwCustomShaderEffect* pShader = pPedDrawHandler->GetShaderEffect();
		if(!pShader)
		{	// if still NULL, then it may be some streamable ped (e.g. player):
			const CPedStreamRenderGfx *pPedStrRenderGfx = pPedDrawHandler->GetConstPedRenderGfx();
			if(pPedStrRenderGfx)
			{
				pShader = pPedStrRenderGfx->m_pShaderEffect;
			}
		}
		return (CCustomShaderEffectPed*)pShader;
	}

	void CommandSetPedEnvEffScale(int PedIndex, float scale)	// <0.0f; 1.0f>
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY( (scale >= 0.0f) && (scale <= 1.0f), "SET_PED_ENVEFF_SCALE - scale should be in the range 0.0f to 1.0f"))
			{
				CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
				scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_PED_ENVEFF_SCALE");
				if(pShader)
				{
					pShader->SetEnvEffScale(scale);
				}
			}
		}
	}

	float CommandGetPedEnvEffScale(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_ENVEFF_SCALE");
			if(pShader)
			{
				return pShader->GetEnvEffScale();
			}
		}
		return(0.0f);
	}


	void CommandSetEnablePedEnvEffScale(int PedIndex, bool enable)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_ENABLE_PED_ENVEFF_SCALE");
			if(pShader)
			{
				pShader->SetEnableEnvEffScale(enable);
			}
		}
	}

	bool CommandGetEnablePedEnvEffScale(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_ENABLE_PED_ENVEFF_SCALE");
			if(pShader)
			{
				return pShader->GetEnableEnvEffScale();
			}
		}
		return(false);
	}

	void CommandSetPedEnvEffCpvAdd(int PedIndex, float cpvAdd)	// <0.0f; 1.0f>
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY( (cpvAdd >= 0.0f) && (cpvAdd <= 1.0f), "SET_PED_ENVEFF_CPV_ADD - add value should be in the range 0.0f to 1.0f"))
			{
				CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
				scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_PED_ENVEFF_CPV_ADD");
				if(pShader)
				{
					pShader->SetEnvEffCpvAdd(cpvAdd);
				}
			}
		}
	}

	float CommandGetPedEnvEffCpvAdd(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_ENVEFF_CPV_ADD");
			if(pShader)
			{
				return pShader->GetEnvEffCpvAdd();
			}
		}
		return(0.0f);
	}

	void CommandSetPedEnvEffColorModulator(int PedIndex, int r, int g, int b) // <0; 255>
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY( (r >= 0) && (r <= 255), "SET_PED_ENVEFF_COLOR_MODULATOR - red should be in the range 0 to 255"))
			{
				if (SCRIPT_VERIFY( (g >= 0) && (g <= 255), "SET_PED_ENVEFF_COLOR_MODULATOR - green should be in the range 0 to 255"))
				{
					if (SCRIPT_VERIFY( (b >= 0) && (b <= 255), "SET_PED_ENVEFF_COLOR_MODULATOR - blue should be in the range 0 to 255"))
					{
						CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
						scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_PED_ENVEFF_COLOR_MODULATOR");
						if( pShader )
						{
							pShader->SetEnvEffColorModulator(u8(r),u8(g),u8(b));
						}
					}
				}
			}
		}
	}

	void CommandSetPedEnvEffColorModulatorf(int PedIndex, float rf, float gf, float bf)	// <0.0f; 1.0f>
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY( (rf >= 0.0f) && (rf <= 1.0f), "SET_PED_ENVEFF_COLOR_MODULATORF - red should be in the range 0.0f to 1.0f"))
			{
				if (SCRIPT_VERIFY( (gf >= 0.0f) && (gf <= 1.0f), "SET_PED_ENVEFF_COLOR_MODULATORF - green should be in the range 0.0f to 1.0f"))
				{
					if (SCRIPT_VERIFY( (bf >= 0.0f) && (bf <= 1.0f), "SET_PED_ENVEFF_COLOR_MODULATORF - blue should be in the range 0.0f to 1.0f"))
					{
						CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
						scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_PED_ENVEFF_COLOR_MODULATORF");
						if( pShader )
						{
							pShader->SetEnvEffColorModulatorf(rf,gf,bf);
						}
					}
				}
			}
		}
	}

	int CommandGetPedEnvEffColorModulatorR(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_ENVEFF_COLOR_MODULATOR_R");
			if( pShader )
			{
				return pShader->GetEnvEffColorModulator().GetRed();
			}
		}
		return 0;
	}

	int CommandGetPedEnvEffColorModulatorG(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_ENVEFF_COLOR_MODULATOR_G");
			if( pShader )
			{
				return pShader->GetEnvEffColorModulator().GetGreen();
			}
		}
		return 0;
	}

	int CommandGetPedEnvEffColorModulatorB(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_ENVEFF_COLOR_MODULATOR_B");
			if( pShader )
			{
				return pShader->GetEnvEffColorModulator().GetBlue();
			}
		}
		return 0;
	}

	void CommandSetPedEmissiveScale(int PedIndex, float scale)	// <0.0f; 1.0f>
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (SCRIPT_VERIFY( (scale >= 0.0f) && (scale <= 1.0f), "SET_PED_EMISSIVE_SCALE - scale should be in the range 0.0f to 1.0f"))
			{
				CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
				scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling SET_PED_EMISSIVE_SCALE");
				if(pShader)
				{
					pShader->SetEmissiveScale(scale);
				}
			}
		}
	}

	float CommandGetPedEmissiveScale(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
			scriptAssertf(pShader,"Please wait for the ped to be fully streamed in before calling GET_PED_EMISSIVE_SCALE");
			if(pShader)
			{
				return pShader->GetEmissiveScale();
			}
		}
		return(0.0f);
	}

	bool CommandIsPedShaderReady(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (pPed->GetDrawHandlerPtr())
			{
				CCustomShaderEffectPed* pShader = GetShaderEffectExt(pPed->GetDrawHandler());
				if(pShader)
				{
					return true;
				}
			}
		}
		return false;
	}

	void CommandSetPedEnableCrewEmblem(int PedIndex, bool enable)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->SetEnableCrewEmblem(enable);		
		}
	}

	int CommandGetPedEnableCrewEmblem(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetEnableCrewEmblem();			
		}

		return 2;	// invalid
	}

	int CommandCreateSynchronizedScene(const scrVector & position, const scrVector & orientation, int RotOrder)
	{
		Vector3 pos(position);
		Vector3 eulers(orientation);
		eulers*=DtoR;
		Quaternion rot;
		CScriptEulers::QuaternionFromEulers(rot, eulers, static_cast<EulerAngleOrder>(RotOrder));

		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		fwSyncedSceneId scene = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene(scriptThreadId);

		bool bValidScene = fwAnimDirectorComponentSyncedScene::IsValidSceneId(scene);
		
#if __ASSERT
		if (!bValidScene)
		{
			atString syncedSceneInfo; fwAnimDirectorComponentSyncedScene::DumpSynchronizedSceneDebugInfo(syncedSceneInfo);
			atArray< atString > splitString; syncedSceneInfo.Split(splitString, "\n", true);
			for(int i = 0; i < splitString.GetCount(); i ++)
			{
				scriptDisplayf("%s", splitString[i].c_str());
			}
		}
#endif // __ASSERT

		if (SCRIPT_VERIFY (bValidScene, "CREATE_SYNCHRONIZED_SCENE - Failed to create synced scene! See the command comments for possible causes of this."))
		{	
			CScriptResource_SyncedScene syncedScene(scene);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(syncedScene);

			fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(scene, pos, rot);
					
			return (int)scene;
		}


		
		return -1;
	}

	int CommandCreateSynchronizedSceneRelativeToMapObject(const scrVector & scrVecNewCoors, float Radius, int ObjectModelHashKey)
	{
		Vector3 VecNewCoors = Vector3 (scrVecNewCoors);
		CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey);

		if (pClosestObj && pClosestObj->GetIsDynamic())
		{
			if (SCRIPT_VERIFY(!NetworkUtils::IsNetworkClone(pClosestObj), "CREATE_SYNCHRONIZED_SCENE_AT_MAP_OBJECT - Cannot call this command on an entity owned by another machine!"))
			{
				Vector3 vRot; 
				Matrix34 m = MAT34V_TO_MATRIX34(pClosestObj->GetMatrix());
				m.ToEulersYXZ(vRot);
				vRot *= RtoD;

				Vector3 vPos = VEC3V_TO_VECTOR3(pClosestObj->GetTransform().GetPosition());
				
				return CommandCreateSynchronizedScene(vPos, vRot, EULER_YXZ);
			}
		}
		return -1; 
	}

	bool CommandIsSynchronizedSceneRunning(int sceneIndex)
	{
		return fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex));
	}

	void CommandSetSynchronizedSceneOrigin(int sceneIndex, const scrVector & position, const scrVector & orientation, int RotOrder)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "SET_SYNCHRONIZED_SCENE_ORIGIN - Invalid scene index"))
		{		
			Vector3 pos(position);
			Vector3 eulers(orientation);
			eulers*=DtoR;
			Quaternion rot;
			CScriptEulers::QuaternionFromEulers(rot, eulers, static_cast<EulerAngleOrder>(RotOrder));

			fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin((fwSyncedSceneId)sceneIndex, pos, rot);
		}
	}

	void CommandSetSynchronizedSceneRate(int localSceneIndex, float newRate)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(localSceneIndex)), "SET_SYNCHRONIZED_SCENE_RATE - Invalid scene index"))
		{		
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneRate((fwSyncedSceneId)localSceneIndex, newRate);

			// if we're a network game....
			if(NetworkInterface::IsGameInProgress())
			{
				int networkId = CNetworkSynchronisedScenes::GetNetworkSceneID(localSceneIndex);

				if(-1 != networkId)
				{
					// find and update the corresponding scene description so it gets serialised across the network...
					CNetworkSynchronisedScenes::SetSceneRate(networkId, newRate);	
#if ENABLE_NETWORK_LOGGING
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // #if ENABLE_NETWORK_LOGGING
				}
			}
		}
	}

	float CommandGetSynchronizedSceneRate(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "GET_SYNCHRONIZED_SCENE_RATE - Invalid scene index"))
		{		
			return fwAnimDirectorComponentSyncedScene::GetSyncedSceneRate((fwSyncedSceneId)sceneIndex);
		}
		return 1.0f;
	}

	void CommandSetSynchronizedScenePhase(int sceneIndex, float newPhase)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "SET_SYNCHRONIZED_SCENE_PHASE - Invalid scene index"))
		{		
			fwAnimDirectorComponentSyncedScene::SetSyncedScenePhase((fwSyncedSceneId)sceneIndex, newPhase);
		}
	}

	float CommandGetSynchronizedScenePhase(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "GET_SYNCHRONIZED_SCENE_PHASE - Invalid scene index"))
		{		
			return fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase((fwSyncedSceneId)sceneIndex);
		}
		return 0.0f;
	}

	void CommandSetSynchronizedSceneAbsolute(int sceneIndex, bool looping)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "SET_SYNCHRONIZED_SCENE_LOOPED - Invalid scene index"))
		{		
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneAbsolute((fwSyncedSceneId)sceneIndex, looping);
		}
	}

	bool CommandIsSynchronizedSceneAbsolute(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "IS_SYNCHRONIZED_SCENE_LOOPED - Invalid scene index"))
		{		
			return fwAnimDirectorComponentSyncedScene::IsSyncedSceneAbsolute((fwSyncedSceneId)sceneIndex);
		}
		return false;
	}

	void CommandSetSynchronizedSceneLooped(int sceneIndex, bool looping)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "SET_SYNCHRONIZED_SCENE_LOOPED - Invalid scene index"))
		{		
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped((fwSyncedSceneId)sceneIndex, looping);
		}
	}

	bool CommandIsSynchronizedSceneLooped(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "IS_SYNCHRONIZED_SCENE_LOOPED - Invalid scene index"))
		{		
			return fwAnimDirectorComponentSyncedScene::IsSyncedSceneLooped((fwSyncedSceneId)sceneIndex);
		}
		return false;
	}

	void CommandSetSynchronizedSceneHoldLastFrame(int sceneIndex, bool holdLastFrame)
	{
        if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress() || (CNetworkSynchronisedScenes::GetNetworkSceneID(sceneIndex) == -1), "SET_SYNCHRONIZED_SCENE_HOLD_LAST_FRAME - Not supported in network synced scenes! - use the option in NETWORK_CREATE_SYNCHRONISED_SCENE() instead"))
        {
		    // Check for valid scene
		    if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "SET_SYNCHRONIZED_SCENE_HOLD_LAST_FRAME - Invalid scene index"))
		    {		
			    fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame((fwSyncedSceneId)sceneIndex, holdLastFrame);
		    }
        }
	}

	bool CommandIsSynchronizedSceneHoldLastFrame(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "IS_SYNCHRONIZED_SCENE_HOLD_LAST_FRAME - Invalid scene index"))
		{		
			return fwAnimDirectorComponentSyncedScene::IsSyncedSceneHoldLastFrame((fwSyncedSceneId)sceneIndex);
		}
		return false;
	}

	void CommandAttachSynchronizedSceneToEntity(int sceneIndex, int entityIndex, int entityBoneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY - Invalid scene index"))
		{		
			CPhysical* pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(entityIndex);
			
			scriptAssertf(pEntity, "%s ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY. Couldn't find valid entity at entityIndex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), entityIndex);
			if (pEntity)
			{
#if __DEV
				if (entityBoneIndex>-1)
				{
					scriptAssertf(pEntity->GetSkeleton(), "%s ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY. Entity %s does not have a skeleton", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pEntity->GetModelName());
					if (pEntity->GetSkeleton())
					{
						scriptAssertf(entityBoneIndex < pEntity->GetSkeleton()->GetBoneCount(), "%s ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY. Invalid entityBoneIndex %d entity %s max index is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), entityBoneIndex, pEntity->GetModelName(), pEntity->GetSkeleton()->GetBoneCount());
					}
				}
#endif //__DEV

				fwAnimDirectorComponentSyncedScene::AttachSyncedScene(static_cast<fwSyncedSceneId>(sceneIndex), pEntity, entityBoneIndex);
			}
		}
	}

	void CommandDetachSynchronizedScene(int sceneIndex)
	{
		// Check for valid scene
		if (SCRIPT_VERIFY (fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "ATTACH_SYNCHRONIZED_SCENE - Invalid scene index"))
		{		
			fwAnimDirectorComponentSyncedScene::DetachSyncedScene(static_cast<fwSyncedSceneId>(sceneIndex));
		}
	}

	void CommandTakeOwnershipSynchronizedScene(int sceneIndex)
	{
		if (SCRIPT_VERIFY(fwAnimDirectorComponentSyncedScene::IsValidSceneId(static_cast<fwSyncedSceneId>(sceneIndex)), "TAKE_OWNERSHIP_OF_SYNCHRONIZED_SCENE - Invalid scene index"))
		{
			scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
			fwAnimDirectorComponentSyncedScene::TakeOwnershipOfSyncedScene(static_cast<fwSyncedSceneId>(sceneIndex), scriptThreadId);
		}
	}

	enum eForceAnimAiUpdateState
	{
		ForceAnimAiUpdateState_Default = 0, 
		ForceAnimAiUpdateState_CutSceneExit
	};

	bool CommandForcePedMotionState(s32 pedIndex, s32 motionState, bool shouldReset, s32 updateState, bool ForceAIPreCameraUpdate)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

		bool forcedState = false;

		if (pPed)
		{
#if FPS_MODE_SUPPORTED
			if(pPed->IsLocalPlayer() && 
				(motionState == CPedMotionStates::MotionState_ActionMode_Idle || motionState == CPedMotionStates::MotionState_ActionMode_Walk || motionState == CPedMotionStates::MotionState_ActionMode_Run) && 
				pPed->WantsToUseActionMode() && 
				!pPed->IsUsingActionMode())
			{
				pPed->UpdateMovementMode(false);
			}

			//! First person does not support action mode, so we shouldn't be able to force motion state to it either. No need to assert, just bail out.
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				switch((CPedMotionStates::eMotionState)motionState)
				{
					case CPedMotionStates::MotionState_ActionMode_Idle:
					case CPedMotionStates::MotionState_ActionMode_Walk:
					case CPedMotionStates::MotionState_ActionMode_Run:
					{
						if(pPed && !pPed->IsUsingActionMode())
						{
							pedWarningf("Can't force action mode state in 1st person!");
							return false;
						}
					}
					default:
						break;
				}
			}
#endif // FPS_MODE_SUPPORTED

			if (scriptVerifyf(CPed::IsAllowedToForceMotionState((CPedMotionStates::eMotionState)motionState, pPed), "%s: FORCE_PED_MOTION_STATE - Failed to force state %d - See TTY for details", CTheScripts::GetCurrentScriptNameAndProgramCounter(), motionState))
			{
				forcedState = pPed->ForceMotionStateThisFrame((CPedMotionStates::eMotionState)motionState, shouldReset);

				switch(updateState)
				{
				case ForceAnimAiUpdateState_CutSceneExit:
					{
						SCRIPT_ASSERT(forcedState, "FORCE_PED_MOTION_STATE - Unrecognised motion state");
						if (ForceAIPreCameraUpdate)
						{
							pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePreCameraAIUpdate, true );
						}
						else
						{
							pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
						}
						pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAiAnimUpdateIfFirstPerson, true);
						pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
#if __DEV
						scriptDisplayf("Force post camera ai/anim update called from script: command:FORCE_PED_MOTION_STATE, entity: %s(%p), frame:%d", pPed->GetModelName(), pPed, fwTimer::GetFrameCount());
						SCRIPT_ASSERT(pPed->m_PedResetFlags.GetForceUpdateCalledLastFrame() != 2, "FORCE_PED_MOTION_STATE (or FORCE_PED_AI_AND_ANIMATION_UPDATE): Called twice in one frame! This command is very expensive and must be called sparingly!");
						SCRIPT_ASSERT(pPed->m_PedResetFlags.GetForceUpdateCalledLastFrame() != 1, "FORCE_PED_MOTION_STATE (or FORCE_PED_AI_AND_ANIMATION_UPDATE): Called on subsequent frames! This command is very expensive and must be called sparingly!");
						pPed->m_PedResetFlags.SetForceUpdateCalledLastFrame( 2 );
#endif //__DEV
					}
					break; 

				default:
				case ForceAnimAiUpdateState_Default:
					{

					}
					break; 
				}
			}
		}
		return forcedState;
	}

	bool CommandGetPedCurrentMoveBlendRatio(int PedIndex, float& fMbrX, float& fMbrY)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(!pPed)
		{
			return false;
		}
		fMbrX = pPed->GetMotionData()->GetCurrentMbrX();
		fMbrY = pPed->GetMotionData()->GetCurrentMbrY();
		return true;
	}

	void CommandSetPedMaxMoveBlendRatio(int PedIndex, float fMaxMoveBlendRatio)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "SET_PED_MAX_MOVE_BLEND_RATIO - Check ped is alive this frame");
			pPed->GetMotionData()->SetScriptedMaxMoveBlendRatio(fMaxMoveBlendRatio);
		}
	}

	void CommandSetPedMinMoveBlendRatio(int PedIndex, float fMinMoveBlendRatio)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if(pPed)
		{
			SCRIPT_ASSERT(pPed->m_nDEflags.bCheckedForDead == TRUE, "SET_PED_MIN_MOVE_BLEND_RATIO - Check ped is alive this frame");
			if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopping))
			{
				pPed->GetMotionData()->SetScriptedMinMoveBlendRatio(fMinMoveBlendRatio);
			}
		}
	}

	void CommandSetDesiredPedMoveRateOverride( int PedIndex, float fDesiredRate )
	{
		const float fMaxOverride = 10.0f;
		if( SCRIPT_VERIFY( (fDesiredRate >= 0.0f) && (fDesiredRate <= fMaxOverride), "SET_PED_MOVE_RATE_OVERRIDE - Rate out of bounds... 0.0f <= rate <= 10.0f") )
		{
			fDesiredRate = Clamp(fDesiredRate, 0.0f, fMaxOverride );
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->GetMotionData()->SetDesiredMoveRateOverride(fDesiredRate);
			}
		}
	}

	void CommandSetDesiredPedMoveRateInWaterOverride( int PedIndex, float fDesiredRate )
	{
		const float fMaxOverride = 3.0f;
		if( SCRIPT_VERIFY( (fDesiredRate >= 0.0f) && (fDesiredRate <= fMaxOverride), "SET_PED_MOVE_RATE_IN_WATER_OVERRIDE - Rate out of bounds... 0.0f <= rate <= 3.0f") )
		{
			fDesiredRate = Clamp(fDesiredRate, 0.0f, fMaxOverride );
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				pPed->GetMotionData()->SetDesiredMoveRateInWaterOverride(fDesiredRate);
			}
		}
	}

	//looks at a ped's skeleton to give us a position sort of in the middle.
	//this is adapted from the code that gives medics a position to stand while
	//healing a downed ped--exposed here to script so designers can implement
	//similar animations in their missions -- see b* 189811
	scrVector CommandGetPositionToSideOfPed(int PedIndex, float fOffsetDist)
	{
		scrVector retVec;
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			
			Matrix34 mWorldBoneMatrix;
			pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);
			Vector3 vOffsetWorldSpace = mWorldBoneMatrix.c;
			vOffsetWorldSpace.z = 0.0f;
			vOffsetWorldSpace.NormalizeSafe(ORIGIN);
			vOffsetWorldSpace.Scale(fOffsetDist);

			//Vector3 vOffsetLocal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vOffsetWorldSpace)));
			//Vector3 vBaseLocal = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform(VECTOR3_TO_VEC3V(mWorldBoneMatrix.d)));
			//retVec = vBaseLocal + vOffsetLocal;
			retVec = mWorldBoneMatrix.d + vOffsetWorldSpace;

			//invert the offset vector here, since we want to face the opposite direction
			//fDirectionOut = rage::Atan2f(vOffsetWorldSpace.x, -vOffsetWorldSpace.y);
		}

		return retVec;
	}

	bool CommandPedHasSexinessFlagSet(int PedIndex, int flag)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return pPed->CheckSexinessFlags(flag);
		}

		return false;
	}
	
	int CommandGetNearbyVehicles(int PedIndex, scrArrayBaseAddr& vehicleArray)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		int count = 0;
		if(pPed)
		{
			scrValue* pAddress = scrArray::GetElements<scrValue>(vehicleArray);
			int size = scrArray::GetCount(vehicleArray);
			
			CEntityScannerIterator vehicleIterator = pPed->GetPedIntelligence()->GetNearbyVehicles();
			fwEntity* pEntity = vehicleIterator.GetFirst();
			while(pEntity && size > 0)
			{
				pAddress->Int = fwScriptGuid::CreateGuid(*pEntity);
				pAddress++;
				count++;
				size--;
				pEntity = vehicleIterator.GetNext();
			}
		}
		return count;
	}

	int CommandGetNearbyPeds(int PedIndex, scrArrayBaseAddr& pedArray, int iExclusionPedType)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		int count = 0;
		if(pPed)
		{
			scrValue* pAddress = scrArray::GetElements<scrValue>(pedArray);
			int size = scrArray::GetCount(pedArray);


			CEntityScannerIterator vehicleIterator = pPed->GetPedIntelligence()->GetNearbyPeds();
			fwEntity* pEntity = vehicleIterator.GetFirst();
			while(pEntity && size > 0)
			{
				if(((CPed*)pEntity)->GetPedType() != iExclusionPedType)
				{
					pAddress->Int = fwScriptGuid::CreateGuid(*pEntity);
					pAddress++;
					count++;
					size--;
				}
				pEntity = vehicleIterator.GetNext();
			}
		}
		return count;
	}

	bool CommandHaveAllStreamingRequestsCompleted(int PedIndex)
	{
		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return const_cast<CPed*>(pPed)->HaveAllStreamingReqsCompleted();
		}
		return false;
	}

	bool CommandIsPedUsingActionMode(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pPed)
		{
			return pPed->IsUsingActionMode();
		}

		return false;
	}

	bool CommandIsPedWantingToUseUseActionMode(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pPed)
		{
			return pPed->WantsToUseActionMode();
		}

		return false;
	}	

	void CommandSetPedUsingActionMode(int PedIndex, bool UsingActionMode, int ActionModeTime, const char* actionModeOverrideName)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pPed)
		{
#if !__NO_OUTPUT && !__FINAL
			scriptDebugf1("%s SET_PED_USING_ACTION_MODE: Ped: 0x%p [%d][%s], UsingActionMode: %d, ActionModeTime: %d, actionModeOverrideName: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed, PedIndex, pPed->GetModelName(), UsingActionMode, ActionModeTime, actionModeOverrideName);
			scrThread::PrePrintStackTrace();
#endif

			// Clear stealth
			pPed->GetMotionData()->SetUsingStealth(false);

			float fActionModeTime = -1.f;
			if(ActionModeTime != -1)
			{
				fActionModeTime = (float)ActionModeTime/1000.f;

				// action mode time is currently not synced, add this later if necessary
				scriptAssertf(!pPed->GetNetworkObject() || pPed->IsAPlayerPed(), "SET_PED_USING_ACTION_MODE: Action mode time (%d) is currently not supported in MP", ActionModeTime);
			}

			pPed->SetUsingActionMode(UsingActionMode, CPed::AME_Script, fActionModeTime);

			//! If script want to set this to false, reset combat state too (as we'll remain in action mode if we have recent battle events).
			if(!UsingActionMode)
			{
				pPed->SetUsingActionMode(UsingActionMode, CPed::AME_Combat, fActionModeTime);
				pPed->GetPedIntelligence()->ResetBattleAwareness();
			}

			u32 nOverrideHash = (UsingActionMode && actionModeOverrideName) ? atStringHash(actionModeOverrideName) : 0;
			pPed->SetMovementModeOverrideHash(nOverrideHash);

			// Update the movement mode variables
			pPed->UpdateMovementMode();
		}
	}

	void CommandSetMovementModeOverride(int PedIndex, const char* actionModeOverrideName)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
		if (pPed)
		{
#if !__NO_OUTPUT && !__FINAL
			scriptDebugf1("%s SET_MOVEMENT_MODE_OVERRIDE: Ped: 0x%p [%d][%s], actionModeOverrideName: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed, PedIndex, pPed->GetModelName(), actionModeOverrideName);
			scrThread::PrePrintStackTrace();
#endif
			u32 nOverrideHash = actionModeOverrideName ? atStringHash(actionModeOverrideName) : 0;
			pPed->SetMovementModeOverrideHash(nOverrideHash);
		}
	}

	void CommandSetPedCapsule(int PedIndex, float Radius)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			pPed->SetScriptCapsuleRadius(Radius);
		}
	}	

	PedHeadshotHandle CommandRegisterPedHeadshot(int PedIndex)
	{
		CPed const* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			return PEDHEADSHOTMANAGER.RegisterPed(const_cast<CPed*>(pPed));
		}

		return 0;
	}

	PedHeadshotHandle CommandRegisterPedHeadshotHiRes(int PedIndex)
	{
		CPed const* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			return PEDHEADSHOTMANAGER.RegisterPed(const_cast<CPed*>(pPed), /*bNeedTransparentBackground*/false, /*bForceHiRes*/true);
		}

		return 0;
	}

	PedHeadshotHandle CommandRegisterPedHeadshotWithTransparentBackground(int PedIndex)
	{
		CPed const* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pPed)
		{
			return PEDHEADSHOTMANAGER.RegisterPed(const_cast<CPed*>(pPed), true);
		}

		return 0;
	}

	PedHeadshotHandle CommandGetPedHeadshotWithTransparentBackground()
	{
		return PEDHEADSHOTMANAGER.GetRegisteredWithTransparentBackground();
	}

	void CommandUnregisterPedHeadshot(int handle)
	{
		PEDHEADSHOTMANAGER.UnregisterPed((PedHeadshotHandle)handle);
	}

	bool CommandIsPedHeadshotValid(int handle)
	{
		return PEDHEADSHOTMANAGER.IsValid((PedHeadshotHandle)handle);
	}

	bool CommandIsPedHeadshotReady(int handle)
	{
		return PEDHEADSHOTMANAGER.IsActive((PedHeadshotHandle)handle);
	}

	s32 CommandGetPedHeadshotTxdHash(int handle)
	{
		return (s32)PEDHEADSHOTMANAGER.GetTextureHash((PedHeadshotHandle)handle);
	}

	const char* CommandGetPedHeadshotTxdString(int handle)
	{
		return PEDHEADSHOTMANAGER.GetTextureName((PedHeadshotHandle)handle);
	}

	void CommandSetPedHeadshotCustomLighting(bool bEnable)
	{
		PEDHEADSHOTMANAGER.SetUseCustomLighting(bEnable);
	}

	bool CommandCanRequestPoseForHeadshot()
	{
		return PEDHEADSHOTMANAGER.CanRequestPoseForPed();
	}

	PedHeadshotHandle CommandRegisterPedHeadshotWithAnimPose(int PedIndex, const char* animClipDicName, const char* animClipName, const scrVector & _camPos, const scrVector & _camRot)
	{
		CPed const* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			Vector3 camPos(_camPos);
			Vector3 camRot(_camRot);
			return PEDHEADSHOTMANAGER.RegisterPedWithAnimPose(const_cast<CPed*>(pPed), atHashString(animClipDicName), atHashString(animClipName), camPos, camRot);
		}
		return 0;
	}

	void CommandSetPedHeadshotCustomLight(int index, const scrVector & _pos, const scrVector & _col, float intensity, float radius)
	{
		Vector3 pos(_pos);
		Vector3 col(_col);
		PEDHEADSHOTMANAGER.SetCustomLightParams(index, pos, col, intensity, radius);
	}

	int CommandGetPedHeadshotState(int handle)
	{
		return (int)PEDHEADSHOTMANAGER.GetPedHeashotState((PedHeadshotHandle)handle);
	}

	bool CommandRequestPedHeadshotTextureUpload(int handle)
	{
		return PEDHEADSHOTMANAGER.RequestTextureUpload((PedHeadshotHandle)handle);
	}

	void CommandReleasePedHeadshotTextureUploadRequest(int handle)
	{
		PEDHEADSHOTMANAGER.ReleaseTextureUploadRequest((PedHeadshotHandle)handle);
	}

	bool CommandIsPedHeadshotTextureUploadAvailable()
	{
		return PEDHEADSHOTMANAGER.IsTextureUploadAvailable();
	}

	bool CommandHasPedHeadshotTextureUploadFailed()
	{
		return PEDHEADSHOTMANAGER.HasTextureUploadFailed();
	}

	bool CommandHasPedHeadshotTextureUploadSucceeded()
	{
		return PEDHEADSHOTMANAGER.HasTextureUploadSucceeded();
	}

	void CommandSetPedHeatScaleOverride(int PedIndex, int inHeatScale)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			// a value of zero is used to disable override
			u8 heatScale = (u8)Clamp(inHeatScale, 1, 255);
			pPed->SetHeatScaleOverride(heatScale);
		}
	}

	void CommandDisablePedHeatScaleOverride(int PedIndex)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (pPed)
		{
			pPed->SetHeatScaleOverride(0);
		}
	}

	void CommandSpawnPointsStartSearch(const scrVector & vInputSearchOrigin, float fSearchRadius, float fMaxDistZ, s32 iSearchFlags, float fMinimumSpacing, s32 iMaxSearchDurationMs)
	{
		Vector3 vSearchOrigin(vInputSearchOrigin);
		CPathServer::GetOpponentPedGen().StartSearch(vSearchOrigin, fSearchRadius, fMaxDistZ, iSearchFlags, fMinimumSpacing, iMaxSearchDurationMs);
	}

	void CommandSpawnPointsStartSearchInAngledArea(const scrVector & vInputPoint1, const scrVector & vInputPoint2, float fWidth, s32 iSearchFlags, float fMinimumSpacing, s32 iMaxSearchDurationMs)
	{
		CPathServer::GetOpponentPedGen().StartSearch(vInputPoint1, vInputPoint2, fWidth, iSearchFlags, fMinimumSpacing, iMaxSearchDurationMs);
	}

	void CommandSpawnPointsCancelSearch()
	{
		CPathServer::GetOpponentPedGen().CancelSeach();
	}

	bool CommandSpawnPointsIsSearchActive()
	{
		return CPathServer::GetOpponentPedGen().IsSearchActive();
	}

	bool CommandSpawnPointsIsSearchComplete()
	{
		return CPathServer::GetOpponentPedGen().IsSearchComplete();
	}

	bool CommandSpawnPointsIsSearchFailed()
	{
		return CPathServer::GetOpponentPedGen().IsSearchFailed();
	}

	s32 CommandSpawnPointsGetNumResults()
	{
		return CPathServer::GetOpponentPedGen().GetNumResults();
	}

	void CommandSpawnPointGetResult(int i, float & x, float & y, float & z)
	{
		Vector3 vPos;
		CPathServer::GetOpponentPedGen().GetResult(i, vPos);
		x = vPos.x;
		y = vPos.y;
		z = vPos.z;
	}

	void CommandSpawnPointGetResultFlags(int i, int & iOutFlags)
	{
		u32 iFlags;
		CPathServer::GetOpponentPedGen().GetResultFlags(i, iFlags);
		// Cast u32 flags to s32 for script; we cannot use the highest bit
		iOutFlags = static_cast<s32>(iFlags);
	}

	enum eIkTargetFlags
	{
		ITF_DEFAULT 					= 0,
		ITF_ARM_TARGET_WRT_HANDBONE		= 1,	// arm target relative to the handbone
		ITF_ARM_TARGET_WRT_POINTHELPER	= 2,	// arm target relative to the pointhelper
		ITF_ARM_TARGET_WRT_IKHELPER		= 4,	// arm target relative to the ikhelper
		ITF_IK_TAG_MODE_NORMAL			= 8,	// use animation tags directly
		ITF_IK_TAG_MODE_ALLOW			= 16,	// use animation tags in ALLOW mode
		ITF_IK_TAG_MODE_BLOCK			= 32,	// use animation tags in BLOCK mode
		ITF_ARM_USE_ORIENTATION			= 64	// solve for orientation in addition to position
	};

	u32 ConvertIkTargetFlags(int iIKPart, int iFlags)
	{
		u32 uFlags = 0;

		switch ((eIkPart)iIKPart)
		{
			case IK_PART_HEAD:
			{
				uFlags = LOOKIK_SCRIPT_REQUEST;
				uFlags |= (iFlags & ITF_IK_TAG_MODE_NORMAL) ? LOOKIK_USE_ANIM_TAGS : 0;
				break;
			}
			case IK_PART_ARM_LEFT:
			case IK_PART_ARM_RIGHT:
			{
				uFlags |= (iFlags & ITF_ARM_TARGET_WRT_HANDBONE) ? AIK_TARGET_WRT_HANDBONE : 0;
				uFlags |= (iFlags & ITF_ARM_TARGET_WRT_POINTHELPER) ? AIK_TARGET_WRT_POINTHELPER : 0;
				uFlags |= (iFlags & ITF_ARM_TARGET_WRT_IKHELPER) ? AIK_TARGET_WRT_IKHELPER : 0;
				uFlags |= (iFlags & ITF_IK_TAG_MODE_ALLOW) ? AIK_USE_ANIM_ALLOW_TAGS : 0;
				uFlags |= (iFlags & ITF_IK_TAG_MODE_BLOCK) ? AIK_USE_ANIM_BLOCK_TAGS : 0;
				uFlags |= (iFlags & ITF_ARM_USE_ORIENTATION) ? AIK_USE_ORIENTATION : 0;
				break;
			}
			case IK_PART_SPINE:
			{
				scriptAssertf(false, "ConvertIkTargetFlags - Unsupported IK part");
				break;
			}
			case IK_PART_LEG_LEFT:
			case IK_PART_LEG_RIGHT:
			{
				//uFlags |= (iFlags & ITF_IK_TAG_MODE_ALLOW) ? PEDIK_LEGS_USE_ANIM_ALLOW_TAGS : 0;
				//uFlags |= (iFlags & ITF_IK_TAG_MODE_BLOCK) ? PEDIK_LEGS_USE_ANIM_BLOCK_TAGS : 0;
				scriptAssertf(false, "ConvertIkTargetFlags - Unsupported IK part");
				break;
			}
			case IK_PART_INVALID:
			default:
			{
				scriptAssertf(false, "ConvertIkTargetFlags - Invalid IK part");
			}
		};

		return uFlags;
	}

	void CommandSetIkTarget(int iPedID, int iIKPart, int iTargetEntityID, int iTargetBone, const scrVector & scrVecTargetOffset, int iFlags, int iBlendInTimeMS, int iBlendOutTimeMS)
	{
		if (iPedID != NULL_IN_SCRIPTING_LANGUAGE)
		{
			CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedID, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pPed)
			{
				const CEntity* pTargetEntity = (iTargetEntityID != NULL_IN_SCRIPTING_LANGUAGE) ? CTheScripts::GetEntityToQueryFromGUID<CEntity>(iTargetEntityID, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK) : NULL;

				eAnimBoneTag eTargetBoneTag = (iTargetBone >= 0) ? (eAnimBoneTag)iTargetBone : BONETAG_INVALID;

				Vector3 vTargetOffset(scrVecTargetOffset);

				ikDebugf3("%u SET_IK_TARGET(pPed %p %s, iIKPart %i, pTargetEntity %p %s, iTargetBone %i, scrVecTargetOffset %.3f,%.3f,%.3f, iFlags %i, iBlendInTimeMs %i, iBlendOutTimeMS %i)",
					fwTimer::GetFrameCount(), pPed, pPed->GetModelName(), iIKPart, pTargetEntity, pTargetEntity->GetModelName(), iTargetBone, scrVecTargetOffset.x, scrVecTargetOffset.y, scrVecTargetOffset.z, iFlags, iBlendInTimeMS, iBlendOutTimeMS);

				pPed->GetIkManager().SetIkTarget((eIkPart)iIKPart, pTargetEntity, eTargetBoneTag, vTargetOffset, ConvertIkTargetFlags(iIKPart, iFlags), iBlendInTimeMS, iBlendOutTimeMS);
			}
			else
			{
				ikDebugf3("%u SET_IK_TARGET(iPedID %i, iIKPart %i, iTargetEntityID %i, iTargetBone %i, scrVecTargetOffset %.3f,%.3f,%.3f, iFlags %i, iBlendInTimeMs %i, iBlendOutTimeMS %i) Could not find ped!\n%s",
					fwTimer::GetFrameCount(), iPedID, iIKPart, iTargetEntityID, iTargetBone, scrVecTargetOffset.x, scrVecTargetOffset.y, scrVecTargetOffset.z, iFlags, iBlendInTimeMS, iBlendOutTimeMS,
					CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		else
		{
			ikDebugf3("%u SET_IK_TARGET(iPedID %i, iIKPart %i, iTargetEntityID %i, iTargetBone %i, scrVecTargetOffset %.3f,%.3f,%.3f, iFlags %i, iBlendInTimeMs %i, iBlendOutTimeMS %i) Ped index is invalid!\n%s",
				fwTimer::GetFrameCount(), iPedID, iIKPart, iTargetEntityID, iTargetBone, scrVecTargetOffset.x, scrVecTargetOffset.y, scrVecTargetOffset.z, iFlags, iBlendInTimeMS, iBlendOutTimeMS,
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void CommandForceInstantLegIkSetup(int iPedID)
	{
		if (iPedID != NULL_IN_SCRIPTING_LANGUAGE)
		{
			CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedID, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK & CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (pPed)
			{
				ikDebugf3("%u FORCE_INSTANT_LEG_SETUP(pPed %p %s)",
					fwTimer::GetFrameCount(), pPed, pPed->GetModelName());

				pPed->GetIkManager().SetFlag(PEDIK_LEGS_INSTANT_SETUP);
			}
			else
			{
				ikDebugf3("%u FORCE_INSTANT_LEG_SETUP(iPedID %i) Could not find ped!\n%s",
					fwTimer::GetFrameCount(), iPedID,
					CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		else
		{
			ikDebugf3("%u FORCE_INSTANT_LEG_SETUP(iPedID %i) Ped index is invalid!\n%s",
				fwTimer::GetFrameCount(), iPedID,
				CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	void RequestMovementModeAsset(const char* MovementModeName, const CPedModelInfo::PersonalityMovementModes::MovementModes Type)
	{
		atHashString movementModeHash(MovementModeName);
		const CPedModelInfo::PersonalityMovementModes* pData = CPedModelInfo::FindPersonalityMovementModes(movementModeHash.GetHash());
		if(scriptVerifyf(pData, "%s Movement mode [%s] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MovementModeName))
		{
			CScriptResource_MovementMode_Asset asset(movementModeHash.GetHash(), Type);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(asset);
		}
	}

	bool HasMovementModeAssetLoaded(const char* MovementModeName, const CPedModelInfo::PersonalityMovementModes::MovementModes Type)
	{
		atHashString movementModeHash(MovementModeName);
		const CPedModelInfo::PersonalityMovementModes* pData = CPedModelInfo::FindPersonalityMovementModes(movementModeHash.GetHash());
		if(scriptVerifyf(pData, "%s Movement mode [%s] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MovementModeName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				ScriptResourceRef Reference = CMovementModeScriptResourceManager::GetReference(movementModeHash.GetHash(), Type);
				scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET, Reference);
				if(scriptVerifyf(pScriptResource, "%s has not been requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MovementModeName))
				{
					if(scriptVerifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET, "%s incorrect type of resource %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptResource->GetType()))
					{
						CScriptResource_MovementMode_Asset* pAsset = static_cast<CScriptResource_MovementMode_Asset*>(pScriptResource);
						CMovementModeScriptResource* pResource = CMovementModeScriptResourceManager::GetResource(pAsset->GetReference());
						if(scriptVerifyf(pResource, "%s NULL resource", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
						{
							return pResource->GetIsStreamedIn();
						}
					}
				}
			}
		}

		return false;
	}

	void RemoveMovementModeAsset(const char* MovementModeName, const CPedModelInfo::PersonalityMovementModes::MovementModes Type)
	{
		atHashString movementModeHash(MovementModeName);
		const CPedModelInfo::PersonalityMovementModes* pData = CPedModelInfo::FindPersonalityMovementModes(movementModeHash.GetHash());
		if(scriptVerifyf(pData, "%s Movement mode [%s] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MovementModeName))
		{
			ScriptResourceRef Reference = CMovementModeScriptResourceManager::GetReference(movementModeHash.GetHash(), Type);
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET, Reference);
		}
	}

	void CommandRequestActionModeAsset(const char* ActionModeName)
	{
		RequestMovementModeAsset(ActionModeName, CPedModelInfo::PersonalityMovementModes::MM_Action);
	}

	bool CommandHasActionModeAssetLoaded(const char* ActionModeName)
	{
		return HasMovementModeAssetLoaded(ActionModeName, CPedModelInfo::PersonalityMovementModes::MM_Action);
	}

	void CommandRemoveActionModeAsset(const char* ActionModeName)
	{
		RemoveMovementModeAsset(ActionModeName, CPedModelInfo::PersonalityMovementModes::MM_Action);
	}

	void CommandRequestStealthModeAsset(const char* StealthModeName)
	{
		RequestMovementModeAsset(StealthModeName, CPedModelInfo::PersonalityMovementModes::MM_Stealth);
	}

	bool CommandHasStealthModeAssetLoaded(const char* StealthModeName)
	{
		return HasMovementModeAssetLoaded(StealthModeName, CPedModelInfo::PersonalityMovementModes::MM_Stealth);
	}

	void CommandRemoveStealthModeAsset(const char* StealthModeName)
	{
		RemoveMovementModeAsset(StealthModeName, CPedModelInfo::PersonalityMovementModes::MM_Stealth);
	}

	void CommandSetPedLodMultiplier(int PedIndex, float multiplier)
	{
		CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(SCRIPT_VERIFY(ped, "SET_PED_LOD_MULTIPLIER - failed to get ped"))
		{
			SCRIPT_ASSERT(multiplier <= 5.f, "SET_PED_LOD_MULTIPLIER used to set a multiplier larger than 5.0! Is this really needed?");
			ped->SetLodMultiplier(multiplier);
			ped->SetLodDistance((u32)(100.f * multiplier)); // peds use the default fade distance of 100
		}
	}

	void CommandSetPedCanLosePropsOnDamage(int PedIndex, bool value, int flags)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			switch (flags)
			{
				//This match PROP_DAMAGE_FLAGS	ENDENUM
				case 0: //PF_CAN_LOSE_PROPS_ON_DAMAGE
					{
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanLosePropsOnDamage, value );
						break;
					}
				case 1: //PF_ALLOW_HELMET_LOSS_ON_HEADSHOT
					{
						pPed->SetPedConfigFlag(  CPED_CONFIG_FLAG_CanLoseHelmetOnDamage, value );
						break;
					}
			}
		}
	}

	void CommandForceFootstepHelperUpdate(int PedIndex, bool value)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetFootstepHelper().ScriptForceUpdate(value);
		}
	}

	void CommandForceStepType(int PedIndex,bool force, int stepType, int detectionValuesIdx)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

		if (pPed)
		{
			pPed->GetFootstepHelper().ForceStepType(force,(audFootstepEvent)stepType,(audDetectionValues)detectionValuesIdx);
		}
	}

	bool CommandIsTargetPedInPerceptionArea(int PedIndex, int TargetPedIndex, float fFocusAngle, float fFocusDistance, float fPeripheralAngle, float fPeripheralDistance)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		const CPed *pTargetPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(TargetPedIndex);

		if(pPed && pTargetPed)
		{
			Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition());
			CPedPerception& pedPerception = pPed->GetPedIntelligence()->GetPedPerception();

			// Figure out which focus distance and angle to use based on if the passed in params are set
			fFocusDistance = fFocusDistance >= 0.0f ? fFocusDistance : pedPerception.GetSeeingRange();
			fFocusAngle = fFocusAngle > 0.0f ? fFocusAngle * .5f : pedPerception.GetCentreOfGazeMaxAngle();

			// Figure out which peripheral distance and angle to use based on if that passed in params are set
			fPeripheralDistance = fPeripheralDistance >= 0.0f ? fPeripheralDistance : pedPerception.GetSeeingRangePeripheral();
			float fMinPeripheralAngle = fPeripheralAngle >= 0.0f ? -(fPeripheralAngle * .5f) : pedPerception.GetVisualFieldMinAzimuthAngle();
			float fMaxPeripheralAngle = fPeripheralAngle >= 0.0f ? fPeripheralAngle * .5f : pedPerception.GetVisualFieldMaxAzimuthAngle();

			// Make sure the peripheral angle is at least as big as the focus angle
			if(fMaxPeripheralAngle < fFocusAngle)
			{
				fMinPeripheralAngle = (-fFocusAngle);
				fMaxPeripheralAngle = fFocusAngle;
			}

			return pedPerception.ComputeFOVVisibility(vTargetPosition, fPeripheralDistance, fFocusDistance, fMinPeripheralAngle * DtoR, fMaxPeripheralAngle * DtoR, fFocusAngle * DtoR);
		}
		
		return false;
	}


	void CommandSetUsePopControlSphereThisFrame(const scrVector& center, float pedRadius, float vehRadius)
	{
		CPedPopulation::UseTempPopControlSphereThisFrame(center, pedRadius, vehRadius);
	}

	void CommandClearPopControlSphere()
	{
		CPedPopulation::ClearTempPopControlSphere();
	}

    int CommandDoesPedExistWithDecorator(const char *label)
    {
        CPed::Pool *pool = CPed::GetPool();

        if(pool)
        {
            atHashWithStringBank decKey(label);

            s32 i = pool->GetSize();

	        while(i--)
	        {
		        CPed *pPed = pool->GetSlot(i);

                if(pPed)
                {
                    if(fwDecoratorExtension::ExistsOn(*pPed, decKey))
                    {
                        return CTheScripts::GetGUIDFromEntity(const_cast<CPed&>(*pPed));
                    }
                }
            }
        }

        return NULL_IN_SCRIPTING_LANGUAGE;
    }

	void CommandForcePedToHaveZeroMassInCollisions( int PedIndex )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pPed )
		{
			pPed->ForcePedToHaveZeroMassInCollisionsThisFrame();
		}
	}

	void CommandSetDisableHighFallDeath( int PedIndex, bool disableFallDeath )
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if( pPed )
		{
			pPed->SetDisableHighFallInstantDeath( disableFallDeath );
		}
	}

	int CommandGetPedPhonePaletteIdx(int pedIdx)
	{
		int res = -1;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIdx);

		if(pPed)
		{
			res = pPed->GetPedPhonePaletteIdx();
		}

		return res;
	}

	void CommandSetPedPhonePaletteIdx(int pedIdx, int idx)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIdx);
		if(pPed)
		{
			pPed->SetPedPhonePaletteIdx(idx);
		}
	}

	void CommandSetPedSteerBias(int PedIndex, float Bias)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			pPed->GetMotionData()->SetDesiredSteerBias(Bias);
		}
	}

	bool CommandIsPedSwitchingWeapon(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon);
		}

		return false;
	}

	int CommandGetDLCPackHashForComponent(int PedIndex, int componentType, int drawableIndex)
	{
		int retVal = 0;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			u32 varInfoHash;
			u32 localIndex;
			CPedVariationPack::GetLocalCompData(pPed, (ePedVarComp)componentType, drawableIndex, varInfoHash, localIndex);

			retVal = int(varInfoHash);
		}

		return(retVal);
	}

	int CommandGetDLCPackHashForProp(int PedIndex, int AnchorPoint, int PropIndex )
	{
		int retVal = 0;
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo()))
			{
				if (CPedVariationInfoCollection* pVarInfoColl = pPedModelInfo->GetVarInfo())
				{
					if (CPedVariationInfo* varInfo = pVarInfoColl->GetVariationInfoFromPropIdx((eAnchorPoints)AnchorPoint, PropIndex))
					{
						retVal = (int)varInfo->GetDlcNameHash();
					}
				}
			}
		}
		return retVal;
	}

	void CommandSetPedTreatedAsFriendly(int PedIndex, bool enable, bool bLocalOnly)
	{
		CPed *pPed = bLocalOnly ? CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex,CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES) : CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			if (SCRIPT_VERIFY(!pPed->IsPlayer(), "SET_PED_TREATED_AS_FRIENDLY can not be set on a player ped."))
			{
				if (!bLocalOnly)
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage, enable);
				}
				else
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamageNonSynced, enable);
				}
			}
		}
	}


	void CommandSetDisablePedMapCollision( int PedIndex )
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID< CPed >( PedIndex );

		if( pPed )
		{
			const fragInstNMGta* pRagDollInst = pPed->GetRagdollInst();
			const phInst* pInst = pPed->GetAnimatedInst();

			if( pInst )
			{
				if( pInst->IsInLevel() )
				{
					u32 includeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags( pInst->GetLevelIndex() );
					includeFlags = includeFlags & ~ArchetypeFlags::GTA_MAP_TYPE_MOVER;

					CPhysics::GetLevel()->SetInstanceIncludeFlags( pInst->GetLevelIndex(), includeFlags );
				}
			}
			if( pRagDollInst )
			{
				if( pRagDollInst->IsInLevel() )
				{
					u32 includeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags( pRagDollInst->GetLevelIndex() );
					includeFlags = includeFlags & ~ArchetypeFlags::GTA_MAP_TYPE_MOVER;

					CPhysics::GetLevel()->SetInstanceIncludeFlags( pInst->GetLevelIndex(), includeFlags );
				}
			}
		}
	}

	void CommandEnableMpLight(int PedIndex, bool bEnable)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID< CPed >( PedIndex );
		if(pPed)
		{
			pPed->EnableMpLight(bEnable);
		}
	}

	bool CommandGetMpLightEnabled(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if(pPed)
		{
			return pPed->GetMpLightEnabled();
		}

		return false;
	}

	void CommandEnableFootLight(int PedIndex, bool bEnable)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID< CPed >(PedIndex);
		if (pPed)
		{
			pPed->EnableFootLight(bEnable);
		}
	}

	bool CommandGetFootLightEnabled(int PedIndex)
	{
		const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			return pPed->GetFootLightEnabled();
		}

		return false;
	}

	void CommandSetAllowStuntJumpCamera(int PedIndex, bool bEnable)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID< CPed >(PedIndex);
		if (pPed)
		{
			pPed->GetPlayerInfo()->SetAllowStuntJumpCamera(bEnable);
		}
	}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(CREATE_PED,0xa8d58c3aada2c41c, CommandCreatePed);
		SCR_REGISTER_SECURE(DELETE_PED,0x05cb75c0837196f9, CommandDeletePed);
		SCR_REGISTER_SECURE(CLONE_PED,0x67e4cf00756bc7f7, CommandClonePed);
		SCR_REGISTER_SECURE(CLONE_PED_ALT,0x951d499767e322eb, CommandClonePedAlt);
		SCR_REGISTER_SECURE(CLONE_PED_TO_TARGET,0xc70fd0e17f3b16a8, CommandClonePedToTarget);
		SCR_REGISTER_SECURE(CLONE_PED_TO_TARGET_ALT,0xdb5929d6bd66757d, CommandClonePedToTargetAlt);

		//Area and locate commands
		SCR_REGISTER_SECURE(IS_PED_IN_VEHICLE,0xf6f5d18ef8eab859, CommandIsPedInVehicle);
		SCR_REGISTER_SECURE(IS_PED_IN_MODEL,0xf30a227f7990ae00, CommandIsPedInModel);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_VEHICLE,0xd5c6b5e3b93a5edc, CommandIsPedInAnyVehicle);
		SCR_REGISTER_SECURE(IS_COP_PED_IN_AREA_3D,0x97d65c0968cfe3af, CommandIsCopPedInArea3d);

		// Health querying commands
		SCR_REGISTER_SECURE(IS_PED_INJURED,0xf68107c40359970c,			CommandIsPedInjured		);
		SCR_REGISTER_SECURE(IS_PED_HURT,0xc96a660f9cab40fc,			CommandIsPedHurt		);
		SCR_REGISTER_SECURE(IS_PED_FATALLY_INJURED,0xfd478ecfa85a941a,	CommandIsPedFatallyInjured	);
		SCR_REGISTER_SECURE(IS_PED_DEAD_OR_DYING,0xe63474311c6e3825,	CommandIsPedDeadOrDying	);
		SCR_REGISTER_SECURE(IS_CONVERSATION_PED_DEAD,0xb6f838c7e14ccb73,	CommandIsConversationPedDead	);

		SCR_REGISTER_SECURE(IS_PED_AIMING_FROM_COVER,0x5e057f1d5da0cf01,	CommandIsPedAimingFromCover);
		SCR_REGISTER_SECURE(IS_PED_RELOADING,0xc56f471f53aca87b,	CommandIsPedReloading);
		SCR_REGISTER_UNUSED(IS_PED_THROWING_GRENADE_AND_AIMING_GUN,0x54f3888840c6d4a9,	CommandIsPedThrowingGrenadeWhileAimingGun);
		SCR_REGISTER_SECURE(IS_PED_A_PLAYER,0x65fab09725e2fe75,		CommandIsPedAPlayer);
		SCR_REGISTER_SECURE(CREATE_PED_INSIDE_VEHICLE,0x487c17b41938052c, CommandCreatePedInsideVehicle);
		SCR_REGISTER_UNUSED(SET_PED_HEADING_AND_PITCH,0x9ab327b74d84cc1a, CommandSetPedHeadingAndPitch);
		SCR_REGISTER_UNUSED(SET_PED_PITCH,0xb854140f1f7813e8, CommandSetPedPitch);
		SCR_REGISTER_SECURE(SET_PED_DESIRED_HEADING,0x2e9120fd2882e04e, CommandSetPedDesiredHeading);
		SCR_REGISTER_SECURE(FORCE_ALL_HEADING_VALUES_TO_ALIGN,0x6b15094e523d0487, CommandForceAllHeadingValuesToAlign);
		SCR_REGISTER_SECURE(IS_PED_FACING_PED,0x2e7f5417d4f4dd69, CommandIsPedFacingPed);
		SCR_REGISTER_SECURE(IS_PED_IN_MELEE_COMBAT,0x7a6b8c262362c722, CommandIsPedInMeleeCombat);
		SCR_REGISTER_UNUSED(IS_PED_DOING_GUN_DISARM,0x3f86225325ad91b7, CommandIsPedDoingGunDisArm);
		SCR_REGISTER_SECURE(IS_PED_STOPPED,0xeeec05469860b219, CommandIsPedStopped);
		SCR_REGISTER_SECURE(IS_PED_SHOOTING_IN_AREA,0x955815b1a675a225, CommandIsPedShootingInArea);
		SCR_REGISTER_SECURE(IS_ANY_PED_SHOOTING_IN_AREA,0x7f97ee72f87f3234, CommandIsAnyPedShootingInArea);
		SCR_REGISTER_SECURE(IS_PED_SHOOTING,0xe323e6755636a70e, CommandIsPedShooting);
		SCR_REGISTER_SECURE(SET_PED_ACCURACY,0x348a212511c78dbf, CommandSetPedAccuracy);
		SCR_REGISTER_SECURE(GET_PED_ACCURACY,0x2bdd0f473c8ec3f7, CommandGetPedAccuracy);
		SCR_REGISTER_SECURE(SET_AMBIENT_LAW_PED_ACCURACY_MODIFIER,0x22fb1e5c91b2ed92, CommandSetAmbientLawPedAccuracyModifier);
		SCR_REGISTER_SECURE(IS_PED_MODEL,0x59bbf1ca2f5cac75, CommandIsPedModel);
		SCR_REGISTER_SECURE(EXPLODE_PED_HEAD,0x978c27cce53d6781, CommandExplodePedHead);
		SCR_REGISTER_SECURE(REMOVE_PED_ELEGANTLY,0x56f6ab6eb61767c5, CommandRemovePedElegantly);
		SCR_REGISTER_SECURE(ADD_ARMOUR_TO_PED,0x4c12922738b94758, CommandAddArmourToPed);
		SCR_REGISTER_SECURE(SET_PED_ARMOUR,0x495f6ebd50bcb090, CommandSetPedArmour);
		SCR_REGISTER_UNUSED(GET_PED_ENDURANCE,0x2a8210c428f480cb, CommandGetPedEndurance);
		SCR_REGISTER_UNUSED(GET_PED_MAX_ENDURANCE,0x5b78f399a98794de, CommandGetPedMaxEndurance);
		SCR_REGISTER_UNUSED(SET_PED_ENDURANCE,0xbd123dfbbfb5cbe3, CommandSetPedEndurance);
		SCR_REGISTER_UNUSED(SET_PED_MAX_ENDURANCE,0x6fc653d4f3413902, CommandSetPedMaxEndurance);
		SCR_REGISTER_SECURE(SET_PED_INTO_VEHICLE,0xa9f390242f9eb2e1, CommandSetPedIntoVehicle);
		SCR_REGISTER_SECURE(SET_PED_ALLOW_VEHICLES_OVERRIDE,0x5d07a2cf7f265fcb, CommandSetPedAllowVehiclesOverride);
		SCR_REGISTER_SECURE(CAN_CREATE_RANDOM_PED,0xdfc6a9270fbcdbf5, CommandCanCreateRandomPed);
		SCR_REGISTER_SECURE(CREATE_RANDOM_PED,0x25b5b67856bac352, CommandCreateRandomPed);
		SCR_REGISTER_UNUSED(CREATE_RANDOM_MALE_PED,0x5dc4fa6a0ea3eb1a, CommandCreateRandomMalePed);
		SCR_REGISTER_UNUSED(CREATE_RANDOM_FEMALE_PED,0xd0537ac0fbc6eabd, CommandCreateRandomFemalePed);
		SCR_REGISTER_SECURE(CREATE_RANDOM_PED_AS_DRIVER,0x267eed49988c5a4d, CommandCreateRandomPedAsDriver);
		SCR_REGISTER_SECURE(CAN_CREATE_RANDOM_DRIVER,0xa0ec17492f2dbaf9, CommandCanCreateRandomDriver);
		SCR_REGISTER_SECURE(CAN_CREATE_RANDOM_BIKE_RIDER,0x550047660d0241e8, CommandCanCreateRandomBikeRider);
		SCR_REGISTER_UNUSED(SET_PED_MOVE_ANIM_SPEED_MULTIPLIER,0x46b06056043917f8, CommandSetPedMoveAnimSpeedMultiplier);
		SCR_REGISTER_SECURE(SET_PED_MOVE_ANIMS_BLEND_OUT,0xa91903a0cebffd9b, CommandSetPedMoveAnimsBlendOut);
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_DRAGGED_OUT,0x027bda95c9bd0667, CommandSetPedCanBeDraggedOut);
		SCR_REGISTER_SECURE(SET_PED_ALLOW_HURT_COMBAT_FOR_ALL_MISSION_PEDS,0xa4a900de57adff59, CommandSetAllowHurtCombatForAllMissionPeds);
		SCR_REGISTER_SECURE(IS_PED_MALE,0xecf091d7e67ff7f2, CommandIsPedMale);
		SCR_REGISTER_SECURE(IS_PED_HUMAN,0xfd0bdfe16368a58a, CommandIsPedHuman);
		SCR_REGISTER_SECURE(GET_VEHICLE_PED_IS_IN,0xb3ff0049c1fd38ec, CommandGetVehiclePedIsIn);
		SCR_REGISTER_SECURE(RESET_PED_LAST_VEHICLE,0x4731a59f8dc19173, CommandResetPedLastVehicle);
		SCR_REGISTER_UNUSED(RESET_PED_LAST_VEHICLE_ON_EXIT,0x264fe2d874f196fb, CommandResetPedLastVehicleOnExit);
		SCR_REGISTER_UNUSED(SET_PED_DENSITY_MULTIPLIER, 0x87fdb5d1, CommandSetPedDensityMultiplier);
		SCR_REGISTER_UNUSED(SET_SCENARIO_PED_DENSITY_MULTIPLIER, 0x874b05be, CommandSetScenarioPedDensityMultiplier);
		SCR_REGISTER_SECURE(SET_PED_DENSITY_MULTIPLIER_THIS_FRAME,0x39b76f783934d136, CommandSetPedDensityMultiplierThisFrame);
		SCR_REGISTER_SECURE(SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME,0x17f3d11fec5656c2, CommandSetScenarioPedDensityMultiplierThisFrame);
		SCR_REGISTER_SECURE(SUPPRESS_AMBIENT_PED_AGGRESSIVE_CLEANUP_THIS_FRAME,0x7bdcf3a4fa45d147, CommandSuppressAmbientPedAggressiveCleanupThisFrame);
		SCR_REGISTER_SECURE(SET_SCRIPTED_CONVERSION_COORD_THIS_FRAME,0x06c47617fe42768f, CommandSetScriptedConversionCoordThisFrame);
		SCR_REGISTER_UNUSED(SET_SCRIPTED_CONVERSION_COORD,0x49d23f85e53a927b, CommandSetScriptedConversionCoord);
		SCR_REGISTER_UNUSED(CLEAR_SCRIPTED_CONVERSION_COORD,0xbd63495bd8693943, CommandClearScriptedConversionCoord);
		SCR_REGISTER_UNUSED(SET_PED_NON_REMOVAL_AREA,0xb0beec8ef4577561, CommandSetPedNonRemovalArea);
		SCR_REGISTER_UNUSED(CLEAR_PED_NON_REMOVAL_AREA,0xf8041330f7a7c66b, CommandClearPedNonRemovalArea);
		SCR_REGISTER_SECURE(SET_PED_NON_CREATION_AREA,0xa8aede296853bb64, CommandSetPedNonCreationArea);
		SCR_REGISTER_SECURE(CLEAR_PED_NON_CREATION_AREA,0x2aa70f35b0b111e6, CommandClearPedNonCreationArea);
		SCR_REGISTER_SECURE(INSTANTLY_FILL_PED_POPULATION,0x6d5eda3dfaea63a7, CommandInstantlyFillPedPopulation);

		SCR_REGISTER_UNUSED(SET_PED_ONTO_MOUNT,0xeafc2c540173fb2c, CommandSetPedOntoMount);
		SCR_REGISTER_UNUSED(REMOVE_PED_FROM_MOUNT,0xf35937a1d475ddde, CommandRemovePedFromMount);
		SCR_REGISTER_SECURE(IS_PED_ON_MOUNT,0xa476005b8c3eeaeb, CommandIsPedOnMount);
		SCR_REGISTER_SECURE(GET_MOUNT,0xcdb459fbaed2d476, CommandGetMount);
		SCR_REGISTER_UNUSED(SET_MAX_MOUNT_SPEED_WHILE_AIMING,0x1fe31c699532d0f3,CommandSetMaxMountSpeedWhileAiming);
		SCR_REGISTER_UNUSED(SET_MOUNT_AGITATION_SCALE,0x94768def7f2cfb42,CommandSetMountAgitationScale);

		SCR_REGISTER_UNUSED(IS_PED_USING_LOWRIDER_ALTERNATE_CLIPSET,0xb6f85aec0fb59585, CommandIsPedUsingLowriderAlternateClipset);

		SCR_REGISTER_SECURE(IS_PED_ON_VEHICLE,0x1f5ae5900921235a, CommandIsPedOnVehicle);
		SCR_REGISTER_SECURE(IS_PED_ON_SPECIFIC_VEHICLE,0x8465e2ab4423b436, CommandIsPedOnSpecificVehicle);

		SCR_REGISTER_SECURE(SET_PED_MONEY,0x62bdda84cccf90a9, CommandSetPedMoney);
		SCR_REGISTER_SECURE(GET_PED_MONEY,0x402553675e4bbba9, CommandGetPedMoney);
		SCR_REGISTER_UNUSED(SET_MONEY_CARRIED_BY_ALL_NEW_PEDS,0x59533590c072028d, CommandSetMoneyCarriedByAllNewPeds);
		SCR_REGISTER_SECURE(SET_HEALTH_SNACKS_CARRIED_BY_ALL_NEW_PEDS,0x1031302428923cdb, CommandSetHealthSnacksCarriedByAllNewPeds);
		SCR_REGISTER_SECURE(SET_AMBIENT_PEDS_DROP_MONEY,0x97d2605f3900ff11, CommandSetAmbientPedsDropMoney);
		SCR_REGISTER_SECURE(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS_FOR_AMBIENT_PEDS_THIS_FRAME,0x50eb67d38f5d4fd9, CommandSetBlockingOfNonTemporaryEventsForAmbientPedsThisFrame);

		SCR_REGISTER_SECURE(SET_PED_SUFFERS_CRITICAL_HITS,0x40a4fa5c4b5da318, CommandSetPedSuffersCriticalHits);
		SCR_REGISTER_SECURE(SET_PED_UPPER_BODY_DAMAGE_ONLY,0xd5bc529fc7a9f778, CommandSetPedUpperBodyDamageOnly);

		SCR_REGISTER_SECURE(IS_PED_SITTING_IN_VEHICLE,0x87b449f4c26e764a, CommandIsPedSittingInVehicle);
		SCR_REGISTER_SECURE(IS_PED_SITTING_IN_ANY_VEHICLE,0x174b84f6b78d6ca7, CommandIsPedSittingInAnyVehicle);
		SCR_REGISTER_SECURE(IS_PED_ON_FOOT,0x3c7ed37a9fe7c585, CommandIsPedOnFoot);
		SCR_REGISTER_SECURE(IS_PED_ON_ANY_BIKE,0x45052a4871a67255, CommandIsPedOnAnyBike);
		SCR_REGISTER_UNUSED(IS_PED_PLANTING_C4,0x19332eb633bc7494, CommandIsPedPlantingC4);
		SCR_REGISTER_SECURE(IS_PED_PLANTING_BOMB,0x3da4842fa4407be2, CommandIsPedPlantingBomb);
		SCR_REGISTER_SECURE(GET_DEAD_PED_PICKUP_COORDS,0x9b2f53f9cb82f448, CommandGetDeadPedPickupCoords);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_BOAT,0xec6430115d7dd916, CommandIsPedInAnyBoat);
        SCR_REGISTER_SECURE(IS_PED_IN_ANY_SUB,0xaff5fd1a82555477, CommandIsPedInAnySub);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_HELI,0x0f92affde5783870, CommandIsPedInAnyHeli);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_PLANE,0xb73d201873640749, CommandIsPedInAnyPlane);
		SCR_REGISTER_SECURE(IS_PED_IN_FLYING_VEHICLE,0x17fe501894ed2713, CommandIsPedInFlyingVehicle);
		SCR_REGISTER_SECURE(SET_PED_DIES_IN_WATER,0x96c405257a1bd81b, CommandSetPedDiesInWater);
		SCR_REGISTER_SECURE(GET_PED_DIES_IN_WATER,0x65671a4fb8218930, CommandGetPedDiesInWater);
		SCR_REGISTER_SECURE(SET_PED_DIES_IN_SINKING_VEHICLE,0x8888993dfeae0d08, CommandSetPedDiesInSinkingVehicle);
		SCR_REGISTER_SECURE(GET_PED_ARMOUR,0x28e4040be8c027ef, CommandGetPedArmour);
		SCR_REGISTER_SECURE(SET_PED_STAY_IN_VEHICLE_WHEN_JACKED,0x4d671450ebb62809, CommandSetPedStayInVehicleWhenJacked);
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_SHOT_IN_VEHICLE,0xf3d629af368d6be8, CommandSetPedCanBeShotInVehicle);
		SCR_REGISTER_SECURE(GET_PED_LAST_DAMAGE_BONE,0x7709b2482f29a1bb, GetPedLastDamageBone);
		SCR_REGISTER_SECURE(CLEAR_PED_LAST_DAMAGE_BONE,0xe657cd67ef2ecdd3, ClearPedLastDamageBone); 
		SCR_REGISTER_UNUSED(SET_SCRIPT_CONTROLLED_RAGDOLL_FLAG_FOR_DAMAGED_PED,0xf0f79acb2596a3be, CommandSetScriptControlledRagdollFlag);
		SCR_REGISTER_SECURE(SET_AI_WEAPON_DAMAGE_MODIFIER,0xbe16a4f63911dc82, SetAiWeaponDamageModifier); 
		SCR_REGISTER_SECURE(RESET_AI_WEAPON_DAMAGE_MODIFIER,0xbb76e4e041cf4402, ResetAiWeaponDamageModifier); 
		SCR_REGISTER_SECURE(SET_AI_MELEE_WEAPON_DAMAGE_MODIFIER,0x74d120c7128bba54, SetAiMeleeWeaponDamageModifier); 
		SCR_REGISTER_SECURE(RESET_AI_MELEE_WEAPON_DAMAGE_MODIFIER,0xc1a5ea5ad1304e68, ResetAiMeleeWeaponDamageModifier); 
		SCR_REGISTER_SECURE(SET_TREAT_AS_AMBIENT_PED_FOR_DRIVER_LOCKON,0x1a9e4c03ab189860, CommandSetTreatAsAmbientPedForDriverLockOn);
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_TARGETTED,0xa7266a50941dbaea, CommandSetPedCanBeTargetted);
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_TARGETTED_BY_TEAM,0x77d9bf25dcfb8b50, CommandSetPedCanBeTargettedByTeam);
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_TARGETTED_BY_PLAYER,0x55f28eb0406b14e5, CommandSetPedCanBeTargettedByPlayer);
		SCR_REGISTER_SECURE(SET_ALLOW_LOCKON_TO_PED_IF_FRIENDLY,0x37c6bc02ba3df98b, CommandAllowLockOnToPedIfFriendly);
		SCR_REGISTER_SECURE(SET_USE_CAMERA_HEADING_FOR_DESIRED_DIRECTION_LOCK_ON_TEST,0x82ec99aeec5eef51, CommandSetUseCameraHeadingForDesiredDirectionLockOnTest);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_POLICE_VEHICLE,0x79135882170e3d22, CommandIsPedInAnyPoliceVehicle);
		SCR_REGISTER_SECURE(FORCE_PED_TO_OPEN_PARACHUTE,0xfd4d36340bc86574, CommandForcePedToOpenParachute);
		SCR_REGISTER_SECURE(IS_PED_IN_PARACHUTE_FREE_FALL,0x90dcd1f6c0982dc1, CommandIsPedInParachuteFreefall);
		SCR_REGISTER_SECURE(IS_PED_FALLING,0x0208598d19e7a49f, CommandIsPedFalling);
		SCR_REGISTER_SECURE(IS_PED_JUMPING,0xab3249acbde2d8b9, CommandIsPedJumping);
		SCR_REGISTER_SECURE(IS_PED_LANDING,0xa63c3585afc99594, CommandIsPedLanding);
		SCR_REGISTER_SECURE(IS_PED_DOING_A_BEAST_JUMP,0x5a880630516d320c, CommandIsPedDoingABeastJump);
		SCR_REGISTER_SECURE(IS_PED_CLIMBING,0x1755a0cd57c780c6, CommandIsPedClimbing);
		SCR_REGISTER_SECURE(IS_PED_VAULTING,0x5048f93429fa0da7, CommandIsPedVaulting);
		SCR_REGISTER_SECURE(IS_PED_DIVING,0x58d958e5f5b6a54c, CommandIsPedDiving);
		SCR_REGISTER_SECURE(IS_PED_JUMPING_OUT_OF_VEHICLE,0x679b82f98ef660a7, CommandIsPedJumpingOutOfVehicle);
		SCR_REGISTER_SECURE(IS_PED_OPENING_DOOR,0xfe4a2839e865d693, CommandIsPedOpeningDoor);
		SCR_REGISTER_SECURE(GET_PED_PARACHUTE_STATE,0x0e68d1fee34d0c80, CommandGetPedParachuteState);
		SCR_REGISTER_SECURE(GET_PED_PARACHUTE_LANDING_TYPE,0xd12c319846733398, CommandGetPedParachuteLandingType);
		SCR_REGISTER_SECURE(SET_PED_PARACHUTE_TINT_INDEX,0xeceeb40254d34d4b,		CommandSetPedParachuteTintIndex);
		SCR_REGISTER_SECURE(GET_PED_PARACHUTE_TINT_INDEX,0xe7c1c44dc8730eee,		CommandGetPedParachuteTintIndex);
		SCR_REGISTER_UNUSED(SET_PED_PARACHUTE_PACK_TINT_INDEX,0x4715f80a6784f3ab,	CommandSetPedParachutePackTintIndex);
		SCR_REGISTER_UNUSED(GET_PED_PARACHUTE_PACK_TINT_INDEX,0xa9b05d2e79085bc0,	CommandGetPedParachutePackTintIndex);
		SCR_REGISTER_SECURE(SET_PED_RESERVE_PARACHUTE_TINT_INDEX,0xe4af87effb8df783,	CommandSetPedReserveParachuteTintIndex);
		SCR_REGISTER_UNUSED(GET_PED_RESERVE_PARACHUTE_TINT_INDEX,0xd1ad2c400759dd67,	CommandGetPedReserveParachuteTintIndex);
		SCR_REGISTER_SECURE(CREATE_PARACHUTE_BAG_OBJECT,0x9fd05a9837a2aff7,	CommandCreateParachuteBagObject);
		SCR_REGISTER_UNUSED(PED_GIVE_JETPACK,0x0f9077e4f806d159,	CommandGiveJetpack);
		SCR_REGISTER_UNUSED(PED_REMOVE_JETPACK,0xe461e5445a2bfd74,	CommandRemoveJetpack);
		SCR_REGISTER_UNUSED(PED_EQUIP_JETPACK,0xe71943fc88621d2b,	CommandEquipJetpack);
		SCR_REGISTER_UNUSED(PED_IS_JETPACK_EQUIPPED,0xd1644be3630d6bff,	CommandIsJetpackEquipped);
		SCR_REGISTER_UNUSED(PED_IS_USING_JETPACK,0x736a1e878921880f,	CommandIsUsingJetpack);
		SCR_REGISTER_SECURE(SET_PED_DUCKING,0xc28a66e18d3390fd, CommandSetPedDucking);
		SCR_REGISTER_SECURE(IS_PED_DUCKING,0xff45c7cd7f890b51, CommandIsPedDucking);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_TAXI,0x4da360fc7c3cfefa, CommandIsPedInAnyTaxi);
		SCR_REGISTER_SECURE(SET_PED_ID_RANGE,0xd9c790c4ce748354, CommandSetPedIdentificationRange);
		SCR_REGISTER_SECURE(SET_PED_HIGHLY_PERCEPTIVE,0x25d0f99524b26091, CommandSetPedHighlyPerceptive);
		SCR_REGISTER_SECURE(SET_COP_PERCEPTION_OVERRIDES,0x1b5e738a60ec68fd, CommandSetCopPerceptionOverrides);
		SCR_REGISTER_SECURE(SET_PED_INJURED_ON_GROUND_BEHAVIOUR,0xb9a5ec462968ebc8, CommandSetPedInjuredOnGroundBehaviour);
		SCR_REGISTER_UNUSED(CLEAR_PED_INJURED_ON_GROUND_BEHAVIOUR,0x8df094b6a61fb30d, CommandClearPedInjuredOnGroundBehaviour);
		SCR_REGISTER_SECURE(DISABLE_PED_INJURED_ON_GROUND_BEHAVIOUR,0x5e15b2064a26e398, CommandDisablePedInjuredOnGroundBehaviour);
		SCR_REGISTER_UNUSED(CLEAR_DISABLE_INJURED_ON_GROUND_BEHAVIOUR,0x872acebf055afa1c, CommandClearDisablePedInjuredOnGroundBehaviour);
		SCR_REGISTER_UNUSED(SET_SUPPRESS_LOW_RAGDOLL_LOD_SWITCH_UPON_DEATH,0x6d46b580c03aa602, CommandSetSuppressLowLODRagdollSwitchUponDeath);
		
		SCR_REGISTER_SECURE(SET_PED_SEEING_RANGE,0xe014ff3c785eefb5, CommandSetPedSeeingRange);
		SCR_REGISTER_SECURE(SET_PED_HEARING_RANGE,0x5c9d9a4ea475f37e, CommandSetPedHearingRange);

		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_MIN_ANGLE,0xa4e1b9926df4a312, CommandSetPedVisualFieldMinAngle);
		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_MAX_ANGLE,0xfcc8ae1090ba5929, CommandSetPedVisualFieldMaxAngle);
		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_MIN_ELEVATION_ANGLE,0x3adf13534ad69560, CommandSetPedVisualFieldMinElevationAngle);
		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_MAX_ELEVATION_ANGLE,0xbf15d9e4f14faa97, CommandSetPedVisualFieldMaxElevationAngle);
		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_PERIPHERAL_RANGE,0x52ea1154d214bfb6, CommandSetPedVisualFieldPeripheralRange);
		SCR_REGISTER_SECURE(SET_PED_VISUAL_FIELD_CENTER_ANGLE,0xed676e22a51c1922, CommandSetPedVisualFieldCenterAngle);
		
		SCR_REGISTER_UNUSED(GET_PED_VISUAL_FIELD_MIN_ANGLE,0x5b716d955c95d1b7, CommandGetPedVisualFieldMinAngle);
		SCR_REGISTER_UNUSED(GET_PED_VISUAL_FIELD_MAX_ANGLE,0xca772418d64e1dec, CommandGetPedVisualFieldMaxAngle);
		SCR_REGISTER_UNUSED(GET_PED_VISUAL_FIELD_MIN_ELEVATION_ANGLE,0x256a13f02de38068, CommandGetPedVisualFieldMinElevationAngle);
		SCR_REGISTER_UNUSED(GET_PED_VISUAL_FIELD_MAX_ELEVATION_ANGLE,0xf9f29f128bef1a03, CommandGetPedVisualFieldMaxElevationAngle);
		SCR_REGISTER_UNUSED(GET_PED_VISUAL_FIELD_PERIPHERAL_RANGE,0x2cc53e28af6b7801, CommandGetPedVisualFieldPeripheralRange);
		SCR_REGISTER_SECURE(GET_PED_VISUAL_FIELD_CENTER_ANGLE,0xcdf7d279e6f9a247, CommandGetPedVisualFieldCenterAngle);		
	

		SCR_REGISTER_SECURE(SET_PED_STEALTH_MOVEMENT,0x42a972967c73ab48, CommandSetPedStealthMovement);
		SCR_REGISTER_SECURE(GET_PED_STEALTH_MOVEMENT,0x6abfd2a0b56d6940, CommandGetPedStealthMovement);
		SCR_REGISTER_SECURE(CREATE_GROUP,0x0cc2bcb995d30ca3, CommandCreateGroup);
		SCR_REGISTER_SECURE(SET_PED_AS_GROUP_LEADER,0xaa1f79c4006d9ae2, CommandSetPedAsGroupLeader);
		SCR_REGISTER_SECURE(SET_PED_AS_GROUP_MEMBER,0xf23e6f5690771f71, CommandSetPedAsGroupMember);
		SCR_REGISTER_SECURE(SET_PED_CAN_TELEPORT_TO_GROUP_LEADER,0x44bc3176e12d5d7c, CommandSetPedCanTeleportToGroupLeader);
		SCR_REGISTER_SECURE(REMOVE_GROUP,0x62e720bbf19b53ad, CommandRemoveGroup);
		SCR_REGISTER_SECURE(REMOVE_PED_FROM_GROUP,0x6221a52631154ef7, CommandRemovePedFromGroup);
		SCR_REGISTER_SECURE(IS_PED_GROUP_MEMBER,0x2f52e009bbc0afce, CommandIsPedGroupMember);
		SCR_REGISTER_UNUSED(IS_PED_GROUP_LEADER,0xb87048bca2627dbc, CommandIsPedGroupLeader);
		SCR_REGISTER_SECURE(IS_PED_HANGING_ON_TO_VEHICLE,0xed99779c40aee6c4, CommandIsPedHangingOnToVehicle);
		SCR_REGISTER_SECURE(SET_GROUP_SEPARATION_RANGE,0x05ff7bcaaef919b8, CommandSetGroupSeparationRange);

		SCR_REGISTER_SECURE(SET_PED_MIN_GROUND_TIME_FOR_STUNGUN,0xb4d956c3c9624b6b, CommandSetPedMinGroundTimeForStunGun);

		SCR_REGISTER_SECURE(IS_PED_PRONE,0xa6bb28cad9fb957e,							CommandIsPedProne			);
		SCR_REGISTER_UNUSED(IS_PED_INVESTIGATING,0x384f62158a0fb144,			CommandIsPedInvestigating		);
		SCR_REGISTER_SECURE(IS_PED_IN_COMBAT,0xced7266bab0bdc20,				CommandIsPedInCombat			);
		SCR_REGISTER_SECURE(GET_PED_TARGET_FROM_COMBAT_PED,0x5c4aaba3e6cebf7f, CommandGetPedTargetFromCombatPed);
		SCR_REGISTER_SECURE(CAN_PED_IN_COMBAT_SEE_TARGET,0xb626e828f46adca7,	CommandCanPedInCombatSeeTarget	);
		SCR_REGISTER_SECURE(IS_PED_DOING_DRIVEBY,0xd2cf464c6a551325,			CommandIsPedDoingDriveby		);
		SCR_REGISTER_SECURE(IS_PED_JACKING,0x706e4529f37489db,				CommandIsPedJacking);
		SCR_REGISTER_SECURE(IS_PED_BEING_JACKED,0x177106d5e97d1958,			CommandIsPedBeingJacked);
		SCR_REGISTER_SECURE(IS_PED_BEING_STUNNED,0x7114c4d8ac9360fe,			CommandIsPedBeingStunned);
		SCR_REGISTER_SECURE(GET_PEDS_JACKER,0xba67eb7a53262b5b,				CommandGetPedsJacker);
		SCR_REGISTER_SECURE(GET_JACK_TARGET,0xc5df16431698b381,				CommandGetJackTarget);
		SCR_REGISTER_SECURE(IS_PED_FLEEING,0x51b334b227ec8042,				CommandIsPedFleeing					);
		SCR_REGISTER_SECURE(IS_PED_IN_COVER,0x1cfdd57741085128,				CommandIsPedInCover				);
		SCR_REGISTER_SECURE(IS_PED_IN_COVER_FACING_LEFT,0x40ae5496b9b55629,	CommandIsPedInCoverFacingLeft	);	
		SCR_REGISTER_SECURE(IS_PED_IN_HIGH_COVER,0x61926d906e949cee,			CommandIsPedInHighCover			);	
		SCR_REGISTER_SECURE(IS_PED_GOING_INTO_COVER,0xd2fb3e2f21b617df,		CommandIsPedGoingIntoCover		);
		SCR_REGISTER_SECURE(SET_PED_PINNED_DOWN,0x1fefcd675975f510,			CommandSetPedPinnedDown			);
		SCR_REGISTER_UNUSED(GIVE_PED_PICKUP_OBJECT,0x98cdf65a9523cb5b, CommandGivePedPickupObject);
		
		SCR_REGISTER_SECURE(GET_SEAT_PED_IS_TRYING_TO_ENTER,0xab51590a27ee5f98, CommandGetSeatPedIsTryingToEnter);
		SCR_REGISTER_SECURE(GET_VEHICLE_PED_IS_TRYING_TO_ENTER,0x6fb52092269a5c69, CommandGetVehiclePedIsTryingToEnter);
		SCR_REGISTER_SECURE(GET_PED_SOURCE_OF_DEATH,0xdb73c91f09bb8060, CommandGetPedSourceOfDeath);
		SCR_REGISTER_SECURE(GET_PED_CAUSE_OF_DEATH,0x91947acf8b072be2, CommandGetPedCauseOfDeath);
		SCR_REGISTER_SECURE(GET_PED_TIME_OF_DEATH,0xafa8dfb27e0b49a3, CommandGetPedTimeOfDeath);
		SCR_REGISTER_SECURE(COUNT_PEDS_IN_COMBAT_WITH_TARGET,0xbe961a177a9ec01c, CommandCountPedsInCombatWithTarget);
		SCR_REGISTER_SECURE(COUNT_PEDS_IN_COMBAT_WITH_TARGET_WITHIN_RADIUS,0xca0ed3b082a9c834, CommandCountPedsInCombatWithTargetWithinRadius);

		// Relationship script fns
		SCR_REGISTER_SECURE(SET_PED_RELATIONSHIP_GROUP_DEFAULT_HASH,0x97fede97c56990c8, CommandSetPedRelationshipGroupDefaultHash);
		SCR_REGISTER_SECURE(SET_PED_RELATIONSHIP_GROUP_HASH,0xff4beb6cff55a013, CommandSetPedRelationshipGroupHash);
		SCR_REGISTER_SECURE(SET_RELATIONSHIP_BETWEEN_GROUPS,0x9972efada7a2a47d, CommandSetRelationshipBetweenGroups);
		SCR_REGISTER_SECURE(CLEAR_RELATIONSHIP_BETWEEN_GROUPS,0x33b3a4723d5dbd35, CommandClearRelationshipBetweenGroups);
		SCR_REGISTER_SECURE(ADD_RELATIONSHIP_GROUP,0xd642e010a287adfd, CommandAddRelationshipGroup);
		SCR_REGISTER_SECURE(REMOVE_RELATIONSHIP_GROUP,0x8813f60f6f44c9e3, CommandRemoveRelationshipGroup);
		SCR_REGISTER_SECURE(DOES_RELATIONSHIP_GROUP_EXIST,0x5b79688a82eac2f0, CommandDoesRelationshipGroupExist);
		SCR_REGISTER_SECURE(GET_RELATIONSHIP_BETWEEN_PEDS,0x2996a489f1c8e5ae, CommandGetRelationshipBetweenPeds); 
		SCR_REGISTER_SECURE(GET_PED_RELATIONSHIP_GROUP_DEFAULT_HASH,0x012a87eeead89783,CommandGetPedRelationshipGroupDefaultHash);
		SCR_REGISTER_SECURE(GET_PED_RELATIONSHIP_GROUP_HASH,0x6f8fb52f5d1d0b84,CommandGetPedRelationshipGroupHash);
		SCR_REGISTER_SECURE(GET_RELATIONSHIP_BETWEEN_GROUPS,0x76932af783b85b6a,CommandGetRelationshipBetweenGroups);
		SCR_REGISTER_SECURE(SET_RELATIONSHIP_GROUP_AFFECTS_WANTED_LEVEL,0x36b51e98abf1418a, CommandSetRelationshipGroupAffectsWantedLevel);
		SCR_REGISTER_UNUSED(SET_BLIP_PEDS_IN_RELATIONSHIP_GROUP,0x072d52d37856b53c, CommandSetBlipPedsInRelationshipGroup);
		SCR_REGISTER_SECURE(TELL_GROUP_PEDS_IN_AREA_TO_ATTACK,0xfcdf7f655a86cf02, CommandTellGroupPedsInAreaToAttack);

		SCR_REGISTER_SECURE(SET_PED_CAN_BE_TARGETED_WITHOUT_LOS,0x6361fa70098d7777, CommandSetPedCanBeTargetedWithoutLos);
		SCR_REGISTER_SECURE(SET_PED_TO_INFORM_RESPECTED_FRIENDS,0x78d13ff59b219912, CommandSetPedInformRespectedFriends);
		SCR_REGISTER_SECURE(IS_PED_RESPONDING_TO_EVENT,0x1a3b69366fd32d14, CommandIsPedRespondingToEvent);
		SCR_REGISTER_UNUSED(GET_PED_INDEX_FROM_FIRED_EVENT,0x048fc51fbfc02257, CommandGetPedIndexFromFiredEvent);
		SCR_REGISTER_SECURE(GET_POS_FROM_FIRED_EVENT,0xae4ea19bb37a2329, CommandGetPosFromFiredEvent);
		SCR_REGISTER_SECURE(SET_PED_FIRING_PATTERN,0x8913620f26b565ce, CommandSetPedFiringPattern);
		SCR_REGISTER_SECURE(SET_PED_SHOOT_RATE,0xc97ba48bc273a1f3, CommandSetPedShootRate);
		SCR_REGISTER_SECURE(SET_COMBAT_FLOAT,0x9dc5ccc4c16546cb, CommandSetCombatFloat);
		SCR_REGISTER_SECURE(GET_COMBAT_FLOAT,0x275aa1bc4c1cc11e, CommandGetCombatFloat);
		SCR_REGISTER_SECURE(GET_GROUP_SIZE,0x7afff71021be70eb, CommandGetGroupSize);
		SCR_REGISTER_SECURE(DOES_GROUP_EXIST,0x3d3394023cf3b07d, CommandDoesGroupExist);

		SCR_REGISTER_SECURE(GET_PED_GROUP_INDEX,0x188e8a7343f4234f, CommandGetPedGroupIndex);
		SCR_REGISTER_SECURE(IS_PED_IN_GROUP,0x611ad675982bebcc, CommandIsPedInGroup);
		SCR_REGISTER_SECURE(GET_PLAYER_PED_IS_FOLLOWING,0x08435d2c2687236d, CommandGetPlayerPedIsFollowing);

		SCR_REGISTER_SECURE(SET_GROUP_FORMATION,0xb8f2b126cc78a3b5, CommandSetGroupFormation);
		SCR_REGISTER_SECURE(SET_GROUP_FORMATION_SPACING,0x4d7356aac1800b4e, CommandSetGroupFormationSpacing);
		SCR_REGISTER_SECURE(RESET_GROUP_FORMATION_DEFAULT_SPACING,0xab53b02740a50682, CommandResetGroupFormationDefaultSpacing);
		SCR_REGISTER_SECURE(GET_VEHICLE_PED_IS_USING,0xd9bd5965b9552645, CommandGetVehiclePedIsUsing);
		SCR_REGISTER_SECURE(GET_VEHICLE_PED_IS_ENTERING,0x8db5c2d06228df32, CommandGetVehiclePedIsEntering);

		SCR_REGISTER_SECURE(SET_PED_GRAVITY,0x2f3339afdd93fd01, CommandSetPedGravity);

		SCR_REGISTER_UNUSED(SET_PED_SWIMS_UNDER_SURFACE,0x1d5e00bda542db81, CommandSetSwimUnderSurface);

		SCR_REGISTER_SECURE(APPLY_DAMAGE_TO_PED,0xad22ea15d9200b6e,CommandSetPedDamage);
		SCR_REGISTER_SECURE(GET_TIME_PED_DAMAGED_BY_WEAPON,0x560e111ab28af995, CommandGetTimePedDamagedByWeapon);
		SCR_REGISTER_SECURE(SET_PED_ALLOWED_TO_DUCK,0xcd2750dc39e123f3, CommandSetPedAllowedToDuck);
		//		SCR_REGISTER(SET_PED_AREA_VISIBLE, CommandSetPedAreaVisible);
		//	SCR_REGISTER(GET_VEHICLE_PED_IS_STANDING_ON, CommandGetVehiclePedIsStandingOn);
		SCR_REGISTER_SECURE(SET_PED_NEVER_LEAVES_GROUP,0x508ea490c0230199, CommandSetPedNeverLeavesGroup);
		SCR_REGISTER_SECURE(GET_PED_TYPE,0xbe07513d5933d805, CommandGetPedType);
		SCR_REGISTER_SECURE(SET_PED_AS_COP,0x8601ad4e55c81412, CommandSetPedAsCop);

		SCR_REGISTER_SECURE(SET_PED_HEALTH_PENDING_LAST_DAMAGE_EVENT_OVERRIDE_FLAG, 0xb3352e018d6f89df, CommandSetHealthPendingLastDamageEventOverrideFlag);
		SCR_REGISTER_SECURE(SET_PED_MAX_HEALTH,0xd170c822231288fa, CommandSetPedMaxHealth);
		SCR_REGISTER_SECURE(GET_PED_MAX_HEALTH,0x3c739e920862f79e, CommandGetPedMaxHealth);
		SCR_REGISTER_SECURE(SET_PED_MAX_TIME_IN_WATER,0xc8a40a36f093f11c, CommandSetPedMaxTimeInWater);
		SCR_REGISTER_SECURE(SET_PED_MAX_TIME_UNDERWATER,0x8ca55705a203c152, CommandSetPedMaxTimeUnderwater);
		SCR_REGISTER_UNUSED(SET_GROUP_PED_DUCKS_WHEN_AIMED_AT, 0x13408c5, CommandSetGroupPedDucksWhenAimedAt);

		SCR_REGISTER_SECURE(SET_CORPSE_RAGDOLL_FRICTION,0x006c6a83fa52591f, CommandSetCorpseRagdollFriction);
		SCR_REGISTER_UNUSED(SET_CORPSE_SLEEP_TOLERANCES,0x2e92957b9e4a0784, CommandSetCorpseSleepTolerances);
		SCR_REGISTER_UNUSED(SET_CORPSE_SLEEPS_ALONE,0x44f13e14d41ea3c9, CommandSetCorpseSleepsAlone);

		SCR_REGISTER_SECURE(SET_PED_VEHICLE_FORCED_SEAT_USAGE,0x3c77f1ec499d217f, CommandSetPedVehicleForcedSeatUsage);
		SCR_REGISTER_SECURE(CLEAR_ALL_PED_VEHICLE_FORCED_SEAT_USAGE,0x3f284a8014906af7, CommandClearAllPedVehicleForcedSeatUsage);
		SCR_REGISTER_UNUSED(GET_VEHICLE_AND_FLAGS_FOR_PEDS_FORCED_SEAT_USAGE,0xf9cd14464039897c, CommandGetVehicleAndFlagsForPedsForcedSeatUsage);

		SCR_REGISTER_SECURE(SET_PED_CAN_BE_KNOCKED_OFF_BIKE,0x86862a0e9a1b98ba, CommandDeprecatedCommandWarningSetPedCanBeKnockedOffBike); // DEPRECATED!
		SCR_REGISTER_SECURE(SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE,0xb11c0cf5ccdfe10a, CommandSetPedCanBeKnockedOffVehicle);
		SCR_REGISTER_UNUSED(CAN_KNOCK_PED_OFF_BIKE,0x22dfba04555d690c, CommandDeprecatedCommandWarningCanKnockPedOffBike); // DEPRECATED:
		SCR_REGISTER_SECURE(CAN_KNOCK_PED_OFF_VEHICLE,0x15b724566dc371c1, CommandCanKnockPedOffVehicle);
		SCR_REGISTER_UNUSED(KNOCK_PED_OFF_BIKE,0x8c7b7c91763215af, CommandDeprecatedCommandWarningKnockPedOffBike); // DEPRECATED:
		SCR_REGISTER_SECURE(KNOCK_PED_OFF_VEHICLE,0x5aff9de62c9eb9a4, CommandKnockPedOffVehicle);

		SCR_REGISTER_SECURE(SET_PED_COORDS_NO_GANG,0x53abb8c1f2ce7610, CommandSetCharCoordsNoGang);
		SCR_REGISTER_SECURE(GET_PED_AS_GROUP_MEMBER,0x3722e3c325acb58e, CommandGetPedAsGroupMember);
		SCR_REGISTER_SECURE(GET_PED_AS_GROUP_LEADER,0xa252a0bae3a14a8d, CommandGetPedAsGroupLeader);

		SCR_REGISTER_SECURE(SET_PED_KEEP_TASK,0xabc2ca6f28903308, CommandSetPedKeepTask);
		SCR_REGISTER_SECURE(SET_PED_ALLOW_MINOR_REACTIONS_AS_MISSION_PED,0x36dd2f43086812aa, CommandSetPedAllowMinorReactionsAsMissionPed);
		SCR_REGISTER_SECURE(IS_PED_SWIMMING,0xda664493accdbe81, CommandIsPedSwimming);
		SCR_REGISTER_SECURE(IS_PED_SWIMMING_UNDER_WATER,0x35d4f23cb8a63edb, CommandIsPedSwimmingUnderWater);
		SCR_REGISTER_SECURE(SET_PED_COORDS_KEEP_VEHICLE,0xcb8859434b382fcc, CommandSetCharCoordsKeepVehicle);
		SCR_REGISTER_SECURE(SET_PED_DIES_IN_VEHICLE,0x279e52abb546c567, CommandSetPedDiesInVehicle);
		//	SCR_REGISTER(SET_PED_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE, CommandSetPedUsesCollisionClosestObjectOfType);
		SCR_REGISTER_SECURE(SET_CREATE_RANDOM_COPS,0x3b8c20ef19a49c3e, CommandSetCreateRandomCops);
		SCR_REGISTER_SECURE(SET_CREATE_RANDOM_COPS_NOT_ON_SCENARIOS,0x400c5dcf491aa8cb, CommandSetCreateRandomCopsNotOnScenarios);
		SCR_REGISTER_SECURE(SET_CREATE_RANDOM_COPS_ON_SCENARIOS,0x3868ab3b4757b449, CommandSetCreateRandomCopsOnScenarios);
		SCR_REGISTER_SECURE(CAN_CREATE_RANDOM_COPS,0x259839223a5768d3, CommandCanCreateRandomCops);
		SCR_REGISTER_UNUSED(SET_PED_IN_CUTSCENE,0x220f63ffeae8ae7e, CommandSetPedInCutscene);
		SCR_REGISTER_SECURE(SET_PED_AS_ENEMY,0x80476b7f23bcbb1b, CommandSetPedAsEnemy);
		SCR_REGISTER_SECURE(SET_PED_CAN_SMASH_GLASS,0x481a9844d115998b, CommandSetPedCanSmashGlass);
		SCR_REGISTER_SECURE(IS_PED_IN_ANY_TRAIN,0x2852e016e0ac134c, CommandIsPedInAnyTrain);
		SCR_REGISTER_SECURE(IS_PED_GETTING_INTO_A_VEHICLE,0xcede6233b8f49499, CommandIsPedGettingInToAVehicle);
		SCR_REGISTER_SECURE(IS_PED_TRYING_TO_ENTER_A_LOCKED_VEHICLE,0x65afecbe42149b44, CommandIsPedTryingToEnterALockedVehicle);
		SCR_REGISTER_SECURE(SET_ENABLE_HANDCUFFS,0xa1b68dbb639cafd5, CommandSetEnableHandCuffs);
		SCR_REGISTER_UNUSED(GET_ENABLE_HANDCUFFS,0xc9939a29a72bd813, CommandGetEnableHandCuffs);		
		SCR_REGISTER_SECURE(SET_ENABLE_BOUND_ANKLES,0xa8921dc376420b57, CommandSetEnableBoundAnkles);
		SCR_REGISTER_UNUSED(GET_ENABLE_BOUND_ANKLES,0x2d778f75373854e6, CommandGetEnableBoundAnkles);
		SCR_REGISTER_SECURE(SET_ENABLE_SCUBA,0xa48f5a9b377b675c, CommandSetEnableScuba);
		SCR_REGISTER_SECURE(SET_CAN_ATTACK_FRIENDLY,0x5457a5652683c8d1, CommandSetCanAttackFriendly);		 
		SCR_REGISTER_SECURE(GET_PED_ALERTNESS,0x44c2858965e93d87, CommandGetPedAlertness);
		SCR_REGISTER_SECURE(SET_PED_ALERTNESS,0x0aebdb297e8270b8, CommandSetPedAlertness);
		SCR_REGISTER_SECURE(SET_PED_GET_OUT_UPSIDE_DOWN_VEHICLE,0x5d1d64e01b76705c, CommandSetPedGetOutUpsideDownVehicle);
		SCR_REGISTER_UNUSED(SET_PED_CAN_REMAIN_ON_BOAT_AFTER_MISSION_ENDS,0x9d645c858391b15b, CommandSetPedCanRemainOnBoatAfterMissionEnds);

		SCR_REGISTER_SECURE(SET_PED_MOVEMENT_CLIPSET,0xabfc84f5c4943d7b, CommandSetPedMovementClipSet);
		SCR_REGISTER_SECURE(RESET_PED_MOVEMENT_CLIPSET,0x03dd3da99c7ed7d1, CommandResetPedMovementClipSet);
		SCR_REGISTER_SECURE(SET_PED_STRAFE_CLIPSET,0x1678c93ea5027838, CommandSetPedStrafeClipSet);
		SCR_REGISTER_SECURE(RESET_PED_STRAFE_CLIPSET,0x9e13b6cec9d1713a, CommandResetPedStrafeClipSet);
		//SCR_REGISTER_UNUSED(SET_PED_INJURED_STRAFE_CLIPSET, 0x3a33f7c6, CommandSetPedInjuredStrafeClipSet);
		SCR_REGISTER_SECURE(SET_PED_WEAPON_MOVEMENT_CLIPSET,0xf31fd609b1e9f297, CommandSetPedWeaponMovementClipSet);
		SCR_REGISTER_SECURE(RESET_PED_WEAPON_MOVEMENT_CLIPSET,0x2dcd5a713355ab1b, CommandResetPedWeaponMovementClipSet);
		SCR_REGISTER_SECURE(SET_PED_DRIVE_BY_CLIPSET_OVERRIDE,0x187c40514bcb5a8f, CommandSetPedDriveByClipSetOverride);
		SCR_REGISTER_SECURE(CLEAR_PED_DRIVE_BY_CLIPSET_OVERRIDE,0x4e03f3499cdd3adc, CommandClearPedDriveByClipSetOverride );
		SCR_REGISTER_SECURE(SET_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE,0x76fa429844b05aeb, CommandSetPedMotionInCoverClipSetOverride);
		SCR_REGISTER_SECURE(CLEAR_PED_MOTION_IN_COVER_CLIPSET_OVERRIDE,0xd902002c7693a977, CommandClearPedMotionInCoverClipSetOverride );			
		SCR_REGISTER_UNUSED(SET_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE,0xd49f3e8d262c582b, CommandSetPedFallUpperBodyClipSetOverride);
		SCR_REGISTER_SECURE(CLEAR_PED_FALL_UPPER_BODY_CLIPSET_OVERRIDE,0xa86ecf8f6fc80b3c, CommandClearPedFallUpperBodyClipSetOverride);
		SCR_REGISTER_SECURE(SET_PED_IN_VEHICLE_CONTEXT,0xc28ceefc530c40f4, CommandSetPedInVehicleContext);
		SCR_REGISTER_SECURE(RESET_PED_IN_VEHICLE_CONTEXT,0x3cac880816f6f3f4, CommandResetPedInVehicleContext);

		// START OF DEPRECATED
		SCR_REGISTER_UNUSED(SET_PED_ANIM_GROUP,0x15d9e91bda6a2187, CommandSetPedAnimGroup);
		SCR_REGISTER_UNUSED(RESET_PED_ANIM_GROUP,0x0336fbfb8ce7d29e, CommandResetPedAnimGroup);
		SCR_REGISTER_UNUSED(SET_PED_ANIM_GROUP_TWO_HANDED,0x3b7db4a0597d23fd, CommandSetPedAnimGroupTwoHanded);
		SCR_REGISTER_UNUSED(RESET_PED_ANIM_GROUP_TWO_HANDED,0x2bfffd8196dadcdd, CommandResetPedAnimGroupTwoHanded);
		SCR_REGISTER_UNUSED(SET_PED_ANIM_GROUP_STRAFE,0x7ca4951f41ed97a4, CommandSetPedAnimGroupStrafe);
		SCR_REGISTER_UNUSED(RESET_PED_ANIM_GROUP_STRAFE,0x8919b36df3167b75, CommandResetPedAnimGroupStrafe);
		SCR_REGISTER_UNUSED(FORCE_WEAPON_MOVEMENT_ANIM_GROUP,0xd88835d51ef9fed8, CommandForceWeaponMovementAnimGroup);
		SCR_REGISTER_UNUSED(RESET_WEAPON_MOVEMENT_ANIM_GROUP,0x3eee594c7289e9a2, CommandResetWeaponMovementAnimGroup);
		// END OF DEPRECATED
		
		SCR_REGISTER_SECURE(IS_SCRIPTED_SCENARIO_PED_USING_CONDITIONAL_ANIM,0x1f4534a6b5a5ac67, CommandIsScriptedScenarioPedUsingConditionalAnim);
		SCR_REGISTER_SECURE(SET_PED_ALTERNATE_WALK_ANIM,0xd49494d2e29696de, CommandSetPedAlternateWalkAnim);
		SCR_REGISTER_SECURE(CLEAR_PED_ALTERNATE_WALK_ANIM,0xba002e17c13f9dd5, CommandClearPedAlternateWalkAnim);
		SCR_REGISTER_SECURE(SET_PED_ALTERNATE_MOVEMENT_ANIM,0x1506c7782f807090, CommandSetPedAlternateMovementAnim);
		SCR_REGISTER_SECURE(CLEAR_PED_ALTERNATE_MOVEMENT_ANIM,0xd14025fd10232a78, CommandClearPedAlternateMovementAnim);

		SCR_REGISTER_SECURE(SET_PED_GESTURE_GROUP,0xbbb4cdefa58f8ea4, CommandSetPedGestureGroup);

		SCR_REGISTER_UNUSED(BLEND_OUT_SYNCED_MOVEMENT_ANIMS,0x6d5d7ba79cedbdf1, CommandBlendOutSynchronisedMovementAnims);

		SCR_REGISTER_SECURE(GET_ANIM_INITIAL_OFFSET_POSITION,0x3dbab8e11699eaee, CommandGetAnimInitialOffsetPosition);
		SCR_REGISTER_SECURE(GET_ANIM_INITIAL_OFFSET_ROTATION,0x34aedcea59226ccb, CommandGetAnimInitialOffsetRotation);

		SCR_REGISTER_SECURE(GET_PED_DRAWABLE_VARIATION,0x5355baa621c153cf, CommandGetPedDrawableVariation);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS,0x37684f94e628d1ac, CommandGetNumberOfPedDrawableVariations);
		SCR_REGISTER_SECURE(GET_PED_TEXTURE_VARIATION,0xf1050e548c37f4a5, CommandGetPedTextureVariation);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_PED_TEXTURE_VARIATIONS,0x91e7da9eaffb9499, CommandGetNumberOfPedTextureVariations);

		SCR_REGISTER_SECURE(GET_NUMBER_OF_PED_PROP_DRAWABLE_VARIATIONS,0x83174885c2f9e9d2, CommandGetNumberOfPedPropDrawableVariations);
		SCR_REGISTER_SECURE(GET_NUMBER_OF_PED_PROP_TEXTURE_VARIATIONS,0x7827f3e641387ab3, CommandGetNumberOfPedPropTextureVariations);

		SCR_REGISTER_SECURE(GET_PED_PALETTE_VARIATION,0x272ab65a4e7277b4, CommandGetPedPaletteVariation);
		SCR_REGISTER_UNUSED(GET_PED_HAS_PALETTE_VARIATIONS,0xd0b596b57ffef115, CommandGetPedHasPaletteVariations);

		SCR_REGISTER_SECURE(GET_MP_OUTFIT_DATA_FROM_METADATA,0x9e594edbc398f0b7, CommandGetMpOutfitDataFromMetadata);
		SCR_REGISTER_SECURE(GET_FM_MALE_SHOP_PED_APPAREL_ITEM_INDEX,0xa713d1504e921b62, CommandGetFmMaleShopPedApparelItemIndex);
		SCR_REGISTER_SECURE(GET_FM_FEMALE_SHOP_PED_APPAREL_ITEM_INDEX,0x033025c1430dd8e8, CommandGetFmFemaleShopPedApparelItemIndex);

		SCR_REGISTER_SECURE(IS_PED_COMPONENT_VARIATION_VALID,0xebce16417d73f442, CommandIsPedComponentVariationValid);
		SCR_REGISTER_UNUSED(IS_PED_DRAWABLE_GEN9_EXCLUSIVE, 0xa322b93edf3308c0, CommandIsPedDrawableGen9Exclusive);
		SCR_REGISTER_SECURE(SET_PED_COMPONENT_VARIATION,0xe3752b10dc995e95, CommandSetPedComponentVariation);
		SCR_REGISTER_SECURE(SET_PED_RANDOM_COMPONENT_VARIATION,0x6e9c0f9eac1f29fd, CommandSetPedRandomComponentVariation);
		SCR_REGISTER_SECURE(SET_PED_RANDOM_PROPS,0xc9cf12827770bd57, CommandSetPedRandomProps);
		SCR_REGISTER_SECURE(SET_PED_DEFAULT_COMPONENT_VARIATION,0xdcb52c614b3660ec, CommandSetPedDefaultComponentVariation);
		SCR_REGISTER_SECURE(SET_PED_BLEND_FROM_PARENTS,0x2ef65220421a8b50, CommandSetPedBlendFromParents);
		SCR_REGISTER_UNUSED(UPDATE_PED_CHILD_BLEND,0xd44ed051bd2935fe, CommandUpdatePedChildBlend);
		SCR_REGISTER_UNUSED(SET_NEW_PARENT_BLEND_VALUES,0x3a5663c87f0197c9, CommandSetNewParentBlendValues);
		SCR_REGISTER_SECURE(SET_PED_HEAD_BLEND_DATA,0x58aec6e61e9195c4, CommandSetPedHeadBlendData);
		SCR_REGISTER_SECURE(GET_PED_HEAD_BLEND_DATA,0xce44afeba40e631e, CommandGetHeadBlendData);
		SCR_REGISTER_SECURE(UPDATE_PED_HEAD_BLEND_DATA,0xc28edf6b742b7487, CommandUpdatePedHeadBlendData);
		SCR_REGISTER_UNUSED(GET_NUM_EYE_COLORS,0xdedf8e927d94a1cf, CommandGetNumEyeColors);
		SCR_REGISTER_SECURE(SET_HEAD_BLEND_EYE_COLOR,0xad6af909b76be761, CommandSetHeadBlendEyeColor);
		SCR_REGISTER_SECURE(GET_HEAD_BLEND_EYE_COLOR,0x60aee53eba7fb84e, CommandGetHeadBlendEyeColor);
		SCR_REGISTER_SECURE(SET_PED_HEAD_OVERLAY,0xeea4716bed50e70e, CommandSetPedHeadOverlay);
		SCR_REGISTER_UNUSED(UPDATE_PED_HEAD_OVERLAY_BLEND,0x9ce884f2da5f83b4, CommandUpdatePedHeadOverlayBlend);
		SCR_REGISTER_SECURE(GET_PED_HEAD_OVERLAY,0x524d86f94d41f595, CommandGetPedHeadOverlay);
		SCR_REGISTER_SECURE(GET_PED_HEAD_OVERLAY_NUM,0x3d91d10f4eb20d19, CommandGetPedHeadOverlayNum);
		SCR_REGISTER_SECURE(SET_PED_HEAD_OVERLAY_TINT,0x091c6c826ff1f4bf, CommandSetPedHeadOverlayTint);
		SCR_REGISTER_SECURE(SET_PED_HAIR_TINT,0x549d1c729f88a5f3, CommandSetPedHairTint);
		SCR_REGISTER_UNUSED(GET_PED_HAIR_TINT,0xa17f1e4c38feda3f, CommandGetPedHairTint);
		SCR_REGISTER_SECURE(GET_NUM_PED_HAIR_TINTS,0x5540951a450d9a99, CommandGetNumPedHairTints);
		SCR_REGISTER_SECURE(GET_NUM_PED_MAKEUP_TINTS,0xec5e0799c4e9eb01, CommandGetNumPedMakeupTints);
		SCR_REGISTER_SECURE(GET_PED_HAIR_TINT_COLOR,0x1e12f87d8ad6a8a4, CommandGetPedHairTintColor);
		SCR_REGISTER_SECURE(GET_PED_MAKEUP_TINT_COLOR,0x3a98a1b237b9fe36, CommandGetPedMakeupTintColor);
		SCR_REGISTER_SECURE(IS_PED_HAIR_TINT_FOR_CREATOR,0xee482139c611b9cc, CommandIsPedHairTintForCreator);
		SCR_REGISTER_SECURE(GET_DEFAULT_SECONDARY_TINT_FOR_CREATOR,0x95246dc935a03da5, CommandGetDefaultSecondaryTintForCreator);
		SCR_REGISTER_UNUSED(IS_PED_ACCS_TINT_FOR_CREATOR,0xd8439398a09bd43f, CommandIsPedAccsTintForCreator);		
		SCR_REGISTER_SECURE(IS_PED_LIPSTICK_TINT_FOR_CREATOR,0xd4011241b1f97322, CommandIsPedLipstickTintForCreator);
		SCR_REGISTER_SECURE(IS_PED_BLUSH_TINT_FOR_CREATOR,0x869c8d46ee635616, CommandIsPedBlushTintForCreator);
		SCR_REGISTER_SECURE(IS_PED_HAIR_TINT_FOR_BARBER,0x7c2f1e7e2ca7fc2f, CommandIsPedHairTintForBarber);
		SCR_REGISTER_SECURE(GET_DEFAULT_SECONDARY_TINT_FOR_BARBER,0xe2d23ecfe5976c9b, CommandGetDefaultSecondaryTintForBarber);		
		SCR_REGISTER_UNUSED(IS_PED_ACCS_TINT_FOR_BARBER,0xde82d711224af469, CommandIsPedAccsTintForBarber);
		SCR_REGISTER_SECURE(IS_PED_LIPSTICK_TINT_FOR_BARBER,0x07a81eb81b548019, CommandIsPedLipstickTintForBarber);
		SCR_REGISTER_SECURE(IS_PED_BLUSH_TINT_FOR_BARBER,0x3e23c81d86d7c3f5, CommandIsPedBlushTintForBarber);
		SCR_REGISTER_SECURE(IS_PED_BLUSH_FACEPAINT_TINT_FOR_BARBER,0xe0a6a67c3ce62f06,CommandIsPedBlushFacepaintTintForBarber);
		SCR_REGISTER_UNUSED(HAIR_STYLE_HAS_SECONDARY_HAIR_COLOR,0xab85ed5e039e2f71, CommandHairStyleHasSecondaryHairColor);
		SCR_REGISTER_UNUSED(HAIR_STYLE_HAS_SECONDARY_ACCS_COLOR,0x29b2d92c0e5d9e20, CommandHairStyleHasSecondaryAccsColor);
		SCR_REGISTER_SECURE(GET_TINT_INDEX_FOR_LAST_GEN_HAIR_TEXTURE,0x2e06c1a087108c70, CommandGetTintIndexForLastGenHairTexture);
		SCR_REGISTER_SECURE(SET_PED_MICRO_MORPH,0x00a45035a743b400, CommandSetPedMicroMorph);
		SCR_REGISTER_SECURE(HAS_PED_HEAD_BLEND_FINISHED,0xa13a75665f7e0689, CommandHasPedHeadBlendFinished);
		SCR_REGISTER_SECURE(FINALIZE_HEAD_BLEND,0xad9047296f9ea375, CommandFinalizeHeadBlend);
        SCR_REGISTER_SECURE(SET_HEAD_BLEND_PALETTE_COLOR,0x891b421a88aeb58d, CommandSetHeadBlendPaletteColor);
        SCR_REGISTER_SECURE(DISABLE_HEAD_BLEND_PALETTE_COLOR,0x3e329db5cfd8fc35, CommandDisableHeadBlendPaletteColor);
        SCR_REGISTER_SECURE(GET_PED_HEAD_BLEND_FIRST_INDEX,0x6ea7474b9396c6ff, CommandGetPedHeadBlendFirstIndex);
        SCR_REGISTER_SECURE(GET_PED_HEAD_BLEND_NUM_HEADS,0x4f025a52c10fead7, CommandGetPedHeadBlendNumHeads);
		SCR_REGISTER_SECURE(SET_PED_PRELOAD_VARIATION_DATA,0xf7438ef713cbdaac, CommandSetPedPreloadVariationData);
		SCR_REGISTER_UNUSED(RELEASE_PED_COMPONENT_PRELOAD_VARIATION_DATA,0xc2051e5d52180b96, CommandReleasePedComponentPreloadVariationData);
		SCR_REGISTER_SECURE(HAS_PED_PRELOAD_VARIATION_DATA_FINISHED,0x5e34706debc06639, CommandHasPedPreloadVariationDataFinished);
		SCR_REGISTER_UNUSED(HAS_PED_PRELOAD_VARIATION_DATA_HANDLE_FINISHED,0x9373bd653f0c5c63, CommandHasPedPreloadVariationDataHandleFinished);
		SCR_REGISTER_SECURE(RELEASE_PED_PRELOAD_VARIATION_DATA,0x6475234c10008e58, CommandReleasePedPreloadVariationData);
		SCR_REGISTER_UNUSED(RELEASE_PED_PRELOAD_VARIATION_DATA_HANDLE,0x6b568734d4bffeeb, CommandReleasePedPreloadVariationDataHandle);
		SCR_REGISTER_SECURE(SET_PED_PRELOAD_PROP_DATA,0x00b2fa06032579d8, CommandSetPedPreloadPropData);
		SCR_REGISTER_SECURE(HAS_PED_PRELOAD_PROP_DATA_FINISHED,0x842411a372bc2b31, CommandHasPedPreloadPropDataFinished);
		SCR_REGISTER_UNUSED(HAS_PED_PRELOAD_PROP_DATA_HANDLE_FINISHED,0x298d4abff6234486, CommandHasPedPreloadPropDataHandleFinished);
		SCR_REGISTER_SECURE(RELEASE_PED_PRELOAD_PROP_DATA,0x5aa75ad81dae23c0, CommandReleasePedPreloadPropData);
		SCR_REGISTER_UNUSED(RELEASE_PED_PRELOAD_PROP_DATA_HANDLE,0xc9299657b1e0bc1e, CommandReleasePedPreloadPropDataHandle);

		SCR_REGISTER_SECURE(GET_PED_PROP_INDEX,0xf2bc143f97765619, CommandGetPedPropIndex);
		SCR_REGISTER_SECURE(SET_PED_PROP_INDEX,0xd9d122a54ffe62b8, CommandSetPedPropIndex);
		SCR_REGISTER_SECURE(KNOCK_OFF_PED_PROP,0x45a9c303f69dd965, CommandKnockOffPedProp);
		SCR_REGISTER_SECURE(CLEAR_PED_PROP,0x920bb8b58bb3563a, CommandClearPedProp);
		SCR_REGISTER_SECURE(CLEAR_ALL_PED_PROPS,0x9c726995fd3f93eb, CommandClearAllPedProps);
		SCR_REGISTER_SECURE(DROP_AMBIENT_PROP,0x862c576661f1aaef, CommandDropAmbientProp);
		SCR_REGISTER_SECURE(GET_PED_PROP_TEXTURE_INDEX,0x7252a84eced5e1d4, CommandGetPropTextureIndex);

		SCR_REGISTER_UNUSED(SET_PED_PARACHUTE_PACK_VARIATION,0x03f139aeb2b77d4a, CommandSetPedParachutePackVariation);
		SCR_REGISTER_SECURE(CLEAR_PED_PARACHUTE_PACK_VARIATION,0x1d73428ceb1c4e64, CommandClearPedParachutePackVariation);
		SCR_REGISTER_SECURE(SET_PED_SCUBA_GEAR_VARIATION,0x0cf7c79f71406fdc, CommandSetPedScubaGearVariation);
		SCR_REGISTER_SECURE(CLEAR_PED_SCUBA_GEAR_VARIATION,0x950a45e233c248aa, CommandClearPedScubaGearVariation);
		SCR_REGISTER_SECURE(IS_USING_PED_SCUBA_GEAR_VARIATION,0x873b4e9bcaf356ca, CommandIsUsingScubaGearVariation);

		SCR_REGISTER_SECURE(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,0x50274a7eaca3133a, CommandSetBlockingOfNonTemporaryEvents);

		SCR_REGISTER_UNUSED(SET_PED_RAGDOLL_BOUNDS_SIZE,0x7ae4016b30cf7bc2, CommandSetPedRagdollBoundsSize);
		SCR_REGISTER_SECURE(SET_PED_BOUNDS_ORIENTATION,0x6f5829203cb3b0aa, CommandSetPedBoundsOrientation);

		// Combat target registration
		SCR_REGISTER_SECURE(REGISTER_TARGET,0xeca3a29060f3b861,							CommandRegisterTarget);
		SCR_REGISTER_UNUSED(REGISTER_HATED_TARGETS_IN_AREA,0xd9cae00f126eff8b,			CommandRegisterHatedTargetsInArea);
		SCR_REGISTER_SECURE(REGISTER_HATED_TARGETS_AROUND_PED,0xd358d1ba340ce9af,			CommandRegisterHatedTargetsAroundPed);

		SCR_REGISTER_SECURE(GET_RANDOM_PED_AT_COORD,0xe66e6fdcd35afafe, CommandGetRandomPedAtCoord);
		SCR_REGISTER_SECURE(GET_CLOSEST_PED,0x72e575062e5d1004, CommandGetClosestPed);

		SCR_REGISTER_SECURE(SET_SCENARIO_PEDS_TO_BE_RETURNED_BY_NEXT_COMMAND,0xca700bb27cecbe1c, CommandSetScenarioPedsCanBeReturnedByNextCommand);

		SCR_REGISTER_SECURE(GET_CAN_PED_BE_GRABBED_BY_SCRIPT,0x0c285f0981ce9e32, CommandGetCanPedBeGrabbedByScript);

		SCR_REGISTER_UNUSED(GET_DRIVER_RACING_MODIFIER,0xf1d03568df52bcef, CommandGetDriverRacingModifier);
		SCR_REGISTER_SECURE(SET_DRIVER_RACING_MODIFIER,0x20eeeabbc2c25c15, CommandSetDriverRacingModifier);
		SCR_REGISTER_SECURE(SET_DRIVER_ABILITY,0x7f3313f2f80dd857, CommandSetDriverAbility);
		SCR_REGISTER_SECURE(SET_DRIVER_AGGRESSIVENESS,0x99bb016876efec17, CommandSetDriverAggressiveness);
		SCR_REGISTER_UNUSED(GET_DRIVER_ABILITY,0xf23065fab32fee6e, CommandGetDriverAbility);
		SCR_REGISTER_UNUSED(GET_DRIVER_AGGRESSIVENESS,0xcde4e3726f4bd11d, CommandGetDriverAggressiveness);

		SCR_REGISTER_SECURE(CAN_PED_RAGDOLL,0xeefbe22d73df1886, CommandCanPedRagdoll);
		SCR_REGISTER_SECURE(SET_PED_TO_RAGDOLL,0x33021f68edb8f06e, CommandSetPedToRagdoll);
		SCR_REGISTER_SECURE(SET_PED_TO_RAGDOLL_WITH_FALL,0xfd90df0233e2f4c4, CommandSetPedToRagdollWithFall);
		SCR_REGISTER_SECURE(SET_PED_RAGDOLL_ON_COLLISION,0x6a4535e90fc3d334, CommandSetPedRagdollOnCollision);
		SCR_REGISTER_UNUSED(SET_PED_TO_ANIMATED,0xc0322cc73cd14c64, CommandSetPedToAnimated);
		SCR_REGISTER_SECURE(IS_PED_RAGDOLL,0x32dcda1b2f8a0694, CommandIsPedRagdoll);
		SCR_REGISTER_SECURE(IS_PED_RUNNING_RAGDOLL_TASK,0x41fecc8db00381bd, CommandIsPedRunningRagdollTask);
		SCR_REGISTER_SECURE(SET_PED_RAGDOLL_FORCE_FALL,0xab75a7231383f0c6, CommandSetPedRagdollForceFall);

		SCR_REGISTER_SECURE(RESET_PED_RAGDOLL_TIMER,0x3ca5291882754781, CommandResetPedRagdollTimer);
		SCR_REGISTER_SECURE(SET_PED_CAN_RAGDOLL,0xc252f409bde7a700, CommandSetPedCanRagdoll);

		SCR_REGISTER_SECURE(IS_PED_RUNNING_MELEE_TASK,0x22aea8376b03e2d2, CommandIsPedRunningMeleeTask);

		SCR_REGISTER_SECURE(IS_PED_RUNNING_MOBILE_PHONE_TASK,0xe7b380b27e19c446, CommandIsPedRunningMobilePhoneTask);
		SCR_REGISTER_SECURE(IS_MOBILE_PHONE_TO_PED_EAR,0x8076906395af3f9d, CommandIsMobilePhoneToPedEar);

		SCR_REGISTER_SECURE(SET_RAGDOLL_BLOCKING_FLAGS,0xd07e491bda082ced, CommandSetRagdollBlockingFlags);
		SCR_REGISTER_SECURE(CLEAR_RAGDOLL_BLOCKING_FLAGS,0xf8a35ac94bea4275, CommandClearRagdollBlockingFlags);

		SCR_REGISTER_SECURE(SET_PED_ANGLED_DEFENSIVE_AREA,0x0913ed93c4ab21cf, CommandSetPedAngledDefensiveArea);
		SCR_REGISTER_SECURE(SET_PED_SPHERE_DEFENSIVE_AREA,0x1ee49aba6b11e9cf, CommandSetPedSphereDefensiveArea);
		SCR_REGISTER_SECURE(SET_PED_DEFENSIVE_SPHERE_ATTACHED_TO_PED,0x79521de15ee9c694, CommandSetPedDefensiveSphereAttachedToPed);
		SCR_REGISTER_SECURE(SET_PED_DEFENSIVE_SPHERE_ATTACHED_TO_VEHICLE,0x776d1abb5a5cc181, CommandSetPedDefensiveSphereAttachedToVehicle);

		SCR_REGISTER_SECURE(SET_PED_DEFENSIVE_AREA_ATTACHED_TO_PED,0xa61720a36f4fba8d, CommandSetPedDefensiveAreaAttachedToPed);
		SCR_REGISTER_SECURE(SET_PED_DEFENSIVE_AREA_DIRECTION,0x6443573599f9e0da,		CommandSetPedDefensiveAreaDirection);

		SCR_REGISTER_SECURE(REMOVE_PED_DEFENSIVE_AREA,0x5174a997f4970ae4, CommandRemovePedDefensiveArea);
		SCR_REGISTER_SECURE(GET_PED_DEFENSIVE_AREA_POSITION,0x4b7afb84113e9200, CommantGetPedDefensiveAreaPosition);
		SCR_REGISTER_SECURE(IS_PED_DEFENSIVE_AREA_ACTIVE,0xf38ab3e04879c43e, CommandIsPedDefensiveAreaActive);

		SCR_REGISTER_SECURE(SET_PED_PREFERRED_COVER_SET,0x929c816b693fe2df, CommandSetPedPreferredCoverSet);
		SCR_REGISTER_SECURE(REMOVE_PED_PREFERRED_COVER_SET,0xd5cc4d7c1f37624a, CommandRemovePedPreferredCoverSet);

		SCR_REGISTER_SECURE(REVIVE_INJURED_PED,0x38bd4cfc2abd412b, CommandReviveInjuredPed );
		SCR_REGISTER_SECURE(RESURRECT_PED,0x6adb1e9f1234cde6, CommandResurrectPed );

		SCR_REGISTER_SECURE(SET_PED_NAME_DEBUG,0xd8b630f464fe1d6d, CommandDebugSetPedName );

		SCR_REGISTER_SECURE(GET_PED_EXTRACTED_DISPLACEMENT,0x3afc066cc1ffd0e2, CommandGetPedExtractedDisplacement );

		SCR_REGISTER_SECURE(SET_PED_DIES_WHEN_INJURED,0xa5d600c274cc186f, CommandSetPedDiesWhenInjured );

		SCR_REGISTER_SECURE(SET_PED_ENABLE_WEAPON_BLOCKING,0xd8eae777cf6367dd, CommandSetPedEnableWeaponBlocking );		

		SCR_REGISTER_SECURE(SPECIAL_FUNCTION_DO_NOT_USE,0x6e598d124998c62c,			CommandDetachPedFromWithinVehicle);
		SCR_REGISTER_UNUSED(SET_HEADING_LIMIT_FOR_ATTACHED_PED, 0xa7813774,		CommandSetHeadingLimitForAttachedPed);
		SCR_REGISTER_UNUSED(SET_ATTACHED_PED_ROTATION,0x7a5c5a23fc8ff225,					CommandSetAttachedPedRotation);

		SCR_REGISTER_SECURE(RESET_PED_VISIBLE_DAMAGE,0x8fa4320f1f0bbd88,					CommandResetPedVisibleDamage);
		
		// old/depricated ped blood functions
		SCR_REGISTER_SECURE(APPLY_PED_BLOOD_DAMAGE_BY_ZONE,0x9777c2bd7f179f2d,				CommandApplyPedBloodDamageByZone);
		
		// new style ped blood/scar functions
		SCR_REGISTER_SECURE(APPLY_PED_BLOOD,0xe5bf90257c46ec94,						CommandApplyPedBlood);
		SCR_REGISTER_SECURE(APPLY_PED_BLOOD_BY_ZONE,0x8b65f723c8dd11a8,						CommandApplyPedBloodByZone);
		SCR_REGISTER_SECURE(APPLY_PED_BLOOD_SPECIFIC,0x2d0c965c5964fc7a,					CommandApplyPedBloodSpecific);
		
		SCR_REGISTER_UNUSED(APPLY_PED_STAB,0x143e6140799f127d,						CommandApplyPedStab);
		SCR_REGISTER_UNUSED(APPLY_PED_STAB_BY_ZONE,0x5384cc72568d34f7,						CommandApplyPedStabByZone);
		SCR_REGISTER_UNUSED(APPLY_PED_SCAR,0x6390bcf9a503072f,						CommandApplyPedScar);
		SCR_REGISTER_UNUSED(APPLY_PED_DIRT,0xae1dc163a6cb41f8,						CommandApplyPedDirt);
		SCR_REGISTER_SECURE(APPLY_PED_DAMAGE_DECAL,0xff4d465afc8008ba,						CommandApplyPedDamageDecal);
		SCR_REGISTER_SECURE(APPLY_PED_DAMAGE_PACK,0x73830442223bb2dc,						CommandApplyPedDamagePack);

		SCR_REGISTER_SECURE(CLEAR_PED_BLOOD_DAMAGE,0x6da9e81ab6f35ba4,						CommandClearPedBloodDamage);
		SCR_REGISTER_SECURE(CLEAR_PED_BLOOD_DAMAGE_BY_ZONE,0xb954d6528bfadfe8,				CommandClearPedBloodDamageByZone);
		SCR_REGISTER_SECURE(HIDE_PED_BLOOD_DAMAGE_BY_ZONE,0x6d68f0c1ce6b3515,				CommandHidePedBloodDamageByZone);
		SCR_REGISTER_SECURE(CLEAR_PED_DAMAGE_DECAL_BY_ZONE,0xa120eb1498b8bdd6,				CommandClearPedDamageDecalByZone);

		SCR_REGISTER_UNUSED(SET_PED_BLOOD_DAMAGE_CLEAR_INFO,0xd1f33f72bc3e68cc,			CommandSetPedBloodDamageClearInfo);

		SCR_REGISTER_SECURE(GET_PED_DECORATIONS_STATE,0x2eb36d96af8b2d12,					CommandGetPedDecorationsState);
		SCR_REGISTER_SECURE(MARK_PED_DECORATIONS_AS_CLONED_FROM_LOCAL_PLAYER,0xe0d107dcd8d58abf, CommandMarkPedDecorationAsClonedFromLocalPlayer);
		
		SCR_REGISTER_SECURE(CLEAR_PED_WETNESS,0x3393d1b291d1bd1b,							CommandClearPedWetness);
		SCR_REGISTER_SECURE(SET_PED_WETNESS_HEIGHT,0x73ff058ad8630d70,						CommandSetPedWetnessHeight);
		SCR_REGISTER_SECURE(SET_PED_WETNESS_ENABLED_THIS_FRAME,0xb2baf3cfc97cf90b,			CommandSetPedWetnessEnabledThisFrame);
		SCR_REGISTER_UNUSED(SET_PED_WETNESS,0xcfc4d66504665132,								CommandSetPedWetness);

		SCR_REGISTER_SECURE(CLEAR_PED_ENV_DIRT,0xb5da14da03892764,							CommandClearPedEnvDirt);
	
		SCR_REGISTER_SECURE(SET_PED_SWEAT,0xffa07ccce40e3cef,								CommandSetPedSweat);

		SCR_REGISTER_UNUSED(APPLY_PED_DECORATION,0x94548006e958bb04,						CommandApplyPedDecoration);
		SCR_REGISTER_UNUSED(ADD_PED_DECORATION,0x3adfa9b5f9b7741f,							CommandAddPedDecoration);
		SCR_REGISTER_UNUSED(ADD_PED_DECORATION_WITH_ALPHA,0x3a45f1faa67aefff,				CommandAddPedDecorationWithAlpha);
		SCR_REGISTER_SECURE(ADD_PED_DECORATION_FROM_HASHES,0x8a6b325ef96205c9,			CommandAddPedDecorationFromHashes);
		SCR_REGISTER_UNUSED(ADD_PED_DECORATION_FROM_HASHES_WITH_ALPHA,0x7c2fd9f2d5d9b890,	CommandAddPedDecorationFromHashesWithAlpha);
		SCR_REGISTER_SECURE(ADD_PED_DECORATION_FROM_HASHES_IN_CORONA,0x46f1674c56f2edd1,	CommandAddPedDecorationFromHashesInCorona);
		SCR_REGISTER_UNUSED(GET_PED_DECORATION_INDEX,0x4668225794aed6df,					CommandGetPedDecorationIndex);
		SCR_REGISTER_UNUSED(GET_PED_DECORATION_ZONE,0x63736bd94c0f4d3f,					CommandGetPedDecorationZone);
		SCR_REGISTER_SECURE(GET_PED_DECORATION_ZONE_FROM_HASHES,0x56bbaa4009d6694c,		CommandGetPedDecorationZoneFromHashes);
		SCR_REGISTER_UNUSED(ADD_PED_DECORATION_BY_INDEX,0x83e6ae9a0bc6762f,				CommandAddPedDecorationByIndex);
		SCR_REGISTER_UNUSED(GET_PED_DECORATION_INDEX_BY_INDEX,0x9045d4cdc31497ab,			CommandGetPedDecorationIndexByIndex);
		SCR_REGISTER_UNUSED(GET_PED_DECORATION_ZONE_BY_INDEX,0x8fa9eadec9ad11a5,			CommandGetPedDecorationZoneByIndex);
		SCR_REGISTER_SECURE(CLEAR_PED_DECORATIONS,0x2623685d22462f7e,						CommandClearPedDecorations);
		SCR_REGISTER_SECURE(CLEAR_PED_DECORATIONS_LEAVE_SCARS,0xa2ec5e23185c2379,			CommandClearPedDecorationsLeaveScars);
		SCR_REGISTER_UNUSED(CLEAR_PED_SCRIPT_DECORATIONS,0x67e8c96c6978e3d3,				CommandClearPedScriptDecorations);
		SCR_REGISTER_UNUSED(GET_NUM_PED_DECORATION_COLLECTIONS,0x599f9e3aadd8fc85,			CommandGetNumPedDecorationCollections);

		SCR_REGISTER_SECURE(WAS_PED_SKELETON_UPDATED,0x96ba485a3be35be8,					CommandWasPedSkeletonUpdated);
		SCR_REGISTER_SECURE(GET_PED_BONE_COORDS,0x6ea486ff6d815b4b,						CommandGetPedBoneCoords);
		SCR_REGISTER_UNUSED(TOGGLE_NM_BINDPOSE_TASK,0xfab1d98f88be6fca,                    ToggleNMBindposeTask);
		SCR_REGISTER_UNUSED(NM_HEAD_LOOK,0xd22f8cf6904bc96e,								CommandNMHeadLook);
		SCR_REGISTER_SECURE(CREATE_NM_MESSAGE,0xee9b674e13c9129d,							CommandCreateNMMessage);
		SCR_REGISTER_SECURE(GIVE_PED_NM_MESSAGE,0xe95ee348fd0fc5fe,						CommandGivePedNMMessage);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_FLOAT,0x28426b0b7fd09ff7,						CommandSetNMMessageFloat);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_INT,0xcd52d68c49645c23,	            			CommandSetNMMessageInt);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_BOOL,0x6208b806da45f36e,						CommandSetNMMessageBool);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_VEC3,0x56928b1a14d0a2f5,						CommandSetNMMessageVec3);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_INSTANCE_INDEX,0xf1dad29fdfd242cc,				CommandSetNMMessageInstanceIndex);
		SCR_REGISTER_UNUSED(SET_NM_MESSAGE_STRING,0xcebea0716d5b499b,						CommandSetNMMessageString);
		SCR_REGISTER_UNUSED(SET_PED_NM_ANIM_POSE,0x22f60ad831e2d9f6,						CommandSetPedNMAnimPose);
		SCR_REGISTER_UNUSED(GET_PED_NM_FEEDBACK,0x5f67cdeeb8429588,						CommandGetPedNMFeedback);

		SCR_REGISTER_SECURE(ADD_SCENARIO_BLOCKING_AREA,0x2915d98110f23a29,				CommandAddScenarioSpawningBlockingArea);
		SCR_REGISTER_SECURE(REMOVE_SCENARIO_BLOCKING_AREAS,0xec6d4f9c59cda6f5,			CommandRemoveScenarioSpawningBlockingAreas);
		SCR_REGISTER_SECURE(REMOVE_SCENARIO_BLOCKING_AREA,0x7bacbb4c6a7b18b7,			CommandRemoveScenarioBlockingArea);
		SCR_REGISTER_SECURE(SET_SCENARIO_PEDS_SPAWN_IN_SPHERE_AREA,0x90d68b004de537b5,	CommandSetScenarioPedsSpawnInSphereArea);
		SCR_REGISTER_SECURE(DOES_SCENARIO_BLOCKING_AREA_EXISTS,0x3c0cad62938c084b,		CommandDoesScenarioBlockingAreaExists);

		SCR_REGISTER_SECURE(IS_PED_USING_SCENARIO,0xfb9eb8de7a01979a,						CommandIsPedUsingScenario);
		SCR_REGISTER_SECURE(IS_PED_USING_ANY_SCENARIO,0xbff766829e7783c7,					CommandIsPedUsingAnyScenario);
		
		SCR_REGISTER_SECURE(SET_PED_PANIC_EXIT_SCENARIO,0xe1cec2a32b5066ca,				CommandSetPedPanicExitScenario);
		SCR_REGISTER_SECURE(TOGGLE_SCENARIO_PED_COWER_IN_PLACE,0x6f84091d46f93fab,			CommandToggleScenarioPedCowerInPlace);

		SCR_REGISTER_SECURE(TRIGGER_PED_SCENARIO_PANICEXITTOFLEE,0x8845cb81ec32b192,		CommandTriggerPedScenarioPanicExitToFlee);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_PANICEXITTOCOMBAT,0x8a979112b5fcaf34,	CommandTriggerPedScenarioPanicExitToCombat);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_COWARDTHENRESUME,0x05adf5d10205cb82,		CommandTriggerPedScenarioCowardThenResume);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_COWARDTHENEXIT,0xf3e5ca8bfe0f787c,		CommandTriggerPedScenarioCowardThenExit);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_AGGROTHENEXIT,0x327a16cd001bf6c4,		CommandTriggerPedScenarioAggroThenExit);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_SHOCKANIMATION,0x7b9e55a26eb0b0db,		CommandTriggerPedScenarioShockAnimation);
		SCR_REGISTER_UNUSED(TRIGGER_PED_SCENARIO_HEADLOOK,0x87fd0d04d53fbd99,				CommandTriggerPedScenarioHeadLook);

		SCR_REGISTER_SECURE(SET_PED_SHOULD_PLAY_DIRECTED_NORMAL_SCENARIO_EXIT,0xde58df0125df4422, CommandSetPedShouldPlayDirectedNormalScenarioExitOnNextCommand);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_PLAY_NORMAL_SCENARIO_EXIT,0x0bf0ee0b51c31e49,				CommandSetPedShouldPlayNormalScenarioExitOnNextCommand);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_PLAY_IMMEDIATE_SCENARIO_EXIT,0x41fbe02637a70478,			CommandSetPedShouldPlayImmediateScenarioExitOnNextCommand);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_PLAY_FLEE_SCENARIO_EXIT,0x63ac696ee80d33d1,					CommandSetPedShouldPlayFleeScenarioExitOnNextCommand);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_IGNORE_SCENARIO_EXIT_COLLISION_CHECKS,0x9da17163846e32fc,	CommandSetPedIgnoresScenarioExitCollisionChecks);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_IGNORE_SCENARIO_NAV_CHECKS,0x1d30c0c13f529123,				CommandSetPedIgnoresScenarioNavChecks);
		SCR_REGISTER_SECURE(SET_PED_SHOULD_PROBE_FOR_SCENARIO_EXITS_IN_ONE_FRAME,0xdda920211a88c4f5,	CommandSetPedProbesForScenarioExitsInOneFrame);

		SCR_REGISTER_SECURE(IS_PED_GESTURING,0x456470ef2d9f29c9,						CommandIsPedGesturing);

		SCR_REGISTER_UNUSED(SET_FACIAL_IDLE_ANIM,0x0e68c20d239d68c6,					CommandSetFacialIdleAnim);
		SCR_REGISTER_UNUSED(RESET_FACIAL_IDLE_ANIM,0x7b80d958e9d5a3f1,					CommandResetFacialIdleAnim);
		SCR_REGISTER_SECURE(PLAY_FACIAL_ANIM,0x1b7a6d8bcba5f722,						CommandPlayFacialAnim);

		SCR_REGISTER_SECURE(SET_FACIAL_CLIPSET,0x48bbf48957099fcf,						CommandSetFacialClipset);
		SCR_REGISTER_UNUSED(CLEAR_FACIAL_CLIPSET,0xed17c23d9b3a6725,					CommandClearFacialClipset);
		SCR_REGISTER_SECURE(SET_FACIAL_IDLE_ANIM_OVERRIDE,0x4ee98fb70f8a09de,			CommandSetFacialIdleAnimOverride);
		SCR_REGISTER_SECURE(CLEAR_FACIAL_IDLE_ANIM_OVERRIDE,0x12fae55d27f5bc42,		CommandClearFacialIdleAnimOverride);

		SCR_REGISTER_SECURE(SET_PED_CAN_PLAY_GESTURE_ANIMS,0xe54845b0a6c7bcd4,				CommandSetPedCanPlayGestureAnims);
		SCR_REGISTER_SECURE(SET_PED_CAN_PLAY_VISEME_ANIMS,0x902a8647c919e9db,				CommandSetPedCanPlayVisemeAnims);
		SCR_REGISTER_SECURE(SET_PED_IS_IGNORED_BY_AUTO_OPEN_DOORS,0xec77988b5e6b57db,		CommandSetPedIgnoredByAutoOpenDoors);
		SCR_REGISTER_SECURE(SET_PED_CAN_PLAY_AMBIENT_ANIMS,0x2378080c93821600,				CommandSetPedCanPlayAmbientAnims);
		SCR_REGISTER_SECURE(SET_PED_CAN_PLAY_AMBIENT_BASE_ANIMS,0xa3400358eee2b4a2,		CommandSetPedCanPlayAmbientBaseAnims);
		SCR_REGISTER_SECURE(TRIGGER_IDLE_ANIMATION_ON_PED,0xc62d4b2ae5894d0e,				CommandTriggerIdleAnimationOnPed);

		SCR_REGISTER_SECURE(SET_PED_CAN_ARM_IK,0xf268885e0f9e2e0d,						CommandSetPedCanArmIK);
		SCR_REGISTER_UNUSED(SET_PED_CAN_BODY_RECOIL_IK,0xfb985a85c62ef073,				CommandSetPedCanBodyRecoilIK);
		SCR_REGISTER_SECURE(SET_PED_CAN_HEAD_IK,0xd29a05000907be6b,					CommandSetPedCanHeadIK);
		SCR_REGISTER_SECURE(SET_PED_CAN_LEG_IK,0x25080da84575852b,						CommandSetPedCanLegIK);
		SCR_REGISTER_SECURE(SET_PED_CAN_TORSO_IK,0xb0acbed00671fb07,					CommandSetPedCanTorsoIK);
		SCR_REGISTER_SECURE(SET_PED_CAN_TORSO_REACT_IK,0x6f2438ddbd108500,				CommandSetPedCanTorsoReactIK);
		SCR_REGISTER_SECURE(SET_PED_CAN_TORSO_VEHICLE_IK,0x5d9f58d99a08750d,			CommandSetPedCanTorsoVehicleIK);

		SCR_REGISTER_SECURE(SET_PED_CAN_USE_AUTO_CONVERSATION_LOOKAT,0x102d1a546fdb4b6d,		CommandSetPedCanUseAutoConversationLookAt);

		SCR_REGISTER_SECURE(IS_PED_HEADTRACKING_PED,0xb15ef36ad557325b,							CommandIsPedHeadtrackingPed);
		SCR_REGISTER_SECURE(IS_PED_HEADTRACKING_ENTITY,0x60b3c5be23bf3368,							CommandIsPedHeadtrackingEntity);
		SCR_REGISTER_SECURE(SET_PED_PRIMARY_LOOKAT,0xd4a45ce9770040ac,							CommandSetPedPrimaryLookAt);
		SCR_REGISTER_UNUSED(SET_PED_SECONDARY_LOOKAT,0x4fb48af706996507,							CommandSetPedSecondaryLookAt);

		SCR_REGISTER_UNUSED(QUEUE_PED_CLOTH_POSE,0x7300453f520980fd,							CommandQueuePedClothPose);
		SCR_REGISTER_SECURE(SET_PED_CLOTH_PIN_FRAMES,0xd3f21cf0ab2ea898,					CommandSetPedClothPinFrames);
		SCR_REGISTER_UNUSED(SET_PED_CLOTH_POSE,0xcda3e03d21666dcc,							CommandSetPedClothPose);
		SCR_REGISTER_SECURE(SET_PED_CLOTH_PACKAGE_INDEX,0xa8264afa40ddcb5e,				CommandSetPedClothPackageIndex);
		SCR_REGISTER_SECURE(SET_PED_CLOTH_PRONE,0x083b98c1349d122d,						CommandSetPedClothProne);
		SCR_REGISTER_SECURE(SET_PED_CONFIG_FLAG,0xd6a76bab45a4b460,						CommandSetPedConfigFlag);
		SCR_REGISTER_SECURE(SET_PED_RESET_FLAG,0x3e3d339bad67f6f2,							CommandSetPedResetFlag);
		SCR_REGISTER_SECURE(GET_PED_CONFIG_FLAG,0x98c2ae9a4c384cbf,						CommandGetPedConfigFlag);
		SCR_REGISTER_SECURE(GET_PED_RESET_FLAG,0x77850b1d48ae14ad,							CommandGetPedResetFlag);

		SCR_REGISTER_UNUSED(SET_PED_CAN_FLY_THROUGH_WINDSCREEN,0xe0c9483b3dd00c61,			CommandSetPedCanFlyThroughWindscreen);
		SCR_REGISTER_SECURE(SET_PED_GROUP_MEMBER_PASSENGER_INDEX,0x950d2d7ea1bff54c,			CommandSetPedGroupMemberPassengerIndex);

		SCR_REGISTER_SECURE(SET_PED_CAN_EVASIVE_DIVE,0xb83d9f54c7f8c2be,						CommandSetPedCanEvasiveDive);
		SCR_REGISTER_SECURE(IS_PED_EVASIVE_DIVING,0xa39ec0e5800da66e,						CommandIsPedEvasiveDiving);
		SCR_REGISTER_SECURE(SET_PED_SHOOTS_AT_COORD,0x53af46d2ef908a45,						CommandSetPedShootsAtCoord);

		SCR_REGISTER_SECURE(SET_PED_MODEL_IS_SUPPRESSED,0x269268ad5d4ca266,				CommandSetPedModelIsSuppressed);
		SCR_REGISTER_SECURE(STOP_ANY_PED_MODEL_BEING_SUPPRESSED,0xbe5f02ccc204fbec,		CommandStopAnyPedModelBeingSuppressed);
		SCR_REGISTER_UNUSED(SET_PED_MODEL_IS_RESTRICTED,0xc8445e031fcac0da,				CommandSetPedModelIsRestricted);
		SCR_REGISTER_UNUSED(STOP_ANY_PED_MODEL_BEING_RESTRICTED,0xa66c6a56131407c1,		CommandStopAnyPedModelBeingRestricted);

		SCR_REGISTER_SECURE(SET_PED_CAN_BE_TARGETED_WHEN_INJURED,0xb62ee77dcde22747,			CommandSetPedCanBeTargetedWhenInjured);	

		SCR_REGISTER_SECURE(SET_PED_GENERATES_DEAD_BODY_EVENTS,0x1b67ac3e4b05d794,			CommandSetPedGeneratesDeadBodyEvents);
		SCR_REGISTER_SECURE(BLOCK_PED_FROM_GENERATING_DEAD_BODY_EVENTS_WHEN_DEAD,0xea5bfd518c713c37, CommandBlockPedFromGeneratingDeadBodyEventsWhenDead);

		SCR_REGISTER_SECURE(SET_PED_WILL_ONLY_ATTACK_WANTED_PLAYER,0x19399c3895328ec3,		CommandSetPedCanOnlyAttackWantedPlayer);	

		SCR_REGISTER_SECURE(SET_PED_CAN_RAGDOLL_FROM_PLAYER_IMPACT,0x3f990964e4a5669a,		CommandSetPedCanRagdollFromPlayerPedImpact);	

		SCR_REGISTER_SECURE(GIVE_PED_HELMET,0x792d27bee2a1111b,								CommandGivePedHelmet);
		SCR_REGISTER_UNUSED(GIVE_PED_HELMET_WITH_OPTS, 0xbec2729,						CommandGivePedHelmetWithOptions);
		SCR_REGISTER_SECURE(REMOVE_PED_HELMET,0x706235cbf64de7fa,								CommandRemovePedHelmet);
		SCR_REGISTER_UNUSED(PED_TAKE_OFF_HELMET,0x09556acd2918fd0d,								CommandPedTakeOffHelmet);
		SCR_REGISTER_SECURE(IS_PED_TAKING_OFF_HELMET,0xd1d8370ff6457e74,						CommandIsPedTakingOffHelmet);
		SCR_REGISTER_SECURE(SET_PED_HELMET,0xa84812d8d6ee8d32,									CommandSetPedHelmet);
		SCR_REGISTER_SECURE(SET_PED_HELMET_FLAG,0x9ace35042165329f,							CommandSetPedHelmetFlag);
		SCR_REGISTER_SECURE(SET_PED_HELMET_PROP_INDEX,0x18d5b5a3b3d122e3,						CommandSetPedHelmetPropIndex);
		SCR_REGISTER_SECURE(SET_PED_HELMET_VISOR_PROP_INDICES,0x1c031d6376af8dce,				CommandSetPedHelmetVisorPropIndices);
		SCR_REGISTER_SECURE(IS_PED_HELMET_VISOR_UP,0xb5619e4e154aa4e9,							CommandGetIsPedHelmetVisorUp);
		SCR_REGISTER_SECURE(SET_PED_HELMET_TEXTURE_INDEX,0xbc38d693dcd5686b,					CommandSetPedHelmetTextureIndex);
		SCR_REGISTER_SECURE(IS_PED_WEARING_HELMET,0x5540488889ec816a,							CommandIsPedWearingHelmet);
		SCR_REGISTER_SECURE(CLEAR_PED_STORED_HAT_PROP,0x1e35410c0af7b0b7,						CommandClearPedStoredHatProp);
		SCR_REGISTER_SECURE(GET_PED_HELMET_STORED_HAT_PROP_INDEX,0x88fed828c9dfbe76,			CommandGetPedHelmetStoredHatPropIndex);
		SCR_REGISTER_SECURE(GET_PED_HELMET_STORED_HAT_TEX_INDEX,0x2fb3ee2b80255afd,			CommandGetPedHelmetStoredHatTexIndex);
		SCR_REGISTER_SECURE(IS_CURRENT_HEAD_PROP_A_HELMET,0xbd965d343583e7b0,					CommandIsCurrentHeadPropAHelmet);
		SCR_REGISTER_UNUSED(ADD_VEHICLE_HELMET_ASSOCIATION,0x6079c0948eaea3ed,					CommandAddVehicleHelmetAssociation);
		SCR_REGISTER_UNUSED(CLEAR_VEHICLE_HELMET_ASSOCIATION,0x5f65e86c449b21e6,				CommandClearVehicleHelmetAssociation);
		SCR_REGISTER_UNUSED(CLEAR_ALL_VEHICLE_HELMET_ASSOCIATIONS,0xec9f07181e86857b,			CommandClearAllVehicleHelmetAssociations);

		SCR_REGISTER_UNUSED(SET_CHARGE_GOAL_POS_OVERRIDE,0xf1b721825737da78, CommandSetChargeGoalPosOverride);

		SCR_REGISTER_SECURE(SET_PED_TO_LOAD_COVER,0x5d204586f4633d2e,							CommandSetPedToLoadCover);

		SCR_REGISTER_SECURE(SET_PED_CAN_COWER_IN_COVER,0x25e8a1cb4046b782,					CommandSetPedCanCowerInCover);
		SCR_REGISTER_SECURE(SET_PED_CAN_PEEK_IN_COVER,0xe317bcb09675f3f8,						CommandSetPedCanPeekInCover);

		SCR_REGISTER_UNUSED(SET_ALLOW_DUMMY_CONVERSIONS, 0x7f0d7e5b,					SetAllowDummyCoversions);
		SCR_REGISTER_SECURE(SET_PED_PLAYS_HEAD_ON_HORN_ANIM_WHEN_DIES_IN_VEHICLE,0xc8882e8058542f83, CommandSetPedPlaysHeadOnHornAnimWhenDiesInVehicle);

		SCR_REGISTER_SECURE(SET_PED_LEG_IK_MODE,0xd34edb9c1da61c7f,							CommandSetPedLegIkMode);

		SCR_REGISTER_UNUSED(GIVE_PED_FAKE_NETWORK_NAME,0xaa03aa2359951bf3,                    CommandGivePedFakeNetworkName);	
		SCR_REGISTER_UNUSED(REMOVE_FAKE_NETWORK_NAME_FROM_PED,0x0255a3b0bbf0329a,             CommandRemoveFakeNetworkNameFromPed);	

		SCR_REGISTER_SECURE(SET_PED_MOTION_BLUR,0xd6a9b2fae14dd809,							CommandSetPedMotionBlur);

		SCR_REGISTER_SECURE(SET_PED_CAN_SWITCH_WEAPON,0xdea78b2c8dacfe9d,						CommandSetPedCanSwitchWeapon);
		SCR_REGISTER_SECURE(SET_PED_DIES_INSTANTLY_IN_WATER,0x157a3b02d5ded24b,				CommandSetPedDiesInstantlyInWater);

		SCR_REGISTER_UNUSED(SET_PED_CLIMB_ANIM_RATE,0x57288dcaa5d1010b,						CommandSetPedClimbAnimRate);
		SCR_REGISTER_SECURE(SET_LADDER_CLIMB_INPUT_STATE,0x1cf0511efc35d2f8,					CommandSetLadderClimbInputState);

		SCR_REGISTER_UNUSED(IS_PED_IN_SPHERE_AREA_OF_ANY_ENEMY_PEDS,0x5e6af58339f55e67,		CommandIsPedInSphereAreaOfAnyEnemyPeds);

		SCR_REGISTER_SECURE(STOP_PED_WEAPON_FIRING_WHEN_DROPPED,0x3261b00df6170ce0,			CommandStopPedWeaponFiringWhenDropped);
		SCR_REGISTER_UNUSED(STOP_PED_DOING_FALL_OFF_TESTS_WHEN_SHOT,0xcfe2c6dc69bde3bc,		CommandStopPedDoingFallOffTestsWhenShot);

		SCR_REGISTER_SECURE(SET_SCRIPTED_ANIM_SEAT_OFFSET,0x2254bff47f4a5852,					CommandSetScriptedAnimSeatOffset);

		SCR_REGISTER_UNUSED(SET_PED_SKIING,0xf8ef679879412915,								CommandSetPedSkiing);
		SCR_REGISTER_UNUSED(IS_PED_SKIING,0xd2137cb6f2b09df6,									CommandIsPedSkiing);
		//new combat system
		SCR_REGISTER_SECURE(SET_PED_COMBAT_MOVEMENT,0x71b502fc2e129665,						CommandSetPedCombatMovement);
		SCR_REGISTER_SECURE(GET_PED_COMBAT_MOVEMENT,0x88f1bf35930e9b1d,						CommandGetPedCombatMovement);
		SCR_REGISTER_SECURE(SET_PED_COMBAT_ABILITY,0x395228f7a14c6543,						CommandSetPedCombatAbility);
		SCR_REGISTER_UNUSED(GET_PED_COMBAT_ABILITY,0x88534ad73a83cc68,						CommandGetPedCombatAbility);
		SCR_REGISTER_SECURE(SET_PED_COMBAT_RANGE,0x3dab02aac9e9ec02,							CommandSetPedCombatRange);
		SCR_REGISTER_SECURE(GET_PED_COMBAT_RANGE,0x3e5fa83d8cbed3e6,							CommandGetPedCombatRange);
		SCR_REGISTER_SECURE(SET_PED_COMBAT_ATTRIBUTES,0xbc12d08ee7559ccd,						CommandSetPedCombatAttributes);
		SCR_REGISTER_UNUSED(SET_PED_COMBAT_INFO,0x10d97a69d88d734a,						CommandSetPedCombatInfo);
		SCR_REGISTER_SECURE(SET_PED_TARGET_LOSS_RESPONSE,0x7dcb4594acf18da7,					CommandSetPedTargetLossResponse);

		// Stealth system
		SCR_REGISTER_SECURE(IS_PED_PERFORMING_MELEE_ACTION,0x1e6abfb33bdd2a4c,				CommandIsPedPeformingMeleeAction);
		SCR_REGISTER_SECURE(IS_PED_PERFORMING_STEALTH_KILL,0x06ff19e64d0c2827,				CommandIsPedPerformingStealthKill);
		SCR_REGISTER_UNUSED(IS_PED_PERFORMING_A_BLOCK,0xd79e249683ab2e3f,					CommandIsPedPerformingABlock);
		SCR_REGISTER_SECURE(IS_PED_PERFORMING_A_COUNTER_ATTACK,0x7ab6ed2ef9dc2f84,			CommandIsPedPerformingACounterAttack);
		SCR_REGISTER_SECURE(IS_PED_BEING_STEALTH_KILLED,0x4b7e6e16d3345fe8,				CommandIsPedBeingStealthKilled);
		SCR_REGISTER_SECURE(GET_MELEE_TARGET_FOR_PED,0xeba91bc1f5199cbc,					CommandGetMeleeTargetForPed);
		SCR_REGISTER_UNUSED(SET_PED_STEALTH_ATTRIBUTES,0x6c6e2f291d259049,					CommandGetPedStealthAttributes);
		SCR_REGISTER_SECURE(WAS_PED_KILLED_BY_STEALTH,0xe573aca59d8c5c18,					CommandWasPedKilledByStealth);
		SCR_REGISTER_SECURE(WAS_PED_KILLED_BY_TAKEDOWN,0xed41aac94202b65c,					CommandWasPedKilledByTakedown);
		SCR_REGISTER_SECURE(WAS_PED_KNOCKED_OUT,0x9936e299ab236ef3,						CommandWasPedKnockedout);

		// New smart flee system
		SCR_REGISTER_SECURE(SET_PED_FLEE_ATTRIBUTES,0xdc6110e0f8ff9da0,						CommandGetPedFleeAttributes);
		
		// Cowering
		SCR_REGISTER_SECURE(SET_PED_COWER_HASH,0x4f93378a3f279d8b,								CommandSetPedCowerHash);

		SCR_REGISTER_SECURE(SET_PED_STEERS_AROUND_DEAD_BODIES,0xbdf37cc07f3d5f4a,				CommandSetPedSteersAroundDeadBodies);
		SCR_REGISTER_UNUSED(GET_PED_STEERS_AROUND_DEAD_BODIES,0x25de1c2bcae4b2fc,				CommandGetPedSteersAroundDeadBodies);
		SCR_REGISTER_SECURE(SET_PED_STEERS_AROUND_PEDS,0x2d1abdbe834c4ac4,						CommandSetPedSteersAroundPeds);
		SCR_REGISTER_UNUSED(GET_PED_STEERS_AROUND_PEDS,0xc2be33a6830ea241,						CommandGetPedSteersAroundPeds);
		SCR_REGISTER_SECURE(SET_PED_STEERS_AROUND_OBJECTS,0xc3be2917fbb1a384,					CommandSetPedSteersAroundObjects);
		SCR_REGISTER_UNUSED(GET_PED_STEERS_AROUND_OBJECTS,0x3a1c0f4baf8495b8,					CommandGetPedSteersAroundObjects);
		SCR_REGISTER_SECURE(SET_PED_STEERS_AROUND_VEHICLES,0xf290300c9587ee4e,					CommandSetPedSteersAroundVehicles);
		SCR_REGISTER_UNUSED(GET_PED_STEERS_AROUND_VEHICLES,0x2fc9b4660e7f0b98,					CommandGetPedSteersAroundVehicles);
		SCR_REGISTER_SECURE(SET_PED_IS_AVOIDED_BY_OTHERS,0x252a55826444c3cb,					CommandSetPedIsAvoidedByOthers);
		SCR_REGISTER_UNUSED(GET_PED_IS_AVOIDED_BY_OTHERS,0x3550e29ace9303be,					CommandGetPedIsAvoidedByOthers);
		SCR_REGISTER_SECURE(SET_PED_INCREASED_AVOIDANCE_RADIUS,0x963e9ab0c31726bd,				CommandPedSetIncreasedAvoidanceRadius);
		SCR_REGISTER_SECURE(SET_PED_BLOCKS_PATHING_WHEN_DEAD,0x5e3ad67f25700d47,				CommandPedSetBlocksPathingWhenDead);

		SCR_REGISTER_SECURE(SET_PED_NO_TIME_DELAY_BEFORE_SHOT,0x1212e12d9c1e55d1,				CommandPedSetNoTimeDelayBeforeShot);		

		SCR_REGISTER_UNUSED(SET_PED_IGNORED_MATERIAL_COLLISION_WEAPON,0xe7577fdde41ceeb2,		CommandSetPedIgnoredMaterialCollisionWeapon);
		SCR_REGISTER_UNUSED(SET_PED_NEVER_FALL_OFF_SKIS,0xcf6ae3aea07a1259,						CommandSetPedNeverFallOffSkis);

		SCR_REGISTER_SECURE(IS_ANY_PED_NEAR_POINT,0xac48ff26faaa8dd0,							CommandIsAnyPedNearPoint);

		SCR_REGISTER_SECURE(FORCE_PED_AI_AND_ANIMATION_UPDATE,0xeb087b19f63ab053,				CommandForcePedAIAndAnimationUpdate);
		SCR_REGISTER_UNUSED(GET_PED_AGE,0x466e9f5f2407358c,										CommandGetPedAge);
		SCR_REGISTER_SECURE(IS_PED_HEADING_TOWARDS_POSITION,0xc9cd8c710046a3dd,					CommandIsPedHeadingTowardsPosition); 

		SCR_REGISTER_UNUSED(REQUEST_PED_HIGH_DETAIL_MODEL,0x733ab0db2108c3eb,					CommandRequestPedHighDetailModel); 
		SCR_REGISTER_UNUSED(REMOVE_PED_HIGH_DETAIL_MODEL,0x5a52fd251bea57a8,					CommandRemovePedHighDetailModel); 

		SCR_REGISTER_SECURE(REQUEST_PED_VISIBILITY_TRACKING,0xad3e09d1957bb0de,					CommandRequestPedVisibilityTracking);
		SCR_REGISTER_SECURE(REQUEST_PED_VEHICLE_VISIBILITY_TRACKING,0x757e8acdd0fcb0ca,			CommandRequestPedsVehicleVisibilityTracking);
		SCR_REGISTER_SECURE(REQUEST_PED_RESTRICTED_VEHICLE_VISIBILITY_TRACKING,0xe524a5d3744ad3ba, CommandRequestPedsUseRestrictedVehicleVisibilityTracking);
		SCR_REGISTER_SECURE(REQUEST_PED_USE_SMALL_BBOX_VISIBILITY_TRACKING,0x677c4c455bb319ca,	CommandRequestPedsHnSBoundingBox);

		SCR_REGISTER_SECURE(IS_TRACKED_PED_VISIBLE,0x2cf76e57d8e46c1f,							CommandIsTrackedPedVisible);
		SCR_REGISTER_SECURE(GET_TRACKED_PED_PIXELCOUNT,0x2596d8147860a4f4,						CommandGetTrackedPedPixelCount);
		SCR_REGISTER_SECURE(IS_PED_TRACKED,0xee1b31f257c88ef0,									CommandIsPedTracked);

		SCR_REGISTER_SECURE(HAS_PED_RECEIVED_EVENT,0xd16a3d24df85c222,							HasPedReceivedEvent);
		SCR_REGISTER_SECURE(CAN_PED_SEE_HATED_PED,0x7e006f2e24f03bb4,							CommandCanPedSeeHatedPed);
		SCR_REGISTER_SECURE(CAN_PED_SHUFFLE_TO_OR_FROM_TURRET_SEAT,0x127ebfada96eb575,			CommandCanPedShuffleToOrFromTurretSeat);
		SCR_REGISTER_SECURE(CAN_PED_SHUFFLE_TO_OR_FROM_EXTRA_SEAT,0x7aed27c1f71c9044,			CommandCanPedShuffleToOrFromExtraSeat);

		SCR_REGISTER_SECURE(GET_PED_BONE_INDEX,0xb897fcfcfa664b38,								CommandGetPedBoneIndex);
		SCR_REGISTER_SECURE(GET_PED_RAGDOLL_BONE_INDEX,0xefccfe881e0da85b,						CommandGetPedRagdollBoneIndex);

		SCR_REGISTER_UNUSED(SET_PED_STUBBLE,0x1d763324abb92e4e,									CommandSetPedStubble );

		SCR_REGISTER_SECURE(SET_PED_ENVEFF_SCALE,0xebc16ef31fff5b24,							CommandSetPedEnvEffScale);
		SCR_REGISTER_SECURE(GET_PED_ENVEFF_SCALE,0x209d59a7a05905eb,							CommandGetPedEnvEffScale);
		SCR_REGISTER_SECURE(SET_ENABLE_PED_ENVEFF_SCALE,0xa1f4f300bfa563e3,						CommandSetEnablePedEnvEffScale);
		SCR_REGISTER_UNUSED(GET_ENABLE_PED_ENVEFF_SCALE,0x4f021a1652208ff9,						CommandGetEnablePedEnvEffScale);
		SCR_REGISTER_SECURE(SET_PED_ENVEFF_CPV_ADD,0xebff74dac1fa7525,							CommandSetPedEnvEffCpvAdd);
		SCR_REGISTER_UNUSED(GET_PED_ENVEFF_CPV_ADD,0x533fbb53a5e3f23e,							CommandGetPedEnvEffCpvAdd);

		SCR_REGISTER_SECURE(SET_PED_ENVEFF_COLOR_MODULATOR,0x7a0079d67714b00e,					CommandSetPedEnvEffColorModulator);
		SCR_REGISTER_UNUSED(SET_PED_ENVEFF_COLOR_MODULATORF,0x7f23f36c38af1d68,					CommandSetPedEnvEffColorModulatorf);
		SCR_REGISTER_UNUSED(GET_PED_ENVEFF_COLOR_MODULATOR_R,0x140f22200d3130d9,				CommandGetPedEnvEffColorModulatorR);
		SCR_REGISTER_UNUSED(GET_PED_ENVEFF_COLOR_MODULATOR_G,0x32237e85aedc1dac,				CommandGetPedEnvEffColorModulatorG);
		SCR_REGISTER_UNUSED(GET_PED_ENVEFF_COLOR_MODULATOR_B,0xf9b5481e28d915fb,				CommandGetPedEnvEffColorModulatorB);

		SCR_REGISTER_SECURE(SET_PED_EMISSIVE_SCALE,0x786b3db2091c710c,							CommandSetPedEmissiveScale);
		SCR_REGISTER_SECURE(GET_PED_EMISSIVE_SCALE,0x5d0b188bde0b8a58,							CommandGetPedEmissiveScale);
		SCR_REGISTER_SECURE(IS_PED_SHADER_READY,0x0c18153716ffbf62, 							CommandIsPedShaderReady);

		SCR_REGISTER_SECURE(SET_PED_ENABLE_CREW_EMBLEM,0x96bd153fbb9efd5a,						CommandSetPedEnableCrewEmblem);
		SCR_REGISTER_UNUSED(GET_PED_ENABLE_CREW_EMBLEM,0x0ccfd47a3ccb0feb,						CommandGetPedEnableCrewEmblem);

		SCR_REGISTER_UNUSED(GET_POSITION_TO_SIDE_OF_PED,0x36c1f30b1c0529cc,						CommandGetPositionToSideOfPed);

		SCR_REGISTER_SECURE(REQUEST_RAGDOLL_BOUNDS_UPDATE,0xa265face1aed5794,					CommandRequestRagdollBoundsUpdate);

		SCR_REGISTER_UNUSED(IS_PED_AO_BLOB_RENDERING_ENALBED       ,0x4790f1cadb600e61,			CommandIsPedAoBlobRenderingEnabled);
		SCR_REGISTER_SECURE(SET_PED_AO_BLOB_RENDERING              ,0x450e7898d3f6649d,			CommandSetPedAoBlobRendering);

		SCR_REGISTER_SECURE(IS_PED_SHELTERED,0xbaecaaa18de8809e,								CommandIsPedSheltered);

		//////////////////////////////////////////////////////////////////////////
		//	Synchronized scene commands
		//	GSALES - These really aren't only to do with peds. 
		//	maybe there should be a commands_anim file?
		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(CREATE_SYNCHRONIZED_SCENE,0x8f5f4164bf5fb513,			CommandCreateSynchronizedScene); 
		SCR_REGISTER_SECURE(CREATE_SYNCHRONIZED_SCENE_AT_MAP_OBJECT,0x5bd4525ef4255c92,			CommandCreateSynchronizedSceneRelativeToMapObject); 
		SCR_REGISTER_SECURE(IS_SYNCHRONIZED_SCENE_RUNNING,0xce2c78e9fc0b01e3,			CommandIsSynchronizedSceneRunning);
		SCR_REGISTER_SECURE(SET_SYNCHRONIZED_SCENE_ORIGIN,0x44b158699315d639,			CommandSetSynchronizedSceneOrigin);
		SCR_REGISTER_SECURE(SET_SYNCHRONIZED_SCENE_PHASE,0xe0f81eaeb49d39ef,			CommandSetSynchronizedScenePhase);
		SCR_REGISTER_SECURE(GET_SYNCHRONIZED_SCENE_PHASE,0x54de1614490c2a83,			CommandGetSynchronizedScenePhase);
		SCR_REGISTER_SECURE(SET_SYNCHRONIZED_SCENE_RATE,0x7541cccb5b62c54d,			CommandSetSynchronizedSceneRate);
		SCR_REGISTER_SECURE(GET_SYNCHRONIZED_SCENE_RATE,0x75408ae37923c89b,			CommandGetSynchronizedSceneRate);
		SCR_REGISTER_UNUSED(SET_SYNCHRONIZED_SCENE_ABSOLUTE,0x6f106c1fa49420d0,			CommandSetSynchronizedSceneAbsolute);
		SCR_REGISTER_UNUSED(IS_SYNCHRONIZED_SCENE_ABSOLUTE,0x6d9a34e5a8c709d4,			CommandIsSynchronizedSceneAbsolute);
		SCR_REGISTER_SECURE(SET_SYNCHRONIZED_SCENE_LOOPED,0xfe1cd80c2f7cbcfb,			CommandSetSynchronizedSceneLooped);
		SCR_REGISTER_SECURE(IS_SYNCHRONIZED_SCENE_LOOPED,0x67596c52bc17fb9c,			CommandIsSynchronizedSceneLooped);
		SCR_REGISTER_SECURE(SET_SYNCHRONIZED_SCENE_HOLD_LAST_FRAME,0x0ba4916484503b63,			CommandSetSynchronizedSceneHoldLastFrame);
		SCR_REGISTER_SECURE(IS_SYNCHRONIZED_SCENE_HOLD_LAST_FRAME,0x1376a84d874169c3,			CommandIsSynchronizedSceneHoldLastFrame);
		SCR_REGISTER_SECURE(ATTACH_SYNCHRONIZED_SCENE_TO_ENTITY,0x961ee84414ca42f8,			CommandAttachSynchronizedSceneToEntity);
		SCR_REGISTER_SECURE(DETACH_SYNCHRONIZED_SCENE,0x7ff9609f5d8471af,			CommandDetachSynchronizedScene);
		SCR_REGISTER_SECURE(TAKE_OWNERSHIP_OF_SYNCHRONIZED_SCENE,0x700b04835b2a30d1,			CommandTakeOwnershipSynchronizedScene);

		//////////////////////////////////////////////////////////////////////////
		//	Ped motion task commands
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(FORCE_PED_MOTION_STATE,0xa8685678dda27fc3,			CommandForcePedMotionState);
		SCR_REGISTER_SECURE(GET_PED_CURRENT_MOVE_BLEND_RATIO,0x503ecec9e9e2fa90,			CommandGetPedCurrentMoveBlendRatio);
		SCR_REGISTER_SECURE(SET_PED_MAX_MOVE_BLEND_RATIO,0xcac4d1050f2e6899,			CommandSetPedMaxMoveBlendRatio);
		SCR_REGISTER_SECURE(SET_PED_MIN_MOVE_BLEND_RATIO,0x3dbd501d46e6a2be,			CommandSetPedMinMoveBlendRatio);		

		//////////////////////////////////////////////////////////////////////////
		//	Ped run/sprint speed override
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SET_PED_MOVE_RATE_OVERRIDE,0x3df902167668dd74,			CommandSetDesiredPedMoveRateOverride);
		SCR_REGISTER_SECURE(SET_PED_MOVE_RATE_IN_WATER_OVERRIDE,0xec503ac575ac80a0,	CommandSetDesiredPedMoveRateInWaterOverride);

		SCR_REGISTER_SECURE(PED_HAS_SEXINESS_FLAG_SET,0x0ad88be2af6bde41,			CommandPedHasSexinessFlagSet);
		
		// Access to ped scanners
		SCR_REGISTER_SECURE(GET_PED_NEARBY_VEHICLES,0x9a39844d6003635c, CommandGetNearbyVehicles);
		SCR_REGISTER_SECURE(GET_PED_NEARBY_PEDS,0x46951d3186547c7a, CommandGetNearbyPeds);

		SCR_REGISTER_SECURE(HAVE_ALL_STREAMING_REQUESTS_COMPLETED,0x6090fc735660b8f7,			CommandHaveAllStreamingRequestsCompleted);

		SCR_REGISTER_SECURE(IS_PED_USING_ACTION_MODE,0x80eade020a9bb643,			CommandIsPedUsingActionMode);
		SCR_REGISTER_UNUSED(IS_PED_WANTING_TO_USE_ACTION_MODE,0x93408490f9f2186c,			CommandIsPedWantingToUseUseActionMode);
		SCR_REGISTER_SECURE(SET_PED_USING_ACTION_MODE,0xa091c233f9d0ab04,			CommandSetPedUsingActionMode);
		SCR_REGISTER_SECURE(SET_MOVEMENT_MODE_OVERRIDE,0xa6b0a6490241fec8,			CommandSetMovementModeOverride);

		SCR_REGISTER_SECURE(SET_PED_CAPSULE,0x54e818a233b540d4,					CommandSetPedCapsule);

		//////////////////////////////////////////////////////////////////////////
		//	Ped headshots commands
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(REGISTER_PEDHEADSHOT,0x5ccf05d0032b9864,				CommandRegisterPedHeadshot);
		SCR_REGISTER_SECURE(REGISTER_PEDHEADSHOT_HIRES,0x5f8d7e19cfca0e80,			CommandRegisterPedHeadshotHiRes);
		SCR_REGISTER_SECURE(REGISTER_PEDHEADSHOT_TRANSPARENT,0x425aeaa424677e5f,	CommandRegisterPedHeadshotWithTransparentBackground);
		SCR_REGISTER_SECURE(UNREGISTER_PEDHEADSHOT,0x326ed9dd64134d76,				CommandUnregisterPedHeadshot);

		SCR_REGISTER_SECURE(IS_PEDHEADSHOT_VALID,0x87af9112820eec25,			CommandIsPedHeadshotValid);
		SCR_REGISTER_SECURE(IS_PEDHEADSHOT_READY,0x1b28b340344ba310,			CommandIsPedHeadshotReady);
		SCR_REGISTER_UNUSED(GET_PEDHEADSHOT_TXD_HASH,0x7abf96207fedef17,			CommandGetPedHeadshotTxdHash);
		SCR_REGISTER_SECURE(GET_PEDHEADSHOT_TXD_STRING,0xe1afe5e1e834bbf0,			CommandGetPedHeadshotTxdString);

		SCR_REGISTER_UNUSED(SET_PEDHEADSHOT_CUSTOM_LIGHTING,0xd0b64552a1b13848,			CommandSetPedHeadshotCustomLighting);
		SCR_REGISTER_UNUSED(SET_PEDHEADSHOT_CUSTOM_LIGHT,0x3420adf7eec16ee6,			CommandSetPedHeadshotCustomLight);

		SCR_REGISTER_UNUSED(CAN_REQUEST_POSE_FOR_HEADSHOT,0x29202f178fe55007,			CommandCanRequestPoseForHeadshot);
		SCR_REGISTER_UNUSED(REGISTER_PEDHEADSHOT_WITH_ANIM_POSE,0xde92a99cf1b6aa7b,			CommandRegisterPedHeadshotWithAnimPose);

		SCR_REGISTER_UNUSED(GET_PEDHEADSHOT_TRANSPARENT_HANDLE,0x81f0491d7b7ef37c,			CommandGetPedHeadshotWithTransparentBackground);

		SCR_REGISTER_UNUSED(GET_PEDHEADSHOT_STATE,0xfd22cbd033fd06ee,			CommandGetPedHeadshotState);

		SCR_REGISTER_SECURE(REQUEST_PEDHEADSHOT_IMG_UPLOAD,0xcd92583b7ca695d6,			CommandRequestPedHeadshotTextureUpload);
		SCR_REGISTER_SECURE(RELEASE_PEDHEADSHOT_IMG_UPLOAD,0x6decc60e53a8ae80,			CommandReleasePedHeadshotTextureUploadRequest);
		SCR_REGISTER_SECURE(IS_PEDHEADSHOT_IMG_UPLOAD_AVAILABLE,0x18e910b44032efdc,			CommandIsPedHeadshotTextureUploadAvailable);
		SCR_REGISTER_SECURE(HAS_PEDHEADSHOT_IMG_UPLOAD_FAILED,0xa855c0f222fa2c15,			CommandHasPedHeadshotTextureUploadFailed);
		SCR_REGISTER_SECURE(HAS_PEDHEADSHOT_IMG_UPLOAD_SUCCEEDED,0xa2ecd66b130e8c24,			CommandHasPedHeadshotTextureUploadSucceeded);

		//////////////////////////////////////////////////////////////////////////
		// Ped heat scale commands
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SET_PED_HEATSCALE_OVERRIDE,0x2588ae73a17d656d,			CommandSetPedHeatScaleOverride);		
		SCR_REGISTER_SECURE(DISABLE_PED_HEATSCALE_OVERRIDE,0x79ebf0add4de0aee,			CommandDisablePedHeatScaleOverride);


		//////////////////////////////////////////////////////////////////////////
		// Asynchronous in-game opponent spawnpoint generation
		// An alternative to CNetworkRespawnMgr, suitable for use during gameplay
		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(SPAWNPOINTS_START_SEARCH,0xc9c2f7b876ee4e15,			CommandSpawnPointsStartSearch);
		SCR_REGISTER_SECURE(SPAWNPOINTS_START_SEARCH_IN_ANGLED_AREA,0xc9aca2e149098784,			CommandSpawnPointsStartSearchInAngledArea);
		SCR_REGISTER_SECURE(SPAWNPOINTS_CANCEL_SEARCH,0x3d9cb48b182f9f3b,			CommandSpawnPointsCancelSearch);
		SCR_REGISTER_SECURE(SPAWNPOINTS_IS_SEARCH_ACTIVE,0xccd530822438fea4,			CommandSpawnPointsIsSearchActive);
		SCR_REGISTER_SECURE(SPAWNPOINTS_IS_SEARCH_COMPLETE,0xf4408128c21a71b1,			CommandSpawnPointsIsSearchComplete);
		SCR_REGISTER_SECURE(SPAWNPOINTS_IS_SEARCH_FAILED,0xd41858be1441f8a7,			CommandSpawnPointsIsSearchFailed);
		SCR_REGISTER_SECURE(SPAWNPOINTS_GET_NUM_SEARCH_RESULTS,0x040e56a95fcfb423,			CommandSpawnPointsGetNumResults);
		SCR_REGISTER_SECURE(SPAWNPOINTS_GET_SEARCH_RESULT,0x27b82f0c405755b0,			CommandSpawnPointGetResult);
		SCR_REGISTER_SECURE(SPAWNPOINTS_GET_SEARCH_RESULT_FLAGS,0x4abaf316add9b9ea,			CommandSpawnPointGetResultFlags);

		//////////////////////////////////////////////////////////////////////////
		// IK
		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(SET_IK_TARGET,0x6c600c7a5953076d,			CommandSetIkTarget);
		SCR_REGISTER_SECURE(FORCE_INSTANT_LEG_IK_SETUP,0x02deecbab9737834,			CommandForceInstantLegIkSetup);

		SCR_REGISTER_SECURE(REQUEST_ACTION_MODE_ASSET,0xf103763065059379,			CommandRequestActionModeAsset);
		SCR_REGISTER_SECURE(HAS_ACTION_MODE_ASSET_LOADED,0xa079c1c6fa6675c2,			CommandHasActionModeAssetLoaded);
		SCR_REGISTER_SECURE(REMOVE_ACTION_MODE_ASSET,0x6cd5e12a8e5d0092,			CommandRemoveActionModeAsset);
		SCR_REGISTER_SECURE(REQUEST_STEALTH_MODE_ASSET,0x5a368832ae432fdf,			CommandRequestStealthModeAsset);
		SCR_REGISTER_SECURE(HAS_STEALTH_MODE_ASSET_LOADED,0x80a1036f74ca9090,			CommandHasStealthModeAssetLoaded);
		SCR_REGISTER_SECURE(REMOVE_STEALTH_MODE_ASSET,0x2ce67ce5cc29d977,			CommandRemoveStealthModeAsset);

		SCR_REGISTER_SECURE(SET_PED_LOD_MULTIPLIER,0x9d4d63eb83a103ad,			CommandSetPedLodMultiplier);

		SCR_REGISTER_SECURE(SET_PED_CAN_LOSE_PROPS_ON_DAMAGE,0x9b78b16ac295162f,			CommandSetPedCanLosePropsOnDamage);

		SCR_REGISTER_SECURE(SET_FORCE_FOOTSTEP_UPDATE,0x66136f80d9c9251e,			CommandForceFootstepHelperUpdate);
		SCR_REGISTER_SECURE(SET_FORCE_STEP_TYPE,0x2bc29bbcb216ba16,			CommandForceStepType);
		

		SCR_REGISTER_SECURE(IS_ANY_HOSTILE_PED_NEAR_POINT,0x3f9ca94b386e2f8c,	        CommandIsAnyHostilePedNearPoint);

		SCR_REGISTER_SECURE(SET_PED_CAN_PLAY_IN_CAR_IDLES,0xf1ee2e7dd05f08dc,         CommandSetPedCanPlayInCarIdles);
		SCR_REGISTER_SECURE(IS_TARGET_PED_IN_PERCEPTION_AREA,0x3fff32ad6436ad9f,			CommandIsTargetPedInPerceptionArea);
		SCR_REGISTER_SECURE(SET_POP_CONTROL_SPHERE_THIS_FRAME,0x940745265434eb5f,         CommandSetUsePopControlSphereThisFrame);
		SCR_REGISTER_UNUSED(CLEAR_POP_CONTROL_SPHERE,0x74551ccd0732f68e,         CommandClearPopControlSphere);

        SCR_REGISTER_UNUSED(DOES_PED_EXIST_WITH_DECORATOR,0x2fb967a06b4c6b19,         CommandDoesPedExistWithDecorator);

		SCR_REGISTER_SECURE(FORCE_ZERO_MASS_IN_COLLISIONS,0xf993e519fbd2cce5,         CommandForcePedToHaveZeroMassInCollisions);
		SCR_REGISTER_SECURE(SET_DISABLE_HIGH_FALL_DEATH,0xc360b1054ef12239,         CommandSetDisableHighFallDeath);

		SCR_REGISTER_SECURE(SET_PED_PHONE_PALETTE_IDX,0x3ae8c8ace275db8a,	CommandSetPedPhonePaletteIdx);
		SCR_REGISTER_UNUSED(GET_PED_PHONE_PALETTE_IDX,0xd17d07956683ad32,	CommandGetPedPhonePaletteIdx);

		SCR_REGISTER_SECURE(SET_PED_STEER_BIAS,0x75f04b33f318bee9,			CommandSetPedSteerBias);

		SCR_REGISTER_SECURE(IS_PED_SWITCHING_WEAPON,0x01c84186537abb55,	CommandIsPedSwitchingWeapon);

		SCR_REGISTER_UNUSED(GET_DLC_PACK_HASH_FOR_COMPONENT,0xa99a911f1e10dbbc,	CommandGetDLCPackHashForComponent);
		SCR_REGISTER_UNUSED(GET_DLC_PACK_HASH_FOR_PROP,0xbed95ec44fb30a72,	CommandGetDLCPackHashForProp);

		SCR_REGISTER_SECURE(SET_PED_TREATED_AS_FRIENDLY,0xe57c0a41f2dd0d21,	CommandSetPedTreatedAsFriendly);

		SCR_REGISTER_SECURE(SET_DISABLE_PED_MAP_COLLISION,0xabfefe3c20be7aad,	CommandSetDisablePedMapCollision);

		SCR_REGISTER_SECURE(ENABLE_MP_LIGHT,0xc5fca9a6790497d0,	CommandEnableMpLight);
		SCR_REGISTER_SECURE(GET_MP_LIGHT_ENABLED,0xc6c09f0963b44ae5,	CommandGetMpLightEnabled);

		SCR_REGISTER_UNUSED(ENABLE_FOOT_LIGHT,0x8b753f845c13e999, CommandEnableFootLight);
		SCR_REGISTER_UNUSED(GET_FOOT_LIGHT_ENABLED,0x94a3e2d79730ae5c, CommandGetFootLightEnabled);

		SCR_REGISTER_SECURE(CLEAR_COVER_POINT_FOR_PED,0x619b6eff5ae09b17, CommandClearCoverPoint);

		SCR_REGISTER_SECURE(SET_ALLOW_STUNT_JUMP_CAMERA,0x5c3ba1efa5ef174d, CommandSetAllowStuntJumpCamera);
	}

}	//	end of namespace ped_commands
