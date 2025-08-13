//
// name:        PedSyncNodes.cpp
// description: Network sync nodes used by CNetObjPeds
// written by:    John Gurney
//

#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"

#include "network/network.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "network/objects/entities/NetObjPed.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "script/Handlers/GameScriptEntity.h"
#include "streaming/streaming.h"
#include "task/General/Phone/TaskMobilePhone.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "weapons/info/weaponinfomanager.h"
#include "Peds/NavCapabilities.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IPedNodeDataAccessor);

void CPedCreationDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetPedCreateData(*this);

	m_inVehicle = (m_vehicleID != NETWORK_INVALID_OBJECT_ID);
}

void CPedCreationDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetPedCreateData(*this);
}

void CPedCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_SEED = 16;
	static const unsigned int SIZEOF_SEAT = datBitsNeeded<MAX_VEHICLE_SEATS>::COUNT;
	static const unsigned int SIZEOF_HEALTH = 13;
	const unsigned SIZEOF_PHYSICAL_PLAYER_INDEX  = 5;
	static const unsigned int SIZEOF_VOICE_HASH = 32;

	SERIALISE_BOOL(serialiser, m_IsRespawnObjId, "Is a Respawn Object Id");
	SERIALISE_BOOL(serialiser, m_RespawnFlaggedForRemoval, "Respawn Flagged For Removal");

	SERIALISE_PED_POP_TYPE(serialiser, m_popType, "Pop type");
	SERIALISE_MODELHASH(serialiser, m_modelHash, "Model");
	SERIALISE_UNSIGNED(serialiser, m_randomSeed, SIZEOF_SEED, "Random Seed");

	SERIALISE_BOOL(serialiser, m_inVehicle, "In Vehicle");
	SERIALISE_UNSIGNED(serialiser, m_voiceHash, SIZEOF_VOICE_HASH, "Ped Voice Hash");

	if(m_inVehicle || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle ID");
		SERIALISE_UNSIGNED(serialiser, m_seat, SIZEOF_SEAT, "Seat + 1");
	}
	else
	{
		m_vehicleID = NETWORK_INVALID_OBJECT_ID;
		m_seat = 0;
	}

	SERIALISE_BOOL(serialiser, m_hasProp);

	if(m_hasProp || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_MODELHASH(serialiser, m_propHash, "Prop");
	}

	SERIALISE_BOOL(serialiser, m_isStanding, "Is Standing");

	SERIALISE_BOOL(serialiser, m_hasAttDamageToPlayer, "Has AttDamageToPlayer");
	if (m_hasAttDamageToPlayer)
	{
		SERIALISE_UNSIGNED(serialiser, m_attDamageToPlayer, SIZEOF_PHYSICAL_PLAYER_INDEX, "Attribute Damage To Player");
	}
	else
	{
		m_attDamageToPlayer = INVALID_PLAYER_INDEX;
	}
	
	SERIALISE_UNSIGNED(serialiser, m_maxHealth, SIZEOF_HEALTH, "Max Health");

	SERIALISE_BOOL(serialiser, m_wearingAHelmet, "Wearing A Helmet");
}

bool CPedCreationDataNode::CanApplyData(netSyncTreeTargetObject* UNUSED_PARAM(pObj)) const
{
    // ensure that the model is loaded and ready for drawing for this ped
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_modelHash, &modelId);

	if (!gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash %u for ped clone", m_modelHash))
	{
		CNetwork::GetMessageLog().Log("\tREJECT: Model is not valid\r\n");
		return false;
	}

	if(!CNetworkStreamingRequestManager::RequestModel(modelId))
	{
        CNetwork::GetMessageLog().Log("\tREJECT: Model is not streamed in\r\n");
		return false;
	}

	return true;
}

void CPedScriptCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_StayInCarWhenJacked, "Stay in car when jacked");
}

PlayerFlags CPedGameStateDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const
{
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);

    if(netObjPed && netObjPed->ForceGameStateRequestPending())
    {
        return ~0u;
    }

    return CSyncDataNodeInfrequent::StopSend(pObj, serMode);
}

void CPedGameStateDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetPedGameStateData(*this);

	m_isLookingAtObject = (m_LookAtObjectID != NETWORK_INVALID_OBJECT_ID);

	m_hasVehicle = (m_vehicleID != NETWORK_INVALID_OBJECT_ID);
	m_onMount   = (m_mountID != NETWORK_INVALID_OBJECT_ID);
	m_hasCustodianOrArrestFlags = (m_custodianID != NETWORK_INVALID_OBJECT_ID) || 
		m_arrestFlags.m_canPerformArrest || 
		m_arrestFlags.m_canPerformUncuff ||
		m_arrestFlags.m_canBeArrested ||
		m_arrestFlags.m_isHandcuffed ||
		m_arrestFlags.m_isInCustody;
}

void CPedGameStateDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetPedGameStateData(*this);
}

void CPedGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	//static const unsigned int SIZEOF_ARRESTSTATE			= 1;
	//static const unsigned int SIZEOF_DEATHSTATE				= 2;
	static const unsigned int SIZEOF_NUM_WEAPON_COMPONENTS  = datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS>::COUNT;
	static const unsigned int SIZEOF_WEAPON_COMPONENT_TINT_INDEX = 5;
	static const unsigned int SIZEOF_WEAPON_COMPONENT_HASH  = 32;
	static const unsigned int SIZEOF_NUM_GADGETS			= datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_GADGETS>::COUNT;
	static const unsigned int SIZEOF_GADGET_HASH			= 32;
	static const unsigned int SIZEOF_SEAT					= datBitsNeeded<MAX_VEHICLE_SEATS>::COUNT;
	static const unsigned int SIZEOF_TINT_INDEX				= 5;
	static const unsigned int SIZEOF_CLEARDAMAGECOUNT		= 2;
	static const unsigned int SIZEOF_HEARINGRANGE			= 10;
	static const unsigned int SIZEOF_SEEINGRANGE			= 10;
	static const unsigned int SIZEOF_SEEINGRANGEPERIPHERAL	= 10;
	static const unsigned int SIZEOF_CENTREGAZEANGLE		= 10;
	static const unsigned int SIZEOF_AZIMUTHANGLE			= 10;
	static const unsigned int SIZEOF_LOOK_AT_FLAGS = LF_NUM_FLAGS; 

	SERIALISE_BOOL(serialiser, m_keepTasksAfterCleanup, "Keep tasks after cleanup");
	SERIALISE_BOOL(serialiser, m_createdByConcealedPlayer, "Ped created by concealed player");
	SERIALISE_BOOL(serialiser, m_dontBehaveLikeLaw, "Dont behave like law");
	SERIALISE_BOOL(serialiser, m_hitByTranqWeapon, "Hit by tranquilizer weapon");

	SERIALISE_BOOL(serialiser, m_permanentlyDisablePotentialToBeWalkedIntoResponse, "Permanently disable potential to be walked in response");
	SERIALISE_BOOL(serialiser, m_dontActivateRagdollFromAnyPedImpact, "Dont activate ragdoll from any ped impact");

	SERIALISE_BOOL(serialiser, m_changeToAmbientPopTypeOnMigration, "Change to ambient pop type on migration");
	SERIALISE_BOOL(serialiser, m_canBeIncapacitated, "If set, the ped may be incapacitated");

	SERIALISE_BOOL(serialiser, m_bDisableStartEngine, "Disable Start Engine");

	SERIALISE_BOOL(serialiser, m_disableBlindFiringInShotReactions, "Disable blind firing in shot reactions")

	SERIALISE_ARREST_STATE(serialiser, m_arrestState, "Arrest State");
	SERIALISE_DEATH_STATE(serialiser, m_deathState, "Death State");

	bool hasWeapon = m_weapon != 0;
	SERIALISE_BOOL(serialiser, hasWeapon);
	if (hasWeapon || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_WEAPON(serialiser, m_weapon, "Weapon");
	}
	else
	{
		m_weapon = 0;
	}
	
	SERIALISE_BOOL(serialiser, m_hasDroppedWeapon, "Has Dropped Weapon");
	SERIALISE_BOOL(serialiser, m_weaponObjectExists, "Weapon Exists");
	SERIALISE_BOOL(serialiser, m_weaponObjectVisible, "Weapon Visible");
	SERIALISE_BOOL(serialiser, m_weaponObjectHasAmmo, "Weapon HasAmmo");
	SERIALISE_BOOL(serialiser, m_weaponObjectAttachLeft, "Weapon AttachLeft");
	SERIALISE_BOOL(serialiser, m_HasValidWeaponClipset, "Has Valid Weapon Clipset");	

	bool hasTint = m_weaponObjectTintIndex != 0;
	SERIALISE_BOOL(serialiser, hasTint, "Has Weapon Tint Index");

	if(hasTint || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_weaponObjectTintIndex, SIZEOF_TINT_INDEX, "Weapon Tint Index");
	}
	else
	{
		m_weaponObjectTintIndex = 0;
	}

	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numWeaponComponents= IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS;
	}

	SERIALISE_UNSIGNED(serialiser, m_numWeaponComponents, SIZEOF_NUM_WEAPON_COMPONENTS, "Num Weapon Components");

	for(u32 index = 0; index < m_numWeaponComponents; index++)
	{
		SERIALISE_UNSIGNED(serialiser, m_weaponComponents[index], SIZEOF_WEAPON_COMPONENT_HASH, "Weapon component Hash");
		bool hasTint = m_weaponComponentsTint[index] != 0;
		SERIALISE_BOOL(serialiser, hasTint, "Has Weapon component Tint");
		if(hasTint || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_weaponComponentsTint[index], SIZEOF_WEAPON_COMPONENT_TINT_INDEX, "Weapon component Tint Index");
		}
		else
		{
			m_weaponComponentsTint[index] = 0;
		}
	}

	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numGadgets = IPedNodeDataAccessor::MAX_SYNCED_GADGETS;
	}

	SERIALISE_UNSIGNED(serialiser, m_numGadgets, SIZEOF_NUM_GADGETS, "Num Equipped Gadgets");

	for(u32 index = 0; index < m_numGadgets; index++)
	{
		SERIALISE_UNSIGNED(serialiser, m_equippedGadgets[index], SIZEOF_GADGET_HASH, "Gadget Hash");
	}

	SERIALISE_BOOL(serialiser, m_hasVehicle, "Has Vehicle");

	if (m_hasVehicle || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_vehicleID, "Vehicle ID");
		SERIALISE_BOOL(serialiser, m_inVehicle, "In Vehicle");

		if (m_inVehicle || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_seat, SIZEOF_SEAT, "Seat + 1");
		}
		else
		{
			m_seat = 0;
		}
	}
	else
	{
		m_vehicleID = NETWORK_INVALID_OBJECT_ID;
		m_inVehicle = false;
	}

	SERIALISE_BOOL(serialiser, m_onMount, "On Mount");

	if(m_onMount || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_mountID, "Mount ID");
	}
	else
	{
		m_mountID = NETWORK_INVALID_OBJECT_ID;
	}

	SERIALISE_BOOL(serialiser, m_hasCustodianOrArrestFlags, "Has Custodian Or Arrest Flags");
	if(m_hasCustodianOrArrestFlags || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_custodianID, "Custodian ID");
		SERIALISE_BOOL(serialiser, m_arrestFlags.m_isHandcuffed,	    "Is Handcuffed");
		SERIALISE_BOOL(serialiser, m_arrestFlags.m_canPerformArrest,	"Can Perform Arrest");
		SERIALISE_BOOL(serialiser, m_arrestFlags.m_canPerformUncuff,	"Can Perform Uncuff");
		SERIALISE_BOOL(serialiser, m_arrestFlags.m_canBeArrested,		"Can Be Arrested");
		SERIALISE_BOOL(serialiser, m_arrestFlags.m_isInCustody,		"Is In Custody");
	}
	else
	{
		m_custodianID = NETWORK_INVALID_OBJECT_ID;
		m_arrestFlags = IPedNodeDataAccessor::PedArrestFlags();
	}

	SERIALISE_BOOL(serialiser, m_flashLightOn, "m_flashLightOn");

	SERIALISE_BOOL(serialiser, m_bActionModeEnabled, "Action Mode Enabled");
	SERIALISE_BOOL(serialiser, m_bStealthModeEnabled, "Stealth Mode Enabled");
	if(m_bActionModeEnabled || m_bStealthModeEnabled || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_nMovementModeOverrideID, 32, "Action Mode Override");
	}
	else
	{
		m_nMovementModeOverrideID = 0;
	}

	SERIALISE_BOOL(serialiser, m_killedByStealth, "m_killedByStealth");
	SERIALISE_BOOL(serialiser, m_killedByTakedown, "m_killedByTakedown");
	SERIALISE_BOOL(serialiser, m_killedByKnockdown, "m_killedByKnockdown");
	SERIALISE_BOOL(serialiser, m_killedByStandardMelee, "m_killedByStandardMelee");

	SERIALISE_BOOL(serialiser, m_isUpright, "Upright");

	SERIALISE_UNSIGNED(serialiser, m_cleardamagecount, SIZEOF_CLEARDAMAGECOUNT, "Clear Damage Count");

	SERIALISE_BOOL(serialiser, m_bPedPerceptionModified, "m_bPedPerceptionModified");
	if (m_bPedPerceptionModified || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_PedPerception.m_HearingRange, SIZEOF_HEARINGRANGE, "Hearing Perception Range");
		SERIALISE_UNSIGNED(serialiser, m_PedPerception.m_SeeingRange,  SIZEOF_SEEINGRANGE, "Seeing Perception Range");

		SERIALISE_UNSIGNED(serialiser, m_PedPerception.m_SeeingRangePeripheral,  SIZEOF_SEEINGRANGEPERIPHERAL, "Seeing Range Peripheral");

		SERIALISE_PACKED_FLOAT(serialiser, m_PedPerception.m_CentreOfGazeMaxAngle, 180.f, SIZEOF_CENTREGAZEANGLE, "Centre Of Gaze Angle");
		SERIALISE_PACKED_FLOAT(serialiser, m_PedPerception.m_VisualFieldMinAzimuthAngle, 180.f, SIZEOF_AZIMUTHANGLE, "Visual Field Min Azimuth Angle");
		SERIALISE_PACKED_FLOAT(serialiser, m_PedPerception.m_VisualFieldMaxAzimuthAngle, 180.f, SIZEOF_AZIMUTHANGLE, "Visual Field Max Azimuth Angle");

		SERIALISE_BOOL(serialiser, m_PedPerception.m_bIsHighlyPerceptive, "Is Highly Perceptive");
	}
	else
	{
		m_PedPerception.m_HearingRange				 = static_cast< u32 >(CPedPerception::ms_fSenseRange);
		m_PedPerception.m_SeeingRange				 = static_cast< u32 >(CPedPerception::ms_fSenseRange);

		m_PedPerception.m_SeeingRangePeripheral		 = static_cast< u32 >(CPedPerception::ms_fSenseRangePeripheral);
		m_PedPerception.m_CentreOfGazeMaxAngle		 = CPedPerception::ms_fCentreOfGazeMaxAngle;
		m_PedPerception.m_VisualFieldMinAzimuthAngle = CPedPerception::ms_fVisualFieldMinAzimuthAngle;
		m_PedPerception.m_VisualFieldMaxAzimuthAngle = CPedPerception::ms_fVisualFieldMaxAzimuthAngle;

		m_PedPerception.m_bIsHighlyPerceptive		 = false;
	}

	SERIALISE_BOOL(serialiser, m_isLookingAtObject, "Is Looking At Object");
	if(m_isLookingAtObject || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_LookAtFlags, SIZEOF_LOOK_AT_FLAGS, "Look At Flags");
		SERIALISE_OBJECTID(serialiser, m_LookAtObjectID, "Look At Object");
	}
	else
	{
		m_LookAtFlags = 0;
		m_LookAtObjectID = NETWORK_INVALID_OBJECT_ID;
	}

	SERIALISE_BOOL(serialiser, m_isDuckingInVehicle, "m_isDuckingInVehicle");

	SERIALISE_BOOL(serialiser, m_isUsingLowriderLeanAnims, "m_isUsingLowriderLeanAnims");
	SERIALISE_BOOL(serialiser, m_isUsingAlternateLowriderLeanAnims, "m_isUsingAlternateLowriderLeanAnims");
}

bool CPedGameStateDataNode::HasDefaultState() const
{
	return (!m_keepTasksAfterCleanup &&
			!m_createdByConcealedPlayer &&
			!m_dontBehaveLikeLaw &&
			!m_hitByTranqWeapon &&
			!m_canBeIncapacitated &&
			!m_bDisableStartEngine &&
			!m_disableBlindFiringInShotReactions &&
			!m_dontActivateRagdollFromAnyPedImpact &&
			!m_permanentlyDisablePotentialToBeWalkedIntoResponse &&
			!m_changeToAmbientPopTypeOnMigration &&
			m_arrestState == ArrestState_None &&
			m_deathState == DeathState_Alive &&
			m_weapon == 0 &&
			!m_weaponObjectExists && 
			!m_weaponObjectVisible &&
			m_numWeaponComponents == 0 &&
			m_numGadgets == 0 &&
			!m_inVehicle &&
			!m_onMount && 
			!m_hasCustodianOrArrestFlags && 
			!m_bActionModeEnabled &&
			!m_bStealthModeEnabled && 
			!m_weaponObjectAttachLeft &&
			!m_bPedPerceptionModified && 
			!m_isLookingAtObject &&
			m_isUpright &&
			!m_hasDroppedWeapon &&
			!m_flashLightOn);
}

void CPedGameStateDataNode::SetDefaultState()
{
	m_keepTasksAfterCleanup = false;
	m_createdByConcealedPlayer = false;
	m_dontBehaveLikeLaw = false;
	m_hitByTranqWeapon = false;
	m_canBeIncapacitated = false;
	m_bDisableStartEngine = false;
	m_disableBlindFiringInShotReactions = false;
	m_permanentlyDisablePotentialToBeWalkedIntoResponse = false;
	m_dontActivateRagdollFromAnyPedImpact = false;
	m_changeToAmbientPopTypeOnMigration = false;
	m_arrestState = ArrestState_None;
	m_deathState = DeathState_Alive;
	m_weapon = 0;
	m_weaponObjectExists = m_weaponObjectVisible = false;
	m_weaponObjectHasAmmo = true;
	m_weaponObjectAttachLeft = false;
	m_numWeaponComponents = m_numGadgets = 0;
	m_inVehicle = m_onMount = false;
	m_hasCustodianOrArrestFlags = false;
	m_custodianID = NETWORK_INVALID_OBJECT_ID;
	m_arrestFlags.m_canPerformArrest	= false;
	m_arrestFlags.m_canPerformUncuff	= false;
	m_arrestFlags.m_canBeArrested		= false;
	m_arrestFlags.m_isHandcuffed		= false;
	m_arrestFlags.m_isInCustody			= false;
	m_bActionModeEnabled = false;
	m_bStealthModeEnabled = false;
	m_nMovementModeOverrideID = 0;
	m_killedByStealth = false;
	m_killedByTakedown = false;
	m_killedByKnockdown = false;
	m_killedByStandardMelee = false;
	m_hasDroppedWeapon = false;

	m_bPedPerceptionModified = false;
	m_PedPerception.m_HearingRange       = static_cast< u32 >(CPedPerception::ms_fSenseRange);
	m_PedPerception.m_SeeingRange        = static_cast< u32 >(CPedPerception::ms_fSenseRange);
	m_PedPerception.m_bIsHighlyPerceptive = false;

	m_isLookingAtObject					 = false;
	m_LookAtFlags						 = 0;
	m_LookAtObjectID					 = NETWORK_INVALID_OBJECT_ID;

	m_isUpright = true;
	m_flashLightOn = false;
}

PlayerFlags CPedSectorPosMapNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);

    if(netObjPed && netObjPed->IsNavmeshTrackerValid())
    {
        return ~0u;
    }

	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}
	
	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

    return playerFlags;
}

void CPedSectorPosMapNode::Serialise(CSyncDataBase& serialiser)
{
    CSectorPositionDataNode::Serialise(serialiser);

    bool hasExtraData = m_IsStandingOnNetworkObject || m_IsRagdolling;

    SERIALISE_BOOL(serialiser, hasExtraData);

	if(hasExtraData || serialiser.GetIsMaximumSizeSerialiser())
	{
		const unsigned SIZEOF_STANDING_OBJECT_OFFSET_POS    = 12;
		const unsigned SIZEOF_STANDING_OBJECT_OFFSET_HEIGHT = 10;

        SERIALISE_BOOL(serialiser, m_IsRagdolling, "Is Ragdolling");
        SERIALISE_BOOL(serialiser, m_IsStandingOnNetworkObject);

        if(m_IsStandingOnNetworkObject || serialiser.GetIsMaximumSizeSerialiser())
        {
		    SERIALISE_OBJECTID(serialiser, m_StandingOnNetworkObjectID, "Standing On");
            SERIALISE_PACKED_FLOAT(serialiser, m_LocalOffset.x, CNetObjPed::MAX_STANDING_OBJECT_OFFSET_POS,    SIZEOF_STANDING_OBJECT_OFFSET_POS,    "Standing On Local Offset X");
		    SERIALISE_PACKED_FLOAT(serialiser, m_LocalOffset.y, CNetObjPed::MAX_STANDING_OBJECT_OFFSET_POS,    SIZEOF_STANDING_OBJECT_OFFSET_POS,    "Standing On Local Offset Y");
		    SERIALISE_PACKED_FLOAT(serialiser, m_LocalOffset.z, CNetObjPed::MAX_STANDING_OBJECT_OFFSET_HEIGHT, SIZEOF_STANDING_OBJECT_OFFSET_HEIGHT, "Standing On Local Offset Z");
        }
	}
	else
	{
        m_IsRagdolling              = false;
        m_IsStandingOnNetworkObject = false;
		m_StandingOnNetworkObjectID = NETWORK_INVALID_OBJECT_ID;
		m_LocalOffset = VEC3_ZERO;
	}
}

PlayerFlags CPedSectorPosNavMeshNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);

    if(netObjPed && !netObjPed->IsNavmeshTrackerValid())
    {
        return ~0u;
    }

	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

    return playerFlags;
}

void CPedSectorPosNavMeshNode::Serialise(CSyncDataBase& serialiser)
{
	const unsigned SIZEOF_NAVMESH_SECTORPOS_XY = 12;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK,  SIZEOF_NAVMESH_SECTORPOS_XY, "Sector Pos X");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK,  SIZEOF_NAVMESH_SECTORPOS_XY, "Sector Pos Y");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_sectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_NAVMESH_SECTORPOS_Z,  "Sector Pos Z");
}

void CPedComponentReservationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_COMPONENTS = 4;

	SERIALISE_UNSIGNED(serialiser, m_numPedComponents, SIZEOF_COMPONENTS);

	// this is the maximum allowable value for the number of wheels
	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numPedComponents = CComponentReservation::MAX_NUM_COMPONENTS;
	}

	for(u32 componentIndex = 0; componentIndex < m_numPedComponents; componentIndex++)
	{
		SERIALISE_OBJECTID(serialiser, m_componentReservations[componentIndex], "Ped Using Component");
	}
}

bool CPedComponentReservationDataNode::HasDefaultState() const 
{ 
	return (m_numPedComponents == 0); 
}

void CPedComponentReservationDataNode::SetDefaultState() 
{ 
	m_numPedComponents = 0; 
}


#if __BANK
bool CSyncedPedVehicleEntryScriptConfig::ValidateData() const
{
	bool bForcingForAnyVehicle = false;
	bool bForcingForSpecificVehicle = false;
	atArray<ObjectId> aEncounteredVehicleIds;

	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		ObjectId objId = m_Data[i].m_SyncedVehicle.GetVehicleID();
		if (aEncounteredVehicleIds.Find(objId) != -1)
		{
			aiDisplayf("Error found on slot %i, duplicate vehicle id found", i);
			return false;
		}

		if (objId != NETWORK_INVALID_OBJECT_ID)
		{
			aEncounteredVehicleIds.PushAndGrow(objId);
		}

		if (m_Data[i].HasConflictingFlagsSet())
		{
			aiDisplayf("Error found on slot %i, conflicting flags have been set", i);
			return false;
		}

		if (IsFlagSet(i, CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle))
		{
			if (GetVehicle(i))
			{
				aiDisplayf("Error found on slot %i, Forced to use certain seats for any vehicle, but vehicle %s", i, AILogging::GetDynamicEntityNameSafe(GetVehicle(i)));
				return false;
			}
			bForcingForAnyVehicle = true;
		}

		if (m_Data[i].HasAForcingFlagSet())
		{
			if (!GetVehicle(i))
			{
				aiDisplayf("Error found on slot %i, Forced to use certain seats NULL vehicle", i);
				return false;
			}

			bForcingForSpecificVehicle = true;
		}

		if (bForcingForAnyVehicle && bForcingForSpecificVehicle)
		{
			aiDisplayf("Error found on slot %i, Forced to use certain seats for any vehicle, but also for a specific vehicle %s", i,  AILogging::GetDynamicEntityNameSafe(GetVehicle(i)));
			return false;
		}
	}

	return true;
}

void CSyncedPedVehicleEntryScriptConfig::SpewDataToTTY() const
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		aiDisplayf("Slot : %i", i);
		aiDisplayf("Flags : %i", GetFlags(i));
		aiDisplayf("Vehicle : %i - %s", GetVehicleId(i), AILogging::GetDynamicEntityNameSafe(GetVehicle(i)));
		aiDisplayf("Seat Index : %i", GetSeatIndex(i));
	}
}
#endif // __BANK

void CSyncedPedVehicleEntryScriptConfig::ClearForcedSeatUsage()
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		if (m_Data[i].HasAForcingFlagSet() || m_Data[i].HasAForcingSeatIndex())
		{
			m_Data[i].Reset();
		}
	}
}

void CSyncedPedVehicleEntryScriptConfigData::Reset() 
{ 
	m_SyncedVehicle.Invalidate(); 
	m_Flags = 0;
	m_SeatIndex = -1;
}

void CSyncedPedVehicleEntryScriptConfigData::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_FORCED_SEAT_USAGE_FLAGS = LastFlag;
	static const unsigned int SIZEOF_FORCED_SEAT_INDEX = 5;

	SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FORCED_SEAT_USAGE_FLAGS, "Forced seat usage flags");
	SERIALISE_INTEGER(serialiser, m_SeatIndex, SIZEOF_FORCED_SEAT_INDEX, "Forced seat index usage");

	m_SyncedVehicle.Serialise(serialiser);
}

void CSyncedPedVehicleEntryScriptConfig::Reset()
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		m_Data[i].Reset();
	}
}

void CSyncedPedVehicleEntryScriptConfig::Serialise(CSyncDataBase& serialiser)
{
	bool bHasNonDefaultState = HasNonDefaultState();
	SERIALISE_BOOL(serialiser, bHasNonDefaultState);

	if (bHasNonDefaultState || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
		{
			bool bInstanceHasNonDefaultState = m_Data[i].HasNonDefaultState();
			SERIALISE_BOOL(serialiser, bInstanceHasNonDefaultState);

			if (bInstanceHasNonDefaultState || serialiser.GetIsMaximumSizeSerialiser())
			{
				m_Data[i].Serialise(serialiser);
			}
			else
			{
				m_Data[i].Reset();
			}
		}
	}
	else
	{
		Reset();
	}
}

bool CSyncedPedVehicleEntryScriptConfig::ShouldRejectBecauseForcedToUseSeatIndex(const CVehicle& rVeh, s32 iSeat) const
{
	const s32 iForcedSeatIndex = GetForcingSeatIndexForThisVehicle(&rVeh);
	if (iSeat == -1 || iForcedSeatIndex == -1)
		return false;

	return iForcedSeatIndex != iSeat;
}

bool CSyncedPedVehicleEntryScriptConfig::HasNonDefaultState() const
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		if (m_Data[i].HasNonDefaultState())
		{
			return true;
		}
	}
	return false;
}

bool CSyncedPedVehicleEntryScriptConfig::HasAForcingFlagSetForThisVehicle(const CVehicle* pVeh) const
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		if ((GetVehicle(i) == pVeh && m_Data[i].HasAForcingFlagSet()) || (!GetVehicle(i) && IsFlagSet(i, CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle)))
		{
			return true;
		}
	}
	return false;
}

bool CSyncedPedVehicleEntryScriptConfig::HasARearForcingFlagSetForThisVehicle(const CVehicle* pVeh) const
{
	for (s32 i=0; i<MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		if (m_Data[i].IsFlagSet(CSyncedPedVehicleEntryScriptConfigData::ForceUseRearSeats))
		{
			if ((GetVehicle(i) == pVeh) || (!GetVehicle(i) && IsFlagSet(i, CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle)))
			{
				return true;
			}
		}
	}
	return false;
}

s32 CSyncedPedVehicleEntryScriptConfig::GetForcingSeatIndexForThisVehicle(const CVehicle* pVeh) const
{
	for (s32 i = 0; i < MAX_VEHICLE_ENTRY_DATAS; ++i)
	{
		const s32 iSeatIndex = GetSeatIndex(i);
		if (iSeatIndex >= 0)
		{
			if ((GetVehicle(i) == pVeh) || (!GetVehicle(i) && IsFlagSet(i, CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle)))
				return iSeatIndex;
		}
	}
	return -1;
}

void CPedScriptGameStateDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetPedScriptGameStateData(*this);

	m_hasPedType = (m_pedType != (u32) PEDTYPE_LAST_PEDTYPE);

	m_HasInVehicleContextHash = (m_inVehicleContextHash != 0);
}

void CPedScriptGameStateDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetPedScriptGameStateData(*this);
}

bool CPedScriptGameStateDataNode::IsScriptPedConfigFlagSynchronized(int Flag)
{
	//Note: please keep in FLAG # order for switch/case evaluation
	switch(Flag)
	{
		case CPED_CONFIG_FLAG_NoCriticalHits:
		case CPED_CONFIG_FLAG_DrownsInWater:
		case CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming:
		case CPED_CONFIG_FLAG_NeverEverTargetThisPed:
		case CPED_CONFIG_FLAG_ThisPedIsATargetPriority:
		case CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead:
		case CPED_CONFIG_FLAG_BlockNonTemporaryEvents:
		case CPED_CONFIG_FLAG_MoneyHasBeenGivenByScript:
		case CPED_CONFIG_FLAG_ForceDieIfInjured:
		case CPED_CONFIG_FLAG_DontDragMeOutCar:
		case CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar:
		case CPED_CONFIG_FLAG_GetOutUndriveableVehicle:
		case CPED_CONFIG_FLAG_WillFlyThroughWindscreen:
		case CPED_CONFIG_FLAG_HasHelmet:
		case CPED_CONFIG_FLAG_DontTakeOffHelmet:
		case CPED_CONFIG_FLAG_DontInfluenceWantedLevel:
		case CPED_CONFIG_FLAG_DisableLockonToRandomPeds:
		case CPED_CONFIG_FLAG_DisableHornAudioWhenDead:
		case CPED_CONFIG_FLAG_StopWeaponFiringOnImpact:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact:
		case CPED_CONFIG_FLAG_OpenDoorArmIK:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions:
		case CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired:
		case CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled:
		case CPED_CONFIG_FLAG_GetOutBurningVehicle:
		case CPED_CONFIG_FLAG_RunFromFiresAndExplosions:
		case CPED_CONFIG_FLAG_DisableMelee:
		case CPED_CONFIG_FLAG_JustGetsPulledOutWhenElectrocuted:
		case CPED_CONFIG_FLAG_WillNotHotwireLawEnforcementVehicle:
		case CPED_CONFIG_FLAG_WillCommandeerRatherThanJack:
		case CPED_CONFIG_FLAG_CanBeAgitated:
		case CPED_CONFIG_FLAG_ForcePedToFaceLeftInCover:
		case CPED_CONFIG_FLAG_ForcePedToFaceRightInCover:
		case CPED_CONFIG_FLAG_BlockPedFromTurningInCover:
		case CPED_CONFIG_FLAG_KeepRelationshipGroupAfterCleanUp:
		case CPED_CONFIG_FLAG_PreventPedFromReactingToBeingJacked:
		case CPED_CONFIG_FLAG_DisableLadderClimbing:
		case CPED_CONFIG_FLAG_CowerInsteadOfFlee:
		case CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown:
		case CPED_CONFIG_FLAG_CanPerformArrest: //note: synchronized in CPedGameStateDataNode rather than CPedScriptGameStateDataNode - but shouldn't assert
		case CPED_CONFIG_FLAG_CanPerformUncuff: //note: synchronized in CPedGameStateDataNode rather than CPedScriptGameStateDataNode - but shouldn't assert
		case CPED_CONFIG_FLAG_CanBeArrested:	//note: synchronized in CPedGameStateDataNode rather than CPedScriptGameStateDataNode - but shouldn't assert
        case CPED_CONFIG_FLAG_ForceDirectEntry:
		case CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents:
		case CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat:
		case CPED_CONFIG_FLAG_UseKinematicModeWhenStationary:
		case CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex:
		case CPED_CONFIG_FLAG_DisableExplosionReactions:
		case CPED_CONFIG_FLAG_ForcedToStayInCover:
		case CPED_CONFIG_FLAG_ListensToSoundEvents:
		case CPED_CONFIG_FLAG_DisablePanicInVehicle:
		case CPED_CONFIG_FLAG_ForceSkinCharacterCloth:
		case CPED_CONFIG_FLAG_PhoneDisableTextingAnimations:
		case CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations:
		case CPED_CONFIG_FLAG_PhoneDisableCameraAnimations:
		case CPED_CONFIG_FLAG_DisableBlindFiringInShotReactions:
		case CPED_CONFIG_FLAG_AllowNearbyCoverUsage:
		case CPED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw:
		case CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes:
		case CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger:
		case CPED_CONFIG_FLAG_PlayerCanJackFriendlyPlayers:
        case CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry:
		case CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats:
		case CPED_CONFIG_FLAG_TeleportToLeaderVehicle:
		case CPED_CONFIG_FLAG_DontBlipCop:
		case CPED_CONFIG_FLAG_AvoidTearGas:
		case CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured:
		case CPED_CONFIG_FLAG_ShouldChargeNow:
        case CPED_CONFIG_FLAG_DisableJumpingFromVehiclesAfterLeader:
		case CPED_CONFIG_FLAG_ShoutToGroupOnPlayerMelee:
		case CPED_CONFIG_FLAG_IgnoredByAutoOpenDoors:
		case CPED_CONFIG_FLAG_CheckLoSForSoundEvents:
		case CPED_CONFIG_FLAG_DontBehaveLikeLaw:
		case CPED_CONFIG_FLAG_LowerPriorityOfWarpSeats:
		case CPED_CONFIG_FLAG_DontBlip:
		case CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers:
		case CPED_CONFIG_FLAG_OnlyWritheFromWeaponDamage:
		case CPED_CONFIG_FLAG_PreventDraggedOutOfCarThreatResponse:
		case CPED_CONFIG_FLAG_PreventAutoShuffleToTurretSeat:
		case CPED_CONFIG_FLAG_DisableEventInteriorStatusCheck:
		case CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat:
		case CPED_CONFIG_FLAG_TreatNonFriendlyAsHateWhenInCombat:
		case CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen:
        case CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets:
		case CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes:
		case CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInAircraft:
		case CPED_CONFIG_FLAG_HasBareFeet:
		case CPED_CONFIG_FLAG_BlockDroppingHealthSnacksOnDeath:
		case CPED_CONFIG_FLAG_DontRespondToRandomPedsDamage:
		case CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates:
		case CPED_CONFIG_FLAG_KeepTargetLossResponseOnCleanup:
		case CPED_CONFIG_FLAG_BroadcastRepondedToThreatWhenGoingToPointShooting:
		case CPED_CONFIG_FLAG_DontLeaveVehicleIfLeaderNotInVehicle:
		case CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle:
		case CPED_CONFIG_FLAG_DisableHomingMissileLockForVehiclePedInside:
		case CPED_CONFIG_FLAG_IgnoreMeleeFistWeaponDamageMult: 
		case CPED_CONFIG_FLAG_LawPedsCanFleeFromNonWantedPlayer:
		case CPED_CONFIG_FLAG_UseGoToPointForScenarioNavigation:
		case CPED_CONFIG_FLAG_DontClearLocalPassengersWantedLevel:
		case CPED_CONFIG_FLAG_BlockAutoSwapOnWeaponPickups:
		case CPED_CONFIG_FLAG_ThisPedIsATargetPriorityForAI:
		case CPED_CONFIG_FLAG_ForceRagdollUponDeath:
		case CPED_CONFIG_FLAG_IsSwitchingHelmetVisor:
		case CPED_CONFIG_FLAG_ForceHelmetVisorSwitch:
		case CPED_CONFIG_FLAG_IsPerformingVehicleMelee:
		case CPED_CONFIG_FLAG_DontAttackPlayerWithoutWantedLevel:
        case CPED_CONFIG_FLAG_UseOverrideFootstepPtFx:
        case CPED_CONFIG_FLAG_DisableShockingEvents:
		case CPED_CONFIG_FLAG_DisableVehicleCombat:
		case CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage:
		case CPED_CONFIG_FLAG_AllowBikeAlternateAnimations:
		case CPED_CONFIG_FLAG_PlayerPreferFrontSeatMP:
		case CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromFire:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromElectrocution:
		case CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting:
		case CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact:
        case CPED_CONFIG_FLAG_Avoidance_Ignore_All:
		case CPED_CONFIG_FLAG_PedsJackingMeDontGetIn:
		case CPED_CONFIG_FLAG_AllowAutoShuffleToDriversSeat: // Don't need to sync just stopping it from asserting
		case CPED_CONFIG_FLAG_LawWillOnlyAttackIfPlayerIsWanted:
		case CPED_CONFIG_FLAG_IgnoreBeingOnFire:
		case CPED_CONFIG_FLAG_DisableWantedHelicopterSpawning:
		case CPED_CONFIG_FLAG_IgnoreNetSessionFriendlyFireCheckForAllowDamage:
		case CPED_CONFIG_FLAG_UseTargetPerceptionForCreatingAimedAtEvents:
		case CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly:
		case CPED_CONFIG_FLAG_DisableTurretOrRearSeatPreference:
		case CPED_CONFIG_FLAG_DisableHomingMissileLockon:
		case CPED_CONFIG_FLAG_ForceIgnoreMaxMeleeActiveSupportCombatants:
		case CPED_CONFIG_FLAG_StayInDefensiveAreaWhenInVehicle:
		case CPED_CONFIG_FLAG_DontShoutTargetPosition:
		case CPED_CONFIG_FLAG_PreventVehExitDueToInvalidWeapon:
		case CPED_CONFIG_FLAG_DisableHurt:
		case CPED_CONFIG_FLAG_DisableHelmetArmor:
		case CPED_CONFIG_FLAG_AllowToBeTargetedInAVehicle:
		case CPED_CONFIG_FLAG_DontLeaveCombatIfTargetPlayerIsAttackedByPolice:
		case CPED_CONFIG_FLAG_CheckLockedBeforeWarp:
		case CPED_CONFIG_FLAG_DontShuffleInVehicleToMakeRoom:
		case CPED_CONFIG_FLAG_GiveWeaponOnGetup:
		case CPED_CONFIG_FLAG_DontHitVehicleWithProjectiles:
		case CPED_CONFIG_FLAG_DisableForcedEntryForOpenVehiclesFromTryLockedDoor:
		case CPED_CONFIG_FLAG_FiresDummyRockets:
		case CPED_CONFIG_FLAG_IsDecoyPed:
		case CPED_CONFIG_FLAG_HasEstablishedDecoy:
		case CPED_CONFIG_FLAG_DisableInjuredCryForHelpEvents:
		case CPED_CONFIG_FLAG_DontCryForHelpOnStun:
		case CPED_CONFIG_FLAG_BlockDispatchedHelicoptersFromLanding:
		case CPED_CONFIG_FLAG_CanBeIncapacitated:
		case CPED_CONFIG_FLAG_DisableStartEngine:
		case CPED_CONFIG_FLAG_DontChangeTargetFromMelee:
		case CPED_CONFIG_FLAG_DisableBloodPoolCreation:
		case CPED_CONFIG_FLAG_RagdollFloatsIndefinitely:
		case CPED_CONFIG_FLAG_BlockElectricWeaponDamage:
			return true;
	}

	return false;
}

void CPedScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_ACCURACY = 8;
	static const unsigned int SIZEOF_SHOOTING_DISTANCE = 16;
	static const unsigned int SIZEOF_MINUTE_IN_SECONDS = 8;
	static const unsigned int SIZEOF_WEAPON_DAMAGE_MODIFIER = 10;
	static const unsigned int SIZEOF_SHOOT_RATE    = 8;
	static const unsigned int SIZEOF_MONEY_CARRIED = 16;
	static const unsigned int SIZEOF_MIN_GROUND_STUN_TIME = 14;
	static const unsigned int SIZEOF_COMBAT_MOVEMENT = datBitsNeeded<CCombatData::Movement_NUM_ENUMS>::COUNT;

	static const unsigned int SIZEOF_CANTBEKNOCKEDOFFBIKE = datBitsNeeded<KNOCKOFFVEHICLE_HARD>::COUNT; // KNOCKOFFVEHICLE_HARD defined in pedDefines.h
	static const unsigned int SIZEOF_RBF = eRagdollBlockingFlags_NUM_ENUMS; 
	static const unsigned int SIZEOF_AMMO_TO_DROP = 8;
	static const unsigned int SIZEOF_VWI_DATA = 3;
	static const unsigned int SIZEOF_SEAT_INDEX = 5;
	static const unsigned int SIZEOF_CONTEXT_HASH = 32;

	static const unsigned int SIZEOF_FLEE_FLAGS = datBitsNeeded<CFleeBehaviour::BF_DisableAmbientClips>::COUNT; // BF_DisableAmbientClips defined in FleeBehaviour.h

	static const unsigned int SIZEOF_NAV_CAP_FLAGS = datBitsNeeded<CPedNavCapabilityInfo::FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER>::COUNT; // FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER defined in NavCapabilities.h

	static const unsigned int SIZEOF_PEDTYPE = datBitsNeeded<PEDTYPE_LAST_PEDTYPE>::COUNT;

	SERIALISE_PED_POP_TYPE(serialiser, m_popType, "Pop type");

	SERIALISE_BOOL(serialiser, m_hasPedType);
	if(m_hasPedType || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_pedType, SIZEOF_PEDTYPE, "Ped Type");
	}
	else
	{
		m_pedType = (u32) PEDTYPE_LAST_PEDTYPE;
	}

	SERIALISE_BOOL(serialiser, m_PedFlags.neverTargetThisPed,          			"Never target this ped");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockNonTempEvents,          			"Block non-temp events");
	SERIALISE_BOOL(serialiser, m_PedFlags.noCriticalHits,              			"No critical hits");
	SERIALISE_BOOL(serialiser, m_PedFlags.isPriorityTarget,            			"Is priority target");
	SERIALISE_BOOL(serialiser, m_PedFlags.doesntDropWeaponsWhenDead,   			"Doesn't drop weapons when dead");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontDragMeOutOfCar,          			"Don't drag me out of car");
	SERIALISE_BOOL(serialiser, m_PedFlags.playersDontDragMeOutOfCar,          	"Players don't drag me out of car");
	SERIALISE_BOOL(serialiser, m_PedFlags.drownsInWater,           				"Drowns in water");
	SERIALISE_BOOL(serialiser, m_PedFlags.drownsInSinkingVehicle,  				"Drowns in sinking vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.forceDieIfInjured,           			"Force die if injured");
	SERIALISE_BOOL(serialiser, m_PedFlags.fallOutOfVehicleWhenKilled,	 		"Fall out of vehicle when killed");
	SERIALISE_BOOL(serialiser, m_PedFlags.diesInstantlyInWater,	     			"Dies Instantly In Water");
	SERIALISE_BOOL(serialiser, m_PedFlags.justGetsPulledOutWhenElectrocuted,  	"Just Gets Pulled Out When Electrocuted");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontRagdollFromPlayerImpact,			"Don't ragdoll from player impact");
	SERIALISE_BOOL(serialiser, m_PedFlags.lockAnim,								"Animation Is Locked");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventPedFromReactingToBeingJacked,	"Prevent Ped From Reacting To Being Jacked");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontInfluenceWantedLevel,				"Don't influence wanted level");
	SERIALISE_BOOL(serialiser, m_PedFlags.getOutUndriveableVehicle,				"Get out of undriveable vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.moneyHasBeenGivenByScript,			"Has money given by script");
	SERIALISE_BOOL(serialiser, m_PedFlags.shoutToGroupOnPlayerMelee,			"Shout to group on player melee");
	SERIALISE_BOOL(serialiser, m_PedFlags.canDrivePlayerAsRearPassenger,		"Ped can drive the player around as a passenger");
	SERIALISE_BOOL(serialiser, m_PedFlags.keepRelationshipGroupAfterCleanup,	"Ped keeps relationship group after cleanup");

	SERIALISE_BOOL(serialiser, m_PedFlags.disableLockonToRandomPeds,			"Disable lockdown to random peds");
	SERIALISE_BOOL(serialiser, m_PedFlags.willNotHotwireLawEnforcementVehicle,	"Will not hotwire law enf vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.willCommandeerRatherThanJack,			"Will commandeer rather than jack");
	SERIALISE_BOOL(serialiser, m_PedFlags.useKinematicModeWhenStationary,		"Use kinematic mode when stationary");
	SERIALISE_BOOL(serialiser, m_PedFlags.forcedToUseSpecificGroupSeatIndex,	"Forced to use specific group seat index");
	SERIALISE_BOOL(serialiser, m_PedFlags.playerCanJackFriendlyPlayers,			"Player can jack friendly players");

	SERIALISE_BOOL(serialiser, m_PedFlags.willFlyThroughWindscreen,				"Will fly through windscreen");
	SERIALISE_BOOL(serialiser, m_PedFlags.getOutBurningVehicle,					"Get out burning vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableLadderClimbing,				"Disable ladder climbing");
    SERIALISE_BOOL(serialiser, m_PedFlags.forceDirectEntry,                   	"Force direct entry");

	SERIALISE_BOOL(serialiser, m_PedFlags.phoneDisableCameraAnimations,			"Phone Disable Camera Animations");
	SERIALISE_BOOL(serialiser, m_PedFlags.notAllowedToJackAnyPlayers,			"Not Allowed To Jack Any Players");

	SERIALISE_BOOL(serialiser, m_PedFlags.runFromFiresAndExplosions,			"Run From Fires And Explosions");
	SERIALISE_BOOL(serialiser, m_PedFlags.ignoreBeingOnFire,					"Ignore Being On Fire");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableWantedHelicopterSpawning,		"Disable Wanted Helicopters");
	SERIALISE_BOOL(serialiser, m_PedFlags.checklockedbeforewarp,				"Will check when entering a vehicle if it is locked before warping");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontShuffleInVehicleToMakeRoom,		"Don't shuffle to make room for another entering player");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableForcedEntryForOpenVehicles,	"Disable Forced Entry For Open Vehicles From Try Locked Door");
	SERIALISE_BOOL(serialiser, m_PedFlags.giveWeaponOnGetup,					"Give the ped a weapon to use once their weapon is removed for getups");
	SERIALISE_BOOL(serialiser, m_PedFlags.ignoreNetSessionFriendlyFireCheckForAllowDamage, "Ignore Net friendly fire check for allow damage");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontLeaveCombatIfTargetPlayerIsAttackedByPolice, "Dont leave combat if target player gets attacked by cops");
	SERIALISE_BOOL(serialiser, m_PedFlags.useTargetPerceptionForCreatingAimedAtEvents,		"Use Target Perception For AimedAt Events");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableHomingMissileLockon,			"Disable homing missile lokcon");
	SERIALISE_BOOL(serialiser, m_PedFlags.forceIgnoreMaxMeleeActiveSupportCombatants, "Ignore max melee support combatants count");
	SERIALISE_BOOL(serialiser, m_PedFlags.stayInDefensiveAreaWhenInVehicle,		"Stay in defensive area in vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontShoutTargetPosition,				"Dont transmit target position to friendlies");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventExitVehicleDueToInvalidWeapon,	"Prevent exit vehicle due to invalid weapon");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableHelmetArmor,					"Apply full damage on headshot, even if ped has helmet");

	SERIALISE_BOOL(serialiser, m_PedFlags.dontHitVehicleWithProjectiles,		"Ped fired projectiles will ignore the vehicle they are in");

	SERIALISE_BOOL(serialiser, m_PedFlags.upperBodyDamageAnimsOnly,				"Upper Body Damage Anims Only");

	SERIALISE_BOOL(serialiser, m_PedFlags.listenToSoundEvents,					"Listen To Sound Events");

	SERIALISE_BOOL(serialiser, m_PedFlags.openDoorArmIK,						"Open Door Arm IK");

	SERIALISE_BOOL(serialiser, m_PedFlags.dontBlipCop,							"Don't Blip Cop");
	SERIALISE_BOOL(serialiser, m_PedFlags.forceSkinCharacterCloth,				"Force Skin Character Cloth");
	SERIALISE_BOOL(serialiser, m_PedFlags.phoneDisableTalkingAnimations,		"Phone Disable Talking Animations");
	SERIALISE_BOOL(serialiser, m_PedFlags.lowerPriorityOfWarpSeats,				"Lower Priority Of Warp Seats");

	SERIALISE_BOOL(serialiser, m_PedFlags.dontBehaveLikeLaw,					"Don't Behave Like Law");
	SERIALISE_BOOL(serialiser, m_PedFlags.phoneDisableTextingAnimations,		"Phone Disable Texting Animations");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontActivateRagdollFromBulletImpact,	"Don't Activate Ragdoll From Bullet Impact");
	SERIALISE_BOOL(serialiser, m_PedFlags.checkLOSForSoundEvents,				"Check LOS For Sound Events");

	SERIALISE_BOOL(serialiser, m_PedFlags.disablePanicInVehicle,				"Disable Panic In Vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.forceRagdollUponDeath,				"Force Ragdoll Upon Death");
	SERIALISE_BOOL(serialiser, m_PedFlags.pedIgnoresAnimInterruptEvents,		"Ped Ignores Anim Interrupt Events");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontBlip,								"Don't Blip");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventAutoShuffleToDriverSeat,		"Prevent Auto Shuffle To Driver Seat");
	SERIALISE_BOOL(serialiser, m_PedFlags.canBeAgitated,						"Can Be Agitated");
    SERIALISE_BOOL(serialiser, m_PedFlags.AIDriverAllowFriendlyPassengerSeatEntry, "AIDriverAllowFriendlyPassengerSeatEntry");
	SERIALISE_BOOL(serialiser, m_PedFlags.onlyWritheFromWeaponDamage,			"OnlyWritheFromWeaponDamage");
	SERIALISE_BOOL(serialiser, m_PedFlags.shouldChargeNow,						"Should Charge Now");
    SERIALISE_BOOL(serialiser, m_PedFlags.disableJumpingFromVehiclesAfterLeader,"Disable Jumping From Vehicles After Leader");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventUsingLowerPrioritySeats,		"Prevent Using Lower Priority Seats");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventDraggedOutOfCarThreatResponse, "Prevent Ped From Reacting to being dragged out of car");
	SERIALISE_BOOL(serialiser, m_PedFlags.canAttackNonWantedPlayerAsLaw,		"Can Attack Non-wanted Player As Law");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableMelee,							"Disable Melee");
	SERIALISE_BOOL(serialiser, m_PedFlags.preventAutoShuffleToTurretSeat,		"Prevent Auto Shuffle To Turret Seat");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableEventInteriorStatusCheck,		"Disable Event Interior Status Check");
	SERIALISE_BOOL(serialiser, m_PedFlags.onlyUpdateTargetWantedIfSeen,			"Only Update Target Wanted If Seen");
	SERIALISE_BOOL(serialiser, m_PedFlags.treatDislikeAsHateWhenInCombat,		"Treat Dislike As Hate When In Combat");
	SERIALISE_BOOL(serialiser, m_PedFlags.treatNonFriendlyAsHateWhenInCombat,	"Treat Non-Friendly peds As Hated When In Combat");
    SERIALISE_BOOL(serialiser, m_PedFlags.preventReactingToCloneBullets,		"Prevent ped From reacting to clone bullets");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableAutoEquipHelmetsInBikes,		"Disable Auto Equip Helmets In Bikes");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableAutoEquipHelmetsInAircraft,	"Disable Auto Equip Helmets In Aircraft");
	SERIALISE_BOOL(serialiser, m_PedFlags.stopWeaponFiringOnImpact,				"Stop Weapon Firing On Impact");
	SERIALISE_BOOL(serialiser, m_PedFlags.keepWeaponHolsteredUnlessFired,		"Keep Weapon Holstered Unless Fired");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableExplosionReactions,			"Disable Explosion Reactions");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontActivateRagdollFromExplosions,	"Don't Activate Ragdoll From Explosions");
	SERIALISE_BOOL(serialiser, m_PedFlags.avoidTearGas,							"Avoid Tear Gas");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableHornAudioWhenDead,				"Avoid using the horn when the ped dies and rests his head against the wheel");
	SERIALISE_BOOL(serialiser, m_PedFlags.ignoredByAutoOpenDoors,				"Ignored By Auto Open Doors");
	SERIALISE_BOOL(serialiser, m_PedFlags.activateRagdollWhenVehicleUpsideDown,	"Activate Ragdoll When Vehicle Upside Down");
	SERIALISE_BOOL(serialiser, m_PedFlags.allowNearbyCoverUsage,				"Allow Nearby Cover Usage");
	SERIALISE_BOOL(serialiser, m_PedFlags.cowerInsteadOfFlee,					"Cower Instead Of Flee");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableGoToWritheWhenInjured,			"Disable Go To Writhe When Injured");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockDroppingHealthSnacksOnDeath,		"Block Dropping Health Snacks On Death");
	SERIALISE_BOOL(serialiser, m_PedFlags.willTakeDamageWhenVehicleCrashes,		"Will Take Damage When Vehicle Crashes");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontRespondToRandomPedsDamage,		"Don't Respond To Random Peds Damage");
	SERIALISE_BOOL(serialiser, m_PedFlags.broadcastRepondedToThreatWhenGoingToPointShooting, "Whenever the ped starts shooting while going to a point, it trigger a responded to threat broadcast");	
	SERIALISE_BOOL(serialiser, m_PedFlags.dontTakeOffHelmet,					"Don't Take Off Helmet");
	SERIALISE_BOOL(serialiser, m_PedFlags.allowContinuousThreatResponseWantedLevelUpdates, "Allow Continuous Threat Response Wanted Level Updates");
	SERIALISE_BOOL(serialiser, m_PedFlags.keepTargetLossResponseOnCleanup,		"Keep Target Loss Response On Cleanup");
	SERIALISE_BOOL(serialiser, m_PedFlags.forcedToStayInCover,					"Forced To Stay In Cover");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockPedFromTurningInCover,			"Block Ped From Turning In Cover");
	SERIALISE_BOOL(serialiser, m_PedFlags.teleportToLeaderVehicle,				"Teleport To Leader Vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontLeaveVehicleIfLeaderNotInVehicle,	"Don't Leave Vehicle If Leader Not In Vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.forcePedToFaceLeftInCover,			"Force Ped To Face Left In Cover");
	SERIALISE_BOOL(serialiser, m_PedFlags.forcePedToFaceRightInCover,			"Force Ped To Face Right In Cover");
	SERIALISE_BOOL(serialiser, m_PedFlags.useNormalExplosionDamageWhenBlownUpInVehicle,	"Use Normal Explosion Damage When Blown Up In Vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.useGoToPointForScenarioNavigation,	"Use GoToPoint For Scenario Navigation");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontClearLocalPassengersWantedLevel, 	"Don't clear local passangers wanted level");
	SERIALISE_BOOL(serialiser, m_PedFlags.hasBareFeet,							"Has bare feet");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockAutoSwapOnWeaponPickups,			"Blocks auto swapping on weapon pickup");
	SERIALISE_BOOL(serialiser, m_PedFlags.isPriorityTargetForAI,            	"Is priority target for AI");
	SERIALISE_BOOL(serialiser, m_PedFlags.hasHelmet,							"Has helmet");
	SERIALISE_BOOL(serialiser, m_PedFlags.isSwitchingHelmetVisor,				"Is playing switch visor anim");
	SERIALISE_BOOL(serialiser, m_PedFlags.forceHelmetVisorSwitch,				"Force ped to do visor switch");
	SERIALISE_BOOL(serialiser, m_PedFlags.isPerformingVehMelee,					"Performing vehicle melee");
    SERIALISE_BOOL(serialiser, m_PedFlags.useOverrideFootstepPtFx,				"Use override footstep PTFX");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontAttackPlayerWithoutWantedLevel,	"Ped won't attack non-wanted players");
    SERIALISE_BOOL(serialiser, m_PedFlags.disableShockingEvents,	            "Disable Shocking Events");
	SERIALISE_BOOL(serialiser, m_PedFlags.treatAsFriendlyForTargetingAndDamage,	"Treat as Friendly for Targeting and Damage");
	SERIALISE_BOOL(serialiser, m_PedFlags.allowBikeAlternateAnimations,			"Allow bike alternate animations");
	SERIALISE_BOOL(serialiser, m_PedFlags.useLockpickVehicleEntryAnimations,	"Use alternate lockpicking animations for forced entry");
	SERIALISE_BOOL(serialiser, m_PedFlags.ignoreInteriorCheckForSprinting,		"Ignore Interior Check For Spinting");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontActivateRagdollFromVehicleImpact,	"Don't Activate Ragdoll From Vehicle Impact");
    SERIALISE_BOOL(serialiser, m_PedFlags.avoidanceIgnoreAll,	                "Avoidance Ignore All");
	SERIALISE_BOOL(serialiser, m_PedFlags.pedsJackingMeDontGetIn,	            "Peds Jacking Me don't Get In");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableTurretOrRearSeatPreference,	"Disable Turrent Or Rear Seat Preference");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableHurt,							"Disable Hurt");
	SERIALISE_BOOL(serialiser, m_PedFlags.allowTargettingInVehicle,				"Allow Targetting In Vehicle");
	SERIALISE_BOOL(serialiser, m_PedFlags.firesDummyRockets,				    "Fires dummy rockets");
	SERIALISE_BOOL(serialiser, m_PedFlags.decoyPed,								"Creates a decoy ped");
	SERIALISE_BOOL(serialiser, m_PedFlags.hasEstablishedDecoy,					"Prevents dispatched helicopters from landing");
	SERIALISE_BOOL(serialiser, m_PedFlags.disableInjuredCryForHelpEvents,		"Disabled Injured Cry For Help Events");
	SERIALISE_BOOL(serialiser, m_PedFlags.dontCryForHelpOnStun,					"Will prevent peds from crying for help when shot with the stun gun");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockDispatchedHelicoptersFromLanding,"Block dispatched helis from landing");
	SERIALISE_BOOL(serialiser, m_PedFlags.ragdollFloatsIndefinitely,			"Prevents a dead ped from sinking");
	SERIALISE_BOOL(serialiser, m_PedFlags.blockElectricWeaponDamage,			"Blocks electric weapon damage");
	// reset flags
	SERIALISE_BOOL(serialiser, m_PedFlags.PRF_disableInVehicleActions,			"PRF Disable In Vehicle Actions");
	SERIALISE_BOOL(serialiser, m_PedFlags.PRF_IgnoreCombatManager,				"PRF DIgnore Combat Manager");

	SERIALISE_BOOL(serialiser, m_isPainAudioDisabled,                           "Is Pain Audio Disabled");
	SERIALISE_BOOL(serialiser, m_isAmbientSpeechDisabled,                       "Is Speech Disabled");

	SERIALISE_COMBAT_BEHAVIOR_FLAGS(serialiser, m_combatBehaviorFlags,			"Combat Behavior Flags");

	SERIALISE_UNSIGNED(serialiser, m_targetLossResponse, datBitsNeeded<CCombatData::TargetLossResponse_MAX_VALUE>::COUNT, "Target Loss Response");

	SERIALISE_UNSIGNED(serialiser, m_fleeBehaviorFlags,	SIZEOF_FLEE_FLAGS, 		"Flee Flags");

	SERIALISE_UNSIGNED(serialiser, m_NavCapabilityFlags,SIZEOF_NAV_CAP_FLAGS, 	"Nav Capability Flags");
	
	SERIALISE_UNSIGNED(serialiser, m_PedFlags.cantBeKnockedOffBike,	SIZEOF_CANTBEKNOCKEDOFFBIKE, "Can't be knocked off bike");

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_shootRate, MAX_SYNCED_SHOOT_RATE, SIZEOF_SHOOT_RATE, "Shoot Rate");

	if (m_PedFlags.moneyHasBeenGivenByScript || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_pedCash, SIZEOF_MONEY_CARRIED, 		"Money Carried");
	}
	else
	{
		m_pedCash = 0;
	}

	bool isTargettable = m_isTargettableByTeam != 0;

	SERIALISE_BOOL(serialiser, isTargettable);

	if (isTargettable || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_isTargettableByTeam, MAX_NUM_TEAMS, "Is Targettable by Team");
	}
	else
	{
		m_isTargettableByTeam   = 0;
	}

	SERIALISE_INTEGER(serialiser, m_minOnGroundTimeForStun,	SIZEOF_MIN_GROUND_STUN_TIME, "Minimum On Ground Time For Stun Gun");
	SERIALISE_UNSIGNED(serialiser, m_combatMovement,			SIZEOF_COMBAT_MOVEMENT, "Combat Movement");

	bool hasRBF = m_ragdollBlockingFlags != 0;
	SERIALISE_BOOL(serialiser, hasRBF);
	if(hasRBF || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_ragdollBlockingFlags, SIZEOF_RBF, "Ragdoll Blocking Flags");
	}
	else
	{
		m_ragdollBlockingFlags = 0;
	}

	bool bHasAmmoToDrop = m_ammoToDrop != 0;

	SERIALISE_BOOL(serialiser, bHasAmmoToDrop);

	if (bHasAmmoToDrop || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_ammoToDrop, SIZEOF_AMMO_TO_DROP, "Ammo To Drop");
	}
	else
	{
		m_ammoToDrop = 0;
	}

    SERIALISE_BOOL(serialiser, m_HasDefensiveArea, "Has Defensive Area");

    if(m_HasDefensiveArea || serialiser.GetIsMaximumSizeSerialiser())
    {
		const unsigned SIZEOF_DEFENSIVE_AREA_RADIUS = 12;
		const unsigned SIZEOF_DEFENSIVE_AREA_TYPE   = 8;
		const float    MAX_DEFENSIVE_AREA_RADIUS    = 760.0f;	

		SERIALISE_UNSIGNED(serialiser, m_DefensiveAreaType, SIZEOF_DEFENSIVE_AREA_TYPE, "Defensive Area Type");

		if (m_DefensiveAreaType == CDefensiveArea::AT_AngledArea || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_POSITION(serialiser, m_AngledDefensiveAreaV1, "Angled Defensive Area V1");
			SERIALISE_POSITION(serialiser, m_AngledDefensiveAreaV2, "Angled Defensive Area V2"); 
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_AngledDefensiveAreaWidth, MAX_DEFENSIVE_AREA_RADIUS, SIZEOF_DEFENSIVE_AREA_RADIUS, "Defensive Area Width");
		}
		else if (m_DefensiveAreaType == CDefensiveArea::AT_Sphere)
		{					
			SERIALISE_POSITION(serialiser, m_DefensiveAreaCentre, "Defensive Area Centre");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_DefensiveAreaRadius, MAX_DEFENSIVE_AREA_RADIUS, SIZEOF_DEFENSIVE_AREA_RADIUS, "Defensive Area Radius");			
		}
		
		SERIALISE_BOOL(serialiser, m_UseCentreAsGotoPos, "Use Defensive Area Centre As Go To Position");       
    }

	bool bHasVehicleWeaponIndex = (m_vehicleweaponindex != -1);

	SERIALISE_BOOL(serialiser, bHasVehicleWeaponIndex);

	if(bHasVehicleWeaponIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_vehicleweaponindex, SIZEOF_VWI_DATA, "Vehicle Weapon Index");
	}
	else
	{
		m_vehicleweaponindex = -1;
	}

	m_PedVehicleEntryScriptConfig.Serialise(serialiser);

	bool bHasSeatIndexForGroup = (m_SeatIndexToUseInAGroup != -1);

	SERIALISE_BOOL(serialiser, bHasSeatIndexForGroup);

	if(bHasSeatIndexForGroup || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_SeatIndexToUseInAGroup, SIZEOF_SEAT_INDEX);
	}
	else
	{
		m_SeatIndexToUseInAGroup = -1;
	}

	SERIALISE_BOOL(serialiser, m_HasInVehicleContextHash);
	if(m_HasInVehicleContextHash || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_inVehicleContextHash, SIZEOF_CONTEXT_HASH, "In Vehicle Context Hash");
	}
	else
	{
		m_inVehicleContextHash = 0;
	}

	SERIALISE_INTEGER(serialiser, m_uMaxNumFriendsToInform, CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_NUM_BITS, "Max num friends to inform");
	SERIALISE_PACKED_FLOAT(serialiser, m_fMaxInformFriendDistance, CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE, CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_NUM_BITS, "Max inform friend distance");
	
	SERIALISE_PACKED_FLOAT(serialiser, m_fTimeBetweenAggressiveMovesDuringVehicleChase, 300.0f, 16, "Time between aggressive moves during vehicle chase");
	SERIALISE_PACKED_FLOAT(serialiser, m_fMaxVehicleTurretFiringRange, 300.0f, 16, "Max firing range for ped in a turret vehicle seat");
	SERIALISE_PACKED_FLOAT(serialiser, m_fMaxShootingDistance, 10000.f, SIZEOF_SHOOTING_DISTANCE, "Shooting Distance");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fBurstDurationInCover, 60.f, SIZEOF_MINUTE_IN_SECONDS, "Burst Duration In Cover");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTimeBetweenBurstsInCover, 60.f, SIZEOF_MINUTE_IN_SECONDS, "Time Between Bursts In Cover");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTimeBetweenPeeks, 60.f, SIZEOF_MINUTE_IN_SECONDS, "Time Between Peeks");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fAccuracy, 1.0f, SIZEOF_ACCURACY, "Accuracy");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fBlindFireChance, 1.0f, SIZEOF_MINUTE_IN_SECONDS, "Chance of doing a blind fire from cover");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fStrafeWhenMovingChance, 1.0f, SIZEOF_MINUTE_IN_SECONDS, "Chance of doing a blind fire from cover");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fWeaponDamageModifier, 10.0f, SIZEOF_WEAPON_DAMAGE_MODIFIER, "Multiplies the weapon damage dealt by the ped");

	SERIALISE_PACKED_FLOAT(serialiser, m_fHomingRocketBreakLockAngle, 1.0f, SIZEOF_ACCURACY, "Homing rocket break lock angle");
	SERIALISE_PACKED_FLOAT(serialiser, m_fHomingRocketBreakLockAngleClose, 1.0f, SIZEOF_ACCURACY, "Homing rocket break lock angle close");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fHomingRocketBreakLockCloseDistance, 10000.f, SIZEOF_SHOOTING_DISTANCE, "Homing rocket break lock distance");

	bool hasFiringPattern = m_FiringPatternHash != 0;
	SERIALISE_BOOL(serialiser, hasFiringPattern, "Has Firing Pattern");
	if(hasFiringPattern || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_FiringPatternHash, 32, "Firing Pattern");
	}
	else
	{
		m_FiringPatternHash = 0;
	}
}

bool CPedScriptGameStateDataNode::ResetScriptStateInternal()
{
	m_popType										= POPTYPE_RANDOM_AMBIENT;

	m_pedType										= (u32) PEDTYPE_LAST_PEDTYPE; //essentially don't reset the remote type

	m_PedFlags.neverTargetThisPed         			= false;
	m_PedFlags.blockNonTempEvents         			= false;
	m_PedFlags.noCriticalHits             			= false;
	m_PedFlags.isPriorityTarget           			= false;
	m_PedFlags.doesntDropWeaponsWhenDead  			= false;
	m_PedFlags.dontDragMeOutOfCar         			= false;
	m_PedFlags.playersDontDragMeOutOfCar         	= false;
	m_PedFlags.drownsInWater              			= false;
	m_PedFlags.drownsInSinkingVehicle      			= false;
	m_PedFlags.forceDieIfInjured          			= false;
	m_PedFlags.fallOutOfVehicleWhenKilled 			= false;
	m_PedFlags.diesInstantlyInWater       			= false;
	m_PedFlags.justGetsPulledOutWhenElectrocuted	= false;
	m_PedFlags.dontRagdollFromPlayerImpact			= false;
	m_PedFlags.lockAnim								= false;
	m_PedFlags.preventPedFromReactingToBeingJacked  = false;
	m_PedFlags.cantBeKnockedOffBike					= KNOCKOFFVEHICLE_DEFAULT;
	m_PedFlags.canDrivePlayerAsRearPassenger		= false;

	m_PedFlags.disableLockonToRandomPeds			= false;
	m_PedFlags.willNotHotwireLawEnforcementVehicle	= false;
	m_PedFlags.willCommandeerRatherThanJack			= false;
	m_PedFlags.useKinematicModeWhenStationary		= false;
	m_PedFlags.forcedToUseSpecificGroupSeatIndex	= false;
	m_PedFlags.playerCanJackFriendlyPlayers			= false;
	m_PedFlags.willFlyThroughWindscreen				= false;
	m_PedFlags.getOutBurningVehicle					= false;
	m_PedFlags.disableLadderClimbing				= false;
    m_PedFlags.forceDirectEntry                     = false;
	m_PedFlags.phoneDisableCameraAnimations			= false;
	m_PedFlags.notAllowedToJackAnyPlayers			= false;
	m_PedFlags.runFromFiresAndExplosions			= false;
	m_PedFlags.ignoreBeingOnFire					= false;
	m_PedFlags.disableWantedHelicopterSpawning		= false;
	m_PedFlags.checklockedbeforewarp				= false;
	m_PedFlags.dontShuffleInVehicleToMakeRoom		= false;
	m_PedFlags.disableForcedEntryForOpenVehicles	= false;
	m_PedFlags.giveWeaponOnGetup					= false;
	m_PedFlags.dontHitVehicleWithProjectiles		= false;
	m_PedFlags.ignoreNetSessionFriendlyFireCheckForAllowDamage = false;
	m_PedFlags.dontLeaveCombatIfTargetPlayerIsAttackedByPolice = false;
	m_PedFlags.useTargetPerceptionForCreatingAimedAtEvents = false;
	m_PedFlags.disableHomingMissileLockon			= false;
	m_PedFlags.forceIgnoreMaxMeleeActiveSupportCombatants = false;
	m_PedFlags.activateRagdollWhenVehicleUpsideDown	= false;
	m_PedFlags.upperBodyDamageAnimsOnly				= false;
	m_PedFlags.listenToSoundEvents					= false;
	m_PedFlags.openDoorArmIK						= false;
	m_PedFlags.dontBlipCop							= false;
	m_PedFlags.forceSkinCharacterCloth				= false;
	m_PedFlags.phoneDisableTalkingAnimations		= false;
	m_PedFlags.lowerPriorityOfWarpSeats				= false;
	m_PedFlags.preventDraggedOutOfCarThreatResponse = false;

	m_PedFlags.dontBehaveLikeLaw					= false;
	m_PedFlags.phoneDisableTextingAnimations		= false;
	m_PedFlags.dontActivateRagdollFromBulletImpact	= false;
	m_PedFlags.checkLOSForSoundEvents				= false;

	m_PedFlags.disablePanicInVehicle				= false;
	m_PedFlags.forceRagdollUponDeath				= false;

	m_PedFlags.pedIgnoresAnimInterruptEvents		= false;

	m_PedFlags.dontBlip								= false;

	m_PedFlags.preventAutoShuffleToDriverSeat		= false;
	m_PedFlags.preventAutoShuffleToTurretSeat		= false;

	m_PedFlags.disableEventInteriorStatusCheck		= false;

	m_PedFlags.canBeAgitated						= false;

    m_PedFlags.AIDriverAllowFriendlyPassengerSeatEntry = false;

	m_PedFlags.stopWeaponFiringOnImpact				= false;

	m_PedFlags.keepWeaponHolsteredUnlessFired		= false;

	m_PedFlags.disableExplosionReactions			= false;

	m_PedFlags.dontActivateRagdollFromExplosions	= false;

	m_PedVehicleEntryScriptConfig.Reset();

	m_PedFlags.avoidTearGas							= false;

	m_PedFlags.disableHornAudioWhenDead = true;

	m_PedFlags.ignoredByAutoOpenDoors				= false;

	m_PedFlags.activateRagdollWhenVehicleUpsideDown = false;

	m_PedFlags.allowNearbyCoverUsage				= false;

	m_PedFlags.cowerInsteadOfFlee					= false;

	m_PedFlags.disableGoToWritheWhenInjured			= false;

	m_PedFlags.blockDroppingHealthSnacksOnDeath		= false;

	m_PedFlags.willTakeDamageWhenVehicleCrashes		= true;

	m_PedFlags.dontRespondToRandomPedsDamage		= false;

	m_PedFlags.broadcastRepondedToThreatWhenGoingToPointShooting = false;

	m_PedFlags.dontTakeOffHelmet					= false;

	m_PedFlags.allowContinuousThreatResponseWantedLevelUpdates = false;

	m_PedFlags.forcedToStayInCover					= false;

	m_PedFlags.blockPedFromTurningInCover			= false;

	m_PedFlags.teleportToLeaderVehicle				= false;

	m_PedFlags.forcePedToFaceLeftInCover			= false;

	m_PedFlags.forcePedToFaceRightInCover			= false;

	//PRF
	m_PedFlags.PRF_disableInVehicleActions			= false;
	m_PedFlags.PRF_IgnoreCombatManager				= false;

	m_PedFlags.hasBareFeet                          = false;

	m_PedFlags.blockAutoSwapOnWeaponPickups         = false;

	m_PedFlags.isPriorityTargetForAI				= false;

	m_PedFlags.hasHelmet							= false;

	m_PedFlags.isSwitchingHelmetVisor				= false;

	m_PedFlags.forceHelmetVisorSwitch				= false;

	m_PedFlags.isPerformingVehMelee					= false;

	m_PedFlags.dontAttackPlayerWithoutWantedLevel	= false;

	m_PedFlags.treatAsFriendlyForTargetingAndDamage = false;

	m_PedFlags.allowBikeAlternateAnimations			= false;

	m_PedFlags.useLockpickVehicleEntryAnimations	= false;

	m_PedFlags.ignoreInteriorCheckForSprinting		= false;
	
	m_PedFlags.dontActivateRagdollFromVehicleImpact = false;

	m_PedFlags.avoidanceIgnoreAll					= false;

	m_PedFlags.pedsJackingMeDontGetIn				= false;

	m_PedFlags.disableTurretOrRearSeatPreference	= false;

	m_PedFlags.disableHurt							= false;

	m_PedFlags.allowTargettingInVehicle				= false;

	m_PedFlags.firesDummyRockets					= false;

	m_PedFlags.decoyPed								= false;

	m_PedFlags.hasEstablishedDecoy					= false;

	m_PedFlags.disableInjuredCryForHelpEvents		= false;

	m_PedFlags.dontCryForHelpOnStun					= false;

	m_PedFlags.blockDispatchedHelicoptersFromLanding= false;

	m_isPainAudioDisabled							= false;

	m_isAmbientSpeechDisabled						= false;

	m_PedFlags.disableBloodPoolCreation				= false;

	m_PedFlags.ragdollFloatsIndefinitely			= false;

	m_PedFlags.blockElectricWeaponDamage			= false;

	m_shootRate										= 1.0f;
	m_pedCash										= 0;
	m_isTargettableByTeam							= (1<<MAX_NUM_TEAMS)-1;
	m_minOnGroundTimeForStun						= -1; 
	m_combatMovement								= CCombatData::CM_Stationary; /* 0 */

	m_ragdollBlockingFlags							= 0;

    m_HasDefensiveArea								= false;

	m_SeatIndexToUseInAGroup						= -1;

	m_HasInVehicleContextHash						= false;
	m_inVehicleContextHash							= 0;

	return true;
}

void CPedOrientationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_HEADING = 8;

	SERIALISE_PACKED_FLOAT(serialiser, m_currentHeading, TWO_PI, SIZEOF_HEADING, "Current Heading");
	SERIALISE_PACKED_FLOAT(serialiser, m_desiredHeading, TWO_PI, SIZEOF_HEADING, "Desired Heading");
}

void CPedHealthDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_ENDURANCE = 13;
	static const unsigned int SIZEOF_ARMOUR = 13;
	static const unsigned int SIZEOF_HEALTH = 13;
	static const unsigned int SIZEOF_WEAPON_HASH = 32;//6; this is a hash now!
	static const unsigned int SIZEOF_HURT_END_TIME  = 2; // 0, 1 or 2 (only need to know if it's off, odd or even on a clone)...
	static const unsigned int SIZEOF_WEAPON_DAMAGE_COMPONENT = 8;

	SERIALISE_BOOL(serialiser, m_hasMaxHealth, "Has max health");
	SERIALISE_BOOL(serialiser, m_maxHealthSetByScript, "Max health set by script");

	if (m_maxHealthSetByScript || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_scriptMaxHealth, SIZEOF_HEALTH, "Script Max Health");
	}

	if (!m_hasMaxHealth || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_health, SIZEOF_HEALTH, "Health");
		SERIALISE_BOOL(serialiser, m_killedWithHeadShot, "Killed with headshot");
		SERIALISE_BOOL(serialiser, m_killedWithMeleeDamage, "Killed with Melee Damage");
	}
	else
	{
		m_killedWithHeadShot = false;
		m_killedWithMeleeDamage = false;
	}

	SERIALISE_BOOL(serialiser, m_hasDefaultArmour, "Has default armour");
	if (!m_hasDefaultArmour || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_armour, SIZEOF_ARMOUR, "Armour");
	}
	else
	{
		m_armour = 0;
	}

	SERIALISE_BOOL(serialiser, m_hasMaxEndurance, "Has max endurance");
	SERIALISE_BOOL(serialiser, m_maxEnduranceSetByScript, "Max endurance set by script");

	if (m_maxEnduranceSetByScript || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_scriptMaxEndurance, SIZEOF_ENDURANCE, "Script Max Endurance");
	}

	if (!m_hasMaxEndurance || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_endurance, SIZEOF_ENDURANCE, "Endurance");
	}

	bool bHasDamageEntity = m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID;
	SERIALISE_BOOL(serialiser, bHasDamageEntity, "Has damage entity");
	if (bHasDamageEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_weaponDamageEntity, "Weapon Damage Entity");
	}
	else
	{
		m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
	}

	SERIALISE_UNSIGNED(serialiser, m_weaponDamageHash, SIZEOF_WEAPON_HASH, "Weapon Damage Hash");
	SERIALISE_BOOL(serialiser, m_hurtStarted, "Hurt Started");
	SERIALISE_UNSIGNED(serialiser, m_hurtEndTime, SIZEOF_HURT_END_TIME, "Hurt End Time");

	bool bHasWeaponDamageComponent = m_weaponDamageComponent != -1;
	SERIALISE_BOOL(serialiser, bHasWeaponDamageComponent, "Has weapon damage component");
	if (bHasWeaponDamageComponent || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_weaponDamageComponent, SIZEOF_WEAPON_DAMAGE_COMPONENT, "Weapon Damage Component");
	}
	else
	{
		m_weaponDamageComponent = -1;
	}
}

bool CPedHealthDataNode::HasDefaultState() const 
{ 
	return (m_hasMaxHealth && m_hasDefaultArmour && m_hasMaxEndurance);
}

void CPedHealthDataNode::SetDefaultState() 
{
	m_killedWithMeleeDamage = false;
	m_killedWithHeadShot = false;
	m_hasMaxHealth       = true; 
	m_hasDefaultArmour   = true;
	m_hasMaxEndurance 	 = true;
	m_armour             = 0; 
	m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
	m_weaponDamageHash   = 0;
	m_hurtStarted		 = false;
	m_hurtEndTime		 = 0;
	m_weaponDamageComponent = -1;
}

void CPedAttachDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetAttachmentData(*this);

	m_attached = (m_attachObjectID != NETWORK_INVALID_OBJECT_ID);
}

void CPedAttachDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetAttachmentData(*this);
}

void CPedAttachDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_ATTACH_OFFSET  = 15;
	static const float		  MAX_ATTACH_OFFSET		= 80.0f;
	static const unsigned int SIZEOF_ATTACH_QUAT    = 16;
	static const unsigned int SIZEOF_ATTACH_HEADING = 8;
	static const unsigned int SIZEOF_ATTACH_BONE    = 8;
	static const unsigned int SIZEOF_ATTACH_FLAGS   = NUM_EXTERNAL_ATTACH_FLAGS;

	bool bHasAttachmentOffset = !m_attachOffset.IsZero();
	bool bHasAttachmentQuat = m_attachQuat.x != 0.0f || m_attachQuat.y != 0.0f || m_attachQuat.z != 0.0f;
	bool bHasAttachmentHeading = m_attachHeading != 0.0f || m_attachHeadingLimit != 0.0f;

	SERIALISE_BOOL(serialiser, m_attached, "Attached");

	if(m_attached || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_attachObjectID, "Attached To");

		SERIALISE_BOOL(serialiser, m_attachedToGround, "Attached To Ground");

		SERIALISE_BOOL(serialiser, bHasAttachmentOffset, "Has offset");

		if (bHasAttachmentOffset || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachOffset.x, MAX_ATTACH_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset X");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachOffset.y, MAX_ATTACH_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachOffset.z, MAX_ATTACH_OFFSET, SIZEOF_ATTACH_OFFSET, "Attachment Offset Z");
		}
		else
		{
			m_attachOffset.Zero();
		}

		SERIALISE_BOOL(serialiser, bHasAttachmentQuat, "Has orientation");

		if (bHasAttachmentQuat || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachQuat.x, 1.01f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion X");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachQuat.y, 1.01f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachQuat.z, 1.01f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion Z");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachQuat.w, 1.01f, SIZEOF_ATTACH_QUAT, "Attachment Quaternion W");
		}
		else
		{
			m_attachQuat.Identity();
		}

		SERIALISE_INTEGER(serialiser, m_attachBone,             SIZEOF_ATTACH_BONE,     "Attachment Bone");
		SERIALISE_UNSIGNED(serialiser, m_attachFlags,           SIZEOF_ATTACH_FLAGS,    "Attachment Flags");

		SERIALISE_BOOL(serialiser, bHasAttachmentHeading, "Has heading");

		if (bHasAttachmentHeading || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_attachHeading,      TWO_PI, SIZEOF_ATTACH_HEADING, "Attachment Heading");
			SERIALISE_PACKED_FLOAT(serialiser, m_attachHeadingLimit, TWO_PI, SIZEOF_ATTACH_HEADING, "Attachment Heading Limit");
		}
		else
		{
			m_attachHeading = m_attachHeadingLimit = 0.0f;
		}
	}
	else
	{
		m_attachObjectID = NETWORK_INVALID_OBJECT_ID;
	}
}

bool CPedAttachDataNode::HasDefaultState() const 
{ 
	return (m_attached == false); 
}

void CPedAttachDataNode::SetDefaultState() 
{ 
	m_attached = 0; 
}

////////////////////////////////////////////////////////////////////////////////////////////
//For syncing ped tennis motion 
////////////////////////////////////////////////////////////////////////////////////////////
void CSyncedTennisMotionData::Reset()
{
//Clear all except m_bAllowOverrideCloneUpdate
	m_Active	= false;

	m_ClipHash		= 0;
	m_DictHash		= 0;
	m_fStartPhase	= 0.0f;
	m_fPlayRate		= 0.0f;

	m_DiveMode			= false;
	m_DiveDirection		= false;
	m_fDiveHorizontal	= 0.0f;
	m_fDiveVertical		= 0.0f;
	m_bSlowBlend		= false;
}

void CSyncedTennisMotionData::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_bControlOutOfDeadZone, "Tennis Control Is Out of Dead Zone");
	SERIALISE_BOOL(serialiser, m_bAllowOverrideCloneUpdate, "Tennis Allow Clone Override");
	SERIALISE_BOOL(serialiser, m_Active, "Tennis motion active");
	if(m_Active || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_DiveMode, "Dive mode");
		SERIALISE_BOOL(serialiser, m_bSlowBlend, "Slow blend");

		if(!m_DiveMode || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_ClipHash, SIZEOF_ANIM, "Tennis swing anim anim");
			SERIALISE_UNSIGNED(serialiser, m_DictHash, SIZEOF_DICT, "Tennis swing anim dict");
			SERIALISE_PACKED_FLOAT(serialiser, m_fStartPhase,  1.1f, SIZEOF_PHASE, "Tennis swing start phase");
			m_fDiveHorizontal	= 0.0f;
			m_fDiveVertical		= 0.0f;
		}
		else
		{
			SERIALISE_BOOL(serialiser, m_DiveDirection, "Dive Direction");
			SERIALISE_PACKED_FLOAT(serialiser, m_fDiveHorizontal, 2.0f, SIZEOF_DIVEHDIR, "Tennis dive horizontal");
			SERIALISE_PACKED_FLOAT(serialiser, m_fDiveVertical, 2.0f, SIZEOF_DIVEVDIR, "Tennis dive vertical");
			m_ClipHash =0;
			m_DictHash =0;
			m_fStartPhase =0.0f;
		}

		SERIALISE_PACKED_FLOAT(serialiser, m_fPlayRate, 2.0f, SIZEOF_RATE, "Tennis anim rate");
	}
	else
	{
		Reset();
	}
}

PlayerFlags CPedMovementGroupDataNode::StopSend(const netSyncTreeTargetObject *pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	PlayerFlags playerFlags = 0;

    // don't try and sync an invalid movement group - examples of when
    // this can happen is when a ped is in a car or climbing
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);
    CPed *ped = netObjPed ? netObjPed->GetPed() : 0;

    if(ped
		&& (ped->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
		&& (ped->GetPrimaryMotionTask()->GetState()==CTaskMotionPed::State_AnimatedVelocity
		||  ped->GetPrimaryMotionTask()->GetState()==CTaskMotionPed::State_DoNothing) &&
        !ped->GetUsingRagdoll())
    {
		playerFlags = ~0U;
    }
	else if(netObjPed && netObjPed->ForceRagdollStateRequestPending())
    {
        playerFlags = ~0U;
    }
	else if (GetIsInCutscene(pObj))
	{
		playerFlags = ~0u;
	}

	return playerFlags;
}

void CPedMovementGroupDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_MOVE_BLEND_TYPE = datBitsNeeded<CPedMovementGroupDataNode::BlenderType_Invalid>::COUNT;
	static const unsigned int SIZEOF_PITCH = 8;

	bool bDefaultData = m_moveBlendType == CTaskTypes::TASK_MOTION_PED && m_moveBlendState == 1 && m_overriddenWeaponSetId.GetHash() == 0 && !m_isCrouching && !m_isStealthy && !m_isStrafing && !m_isRagdolling && !m_isRagdollConstraintAnkleActive && !m_isRagdollConstraintWristActive && m_overriddenStrafeSetId.GetHash() == 0;

	SERIALISE_MOTION_GROUP(serialiser, m_motionSetId, "Motion Group");

	SERIALISE_BOOL(serialiser, bDefaultData);

	if (!bDefaultData || serialiser.GetIsMaximumSizeSerialiser())
	{
		u16 blendType = PackMotionTaskType(m_moveBlendType);
		SERIALISE_UNSIGNED(serialiser, blendType, SIZEOF_MOVE_BLEND_TYPE, "Move Blend Type");
		UnpackMotionTaskType(blendType);

		SERIALISE_MOVE_BLEND_STATE(serialiser, m_moveBlendType, m_moveBlendState, "Move Blend State");
		SERIALISE_MOTION_GROUP(serialiser, m_overriddenWeaponSetId, "Overridden Weapon Group");
		SERIALISE_BOOL(serialiser, m_isCrouching, "Is Crouching");
		SERIALISE_BOOL(serialiser, m_isStealthy, "Is Stealthy");
		SERIALISE_BOOL(serialiser, m_isStrafing, "Is Strafing");
		SERIALISE_BOOL(serialiser, m_isRagdolling, "Is Ragdolling");
		SERIALISE_BOOL(serialiser, m_isRagdollConstraintAnkleActive, "Is Ragdoll Constraint Ankle Active");
		SERIALISE_BOOL(serialiser, m_isRagdollConstraintWristActive, "Is Ragdoll Constraint Wrist Active");

		bool bHasVehiclePitch = (m_motionInVehiclePitch != 0.f);
		SERIALISE_BOOL(serialiser, bHasVehiclePitch);
		if (bHasVehiclePitch || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_motionInVehiclePitch,	 1.0f,	 SIZEOF_PITCH, "Motion In Vehicle Pitch");
		}
		else
		{
			m_motionInVehiclePitch = 0.f;
		}

        bool hasOverriddenStrafeSet = m_overriddenStrafeSetId.GetHash() != 0;

        SERIALISE_BOOL(serialiser, hasOverriddenStrafeSet);

        if(hasOverriddenStrafeSet || serialiser.GetIsMaximumSizeSerialiser())
        {
            SERIALISE_MOTION_GROUP(serialiser, m_overriddenStrafeSetId, "Overridden Strafe Group");
        }
        else
        {
            m_overriddenStrafeSetId = CLIP_SET_ID_INVALID;
        }

		m_TennisMotionData.Serialise(serialiser);
	}
	else
	{
		m_moveBlendType = CTaskTypes::TASK_MOTION_PED;
		m_moveBlendState = 1; // on foot
		m_overriddenWeaponSetId = CLIP_SET_ID_INVALID;
        m_overriddenStrafeSetId = CLIP_SET_ID_INVALID;
		m_isCrouching = m_isStealthy = m_isStrafing = m_isRagdolling = m_isRagdollConstraintAnkleActive = m_isRagdollConstraintWristActive = false;

		m_motionInVehiclePitch = 0.f;

		m_TennisMotionData.Reset();
	}
}

u16 CPedMovementGroupDataNode::PackMotionTaskType(u16 const type) const
{
	switch(type)
	{
		case CTaskTypes::TASK_MOTION_PED:			return (u16)BlenderType_Ped;		
		case CTaskTypes::TASK_MOTION_PED_LOW_LOD:	return (u16)BlenderType_PedLowLod;	
		case CTaskTypes::TASK_MOTION_RIDE_HORSE:	return (u16)BlenderType_Horse;		
		case CTaskTypes::TASK_ON_FOOT_BIRD:		    return (u16)BlenderType_OnFootBird; 
	    case CTaskTypes::TASK_ON_FOOT_HORSE:		return (u16)BlenderType_OnFootHorse; 
		default: 
			break;
	}

	return (u16)BlenderType_Invalid;
}

void CPedMovementGroupDataNode::UnpackMotionTaskType(u16 const packed_type)
{
	switch(packed_type)
	{
		case BlenderType_Ped:			Assign(m_moveBlendType, CTaskTypes::TASK_MOTION_PED);			break;
		case BlenderType_PedLowLod:		Assign(m_moveBlendType, CTaskTypes::TASK_MOTION_PED_LOW_LOD);	break;
		case BlenderType_Horse:			Assign(m_moveBlendType, CTaskTypes::TASK_MOTION_RIDE_HORSE);	break;
		case BlenderType_OnFootHorse:	Assign(m_moveBlendType, CTaskTypes::TASK_ON_FOOT_HORSE);		break;
		case BlenderType_OnFootBird:	Assign(m_moveBlendType, CTaskTypes::TASK_ON_FOOT_BIRD);		    break;
		case BlenderType_Invalid:
		default: 
			m_moveBlendType = 0;
			break;
	}
}

void CPedMovementDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetPedMovementData(*this);
	m_HasDesiredMoveBlendRatioX = m_DesiredMoveBlendRatioX != 0.0f;
	m_HasDesiredMoveBlendRatioY = m_DesiredMoveBlendRatioY != 0.0f;
}

void CPedMovementDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetPedMovementData(*this);
}

void CPedMovementDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_MOVEBLENDRATIO = 8;
	static const unsigned int SIZEOF_PITCH          = 8;

	SERIALISE_BOOL(serialiser, m_HasDesiredMoveBlendRatioX, "Has Desired Move Blend Ratio X");
	SERIALISE_BOOL(serialiser, m_HasDesiredMoveBlendRatioY, "Has Desired Move Blend Ratio Y");

	if(m_HasDesiredMoveBlendRatioX || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_DesiredMoveBlendRatioX, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVEBLENDRATIO, "Desired Move Blend Ratio X");
	}
	else
	{
		m_DesiredMoveBlendRatioX = 0.0f;
	}

	if(m_HasDesiredMoveBlendRatioY || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_DesiredMoveBlendRatioY, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVEBLENDRATIO, "Desired Move Blend Ratio Y");
	}
	else
	{
		m_DesiredMoveBlendRatioY = 0.0f;
	}

    SERIALISE_BOOL(serialiser, m_HasStopped, "Has Ped Stopped");

	bool hasPitch = !IsClose(m_DesiredPitch, 0.0f, 0.01f);

	SERIALISE_BOOL(serialiser, hasPitch);

	if(hasPitch || serialiser.GetIsMaximumSizeSerialiser())
	{
		const float MAX_PITCH = PI;
		SERIALISE_PACKED_FLOAT(serialiser, m_DesiredPitch, MAX_PITCH, SIZEOF_PITCH, "Desired Pitch");
	}
	else
	{
		m_DesiredPitch = 0.0f;
	}

	bool hasMaxMoveBlendRatioChanged = !IsClose(m_MaxMoveBlendRatio, MOVEBLENDRATIO_SPRINT, 0.01f);
	SERIALISE_BOOL(serialiser, hasMaxMoveBlendRatioChanged, "Has Max Move Blend Ratio Changed");
	if(hasMaxMoveBlendRatioChanged || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_MaxMoveBlendRatio, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVEBLENDRATIO, "Max Move Blend Ratio");
	}
	else
	{
		m_MaxMoveBlendRatio = MOVEBLENDRATIO_SPRINT;
	}
}

bool CPedMovementDataNode::HasDefaultState() const 
{ 
	return (!m_HasDesiredMoveBlendRatioX && !m_HasDesiredMoveBlendRatioY && m_HasStopped && IsClose(m_DesiredPitch, 0.0f, 0.01f)); 
}

void CPedMovementDataNode::SetDefaultState() 
{ 
	m_HasDesiredMoveBlendRatioX = m_HasDesiredMoveBlendRatioY = false; 
    m_HasStopped = true;
}

CPedAppearanceDataNode::CPedAppearanceDataNode()
: m_PhoneMode(0),
  m_parachuteTintIndex(0),
  m_parachutePackTintIndex(0),
  m_facialIdleAnimOverrideClipNameHash(0),
  m_facialIdleAnimOverrideClipDictNameHash(0),
  m_isAttachingHelmet(false),
  m_isRemovingHelmet(false),
  m_isWearingHelmet(false),
  m_helmetTextureId(0),
  m_helmetProp(0),
  m_visorUpProp(0),
  m_visorDownProp(0),
  m_visorIsUp(false),
  m_supportsVisor(false),
  m_isVisorSwitching(false),
  m_targetVisorState(0)
{

}

void CPedAppearanceDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetPedAppearanceData(m_pedProps, m_variationData, m_PhoneMode, m_parachuteTintIndex, m_parachutePackTintIndex, m_facialClipSetId, m_facialIdleAnimOverrideClipNameHash, m_facialIdleAnimOverrideClipDictNameHash, m_isAttachingHelmet, m_isRemovingHelmet, m_isWearingHelmet, m_helmetTextureId, m_helmetProp, m_visorUpProp, m_visorDownProp, m_visorIsUp, m_supportsVisor, m_isVisorSwitching, m_targetVisorState);
}

void CPedAppearanceDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetPedAppearanceData(m_pedProps, m_variationData, m_PhoneMode, m_parachuteTintIndex, m_parachutePackTintIndex, m_facialClipSetId, m_facialIdleAnimOverrideClipNameHash, m_facialIdleAnimOverrideClipDictNameHash, m_isAttachingHelmet, m_isRemovingHelmet, m_isWearingHelmet, m_helmetTextureId, m_helmetProp, m_visorUpProp, m_visorDownProp, m_visorIsUp, m_supportsVisor, m_isVisorSwitching, m_targetVisorState);
}

void CPedAppearanceDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_PHONE_MODE = datBitsNeeded<CTaskMobilePhone::Mode_Max-1>::COUNT;
	static const unsigned int SIZE_OF_PARACHUTE_TINT_INDEX = 4; // 16 options...
	static const unsigned int SIZE_OF_PARACHUTE_PACK_TINT_INDEX = 4; // 16 options...
	NET_ASSERTS_ONLY(static const unsigned int MAX_PARACHUTE_TINT_INDEX = 1 << SIZE_OF_PARACHUTE_TINT_INDEX;)
	NET_ASSERTS_ONLY(static const unsigned int MAX_PARACHUTE_PACK_TINT_INDEX = 1 << SIZE_OF_PARACHUTE_PACK_TINT_INDEX;)
	static const unsigned int SIZEOF_HASH = 32;
	static const unsigned int SIZEOF_HELMET_TEXTURE = 8;
	static const unsigned int SIZEOF_HELMET_PROP = 10;

	m_pedProps.Serialise(serialiser);

	m_variationData.Serialise(serialiser);

	SERIALISE_UNSIGNED(serialiser, m_PhoneMode, SIZEOF_PHONE_MODE, "Phone mode");
	SERIALISE_BOOL(serialiser, m_isRemovingHelmet, "Is Removing Helmet");
	SERIALISE_BOOL(serialiser, m_isAttachingHelmet, "Is Attaching Helmet");
	SERIALISE_BOOL(serialiser, m_isWearingHelmet, "Is Wearing Helmet");
	SERIALISE_BOOL(serialiser, m_visorIsUp, "Is Switching Visor Down");
	SERIALISE_BOOL(serialiser, m_supportsVisor, "Is Switching Visor Up");
	SERIALISE_BOOL(serialiser, m_isVisorSwitching, "Has Take Off Helmet Task Started");
	SERIALISE_UNSIGNED(serialiser, m_targetVisorState, SIZEOF_HELMET_TEXTURE, "Target Visor State");

	if((m_isAttachingHelmet) || (m_isRemovingHelmet) || (m_isWearingHelmet) || (m_isVisorSwitching) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		SERIALISE_UNSIGNED(serialiser, m_helmetTextureId, SIZEOF_HELMET_TEXTURE, "Helmet Texture");
		SERIALISE_UNSIGNED(serialiser, m_helmetProp, SIZEOF_HELMET_PROP, "Helmet Prop");
		SERIALISE_UNSIGNED(serialiser, m_visorUpProp, SIZEOF_HELMET_PROP, "Visor Up Prop");
		SERIALISE_UNSIGNED(serialiser, m_visorDownProp, SIZEOF_HELMET_PROP, "Visor Down Prop");
	}

	bool hasTintIndex = ((m_parachuteTintIndex != 0) || (m_parachutePackTintIndex != 0));
	SERIALISE_BOOL(serialiser, hasTintIndex, "Has Parachute Tint Index");
	if(hasTintIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		gnetAssertf(m_parachuteTintIndex < MAX_PARACHUTE_TINT_INDEX,"CPedAppearanceDataNode::Serialise m_parachuteTintIndex[%d] >= MAX_PARACHUTE_TINT_INDEX[%d] -- increase SIZE_OF_PARACHUTE_TINT_INDEX[%d] - fix playersyncnodes and pedsyncnodes",m_parachuteTintIndex,MAX_PARACHUTE_TINT_INDEX,SIZE_OF_PARACHUTE_TINT_INDEX);
		SERIALISE_UNSIGNED(serialiser, m_parachuteTintIndex,	SIZE_OF_PARACHUTE_TINT_INDEX, "Parachute Tint Index");
		gnetAssertf(m_parachutePackTintIndex < MAX_PARACHUTE_PACK_TINT_INDEX,"CPedAppearanceDataNode::Serialise m_parachutePackTintIndex[%d] >= MAX_PARACHUTE_PACK_TINT_INDEX[%d] -- increase SIZE_OF_PARACHUTE_PACK_TINT_INDEX[%d]- fix playersyncnodes and pedsyncnodes",m_parachutePackTintIndex,MAX_PARACHUTE_PACK_TINT_INDEX,SIZE_OF_PARACHUTE_PACK_TINT_INDEX);
		SERIALISE_UNSIGNED(serialiser, m_parachutePackTintIndex,	SIZE_OF_PARACHUTE_PACK_TINT_INDEX, "Parachute Pack Tint Index");
	}
	else
	{
		m_parachuteTintIndex = 0;
		m_parachutePackTintIndex = 0;
	}

	bool serialiseFacialClipSetData = (m_facialClipSetId != CLIP_SET_ID_INVALID);
	bool serialiseFacialClipHash	= (m_facialIdleAnimOverrideClipNameHash != 0);
	bool serialiseFacialClipDictHash= (m_facialIdleAnimOverrideClipDictNameHash != 0);

	SERIALISE_BOOL(serialiser, serialiseFacialClipSetData, "Has Facial ClipSet");
	if((serialiseFacialClipSetData) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		fwMvClipId m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
		SERIALISE_ANIMATION_CLIP(serialiser, m_facialClipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall, "Facial ClipSet");
	}
	else
	{
		m_facialClipSetId = CLIP_SET_ID_INVALID;
	}

	SERIALISE_BOOL(serialiser, serialiseFacialClipHash, "Has Facial Clip Hash");
	if((serialiseFacialClipHash) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		SERIALISE_UNSIGNED(serialiser, m_facialIdleAnimOverrideClipNameHash, SIZEOF_HASH, "Facial Override Clip Name Hash");
	}
	else
	{
		m_facialIdleAnimOverrideClipNameHash = 0;
	}

	SERIALISE_BOOL(serialiser, serialiseFacialClipDictHash, "Has Facial Clip Dict Hash");
	if((serialiseFacialClipDictHash) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		SERIALISE_UNSIGNED(serialiser, m_facialIdleAnimOverrideClipDictNameHash, SIZEOF_HASH, "Facial Override Clip Dict Name Hash");
	}
	else
	{
		m_facialIdleAnimOverrideClipDictNameHash = 0;
	}
}

void CPedAIDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_RELATIONSHIP_GROUP(serialiser, m_relationshipGroup, "Relationship Group");
	SERIALISE_DECISION_MAKER(serialiser,     m_decisionMakerType, "Decision Maker Type");
}

bool CPedAIDataNode::HasDefaultState() const 
{ 
	return (m_relationshipGroup == 0 && m_decisionMakerType == 0); 
}

void CPedAIDataNode::SetDefaultState() 
{ 
	m_relationshipGroup = m_decisionMakerType = 0; 
}

bool CPedTaskTreeDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const
{
	bool bReady = CSyncDataNodeFrequent::IsReadyToSendToPlayer(pObj, player, serMode, actFlags, currTime);

	if (!bReady)
	{
		for (u32 i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
		{
			const CPedTaskSpecificDataNode* pTaskNode = static_cast<CPedSyncTreeBase*>(m_ParentTree)->GetPedTaskSpecificDataNode(i);

			if (AssertVerify(pTaskNode) && pObj->GetSyncData())
			{
				if (pTaskNode->IsTimeToSendUpdateToPlayer(pObj, player, currTime))
				{
					if (m_pCurrentSyncDataUnit && !m_pCurrentSyncDataUnit->IsSyncedWithPlayer(player))
					{
						// always send task tree data with the task data if the task tree data is unacked
						bReady = true;
						break;
					}
				}
			}
		}
	}

	return bReady;
}

PlayerFlags CPedTaskTreeDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const
{
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);

    if(netObjPed && (netObjPed->ForceTaskStateRequestPending() || netObjPed->IsTaskUpdatePrevented()))
    {
        return ~0u;
    }

	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

    return CSyncDataNodeFrequent::StopSend(pObj, serMode);
}

void CPedTaskTreeDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetTaskTreeData(*this);

	if (m_taskStage >= (1<<SIZEOF_SCRIPTTASKSTAGE))
	{
		gnetAssertf(0, "Extracted invalid scripted task status - Command: %d Stage: %d", m_scriptCommand, m_taskStage);
	}

	m_taskSlotsUsed = 0;

	for (int i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
	{
		if (m_taskTreeData[i].m_taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
		{
			m_taskSlotsUsed |= (1<<i);
		}
	}
}

void CPedTaskTreeDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.SetTaskTreeData(*this);
}

const unsigned int CPedTaskTreeDataNode::SIZEOF_SCRIPTTASKSTAGE = 3;

void CPedTaskTreeDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_TASKPRIORITY	 = 3;
	static const unsigned int SIZEOF_TASKTREEDEPTH	 = 3;
	static const unsigned int SIZEOF_SCRIPTCOMMAND	 = 32;

	bool hasScriptedTask = (m_scriptCommand != SCRIPT_TASK_INVALID);

	SERIALISE_BOOL(serialiser, hasScriptedTask);

	if(hasScriptedTask || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_scriptCommand, SIZEOF_SCRIPTCOMMAND,   "Script Command");
		SERIALISE_UNSIGNED(serialiser, m_taskStage,     SIZEOF_SCRIPTTASKSTAGE, "Script Task Stage");
	}
	else
	{
		m_scriptCommand = SCRIPT_TASK_INVALID;
		m_taskStage     = CPedScriptedTaskRecordData::VACANT_STAGE;
	}

	SERIALISE_UNSIGNED(serialiser, m_taskSlotsUsed, IPedNodeDataAccessor::NUM_TASK_SLOTS);

	for (int i=0; i<IPedNodeDataAccessor::NUM_TASK_SLOTS; i++)
	{
		if ((m_taskSlotsUsed & (1<<i)) || serialiser.GetIsMaximumSizeSerialiser())
		{
#if ENABLE_NETWORK_LOGGING
			char logName[10];
			sprintf(logName, "Task %d", i);
#endif
			SERIALISE_TASKTYPE(serialiser, m_taskTreeData[i].m_taskType, logName);

			SERIALISE_BOOL(serialiser, m_taskTreeData[i].m_taskActive, "    Active");
			SERIALISE_UNSIGNED(serialiser, m_taskTreeData[i].m_taskPriority,  SIZEOF_TASKPRIORITY,  "    Priority");
			SERIALISE_UNSIGNED(serialiser, m_taskTreeData[i].m_taskTreeDepth, SIZEOF_TASKTREEDEPTH, "    Tree Depth");
			SERIALISE_UNSIGNED(serialiser, m_taskTreeData[i].m_taskSequenceId, SIZEOF_TASK_SEQUENCE_ID, "    Sequence ID");
		}
		else
		{
			m_taskTreeData[i].m_taskType = CTaskTypes::MAX_NUM_TASK_TYPES;
		}
	}
}

PlayerFlags CPedTaskSpecificDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags serMode) const
{
    const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);

    if(netObjPed && (netObjPed->ForceTaskStateRequestPending() || netObjPed->IsTaskUpdatePrevented()))
    {
        return ~0u;
    }

	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

    return CSyncDataNodeFrequent::StopSend(pObj, serMode);
}

void CPedTaskSpecificDataNode::ExtractDataForSerialising(IPedNodeDataAccessor &pedNodeData)
{
	pedNodeData.GetTaskSpecificData(*this);
}

void CPedTaskSpecificDataNode::ApplyDataFromSerialising(IPedNodeDataAccessor &pedNodeData)
{
	if (m_taskData.m_TaskDataSize > 0)
	{
		pedNodeData.SetTaskSpecificData(*this);
	}
}

void CPedTaskSpecificDataNode::Serialise(CSyncDataBase& serialiser)
{
	//static const unsigned int SIZEOF_TASKINDEX = 4;

	// Have to serialise the task type in DEV builds so that we can log the data properly. There is no way of doing this otherwise.
#if ENABLE_NETWORK_LOGGING
	m_taskData.Serialise(serialiser, m_taskDataLogFunction, true);
#else
	m_taskData.Serialise(serialiser, NULL, false);
#endif
}

bool CPedTaskSpecificDataNode::HasDefaultState() const 
{ 
	return (m_taskData.m_TaskDataSize == 0); 
}

void CPedTaskSpecificDataNode::SetDefaultState() 
{ 
	m_taskData.m_TaskDataSize = 0; 
}

void CPedInventoryDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_NUM_ITEMS     = datBitsNeeded<MAX_WEAPONS>::COUNT;
	static const unsigned int SIZEOF_NUM_AMMOS     = datBitsNeeded<MAX_AMMOS>::COUNT;
	static const unsigned int SIZEOF_AMMO_QUANTITY = 14;
	//static const unsigned int SIZEOF_VEHICLEWEAPON_INDEX = 8;

	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numItems = MAX_WEAPONS;
		m_numAmmos = MAX_AMMOS;
	}

	SERIALISE_UNSIGNED(serialiser, m_numItems, SIZEOF_NUM_ITEMS, "Num Items");

	for(u32 index = 0; index < m_numItems; index++)
	{
		SERIALISE_WEAPON_TYPE(serialiser, m_itemSlots[index], "Item");
		m_itemSlotTint[index] = 0; //reset - not sent
		m_itemSlotNumComponents[index] = 0; //reset - not sent
	}

	SERIALISE_UNSIGNED(serialiser, m_numAmmos, SIZEOF_NUM_AMMOS, "Num Ammos");
	SERIALISE_BOOL(serialiser, m_allAmmoInfinite, "All ammo infinite");

	for(u32 index = 0; index < m_numAmmos; index++)
	{
		SERIALISE_AMMO_TYPE(serialiser, m_ammoSlots[index], "Ammo");

		if(!m_allAmmoInfinite || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BOOL(serialiser, m_ammoInfinite[index], "Ammo infinite");

			if(!m_ammoInfinite[index] || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, m_ammoQuantity[index], SIZEOF_AMMO_QUANTITY, "Ammo Quantity");
			}
		}
	}
}

bool CPedInventoryDataNode::HasDefaultState() const 
{ 
	return (m_numItems == 0 && m_numAmmos == 0); 
}

void CPedInventoryDataNode::SetDefaultState() 
{
	m_numItems = 0;
	m_numAmmos = 0;
}

bool CPedTaskSequenceDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	bool canApply = true;

	const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);
	CPed *ped = netObjPed ? netObjPed->GetPed() : 0;

	if (ped && m_hasSequence)
	{
		bool scriptSequence = m_sequenceResourceId != 0;

		// script peds migrating to another machine running the script assume that the other player has the sequence and so do not send the task data
		// we need to reject this if we don't have the sequence
		if (scriptSequence && m_numTasks == 0)
		{
			canApply = false;

			CScriptEntityExtension* extension = ped->GetExtension<CScriptEntityExtension>();

			if (extension)
			{
				scriptHandler* handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(extension->GetScriptInfo()->GetScriptId());

				if (handler)
				{
					scriptResource* pScriptResource = handler->GetScriptResource(CScriptResource_SequenceTask::GetResourceIdFromSequenceId(m_sequenceResourceId));

					if (pScriptResource && gnetVerifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_SEQUENCE_TASK, "Script sequence resource not found for sequence resource id %d (%s)", m_sequenceResourceId, netObjPed->GetLogName()))
					{
						ScriptResourceRef sequenceId = pScriptResource->GetReference();

						if (!CTaskSequences::ms_TaskSequenceLists[sequenceId].IsEmpty())
						{
							canApply = true;
						}
					}
				}
			}

#if ENABLE_NETWORK_LOGGING
			if (!canApply)
			{
				NetworkInterface::GetMessageLog().WriteDataValue("FAIL", "** No script sequence exists for the ped **");
			}
#endif // ENABLE_NETWORK_LOGGING
		}
	}
	
	return canApply;
}

bool CPedTaskSequenceDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex UNUSED_PARAM(player), SerialiseModeFlags UNUSED_PARAM(serMode), ActivationFlags UNUSED_PARAM(actFlags), const unsigned UNUSED_PARAM(currTime)) const
{
	bool bReadyToSend = false;

	const CNetObjPed *netObjPed = static_cast<const CNetObjPed *>(pObj);
	CPed *ped = netObjPed ? netObjPed->GetPed() : 0;

	// only send if the ped is running a networked sequence
	if (ped)
	{
		CTaskUseSequence* pSequenceTask = static_cast<CTaskUseSequence*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE));

		if (pSequenceTask)
		{
			const CTaskSequenceList* pSequenceList = pSequenceTask->GetTaskSequenceList();

			if (pSequenceList && !pSequenceList->IsMigrationPrevented())
			{
				bReadyToSend = true;
			}
		}
	}

	return bReadyToSend;
}

void CPedTaskSequenceDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZE_OF_NUM_TASKS = datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS-1>::COUNT;
	static const unsigned SIZE_OF_REPEAT_MODE = 1;

	bool scriptSequence = m_sequenceResourceId != 0;

	SERIALISE_BOOL(serialiser, m_hasSequence, "Has sequence");

	if (m_hasSequence)
	{
		SERIALISE_BOOL(serialiser, scriptSequence, "Script sequence");

		if (scriptSequence || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_sequenceResourceId, CScriptResource_SequenceTask::SIZEOF_SEQUENCE_ID, "Sequence resource id");
		}
		else
		{
			m_sequenceResourceId = 0;
		}

		if (serialiser.GetIsMaximumSizeSerialiser())
		{
			m_numTasks = IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS;
		}

		SERIALISE_UNSIGNED(serialiser, m_numTasks, SIZE_OF_NUM_TASKS);

		gnetAssertf(m_numTasks <= IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS, "Trying to serialise too many sequence tasks!");
		m_numTasks = rage::Min(m_numTasks, IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS);

		for (u32 i=0; i<m_numTasks; i++)
		{
#if ENABLE_NETWORK_LOGGING
			m_taskData[i].Serialise(serialiser, m_taskDataLogFunction, true);
#else
			m_taskData[i].Serialise(serialiser, NULL, true);
#endif			
		}

		if (m_numTasks > 0)
		{
			SERIALISE_UNSIGNED(serialiser, m_repeatMode, SIZE_OF_REPEAT_MODE);
		}
	}
}

IPedNodeDataAccessor::fnTaskDataLogger CPedTaskSpecificDataNode::m_taskDataLogFunction = 0;
IPedNodeDataAccessor::fnTaskDataLogger CPedTaskSequenceDataNode::m_taskDataLogFunction = 0;
