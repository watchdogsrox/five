//
// name:        PlayerSyncNodes.cpp
// description: Network sync nodes used by CNetObjPlayerPeds
// written by:    John Gurney
//

#include "rline/rlgamerinfo.h"
#include "network/network.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "network/objects/entities/NetObjPed.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/objects/synchronisation/syncnodes/PlayerSyncNodes.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "Peds/ped.h"
#include "Network/Players/NetGamePlayer.h"
#include "Task/General/Phone/TaskMobilePhone.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IPlayerNodeDataAccessor);

//Interval that we can wait for the respawns object id to be created locally,
//  after that we give up.
static const unsigned RESPAWN_PED_CREATION_INTERVAL = 1000;

bool CPlayerCreationDataNode::CanApplyData(netSyncTreeTargetObject* UNUSED_PARAM(pObj)) const
{
    // ensure that the model is loaded and ready for drawing for this ped
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_ModelHash, &modelId);

	if (!gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash %u for player clone", m_ModelHash))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not valid\r\n");
		return false;
	}

	if(!CNetworkStreamingRequestManager::RequestModel(modelId))
    {
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not loaded\r\n");
        return false;
    }

	return true;
}

void CPlayerCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_MODELHASH(serialiser, m_ModelHash, "Model");

	const unsigned SIZE_OF_NUM_BLOOD_MARKS  = 5;
    const unsigned SIZE_OF_NUM_SCARS   		= 3;
    const unsigned SIZE_OF_UV          		= 9;
    const unsigned SIZE_OF_ROTATION    		= 8;
	const unsigned SIZE_OF_AGE		   		= 8;
    const unsigned SIZE_OF_SCALE       		= 8;
    const unsigned SIZE_OF_FORCE_FRAME 		= 8;
    const unsigned SIZE_OF_SCAR_HASH   		= 32;
	const unsigned SIZE_OF_BLOOD_HASH		= 32;
    const unsigned SIZE_OF_DAMAGE_ZONE 		= 3;
	const unsigned SIZE_OF_UVFLAGS			= 8;

	// this really means "was the helmet put on via the CPedHelmetComponent"
	SERIALISE_BOOL(serialiser, m_wearingAHelmet, "Wearing A Helmet");

    SERIALISE_UNSIGNED(serialiser, m_NumScars, SIZE_OF_NUM_SCARS, "Num Scars");

    for(unsigned index = 0; index < MAX_SCARS_SYNCED; index++)
    {
        CPedScarNetworkData &scarData = m_ScarData[index];

        if(index < m_NumScars || serialiser.GetIsMaximumSizeSerialiser())
        {
            unsigned hash = scarData.scarNameHash.GetHash();
            unsigned zone = static_cast<unsigned>(scarData.zone);

            SERIALISE_PACKED_FLOAT(serialiser, scarData.uvPos.x,          2.0f,   SIZE_OF_UV,          "Scar UV X");
            SERIALISE_PACKED_FLOAT(serialiser, scarData.uvPos.y,          2.0f,   SIZE_OF_UV,          "Scar UV Y");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.rotation, 360.0f, SIZE_OF_ROTATION,    "Scar Rotation");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.age,	   512.0f, SIZE_OF_AGE,			"Scar Age");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, scarData.scale,    2.0f,   SIZE_OF_SCALE,       "Scar Scale");
            SERIALISE_INTEGER(serialiser, scarData.forceFrame,                   SIZE_OF_FORCE_FRAME, "Forced Frame");
            SERIALISE_UNSIGNED(serialiser, hash,                                 SIZE_OF_SCAR_HASH,   "Scar Hash");
            SERIALISE_UNSIGNED(serialiser, zone,                                 SIZE_OF_DAMAGE_ZONE, "Damage Zone");
			SERIALISE_UNSIGNED(serialiser, scarData.uvFlags,                     SIZE_OF_UVFLAGS,	  "UV Flags");

            scarData.scarNameHash.SetHash(hash);
            scarData.zone = static_cast<ePedDamageZones>(zone);
        }
        else
        {
            scarData.uvPos.Zero();

	        scarData.rotation     = 0.0f;
	        scarData.scale        = 1.0f;
			scarData.age		  = 0.0f;
	        scarData.forceFrame   = -1;
	        scarData.scarNameHash = 0u;
	        scarData.zone         = kDamageZoneInvalid;
			scarData.uvFlags	  = 255;
        }
    }
	
	SERIALISE_UNSIGNED(serialiser, m_NumBloodMarks, SIZE_OF_NUM_BLOOD_MARKS, "Num Blood Marks");
	
	for(int i = 0; i < MAX_BLOOD_MARK_SYNCED; ++i)
	{
		CPedBloodNetworkData & bloodData = m_BloodMarkData[i];

		if(i < m_NumBloodMarks || serialiser.GetIsMaximumSizeSerialiser())
		{
			unsigned zone = static_cast<unsigned>(bloodData.zone);
			unsigned hash = bloodData.bloodNameHash.GetHash();

			SERIALISE_PACKED_FLOAT(serialiser, bloodData.uvPos.x,          1.0f,   SIZE_OF_UV,          "Blood UV X");
            SERIALISE_PACKED_FLOAT(serialiser, bloodData.uvPos.y,          1.0f,   SIZE_OF_UV,          "Blood UV Y");
			SERIALISE_UNSIGNED(serialiser, zone,                                  SIZE_OF_DAMAGE_ZONE, "Damage Zone");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.rotation, 360.0f, SIZE_OF_ROTATION,    "Blood Rotation");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.age,	    512.0f, SIZE_OF_AGE,		 "Blood Age");
            SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, bloodData.scale,    2.0f,   SIZE_OF_SCALE,       "Blood Scale");
			SERIALISE_UNSIGNED(serialiser, hash,									SIZE_OF_BLOOD_HASH,	 "Blood Name Hash");
			SERIALISE_UNSIGNED(serialiser, bloodData.uvFlags,						SIZE_OF_UVFLAGS,	 "Blood UV Flags");

			bloodData.zone = static_cast<ePedDamageZones>(zone);
			bloodData.bloodNameHash.SetHash(hash);
		}
		else
		{
			bloodData.age			= 0.0f;
			bloodData.bloodNameHash = 0U;
			bloodData.rotation		= 0.0f;
			bloodData.scale			= 0.0f;
			bloodData.uvPos.x		= 0.0f;
			bloodData.uvPos.y		= 0.0f;
			bloodData.zone			= kDamageZoneInvalid;
			bloodData.uvFlags		= 255;
		}
	}
}

const float CPlayerSectorPosNode::MAX_STEALTH_NOISE		= 20.0f; 

float  CPlayerSectorPosNode::ms_aStealthNoiseThresholds[NUM_STEALTH_NOISES] =
{
	0.0f,										// STEALTH_NOISE_NONE
	0.33f * MAX_STEALTH_NOISE,					// STEALTH_NOISE_LOW
	0.66f * MAX_STEALTH_NOISE,					// STEALTH_NOISE_MED	
	MAX_STEALTH_NOISE							// STEALTH_NOISE_HIGH
};

PlayerFlags CPlayerSectorPosNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}
	
	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

	return playerFlags;
}

void CPlayerSectorPosNode::ExtractDataForSerialising(IPlayerNodeDataAccessor &nodeData)
{
	nodeData.GetPlayerSectorPosData(*this);

	m_IsStandingOnNetworkObject = m_StandingOnNetworkObjectID != NETWORK_INVALID_OBJECT_ID;

	m_PackedStealthNoise = 0;

	for (int i=1; i<NUM_STEALTH_NOISES; i++)
	{
		if (m_StealthNoise >= ms_aStealthNoiseThresholds[i])
		{
			m_PackedStealthNoise = i;
		}
		else
		{
			break;
		}
	}
}

void CPlayerSectorPosNode::ApplyDataFromSerialising(IPlayerNodeDataAccessor &nodeData)
{
	if(!m_IsStandingOnNetworkObject)
	{
		m_StandingOnNetworkObjectID = NETWORK_INVALID_OBJECT_ID;
		m_LocalOffset.Zero();
	}

	m_StealthNoise = ms_aStealthNoiseThresholds[m_PackedStealthNoise];

	nodeData.SetPlayerSectorPosData(*this);
}

void CPlayerSectorPosNode::Serialise(CSyncDataBase& serialiser)
{
    bool hasExtraData = m_IsStandingOnNetworkObject || m_IsRagdolling || m_IsOnStairs;

	SERIALISE_BOOL(serialiser, hasExtraData);

	if(hasExtraData || serialiser.GetIsMaximumSizeSerialiser())
	{
		const unsigned SIZEOF_STANDING_OBJECT_OFFSET_POS    = 14;
		const unsigned SIZEOF_STANDING_OBJECT_OFFSET_HEIGHT = 10;

        SERIALISE_BOOL(serialiser, m_IsOnStairs, "Is On Stairs");
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
        m_IsStandingOnNetworkObject = false;
        m_IsRagdolling              = false;
        m_IsOnStairs                = false;
    }
	
	//Need to send the sector position regardless of whether the ped is on an object or not, as the object may not exist on the remote this player is sync'd to.
	//If the object doesn't exist the sector position is used to position the ped remotely. lavalley.
	const unsigned SIZEOF_PLAYER_SECTORPOS = 12;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK,  SIZEOF_PLAYER_SECTORPOS, "Sector Pos X");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK,  SIZEOF_PLAYER_SECTORPOS, "Sector Pos Y");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_PLAYER_SECTORPOS, "Sector Pos Z");

	SERIALISE_UNSIGNED(serialiser, m_PackedStealthNoise, SIZEOF_STEALTH_NOISE, "Stealth noise");
    
}

CPlayerCameraDataNode::CPlayerCameraDataNode()
{
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH] = CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH]      = CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]    = CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]       = CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW]  = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
}

const float CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE	= 1500.0f;
const float CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE	= 150.0f;
const float CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET		= 6.0f; 

void CPlayerCameraDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_LOCKON_TARGET_OFFSETX = 8;
	static const unsigned int SIZEOF_LOCKON_TARGET_OFFSETY = 8;
	static const unsigned int SIZEOF_LOCKON_TARGET_OFFSETZ = 8;

	static const unsigned int SIZEOF_POSITIONX_LONG_RANGE = 13;
	static const unsigned int SIZEOF_POSITIONY_LONG_RANGE = 13;
	static const unsigned int SIZEOF_POSITIONZ_LONG_RANGE = 13;

	static const unsigned int SIZEOF_POSITIONX_SHORT_RANGE = 9;
	static const unsigned int SIZEOF_POSITIONY_SHORT_RANGE = 9;
	static const unsigned int SIZEOF_POSITIONZ_SHORT_RANGE = 9;

	static const unsigned int SIZEOF_CAMERAANGLE_HIGH = 10;

    SERIALISE_BOOL(serialiser, m_UsingFreeCamera, "Is Using Free Camera");

    if(m_UsingFreeCamera && !serialiser.GetIsMaximumSizeSerialiser())
    {
		SERIALISE_BOOL(serialiser, m_UsingCinematicVehCamera, "Using cinematic vehicle camera");
        SERIALISE_POSITION(serialiser, m_Position, "Free Cam Position");
        SERIALISE_PACKED_FLOAT(serialiser, m_eulersX,  TWO_PI, SIZEOF_CAMERAANGLE_HIGH, "Camera X");
	    SERIALISE_PACKED_FLOAT(serialiser, m_eulersZ,  TWO_PI, SIZEOF_CAMERAANGLE_HIGH, "Camera Z");
    }
    else
    {				
		m_largeOffset = m_Position.Mag() >= 10.0f;
		SERIALISE_BOOL(serialiser, m_largeOffset);
		SERIALISE_BOOL(serialiser, m_morePrecision);

		if (m_largeOffset || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_Position, WORLDLIMITS_XMAX, SIZEOF_POSITION, "Position Offset");
		}
		
		SERIALISE_PACKED_FLOAT(serialiser, m_eulersX,  TWO_PI, SIZEOF_CAMERAANGLE_HIGH, "Camera X");
		SERIALISE_PACKED_FLOAT(serialiser, m_eulersZ,  TWO_PI, SIZEOF_CAMERAANGLE_HIGH, "Camera Z");

		SERIALISE_BOOL(serialiser, m_aiming);
	    if (m_aiming || serialiser.GetIsMaximumSizeSerialiser())
	    {
			SERIALISE_BOOL(serialiser, m_canOwnerMoveWhileAiming, "Aiming Through Scope");

		    bool hasTargetId = m_targetId != NETWORK_INVALID_OBJECT_ID;
		    SERIALISE_BOOL(serialiser, hasTargetId, "Has Target Id");

		    if(hasTargetId && !serialiser.GetIsMaximumSizeSerialiser())
		    {
			    SERIALISE_BOOL(serialiser, m_freeAimLockedOnTarget, "Free Aim Locked on target");

				SERIALISE_BOOL(serialiser, m_bAimTargetEntity, "Whether the target is the aim target");

			    // 13 bits...
			    SERIALISE_OBJECTID(serialiser, m_targetId, "Target Id");

			    // 24 bits...
			    SERIALISE_PACKED_FLOAT(serialiser, m_lockOnTargetOffset.x, MAX_LOCKON_TARGET_OFFSET, SIZEOF_LOCKON_TARGET_OFFSETX, "Lock On Target Offset X");
			    SERIALISE_PACKED_FLOAT(serialiser, m_lockOnTargetOffset.y, MAX_LOCKON_TARGET_OFFSET, SIZEOF_LOCKON_TARGET_OFFSETY, "Lock On Target Offset Y");
			    SERIALISE_PACKED_FLOAT(serialiser, m_lockOnTargetOffset.z, MAX_LOCKON_TARGET_OFFSET, SIZEOF_LOCKON_TARGET_OFFSETZ, "Lock On Target Offset Z");
		    }
		    else
		    {
				m_targetId = NETWORK_INVALID_OBJECT_ID;

				m_bAimTargetEntity = false;

			    SERIALISE_BOOL(serialiser, m_longRange, "Long Range Target");
			    if(m_longRange || serialiser.GetIsMaximumSizeSerialiser())
			    {
				    // 39 bits...
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.x, MAX_WEAPON_RANGE_LONG_RANGE, SIZEOF_POSITIONX_LONG_RANGE, "Target Offset X");
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.y, MAX_WEAPON_RANGE_LONG_RANGE, SIZEOF_POSITIONY_LONG_RANGE, "Target Offset Y");
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.z, MAX_WEAPON_RANGE_LONG_RANGE, SIZEOF_POSITIONZ_LONG_RANGE, "Target Offset Z");
			    }
			    else
			    {
				    // 27 bits...
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.x, MAX_WEAPON_RANGE_SHORT_RANGE, SIZEOF_POSITIONX_SHORT_RANGE, "Target Offset X");
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.y, MAX_WEAPON_RANGE_SHORT_RANGE, SIZEOF_POSITIONY_SHORT_RANGE, "Target Offset Y");
				    SERIALISE_PACKED_FLOAT(serialiser, m_playerToTargetAimOffset.z, MAX_WEAPON_RANGE_SHORT_RANGE, SIZEOF_POSITIONZ_SHORT_RANGE, "Target Offset Z");
			    }
		    }
	    }
		else
		{
			m_canOwnerMoveWhileAiming = true;
		}
    }

#if FPS_MODE_SUPPORTED
	SERIALISE_BOOL(serialiser, m_usingFirstPersonCamera, "Using FP camera")
	SERIALISE_BOOL(serialiser, m_usingFirstPersonVehicleCamera, "Using FP vehicle camera")

	if (m_usingFirstPersonCamera || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_inFirstPersonIdle, "In FP Idle");
		SERIALISE_BOOL(serialiser, m_stickWithinStrafeAngle, "Stick Within Strafe Area");
		SERIALISE_BOOL(serialiser, m_usingSwimMotionTask, "Using swim motion task");
		SERIALISE_BOOL(serialiser, m_onSlope, "On slope");
	}
#endif

	SERIALISE_BOOL(serialiser, m_isLooking, "Is Looking");
	if(m_isLooking || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_LookAtPosition, "Look At Position");
	}

    SERIALISE_BOOL(serialiser, m_UsingLeftTriggerAimMode, "Is Using Left Trigger Aim Mode");
}

void CPlayerGameStateDataNode::ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.GetPlayerGameStateData(*this);

	m_bvehicleweaponindex = (m_vehicleweaponindex != -1);
}

void CPlayerGameStateDataNode::ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.SetPlayerGameStateData(*this);
}

void CPlayerGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	const unsigned SIZEOF_RING_STATE             = 8;
	const unsigned SIZEOF_PLAYER_TEAM            = datBitsNeeded<MAX_NUM_TEAMS>::COUNT + 1;
	const unsigned SIZEOF_AIR_DRAG_MULT          = 7;
	const unsigned SIZEOF_MAX_HEALTH             = 13;
	const unsigned SIZEOF_MAX_ARMOUR             = 12;
	const unsigned SIZEOF_JACK_SPEED             = 7;
	const unsigned SIZEOF_PHYSICAL_PLAYER_INDEX  = 5;
	const unsigned SIZEOF_TUTORIAL_INDEX         = 3;
	const unsigned SIZEOF_TUTORIAL_INSTANCE_ID   = 7;
	const unsigned SIZEOF_NET_ARRAY_DATA		 = 19;
	const unsigned SIZEOF_VWI_DATA				 = 3;
	const unsigned SIZEOF_DECORS_COUNT			 = datBitsNeeded<MAX_PLAYERINFO_DECOR_ENTRIES>::COUNT;
	const unsigned SIZEOF_DECORS_DATATYPE		 = 3;
	const unsigned SIZEOF_DECORS_KEY			 = 32;
	const unsigned SIZEOF_DECORS_DATAVAL		 = 32;
	const unsigned SIZEOF_VOICE_CHANNEL			 = 32;
	const unsigned SIZEOF_WEAPON_DEFENSE_MODIFIER = 8;
	const unsigned SIZEOF_CANT_BE_KNOCKED_OFF_BIKE = 2;
	const unsigned SIZEOF_VEHICLE_SHARE_MULTIPLIER = 8;
	const unsigned SIZEOF_WEAPON_DAMAGE_MULTIPLIER = 10;
	const unsigned SIZEOF_MELEE_DAMAGE_MULTIPLIER = 10;
	const unsigned int SIZEOF_ARCADE_TEAM = datBitsNeeded<(int)eArcadeTeam::AT_NUM_TYPES>::COUNT;
	const unsigned int SIZEOF_ARCADE_ROLE = datBitsNeeded<(int)eArcadeRole::AR_NUM_TYPES>::COUNT;
	const unsigned int SIZEOF_ARCADE_PASSIVE_ABILITY_FLAGS = eArcadeAbilityEffectType::AAE_NUM_TYPES;	// passive ability flags are a u8 bitmask, so one bit per type is required

	SERIALISE_PLAYER_STATE(serialiser, m_PlayerState,  "Player State");

	SERIALISE_BOOL(serialiser, m_GameStateFlags.controlsDisabledByScript, "Controls Disabled By Script");

	SERIALISE_INTEGER(serialiser, m_PlayerTeam,       SIZEOF_PLAYER_TEAM, "Player Team");

	bool hasRingState = m_MobileRingState != CPedPhoneComponent::DEFAULT_RING_STATE;
	SERIALISE_BOOL(serialiser, hasRingState, "Has Ring State");
	if(hasRingState || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_MobileRingState, SIZEOF_RING_STATE,  "Mobile Ring State");
	}
	else
	{
		m_MobileRingState = CPedPhoneComponent::DEFAULT_RING_STATE;
	}
    // some physics code treats an air drag multiplier of exactly 1.0f as a special case,
    // but 1.0f serialised normally loses some accuracy which prevents these checks succeeding.
    // do a specific check for this case and sync it as a bool if so to avoid these problems
    bool airDragMultIs1 = IsClose(m_AirDragMult, 1.0f);
    SERIALISE_BOOL(serialiser, airDragMultIs1, "Air Drag is 1.0f");

    if(!airDragMultIs1 || serialiser.GetIsMaximumSizeSerialiser())
    {
	    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_AirDragMult, 50.0f, SIZEOF_AIR_DRAG_MULT, "Air Drag Multiplier");
    }
    else
    {
        m_AirDragMult = 1.0f;
    }

	SERIALISE_BOOL(serialiser, m_GameStateFlags.newMaxHealthArmour);

	if (m_GameStateFlags.newMaxHealthArmour || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_MaxHealth, SIZEOF_MAX_HEALTH, "Max Health");
		SERIALISE_UNSIGNED(serialiser, m_MaxArmour, SIZEOF_MAX_ARMOUR, "Max Armour");
	}

	SERIALISE_BOOL(serialiser, m_GameStateFlags.bHasMaxHealth);
    SERIALISE_BOOL(serialiser, m_GameStateFlags.myVehicleIsMyInteresting, "My vehicle in my interesting");
	SERIALISE_UNSIGNED(serialiser, m_GameStateFlags.cantBeKnockedOffBike, SIZEOF_CANT_BE_KNOCKED_OFF_BIKE, "Cant Be Knocked Off bike");

	SERIALISE_BOOL(serialiser, m_GameStateFlags.hasSetJackSpeed);

	SERIALISE_BOOL(serialiser, m_GameStateFlags.bHelmetHasBeenShot, "Helmet has been shot");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedByBullets, "Not damaged by bullets");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedByFlames, "Not damaged by flames");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.ignoresExplosions, "Ignores explosions");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedByCollisions, "Not damaged by collisions");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedByMelee, "Not damaged by melee");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedBySmoke, "Not damaged by smoke");
    SERIALISE_BOOL(serialiser, m_GameStateFlags.notDamagedBySteam, "Not damaged by steam");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.PRF_BlockRemotePlayerRecording, "Block Remote Player Recording");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.PRF_UseScriptedWeaponFirePosition, "PRF Use Scripted Fire Position");

	if (m_GameStateFlags.hasSetJackSpeed || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_JackSpeed, SIZEOF_JACK_SPEED, "Jack Speed");
	}

	SERIALISE_BOOL(serialiser, m_GameStateFlags.neverTarget, "Never Target");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.useKinematicPhysics, "Use Kinematic Physics");

	bool isOverridingReceive = m_OverrideReceiveChat != 0;
	SERIALISE_BOOL(serialiser, isOverridingReceive, "Is Overriding Receive Chat");
	if(isOverridingReceive || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_OverrideReceiveChat, MAX_NUM_PHYSICAL_PLAYERS, "Override Receive Chat");
	}
	else
	{
		m_OverrideReceiveChat = 0;
	}

	bool isOverridingSend = m_OverrideSendChat != 0;
	SERIALISE_BOOL(serialiser, isOverridingSend, "Is Overriding Send Chat");
	if(isOverridingSend || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_OverrideSendChat, MAX_NUM_PHYSICAL_PLAYERS, "Override Send Chat");
	}
	else
	{
		m_OverrideSendChat = 0;
	}

    SERIALISE_BOOL(serialiser, m_bOverrideTutorialChat, "Override Tutorial Chat");
    SERIALISE_BOOL(serialiser, m_bOverrideTransitionChat, "Override Transition Chat");

	SERIALISE_BOOL(serialiser, m_GameStateFlags.isSpectating, "Is Spectating");

	if (m_GameStateFlags.isSpectating || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_SpectatorId, "Spectator Id");
	}
	SERIALISE_BOOL(serialiser, m_bIsSCTVSpectating, "Is SCTV Spectator");

	SERIALISE_BOOL(serialiser, m_GameStateFlags.isAntagonisticToPlayer, "Is Antagonistic to another player");

	if (m_GameStateFlags.isAntagonisticToPlayer || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_AntagonisticPlayerIndex, SIZEOF_PHYSICAL_PLAYER_INDEX, "Antagonistic player Index");
	}

	SERIALISE_BOOL(serialiser, m_GameStateFlags.inTutorial, "In tutorial");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.pendingTutorialSessionChange, "Pending tutorial change");

	if (m_GameStateFlags.inTutorial || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_TutorialIndex,      SIZEOF_TUTORIAL_INDEX,       "Tutorial Index");
		SERIALISE_UNSIGNED(serialiser, m_TutorialInstanceID, SIZEOF_TUTORIAL_INSTANCE_ID, "Tutorial Instance ID");
	}
	else
	{
		m_TutorialIndex = CNetObjPlayer::INVALID_TUTORIAL_INDEX;
		m_TutorialInstanceID = CNetObjPlayer::INVALID_TUTORIAL_INSTANCE_ID;
	}

	SERIALISE_BOOL(serialiser, m_GameStateFlags.respawning,         "Respawning");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.willJackAnyPlayer,  "Will Jack Any Player");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.willJackWantedPlayersRatherThanStealCar, "Will Jack Wanted Players");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.dontDragMeOutOfCar, "Dont Drag From Car");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.playersDontDragMeOutOfCar, "Players dont Drag From Car");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.randomPedsFlee,     "Random Peds Flee");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.everybodyBackOff,   "Everybody back off");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.playerIsWeird, "Player is weird");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.noCriticalHits, "No critical hits");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.disableHomingMissileLockForVehiclePedInside, "Disable homing lock on for peds vehicle");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.ignoreMeleeFistWeaponDamageMult, "Ignore Melee Fist Damage Multiplier");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.lawPedsCanFleeFromNonWantedPlayer, "Law peds can flee from non-wanted player");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.hasHelmet, "Has helmet");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.isSwitchingHelmetVisor, "Is switching visor");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.dontTakeOffHelmet, "Dont take off helmet");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.forceHelmetVisorSwitch, "Force ped to do visor switch");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.isPerformingVehicleMelee, "Performing vehicle melee hit");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.useOverrideFootstepPtFx, "Override footstep ptfx");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.disableVehicleCombat, "Disable Vehicle Combat");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.treatFriendlyTargettingAndDamage, "Treat As Friendly For Targetting And Damage");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.useKinematicModeWhenStationary, "Use Kinematic Mode When Stationary");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.dontActivateRagdollFromExplosions, "Don't Activate Ragdoll From Explosions");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.allowBikeAlternateAnimations, "Allow bike alternate animations");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.useLockpickVehicleEntryAnimations, "Use alternate lockpicking animations for forced entry");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.m_PlayerPreferFrontSeat, "Player Prefer Front Seat");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.ignoreInteriorCheckForSprinting, "Ignore Interior Check For Spinting");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.dontActivateRagdollFromVehicleImpact, "Don't Activate Ragdoll From Vehicle Impact");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.swatHeliSpawnWithinLastSpottedLocation, "Swat Helis Spawn Within Last Spotted Location");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.disableStartEngine, "Prevent Start Engine");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.lawOnlyAttackIfPlayerIsWanted, "Law Only Attack If Player Is Wanted");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.disableHelmetArmor, "Disable helmet armour");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.pedIsArresting, "Is ped playing arrest animation");
	SERIALISE_BOOL(serialiser, m_GameStateFlags.isScuba, "Ped has scuba gear");
	bool isTargettable = m_IsTargettableByTeam != 0;

	SERIALISE_BOOL(serialiser, isTargettable);

	if (isTargettable || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_IsTargettableByTeam, MAX_NUM_TEAMS, "Player is Targettable by Team");
	}
	else
	{
		m_IsTargettableByTeam   = 0;
	}

	SERIALISE_BOOL(serialiser, m_GameStateFlags.bHasMicrophone, "Has Microphone"); 
	if(m_GameStateFlags.bHasMicrophone || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_fVoiceLoudness, 1.0f, SIZEOF_LOUDNESS, "Voice Loudness");
	}
	
	bool bHasVoiceChannel = (m_nVoiceChannel != netPlayer::VOICE_CHANNEL_NONE);
	SERIALISE_BOOL(serialiser, bHasVoiceChannel, "Has Voice Channel");
	if(bHasVoiceChannel || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_nVoiceChannel, SIZEOF_VOICE_CHANNEL, "Voice Channel");
	}
	else
	{
		m_nVoiceChannel = netPlayer::VOICE_CHANNEL_NONE;
	}

    SERIALISE_BOOL(serialiser, m_bHasVoiceProximityOverride, "Has Voice Proximity Override");
    if(m_bHasVoiceProximityOverride || serialiser.GetIsMaximumSizeSerialiser())
	{
        SERIALISE_POSITION(serialiser, m_vVoiceProximityOverride, "Voice Proximity");
	}
    else
	{
        m_vVoiceProximityOverride = Vector3(VEC3_ZERO);
	}

	SERIALISE_UNSIGNED(serialiser, m_sizeOfNetArrayData, SIZEOF_NET_ARRAY_DATA, "Size of net array data");
	
	SERIALISE_BOOL(serialiser, m_GameStateFlags.bInvincible, "Invincible");

#if !__FINAL
	SERIALISE_BOOL(serialiser, m_GameStateFlags.bDebugInvincible, "Debug Invincible");
#endif

	SERIALISE_BOOL(serialiser, m_bvehicleweaponindex, "m_bvehicleweaponindex");
	if(m_bvehicleweaponindex || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_vehicleweaponindex, SIZEOF_VWI_DATA, "m_vehicleweaponindex");
	}
	else
	{
		m_vehicleweaponindex = -1;
	}

	// Decorator sync
	bool hasDecorators = ( m_decoratorListCount > 0 );
	bool isMaxSizeSerialiser = serialiser.GetIsMaximumSizeSerialiser();

	SERIALISE_BOOL(serialiser, hasDecorators);

	if( hasDecorators || isMaxSizeSerialiser )
	{
		if( isMaxSizeSerialiser )
		{
			m_decoratorListCount = MAX_PLAYERINFO_DECOR_ENTRIES;
		}

		SERIALISE_UNSIGNED(serialiser, m_decoratorListCount,SIZEOF_DECORS_COUNT,"Decorator count");

		for(u32 decor = 0; decor < m_decoratorListCount; decor++)
		{
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].Type,SIZEOF_DECORS_DATATYPE);
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].Key,SIZEOF_DECORS_KEY);
			SERIALISE_UNSIGNED(serialiser, m_decoratorList[decor].String,SIZEOF_DECORS_DATAVAL);	// union of float/bool/int/string
		}
	}
	else
	{
		m_decoratorListCount = 0;
	}

	SERIALISE_BOOL(serialiser, m_bIsFriendlyFireAllowed, "Is Friendly Fire Allowed");
	SERIALISE_BOOL(serialiser, m_bIsPassiveMode, "Is passive mode");

    // serialise garage instance index
    bool hasGarageInstanceIndex = m_GarageInstanceIndex != INVALID_GARAGE_INDEX;

    SERIALISE_BOOL(serialiser, hasGarageInstanceIndex);

    if(hasGarageInstanceIndex || serialiser.GetIsMaximumSizeSerialiser())
    {
        static const unsigned SIZEOF_GARAGE_INDEX = 5;

        SERIALISE_UNSIGNED(serialiser, m_GarageInstanceIndex, SIZEOF_GARAGE_INDEX, "Garage Instance Index");
    }
    else
    {
        m_GarageInstanceIndex = INVALID_GARAGE_INDEX;
    }

	// serialise property ID
	bool bHasPropertyID = m_nPropertyID != NO_PROPERTY_ID;
	SERIALISE_BOOL(serialiser, bHasPropertyID);

	if(bHasPropertyID || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_PROPERTY_ID = 8;
		SERIALISE_UNSIGNED(serialiser, m_nPropertyID, SIZEOF_PROPERTY_ID, "Property ID");
	}
	else
	{
		m_nPropertyID = NO_PROPERTY_ID;
	}

	static const unsigned SIZEOF_MENTAL_STATE = datBitsNeeded<MAX_MENTAL_STATE>::COUNT;
	SERIALISE_UNSIGNED(serialiser, m_nMentalState, SIZEOF_MENTAL_STATE, "Mental State");
#if RSG_PC
	static const unsigned SIZEOF_PED_DENSITY = 4; // max value is 10
	SERIALISE_UNSIGNED(serialiser, m_nPedDensity, SIZEOF_PED_DENSITY, "Ped Density");
#endif

	SERIALISE_BOOL(serialiser, m_bBattleAware,    "Battle Aware");
    SERIALISE_BOOL(serialiser, m_VehicleJumpDown, "Vehicle Jump Down");

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_WeaponDefenseModifier, 2.0f, SIZEOF_WEAPON_DEFENSE_MODIFIER, "Weapon Defense Modifier");
	SERIALISE_PACKED_FLOAT(serialiser, m_WeaponMinigunDefenseModifier, 2.0f, SIZEOF_WEAPON_DEFENSE_MODIFIER, "Weapon Minigun Defense Modifier");
	
	SERIALISE_BOOL(serialiser, m_bUseExtendedPopulationRange, "Extended Population Range");

    if(m_bUseExtendedPopulationRange || serialiser.GetIsMaximumSizeSerialiser())
    {
		SERIALISE_POSITION(serialiser, m_vExtendedPopulationRangeCenter, "Extended Population Range Center");
    }

    static const unsigned MAX_RANK = 5000;
    static const unsigned SIZEOF_CHARACTER_RANK = datBitsNeeded<MAX_RANK>::COUNT;
    SERIALISE_UNSIGNED(serialiser, m_nCharacterRank, SIZEOF_CHARACTER_RANK, "Character Rank");

	SERIALISE_BOOL(serialiser, m_bDisableLeavePedBehind, "Disable Leave Ped Behind");
	SERIALISE_BOOL(serialiser, m_bCollisionsDisabledByScript, "Collisions disabled by script");
	SERIALISE_BOOL(serialiser, m_bInCutscene, "In mocap cutscene");
	SERIALISE_BOOL(serialiser, m_bGhost, "Player is ghost");

	bool bHasLockOnTarget		= m_LockOnTargetID    != NETWORK_INVALID_OBJECT_ID;
	SERIALISE_BOOL(serialiser, bHasLockOnTarget);

	if(bHasLockOnTarget || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_LockOnTargetID, "Lock on Target ID");
		SERIALISE_UNSIGNED(serialiser, m_LockOnState, SIZEOF_LOCKON_STATE, "Lock-on State");
	}
	else
	{
		m_LockOnTargetID = NETWORK_INVALID_OBJECT_ID;
		m_LockOnState  = 0;
	}

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_VehicleShareMultiplier, 10.0f, SIZEOF_VEHICLE_SHARE_MULTIPLIER, "Vehicle share multiplier");

	const float MAX_WEAPON_DAMAGE_MULTIPLIER = 10.0f;
	bool isWeaponDamageModifierDiffer = !IsClose(m_WeaponDamageModifier, 1.0f);
	SERIALISE_BOOL(serialiser, isWeaponDamageModifierDiffer, "Has Weapon Damage Modifier");
	if(isWeaponDamageModifierDiffer || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_WeaponDamageModifier, MAX_WEAPON_DAMAGE_MULTIPLIER, SIZEOF_WEAPON_DAMAGE_MULTIPLIER, "Player's weapon damage modifier");
	}
	else
	{
		m_WeaponDamageModifier = 1.0f;
	}

	const float MAX_MELEE_DAMAGE_MULTIPLIER = 100.0f;
	bool isMeleeDamageModifierDiffer = !IsClose(m_MeleeDamageModifier, 1.0f);
	SERIALISE_BOOL(serialiser, isMeleeDamageModifierDiffer, "Has Melee Damage Modifier");
	if(isMeleeDamageModifierDiffer || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MeleeDamageModifier, MAX_MELEE_DAMAGE_MULTIPLIER, SIZEOF_MELEE_DAMAGE_MULTIPLIER, "Melee damage modifier");
	}
	else
	{
		m_MeleeDamageModifier = 1.0f;
	}

	bool isMeleeUnarmedDamageModifierDiffer = !IsClose(m_MeleeUnarmedDamageModifier, 1.0f);
	SERIALISE_BOOL(serialiser, isMeleeUnarmedDamageModifierDiffer, "Has Melee Unarmed Damage Modifier");
	if(isMeleeUnarmedDamageModifierDiffer || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MeleeUnarmedDamageModifier, MAX_MELEE_DAMAGE_MULTIPLIER, SIZEOF_MELEE_DAMAGE_MULTIPLIER, "Melee unarmed damage modifier");
	}
	else
	{
		m_MeleeUnarmedDamageModifier = 1.0f;
	}

	SERIALISE_BOOL(serialiser, m_bIsSuperJump, "Is Super Jump On");
	SERIALISE_BOOL(serialiser, m_EnableCrewEmblem, "Is Crew Emblem Enabled");
	
	SERIALISE_BOOL(serialiser, m_ConcealedOnOwner, "Player Concealed Locally");

	SERIALISE_BOOL(serialiser, m_bHasScriptedWeaponFirePos, "Has Scripted weapon fire position");
	if (m_bHasScriptedWeaponFirePos || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_ScriptedWeaponFirePos, "Scripted weapon fire position");
	}
	else
	{
		m_ScriptedWeaponFirePos = Vector3(VEC3_ZERO);
	}

	bool bHasArcadeInfo = (m_arcadeTeamInt != static_cast<u8>(eArcadeTeam::AT_NONE) 
		|| m_arcadeRoleInt != static_cast<u8>(eArcadeRole::AR_NONE)
		|| m_arcadePassiveAbilityFlags != static_cast<u8>(CArcadePassiveAbilityEffects::Flag::AAE_NULL));

	SERIALISE_BOOL(serialiser, bHasArcadeInfo, "Has Arcade Information");

	if (bHasArcadeInfo || isMaxSizeSerialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_arcadeTeamInt, SIZEOF_ARCADE_TEAM, "Arcade Team");
		SERIALISE_UNSIGNED(serialiser, m_arcadeRoleInt, SIZEOF_ARCADE_ROLE, "Arcade Role");
		SERIALISE_BOOL(serialiser, m_arcadeCNCVOffender, "Arcade CNC V Offender")
		SERIALISE_UNSIGNED(serialiser, m_arcadePassiveAbilityFlags, SIZEOF_ARCADE_PASSIVE_ABILITY_FLAGS, "Arcade Passive Ability Flags");
	}
	else
	{
		m_arcadeTeamInt = static_cast<u8>(eArcadeTeam::AT_NONE);
		m_arcadeRoleInt = static_cast<u8>(eArcadeRole::AR_NONE);
		m_arcadeCNCVOffender = false;
		m_arcadePassiveAbilityFlags = static_cast<u8>(CArcadePassiveAbilityEffects::Flag::AAE_NULL);
	}

	SERIALISE_BOOL(serialiser, m_bIsShockedFromDOTEffect, "Shocked from Damage over Time Effect");
	SERIALISE_BOOL(serialiser, m_bIsChokingFromDOTEffect, "Choking from Damage over Time Effect");

}

//For syncing player action anims in CPedAppearanceDataNode
void CSyncedPlayerSecondaryPartialAnim::Reset()
{
	m_MoVEScripted		= false;
	m_Active			= false;
	m_SlowBlendDuration = false;
	m_Phone				= false;
	m_PhoneSecondary	= false;
	m_IsBlocked			= false;
	m_BoneMaskHash		= 0;
	m_ClipHash			= 0;
	m_DictHash			= 0;
	m_Flags				= 0;
	m_taskSequenceId	= MAX_TASK_SEQUENCE_ID;


	m_FloatHash0			= 0;
	m_FloatHash1			= 0;
	m_FloatHash2			= 0;
	m_StatePartialHash		= 0;
	m_MoveNetDefHash		= 0;
	m_Float0				=0.0f;
	m_Float1				=0.0f;
	m_Float2				=0.0f;

}

void CSyncedPlayerSecondaryPartialAnim::Serialise(CSyncDataBase& serialiser)
{
	static const u32 SIZEOF_PSPA_SMALL_FLAGS = 8;

	SERIALISE_BOOL(serialiser, m_Active, "Secondary anim active");
	if(m_Active || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_DictHash, SIZEOF_DICT, "Secondary anim dict");
		SERIALISE_BOOL(serialiser, m_MoVEScripted, "Is MoVE Scripted secondary anim");
		SERIALISE_BOOL(serialiser, m_SlowBlendDuration, "Slow Blend Duration");

		if(m_MoVEScripted || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BOOL(serialiser, m_IsBlocked, "Is Blocked");

			bool bFloatHash0 = m_FloatHash0!=0;
			bool bFloatHash1 = m_FloatHash1!=0;
			bool bFloatHash2 = m_FloatHash2!=0;

			SERIALISE_BOOL(serialiser, bFloatHash0, "Using float 0 hash");
			SERIALISE_BOOL(serialiser, bFloatHash1, "Using float 1 hash");
			SERIALISE_BOOL(serialiser, bFloatHash2, "Using float 2 hash");

			if(bFloatHash0 || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, m_FloatHash0, SIZEOF_FLOATHASH, "Secondary float 0 hash");
				SERIALISE_PACKED_FLOAT(serialiser, m_Float0, 2.0f, SIZEOF_MOVEFLOAT, "Float 0 value");
			}
			if(bFloatHash1 || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, m_FloatHash1, SIZEOF_FLOATHASH, "Secondary float 1 hash");
				SERIALISE_PACKED_FLOAT(serialiser, m_Float1, 2.0f, SIZEOF_MOVEFLOAT, "Float 1 value");
			}
			if(bFloatHash2 || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, m_FloatHash2, SIZEOF_FLOATHASH, "Secondary float 2 hash");
				SERIALISE_PACKED_FLOAT(serialiser, m_Float2, 2.0f, SIZEOF_MOVEFLOAT, "Float 2 value");
			}

			SERIALISE_UNSIGNED(serialiser, m_StatePartialHash, SIZEOF_STATEPARTIALHASH, "Secondary state partial hash");
			SERIALISE_UNSIGNED(serialiser, m_MoveNetDefHash, SIZEOF_MOVENETDEFHASH, "Secondary MoVE net def hash");

			m_Phone = false;
			m_PhoneSecondary = false;
		}
		else
		{
			SERIALISE_BOOL(serialiser, m_Phone, "Phone");

			if (m_Phone)
			{
				SERIALISE_BOOL(serialiser, m_PhoneSecondary, "Phone Secondary");

				if (m_PhoneSecondary)
				{
					//Note: this is currently less than the max serialized value as calculated in the above section - if it expands in the future
					//greater than the max serialiser - then make this the max serialized value (lavalley)
					SERIALISE_UNSIGNED(serialiser, m_ClipHash, SIZEOF_ANIM, "Secondary anim anim");
					SERIALISE_UNSIGNED(serialiser, m_BoneMaskHash, SIZEOF_BONEMASKHASH, "Bone mask hash");
					SERIALISE_PACKED_FLOAT(serialiser, m_Float1, 1.0f, SIZEOF_MOVEFLOAT, "Float 1 value");
					SERIALISE_PACKED_FLOAT(serialiser, m_Float2, 1.0f, SIZEOF_MOVEFLOAT, "Float 2 value");
					SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_PSPA_SMALL_FLAGS, "Secondary anim flags");
				}
				else
				{
					m_ClipHash = 0;
					m_BoneMaskHash = 0;
					m_Float0 = 0.f;
					m_Float1 = 0.f;
					m_Float2 = 0.f;
					m_Flags = 0;
				}
			}
			else
			{
				bool bDefaultBoneMask = m_BoneMaskHash==0;
				SERIALISE_BOOL(serialiser, bDefaultBoneMask, "Using default bone mask");
				SERIALISE_UNSIGNED(serialiser, m_taskSequenceId, SIZEOF_TASK_SEQUENCE_ID, "Secondary Task Sequence ID");

				if(!bDefaultBoneMask || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_BoneMaskHash, SIZEOF_BONEMASKHASH, "Bone mask hash");
				}
				else
				{
					m_BoneMaskHash = 0;
				}

				SERIALISE_UNSIGNED(serialiser, m_ClipHash, SIZEOF_ANIM, "Secondary anim anim");
				SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Secondary anim flags");

				m_Phone = false;
				m_PhoneSecondary = false;
			}
		}
	}
	else
	{
		Reset();
	}
}

CPlayerAppearanceDataNode::CPlayerAppearanceDataNode()
: m_networkedDamagePack(0), 
  m_NewModelHash(0), 
  m_VoiceHash(0), 
  m_phoneMode(0), 
  m_crewEmblemVariation(0), 
  m_parachuteTintIndex(0), 
  m_parachutePackTintIndex(0),
  m_RespawnNetObjId(0),
  m_HasHeadBlendData(false),
  m_HasDecorations(false),
  m_HasRespawnObjId(false),
  m_facialIdleAnimOverrideClipNameHash(0),
  m_facialIdleAnimOverrideClipDictNameHash(0),
  m_isAttachingHelmet(false),
  m_isRemovingHelmet(false),
  m_isWearingHelmet(false),
  m_helmetTextureId(0),
  m_helmetProp(0),
  m_visorPropDown(0),
  m_visorPropUp(0),
  m_visorIsUp(false),
  m_supportsVisor(false),
  m_isVisorSwitching(false),
  m_targetVisorState(0),
  m_crewLogoTxdHash(0),
  m_crewLogoTexHash(0)
{

}

bool CPlayerAppearanceDataNode::CanApplyData(netSyncTreeTargetObject*) const
{
	// ensure that the model is loaded and ready for drawing for this ped
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_NewModelHash, &modelId);

	if (!gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash %u for new player model", m_NewModelHash))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, new player model not valid\r\n");
		return false;
	}

	if(!CNetworkStreamingRequestManager::RequestModel(modelId))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, new player model not loaded\r\n");
		return false;
	}

	return true;
}

void CPlayerAppearanceDataNode::ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.GetPlayerAppearanceData(*this);

	m_HasRespawnObjId = (m_RespawnNetObjId != NETWORK_INVALID_OBJECT_ID);
}

void CPlayerAppearanceDataNode::ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.SetPlayerAppearanceData(*this);
}

void CPlayerAppearanceDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_PLAYER_VOICE_HASH  = 32;
	static const unsigned SIZEOF_CREW_LOGO_TEX_HASH  = 32;
	//static const unsigned SIZEOF_POSITION           = 14;
	//static const unsigned SIZEOF_BLEND_RATIO        = 8;
	//static const unsigned SIZEOF_HEAD_ID            = 8;
	//static const unsigned SIZEOF_TEX_ID             = 8;
	static const unsigned SIZEOF_PACKED_DECORATIONS = 32;
	static const unsigned SIZE_OF_PARACHUTE_TINT_INDEX		= 4; // 16 options...
	static const unsigned SIZE_OF_PARACHUTE_PACK_TINT_INDEX = 4; // 16 options...
	NET_ASSERTS_ONLY(static const unsigned MAX_PARACHUTE_TINT_INDEX = 1 << SIZE_OF_PARACHUTE_TINT_INDEX;)
	NET_ASSERTS_ONLY(static const unsigned MAX_PARACHUTE_PACK_TINT_INDEX = 1 << SIZE_OF_PARACHUTE_PACK_TINT_INDEX;)
	static const unsigned SIZE_OF_HELMET_TEXTURE_ID = 8;
	static const unsigned SIZE_OF_HELMET_PROP = 10;
	static const unsigned SIZEOF_PHONE_MODE = datBitsNeeded<CTaskMobilePhone::Mode_Max-1>::COUNT;
	static const unsigned int SIZEOF_HASH = 32;
	static const unsigned int SIZEOF_CREW_EMBLEM_VARIATION = 32;

	SERIALISE_MODELHASH(serialiser, m_NewModelHash, "Model");
	SERIALISE_UNSIGNED(serialiser, m_VoiceHash, SIZEOF_PLAYER_VOICE_HASH, "Voice Hash");

	SERIALISE_UNSIGNED(serialiser, m_phoneMode, SIZEOF_PHONE_MODE, "Phone Mode");
	SERIALISE_BOOL(serialiser, m_isRemovingHelmet, "Is Removing Helmet");
	SERIALISE_BOOL(serialiser, m_isAttachingHelmet, "Is Attaching Helmet");
	SERIALISE_BOOL(serialiser, m_isWearingHelmet, "Is Wearing Helmet");
	SERIALISE_BOOL(serialiser, m_visorIsUp, "Is Visor Up Currently");
	SERIALISE_BOOL(serialiser, m_supportsVisor, "Supports Visor");
	SERIALISE_BOOL(serialiser, m_isVisorSwitching, "Has Take Off Helmet Task Started");
	SERIALISE_UNSIGNED(serialiser, m_targetVisorState, SIZE_OF_HELMET_TEXTURE_ID, "Target Visor State");
	SERIALISE_UNSIGNED(serialiser, m_crewLogoTxdHash, SIZEOF_CREW_LOGO_TEX_HASH, "Crew Logo TXD Hash");
	SERIALISE_UNSIGNED(serialiser, m_crewLogoTexHash, SIZEOF_CREW_LOGO_TEX_HASH, "Crew Logo TEX Hash");

	if((m_isAttachingHelmet) || (m_isRemovingHelmet) || (m_isWearingHelmet) || (m_isVisorSwitching) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		SERIALISE_UNSIGNED(serialiser, m_helmetTextureId, SIZE_OF_HELMET_TEXTURE_ID, "Helmet Texture");
		SERIALISE_UNSIGNED(serialiser, m_helmetProp, SIZE_OF_HELMET_PROP, "Helmet Prop");
		SERIALISE_UNSIGNED(serialiser, m_visorPropDown, SIZE_OF_HELMET_PROP, "Visor Down Prop");
		SERIALISE_UNSIGNED(serialiser, m_visorPropUp, SIZE_OF_HELMET_PROP, "Visor Up Prop");

	}

	bool hasTintIndex = ((m_parachuteTintIndex != 0) || (m_parachutePackTintIndex != 0));
	SERIALISE_BOOL(serialiser, hasTintIndex, "Has Parachute Tint Index");
	if(hasTintIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		gnetAssertf(m_parachuteTintIndex < MAX_PARACHUTE_TINT_INDEX,"CPlayerAppearanceDataNode::Serialise m_parachuteTintIndex[%d] >= MAX_PARACHUTE_TINT_INDEX[%d] -- increase SIZE_OF_PARACHUTE_TINT_INDEX[%d]- fix playersyncnodes and pedsyncnodes",m_parachuteTintIndex,MAX_PARACHUTE_TINT_INDEX,SIZE_OF_PARACHUTE_TINT_INDEX);
		SERIALISE_UNSIGNED(serialiser, m_parachuteTintIndex,	SIZE_OF_PARACHUTE_TINT_INDEX, "Parachute Tint Index");
		gnetAssertf(m_parachutePackTintIndex < MAX_PARACHUTE_PACK_TINT_INDEX,"CPlayerAppearanceDataNode::Serialise m_parachutePackTintIndex[%d] >= MAX_PARACHUTE_PACK_TINT_INDEX[%d] -- increase SIZE_OF_PARACHUTE_PACK_TINT_INDEX[%d]- fix playersyncnodes and pedsyncnodes",m_parachutePackTintIndex,MAX_PARACHUTE_PACK_TINT_INDEX,SIZE_OF_PARACHUTE_PACK_TINT_INDEX);		
		SERIALISE_UNSIGNED(serialiser, m_parachutePackTintIndex,	SIZE_OF_PARACHUTE_PACK_TINT_INDEX, "Parachute Pack Tint Index");
	}
	else
	{
		m_parachuteTintIndex = 0;
		m_parachutePackTintIndex = 0;
	}

	SERIALISE_BOOL(serialiser, m_HasRespawnObjId, "Has a Respawn Object Id");
	if(m_HasRespawnObjId || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_RespawnNetObjId, "Respawn Obj Id");
	}
	else
	{
		m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID;
	}

	m_PedProps.Serialise(serialiser);

	m_VariationData.Serialise(serialiser);

	SERIALISE_BOOL(serialiser, m_HasHeadBlendData);

	if(m_HasHeadBlendData || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_HeadBlendData.Serialise(serialiser);
	}

	SERIALISE_BOOL(serialiser, m_HasDecorations);

	if(m_HasDecorations || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(u32 index = 0; index < NUM_DECORATION_BITFIELDS; index++)
		{
			SERIALISE_UNSIGNED(serialiser, m_PackedDecorations[index], SIZEOF_PACKED_DECORATIONS, "Packed Decorations");
		}

		bool bHasCrewEmblemVariation = m_crewEmblemVariation != EmblemDescriptor::INVALID_EMBLEM_ID;

		SERIALISE_BOOL(serialiser, bHasCrewEmblemVariation);

		if(bHasCrewEmblemVariation || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_crewEmblemVariation, SIZEOF_CREW_EMBLEM_VARIATION, "Crew emblem variation");
		}
		else
		{
			m_crewEmblemVariation = EmblemDescriptor::INVALID_EMBLEM_ID;
		}
	}

	m_secondaryPartialAnim.Serialise(serialiser);

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

	bool bUseNetworkDamagePack = (m_networkedDamagePack != 0);
	SERIALISE_BOOL(serialiser, bUseNetworkDamagePack, "bUseNetworkDamagePack");
	if (bUseNetworkDamagePack || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		SERIALISE_UNSIGNED(serialiser, m_networkedDamagePack, SIZEOF_HASH, "Networked Damage Pack");
	}
	else
	{
		m_networkedDamagePack = 0;
	}
}

void CPlayerPedGroupDataNode::Serialise(CSyncDataBase& serialiser)
{
	m_pedGroup.Serialise(serialiser);
}

void CPlayerWantedAndLOSDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_WANTEDLEVEL = 3;

	bool bSpotted = m_timeLastSpotted != 0 || m_timeFirstSpotted != 0;
	bool bVisiblePlayers = m_visiblePlayers != 0;

	SERIALISE_UNSIGNED(serialiser, m_wantedLevel, SIZEOF_WANTEDLEVEL, "Wanted Level");
	SERIALISE_UNSIGNED(serialiser, m_WantedLevelBeforeParole, SIZEOF_WANTEDLEVEL, "Wanted Level Before Parole");
	SERIALISE_UNSIGNED(serialiser, m_fakeWantedLevel, SIZEOF_WANTEDLEVEL, "Fake Wanted Level");

	SERIALISE_BOOL(serialiser, m_causedByThisPlayer, "WL Caused By This Player");
	SERIALISE_BOOL(serialiser, m_bIsOutsideCircle, "Is Outside Circle");

	if (m_causedByThisPlayer)
	{
		m_causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX;	
	}
	else if (serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_causedByPlayerPhysicalIndex, 8, "WL Caused By");
		m_causedByThisPlayer = false;		
	}

	SERIALISE_BOOL(serialiser, bSpotted, "Spotted");
	
	if (bSpotted || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_lastSpottedLocation, "Last Spotted Location");
		SERIALISE_POSITION(serialiser, m_searchAreaCentre, "Search Area Centre");
		SERIALISE_UNSIGNED(serialiser, m_timeLastSpotted, 32, "Time Last Spotted");
		SERIALISE_UNSIGNED(serialiser, m_timeFirstSpotted, 32, "Time First Spotted");
	}
	else
	{
		m_lastSpottedLocation = m_searchAreaCentre = Vector3(0.0f, 0.0f, 0.0f);
		m_timeLastSpotted = 0;
		m_timeFirstSpotted = 0;
	}

	SERIALISE_BOOL(serialiser, bVisiblePlayers, "Visible Players");

	if (bVisiblePlayers || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_visiblePlayers, MAX_NUM_PHYSICAL_PLAYERS, "Visible Players");
	}
	else
	{
		m_visiblePlayers = 0;
	}

	SERIALISE_BOOL(serialiser, m_HasLeftInitialSearchArea, "Has Left Initial Search Area");
	SERIALISE_BOOL(serialiser, m_copsAreSearching, "Cops Are Searching");
}

bool CPlayerWantedAndLOSDataNode::HasDefaultState() const 
{ 
	return (m_wantedLevel == 0 && m_visiblePlayers == 0 && m_fakeWantedLevel == 0); 
}

void CPlayerWantedAndLOSDataNode::SetDefaultState()
{
	m_WantedLevelBeforeParole = m_wantedLevel = m_visiblePlayers = m_fakeWantedLevel = 0;
	m_lastSpottedLocation = m_searchAreaCentre = Vector3(0.0f, 0.0f, 0.0f);
	m_timeLastSpotted = 0;
	m_timeFirstSpotted = 0;
	m_bIsOutsideCircle = false;
}

CPlayerAmbientModelStreamingNode::CPlayerAmbientModelStreamingNode()
{
    // This node never needs to sent that frequently
    m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH] = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH]      = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]    = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]       = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW]  = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
}

void CPlayerAmbientModelStreamingNode::Serialise(CSyncDataBase& serialiser)
{
    const unsigned SIZEOF_ENTRY_POINT  = 5;
	const unsigned SIZEOF_MODEL_OFFSET = 7;

	SERIALISE_UNSIGNED(serialiser, m_AllowedPedModelStartOffset,     SIZEOF_MODEL_OFFSET, "Allowed Ped Model Start Offset");
	SERIALISE_UNSIGNED(serialiser, m_AllowedVehicleModelStartOffset, SIZEOF_MODEL_OFFSET, "Allowed Vehicle Model Start Offset");
    SERIALISE_OBJECTID(serialiser, m_TargetVehicleForAnimStreaming,  "Vehicle Anim Streaming Target");
    SERIALISE_INTEGER(serialiser, m_TargetVehicleEntryPoint,         SIZEOF_ENTRY_POINT,  "Vehicle Anim Streaming Target Entry Point");
}

void CPlayerGamerDataNode::Serialise(CSyncDataBase& serialiser)
{
	bool bClanDescValid = m_clanMembership.IsValid();
	SERIALISE_BOOL(serialiser, bClanDescValid, "bClanDescValid");

	if (bClanDescValid || serialiser.GetIsMaximumSizeSerialiser())
	{
		/*rlClanMemberId m_Id;
		rlClanDesc m_Clan;
		rlClanRank m_Rank;
		bool m_IsPrimary;
		*/

		//rlClanMemberId
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Id, 64, "clan member id");

		//rlClanDesc
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Clan.m_Id, 64, "clan id");
		SERIALISE_STRING(serialiser, m_clanMembership.m_Clan.m_ClanName, RL_CLAN_NAME_MAX_CHARS, "clan name");
		SERIALISE_STRING(serialiser, m_clanMembership.m_Clan.m_ClanTag, RL_CLAN_TAG_MAX_CHARS, "clan tag");
		SERIALISE_STRING(serialiser, m_clanMembership.m_Clan.m_ClanMotto, RL_CLAN_MOTTO_MAX_CHARS, "clan motto");
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Clan.m_MemberCount, 32, "clan member count" );
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Clan.m_CreatedTimePosix, 32, "clan created time");
		SERIALISE_BOOL(serialiser, m_clanMembership.m_Clan.m_IsSystemClan, "clan system clan");
		SERIALISE_BOOL(serialiser, m_clanMembership.m_Clan.m_IsOpenClan, "clan Open");
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Clan.m_clanColor, 32, "clan Color");

		//rlClanRank
		//Don't send duplicate data.  Just use the id we just got from the rlClanDesc.
		if (serialiser.GetType() == CSyncDataBase::SYNC_DATA_READ)
		{
			m_clanMembership.m_Rank.m_Id = m_clanMembership.m_Clan.m_Id; //rlClanId m_Id;
		}
		SERIALISE_STRING(serialiser, m_clanMembership.m_Rank.m_RankName, RL_CLAN_RANK_NAME_MAX_CHARS, "clan rank name"); //char m_RankName[RL_CLAN_RANK_NAME_MAX_CHARS];
		SERIALISE_INTEGER(serialiser, m_clanMembership.m_Rank.m_RankOrder, 32, "rank order"); 	//int m_RankOrder;
		
		//These system flags are mostly not used, so only serialise them if they're set.
		SERIALISE_BOOL(serialiser, m_bNeedToSerialiseRankSystemFlags, "need rank sysFlags");
		if(m_bNeedToSerialiseRankSystemFlags || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_clanMembership.m_Rank.m_SystemFlags, 64, "rank flags"); //u64 m_SystemFlags;
		}
		else
		{
			m_clanMembership.m_Rank.m_SystemFlags = 0;
		}

		m_clanMembership.m_IsPrimary = true;  //It's always the primary.
	}
	else
	{
		m_clanMembership.Clear();
	}

	SERIALISE_BOOL(serialiser, m_bNeedToSerialiseCrewRankTitle, "bNeed Crew Rank Title");
	if (m_bNeedToSerialiseCrewRankTitle || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_STRING(serialiser, m_crewRankTitle, sizeof(m_crewRankTitle), "crew rank title");
	}
	else
	{	
		m_crewRankTitle[0] = '\0';
	}

	// transition sessions
	SERIALISE_BOOL(serialiser, m_bHasStartedTransition, "Has Started Transition");
	SERIALISE_BOOL(serialiser, m_bHasTransitionInfo, "Has Transition Info");
	if(m_bHasTransitionInfo || serialiser.GetIsMaximumSizeSerialiser())
		SERIALISE_STRING(serialiser, m_aInfoBuffer, rlSessionInfo::TO_STRING_BUFFER_SIZE, "Info Buffer");

	//Serialize the number of mutes.  Only send the talker count if we have mutes, since it's not necessary otherwise.
	SERIALISE_BOOL(serialiser, m_bNeedToSerialiseMuteData, "needMuteData");
	if (m_bNeedToSerialiseMuteData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_muteCount, 16, "MuteCount");
		SERIALISE_INTEGER(serialiser, m_muteTotalTalkersCount, 24, "MuteTalkersCount");
	}
	else
	{
		m_muteCount = 0;
		m_muteTotalTalkersCount = 0;
	}

	SERIALISE_BOOL(serialiser, m_bIsRockstarDev, "Is Rockstar Dev");
	SERIALISE_BOOL(serialiser, m_bIsRockstarQA, "Is Rockstar QA");
	SERIALISE_BOOL(serialiser, m_bIsCheater, "Is Cheater");

    static const unsigned SIZE_OF_MM_GROUP = datBitsNeeded<MM_GROUP_MAX>::COUNT;
    SERIALISE_UNSIGNED(serialiser, m_nMatchMakingGroup, SIZE_OF_MM_GROUP, "Matchmaking Group");

    SERIALISE_BOOL(serialiser, m_hasCommunicationPrivileges, "Comm Priv");

	SERIALISE_UNSIGNED(serialiser, m_kickVotes, MAX_NUM_ACTIVE_PLAYERS, "Kick Votes");

	SERIALISE_INTEGER(serialiser, m_playerAccountId, 32, "Player Account Id");
}

void CPlayerGamerDataNode::ExtractDataForSerialising( IPlayerNodeDataAccessor &playerNodeData )
{
	playerNodeData.GetPlayerGamerData(*this);

	//See if our system flags have anything useful and we can save ourselves some bits.
	m_bNeedToSerialiseRankSystemFlags = m_clanMembership.m_Rank.m_SystemFlags != 0;
	m_bNeedToSerialiseMuteData = (m_muteCount != 0);
	if (m_muteCount > 0xffff)
	{
		m_muteCount = 0xffff;
	}
	if (m_muteTotalTalkersCount > 0x00ffffff)
	{
		m_muteTotalTalkersCount = 0x00ffffff;  //Since we only sync 24 bits.
	}

	m_bNeedToSerialiseCrewRankTitle = strlen(m_crewRankTitle) > 0;
}

void CPlayerGamerDataNode::ApplyDataFromSerialising( IPlayerNodeDataAccessor &playerNodeData )
{
	playerNodeData.SetPlayerGamerData(*this);
}

void CPlayerExtendedGameStateNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_WAYPOINT_OBJECTID = 16;
	static const unsigned int SIZEOF_WAYPOINT_TIMESTAMP = 32;
	static const unsigned int SIZEOF_CAMERA_FOV  = 8;
	static const unsigned int SIZEOF_CAMERA_ASPECT  = 8;
	static const unsigned int SIZEOF_EXPLOSION_DAMAGE  = 10;
	static const unsigned int SIZEOF_GHOST_PLAYERS  = sizeof(PlayerFlags)<<3;

	SERIALISE_UNSIGNED(serialiser, m_WaypointLocalDirtyTimestamp, SIZEOF_WAYPOINT_TIMESTAMP, "Waypoint Dirty Timestamp");
	SERIALISE_BOOL(serialiser, m_bHasActiveWaypoint, "Has Active Waypoint");

	if(m_bHasActiveWaypoint || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_bOwnsWaypoint, "Owns Waypoint");
		SERIALISE_PACKED_FLOAT(serialiser, m_fxWaypoint, WORLDLIMITS_XMAX, SIZEOF_POSITION, "Waypoint X");
		SERIALISE_PACKED_FLOAT(serialiser, m_fyWaypoint, WORLDLIMITS_YMAX, SIZEOF_POSITION, "Waypoint Y");
		SERIALISE_INTEGER(serialiser, m_WaypointObjectId, SIZEOF_WAYPOINT_OBJECTID, "Waypoint ObjectId");
	}
	else
	{
		m_bOwnsWaypoint = false;
	}

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fovRatio, 1.0f, SIZEOF_CAMERA_FOV, "Camera FOV");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_aspectRatio, 10.0f, SIZEOF_CAMERA_ASPECT, "Camera aspect ratio")

    const unsigned SIZEOF_CITY_DENSITY = 8;
    SERIALISE_PACKED_FLOAT(serialiser, m_CityDensity, 1.0f, SIZEOF_CITY_DENSITY, "City Density");

	bool bHasGhostPlayers = m_ghostPlayers != 0;

	SERIALISE_BOOL(serialiser, bHasGhostPlayers);

	if(bHasGhostPlayers || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_ghostPlayers, SIZEOF_GHOST_PLAYERS, "Ghost players");
	}
	else
	{
		m_ghostPlayers = 0;
	}

	bool hasMaxExplosionDamageSet = m_MaxExplosionDamage  > 0;
	SERIALISE_BOOL(serialiser, hasMaxExplosionDamageSet);
	
	if(hasMaxExplosionDamageSet || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MaxExplosionDamage, 1000.0f, SIZEOF_EXPLOSION_DAMAGE, "Maximum Explosion Damage");
	}
	else
	{
		m_MaxExplosionDamage = -1.0f;
	}
}

void CPlayerExtendedGameStateNode::ExtractDataForSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.GetPlayerExtendedGameStateData(*this);
}

void CPlayerExtendedGameStateNode::ApplyDataFromSerialising(IPlayerNodeDataAccessor &playerNodeData)
{
	playerNodeData.SetPlayerExtendedGameStateData(*this);
}
