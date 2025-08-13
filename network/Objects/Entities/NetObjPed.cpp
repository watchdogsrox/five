//
// name:        NetObjPed.cpp
// description: Derived from netObject, this class handles all ped-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjPed.h"

// rage headers
#include "spatialdata/aabb.h"

// framework headers
#include "grcore/debugdraw.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwnet/netlogutils.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "system/param.h"

#if !__FINAL
#include "system/stack.h"
#endif

// game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "camera/CamInterface.h"
#include "event/decision/EventDecisionMaker.h"
#include "event/EventDamage.h"
#include "event/EventWeapon.h"
#include "network/Debug/NetworkDebug.h"
#include "network/NetworkInterface.h"
#include "network/arrays/NetworkArrayHandlerTypes.h"
#include "network/arrays/NetworkArrayMgr.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkDamageTracker.h"
#include "network/general/NetworkSynchronisedScene.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "Network/Players/NetGamePlayer.h"
#include "peds/Horse/horseComponent.h"
#include "peds/Ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIKSettings.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "peds/PedTaskRecord.h"
#include "peds/pedpopulation.h"
#include "peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "peds/rendering/PedVariationPack.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/handlers/GameScriptIds.h"
#include "streaming/streaming.h"			// For CStreaming::HasObjectLoaded(), etc.
#include "task/combat/TaskWrithe.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskWander.h"
#include "task/General/Phone/TaskMobilePhone.h"
#include "task/General/TaskBasic.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"
#include "Task/motion/locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "task/Motion/Locomotion/TaskMotionTennis.h"
#include "task/Movement/Climbing/TaskVault.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Physics/TaskNM.h"
#include "task/Physics/TaskNMHighFall.h"
#include "task/Physics/TaskNMRelax.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/ScenarioPointRegion.h"
#include "task/System/TaskClassInfo.h"
#include "task/System/TaskFSMClone.h"
#include "task/System/TaskTreeClone.h"
#include "task/System/TaskTreePed.h"
#include "Task/System/MotionTaskData.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "task/Response/TaskFlee.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Peds/PedGeometryAnalyser.h"
#include "PedGroup/PedGroup.h"
#include "script/script.h"
#include "text/text.h"
#include "text/TextConversion.h"
#include "weapons/components/WeaponComponentClip.h"
#include "weapons/gadgets/Gadget.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/weapondamage.h"
#include "VehicleAi/task/TaskVehiclePlayer.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/VehicleGadgets.h"
#include "physics/RagdollConstraints.h"
#include "Vehicles/Metadata/VehicleEnterExitFlags.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "shaders/CustomShaderEffectPed.h"
#include "task/Combat/TaskDamageDeath.h"
#include "Stats/StatsMgr.h"
#include "animation/FacialData.h"
#include "Task/General/TaskSecondary.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "peds/pedhelmetcomponent.h"
#include "Peds/NavCapabilities.h"

#if !__NO_OUTPUT
#include "renderer/PostScan.h"
#endif

NETWORK_OPTIMISATIONS()

#if 0
	#define NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT(...)	if(GetPed() && GetPed()->IsPlayer() && GetPed()->IsNetworkClone()) \
														{\
															Displayf("Frame %d : %s : Ped %p %s", fwTimer::GetFrameCount(), __FUNCTION__, GetPed(), GetPed() ? GetPed()->GetDebugName() : "NO PED"); Displayf(__VA_ARGS__);\
														}
#else
	#define NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT(...)
#endif /* __BANK */

PARAM(nodeadpedremoval, "Disable automatic removal of dead peds left behind.");

#if __BANK
s32 CNetObjPed::ms_updateRateOverride = -1;
#endif

// this is declared in speechaudio entity
extern const u32 g_NoVoiceHash;

static const unsigned MAX_TENNIS_MOTION_DATA = 8;

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CNetObjPed, MAX_NUM_NETOBJPEDS+MAX_NUM_NETOBJPLAYERS, 0.20f, atHashString("CNetObjPed",0x8960845d), LARGEST_NETOBJPED_CLASS);
FW_INSTANTIATE_CLASS_POOL(CNetObjPed::CPedSyncData, MAX_NUM_NETOBJPEDS, atHashString("CPedSyncData",0x1952e795));
FW_INSTANTIATE_CLASS_POOL(CNetObjPed::CNetTennisMotionData, MAX_TENNIS_MOTION_DATA, atHashString("CNetObjPed::CNetTennisMotionData",0x81c23a42));

CPedBlenderDataInWater                  s_pedBlenderDataInWater;
CPedBlenderDataInAir                    s_pedBlenderDataInAir;
CPedBlenderDataTennis                   s_pedBlenderDataTennis;
CPedBlenderDataOnFoot                   s_pedBlenderDataOnFoot;
CPedBlenderDataFirstPersonMode          s_pedBlenderDataFirstPerson;
CPedBlenderDataHiddenUnderMapForVehicle s_pedBlenderDataHiddenUnderMapForVehicle;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataInWater                  = &s_pedBlenderDataInWater;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataInAir                    = &s_pedBlenderDataInAir;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataTennis                   = &s_pedBlenderDataTennis;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataOnFoot                   = &s_pedBlenderDataOnFoot;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataFirstPerson              = &s_pedBlenderDataFirstPerson;
CPedBlenderData *CNetObjPed::ms_pedBlenderDataHiddenUnderMapForVehicle = &s_pedBlenderDataHiddenUnderMapForVehicle;

CNetObjPed::CPedScopeData       CNetObjPed::ms_pedScopeData;
CPedSyncTree                   *CNetObjPed::ms_pedSyncTree;

const float CNetObjPed::MAX_STANDING_OBJECT_OFFSET_POS    = 40.0f;
const float CNetObjPed::MAX_STANDING_OBJECT_OFFSET_HEIGHT = 20.0f;

ObjectId CNetObjPed::ms_PedsNotRespondingToCloneBullets[MAX_NUM_NETOBJPEDS];
unsigned CNetObjPed::ms_NumPedsNotRespondingToCloneBullets         = 0;
unsigned CNetObjPed::ms_PedsNotRespondingToCloneBulletsUpdateFrame = 0;
bool CNetObjPed::ms_playerMaxHealthPendingLastDamageEventOverride = false;

// ================================================================================================================
// CNetObjPed
// ================================================================================================================

CNetObjPed::CNetObjPed(CPed							*ped,
                       const NetworkObjectType		type,
                       const ObjectId				objectID,
                       const PhysicalPlayerIndex    playerIndex,
                       const NetObjFlags			localFlags,
                       const NetObjFlags			globalFlags) :
CNetObjPhysical(ped, type, objectID, playerIndex, localFlags, globalFlags),
m_bAllowSetTask(!IsClone()),
m_CanAlterPedInventoryData(false),
m_bUsingRagdoll(false),
m_weaponObjectExists(false),
m_weaponObjectVisible(false),
m_weaponObjectAttachLeft(false),
m_inVehicle(false),
m_onMount(false),
m_pendingMotionSetId(CLIP_SET_ID_INVALID),
m_pendingOverriddenWeaponSetId(CLIP_SET_ID_INVALID),
m_pendingOverriddenStrafeSetId(CLIP_SET_ID_INVALID),
m_PendingMoveBlenderState(INVALID_PENDING_MOTION_STATE),
m_PendingMoveBlenderType(INVALID_PENDING_MOTION_TYPE),
m_pendingRelationshipGroup(0),
m_pendingAttachHeading(0.0f),
m_pendingAttachHeadingLimit(0.0f),
m_registeredWithKillTracker(false),
m_registeredWithHeadshotTracker(false),
m_TaskOverridingPropsWeapons(false),
m_TaskOverridingProps(false),
m_targetVehicleId(NETWORK_INVALID_OBJECT_ID),
m_myVehicleId(NETWORK_INVALID_OBJECT_ID),
m_targetVehicleSeat(-1),
m_targetVehicleTimer(0),
m_targetMountId(NETWORK_INVALID_OBJECT_ID),
m_targetMountTimer(0),
m_isTargettableByTeam((1<<MAX_NUM_TEAMS)-1),
m_isTargettableByPlayer(rage::PlayerFlags(~0)),
m_isDamagableByPlayer(rage::PlayerFlags(~0)),
m_DesiredHeading(0.0f),
m_DesiredMoveBlendRatioX(0.0f),
m_DesiredMoveBlendRatioY(0.0f),
m_DesiredMaxMoveBlendRatio(0.0f),
m_ForceTaskUpdate(false),
m_UsesCollisionBeforeTaskOverride(true),
m_weaponObjectFlashLightOn(false),
m_weaponObjectTintIndex(0),
m_temporaryTaskSequenceId(-1),
m_RespawnFailedFlags(0),
m_RespawnAcknowledgeFlags(0),
m_RespawnRemovalTimer(0),
m_lastRequestedSyncScene(0),
m_pMigrateTaskHelper(NULL),
m_NeedsNavmeshFixup(0),
m_pendingLookAtObjectId(NETWORK_INVALID_OBJECT_ID),
m_pendingLookAtFlags(0),
m_ammoToDrop(0),
m_pendingLastDamageEvent(true),
m_UpdateBlenderPosAfterVehicleUpdate(false),
m_Wandering(false),
m_FrameForceSendRequested(0),
m_ForceSendRagdollState(false),
m_ForceSendTaskState(false),
m_ForceSendGameState(false),
m_AttDamageToPlayer(INVALID_PLAYER_INDEX),
m_PhoneMode(0),
m_bNewPhoneMode(false),
m_pNetTennisMotionData(NULL),
m_ForceCloneTransitionChangeOwner(false),
m_OverrideScopeBiggerWorldGridSquare(false),
m_OverridingMoveBlendRatios(false),
m_killedWithHeadShot(false),
m_killedWithMeleeDamage(false),
m_bIsOwnerWearingAHelmet(false),
m_bIsOwnerRemovingAHelmet(false),
m_bIsOwnerAttachingAHelmet(false),
m_bIsOwnerSwitchingVisor(false),
m_nTargetVisorState(-1),
m_RappellingPed(false),
m_HasStopped(true),
m_HiddenUnderMapDueToVehicle(false),
m_ClearDamageCount(0),
m_PreventSynchronise(false),
m_RunningSyncedScene(false),
m_ZeroMBRLastFrame(true),
m_PreventTaskUpdateThisFrame(false),
m_IsSwimming(false),
m_IsDivingToSwim(false),
m_pendingPropUpdate(false),
m_numRemoteWeaponComponents(0),
m_uMaxNumFriendsToInform(CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM),
m_fMaxInformFriendDistance(CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE),
m_PRF_disableInVehicleActions(false),
m_PRF_IgnoreCombatManager(false),
m_Vaulting(false),
m_changenextpedhealthdatasynch(0),
m_lastSyncedWeaponHash(0),
m_hasDroppedWeapon(false),
m_MaxEnduranceSetByScript(false),
m_blockBuildQueriableState(false)
{
    m_taskSlotCache.Reset();
	m_CachedWeaponInfo.Reset();

	gnetAssert(MAX_NUM_TEAMS <= (sizeof(m_isTargettableByTeam)*8));
	gnetAssert(MAX_NUM_PHYSICAL_PLAYERS <= (sizeof(m_isTargettableByPlayer)*8));
	gnetAssert(MAX_NUM_PHYSICAL_PLAYERS <= (sizeof(m_isDamagableByPlayer)*8));
#if __ASSERT
	if(ped)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
		gnetAssertf(ped->IsPedAllowedInMultiplayer(), "ERROR : CNetObjPed::CNetObjPed : Ped %s %s - Unsupported ped type in MP - Non Horse Animal?! Invalid Scenario Point?! %f %f %f", 
			ped->GetDebugName(),
			ped->GetModelName(),
			pos.x, pos.y, pos.z);
	}
#endif

#if __BANK
	m_NumInventoryItems = 0;
	m_PedUsingInfiniteAmmo = false;
#endif
}

CNetObjPed::~CNetObjPed()
{
	// release any anims we have a hold of...
	m_motionSetClipSetRequestHelper.Release();
	m_overriddenWeaponSetClipSetRequestHelper.Release();
    m_overriddenStrafeSetClipSetRequestHelper.Release();

	ClearMigrateTaskHelper();

	// any task sequence should have been cleaned up in CleanUpGameObject()
	gnetAssert(m_temporaryTaskSequenceId == -1);

	if (m_pNetTennisMotionData)
	{
		delete m_pNetTennisMotionData;
	}
}

void CNetObjPed::CreateSyncTree()
{
    ms_pedSyncTree = rage_new CPedSyncTree(PED_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjPed::DestroySyncTree()
{
	ms_pedSyncTree->ShutdownTree();
    delete ms_pedSyncTree;
    ms_pedSyncTree = 0;
}

netINodeDataAccessor *CNetObjPed::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IPedNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IPedNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjPhysical::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

bool CNetObjPed::CanSetTask()
{
    if(IsClone() || IsPendingOwnerChange())
    {
        return m_bAllowSetTask;
    }
    else
    {
        gnetAssert(m_bAllowSetTask);
        return true;
    }
}

void CNetObjPed::RemoveTemporaryTaskSequence()
{
	if (m_temporaryTaskSequenceId != -1)
	{
		// need to restore the game heap here as DeRegister may delete tasks allocated from the main heap
		sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		CTaskSequences::ms_TaskSequenceLists[m_temporaryTaskSequenceId].DeRegister();

		sysMemAllocator::SetCurrent(*oldHeap);

		m_temporaryTaskSequenceId = -1;
	}
}

bool CNetObjPed::IsNavmeshTrackerValid() const
{
    CPed *ped = GetPed();

	VALIDATE_GAME_OBJECT(ped);

    if(!IsScriptObject() && ped && !ped->GetUsingRagdoll())
    {
        if(ped->GetNavMeshTracker().GetIsValid() && !ped->GetIsInInterior())
        {
            float navMeshPedPosZ = ped->GetNavMeshTracker().GetLastNavMeshIntersection().z + 1.0f;

            Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

            const float epsilon = 0.2f; // the acceptable error

            if(IsClose(pos.z, navMeshPedPosZ, epsilon))
            {
                return true;
            }
        }
    }

    return false;
}

// Name         :   SetPedOutOfVehicle
// Purpose      :   put the ped out of a car
void CNetObjPed::SetPedOutOfVehicle(const u32 flags)
{
    CPed* pPed = GetPed();

	VALIDATE_GAME_OBJECT(pPed);

    if (pPed && pPed->GetMyVehicle())
    {
        // ped is being removed from vehicle
        if (!IsClone())
        {
#if __ASSERT
			//Make sure we clear Local Flag - LOCALFLAG_TELEPORT in case the player was being teleported with his car
			//   but the car got left behind for some reason.
			if (pPed->IsLocalPlayer())
			{
				netObject* vehicleNetObj = pPed->GetMyVehicle()->GetNetworkObject();
				if (vehicleNetObj)
				{
					gnetAssert(!vehicleNetObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_TELEPORT));
				}
			}
#endif // __ASSERT

            bool oldAllowSetTask = m_bAllowSetTask;
            m_bAllowSetTask      = true;

            pPed->GetPedIntelligence()->FlushImmediately(true);
            gnetAssert(pPed->GetPedIntelligence()->GetTaskDefault() != 0);

            m_bAllowSetTask = oldAllowSetTask;

			// start synchronising immediately as the ped may try to migrate before the sync data has been re-established
			if (!GetSyncData() && CanSynchronise(false))
			{
				StartSynchronising();
			}
 
			// reset alpha in case the ped was in a vehicle that was fading / alpha ramping
			ResetAlphaActive();

			if (m_bHideWhenInvisible)
			{
				SetHideWhenInvisible(false);
			}
		}
        else
        {
            SetClonePedOutOfVehicle(false, flags);
        }
    }
}

// Name         :   MigratesWithVehicle
// Purpose      :   Returns true if the ped will migrate with the vehicle it is in
bool CNetObjPed::MigratesWithVehicle() const
{
	CPed* pPed = GetPed();

	VALIDATE_GAME_OBJECT(pPed);

	if (pPed->IsAPlayerPed())
	{
		return false;
	}

	// script peds have to migrate separately so that any migration data is also sent for them
	if (IsOrWasScriptObject())
	{
		return false;
	}

    // all non-player peds migrate with their vehicle
    return true;
}


// Name         :   SetClonePedInVehicle
// Purpose      :   Sets up correct states for a cloned ped when he is being placed in a vehicle
bool CNetObjPed::SetClonePedInVehicle(class CVehicle* pVehicle, const s32 iSeat)
{
    CPed* pPed = GetPed();

	VALIDATE_GAME_OBJECT(pPed);
	VALIDATE_GAME_OBJECT(pVehicle);

    gnetAssert(IsClone());
    gnetAssert(iSeat > -1);

    int nEntryExitPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

	if (pPed)
	{
		gnetAssert(!pPed->GetUsingRagdoll());

  		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if (pPed->GetMyVehicle() == pVehicle && pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) == iSeat)
			{
				pPed->AttachPedToEnterCar(pVehicle, ATTACH_STATE_PED_IN_CAR, iSeat, nEntryExitPoint);
				pPed->ProcessAllAttachments();
				pPed->ResetStandData();
				return true;
			}
			else
			{
				SetClonePedOutOfVehicle(true, 0);
			}
		}

		if (pPed->GetMyVehicle())
		{
			pPed->SetMyVehicle(NULL);
			m_myVehicleId = NETWORK_INVALID_OBJECT_ID;
		}

		CPed* pOccupier = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);

		if (pOccupier && pOccupier != pPed)
		{
			// get previous occupant out of the car
			if (!pOccupier->IsNetworkClone())
			{
				// if this is a local ped, force him out
				pOccupier->SetPedOutOfVehicle(CPed::PVF_Warp);
			}
			else
			{
				((CNetObjPed*) pOccupier->GetNetworkObject())->SetClonePedOutOfVehicle(false, 0);
			}
		}

		bool bAddedToCar=pVehicle->AddPedInSeat(pPed, iSeat);
#if __ASSERT
		CPed* pGetPedInSeat = !bAddedToCar ? pVehicle->GetPedInSeat(iSeat) : NULL;
		gnetAssertf(bAddedToCar, "Failed to add %s as passenger in %s at seat %d. Note GetPedInSeat()[%p][%s].", GetLogName(), pVehicle->GetNetworkObject()->GetLogName(), iSeat,pGetPedInSeat,pGetPedInSeat && pGetPedInSeat->GetNetworkObject() ? pGetPedInSeat->GetNetworkObject()->GetLogName() : "");
#endif

		if (bAddedToCar)
		{
			pPed->AttachPedToEnterCar(pVehicle, ATTACH_STATE_PED_IN_CAR, iSeat, nEntryExitPoint);
			pPed->ProcessAllAttachments();
			pPed->ResetStandData();

			pPed->SetMyVehicle(pVehicle);
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, true );

			m_myVehicleId = pVehicle->GetNetworkObject()->GetObjectID();

			if (pPed->IsPlayer() && pVehicle->GetStatus() != STATUS_WRECKED)
			{
				pVehicle->SetStatus(STATUS_PLAYER);

			}

			// Register in the spatial array that we are now using a vehicle.
			pPed->UpdateSpatialArrayTypeFlags();

			// If entering an RC car, make player invisible
			if(pVehicle->pHandling->mFlags & MF_IS_RC)
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_INVISIBLE_FOR_RC", GetLogName());

				pPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, false, true);
				pPed->DisableCollision(0, true);
			}

			return true;
		}
		else
		{
			pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		}
	}

    return false;
}

// Name         :   SetClonePedOutOfVehicle
// Purpose      :   Sets up correct states for a cloned ped when he is being removed from a vehicle
void CNetObjPed::SetClonePedOutOfVehicle(bool bClearMyVehiclePtr, const u32 flags)
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    gnetAssert(IsClone());

    if (pPed && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_CLONE_PED_OUT_OF_VEHICLE", GetLogName());

        u16 nDetachFlags = DETACH_FLAG_ACTIVATE_PHYSICS;
        if(flags&CPed::PVF_IgnoreSafetyPositionCheck)
			nDetachFlags |= DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;
        if(flags&CPed::PVF_NoCollisionUntilClear)
			nDetachFlags |= DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR;
		if(flags&CPed::PVF_ExcludeVehicleFromSafetyCheck)
			nDetachFlags |= DETACH_FLAG_EXCLUDE_VEHICLE;
		if(flags&CPed::PVF_Warp)
			nDetachFlags |= DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK;

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, false );

        pPed->DetachFromParent(nDetachFlags);

		// Register in the spatial array that we are no longer using a vehicle.
		pPed->UpdateSpatialArrayTypeFlags();

        CVehicle *pVehicle = pPed->GetMyVehicle();
        if (!pVehicle)
        {
            return;
        }

		if (flags&CPed::PVF_InheritVehicleVelocity)
		{
			Vector3 vDesiredVelocity = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity())));
			vDesiredVelocity += pVehicle->GetVelocity();
			const Vector3 vDesiredAngVelocity = pPed->GetDesiredAngularVelocity();
			pPed->SetVelocity(vDesiredVelocity);
			pPed->SetAngVelocity(vDesiredAngVelocity);		
		}

        if (!pVehicle->GetIsUsingScriptAutoPilot() && pPed->IsPlayer() && pVehicle->GetDriver() == pPed)
        {
            if(pVehicle->GetStatus() != STATUS_WRECKED)
            {
                pVehicle->SetIsAbandoned();
            }
        }

        // remove reference to this ped in the vehicle
		pVehicle->RemovePedFromSeat(pPed);

        if (bClearMyVehiclePtr)
        {
            pPed->SetMyVehicle(NULL);
        }

		// make sure the respot visibility module is reset (the ped may be ejected from a car while it is respotting)
		SetIsVisibleForModuleSafe( pPed, SETISVISIBLE_MODULE_VEHICLE_RESPOT, true, true );

		// reset alpha in case the ped was in a vehicle that was fading / alpha ramping
		ResetAlphaActive();

		if (m_bHideWhenInvisible)
		{
			SetHideWhenInvisible(false);
		}

		CPedWeaponManager *pPedWeaponMgr = pPed->GetWeaponManager();
		if (pPedWeaponMgr && pPedWeaponMgr->GetEquippedWeaponHash() != 0)
			pPedWeaponMgr->EquipWeapon(pPedWeaponMgr->GetEquippedWeaponHash(), -1, true);

        // ensure the network blender won't position the ped back inside the car
        spdAABB tempBox;
        const spdAABB &boundBox = pVehicle->GetBoundBox(tempBox);

        // shrink the bounding box to reduce finding false positives
        static const float BOX_SHRINK_SIZE         = 0.15f;
        static const float BOX_SHRINK_SIZE_DOUBLED = BOX_SHRINK_SIZE * 2.0f;
        Vector3 min = boundBox.GetMinVector3();
        Vector3 max = boundBox.GetMaxVector3();

        if((max.x - min.x) > BOX_SHRINK_SIZE_DOUBLED)
        {
            min.x += BOX_SHRINK_SIZE;
            max.x -= BOX_SHRINK_SIZE;
        }

        if((max.y - min.y) > BOX_SHRINK_SIZE_DOUBLED)
        {
            min.y += BOX_SHRINK_SIZE;
            max.y -= BOX_SHRINK_SIZE;
        }
        
        if((max.z - min.z) > BOX_SHRINK_SIZE_DOUBLED)
        {
            min.z += BOX_SHRINK_SIZE;
            max.z -= BOX_SHRINK_SIZE;
        }

        spdAABB shrunkBoundBox(VECTOR3_TO_VEC3V(min), VECTOR3_TO_VEC3V(max));

        CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

        if(pPedBlender)
        {
            Vector3 targetPosition = pPedBlender->GetLastPositionReceived();

            bool blendTargetInsideVehicle = shrunkBoundBox.ContainsPoint(VECTOR3_TO_VEC3V(targetPosition));

            if(blendTargetInsideVehicle || m_UpdateBlenderPosAfterVehicleUpdate)
            {
                // update the blender with the new position outside of the car
                pPedBlender->Reset();

                m_UpdateBlenderPosAfterVehicleUpdate = false;
            }
            else if(!IsUsingRagdoll() && !(flags & CPed::PVF_NoNetBlenderTeleport))
            {
                // we can only do this if we have received the latest vehicle state from the network,
                // when this occurs m_UpdateBlenderPosAfterVehicleUpdate is set and this code path doesn't run,
                // as it is not cleared until the next position message arrives.
                if(m_targetVehicleId == NETWORK_INVALID_OBJECT_ID)
                {
                    static const float MAX_DIST_FROM_TARGET = 0.5f;

                    Vector3 currentPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

                    if((currentPosition - targetPosition).Mag2() > MAX_DIST_FROM_TARGET)
                    {
                        pPedBlender->GoStraightToTarget();
                    }
                }
            }
        }

		if( flags&CPed::PVF_Warp )
		{
			pPed->GetIkManager().ResetAllSolvers();
		}

		// If exiting an RC car, make the player visible
		if(pVehicle && pVehicle->pHandling->mFlags & MF_IS_RC)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_VISIBLE_FOR_RC", GetLogName());

			pPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, true, true);
			pPed->EnableCollision();
		}
    }
}

void CNetObjPed::SetClonePedOnMount(CPed* pMount)
{
	//Grab the ped.
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	if(!pPed)
		return;

	//Ensure the ped is a clone.
	gnetAssert(IsClone());
	
	//Set the ped on the mount.
	pPed->SetPedOnMount(pMount);
}

void CNetObjPed::SetClonePedOffMount()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	if(!pPed)
		return;

	//Ensure the ped is a clone.
	gnetAssert(IsClone());

	//Set the ped off the mount.
	pPed->SetPedOffMount();
}

// Name         :   GetPedsVehicle
// Purpose      :   Returns the target vehicle for a clone ped
CVehicle* CNetObjPed::GetPedsTargetVehicle(s32* pSeat)
{
    if (pSeat)
        *pSeat = m_targetVehicleSeat;

    if (m_targetVehicleId != NETWORK_INVALID_OBJECT_ID)
    {
        netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetVehicleId);

        CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(pNetObj);
        return vehicle;
    }

    return NULL;
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   player        - the new owner
//                  migrationType - how the entity migrated
// Returns      :   void
void CNetObjPed::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	bool bWasClone = IsClone();

	if (pPed)
	{
		// Cloned peds don't count toward certain population counts
		CPedPopulation::RemovePedFromPopulationCount(pPed);
	}

	// make sure the ped's vehicle is correct before migrating locally
	if (!m_ForceCloneTransitionChangeOwner && bWasClone && player.IsLocal())
	{
		UpdateVehicleForPed(true);
	}

	CNetObjPhysical::ChangeOwner(player, migrationType);

	if (pPed)
	{
		CTaskMobilePhone* pMobilePhoneTask = NULL;
		pMobilePhoneTask = static_cast<CTaskMobilePhone*>(pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim());
		if (pMobilePhoneTask && pMobilePhoneTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE)
		{
			m_PhoneMode = pMobilePhoneTask->GetPhoneMode(); //If changing from local to clone the current state needs to be recorded in m_PhoneMode, it is cleared if ped becomes local.
		}

		CPedPopulation::AddPedToPopulationCount(pPed);

#if __BANK
		const bool pedIsDeadOrDying = pPed->GetIsDeadOrDying();
		const bool bRunningTaskDyingDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD)) ? true : false;
		if(pedIsDeadOrDying || bRunningTaskDyingDead)
		{
			NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Begin"); )
		}
#endif

#if ENABLE_NETWORK_LOGGING
		if (IsRespawnInProgress())
		{
			netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
			NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "CHANGEOWNER_TEAM_SWAP", GetLogName());
		}
#endif

		if ((!bWasClone || m_ForceCloneTransitionChangeOwner) && IsClone())
		{
			pedDebugf2("CNetObjPed::ChangeOwner ped[%p][%s][%s] was local, now a clone. owner[%s]",pPed,pPed->GetModelName(),GetLogName(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");
			
			SetRequiredProp();

			if (GetLastDamageEventPending() && pPed->IsFatallyInjured())
			{
				gnetDebug1("SetLastDamageEventPending 'FALSE' - '%s' - Clone change.", GetLogName());
				SetLastDamageEventPending( false );
			}

			// copy the peds vehicle into the target info
			if (pPed->GetIsInVehicle() && pPed->GetMyVehicle()->GetNetworkObject())
			{
				m_targetVehicleId = pPed->GetMyVehicle()->GetNetworkObject()->GetObjectID();
				m_targetVehicleSeat = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
			}
			else
			{
				m_targetVehicleId = NETWORK_INVALID_OBJECT_ID;
			}

			m_targetVehicleTimer = 0;
			
			// we need to grab the desired movement parameters because they will get applied in the next update for the clone
			pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio(m_DesiredMoveBlendRatioX, m_DesiredMoveBlendRatioY);
			m_DesiredMaxMoveBlendRatio = pPed->GetMotionData()->GetScriptedMaxMoveBlendRatio();
			m_DesiredHeading = pPed->GetDesiredHeading();

			// copy the peds mount into the target info
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) && pPed->GetMyMount()->GetNetworkObject())
			{
				m_targetMountId = pPed->GetMyMount()->GetNetworkObject()->GetObjectID();
			}
			else
			{
				m_targetMountId = NETWORK_INVALID_OBJECT_ID;
			}

			m_targetMountTimer = 0;

			// need to cache the current motion group data and restore it after replacing the tasks
			CPedMovementGroupDataNode pedMovementGroupDataNode;
			GetMotionGroup(pedMovementGroupDataNode);

			u32 weaponHash;
			bool weaponObjectExists, weaponObjectVisible, weaponObjectFlashLightOn, hasAmmo, attachLeft;
			GetWeaponData(weaponHash, weaponObjectExists, weaponObjectVisible, m_weaponObjectTintIndex, weaponObjectFlashLightOn, hasAmmo, attachLeft);
			m_weaponObjectExists = weaponObjectExists;
			m_weaponObjectVisible = weaponObjectVisible;
			m_weaponObjectFlashLightOn = weaponObjectFlashLightOn;
			m_weaponObjectHasAmmo = hasAmmo;
			m_weaponObjectAttachLeft = attachLeft;

			// NOTE[egarcia]: Update ped targetting. We do not want the cloned ped to keep a dead player as a target.
			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting();
			if (pPedTargetting)
			{
				pPedTargetting->UpdateTargetting(true);
			}

			if(!m_ForceCloneTransitionChangeOwner)
			{
				CheckPedsCurrentTasksForMigrateData();
			}
#if __ASSERT
			else
			{
				gnetDebug3("%s Ignored migrate data for Player ped", GetLogName());
			}
#endif 
			CreateCloneTasksFromLocalTasks();

            if(!pPed->GetPedIntelligence()->GetTaskDefault())
            {
                pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
            }

			gnetAssert(pPed->GetPedIntelligence()->GetTaskDefault() != 0);

            CTask *activeTask = pPed->GetPedIntelligence()->GetTaskActive();
            if(activeTask && activeTask->IsClonedFSMTask())
            {
                CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(activeTask);
			    if (taskFSMClone)
			    {
				    taskFSMClone->HandleLocalToCloneSwitch(pPed);
			    }
            }

			// put ped back in the vehicle (the tasks being flushed will punt him out)
			UpdateVehicleForPed();

			// don't allow tasks to be set on this ped
			m_bAllowSetTask = false;

			// remove any pending events
			pPed->GetPedIntelligence()->FlushEvents();

			AddRequiredProp();

			// restore the cached motion group data
			SetMotionGroup(pedMovementGroupDataNode);

			// release the cover point the ped was using, clone peds do not use them
			pPed->ReleaseCoverPoint();

			//Ensure clones continue to have infinite ammo after migration - if they become local they will have whatever the cached true value is for infiniteammo status
			if (pPed->GetInventory())
			{
				SetCacheUsingInfiniteAmmo(pPed->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteAmmo());
				pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(true);
			}
#if __BANK
			if(pedIsDeadOrDying || bRunningTaskDyingDead)
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Recreated Clone Tasks"); )
			}
#endif
            m_FrameForceSendRequested = 0;
            m_ForceSendRagdollState   = false;
            m_ForceSendTaskState      = false;
            m_ForceSendGameState   = false;

			pPed->SetExitVehicleOnChangeOwner(false);

            m_HasStopped = pPed->GetVelocity().Mag2() < 0.1f;
		}
		else if (bWasClone && !IsClone())
		{
			pedDebugf2("CNetObjPed::ChangeOwner ped[%p][%s][%s] was clone, now local. owner[%s]",pPed,pPed->GetModelName(),GetLogName(),GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");

			// allow tasks to be set on this ped
			m_bAllowSetTask = true;

			// remove the ped from the vehicle and keep track of the
			// vehicle information prior to recreating the tasks on
			// this ped. This information will be lost when the ped
			// has their tasks flushed
			bool bWasInVehicle = pPed->GetIsInVehicle();
			CVehicle *pVehicle = pPed->GetMyVehicle();
			s32  iSeat     = -1;

			if(bWasInVehicle)
			{
				iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				gnetAssert(iSeat != -1);

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, false );

				const bool bAllowMarkAbandoned = pVehicle->IsDriver(pPed) && (pPed->GetExitVehicleOnChangeOwner() || pVehicle->IsNetworkClone());

				pVehicle->RemovePedFromSeat(pPed, bAllowMarkAbandoned, false);

				pPed->SetMyVehicle(NULL);
			}

			//! Fixup melee task on migration.
			if(pPed->GetPedIntelligence()->GetTaskMeleeActionResult())
			{
				m_PreventSynchronise = true;
				pPed->GetPedIntelligence()->GetTaskMeleeActionResult()->FixupKillMoveOnMigration();
				m_PreventSynchronise = false;
			}

			if(!m_ForceCloneTransitionChangeOwner)
			{
				CheckPedsCurrentTasksForMigrateData();
			}
#if __ASSERT
			else
			{
				gnetDebug3("%s Ignored migrate data for Player ped", GetLogName());
			}
#endif 
			m_blockBuildQueriableState = true; // set flag to avoid rebuilding the queriable state to avoid crashing due to bad casts
			CreateLocalTasksFromCloneTasks();
			m_blockBuildQueriableState = false;

			if(m_PhoneMode!=0)
			{   //clone had secondary phone task so reapply to local
				CTaskMobilePhone* pMobilePhoneTask = rage_new CTaskMobilePhone(m_PhoneMode, -1, true, true);
				 
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations, false);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations, false);
				pPed->GetPedIntelligence()->AddTaskSecondary( pMobilePhoneTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
			}

			CTask* pNewTask = static_cast<CTask*>(pPed->GetPedIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_PRIMARY));

			if (pNewTask && pNewTask->IsClonedFSMTask())
			{
				static_cast<CTaskFSMClone *>(pNewTask)->HandleCloneToLocalSwitch(pPed);
			}

			//Ped is beaming out - set the ped to stand still
			if (pPed->IsSpecialNetworkLeaveActive())
			{
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(-1,0)); //still forever
				pPed->GetPedIntelligence()->AddEvent(event);
			}

			if(bWasInVehicle)
			{
                pPed->SetPedInVehicle(pVehicle,iSeat, CPed::PVF_IfForcedTestVehConversionCollision | CPed::PVF_DontResetDefaultTasks | CPed::PVF_DontApplyEnterVehicleForce | CPed::PVF_DontRestartVehicleMotionState);

				if(pPed->GetExitVehicleOnChangeOwner())
				{
					VehicleEnterExitFlags vehicleFlags;
					CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
					pSequence->AddTask(rage_new CTaskExitVehicle(pVehicle, vehicleFlags));
					pSequence->AddTask(rage_new CTaskSmartFlee(CAITarget(pVehicle)));
					pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskUseSequence(*pSequence), true);

					pPed->SetExitVehicleOnChangeOwner(false);
				}

                if(!pPed->GetPedIntelligence()->GetTaskDefault())
                {
                    pPed->GetPedIntelligence()->AddTaskDefault(pPed->ComputeDefaultDrivingTask(*pPed, pVehicle, CPed::PVF_UseExistingNodes));
                }

                pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
			}

			pPed->SetIsCrouching(false);

			m_motionSetClipSetRequestHelper.Release();
			m_overriddenWeaponSetClipSetRequestHelper.Release();
            m_overriddenStrafeSetClipSetRequestHelper.Release();

			//Local peds should use whatever the cached value was for InfiniteAmmo state as clones always have infinite ammo
			if (pPed->GetInventory())
				pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(m_CacheUsingInfiniteAmmo);

			// reassign any orders that this ped may have
			COrderManager::GetInstance().ReassignOrdersToPed(pPed);

            m_UpdateBlenderPosAfterVehicleUpdate = false;
            m_Wandering                          = false;
            m_OverridingMoveBlendRatios          = false;
			m_RunningSyncedScene                 = false;
			m_Vaulting							 = false;

            // ensure the ped is always using the last received values for the move blend ratio and desired heading
            // other code systems can alter this and leaves clones in an incorrect state
            pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_DesiredMoveBlendRatioY, m_DesiredMoveBlendRatioX);
			pPed->GetMotionData()->SetScriptedMaxMoveBlendRatio(m_DesiredMaxMoveBlendRatio);
			pPed->SetExitVehicleOnChangeOwner(false);

			//! Tidy up from being in a road block.
			if(pPed->PopTypeGet()==POPTYPE_RANDOM_PERMANENT && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ChangeFromPermanentToAmbientPopTypeOnMigration))
			{
				pPed->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ChangeFromPermanentToAmbientPopTypeOnMigration, false);
				pPed->GetNetworkObject()->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, false);
			}

#if __BANK
			if(pedIsDeadOrDying || bRunningTaskDyingDead)
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Recreated Local Tasks"); )
			}
#endif
		}
        else
        {
            if(!IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
            {
                HandleCloneTaskOwnerChange(migrationType);
            }

#if __BANK
			if(pedIsDeadOrDying || bRunningTaskDyingDead)
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Handling clone owner change"); )
			}
#endif
        }

		RemoveTemporaryTaskSequence();

		//Do some setup on the respawn ped:
		//  Create task exit vehicle if it is applied.
		if (IsRespawnInProgress() && !IsClone())
		{
			CreateLocalTasksForRespawnPed();

#if __BANK
			if(pedIsDeadOrDying || bRunningTaskDyingDead)
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Recreated Respawn Ped Tasks"); )
			}
#endif
		}

        // reset all task sequences - they are only relevant to the previous owner
        pPed->GetPedIntelligence()->GetQueriableInterface()->ResetTaskSequenceInfo();

		// reset all task slots - we cannot guarantee this data anymore
		ResetTaskSlotData();

		//TODO: B*2079690 - 
		//The CreateLocalTasksFromCloneTasks before ResetTaskSequenceInfo above sets up all TaskSequenceInfo 
		//for the tasks, however the task sequences are then reset in ResetTaskSequenceInfo.
		//We need to check the peds primary task and reapply a task sequence info as follows so subsequent sequence ID's are kept correctly incremented
		// using HandleCloneToLocalTaskSwitch will apply a taskSequenceInfo if missing and ensure its sequence number is aligned with the current task:
		if(bWasClone && !IsClone())
		{
			CTask* pTask=pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
			if(pTask && pTask->IsClonedFSMTask())
			{
				pPed->GetPedIntelligence()->GetQueriableInterface()->HandleCloneToLocalTaskSwitch(*static_cast<CTaskFSMClone*>(pTask));
			}
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions,m_PRF_disableInVehicleActions);

		// Ensure that any tasks given directly to the ped end up in the correct state
		// Since network events are handled at the start of a game frame we could end up with a t-posed
		// character if we didn't ensure that the new tasks received an update prior to the animation update
		pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true);
		pPed->InstantAIUpdate(true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, false);

#if __BANK
		if (pedIsDeadOrDying && !IsClone())
		{
			if(!pPed->GetIsDeadOrDying() && pPed->ShouldBeDead() && GetPlayerOwner() && IsInScope(*GetPlayerOwner()))
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - Dead End"); )
				gnetAssertf(0, "Ped %s changed Ownership and lost the dead dying State. Running Task Before/After= %s/%s", GetLogName(), 
					bRunningTaskDyingDead ? "true" : "false",
					static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD)) ? "true" : "false");
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped Lost Dying Dead State:", "%s %s", GetLogName(), IsRespawnInProgress() ? "true" : "false");
			}
		}

		if (bRunningTaskDyingDead && !IsClone())
		{
			bool bStillRunningTaskDyingDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD)) ? true : false;
			if(!bStillRunningTaskDyingDead && pPed->ShouldBeDead() && GetPlayerOwner() && IsInScope(*GetPlayerOwner()))
			{
				NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - End"); )
				gnetAssertf(0, "Ped %s changed Ownership and lost the dead dying task", GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped (Lost Dying Dead Task:", "%s %s", GetLogName(), IsRespawnInProgress() ? "true" : "false");
			}
		}

		if (!pPed->IsDead() && (pPed->IsCopPed() || pPed->IsSwatPed()))
		{
			CPedInventoryDataNode currdata;
			GetPedInventoryData(currdata);

			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "COP_PED_INVENTORY", GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Num inventory items:", "%d (old: %d)", currdata.m_numItems, m_NumInventoryItems);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Infinite ammo:", "%d (old: %d)", currdata.m_allAmmoInfinite, m_PedUsingInfiniteAmmo);

			if (m_NumInventoryItems > 0)
			{
				if (currdata.m_numItems == 0)
				{
					gnetAssertf(0, "Cop ped %s has lost his inventory after migration", GetLogName());
				}
			}
			else 
			{
				m_NumInventoryItems = currdata.m_numItems;
			}

			if (m_PedUsingInfiniteAmmo)
			{
				gnetAssertf(currdata.m_allAmmoInfinite, "Cop ped %s has lost infinite ammo after migration", GetLogName());
			}
			else
			{
				m_PedUsingInfiniteAmmo = currdata.m_allAmmoInfinite;
			}
		}
#endif // __BANK
	}

	// DAN TEMP - clear the force synchronise flag - this should be done at a higher level
	SetLocalFlag(CNetObjGame::LOCALFLAG_FORCE_SYNCHRONISE, false);

	// Reset respawn ped local flag.
	SetLocalFlag(CNetObjGame::LOCALFLAG_RESPAWNPED, false);
	SetLocalFlag(CNetObjGame::LOCALFLAG_NOREASSIGN, false);
	m_RespawnFailedFlags      = 0;
	m_RespawnAcknowledgeFlags = 0;
	m_ForceCloneTransitionChangeOwner = false;
}

// Name         :   ClearMigrateTaskHelper
// Purpose      :   Used by tasks to delete and reset the pointer when migration data has been finished with
void CNetObjPed::ClearMigrateTaskHelper(bool bAtCleanUp)
{
	sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
	if(m_pMigrateTaskHelper!=NULL)
	{		
		if(bAtCleanUp)
		{
			CPed* pPed = GetPed();
			VALIDATE_GAME_OBJECT(pPed);

			if (pPed)
			{
				if (pPed->GetIsAttached())
				{
					pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				}

				m_pMigrateTaskHelper->NetObjPedCleanUp(pPed);
			}
		}
		delete m_pMigrateTaskHelper;
		m_pMigrateTaskHelper=NULL;
	}
	sysMemAllocator::SetCurrent(*oldHeap);
}

// Name         :   CheckCurrentTasksForMigrateData
// Purpose      :   Checks the ped for any tasks running that require migrate helper data to be set for post migrate
void CNetObjPed::CheckPedsCurrentTasksForMigrateData()
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		ClearMigrateTaskHelper();

		CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));

		//If there is an CTaskAmbientClips subtask running at migrate set up the migrate helper to ensure clips and props swap smoothly
		if(pTaskAmbientClips!=NULL)
		{
			sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
			sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
			SetMigrateTaskHelper(rage_new CTaskAmbientMigrateHelper(pTaskAmbientClips));
			sysMemAllocator::SetCurrent(*oldHeap);

			if(m_pMigrateTaskHelper->GetHasMigrateData())
			{
				pTaskAmbientClips->StopClip(MIGRATE_SLOW_BLEND_OUT_DELTA);
				if(pTaskAmbientClips->GetBaseClip())
				{
					pTaskAmbientClips->GetBaseClip()->StopClip(MIGRATE_SLOW_BLEND_OUT_DELTA);
				}
			}
			else
			{
				gnetDebug3("%s CTaskAmbientClips [%p] task has no migrate data so dont set slow blend out on its clips",GetLogName(),pTaskAmbientClips);
			}
		}
	}
}

void CNetObjPed::ExtractTaskSequenceData(ObjectId NET_ASSERTS_ONLY(objectID), const CTaskSequenceList& taskSequenceList, u32& numTasks, CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32& repeatMode)
{
	numTasks = 0;

	u32 seqTask = 0;

	const CTask* pCurrTask = static_cast<const CTask*>(taskSequenceList.GetTask(seqTask));

	while (pCurrTask && gnetVerifyf(numTasks < MAX_SYNCED_SEQUENCE_TASKS, "%d has too many sequence tasks! Please reduce the number of tasks added to this sequence or speak to a network programmer!", objectID))
	{
		IPedNodeDataAccessor::CTaskData* pCurrTaskData = &taskData[numTasks];

		if (gnetVerifyf(CQueriableInterface::CanTaskBeGivenToNonLocalPeds(pCurrTask->GetTaskType()), "%s is part of a task sequence but is not set up to be sent over the network!", pCurrTask->GetTaskName()))
		{
			CTaskInfo* pTaskInfo = pCurrTask->CreateQueriableState();
			u32 maxSizeOfData = sizeof(pCurrTaskData->m_TaskData);

			if (AssertVerify(pTaskInfo))
			{
				// CTaskComplexControlMovement creates a task info for a different task type, so we need this check here
				if (pTaskInfo->GetTaskType() == CTaskTypes::TASK_NONE)
				{
					pTaskInfo->SetType(pCurrTask->GetTaskType());
				}

				if (gnetVerifyf(pTaskInfo->GetSpecificTaskInfoSize() <= maxSizeOfData, "Task %s is larger than MAX_SPECIFIC_TASK_INFO_SIZE, increase to %d", pCurrTask->GetTaskName(), pTaskInfo->GetSpecificTaskInfoSize()))
				{
					pCurrTaskData->m_TaskType = pTaskInfo->GetTaskType();

					datBitBuffer messageBuffer;
					messageBuffer.SetReadWriteBits(pCurrTaskData->m_TaskData, maxSizeOfData+1, 0);
					pTaskInfo->WriteSpecificTaskInfo(messageBuffer);

					pCurrTaskData->m_TaskDataSize = messageBuffer.GetCursorPos();
				}

				delete pTaskInfo;

				numTasks++;
			}
		}

		pCurrTask = static_cast<const CTask*>(taskSequenceList.GetTask(++seqTask));
	}

	repeatMode = (u32)taskSequenceList.GetRepeatMode();
}


// Name         :   IsFadingOut
// Purpose      :   Returns TRUE if its still fading this ped and the ped's weapon
bool CNetObjPed::IsFadingOut()
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	bool isFadingOut = false;

	if (pPed)
	{
		//check if the ped is currently being faded out.
		if (pPed->GetLodData().GetFadeToZero() && pPed->GetLodData().GetAlpha() > 0)
		{
			const bool isVisible = (IsVisibleToScript() && pPed->IsVisible() && pPed->GetLodData().IsVisible());
			if (isVisible)
			{
				isFadingOut = true;
			}
		}

		//check if the ped weapons is currently being faded out.
		if (!isFadingOut && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject())
		{
			const CObject* obj = pPed->GetWeaponManager()->GetEquippedWeaponObject();

			const bool isVisibleToScript = obj->GetNetworkObject() ? static_cast< CNetObjEntity* >(obj->GetNetworkObject())->IsVisibleToScript() : true;

			const bool isVisible = (isVisibleToScript && obj->IsVisible() && obj->GetLodData().IsVisible());

			if (isVisible && obj->m_nObjectFlags.bFadeOut && obj->GetAlpha() > 0)
			{
				isFadingOut = true;
			}

			const CWeapon::Components& components = pPed->GetWeaponManager()->GetEquippedWeapon()->GetComponents();

			for (int i=0; i<components.GetCount() && !isFadingOut;i++)
			{
				if (components[i]->GetDrawable())
				{
					const CDynamicEntity* dynamicEntity = components[i]->GetDrawable();

					const bool isVisibleToScript = dynamicEntity->GetNetworkObject() ? static_cast< CNetObjEntity* >(dynamicEntity->GetNetworkObject())->IsVisibleToScript() : true;

					if (isVisibleToScript && dynamicEntity->GetIsVisible() && dynamicEntity->GetLodData().IsVisible())
					{
						if (dynamicEntity->GetIsTypeObject())
						{
							CObject* obj = static_cast<CObject*>(components[i]->GetDrawable());

							if (obj->m_nObjectFlags.bFadeOut && obj->GetAlpha() > 0)
							{
								isFadingOut = true;
							}
						}
						else
						{
							const fwLodData& lod = dynamicEntity->GetLodData();

							if (lod.GetFadeToZero() && lod.IsVisible() && lod.GetAlpha() > 0)
							{
								isFadingOut = true;
							}
						}
					}
				}
			}
		}
	}

	return isFadingOut;
}

// Name         :   FadeOutPed
// Purpose      :   Alpha fades the ped and the ped's weapon
void CNetObjPed::FadeOutPed(bool fadeOut)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed && fadeOut != pPed->GetLodData().GetFadeToZero())
	{
		pPed->GetLodData().SetFadeToZero(fadeOut);

		if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject())
		{
			pPed->GetWeaponManager()->GetEquippedWeaponObject()->m_nObjectFlags.bFadeOut = fadeOut;

			const CWeapon::Components& components = pPed->GetWeaponManager()->GetEquippedWeapon()->GetComponents();

			for (int i=0; i<components.GetCount(); i++)
			{
				if (components[i]->GetDrawable())
				{
					if (components[i]->GetDrawable()->GetIsTypeObject())
					{
						static_cast<CObject*>(components[i]->GetDrawable())->m_nObjectFlags.bFadeOut = fadeOut;
					}
					else
					{
						components[i]->GetDrawable()->GetLodData().SetFadeToZero(fadeOut);
					}
				}
			}
		}
	}
}

// Name         :   CreateLocalTasksForRespawnPed
// Purpose      :   Called when the respawn ped ownership changes to set some tasks on him if needed.
void CNetObjPed::CreateLocalTasksForRespawnPed()
{
	CPed* ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (!gnetVerify(ped))
		return;

	if (!gnetVerify(!IsClone()))
		return;

	if (!IsRespawnInProgress())
		return;

	netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();

	const bool isDeadOrDying = ped->GetIsDeadOrDying() || ped->IsInjured();
	bool vehicleInTheGround  = false;

	CVehicle* vehicle = NULL;
	if (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		vehicle = ped->GetMyVehicle();

		if (vehicle)
		{
			const bool flyes = vehicle->GetIsAircraft() ;
			bool vehicleIsFlying = false;
			if (flyes)
			{
				Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
				const float ground = WorldProbe::FindGroundZForCoord(TOP_SURFACE, pos.x, pos.y);
				if (pos.z - ground > 2.0f)
				{
					vehicleIsFlying = true;
				}
			}

			vehicleInTheGround = !flyes || !vehicleIsFlying;
		}
	}

	networkLog.WriteDataValue("In Vehicle:", "%s", vehicle ? "True" : "False");
	networkLog.WriteDataValue("Is Dead Or Dying:", "%s", isDeadOrDying ? "True" : "False");
	networkLog.WriteDataValue("Ped should be dead:", "%s", ped->IsDead() ? "True" : "False");

	//Leave the car 
	if(vehicle && !isDeadOrDying && vehicleInTheGround)
	{
		CEvent* pEvent                      = ped->GetPedIntelligence()->GetEventOfType(EVENT_GIVE_PED_TASK);
		CEventGivePedTask* eventGivePedTask = pEvent           ? SafeCast(CEventGivePedTask, pEvent) : NULL;
		aiTask* pTask                       = eventGivePedTask ? eventGivePedTask->GetTask()         : NULL;

		bool setExitVehicleTask = true;
		if (pTask)
		{
			const s32 taskType = pTask->GetTaskType();
			setExitVehicleTask = !(taskType == CTaskTypes::TASK_EXIT_VEHICLE 
									|| taskType == CTaskTypes::TASK_EXIT_VEHICLE_SEAT 
									|| taskType == CTaskTypes::TASK_REACT_TO_BEING_JACKED);
		}

		//Check if someone else has reserved the seat we are in.
		CComponentReservationManager* rmgr = vehicle->GetComponentReservationMgr();
		if (rmgr)
		{
			CComponentReservation* componentReservation = rmgr->GetSeatReservation(0, SA_directAccessSeat);
			if (componentReservation && componentReservation->IsComponentInUse())
			{
				setExitVehicleTask = false;
				networkLog.WriteDataValue("Set task exit vehicle:", "False");
				networkLog.WriteDataValue("Someone has reserved the seat:", "");
			}
		}

		if (setExitVehicleTask)
		{
			const CQueriableInterface* qi = ped->GetPedIntelligence()->GetQueriableInterface();

			if (!qi->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) 
				&& !qi->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT)
				&& !qi->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_TO_BEING_JACKED))
			{
				networkLog.WriteDataValue("Set task exit vehicle:", "True");

				s32 seat = vehicle->GetSeatManager()->GetPedsSeatIndex(ped);
				gnetAssert(seat != -1);

				ped->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, false );
				vehicle->RemovePedFromSeat(ped);
				ped->SetMyVehicle(NULL);

				ped->GetPedIntelligence()->FlushImmediately(true);
				gnetAssert(ped->GetPedIntelligence()->GetTaskDefault() != 0);

				VehicleEnterExitFlags vehicleFlags;
				ped->GetPedIntelligence()->AddTaskDefault(rage_new CTaskExitVehicle(vehicle, vehicleFlags), true);

				//Set the ped inside the vehicle.
				ped->SetPedInVehicle(vehicle, seat, CPed::PVF_IfForcedTestVehConversionCollision | CPed::PVF_DontResetDefaultTasks);
			}
		}
		else
		{
			networkLog.WriteDataValue("Dont Set task exit vehicle:", "Already has a task event to exit vehicle");
		}
	}
}

// Name         :   PostSetupForRespawnPed
// Purpose      :   Called when a player ped is converted to a respawn ped.
void CNetObjPed::PostSetupForRespawnPed(CVehicle* vehicle, const u32 seat, bool killPed, TaskSlotCache& taskSlotData)
{
	CPed* ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (!ped)
		return;

	gnetAssert(!ped->IsAPlayerPed());

	//Make sure we don't Trigger last damage event for clone peds.
	gnetDebug1("SetLastDamageEventPending 'FALSE' - '%s' - PostSetupForRespawnPed.", GetLogName());
	SetLastDamageEventPending( false );

	//Flag respawn ped with players who have failed the respawn.
	m_RespawnFailedFlags      = 0;
	//Flag respawn ped with players who have acknowledge the respawn event.
	m_RespawnAcknowledgeFlags = 0;

	//This here because during the swap process some network synch's
	//could have arrived set the the player ped invisible and not Collidable.
#if __FINAL
	SetUsesCollision(true, "net");
	SetIsVisible(true, "net");
#else
	SetUsesCollision(true, "PostSetupForRespawnPed");
	SetIsVisible(true, "PostSetupForRespawnPed");
#endif // __FINAL
	SetIsFixed(false);

	const bool isDeadOrDying = ped->GetIsDeadOrDying();
	const bool isDeadOrDyingOrInjured = ped->GetIsDeadOrDying() || ped->IsInjured();

	//If the ped is dead then setup the 10s removal timer
	m_RespawnRemovalTimer = 0;
	if (isDeadOrDyingOrInjured)
	{
		SetDeadPedRespawnRemovalTimer();
	}

	//Setup the ped inside the vehicle.
	if (vehicle)
	{
		if (vehicle->GetNetworkObject() && vehicle->GetSeatManager())
		{
			m_targetVehicleId = vehicle->GetNetworkObject()->GetObjectID();
			m_targetVehicleSeat = vehicle->GetSeatManager()->GetPedsSeatIndex(ped);
			SetVehicleData(m_targetVehicleId, seat+1);
		}

		if (!isDeadOrDyingOrInjured)
		{
			//Don't let ped jacking me get inside - I will exit the vehicle when possible.
			ped->SetPedConfigFlag(CPED_CONFIG_FLAG_PedsJackingMeDontGetIn, true);

			const bool flyes       = vehicle->GetIsAircraft() ;
			const bool pedIsDriver = (vehicle->GetDriver() == ped);
			const bool isMoving    = (vehicle->GetVelocity().Mag2() > 0.1f && vehicle->GetDistanceTravelled() > 0.2f);

			bool vehicleIsFlying = false;
			if (flyes)
			{
				Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
				const float ground = WorldProbe::FindGroundZForCoord(TOP_SURFACE, pos.x, pos.y);
				if (pos.z - ground > 2.0f)
				{
					vehicleIsFlying = true;
				}
			}

			if (pedIsDriver && flyes && (vehicleIsFlying || isMoving))
			{
				killPed = true;
			}

			netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
			log.WriteDataValue("Ped is Driver:", "%s", pedIsDriver ? "True" : "False");
			log.WriteDataValue("Vehicle is moving:", "%s", isMoving ? "True" : "False");
			log.WriteDataValue("Vehicle Can Fly:", "%s", flyes ? "True" : "False");
			log.WriteDataValue("Vehicle is Flying:", "%s", vehicleIsFlying ? "True" : "False");
		}
	}
	else
	{
		m_targetVehicleId = NETWORK_INVALID_OBJECT_ID;
		SetVehicleData(m_targetVehicleId, seat+1);
	}

	if (gnetVerify(ped->GetPedIntelligence()))
	{
		//Reset the ped Relationship Group
		if(ped->GetPedType() == PEDTYPE_COP)
		{
			ped->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pCopGroup);
		}
		else if (ped->GetPedType() == PEDTYPE_CIVMALE || ped->GetPedType() == PEDTYPE_ANIMAL)
		{
			ped->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pCivmaleGroup);
		}
		else if (ped->GetPedType() == PEDTYPE_CIVFEMALE)
		{
			ped->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pCivfemaleGroup);
		}
		else
		{
			ped->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pNoRelationshipGroup);
		}

		//It cant use vehicles. (TaskPolicePatrol puts the ped back in the vehicle)
		ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseVehicles);
		ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_AlwaysFight);
		ped->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_Aggressive);
		//We want him to flee in case of combat
		ped->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_AlwaysFlee);
	}

	if (IsClone())
	{
		//Copy the cached task slot data
		//  - Due to the task slot cache for a respawn ped being empty when it receives a task specific update.
		m_taskSlotCache = taskSlotData;

#if __BANK
		if(isDeadOrDying)
		{
			NOTFINAL_ONLY( DumpTaskInformation("CNetObjPed::ChangeOwner() - End"); )
		}

		bool isDeadTaskRunning = ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetQueriableInterface() && ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DEAD);
		bool isDyingDeadTaskRunning = ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetQueriableInterface() && ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD);
		bool isDeadActiveTask = ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetTaskActive() && (ped->GetPedIntelligence()->GetTaskActive()->GetTaskType() == CTaskTypes::TASK_DEAD);
		bool isDyingDeadActiveTask = ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetTaskActive() && (ped->GetPedIntelligence()->GetTaskActive()->GetTaskType() == CTaskTypes::TASK_DYING_DEAD); 

		gnetAssertf(!isDeadOrDying || isDeadTaskRunning || isDyingDeadTaskRunning || isDeadActiveTask || isDyingDeadActiveTask
			,"isDeadOrDying[%d] GetDeathState[%d] GetIsDeadOrDying[%d] IsInjured[%d] GetHealth[%f] GetInjuredHealthThreshold[%f] GetPedIntelligence[%p] GetQueriableInterface[%p] IsTaskCurrentlyRunning(CTaskTypes::TASK_DEAD)[%d] IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD)[%d] ped->GetPedIntelligence()->GetTaskActive()->GetTaskType()[%d]"
			,isDeadOrDying
			,ped->GetDeathState()
			,ped->GetIsDeadOrDying()
			,ped->IsInjured()
			,ped->GetHealth()
			,ped->GetInjuredHealthThreshold()
			,ped->GetPedIntelligence()
			,ped->GetPedIntelligence() ? ped->GetPedIntelligence()->GetQueriableInterface() : 0
			,ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetQueriableInterface() ? ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DEAD) : false
			,ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetQueriableInterface() ? ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD) : false
			,ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetTaskActive() ? ped->GetPedIntelligence()->GetTaskActive()->GetTaskType() : -1
			);
		gnetAssert(ped->GetPedIntelligence() && (ped->GetPedIntelligence()->GetTaskDefault() != 0));
#else
		(void)isDeadOrDying;
#endif
	}
	else
	{
		//Remove all weapons
		CPedInventory* pedInventory = ped->GetInventory();
		if(pedInventory)
		{
				pedInventory->RemoveAllWeaponsExcept(OBJECTTYPE_OBJECT, ped->GetDefaultUnarmedWeaponHash());
		}

		if (killPed)
		{
			ped->SetHealth(0.0f, true);
		}

		//Clear weapon damage entity
		ped->ResetWeaponDamageInfo();
		ped->SetWeaponDamageComponent(-1);

		//Resend all dirty data.
		ForceResendAllData();
	}

	gnetAssert(!isDeadOrDying || !ped->GetPedIntelligence() || !ped->GetPedIntelligence()->GetQueriableInterface() || !ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER));
}


// Name         :  SetPlayerRespawnFlags
// Purpose      :   Flag respawn ped with players who have acted on the respawn event.
//                  Flag respawn ped with players who have failed the respawn of the PED.
void
CNetObjPed::SetPlayerRespawnFlags(const bool failed, const netPlayer& player)
{
	if (IsRespawnInProgress())
	{
		if(failed)
		{
			m_RespawnFailedFlags |= (1<<player.GetPhysicalPlayerIndex());
		}

		m_RespawnAcknowledgeFlags |= (1<<player.GetPhysicalPlayerIndex());
	}
}

// Name         :   IsRespawnFailed
// Purpose      :   Returns true if this player has failed to respawn this AI ped. 
bool 
CNetObjPed::IsRespawnFailed(const netPlayer& player) const
{
	if (IsRespawnInProgress())
	{
		return ( (m_RespawnFailedFlags & (1<<player.GetPhysicalPlayerIndex())) != 0 );
	}

	return false;
}

// Name         :   IsRespawnUnacked
// Purpose      :   Returns true if this player has unacked on the respawn events. 
bool 
CNetObjPed::IsRespawnUnacked(const netPlayer& player) const
{
	if (IsRespawnInProgress())
	{
		return ( (m_RespawnAcknowledgeFlags & (1<<player.GetPhysicalPlayerIndex())) == 0 );
	}

	return false;
}

// Name         :   IsRespawnInProgress
// Purpose      :   Returns true if this AI ped is a respawn PED waiting for all players to act on the respawn event.
bool 
CNetObjPed::IsRespawnInProgress() const
{
	return IsLocalFlagSet(CNetObjGame::LOCALFLAG_RESPAWNPED);
}

// Name         :   IsRespawnFailed
// Purpose      :   Returns true if any player has failed to respawn this AI ped. 
bool 
CNetObjPed::IsRespawnFailed( ) const
{
	bool result = false;

	if (IsRespawnInProgress())
	{
		const unsigned          numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer* const* remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers && !result; index++)
		{
			const netPlayer* player = remoteActivePlayers[index];

			if(IsRespawnFailed(*player))
			{
				result = true;
			}
		}
	}

	return result;
}

// Name         :   IsRespawnUnacked
// Purpose      :   Returns true if this AI ped still has player's that have not acted on the respawn event...
bool 
CNetObjPed::IsRespawnUnacked( ) const
{
	bool result = false;

	if (IsRespawnInProgress())
	{
		const unsigned          numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer* const* remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers && !result; index++)
		{
			const netPlayer* player = remoteActivePlayers[index];

			if(IsRespawnUnacked(*player))
			{
				result = true;
			}
		}
	}

	return result;
}

// Name         :   IsRespawnPendingCloneRemove
// Purpose      :   Once all players have all acked event, wait until ped is not cloned on failed players.
bool 
CNetObjPed::IsRespawnPendingCloneRemove( ) const
{
	bool result = false;

	if (IsRespawnInProgress() && !IsRespawnUnacked())
	{
		const unsigned          numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer* const* remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers && !result; index++)
		{
			const netPlayer* player = remoteActivePlayers[index];

			//HasBeenCloned is true already... so what can I check here?
			if(IsRespawnFailed(*player) && HasBeenCloned(*player))
			{
				result = true;
			}
		}
	}

	return result;
}


// Name         :   RemoteVaultMbrFix
// Purpose      :   Checks if remote players are in clamberStand state while vaulting and applies 0 MBR so that the local blender doesn't make them run
void CNetObjPed::RemoteVaultMbrFix()
{
	CPed* ped = GetPed();

	if (ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT) && (ped->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_VAULT) == CTaskVault::State_ClamberStand))
	{
		NetworkInterface::OverrideMoveBlendRatiosThisFrame(*ped, 0.0f, 0.0f);
	}
}

// Name         :   RegisterKillWithNetworkTracker
// Purpose      :   on death, the kill is registered with the NetworkDamageTracker...
// Parameters   :   none
// Returns      :   void
void CNetObjPed::RegisterKillWithNetworkTracker(void)
{
	CPed *pPed = GetPed();

	if( pPed )
	{
		// DON'T test IsDead( ) here as it's not set until the ped hits the floor in TaskDyingDead
		// This function is called just after the damage to the ped takes place so test health level...
		if( pPed->IsFatallyInjured() )
		{
			// check if this ped is dead (or about to die) and has not been registered with the kill tracking code yet
			if(!m_registeredWithKillTracker)
			{
				CNetGamePlayer *player = NetworkUtils::GetPlayerFromDamageEntity(pPed, pPed->GetWeaponDamageEntity());

				if(player && player->IsMyPlayer())
				{
					CNetworkKillTracker::RegisterKill(pPed->GetModelIndex(), pPed->GetWeaponDamageHash(), !pPed->PopTypeIsMission() );

					// Move this inside so we can't claim we've registered a kill when we havent (i.e. (timeSinceLastDamaged < KILL_TRACKING_MIN_DAMAGE_TIME) failed))
					m_registeredWithKillTracker = true;
				}
			}
		}
	}
}

// Name         :   RegisterHeadShotWithNetworkTracker
// Purpose      :   on head shot death, the head shot is registered with the NetworkDamageTracker...
// Parameters   :   none
// Returns      :   void

void CNetObjPed::RegisterHeadShotWithNetworkTracker(void)
{
	CPed *pPed = GetPed();

	bool killedWithHeadshot = false;

	//Check if this ped is head shotted and is dead or dying...
	const int nHitComponent = pPed->GetWeaponDamageComponent();
	if (nHitComponent > -1)
	{
		gnetAssertf(nHitComponent > RAGDOLL_INVALID && nHitComponent < RAGDOLL_NUM_COMPONENTS, "Invalid RAGDOLL component=\"%d\"", nHitComponent);
		killedWithHeadshot = (nHitComponent == RAGDOLL_NECK || nHitComponent == RAGDOLL_HEAD);
	}

	if( pPed )
	{
		// DON'T test IsDead( ) here as it's not set until the ped hits the floor in TaskDyingDead
		// This function is called just after the damage to the ped takes place so test health level...
		if( killedWithHeadshot && pPed->IsFatallyInjured() )
		{
			if(!m_registeredWithHeadshotTracker)
			{
				CNetGamePlayer *player = NetworkUtils::GetPlayerFromDamageEntity(pPed, pPed->GetWeaponDamageEntity());

				if(player && player->IsMyPlayer())
				{
					CNetworkHeadShotTracker::RegisterHeadShot(pPed->GetModelIndex(), pPed->GetWeaponDamageHash());

					// Move this inside so we can't claim we've registered a kill when we havent...
					m_registeredWithHeadshotTracker = true;
				}
			}
		}	
	}
}

#define TIME_BETWEEN_CLONE_COMMUNICATION (5000)

void CNetObjPed::CreateCloneCommunication()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed && pPed->GetPedType() == PEDTYPE_COP && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		CEntity* pTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();

		if( pTarget && pTarget->GetIsTypePed())
		{
			CPed* pPedTarget = ((CPed*)pTarget);
			
			if (pPedTarget->IsLocalPlayer())
			{
				pPedTarget->GetPlayerWanted()->ReportPoliceSpottingPlayer(pPed, VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()), pPedTarget->GetPlayerWanted()->m_WantedLevel, false);
			}
		}
	}
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns      :   True if the object wants to unregister itself
bool CNetObjPed::Update()
{
#if __DEV
    AssertPedTaskInfosValid();
#endif

    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (!pPed)
		return false;

#if __BANK
	if (!pPed->IsDead() && (pPed->IsCopPed() || pPed->IsSwatPed()))
	{
		CPedInventoryDataNode currdata;
		GetPedInventoryData(currdata);

		if (m_NumInventoryItems > 0)
		{
			if (currdata.m_numItems == 0)
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "COP_PED_INVENTORY", GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("*Error*", "Now Empty");

				m_NumInventoryItems = 0;
			}
		}
		else 
		{
			m_NumInventoryItems = currdata.m_numItems;
		}

		if (m_PedUsingInfiniteAmmo)
		{
			if (!currdata.m_allAmmoInfinite)
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "COP_PED_INVENTORY", GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("*Error*", "Ammo not infinite");

				gnetAssertf(currdata.m_allAmmoInfinite, "Cop ped %s has lost infinite ammo", GetLogName());

				m_PedUsingInfiniteAmmo = false;
			}
		}
		else
		{
			m_PedUsingInfiniteAmmo = currdata.m_allAmmoInfinite;
		}
	}
#endif

	if (IsClone())
	{
		RemoteVaultMbrFix();

		if (!IsInCutsceneLocallyOrRemotely())
		{
			if(!m_OverridingMoveBlendRatios)
			{
				CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

				bool bTennis = false;

				if(pPed->IsPlayer())
				{
					//If a player is currently running tennis motion task don't allow clearing the desired MBR
					CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
					if (pTask)
					{
						if(pTask->GetTaskType() == CTaskTypes::TASK_MOTION_TENNIS)
						{
							bTennis = true;
						}
					}
				}
			
				if(!bTennis && netBlenderPed->HasStoppedPredictingPosition())
				{
					// if the blender for this ped has stopped predicting position (due to not receiving a
					// position update for too long) stop the ped running/walking
					pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
				}
				else
				{
					// ensure the ped is always using the last received values for the move blend ratio and desired heading
					// other code systems can alter this and leaves clones in an incorrect state
					pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_DesiredMoveBlendRatioY, m_DesiredMoveBlendRatioX);
					pPed->GetMotionData()->SetScriptedMaxMoveBlendRatio(m_DesiredMaxMoveBlendRatio);
				}
			}

			if(!pPed->GetIsAttached() && !NetworkHeadingBlenderIsOverridden())
			{
				pPed->SetDesiredHeading(m_DesiredHeading);
			}

			UpdatePendingMovementData();
			UpdateCachedWeaponInfo();
			UpdateWeaponStatusFromPed();
			UpdateVehicleForPed();
			UpdateMountForPed();
			UpdatePendingPropData();
			AddRequiredProp();

			// check the pending look at state
			if(m_pendingLookAtObjectId != NETWORK_INVALID_OBJECT_ID)
			{
				netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_pendingLookAtObjectId);
				CEntity* pEntity = pNetObj ? pNetObj->GetEntity() : 0;
				if(pEntity && (!pPed->GetIkManager().IsLooking() || (pEntity != pPed->GetIkManager().GetLookAtEntity())))
				{
					// trigger look at
					eAnimBoneTag boneTag = pEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID;
					pPed->GetIkManager().LookAt(0, pEntity, -1, boneTag, NULL, m_pendingLookAtFlags, 500, 500, CIkManager::IK_LOOKAT_MEDIUM);
				
					// reset look at pending state
					m_pendingLookAtObjectId = NETWORK_INVALID_OBJECT_ID;
				}
			}

			// check synced scene state
			if(m_RunningSyncedScene)
			{
				const unsigned SYNCED_SCENE_CHECK_FRAME = 16;
				if((fwTimer::GetSystemFrameCount() & (SYNCED_SCENE_CHECK_FRAME - 1)) == 0)
				{
					if(!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE))
					{
						int syncedSceneID = pPed->GetPedIntelligence()->GetQueriableInterface()->GetNetworkSynchronisedSceneID();

						if(syncedSceneID != -1 && !CNetworkSynchronisedScenes::IsSceneActive(syncedSceneID))
						{
							CNetworkSynchronisedScenes::CheckSyncedSceneStateForPed(pPed);
						}
					}
				}
			}
		}

		//Set remotely synced PRF values

		//Also applies in cutscenes as related to 
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions,m_PRF_disableInVehicleActions);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatManager,m_PRF_IgnoreCombatManager);
	}
	else
	{
		//Once all players have all acked event, wait until ped is not cloned on failed players
		UpdateLocalRespawnFlagForPed();

        // keep track of what vehicle the ped is in - this is required when the object is unregistered
        // to prevent the vehicle they were in from migrating until the unregistration process is complete
        u32 seat = 0;
        GetVehicleData(m_targetVehicleId, seat);
        m_targetVehicleSeat = static_cast<s32>(seat) - 1;

		// locally owned peds should always be allowed to set tasks on themselves (unless they are pending owner change)
		gnetAssert(CanSetTask() || IsPendingOwnerChange());

        // MP cutscenes overrides the collision state so we can't process this code here
        if(!NetworkInterface::IsInMPCutscene())
        {
		    // some tasks change the collision state of the ped while they are running at key points in the
            // task that will be done when the tasks run as clone tasks on remote machines, we don't want to sync
            // this state as the local ped will be running ahead of the remote ped
            bool overridingCollision = false;
			atString overridingTaskName;

            CTask *activeTask = pPed->GetPedIntelligence()->GetTaskActive();
            while(activeTask)
            {
                if(activeTask->IsClonedFSMTask())
                {
                    CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(activeTask);
		            if (taskFSMClone && taskFSMClone->OverridesCollision())
		            {
                        overridingCollision = true;
#if !__NO_OUTPUT
						overridingTaskName = activeTask->GetName();
#endif
                    }
                }

                activeTask = activeTask->GetSubTask();
            }

            if(overridingCollision && !IsPendingOwnerChange() && !pPed->m_nDEflags.bFrozen)
            {
				char reasonText[256];
				formatf(reasonText, "Tasks overriding collision: %s", overridingTaskName.c_str());
                SetOverridingRemoteCollision(true, m_UsesCollisionBeforeTaskOverride, reasonText);
            }
            else
            {
                m_UsesCollisionBeforeTaskOverride = pPed->IsCollisionEnabled();

                if(GetOverridingRemoteCollision())
                {
                    SetOverridingRemoteCollision(false, m_UsesCollisionBeforeTaskOverride, "Tasks overriding collision");
                }
            }
        }

        UpdateForceSendRequests();

		TUNE_INT(RESPAWN_DEAD_PED_FADEOUT_TIMER, 1000, 0, 5000, 100);

		//Flag dead ped's for removal.
		if (m_RespawnRemovalTimer > 0 && !IsRespawnInProgress())
		{
			if (sysTimer::GetSystemMsTime() > m_RespawnRemovalTimer)
			{
				if (pPed->CanBeDeleted(true, true) && !PARAM_nodeadpedremoval.Get())
				{
					gnetDebug1("Flag Ped %s (0x%p) for deletion.", GetLogName(), pPed);
					FlagForDeletion();
					m_RespawnRemovalTimer = 0;
				}
			}
			else if (sysTimer::GetSystemMsTime() > m_RespawnRemovalTimer - RESPAWN_DEAD_PED_FADEOUT_TIMER && !IsAlphaFading())
			{
				SetAlphaFading(true);
			}
		}

        // check if this ped is standing on a network vehicle, and tell it if so. This allows the
        // network object to move any peds attached to the vehicle if it's position is corrected by the network blender
        CPhysical *groundPhysical = pPed->GetGroundPhysical();
		if (!groundPhysical && pPed->GetLastValidGroundPhysical())
		{
			const bool isJumping = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);
			const bool isInAir   = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
			const bool isFalling = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling);
			bool bPedInAir = (isJumping || isInAir || isFalling);
			if (bPedInAir)
				groundPhysical = pPed->GetLastValidGroundPhysical();
		}

        if(groundPhysical && groundPhysical->GetNetworkObject() && groundPhysical->GetIsTypeVehicle())
        {
            CNetBlenderPhysical *netBlender = SafeCast(CNetBlenderPhysical, groundPhysical->GetNetworkObject()->GetNetBlender());

            if(netBlender)
            {
                netBlender->MarkPedStandingOnObject();
            }
        }
	}

	// The migrate helper should only exist for a frame to handle migration data.
	// We make sure it's cleared here in order to avoid certain cases where it's preseved with bad data.
	if (m_pMigrateTaskHelper &&(fwTimer::GetSystemFrameCount() - m_frameMigrateHelperSet > 2))
	{
		ClearMigrateTaskHelper();
	}

	m_OverridingMoveBlendRatios = false;

	CheckForPedStopping();

	UpdateOrphanedMoveNetworkTaskSubNetwork();

	UpdateHelmet();
	UpdatePhoneMode();
	UpdateTennisMotionData();

#if __ASSERT
	if (pPed->GetPedModelInfo() ? pPed->GetPedModelInfo()->GetPersonalitySettings().GetIsHuman() : false)
	{
		gnetAssertf(!pPed->GetInventory() || pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()) != NULL, "%s, model %s, AnimalAudio=%u, has lost the unarmed weapon", 
			GetLogName(), pPed->GetModelName(), pPed->GetPedModelInfo() ? pPed->GetPedModelInfo()->GetAnimalAudioObject() : 0);
	}
#endif

	// try to assign the ped's relationship group, if it exists (wait until the script that created the ped is running)
	if (m_pendingRelationshipGroup != 0 && ((GetScriptHandlerObject() && GetScriptHandlerObject()->GetScriptHandler()) || pPed->IsPlayer()))
	{
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(m_pendingRelationshipGroup);

		if( pGroup )
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
			m_pendingRelationshipGroup = 0;
		}
	}

    if(m_NeedsNavmeshFixup && !pPed->GetIsInVehicle() && !pPed->GetIsAttached())
    {
        if(HasCollisionLoadedUnderEntity())
        {
            Vector3 position = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

            if(FixupNavmeshPosition(position))
            {
                m_NeedsNavmeshFixup = false;

                if(IsClone())
                {
                    CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

                    pPedBlender->UpdatePosition(position, pPedBlender->GetLastSyncMessageTime());
                    pPedBlender->GoStraightToTarget();
                }
                else
                {
                    pPed->SetPosition(position);
                }
            }
            else
            {
                gnetAssertf(0, "Failed to fixup navmesh position for %s (position is (%.2f, %.2f, %.2f))", GetLogName(), position.x, position.y, position.z);
            }
        }
    }

    UpdatePendingScriptedTask();

    // update the list of peds not responding to bullets
    if(fwTimer::GetSystemFrameCount() != ms_PedsNotRespondingToCloneBulletsUpdateFrame)
    {
        ms_PedsNotRespondingToCloneBulletsUpdateFrame = fwTimer::GetSystemFrameCount();
        ms_NumPedsNotRespondingToCloneBullets         = 0;
    }

    if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets) && 
       gnetVerifyf(ms_NumPedsNotRespondingToCloneBullets < MAX_NUM_NETOBJPEDS, "Too many peds not responding to clone bullets!"))
    {
        ms_PedsNotRespondingToCloneBullets[ms_NumPedsNotRespondingToCloneBullets] = GetObjectID();
        ms_NumPedsNotRespondingToCloneBullets++;
    }

	if (m_RappellingPed && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL))
	{
		// keep the m_RappellingPed flag set while the ped is falling, this is to prevent them disappearing immediately on remote machines when they are shot off the rope
		if (pPed->IsOnGround())
		{
			m_RappellingPed = false;
		}
	}

#if __DEV
	AssertPedTaskInfosValid();
#endif

   return CNetObjPhysical::Update();
}

// name:        UpdateAfterScripts
// description: Called after the scripts have been processed
void CNetObjPed::UpdateAfterScripts()
{
	CNetObjPhysical::UpdateAfterScripts();
	UpdateWeaponVisibilityFromPed();

    // this needs to be cleared later in the frame than the network update
    m_PreventTaskUpdateThisFrame = false;
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjPed::ProcessControl()
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    CNetObjPhysical::ProcessControl();

    if (pPed && IsClone())
    {
		HandleUnmanagedCloneRagdoll();
		
		// we don't clone the wander task, which allows the ped to switch to low LOD motion task updates,
        // so we need to do this here
        if(m_Wandering)
        {
            pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
        }

		// Block if necessary and wait for the anim state to be updated
		pPed->ProcessControl_AnimStateUpdate();

		// Reset a load of flags before the main intelligence update
		pPed->ProcessControl_ResetVariables();

		// Update the peds inventory, weapons and any other accessories that need to be done before the AI update
		pPed->ProcessControl_WeaponsAndAccessoriesPreIntelligence();

		// Update movement mode.
		pPed->UpdateMovementMode();

        // Update the clone task manager
		pPed->GetMotionData()->ProcessPreTaskTrees();

        bool fullUpdate = CPedAILodManager::ShouldDoFullUpdate(*pPed);

		pPed->GetPedIntelligence()->GetEventScanner()->CheckForTranquilizerDamage(*pPed);

        const float dt = fwTimer::GetTimeStep();
	    gnetAssert(dt >= 0.0f);
	    const float fTimeslicedTimeStep = pPed->GetPedIntelligence()->m_fTimeSinceLastAiUpdate + dt;
	    pPed->GetPedIntelligence()->m_fTimeSinceLastAiUpdate = fTimeslicedTimeStep;

		pPed->GetPedIntelligence()->Process_Tasks(fullUpdate, fTimeslicedTimeStep);

        if(fullUpdate)
        {
    		pPed->GetPedIntelligence()->m_fTimeSinceLastAiUpdate = 0.0f;
        }

	    pPed->GetMotionData()->ProcessPostTaskTrees();

#if LAZY_RAGDOLL_BOUNDS_UPDATE
		// Check for some more conditions that should trigger a bounds update
		pPed->GetPedIntelligence()->Process_BoundUpdateRequest();
#endif

		// Update the peds inventory, weapons and any other accessories that need to be done after the AI update
		pPed->ProcessControl_WeaponsAndAccessoriesPostIntelligence(fullUpdate);

		// Process control animation update, contains gestures and facial animation activation
		pPed->ProcessControl_Animation();

		// Graphics update including damage mapping and vfx
		pPed->ProcessControl_Graphics(fullUpdate);

		if(fullUpdate)
		{
			// Update the component reservations.
			pPed->ProcessControl_ComponentReservations();

			// Fire off an asynchronous probe to test if this physical is in a river and how deep it
			// is submerged for the buoyancy code. Don't do this if the ped is in a vehicle or if the
			// ped is using low lod physics.
			//
			// Only do this for peds that are on screen as it's an additional probe.
			Vector3 camPos		= camInterface::GetPos();
			Vector3 clonePos	= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			pPed->ProcessBuoyancyProbe(pPed->GetIsVisibleInSomeViewportThisFrame() || (camPos - clonePos).XYMag() < (20.f * 20.f));

			// Any function calls relating to ped population
			pPed->ProcessControl_Population();

			// Update any audio related members, variables
			pPed->ProcessControl_Audio();
		}

		// Any functions, variables related to physics that are updated during process control
		pPed->ProcessControl_Physics();

        // allow clone tasks to temporarily control the collision flag
        if(pPed->GetPedIntelligence()->NetworkCollisionIsOverridden() && !m_entityConcealed)
        {
		    SetOverridingLocalCollision(pPed->IsCollisionEnabled(), "Tasks overriding collision");
        }

		ProcessLowLodPhysics();

		// We have now updated the motion task (TASK_MOTION_PED) at least once
		// since doing a CNetObjPed::SetMotionGroup call (i.e. since the strafing flag
		// may have changed) - we can now allow the UpdatePendingMovementData() calls
		// to make changes....
		SetForceMotionTaskUpdate(false);
	}

    return true;
}

// Name         :   StartSynchronisation
// Purpose      :
void CNetObjPed::StartSynchronising()
{
    CNetObjPhysical::StartSynchronising();

	// TODO: try to get rid of this, we don't want to be unnecessarily broadcasting task data after every migration 
	ForceResendOfAllTaskData();
}

// Name         :   CreateNetBlender
// Purpose      :
void CNetObjPed::CreateNetBlender()
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		gnetAssert(!GetNetBlender());

		netBlender *blender = rage_new CNetBlenderPed(pPed, ms_pedBlenderDataOnFoot);
		gnetAssert(blender);
        blender->Reset();
        SetNetBlender(blender);
	}
}


// name:        CanDelete
// description: returns true this ped can be deleted
bool CNetObjPed::CanDelete(unsigned* LOGGING_ONLY(reason))
{
	if (!CNetObjPhysical::CanDelete(LOGGING_ONLY(reason)))
		return false;

	//If the respawn is in progress for this ped then we cant delete it
	if (!IsClone() && IsRespawnInProgress())
	{
#if ENABLE_NETWORK_LOGGING
		if(reason)
			*reason = CTD_RESPAWNING;
#endif // ENABLE_NETWORK_LOGGING
		return false;
	}

	return true;
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjPed::CleanUpGameObject(bool bDestroyObject)
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// need to restore the game heap here in case the ped constructor/destructor allocates from the heap
		sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		// we need to remove the ped from any vehicle it is in beforehand, otherwise when the ped is destroyed the code that evicts
		// non-clone peds from vehicles will be called and this causes various crashes (due to the ped not having any control, etc)
		// Note: this needs to be done after the game heap is reinstated as it can result in some memory deallocation (in world quad tree)
		if (IsClone())
        {
			SetClonePedOutOfVehicle(true, 0);

            // restore the local ped ai state
            SetupPedAIAsLocal();

            BANK_ONLY(NetworkDebug::AddPedCloneRemove());
        }

		ClearMigrateTaskHelper(true);


		const bool bPedShouldBeDestroyed = (bDestroyObject && !GetEntityLeftAfterCleanup());
		// unregister the entity with the scripts here when the ped still has a network object ptr and the reservations can be adjusted
		CTheScripts::UnregisterEntity(pPed, !bPedShouldBeDestroyed);

		// we have to call this here as some pop counting depends on whether the ped is a clone or not. When the ped is destroyed it will
		// have no network object.
		CPedPopulation::RemovePedFromPopulationCount(pPed);

		pPed->SetNetworkObject(NULL);

		// delete the game object if it is a clone
		if (bPedShouldBeDestroyed)
		{
			DestroyPed();
		}
		else
		{
			pPed->GetPedIntelligence()->FlushImmediately(true);

			if(pPed->GetNoCollisionFlags() & NO_COLLISION_NETWORK_OBJECTS)
			{
				pPed->ResetNoCollision();

				int collisionFlags = pPed->GetNoCollisionFlags();
				collisionFlags &= ~NO_COLLISION_NETWORK_OBJECTS;
				pPed->SetNoCollisionFlags((u8)collisionFlags); 
			}
		}

		// cleanup any task sequence being used by the ped
		RemoveTemporaryTaskSequence();

		sysMemAllocator::SetCurrent(*oldHeap);
	}
#if __ASSERT
	else
	{
		gnetAssert(0 == GetGameObject());
	}
#endif

	SetGameObject(0);
}

bool CNetObjPed::CanClone(const netPlayer& player, unsigned *resultCode) const
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed && pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
    {
        if(resultCode)
        {
            *resultCode = CC_REMOVE_FROM_WORLD_FLAG;
        }

        return false;
    }

    return CNetObjPhysical::CanClone(player, resultCode);
}

// Name         :   CanSynchronise
// Purpose      :
bool CNetObjPed::CanSynchronise(bool bOnRegistration) const
{
    if (CNetObjPhysical::CanSynchronise(bOnRegistration))
    {
		if(m_PreventSynchronise)
		{
			return false;
		}

        CPed* pPed = GetPed();
		VALIDATE_GAME_OBJECT(pPed);

		if (pPed)
		{
			if (pPed->IsAPlayerPed() || pPed->PopTypeIsMission())
				return true;

			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
                // if the vehicle a ped is in contains a player they need to be synced
                if((pPed->GetMyVehicle() && pPed->GetMyVehicle()->ContainsPlayer()))
				{
					return true;
				}

				// DAN TEMP - this check needs to be done at a higher level
				if(IsLocalFlagSet(CNetObjGame::LOCALFLAG_FORCE_SYNCHRONISE))
				{
					return true;
				}

				return false;
			}

			return true;
		}
    }

    return false;
}

bool CNetObjPed::CanStopSynchronising() const
{
	// ignore non-critical state as some of it will never be sent if the ped is in a vehicle (eg the position)
	return (IsClone() || IsCriticalStateSyncedWithAllPlayers());
}

bool CNetObjPed::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    if (!CNetObjPhysical::CanPassControl(player, migrationType, resultCode))
        return false;

	if (AssertVerify(pPed))
	{
		// don't pass control while respawn ped flag set.
		if (IsRespawnInProgress())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_RESPAWN_IN_PROGRESS;
			}
			return false;
		}

		// don't pass control if the the current tasks to not permit migration
		if(!pPed->m_nDEflags.bFrozen && !pPed->GetPedIntelligence()->NetworkMigrationAllowed(player, migrationType))
		{
            if(resultCode)
            {
                *resultCode = CPC_FAIL_PED_TASKS;
            }
			return false;
		}

		if (pPed->GetPedsGroup() && !pPed->GetPedsGroup()->IsSyncedWithPlayer((const CNetGamePlayer&)player))
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_PED_GROUP_UNSYNCED;
			}
			return false;

		}
	}

    return true;
}

// Name         :   CanAcceptControl
// Purpose      :   Returns true if control of this object can be passed to our peer
// Parameters   :   player - the player passing control
bool CNetObjPed::CanAcceptControl(const netPlayer &player, eMigrationType migrationType, unsigned *resultCode) const
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// the ped may be running a clone task that prevents control passing
	if(pPed && IsClone())
	{
		bool denyAcceptControl = false;

		if ((migrationType != MIGRATE_FROZEN_PED) && !pPed->GetPedIntelligence()->ControlPassingAllowed(player, migrationType))
		{
			denyAcceptControl = true;
		}

		if (denyAcceptControl)
		{
			if(resultCode)
			{
				*resultCode = CAC_FAIL_PED_TASKS;
			}

			return false;
		}

		// we can't accept control if the ped is ragdolling and there is no task in the queriable state to manage the ragdoll. This can happen if the ped has just
		// been hit by a vehicle locally and migrated immediately afterwards. Ignoring this check can result in the assert firing: "A ragdoll has been activated by a collision record, but ProcessRagdollImpact didn't return a ragdoll event"
		if (pPed->GetRagdollInst() && pPed->GetRagdollState() > RAGDOLL_STATE_ANIM_DRIVEN)
		{
			// The owner is Task_Writhing which means it's already completed the TaskNMControl and switched back to animated.
			// If we're not writhing and still running TaskNMControl, it means we're lagging behind and are still in ragdoll
			// Migrating now will cause the new clone to stand up with no tasks running as:
			// 1. CreateCloneTasksFromLocalTask will create a TaskWrithe but
			// 2. When the QI is updated and its not part of the QI, it will get killed off and repalced with a TaskNMControl
			// 3. But that will also immediately get killed off because the ped is already out of ragdoll and is animated.			

			// If i'm ragdolling and using TaskNMControl and the owner is taskwrithing, we're lagging behind and not in a suitable position to migrate...
			if(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_WRITHE, PED_TASK_PRIORITY_MAX, false) &&
				pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL))
			{
					if(resultCode)
					{
						*resultCode = CAC_FAIL_PED_TASKS;
					}

					return false;
			}

			if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL) &&
				!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL))
			{
				if(resultCode)
				{
					*resultCode = CAC_FAIL_PED_TASKS;
				}

				return false;
			}
		}

		if(m_RunningSyncedScene &&
			!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SYNCHRONIZED_SCENE) &&
			!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE))
		{			
			if(resultCode)
			{
				*resultCode = CAC_FAIL_SYNCED_SCENE_NO_TASK;
			}
			return false;			
		}

		// If this is a script ped that is in a vehicle we don't know about, don't accept it yet!
		if (IsScriptObject() && m_inVehicle)
		{
			netObject* vehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_myVehicleId);	

			if (vehicleObj == NULL)
			{
				if(resultCode)
				{
					*resultCode = CAC_FAIL_VEHICLE_I_AM_IN_NOT_CLONED;
				}
				return false;			
			}
		}
	}

	return CNetObjPhysical::CanAcceptControl(player, migrationType, resultCode);
}


u32 CNetObjPed::CalcReassignPriority() const
{
	u32 reassignPriority = CNetObjDynamicEntity::CalcReassignPriority();

	CPed* pPed = GetPed();

    // ped's in vehicles should be reassigned with the vehicle
    if(pPed && pPed->GetIsInVehicle())
    {
        CVehicle *pMyVehicle = pPed->GetMyVehicle();

        if(pMyVehicle && pMyVehicle->GetNetworkObject())
        {
            reassignPriority = pMyVehicle->GetNetworkObject()->CalcReassignPriority();
        }
    }
    else
    {
	    // force peds running a script sequence to migrate to machines running the script, so the sequence does not get lost
	    if (pPed && IsScriptObject())
	    {
		    if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SEQUENCE))
		    {
			    CScriptEntityExtension* extension = pPed->GetExtension<CScriptEntityExtension>();

			    if (extension)
			    {
				    scriptHandler* handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(extension->GetScriptInfo()->GetScriptId());

				    if (handler && handler->GetNetworkComponent() && handler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
				    {
					    reassignPriority |= ((1<<(SIZEOF_REASSIGNPRIORITY))-1);
				    }
			    }
		    }
	    }
    }
	
	return reassignPriority;
}

// name:        CanBlend
// description:  returns true if this object can use the net blender to blend updates
bool CNetObjPed::CanBlend(unsigned *resultCode) const
{
    bool canBlend = CNetObjPhysical::CanBlend(resultCode);

    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    if(ped)
    {
        if(ped->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
        {
            if(resultCode)
            {
                *resultCode = CB_FAIL_IN_VEHICLE;
            }

            canBlend = false;
        }
        else if(NetworkBlenderIsOverridden())
        {
            if(resultCode)
            {
                *resultCode = CB_FAIL_BLENDER_OVERRIDDEN;
            }

            canBlend = false;
        }

        if(m_DesiredMoveBlendRatioX > 0.01f || m_DesiredMoveBlendRatioY > 0.01f)
        {
            CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

            if(netBlenderPed->HasStoppedPredictingPosition())
            {
                if(resultCode)
                {
                    *resultCode = CB_FAIL_STOPPED_PREDICTING;
                }

                canBlend = false;
            }
        }
    }

    return canBlend;
}

// name:         NetworkBlenderIsOverridden
// description:  returns true if the blender is being overridden
bool CNetObjPed::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
    bool overridden = CNetObjPhysical::NetworkBlenderIsOverridden(resultCode);

    if(AssertVerify(GetPed()))
    {
		if (GetPed()->GetIsDeadOrDying())
		{
			if(resultCode)
			{
				*resultCode = NBO_DEAD_PED;
			}

			overridden = true;
		}

        if(GetPed()->GetPedIntelligence()->NetworkBlenderIsOverridden() || m_RunningSyncedScene)
        {
            if(resultCode)
            {
                *resultCode = NBO_PED_TASKS;
            }

            overridden = true;
        }
    }

    return overridden;
}

// name:         NetworkkHeadingBlenderIsOverridden
// description:  returns true if the heading blending is being overridden
bool CNetObjPed::NetworkHeadingBlenderIsOverridden(unsigned *resultCode) const
{
	bool overridden = false;

	if(AssertVerify(GetPed()))
	{
		if (GetPed()->GetIsDeadOrDying())
		{
			if(resultCode)
			{
				*resultCode = NBO_DEAD_PED;
			}

			overridden = true;
		}
		else if(GetPed()->GetPedIntelligence()->NetworkHeadingBlenderIsOverridden())
		{
			if(resultCode)
			{
				*resultCode = NBO_PED_TASKS;
			}

			overridden = true;
		}
		else if (IsInCutsceneLocallyOrRemotely())
		{
			if(resultCode)
			{
				*resultCode = NBO_FAIL_RUNNING_CUTSCENE;
			}

			overridden = true;
		}
	}

	return overridden;
}

// name:         NetworkAttachmentIsOverridden
// description:  returns true if the attachment update is being overridden locally
bool CNetObjPed::NetworkAttachmentIsOverridden() const
{
	bool overridden = netObject::NetworkAttachmentIsOverridden();

	if(AssertVerify(GetPed()))
	{
		overridden |= GetPed()->GetPedIntelligence()->NetworkAttachmentIsOverridden();
	}

	return overridden;
}

//
// name:        IsInScope
// description: This is used by the object manager to determine whether we need to create a
//              clone to represent this object on a remote machine. The decision is made using
//              the player that is passed into the method - this decision is usually based on
//              the distance between this player's players and the network object, but other criterion can
//              be used.
// Parameters:  pPlayer - the player that scope is being tested against
bool CNetObjPed::IsInScope(const netPlayer& player, unsigned* scopeReason) const
{
	gnetAssert(!scopeReason || *scopeReason == 0);

	//Should be out of scope for the players that failed the event respawn
	if (!IsClone() && IsRespawnInProgress() && IsRespawnFailed(player))
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_PED_RESPAWN_FAILED;
		}
		return false;
	}

	// respawn peds wont GameObject when the network object is being registered
	if(!HasGameObject())
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_NO_GAME_OBJECT;
		}
		return false;
	}

	// all checks on whether a ped is in scope due to the vehicle they are in should be
	// added to IsInScopeVehicleChecks(). Any other checks should be added to IsInScopeNoVehicleChecks()
    if(IsInScopeVehicleChecks(player, scopeReason))
    {
        return true;
    }
    else if (!scopeReason || *scopeReason == 0)
	{
        return IsInScopeNoVehicleChecks(player, scopeReason);
	}
	else
	{
		return false;
	}
}

float CNetObjPed::GetScopeDistance(const netPlayer* pRelativePlayer) const
{
	static float MINIMUM_SCOPE_TO_EXCEED_EFFECTIVE_NETWORK_WORLD_GRIDSQUARE_RADIUS = 190.0f;  // Should be > CNetworkWorldGridManager::GRID_SQUARE_SIZE x 2.5 (GRID_SQUARE_SIZE is 75.0f @ May 2013)
	static const float SCOPED_WEAPON_MAX_POP_RANGE_SCOPE_AMBIENT = 200.0f;
	static const float SCOPED_WEAPON_MAX_POP_RANGE_SCOPE_SCRIPT = 500.0f;
	static const float SCOPED_WEAPON_ENTITY_RADIUS = 100.0f;
	static const float EXTENDED_POP_RANGE_SCOPE = 400.0f;
	static const float ROAD_BLOCK_SCOPE = 425.0f;
	static const float EXTENDED_SCENARIO_PED_EXTRA_DIST = 300.0f;
	
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pRelativePlayer)
	{
		const CNetGamePlayer &netgameplayer = static_cast<const CNetGamePlayer &>(*pRelativePlayer);
		CPed* pPlayerPed = netgameplayer.GetPlayerPed();
		CNetObjPlayer const* netObjPlayer = pPlayerPed ? static_cast<CNetObjPlayer*>(pPlayerPed->GetNetworkObject()) : NULL;			

		if (netObjPlayer)
		{
			bool lookingThroughScope = false;
			bool zoomed = false;
			float weaponRange = 0.0f;

			if (netObjPlayer->GetViewport() && NetworkInterface::IsPlayerUsingScopedWeapon(*pRelativePlayer, &lookingThroughScope, &zoomed, &weaponRange) && lookingThroughScope && zoomed)
			{
				Vector3 position = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

				if (netObjPlayer->GetViewport()->IsSphereVisible(position.x, position.y, position.z, SCOPED_WEAPON_ENTITY_RADIUS))
				{
					if (IsOrWasScriptObject())
					{
						return Min(weaponRange, SCOPED_WEAPON_MAX_POP_RANGE_SCOPE_SCRIPT);
					}

					return Min(weaponRange, SCOPED_WEAPON_MAX_POP_RANGE_SCOPE_AMBIENT);
				}
			}

			if(netObjPlayer && netObjPlayer->IsUsingExtendedPopulationRange())
			{
				return EXTENDED_POP_RANGE_SCOPE;
			}
		}

		// create roadblock cops at a greater range if the player is in a car and wanted, so roadblocks are blipped for him
        COrder* pOrder = pPed ? pPed->GetPedIntelligence()->GetOrder() : 0;

		if (pOrder && pOrder->GetDispatchType() == DT_PoliceRoadBlock)
		{
			return ROAD_BLOCK_SCOPE;
		}
	}

	float fScopeDist = CNetObjProximityMigrateable::GetScopeDistance(pRelativePlayer);

	//B* 1377096 
	//To prevent multiple generating peds on same Scenario points ensure that scope distance 
	//for changing ownership is larger than the distance used for allowing generating peds.
	if(GetOverrideScopeBiggerWorldGridSquare())
	{
		fScopeDist = MINIMUM_SCOPE_TO_EXCEED_EFFECTIVE_NETWORK_WORLD_GRIDSQUARE_RADIUS;
	}

	const CScenarioPoint* pPoint = pPed->GetScenarioPoint(*pPed,true);
	if (pPoint && pPoint->HasExtendedRange())
	{
		fScopeDist = MAX(fScopeDist, EXTENDED_SCENARIO_PED_EXTRA_DIST);
	}

	return fScopeDist;
}

Vector3 CNetObjPed::GetPosition() const
{
    CPed* pPed = GetPed();
    VALIDATE_GAME_OBJECT(pPed);

    if(pPed && pPed->GetUsingRagdoll() && pPed->GetRagdollInst())
    {
        Matrix34 matComponent = MAT34V_TO_MATRIX34(pPed->GetMatrix());
        pPed->GetRagdollComponentMatrix(matComponent, RAGDOLL_BUTTOCKS);
        return matComponent.d;
    }
    else
    {
        return CNetObjPhysical::GetPosition();
    }
}

bool CNetObjPed::IsInScopeNoVehicleChecks(const netPlayer& player, unsigned *scopeReason) const
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// peds midway through a rappel and out of their vehicle on the rope:
	if (m_RappellingPed && !pPed->GetIsInVehicle())
	{
		if (!HasBeenCloned(player))
		{
			// don't clone these peds - the rappel task currently does not cope with this (jumping straight to a descend state)
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_PED_RAPPELLING_NOT_CLONED;
			}
			return false;
		}
		else if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetNetworkObject())
		{
			// keep the peds cloned on machines that have their vehicle, previously they may be removed while on the rope
			if (!pPed->GetMyVehicle()->IsNetworkClone())
			{
				if (pPed->GetMyVehicle()->GetNetworkObject()->HasBeenCloned(player))
				{
					if (scopeReason)
					{
						*scopeReason = SCOPE_IN_PED_RAPPELLING_HELI_CLONED;
					}
					return true;
				}
			}
			else
			{
				if (pPed->GetMyVehicle()->GetNetworkObject()->IsInScope(player))
				{
					if (scopeReason)
					{
						*scopeReason = SCOPE_IN_PED_RAPPELLING_HELI_IN_SCOPE;
					}
					return true;
				}
			}
		}
	}

	return CNetObjPhysical::IsInScope(player, scopeReason);
}

bool CNetObjPed::IsInScopeVehicleChecks(const netPlayer& player, unsigned *scopeReason) const
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// peds in cars have the same scope as their vehicle
	if(pPed && !IsGlobalFlagSet(netObject::GLOBALFLAG_CLONEALWAYS))
	{
		CVehicle *pVehicle = pPed->GetIsInVehicle()?pPed->GetMyVehicle():NULL;

		if(!pVehicle)
		{   //if no vehicle is so far seen then see if ped is about to get into vehicle
			CTaskEnterVehicle *enterVehTask = (CTaskEnterVehicle*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
			if (enterVehTask)
			{
				pVehicle = enterVehTask->GetVehicle();
			}
		}

		if (pVehicle)
		{
			netObject *pVehicleNetObj = pVehicle->GetNetworkObject();

			gnetAssertf(pVehicleNetObj || NetworkInterface::IsInMPCutscene() || pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD), "%s is in a vehicle with no network object (%p)!", GetLogName(), pVehicle);

			if (pVehicleNetObj)
			{
				if(pVehicleNetObj->IsClone())
				{
					if (pVehicle->GetDriver() == pPed)
					{
						netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

						NetworkLogUtils::WriteLogEvent(log, "Ped owned by this machine is in a vehicle controlled by another machine!", "");
						log.WriteDataValue("PED", GetLogName());
						log.WriteDataValue("PED OWNER", "%s", GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "REASSIGNING");
						log.WriteDataValue("VEHICLE", pVehicleNetObj->GetLogName());
						log.WriteDataValue("VEHICLE OWNER", "%s", pVehicleNetObj->GetPlayerOwner() ? pVehicleNetObj->GetPlayerOwner()->GetLogName() : "REASSIGNING");
					}

					if (player.GetPhysicalPlayerIndex() == pVehicleNetObj->GetPhysicalPlayerIndex())
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_IN_PED_DRIVER_VEHICLE_OWNER;
						}
						return true;
					}
				}
				else 
				{
					if (pVehicleNetObj->HasBeenCloned(player) && !pVehicleNetObj->IsPendingRemoval(player))
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_IN_PED_VEHICLE_CLONED;
						}
						return true;
					}
					else if (!(IsGlobalFlagSet(GLOBALFLAG_CLONEALWAYS_SCRIPT) && GetScriptObjInfo() && GetScriptObjInfo()->IsScriptHostObject())) // don't hold back script peds as this can delay script progression
					{
						if (scopeReason)
						{
							*scopeReason = SCOPE_OUT_PED_VEHICLE_NOT_CLONED;
						}
						return false;
					}
				}
			}
		}
	}

    return false;
}

// name:        ManageUpdateLevel
// description: Decides what the default update level should be for this object
void CNetObjPed::ManageUpdateLevel(const netPlayer& player)
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

#if __BANK
	if(ms_updateRateOverride >= 0)
	{
		SetUpdateLevel( player.GetPhysicalPlayerIndex(), static_cast<u8>(ms_updateRateOverride) );
		return;
	}
#endif

	CNetObjPhysical::ManageUpdateLevel(player);
	
	CPed *playerPed = ((CNetGamePlayer&)player).GetPlayerPed();

	if(playerPed)
	{
		// peds inside cars the player is trying to enter need to be set to a very high update level
		if(playerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskInfo *taskInfo = playerPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX);

			if(taskInfo)
			{
				CClonedEnterVehicleInfo *enterTask = SafeCast(CClonedEnterVehicleInfo, taskInfo);

				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle() && pPed->GetMyVehicle() == enterTask->GetVehicle())
				{
					SetUpdateLevel( player.GetPhysicalPlayerIndex(), CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH );
				}
			}
		}

		gnetAssert(pPed->GetPedIntelligence());
		gnetAssert(pPed->GetPedIntelligence()->GetQueriableInterface());

		//Peds aiming at the player need to be set to a very high update level so firing sounds arrive at same rate as hit sounds from CWeaponDamageEvent 
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN))
		{
			CTaskInfo *taskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_GUN, PED_TASK_PRIORITY_MAX);

			if(taskInfo)
			{
				CClonedGunInfo *gunTask = SafeCast(CClonedGunInfo, taskInfo);
				if(gunTask && gunTask->GetWeaponTargetHelper().GetTargetEntity() && gunTask->GetWeaponTargetHelper().GetTargetEntity()->GetIsTypePed() && (CPed *)gunTask->GetWeaponTargetHelper().GetTargetEntity()==playerPed)
				{
					SetUpdateLevel( player.GetPhysicalPlayerIndex(), CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH );
				}
			}
		}

		// don't allow the ped's update level to drop too low if the ped can potentially viewed through a scope
        bool potentiallyVisible        = !NetworkDebug::UsingPlayerCameras() || IsVisibleToPlayer(player);
        u8   minimumUpdateLevelToCheck = IsImportant() ? (u8) CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH : (u8) CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;

		if(potentiallyVisible && GetUpdateLevel(player.GetPhysicalPlayerIndex()) < minimumUpdateLevelToCheck)
		{
			bool lookingThroughScope = false;
			bool zoomed = false;

			if (NetworkInterface::IsPlayerUsingScopedWeapon(player, &lookingThroughScope, &zoomed) && lookingThroughScope && zoomed)
			{
				SetUpdateLevel( player.GetPhysicalPlayerIndex(), minimumUpdateLevelToCheck);
			}
		}
	}
}

u8 CNetObjPed::GetLowestAllowedUpdateLevel() const
{
    CPed *pPed = GetPed();

    if(pPed && pPed->IsLawEnforcementPed() && m_TimeToForceLowestUpdateLevel==0)
    {
        return CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
    }
    else
    {
        return CNetObjPhysical::GetLowestAllowedUpdateLevel();
    }
}

void CNetObjPed::ForceHighUpdateLevel()
{
	static const unsigned TIME_TO_FORCE_UPDATE = 2000;
	SetLowestAllowedUpdateLevel(CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH, TIME_TO_FORCE_UPDATE);
}


// name:		CNetObjPed::ChangePed
// description: Called when the ped changes game object.
void CNetObjPed::ChangePed(CPed* ped, const bool recreateBlender, const bool setupPedAIAsClone)
{
	netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "CHANGE_PED", GetLogName());
#if !__FINAL
	log.WriteDataValue("Model", "%s", AssertVerify(ped) ? CModelInfo::GetBaseModelInfoName(ped->GetModelId()) : "Null Ped");
#endif
	log.WriteDataValue("Ped:", "%s", IsClone() ? "Clone" : "Local");
	log.WriteDataValue("Recreate Blender:", "%s", recreateBlender ? "True" : "False");

	CPed* pLastPed = GetPed();

	// the dying dead task can be overriding the props for the dead player, clear this here 
	if (IsClone() && pLastPed && pLastPed->IsPlayer() && pLastPed->IsDead())
	{
		SetTaskOverridingPropsWeapons(false);
	}

	gnetAssert(GetGameObject());
	SetGameObject(0);
	gnetAssert(GetGameObject() == 0);
	SetGameObject(ped);

	if (recreateBlender)
	{
		//Recreate Blender pointer
		DestroyNetBlender();
		CreateNetBlender();
	}

	if(IsClone() && setupPedAIAsClone)
	{
		SetupPedAIAsClone();
	}

	// the ghost collision slot code keeps at ptr to the physical entity, so we need to copy the collision slot in the network object across to the new network object representing the old ped entity 
	// and clear our current one, as this network object is being assigned to a different entity. 
	if (GetGhostCollisionSlot() != CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT)
	{
		CNetObjPed* pPedObj = ped->GetNetworkObject() ? static_cast<CNetObjPed*>(ped->GetNetworkObject()) : NULL;

		if (gnetVerify(pPedObj))
		{
			pPedObj->SetGhostCollisionSlot(GetGhostCollisionSlot());
		}

		SetGhostCollisionSlot(CNetworkGhostCollisions::INVALID_GHOST_COLLISION_SLOT);
	}
}


// name:		CNetObjPed::SetGameObject
// description: Set a valid game object associated with the network object.
void CNetObjPed::SetGameObject(void* gameObject)
{
	CNetObjProximityMigrateable::SetGameObject(gameObject);

	ResetTaskSlotData();

	if (!IsClone() && HasGameObject())
	{
		ForceResendOfAllTaskData();
	}
}

void CNetObjPed::OnUnregistered()
{
    // if this is a local ped that was in a vehicle tell the vehicle is has an unregistering occupant
    if(!IsClone() && m_targetVehicleId != NETWORK_INVALID_OBJECT_ID)
    {
        netObject *networkObject = CNetwork::GetObjectManager().GetNetworkObject(m_targetVehicleId);

        if(networkObject && !networkObject->IsClone())
        {
            gnetAssertf(!networkObject->IsPendingOwnerChange() || NetworkInterface::GetObjectManager().IsShuttingDown(), "Unregistering a ped from a vehicle pending owner change!");
            
            if(gnetVerifyf(IsVehicleObjectType(networkObject->GetObjectType()), "Unregistering a ped with a vehicle ID that is not a vehicle!"))
            {
                CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, networkObject);
                netObjVehicle->SetHasUnregisteringOccupants();
            }
        }
    }

	CNetObjPhysical::OnUnregistered();
}

bool CNetObjPed::OnReassigned()
{
	if (!IsClone())
	{
		CPed* pPed = GetPed();

		if (pPed)
		{
			// if the ped has been reassigned locally, and is the leader of a group, we need to dirty the group here so that the arbitrator of the group
			// is set to our local player (the arbitrator will have been lost because the group leader has no owner when on the reassignment list)
			CPedGroup* pPedGroup = pPed->GetPedsGroup();

			if (pPedGroup && pPedGroup->GetGroupMembership()->GetLeader() == pPed && pPedGroup->IsLocallyControlled())
			{
				pPedGroup->SetDirty();
			}
		}
	}

	return CNetObjPhysical::OnReassigned();
}

void CNetObjPed::PostCreate()
{
    CNetObjPhysical::PostCreate();

    if(IsClone())
	{
		CPed *ped = GetPed();

		if(ped)
		{
            CNetworkSynchronisedScenes::PostTaskUpdateFromNetwork(ped);

			// In ::SetPedAppearanceData we block motorbike helmet syncing to let the clone sort itself out
			// if the ped is putting a helmet on or taking it off - in case a ped is coming in to scope during 
			// this, we re-apply the props here...
			CPedPropsMgr::SetPedPropsPacked(ped, m_pendingPropUpdateData);
			ped->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet, IsOwnerWearingAHelmet());

			// make sure we do an camera anim update now so the ped gets posed correctly
			ped->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );

            // ensure the ped is put in the vehicle he is in after creation, even if
            // currently running tasks on the owner machine would usually prevent this
            if(!ped->GetIsInVehicle() && m_targetVehicleId != NETWORK_INVALID_OBJECT_ID && m_targetVehicleSeat != -1)
            {
                UpdateVehicleForPed(true);
            }
		}

        BANK_ONLY(NetworkDebug::AddPedCloneCreate());
    }
}

void CNetObjPed::PostSync()
{
    CNetObjPhysical::PostSync();

    if(GetSyncTree() && static_cast<CProjectSyncTree*>(GetSyncTree())->GetPositionWasUpdated())
    {
        m_UpdateBlenderPosAfterVehicleUpdate = false;
    }
}

void CNetObjPed::PostMigrate(eMigrationType migrationType)
{
    CNetObjPhysical::PostMigrate(migrationType);

    if (!IsClone())
	{
		CPed *ped = GetPed();

		if(ped && !ped->GetVelocity().IsZero())
		{
			// force sending of important state
			CPedSyncTree *pedSyncTree = SafeCast(CPedSyncTree, GetSyncTree());

			if(pedSyncTree)
			{
				pedSyncTree->ForceSendOfPositionData(*this, GetActivationFlags());
			}
		}
	}
}

void CNetObjPed::ConvertToAmbientObject()
{
	if(!IsClone())
	{
		CPed* pPed = GetPed();
		if(pPed && pPed->GetIkManager().IsLooking())
		{
			pPed->GetIkManager().AbortLookAt(500);
			m_pendingLookAtObjectId = NETWORK_INVALID_OBJECT_ID;
		}
	}

	CNetObjPhysical::ConvertToAmbientObject();
}

void CNetObjPed::OnConversionToAmbientObject()
{
	bool bBlockNonTempEvents = GetPed() ? GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents ) : false;
	bool bDoesntDropWeaponsWhenDead = GetPed() ? GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead ) : false;
	bool bStopWeaponFiringOnImpact = GetPed() ? GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact ) : false;
	bool bAllowContinuousThreatResponseWantedLevelUpdates = GetPed() ? GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates ) : false;
	bool bDontBehaveLikeLaw =  GetPed() ? GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw ) : false;

	CCombatData::Movement nCombatMovement = GetPed() ? GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() : CCombatData::CM_Stationary;

	CombatBehaviourFlags combatBehaviourFlags = GetPed() ? GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatBehaviorFlags() : CombatBehaviourFlags();

	fwFlags32 fleeBehaviourFlags = GetPed() ? GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags() : fwFlags32();

	CNetObjPhysical::OnConversionToAmbientObject();

	if(GetPed())
	{
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents, bBlockNonTempEvents );
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead, bDoesntDropWeaponsWhenDead );
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact, bStopWeaponFiringOnImpact );
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates, bAllowContinuousThreatResponseWantedLevelUpdates );
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw, bDontBehaveLikeLaw );

		GetPed()->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(nCombatMovement);
		GetPed()->GetPedIntelligence()->GetCombatBehaviour().SetCombatBehaviorFlags(combatBehaviourFlags);
		GetPed()->GetPedIntelligence()->GetFleeBehaviour().SetFleeFlags(fleeBehaviourFlags);
	}
}

void  CNetObjPed::PlayerHasJoined(const netPlayer& player)
{
	SetPlayerRespawnFlags(true, player);
	CNetObjPhysical::PlayerHasJoined(player);
}

void  CNetObjPed::PlayerHasLeft(const netPlayer& player)
{
	if (m_AttDamageToPlayer == player.GetPhysicalPlayerIndex())
	{
		m_AttDamageToPlayer = INVALID_PLAYER_INDEX;
	}

	SetPlayerRespawnFlags(true, player);
	CNetObjPhysical::PlayerHasLeft(player);
}

// name:		CNetObjEntity::CanOverrideCollision
//
bool CNetObjPed::CanEnableCollision() const
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	bool bCanEnableCollision = CNetObjPhysical::CanEnableCollision();

	if (pPed)
	{
		// peds in vehicles can't have their collision tampered with
		CVehicle* pPedVehicle = pPed->GetIsInVehicle() ? pPed->GetMyVehicle() : NULL;

		if (pPedVehicle)
		{
			bCanEnableCollision = false;

			const s32 seat = pPedVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

			if (pPedVehicle->IsSeatIndexValid(seat))
			{
				const CVehicleSeatAnimInfo* pSeatAnimInfo = pPedVehicle->GetSeatAnimationInfo(seat);
				if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
				{
					bCanEnableCollision = true;
				}
			}
		}
		else
		{
			// peds in low lod have their collision disabled, so cannot have it enabled while in low lod
			if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) )
			{
				bCanEnableCollision = false;
			}
		}
	}

	return bCanEnableCollision;
}


//
// name:        CNetObjEntity::TryToPassControlProximity
// description: Tries to pass control of this entity to another machine if a player from that machine is closer
void CNetObjPed::TryToPassControlProximity()
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		gnetAssert(!IsClone());

		// forcibly try to pass any occupants that migrate with the vehicle onto the owner of their vehicle
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && (!IsScriptObject() || pPed->GetMyVehicle()->GetDriver()==pPed))
		{
			if (pPed->GetMyVehicle()->IsNetworkClone())
			{
				CNetGamePlayer *pPlayer = pPed->GetMyVehicle()->GetNetworkObject()->GetPlayerOwner();

				if (pPlayer && pPlayer->CanAcceptMigratingObjects() &&
					CanPassControl(*pPlayer, MIGRATE_FORCED))
				{
					if (!GetSyncData())
					{
						CNetObjPhysical::StartSynchronising();
					}

					CGiveControlEvent::Trigger(*pPlayer, this, MIGRATE_FORCED);
				}
			}
		}
		else
		{
			CNetObjDynamicEntity::TryToPassControlProximity(); // bypass CNetObjPhysical::TryToPassControlProximity() as we don't want to run that code for peds
		}
	}
}

// Name         :   TryToPassControlOutOfScope
// Purpose      :   Tries to pass control of this entity when it is going out of scope with our players.
// Parameters   :   pPlayer - if this is set we have to pass control to this player
// Returns      :   true if object is not in scope with any other players and can be deleted.
bool CNetObjPed::TryToPassControlOutOfScope()
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		if (!NetworkInterface::IsGameInProgress())
			return true;

		gnetAssert(!IsClone());

        netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

		if (IsPendingOwnerChange())
        {
            NetworkLogUtils::WriteLogEvent(log, "FAILED_TO_PASS_OUT_OF_SCOPE", GetLogName());
            log.WriteDataValue("Reason", "Pending owner change");
			return false;
        }

		if (!HasBeenCloned() &&
			!IsPendingCloning() &&
			!IsPendingRemoval())
		{
			return true;
		}

		if (IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
        {
			return false;
        }

        const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
        u32 numPlayers = NetworkUtils::GetClosestRemotePlayersInScope(*this, closestPlayers);

        unsigned failReasons[MAX_NUM_PHYSICAL_PLAYERS];

        for(u32 index = 0; index < numPlayers; index++)
        {
            if (CanPassControl(*closestPlayers[index], MIGRATE_OUT_OF_SCOPE, &failReasons[index]))
            {
                if (!GetSyncData())
                {
                    StartSynchronising();
                }

                CGiveControlEvent::Trigger(*closestPlayers[index], this, MIGRATE_OUT_OF_SCOPE);
                return false;
            }
        }

#if ENABLE_NETWORK_LOGGING
        if(numPlayers > 0)
        {
            NetworkLogUtils::WriteLogEvent(log, "FAILED_TO_PASS_OUT_OF_SCOPE", GetLogName());
            for(u32 index = 0; index < numPlayers; index++)
            {
                log.WriteDataValue(closestPlayers[index]->GetLogName(), NetworkUtils::GetCanPassControlErrorString(failReasons[index]));
            }
        }
#endif // ENABLE_NETWORK_LOGGING

        return (numPlayers==0); // we need to keep retrying if a player is nearby - we can't remove the object in front of him!
	}

	return true;
}

void CNetObjPed::TryToPassControlFromTutorial()
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		gnetAssert(!IsClone());

		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
		    CNetObjPhysical::TryToPassControlFromTutorial();
		}
	}
}

// sync parser getter functions
void CNetObjPed::GetPedCreateData(CPedCreationDataNode& data)
{
    CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	FatalAssertf(HasGameObject(), "Network Object %s does not have a GameObject.", GetLogName());

	if (HasGameObject() && gnetVerify(pPed))
	{
        CBaseModelInfo *modelInfo = pPed->GetBaseModelInfo();
        if(gnetVerifyf(modelInfo, "No model info for networked object!"))
        {
            data.m_modelHash = modelInfo->GetHashKey();
        }

		data.m_popType	= pPed->PopTypeGet();
		data.m_randomSeed = pPed->GetRandomSeed();

        GetVehicleData(data.m_vehicleID, data.m_seat);

		// write the ped's prop
		SetRequiredProp();

		if (m_requiredProp.IsNotNull())
		{
			data.m_hasProp	= true;
			data.m_propHash = m_requiredProp.GetHash();
		}
		else
		{
			data.m_hasProp = false;
		}

		data.m_isStanding = pPed->GetIsStanding();

		data.m_IsRespawnObjId = IsRespawnInProgress();

		data.m_RespawnFlaggedForRemoval = m_RespawnRemovalTimer > 0 ? true : false;

		data.m_hasAttDamageToPlayer = (m_AttDamageToPlayer != INVALID_PLAYER_INDEX);
		if (data.m_hasAttDamageToPlayer)
		{
			data.m_attDamageToPlayer = m_AttDamageToPlayer;
		}

		//Gather max health
		float fMaxHealth = pPed->GetMaxHealth();
		data.m_maxHealth = (s32) fMaxHealth;

		// round it up if necessary
		if (fMaxHealth - (float) data.m_maxHealth >= 0.5f)
			data.m_maxHealth++;

		// did we put a helmet on via CPedHelmetComponent?
		data.m_wearingAHelmet = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet );
		if (pPed->GetSpeechAudioEntity())
		{
			data.m_voiceHash = pPed->GetSpeechAudioEntity()->GetAmbientVoiceName();
		}	
		else
		{
			data.m_voiceHash = g_NoVoiceHash;
		}
	}
	else
	{
		data.m_popType = 0;
		data.m_modelHash = 0;
		data.m_randomSeed = 0;
		data.m_seat = 0;
		data.m_propHash = 0;
		data.m_hasProp = false;
		data.m_inVehicle = false;
		data.m_isStanding = false;
		data.m_IsRespawnObjId = false;
		data.m_RespawnFlaggedForRemoval = false;
		data.m_hasAttDamageToPlayer = false;
		data.m_wearingAHelmet = false;
		data.m_voiceHash = g_NoVoiceHash;
	}
}

void CNetObjPed::GetScriptPedCreateData(CPedScriptCreationDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
        data.m_StayInCarWhenJacked = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_StayInCarOnJack );
	}
	else
	{
        data.m_StayInCarWhenJacked = false;
	}
}

void CNetObjPed::GetPedGameStateData(CPedGameStateDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		data.m_keepTasksAfterCleanup = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepTasksAfterCleanUp );
		data.m_createdByConcealedPlayer = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByConcealedPlayer );
		data.m_dontBehaveLikeLaw = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw );
		data.m_hitByTranqWeapon = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HitByTranqWeapon );
		data.m_canBeIncapacitated = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated);
		data.m_bDisableStartEngine = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableStartEngine);
		data.m_disableBlindFiringInShotReactions = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableBlindFiringInShotReactions);

		data.m_permanentlyDisablePotentialToBeWalkedIntoResponse = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PermanentlyDisablePotentialToBeWalkedIntoResponse );
		data.m_dontActivateRagdollFromAnyPedImpact = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact );

		data.m_changeToAmbientPopTypeOnMigration = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ChangeFromPermanentToAmbientPopTypeOnMigration );

		data.m_arrestState = static_cast<u32>(pPed->GetArrestState());
		data.m_deathState = static_cast<u32>(pPed->GetDeathState());

		u32 weaponHash = 0;
		GetWeaponData(weaponHash, data.m_weaponObjectExists, data.m_weaponObjectVisible, data.m_weaponObjectTintIndex, data.m_flashLightOn, data.m_weaponObjectHasAmmo, data.m_weaponObjectAttachLeft, &data.m_doingWeaponSwap);

		data.m_weapon = weaponHash;
		data.m_HasValidWeaponClipset = m_HasValidWeaponClipset;

		if (data.m_weapon && data.m_weapon != pPed->GetDefaultUnarmedWeaponHash())
		{
			m_hasDroppedWeapon = false;
		}
		data.m_hasDroppedWeapon = m_hasDroppedWeapon;

		data.m_numGadgets = GetGadgetData(data.m_equippedGadgets);
		data.m_numWeaponComponents = GetWeaponComponentData(data.m_weaponComponents, data.m_weaponComponentsTint);

#if !__NO_OUTPUT
		if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			if (pPed->IsPlayer())
			{
				weaponitemDebugf3("CNetObjPed::GetPedGameStateData pPed[%p][%s][%s] player[%s] isplayer[%d] data.m_weaponObjectExists[%d] data.m_numWeaponComponents[%d] data.m_weaponObjectTintIndex[%u] data.m_weapon[%u]",
					pPed,
					pPed->GetModelName(),
					pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",
					GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",
					pPed->IsPlayer(),
					data.m_weaponObjectExists,data.m_numWeaponComponents,data.m_weaponObjectTintIndex,data.m_weapon);
			}
		}
#endif


		GetVehicleData(data.m_vehicleID, data.m_seat);

		data.m_inVehicle = (data.m_vehicleID != NETWORK_INVALID_OBJECT_ID);

		if (!data.m_inVehicle && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetNetworkObject())
		{
			data.m_vehicleID = pPed->GetMyVehicle()->GetNetworkObject()->GetObjectID();
		}

		GetMountData(data.m_mountID);

		if (pPed->GetCustodian() && pPed->GetCustodian()->GetNetworkObject())
			data.m_custodianID = pPed->GetCustodian()->GetNetworkObject()->GetObjectID();
		else
		{
			data.m_custodianID = NETWORK_INVALID_OBJECT_ID;
		}

		data.m_arrestFlags.m_isHandcuffed       = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed );
		data.m_arrestFlags.m_canPerformArrest	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanPerformArrest );
		data.m_arrestFlags.m_canPerformUncuff	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanPerformUncuff );
		data.m_arrestFlags.m_canBeArrested		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanBeArrested );
		data.m_arrestFlags.m_isInCustody		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInCustody );
	
		// we only want to sync player specific action modes (set in TaskPlayer), or ones set in script. The clone tasks should handle the rest.
		data.m_bActionModeEnabled = pPed->WantsToUseActionMode() && (pPed->IsAPlayerPed() || pPed->IsActionModeReasonActive(CPed::AME_Script));	
		data.m_bStealthModeEnabled = pPed->GetMotionData()->GetUsingStealth();
		data.m_nMovementModeOverrideID = pPed->GetMovementModeOverrideHash();

		data.m_killedByStealth = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth );
		data.m_killedByTakedown = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown );
		data.m_killedByKnockdown = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout );
		data.m_killedByStandardMelee = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStandardMelee );

		data.m_cleardamagecount = m_ClearDamageCount;

		data.m_bPedPerceptionModified = false;
		if (AssertVerify(pPed->GetPedIntelligence()))
		{
			data.m_PedPerception.m_HearingRange			= static_cast<u32> (pPed->GetPedIntelligence()->GetPedPerception().GetHearingRange());
			data.m_PedPerception.m_SeeingRange	        = static_cast<u32> (pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange());

			data.m_PedPerception.m_SeeingRangePeripheral = static_cast<u32> (pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRangePeripheral());
			data.m_PedPerception.m_CentreOfGazeMaxAngle = pPed->GetPedIntelligence()->GetPedPerception().GetCentreOfGazeMaxAngle();

			float fMinAzimuthAngle = pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMinAzimuthAngle();
			if (fMinAzimuthAngle > CPedPerception::ms_fVisualFieldMaxAzimuthAngle)
			{
				//something is wrong with the value here - use the default
				fMinAzimuthAngle = CPedPerception::ms_fVisualFieldMinAzimuthAngle;
			}
			data.m_PedPerception.m_VisualFieldMinAzimuthAngle = fMinAzimuthAngle;

			float fMaxAzimuthAngle = pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldMaxAzimuthAngle();
			if (fMaxAzimuthAngle < CPedPerception::ms_fVisualFieldMinAzimuthAngle)
			{
				//something is wrong with the value here - use the default
				fMaxAzimuthAngle = CPedPerception::ms_fVisualFieldMaxAzimuthAngle;
			}
			data.m_PedPerception.m_VisualFieldMaxAzimuthAngle = fMaxAzimuthAngle;

			data.m_PedPerception.m_bIsHighlyPerceptive	= pPed->GetPedIntelligence()->GetPedPerception().GetIsHighlyPerceptive();

			if ( (data.m_PedPerception.m_bIsHighlyPerceptive == true) || 
				(data.m_PedPerception.m_HearingRange != static_cast< u32 >(CPedPerception::ms_fSenseRange)) || 
				(data.m_PedPerception.m_SeeingRange != static_cast< u32 >(CPedPerception::ms_fSenseRange)) ||
				(data.m_PedPerception.m_SeeingRangePeripheral != static_cast< u32 >(CPedPerception::ms_fSenseRangePeripheral)) ||
				(data.m_PedPerception.m_CentreOfGazeMaxAngle != CPedPerception::ms_fCentreOfGazeMaxAngle) ||
				(data.m_PedPerception.m_VisualFieldMinAzimuthAngle != CPedPerception::ms_fVisualFieldMinAzimuthAngle) ||
				(data.m_PedPerception.m_VisualFieldMaxAzimuthAngle != CPedPerception::ms_fVisualFieldMaxAzimuthAngle) )
			{
				data.m_bPedPerceptionModified = true;
			}
		}

		// check if the ped is looking
		data.m_isLookingAtObject = pPed->GetIkManager().IsLooking();
		data.m_LookAtObjectID = NETWORK_INVALID_OBJECT_ID;

		if(data.m_isLookingAtObject)
		{
			data.m_LookAtFlags = pPed->GetIkManager().GetLookAtFlags();

			const CEntity* pEntity = pPed->GetIkManager().GetLookAtEntity();

			netObject* pNetObject = NULL;
			if(pEntity)
			{
				pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
				if(pNetObject)
					data.m_LookAtObjectID = pNetObject->GetObjectID();
			}

			// if the object is not networked, reset the look at flag
			if(data.m_LookAtObjectID == NETWORK_INVALID_OBJECT_ID)
				data.m_isLookingAtObject = false; 
		}
		else
		{
			data.m_LookAtFlags = 0;
			data.m_LookAtObjectID = NETWORK_INVALID_OBJECT_ID;
		}

		if(!pPed->GetUsingRagdoll() && pPed->GetMovePed().GetState() != CMovePed::kStateStaticFrame)
		{
			data.m_isUpright = (pPed->GetTransform().GetC().GetZf() == 1.0f);
		}
		else
		{
			data.m_isUpright = false;
		}

		data.m_isDuckingInVehicle = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsDuckingInVehicle );
		data.m_isUsingLowriderLeanAnims = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UsingLowriderLeans );
		data.m_isUsingAlternateLowriderLeanAnims = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UsingAlternateLowriderLeans );
	}
	else
	{
		data.m_keepTasksAfterCleanup = data.m_weaponObjectExists = data.m_weaponObjectVisible = data.m_changeToAmbientPopTypeOnMigration = false;
		data.m_createdByConcealedPlayer = false;
		data.m_dontBehaveLikeLaw = false;
		data.m_hitByTranqWeapon = false;
		data.m_canBeIncapacitated = false;
		data.m_bDisableStartEngine = false;
		data.m_disableBlindFiringInShotReactions = false;
		data.m_dontActivateRagdollFromAnyPedImpact = false;
		data.m_permanentlyDisablePotentialToBeWalkedIntoResponse = false;
		data.m_weaponObjectHasAmmo = true;
		data.m_weaponObjectAttachLeft = false;
        data.m_weaponObjectTintIndex = 0;
		data.m_arrestState = data.m_deathState = data.m_weapon = data.m_numGadgets = data.m_seat = 0;
		data.m_vehicleID = NETWORK_INVALID_OBJECT_ID;
		data.m_mountID = NETWORK_INVALID_OBJECT_ID;

		data.m_custodianID = NETWORK_INVALID_OBJECT_ID;
		data.m_arrestFlags.m_isHandcuffed      = false;
		data.m_arrestFlags.m_canPerformArrest	= false;
		data.m_arrestFlags.m_canPerformUncuff	= false;
		data.m_arrestFlags.m_canBeArrested		= false;
		data.m_arrestFlags.m_isInCustody		= false;

		data.m_bActionModeEnabled = false;
		data.m_bStealthModeEnabled = false;
		data.m_nMovementModeOverrideID = 0;

		data.m_killedByStealth = false;
		data.m_killedByTakedown = false;
		data.m_killedByKnockdown = false;
		data.m_killedByStandardMelee = false;

		data.m_cleardamagecount = 0;

		data.m_bPedPerceptionModified = false;
		data.m_PedPerception.m_HearingRange       = static_cast< u32 >(CPedPerception::ms_fSenseRange);
		data.m_PedPerception.m_SeeingRange        = static_cast< u32 >(CPedPerception::ms_fSenseRange);
		data.m_PedPerception.m_bIsHighlyPerceptive = false;

		data.m_PedPerception.m_SeeingRangePeripheral = static_cast< u32 >(CPedPerception::ms_fSenseRangePeripheral);
		data.m_PedPerception.m_CentreOfGazeMaxAngle = CPedPerception::ms_fCentreOfGazeMaxAngle;
		data.m_PedPerception.m_VisualFieldMinAzimuthAngle = CPedPerception::ms_fVisualFieldMinAzimuthAngle;
		data.m_PedPerception.m_VisualFieldMaxAzimuthAngle = CPedPerception::ms_fVisualFieldMaxAzimuthAngle;

		data.m_LookAtFlags							= 0;
		data.m_LookAtObjectID						= NETWORK_INVALID_OBJECT_ID;

		data.m_isUpright = true;

		data.m_isDuckingInVehicle = false;
		data.m_isUsingLowriderLeanAnims = false;
		data.m_isUsingAlternateLowriderLeanAnims = false;
		
		data.m_hasDroppedWeapon = false;
		data.m_flashLightOn = false;
	}
}

void CNetObjPed::GetPedScriptGameStateData(CPedScriptGameStateDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		data.m_popType = pPed->PopTypeGet();

		data.m_pedType = (pPed->GetPedType() != PEDTYPE_INVALID) ? (u32) pPed->GetPedType() : (u32) PEDTYPE_LAST_PEDTYPE; //ped type can sometimes be altered by script after the ped is created, via commands like CommandSetPedAsCop...

		data.m_PedFlags.neverTargetThisPed					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed );
		data.m_PedFlags.blockNonTempEvents					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents );
		data.m_PedFlags.noCriticalHits						= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits );
		data.m_PedFlags.isPriorityTarget					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ThisPedIsATargetPriority );
		data.m_PedFlags.doesntDropWeaponsWhenDead			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead );
		data.m_PedFlags.dontDragMeOutOfCar					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar );
		data.m_PedFlags.playersDontDragMeOutOfCar			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar );
		data.m_PedFlags.drownsInWater						= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater );
		data.m_PedFlags.drownsInSinkingVehicle				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle );
		data.m_PedFlags.forceDieIfInjured					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceDieIfInjured );
		data.m_PedFlags.fallOutOfVehicleWhenKilled			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled );
        data.m_PedFlags.diesInstantlyInWater       			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming );
		data.m_PedFlags.justGetsPulledOutWhenElectrocuted	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_JustGetsPulledOutWhenElectrocuted );
		data.m_PedFlags.dontRagdollFromPlayerImpact			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact );
		data.m_PedFlags.lockAnim							= pPed->GetRagdollState() == RAGDOLL_STATE_ANIM_LOCKED;
		data.m_PedFlags.preventPedFromReactingToBeingJacked	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventPedFromReactingToBeingJacked );
		data.m_PedFlags.dontInfluenceWantedLevel			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontInfluenceWantedLevel );
		data.m_PedFlags.getOutUndriveableVehicle			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle );
		data.m_PedFlags.moneyHasBeenGivenByScript			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_MoneyHasBeenGivenByScript );
		data.m_PedFlags.shoutToGroupOnPlayerMelee			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShoutToGroupOnPlayerMelee );
		data.m_PedFlags.cantBeKnockedOffBike				= pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle();
		data.m_PedFlags.canDrivePlayerAsRearPassenger		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger );
		data.m_PedFlags.keepRelationshipGroupAfterCleanup   = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepRelationshipGroupAfterCleanUp );
		
		data.m_PedFlags.disableLockonToRandomPeds			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableLockonToRandomPeds );
		data.m_PedFlags.willNotHotwireLawEnforcementVehicle = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillNotHotwireLawEnforcementVehicle );
		data.m_PedFlags.willCommandeerRatherThanJack		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillCommandeerRatherThanJack );
		data.m_PedFlags.useKinematicModeWhenStationary		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseKinematicModeWhenStationary );
		data.m_PedFlags.forcedToUseSpecificGroupSeatIndex	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex );
		data.m_PedFlags.playerCanJackFriendlyPlayers		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayerCanJackFriendlyPlayers );

		data.m_PedFlags.willFlyThroughWindscreen			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillFlyThroughWindscreen );
		data.m_PedFlags.getOutBurningVehicle				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutBurningVehicle );
		data.m_PedFlags.disableLadderClimbing				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableLadderClimbing );
        data.m_PedFlags.forceDirectEntry                    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceDirectEntry );

		data.m_PedFlags.phoneDisableCameraAnimations		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableCameraAnimations );
		data.m_PedFlags.notAllowedToJackAnyPlayers			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers );

		data.m_PedFlags.runFromFiresAndExplosions			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_RunFromFiresAndExplosions );
		data.m_PedFlags.ignoreBeingOnFire					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IgnoreBeingOnFire );
		data.m_PedFlags.disableWantedHelicopterSpawning		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning );
		data.m_PedFlags.ignoreNetSessionFriendlyFireCheckForAllowDamage = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreNetSessionFriendlyFireCheckForAllowDamage);
		data.m_PedFlags.dontLeaveCombatIfTargetPlayerIsAttackedByPolice = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontLeaveCombatIfTargetPlayerIsAttackedByPolice);
		data.m_PedFlags.useTargetPerceptionForCreatingAimedAtEvents		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseTargetPerceptionForCreatingAimedAtEvents );
		data.m_PedFlags.disableHomingMissileLockon			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableHomingMissileLockon );
		data.m_PedFlags.forceIgnoreMaxMeleeActiveSupportCombatants = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceIgnoreMaxMeleeActiveSupportCombatants );
		data.m_PedFlags.stayInDefensiveAreaWhenInVehicle	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_StayInDefensiveAreaWhenInVehicle );
		data.m_PedFlags.dontShoutTargetPosition				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontShoutTargetPosition );
		data.m_PedFlags.preventExitVehicleDueToInvalidWeapon = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventVehExitDueToInvalidWeapon );
		data.m_PedFlags.disableHelmetArmor					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableHelmetArmor );
		data.m_PedFlags.upperBodyDamageAnimsOnly			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly );

		data.m_PedFlags.checklockedbeforewarp				= pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLockedBeforeWarp);
		data.m_PedFlags.dontShuffleInVehicleToMakeRoom		= pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontShuffleInVehicleToMakeRoom);
		data.m_PedFlags.disableForcedEntryForOpenVehicles	= pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableForcedEntryForOpenVehiclesFromTryLockedDoor);

		data.m_PedFlags.giveWeaponOnGetup					= pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_GiveWeaponOnGetup);

		data.m_PedFlags.dontHitVehicleWithProjectiles		= pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontHitVehicleWithProjectiles);

		data.m_PedFlags.listenToSoundEvents					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ListensToSoundEvents );

		data.m_PedFlags.openDoorArmIK						= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK );

		data.m_PedFlags.dontBlipCop							= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontBlipCop );
		data.m_PedFlags.forceSkinCharacterCloth				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth );
		data.m_PedFlags.phoneDisableTalkingAnimations		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations );
		data.m_PedFlags.lowerPriorityOfWarpSeats			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_LowerPriorityOfWarpSeats );

		data.m_PedFlags.dontBehaveLikeLaw					= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw );
		data.m_PedFlags.phoneDisableTextingAnimations		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableTextingAnimations );
		data.m_PedFlags.dontActivateRagdollFromBulletImpact	= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact );
		data.m_PedFlags.checkLOSForSoundEvents				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CheckLoSForSoundEvents );

		data.m_PedFlags.disablePanicInVehicle				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisablePanicInVehicle );
		data.m_PedFlags.forceRagdollUponDeath               = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceRagdollUponDeath );

		data.m_PedFlags.pedIgnoresAnimInterruptEvents		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents );

		data.m_PedFlags.dontBlip							= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontBlip );

		data.m_PedFlags.preventAutoShuffleToDriverSeat		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat );

		data.m_PedFlags.canBeAgitated						= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanBeAgitated );

        data.m_PedFlags.AIDriverAllowFriendlyPassengerSeatEntry = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry );
		data.m_PedFlags.onlyWritheFromWeaponDamage			    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnlyWritheFromWeaponDamage );
		data.m_PedFlags.shouldChargeNow						    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShouldChargeNow );
        data.m_PedFlags.disableJumpingFromVehiclesAfterLeader   = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableJumpingFromVehiclesAfterLeader);
		data.m_PedFlags.preventUsingLowerPrioritySeats		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats );
		data.m_PedFlags.preventDraggedOutOfCarThreatResponse    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventDraggedOutOfCarThreatResponse );
		data.m_PedFlags.canAttackNonWantedPlayerAsLaw		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw );
		data.m_PedFlags.disableMelee						    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableMelee );
		data.m_PedFlags.preventAutoShuffleToTurretSeat		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAutoShuffleToTurretSeat );
		data.m_PedFlags.disableEventInteriorStatusCheck		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableEventInteriorStatusCheck );
		data.m_PedFlags.onlyUpdateTargetWantedIfSeen		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen );
		data.m_PedFlags.treatDislikeAsHateWhenInCombat		    = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat );
		data.m_PedFlags.treatNonFriendlyAsHateWhenInCombat		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_TreatNonFriendlyAsHateWhenInCombat );
        data.m_PedFlags.preventReactingToCloneBullets           = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets );
		data.m_PedFlags.disableAutoEquipHelmetsInBikes			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes );
		data.m_PedFlags.disableAutoEquipHelmetsInAircraft		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInAircraft );
		data.m_PedFlags.stopWeaponFiringOnImpact				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact );
		data.m_PedFlags.keepWeaponHolsteredUnlessFired			= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired );
		data.m_PedFlags.disableExplosionReactions				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableExplosionReactions );
		data.m_PedFlags.dontActivateRagdollFromExplosions		= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions );
		data.m_PedFlags.dontChangeTargetFromMelee				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontChangeTargetFromMelee );
		data.m_PedFlags.ragdollFloatsIndefinitely				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_RagdollFloatsIndefinitely );
		data.m_PedFlags.blockElectricWeaponDamage				= pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockElectricWeaponDamage );

		const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig();
		if (pVehicleEntryConfig)
		{
			data.m_PedVehicleEntryScriptConfig = *pVehicleEntryConfig;
		}
		else
		{
			data.m_PedVehicleEntryScriptConfig.Reset();
		}

		data.m_PedFlags.avoidTearGas							          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AvoidTearGas );
		data.m_PedFlags.disableHornAudioWhenDead				          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableHornAudioWhenDead );
		data.m_PedFlags.ignoredByAutoOpenDoors					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IgnoredByAutoOpenDoors );
		data.m_PedFlags.activateRagdollWhenVehicleUpsideDown	          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown );
		data.m_PedFlags.allowNearbyCoverUsage					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowNearbyCoverUsage );
		data.m_PedFlags.cowerInsteadOfFlee						          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CowerInsteadOfFlee );
		data.m_PedFlags.disableGoToWritheWhenInjured			          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured );
		data.m_PedFlags.blockDroppingHealthSnacksOnDeath		          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockDroppingHealthSnacksOnDeath );
		data.m_PedFlags.willTakeDamageWhenVehicleCrashes		          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes );
		data.m_PedFlags.dontRespondToRandomPedsDamage			          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontRespondToRandomPedsDamage );
		data.m_PedFlags.broadcastRepondedToThreatWhenGoingToPointShooting = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BroadcastRepondedToThreatWhenGoingToPointShooting );
		data.m_PedFlags.dontTakeOffHelmet						          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet );
		data.m_PedFlags.allowContinuousThreatResponseWantedLevelUpdates	  = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates );
		data.m_PedFlags.keepTargetLossResponseOnCleanup			          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepTargetLossResponseOnCleanup );
		data.m_PedFlags.forcedToStayInCover						          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcedToStayInCover );
		data.m_PedFlags.blockPedFromTurningInCover				          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockPedFromTurningInCover );
		data.m_PedFlags.teleportToLeaderVehicle					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_TeleportToLeaderVehicle );
		data.m_PedFlags.dontLeaveVehicleIfLeaderNotInVehicle	          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontLeaveVehicleIfLeaderNotInVehicle );
		data.m_PedFlags.forcePedToFaceLeftInCover				          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedToFaceLeftInCover );
		data.m_PedFlags.forcePedToFaceRightInCover				          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedToFaceRightInCover );
		data.m_PedFlags.useNormalExplosionDamageWhenBlownUpInVehicle	  = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle );
		data.m_PedFlags.dontClearLocalPassengersWantedLevel               = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontClearLocalPassengersWantedLevel);
		data.m_PedFlags.useGoToPointForScenarioNavigation		          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseGoToPointForScenarioNavigation);
    	data.m_PedFlags.hasBareFeet								          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasBareFeet);
		data.m_PedFlags.blockAutoSwapOnWeaponPickups                      = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockAutoSwapOnWeaponPickups);
        data.m_PedFlags.isPriorityTargetForAI					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ThisPedIsATargetPriorityForAI );
		data.m_PedFlags.hasHelmet								          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet );
		data.m_PedFlags.isSwitchingHelmetVisor					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwitchingHelmetVisor );
		data.m_PedFlags.forceHelmetVisorSwitch					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceHelmetVisorSwitch );
		data.m_PedFlags.isPerformingVehMelee					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsPerformingVehicleMelee );
		data.m_PedFlags.useOverrideFootstepPtFx					          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseOverrideFootstepPtFx );
		data.m_PedFlags.dontAttackPlayerWithoutWantedLevel		          = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontAttackPlayerWithoutWantedLevel );
        data.m_PedFlags.disableShockingEvents                             = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableShockingEvents );
		data.m_PedFlags.treatAsFriendlyForTargetingAndDamage		      = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage );
		data.m_PedFlags.allowBikeAlternateAnimations				      = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AllowBikeAlternateAnimations );
		data.m_PedFlags.useLockpickVehicleEntryAnimations			      = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations );
		data.m_PedFlags.ignoreInteriorCheckForSprinting					  = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting);
		data.m_PedFlags.dontActivateRagdollFromVehicleImpact			  = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact);
        data.m_PedFlags.avoidanceIgnoreAll                                = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_All);
		data.m_PedFlags.pedsJackingMeDontGetIn                            = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PedsJackingMeDontGetIn);
		//PED RESET FLAGS - get a bool sync to remote store in netobjped member in member update set the ped reset flag so it occurs every frame while set
		data.m_PedFlags.PRF_IgnoreCombatManager							  = pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatManager);
		data.m_isPainAudioDisabled								          = pPed->GetSpeechAudioEntity() ? pPed->GetSpeechAudioEntity()->IsPainAudioDisabled() : false;
		data.m_isAmbientSpeechDisabled									  = pPed->GetSpeechAudioEntity() ? pPed->GetSpeechAudioEntity()->IsSpeakingDisabledSynced() : false;
		data.m_PedFlags.disableTurretOrRearSeatPreference                 = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableTurretOrRearSeatPreference);
		data.m_PedFlags.disableHurt										  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHurt);
		data.m_PedFlags.allowTargettingInVehicle						  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowToBeTargetedInAVehicle);
		data.m_PedFlags.firesDummyRockets                                 = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_FiresDummyRockets);
		data.m_PedFlags.decoyPed										  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDecoyPed);
		data.m_PedFlags.hasEstablishedDecoy								  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasEstablishedDecoy);
		data.m_PedFlags.disableInjuredCryForHelpEvents				      = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableInjuredCryForHelpEvents);
		data.m_PedFlags.dontCryForHelpOnStun							  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontCryForHelpOnStun);
		data.m_PedFlags.blockDispatchedHelicoptersFromLanding			  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockDispatchedHelicoptersFromLanding);
		data.m_PedFlags.disableBloodPoolCreation						  = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableBloodPoolCreation);

		m_PRF_disableInVehicleActions = data.m_PedFlags.PRF_disableInVehicleActions = pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions);

		if (AssertVerify(pPed->GetPedIntelligence()))
		{
			data.m_shootRate					        = pPed->GetWeaponManager() ? pPed->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier() : 0.0f;

			data.m_combatMovement						= (u8)(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement());
			data.m_combatBehaviorFlags					= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatBehaviorFlags();
			data.m_targetLossResponse					= pPed->GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse();
			data.m_fleeBehaviorFlags					= pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags();
			data.m_NavCapabilityFlags					= pPed->GetPedIntelligence()->GetNavCapabilities().GetAllFlags();

			data.m_uMaxNumFriendsToInform				= pPed->GetPedIntelligence()->GetMaxNumRespectedFriendsToInform();
			data.m_fMaxInformFriendDistance				= pPed->GetPedIntelligence()->GetMaxInformRespectedFriendDistance();

			data.m_fTimeBetweenAggressiveMovesDuringVehicleChase	= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeBetweenAggressiveMovesDuringVehicleChase);
			data.m_fMaxVehicleTurretFiringRange			= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxVehicleTurretFiringRange);

			//sync of additional combat behaviour variables that are set after the peds are created locally - also moved accuracy here from create - so might be after sync - these are set in script in MP - lavalley
			data.m_fMaxShootingDistance					= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxShootingDistance);
			data.m_fBurstDurationInCover				= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatBurstDurationInCover);
			data.m_fTimeBetweenBurstsInCover			= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeBetweenBurstsInCover);
			data.m_fTimeBetweenPeeks					= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeBetweenPeeks);
			data.m_fBlindFireChance						= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatBlindFireChance);
			data.m_fStrafeWhenMovingChance				= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatStrafeWhenMovingChance);
			data.m_fAccuracy							= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponAccuracy);
			data.m_fWeaponDamageModifier				= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier);

			data.m_fHomingRocketBreakLockAngle			= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngle);
			data.m_fHomingRocketBreakLockAngleClose		= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngleClose);
			data.m_fHomingRocketBreakLockCloseDistance	= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockCloseDistance);
		}

		data.m_pedCash						        = static_cast<u32>(pPed->GetMoneyCarried());
		data.m_isTargettableByTeam					= m_isTargettableByTeam;
		data.m_minOnGroundTimeForStun				= pPed->GetMinOnGroundTimeForStunGun();
		data.m_ammoToDrop							= (u32) m_ammoToDrop;
	

		// if this fails, set MAX_SYNCED_SHOOT_RATE = MAX_SHOOT_RATE_MODIFIER
		// header dependency headache if we try to access MAX_SHOOT_RATE_MODIFIER from sync node
		gnetAssertf(data.m_shootRate <= MAX_SYNCED_SHOOT_RATE, "Set MAX_SYNCED_SHOOT_RATE equal to current value of MAX_SHOOT_RATE_MODIFIER");

		// build ragdoll blocking flags
		eRagdollBlockingFlagsBitSet pedFlags = pPed->GetRagdollBlockingFlags();
		data.m_ragdollBlockingFlags = reinterpret_cast<u32&>(pedFlags);

		CDefensiveArea *pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea();

        data.m_HasDefensiveArea = pDefensiveArea && pDefensiveArea->IsActive();
		data.m_DefensiveAreaType = (u8)pDefensiveArea->GetType();

        if(data.m_HasDefensiveArea && data.m_DefensiveAreaType == CDefensiveArea::AT_Sphere)
        {
            pDefensiveArea->GetCentre(data.m_DefensiveAreaCentre);
            pDefensiveArea->GetMaxRadius(data.m_DefensiveAreaRadius);
            data.m_UseCentreAsGotoPos = pDefensiveArea->GetUseCenterAsGoToPosition();
        }
		else if(data.m_HasDefensiveArea && data.m_DefensiveAreaType == CDefensiveArea::AT_AngledArea)
		{
			data.m_AngledDefensiveAreaV1 = pDefensiveArea->GetVec1();
			data.m_AngledDefensiveAreaV2 = pDefensiveArea->GetVec2();
			data.m_AngledDefensiveAreaWidth = pDefensiveArea->GetWidth();
            data.m_UseCentreAsGotoPos = pDefensiveArea->GetUseCenterAsGoToPosition();
		}
		else
		{
			data.m_DefensiveAreaRadius = 0.0f;
			data.m_AngledDefensiveAreaWidth = 0.0f;
			data.m_DefensiveAreaCentre = VEC3_ZERO;
			data.m_AngledDefensiveAreaV1 = VEC3_ZERO;
			data.m_AngledDefensiveAreaV2 = VEC3_ZERO;
			data.m_UseCentreAsGotoPos = false;
		}

		data.m_vehicleweaponindex = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() : -1;

		data.m_SeatIndexToUseInAGroup = pPed->m_PedConfigFlags.GetPassengerIndexToUseInAGroup();

		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTaskIfExists();
		data.m_inVehicleContextHash = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;

		data.m_FiringPatternHash = 0;
		if(pPed->GetPedIntelligence())
		{
			data.m_FiringPatternHash = pPed->GetPedIntelligence()->GetCombatBehaviour().GetFiringPattern().GetFiringPatternHash();
		}
	}
	else
	{
		data.m_popType = 0;
		if(IsLocalFlagSet(LOCALFLAG_RESPAWNPED))
		{
			data.m_popType = POPTYPE_RANDOM_AMBIENT;
		}

		data.m_pedType = PEDTYPE_LAST_PEDTYPE; //default to PEDTYPE_LAST_PEDTYPE if we don't have a ped? should always have a ped.

        data.m_PedFlags.neverTargetThisPed					              = false;
        data.m_PedFlags.blockNonTempEvents					              = false;
        data.m_PedFlags.noCriticalHits						              = false;
        data.m_PedFlags.isPriorityTarget					              = false;
        data.m_PedFlags.doesntDropWeaponsWhenDead			              = false;
        data.m_PedFlags.dontDragMeOutOfCar					              = false;
        data.m_PedFlags.playersDontDragMeOutOfCar			              = false;
        data.m_PedFlags.drownsInWater						              = false;
        data.m_PedFlags.forceDieIfInjured					              = false;
        data.m_PedFlags.fallOutOfVehicleWhenKilled			              = false;
        data.m_PedFlags.diesInstantlyInWater       			              = false;
        data.m_PedFlags.justGetsPulledOutWhenElectrocuted	              = false;
        data.m_PedFlags.dontRagdollFromPlayerImpact			              = false;
        data.m_PedFlags.lockAnim							              = false;
        data.m_PedFlags.preventPedFromReactingToBeingJacked	              = false;
        data.m_PedFlags.cantBeKnockedOffBike				              = KNOCKOFFVEHICLE_DEFAULT; 
        data.m_PedFlags.dontInfluenceWantedLevel			              = false;
        data.m_PedFlags.getOutUndriveableVehicle			              = false;
        data.m_PedFlags.moneyHasBeenGivenByScript			              = false;
        data.m_PedFlags.shoutToGroupOnPlayerMelee			              = false;
        data.m_PedFlags.canDrivePlayerAsRearPassenger		              = false;
        data.m_PedFlags.keepRelationshipGroupAfterCleanup	              = false;

        data.m_PedFlags.disableLockonToRandomPeds			              = false;
        data.m_PedFlags.willNotHotwireLawEnforcementVehicle               = false;
        data.m_PedFlags.willCommandeerRatherThanJack		              = false;
        data.m_PedFlags.useKinematicModeWhenStationary		              = false;
        data.m_PedFlags.playerCanJackFriendlyPlayers		              = false;
        data.m_PedFlags.willFlyThroughWindscreen			              = false;
        data.m_PedFlags.getOutBurningVehicle				              = false;
        data.m_PedFlags.disableLadderClimbing				              = false;
        data.m_PedFlags.forceDirectEntry                                  = false;
        data.m_PedFlags.shouldChargeNow                                   = false;
        data.m_PedFlags.disableJumpingFromVehiclesAfterLeader             = false;
        data.m_PedFlags.preventUsingLowerPrioritySeats                    = false;
        data.m_PedFlags.preventDraggedOutOfCarThreatResponse              = false;
        data.m_PedFlags.stopWeaponFiringOnImpact			              = false;
        data.m_PedFlags.keepWeaponHolsteredUnlessFired		              = false;
        data.m_PedFlags.disableExplosionReactions			              = false;
        data.m_PedFlags.dontActivateRagdollFromExplosions	              = false;
        data.m_PedFlags.avoidTearGas						              = false;
        data.m_PedFlags.disableHornAudioWhenDead			              = true;
        data.m_PedFlags.ignoredByAutoOpenDoors				              = false;
        data.m_PedFlags.activateRagdollWhenVehicleUpsideDown              = false;
        data.m_PedFlags.allowNearbyCoverUsage				              = false;
        data.m_PedFlags.cowerInsteadOfFlee					              = false;
        data.m_PedFlags.disableGoToWritheWhenInjured		              = false;
        data.m_PedFlags.blockDroppingHealthSnacksOnDeath	              = false;
        data.m_PedFlags.willTakeDamageWhenVehicleCrashes	              = true;
        data.m_PedFlags.dontRespondToRandomPedsDamage		              = false;
        data.m_PedFlags.broadcastRepondedToThreatWhenGoingToPointShooting = false;
        data.m_PedFlags.dontTakeOffHelmet					              = false;
        data.m_PedFlags.allowContinuousThreatResponseWantedLevelUpdates   = false;
        data.m_PedFlags.keepTargetLossResponseOnCleanup		              = false;
        data.m_PedFlags.forcedToStayInCover					              = false;
        data.m_PedFlags.blockPedFromTurningInCover			              = false;
        data.m_PedFlags.teleportToLeaderVehicle				              = false;
        data.m_PedFlags.dontLeaveVehicleIfLeaderNotInVehicle              = false;
        data.m_PedFlags.forcePedToFaceLeftInCover			              = false;
        data.m_PedFlags.forcePedToFaceRightInCover			              = false;
        data.m_PedFlags.useNormalExplosionDamageWhenBlownUpInVehicle      = false;
        data.m_PedFlags.dontClearLocalPassengersWantedLevel               = false;
        data.m_PedFlags.useGoToPointForScenarioNavigation	              = false;
        data.m_PedFlags.isPriorityTargetForAI				              = false;
        data.m_PedFlags.hasHelmet							              = false;
        data.m_PedFlags.isSwitchingHelmetVisor				              = false;
        data.m_PedFlags.forceHelmetVisorSwitch				              = false;
        data.m_PedFlags.isPerformingVehMelee				              = false;
        data.m_PedFlags.useOverrideFootstepPtFx				              = false;
        data.m_PedFlags.disableShockingEvents                             = false;
        data.m_PedFlags.dontAttackPlayerWithoutWantedLevel	              = false;
		data.m_PedFlags.treatAsFriendlyForTargetingAndDamage			  = false;
		data.m_PedFlags.allowBikeAlternateAnimations					  = false;
		data.m_PedFlags.useLockpickVehicleEntryAnimations				  = false;
		data.m_PedFlags.ignoreInteriorCheckForSprinting					  = false; 
	    data.m_PedFlags.dontActivateRagdollFromVehicleImpact			  = false;
		data.m_PedFlags.avoidanceIgnoreAll								  = false;
		data.m_PedFlags.pedsJackingMeDontGetIn							  = false;
		data.m_PedFlags.disableTurretOrRearSeatPreference				  = false;
		data.m_PedFlags.disableHurt										  = false;
		data.m_PedFlags.allowTargettingInVehicle						  = false;
		data.m_PedFlags.firesDummyRockets								  = false;
		data.m_PedFlags.decoyPed										  = false;
		data.m_PedFlags.hasEstablishedDecoy								  = false;
		data.m_PedFlags.disableInjuredCryForHelpEvents					  = false;
		data.m_PedFlags.dontCryForHelpOnStun							  = false;
		data.m_PedFlags.blockDispatchedHelicoptersFromLanding             = false;
		data.m_PedFlags.dontChangeTargetFromMelee						  = false;
		data.m_PedFlags.ragdollFloatsIndefinitely						  = false;
		data.m_PedFlags.blockElectricWeaponDamage						  = false;

		//PED RESET FLAGS
		data.m_PedFlags.PRF_disableInVehicleActions			 = false;
		data.m_PedFlags.PRF_IgnoreCombatManager				 = false;
		data.m_isPainAudioDisabled							 = false;
		data.m_isAmbientSpeechDisabled						 = false;

		data.m_shootRate							= 0.0f;
		data.m_pedCash								= 0;
		data.m_isTargettableByTeam					= 0;
		data.m_minOnGroundTimeForStun				= -1;
		data.m_combatMovement						= CCombatData::CM_Stationary; /* 0 */
		data.m_ragdollBlockingFlags					= 0;
		data.m_ammoToDrop							= 0;

        data.m_HasDefensiveArea                     = false;
		data.m_DefensiveAreaRadius					= 0.0;
		data.m_DefensiveAreaCentre					= VEC3_ZERO;
		data.m_UseCentreAsGotoPos					= false;
		data.m_AngledDefensiveAreaWidth             = 0.0f;
		data.m_AngledDefensiveAreaV1                = VEC3_ZERO;
		data.m_AngledDefensiveAreaV2                = VEC3_ZERO;


		data.m_AngledDefensiveAreaV1                = VEC3_ZERO;
		data.m_AngledDefensiveAreaV2                = VEC3_ZERO;
		data.m_AngledDefensiveAreaWidth             = 0.0f;

		data.m_vehicleweaponindex					= -1;

		data.m_uMaxNumFriendsToInform				= CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM;
		data.m_fMaxInformFriendDistance				= CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE;

		data.m_fTimeBetweenAggressiveMovesDuringVehicleChase	= -1.0f;
		data.m_fMaxVehicleTurretFiringRange			= -1.0f;

		data.m_fAccuracy							= 0.4f; //default value from commands_ped.sch
		data.m_fMaxShootingDistance					= -1.0f; //default value from commands_ped.sch
		data.m_fBurstDurationInCover				= 2.0f; //default value from commands_ped.sch
		data.m_fTimeBetweenBurstsInCover			= 1.25f; //default value from commands_ped.sch
		data.m_fTimeBetweenPeeks					= 10.0f; //default value from commands_ped.sch
		data.m_fStrafeWhenMovingChance				= 1.0f; //default value from commands_ped.sch
		data.m_fBlindFireChance						= 0.0f; //default varies between law and non law so it's safest to default to the law value
		data.m_fWeaponDamageModifier				= 1.0f;
		
		data.m_fHomingRocketBreakLockAngle			= 0.2f; //default value from CProjectileRocket::ProcessPhysics
		data.m_fHomingRocketBreakLockAngleClose		= 0.6f; //default value from CProjectileRocket::ProcessPhysics
		data.m_fHomingRocketBreakLockCloseDistance	= 20.f; //default value from CProjectileRocket::ProcessPhysics

		data.m_SeatIndexToUseInAGroup				= -1;

		data.m_inVehicleContextHash					= 0;

		data.m_FiringPatternHash					= 0;
	}
}

void CNetObjPed::GetSectorPosMapNode(CPedSectorPosMapNode &data)
{
    CPed *pPed = GetPed();
    VALIDATE_GAME_OBJECT(pPed);

    GetSectorPosition(data.m_sectorPosX, data.m_sectorPosY, data.m_sectorPosZ);

    data.m_IsRagdolling = pPed ? pPed->GetUsingRagdoll() : false;

    GetStandingOnObjectData(data.m_StandingOnNetworkObjectID, data.m_LocalOffset);
}

void CNetObjPed::GetSectorNavmeshPosition(CPedSectorPosNavMeshNode& data)
{
    // we just return the ped's real position here - it should be close enough to
    // the nav mesh position and we are only interested in syncing a rough Z
    GetSectorPosition(data.m_sectorPosX, data.m_sectorPosY, data.m_sectorPosZ);
}

void CNetObjPed::GetPedHeadings(CPedOrientationDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		data.m_currentHeading = pPed->GetCurrentHeading();
		data.m_desiredHeading = pPed->GetDesiredHeading();

		data.m_currentHeading = fwAngle::LimitRadianAngle(data.m_currentHeading);
		data.m_desiredHeading = fwAngle::LimitRadianAngle(data.m_desiredHeading);
	}
	else
	{
		data.m_currentHeading = data.m_desiredHeading = 0.0f;
	}
}

void CNetObjPed::GetWeaponData(u32 &weapon, bool &weaponObjectExists, bool &weaponObjectVisible, u8 &tintIndex, bool &flashLightOn, bool &hasAmmo, bool &attachLeft, bool *doingWeaponSwap)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed && pPed->GetWeaponManager())
	{
		weapon = pPed->GetWeaponManager()->GetEquippedWeaponHash();

		if (IsClone())
		{
			// the clone may not have a weapon object yet (it hasn't streamed in) so always use cached values received in last update
			weaponObjectExists  = m_weaponObjectExists;
			weaponObjectVisible = m_weaponObjectVisible;
            tintIndex           = m_weaponObjectTintIndex;
			hasAmmo				= m_weaponObjectHasAmmo;
			attachLeft			= m_weaponObjectAttachLeft;
			flashLightOn		= false;
		}
		else
		{
            tintIndex = 0;

			attachLeft = pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand ? true : false;

            CWeapon *equippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

			bool bRunningSwapWeaponTask = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_SWAP_WEAPON) != NULL;

            if(equippedWeapon)
            {
                tintIndex = equippedWeapon->GetTintIndex();

				CWeaponComponentFlashLight* pFlashLight = equippedWeapon->GetFlashLightComponent();
				flashLightOn = pFlashLight ? pFlashLight->GetActive() : false;

				s32 ammoTotal = equippedWeapon->GetAmmoTotal();
				hasAmmo = (ammoTotal > 0) || (ammoTotal == CWeapon::INFINITE_AMMO);

				//If the equipped weapon currently differs from the incoming equipped weapon hash use the equipped weapon - weapon hash as the weapon value
				if (weapon != equippedWeapon->GetWeaponHash())
					weapon = equippedWeapon->GetWeaponHash();
            }
			else
			{
				flashLightOn = false;
				//If the weapon isn't equipped yet and we don't know it's ammo status yet from the equipped weapon object but we can get it from the ammo repository
				hasAmmo = pPed->GetInventory() && CWeaponInfoManager::GetInfo<CWeaponInfo>(weapon) ? !pPed->GetInventory()->GetIsWeaponOutOfAmmo(weapon) : false;
			}

			// get the chosen weapon object ID if it is an object
			const CObject *weaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();

			// if we're running TASK_SWAP_WEAPON, we override any updates from the owner telling us to destroy the weapon.
			// Because the weapon sync uses "weapon that is *going to be* equipped" (GetEquippedWeaponHash()) rather than 
			// "weapon that *is* equipped" (GetEquippedWeaponObjectHash() and TASK_SWAP_WEAPON is removing / adding weapons
			// the clone can get in a situation where the network is telling it to delete the weapon it is trying / has already 
			// switched to - causing flashing as it's deleted / recreated. This forces the network to let the task sort it out.
			if ((bRunningSwapWeaponTask && (weapon != 0)) || pPed->GetWeaponManager()->GetCreateWeaponObjectWhenLoaded())
			{
				m_weaponObjectExists = weaponObjectExists = true;
			}
			else
			{
				m_weaponObjectExists = weaponObjectExists = (weaponObject != NULL);
			}

			m_weaponObjectVisible = weaponObjectVisible = (weaponObject && weaponObject->GetIsVisible());

			// in 1st person the weapon can be hidden, make sure it is always visible remotely when running a drive by task
			if(!weaponObjectVisible && pPed->GetIsVisible() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY))
			{
				weaponObjectVisible = true;
			}

            if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
            {
                weapon = pPed->GetWeaponManager()->GetBackupWeapon();
            }

			if (doingWeaponSwap)
			{
				*doingWeaponSwap = bRunningSwapWeaponTask;
			}
		}
	}
	else
	{
		weapon              = 0;
		weaponObjectExists  = false;
        weaponObjectVisible = false;
        tintIndex           = 0;
		hasAmmo				= true;
		flashLightOn		= false;

		if (doingWeaponSwap)
		{
			*doingWeaponSwap = false;
		}
	}
}

u32 CNetObjPed::GetGadgetData(u32 equippedGadgets[MAX_SYNCED_GADGETS])
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	u32 numEquippedGadgets = 0;

	const CPedWeaponManager *pPedWeaponMgr = pPed ? pPed->GetWeaponManager() : NULL;
	if (pPed && pPedWeaponMgr)
	{
		// ensure the ped doesn't have more currently equipped gadgets than the network game syncs
		gnetAssert(pPedWeaponMgr && pPedWeaponMgr->GetNumEquippedGadgets() <= MAX_SYNCED_GADGETS);

		numEquippedGadgets = MIN(pPedWeaponMgr->GetNumEquippedGadgets(), MAX_SYNCED_GADGETS);

		for(u32 index = 0; index < numEquippedGadgets; index++)
		{
			const CGadget *gadget = pPedWeaponMgr->GetEquippedGadget(index);

			if(AssertVerify(gadget))
			{
				equippedGadgets[index] = gadget->GetWeaponHash();
			}
		}
	}
	else
	{
		for(u32 index = 0; index < MAX_SYNCED_GADGETS; index++)
		{
			equippedGadgets[index] = 0;
		}
	}

	return numEquippedGadgets;
}

u32 CNetObjPed::GetWeaponComponentData(u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS], u8 weaponComponentTints[MAX_SYNCED_WEAPON_COMPONENTS])
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	u32 numWeaponComponents = 0;

	const CPedWeaponManager *pPedWeaponMgr = pPed ? pPed->GetWeaponManager() : NULL;

	if (pPed && pPedWeaponMgr)
	{
		u32 weapon = pPedWeaponMgr->GetEquippedWeapon() ? pPedWeaponMgr->GetEquippedWeapon()->GetWeaponHash() : pPedWeaponMgr->GetEquippedWeaponHash();

		const CPedInventory* inventory	= pPed->GetInventory();

		if(weapon && inventory)
		{
			const CWeaponItem* weaponItem = inventory->GetWeapon(weapon);

			if(weaponItem)
			{
				if(weaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *weaponItem->GetComponents();

					// ensure the weapon doesn't have more currently equipped weapon components than the network game syncs
					gnetAssert(components.GetCount() <= MAX_SYNCED_WEAPON_COMPONENTS);

					numWeaponComponents = MIN(components.GetCount(), MAX_SYNCED_WEAPON_COMPONENTS);

					for(s32 index=0; index<numWeaponComponents; index++)
					{
						const CWeaponComponentInfo* componentInfo = components[index].pComponentInfo;
						if(componentInfo)
						{
							weaponComponentTints[index] = components[index].m_uTintIndex;
							weaponComponents[index] = componentInfo->GetHash();
						}
					}
				}
			}
		}
	}
	else
	{
		for(u32 index = 0; index < MAX_SYNCED_WEAPON_COMPONENTS; index++)
		{
			weaponComponentTints[index] = 0;
			weaponComponents[index] = 0;
		}
	}

	return numWeaponComponents;
}

void CNetObjPed::GetVehicleData(ObjectId &vehicleID, u32 &seat)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	vehicleID = NETWORK_INVALID_OBJECT_ID;
    seat      = 0;

	if (pPed)
	{
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetNetworkObject())
			{
				vehicleID = pPed->GetMyVehicle()->GetNetworkObject()->GetObjectID();
				s32 seatId = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
				seat = static_cast<u32>(seatId+1); // so we can include Seat_Invalid without using another bit
			}
		}
	}
	else
	{
		seat = 0;
	}

	m_inVehicle = (vehicleID != NETWORK_INVALID_OBJECT_ID);
}

void CNetObjPed::GetMountData(ObjectId &mountID)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	mountID = NETWORK_INVALID_OBJECT_ID;
	
	if (pPed)
	{
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ))
		{
			if (pPed->GetMyMount() && pPed->GetMyMount()->GetNetworkObject())
				mountID = pPed->GetMyMount()->GetNetworkObject()->GetObjectID();
		}
	}

	m_onMount = (mountID != NETWORK_INVALID_OBJECT_ID);
}

void CNetObjPed::GetGroupLeader(ObjectId &groupLeaderID)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	groupLeaderID = NETWORK_INVALID_OBJECT_ID;

	if (pPed && !pPed->IsAPlayerPed() && pPed->GetPedsGroup())
	{
		CPed* pLeader = pPed->GetPedsGroup()->GetGroupMembership()->GetLeader();

		gnetAssert(pLeader != pPed);

		if (pLeader && AssertVerify(pLeader->GetNetworkObject()))
		{
			groupLeaderID = pLeader->GetNetworkObject()->GetObjectID();
		}
	}
}

void CNetObjPed::GetPedHealthData(CPedHealthDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		data.m_killedWithHeadShot = false;
		data.m_killedWithMeleeDamage = false;

		//Pack float into unsigned int. Zeroes negative, rounds up to 1, rounds to nearest.
#define PACK_FLOAT(VALUE) (VALUE <= 0.0f ? 0 : (VALUE < 1.0f ? 1 : ::round(VALUE)))

		data.m_endurance = PACK_FLOAT(pPed->GetEndurance());
		data.m_hasMaxEndurance = (data.m_endurance >= pPed->GetMaxEndurance());

		data.m_armour = PACK_FLOAT(pPed->GetArmour());
		data.m_hasDefaultArmour = (data.m_armour == 0);

		data.m_health = PACK_FLOAT(pPed->GetHealth());
		data.m_hasMaxHealth = (data.m_health >= pPed->GetMaxHealth());

#undef PACK_FLOAT

		data.m_maxHealthSetByScript = m_MaxHealthSetByScript;
		if (data.m_maxHealthSetByScript)
		{
			data.m_scriptMaxHealth = (u32)pPed->GetMaxHealth();
		}

		data.m_maxEnduranceSetByScript = m_MaxEnduranceSetByScript;
		if (data.m_maxEnduranceSetByScript)
		{
			data.m_scriptMaxEndurance = (u32)pPed->GetMaxEndurance();
		}

		data.m_weaponDamageComponent = -1; //only fully set on death below

		if (!data.m_hasMaxHealth && pPed->IsFatallyInjured())
		{
			data.m_weaponDamageComponent = pPed->GetWeaponDamageComponent();

			if (data.m_weaponDamageComponent == RAGDOLL_NECK || data.m_weaponDamageComponent == RAGDOLL_HEAD)
			{
				data.m_killedWithHeadShot = m_killedWithHeadShot;
			}

			data.m_killedWithMeleeDamage = m_killedWithMeleeDamage;
		}

		//Calculate the weapon damage entity ID
		data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		data.m_weaponDamageHash   = 0;

		netObject* networkObject = NetworkUtils::GetNetworkObjectFromEntity(pPed->GetWeaponDamageEntity());
		if (networkObject)
		{
			data.m_weaponDamageEntity = networkObject->GetObjectID();
		}
		data.m_weaponDamageHash = pPed->GetWeaponDamageHash();

		if (m_changenextpedhealthdatasynch == WEAPONTYPE_SCRIPT_HEALTH_CHANGE)
		{
			data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
			data.m_weaponDamageHash = WEAPONTYPE_SCRIPT_HEALTH_CHANGE;

			m_changenextpedhealthdatasynch = 0;
		}

		data.m_hurtStarted = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted);
		data.m_hurtEndTime = 0;

		if(pPed->GetHurtEndTime() > 0)
		{
			// only need to store if hurtEndTime is off (0), odd or even for the clone...
			data.m_hurtEndTime = (pPed->GetHurtEndTime() & 1) ? 1 : 2;
		}
	}
	else
	{
		data.m_hasMaxHealth         = true;
		data.m_health               = 0;
		data.m_hasDefaultArmour     = true;
		data.m_armour               = 0;
		data.m_hasMaxEndurance		= true;
		data.m_endurance			= 0;
		data.m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		data.m_weaponDamageHash     = 0;
		data.m_killedWithHeadShot   = false;
		data.m_killedWithMeleeDamage = false;
		data.m_hurtStarted			= false;
		data.m_hurtEndTime			= 0;
	}
}

void CNetObjPed::GetAttachmentData(CPedAttachDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity((CPhysical *) pPed->GetAttachParent());

		bool attached = (pPed->GetAttachParent() && networkObject);

		// peds attached to a car will have their attachment state handled by a clone task
		if (pPed->GetIsAttachedInCar())
			attached = false;
			
		//Peds with a mount will have  their attachment state handled by a clone task.
		if(pPed->GetIsAttachedOnMount())
			attached = false;

		if (attached == false)
		{
			data.m_attachObjectID = NETWORK_INVALID_OBJECT_ID;
		}
		else
		{
			fwAttachmentEntityExtension *extension = GetAttachmentExtension();
			gnetAssert(extension);

			if(extension->GetAttachFlags() & ATTACH_FLAG_DONT_NETWORK_SYNC)
			{
				data.m_attachObjectID = NETWORK_INVALID_OBJECT_ID;
			}
			else
			{
				data.m_attachObjectID		= networkObject->GetObjectID();
				data.m_attachOffset			= extension->GetAttachOffset();
				data.m_attachQuat			= extension->GetAttachQuat();
				data.m_attachBone	        = extension->GetAttachBone();
				data.m_attachFlags	        = extension->GetExternalAttachFlags();
				data.m_attachHeading		= pPed->GetAttachHeading();
				data.m_attachHeadingLimit	= pPed->GetAttachHeadingLimit();
				data.m_attachedToGround		= (extension->GetAttachState() == ATTACH_STATE_PED_ON_GROUND);
			}
		}
	}
	else
	{
		data.m_attachObjectID = NETWORK_INVALID_OBJECT_ID;
		data.m_attachOffset.Zero();
        data.m_attachQuat.Identity();
		data.m_attachBone         = 0;
		data.m_attachFlags        = 0;
		data.m_attachHeading      = 0.0f;
		data.m_attachHeadingLimit = 0.0f;
		data.m_attachedToGround   = false;
	}
}

void CNetObjPed::SetDeadPedRespawnRemovalTimer()
{
	TUNE_INT(RESPAWN_DEAD_PED_REMOVAL_TIMER, 2000, 0, 10000, 100);
	m_RespawnRemovalTimer = sysTimer::GetSystemMsTime() + RESPAWN_DEAD_PED_REMOVAL_TIMER;
}

#if __ASSERT && __BANK

void CNetObjPed::ValidateMotionGroup(CPed* pPed)
{
	CTaskMotionBase *motionTask = pPed->GetPrimaryMotionTask();

	if(gnetVerifyf(motionTask, "Ped has no motion task!"))
	{
		CTaskTypes::eTaskType motionTaskType = (CTaskTypes::eTaskType)motionTask->GetTaskType();

		// I am a human (ped, ped low lod, ride horse) or a horse (on foot horse)
		// only humans and horses in network games...
		if((CTaskTypes::TASK_MOTION_PED != motionTaskType) && 
				(CTaskTypes::TASK_MOTION_PED_LOW_LOD != motionTaskType) &&
					(CTaskTypes::TASK_ON_FOOT_HORSE != motionTaskType) &&
						(CTaskTypes::TASK_MOTION_RIDE_HORSE != motionTaskType))
		{
			u32 scenarioType = CPed::GetScenarioType(*pPed, true);

			if(scenarioType != Scenario_Invalid)
			{
				const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
				if(pScenarioInfo)
				{
					char const* scenarioName = pScenarioInfo->GetName();
				
					CScenarioPoint const* scenarioPoint = CPed::GetScenarioPoint(*pPed);
					if(scenarioPoint)
					{
						int numScenarioRegions = SCENARIOPOINTMGR.GetNumRegions();
						for(int r = 0; r < numScenarioRegions; ++r)
						{
							const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(r);
							if(region)
							{
								const CScenarioPointRegionEditInterface bankRegion(*region);
								if(bankRegion.IsPointInRegion(*scenarioPoint))
								{
									char const* regionName = SCENARIOPOINTMGR.GetRegionName(r);

									Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

									gnetAssertf(false,
										"PLEASE COPY THIS MEESAGE TO B* 505988 - ERROR : CNetObjPed::GetMotionGroup : Ped %s %s - Unsupported ped type in MP - Non Horse Animal?! Invalid Scenario Point?! %f %f %f : motionTaskType: %d scenarioName: %s Region: %s", 
										pPed->GetDebugName(),
										pPed->GetModelName(),
										pos.x, pos.y, pos.z,
										motionTaskType,
										scenarioName,
										regionName);

									return;
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif /* __ASSERT && __BANK */

#if __ASSERT

void  CNetObjPed::CheckDamageEventOnDie()
{
#if __BANK
	CPed* ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped && GetLastDamageEventPending() && NetworkDebug::DebugPedMissingDamageEvents() && !ped->IsPlayer())
	{
		netObject* neInflictor = NetworkUtils::GetNetworkObjectFromEntity(ped->GetWeaponDamageEntity());
		if (neInflictor)
		{
			const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(ped->GetWeaponDamageHash());
			if (wi)
			{
				gnetAssertf(0, "Setting Ped %s dead without triggering a damage event - Inflictor=%s, weapon=%s.", GetLogName(), neInflictor->GetLogName(), wi->GetName());
			}
			else
			{
				gnetAssertf(0, "Setting Ped %s dead without triggering a damage event - Inflictor=%s, weapon=%d.", GetLogName(), neInflictor->GetLogName(), ped->GetWeaponDamageHash());
			}
		}
		else
		{
			gnetAssertf(0, "Setting Ped %s dead without triggering a damage event - Inflictor=None, weapon=None.", GetLogName());
		}
	}
#endif /* __BANK */
}

#endif /* __ASSERT */

void CNetObjPed::GetMotionGroup(CPedMovementGroupDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{

#if __ASSERT && __BANK

		ValidateMotionGroup(pPed);

#endif /* __ASSERT && __BANK*/

		CTaskMotionBase *motionTask = pPed->GetPrimaryMotionTask();

		if(gnetVerifyf(motionTask, "Ped has no motion task!"))
		{
			data.m_moveBlendType			= (u16)motionTask->GetTaskType();
			data.m_moveBlendState			= motionTask->GetState();
			data.m_motionSetId				= motionTask->GetActiveClipSet();
			data.m_overriddenWeaponSetId	= motionTask->GetOverrideWeaponClipSet();
            data.m_overriddenStrafeSetId    = motionTask->GetOverrideStrafingClipSet();
            data.m_isCrouching	            = pPed->GetMotionData()->GetIsCrouching();
            data.m_isStealthy		        = pPed->GetMotionData()->GetUsingStealth();
			data.m_isStrafing				= pPed->GetMotionData()->GetIsStrafing();
			data.m_isRagdolling				= pPed->GetRagdollState() > RAGDOLL_STATE_ANIM || pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame;

			data.m_isRagdollConstraintAnkleActive = false;
			data.m_isRagdollConstraintWristActive = false;

			if(pPed->GetRagdollConstraintData())
			{
				data.m_isRagdollConstraintAnkleActive = pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled();
				data.m_isRagdollConstraintWristActive = pPed->GetRagdollConstraintData()->HandCuffsAreEnabled();
			}

			if(motionTask->GetSubTask() && motionTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION)
            {
                CTaskMotionBasicLocomotion *locoTask = static_cast<CTaskMotionBasicLocomotion *>(motionTask->GetSubTask());
                data.m_motionSetId = locoTask->GetClipSet();
            }

			data.m_motionInVehiclePitch = 0.0f;
			if (pPed && pPed->GetIsDrivingVehicle())
			{
				CVehicle* pVehicle = pPed->GetMyVehicle();
				VehicleType eVehicleType = pVehicle->GetVehicleType();
				if ((eVehicleType == VEHICLE_TYPE_BIKE) || (eVehicleType == VEHICLE_TYPE_QUADBIKE) || (eVehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
				{
					CTaskMotionInAutomobile* pMotionInAutomobileTask = static_cast<CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
					if (pMotionInAutomobileTask)
					{
						data.m_motionInVehiclePitch = pMotionInAutomobileTask->GetPitchAngle();
					}
				}
			}

			GetTennisMotionData(data.m_TennisMotionData);
        }
	}
	else
	{
		data.m_moveBlendType = INVALID_PENDING_MOTION_TYPE;
		data.m_moveBlendState = INVALID_PENDING_MOTION_STATE;
		data.m_motionSetId = CLIP_SET_ID_INVALID;
		data.m_overriddenWeaponSetId = CLIP_SET_ID_INVALID;
        data.m_overriddenStrafeSetId = CLIP_SET_ID_INVALID;
		data.m_isCrouching = data.m_isStealthy = data.m_isStrafing = data.m_isRagdolling = data.m_isRagdollConstraintAnkleActive = data.m_isRagdollConstraintWristActive = false;
		data.m_motionInVehiclePitch = 0.f;
		data.m_TennisMotionData.Reset();
	}
}

void CNetObjPed::GetPedMovementData(CPedMovementDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio(data.m_DesiredMoveBlendRatioX, data.m_DesiredMoveBlendRatioY);
		
		data.m_MaxMoveBlendRatio = pPed->GetMotionData()->GetScriptedMaxMoveBlendRatio();

        data.m_HasStopped   = pPed->GetVelocity().Mag2() < 0.1f;
        data.m_DesiredPitch = pPed->GetDesiredPitch();
	}
	else
	{
		data.m_DesiredMoveBlendRatioX = data.m_DesiredMoveBlendRatioY = data.m_DesiredPitch = 0.0f;
		data.m_MaxMoveBlendRatio = MOVEBLENDRATIO_SPRINT;
        data.m_HasStopped = true;
	}

    gnetAssert(rage::Abs(data.m_DesiredMoveBlendRatioX) <= MOVEBLENDRATIO_SPRINT);
    gnetAssert(rage::Abs(data.m_DesiredMoveBlendRatioY) <= MOVEBLENDRATIO_SPRINT);
}

void CNetObjPed::GetPedAppearanceData(CPackedPedProps &pedProps, CSyncedPedVarData& variationData, u32 &phoneMode, u8& parachuteTintIndex, u8& parachutePackTintIndex, fwMvClipSetId& facialClipSetId, u32 &facialIdleAnimOverrideClipNameHash, u32 &facialIdleAnimOverrideClipDictHash, bool& isAttachingHelmet, bool& isRemovingHelmet, bool& isWearingHelmet, u8& helmetTextureId, u16& helmetProp, u16& visorPropUp, u16& visorPropDown, bool& visorIsUp, bool& supportsVisor, bool& isVisorSwitching, u8& targetVisorState)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		//! Clones don't need to know about reserve chute - they just need to know the tint ID of the chute they will be using.
		parachuteTintIndex = (u8)pPed->GetPedIntelligence()->GetCurrentTintIndexForParachute();
	
		parachutePackTintIndex	= (u8)pPed->GetPedIntelligence()->GetTintIndexForParachutePack();

#if !__NO_OUTPUT
		if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			if (pPed->IsPlayer())
			{
				weaponitemDebugf3("CNetObjPed::GetPedAppearanceData pPed[%p][%s] player[%s] isplayer[%d] tintIndex[%d] packTintIndex[%d]",pPed,pPed->GetModelName(), GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",pPed->IsPlayer(),parachuteTintIndex,parachutePackTintIndex);
			}
		}
#endif

		pedProps = CPedPropsMgr::GetPedPropsPacked(pPed);

		variationData.ExtractFromPed(*pPed);
		GetPhoneModeData(phoneMode);
		GetHelmetAttachmentData(isAttachingHelmet, isRemovingHelmet, isWearingHelmet, isVisorSwitching, targetVisorState);
		GetHelmetComponentData(helmetTextureId, helmetProp, visorPropUp, visorPropDown, visorIsUp, supportsVisor);

		if(pPed->GetFacialData())
		{
			facialClipSetId						= pPed->GetFacialData()->GetFacialClipSet();
			facialIdleAnimOverrideClipNameHash	= pPed->GetFacialData()->GetFacialIdleAnimOverrideClipNameHash();
			facialIdleAnimOverrideClipDictHash	= pPed->GetFacialData()->GetFacialIdleAnimOverrideClipDictNameHash();
		}
		else
		{
			facialClipSetId						= CLIP_SET_ID_INVALID;
			facialIdleAnimOverrideClipNameHash	= 0;
			facialIdleAnimOverrideClipDictHash	= 0;		
		}
	}
	else
	{
		parachuteTintIndex		= 0;
		parachutePackTintIndex	= 0;

#if !__NO_OUTPUT
		if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			if (pPed->IsPlayer())
				weaponitemDebugf3("CNetObjPed::GetPedAppearanceData player[%s] !pPed -- zero",GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "");
		}
#endif

		isRemovingHelmet  = false;
		isAttachingHelmet = false;
		helmetTextureId   = 0;
		helmetProp		  = 0;

		pedProps.Reset();
		variationData.Reset();
		phoneMode = 0;

		facialClipSetId							= CLIP_SET_ID_INVALID;
		facialIdleAnimOverrideClipNameHash		= 0;
		facialIdleAnimOverrideClipDictHash		= 0;
	}
}

void CNetObjPed::GetAIData(CPedAIDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// write relationship group
		gnetAssert(pPed->GetPedIntelligence()->GetRelationshipGroup());
		data.m_relationshipGroup = static_cast<u32>(pPed->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash());

		// write decision makers
		data.m_decisionMakerType = pPed->GetPedIntelligence()->GetDecisionMakerId();
	}
	else
	{
		data.m_relationshipGroup = data.m_decisionMakerType = 0;
	}

    if(m_pendingRelationshipGroup != 0)
    {
        data.m_relationshipGroup = m_pendingRelationshipGroup;
    }
}
void CNetObjPed::GetTaskTreeData(CPedTaskTreeDataNode& data)
{
	// we need to update the local task slot cache here as task data can be read
	// in response to message received from the network, which are processed before
	// the network objects are updated. This function will only update the cache once
	// per frame, so it shouldn't have any performance impact
	if (!IsClone())
	{
		UpdateLocalTaskSlotCache();
	}

	for (int i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
	{
		IPedNodeDataAccessor::TaskSlotData &taskInfo = data.m_taskTreeData[i];
		const IPedNodeDataAccessor::TaskSlotData &pedTaskInfo = m_taskSlotCache.m_taskSlots[i];

		taskInfo.m_taskType			= pedTaskInfo.m_taskType;
		taskInfo.m_taskPriority		= pedTaskInfo.m_taskPriority;
		taskInfo.m_taskTreeDepth	= pedTaskInfo.m_taskTreeDepth;
		taskInfo.m_taskSequenceId	= pedTaskInfo.m_taskSequenceId;
		taskInfo.m_taskActive		= pedTaskInfo.m_taskActive;
	}

	// write the scripted task status 
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(!ped)
	{
		data.m_scriptCommand = SCRIPT_TASK_INVALID;
		data.m_taskStage     = CPedScriptedTaskRecordData::VACANT_STAGE;
	}
	else
	{
		data.m_scriptCommand = ped->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskCommand();
		data.m_taskStage     = ped->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskStage();
	}
}

void CNetObjPed::GetTaskSpecificData(CPedTaskSpecificDataNode& data)
{
    IPedNodeDataAccessor::TaskSlotData &taskSlot = m_taskSlotCache.m_taskSlots[data.m_taskIndex];

	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	data.m_taskData.m_TaskType		= taskSlot.m_taskType;
	data.m_taskData.m_TaskDataSize	= 0;

	if(data.m_taskData.m_TaskType != CTaskTypes::MAX_NUM_TASK_TYPES)
    {
		CQueriableInterface *queriableInterface = ped ? ped->GetPedIntelligence()->GetQueriableInterface() : 0;

		CTaskInfo *taskInfo = queriableInterface ? queriableInterface->GetTaskInfoForTaskType(data.m_taskData.m_TaskType, taskSlot.m_taskPriority) : 0;

		if(taskInfo)
		{
			NET_ASSERTS_ONLY(u32 taskSize = taskInfo->GetSpecificTaskInfoSize());
			gnetAssertf(taskSize <= CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE, "Task %s is larger than MAX_SPECIFIC_TASK_INFO_SIZE, increase to %d", TASKCLASSINFOMGR.GetTaskName(taskInfo->GetTaskType()), taskSize);

			datBitBuffer messageBuffer;
			messageBuffer.SetReadWriteBits(data.m_taskData.m_TaskData, CTaskInfo::MAX_SPECIFIC_TASK_INFO_SIZE+1, 0);
			taskInfo->WriteSpecificTaskInfo(messageBuffer);
			data.m_taskData.m_TaskDataSize = messageBuffer.GetCursorPos();
		}
		else
		{
			gnetAssertf(0, "%s: task %s not found for task specific data slot %d", GetLogName(), TASKCLASSINFOMGR.GetTaskName(data.m_taskData.m_TaskType), data.m_taskIndex);
	    }
	}
}

void CNetObjPed::GetPedInventoryData(CPedInventoryDataNode& data)
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	data.m_numItems = data.m_numAmmos = 0;

    if(ped && ped->GetInventory())
    {
        // grab the hashes of all items in the inventory
        const atArray<CWeaponItem*>& inventoryItems = ped->GetInventory()->GetWeaponRepository().GetAllItems();
 
        for(u32 index = 0; index < inventoryItems.GetCount(); index++)
        {
            const CWeaponInfo *weaponInfo = inventoryItems[index] ? inventoryItems[index]->GetInfo() : 0;

			// ignore props and the unarmed weapon
            if(weaponInfo && !weaponInfo->GetIsUnarmed() && weaponInfo->GetHash() != OBJECTTYPE_OBJECT)
            {
				data.m_itemSlots[data.m_numItems] = CWeaponInfoManager::GetInfoSlotForWeapon(weaponInfo->GetHash());

				data.m_itemSlotTint[data.m_numItems] = inventoryItems[index]->GetTintIndex(); //u8

				data.m_itemSlotNumComponents[data.m_numItems] = 0; //reset to zero in case there aren't any components but previously there were.

				if (inventoryItems[index]->GetComponents())
				{
					const CWeaponItem::Components& components = *inventoryItems[index]->GetComponents();

					data.m_itemSlotNumComponents[data.m_numItems] = (u8) components.GetCount();

					// ensure the weapon doesn't have more currently equipped weapon components than the network game syncs
					gnetAssert(data.m_itemSlotNumComponents[data.m_numItems] <= IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS);
					
					data.m_itemSlotNumComponents[data.m_numItems] = (u8) MIN(data.m_itemSlotNumComponents[data.m_numItems], IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS);

					for (u8 i = 0; i < data.m_itemSlotNumComponents[data.m_numItems]; i++)
					{
						data.m_itemSlotComponent[data.m_numItems][i] = 0; //ensure this is reset

						const CWeaponComponentInfo* pComponentInfo = components[i].pComponentInfo;
						if (pComponentInfo)
						{
							data.m_itemSlotComponent[data.m_numItems][i] = pComponentInfo->GetHash();
							data.m_itemSlotComponentActive[data.m_numItems][i] = components[i].m_bActive;
						}
					}
				}

				data.m_numItems++;
            }
        }

        // grab the information about the ammo in the inventory
        data.m_allAmmoInfinite = ped->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteAmmo();

        const atArray<CAmmoItem*>& ammoItems = ped->GetInventory()->GetAmmoRepository().GetAllItems();
        data.m_numAmmos = ammoItems.GetCount();

        for(u32 index = 0; index < data.m_numAmmos; index++)
        {
            const CAmmoInfo *ammoInfo = ammoItems[index] ? ammoItems[index]->GetInfo() : 0;

            if(ammoInfo)
            {
                data.m_ammoSlots[index]    = CWeaponInfoManager::GetInfoSlotForWeapon(ammoInfo->GetHash());
                data.m_ammoQuantity[index] = ammoItems[index]->GetAmmo();
				data.m_ammoInfinite[index] = ammoInfo->GetIsUsingInfiniteAmmo();
            }
        }
    }
	else
	{
		for (int i=0; i<MAX_WEAPONS; i++)
		{
			data.m_itemSlots[i] = 0;
		}

        data.m_numItems = 0;

		for (int i=0; i<MAX_AMMOS; i++)
		{
			data.m_ammoSlots[i] = data.m_ammoQuantity[i] = 0;
			data.m_ammoInfinite[i] = false;
		}

        data.m_numAmmos        = 0;
        data.m_allAmmoInfinite = false;
	}
}

void CNetObjPed::GetPedComponentReservations(CPedComponentReservationDataNode& data)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	if(pPed && pPed->GetComponentReservationMgr())
	{
		CComponentReservationManager* pCRM = pPed->GetComponentReservationMgr();

		data.m_numPedComponents = pCRM->GetNumReserveComponents();

		for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
		{
			CComponentReservation *componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);

			if(AssertVerify(componentReservation))
			{
				CPed *ped = componentReservation->GetPedUsingComponent();
				data.m_componentReservations[componentIndex] = NetworkUtils::GetObjectIDFromGameObject(ped);
			}
		}
	}
	else
	{
		//Clear the number of components.
		data.m_numPedComponents = 0;
	}
}

void CNetObjPed::GetTaskSequenceData(bool& hasSequence, u32& sequenceResourceId, u32& numTasks, IPedNodeDataAccessor::CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32& repeatMode)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	numTasks = 0;
	sequenceResourceId = 0;

	if(pPed)
	{
		CTaskUseSequence* pSequenceTask = static_cast<CTaskUseSequence*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE));

		if (pSequenceTask)
		{
			hasSequence = true;

			sequenceResourceId = CScriptResource_SequenceTask::GetSequenceIdFromResourceId(pSequenceTask->GetSequenceResourceID());

			if (sequenceResourceId != 0)
			{
				netPlayer* pPendingPlayer = GetPendingPlayerOwner();

				// don't send the task sequence to another player that is running the script as they will already have it
				if (pPendingPlayer)
				{
					CScriptEntityExtension* extension = pPed->GetExtension<CScriptEntityExtension>();

					if (extension)
					{
						if (CTheScripts::GetScriptHandlerMgr().IsPlayerAParticipant(extension->GetScriptInfo()->GetScriptId(), *pPendingPlayer))
						{
							return;
						}
					}
				}
				
				const CTaskSequenceList* pSequenceList = pSequenceTask->GetTaskSequenceList();

				if (pSequenceList && AssertVerify(!pSequenceList->IsMigrationPrevented()))
				{
					ExtractTaskSequenceData(GetObjectID(), *pSequenceList, numTasks, taskData, repeatMode);
				}
			}
		}
	}
}



// sync parser setter functions
void CNetObjPed::SetPedCreateData(const CPedCreationDataNode& data)
{
    // ensure that the model is loaded and ready for drawing for this ped
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(data.m_modelHash, &modelId);

	bool createGameObject = true;

	if (data.m_IsRespawnObjId)
	{
		createGameObject = false;

		//These flags are set to the ped that is created for use during player ped swapping.
		SetLocalFlag(CNetObjGame::LOCALFLAG_RESPAWNPED, true);
		SetLocalFlag(CNetObjGame::LOCALFLAG_NOREASSIGN, true);

		netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "PEDCREATE_TEAM_SWAP", GetLogName());
		networkLog.WriteDataValue("Flag Set:", "%s", "LOCALFLAG_RESPAWNPED");
	}

    if (createGameObject && AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)))
    {
        Matrix34 tempMat;
        tempMat.Identity();

#if PHINST_VALIDATE_POSITION
        // set the x,y and z components to random numbers to avoid a physics assert
        tempMat.d.x = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
        tempMat.d.y = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
        tempMat.d.z = fwRandom::GetRandomNumberInRange(10.1f, 11.0f);
#endif // PHINST_VALIDATE_POSITION

#if __DEV
		// verify that ped should have collision data - bug 228055 - otherwise ped creation is going to fail
		// check that ped archetype isn't specifying a fragment (which it should),
		// or that the fragment has loaded (which is should have - as it is an archetype dependency)
		CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(modelId);
		gnetAssert(pMI);
		if (pMI){
			gnetAssertf(pMI->GetFragmentIndex() != INVALID_FRAG_IDX, "ped %s has invalid fragment idx", pMI->GetModelName());
			gnetAssertf(g_FragmentStore.HasObjectLoaded(strLocalIndex(pMI->GetFragmentIndex())), "ped %s fragment has not loaded- even though it's a dependency",pMI->GetModelName());
		}
#endif

        // create the ped
        const CControlledByInfo networkNpcControl(true, false);
		
		// try to reuse a ped
		CPed * pPed = NULL;
		int pedReuseIndex = CPedPopulation::FindPedToReuse(modelId.GetModelIndex());		
		
		if(pedReuseIndex != -1)
		{
			pPed = CPedPopulation::GetPedFromReusePool(pedReuseIndex);
			if(CPedPopulation::ReusePedFromPedReusePool(pPed, tempMat, networkNpcControl, false/*createdByScript*/, false/*shouldBeCloned*/, pedReuseIndex))
			{
				// Reuse Successful
				CPedPopulation::RemovePedFromReusePool(pedReuseIndex);
			}
			else
			{
				pPed = NULL; // failed to reuse ped
			}
		}

		if(!pPed)
		{
			// create the ped
			pPed = CPedFactory::GetFactory()->CreatePed(networkNpcControl, modelId, &tempMat, true, false, false);
		}

		NETWORK_QUITF(pPed, "Failed to create a cloned ped. Cannot continue.");

        pPed->PopTypeSet((ePopType)data.m_popType);

		if (pPed->PopTypeIsMission())
		{
			pPed->SetupMissionState();
		}
		else
		{
			pPed->SetDefaultDecisionMaker();
			pPed->SetCharParamsBasedOnManagerType();
			pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		}

		SetGameObject((void*) pPed);
		pPed->SetNetworkObject(this);

        pPed->SetRandomSeed((u16)data.m_randomSeed);

        SetVehicleData(data.m_vehicleID, data.m_seat);

        if (data.m_hasProp)
        {
            m_requiredProp.SetHash(data.m_propHash);
        }

        CGameWorld::Add(pPed, CGameWorld::OUTSIDE );
		pPed->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);
		pPed->GetPortalTracker()->RequestRescanNextUpdate();
		pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

        // remove any initial weapon from the ped - this is dictated by the sync data
		if (pPed->GetInventory())
			CPedInventoryLoadOutManager::SetDefaultLoadOut(pPed);

		// setup this ped's AI as a network clone
		SetupPedAIAsClone();

        CPedPopulation::AddPedToPopulationCount(pPed);

        // setup the players default variations
        pPed->SetVarDefault();

        pPed->SetIsStanding(data.m_isStanding);

		// activate physics so that ped finds the ground to stand on (otherwise ped will remain sunk into the ground if it does not move)
		pPed->ActivatePhysics();

		// setup dead ped removal flag
		m_RespawnRemovalTimer = 0;
		if (data.m_RespawnFlaggedForRemoval)
		{
			SetDeadPedRespawnRemovalTimer();
		}

		m_AttDamageToPlayer = INVALID_PLAYER_INDEX;
		if (data.m_hasAttDamageToPlayer)
		{
			m_AttDamageToPlayer = data.m_attDamageToPlayer;
		}

		pPed->SetMaxHealth((float)data.m_maxHealth);

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, data.m_wearingAHelmet );

		if (pPed->GetSpeechAudioEntity())
		{
			pPed->GetSpeechAudioEntity()->SetAmbientVoiceName(data.m_voiceHash, true);
		}

#if __BANK
		CPedInventoryDataNode currdata;
		GetPedInventoryData(currdata);

		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "COP_PED_INVENTORY", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Num inventory items:", "%d (old: %d)", currdata.m_numItems, m_NumInventoryItems);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Infinite ammo:", "%d (old: %d)", currdata.m_allAmmoInfinite, m_PedUsingInfiniteAmmo);
#endif
    }

	VALIDATE_GAME_OBJECT(GetPed());
}

void CNetObjPed::SetScriptPedCreateData(const CPedScriptCreationDataNode& data)
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped)
	{
        ped->SetPedConfigFlag( CPED_CONFIG_FLAG_StayInCarOnJack, data.m_StayInCarWhenJacked );
	}
}

void CNetObjPed::SetPedGameStateData(const CPedGameStateDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepTasksAfterCleanUp, data.m_keepTasksAfterCleanup );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByConcealedPlayer, data.m_createdByConcealedPlayer );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw, data.m_dontBehaveLikeLaw );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HitByTranqWeapon, data.m_hitByTranqWeapon );
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated, data.m_canBeIncapacitated);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableStartEngine, data.m_bDisableStartEngine);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableBlindFiringInShotReactions, data.m_disableBlindFiringInShotReactions);

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact, data.m_dontActivateRagdollFromAnyPedImpact );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PermanentlyDisablePotentialToBeWalkedIntoResponse, data.m_permanentlyDisablePotentialToBeWalkedIntoResponse );

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ChangeFromPermanentToAmbientPopTypeOnMigration, data.m_changeToAmbientPopTypeOnMigration );

		pPed->SetArrestState((eArrestState)data.m_arrestState);
		pPed->SetDeathState((eDeathState)data.m_deathState);

		m_HasValidWeaponClipset = data.m_HasValidWeaponClipset;

		m_lastSyncedWeaponHash = data.m_weapon;

		m_hasDroppedWeapon = data.m_hasDroppedWeapon;

		bool bCacheWeaponInfo = CacheWeaponInfo() || data.m_doingWeaponSwap;

		weaponitemDebugf2("CNetObjPed::SetPedGameStateData player[%s] isplayer[%d] data.m_weaponObjectExists[%d] data.m_numWeaponComponents[%d] data.m_weapon[%u] weaponHash[%u] bCacheWeaponInfo[%d]",
			GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",
			pPed->IsPlayer(),
			data.m_weaponObjectExists,data.m_numWeaponComponents,data.m_weapon,m_lastSyncedWeaponHash,bCacheWeaponInfo);

		if(!bCacheWeaponInfo)
		{
			m_CachedWeaponInfo.m_pending = false;

			m_CanAlterPedInventoryData = true;

			if (m_numRemoteWeaponComponents != (u8) data.m_numWeaponComponents)
			{
				weaponitemDebugf2("CNetObjPed::SetPedGameStateData (m_numRemoteWeaponComponents != data.m_numWeaponComponents) --> UpdateWeaponSlots(0)");
				UpdateWeaponSlots(0);
			}

			SetWeaponComponentData(m_lastSyncedWeaponHash, data.m_weaponComponents, data.m_weaponComponentsTint, data.m_numWeaponComponents);
			SetWeaponData(m_lastSyncedWeaponHash, data.m_weaponObjectExists, data.m_weaponObjectVisible, data.m_weaponObjectTintIndex, data.m_flashLightOn, data.m_weaponObjectHasAmmo, data.m_weaponObjectAttachLeft);
			SetGadgetData(data.m_equippedGadgets, data.m_numGadgets);

			m_CanAlterPedInventoryData = false;
		}
		else
		{
			SetCachedWeaponInfo(m_lastSyncedWeaponHash,
								data.m_weaponObjectExists,
								data.m_weaponObjectVisible,
								data.m_weaponObjectTintIndex,
								data.m_weaponComponentsTint,
								data.m_weaponComponents,
								data.m_numWeaponComponents,
								data.m_equippedGadgets,
								data.m_numGadgets,
								data.m_flashLightOn,
								data.m_weaponObjectHasAmmo,
								data.m_weaponObjectAttachLeft);
		}

		m_numRemoteWeaponComponents = (u8) data.m_numWeaponComponents;

		if (data.m_inVehicle)
		{
			SetVehicleData(data.m_vehicleID, data.m_seat);
		}
		else
		{
			SetVehicleData(NETWORK_INVALID_OBJECT_ID, 0);
		}

		m_myVehicleId = data.m_vehicleID;

		SetMountData(data.m_mountID);

		CPed* pCustodianPed = NULL;
		if(data.m_custodianID != NETWORK_INVALID_OBJECT_ID)
		{
			pCustodianPed = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(data.m_custodianID));
		}

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed,      data.m_arrestFlags.m_isHandcuffed);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest,  data.m_arrestFlags.m_canPerformArrest);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff,  data.m_arrestFlags.m_canPerformUncuff);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested,     data.m_arrestFlags.m_canBeArrested);
		pPed->SetInCustody(data.m_arrestFlags.m_isInCustody, pCustodianPed);

		if (pPed->IsAPlayerPed())
		{
			pPed->SetUsingActionMode(data.m_bActionModeEnabled, CPed::AME_Network);
		}
		else
		{
			pPed->SetUsingActionMode(data.m_bActionModeEnabled, CPed::AME_Script);
		}

		pPed->GetMotionData()->SetUsingStealth(data.m_bStealthModeEnabled);
		pPed->SetMovementModeOverrideHash(data.m_nMovementModeOverrideID);

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth, data.m_killedByStealth);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown, data.m_killedByTakedown);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout, data.m_killedByKnockdown);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStandardMelee, data.m_killedByStandardMelee);

		if (m_ClearDamageCount != data.m_cleardamagecount)
		{
			pPed->ClearDamage();
			m_ClearDamageCount = data.m_cleardamagecount;
		}

		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
		if (AssertVerify(pPedIntelligence))
		{
			pPedIntelligence->GetPedPerception().SetHearingRange(static_cast< float > (data.m_PedPerception.m_HearingRange));
			pPedIntelligence->GetPedPerception().SetSeeingRange(static_cast< float > (data.m_PedPerception.m_SeeingRange));

			pPedIntelligence->GetPedPerception().SetSeeingRangePeripheral(static_cast< float > (data.m_PedPerception.m_SeeingRangePeripheral));
			pPedIntelligence->GetPedPerception().SetCentreOfGazeMaxAngle(data.m_PedPerception.m_CentreOfGazeMaxAngle);
			pPedIntelligence->GetPedPerception().SetVisualFieldMinAzimuthAngle(data.m_PedPerception.m_VisualFieldMinAzimuthAngle);
			pPedIntelligence->GetPedPerception().SetVisualFieldMaxAzimuthAngle(data.m_PedPerception.m_VisualFieldMaxAzimuthAngle);

			pPedIntelligence->GetPedPerception().SetIsHighlyPerceptive(data.m_PedPerception.m_bIsHighlyPerceptive);
		}

		bool bIsLooking = pPed->GetIkManager().IsLooking();
		if(bIsLooking && !data.m_isLookingAtObject)
		{
			pPed->GetIkManager().AbortLookAt(500);
			m_pendingLookAtObjectId = NETWORK_INVALID_OBJECT_ID;
		}
		else if(!bIsLooking && data.m_isLookingAtObject)
		{
			// might not have the object - pick this up in Update
			m_pendingLookAtObjectId = data.m_LookAtObjectID;
			m_pendingLookAtFlags = data.m_LookAtFlags;
		}
		else if(bIsLooking && data.m_isLookingAtObject)
		{
			netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_LookAtObjectID);
			CEntity* pEntity = pNetObj ? pNetObj->GetEntity() : 0;

			if(pEntity != pPed->GetIkManager().GetLookAtEntity())
			{
				// we've changed the object we were looking at....
				m_pendingLookAtObjectId = data.m_LookAtObjectID;
				m_pendingLookAtFlags = data.m_LookAtFlags;
			}
		}

		if (data.m_isUpright && !pPed->GetIsAttached() && !pPed->GetUsingRagdoll() && pPed->GetMovePed().GetState() != CMovePed::kStateStaticFrame)
		{
			float fUpright = pPed->GetTransform().GetC().GetZf();
			if (fUpright < 0.99999f)
			{
				pedDebugf3("CNetObjPed::SetPedGameStateData--pPed[%p][%s][%s]--fUpright[%f] correct to 1.0f",pPed,pPed->GetModelName(),pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",fUpright);

				// ensure the player is upright
				Matrix34 newMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());

				// make the peds's matrix vertical again.
				newMatrix.MakeRotateZ(pPed->GetTransform().GetHeading());

				// set the matrix to the newMatrix to enforce the uprightness
				pPed->SetMatrix(newMatrix);

				// ensure the desired bound pitch is set to zero also
				pPed->SetDesiredBoundPitch(0.0f);
			}
		}

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle, data.m_isDuckingInVehicle);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans, data.m_isUsingLowriderLeanAnims);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingAlternateLowriderLeans, data.m_isUsingAlternateLowriderLeanAnims );
	}
}

void CNetObjPed::UpdateCachedWeaponInfo(bool const forceUpdate /* = false */)
{
	if(m_CachedWeaponInfo.m_pending && (!CacheWeaponInfo() || forceUpdate))
	{
		m_CanAlterPedInventoryData = true;

		// make sure the weapon object is created 
		if (GetPed())
		{
			GetPed()->GetWeaponManager()->SetCreateWeaponObjectWhenLoaded(true);
		}

		if (m_CachedWeaponInfo.m_numWeaponComponents && GetWeaponComponentsCount(m_CachedWeaponInfo.m_weapon) != m_CachedWeaponInfo.m_numWeaponComponents)
		{
			UpdateWeaponSlots(0);
		}

		SetWeaponComponentData(m_CachedWeaponInfo.m_weapon, m_CachedWeaponInfo.m_weaponComponents, m_CachedWeaponInfo.m_weaponComponentsTint, m_CachedWeaponInfo.m_numWeaponComponents);
		SetWeaponData(m_CachedWeaponInfo.m_weapon, m_CachedWeaponInfo.m_weaponObjectExists, m_CachedWeaponInfo.m_weaponObjectVisible, m_CachedWeaponInfo.m_weaponObjectTintIndex, m_CachedWeaponInfo.m_weaponObjectFlashLightOn, m_CachedWeaponInfo.m_weaponObjectHasAmmo, m_CachedWeaponInfo.m_weaponObjectAttachLeft, forceUpdate);
		SetGadgetData(m_CachedWeaponInfo.m_equippedGadgets, m_CachedWeaponInfo.m_numGadgets);

		m_CanAlterPedInventoryData = false;

		m_CachedWeaponInfo.Reset();
	}
}

void CNetObjPed::SetCachedWeaponInfo(	const u32      weapon,
										const bool     weaponObjectExists,
										const bool     weaponObjectVisible,
                                        const u8       weaponObjectTintIndex,
										const u8	   weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS],
										const u32      weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS],
										const u32      numWeaponComponents,
										const u32      equippedGadgets[MAX_SYNCED_GADGETS],
										const u32      numGadgets,
										const bool	   weaponObjectFlashLightOn,
										const bool	   weaponObjectHasAmmo,
										const bool	   weaponObjectAttachLeft)
{
	m_CachedWeaponInfo.m_pending					= true;

	m_CachedWeaponInfo.m_weapon						= weapon;
	m_CachedWeaponInfo.m_weaponObjectExists			= weaponObjectExists;
	m_CachedWeaponInfo.m_weaponObjectVisible		= weaponObjectVisible;
    m_CachedWeaponInfo.m_weaponObjectTintIndex      = weaponObjectTintIndex;
	m_CachedWeaponInfo.m_numWeaponComponents		= numWeaponComponents;
	m_CachedWeaponInfo.m_numGadgets					= numGadgets;
	m_CachedWeaponInfo.m_weaponObjectFlashLightOn   = weaponObjectFlashLightOn;
	m_CachedWeaponInfo.m_weaponObjectHasAmmo		= weaponObjectHasAmmo;
	m_CachedWeaponInfo.m_weaponObjectAttachLeft		= weaponObjectAttachLeft;

	for(u32 index = 0; index < MAX_SYNCED_WEAPON_COMPONENTS; index++)
	{
		m_CachedWeaponInfo.m_weaponComponentsTint[index]	= weaponComponentsTint[index];
	}
	for(u32 index = 0; index < MAX_SYNCED_WEAPON_COMPONENTS; index++)
	{
		m_CachedWeaponInfo.m_weaponComponents[index]	= weaponComponents[index];
	}
	for(u32 index = 0; index < MAX_SYNCED_GADGETS; index++)
	{
		m_CachedWeaponInfo.m_equippedGadgets[index]		= equippedGadgets[index];
	}
}

void CNetObjPed::SetPedScriptGameStateData(const CPedScriptGameStateDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// has char become a mission char?
		if (data.m_popType == POPTYPE_MISSION)
		{
			if(!pPed->PopTypeIsMission())
			{
				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowMedicsToReviveMe, FALSE );
			}
		}

		if (((ePopType)data.m_popType) != pPed->PopTypeGet())
		{
			gnetAssertf(!(pPed->IsLocalPlayer()), "The real player ped should always be a permanent character");
			pPed->PopTypeSet((ePopType)data.m_popType);
			pPed->SetDefaultDecisionMaker();
			pPed->SetCharParamsBasedOnManagerType();
			if(pPed->GetPedIntelligence())
				pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		}

		if (data.m_pedType != PEDTYPE_LAST_PEDTYPE)
		{
			//When the ped type changes...
			if (data.m_pedType != (u32) pPed->GetPedType())
			{
				//mimic code from CommandSetPedAsCop - we need to ensure the population counts are accurate for different machines too
				CPedPopulation::RemovePedFromPopulationCount(pPed);
				pPed->SetPedType((ePedType) data.m_pedType);
				CPedPopulation::AddPedToPopulationCount(pPed);
			}
		}

		pPed->SetIsAHighPriorityTarget( data.m_PedFlags.isPriorityTarget );

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed,                            data.m_PedFlags.neverTargetThisPed );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents,                           data.m_PedFlags.blockNonTempEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits,                                    data.m_PedFlags.noCriticalHits );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead,                         data.m_PedFlags.doesntDropWeaponsWhenDead );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar,                                  data.m_PedFlags.dontDragMeOutOfCar );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar,                         data.m_PedFlags.playersDontDragMeOutOfCar );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater,                                     data.m_PedFlags.drownsInWater );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle,                            data.m_PedFlags.drownsInSinkingVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceDieIfInjured,                                 data.m_PedFlags.forceDieIfInjured );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled,                        data.m_PedFlags.fallOutOfVehicleWhenKilled );
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming,                         data.m_PedFlags.diesInstantlyInWater );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_JustGetsPulledOutWhenElectrocuted,                 data.m_PedFlags.justGetsPulledOutWhenElectrocuted );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact,               data.m_PedFlags.dontRagdollFromPlayerImpact );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventPedFromReactingToBeingJacked,               data.m_PedFlags.preventPedFromReactingToBeingJacked );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontInfluenceWantedLevel,                          data.m_PedFlags.dontInfluenceWantedLevel );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_GetOutUndriveableVehicle,                          data.m_PedFlags.getOutUndriveableVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_MoneyHasBeenGivenByScript,                         data.m_PedFlags.moneyHasBeenGivenByScript );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ShoutToGroupOnPlayerMelee,                         data.m_PedFlags.shoutToGroupOnPlayerMelee );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger,                   data.m_PedFlags.canDrivePlayerAsRearPassenger );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepRelationshipGroupAfterCleanUp,                 data.m_PedFlags.keepRelationshipGroupAfterCleanup );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableLockonToRandomPeds,                         data.m_PedFlags.disableLockonToRandomPeds );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillNotHotwireLawEnforcementVehicle,               data.m_PedFlags.willNotHotwireLawEnforcementVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillCommandeerRatherThanJack,                      data.m_PedFlags.willCommandeerRatherThanJack );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseKinematicModeWhenStationary,                    data.m_PedFlags.useKinematicModeWhenStationary );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex,                 data.m_PedFlags.forcedToUseSpecificGroupSeatIndex );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PlayerCanJackFriendlyPlayers,                      data.m_PedFlags.playerCanJackFriendlyPlayers );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillFlyThroughWindscreen,                          data.m_PedFlags.willFlyThroughWindscreen );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_GetOutBurningVehicle,                              data.m_PedFlags.getOutBurningVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableLadderClimbing,                             data.m_PedFlags.disableLadderClimbing );
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceDirectEntry,                                  data.m_PedFlags.forceDirectEntry );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableCameraAnimations,                      data.m_PedFlags.phoneDisableCameraAnimations );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers,                        data.m_PedFlags.notAllowedToJackAnyPlayers );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_RunFromFiresAndExplosions,                         data.m_PedFlags.runFromFiresAndExplosions );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IgnoreBeingOnFire,                                 data.m_PedFlags.ignoreBeingOnFire );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning,                   data.m_PedFlags.disableWantedHelicopterSpawning );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IgnoreNetSessionFriendlyFireCheckForAllowDamage,   data.m_PedFlags.ignoreNetSessionFriendlyFireCheckForAllowDamage );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CheckLockedBeforeWarp,								data.m_PedFlags.checklockedbeforewarp);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontShuffleInVehicleToMakeRoom,					data.m_PedFlags.dontShuffleInVehicleToMakeRoom);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableForcedEntryForOpenVehiclesFromTryLockedDoor, data.m_PedFlags.disableForcedEntryForOpenVehicles);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_GiveWeaponOnGetup,									data.m_PedFlags.giveWeaponOnGetup); 
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontHitVehicleWithProjectiles,						data.m_PedFlags.dontHitVehicleWithProjectiles);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontLeaveCombatIfTargetPlayerIsAttackedByPolice,   data.m_PedFlags.dontLeaveCombatIfTargetPlayerIsAttackedByPolice);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseTargetPerceptionForCreatingAimedAtEvents,       data.m_PedFlags.useTargetPerceptionForCreatingAimedAtEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableHomingMissileLockon,                        data.m_PedFlags.disableHomingMissileLockon );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceIgnoreMaxMeleeActiveSupportCombatants,        data.m_PedFlags.forceIgnoreMaxMeleeActiveSupportCombatants );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_StayInDefensiveAreaWhenInVehicle,                  data.m_PedFlags.stayInDefensiveAreaWhenInVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontShoutTargetPosition,                           data.m_PedFlags.dontShoutTargetPosition );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventVehExitDueToInvalidWeapon,                  data.m_PedFlags.preventExitVehicleDueToInvalidWeapon );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableHelmetArmor,                                data.m_PedFlags.disableHelmetArmor );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly,                          data.m_PedFlags.upperBodyDamageAnimsOnly );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ListensToSoundEvents,                              data.m_PedFlags.listenToSoundEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK,                                     data.m_PedFlags.openDoorArmIK );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontBlipCop,                                       data.m_PedFlags.dontBlipCop );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceSkinCharacterCloth,                           data.m_PedFlags.forceSkinCharacterCloth );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations,                     data.m_PedFlags.phoneDisableTalkingAnimations );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_LowerPriorityOfWarpSeats,                          data.m_PedFlags.lowerPriorityOfWarpSeats );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontBehaveLikeLaw,                                 data.m_PedFlags.dontBehaveLikeLaw );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PhoneDisableTextingAnimations,                     data.m_PedFlags.phoneDisableTextingAnimations );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact,               data.m_PedFlags.dontActivateRagdollFromBulletImpact );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CheckLoSForSoundEvents,                            data.m_PedFlags.checkLOSForSoundEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisablePanicInVehicle,                             data.m_PedFlags.disablePanicInVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceRagdollUponDeath,                             data.m_PedFlags.forceRagdollUponDeath );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents,                     data.m_PedFlags.pedIgnoresAnimInterruptEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontBlip,                                          data.m_PedFlags.dontBlip );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat,                   data.m_PedFlags.preventAutoShuffleToDriverSeat );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanBeAgitated,                                     data.m_PedFlags.canBeAgitated );
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry,           data.m_PedFlags.AIDriverAllowFriendlyPassengerSeatEntry );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OnlyWritheFromWeaponDamage,                        data.m_PedFlags.onlyWritheFromWeaponDamage);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ShouldChargeNow,                                   data.m_PedFlags.shouldChargeNow );
        pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableJumpingFromVehiclesAfterLeader,              data.m_PedFlags.disableJumpingFromVehiclesAfterLeader);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats,                    data.m_PedFlags.preventUsingLowerPrioritySeats );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventDraggedOutOfCarThreatResponse,              data.m_PedFlags.preventDraggedOutOfCarThreatResponse );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw,                     data.m_PedFlags.canAttackNonWantedPlayerAsLaw);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableMelee,                                      data.m_PedFlags.disableMelee );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventAutoShuffleToTurretSeat,                    data.m_PedFlags.preventAutoShuffleToTurretSeat );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableEventInteriorStatusCheck,                   data.m_PedFlags.disableEventInteriorStatusCheck );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen,                      data.m_PedFlags.onlyUpdateTargetWantedIfSeen );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat,                    data.m_PedFlags.treatDislikeAsHateWhenInCombat );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TreatNonFriendlyAsHateWhenInCombat,                data.m_PedFlags.treatNonFriendlyAsHateWhenInCombat );
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets,             data.m_PedFlags.preventReactingToCloneBullets );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes,		            data.m_PedFlags.disableAutoEquipHelmetsInBikes );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInAircraft,		            data.m_PedFlags.disableAutoEquipHelmetsInAircraft );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact,				            data.m_PedFlags.stopWeaponFiringOnImpact );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired,		            data.m_PedFlags.keepWeaponHolsteredUnlessFired );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableExplosionReactions,				            data.m_PedFlags.disableExplosionReactions );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions,		            data.m_PedFlags.dontActivateRagdollFromExplosions );
		pPed->CopyVehicleEntryConfigData(data.m_PedVehicleEntryScriptConfig);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AvoidTearGas,							            data.m_PedFlags.avoidTearGas );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableHornAudioWhenDead,				            data.m_PedFlags.disableHornAudioWhenDead );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IgnoredByAutoOpenDoors,				            data.m_PedFlags.ignoredByAutoOpenDoors );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown,           data.m_PedFlags.activateRagdollWhenVehicleUpsideDown );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowNearbyCoverUsage,					            data.m_PedFlags.allowNearbyCoverUsage );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CowerInsteadOfFlee,					            data.m_PedFlags.cowerInsteadOfFlee );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured,			            data.m_PedFlags.disableGoToWritheWhenInjured );
    	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockDroppingHealthSnacksOnDeath,		            data.m_PedFlags.blockDroppingHealthSnacksOnDeath );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes,		            data.m_PedFlags.willTakeDamageWhenVehicleCrashes );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontRespondToRandomPedsDamage,			            data.m_PedFlags.dontRespondToRandomPedsDamage );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BroadcastRepondedToThreatWhenGoingToPointShooting, data.m_PedFlags.broadcastRepondedToThreatWhenGoingToPointShooting );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet,						            data.m_PedFlags.dontTakeOffHelmet );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates,	data.m_PedFlags.allowContinuousThreatResponseWantedLevelUpdates );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepTargetLossResponseOnCleanup,		            data.m_PedFlags.keepTargetLossResponseOnCleanup );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcedToStayInCover,					            data.m_PedFlags.forcedToStayInCover );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockPedFromTurningInCover,			            data.m_PedFlags.blockPedFromTurningInCover );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TeleportToLeaderVehicle,				            data.m_PedFlags.teleportToLeaderVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontLeaveVehicleIfLeaderNotInVehicle,	            data.m_PedFlags.dontLeaveVehicleIfLeaderNotInVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedToFaceLeftInCover,				            data.m_PedFlags.forcePedToFaceLeftInCover );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedToFaceRightInCover,			            data.m_PedFlags.forcePedToFaceRightInCover );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle,	    data.m_PedFlags.useNormalExplosionDamageWhenBlownUpInVehicle );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontClearLocalPassengersWantedLevel,	            data.m_PedFlags.dontClearLocalPassengersWantedLevel );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseGoToPointForScenarioNavigation,		            data.m_PedFlags.useGoToPointForScenarioNavigation );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasBareFeet,							            data.m_PedFlags.hasBareFeet );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockAutoSwapOnWeaponPickups,                      data.m_PedFlags.blockAutoSwapOnWeaponPickups );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ThisPedIsATargetPriorityForAI,                     data.m_PedFlags.isPriorityTargetForAI );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet,								            data.m_PedFlags.hasHelmet );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,				            data.m_PedFlags.isSwitchingHelmetVisor );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceHelmetVisorSwitch,				            data.m_PedFlags.forceHelmetVisorSwitch );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsPerformingVehicleMelee,				            data.m_PedFlags.isPerformingVehMelee );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseOverrideFootstepPtFx,				            data.m_PedFlags.useOverrideFootstepPtFx );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontAttackPlayerWithoutWantedLevel,	            data.m_PedFlags.dontAttackPlayerWithoutWantedLevel );
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableShockingEvents,				                data.m_PedFlags.disableShockingEvents );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage,				data.m_PedFlags.treatAsFriendlyForTargetingAndDamage );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowBikeAlternateAnimations,						data.m_PedFlags.allowBikeAlternateAnimations );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations,					data.m_PedFlags.useLockpickVehicleEntryAnimations );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting,					data.m_PedFlags.ignoreInteriorCheckForSprinting);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact,				data.m_PedFlags.dontActivateRagdollFromVehicleImpact);
        pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_All,				                data.m_PedFlags.avoidanceIgnoreAll);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedsJackingMeDontGetIn,				            data.m_PedFlags.pedsJackingMeDontGetIn);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableTurretOrRearSeatPreference,                 data.m_PedFlags.disableTurretOrRearSeatPreference);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableHurt,										data.m_PedFlags.disableHurt);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowToBeTargetedInAVehicle,						data.m_PedFlags.allowTargettingInVehicle);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_FiresDummyRockets,					                data.m_PedFlags.firesDummyRockets);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsDecoyPed,										data.m_PedFlags.decoyPed);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasEstablishedDecoy,								data.m_PedFlags.hasEstablishedDecoy);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableInjuredCryForHelpEvents,					data.m_PedFlags.disableInjuredCryForHelpEvents);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontCryForHelpOnStun,								data.m_PedFlags.dontCryForHelpOnStun);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockDispatchedHelicoptersFromLanding,				data.m_PedFlags.blockDispatchedHelicoptersFromLanding);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontChangeTargetFromMelee,							data.m_PedFlags.dontChangeTargetFromMelee);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableBloodPoolCreation,							data.m_PedFlags.disableBloodPoolCreation);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_RagdollFloatsIndefinitely,							data.m_PedFlags.ragdollFloatsIndefinitely);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockElectricWeaponDamage,							data.m_PedFlags.blockElectricWeaponDamage);

		//PED RESET FLAGS - get a bool sync to remote store in netobjped member in member update set the ped reset flag so it occurs every frame while set
		m_PRF_disableInVehicleActions = data.m_PedFlags.PRF_disableInVehicleActions;
		m_PRF_IgnoreCombatManager = data.m_PedFlags.PRF_IgnoreCombatManager;

		if (pPed->GetSpeechAudioEntity())
		{
			pPed->GetSpeechAudioEntity()->DisablePainAudio(data.m_isPainAudioDisabled);
			pPed->GetSpeechAudioEntity()->DisableSpeakingSynced(data.m_isAmbientSpeechDisabled);
		}

		pPed->m_PedConfigFlags.SetCantBeKnockedOffVehicle( data.m_PedFlags.cantBeKnockedOffBike );

		if (data.m_PedFlags.lockAnim)
		{
			if(pPed->GetRagdollState() > RAGDOLL_STATE_ANIM)
			{
				nmEntityDebugf(pPed, "CNetObjPed::SetPedScriptGameStateData switching to animated");
				pPed->SwitchToAnimated();
			}

			pPed->SetRagdollState(RAGDOLL_STATE_ANIM_LOCKED);
		}
		else
		{
			if (pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED)
			{
				pPed->SetRagdollState(RAGDOLL_STATE_ANIM, true);
			}
		}

		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
		if (AssertVerify(pPedIntelligence))
		{
			if (pPed->GetWeaponManager() && (data.m_shootRate>0.0f) )
				pPedIntelligence->GetCombatBehaviour().SetShootRateModifier(data.m_shootRate);

			pPedIntelligence->GetCombatBehaviour().SetCombatMovement((CCombatData::Movement)data.m_combatMovement);

			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatBehaviorFlags(data.m_combatBehaviorFlags);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(static_cast<CCombatData::TargetLossResponse>(data.m_targetLossResponse));

			pPed->GetPedIntelligence()->GetFleeBehaviour().SetFleeFlags(data.m_fleeBehaviorFlags);
			pPed->GetPedIntelligence()->GetNavCapabilities().SetAllFlags(data.m_NavCapabilityFlags);

			pPedIntelligence->SetInformRespectedFriends(data.m_fMaxInformFriendDistance, data.m_uMaxNumFriendsToInform);

			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatTimeBetweenAggressiveMovesDuringVehicleChase, data.m_fTimeBetweenAggressiveMovesDuringVehicleChase);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatMaxVehicleTurretFiringRange, data.m_fMaxVehicleTurretFiringRange);

			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatMaxShootingDistance, data.m_fMaxShootingDistance);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatBurstDurationInCover, data.m_fBurstDurationInCover);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatTimeBetweenBurstsInCover, data.m_fTimeBetweenBurstsInCover);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatTimeBetweenPeeks, data.m_fTimeBetweenPeeks);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponAccuracy, data.m_fAccuracy);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatBlindFireChance, data.m_fBlindFireChance);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatStrafeWhenMovingChance, data.m_fStrafeWhenMovingChance);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponDamageModifier, data.m_fWeaponDamageModifier);
			
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatHomingRocketBreakLockAngle, data.m_fHomingRocketBreakLockAngle);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatHomingRocketBreakLockAngleClose, data.m_fHomingRocketBreakLockAngleClose);
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatHomingRocketBreakLockCloseDistance, data.m_fHomingRocketBreakLockCloseDistance);
		}

		// only set money on script peds, this is so that the money carried does not get reset when the ped becomes ambient.
		if (IsScriptObject() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_MoneyHasBeenGivenByScript ))
		{
			pPed->SetMoneyCarried( static_cast<s32>(data.m_pedCash) );
		}

		m_isTargettableByTeam   = data.m_isTargettableByTeam;

		pPed->SetMinOnGroundTimeForStunGun(data.m_minOnGroundTimeForStun);
		
		pPed->ApplyRagdollBlockingFlags(reinterpret_cast<const eRagdollBlockingFlagsBitSet&>(data.m_ragdollBlockingFlags));
		u32 inverseFlags = ~data.m_ragdollBlockingFlags;
		pPed->ClearRagdollBlockingFlags(reinterpret_cast<const eRagdollBlockingFlagsBitSet&>(inverseFlags));

		m_ammoToDrop = (u8)data.m_ammoToDrop;

        CDefensiveArea *pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea();

        if(data.m_HasDefensiveArea == false)
        {
            pDefensiveArea->Reset();
        }
        else
        {
			if (data.m_DefensiveAreaType == CDefensiveArea::AT_Sphere)
			{
				// ensure the radius isn't less than the minimum, this can happen due to quantisation error
				float radius = data.m_DefensiveAreaRadius;

				if(radius < CDefensiveArea::GetMinRadius())
				{
					radius = CDefensiveArea::GetMinRadius();
				}

				pDefensiveArea->SetAsSphere(data.m_DefensiveAreaCentre, radius);
				pDefensiveArea->SetUseCenterAsGoToPosition(data.m_UseCentreAsGotoPos);
			}
			else if (data.m_DefensiveAreaType == CDefensiveArea::AT_AngledArea)
			{
				pDefensiveArea->Set(data.m_AngledDefensiveAreaV1, data.m_AngledDefensiveAreaV2, data.m_AngledDefensiveAreaWidth, NULL, false );
				pDefensiveArea->SetUseCenterAsGoToPosition(data.m_UseCentreAsGotoPos);
				
				// If our ped is set as will advance movement then change it to defensive
				CCombatBehaviour& pedCombatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
				if(pedCombatBehaviour.GetCombatMovement() == CCombatData::CM_WillAdvance)
				{
					pedCombatBehaviour.SetCombatMovement(CCombatData::CM_Defensive);
				}				
			}           
        }

		if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != data.m_vehicleweaponindex)
		{
			if(!m_weaponObjectExists || (m_inVehicle && data.m_vehicleweaponindex!=-1) )
			{
				bool saveCanAlter = m_CanAlterPedInventoryData;
				m_CanAlterPedInventoryData = true;
				pPed->GetWeaponManager()->EquipWeapon(0, data.m_vehicleweaponindex);
				m_CanAlterPedInventoryData = saveCanAlter;		
			}
		}

		pPed->m_PedConfigFlags.SetPassengerIndexToUseInAGroup(data.m_SeatIndexToUseInAGroup);

		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTaskIfExists();
		if (pTask)
		{
			u32 uCurrentInVehicleContextHash = pTask->GetOverrideInVehicleContextHash();

			if (uCurrentInVehicleContextHash != data.m_inVehicleContextHash)
				pTask->SetInVehicleContextHash(data.m_inVehicleContextHash, true);
		}

		if(pPed->GetPedIntelligence())
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFiringPattern(data.m_FiringPatternHash);
		}
	}
}

void CNetObjPed::SetSectorPosition(const float sectorPosX, const float sectorPosY, const float sectorPosZ)
{
    m_NeedsNavmeshFixup = false;

    CNetObjPhysical::SetSectorPosition(sectorPosX, sectorPosY, sectorPosZ);
}

void CNetObjPed::SetSectorPosMapNode(const CPedSectorPosMapNode &data)
{
    SetSectorPosition(data.m_sectorPosX, data.m_sectorPosY, data.m_sectorPosZ);

    CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

    if(netBlenderPed)
    {
        netBlenderPed->SetIsRagdolling(data.m_IsRagdolling);
        netBlenderPed->UpdateStandingData(data.m_StandingOnNetworkObjectID, data.m_LocalOffset, netBlenderPed->GetLastSyncMessageTime());
    }
}

void CNetObjPed::SetSectorNavmeshPosition(const CPedSectorPosNavMeshNode& data)
{
    SetSectorPosition(data.m_sectorPosX, data.m_sectorPosY, data.m_sectorPosZ);

    Vector3 newPos = m_LastReceivedMatrix.d;

    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    if(ped)
    {
        if(FixupNavmeshPosition(newPos))
        {
            SetPosition(newPos);
        }
        else
        {
            m_NeedsNavmeshFixup = true;
        }
    }

    CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

    if(netBlenderPed)
    {
        netBlenderPed->UpdateStandingData(NETWORK_INVALID_OBJECT_ID, VEC3_ZERO, netBlenderPed->GetLastSyncMessageTime());
    }
}

void CNetObjPed::SetPedHeadings(const CPedOrientationDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		gnetAssert(data.m_currentHeading >= -TWO_PI && data.m_currentHeading <= TWO_PI);
		gnetAssert(data.m_desiredHeading >= -TWO_PI && data.m_desiredHeading <= TWO_PI);

		CNetBlenderPed* pPedBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

		bool bHeadingBlenderOverridden = NetworkBlenderIsOverridden() || NetworkHeadingBlenderIsOverridden();

		if (pPedBlender)
		{
			if(!CanBlend() || !pPedBlender->IsBlendingOn())
			{
				pPedBlender->UpdateHeading(data.m_currentHeading, pPedBlender->GetLastSyncMessageTime());

				if (!bHeadingBlenderOverridden && !pPed->GetUsingRagdoll() && !pPed->GetIsAttached())
				{
					pPed->SetHeading(data.m_currentHeading);
				}
			}
			else
			{
				pPedBlender->UpdateHeading(data.m_currentHeading, pPedBlender->GetLastSyncMessageTime());
			}
		}
		else if(!pPed->GetUsingRagdoll() && !pPed->GetIsAttached())
		{
			pPed->SetHeading(data.m_currentHeading);
		}

        m_DesiredHeading = data.m_desiredHeading;

		if (!bHeadingBlenderOverridden && !pPed->GetUsingRagdoll())
		{
			pPed->SetDesiredHeading(m_DesiredHeading);
		}
	}
}

void CNetObjPed::SetWeaponData(u32 weapon, bool weaponObjectExists, bool weaponObjectVisible, const u8 tintIndex, const bool flashLightOn, const bool hasAmmo, const bool attachLeft, bool forceUpdate)
{
	weaponitemDebugf3("CNetObjPed::SetWeaponData weapon[%u] weaponObjectExists[%d] weaponObjectVisible[%d] tintIndex[%d] flashLightOn[%d] hasAmmo[%d] attachLeft[%d] forceUpdate[%d]",weapon,weaponObjectExists,weaponObjectVisible,tintIndex,flashLightOn,hasAmmo,attachLeft,forceUpdate);

	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		m_weaponObjectExists = weaponObjectExists;
		m_weaponObjectVisible = weaponObjectVisible;

        m_weaponObjectTintIndex = tintIndex;
		weaponitemDebugf3("CNetObjPed::SetWeaponData m_weaponObjectTintIndex[%u]",m_weaponObjectTintIndex);

		m_weaponObjectFlashLightOn = flashLightOn;

		m_weaponObjectHasAmmo = hasAmmo;

		m_weaponObjectAttachLeft = attachLeft;

		UpdateWeaponSlots(static_cast<int>(weapon), forceUpdate);
		UpdateWeaponStatusFromPed();
	}
}

void CNetObjPed::SetGadgetData(const u32 equippedGadgets[MAX_SYNCED_GADGETS], const u32 numGadgets)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// ensure the ped doesn't have more currently equipped gadgets than the network game syncs
		if(AssertVerify(numGadgets <= MAX_SYNCED_GADGETS))
		{
			if(pPed->GetWeaponManager())
			{
				pPed->GetWeaponManager()->DestroyEquippedGadgets();

				for(u32 index = 0; index < numGadgets; index++)
				{
					pPed->GetWeaponManager()->EquipWeapon(equippedGadgets[index], -1, true);
				}
			}
		}

		// we need to update the blender data here as we have a separate
		
	}
}

void CNetObjPed::SetWeaponComponentData(u32 weapon, const u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS], const u8 weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS], const u32 numWeaponComponents)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	const CPedWeaponManager *pPedWeaponMgr = pPed ? pPed->GetWeaponManager() : NULL;

	if (pPed && pPedWeaponMgr && (weapon !=0) )
	{
		if(AssertVerify(numWeaponComponents <= MAX_SYNCED_WEAPON_COMPONENTS))
		{
			CPedInventory* inventory	= pPed->GetInventory();
			if (inventory)
			{
				CWeaponItem* weaponItem = inventory->GetWeaponRepository().GetItem(weapon);
				if (!weaponItem)
				{
					// flush the inventory for remote players as we do not care about the rest of the contents, and the inventory item pool can overflow if all 32 players have full inventories
					if (pPed->IsAPlayerPed() && !pPed->IsLocalPlayer())
					{
						inventory->RemoveAllWeaponsExcept(pPed->GetDefaultUnarmedWeaponHash());
					}

					weaponItem = inventory->GetWeaponRepository().AddItem(weapon, pPed);
				}

				if (weaponItem)
				{
					weaponItem->RemoveAllComponents();

					for(s32 index=0; index< numWeaponComponents; index++)
					{
						weaponitemDebugf3("CNetObjPed::SetWeaponComponentData weapon[%u] weaponComponents[%d][%u] invoke AddComponent",weapon, index, weaponComponents[index]);
						pPed->GetInventory()->GetWeaponRepository().AddComponent(weapon, weaponComponents[index]);
					}

					for(s32 index=0; index< numWeaponComponents; index++)
					{
						weaponitemDebugf3("CNetObjPed::SetWeaponComponentData weapon[%u] weaponComponents[%d][%u] invoke SetComponentTintIndex: %d",weapon, index, weaponComponents[index], weaponComponentsTint[index]);
						weaponItem->SetComponentTintIndex(index, weaponComponentsTint[index]);
					}
				}
			}
		}
	}
}

const u32 CNetObjPed::GetWeaponComponentsCount(u32 weapon) const
{
	CPed *pPed = GetPed();
	if (pPed && (weapon != 0) )
	{
		CPedInventory* inventory = pPed->GetInventory();
		if(inventory)
		{
			CWeaponItem* weaponItem = inventory->GetWeaponRepository().GetItem(weapon);
			if (weaponItem && weaponItem->GetComponents())
				return weaponItem->GetComponents()->GetCount();
		}
	}

	return 0;
}

void CNetObjPed::SetVehicleData(const ObjectId vehicleID, u32 vehicleSeat)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    m_inVehicle                          = (vehicleID != NETWORK_INVALID_OBJECT_ID);
    m_targetVehicleId                    = vehicleID;
	m_myVehicleId						 = vehicleID;
    m_targetVehicleSeat                  = m_inVehicle ? static_cast<s32>(vehicleSeat-1) : -1;

    if(!m_inVehicle && pPed && pPed->GetIsInVehicle())
    {
        m_UpdateBlenderPosAfterVehicleUpdate = true;
    }
}

void CNetObjPed::SetMountData(const ObjectId mountID)
{
	VALIDATE_GAME_OBJECT(GetPed());

	m_onMount       = (mountID != NETWORK_INVALID_OBJECT_ID);
	m_targetMountId = mountID;
}

void CNetObjPed::SetGroupLeader(const ObjectId groupLeaderID)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    // players can't be members of other groups and always remain leaders of their own groups
    if (pPed && !pPed->IsAPlayerPed())
    {
        if (groupLeaderID != NETWORK_INVALID_OBJECT_ID)
        {
            netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(groupLeaderID);
            CPed* pLeader = NetworkUtils::GetPedFromNetworkObject(pObj);

            if (AssertVerify(pLeader))
            {
                if (pPed->GetPedsGroup() && pPed->GetPedsGroup()->GetGroupMembership()->GetLeader() != pLeader)
                {
                    pPed->GetPedsGroup()->GetGroupMembership()->RemoveMember(pPed);
                }

                if (!pPed->GetPedsGroup())
                {
                    u32 i=0;

                    for (i=0; i<CPedGroups::MAX_NUM_GROUPS; i++)
                    {
                        if (CPedGroups::ms_groups[i].IsActive() &&
                            CPedGroups::ms_groups[i].GetGroupMembership()->GetLeader() == pLeader)
                        {
                            CPedGroups::ms_groups[i].GetGroupMembership()->AddFollower(pPed);
                            break;
                        }
                    }

                    // we assume the group must exist at the moment (trying to avoid storing the group leader in CNetObjPed!)
                    gnetAssert(i < CPedGroups::MAX_NUM_GROUPS);
                }
            }
        }
        else if (pPed->GetPedsGroup())
        {
            pPed->GetPedsGroup()->GetGroupMembership()->RemoveMember(pPed);
        }
    }
}

void CNetObjPed::SetPedHealthData(const CPedHealthDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// Update all damage trackers and trigger damage events.
		UpdateDamageTrackers(data);

		//Setup new endurance
		m_MaxEnduranceSetByScript = data.m_maxEnduranceSetByScript;
		if (m_MaxEnduranceSetByScript)
		{
			pPed->SetMaxEndurance((float)data.m_scriptMaxEndurance, true);
		}

		if (data.m_hasMaxEndurance)
		{
			pPed->SetEndurance(pPed->GetMaxEndurance(), true);
		}
		else
		{
			pPed->SetEndurance((float)data.m_endurance, true);
		}

		//Setup new armour
		if (data.m_hasDefaultArmour)
		{
			pPed->SetArmour(0.0f);
		}
		else
		{
			pPed->SetArmour((float)data.m_armour);
		}

		if (data.m_maxHealthSetByScript)
		{
			pPed->SetMaxHealth((float)data.m_scriptMaxHealth);
			m_MaxHealthSetByScript = true;
		}

		//Setup new health
		if (data.m_hasMaxHealth)
		{
			pPed->SetHealth(pPed->GetMaxHealth(), true);
			SetLastDamageEventPending( true );
		}
		else
		{
			bool canResetPendingLast = (pPed->IsFatallyInjured() || pPed->GetIsDeadOrDying() || pPed->GetHealth() <= 0.0f) && !GetLastDamageEventPending();
			//Hack for B*2675422 - Flag m_pendingLastDamageEvent was not being reset back to true because we were losing the 1st 
			//  damage event with data.m_hasMaxHealth set to TRUE. Because in that game mode players lose health constantly.
			if (((float)data.m_health) > (pPed->GetMaxHealth()/2.0f) && canResetPendingLast)
			{
				gnetDebug1("SetLastDamageEventPending 'TRUE' - '%s' - for B*2675422 - GetHealth='%f', data.m_health='%u', MaxHealth='%f'."
							,GetLogName()
							,pPed->GetHealth()
							,data.m_health
							,pPed->GetMaxHealth()/2.0f);

				SetLastDamageEventPending( true );
			}

			if (CNetwork::IsLastDamageEventPendingOverrideEnabled() && ms_playerMaxHealthPendingLastDamageEventOverride && pPed->IsPlayer() && canResetPendingLast)
			{
				SetLastDamageEventPending(true);
			}

			pPed->SetHealth((float)data.m_health, true);

			if (GetLastDamageEventPending() && pPed->IsFatallyInjured())
			{
				gnetDebug1("SetLastDamageEventPending 'FALSE' - '%s' - SetHealth='%u'.", GetLogName(), data.m_health);
				SetLastDamageEventPending( false );
			}

			if (data.m_killedWithHeadShot)
			{
#if !__FINAL
				if(!pPed->IsFatallyInjured())
				{
					DumpTaskInformation("CNetObjPed::SetPedHealthData()");
				}
#endif // !__FINAL

				gnetAssert(pPed->IsFatallyInjured());
				pPed->SetWeaponDamageComponent(pPed->GetRagdollComponentFromBoneTag(BONETAG_HEAD));

				//Register the kill with the headshot tracker if the ped was killed by headshot...
				NetworkInterface::RegisterHeadShotWithNetworkTracker(pPed);
			}
			else if (data.m_weaponDamageComponent > -1)
			{
				pPed->SetWeaponDamageComponent(data.m_weaponDamageComponent);
			}
		}

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, data.m_hurtStarted);
		pPed->SetHurtEndTime(data.m_hurtEndTime);

		//Register the damage with the kill trackers (tests health internally)....
		RegisterKillWithNetworkTracker();
	}
}

void  CNetObjPed::UpdateDamageTracker(const NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		//Last damage has been set
		if (damageInfo.m_KillsVictim)
		{
			gnetAssertf(GetLastDamageEventPending(), "Ped '%s' already has a done the last damage event.", GetLogName());
			SetLastDamageEventPending( false );
		}

		if (!IsClone())
		{
			m_killedWithHeadShot = damageInfo.m_HeadShoot;
			m_killedWithMeleeDamage = damageInfo.m_WithMeleeWeapon;
		}
	}

	CNetObjPhysical::UpdateDamageTracker(damageInfo, isResponsibleForCollision);
}

void  CNetObjPed::SetLastDamageEventPending(const bool value) 
{
	m_pendingLastDamageEvent = value;

#if !__FINAL
	if (!m_pendingLastDamageEvent)
	{
		gnetDebug1("VICTIM KILLED %s, on frame %d.", GetLogName(), fwTimer::GetFrameCount());
		//sysStack::PrintStackTrace( );
	}
#endif // !__FINAL
}


bool CNetObjPed::CanResetAlpha() const
{
	CPed *pPed = GetPed();

	if (pPed && pPed->GetMyVehicle() && pPed->GetIsInVehicle())
	{
		CNetObjVehicle* pVehicleObj = static_cast<CNetObjVehicle*>(pPed->GetMyVehicle()->GetNetworkObject());

		if (pVehicleObj && (pVehicleObj->IsAlphaActive() || pVehicleObj->GetHideWhenInvisible()))
		{
			return false;
		}
	}

	return CNetObjPhysical::CanResetAlpha();
}

void CNetObjPed::SetHideWhenInvisible(bool bSet)
{
	CPed *pPed = GetPed();

	m_bHideWhenInvisible = bSet;

	if (pPed && !pPed->GetIsInVehicle())
	{
		CNetObjPhysical::SetHideWhenInvisible(bSet);
	}
	else
	{
		m_bHideWhenInvisible = bSet;

		if (!bSet)
		{
			SetIsVisible(IsClone()?m_LastReceivedVisibilityFlag:true, "SetHideWhenInvisible");
		}
	}
}

void CNetObjPed::UpdateDamageTrackers(const CPedHealthDataNode& data)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed && gnetVerify(IsClone()))
	{
		pPed->ResetWeaponDamageInfo();

		bool triggerDamageEvent = false;

		CEntity* damager = NULL;
		netObject* pNetObj = 0;
		float damageDealt  = 0.0f;
		float armourLost   = 0.0f;
		float enduranceLost = 0.0f;

		//Setup damage trackers
		if (data.m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID)
		{
			pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_weaponDamageEntity);
			if (pNetObj && pNetObj->GetEntity())
			{
				damager = pNetObj->GetEntity();

				//If WEAPONTYPE_SCRIPT_HEALTH_CHANGE then skip this.
				if (data.m_weaponDamageHash != WEAPONTYPE_SCRIPT_HEALTH_CHANGE)
				{
					pPed->SetWeaponDamageInfo(pNetObj->GetEntity(), data.m_weaponDamageHash, fwTimer::GetTimeInMilliseconds());
				}
			}
		}

		//We have received a RECEIVED_CLONE_CREATE - so this clone can be receiving a damage event.
		const bool isCloneCreateUpdate = GetSyncTree() && GetSyncTree()->GetCreateNode() ? GetSyncTree()->GetCreateNode()->GetWasUpdated() : false;
		if (!pPed->IsFatallyInjured() && !isCloneCreateUpdate)
		{
			damageDealt = pPed->GetHealth() - (float)data.m_health;
			if (data.m_hasMaxHealth || damageDealt < 0.0f)
			{
				damageDealt = 0.0f;
			}

			armourLost = pPed->GetArmour() - (float)data.m_armour;
			if (armourLost < 0.0f)
			{
				armourLost = 0.0f;
			}

			enduranceLost = pPed->GetEndurance() - (float)data.m_endurance;
			if (data.m_hasMaxEndurance || enduranceLost < 0.0f)
			{
				enduranceLost = 0.0f;
			}

			//Trigger damage events
			if (damageDealt+armourLost+enduranceLost > 0.0f)
			{
                CTaskSynchronizedScene *syncedSceneTask = static_cast<CTaskSynchronizedScene *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));

                if(syncedSceneTask && syncedSceneTask->IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
                {
                    syncedSceneTask->ExitSceneNextUpdate();
                }

				triggerDamageEvent = true;
			}
		}

		if ((triggerDamageEvent && GetLastDamageEventPending()) || (GetLastDamageEventPending() && pPed->IsFatallyInjured()))
		{
			//We have a no damager but this still needs to be recorded.
			if (!pNetObj)
			{
				pPed->SetWeaponDamageInfo(NULL, data.m_weaponDamageHash, fwTimer::GetTimeInMilliseconds());
			}

			//Update some stats.
			if(!pPed->IsPlayer() && damager && damager->GetIsTypeVehicle())
			{
				if (WEAPONTYPE_HIT_BY_WATER_CANNON != data.m_weaponDamageHash && ((CVehicle*)damager)->GetDriver() && ((CVehicle*)damager)->GetDriver()->IsLocalPlayer())
				{
					CStatsMgr::UpdateStatsWhenPedRunDown(pPed->GetRandomSeed(), (CVehicle*)damager);
				}
			}

			NetworkDamageInfo damageInfo(pNetObj ? pNetObj->GetEntity() : 0
											,damageDealt
											,armourLost
											,enduranceLost
											,((float)data.m_endurance <= 0.0f && enduranceLost > 0.0f)
											,data.m_weaponDamageHash
											,data.m_weaponDamageComponent
											,data.m_killedWithHeadShot
											,(data.m_health < pPed->GetDyingHealthThreshold() && !data.m_hasMaxHealth) || data.m_killedWithHeadShot
											,data.m_killedWithMeleeDamage);

			UpdateDamageTracker(damageInfo);
		}
		else if (triggerDamageEvent && !GetLastDamageEventPending())
		{
			gnetWarning("Wil want to trigger damage event for - This PED '%s' has already been killed.", GetLogName());
		}
	}
}

bool CNetObjPed::FixupNavmeshPosition(Vector3 &position)
{
    bool succeeded = false;

#if __BANK
    Vector3 originalPosition = position;
#endif // __BANK

    CPed *ped = GetPed();

    position.z += CPedSectorPosNavMeshNode::GetQuantisationError();

    const float rescanProbeLength = CNavMeshTrackedObject::DEFAULT_RESCAN_PROBE_LENGTH + CPedSectorPosNavMeshNode::GetQuantisationError();

    CPathServer::UpdateTrackedObject(ped->GetNavMeshTracker(), position, rescanProbeLength);

#if __BANK
    if(ped->GetNavMeshTracker().PerformedRescanLastUpdate())
    {
        NetworkDebug::AddPedNavmeshPosSyncRescan();
    }
#endif // __BANK

    const float groundToRootOffset = ped->GetCapsuleInfo() ? ped->GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;

    // if this fails we need to ground probe to find the position (this should only happen rarely)
    if(!ped->GetNavMeshTracker().GetIsValid())
    {
        position.z -= groundToRootOffset;

        const float    probeStartHeight  = position.z + 0.5f;
        const float    probeEndHeight    = position.z - CPedSectorPosNavMeshNode::GetQuantisationError() - 0.5f;
        const unsigned NUM_INTERSECTIONS = 3;

        WorldProbe::CShapeTestProbeDesc probeDesc;
        WorldProbe::CShapeTestHitPoint probeIsects[NUM_INTERSECTIONS];
        WorldProbe::CShapeTestResults probeResult(probeIsects, NUM_INTERSECTIONS);
        probeDesc.SetStartAndEnd(Vector3(position.x, position.y, probeStartHeight), Vector3(position.x, position.y, probeEndHeight));
        probeDesc.SetResultsStructure(&probeResult);
        probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
        probeDesc.SetContext(WorldProbe::ENetwork);

        if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
        {
            gnetDebug2("Frame %d: Performing probe for %s", fwTimer::GetFrameCount(), GetLogName());

            float groundPos = probeResult[0].GetHitPosition().z;

            if(probeResult.GetNumHits() > 1)
            {
                float bestDiff = fabs(groundPos - position.z);

                for(unsigned index = 1; index < probeResult.GetNumHits(); index++)
                {
                    float newZ    = probeResult[index].GetHitPosition().z;
                    float newDiff = fabs(newZ - position.z);

                    if(newDiff < bestDiff)
                    {
                        bestDiff  = newDiff;
                        groundPos = newZ;
                    }
                }
            }

            position.z = groundPos + groundToRootOffset;

            BANK_ONLY(NetworkDebug::AddPedNavmeshPosGroundProbe());

            succeeded = true;
        }
#if __BANK
        else
        {
            // probe failed! output information about what we tried to do!
            gnetDebug2("Failed to fixup navmesh position for %s!", GetLogName());
            gnetDebug2("Has Loaded About Position: %s", g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(originalPosition), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER) ? "true" : "false");
            gnetDebug2("Position to fixup: %.2f, %.2f, %.2f", originalPosition.x, originalPosition.y, originalPosition.z);
            gnetDebug2("Probe start height: %.2f", probeStartHeight);
            gnetDebug2("Probe end height: %.2f", probeEndHeight);
        }
#endif // __BANK
    }
    else
    {
        position.z = ped->GetNavMeshTracker().GetLastNavMeshIntersection().z + groundToRootOffset;
        succeeded  = true;
    }

    CPathServer::UpdateTrackedObject(ped->GetNavMeshTracker(), VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()));

    return succeeded;
}

void CNetObjPed::ForceSendOfCutsceneState()
{
	if (!IsClone() && GetSyncData())
	{
		CPedSyncTree *syncTree = SafeCast(CPedSyncTree, GetSyncTree());

		if(syncTree)
		{
			syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPhysicalScriptGameStateNode());
		}
	}
}

void CNetObjPed::SetIsInCutsceneRemotely(bool bSet)
{
	if (bSet && bSet != m_bInCutsceneRemotely)
	{
		CPed *pPed = GetPed();
		
		if (pPed && !pPed->GetIsInVehicle())
		{
			// flush all clone tasks on the ped if it is now in a cutscene
			pPed->GetPedIntelligence()->FlushImmediately(false);
			pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());

			pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
			pPed->SetDesiredHeading(0.0f);
		}
	}

	CNetObjPhysical::SetIsInCutsceneRemotely(bSet);
}

void CNetObjPed::UpdateForceSendRequests()
{
    CPedSyncTreeBase *syncTree = SafeCast(CPedSyncTreeBase, GetSyncTree());

    if(syncTree)
    {
        u8 frameCount = static_cast<u8>(fwTimer::GetSystemFrameCount());
        
        if(frameCount != m_FrameForceSendRequested)
        {
            if(m_ForceSendRagdollState)
            {
                m_ForceSendRagdollState = false;
				if(HasSyncData())
				{
					syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPedMovementGroupDataNode());
				}
			}
             
			if(m_ForceSendTaskState)
            {
	            m_ForceSendTaskState = false;

				if(HasSyncData())
				{
					syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPedTaskTreeNode());
				}
            }

            if(m_ForceSendGameState)
            {
                m_ForceSendGameState = false;

				if(HasSyncData())
				{
					syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPedGameStateNode());
				}
            }
        }
    }
}

void CNetObjPed::CheckForPedStopping()
{
    CPed *pPed = GetPed();
    VALIDATE_GAME_OBJECT(pPed);

    bool zeroMBRThisFrame = true;

    if(pPed && !pPed->GetIsInVehicle() && !pPed->GetIsAttached())
    {
        // force send the movement and position nodes when peds are stopping
        float desiredMBRX = 0.0f;
        float desiredMBRY = 0.0f;
        pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio(desiredMBRX, desiredMBRY);

        zeroMBRThisFrame = fabs(desiredMBRX) < 0.01f && fabs(desiredMBRY) < 0.01f;

        if(!m_ZeroMBRLastFrame && zeroMBRThisFrame)
        {
            if(!IsClone())
            {
                CPedSyncTreeBase *pedSyncTree = SafeCast(CPedSyncTreeBase, GetSyncTree());

                if(pedSyncTree && HasSyncData())
                {
                    pedSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *pedSyncTree->GetPedMovementDataNode());
                    pedSyncTree->ForceSendOfPositionData(*this, GetActivationFlags());
                }
            }
        }
    }

    m_ZeroMBRLastFrame = zeroMBRThisFrame;
}

void CNetObjPed::SetAttachmentData(const CPedAttachDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	m_pendingAttachmentObjectID = NETWORK_INVALID_OBJECT_ID;

	if (data.m_attachObjectID != NETWORK_INVALID_OBJECT_ID)
	{
		bool bUseReceivedData = true;

		if (data.m_attachedToGround && pPed && pPed->GetGroundPhysical())
		{
			CPhysical* pTempGroundPhysical = NULL;
			Vec3V vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal;
			int iGroundPhysicalComponent;
			pPed->GetGroundData(pTempGroundPhysical, vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal, iGroundPhysicalComponent);

			Vector3 vector3GroundOffset = VEC3V_TO_VECTOR3(vGroundOffset);
			Vector3 vDelta = vector3GroundOffset - data.m_attachOffset;
			float fDelta = vDelta.Mag();

			//If we are within tolerance then allow the current ped position to be used without popping the ped to the new location
			if (fDelta < 1.0f)
			{
				//Calculate the ground heading.
				Vec3V vForward = pPed->GetGroundPhysical()->GetTransform().GetForward();
				float fGroundHeading = rage::Atan2f(-vForward.GetXf(), vForward.GetYf());

				//Calculate the heading.
				float fHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() - fGroundHeading);

				m_pendingAttachmentObjectID = data.m_attachObjectID;
				m_pendingAttachOffset		= vector3GroundOffset;
				m_pendingAttachQuat			= data.m_attachQuat;
				m_pendingOtherAttachBone	= data.m_attachBone;
				m_pendingMyAttachBone		= -1;
				m_pendingAttachFlags		= data.m_attachFlags;
				m_pendingAttachHeading		= fHeading;
				m_pendingAttachHeadingLimit	= data.m_attachHeadingLimit;

				//Overridden above...
				bUseReceivedData = false;
			}
		}
		
		if (bUseReceivedData)
		{
			m_pendingAttachmentObjectID = data.m_attachObjectID;
			m_pendingAttachOffset       = data.m_attachOffset;
			m_pendingAttachQuat         = data.m_attachQuat;
			m_pendingOtherAttachBone    = data.m_attachBone;
			m_pendingMyAttachBone       = -1;
			m_pendingAttachFlags        = data.m_attachFlags;
			m_pendingAttachHeading      = data.m_attachHeading;
			m_pendingAttachHeadingLimit = data.m_attachHeadingLimit;
		}
	}

	ProcessPendingAttachment();
}

void CNetObjPed::SetMotionGroup(const CPedMovementGroupDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		m_PendingMoveBlenderType	   = data.m_moveBlendType;
		m_PendingMoveBlenderState      = data.m_moveBlendState;
		m_pendingMotionSetId           = data.m_motionSetId;
		m_pendingOverriddenWeaponSetId = data.m_overriddenWeaponSetId;
        m_pendingOverriddenStrafeSetId = data.m_overriddenStrafeSetId;
        m_IsDiving                     = data.m_moveBlendState == CTaskMotionPed::State_Diving;
		m_IsDivingToSwim			   = data.m_moveBlendState == CTaskMotionPed::State_DiveToSwim;
		m_IsSwimming				   = data.m_moveBlendState == CTaskMotionPed::State_Swimming || pPed->GetIsFPSSwimming();

		if (m_pendingMotionSetId != CLIP_SET_ID_INVALID)
		{
			m_motionSetClipSetRequestHelper.Request(m_pendingMotionSetId);
		}

		if (m_pendingOverriddenWeaponSetId != CLIP_SET_ID_INVALID)
		{
			m_overriddenWeaponSetClipSetRequestHelper.Request(m_pendingOverriddenWeaponSetId);
		}

        if (m_pendingOverriddenStrafeSetId != CLIP_SET_ID_INVALID)
        {
            m_overriddenStrafeSetClipSetRequestHelper.Request(m_pendingOverriddenStrafeSetId);
        }

		// if we're updating the strafing flag we have to let the AI update once before changing tasks 
		// as it will make a difference to TaskMotionPed...
		if( (pPed->GetMotionData()->GetIsStrafing() != data.m_isStrafing) ||
			(pPed->GetMotionData()->GetUsingStealth() != data.m_isStealthy) )
		{
			SetForceMotionTaskUpdate(true);
		}

		UpdatePendingMovementData();

        pPed->SetIsCrouching(data.m_isCrouching, -1, false, true);
        pPed->GetMotionData()->SetUsingStealth(data.m_isStealthy);
		pPed->SetIsStrafing(data.m_isStrafing);
		
		m_bUsingRagdoll = data.m_isRagdolling;

		if(pPed->GetRagdollConstraintData())
		{
			pPed->GetRagdollConstraintData()->EnableBoundAnkles(pPed, data.m_isRagdollConstraintAnkleActive);
			pPed->GetRagdollConstraintData()->EnableHandCuffs(pPed, data.m_isRagdollConstraintWristActive);
		}

		if (data.m_motionInVehiclePitch > 0.f)
		{
			if (pPed && pPed->GetIsDrivingVehicle())
			{
				CVehicle* pVehicle = pPed->GetMyVehicle();
				VehicleType eVehicleType = pVehicle->GetVehicleType();
				if ((eVehicleType == VEHICLE_TYPE_BIKE) || (eVehicleType == VEHICLE_TYPE_QUADBIKE) || (eVehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
				{
					CTaskMotionInAutomobile* pMotionInAutomobileTask = static_cast<CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
					if (pMotionInAutomobileTask)
					{
						pMotionInAutomobileTask->SetRemoteDesiredPitchAngle(data.m_motionInVehiclePitch);
					}
				}
			}
		}

		SetTennisMotionData(data.m_TennisMotionData);
	}
}

static float RemoveMBRQuantisationError(float MBR)
{
    static const float QUANTISATION_ERROR = 0.025f; // current quantisation error for MBR synced values

    if(IsClose(MBR, MOVEBLENDRATIO_WALK, QUANTISATION_ERROR))
    {
        return MOVEBLENDRATIO_WALK;
    }

	if (IsClose(MBR, MBR_RUN_BOUNDARY, QUANTISATION_ERROR))
	{
		return MBR_RUN_BOUNDARY;
	}

    if(IsClose(MBR, MOVEBLENDRATIO_RUN, QUANTISATION_ERROR))
    {
        return MOVEBLENDRATIO_RUN;
    }

	if (IsClose(MBR, MBR_SPRINT_BOUNDARY, QUANTISATION_ERROR))
	{
		return MBR_SPRINT_BOUNDARY;
	}

    if(IsClose(MBR, MOVEBLENDRATIO_SPRINT, QUANTISATION_ERROR))
    {
        return MOVEBLENDRATIO_SPRINT;
    }

    return MBR;
}

void CNetObjPed::SetPedMovementData(const CPedMovementDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
        m_DesiredMoveBlendRatioX = RemoveMBRQuantisationError(data.m_DesiredMoveBlendRatioX);
        m_DesiredMoveBlendRatioY = RemoveMBRQuantisationError(data.m_DesiredMoveBlendRatioY);
		m_DesiredMaxMoveBlendRatio = data.m_MaxMoveBlendRatio;
        m_HasStopped             = data.m_HasStopped;

		pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_DesiredMoveBlendRatioY, m_DesiredMoveBlendRatioX);
		pPed->GetMotionData()->SetScriptedMaxMoveBlendRatio(m_DesiredMaxMoveBlendRatio);

        pPed->SetDesiredPitch(data.m_DesiredPitch);
	}
}

void CNetObjPed::SetPedAppearanceData(const CPackedPedProps pedProps, const CSyncedPedVarData& variationData, const u32 phoneMode, const u8 parachuteTintIndex, const u8 parachutePackTintIndex, const fwMvClipSetId& facialClipSetId, const u32 facialIdleAnimOverrideClipHash, const u32 facialIdleAnimOverrideClipDictHash, const bool isAttachingHelmet, const bool isRemovingHelmet, const bool isWearingHelmet, const u8 helmetTextureId, const u16 helmetProp, const u16 visorUpPropId, const u16 visorDownPropId, const bool visorIsUp, const bool supportsVisor, const bool isVisorSwitching, const u8 targetVisorState, bool forceApplyAppearance)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		// Cache the prop update to a ped - sometimes a prop can take a frame to be deleted to if we try and set all three
		// props, there won't be a slot for one of them and we'll hit an assert - need to do it on subsequent frames...
		m_pendingPropUpdateData = pedProps;
		m_pendingPropUpdate		= true;
		UpdatePendingPropData();

		pedDebugf3("CNetObjPed::SetPedAppearanceData pPed[%p][%s] player[%s] isplayer[%d]",pPed,pPed->GetModelName(), GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",pPed->IsPlayer());

		variationData.ApplyToPed(*pPed, forceApplyAppearance);
		SetPhoneModeData(phoneMode);
		
		SetHelmetAttachmentData(isAttachingHelmet, isRemovingHelmet, isWearingHelmet, isVisorSwitching, targetVisorState);
		if((isAttachingHelmet || isRemovingHelmet || isWearingHelmet || isVisorSwitching) && helmetProp != 0)
		{
			// only sync component data in CPedAppearanceDataNode::Serialise if we're doing one or the other...
			SetHelmetComponentData(helmetTextureId, helmetProp, visorUpPropId, visorDownPropId, visorIsUp, supportsVisor);
		}

		pPed->GetPedIntelligence()->SetTintIndexForParachute(parachuteTintIndex);
		pPed->GetPedIntelligence()->SetTintIndexForParachutePack(parachutePackTintIndex);

		weaponitemDebugf3("CNetObjPed::SetPedAppearanceData pPed[%p][%s] player[%s] isplayer[%d] tintIndex[%d] packTintIndex[%d]",pPed,pPed->GetModelName(), GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",pPed->IsPlayer(),parachuteTintIndex,parachutePackTintIndex);

		if(pPed->GetFacialData())
		{
			if( facialClipSetId != CLIP_SET_ID_INVALID ) 
			{
				pPed->GetFacialData()->SetFacialClipSet(pPed, facialClipSetId);
			}
			else if(pPed->GetFacialData()->GetFacialClipSet() != pPed->GetFacialData()->GetDefaultFacialClipSetId(pPed))
			{
				pPed->GetFacialData()->SetFacialClipsetToDefault(pPed);
			}

			//--

			if(facialIdleAnimOverrideClipHash != 0)
			{
				pPed->GetFacialData()->SetFacialIdleAnimOverride(pPed, facialIdleAnimOverrideClipHash, facialIdleAnimOverrideClipDictHash);
			}
			else if(pPed->GetFacialData()->GetFacialIdleAnimOverrideClipNameHash() != 0)
			{
				pPed->GetFacialData()->ClearFacialIdleAnimOverride(pPed);
			}
		}

		pedDebugf3("CNetObjPed::SetPedAppearanceData--complete");
	}
}

void CNetObjPed::SetAIData(const CPedAIDataNode& data)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (pPed)
	{
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(data.m_relationshipGroup);
		if( pGroup )
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
			m_pendingRelationshipGroup = 0;
		}
		else
		{
			pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
			m_pendingRelationshipGroup = data.m_relationshipGroup; // cache the group so we can try to find it later
		}

		if (data.m_decisionMakerType==0)
			pPed->SetDefaultDecisionMaker();
		else
			pPed->GetPedIntelligence()->SetDecisionMakerId(data.m_decisionMakerType);
	}
}

void CNetObjPed::SetTaskTreeData(const CPedTaskTreeDataNode& data)
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped)
	{
		CQueriableInterface *queriableInterface = AssertVerify(ped) ? ped->GetPedIntelligence()->GetQueriableInterface() : 0;

		if(AssertVerify(queriableInterface))
		{
			// set the scripted task status (only for the first task node)
			ped->GetPedIntelligence()->GetQueriableInterface()->SetScriptedTaskStatus(data.m_scriptCommand, static_cast<u8>(data.m_taskStage));
	
			for (int i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
			{
				const IPedNodeDataAccessor::TaskSlotData &taskSlot	= data.m_taskTreeData[i];
				IPedNodeDataAccessor::TaskSlotData &pedTaskSlot		= m_taskSlotCache.m_taskSlots[i];

				// remove any existing task from this slot and queriable interface
				if(pedTaskSlot.m_taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
				{
					CTaskInfo *taskInfo = queriableInterface->GetTaskInfoForTaskType(pedTaskSlot.m_taskType, pedTaskSlot.m_taskPriority);

					if(AssertVerify(taskInfo))
					{
						queriableInterface->UnlinkTaskInfoFromNetwork(taskInfo);
						delete taskInfo;
					}
				}

				// add a new task into the slot and queriable interface if one exists
				pedTaskSlot.m_taskType = taskSlot.m_taskType;

				if(pedTaskSlot.m_taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
				{
					pedTaskSlot.m_taskPriority		= taskSlot.m_taskPriority;
					pedTaskSlot.m_taskActive		= taskSlot.m_taskActive;
					pedTaskSlot.m_taskTreeDepth		= taskSlot.m_taskTreeDepth;
					pedTaskSlot.m_taskSequenceId	= taskSlot.m_taskSequenceId;

					queriableInterface->AddTaskInformationFromNetwork(pedTaskSlot.m_taskType, pedTaskSlot.m_taskActive, pedTaskSlot.m_taskPriority, pedTaskSlot.m_taskSequenceId);
				}
			}

			queriableInterface->UpdateCachedInfo();
		}
	}
}

void CNetObjPed::SetTaskSpecificData(const CPedTaskSpecificDataNode& data)
{
	if(!AssertVerify(data.m_taskIndex < IPedNodeDataAccessor::NUM_TASK_SLOTS) || 
	   !AssertVerify(data.m_taskData.m_TaskDataSize > 0) || 
	   !AssertVerify(data.m_taskData.m_TaskData))
	{
		return;
	}

	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped)
	{
		CQueriableInterface *queriableInterface = AssertVerify(ped) ? ped->GetPedIntelligence()->GetQueriableInterface() : 0;

		if(AssertVerify(queriableInterface))
		{
			IPedNodeDataAccessor::TaskSlotData taskSlot = m_taskSlotCache.m_taskSlots[data.m_taskIndex];

			gnetAssertf(taskSlot.m_taskType == data.m_taskData.m_TaskType, "Trying to set task specific data for an incorrect task for %s (new task: %s, our task: %s)", GetLogName(), TASKCLASSINFOMGR.GetTaskName(data.m_taskData.m_TaskType), taskSlot.m_taskType == CTaskTypes::MAX_NUM_TASK_TYPES ? "None" : TASKCLASSINFOMGR.GetTaskName(taskSlot.m_taskType));

			u32 byteSizeOfTaskData = (data.m_taskData.m_TaskDataSize+7)>>3;
			datBitBuffer messageBuffer;
			messageBuffer.SetReadOnlyBits((u8*)data.m_taskData.m_TaskData, byteSizeOfTaskData<<3, 0);

			CTaskInfo* pTaskInfo = queriableInterface->GetTaskInfoForTaskType(taskSlot.m_taskType, taskSlot.m_taskPriority);

			if (gnetVerifyf(pTaskInfo, "CNetObjPed::SetTaskSpecificData: No corresponding task info"))
			{
				pTaskInfo->ReadSpecificTaskInfo(messageBuffer);
			}
		}
	}
}

void CNetObjPed::SetPedInventoryData(const CPedInventoryDataNode& data)
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(ped)
    {
		const CItemInfo* weaponInfos[MAX_WEAPONS];
		const CItemInfo* ammoInfos[MAX_AMMOS];

		u32 numItems = (data.m_numItems > MAX_WEAPONS) ? MAX_WEAPONS : data.m_numItems;
		for (int i=0; i<numItems; i++)
		{
			weaponInfos[i] = CWeaponInfoManager::GetInfoFromSlot(data.m_itemSlots[i]);
		}

		u32 numAmmos = (data.m_numAmmos > MAX_AMMOS) ? MAX_AMMOS : data.m_numAmmos;
		for (int i=0; i<numAmmos; i++)
		{
			ammoInfos[i] = CWeaponInfoManager::GetInfoFromSlot(data.m_ammoSlots[i]);
		}

        // allow ped inventory and weapon changes on this ped
        m_CanAlterPedInventoryData = true;

        // cache all currently equipped items
        u32 weapon  = 0;
		bool   exists = false;
		bool   visible = true;
        u8     tintIndex = 0;
		bool bFlashLightOn = false;
		bool bHasAmmo = true;
		bool bAttachLeft = false;
        GetWeaponData(weapon, exists, visible, tintIndex, bFlashLightOn, bHasAmmo, bAttachLeft);
        u32 equippedGadgets[MAX_SYNCED_GADGETS];
        u32 numGadgets = GetGadgetData(equippedGadgets);
		u8	weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS];
		u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS];
		u32 numWeaponComponents = GetWeaponComponentData(weaponComponents, weaponComponentsTint);

		CPedInventory *pPedInventory = ped->GetInventory();
		if(pPedInventory)
		{
			// grab current inventory and compare
			CPedInventoryDataNode currdata;
			GetPedInventoryData(currdata);

			bool bInventoryDiffers = (currdata.m_numItems != numItems);

			if (!bInventoryDiffers)
			{
				for (u32 i=0; i<numItems; i++)
				{
					bool bTintOrComponentChanged = ped->IsPlayer() && ((currdata.m_itemSlotTint[i] != data.m_itemSlotTint[i]) || (currdata.m_itemSlotNumComponents[i] != data.m_itemSlotNumComponents[i]));

					if (bTintOrComponentChanged || (currdata.m_itemSlots[i] != data.m_itemSlots[i]))
					{
						bInventoryDiffers = true;
						break;
					}
				}
			}

			if (bInventoryDiffers)
			{
#if __ASSERT
				bool bHadUnarmedWeapon = pPedInventory->GetWeapon(ped->GetDefaultUnarmedWeaponHash()) != NULL;
#endif
				// keep any props
				pPedInventory->RemoveAllWeaponsExcept(OBJECTTYPE_OBJECT, ped->GetDefaultUnarmedWeaponHash());

				// add all specified items to the inventory
				for(u32 index = 0; index < numItems; index++)
				{
					if (weaponInfos[index])
					{
						pPedInventory->AddWeapon(weaponInfos[index]->GetHash());

						CWeaponItem* pWeaponItem = pPedInventory->GetWeapon(weaponInfos[index]->GetHash());
						if (pWeaponItem)
						{
							if (data.m_itemSlotTint[index])
								pWeaponItem->SetTintIndex(data.m_itemSlotTint[index]);

							if (data.m_itemSlotNumComponents[index])
							{
								for (int i = 0; i < data.m_itemSlotNumComponents[index]; i++)
								{
									if (data.m_itemSlotComponent[index][i])
									{
										pWeaponItem->AddComponent(data.m_itemSlotComponent[index][i]);
										pWeaponItem->SetActiveComponent(i,data.m_itemSlotComponentActive[index][i]);
									}
								}
							}
						}
					}
				}
#if __ASSERT
				if (bHadUnarmedWeapon && !pPedInventory->GetWeapon(ped->GetDefaultUnarmedWeaponHash()))
				{
					gnetAssertf(0, "%s lost the unarmed weapon during migration", GetLogName()); 
				}
#endif
			}

			// grab the information about the ammo in the inventory
			pPedInventory->GetAmmoRepository().SetUsingInfiniteAmmo(data.m_allAmmoInfinite);

#if __BANK
			if (!ped->IsDead() && (ped->IsCopPed() || ped->IsSwatPed()))
			{
				if (m_NumInventoryItems != data.m_numItems || m_PedUsingInfiniteAmmo != data.m_allAmmoInfinite)
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "COP_PED_INVENTORY", GetLogName());

					if (m_NumInventoryItems != data.m_numItems)
					{
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Setting num inventory items", "%d", data.m_numItems);
					}

					if (m_PedUsingInfiniteAmmo != data.m_allAmmoInfinite)
					{
						NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Setting infinite ammo", "%d", currdata.m_allAmmoInfinite);
					}

					if (m_NumInventoryItems > 0)
					{
						gnetAssertf(data.m_numItems != 0, "Cop ped %s has lost his inventory", GetLogName());
					}

					if (m_PedUsingInfiniteAmmo)
					{
						gnetAssertf(data.m_allAmmoInfinite, "Cop ped %s has lost infinte ammo", GetLogName());
					}

					m_NumInventoryItems = data.m_numItems;
					m_PedUsingInfiniteAmmo = data.m_allAmmoInfinite;
				}
			}
#endif
			//The remote will have a different real value here as it is modified in UpdateWeaponSlots so that remotes always have infinite ammo to prevent problems firing.
			//The cached version here will be re-instated as the real value if the ped becomes local in ChangeOwner
			SetCacheUsingInfiniteAmmo(data.m_allAmmoInfinite); 

			// add all ammo to the inventory
			for(u32 index = 0; index < numAmmos; index++)
			{
				if (ammoInfos[index])
				{
					u32 ammoHash = ammoInfos[index]->GetHash();

					pPedInventory->AddAmmo(ammoHash, 0);

					if(data.m_ammoInfinite[index])
					{
						pPedInventory->GetAmmoRepository().SetUsingInfiniteAmmo(ammoHash, true);
					}
					else
					{
						pPedInventory->SetAmmo(ammoHash, data.m_ammoQuantity[index]);
					}
				}
			}

			// reequip the currently equipped items
			if (bInventoryDiffers)
			{
				if(!CacheWeaponInfo() )
				{
					m_CachedWeaponInfo.m_pending = false;
					m_CanAlterPedInventoryData = true;
					SetWeaponComponentData(weapon, weaponComponents, weaponComponentsTint, numWeaponComponents);
					SetWeaponData(weapon, exists, visible, tintIndex, bFlashLightOn, bHasAmmo, bAttachLeft);
					SetGadgetData(equippedGadgets, numGadgets);
					m_CanAlterPedInventoryData = false;
				}
				else
				{
					SetCachedWeaponInfo(weapon,
						exists,
						visible,
						tintIndex,
						weaponComponentsTint,
						weaponComponents,
						numWeaponComponents,
						equippedGadgets,
						numGadgets,
						bFlashLightOn,
						bHasAmmo,
						bAttachLeft);
				}
			}
		}

        // disallow ped inventory and weapon changes on this ped
        m_CanAlterPedInventoryData = false;
    }
}

void CNetObjPed::SetPedComponentReservations(const CPedComponentReservationDataNode& data)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	if(pPed && pPed->GetComponentReservationMgr())
	{
		CComponentReservationManager* pCRM = pPed->GetComponentReservationMgr();
		if(!pCRM)
			return;

		gnetAssert(data.m_numPedComponents == pCRM->GetNumReserveComponents());

		for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
		{
			CComponentReservation *componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);
			netObject        *pedUsingComponent    = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_componentReservations[componentIndex]);

			// if ped doesn't exist on this machine fail the sync?
			if(AssertVerify(componentReservation))
			{
				CPed *ped = NetworkUtils::GetPedFromNetworkObject(pedUsingComponent);
	#if __ASSERT
				gnetAssert(ped || (pedUsingComponent==0));
	#endif
				componentReservation->UpdatePedUsingComponentFromNetwork(ped);
			}
		}
	}
}

void CNetObjPed::SetTaskSequenceData(u32 numTasks, IPedNodeDataAccessor::CTaskData taskData[MAX_SYNCED_SEQUENCE_TASKS], u32 repeatMode)
{
	CPed* pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	RemoveTemporaryTaskSequence();

	// need to restore the game heap here as we are allocating tasks and task infos from the main heap
	sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

	if(pPed && numTasks > 0)
	{
		m_temporaryTaskSequenceId = static_cast<s16>(CTaskSequences::GetAvailableSlot(IsScriptObject()));

		if (gnetVerifyf(m_temporaryTaskSequenceId>=0, "Ran out of task sequences for %s", GetLogName()))
		{
			CTaskSequences::ms_TaskSequenceLists[m_temporaryTaskSequenceId].Flush();
			CTaskSequences::ms_TaskSequenceLists[m_temporaryTaskSequenceId].Register();

			for (u32 i=0; i<numTasks; i++)
			{
				IPedNodeDataAccessor::CTaskData* pCurrData = &taskData[i];

				CTaskInfo* pTaskInfo = CQueriableInterface::CreateEmptyTaskInfo(pCurrData->m_TaskType, true);

				if (AssertVerify(pTaskInfo))
				{
					u32 byteSizeOfTaskData = (pCurrData->m_TaskDataSize+7)>>3;
					datBitBuffer messageBuffer;
					messageBuffer.SetReadOnlyBits(pCurrData->m_TaskData, byteSizeOfTaskData<<3, 0);

					pTaskInfo->ReadSpecificTaskInfo(messageBuffer);

					CTask* pTask = pTaskInfo->CreateLocalTask(pPed);

					if (gnetVerifyf(pTask, "%s: The task info for task %s failed to create a task in a sequence (is CreateLocalTask defined for this info?)", GetLogName(), TASKCLASSINFOMGR.GetTaskName(pCurrData->m_TaskType)))
					{
						CTaskSequences::ms_TaskSequenceLists[m_temporaryTaskSequenceId].AddTask(pTask);
					}

					CTaskSequences::ms_TaskSequenceLists[m_temporaryTaskSequenceId].SetRepeatMode((s32)repeatMode);

					delete pTaskInfo;
				}
			}
		}
	}

	sysMemAllocator::SetCurrent(*oldHeap);
}

void CNetObjPed::PostTasksUpdate()
{
    DEV_ONLY(AssertPedTaskInfosValid());

    // now reorder the task infos based on their tree depths
    ReorderTasksBasedOnTreeDepths();

    DEV_ONLY(AssertPedTaskInfosValid());

    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	m_RunningSyncedScene = false;

    if(ped)
    {
        ped->GetPedIntelligence()->RecalculateCloneTasks();

        CNetworkSynchronisedScenes::PostTaskUpdateFromNetwork(ped);

		// can't do the instant AI update for newly created network clone peds that are not
        // full registered with the network yet. They are not fully setup so this leads to undefined behaviour
		bool hasBeenRegistered = GetListNode() != 0;

        if(hasBeenRegistered)
        {
			ped->SetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions,m_PRF_disableInVehicleActions);

            // Ensure that any tasks given directly to the ped end up in the correct state
            // Since network events are handled at the start of a game frame we could end up with a t-posed
            // character if we didn't ensure that the new tasks received an update prior to the animation update
            ped->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true);
		    ped->InstantAIUpdate(false);
		    ped->SetPedResetFlag(CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, false);
        }

        m_Wandering = ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER);

        if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SYNCHRONIZED_SCENE))
        {
            int networkSceneID   = ped->GetPedIntelligence()->GetQueriableInterface()->GetNetworkSynchronisedSceneID();
            m_RunningSyncedScene = networkSceneID != -1;
        }

		// we always want to update tasks for a clone ped when we have received a task update
        if(CPedAILodManager::IsTimeslicingEnabled())
	    {
		    ped->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	    }

		m_Vaulting = ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT);


		if(IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
		{
			CTaskInfo*	pTaskInfo = ped->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS, PED_TASK_PRIORITY_MAX, false);

			if(pTaskInfo)
			{
				bool bRunningTaskUseScenario = ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO);

				if(!bRunningTaskUseScenario && ped->GetAttachState() == ATTACH_STATE_WORLD)
				{
					gnetWarning("%s CNetObjPed::PostTasksUpdate remote script ped is using TASK_GO_TO_POINT_ANY_MEANS yet clone is still attached! Detaching from ATTACH_STATE_WORLD", GetLogName());
					ped->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				}
			}
		}
    }

    DEV_ONLY(AssertPedTaskInfosValid());

    AddRequiredProp();

    DEV_ONLY(AssertPedTaskInfosValid());
}

void CNetObjPed::UpdateTaskSpecificData()
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(ped)
	{
		ped->GetPedIntelligence()->UpdateTaskSpecificData();
	}
}

void CNetObjPed::SetIsTargettableByTeam(int team, bool bTargettable)
{
	if (gnetVerify(team >= 0 && team < MAX_NUM_TEAMS))
	{
		if (bTargettable)
		{
			m_isTargettableByTeam |= (1<<team);
		}
		else
		{
			m_isTargettableByTeam &= ~(1<<team);
		}
	}
}

bool CNetObjPed::GetIsTargettableByTeam(int team) const
{
	gnetAssert(team >= -1 && team < MAX_NUM_TEAMS);
	return (team==-1) || (m_isTargettableByTeam & (1<<team));
}

void CNetObjPed::SetIsTargettableByPlayer(int player, bool bTargettable)
{
	if (gnetVerify(player >= 0 && player < MAX_NUM_PHYSICAL_PLAYERS))
	{
		if (bTargettable)
		{
			m_isTargettableByPlayer |= (1<<player);
		}
		else
		{
			m_isTargettableByPlayer &= ~(1<<player);
		}
	}
}

bool CNetObjPed::GetIsTargettableByPlayer(int player) const
{
	gnetAssert(player > -1 && player < MAX_NUM_PHYSICAL_PLAYERS);
	return (m_isTargettableByPlayer & (1<<player)) != 0;
}

void CNetObjPed::SetIsDamagableByPlayer(int player, bool bTargettable)
{
	if (gnetVerify(player >= 0 && player < MAX_NUM_PHYSICAL_PLAYERS))
	{
		if (bTargettable)
		{
			m_isDamagableByPlayer |= (1<<player);
		}
		else
		{
			m_isDamagableByPlayer &= ~(1<<player);
		}
	}
}

bool CNetObjPed::GetIsDamagableByPlayer(int player) const
{
	gnetAssert(player > -1 && player < MAX_NUM_PHYSICAL_PLAYERS);
	return (m_isDamagableByPlayer & (1<<player)) != 0;
}

//
// name:        UpdateLocalTaskSlotCacheForSync
// description: this function is responsible to update the local task slot cache before a force sync.
//
void CNetObjPed::UpdateLocalTaskSlotCacheForSync()
{
	m_taskSlotCache.m_lastFrameUpdated = 0;
	UpdateLocalTaskSlotCache();
}

void CNetObjPed::GetStandingOnObjectData(ObjectId &standingOnNetworkObjectID, Vector3 &localOffset) const
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    standingOnNetworkObjectID = NETWORK_INVALID_OBJECT_ID;

    CPhysical *objectStoodOn = ped->GetGroundPhysical();
    float      groundZ       = ped->GetGroundPos().z;

    if(ped->GetUsingRagdoll() && ped->GetRagdollInst() && objectStoodOn == 0)
    {
        // ragdolling peds always have a NULL ground physical object, so we need to do the probe ourselves
        int nTestTypeFlags = ArchetypeFlags::GTA_VEHICLE_TYPE;

        const float PROBE_DIST = 2.0f;
        Vector3 startPos = VEC3V_TO_VECTOR3(ped->GetRagdollInst()->GetPosition());
        Vector3 endPos   = startPos - Vector3(0.0f, 0.0f, PROBE_DIST);

        WorldProbe::CShapeTestHitPoint result;
        WorldProbe::CShapeTestResults probeResult(result);
        WorldProbe::CShapeTestProbeDesc probeDesc;

        probeDesc.SetStartAndEnd(startPos, endPos);
        probeDesc.SetResultsStructure(&probeResult);
        probeDesc.SetIncludeFlags(nTestTypeFlags);
        probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
        probeDesc.SetContext(WorldProbe::ENetwork);

        if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
        {
            if(probeResult.GetResultsReady() && probeResult[0].GetHitDetected() && probeResult[0].GetHitInst())
            {
                CEntity *hitEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());

                if(hitEntity && hitEntity->GetIsPhysical())
                {
                    objectStoodOn = static_cast<CPhysical *>(hitEntity);
                    groundZ       = probeResult[0].GetHitPosition().z;
                }
            }
        }
    }

	static float MIN_RADIUS_VALID_STAND_ON_OBJECT = 1.0f; //Min radius of object to register as something the player is stood on - to avoid blend move issue with small vending cans etc B*1588069

    // we don't send positions relative to peds we are standing on
    if(objectStoodOn) 
    {
		CObject* cObjectStoodOn = nullptr;
		if(objectStoodOn->GetIsTypeObject())
		{
			cObjectStoodOn = static_cast<CObject *>(objectStoodOn);
		}		
		if(objectStoodOn->GetIsTypePed() || objectStoodOn->GetBoundRadius() < MIN_RADIUS_VALID_STAND_ON_OBJECT || (cObjectStoodOn && cObjectStoodOn->m_nObjectFlags.bIsNetworkedFragment))
		{
			objectStoodOn = 0;
		}
    }

    localOffset.Zero();
    if(objectStoodOn && objectStoodOn->GetNetworkObject())
    {
		Vec3V diff = ped->GetTransform().GetPosition() - objectStoodOn->GetTransform().GetPosition();
	
		// fix for clones walking on boats. Minimising this fix just to boats for now.
		if (objectStoodOn->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_BOAT &&
			objectStoodOn->GetNetworkObject()->GetNetBlender() && 
			SafeCast(netBlenderLinInterp, objectStoodOn->GetNetworkObject()->GetNetBlender())->DisableZBlending())
		{
			localOffset = VEC3V_TO_VECTOR3(objectStoodOn->GetTransform().UnTransform3x3(diff));	
		}
		else
		{
			float diffZ = diff.GetZf();
			diff.SetZ(ScalarV(V_ZERO));
			localOffset = VEC3V_TO_VECTOR3(objectStoodOn->GetTransform().UnTransform3x3(diff));

			if(ped->GetUsingRagdoll())
			{
				localOffset.z = ped->GetTransform().GetPosition().GetZf() - groundZ;
			}
			else
			{
				const float groundToRootOffset = ped->GetCapsuleInfo() ? ped->GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;

				localOffset.z = diffZ - groundToRootOffset;
			}
		}

		if(rage::Abs(localOffset.x) < MAX_STANDING_OBJECT_OFFSET_POS &&
			rage::Abs(localOffset.y) < MAX_STANDING_OBJECT_OFFSET_POS &&
			rage::Abs(localOffset.z) < MAX_STANDING_OBJECT_OFFSET_HEIGHT)
		{
			standingOnNetworkObjectID = objectStoodOn->GetNetworkObject()->GetObjectID();
		}		
   }
}

void CNetObjPed::SelectCurrentClipSetForClone()
{
	// update the pending motion group if it is now streamed in
	if(gnetVerifyf(m_pendingMotionSetId != CLIP_SET_ID_INVALID, "Trying to select an invalid clipset for a clone!"))
	{
		// check if animations are in memory
		bool animsLoaded = m_motionSetClipSetRequestHelper.IsLoaded();

		if(gnetVerifyf(animsLoaded, "Selecting an anim group for a clone that has not been streamed in! ped %p %s %s : pending blend state %d : type %d : %d %s", 
			GetPed(), 
			GetPed() ? GetPed()->GetDebugName() : "NULL PED",
			GetLogName(),
			m_PendingMoveBlenderState, 
			m_PendingMoveBlenderType, 
			m_pendingMotionSetId.GetHash(),
			m_pendingMotionSetId.GetCStr()))
		{
        	CPed *ped = GetPed();

			if(ped)
			{
				CTaskMotionBase *motionTask = ped->GetPrimaryMotionTask();
				if(gnetVerifyf(motionTask, "Trying to select a clipset for a ped without a motion task!"))
				{
					if(motionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
					{
						switch(m_PendingMoveBlenderState)
						{
						case CTaskMotionPed::State_Strafing:
						case CTaskMotionPed::State_Aiming:
							motionTask->SetOverrideStrafingClipSet(m_pendingMotionSetId);
							break;
						case CTaskMotionPed::State_OnFoot:
						case CTaskMotionPed::State_Crouch:
						case CTaskMotionPed::State_StandToCrouch:
						case CTaskMotionPed::State_CrouchToStand:
						case CTaskMotionPed::State_StrafeToOnFoot:
							motionTask->SetOnFootClipSet(m_pendingMotionSetId);
							break;
						case CTaskMotionPed::State_Swimming:
							motionTask->SetDefaultSwimmingClipSet(m_pendingMotionSetId);
							break;
						case CTaskMotionPed::State_Diving:
							motionTask->SetDefaultDivingClipSet(m_pendingMotionSetId);
							break;
						default:
							gnetAssertf(0, "Unexpected Move Blender Type! %s", m_pendingMotionSetId.GetCStr());
						}
					}
					else if(motionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED_LOW_LOD)
					{
						switch(m_PendingMoveBlenderState)
						{
						case CTaskMotionPedLowLod::State_OnFoot:
							motionTask->SetOnFootClipSet(m_pendingMotionSetId);
							break;
						case CTaskMotionPedLowLod::State_Swimming:
							motionTask->SetDefaultSwimmingClipSet(m_pendingMotionSetId);
							break;
						default:
							gnetAssertf(0, "Unexpected Move Blender Type! %s", m_pendingMotionSetId.GetCStr());
						}
					}
					else 
					{
						switch(motionTask->GetTaskType())
						{
						case CTaskTypes::TASK_MOTION_TENNIS:							
						case CTaskTypes::TASK_ON_FOOT_BIRD:
						case CTaskTypes::TASK_ON_FOOT_FLIGHTLESS_BIRD:
						case CTaskTypes::TASK_ON_FOOT_FISH:
						case CTaskTypes::TASK_ON_FOOT_QUAD:
							motionTask->SetOnFootClipSet(m_pendingMotionSetId);
							break;
						default:
							gnetAssertf(false, "ERROR : Invalid Task Motion! %s : motionTaskType = %d : pendingMotionSet = %s", __FUNCTION__, motionTask->GetTaskType(), m_pendingMotionSetId.GetCStr());
						}					
					}
                }
            }
		}
	}
}

void CNetObjPed::SetupPedAIAsLocal()
{
    CPed *pPed = GetPed();

    if(pPed)
    {
        pPed->GetPedIntelligence()->GetTaskManager()->DeleteTree(PED_TASK_TREE_PRIMARY);
        pPed->GetPedIntelligence()->GetTaskManager()->SetTree(PED_TASK_TREE_PRIMARY, rage_new CTaskTreePed(PED_TASK_PRIORITY_MAX, pPed, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, PED_TASK_PRIORITY_DEFAULT));
    }
}

//
// name:        SetupPedAIAsClone
// description: setup the ped AI as a network clone
//
void CNetObjPed::SetupPedAIAsClone()
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    if(pPed)
    {
        m_bAllowSetTask = true;

		//Set abort Ragdoll to False because it causes issues during player respawn 
		// if the Ped Inteligence is flushed with Ragdoll aborted.
		//Issues are: T-Posing and Ped getting up and ragdoll to the floor.
		const bool abortRagdoll = (!IsLocalFlagSet(LOCALFLAG_RESPAWNPED));
		pPed->GetPedIntelligence()->FlushImmediately(false, false, abortRagdoll);

        pPed->GetPedIntelligence()->GetTaskManager()->DeleteTree(PED_TASK_TREE_PRIMARY);
        pPed->GetPedIntelligence()->GetTaskManager()->SetTree(PED_TASK_TREE_PRIMARY, rage_new CTaskTreeClone(pPed, PED_TASK_PRIORITY_MAX));

        pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
       
		m_bAllowSetTask = false;
    }
}

u8 CNetObjPed::GetHighestAllowedUpdateLevel() const
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if(pPed && !pPed->GetUsingRagdoll() && m_TimeToForceLowestUpdateLevel == 0)
	{
		if (!pPed->IsAPlayerPed() && (pPed->GetMotionData()->GetIsStill() || pPed->GetMotionData()->GetIsWalking()))
		{
			// walking peds do not need to be in a high update level
			return CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
		}
	}

	return CNetObjPhysical::GetHighestAllowedUpdateLevel();
}

bool CNetObjPed::CanBlendWhenAttached() const
{
    CNetBlenderPed *netBlender = SafeCast(CNetBlenderPed, GetNetBlender());

    if(netBlender && netBlender->HasAttachedRagdoll())
    {
        return true;
    }

    return CNetObjPhysical::CanBlendWhenAttached();
}

bool CNetObjPed::IsAttachmentStateSentOverNetwork() const
{
    CPed *pPed = GetPed();

    if(IsScriptObject(true) || IsGlobalFlagSet(GLOBALFLAG_WAS_SCRIPTOBJECT))
    {
        if(pPed && (pPed->GetAttachState() != ATTACH_STATE_PED_ON_GROUND))
        {
            return true;
        }
    }

    return false;
}

bool CNetObjPed::CanProcessPendingAttachment(CPhysical** ppCurrentAttachmentEntity, int* failReason) const
{
	CPed *pPed = GetPed();

	bool bCanProcess = true;

	if (pPed && (pPed->GetIsAttachedInCar() || pPed->GetIsAttachedOnMount()))
	{
		// don't log ped in car failure reasons as this spams the logs
		/*
		if (failReason)
		{
			*failReason = CPA_PED_IN_CAR;
		}
		*/ 

		bCanProcess = false;
	}

	// we don't want to change the attachment state for a ped while they are attached to a vehicle by being in it
	if (bCanProcess)
	{
		bCanProcess = CNetObjPhysical::CanProcessPendingAttachment(ppCurrentAttachmentEntity, failReason);
	}

	if (bCanProcess)
	{
		// we don't want to attach ragdolling peds (wait until they have stopped ragdolling)
		if(pPed && pPed->GetUsingRagdoll())
		{
			if (failReason)
			{
				*failReason = CPA_PED_RAGDOLLING;
			}

			bCanProcess = false;
		}

		// we don't want to change the attachment state of a ped while the network blender is attaching it to something
		CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

		if(bCanProcess && netBlenderPed && netBlenderPed->HasAttachedRagdoll())
		{
			if (failReason)
			{
				*failReason = CPA_PED_HAS_ATTACHED_RAGDOLL;
			}

			bCanProcess = false;
		}
	}

	return bCanProcess;
}

bool CNetObjPed::AttemptPendingAttachment(CPhysical* pEntityToAttachTo, unsigned* LOGGING_ONLY(reason))
{
    CPed *pPed = GetPed();

	bool bSuccess = false;
#if ENABLE_NETWORK_LOGGING
	if(reason)
	{
		*reason = APA_NONE_PED;
	}
#endif // ENABLE_NETWORK_LOGGING
	if(m_pendingAttachFlags & ATTACH_FLAG_DONT_NETWORK_SYNC)
	{
		gnetAssert(0);
	}

    switch(m_pendingAttachFlags & ATTACH_STATES)
    {
	case ATTACH_STATE_RAGDOLL:
		bSuccess = pPed->AttachToPhysicalUsingPhysics(pEntityToAttachTo, (s16)m_pendingOtherAttachBone, -1, m_pendingAttachFlags, &m_pendingAttachOffset, 0, 0, 10.0f);
		break;
	case ATTACH_STATE_BASIC:
        pPed->AttachToPhysicalBasic(pEntityToAttachTo, (s16)m_pendingOtherAttachBone, m_pendingAttachFlags, &m_pendingAttachOffset, &m_pendingAttachQuat);
        bSuccess = true;
		break;
    case ATTACH_STATE_PED:
	    pPed->AttachPedToPhysical(pEntityToAttachTo, (s16)m_pendingOtherAttachBone, m_pendingAttachFlags, &m_pendingAttachOffset, NULL, m_pendingAttachHeading, m_pendingAttachHeadingLimit);
		bSuccess = true;
	    break;
	case ATTACH_STATE_PED_ON_GROUND:
		pPed->AttachPedToGround(pEntityToAttachTo, (s16)m_pendingMyAttachBone, m_pendingAttachFlags, &m_pendingAttachOffset, m_pendingAttachHeading, m_pendingAttachHeadingLimit);
        pPed->OnAttachToGround();
		bSuccess = true;
		break;
	// intentional fallthrough, we want to attach the ped to the car for any of these attach states
    case ATTACH_STATE_PED_ENTER_CAR:
    case ATTACH_STATE_PED_IN_CAR:
    case ATTACH_STATE_PED_EXIT_CAR:
	    gnetAssert(0);
	    break;
    default:
	    break;
	}

	return bSuccess;
}

void CNetObjPed::AttemptPendingDetachment(CPhysical *pEntityToAttachTo)
{
    CPed *ped = GetPed();

    bool wasAttachedToGround = ped ? (ped->GetAttachState() == ATTACH_STATE_PED_ON_GROUND) : false;

    CNetObjPhysical::AttemptPendingDetachment(pEntityToAttachTo);

    if(wasAttachedToGround)
    {
        ped->OnDetachFromGround();
    }
}

void CNetObjPed::UpdatePendingAttachment()
{
	GetPed()->SetAttachHeading(m_pendingAttachHeading);
	GetPed()->SetAttachHeadingLimit(m_pendingAttachHeadingLimit);

	CNetObjPhysical::UpdatePendingAttachment();
}

void CNetObjPed::ClearPendingAttachmentData()
{
	m_pendingAttachHeading      = 0.0f;
	m_pendingAttachHeadingLimit = 0.0f;

	CNetObjPhysical::ClearPendingAttachmentData();
}

bool CNetObjPed::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	CPed *pPed = GetPed();

	bool bFix = CNetObjPhysical::FixPhysicsWhenNoMapOrInterior(npfbnReason);

	// peds in low lod physics do not need to be fixed if they have a valid nav mesh - player's are fixed as we can't guarantee they will stay
    // on the navmesh, which can lead to them popping around as they switch between fixed and non-fixed - they are also made invisible at this point
    if (pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
	{
        if(pPed->GetNavMeshTracker().GetIsValid() && !pPed->IsPlayer())
        {
			if(npfbnReason)
			{
				*npfbnReason = NPFBN_LOW_LOD_NAVMESH;
			}
		    bFix = false;
        }
	}
    else
    {
        // peds getting their Z position from animation don't need to be fixed
        if(pPed->GetUseExtractedZ())
        {
            // unless they are already fixed, we don't want peds temporarily unfixing while they jump
            if(!pPed->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
            {
				if(npfbnReason)
				{
					*npfbnReason = NPFBN_ANIMATED_Z_POS;
				}
                bFix = false;
            }
        }
    }

	return bFix;
}

bool CNetObjPed::ShouldDoFixedByNetworkInAirCheck() const
{
    CPed *ped = GetPed();

    if(ped && IsClose(ped->GetGroundPos().z, PED_GROUNDPOS_RESET_Z))
    {
        return true;
    }

    return false;
}

bool CNetObjPed::CheckBoxStreamerBeforeFixingDueToDistance() const
{
    CPed *ped = GetPed();

    if(ped && (ped->IsLawEnforcementPed() || IsImportant() || IsOrWasScriptObject()))
    {
        return true;
    }

    return CNetObjPhysical::CheckBoxStreamerBeforeFixingDueToDistance();
}

void CNetObjPed::UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason)
{
	CPed *ped = GetPed();

	bool bInVehicle = ped && ped->GetIsInVehicle();

	if(fixByNetwork && !bInVehicle && !ped->IsAPlayerPed())
	{
		// don't hide peds involved in a cutscene
		if (!(NetworkInterface::IsInMPCutscene() && IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE)))
		{
	        SetOverridingLocalVisibility(false, NetworkUtils::GetFixedByNetworkReason(reason));
		}
    }

	if(m_RunningSyncedScene)
	{
		CTask* syncedSceneTask = ped->GetPedIntelligence()->GetTaskActive();
		if(!syncedSceneTask || syncedSceneTask->GetTaskType() != CTaskTypes::TASK_SYNCHRONIZED_SCENE)
		{
			if(!ped->GetIsOnScreen())
			{
				reason = FBN_SYNCED_SCENE_NO_TASK;
				fixByNetwork = true;
			}
		}
	}

    if(m_HiddenUnderMapDueToVehicle)
    {
        reason = FBN_HIDDEN_UNDER_MAP;
        fixByNetwork = true;
    }

    CNetObjPhysical::UpdateFixedByNetwork(fixByNetwork, reason, npfbnReason);
}

void CNetObjPed::DestroyPed()
{
	CPed *pPed = GetPed();

    if(pPed)
    {
        // make sure it is on the scene update so it actually gets removed
        if(!pPed->GetIsOnSceneUpdate() || !gnetVerifyf(pPed->GetUpdatingThroughAnimQueue() || pPed->GetIsMainSceneUpdateFlagSet(),
                                                       "Flagging a ped for removal (%s) that is on neither the main scene update or update through anim queue list! This will likely cause a ped leak!", GetLogName()))
        {
            pPed->AddToSceneUpdate();
        }

        // clear out the player info for this player, this will be deleted by the reference
        // in CNetGamePlayer
        if(pPed->IsPlayer())
        {
            CPlayerInfo *playerInfo = pPed->GetPlayerInfo();

            if(playerInfo && 
               gnetVerifyf(GetPlayerOwner(), "Deleting a player ped with no player owner!") &&
               gnetVerifyf(GetPlayerOwner()->GetPlayerInfo() == playerInfo, "Deleting a player ped for a network player with no reference to the player info!"))
            {
                pPed->SetPlayerInfo(0);
                playerInfo->SetPlayerPed(0);
            }

            const CControlledByInfo controlInfo(false, false);
            pPed->SetControlledByInfo(controlInfo);
        }

        pPed->FlagToDestroyWhenNextProcessed();
    }
}

//
// name:        CNetObjPed::SetRequiredProp
// description: sets m_requiredProp if a play random ambients task is running
//
void CNetObjPed::SetRequiredProp()
{
    /*CPed *pPed = GetPed();

    m_requiredProp = -1;

    if(AssertVerify(pPed))
    {
        if ( pPed->GetWeaponMgr()->GetChosenWeaponType() == WEAPONTYPE_OBJECT )
        {
            CTaskSimplePlayRandomAmbients* pTask = (CTaskSimplePlayRandomAmbients*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SIMPLE_PLAY_RANDOM_AMBIENTS);

            if( pTask )
            {
                if (pTask->HasPermanentProp(pPed))
                {
                    if(AssertVerify(pPed->GetWeaponMgr()->GetEquippedWeaponObject()))
                    {
                        m_requiredProp = pPed->GetWeaponMgr()->GetEquippedWeaponObject()->GetModelIndex();
                    }
                }
                else
                {
                    m_requiredProp = pTask->GetForcedProp();
                }
            }
        }
    }*/
}

//
// name:        CNetObjPed::AddRequiredProp
// description: adds the required prop if a play random ambients task is running
//
void CNetObjPed::AddRequiredProp()
{
    // if the ped is running an ambient anims task tell it about the prop
    if (m_requiredProp.IsNotNull())
    {
		// The body of SetRequiredProp has been commented out since 2010. Not concerned with getting this working right now. [9/13/2012 mdawe]
  //      CPed *pPed = GetPed();
		//VALIDATE_GAME_OBJECT(pPed);

		//if(pPed)
  //      {
  //          bool propAdded  = CPropManagementHelper::GivePedProp(pPed, m_requiredProp);

  //          if(propAdded)
  //          {
  //              m_requiredProp.Clear();
  //          }
  //      }
    }
}

//
// name:        CNetObjPed::UpdateWeaponForPed
// description: update the currently equipped weapon for ped
//
void CNetObjPed::UpdateWeaponForPed()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		CPedWeaponManager* pPedWeaponMgr = pPed->GetWeaponManager();
		CWeapon* equippedWeapon = pPedWeaponMgr ? pPedWeaponMgr->GetEquippedWeapon() : NULL;
		if (equippedWeapon && equippedWeapon->GetWeaponInfo() && !equippedWeapon->GetWeaponInfo()->GetIsUnarmed())
		{
			if(equippedWeapon->GetDrawableEntityTintIndex() != m_weaponObjectTintIndex)
			{
				weaponitemDebugf3("CNetObjPed::UpdateWeaponForPed pPed[%p] -- (equippedWeapon->GetDrawableEntityTintIndex()[%u] != m_weaponObjectTintIndex[%u]) --> UpdateShaderVariables",pPed,equippedWeapon->GetDrawableEntityTintIndex(),m_weaponObjectTintIndex);
				equippedWeapon->UpdateShaderVariables(m_weaponObjectTintIndex);
			}

			CWeaponComponentFlashLight* pFlashLight = equippedWeapon->GetFlashLightComponent();
			if(pFlashLight && pFlashLight->GetActive() != m_weaponObjectFlashLightOn)
			{
				pFlashLight->SetActive(m_weaponObjectFlashLightOn);
			}

			//fallback check to ensure that if the ped alpha is 255 then the weapon alpha should be 255, there are some cases where the weapon can have a 0 alpha making it invisible remotely.
			CEntity* pWeaponEntity = const_cast<CEntity *>(equippedWeapon->GetEntity());
			if (pWeaponEntity && (pWeaponEntity->GetAlpha() < 255))
			{
				if (pPed->GetAlpha() == 255)
				{
					//make the weapon alpha 255 also - saving it from invisibility or partial alpha.
					pWeaponEntity->SetAlpha(255);
				}
			}
		}
	}
}

//
// name:        CNetObjPed::UpdateWeaponStatusFromPed
// description: update weapon status from the ped
//
void CNetObjPed::UpdateWeaponStatusFromPed()
{
    CPed    *pPed    = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if(CacheWeaponInfo())
	{
		return;
	}

	CPedWeaponManager *pPedWeaponMgr = pPed ? pPed->GetWeaponManager() : NULL;
	CObject *pWeaponObject = pPedWeaponMgr ? pPedWeaponMgr->GetEquippedWeaponObject() : 0;

	u32 desiredWeapon = pPedWeaponMgr->GetEquippedWeaponHash();
	u32 currentWeapon = pPedWeaponMgr->GetEquippedWeapon() ? pPedWeaponMgr->GetEquippedWeapon()->GetWeaponInfo()->GetHash() : 0;

	// is the held weapon the same as the desired weapon?
	if (desiredWeapon == currentWeapon || (pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee)))
	{
		UpdateWeaponForPed();
		return;
	}

	bool saveCanAlter = m_CanAlterPedInventoryData;

	m_CanAlterPedInventoryData = true;

	if (m_weaponObjectExists)
	{
		if (!pWeaponObject)
		{
#if !__NO_OUTPUT
			if (pPed && pPedWeaponMgr && pPed->GetInventory() && (pPed->GetWeaponManager()->GetEquippedWeaponHash() == 0))
			{
				weaponitemWarningf("CNetObjPed::UpdateWeaponStatusFromPed (%s) m_weaponObjectExists && !pWeaponObject && GetEquippedWeaponHash == 0", GetLogName());
			}
#endif

			if (pPed && pPedWeaponMgr && pPed->GetInventory()
				&& (pPed->GetWeaponManager()->GetEquippedWeaponHash() != 0)
				&& (pPed->GetWeaponManager()->GetEquippedWeaponHash() != pPed->GetDefaultUnarmedWeaponHash())
				&& pPed->GetInventory()->GetIsStreamedIn(pPed->GetWeaponManager()->GetEquippedWeaponHash()))
			{
				CPedEquippedWeapon::eAttachPoint attach = m_weaponObjectAttachLeft ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand;
				
				CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash());
				if (pWeaponItem)
					pWeaponItem->SetTintIndex(m_weaponObjectTintIndex);

				pPed->GetWeaponManager()->CreateEquippedWeaponObject(attach);
				pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			}
		}
		else
		{
			if(pPed->GetWeaponManager()->GetCreateWeaponObjectWhenLoaded())
			{
				if(pPed->GetWeaponManager()->CreateEquippedWeaponObject())
				{
					pPed->GetWeaponManager()->SetCreateWeaponObjectWhenLoaded(false);
				}
			}
		}

		UpdateWeaponForPed();
	}
	else if (pWeaponObject)
	{
		pPedWeaponMgr->DestroyEquippedWeaponObject(false OUTPUT_ONLY(,"CNetObjPed::UpdateWeaponStateFromPed"));
		pWeaponObject = NULL;
	}

	m_CanAlterPedInventoryData = saveCanAlter;
}

//
// name:        CNetObjPed::UpdateWeaponVisibilityFromPed
// description: update weapon visibility from the ped
//
void CNetObjPed::UpdateWeaponVisibilityFromPed()
{
	CPed    *pPed    = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// update weapon visibility
	CPedWeaponManager *pPedWeaponMgr = pPed ? pPed->GetWeaponManager() : NULL;
	CObject *pWeaponObject = pPedWeaponMgr ? pPedWeaponMgr->GetEquippedWeaponObject() : 0;

	if(pWeaponObject && !pWeaponObject->GetNetworkObject() && !CacheWeaponInfo() )
	{
		if(pPed->GetIsVisible())
		{
			bool isScopedWeapon = pWeaponObject->GetWeapon() && pWeaponObject->GetWeapon()->GetScopeComponent();

#if !__NO_OUTPUT
			if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG3))
			{
				if (pPed)
				{
					bool bCurrentVisible = pWeaponObject->GetIsVisibleForModule( SETISVISIBLE_MODULE_NETWORK );
					bool bTestValue = (IsClone() && !isScopedWeapon) ? m_weaponObjectVisible : true;
					if (bCurrentVisible != bTestValue)
					{
						weaponitemDebugf3("CNetObjPed::UpdateWeaponVisibilityFromPed--ped[%p][%s][%s]--pWeaponObject[%p][%s]--IsClone[%d]--isScopedWeapon[%d]--attached[%d]--bCurrentVisible[%d]--bTestValue[%d]",pPed,pPed->GetModelName(),pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",pWeaponObject,pWeaponObject->GetModelName(),IsClone(),isScopedWeapon,pWeaponObject->GetIsAttached(),bCurrentVisible,bTestValue);
					}
				}
			}
#endif

			if(IsClone() && !isScopedWeapon)
			{
				SetIsVisibleForModuleSafe( pWeaponObject, SETISVISIBLE_MODULE_NETWORK, m_weaponObjectVisible, true );
			}
			else
			{
				SetIsVisibleForModuleSafe( pWeaponObject, SETISVISIBLE_MODULE_NETWORK, true, true );
			}
		}
		else
		{
			// weapons should never be visible when the ped is invisible
			SetIsVisibleForModuleSafe( pWeaponObject, SETISVISIBLE_MODULE_NETWORK, false, true );
		}
	}
}

//
// name:        CNetObjPed::UpdateWeaponSlots
// description: update the ped's weapon slots
//
void CNetObjPed::UpdateWeaponSlots(u32 chosenWeaponType, bool forceUpdate)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	if (!pPed)
		return;

	weaponitemDebugf3("CNetObjPed::UpdateWeaponSlots pPed[%p] chosenWeaponType[%u]",pPed,chosenWeaponType);

	CPedWeaponManager *pPedWeaponMgr = pPed->GetWeaponManager();
	CPedInventory* pPedInventory = pPed->GetInventory();

	if ( pPedWeaponMgr && pPedInventory && (forceUpdate || (pPedWeaponMgr->GetEquippedWeaponObjectHash() != chosenWeaponType) || (pPedInventory->GetAmmoRepository().GetIsUsingInfiniteAmmo() != m_weaponObjectHasAmmo)) )
    {
		weaponitemDebugf3("( pPedWeaponMgr && pPedInventory && ((pPedWeaponMgr->GetEquippedWeaponHash() != chosenWeaponType) || (pPedInventory->GetAmmoRepository().GetIsUsingInfiniteAmmo() != m_weaponObjectHasAmmo)) )");

		bool saveCanAlter = m_CanAlterPedInventoryData;

		CPedEquippedWeapon::eAttachPoint attach = m_weaponObjectAttachLeft ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand;

		m_CanAlterPedInventoryData = true;

		// ensure the ped has the weapon that is being equipped
        if((chosenWeaponType != 0) && pPed->GetInventory())
		{
			weaponitemDebugf3("((chosenWeaponType != 0) && pPed->GetInventory())");

			CWeaponItem *weaponItem = pPed->GetInventory()->GetWeapon(chosenWeaponType);
			if (!weaponItem)
			{
				// flush the inventory for remote players as we do not care about the rest of the contents, and the inventory item pool can overflow if all 32 players have full inventories
				if (pPed->IsAPlayerPed() && !pPed->IsLocalPlayer())
				{					
					pPed->GetInventory()->RemoveAllWeaponsExcept(pPed->GetDefaultUnarmedWeaponHash());
				}

				weaponitemDebugf3("!weaponItem-->invoke AddWeapon");
				weaponItem = pPed->GetInventory()->AddWeapon(chosenWeaponType);
			}

			if(weaponItem)
			{
				weaponitemDebugf3("invoke weaponItem->SetTintIndex");
				weaponItem->SetTintIndex(m_weaponObjectTintIndex);
			}
        }

		s32 iNewVehicleSelection = -1;

		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		bool bPedInitiallyInsideVehicle = pVehicle != 0 ? true : false;

		weaponitemDebugf3("GetVehiclePedInside pVehicle[%p]",pVehicle);

		//If the ped isn't currently in the vehicle, he may be entering it.
		//Get the vehicle from ped entering vehicle task if the ped inside isn't set yet. lavalley.
		if (!pVehicle)
		{
			if (pPed->GetIsEnteringVehicle(true))
				pVehicle = pPed->GetVehiclePedEntering();
		}

		weaponitemDebugf3("revised pVehicle[%p]",pVehicle);

		if(pVehicle)
		{
			if (!pVehicle->GetVehicleWeaponMgr() && pVehicle->GetStatus()!=STATUS_WRECKED)
				pVehicle->CreateVehicleWeaponMgr(); //needs to have it at this point, otherwise the set below will not happen and that is bad

			if (pVehicle->GetVehicleWeaponMgr())
			{
				atArray<const CVehicleWeapon*> weapons;

				//When a ped is entering a vehicle the seat that a ped is in isn't set yet.
				//So, using it here will preclude using the vehicle weapons: water cannon, rhino tank turret.
				//Just use the driver seat here explicitly.  This works and fixes the problem for the driver of
				//these vehicles.  Also, if another ped gets into another seat they don't get control of the vehicle weapon.
				//This fixes the issues with vehicle weapon control and firing. lavalley.
				//
				//Also - only allow use of seatIndex 0 in lieu of -1 if the ped is just trying to enter the vehicle - otherwise can try to use 0 when the 
				//ped has GetVehiclePedInside valid but ped is about to exit the vehicle on the remote and that hasn't hit yet - only use the fallback
				//for entering the vehicle.  If the ped is inside the vehicle but not entering they should have a valid seat index and the fallback shouldn't be used. lavalley.
				s32 seatIndex = pVehicle->GetSeatManager() ? pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) : -1;
				weaponitemDebugf3("seatIndex[%d] bPedInitiallyInsideVehicle[%d]",seatIndex,bPedInitiallyInsideVehicle);
				if (!bPedInitiallyInsideVehicle && (seatIndex == -1))
				{
					//If the seat index isn't set yet then default the seatIndex value here to 0
					weaponitemDebugf3("!bPedInitiallyInsideVehicle && (seatIndex == -1) --> seatIndex = 0");
					seatIndex = 0;
				}

				if (seatIndex >= 0)
				{
					weaponitemDebugf3("seatIndex[%d]",seatIndex);

					pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIndex, weapons);
					if(weapons.GetCount() > 0)
					{
						weaponitemDebugf3("weapons.GetCount()[%d] --> iNewVehicleSelection = 0; chosenWeaponType = 0;",weapons.GetCount());
						if(pPed->IsPlayer()) // players sync the vehicle weapon via the player state node
						{
							iNewVehicleSelection = GetVehicleWeaponIndex();
							if(iNewVehicleSelection != -1)
								chosenWeaponType = 0;
						}
						else 
						{
							iNewVehicleSelection = 0;
							chosenWeaponType = 0; //reset this to zero because in EquipWeapon if you have a hash it will take precedence over vehicle weapon - we don't want this to happen (lavalley)
						}
					}
				}
			}
		}

		if(pPed->IsNetworkClone())
		{
			if (m_weaponObjectHasAmmo)
			{
				//Cloned player may have cheated or picked up a weapon after it 
				//was created and will be managing its own ammo status,
				//but locally we need to ensure that it is seen to have ammo so
				//its visual firing effects are seen
				pPedInventory->GetAmmoRepository().SetUsingInfiniteAmmo(true);
			}
			else
			{
				pPedInventory->GetAmmoRepository().SetUsingInfiniteAmmo(m_CacheUsingInfiniteAmmo);
			}
		}

		bool bCreateWeaponObject = m_weaponObjectExists && (!CacheWeaponInfo() || forceUpdate);

		if ((chosenWeaponType == 0 || pPed->GetDefaultUnarmedWeaponHash() == chosenWeaponType) && m_hasDroppedWeapon)
		{
			u32 currentWeapon = pPedWeaponMgr->GetEquippedWeaponHash();
			if (currentWeapon != 0)
			{
				CObject* pObject = pPedWeaponMgr->DropWeapon(currentWeapon, false, false, false, true);
				if (pObject)
				{
					// bug:url:bugstar:6019704 - GTAO Mugging - killing a ped when another player is far away, then teleporting close by after the case is dropped means there is no case.
					bool bWasVisible = pObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK);
					pObject->SetNetworkVisibility(true, "reset after drop");
					pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, true, true);
					bool bPlacedOnGround = pObject->PlaceOnGroundProperly();
					if (!bPlacedOnGround && !bWasVisible)
					{
						pObject->SetIsFixedWaitingForCollision(true);
					}
				}
			}
		}
		else
		{
			weaponitemDebugf3("%s equippeding weapon: %d create? %s", GetLogName(), chosenWeaponType, bCreateWeaponObject?"TRUE":"FALSE");
			pPedWeaponMgr->EquipWeapon(chosenWeaponType, iNewVehicleSelection, bCreateWeaponObject, false, attach);
		}

		if (pPed->IsNetworkClone() && m_weaponObjectHasAmmo && pPedWeaponMgr->GetEquippedWeapon())
		{
			pPedWeaponMgr->GetEquippedWeapon()->SetAmmoInClip(255);
			pPedWeaponMgr->GetEquippedWeapon()->SetAmmoTotalInfinite();
		}

		m_CanAlterPedInventoryData = saveCanAlter;
    }
}

bool CNetObjPed::TaskIsOverridingProp()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (IsClone() && GetTaskOverridingProps() && pPed->GetWeaponManager())
	{
		u32 weaponHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();

		if( weaponHash == OBJECTTYPE_OBJECT )
		{
			return true;
		}
	}

	return false;
}

bool CNetObjPed::CacheWeaponInfo()
{
	CPed *pPed = GetPed();

	if( TaskIsOverridingProp() ||
		GetTaskOverridingPropsWeapons() ||
		GetMigrateTaskHelper())
	{
		weaponitemDebugf3("CNetObjPed::CacheWeaponInfo pPed[%s] TaskIsOverridingProp[%d] GetTaskOverridingPropsWeapons[%d] GetMigrateTaskHelper[%d]",GetLogName(),(int)GetTaskOverridingProps(), (int)GetTaskOverridingPropsWeapons(), (int)(GetMigrateTaskHelper()!=NULL));

		return true;
	}

	if (pPed && pPed->IsAPlayerPed() && pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_SWAP_WEAPON, PED_TASK_PRIORITY_MAX, false))
	{
		weaponitemDebugf3("CNetObjPed::CacheWeaponInfo--TASK_SWAP_WEAPON");
		return true;
	}

	return false;
}


//
// name:        CNetObjPed::UpdateVehicleForPed
// description: update the ped's vehicle
//
void CNetObjPed::UpdateVehicleForPed(bool skipTaskChecks)
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if (!pPed)
		return;

	netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
	bool hidePedUnderMap = false;

    gnetAssert(IsClone());

    m_HiddenUnderMapDueToVehicle = false;

    if(m_targetVehicleId != NETWORK_INVALID_OBJECT_ID)
    {
		// ensure the collision state is correct for the ped
		if(!NetworkInterface::IsInMPCutscene())
		{
			if(!pPed->GetIsInVehicle() && !pPed->IsCollisionEnabled() && !pPed->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
			{
				SetOverridingLocalCollision(true, "CNetObjPed::UpdateVehicleForPed()");
			}
		}

		// don't place dead peds back in vehicles
		if (!skipTaskChecks && (pPed->GetIsDeadOrDying() || pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD)))
		{
			return;
		}

        if(!pPed->GetIsInVehicle() || (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetNetworkObject() && pPed->GetMyVehicle()->GetNetworkObject()->GetObjectID() != m_targetVehicleId))
        {
            netObject *targetVehicle = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetVehicleId);

            if(targetVehicle && IsVehicleObjectType(targetVehicle->GetObjectType()))
            {
                CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, targetVehicle);

                // we need to delay for 2 frames here as its possible for the vehicle to be updated
                // after this ped which will decrement the count in the same frame
                const unsigned FRAMES_TO_PREVENT_MIGRATION = 2;
                netObjVehicle->SetPreventCloneMigrationCountdown(FRAMES_TO_PREVENT_MIGRATION);

                NetworkLogUtils::WriteLogEvent(log, "SET_PREVENT_CLONE_MIGRATE_TIMER", netObjVehicle->GetLogName());
                log.WriteDataValue("Preventing Due To Ped", GetLogName());
            }
        }
    }
    else
	{
		if(!NetworkInterface::IsInMPCutscene())
        {
            if(pPed->GetIsInVehicle() && pPed->IsCollisionEnabled())
            {
                SetOverridingLocalCollision(false, "CNetObjPed::UpdateVehicleForPed()");
            }
        }
    }

    if (pPed->GetUsingRagdoll() || pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame) 
        return;

    // don't run this code while running get in/get out vehicle clone tasks
    if(!skipTaskChecks)
    {
		// when m_targetVehicleTimer is set, we don't update the vehicle until it times out. This is to stop this function briefly punting a ped
		// out of a car again after he has just got in, due to the target vehicle arriving late.
		if (m_targetVehicleTimer > 0)
		{
			u8 timeStep = static_cast<u8>(MIN(fwTimer::GetTimeStepInMilliseconds(), 255));
			m_targetVehicleTimer -= MIN(m_targetVehicleTimer, timeStep);
			return;
		}

        CTask *cloneTask = pPed->GetPedIntelligence()->GetTaskActive();

        while(cloneTask)
        {
			if (cloneTask->IsClonedFSMTask() && static_cast<CTaskFSMClone*>(cloneTask)->OverridesVehicleState())
			{
				return;
			}
  
            cloneTask = cloneTask->GetSubTask();
        }
    }

    if (pPed->GetIsInVehicle())
    {
        CVehicle *myVehicle = pPed->GetMyVehicle();
		netObject* pMyVehicleObj = myVehicle ? myVehicle->GetNetworkObject() : NULL;

        if (m_targetVehicleId == NETWORK_INVALID_OBJECT_ID)
        {
            // Don't put the ped out of the vehicle while the ped is running a ragdoll task remotely,
            // we want the clone task to be reponsible for putting the ped out of the vehicle so the
            // ragdoll velocity is calculated correctly
            if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
            {
				AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "Failing to set clone ped %s out of vehicle %s due to NM_CONTROL_TASK running", AILogging::GetDynamicEntityNameSafe(pPed), pMyVehicleObj ? pMyVehicleObj->GetLogName() : "??");
                return;
            }

            NetworkLogUtils::WriteLogEvent(log, "UPDATE_PED_VEHICLE", GetLogName());
            log.WriteDataValue("Update Reason", "Ped no longer in a vehicle");
			log.WriteDataValue("Setting out of", pMyVehicleObj ? pMyVehicleObj->GetLogName() : "??");

            // flushing the tasks puts the ped out of the vehicle and the allows us to
            // clear out the driving anims instantly with the following section of code
            m_bAllowSetTask = true;

			const bool bShouldSetPedOutAndClearAnims = !IsUsingRagdoll() || pPed->GetIsDeadOrDying() || !CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NUM, NULL, 0.0f) || !pPed->GetRagdollInst()->CheckCanActivate(pPed, false);
			log.WriteDataValue("Set Ped Out", "%s", bShouldSetPedOutAndClearAnims ? "TRUE" : "FALSE");
			if (bShouldSetPedOutAndClearAnims)
			{
				pPed->GetPedIntelligence()->FlushImmediately(false);
				pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
			}
            m_bAllowSetTask = false;

            if(bShouldSetPedOutAndClearAnims && !pPed->GetMovePed().GetTaskNetwork())
		    {
			    pPed->GetMovePed().SetTaskNetwork(NULL);
		    }

            pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
        }
        else if (m_targetVehicleSeat != -1)
        {
            bool restartMotionTask = false;

            if (pPed->GetMyVehicle()->GetNetworkObject()->GetObjectID() != m_targetVehicleId)
            {
                netObject* pVehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetVehicleId);
                CVehicle        *pVehicle   = NetworkUtils::GetVehicleFromNetworkObject(pVehicleObj);

                NetworkLogUtils::WriteLogEvent(log, "UPDATE_PED_VEHICLE", GetLogName());
                log.WriteDataValue("Update Reason", "Ped is in a different vehicle");
				log.WriteDataValue("Setting out of", pMyVehicleObj ? pMyVehicleObj->GetLogName() : "??");

                if (pVehicleObj && !pVehicle)
                {
                    gnetAssertf(0, "%s has a target vehicle %s which is not a vehicle!", GetLogName(), pVehicleObj->GetLogName());
                }

                SetClonePedOutOfVehicle(false, 0);

                if (pVehicleObj && pVehicle)
                {
                    CPed* pOccupier = pVehicle->GetSeatManager()->GetPedInSeat(m_targetVehicleSeat);

                    if (pVehicleObj->IsPendingOwnerChange())
                    {
                        log.WriteDataValue("Failed adding to vehicle", "Vehicle is pending owner change!");
                    }
                    else if (!pOccupier)
                    {
                        log.WriteDataValue("Setting into", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "??");
                        log.WriteDataValue("Seat", "%d", m_targetVehicleSeat);
                        SetClonePedInVehicle(pVehicle, m_targetVehicleSeat);
                    }
                    else if (pOccupier != pPed)
                    {
                        log.WriteDataValue("Failed adding to vehicle", "%s is occupying the seat!", pOccupier->GetNetworkObject() ? pOccupier->GetNetworkObject()->GetLogName() : "??");
                    }
                }
                else
                {
                    log.WriteDataValue("Failed adding to vehicle", "Vehicle does not exist!");
                }

                // need to restart the in vehicle motion task when we move from one vehicle to another
                restartMotionTask = true;
            }
            else if (pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed) != m_targetVehicleSeat)
            {
                netObject* pVehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetVehicleId);
                CVehicle        *pVehicle   = NetworkUtils::GetVehicleFromNetworkObject(pVehicleObj);

                NetworkLogUtils::WriteLogEvent(log, "UPDATE_PED_VEHICLE", GetLogName());
                log.WriteDataValue("Update Reason", "Ped is in a different seat");

				log.WriteDataValue("Setting out of ", pMyVehicleObj ? pMyVehicleObj->GetLogName() : "??");               
				log.WriteDataValue("Setting into", (pVehicle && pVehicle->GetNetworkObject()) ? pVehicle->GetNetworkObject()->GetLogName() : "??");
				log.WriteDataValue("Seat", "%d", m_targetVehicleSeat);
                SetClonePedOutOfVehicle(false, 0);

				if (pVehicle)
				{
					SetClonePedInVehicle(pVehicle, m_targetVehicleSeat);
				}

                // need to restart the in vehicle motion task when we move from one seat to another
                restartMotionTask = true;
            }


            // Restart the in vehicle state if we're in it (This will trigger a transition in the in vehicle state to another in vehicle state)
            // otherwise let the transition from on foot to in vehicle happen
            if(restartMotionTask)
            {
                CTaskMotionBase* pPrimaryTask = pPed->GetPrimaryMotionTask();

	            if (pPrimaryTask && pPrimaryTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
	            {
		            if (pPrimaryTask->GetState() == CTaskMotionPed::State_InVehicle)
		            {
			            static_cast<CTaskMotionPed*>(pPrimaryTask)->SetRestartCurrentStateThisFrame(true);
		            }
	            }
            }
        }

		// Re cache myvehicle because it could have changed in the previous if block
		myVehicle = pPed->GetMyVehicle();
		pMyVehicleObj = myVehicle ? myVehicle->GetNetworkObject() : NULL;

		if(myVehicle)
		{
			// if the vehicle the ped is in is invisible, hide the ped too (it looks daft otherwise)
			if(!myVehicle->GetIsVisible())
			{
				// don't hide peds involved in a cutscene
				if (!(NetworkInterface::IsInMPCutscene() && IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE)))
				{
					SetOverridingLocalVisibility(false, "Inside an invisible vehicle");
				}
			}

			// if the ped is in a seat where collision would normally remain on, disable it for clones
			// this stops the clone being knocked off a vehicle when the this has not happened on the owner machine
			const s32 iSeatIndex = myVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			const CVehicleSeatAnimInfo* pSeatAnimInfo = myVehicle->IsSeatIndexValid(iSeatIndex) ? myVehicle->GetSeatAnimationInfo(iSeatIndex) : NULL;

			if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
			{
				if(pPed->IsCollisionEnabled() || GetOverridingLocalCollision())
				{
					SetOverridingLocalCollision(false, "CNetObjPed::UpdateVehicleForPed() - GetKeepCollisionOnWhenInVehicle()");
				}
			}
		}
    }
    else if (m_targetVehicleId != NETWORK_INVALID_OBJECT_ID && m_targetVehicleSeat != -1)
    {
		if (m_myVehicleId != NETWORK_INVALID_OBJECT_ID)
		{
			netObject* pPedVehicleObj = pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetNetworkObject() : NULL;

			if (!pPedVehicleObj || pPedVehicleObj->GetObjectID() != m_myVehicleId)
			{
				netObject* pVehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_myVehicleId);

				if (pVehicleObj && gnetVerifyf(IsVehicleObjectType(pVehicleObj->GetObjectType()), "%s has a my vehicle pointer to %s!", GetLogName(), pVehicleObj->GetLogName()))
				{
					pPed->SetMyVehicle(SafeCast(CNetObjVehicle, pVehicleObj)->GetVehicle());
				}
			}
		}
		
		// ped should be in a vehicle
        if (skipTaskChecks || !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
        {
            netObject *pVehicleObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetVehicleId);
            CVehicle  *pVehicle    = NetworkUtils::GetVehicleFromNetworkObject(pVehicleObj);

            if (pVehicleObj && !pVehicle)
            {
                gnetAssertf(0, "%s has a target vehicle %s which is not a vehicle!", GetLogName(), pVehicleObj->GetLogName());
            }

            if (pVehicleObj && pVehicle)
            {
                NetworkLogUtils::WriteLogEvent(log, "UPDATE_PED_VEHICLE", GetLogName());
                log.WriteDataValue("Update Reason", "Ped is now in a vehicle");

                CPed* pOccupier = pVehicle->GetSeatManager()->GetPedInSeat(m_targetVehicleSeat);

                if (pVehicleObj->IsPendingOwnerChange())
                {
                    log.WriteDataValue("Failed adding to vehicle", "Vehicle is pending owner change!");
                }
                else if (!pOccupier)
                {
                    log.WriteDataValue("Setting into", pVehicleObj->GetLogName());
                    log.WriteDataValue("Seat", "%d", m_targetVehicleSeat);
                    SetClonePedInVehicle(pVehicle, m_targetVehicleSeat);
					SetAlpha(pVehicle->GetAlpha());
                }
                else if (pOccupier != pPed)
                {
                    log.WriteDataValue("Failed adding to vehicle", "%s is occupying the seat!", pOccupier->GetNetworkObject() ? pOccupier->GetNetworkObject()->GetLogName() : "??");
                }
            }
            else
            {
				if(!m_bWasHiddenForTutorial)
				{
					hidePedUnderMap = true;
				}
			}
        }
    }

	if(hidePedUnderMap)
	{
		SetOverridingLocalVisibility(false, "Hide Ped Under Map");
		SetOverridingLocalCollision(false, "Hide Ped Under Map");
		// some aspects of game interact with invisible objects so park under the map
		float fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION = -50.0f; // DO NOT GO UNDER -80 WITH THIS (WATER CHECK)
		Vector3 vPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vPosition.z = fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION;
		pPed->SetPosition(vPosition, true, true, true);

        m_HiddenUnderMapDueToVehicle = true;
	}
}

void CNetObjPed::UpdateMountForPed()
{
	//Grab the ped.
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	//Ensure the ped is valid.
	if(!pPed)
		return;

	//Grab the log.
	netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

    gnetAssert(IsClone());

	//Ensure the ped is not using a ragdoll.
    if(pPed->GetUsingRagdoll())
        return;

    // ensure the ped is not injured
    if(pPed->IsInjured())
    {
        return;
    }
        
    //Ensure the mount timer is not active.
	if(m_targetMountTimer > 0)
	{
		m_targetMountTimer--;
		return;
	}

    //Don't run this code during mount tasks.

#if ENABLE_HORSE
    CTask *cloneTask = pPed->GetPedIntelligence()->GetTaskActive();
    
    if(cloneTask)
    {
        switch(cloneTask->GetTaskType())
        {
        case CTaskTypes::TASK_MOUNT_ANIMAL:
        case CTaskTypes::TASK_DISMOUNT_ANIMAL:
            return;
        default:
            break;
        }
    }
#endif
    
    //Check if the ped is mounted.
    bool mounted = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount);
    
    //Check if the ped should be mounted.
    if(m_targetMountId != NETWORK_INVALID_OBJECT_ID)
    {
		//Set the ped off the mount, if necessary.
		if(mounted && pPed->GetMyMount() && pPed->GetMyMount()->GetNetworkObject()->GetObjectID() != m_targetMountId)
			SetClonePedOffMount();
		
		//Grab the mount.
		netObject* pMountObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_targetMountId);
		CPed*      pMount    = NetworkUtils::GetPedFromNetworkObject(pMountObj);

		NetworkLogUtils::WriteLogEvent(log, "UPDATE_PED_MOUNT", GetLogName());

		if (pMountObj && !pMount)
		{
			gnetAssertf(0, "%s has a target mount %s which is not a mount!", GetLogName(), pMountObj->GetLogName());
		}

		if (pMountObj && pMount)
		{
			CPed* pOccupier = pMount->GetSeatManager()->GetPedInSeat(0);

			if (pMountObj->IsPendingOwnerChange())
			{
				log.Log("\t\t** Failed: vehicle is pending owner change! **\r\n");
			}
			else if (!pOccupier)
			{
				SetClonePedOnMount(pMount); //
			}
			else if (pOccupier != pPed)
			{
				log.Log("\t\t** Failed: %s is occupying the seat! **\r\n", pOccupier->GetNetworkObject()->GetLogName());
			}
		}
		else
		{
			log.Log("\t\t** Failed: Mount does not exist **\r\n");
		}
    }
    else
    {
		//Set the ped off the mount, if necessary.
		if(mounted)
			SetClonePedOffMount();
    }
}

void CNetObjPed::CreateCloneTasksFromLocalTasks()
{
    CPed *pPed = GetPed();

    if(pPed)
    {
        // Now add our tasks, its important to do it in reverse order so the abort status is up-to-date.
        CTaskFSMClone *pNewCloneTask = 0;

        for( s32 i = 0; i < PED_TASK_PRIORITY_MAX  && !pNewCloneTask; i++ )
        {
            CTask *task = pPed->GetPedIntelligence()->GetTaskAtPriority(i);

            while(task && !pNewCloneTask)
            {
				if(task->IsClonedFSMTask())
				{
					CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(task);
					pNewCloneTask = taskFSMClone->CreateTaskForClonePed(pPed);

					if(pNewCloneTask)
					{
						pNewCloneTask->SetRunAsAClone(true);

						u32 sequenceID = taskFSMClone->GetNetSequenceID();

						if (m_ForceCloneTransitionChangeOwner && sequenceID==-1)
						{
							sequenceID = 0;
						}

						pNewCloneTask->SetNetSequenceID(sequenceID);
					}
				}

				task = task->GetSubTask();
            }

			//Go through and now set the MarkAbortable separately.  When we did this above a parent task might cleanup and cause problems for a subtask createtaskforcloneped that happened after the initial MarkAbortable.
			//Now, do the MarkAbortable after the CreateTaskForClonePed has happened.
			task = pPed->GetPedIntelligence()->GetTaskAtPriority(i);
            while(task)
            {
				if(task->IsClonedFSMTask() && (task != pNewCloneTask))
				{
					CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(task);

					if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
					{
						if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
						{
							gnetAssertf(0, "CTaskTreeClone::CreateCloneTasksFromLocalTasks: %s wouldn't abort immediately", TASKCLASSINFOMGR.GetTaskName(taskFSMClone->GetTaskType()));
						}
					}

				}

				task = task->GetSubTask();
            }
        }

        pPed->GetPedIntelligence()->GetTaskManager()->DeleteTree(PED_TASK_TREE_PRIMARY);
        pPed->GetPedIntelligence()->GetTaskManager()->SetTree(PED_TASK_TREE_PRIMARY, rage_new CTaskTreeClone(pPed, PED_TASK_PRIORITY_MAX));

        if(pNewCloneTask)
        {
             pPed->GetPedIntelligence()->AddTaskAtPriority(pNewCloneTask, PED_TASK_PRIORITY_PRIMARY, true);
        }
    }
}

void CNetObjPed::CreateLocalTasksFromCloneTasks()
{
    CPed *pPed = GetPed();

    if(pPed)
    {
        // see if we can create a local task from any of the clone tasks running on this ped
        aiTask* cloneTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
        CTask * newTask   = 0;
		bool hasLocallyRunningCloneTask = false;

		bool bPedIsDead	= pPed->IsFatallyInjured();

		CQueriableInterface *queriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();

        while(cloneTask && !newTask)
        {
            if(((CTask*)cloneTask)->IsClonedFSMTask())
            {
                CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(cloneTask);
                newTask = taskFSMClone->CreateTaskForLocalPed(pPed);

				if (newTask && newTask->IsClonedFSMTask())
				{
					u32 sequenceId = taskFSMClone->GetNetSequenceID();
					u32 priority = taskFSMClone->GetPriority();

					hasLocallyRunningCloneTask = taskFSMClone->IsRunningLocally();

					// the clone task will not have been given a sequence yet if given locally
					if (!hasLocallyRunningCloneTask)
					{
						static_cast<CTaskFSMClone*>(newTask)->SetNetSequenceID(sequenceId);
					}

					static_cast<CTaskFSMClone*>(newTask)->SetPriority(priority);
				}
            }

            cloneTask = cloneTask->GetSubTask();
        }

		cloneTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		while(cloneTask)
		{
			if(((CTask*)cloneTask)->IsClonedFSMTask() && (cloneTask != newTask))
			{
				CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(cloneTask);
				if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
				{
					if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
					{
						gnetAssertf(0, "CTaskTreeClone::CreateLocalTasksFromCloneTasks: %s wouldn't abort immediately", TASKCLASSINFOMGR.GetTaskName(taskFSMClone->GetTaskType()));
					}
				}
			}

			cloneTask = cloneTask->GetSubTask();
		}

		bool bPedRunningDeadDyingTask	= newTask && newTask->GetTaskType() == CTaskTypes::TASK_DYING_DEAD;

        SetupPedAIAsLocal();

		if (hasLocallyRunningCloneTask && !bPedIsDead)
		{
			pPed->GetPedIntelligence()->FlushImmediately(false, false, false);
			pPed->GetPedIntelligence()->AddTaskAtPriority(newTask, static_cast<CTaskFSMClone*>(newTask)->GetPriority());

			// ensure the ped still has a default task
			if(!pPed->GetPedIntelligence()->GetTaskDefault())
			{
				gnetAssertf(newTask || !bPedIsDead, "%s is dead but is being given a default task", GetLogName());

				// calculate and add the peds default task
				CTask *pDefaultTask = pPed->ComputeDefaultTask(*pPed);

				if(AssertVerify(pDefaultTask) && pDefaultTask->GetTaskType() == CTaskTypes::TASK_WANDER)
				{
					((CTaskWander *)pDefaultTask)->KeepMovingWhilstWaitingForFirstPath(pPed);
				}
				pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);
		    
		#if __BANK
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped re-creating default task after migration:", "%s %s", GetLogName(), pDefaultTask->GetTaskName());
		#endif
			}

			pPed->GetPedIntelligence()->BuildQueriableState();
			return;
		}

        // if none of the clone tasks have created a new local task, see if we can create a task from the
        // queriable interface
        if(newTask == 0)
        {
           newTask = queriableInterface->CreateNewTaskFromQueriableState(pPed);
        }
		else if (!bPedRunningDeadDyingTask)
		{
			// check if there is a parent task that needs creating (that might not run as a clone task)
			CTaskFSMClone *existingTask = static_cast<CTaskFSMClone*>(newTask);
			CTask		  *parentTask   = 0;

			CTaskInfo *taskInfo = queriableInterface->GetFirstTaskInfo();

			while(taskInfo && !parentTask)
			{
				if(taskInfo->GetTaskPriority() == existingTask->GetPriority())
				{
					if(taskInfo->GetTaskType() == existingTask->GetTaskType())
					{
						break;
					}
					else
					{
						parentTask = taskInfo->CreateLocalTask(pPed);
					}
				}
				else if (taskInfo->GetTaskType() == CTaskTypes::TASK_DYING_DEAD)
				{
					// found a dead dying task but the ped is not running it yet. Force it here.
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Found dying-dead task in queriable interface, forcing this task", "%s", GetLogName());
					parentTask = taskInfo->CreateLocalTask(pPed);
				}

				taskInfo = taskInfo->GetNextTaskInfo();
			}
		
			if(parentTask)
			{
				delete newTask;
				newTask = parentTask;
			}
		}

        CTaskInfo *taskInfo = 0;
        
        if(newTask && queriableInterface->IsTaskCurrentlyRunning(newTask->GetTaskType()))
        {
            taskInfo = queriableInterface->GetTaskInfoForTaskType(newTask->GetTaskType(), PED_TASK_PRIORITY_MAX);
        }

        s32  scriptCommand = queriableInterface->GetScriptedTaskCommand();
        u8   taskStage     = queriableInterface->GetScriptedTaskStage();
        u32  taskPriority  = taskInfo ? taskInfo->GetTaskPriority() : PED_TASK_PRIORITY_MAX;

        pPed->GetPedIntelligence()->FlushImmediately(false, false, false);

		bPedRunningDeadDyingTask = newTask && newTask->GetTaskType() == CTaskTypes::TASK_DYING_DEAD;

		// make sure the ped has a dying dead task if he is dead, or dead if he has a dying dead task
		if (bPedIsDead)
		{
			if (!bPedRunningDeadDyingTask)
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped is dead but no dying-dead task available, creating new task", "%s", GetLogName());

				aiTask* pNMControlTask = NULL;
				if (newTask)
				{
					// If the clone was running an NM behavior that continues after death then let's try to keep that going...
					// We do a similar thing in CEventDamage::CreateResponseTask for peds that are died and happened to already be running an NM task
					if (newTask->GetTaskType() == CTaskTypes::TASK_NM_CONTROL && static_cast<CTaskNMControl*>(newTask)->GetForcedSubTask() != NULL &&
						static_cast<const CTask*>(static_cast<CTaskNMControl*>(newTask)->GetForcedSubTask())->IsNMBehaviourTask() &&
						static_cast<const CTaskNMBehaviour*>(static_cast<CTaskNMControl*>(newTask)->GetForcedSubTask())->ShouldContinueAfterDeath())
					{
						pNMControlTask = newTask;
					}
					else
					{
						delete newTask;
					}
					newTask = NULL;
				}

				// force health to 0 (IsFatallyInjured() returns true for health slightly above 0)
				pPed->SetHealth(0.0f);

				newTask = rage_new CTaskDyingDead(NULL, 0, 0);
				static_cast<CTaskDyingDead*>(newTask)->SetForcedNaturalMotionTask(pNMControlTask);

				bPedRunningDeadDyingTask = true;
			}
		}
		else if (bPedRunningDeadDyingTask)
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Found dying-dead task in queriable interface but ped is not dead, forcing death", "%s", GetLogName());

			// forcibly kill the ped
			NET_ASSERTS_ONLY(f32 ret =)
			CWeaponDamage::GeneratePedDamageEvent(NULL, pPed, WEAPONTYPE_FALL, 1000.0f, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, CPedDamageCalculator::DF_None);

			if (!gnetVerifyf(pPed->IsFatallyInjured(), "GeneratePedDamageEvent failed to kill migrating dead ped (returned %f). Forcing health to 0.", ret))
			{
				pPed->SetHealth(0.0f);
			}

			delete newTask;
			newTask = NULL;
		}

		if(newTask)
        {
            bool requiresTaskInfoToMigrate = (!newTask->IsClonedFSMTask() || static_cast<CTaskFSMClone *>(newTask)->RequiresTaskInfoToMigrate()) && !bPedRunningDeadDyingTask;

            if (!requiresTaskInfoToMigrate && (taskPriority == PED_TASK_PRIORITY_MAX))
            {
                taskPriority = PED_TASK_PRIORITY_PRIMARY;
            }

            if(taskPriority == PED_TASK_PRIORITY_MAX)
            {
#if __BANK
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped could not get task info for new task:", "%s %s", GetLogName(), newTask->GetTaskName() );
				gnetAssertf(newTask->GetTaskType() != CTaskTypes::TASK_DYING_DEAD, "Couldn't find task info - about to lose dead dying task!");
#endif

				delete newTask;
				newTask = 0;
            }
            else
            {
                // if this task was started by a script we need to hook up the info here
                if(taskPriority == PED_TASK_PRIORITY_PRIMARY && scriptCommand != SCRIPT_TASK_INVALID)
                {
                    const int vacantSlot=CPedScriptedTaskRecord::GetVacantSlot();
                    if(gnetVerifyf(vacantSlot>=0, "Invalid vacantSlot: %d", vacantSlot))
					{
						CPedScriptedTaskRecord::ms_scriptedTasks[vacantSlot].Set(pPed, scriptCommand, newTask);
						queriableInterface->SetScriptedTaskStatus(scriptCommand, taskStage); 
					}
                }

                pPed->GetPedIntelligence()->AddTaskAtPriority(newTask, taskPriority);

				// inform the queriable interface that a new task has been generated to replace an existing clone task, so that it does not 
				// generate a new sequence number for this task.
				queriableInterface->HandleCloneToLocalTaskSwitch(*static_cast<CTaskFSMClone*>(newTask));
			}
        }

        // ensure the ped still has a default task
        if(!pPed->GetPedIntelligence()->GetTaskDefault())
        {
			gnetAssertf(newTask || !bPedIsDead, "%s is dead but is being given a default task", GetLogName());

            // calculate and add the peds default task
			CTask *pDefaultTask = pPed->ComputeDefaultTask(*pPed);

            if(AssertVerify(pDefaultTask) && pDefaultTask->GetTaskType() == CTaskTypes::TASK_WANDER)
            {
                ((CTaskWander *)pDefaultTask)->KeepMovingWhilstWaitingForFirstPath(pPed);
            }
            pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);
        
#if __BANK
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Ped re-creating default task after migration:", "%s %s", GetLogName(), pDefaultTask->GetTaskName());
#endif
		}

        // rewrite the queriable state with the rebuilt task tree
        pPed->GetPedIntelligence()->BuildQueriableState();

        gnetAssert(pPed->GetPedIntelligence()->GetTaskDefault() != 0);
    }
}

void CNetObjPed::HandleCloneTaskOwnerChange(eMigrationType migrationType)
{
    CPed *pPed = GetPed();

    if(pPed && gnetVerifyf(pPed->IsNetworkClone(), "Calling HandleCloneTaskOwnerChange() on a locally owned ped!"))
    {
        netPlayer *owner = GetPlayerOwner();

        // if we are still running any clone tasks that don't support control passing we must abort them now
        aiTask* cloneTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);

        while(cloneTask)
        {
            if(((CTask*)cloneTask)->IsClonedFSMTask())
            {
                CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone *>(cloneTask);

                if(!owner || !taskFSMClone->ControlPassingAllowed(pPed, *owner, migrationType))
                {
					gnetAssertf(taskFSMClone->GetTaskType() != CTaskTypes::TASK_DYING_DEAD, "CNetObjPed::HandleCloneTaskOwnerChange() - Aborting dead task!");
	
                    if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, NULL))
				    {
					    if (!taskFSMClone->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL))
					    {
						    gnetAssertf(0, "CTaskTreeClone::HandleCloneTaskOwnerChange: %s wouldn't abort immediately", TASKCLASSINFOMGR.GetTaskName(taskFSMClone->GetTaskType()));
					    }
				    }
                }
            }

            cloneTask = cloneTask->GetSubTask();
        }
    }
}

//
// name:        UpdateLocalTaskSlotCache
// description: This function updates a cache of currently running task on the ped once per frame.
//              Tasks that were running in the previous frame are kept in their existing slots to
//              simply the comparison of tasks in the synchronisation code.
//
void CNetObjPed::UpdateLocalTaskSlotCache()
{
    gnetAssertf(!IsClone(), "Attempting to update the local task slot cache for a cloned ped!");

    if(IsClone())
    {
        return;
    }

    //if(m_taskSlotCache.m_lastFrameUpdated < fwTimer::GetFrameCount())
    {
        CPed *ped = GetPed();
		VALIDATE_GAME_OBJECT(ped);

        CQueriableInterface *queriableInterface = ped ? ped->GetPedIntelligence()->GetQueriableInterface() : 0;

        if(queriableInterface)
        {
            // keep track of how many tasks at each priority level we have processed
            u32 tasksAtPriorityCount[PED_TASK_PRIORITY_MAX];

            for(u32 index = 0; index < PED_TASK_PRIORITY_MAX; index++)
            {
                tasksAtPriorityCount[index] = 0;
            }

            // build an array of the currently running tasks on this ped
            const unsigned MAX_CURRENT_TASKS = 32;
            CTaskInfo *currentTaskInfos[MAX_CURRENT_TASKS];
            bool       currentTaskInfoImportant[MAX_CURRENT_TASKS];
            u32 numTaskInfos = 0;

            CTaskInfo *taskInfo = queriableInterface->GetFirstTaskInfo();

            while(taskInfo && numTaskInfos < MAX_CURRENT_TASKS)
            {
                bool isTaskImportant = false;
                if(CQueriableInterface::IsTaskSentOverNetwork(taskInfo->GetTaskType(), &isTaskImportant) && taskInfo->IsNetworked())
                {
                    currentTaskInfos[numTaskInfos]         = taskInfo;
                    currentTaskInfoImportant[numTaskInfos] = isTaskImportant;
                    numTaskInfos++;

                    // update the task tree depth
                    taskInfo->SetTaskTreeDepth(tasksAtPriorityCount[taskInfo->GetTaskPriority()]);
                    tasksAtPriorityCount[taskInfo->GetTaskPriority()]++;
                }

                taskInfo = taskInfo->GetNextTaskInfo();
            }

            gnetAssertf(taskInfo==0, "MAX_CURRENT_TASKS needs increasing in CNetObjPed::UpdateLocalTaskSlotCache()!");

            // if we have too many task infos to sync we need to prioritise them, first
            // we ditch non-important tasks (i.e. tasks that are only used for AI queries,
            // rather than creating clone tasks or recreating tasks when a ped migrates). If
            // we still have too many task infos we ditch the temporary response tasks
            // with the lowest tree depths
            if(numTaskInfos > IPedNodeDataAccessor::NUM_TASK_SLOTS)
            {
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "TOO MANY SYNCED TASK INFOS!", GetLogName());
                u32 tasksToDump = numTaskInfos - IPedNodeDataAccessor::NUM_TASK_SLOTS;

                // dump the task infos used for AI querying first
                for(u32 index = 0; index < numTaskInfos && tasksToDump; index++)
                {
                    if(!currentTaskInfoImportant[index])
                    {
                        currentTaskInfos[index] = 0;
                        tasksToDump--;
                    }
                }

                // dump any extra task infos using response priorities next
                for(u32 priority = PED_TASK_PRIORITY_PHYSICAL_RESPONSE; priority < PED_TASK_PRIORITY_PRIMARY && tasksToDump; priority++)
                {
                    for(int index = (numTaskInfos - 1); index >= 0 && tasksToDump; index--)
                    {
                        if(currentTaskInfos[index] && currentTaskInfos[index]->GetTaskPriority() == static_cast<u32>(priority))
                        {
                            currentTaskInfos[index] = 0;
                            tasksToDump--;
                        }
                    }
                }

                // if we still have too many tasks log an error
                if(!gnetVerifyf(tasksToDump == 0, "More tasks sent over the network than task slots!"))
                {
#if __DEV
				    gnetDebug1("Dumping tasks:\n");

				    CTaskInfo *taskInfo = queriableInterface->GetFirstTaskInfo();

				    while(taskInfo)
				    {
                        bool important = false;
					    if(CQueriableInterface::IsTaskSentOverNetwork(taskInfo->GetTaskType(), &important))
					    {
                            gnetDebug1("\t%s, important: %s, priority: %d\n", TASKCLASSINFOMGR.GetTaskName(taskInfo->GetTaskType()), important ? "YES" : "NO", taskInfo->GetTaskPriority());
					    }

					    taskInfo = taskInfo->GetNextTaskInfo();
				    }
#endif // __DEV
                }
            }

            // update the task slot cache
            for(u32 cacheIndex = 0; cacheIndex < IPedNodeDataAccessor::NUM_TASK_SLOTS; cacheIndex++)
            {
                const u32 taskType			= m_taskSlotCache.m_taskSlots[cacheIndex].m_taskType;
                const u32 taskPriority		= m_taskSlotCache.m_taskSlots[cacheIndex].m_taskPriority;
				const u32 taskSequenceID	= m_taskSlotCache.m_taskSlots[cacheIndex].m_taskSequenceId;

                // tasks that are already in the cache are removed from the temporary array,
                // any remaining tasks in the array will be added into empty task slots
                bool taskExists = false;
                for(u32 newIndex = 0; newIndex < numTaskInfos && !taskExists; newIndex++)
                {
                    if(currentTaskInfos[newIndex] &&
                       currentTaskInfos[newIndex]->GetTaskType()     == static_cast<s32>(taskType) &&
                       currentTaskInfos[newIndex]->GetTaskPriority() == taskPriority &&
					   currentTaskInfos[newIndex]->GetNetSequenceID() == taskSequenceID) 
                    {
                        // this task may have moved down the hierarchy, so we must update the task tree depth. Similarly
                        // the active status of the task may have changed
                        m_taskSlotCache.m_taskSlots[cacheIndex].m_taskTreeDepth = static_cast<u8>(currentTaskInfos[newIndex]->GetTaskTreeDepth());
                        m_taskSlotCache.m_taskSlots[cacheIndex].m_taskActive    = currentTaskInfos[newIndex]->GetIsActiveTask();
                        currentTaskInfos[newIndex] = 0;
                        taskExists = true;
                    }
                }

                // tasks that no longer exist are removed from the cache
                if(!taskExists)
                {
                    m_taskSlotCache.m_taskSlots[cacheIndex].m_taskType = CTaskTypes::MAX_NUM_TASK_TYPES;
                }
            }

            // add any remaining new tasks into empty task slots
            for(u32 newIndex = 0; newIndex < numTaskInfos; newIndex++)
            {
                CTaskInfo *taskInfo = currentTaskInfos[newIndex];

                if(taskInfo)
                {
                    bool taskAdded = false;

                    for(u32 cacheIndex = 0; cacheIndex < IPedNodeDataAccessor::NUM_TASK_SLOTS && !taskAdded; cacheIndex++)
                    {
						IPedNodeDataAccessor::TaskSlotData &taskSlotInfo = m_taskSlotCache.m_taskSlots[cacheIndex];

                        const u32 taskType = taskSlotInfo.m_taskType;

                        if(taskType == CTaskTypes::MAX_NUM_TASK_TYPES)
                        {
                            taskSlotInfo.m_taskType			= static_cast<u16>(taskInfo->GetTaskType());
                            taskSlotInfo.m_taskPriority		= static_cast<u8>(taskInfo->GetTaskPriority());
							taskSlotInfo.m_taskTreeDepth	= static_cast<u8>(taskInfo->GetTaskTreeDepth());
							taskSlotInfo.m_taskSequenceId	= static_cast<u8>(taskInfo->GetNetSequenceID());
                            taskSlotInfo.m_taskActive		= taskInfo->GetIsActiveTask();

                            taskAdded = true;
                        }
                    }
                }
            }
        }

        //m_taskSlotCache.m_lastFrameUpdated = fwTimer::GetFrameCount();
    }
}

// name:        ResurrectClone
// description: Called on the clone when the main player has resurrected
void  CNetObjPed::ResurrectClone()
{
	respawnDebugf3("CNetObjPed::ResurrectClone");

	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	//Can only be called on the player
	if (ped && gnetVerify(ped->IsNetworkPlayer()))
	{
		//Reset all task sequences - they are not relevant anymore.
		CQueriableInterface* qi = ped ? ped->GetPedIntelligence()->GetQueriableInterface() : 0;
		if(qi)
		{
			qi->ResetTaskSequenceInfo();
		}

		//Reset all task slots - we cannot guarantee this data anymore.
		ResetTaskSlotData();

		//Reset damage dealt if we are tracking damage
		ResetDamageDealtByAllPlayers();
	}
}

void CNetObjPed::UpdateBlenderData()
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    if(ped && GetNetBlender())
    {
        bool isUsingInAirBlender = GetNetBlender()->GetBlenderData() == ms_pedBlenderDataInAir;

        if(ped->GetPedIntelligence()->NetworkUseInAirBlender() || (isUsingInAirBlender && IsClose(ped->GetGroundPos().z, PED_GROUNDPOS_RESET_Z)))
        {
            GetNetBlender()->SetBlenderData(ms_pedBlenderDataInAir);
        }
        else if(m_HiddenUnderMapDueToVehicle)
        {
            GetNetBlender()->SetBlenderData(ms_pedBlenderDataHiddenUnderMapForVehicle);
        }
        else
        {
		    CTaskMotionBase* pTask = ped->GetCurrentMotionTask();

		    if (pTask)
		    {
			    if(pTask->IsInWater())
			    {
				    GetNetBlender()->SetBlenderData(ms_pedBlenderDataInWater);
				    return;
			    }

                if(pTask->GetTaskType() == CTaskTypes::TASK_MOTION_TENNIS)
                {
                    GetNetBlender()->SetBlenderData(ms_pedBlenderDataTennis);
                    return;
                }
		    }

            GetNetBlender()->SetBlenderData(ms_pedBlenderDataOnFoot);
        }
    }
}

void CNetObjPed::UpdatePendingMovementData()
{
	// if we've not update the motion task tree for this ped yet, then just bail....
	if(GetForceMotionTaskUpdate())
	{
		return;
	}

	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(!ped)
		return;

	bool canChangeMoveBlenderState = false;

	CTaskMotionBase* task = ped->GetPrimaryMotionTask();
	if(task && (m_PendingMoveBlenderType != INVALID_PENDING_MOTION_TYPE) && (m_PendingMoveBlenderState != INVALID_PENDING_MOTION_STATE))
	{
		s32 currentTaskType = task->GetTaskType();

		if(currentTaskType != m_PendingMoveBlenderType)
		{
			// A ped can have a high lod motion task on one machine and a low lod 
			// motion task on the other (and vice versa) so we need to convert the
			// state from one task state enum to the other if necessary...
			if((currentTaskType == CTaskTypes::TASK_MOTION_PED) && (m_PendingMoveBlenderType == CTaskTypes::TASK_MOTION_PED_LOW_LOD))
			{
				m_PendingMoveBlenderState = ((CTaskMotionPed*)task)->ConvertMoveNetworkStateAndClipSetLod(m_PendingMoveBlenderState, m_pendingMotionSetId);
				m_PendingMoveBlenderType = currentTaskType;
			}
			else if((currentTaskType == CTaskTypes::TASK_MOTION_PED_LOW_LOD) && (m_PendingMoveBlenderType == CTaskTypes::TASK_MOTION_PED))
			{
				m_PendingMoveBlenderState = ((CTaskMotionPedLowLod*)task)->ConvertMoveNetworkStateAndClipSetLod(m_PendingMoveBlenderState, m_pendingMotionSetId);
				m_PendingMoveBlenderType = currentTaskType;
			}			
		}
	}

	switch((CTaskTypes::eTaskType)m_PendingMoveBlenderType)
	{
		case CTaskTypes::TASK_MOTION_PED: 			canChangeMoveBlenderState = UpdatePedPendingMovementData(); 		            break;
		case CTaskTypes::TASK_MOTION_PED_LOW_LOD:	canChangeMoveBlenderState = UpdatePedLowLodPendingMovementData();	            break;
	    case CTaskTypes::TASK_ON_FOOT_BIRD:         canChangeMoveBlenderState = UpdateBirdPendingMovementData(task->GetTaskType()); break;
		case CTaskTypes::TASK_ON_FOOT_HORSE:
		case CTaskTypes::TASK_MOTION_RIDE_HORSE:
		//	gnetAssertf(false, "%s : invalid task type - not supported in MP!", __FUNCTION__);						break;
		default:
			break;
	}

	NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("canChangeMoveBlenderState %d", canChangeMoveBlenderState);

	// some move blenders are only valid when a ped is inside or out of a vehicle
	// we can't set these move blenders until this state is synced up
	if(canChangeMoveBlenderState)
	{
		ped->RequestNetworkClonePedMotionTaskStateChange(m_PendingMoveBlenderState);

		if(m_pendingMotionSetId != CLIP_SET_ID_INVALID)
		{
			SelectCurrentClipSetForClone();
		}

		m_PendingMoveBlenderType  = INVALID_PENDING_MOTION_TYPE;
		m_PendingMoveBlenderState = INVALID_PENDING_MOTION_STATE;
		m_pendingMotionSetId      = CLIP_SET_ID_INVALID;
	}

	if (ped->GetPrimaryMotionTask()->GetOverrideWeaponClipSet() != m_pendingOverriddenWeaponSetId)
	{
		if (m_pendingOverriddenWeaponSetId != CLIP_SET_ID_INVALID)
		{
			gnetAssert(!m_overriddenWeaponSetClipSetRequestHelper.IsInvalid());

			if(m_overriddenWeaponSetClipSetRequestHelper.Request(m_pendingOverriddenWeaponSetId))
			{
				ped->GetPrimaryMotionTask()->SetOverrideWeaponClipSet(m_pendingOverriddenWeaponSetId);
			}
		}
		else
		{
			ped->GetPrimaryMotionTask()->ClearOverrideWeaponClipSet();
		}
	}

    if (ped->GetPrimaryMotionTask()->GetOverrideStrafingClipSet() != m_pendingOverriddenStrafeSetId)
    {
        if (m_pendingOverriddenStrafeSetId != CLIP_SET_ID_INVALID)
        {
            gnetAssert(!m_overriddenStrafeSetClipSetRequestHelper.IsInvalid());

            if(m_overriddenStrafeSetClipSetRequestHelper.Request(m_pendingOverriddenStrafeSetId))
            {
                ped->GetPrimaryMotionTask()->SetOverrideStrafingClipSet(m_pendingOverriddenStrafeSetId);
            }
        }
        else
        {
            ped->GetPrimaryMotionTask()->SetOverrideStrafingClipSet(m_pendingOverriddenStrafeSetId);
        }
    }
}

bool CNetObjPed::IsPendingMovementStateLocallyControlledByClone(void)
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(!ped)
		return false;

	if((m_PendingMoveBlenderType != INVALID_PENDING_MOTION_TYPE) && (m_PendingMoveBlenderState != INVALID_PENDING_MOTION_STATE))
	{
		switch((CTaskTypes::eTaskType)m_PendingMoveBlenderType)
		{
			case CTaskTypes::TASK_MOTION_PED: 	
			{
				if(ped->GetPrimaryMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
				{
					// Clone decides both when to enter and exit swimming...they get pushed towards the right choice by modifiers in the buoyancy code...
					if(m_PendingMoveBlenderState == CTaskMotionPed::State_Swimming)
					{
						// the clone is free to switch between diving and swimtodive and swimming though
						if((ped->GetPrimaryMotionTask()->GetState() == CTaskMotionPed::State_Diving) || (ped->GetPrimaryMotionTask()->GetState() == CTaskMotionPed::State_DiveToSwim))
						{
							return false;
						}

						return true;
					}
					else if(ped->GetPrimaryMotionTask()->GetState() == CTaskMotionPed::State_Swimming)
					{
						// if we're in swimming but the network is saying we should be in a vehicle, allow this
						if(m_PendingMoveBlenderState == CTaskMotionPed::State_InVehicle)
						{
							return false;
						}

						// the clone is free to switch between diving and swimtodive and swimming though
						if((m_PendingMoveBlenderState == CTaskMotionPed::State_DiveToSwim) || (m_PendingMoveBlenderState == CTaskMotionPed::State_Diving))
						{
							return false;
						}

						return true;
					}
					else if(ped->GetPrimaryMotionTask()->GetState() == CTaskMotionPed::State_DiveToSwim)
					{
						// invalid transition - block - forcing it breaks the move network...
						if(m_PendingMoveBlenderState == CTaskMotionPed::State_Diving)
						{
							return true;
						}
					}
				}
				else if(ped->GetPrimaryMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_TENNIS)
				{
					return false;
				}
			}
			break;
			case CTaskTypes::TASK_MOTION_PED_LOW_LOD:
				break;
			default:
				gnetAssert(false);
				break;
		}
	}

	return false;
}

bool CNetObjPed::UpdatePedPendingMovementData()
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : m_PendingMoveBlenderState = %d : m_pendingMotionSetId = %s", __FUNCTION__, m_PendingMoveBlenderState, m_pendingMotionSetId.GetCStr());

	if(!ped)
	{
		NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Invalid ped", __FUNCTION__)
		return false;
	}

	// if we've got a clip set that goes with the state, make sure it's loaded first...
	if( (m_pendingMotionSetId != CLIP_SET_ID_INVALID) && (!m_motionSetClipSetRequestHelper.Request(m_pendingMotionSetId)) )
	{
		NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Pending Clip Set Not Loaded! %s", __FUNCTION__, m_pendingMotionSetId.GetCStr())
		return false;
	}

	if (ped->GetPrimaryMotionTask() && ped->GetPrimaryMotionTask()->IsWaitingForTargetState())
	{
		NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Motion Task Waiting For Target State!", __FUNCTION__)
		return false;
	}

	// make sure we're not just trying to switch the clip set while staying in our current state 
	// (e.g. stay in OnFoot but change clipset from move_m@multiplayer to move_m@drunk@MODERATEDDRUNK)
	if((ped->GetPrimaryMotionTask() && ped->GetPrimaryMotionTask()->GetState() == m_PendingMoveBlenderState) && (m_pendingMotionSetId == CLIP_SET_ID_INVALID))
	{
		NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Already In Pending Motion State And No Clip Set To Set!", __FUNCTION__)
		return false;
	}

	gnetAssert((m_PendingMoveBlenderState == INVALID_PENDING_MOTION_STATE) || ((m_PendingMoveBlenderState >= CTaskMotionPed::State_Start) && (m_PendingMoveBlenderState <= CTaskMotionPed::State_Exit)));

	if((m_PendingMoveBlenderState != INVALID_PENDING_MOTION_STATE) && (ped->GetPrimaryMotionTask()->GetState() != m_PendingMoveBlenderState))
	{
		// if this is a blocked state that the clone selects itself
		if(IsPendingMovementStateLocallyControlledByClone())
		{
			NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Pending State Is Locally Controlled!", __FUNCTION__);
			return false;
		}

		bool pedOnFoot				= (ped->GetIsOnFoot());
		bool pedInVehicle			= (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)	&& ped->GetMyVehicle());
		
		switch(m_PendingMoveBlenderState)
		{
			// On foot related...must be on foot....
			case CTaskMotionPed::State_OnFoot:				
			case CTaskMotionPed::State_Strafing:			
			case CTaskMotionPed::State_DoNothing:			
			case CTaskMotionPed::State_AnimatedVelocity:	
			case CTaskMotionPed::State_Crouch:				
			case CTaskMotionPed::State_Stealth:				
			case CTaskMotionPed::State_ActionMode:			
#if ENABLE_DRUNK
			case CTaskMotionPed::State_Drunk:				
#endif // ENABLE_DRUNK
			case CTaskMotionPed::State_StrafeToOnFoot:		
				{
					NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : %s : PedOnFoot = %s", __FUNCTION__, pedOnFoot ? "SUCCESS" : "FAILED", pedOnFoot ? "true" : "false");
					return pedOnFoot;
				}
				break;
			// Transitional states...no anims so don't switch over...
			case CTaskMotionPed::State_SwimToRun:			
			case CTaskMotionPed::State_StandToCrouch:		
			case CTaskMotionPed::State_CrouchToStand:		
			case CTaskMotionPed::State_DiveToSwim:		
				{
					NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : FAILED : Transitional States, No Anims : Don't Switch Over!", __FUNCTION__);
					return false;
				}
				break;
			// Main states...has anims so definitely switch over...
			case CTaskMotionPed::State_Diving:				
			case CTaskMotionPed::State_Parachuting:			
			case CTaskMotionPed::State_Dead:				
				{
					NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : SUCCESS : Main state - Must switch over!", __FUNCTION__);
					return true;
				}
				break;
			case CTaskMotionPed::State_InVehicle:			return pedInVehicle;break;
			case CTaskMotionPed::State_Aiming:				return CTaskAimGun::ShouldUseAimingMotion(ped); break;
			// If we've got here, there's an exception to the clone deciding things itself and we should switch...
			case CTaskMotionPed::State_Swimming:			return true;	break;
			// Invalid states...
			case CTaskMotionPed::State_Start:				gnetAssertf(false, "%s : Invalid State?!", __FUNCTION__);									break;
			case CTaskMotionPed::State_Exit:				gnetAssertf(false, "%s : Invalid State?!", __FUNCTION__);									break;
			default:
				gnetAssertf(false, "CNetObjPed::UpdatePedPendingMovementData : Missing State? Has Someone added a new state to TaskMotionPed? %d, %s", m_PendingMoveBlenderState, CTaskMotionPed::GetStaticStateName(m_PendingMoveBlenderState)); break;
		}
	}

	// are we just trying to change a clip set over?
	if( (m_pendingMotionSetId != CLIP_SET_ID_INVALID) && (m_pendingMotionSetId != ped->GetPrimaryMotionTask()->GetActiveClipSet()) )
	{
		NETWORK_TASKMOTIONPED_DEBUG_TTY_OUTPUT("%s : SUCCESS : Pending Clip Set Only!", __FUNCTION__);
		return true;
	}

	return false;
}

bool CNetObjPed::UpdatePedLowLodPendingMovementData()
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(!ped)
		return false;

	if( (m_pendingMotionSetId != CLIP_SET_ID_INVALID) && (!m_motionSetClipSetRequestHelper.Request(m_pendingMotionSetId)) )
	{
		return false;
	}

	if (ped->GetPrimaryMotionTask() && ped->GetPrimaryMotionTask()->IsWaitingForTargetState())
	{
		return false;
	}

	// make sure we're not just trying to switch the clip set while staying in our current state 
	// (e.g. stay in OnFoot but change clipset from move_m@multiplayer to move_m@drunk@MODERATEDDRUNK)
	if((ped->GetPrimaryMotionTask() && ped->GetPrimaryMotionTask()->GetState() == m_PendingMoveBlenderState) && (m_pendingMotionSetId == CLIP_SET_ID_INVALID))
	{
		return false;
	}

	gnetAssert((m_PendingMoveBlenderState == INVALID_PENDING_MOTION_STATE) || ((m_PendingMoveBlenderState >= CTaskMotionPedLowLod::State_Start) && (m_PendingMoveBlenderState <= CTaskMotionPedLowLod::State_Exit)));
	
	if((m_PendingMoveBlenderState != INVALID_PENDING_MOTION_STATE) && (ped->GetPrimaryMotionTask()->GetState() != m_PendingMoveBlenderState))
	{
		bool pedInVehicle			= (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && ped->GetMyVehicle());

		if(m_PendingMoveBlenderState == CTaskMotionPedLowLod::State_InVehicle)
		{
			return pedInVehicle;
		}
	}

	// are we just trying to change a clip set over - is it to one we've set already?
	if( (m_pendingMotionSetId != CLIP_SET_ID_INVALID) && (m_pendingMotionSetId != ped->GetPrimaryMotionTask()->GetActiveClipSet()) )
	{
		return true;
	}

	return false;
}

bool CNetObjPed::UpdateBirdPendingMovementData(s32 primaryMotionTaskType)
{
	CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

	if(!ped)
		return false;

	if( (m_pendingMotionSetId != CLIP_SET_ID_INVALID) && (!m_motionSetClipSetRequestHelper.Request(m_pendingMotionSetId)) )
	{
		return false;
	}

	if (primaryMotionTaskType != CTaskTypes::TASK_ON_FOOT_BIRD)
	{
		return false;
	}

	return true;
}

// Apply the prop update to a ped - sometimes a prop can take a frame to be deleted to if we try and set all three
// props, there won't be a slot for them and we'll hit an assert - caching the prop update allows us to come back
// on subsequent frames and try again....
void CNetObjPed::UpdatePendingPropData()
{
	CPed* pPed = GetPed();

	if(!pPed || !pPed->IsNetworkClone())
	{
		return;
	}

	if(!pPed->GetPedModelInfo())
	{
		return;
	}

	// apply props if different
	if(m_pendingPropUpdate)
	{
		// Get out current set up....
		CPackedPedProps pedProps = CPedPropsMgr::GetPedPropsPacked(pPed);

		bool	blockHelmetUpdate	= false;
		u32		numPropsInUpdate	= 0;

		// AI peds never put on or remove a helmet, so we just blanket set...
		if(pPed->IsPlayer())
		{
			for(int index = 0; index < MAX_PROPS_PER_PED; index++)
			{
				// Get the number of props we have in our update as we may not be able to apply all of them this frame so it will have to wait.
				if(m_pendingPropUpdateData.GetAnchorID(index) != ANCHOR_NONE)
				{
					++numPropsInUpdate;
				}

				if((m_pendingPropUpdateData.GetAnchorID(index) == ANCHOR_HEAD) || (pedProps.GetAnchorID(index) == ANCHOR_HEAD))
				{
					// This means we can't update other props when putting a helmet on or off but the ped shouldn't be able to do anything anyway
					// if this becomes a problem, we can come back and implement a much longer and more tedious solution... (applying updates
					// on a prop by prop manner instead of blanket for looping through them all in clear and set)...
					if( IsOwnerAttachingAHelmet() || IsOwnerRemovingAHelmet() || IsAttachingAHelmet() || IsRemovingAHelmet() )
					{
						blockHelmetUpdate = true;
						break;
					}
				}
			}
		}

		if(!blockHelmetUpdate)
		{
			// Disable hair scale lerping
			pPed->SetHairScaleLerp(false);

			// Apply the prop update manually instead of using SetPedPackedProps because
			// if we have three props on the ped (helmet, watch, glasses) and we are 
			// trying to switch the helmet to a hat with the update, when SetPedPackedProps
			// clears out the props, it puts the helmet onto a delayedDelete blocking the slot.
			// This means we don't have enough slots to apply the three props in the update so
			// we get an assert. Not clearing them out first means SetPedProp recycles the slot.
			for(int index = 0; index < MAX_PROPS_PER_PED; index++)
            {
                CPedPropData::SinglePropData &propData = pPed->GetPedDrawHandler().GetPropData().GetPedPropData(index);

				eAnchorPoints previousAnchorID = propData.m_anchorID;

                propData.m_anchorID			= m_pendingPropUpdateData.GetAnchorID(index);
				propData.m_anchorOverrideID = m_pendingPropUpdateData.GetAnchorOverrideID(index);
                propData.m_propModelID		= m_pendingPropUpdateData.GetModelIdx(index, pPed);
                propData.m_propTexID		= m_pendingPropUpdateData.GetPropTextureIdx(index);

				if (propData.m_propModelID == PED_PROP_NONE)
				{
					if (propData.m_anchorID != ANCHOR_NONE)
						CPedPropsMgr::ClearPedProp(pPed, propData.m_anchorID);
					else if (previousAnchorID != ANCHOR_NONE)
						CPedPropsMgr::ClearPedProp(pPed, previousAnchorID);
				}
				else
				{
					CPedPropsMgr::SetPedProp(pPed, propData.m_anchorID, propData.m_propModelID, propData.m_propTexID, propData.m_anchorOverrideID);
				}
            }

			bool bHasHelmet = IsOwnerWearingAHelmet() && pPed->GetHelmetComponent() && pPed->GetHelmetComponent()->GetNetworkHelmetIndex() != -1;
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet, bHasHelmet);

			// Do a post camera anim update, necessary to fix-up high heels / hair
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);

			// set the flag so we're done updating...
			m_pendingPropUpdate = false;
		}
	}
}

void CNetObjPed::UpdateLocalRespawnFlagForPed()
{
	if (IsRespawnInProgress() && !IsRespawnUnacked() && !IsRespawnPendingCloneRemove())
	{
		SetLocalFlag(CNetObjGame::LOCALFLAG_RESPAWNPED, false);
		SetLocalFlag(CNetObjGame::LOCALFLAG_NOREASSIGN, false);
		m_RespawnFailedFlags      = 0;
		m_RespawnAcknowledgeFlags = 0;
	}
}

void CNetObjPed::ProcessLowLodPhysics()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// we can't activate low lod physics if we are locally overriding the collisions because this can screw up the collision state for peds as it conflicts
	// with the collision state being set by the low lod stuff.
	// if we're landing from parachuting we need to keep high lod so the landing physics are resolved correctly (switching to low lod phyiscs will mean the ped preserves whatever angle it hit the floor at).
	if (pPed && pPed->GetNavMeshTracker().GetIsValid() && !pPed->GetUsingRagdoll() && !GetOverridingLocalCollision() && !pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE))
	{
        bool pedInWater = IsNetworkObjectInWater() || pPed->GetIsInWater();

        if(!pedInWater && !pPed->IsDead() && !pPed->GetIsAttached())
        {
		    // allow low lod physics to be switched on
		    BANK_ONLY(if (NetworkDebug::ArePhysicsOptimisationsEnabled()))
		    {
                // wandering is not cloned and can use low LOD physics, other tasks are 
                // cloned that can set this in the task update
                if(m_Wandering)
                {
			        pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
                }
		    }

		    // force peds in low lod physics onto the navmesh (they have their gravity switched off)
		    if (pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
		    {
			    Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			    pedPos.z = pPed->GetNavMeshTracker().GetLastNavMeshIntersection().z + 1.0f;
			    pPed->SetPosition(pedPos);
		    }
        }
	}
}

// name:        CNetObjPed::ForceResendOfAllTaskData
// description: forces all task data to be sent in the next send to each player
void CNetObjPed::ForceResendOfAllTaskData()
{
	if (GetSyncData())
	{
		CPed *pPed = GetPed();

		if (pPed && !m_blockBuildQueriableState)
		{
			pPed->GetPedIntelligence()->BuildQueriableState();
		}

		UpdateLocalTaskSlotCacheForSync();

		static_cast<CPedSyncTree*>(GetSyncTree())->ForceResendOfAllTaskData(this);
		RequestForceSendOfTaskState();
	}
}

// name:        CNetObjPed::ForceResendAllData
// Description: forces a full resync of all nodes
void CNetObjPed::ForceResendAllData()
{
	UpdateLocalTaskSlotCacheForSync();

	if(GetSyncData())
	{
		GetSyncTree()->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this);
	}
}

void CNetObjPed::AddPendingScriptedTask(CTask *task)
{
    if(gnetVerifyf(task, "Trying to give a network ped an invalid scripted task!"))
    {
        m_CachedTaskToGive.Reset();
        m_CachedTaskToGive.m_TaskInfo               = task->CreateQueriableState();
        m_CachedTaskToGive.m_TaskType               = static_cast<u16>(task->GetTaskType());
        m_CachedTaskToGive.m_TaskUpdateTimer        = 0;
        m_CachedTaskToGive.m_WaitingForNetworkEvent = false;

        if(m_CachedTaskToGive.m_TaskInfo)
        {
            m_CachedTaskToGive.m_TaskInfo->SetType(m_CachedTaskToGive.m_TaskType);
        }

        delete task;

        UpdatePendingScriptedTask();
    }
}

void CNetObjPed::UpdatePendingScriptedTask()
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    if(m_CachedTaskToGive.m_TaskInfo)
    {
        if(!IsPendingOwnerChange())
        {
            if(IsClone())
            {
                gnetAssertf(IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) ||
                            IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT), "Giving a scripted task to an ambient ped!");

				if (m_CachedTaskToGive.m_TaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SEQUENCE)
				{
					CGivePedSequenceTaskEvent::Trigger(ped, m_CachedTaskToGive.m_TaskInfo);
				}
				else
				{
					CGivePedScriptedTaskEvent::Trigger(ped, m_CachedTaskToGive.m_TaskInfo);
				}
                m_CachedTaskToGive.m_TaskInfo               = 0;
                m_CachedTaskToGive.m_WaitingForNetworkEvent = true;
            }
            else
            {
                int    scriptedTaskType = m_CachedTaskToGive.m_TaskInfo->GetScriptedTaskType();
                CTask *task             = m_CachedTaskToGive.m_TaskInfo->CreateLocalTask(ped);

                if(task)
                {
                    GivePedScriptedTask(task, scriptedTaskType);
                }

                m_CachedTaskToGive.Reset();
            }
        }
    }
    else if(m_CachedTaskToGive.m_WaitingForNetworkEvent)
    {
        if(!CGivePedScriptedTaskEvent::AreEventsPending(GetObjectID()))
        {
            const unsigned WAIT_FOR_SCRIPTED_TASK_TIME = 1000;
            m_CachedTaskToGive.m_WaitingForNetworkEvent = false;
            m_CachedTaskToGive.m_TaskUpdateTimer        = WAIT_FOR_SCRIPTED_TASK_TIME;
        }
    }
    else if(m_CachedTaskToGive.m_TaskUpdateTimer > 0)
    {
        if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskPresentAtPriority(m_CachedTaskToGive.m_TaskType, PED_TASK_PRIORITY_PRIMARY))
        {
            m_CachedTaskToGive.m_TaskUpdateTimer = 0;
        }
        else
        {
            m_CachedTaskToGive.m_TaskUpdateTimer -= MIN(m_CachedTaskToGive.m_TaskUpdateTimer, static_cast<u16>(fwTimer::GetSystemTimeStepInMilliseconds()));
        }

        if(m_CachedTaskToGive.m_TaskUpdateTimer == 0)
        {
            m_CachedTaskToGive.Reset();
        }
    }
}

void CNetObjPed::GivePedScriptedTask(CTask *task, int scriptedTaskType, const char* DEV_ONLY(pszScriptName), int DEV_ONLY(iScriptThreadPC))
{
    CPed *ped = GetPed();
	VALIDATE_GAME_OBJECT(ped);

    bool taskAssigned = false;

    if(gnetVerifyf(task, "Giving a ped a NULL scripted task!") &&
       gnetVerifyf(!IsClone(), "Trying to give a clone ped a scripted task directly!") &&
       gnetVerifyf(!IsPendingOwnerChange(), "Trying to give a ped that is pending owner change a scripted task!"))
    {
        if(gnetVerifyf(scriptedTaskType != SCRIPT_TASK_INVALID, "Trying to give a ped a scripted task with an unsupported type!"))
        {
            if(task)
            {
                CEventScriptCommand event(PED_TASK_PRIORITY_PRIMARY, task);
#if __DEV
				event.SetScriptNameAndCounter(pszScriptName, iScriptThreadPC);
#endif
                ped->GetPedIntelligence()->RemoveEventsOfType( EVENT_SCRIPT_COMMAND );
	            CEvent *pEvent=ped->GetPedIntelligence()->AddEvent(event,true);

                taskAssigned = true;

                if(pEvent)
                {
                    const int vacantSlot=CPedScriptedTaskRecord::GetVacantSlot();
                    if(gnetVerifyf(vacantSlot >= 0, "Ran out of vacant slots for scripted tasks"))
                    {
#if __DEV
                        CPedScriptedTaskRecord::ms_scriptedTasks[vacantSlot].Set(ped, scriptedTaskType, (const CEventScriptCommand*)pEvent, "");
#else
                        CPedScriptedTaskRecord::ms_scriptedTasks[vacantSlot].Set(ped, scriptedTaskType, (const CEventScriptCommand*)pEvent);
#endif
                    }
                }
            }
        }
    }

    if(!taskAssigned)
    {
        delete task;
    }
}

int CNetObjPed::GetPendingScriptedTaskStatus() const
{
    int taskStatus = -1;

    if(m_CachedTaskToGive.m_TaskInfo ||
       m_CachedTaskToGive.m_WaitingForNetworkEvent ||
       m_CachedTaskToGive.m_TaskUpdateTimer > 0)
    {
        taskStatus = CPedScriptedTaskRecordData::EVENT_STAGE;
    }

    return taskStatus;
}

// reorders the peds task infos based on their tree depths
void CNetObjPed::ReorderTasksBasedOnTreeDepths()
{
    CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

    CQueriableInterface *pQueriableInterface = pPed ? pPed->GetPedIntelligence()->GetQueriableInterface() : 0;

    if(pQueriableInterface)
    {
        CTaskInfo *taskInfosToAdd[PED_TASK_PRIORITY_MAX][MAX_TREE_DEPTH];

        for(u32 index = 0; index < PED_TASK_PRIORITY_MAX; index++)
        {
            for(u32 index2 = 0; index2 < MAX_TREE_DEPTH; index2++)
            {
                taskInfosToAdd[index][index2] = 0;
            }
        }

        for(u32 index = 0; index < IPedNodeDataAccessor::NUM_TASK_SLOTS; index++)
        {
            u32 taskType		= m_taskSlotCache.m_taskSlots[index].m_taskType;
            u32 taskPriority	= m_taskSlotCache.m_taskSlots[index].m_taskPriority;
			u32 taskTreeDepth	= m_taskSlotCache.m_taskSlots[index].m_taskTreeDepth;

			if(taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
			{
				gnetAssert(taskTreeDepth <  MAX_TREE_DEPTH);

				if(taskTreeDepth < MAX_TREE_DEPTH)
				{
                    gnetAssert(taskPriority <  PED_TASK_PRIORITY_MAX);

                    if(taskPriority < PED_TASK_PRIORITY_MAX)
                    {
                        CTaskInfo *taskInfo = pQueriableInterface->GetTaskInfoForTaskType(taskType, taskPriority);

                        if(taskInfo)
                        {
                            gnetAssertf(taskInfosToAdd[taskPriority][taskTreeDepth] == 0, "%s is already has a task of priority %d, tree level %d!", GetLogName(), taskPriority, taskTreeDepth);
                            taskInfosToAdd[taskPriority][taskTreeDepth] = taskInfo;
                            pQueriableInterface->UnlinkTaskInfoFromNetwork(taskInfo);
                        }
                    }
                }
            }
        }

        pQueriableInterface->Reset();

        // finally add all of the rebuilt task list to the queriable state
        for(u32 index = 0; index < PED_TASK_PRIORITY_MAX; index++)
        {
            for(u32 index2 = 0; index2 < MAX_TREE_DEPTH; index2++)
            {
                if(taskInfosToAdd[index][index2])
                {
                    pQueriableInterface->AddTaskInfoFromNetwork(taskInfosToAdd[index][index2]);
                    gnetAssert(pQueriableInterface->GetTaskInfoForTaskType(taskInfosToAdd[index][index2]->GetTaskType(), taskInfosToAdd[index][index2]->GetTaskPriority()));
                }
            }
        }

		pQueriableInterface->UpdateCachedInfo();
    }
}

// name:		CNetObjEntity::IsLocalEntityCollidableOverNetwork
// description: Returns whether the entity collides on remote machines.
//
bool CNetObjPed::IsLocalEntityCollidableOverNetwork(void)
{
	bool collidableOverNetwork = CNetObjPhysical::IsLocalEntityCollidableOverNetwork();

	CPed* pPed = GetPed();

	if(pPed && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) )
	{
		collidableOverNetwork = true;
	}

	return collidableOverNetwork;
}

bool CNetObjPed::GetCachedEntityCollidableState(void)
{
	bool cachedState = CNetObjPhysical::GetCachedEntityCollidableState();

	if (!IsClone())
	{
		CPed* pPed = GetPed();

		if(pPed && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) )
		{
			cachedState = true;
		}
	}

	return cachedState;
}

// name:		CNetObjPed::UpdateNonPlayerCloneTaskTarget
// description: Updates a task target (used by TaskGun, TaskGetUp, TaskWrithe) or sets out of scope target ID....
//				Used to avoid duplicating this code in all of the tasks...

/* [static] */ void CNetObjPed::UpdateNonPlayerCloneTaskTarget(CPed const* ped, CAITarget& target, ObjectId& objectId)
{
	if(ped && ped->IsNetworkClone() && !ped->IsPlayer())
	{
		// if we're using an entity form of targetting or we've not been set yet (on construction in CreateCloneFSMTask with no valid target for example)
		if( (CAITarget::Mode_Entity == target.GetMode()) || (CAITarget::Mode_EntityAndOffset == target.GetMode()))
		{
			// if we've got a target that's out of scope
			if(!target.GetEntity())
			{
				if(objectId != NETWORK_INVALID_OBJECT_ID)
				{
					// Try and fix it up...
					netObject *networkObject = NetworkInterface::GetNetworkObject(objectId);
					if(networkObject)
					{
						CEntity* entity = networkObject->GetEntity();
						if(entity)
						{
							target.SetEntity(entity);
						}
					}
				}
			}
			else
			{
				netObject* networkObject = NetworkUtils::GetNetworkObjectFromEntity(target.GetEntity());
				if(networkObject)
				{
					objectId = networkObject->GetObjectID();
				}
			}
		}
	}
}

void CNetObjPed::GetPhoneModeData(u32& phoneMode)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	CTaskMobilePhone* pTask = NULL;

	phoneMode = 0;
	bool scriptObject = IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT);
	if (scriptObject)
	{
		pTask = static_cast<CTaskMobilePhone*>(pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim());
		if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE)
		{
			phoneMode = pTask->GetPhoneMode();
		}
	}
}

void CNetObjPed::SetPhoneModeData(const u32& phoneMode)
{
	bool scriptObject = IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT);

	if(m_PhoneMode != phoneMode && taskVerifyf(scriptObject,"%s only expect to sync phone mode on script peds GetGlobalFlags %x ", GetLogName(), GetGlobalFlags())  )
	{
		m_PhoneMode = phoneMode;
		m_bNewPhoneMode = true;
	}
}

void CNetObjPed::UpdatePhoneMode()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	bool scriptObject = IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT);

	if( !IsClone() )
	{
		m_PhoneMode = 0;
		m_bNewPhoneMode = false;
		return;
	}

	if(m_bNewPhoneMode && taskVerifyf(scriptObject,"%s  only expect to sync phone mode on script peds GetGlobalFlags %x ", GetLogName(), GetGlobalFlags()) )
	{
		m_bNewPhoneMode = false;

		CTaskMobilePhone* pTask = NULL;
		u32 uCurrentPhoneMode = 0;

		pTask = static_cast<CTaskMobilePhone*>(pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim());
		if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE)
		{
			uCurrentPhoneMode = pTask->GetPhoneMode();
		}

		if(m_PhoneMode!=0)
		{
			if(m_PhoneMode == uCurrentPhoneMode)
			{
				//current running a phone task and has the same mode
				return;
			}

			pTask = rage_new CTaskMobilePhone(m_PhoneMode, -1, true, true);

			if(pTask)
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations, false);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations, false);
				pPed->GetPedIntelligence()->AddTaskSecondary( pTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
			}
			else
			{
				gnetAssertf(0, "%s Failed to create CTaskMobilePhone with mode %d ", GetLogName(), m_PhoneMode);
			}
		}
		else
		{
			if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE)
			{
				pPed->GetPedIntelligence()->AddTaskSecondary(NULL, PED_TASK_SECONDARY_PARTIAL_ANIM);					
			}
		}
	}
}

void CNetObjPed::UpdateHelmet(void)
{	
	CPed* ped = GetPed();

	if(ped)
	{
		CTask* pTask = static_cast<CTaskTakeOffHelmet*>(ped->GetPedIntelligence()->GetTaskSecondaryPartialAnim());
		bool takingOffHelmet = (pTask && pTask->GetTaskType() == CTaskTypes::TASK_TAKE_OFF_HELMET);

		CTask* pExitTask = static_cast<CTaskExitVehicle*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));

		bool bCloneVisorStateOutOfSync = false;
		bool bCloneVisorUp = ped->GetHelmetComponent() ? ped->GetHelmetComponent()->GetIsVisorUp() : false;
		if ((m_nTargetVisorState == 0 && bCloneVisorUp) || (m_nTargetVisorState == 1 && !bCloneVisorUp))
		{
			bCloneVisorStateOutOfSync = true;
		}

		if((m_bIsOwnerRemovingAHelmet || bCloneVisorStateOutOfSync) && (!takingOffHelmet) && (!pExitTask))
		{
			// The take off helmet task currently inserts anims on the task slot, if the enter vehicle network has already been inserted we will remove it and
			// leave the task in a bad state, see B*2224412, should probably run the anims on the secondary slot
			const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			if (!pEnterVehicleTask || (pEnterVehicleTask->GetMoveNetworkHelper() && !pEnterVehicleTask->GetMoveNetworkHelper()->IsNetworkActive()))
			{
				CTaskTakeOffHelmet* takeOffTask = rage_new CTaskTakeOffHelmet();
				takeOffTask->SetIsVisorSwitching(m_bIsOwnerSwitchingVisor);
				if(takeOffTask)
				{
					ped->GetPedIntelligence()->AddTaskSecondary( takeOffTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
				}
			}
		}
	}
}

void CNetObjPed::GetHelmetComponentData(u8& textureId, u16& prop, u16& visorPropUp, u16& visorPropDown, bool& visorIsUp, bool& supportsVisor)
{
	CPed const* ped = GetPed();

	textureId	= 0;
	prop		= 0;
	visorPropDown = 0;
	visorPropUp	  = 0;
	visorIsUp = false;
	supportsVisor = false;


	// Convert from -1 - blah to 0 - blah to save bits....
	if(ped && ped->GetHelmetComponent())
	{
		u32 textureOverrideId = ped->GetHelmetComponent()->GetNetworkTextureIndex();
		textureId = static_cast<u8>(++textureOverrideId);

		//--

		u32 propIndex = ped->GetHelmetComponent()->GetNetworkHelmetIndex();
		prop = static_cast<u16>(++propIndex);

		u32 visorUpPropIndex = ped->GetHelmetComponent()->GetHelmetVisorUpPropIndex();
		visorPropUp = static_cast<u16>(++visorUpPropIndex);

		u32 visorDownPropIndex = ped->GetHelmetComponent()->GetHelmetVisorDownPropIndex();
		visorPropDown = static_cast<u16>(++visorDownPropIndex);

		visorIsUp = ped->GetHelmetComponent()->GetIsVisorUp();
		supportsVisor = ped->GetHelmetComponent()->GetCurrentHelmetSupportsVisors();
	}
}

void CNetObjPed::SetHelmetComponentData(u8 const textureId, u16 const propId, u16 const visorUpropId, u16 const visorDownPropId, const bool visorIsUp, const bool supportsVisor)
{
	CPed* ped = GetPed();

	if(ped && ped->GetHelmetComponent())
	{
		// convert from 0 - X back to -1 - X
		s32 texture = textureId;
		--texture;

		// The clone clear out the texture index itself when it has finished putting on / taking off a helmet.
		if(texture != -1)
		{
			// Force the helmet texture to whatever the texture is on the owner...
			ped->GetHelmetComponent()->SetNetworkTextureIndex(texture);
		}
	
		//--

		s32 prop = propId;
		--prop;

		// The clone clear out the prop index itself when it has finished putting on / taking off a helmet.
		if(prop != -1)
		{
			ped->GetHelmetComponent()->SetNetworkHelmetIndex(prop);
		}

		s32 visorUprop = visorUpropId;
		--visorUprop;

		// The clone clear out the prop index itself when it has finished putting on / taking off a helmet.
		if(visorUprop != -1)
		{
			ped->GetHelmetComponent()->SetHelmetVisorUpPropIndex(visorUprop);
		}

		s32 visorDownProp = visorDownPropId;
		--visorDownProp;

		// The clone clear out the prop index itself when it has finished putting on / taking off a helmet.
		if(visorDownProp != -1)
		{
			ped->GetHelmetComponent()->SetHelmetVisorDownPropIndex(visorDownProp);
		}

		ped->GetHelmetComponent()->SetCurrentHelmetSupportsVisors(supportsVisor);
		ped->GetHelmetComponent()->SetIsVisorUp(visorIsUp);
	}	
}

void CNetObjPed::GetHelmetAttachmentData(bool& isAttachingHelmet, bool& isRemovingHelmet, bool& isWearingHelmet, bool& isVisorSwitching, u8& targetVisorState)
{
	isAttachingHelmet = false;
	isRemovingHelmet  = false;
	isVisorSwitching  = false;
	targetVisorState = 0;

	CPed* ped = GetPed();
	if(ped && ped->GetPedIntelligence())
	{
		// attaching
		CTask* pAttaching = static_cast<CTaskMotionInAutomobile*>(ped->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		if(pAttaching && pAttaching->GetState() == CTaskMotionInAutomobile::State_PutOnHelmet)
		{
			isAttachingHelmet = true;
		}
		else
		{
			pAttaching = static_cast<CTaskMotionOnBicycle*>(ped->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE));
			if(pAttaching && pAttaching->GetState() == CTaskMotionOnBicycle::State_PutOnHelmet)
			{
				isAttachingHelmet = true;
			}
		}

		// detaching
		CTaskTakeOffHelmet* pTakeOffHelmetTask = static_cast<CTaskTakeOffHelmet*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_HELMET));
		if (pTakeOffHelmetTask)
		{
			isRemovingHelmet = true;
			isVisorSwitching = pTakeOffHelmetTask->GetIsVisorSwitching();
			
			u32 visorTargetState = pTakeOffHelmetTask->GetVisorTargetState();
			targetVisorState = static_cast<u8>(++visorTargetState);
		}

		// wearing
		isWearingHelmet = ped->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet);
	}
}

void CNetObjPed::SetHelmetAttachmentData(const bool& isAttachingHelmet, const bool& isRemovingHelmet, const bool& isWearingHelmet, const bool& isVisorSwitching, const u8& targetVisorState)
{
	gnetAssert((!isAttachingHelmet && !isRemovingHelmet) || (isAttachingHelmet != isRemovingHelmet));

	m_bIsOwnerAttachingAHelmet	= isAttachingHelmet;
	m_bIsOwnerRemovingAHelmet	= isRemovingHelmet;
	m_bIsOwnerWearingAHelmet	= isWearingHelmet;
	m_bIsOwnerSwitchingVisor	= isVisorSwitching;

	s32 nTargetState = targetVisorState;
	--nTargetState;
	m_nTargetVisorState = nTargetState;
}

bool CNetObjPed::IsAttachingAHelmet(void) const
{
	CPed* ped = GetPed();
	if(ped && ped->GetPedIntelligence())
	{
		CTask* pAttaching = static_cast<CTaskMotionInAutomobile*>(ped->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		if(pAttaching && pAttaching->GetState() == CTaskMotionInAutomobile::State_PutOnHelmet)
		{
			return true;
		}
	}

	return false;
}

bool CNetObjPed::IsRemovingAHelmet(void) const
{
	CPed* ped = GetPed();
	if(ped && ped->GetPedIntelligence())
	{
		CTask* pDetaching = static_cast<CTaskTakeOffHelmet*>(ped->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_TAKE_OFF_HELMET));
		if (pDetaching)
		{
			return true;
		}
	}

	return false;
}

void CNetObjPed::GetTennisMotionData(CSyncedTennisMotionData& tennisData)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	tennisData.Reset();

	CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
	if(pTask)
	{
		tennisData.m_bAllowOverrideCloneUpdate = pTask->GetAllowOverrideCloneUpdate();
		tennisData.m_bControlOutOfDeadZone	   = pTask->GetControlOutOfDeadZone();

		if(!tennisData.m_bAllowOverrideCloneUpdate)
		{
			if(pTask->IsNetSyncTennisSwingAnimDataValid())
			{
				tennisData.m_Active = true;
				pTask->GetNetSyncTennisSwingAnimData(tennisData.m_DictHash, tennisData.m_ClipHash, tennisData.m_fStartPhase, tennisData.m_fPlayRate, tennisData.m_bSlowBlend);
			}
			else if(pTask->IsNetSyncTennisDiveAnimDataValid())
			{
				tennisData.m_Active = true;
				tennisData.m_DiveMode = true;
				pTask->GetNetSyncTennisDiveAnimData(tennisData.m_DiveDirection,tennisData.m_fDiveHorizontal,tennisData.m_fDiveVertical,tennisData.m_fPlayRate, tennisData.m_bSlowBlend);
			}
		}
	}
}

void CNetObjPed::SetTennisMotionData(const CSyncedTennisMotionData& tennisData)
{
	VALIDATE_GAME_OBJECT(GetPed());

	if(tennisData.m_Active || tennisData.m_bAllowOverrideCloneUpdate || tennisData.m_bControlOutOfDeadZone )
	{
		if(m_pNetTennisMotionData)
		{
			m_pNetTennisMotionData->m_TennisMotionData = tennisData;
			m_pNetTennisMotionData->m_bNewTennisData = true;
		}
		else
		{
			SetNetTennisMotionData(tennisData);
			if(taskVerifyf(m_pNetTennisMotionData,"Not enough in pool"))
			{   //if created new then set this bool 
				m_pNetTennisMotionData->m_bNewTennisData = true;
			}
		}
	}
	else
	{
		if(m_pNetTennisMotionData)
		{
			ClearNetTennisMotionData();
		}
	}
}

void CNetObjPed::UpdateTennisMotionData()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if(!m_pNetTennisMotionData)
	{
		return;
	}
	
	if( !IsClone() )
	{
		ClearNetTennisMotionData();
		return;
	}

	if(m_pNetTennisMotionData->m_bNewTennisData)
	{
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask && pTask->GetState() > CTaskMotionTennis::State_Initial)
		{
			pTask->SetControlOutOfDeadZone(m_pNetTennisMotionData->m_TennisMotionData.m_bControlOutOfDeadZone);

			if( m_pNetTennisMotionData->m_TennisMotionData.m_bControlOutOfDeadZone && 
				!(m_pNetTennisMotionData->m_TennisMotionData.m_Active || m_pNetTennisMotionData->m_TennisMotionData.m_bAllowOverrideCloneUpdate))
			{
				//Just needed to set dead zone status so clear m_bNewTennisData flag and leave 
				m_pNetTennisMotionData->m_bNewTennisData = false;
				return;
			}

			if(m_pNetTennisMotionData->m_TennisMotionData.m_bAllowOverrideCloneUpdate)
			{
				gnetAssertf(!m_pNetTennisMotionData->m_TennisMotionData.m_Active,"Dont expect m_Active while overriding clone");

				pTask->SetAllowOverrideCloneUpdate(true);
				m_pNetTennisMotionData->m_bNewTennisData = false;
			}
			else if(taskVerifyf(m_pNetTennisMotionData->m_TennisMotionData.m_Active,"When m_bAllowOverrideCloneUpdate false expect m_Active true"))
			{
				pTask->SetAllowOverrideCloneUpdate(false);

				if(pTask->GetCloneAllowApplySwing())
				{   //No swing/dive currently underway, or it can be interrupted, so apply the waiting new data
					if(m_pNetTennisMotionData->m_TennisMotionData.m_DiveMode)
					{
						pTask->PlayDiveAnim(m_pNetTennisMotionData->m_TennisMotionData.m_DiveDirection,
							m_pNetTennisMotionData->m_TennisMotionData.m_fDiveHorizontal,
							m_pNetTennisMotionData->m_TennisMotionData.m_fDiveVertical, 
							m_pNetTennisMotionData->m_TennisMotionData.m_fPlayRate,
							m_pNetTennisMotionData->m_TennisMotionData.m_bSlowBlend);
					}
					else
					{
						pTask->PlayTennisSwingAnim( atHashWithStringNotFinal(m_pNetTennisMotionData->m_TennisMotionData.m_DictHash), 
							atHashWithStringNotFinal(m_pNetTennisMotionData->m_TennisMotionData.m_ClipHash), 
							m_pNetTennisMotionData->m_TennisMotionData.m_fStartPhase, 
							m_pNetTennisMotionData->m_TennisMotionData.m_fPlayRate,
							m_pNetTennisMotionData->m_TennisMotionData.m_bSlowBlend);
					}
					m_pNetTennisMotionData->m_bNewTennisData = false;
				}
			}
		}
	}
}

void CNetObjPed::SetNetTennisMotionData(const CSyncedTennisMotionData& tennisData)
{
	if (AssertVerify(!m_pNetTennisMotionData))
	{
		if (CNetTennisMotionData::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			m_pNetTennisMotionData = rage_new CNetTennisMotionData(tennisData);
		}
		else
		{
			gnetAssertf(0, "Ran out of CNetTennisMotionData");
		}
	}
}

void CNetObjPed::ClearNetTennisMotionData()
{
	if (AssertVerify(m_pNetTennisMotionData))
	{
		//When ever clearing the tennis sync data object make sure the dead zone is set back on the current tennis task if it's still running
		CPed *pPed = GetPed();
		VALIDATE_GAME_OBJECT(pPed);
		CTaskMotionTennis* pTask = (CTaskMotionTennis*)(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_TENNIS));
		if(pTask)
		{
			pTask->SetControlOutOfDeadZone(false);
		}

		delete m_pNetTennisMotionData;
		m_pNetTennisMotionData = 0;
	}
}

void CNetObjPed::OnPedClearDamage()
{
	m_ClearDamageCount++;
	if (m_ClearDamageCount > 3)
		m_ClearDamageCount = 0;
}

void CNetObjPed::TakeControlOfOrphanedMoveNetworkTaskSubNetwork( fwMoveNetworkPlayer* orphanMoveNetworkPlayer, float const blendOutDuration /* = NORMAL_BLEND_DURATION */, u32 const flags /* = 0 */ )
{
	if(orphanMoveNetworkPlayer)
	{
		// Before the 10 frames duration we allow an orphaned move network to live without a task is up ethier:
		// - it's being registered a second time (sometimes Task::CleanUp() gets called twice or 
		// - another task has already ended for this ped and is trying to orphan another task move sub network
		// So we just free the one that we've currently got stored as it's already been replaced by virtue 
		// of another one coming in to be orphaned...
		if((m_orphanMoveNetwork.GetNetworkPlayer()) && (orphanMoveNetworkPlayer != m_orphanMoveNetwork.GetNetworkPlayer()))
		{
			FreeOrphanedMoveNetworkTaskSubNetwork();
		}

		m_orphanMoveNetwork.SetNetworkPlayer(orphanMoveNetworkPlayer);
		m_orphanMoveNetwork.SetBlendDuration(blendOutDuration);
		m_orphanMoveNetwork.SetFlags(flags);
		m_orphanMoveNetwork.SetFrameNum(fwTimer::GetFrameCount());
	}
}

void CNetObjPed::HandleUnmanagedCloneRagdoll()
{
	// catch when the clone is doing an unmanaged ragdoll, and give him a local NM task to handle it. This is to prevent the current
	// clone task trying so start move networks, etc on the ped.
	CPed* pPed = GetPed();
	if (pPed && gnetVerifyf(pPed->IsNetworkClone(), "Calling HandleUnmanagedCloneRagdoll() on a locally owned ped!") && pPed->GetUsingRagdoll())
	{
		CTask *pCloneTask = pPed->GetPedIntelligence()->GetTaskActive();

		if ((!pCloneTask || !pCloneTask->HandlesRagdoll(pPed)) && pCloneTask->GetTaskType() != CTaskTypes::TASK_INCAPACITATED)
		{
			static const unsigned MAX_LOCAL_RAGDOLL_TIME = 1000; // ped will locally ragdoll for 1 sec and abort if no NM task update arrives

			CTaskNMHighFall* pTaskNM = rage_new CTaskNMHighFall(MAX_LOCAL_RAGDOLL_TIME, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE);
			pTaskNM->SetMaxTime(MAX_LOCAL_RAGDOLL_TIME);
			CTaskNMControl* pNMControlTask = rage_new CTaskNMControl(MAX_LOCAL_RAGDOLL_TIME, MAX_LOCAL_RAGDOLL_TIME, pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM);
			pPed->GetPedIntelligence()->AddLocalCloneTask(pNMControlTask, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
		}
	}
}

void CNetObjPed::ProcessFailmark()
{
#if !__NO_OUTPUT
	CPed* pPed = GetPed();
	if (pPed)
	{
		bool bGetIsVisible = pPed->GetIsVisible();
		bool bIsOnScreen = pPed->GetIsOnScreen();
		bool bFollowingThisPlayer = (CGameWorld::FindFollowPlayer() == pPed);
		bool bNotDamagedByAnything = pPed->m_nPhysicalFlags.bNotDamagedByAnything;
		bool bIsDead = pPed->IsDead();
		bool bIsInjured = pPed->IsInjured();
		bool bIsUsingRagdoll = IsUsingRagdoll();
		bool bGetUsingRagdoll = pPed->GetUsingRagdoll();

		bool bIsNetworkClone = pPed->IsNetworkClone();

		bool bIsNetBlendingOn = bIsNetworkClone && GetNetBlender() ? GetNetBlender()->IsBlendingOn() : false;
		bool bIsNetBlenderOverridden = bIsNetworkClone ? NetworkBlenderIsOverridden() : false;

		const Vector3& pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 ragdollPos;
		Vector3 animInstPos;
		if( pPed->GetRagdollInst())
		{
			ragdollPos = VEC3V_TO_VECTOR3(pPed->GetRagdollInst()->GetMatrix().d());
		}
		else
		{
			ragdollPos.Set(-999,-999,-999);
		}
		if(pPed->GetAnimatedInst())
		{
			animInstPos = VEC3V_TO_VECTOR3(pPed->GetAnimatedInst()->GetMatrix().d());
		}
		else
		{
			animInstPos.Set(-999,-999,-999);
		}

		const Vector3& vVelocity = pPed->GetVelocity();
		const Vector3& vDesiredVelocity = pPed->GetDesiredVelocity();

		bool bGetIsInInterior = pPed->GetIsInInterior();
		bool bPTIsInsideInterior = false;
		if (pPed->GetPortalTracker())
			bPTIsInsideInterior = pPed->GetPortalTracker()->IsInsideInterior();

		const char* currentRoomName = NULL;
		if (bPTIsInsideInterior)
		{
			currentRoomName = "BETA_ONLY";
#if __DEV
			currentRoomName = pPed->GetPortalTracker()->GetCurrRoomName();
#endif
		}

		const char* gamertag = pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner() ? pPed->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
		Displayf("-->pPed[%p] owner[%s] name[%s] model[%s] %s %s followingThisPlayer[%d] bNotDamagedByAnything[%d] IsDead[%d] IsInjured[%d] bIsUsingRagdoll[%d] bGetUsingRagdoll[%d]",
			pPed,
			gamertag,
			GetLogName(),
			pPed->GetModelName(),
			bIsNetworkClone ? "REMOTE" : "LOCAL",
			pPed->IsMale() ? "MALE" : "FEMALE",
			bFollowingThisPlayer,
			bNotDamagedByAnything,
			bIsDead,bIsInjured,
			bIsUsingRagdoll,bGetUsingRagdoll);

		Displayf("   bGetIsVisible[%d] bIsOnScreen[%d] alpha[%d]",
			bGetIsVisible,
			bIsOnScreen,
			pPed->GetAlpha());

		if (!bGetIsVisible)
		{
			pPed->ProcessFailmarkVisibility();
		}

		Displayf("   bIsNetBlendingOn[%d] bIsNetBlenderOverridden[%d]",
			bIsNetBlendingOn,
			bIsNetBlenderOverridden);

		Displayf("   pos [%f %f %f]",pos.x,pos.y,pos.z);
		Displayf("   Ragdoll Pos [%f %f %f]", ragdollPos.x, ragdollPos.y, ragdollPos.z);
		Displayf("   Phys Inst Pos [%f %f %f]", animInstPos.x, animInstPos.y, animInstPos.z);

		Displayf("   Entity UprightValue[%f] GetCurrentPitch[%f] GetDesiredPitch[%f] GetBoundPitch[%f]",pPed->GetTransform().GetC().GetZf(),pPed->GetCurrentPitch(),pPed->GetDesiredPitch(),pPed->GetBoundPitch());

		Displayf("   vMoveSpeed[%f %f %f][%f]", vVelocity.x, vVelocity.y, vVelocity.z, vVelocity.Mag());
		Displayf("   vDesiredVelocity[%f %f %f][%f]", vDesiredVelocity.x, vDesiredVelocity.y, vDesiredVelocity.z, vDesiredVelocity.Mag());

		Displayf("   bGetIsInInterior[%d] bPTIsInsideInterior[%d] currentRoomName[%s]",bGetIsInInterior,bPTIsInsideInterior,currentRoomName);

		Displayf("   IsCollisionEnabled[%d] GetNoCollisionFlags[0x%x] IsFixed[%d] IsFixedByNetwork[%d]",pPed->IsCollisionEnabled(),pPed->GetNoCollisionFlags(),pPed->GetIsFixedFlagSet(),pPed->GetIsFixedByNetworkFlagSet());

		if (pPed->GetIsAttached() && pPed->GetAttachParent())
		{
			const fwEntity* pAttachParent = pPed->GetAttachParent();
			const fwArchetype* pArchetype = pAttachParent->GetArchetype();
			Displayf("   IsAttached pAttachParent[%p] modelname[%s] type[%d] modelindex[%u] modelnamehash[%u]",pAttachParent,pAttachParent->GetModelName(),pAttachParent->GetType(),pAttachParent->GetModelIndex(),pArchetype ? pArchetype->GetModelNameHash() : 0);
		}

		Displayf("   getisswimming[%d] getisinwater[%d] isonground[%d] isjumping[%d] isinair[%d] isfalling[%d]",pPed->GetIsSwimming(),pPed->GetIsInWater(),pPed->IsOnGround(),pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping),pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir),pPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling));

		Displayf("   health[%f] armour[%f]",pPed->GetHealth(),pPed->GetArmour());

		if (pPed && pPed->GetPedIntelligence())
		{
			u8 combatMovement = (u8) pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement();
			Displayf("   combatmovement[%d]",combatMovement);
		}

		CWanted* pWanted = pPed->GetPlayerWanted();
		if (pWanted && (pWanted->GetWantedLevel() > 0))
		{
#if __BANK
			if(pWanted->GetWantedLevelLastSetFrom() == WL_FROM_SCRIPT)
			{
				Displayf("   wanted[%d] wantedlevellastsetfrom[%d][%s] wantedlevellastsetfromscriptname[%s]",pWanted->GetWantedLevel(),pWanted->GetWantedLevelLastSetFrom(),sm_WantedLevelSetFromLocations[pWanted->GetWantedLevelLastSetFrom()],pWanted->m_WantedLevelLastSetFromScriptName);
			}
			else
			{
				Displayf("   wanted[%d] wantedlevellastsetfrom[%d][%s]",pWanted->GetWantedLevel(),pWanted->GetWantedLevelLastSetFrom(),sm_WantedLevelSetFromLocations[pWanted->GetWantedLevelLastSetFrom()]);
			}
#else
			Displayf("   wanted[%d] wantedlevellastsetfrom[%d]",pWanted->GetWantedLevel(),pWanted->GetWantedLevelLastSetFrom());
#endif
		}

		if (pPed->IsPlayer())
		{
			CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(this);
			if (pNetObjPlayer)
			{
				Displayf("   IsFriendlyFireAllowed[%d] CNetwork::IsFriendlyFireAllowed[%d] IsPassiveMode[%d]",pNetObjPlayer->IsFriendlyFireAllowed(),CNetwork::IsFriendlyFireAllowed(),pNetObjPlayer->IsPassiveMode());

				u8 fakeWantedLevel = pNetObjPlayer->GetRemoteFakeWantedLevel();
				if (fakeWantedLevel > 0)
				{
					Displayf("   fakewantedlevel[%u]",fakeWantedLevel);
				}
			}
		}

		Displayf("   parachute tint[%d] packtintindex[%d]",pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetTintIndexForParachute() : -1,pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetTintIndexForParachutePack() : -1);

		ProcessFailmarkHeadBlend();

		ProcessFailmarkDecorations();

		ProcessFailmarkVariations();

		ProcessFailmarkColors();

#if !__FINAL
		// Print all active tasks
		CTask* pActiveTask=pPed->GetPedIntelligence()->GetTaskActive();
		if(pActiveTask)
		{
			CTask* pTaskToPrint=pActiveTask;
			while(pTaskToPrint)
			{
				gnetAssertf(pTaskToPrint->GetName(), "Found a task without a name!");

				Displayf("   Active:%s", (const char*) pTaskToPrint->GetName());

				pTaskToPrint=pTaskToPrint->GetSubTask();
			}
		}
#endif // !__FINAL

		CVehicle* pVehicle = pPed ? pPed->GetVehiclePedInside() : 0;
		if (pVehicle)
		{
			Displayf("   VEHICLE: GetVehiclePedInside pVehicle[%p] model[%s] name[%s] isnetworkclone[%d]",pVehicle,pVehicle->GetModelName(),pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "",pVehicle->IsNetworkClone());
		}

		ProcessFailmarkInventory();
	}
#endif // !__NO_OUTPUT
}

void CNetObjPed::ProcessFailmarkInventory()
{
#if !__NO_OUTPUT
	CPed* pPed = GetPed();
	if (!pPed)
		return;

	Displayf("   INVENTORY:");

	Displayf("     m_weaponObjectExists[%d] m_weaponObjectTintIndex[%u] CacheWeaponInfo[%d] TaskIsOverridingProp[%d] GetTaskOverridingPropsWeapons[%d] GetMigrateTaskHelper[%p]",m_weaponObjectExists,m_weaponObjectTintIndex,CacheWeaponInfo(),TaskIsOverridingProp(),GetTaskOverridingPropsWeapons(),GetMigrateTaskHelper());

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon)
	{
		Displayf("     pWeapon[%p][%s][%s] hash[%u] GetIsVisible[%d] GetAlpha[%d] GetAlphaOverride[%d] GetTintIndex[%d] GetWeaponItemTintIndex[%d] GetAmmoInClip[%d] GetAmmoTotal[%d] GetState[%d]",
			pWeapon,
			pWeapon && pWeapon->GetWeaponInfo() ? pWeapon->GetWeaponInfo()->GetModelName() : "",
			pWeapon && pWeapon->GetWeaponInfo() ? pWeapon->GetWeaponInfo()->GetName() : "",
			pWeapon ? pWeapon->GetWeaponHash() : -9,
			pWeapon && pWeapon->GetEntity() ? pWeapon->GetEntity()->GetIsVisible() : - 9,
			pWeapon && pWeapon->GetEntity() ? pWeapon->GetEntity()->GetAlpha() : -9,
			pWeapon && pWeapon->GetEntity() ? CPostScan::GetAlphaOverride(const_cast<CEntity *>(pWeapon->GetEntity())) : -9,
			pWeapon ? pWeapon->GetTintIndex() : -9,
			pWeapon && pPed->GetInventory() && pPed->GetInventory()->GetWeapon(pWeapon->GetWeaponHash()) ? pPed->GetInventory()->GetWeapon(pWeapon->GetWeaponHash())->GetTintIndex() : -9,
			pWeapon ? pWeapon->GetAmmoInClip() : -9,
			pWeapon ? pWeapon->GetAmmoTotal() : -9,
			pWeapon ? pWeapon->GetState() : -9);

		Displayf("       GetAttachParent[0x%p] GetAttachBone[%d] GetAttachState[%s] GetAttachOffset[%f %f %f]",
			pWeapon && pWeapon->GetEntity() ? pWeapon->GetEntity()->GetAttachParent() : 0,
			pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetAttachmentExtension() ? pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachBone() : -9,
#if __BANK
			pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetAttachmentExtension() ? pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachStateString(pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachState()) : "",
#else
			"",
#endif
			pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetAttachmentExtension() ? pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachOffset().x : -9,
			pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetAttachmentExtension() ? pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachOffset().y : -9,
			pWeapon && pWeapon->GetEntity() && pWeapon->GetEntity()->GetAttachmentExtension() ? pWeapon->GetEntity()->GetAttachmentExtension()->GetAttachOffset().z : -9);

		int count = pWeapon->GetComponents().GetCount();
		if (count > 0)
		{
			for(int i=0; i < count; i++)
			{
				if (pWeapon->GetComponents()[i] && pWeapon->GetComponents()[i]->GetInfo())
                {
#if !__FINAL
					Displayf("       Component[%d] [%s] [0x%x]",i,pWeapon->GetComponents()[i]->GetInfo()->GetName(),pWeapon->GetComponents()[i]->GetInfo()->GetHash());
#else
                    Displayf("       Component[%d] [0x%x]",i,pWeapon->GetComponents()[i]->GetInfo()->GetHash());
#endif
                }
            }
		}
	}
#endif
}

void CNetObjPed::ProcessFailmarkHeadBlend()
{
#if !__NO_OUTPUT

	CPed* pPed = GetPed();
	if (!pPed)
		return;

	Displayf("   headblend:");

	CPedHeadBlendData *pedHeadBlendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

	if(!pedHeadBlendData)
	{
		Displayf("     no head blend data");
	}
	else
	{
		Displayf("     headblend[%f] texBlend[%f] varblend[%f]",pedHeadBlendData->m_headBlend,pedHeadBlendData->m_texBlend,pedHeadBlendData->m_varBlend);
		Displayf("     head0[%u] head1[%u] head2[%u]",pedHeadBlendData->m_head0,pedHeadBlendData->m_head1,pedHeadBlendData->m_head2);
		Displayf("     tex0[%u] tex1[%u] tex2[%u]",pedHeadBlendData->m_tex0,pedHeadBlendData->m_tex1,pedHeadBlendData->m_tex2);
		Displayf("     eyecolor[%d]",pedHeadBlendData->m_eyeColor);
		Displayf("     hairTintIndex[%u] m_hairTintIndex2[%u]",pedHeadBlendData->m_hairTintIndex,pedHeadBlendData->m_hairTintIndex2);

		for (s32 i = 0; i < HOS_MAX; ++i)
		{
			Displayf("     i[%d] overlayTex[%u] overlayAlpha[%f] overlayNormAlpha[%f] overlayTintIndex[%u] overlayTintIndex2[%u]",
				i,
				pedHeadBlendData->m_overlayTex[i],
				pedHeadBlendData->m_overlayAlpha[i],
				pedHeadBlendData->m_overlayNormAlpha[i],
				pedHeadBlendData->m_overlayTintIndex[i],
				pedHeadBlendData->m_overlayTintIndex2[i]);
		}

		for (s32 i = 0; i < MMT_MAX; ++i)
		{
			if (pedHeadBlendData->m_microMorphBlends[i] != 0.f)
				Displayf("     i[%d] micromorph[%s][%f]",i,s_microMorphAssets[i],pedHeadBlendData->m_microMorphBlends[i]);
		}

		if (pedHeadBlendData->m_usePaletteRgb)
		{
			for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
			{
				Displayf("     i[%d] red[%u] green[%u] blue[%u]",i,pedHeadBlendData->m_paletteRed[i],pedHeadBlendData->m_paletteGreen[i],pedHeadBlendData->m_paletteBlue[i]);
			}
		}

		if (pedHeadBlendData->m_hasParents)
		{
			Displayf("     head0parent0[%u] head0parent1[%u] parentblend[%f] parentblend2[%f]",pedHeadBlendData->m_extraParentData.head0Parent0,pedHeadBlendData->m_extraParentData.head0Parent1,pedHeadBlendData->m_extraParentData.parentBlend,pedHeadBlendData->m_parentBlend2);
		}

	}
#endif
}

void CNetObjPed::ProcessFailmarkDecorations()
{
#if !__NO_OUTPUT
	CPed* pPed = GetPed();
	if (!pPed)
		return;
	
	PEDDAMAGEMANAGER.DumpDecorations(pPed);
#endif
}

void CNetObjPed::ProcessFailmarkVariations()
{
#if !__NO_OUTPUT
	CPed* pPed = GetPed();
	if (!pPed)
		return;

	CPedVariationData&	varData = pPed->GetPedDrawHandler().GetVarData();

	bool bFoundData = false;

	for (int i=0; i<PV_MAX_COMP; i++)
	{
		u32 textureData	= (u32)varData.GetPedTexIdx(static_cast<ePedVarComp>(i));
		u32 paletteData	= (u32)varData.GetPedPaletteIdx(static_cast<ePedVarComp>(i));

		u32 varInfoHash = 0;
		u32 componentData = 0;

		CPedVariationPack::GetLocalCompData(pPed, (ePedVarComp)i, varData.GetPedCompIdx(static_cast<ePedVarComp>(i)), varInfoHash, componentData);

		bool hasComponent = componentData != 0 && componentData != 255;
		bool hasTexture = textureData != 0 && textureData != 255;
		bool hasPalette = paletteData != 0 && paletteData != 255;
		bool hasVarInfoHash = varInfoHash != 0;

		if (hasComponent || hasTexture || hasPalette || hasVarInfoHash)
		{
			if (!bFoundData)
				Displayf("   variations:");

			Displayf("     slot[%d][%s] component[%d] texture[%d] palette[%d] varinfohash[0x%x]"
				,i
				,varSlotNames[i]
				,componentData
				,textureData
				,paletteData
				,varInfoHash);

			bFoundData = true;
		}
	}
#endif
}

void CNetObjPed::ProcessFailmarkColors()
{
#if !__NO_OUTPUT
	CPed* pPed = GetPed();
	if (!pPed)
		return;

	bool bHasColors = false;

	u8 r[MAX_PALETTE_COLORS];
	u8 g[MAX_PALETTE_COLORS];
	u8 b[MAX_PALETTE_COLORS];

	for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
	{
		r[i] = 255; 
		g[i] = 255;
		b[i] = 255;

		pPed->GetHeadBlendPaletteColor(r[i], g[i], b[i], i);

		if (!bHasColors)
		{
			if (r[i] != 255 || g[i] != 255 || b[i] != 255)
				bHasColors = true;
		}
	}

	if (bHasColors)
	{
		Displayf("   colors:");
		Displayf("     0_rgb[%d %d %d] 1_rgb[%d %d %d] 2_rgb[%d %d %d] 3_rgb[%d %d %d]",r[0],g[0],b[0],r[1],g[1],b[1],r[2],g[2],b[2],r[3],g[3],b[3]);
	}

	if (pPed->IsPlayer())
	{
		const CNetGamePlayer* pPlayer = GetPlayerOwner();
		if (pPlayer)
		{
			const rlClanDesc& curClanDesc = pPlayer->GetClanDesc();
			if (curClanDesc.IsValid())
			{
				Color32 clanColor(curClanDesc.m_clanColor);
				u8 cr = clanColor.GetRed();
				u8 cg = clanColor.GetGreen();
				u8 cb = clanColor.GetBlue();

				Displayf("   crew color:");
				Displayf("     rgb[%d %d %d] crew[%s]",cr,cg,cb,curClanDesc.m_ClanName);
			}
		}
	}
#endif
}

void CNetObjPed::OnAIEventAdded(const fwEvent& event)
{
    switch(event.GetEventType())
    {
    case EVENT_SHOT_FIRED_WHIZZED_BY:
        OnGunShotWhizzedByEvent(static_cast<const CEventGunShotWhizzedBy &>(event));
        break;
    case EVENT_SHOT_FIRED_BULLET_IMPACT:
        OnGunShotBulletImpactEvent(static_cast<const CEventGunShotBulletImpact &>(event));
        break;
    default:
        break;
    }
}

void CNetObjPed::OnGunShotBulletImpactEvent(const CEventGunShotBulletImpact &aiEvent)
{
	weaponDebugf3("CNetObjPed::OnGunShotBulletImpactEvent IsSilent[%d]",aiEvent.IsSilent());
    if(aiEvent.IsSilent())
    {
        for(unsigned index = 0; index < ms_NumPedsNotRespondingToCloneBullets; index++)
        {
            ObjectId   objectID      = ms_PedsNotRespondingToCloneBullets[index];
            netObject *networkObject = NetworkInterface::GetNetworkObject(objectID);

            if(networkObject && networkObject->IsClone())
            {
                CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

                if(ped)
                {
                    float testDistance = ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatBulletImpactDetectionRange);
                          testDistance = rage::Min(testDistance, CMiniMap::sm_Tunables.Sonar.fSoundRange_SilencedGunshot);

                    Vector3 deltaPos = aiEvent.GetShotTarget() - VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
                    float   distSq   = deltaPos.Mag2();
                    
                    if(distSq < rage::square(testDistance))
                    {
                        if(aiEvent.GetSourceEntity() == FindPlayerPed())
                        {
							weaponDebugf3("CNetObjPed::OnGunShotBulletImpactEvent invoke CInformSilencedGunShotEvent::Trigger");

							CInformSilencedGunShotEvent::Trigger(ped, static_cast<CPed *>(aiEvent.GetSourceEntity()), aiEvent.GetSourcePos(), aiEvent.GetShotTarget(),
                                                                      aiEvent.GetWeaponHash(), aiEvent.MustBeSeenWhenInVehicle());
                        }
                    }
                }
            }
        }
    }
}

void CNetObjPed::OnGunShotWhizzedByEvent(const CEventGunShotWhizzedBy &aiEvent)
{
	weaponDebugf3("CNetObjPed::OnGunShotWhizzedByEvent IsSilent[%d]",aiEvent.IsSilent());
    if(aiEvent.IsSilent())
    {
        for(unsigned index = 0; index < ms_NumPedsNotRespondingToCloneBullets; index++)
        {
            ObjectId   objectID      = ms_PedsNotRespondingToCloneBullets[index];
            netObject *networkObject = NetworkInterface::GetNetworkObject(objectID);

            if(networkObject && networkObject->IsClone())
            {
                CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

                if(ped)
                {
                    Vec3V closestPosition;
                    const Vector3 pedPosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

                    if(aiEvent.IsBulletInRange(pedPosition, closestPosition))
                    {
                        if(aiEvent.GetSourceEntity() == FindPlayerPed())
                        {
							weaponDebugf3("CNetObjPed::OnGunShotWhizzedByEvent invoke CInformSilencedGunShotEvent::Trigger");

                            CInformSilencedGunShotEvent::Trigger(ped, static_cast<CPed *>(aiEvent.GetSourceEntity()), aiEvent.GetSourcePos(), aiEvent.GetShotTarget(),
                                                                      aiEvent.GetWeaponHash(), false);
                        }
                    }
                }
            }
        }
    }
}

void CNetObjPed::OnInformSilencedGunShot(CPed *firingPed, const Vector3 &shotPos, const Vector3 &shotTarget, u32 weaponHash, bool mustBeSeenInVehicle)
{
	weaponDebugf3("CNetObjPed::OnInformSilencedGunShot");
    for(unsigned index = 0; index < ms_NumPedsNotRespondingToCloneBullets; index++)
    {
        ObjectId   objectID      = ms_PedsNotRespondingToCloneBullets[index];
        netObject *networkObject = NetworkInterface::GetNetworkObject(objectID);

        if(networkObject && !networkObject->IsClone())
        {
            CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

            if(ped)
            {
				weaponDebugf3("CNetObjPed::OnInformSilencedGunShot invoke CEventGunShotWhizzedBy");
                CEventGunShotWhizzedBy eventGunShotWhizzedBy(firingPed, shotPos, shotTarget, true, weaponHash, true);
                ped->GetPedIntelligence()->AddEvent(eventGunShotWhizzedBy);

				weaponDebugf3("CNetObjPed::OnInformSilencedGunShot invoke CEventGunShotBulletImpact");
				CEventGunShotBulletImpact eventGunShotBulletImpact(firingPed, shotPos, shotTarget, true, weaponHash, mustBeSeenInVehicle, true);
                ped->GetPedIntelligence()->AddEvent(eventGunShotBulletImpact);
            }
        }
    }
}

// name:        UpdateOrphanedMoveNetworkTaskSubNetwork
// description: Keep an eye on any move network task networks 

void CNetObjPed::UpdateOrphanedMoveNetworkTaskSubNetwork(void)
{
	static const u32 NUM_FRAMES_TO_ALLOW_ANIM_TASK_SUB_NETWORK_TO_RUN = 10;

	CPed* pPed = GetPed();

	if(!pPed)
	{
		return;
	}

	if(!m_orphanMoveNetwork.GetNetworkPlayer())
	{
		m_orphanMoveNetwork.Reset();
		return;
	}

	gnetAssert(m_orphanMoveNetwork.GetFrameNum() != 0);

	u32 frameNum = fwTimer::GetFrameCount();
	if((frameNum - m_orphanMoveNetwork.GetFrameNum()) > NUM_FRAMES_TO_ALLOW_ANIM_TASK_SUB_NETWORK_TO_RUN)
	{
		FreeOrphanedMoveNetworkTaskSubNetwork();
	}
}

void CNetObjPed::FreeOrphanedMoveNetworkTaskSubNetwork(void)
{
	if(m_orphanMoveNetwork.GetNetworkPlayer())
	{
		CPed* pPed = GetPed();

		if(m_orphanMoveNetwork.GetNetworkPlayer() == pPed->GetMovePed().GetTaskNetwork())
		{
			// another task really should have taken over by now - we quit this out and let the ped bind pose...
			pPed->GetMovePed().ClearTaskNetwork(m_orphanMoveNetwork.GetNetworkPlayer(), m_orphanMoveNetwork.GetBlendDuration(), m_orphanMoveNetwork.GetFlags());
		}
	}
		
	m_orphanMoveNetwork.Reset();
}

#if __DEV

//
// name:        AssertPedTaskInfosValid
// description: ensures all task infos are currently valid for this ped
//
void CNetObjPed::AssertPedTaskInfosValid()
{
    CPed* pPed = GetPed();
    VALIDATE_GAME_OBJECT(pPed);

    CQueriableInterface *pQueriableInterface = pPed ? pPed->GetPedIntelligence()->GetQueriableInterface() : 0;

    if(pQueriableInterface)
    {
        if(IsClone())
        {
            for(u32 index = 0; index < IPedNodeDataAccessor::NUM_TASK_SLOTS; index++)
            {
                const u32 taskType     = m_taskSlotCache.m_taskSlots[index].m_taskType;
                const u32 taskPriority = m_taskSlotCache.m_taskSlots[index].m_taskPriority;

                if(taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
                {
                    gnetAssert(pQueriableInterface->GetTaskInfoForTaskType(taskType, taskPriority));
                }
            }
        }
    }
}

#endif

#if !__FINAL

//
// name:        DumpTaskInformation
// description: dumps to tty useful information about the ped tasks.
//
void CNetObjPed::DumpTaskInformation(const char* callFrom)
{
	CPed* ped = GetPed();
	if (ped)
	{
		static const char* DeathStateNames[] =
		{
			"DeathState_Alive",
			"DeathState_Dying",
			"DeathState_Dead",
			"DeathState_Max"
		};

		gnetWarning("- %s : Notes Start\n", callFrom);

		const CTask* pTaskActive = ped->GetPedIntelligence()->GetTaskActive();
		while( pTaskActive )
		{
			gnetWarning("- Active Task : %s (Begun=%s)\n", TASKCLASSINFOMGR.GetTaskName(pTaskActive->GetTaskType()), ( pTaskActive->GetIsFlagSet(aiTaskFlags::HasBegun) ? "true" : "false" ) );
			pTaskActive = pTaskActive->GetSubTask();
		}

		const CTask* pTaskSecondary = ped->GetPedIntelligence()->GetTaskSecondary(0);
		while( pTaskSecondary )
		{
			gnetWarning("B- Secondary Task : %s (Begun=%s)\n", TASKCLASSINFOMGR.GetTaskName(pTaskSecondary->GetTaskType()), ( pTaskSecondary->GetIsFlagSet(aiTaskFlags::HasBegun) ? "true" : "false" ));
			pTaskSecondary = pTaskSecondary->GetSubTask();
		}

		const CTask* pTaskMovement = ped->GetPedIntelligence()->GetActiveMovementTask();
		while( pTaskMovement )
		{
			gnetWarning("- Movement Task : %s (Begun=%s)\n", TASKCLASSINFOMGR.GetTaskName(pTaskMovement->GetTaskType()), ( pTaskMovement->GetIsFlagSet(aiTaskFlags::HasBegun) ? "true" : "false" ));
			pTaskMovement = pTaskMovement->GetSubTask();
		}

		const CTask* pTaskMotion = (CTask*)ped->GetPedIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_MOTION);
		while( pTaskMotion )
		{
			gnetWarning("- Motion Task : %s (Begun=%s)\n", TASKCLASSINFOMGR.GetTaskName(pTaskMotion->GetTaskType()), ( pTaskMotion->GetIsFlagSet(aiTaskFlags::HasBegun) ? "true" : "false" ));
			pTaskMotion = pTaskMotion->GetSubTask();
		}

		const char *pName = NULL;
#if __BANK
		pName = ped->GetDebugName();
#endif

		gnetWarning("- Ped %s : IsPlayer %d : IsClone %d : Health %f : Armour %f : DeathState %s\n", pName, (int)ped->IsPlayer(), (int)ped->IsNetworkClone(), ped->GetHealth(), ped->GetArmour(), DeathStateNames[ped->GetDeathState()]);
		gnetWarning("- Weapon hash %d, damage entity %p, damage time %d, damage component %d\n", ped->GetWeaponDamageHash(), ped->GetWeaponDamageEntity(), ped->GetWeaponDamagedTime(), ped->GetWeaponDamageComponent());
		gnetWarning("- Notes End\n");
	}
}

#endif // !__FINAL

#if __BANK

u32 CNetObjPed::GetNetworkInfoStartYOffset()
{
    u32 offset = 0;

    if(GetPed() && GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
    {
        offset++;
    }

    return offset;
}

bool CNetObjPed::ShouldDisplayAdditionalDebugInfo()
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if (displaySettings.m_displayTargets.m_displayPeds && GetPed())
	{
		return true;
	}

	return CNetObjPhysical::ShouldDisplayAdditionalDebugInfo();
}


// Name         :   DisplayNetworkInfoForObject
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjPed::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
	char str[50];
	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();
	if(displaySettings.m_displayVisibilityInfo)
	{
		if( GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame))
		{
			sprintf(str, "DontRenderThisFrame flag set");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}
	}

	CNetObjPhysical::DisplayNetworkInfoForObject(col, scale, screenCoords, debugTextYIncrement);

    if(displaySettings.m_displayInformation.m_displayVehicle && GetPed())
    {
        CVehicle* pVehicle = GetPed()->GetMyVehicle();

        if (pVehicle && pVehicle->GetNetworkObject())
        {
            sprintf(str, "%s", pVehicle->GetNetworkObject()->GetLogName());
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
            screenCoords.y += debugTextYIncrement;
        }
    }

    if(displaySettings.m_displayInformation.m_displayPedRagdoll && GetPed())
    {
        sprintf(str, "%s", IsUsingRagdoll() ? "true" : "false");
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;
    }
}

void CNetObjPed::DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const
{
    const unsigned DEBUG_STR_LEN = 200;
	char debugStr[DEBUG_STR_LEN];

    CNetBlenderPed *netBlenderPed = GetNetBlender() ? SafeCast(CNetBlenderPed, GetNetBlender()) : 0;

    if(netBlenderPed)
    {
        CNetObjPhysical::DisplayBlenderInfo(screenCoords, playerColour, scale, debugYIncrement);

        if (netBlenderPed->IsBlendingInAllDirections())
        {
            snprintf(debugStr, DEBUG_STR_LEN, "Blending in all directions");
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;
        }

        if (netBlenderPed->UsedAnimatedRagdollBlendingLastFrame())
        {
            snprintf(debugStr, DEBUG_STR_LEN, "Used animated ragdoll blending last frame");
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;
        }

        ObjectId objectID = NETWORK_INVALID_OBJECT_ID;
        Vector3  offset;

        if(netBlenderPed->IsStandingOnObject(objectID, offset))
        {
            char objectName[128];
            NetworkInterface::GetObjectManager().GetLogName(objectName, sizeof(objectName), objectID);

            snprintf(debugStr, DEBUG_STR_LEN, "Standing On: %s%s", objectName, netBlenderPed->IsRagdolling() ? " (Ragdolling)" : "");
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;

            snprintf(debugStr, DEBUG_STR_LEN, "Offset (Local Coords): %.2f, %.2f, %.2f", offset.x, offset.y, offset.z);
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
            screenCoords.y += debugYIncrement;
        }
    }
}

void CNetObjPed::GetOrientationDisplayString(char *buffer) const
{
    if(buffer)
    {
        if(GetPed())
        {
            sprintf(buffer, "%.2f (%.2f)", GetPed()->GetCurrentHeading(), GetPed()->GetDesiredHeading());
        }
    }
}

void CNetObjPed::GetHealthDisplayString(char *buffer) const
{
    if(buffer)
    {
        CPed *ped = GetPed();

        if(ped)
        {
            sprintf(buffer, "Hlth: %.2f, Arm: %.2f, MaxHlth: %.2f", ped->GetHealth(), ped->GetArmour(), ped->GetMaxHealth());
        }
    }
}

const char *CNetObjPed::GetNetworkBlenderIsOverridenString(unsigned resultCode) const
{
    if(resultCode == NBO_PED_TASKS)
    {
        const char *overridingTask = 0;

        CPed *ped = GetPed();

        if(ped)
        {
            CTask *task = ped->GetPedIntelligence()->GetTaskActive();

            while(task && (overridingTask == 0))
            {
                if(task->IsClonedFSMTask() && static_cast<CTaskFSMClone *>(task)->OverridesNetworkBlender(ped))
                {
                    overridingTask = TASKCLASSINFOMGR.GetTaskName(task->GetTaskType());
                }

                task = task->GetSubTask();
            }
        }

        if(overridingTask == 0)
        {
            overridingTask = "Unknown Ped Task";
        }

        return overridingTask;
    }
    else
    {
        return CNetObjPhysical::GetNetworkBlenderIsOverridenString(resultCode);
    }
}

const char *CNetObjPed::GetCanPassControlErrorString(unsigned cpcFailReason, eMigrationType migrationType) const
{
    if(cpcFailReason == CPC_FAIL_PED_TASKS)
    {
        const char *failingTask = 0;

        CPed *ped = GetPed();

        if(ped && GetPlayerOwner())
        {
            CTask *task = ped->GetPedIntelligence()->GetTaskActive();

            while(task && (failingTask == 0))
            {
                if(task->IsClonedFSMTask() && !static_cast<CTaskFSMClone *>(task)->ControlPassingAllowed(ped, *GetPlayerOwner(), migrationType))
                {
                    failingTask = TASKCLASSINFOMGR.GetTaskName(task->GetTaskType());
                }

                task = task->GetSubTask();
            }
        }

        if(failingTask == 0)
        {
            failingTask = "Unknown Ped Task";
        }

        return failingTask;
    }
    else
    {
        return CNetObjPhysical::GetCanPassControlErrorString(cpcFailReason, migrationType);
    }
}

#endif  //__BANK...
