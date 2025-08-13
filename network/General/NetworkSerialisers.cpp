#include "NetworkSerialisers.h"

// game includes
#include "event/decision/EventDecisionMaker.h"
#include "event/decision/EventDecisionMakerManager.h"
#include "game/Dispatch/Incidents.h"
#include "objects/door.h"
#include "peds/PlayerInfo.h"
#include "task/motion/Locomotion/TaskMotionPed.h"
#include "task/motion/Locomotion/TaskHorseLocomotion.h"
#include "task/motion/Locomotion/TaskBirdLocomotion.h"
#include "task/vehicle/TaskRideHorse.h"
#include "vehicles/MetaData/VehicleEnterExitFlags.h"

// framework includes
#include "fwanimation/clipsets.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, serialiser, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_serialiser

void SerialiseGlobalFlags(CSyncDataBase& serialiser, u32 &globalFlags, const char *name)
{
	const unsigned SIZEOF_GLOBALFLAGS = SIZEOF_NETOBJ_GLOBALFLAGS;

	SERIALISE_UNSIGNED(serialiser, globalFlags, SIZEOF_GLOBALFLAGS);

	if(name && serialiser.GetLog())
	{
		LogGlobalFlags(*serialiser.GetLog(), globalFlags, name);
	}
}

void SerialiseAnimation(CSyncDataBase& serialiser, fwMvClipSetId &clipSetId, fwMvClipId &clipId, const char *name)
{
	const unsigned SIZEOF_CLIP_SET_ID = 32;
	const unsigned SIZEOF_ANIM_ID     = 32;

	u32 clipSetIdHash = clipSetId.GetHash();
	SERIALISE_UNSIGNED(serialiser, clipSetIdHash, SIZEOF_CLIP_SET_ID);
	clipSetId.SetHash(clipSetIdHash);

	u32 animIdHash = clipId.GetHash();
	SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID);
	clipId.SetHash(animIdHash);

	if(name && serialiser.GetLog())
	{
		const char *clipName = "Unknown";
		const char *clipSetName = "Unknown";
#if !__FINAL
		clipName = clipId.TryGetCStr();
		clipSetName = clipSetId.TryGetCStr();
#endif
		serialiser.GetLog()->WriteDataValue(name, "Clip %s, Clip Set %s", clipName ? clipName : "Unknown", clipSetName ? clipSetName : "Unknown" );
	}
}

void SerialiseAnimation(CSyncDataBase& serialiser, s32 &animSlotIndex, u32 &animHashKey, const char *name)
{
	// the maximum value of animSlotIndex is derived from the AnimStorePool
	const int SIZEOF_ANIMSLOTINDEX	= 14;
	const unsigned SIZEOF_ANIM_HASH = 32;

	SERIALISE_UNSIGNED(serialiser, animSlotIndex, SIZEOF_ANIMSLOTINDEX);
	SERIALISE_UNSIGNED(serialiser, animHashKey, SIZEOF_ANIM_HASH);

	if(name && serialiser.GetLog())
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(animSlotIndex, animHashKey);

		if (pClip)
		{
			serialiser.GetLog()->WriteDataValue(name, pClip->GetName());
		}
	}
}

void SerialiseAnimBlend(CSyncDataBase& serialiser, float& blend, const char *name)
{
	static const unsigned int SIZEOF_BLEND_DURATION = 8;

	static const float MAX_BLEND_VALUE = 1.0f;

	bool bInstantBlend = (blend - INSTANT_BLEND_DURATION) < 0.01f;
	bool bNormalBlend = Abs(blend - NORMAL_BLEND_DURATION) < 0.01f;
	bool bFastBlend = Abs(blend - FAST_BLEND_DURATION) < 0.01f;
	bool bReallySlowBlend = Abs(blend) > MAX_BLEND_VALUE;

	SERIALISE_BOOL(serialiser, bNormalBlend);

	if (!bNormalBlend || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, bFastBlend);

		if (!bFastBlend || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BOOL(serialiser, bInstantBlend);

			if (!bInstantBlend || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_BOOL(serialiser, bReallySlowBlend);

				if(!bReallySlowBlend || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_PACKED_FLOAT(serialiser, blend, MAX_BLEND_VALUE, SIZEOF_BLEND_DURATION);
				}
				else
				{
					blend = MIGRATE_SLOW_BLEND_DURATION;
				}
			}
			else
			{
				blend = INSTANT_BLEND_DURATION;
			}
		}
		else
		{
			blend = FAST_BLEND_DURATION;
		}
	}
	else
	{
		blend = NORMAL_BLEND_DURATION;
	}

	if(name && serialiser.GetLog())
	{
		serialiser.GetLog()->WriteDataValue(name, "%f", blend);
	}
}

void SerialiseModelHash(CSyncDataBase& serialiser, u32 &modelHash, const char *name)
{
	const unsigned SIZEOF_MODEL_HASH = 32;

	SERIALISE_UNSIGNED(serialiser, modelHash, SIZEOF_MODEL_HASH);

	if(name && serialiser.GetLog())
	{
		const char *modelName = "Unknown";

#if !__FINAL
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
		modelName = CModelInfo::GetBaseModelInfoName(modelId);
#endif //!__FINAL

		serialiser.GetLog()->WriteDataValue(name, modelName);
	}
}

void SerialiseWeapon(CSyncDataBase& serialiser, u32 &weaponHash, const char *name)
{
	const unsigned SIZEOF_WEAPON = 32;
	SERIALISE_UNSIGNED(serialiser, weaponHash, SIZEOF_WEAPON);
	
	if(name && serialiser.GetLog())
	{
		const char *weaponName = "Unknown Weapon";
#if !__FINAL
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
		if (pWeaponInfo)
		{
			weaponName = pWeaponInfo->GetName();
		}		
#endif //!__FINAL

		serialiser.GetLog()->WriteDataValue(name, weaponName);
	}
}

void SerialiseVelocity(CSyncDataBase& serialiser, s32 &packedVelX, s32 &packedVelY, s32 &packedVelZ, int sizeOfVelocity, const char *name)
{
	const unsigned int SIZEOF_VELOCITY_POINT = 4;

	SERIALISE_INTEGER(serialiser, packedVelX, sizeOfVelocity);
	SERIALISE_INTEGER(serialiser, packedVelY, sizeOfVelocity);
	SERIALISE_INTEGER(serialiser, packedVelZ, sizeOfVelocity);

	if(name && serialiser.GetLog())
	{
		Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
		velocity.x = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelX, SIZEOF_VELOCITY_POINT);
		velocity.y = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelY, SIZEOF_VELOCITY_POINT);
		velocity.z = netSerialiserUtils::UnPackFixedPoint16((s16)packedVelZ, SIZEOF_VELOCITY_POINT);

		serialiser.GetLog()->WriteDataValue(name, "%f, %f, %f", velocity.x, velocity.y, velocity.z);
	}
}

void SerialiseTaskType(CSyncDataBase& serialiser, u32 &taskType, const char *name)
{
	const unsigned SIZEOF_TASK_TYPE = datBitsNeeded<CTaskTypes::MAX_NUM_TASK_TYPES>::COUNT;

	SERIALISE_UNSIGNED(serialiser, taskType, SIZEOF_TASK_TYPE);

	if(name && serialiser.GetLog())
	{
		const char *taskName = "NO TASK";

		if(taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
		{
			LOGGING_ONLY(taskName = TASKCLASSINFOMGR.GetTaskName(taskType));
		}

		serialiser.GetLog()->WriteDataValue(name, taskName);
	}
}

void SerialiseVehicleTaskType(CSyncDataBase& serialiser, u32 &taskType, const char *name)
{
	const unsigned SIZEOF_TASK_TYPE = datBitsNeeded<CTaskTypes::MAX_NUM_TASK_TYPES-CTaskTypes::TASK_VEHICLE_FIRST_TASK>::COUNT;

	if (taskType >= CTaskTypes::TASK_VEHICLE_FIRST_TASK)
		taskType -= CTaskTypes::TASK_VEHICLE_FIRST_TASK;

	SERIALISE_UNSIGNED(serialiser, taskType, SIZEOF_TASK_TYPE);

	taskType += CTaskTypes::TASK_VEHICLE_FIRST_TASK;

	if(name && serialiser.GetLog())
	{
		const char *taskName = "NO TASK";

		if(taskType != CTaskTypes::MAX_NUM_TASK_TYPES)
		{
			LOGGING_ONLY(taskName = TASKCLASSINFOMGR.GetTaskName(taskType));
		}

		serialiser.GetLog()->WriteDataValue(name, taskName);
	}

}

static void GetEnterExitFlagsForNetwork(const VehicleEnterExitFlags &iFlags, u32 &flagsPart1, u32 &flagsPart2, u32 &flagsPart3)
{
    flagsPart1 = 0;
    flagsPart2 = 0;
    flagsPart3 = 0;

    const unsigned numFlags1 = rage::Min(iFlags.BitSet().GetNumBits(), 32);
    const unsigned numFlags2 = (numFlags1 == 32) ? rage::Min((iFlags.BitSet().GetNumBits() - 32), 32) : 0;
    const unsigned numFlags3 = (numFlags2 == 32) ? rage::Min((iFlags.BitSet().GetNumBits() - 64), 32) : 0;
    taskAssertf(iFlags.BitSet().GetNumBits() <= 96, "More running flags exist than can currently be synced!");

    for(unsigned index = 0; index < numFlags1; index++)
    {
        if(iFlags.BitSet().IsSet(index))
        {
            flagsPart1 |= (1<<index);
        }
    }

    for(unsigned index = 0; index < numFlags2; index++)
    {
        if(iFlags.BitSet().IsSet(32 + index))
        {
            flagsPart2 |= (1<<index);
        }
    }

    for(unsigned index = 0; index < numFlags3; index++)
    {
        if(iFlags.BitSet().IsSet(64 + index))
        {
            flagsPart3 |= (1<<index);
        }
    }
}

static void SetEnterExitFlagsFromNetwork(VehicleEnterExitFlags &iFlags, u32 flagsPart1, u32 flagsPart2, u32 flagsPart3)
{
    iFlags.BitSet().Reset();

    static const unsigned MAX_POSSIBLE_FLAGS_PER_PART = 32;
    static const unsigned FLAGS_PART2_OFFSET          = MAX_POSSIBLE_FLAGS_PER_PART;
    static const unsigned FLAGS_PART3_OFFSET          = MAX_POSSIBLE_FLAGS_PER_PART * 2;

    for(unsigned index = 0; index < MAX_POSSIBLE_FLAGS_PER_PART; index++)
    {
        if((flagsPart1 & (1<<index)) != 0)
        {
            iFlags.BitSet().Set(index);
        }

        if((flagsPart2 & (1<<index)) != 0)
        {
            iFlags.BitSet().Set(FLAGS_PART2_OFFSET + index);
        }

        if((flagsPart3 & (1<<index)) != 0)
        {
            iFlags.BitSet().Set(FLAGS_PART3_OFFSET + index);
        }
    }
}

void SerialiseVehicleEnterExitFlags(CSyncDataBase& serialiser, VehicleEnterExitFlags &iRunningFlags, const char *name)
{
	const unsigned SIZEOF_ENTER_EXIT_FLAGS_1 = (CVehicleEnterExitFlags::Flags_NUM_ENUMS > 32) ? 32 : CVehicleEnterExitFlags::Flags_NUM_ENUMS;
    const unsigned SIZEOF_ENTER_EXIT_FLAGS_2 = (CVehicleEnterExitFlags::Flags_NUM_ENUMS > 32) ? rage::Min(CVehicleEnterExitFlags::Flags_NUM_ENUMS - 32, 32) : 0;
    const unsigned SIZEOF_ENTER_EXIT_FLAGS_3 = (CVehicleEnterExitFlags::Flags_NUM_ENUMS > 64) ? rage::Min(CVehicleEnterExitFlags::Flags_NUM_ENUMS - 64, 32) : 0;

	u32 enterExitFlags1 = 0;
	u32 enterExitFlags2 = 0;
    u32 enterExitFlags3 = 0;
	GetEnterExitFlagsForNetwork(iRunningFlags, enterExitFlags1, enterExitFlags2, enterExitFlags3);

	SERIALISE_UNSIGNED(serialiser, enterExitFlags1, SIZEOF_ENTER_EXIT_FLAGS_1);

	if(SIZEOF_ENTER_EXIT_FLAGS_2 > 0)
	{
		SERIALISE_UNSIGNED(serialiser, enterExitFlags2, SIZEOF_ENTER_EXIT_FLAGS_2);
	}

    if(SIZEOF_ENTER_EXIT_FLAGS_3 > 0)
	{
		SERIALISE_UNSIGNED(serialiser, enterExitFlags3, SIZEOF_ENTER_EXIT_FLAGS_3);
	}

	SetEnterExitFlagsFromNetwork(iRunningFlags, enterExitFlags1, enterExitFlags2, enterExitFlags3);

	if(name && serialiser.GetLog())
	{
		LogVehicleEnterExitFlags(*serialiser.GetLog(), iRunningFlags, name);
	}
}

void SerialiseCombatBehaviorFlags(CSyncDataBase& serialiser, CombatBehaviorFlags &behaviorFlags, const char* name)
{
	for (s32 i=0; i<CCombatData::MAX_COMBAT_FLAGS; i++)
	{
		bool bValue = behaviorFlags.BitSet().IsSet(i);
		SERIALISE_BOOL(serialiser, bValue);
		behaviorFlags.BitSet().Set(i,bValue);
	}

	if(name && serialiser.GetLog())
	{
		LogCombatBehaviorFlags(*serialiser.GetLog(), behaviorFlags, name);
	}
}

void SerialisePedPopType(CSyncDataBase& serialiser, u32 &popType, const char *name)
{
	const unsigned SIZEOF_POPTYPE = 4;  // to allow POPTYPE_TOOL & POPTYPE_CACHE

	SERIALISE_UNSIGNED(serialiser, popType, SIZEOF_POPTYPE);

	if(name && serialiser.GetLog())
	{
		LogPedPopType(*serialiser.GetLog(), popType, name);
	}
}

void SerialiseObjectCreatedBy(CSyncDataBase& serialiser, u32 &ownedBy, const char *name)
{
	const unsigned SIZEOF_OBJECT_CREATED_BY = 5;

	SERIALISE_UNSIGNED(serialiser, ownedBy, SIZEOF_OBJECT_CREATED_BY);

	if(name && serialiser.GetLog())
	{
		LogObjectCreatedBy(*serialiser.GetLog(), ownedBy, name);
	}
}

void SerialiseVehiclePopType(CSyncDataBase& serialiser, u32 &popType, const char *name)
{
	const unsigned SIZEOF_VEHICLE_POPTYPE = 4;  // to allow POPTYPE_TOOL & POPTYPE_CACHE

	SERIALISE_UNSIGNED(serialiser, popType, SIZEOF_VEHICLE_POPTYPE);

	if(name && serialiser.GetLog())
	{
		LogVehiclePopType(*serialiser.GetLog(), popType, name);
	}
}

void SerialiseMoveBlendState(CSyncDataBase& serialiser, u16 &moveBlendType, u32 &moveBlendState, const char *name)
{
	const unsigned SIZEOF_MOVEBLENDSTATE = 5;

	SERIALISE_UNSIGNED(serialiser, moveBlendState, SIZEOF_MOVEBLENDSTATE);

	if(name && serialiser.GetLog())
	{
		LogMoveBlendState(*serialiser.GetLog(), moveBlendType, moveBlendState, name);
	}
}

void SerialiseMotionGroup(CSyncDataBase& serialiser, fwMvClipSetId &motionGroupId, const char *name)
{
	const unsigned SIZEOF_MOTION_GROUP = 32;

	u32 motionGroupIdHash = motionGroupId.GetHash();
	SERIALISE_UNSIGNED(serialiser, motionGroupIdHash, SIZEOF_MOTION_GROUP);
	motionGroupId.SetHash(motionGroupIdHash);

	if(name && serialiser.GetLog())
	{
		LogMotionGroup(*serialiser.GetLog(), motionGroupId, name);
	}
}

void SerialiseRelationshipGroup(CSyncDataBase& serialiser, u32 &relGroup, const char *name)
{
	const unsigned SIZEOF_RELATIONSHIPS = 32;

	SERIALISE_UNSIGNED(serialiser, relGroup, SIZEOF_RELATIONSHIPS);

	if(name && serialiser.GetLog())
	{
		LogRelationshipGroup(*serialiser.GetLog(), relGroup, name);
	}
}

void SerialiseDecisionMaker(CSyncDataBase& serialiser, u32 &decisionMaker, const char *name)
{
	const unsigned SIZEOF_DECISIONMAKER = 32;

	SERIALISE_UNSIGNED(serialiser, decisionMaker, SIZEOF_DECISIONMAKER);

	if(name && serialiser.GetLog())
	{
		LogDecisionMaker(*serialiser.GetLog(), decisionMaker, name);
	}
}

void SerialiseWeaponType(CSyncDataBase& serialiser, u32 &weaponType, const char *name)
{
	SERIALISE_UNSIGNED(serialiser, weaponType, SIZEOF_WEAPON_TYPE);

	if(name && serialiser.GetLog())
	{
		LogWeaponType(*serialiser.GetLog(), weaponType, name);
	}
}

void SerialiseAmmoType(CSyncDataBase& serialiser, u32 &ammoType, const char *name)
{
	static const unsigned SIZEOF_AMMO = 9;

	SERIALISE_UNSIGNED(serialiser, ammoType, SIZEOF_AMMO);

	if(name && serialiser.GetLog())
	{
		LogAmmoType(*serialiser.GetLog(), ammoType, name);
	}
}

void SerialiseArrestState(CSyncDataBase& serialiser, u32 &arrestState, const char *name)
{
	static const unsigned SIZEOF_ARRESTSTATE = 1;

	SERIALISE_UNSIGNED(serialiser, arrestState, SIZEOF_ARRESTSTATE);

	if(name && serialiser.GetLog())
	{
		LogArrestState(*serialiser.GetLog(), arrestState, name);
	}
}

void SerialiseDeathState(CSyncDataBase& serialiser, u32 &deathState, const char *name)
{
	static const unsigned SIZEOF_DEATHSTATE = 2;

	SERIALISE_UNSIGNED(serialiser, deathState, SIZEOF_DEATHSTATE);

	if(name && serialiser.GetLog())
	{
		LogDeathState(*serialiser.GetLog(), deathState, name);
	}
}

void SerialisePlayerState(CSyncDataBase& serialiser, u32 &playerState, const char *name)
{
	const unsigned SIZEOF_PLAYER_STATE = datBitsNeeded<CPlayerInfo::PLAYERSTATE_MAX_STATES>::COUNT;

	SERIALISE_UNSIGNED(serialiser, playerState, SIZEOF_PLAYER_STATE);

	if(name && serialiser.GetLog())
	{
		LogPlayerState(*serialiser.GetLog(), playerState, name);
	}
}

void SerialiseTaskState(CSyncDataBase& serialiser, const s32 taskType, s32 &taskState, const u32 sizeOfState, const char *name)
{
	SERIALISE_UNSIGNED(serialiser, taskState, sizeOfState);

	if(name && serialiser.GetLog())
	{
		LogTaskState(*serialiser.GetLog(), taskType, taskState, name);
	}
}

void SerialiseDoorSystemState(CSyncDataBase& serialiser, u32& state, const char *name)
{
	static const unsigned SIZEOF_DOORSTATE = 3;

	SERIALISE_UNSIGNED(serialiser, state, SIZEOF_DOORSTATE);

	if (name && serialiser.GetLog())
	{
		LogDoorSystemState(*serialiser.GetLog(), state, name);
	}
}

void SerialiseMigrationType(CSyncDataBase& serialiser, eMigrationType& migrationType, const char *name)
{
	static const unsigned SIZEOF_MIGRATION_TYPE = datBitsNeeded<NUM_MIGRATION_TYPES-1>::COUNT;

	u32 mType = (u32)migrationType;
	SERIALISE_UNSIGNED(serialiser, mType, SIZEOF_MIGRATION_TYPE);
	migrationType = (eMigrationType)mType;

	if (name && serialiser.GetLog())
	{
		LogMigrationType(*serialiser.GetLog(), migrationType, name);
	}
}

void SerialiseIncidentType(CSyncDataBase& serialiser, u32& type, const char *name)
{
	unsigned SIZEOF_INCIDENT_TYPE	= datBitsNeeded<CIncident::IT_NumTypes>::COUNT;

	SERIALISE_UNSIGNED(serialiser, type, SIZEOF_INCIDENT_TYPE);

	if (serialiser.GetLog())
	{
		LogIncidentType(*serialiser.GetLog(), type, name);
	}
}

void SerialiseHash(CSyncDataBase& serialiser, u32 &hash, const char *name)
{
	const unsigned SIZEOF_HASH = 32;

	SERIALISE_UNSIGNED(serialiser, hash, SIZEOF_HASH);

	if(name && serialiser.GetLog())
	{
		const char *hashName = "Unknown";

#if !__FINAL
		atHashWithStringNotFinal hashString = atHashWithStringNotFinal(hash);
		hashName = hashString.TryGetCStr();
#endif //!__FINAL

		serialiser.GetLog()->WriteDataValue(name, "[Hash: %u] %s",hash, hashName);
	}
}

void SerialisePartialHash(CSyncDataBase& serialiser, u32 &partialHash, const char *name)
{
	const unsigned SIZEOF_PARTIAL_HASH = 32;

	SERIALISE_UNSIGNED(serialiser, partialHash, SIZEOF_PARTIAL_HASH);

	if(name && serialiser.GetLog())
	{
		const char *partialHashName = "Unknown";

#if !__FINAL
		atHashWithStringNotFinal finalisedHash = atHashWithStringNotFinal(atFinalizeHash(partialHash));
		partialHashName = finalisedHash.TryGetCStr();
#endif //!__FINAL

		serialiser.GetLog()->WriteDataValue(name, "[Partial hash: %u] %s",partialHash, partialHashName);
	}
}

void SerialiseRoadNodeAddress(CSyncDataBase& serialiser, s32 &nodeAddress, const char *name)
{
    const unsigned SIZEOF_ROAD_NODE_ADDRESS = 32;

    SERIALISE_INTEGER(serialiser, nodeAddress, SIZEOF_ROAD_NODE_ADDRESS);

    if(name && serialiser.GetLog())
    {
        CNodeAddress logNodeAddress;
        logNodeAddress.FromInt(nodeAddress);
        serialiser.GetLog()->WriteDataValue(name, "(%d:%d)", logNodeAddress.GetRegion(), logNodeAddress.GetIndex());
    }
}

#if !__FINAL
void LogGlobalFlags(netLoggingInterface &log, u32 globalFlags, const char *name)
{
    const unsigned MAX_BUFFER_SIZE = 256;
    char buffer[MAX_BUFFER_SIZE] = "";

    unsigned currentFlag = 0;
    while(globalFlags)
    {
        if(globalFlags & 1)
        {
            if(strlen(buffer))
            {
                safecat(buffer, ";", MAX_BUFFER_SIZE);
            }
            const char *flagName = CNetObjGame::GetGlobalFlagName(currentFlag);
            safecat(buffer, flagName, MAX_BUFFER_SIZE);
        }

        globalFlags>>=1;
        currentFlag++;
    }

    log.WriteDataValue(name, buffer);
}

void LogPedPopType(netLoggingInterface &log, u32 popType, const char *name)
{
    switch(popType)
    {
    case POPTYPE_UNKNOWN:
		log.WriteDataValue(name, "POPTYPE_UNKNOWN");
		gnetError("SerialiseCharPopType - Unexpected ped pop type - \"%s\"", name);
	    break;
    case POPTYPE_RANDOM_PERMANENT:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_PERMANENT");
	    break;
    case POPTYPE_RANDOM_PARKED:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_PARKED");
	    break;
    case POPTYPE_RANDOM_PATROL:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_PATROL");
	    break;
    case POPTYPE_RANDOM_SCENARIO:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_SCENARIO");
	    break;
    case POPTYPE_RANDOM_AMBIENT:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_AMBIENT");
	    break;
    case POPTYPE_PERMANENT:
	    log.WriteDataValue(name, "POPTYPE_PERMANENT");
	    break;
    case POPTYPE_MISSION:
	    log.WriteDataValue(name, "POPTYPE_MISSION");
	    break;
    case POPTYPE_REPLAY:
	    log.WriteDataValue(name, "POPTYPE_REPLAY");
	    break;
    case POPTYPE_TOOL:
	    log.WriteDataValue(name, "POPTYPE_TOOL");
		break;
	case POPTYPE_CACHE:
		log.WriteDataValue(name, "POPTYPE_CACHE");
	    break;
    }
}

void LogObjectCreatedBy(netLoggingInterface &log, u32 ownedBy, const char *name)
{
	bool bIsValid = false;

	switch(ownedBy)
	{
	case ENTITY_OWNEDBY_RANDOM:
	case ENTITY_OWNEDBY_SCRIPT:
	case ENTITY_OWNEDBY_TEMP:
	case ENTITY_OWNEDBY_FRAGMENT_CACHE:
	case ENTITY_OWNEDBY_CUTSCENE:
	case ENTITY_OWNEDBY_GAME:
    case ENTITY_OWNEDBY_POPULATION:
		bIsValid = true;
		break;
	default:
		break;
	}

	(void)bIsValid; // suppress warning 552

	switch(ownedBy)
    {
	case ENTITY_OWNEDBY_RANDOM:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_RANDOM");
		break;
	case ENTITY_OWNEDBY_TEMP:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_TEMP");
		break;
	case ENTITY_OWNEDBY_FRAGMENT_CACHE:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_FRAGMENT_CACHE");
		break;
	case ENTITY_OWNEDBY_GAME:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_GAME");
		break;
	case ENTITY_OWNEDBY_SCRIPT:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_SCRIPT");
		break;
	case ENTITY_OWNEDBY_AUDIO:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_AUDIO");
		break;
	case ENTITY_OWNEDBY_CUTSCENE:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_CUTSCENE");
		break;
	case ENTITY_OWNEDBY_DEBUG:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_DEBUG");
		break;
	case ENTITY_OWNEDBY_OTHER:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_OTHER");
		break;
	case ENTITY_OWNEDBY_PROCEDURAL:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_PROCEDURAL");
		break;
	case ENTITY_OWNEDBY_POPULATION:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_POPULATION");
		break;
	case ENTITY_OWNEDBY_STATICBOUNDS:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_STATICBOUNDS");
		break;
	case ENTITY_OWNEDBY_PHYSICS:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_PHYSICS");
		break;
	case ENTITY_OWNEDBY_IPL:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_IPL");
		break;
	case ENTITY_OWNEDBY_VFX:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_VFX");
		break;
	case ENTITY_OWNEDBY_NAVMESHEXPORTER:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_NAVMESHEXPORTER");
		break;
	case ENTITY_OWNEDBY_INTERIOR:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_INTERIOR");
		break;
	case ENTITY_OWNEDBY_COMPENTITY:
		log.WriteDataValue(name, "ENTITY_OWNEDBY_COMPENTITY");
		break;
	default:
        gnetAssertf(0, "Unknown owned-by type %d", ownedBy);
    }

	gnetAssertf(bIsValid, "Owned-by type %d should not be syched across the network", ownedBy);
}

void LogVehiclePopType(netLoggingInterface &log, u32 popType, const char *name)
{
    switch(popType)
    {
    case POPTYPE_UNKNOWN:
	    {
		    gnetError("SerialiseVehiclePopType - Unexpected vehicle population type - \"%s\"", name);
	    }
	    break;
    case POPTYPE_RANDOM_PERMANENT:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_PERMANENT");
	    break;
    case POPTYPE_RANDOM_PARKED:
        log.WriteDataValue(name, "POPTYPE_RANDOM_PARKED");
        break;
    case POPTYPE_RANDOM_PATROL:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_PATROL");
	    break;
    case POPTYPE_RANDOM_SCENARIO:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_SCENARIO");
	    break;
    case POPTYPE_RANDOM_AMBIENT:
	    log.WriteDataValue(name, "POPTYPE_RANDOM_AMBIENT");
	    break;
    case POPTYPE_PERMANENT:
	    log.WriteDataValue(name, "POPTYPE_PERMANENT");
	    break;
    case POPTYPE_MISSION:
	    log.WriteDataValue(name, "POPTYPE_MISSION");
	    break;
    case POPTYPE_REPLAY:
	    log.WriteDataValue(name, "POPTYPE_REPLAY");
	    break;
    case POPTYPE_TOOL:
	    log.WriteDataValue(name, "POPTYPE_TOOL");
	    break;
	case POPTYPE_CACHE:
		log.WriteDataValue(name, "POPTYPE_CACHE");
		break;
    }
}

void LogMoveBlendState(netLoggingInterface &log, u16 moveBlendType, u32 moveBlendState, const char *name)
{
	const char *type = "UNKNOWN";

	if(moveBlendType == CTaskTypes::TASK_MOTION_PED)
	{
		switch(moveBlendState)
		{
			case CTaskMotionPed::State_Start:			type = "START";				break;
			case CTaskMotionPed::State_OnFoot:			type = "ON FOOT";			break;
			case CTaskMotionPed::State_Strafing:		type = "STRAFING";			break;
			case CTaskMotionPed::State_StrafeToOnFoot:	type = "STRAFE TO ON FOOT";	break;
			case CTaskMotionPed::State_Swimming:		type = "SWIMMING";			break;
			case CTaskMotionPed::State_SwimToRun:		type = "SWIM TO RUN";		break;
			case CTaskMotionPed::State_DiveToSwim:		type = "DIVE TO SWIM";		break;
			case CTaskMotionPed::State_Diving:			type = "DIVING";			break;
			case CTaskMotionPed::State_DoNothing:		type = "DO NOTHING";		break;
			case CTaskMotionPed::State_AnimatedVelocity:type = "ANIMATED VELOCITY";	break;
			case CTaskMotionPed::State_Crouch:			type = "CROUCHING";			break;
			case CTaskMotionPed::State_Stealth:			type = "STEALTH";			break;
			case CTaskMotionPed::State_InVehicle:		type = "IN VEHICLE";		break;
			case CTaskMotionPed::State_Aiming:			type = "AIMING";			break;
			case CTaskMotionPed::State_StandToCrouch:	type = "STAND TO CROUCH";	break;
			case CTaskMotionPed::State_CrouchToStand:	type = "CROUCH TO STAND";	break;
			case CTaskMotionPed::State_ActionMode:		type = "ACTION MODE";		break;
			case CTaskMotionPed::State_Parachuting:		type = "PARACHUTING";		break;
			case CTaskMotionPed::State_Jetpack:			type = "JETPACK";			break;
#if ENABLE_DRUNK
			case CTaskMotionPed::State_Drunk:			type = "DRUNK";				break;
#endif // ENABLE_DRUNK
			case CTaskMotionPed::State_Dead:			type = "DEAD";				break;
			case CTaskMotionPed::State_Exit:			type = "EXIT";				break;
			default:
				gnetAssertf(0, "Unexpected move blend state! - someone has added a new state to CTaskMotionPed - be sure to check it's valid in NetObjPed::UpdatePedPendingMovementData");
		}
	}
	else if(moveBlendType == CTaskTypes::TASK_MOTION_PED_LOW_LOD)
	{
		switch(moveBlendState)
		{
			case CTaskMotionPedLowLod::State_Start:			type = "START";			break;
			case CTaskMotionPedLowLod::State_OnFoot:		type = "ON FOOT";		break;
			case CTaskMotionPedLowLod::State_Swimming:		type = "SWIMMING";		break;
			case CTaskMotionPedLowLod::State_InVehicle:		type = "IN VEHICLE";	break;
			case CTaskMotionPedLowLod::State_Exit:			type = "EXIT";			break;
			default:
				gnetAssertf(0, "Unexpected move blend state! - someone has added a new state to CTaskMotionPedLowLod - be sure to check it's valid in NetObjPed::UpdatePedLowLodPendingMovementData");
		}
	}
	else if(moveBlendType == CTaskTypes::TASK_ON_FOOT_BIRD)
	{
		switch(moveBlendState)
		{
			case CTaskBirdLocomotion::State_Initial:	type = "INITIAL";      break;
			case CTaskBirdLocomotion::State_Idle:	    type = "IDLE";         break;
			case CTaskBirdLocomotion::State_Fly:		type = "FLY";		   break;
			case CTaskBirdLocomotion::State_Glide:		type = "GLIDE";		   break;	
			case CTaskBirdLocomotion::State_Walk:		type = "WALK";         break;	
			case CTaskBirdLocomotion::State_TakeOff:	type = "TAKE OFF";     break;
			case CTaskBirdLocomotion::State_Land:     	type = "LAND";         break;
			case CTaskBirdLocomotion::State_Descend:    type = "DESCEND";      break;
			case CTaskBirdLocomotion::State_FlyUpwards: type = "FLY UPWARDS";  break;				
			case CTaskBirdLocomotion::State_Dead:       type = "DEAD";         break;		
				
			default:
				gnetAssertf(0, "Unexpected move blend state! - someone has added a new state to CTaskBirdLocomotion (%d)", moveBlendState);
		}

	}
#if ENABLE_HORSE
	else if(moveBlendType == CTaskTypes::TASK_ON_FOOT_HORSE)
	{
		switch(moveBlendState)
		{
			case CTaskHorseLocomotion::State_Initial:			type = "INITIAL";		break;
			case CTaskHorseLocomotion::State_Idle:				type = "IDLE";			break;
			case CTaskHorseLocomotion::State_TurnInPlace:		type = "TURN IN PLACE";	break;
			case CTaskHorseLocomotion::State_Locomotion:		type = "LOCOMOTION";	break;
			case CTaskHorseLocomotion::State_QuickStop:			type = "QUICK STOP";	break;
			case CTaskHorseLocomotion::State_RearUp:			type = "REAR UP";		break;
			case CTaskHorseLocomotion::State_Slope_Scramble:	type = "SLOPE SCRAMBLE";break;
			default:
				gnetAssertf(0, "Unexpected move blend state! - someone has added a new state to CTaskHorseLocomotion - be sure to check it's valid in NetObjPed::UpdatePedOnHorsePendingMovementData");
		}
	}
	else if(moveBlendType == CTaskTypes::TASK_MOTION_RIDE_HORSE)
	{
		switch(moveBlendState)
		{
			case CTaskMotionRideHorse::State_Start:				type = "START";			break;
			case CTaskMotionRideHorse::State_StreamClips:		type = "STREAM CLIPS";	break;
			case CTaskMotionRideHorse::State_SitIdle:			type = "SIT IDLE";		break;
			case CTaskMotionRideHorse::State_Locomotion:		type = "LOCOMOTION";	break;
			case CTaskMotionRideHorse::State_QuickStop:			type = "QUICK STOP";	break;
			case CTaskMotionRideHorse::State_Exit:				type = "EXIT";			break;
			default:
				gnetAssertf(0, "Unexpected move blend state! - someone has added a new state to CTaskMotionRideHorse - be sure to check it's valid in NetObjPed::UpdateHorsePendingMovementData");
		}
	}
#endif

    log.WriteDataValue(name, type);
}

void LogMotionGroup(netLoggingInterface &log, fwMvClipSetId motionGroupId, const char *name)
{
    if(motionGroupId.GetCStr())
	{
		log.WriteDataValue(name, motionGroupId.GetCStr());
	}
}

void LogRelationshipGroup(netLoggingInterface &log, u32 relGroup, const char *name)
{
    if(relGroup == 0)
    {
        log.WriteDataValue(name, "None");
    }
    else
    {
        CRelationshipGroup *group = CRelationshipManager::FindRelationshipGroup(relGroup);

        if(group == 0)
        {
            log.WriteDataValue(name, "Unknown rel group: %u", relGroup);
        }
        else
        {
            log.WriteDataValue(name, group->GetName().GetCStr());
        }
    }
}

void LogDecisionMaker(netLoggingInterface &log, u32 decisionMaker, const char *name)
{
    if(decisionMaker == 0)
    {
        log.WriteDataValue(name, "None");
    }
    else
    {
        CEventDecisionMaker *eventDecisionMaker = CEventDecisionMakerManager::GetDecisionMaker(decisionMaker);

        if(eventDecisionMaker == 0)
        {
            log.WriteDataValue(name, "Unknown DM: %u", decisionMaker);
        }
        else
        {
            log.WriteDataValue(name, eventDecisionMaker->GetName());
        }
    }
}

void LogWeaponType(netLoggingInterface &log, u32 weaponType, const char *name)
{
    if(weaponType == NO_WEAPON_TYPE)
    {
        log.WriteDataValue(name, "No weapon");
    }
    else 
    {
        const CItemInfo *itemInfo = CWeaponInfoManager::GetInfoFromSlot(weaponType);

        if(itemInfo == 0)
        {
            log.WriteDataValue(name, "Unknown weapon: %u", weaponType);
        }
        else
        {
#if !__FINAL
            log.WriteDataValue(name, itemInfo->GetName());
#endif // !__FINAL 
        }
    }
}

void LogAmmoType(netLoggingInterface &log, u32 ammoType, const char *name)
{
    if(ammoType == 0)
    {
        log.WriteDataValue(name, "No ammo");
    }
    else 
    {
        const CItemInfo *itemInfo = CWeaponInfoManager::GetInfoFromSlot(ammoType);

        if(itemInfo == 0)
        {
            log.WriteDataValue(name, "Unknown ammo: %u", ammoType);
        }
        else
        {
            log.WriteDataValue(name, itemInfo->GetName());
        }
    }
}

void LogArrestState(netLoggingInterface &log, u32 arrestState, const char *name)
{
    const char *state = "UNKNOWN";

    switch(arrestState)
    {
    case ArrestState_None:
        state = "NONE";
        break;
    case ArrestState_Arrested:
        state = "ARRESTED";
        break;
    default:
        gnetAssertf(0, "Unexpected arrest state!");
        break;
    }

    log.WriteDataValue(name, state);
}

void LogDeathState(netLoggingInterface &log, u32 deathState, const char *name)
{
    const char *state = "UNKNOWN";

    switch(deathState)
    {
    case DeathState_Alive:
        state = "ALIVE";
        break;
    case DeathState_Dying:
        state = "DYING";
        break;
    case DeathState_Dead:
        state = "DEAD";
        break;
    default:
        gnetAssertf(0, "Unexpected death state!");
        break;
    }

    log.WriteDataValue(name, state);
}

void LogPlayerState(netLoggingInterface &log, u32 playerState, const char *name)
{
    const char *state = "UNKNOWN";

    //Note: Player state is synchronised with an extra bit to handle the -1 case
    //      when syncing as an unsigned value (which saves requiring the sign bit)
    switch((int)playerState - 1)
    {
    case CPlayerInfo::PLAYERSTATE_INVALID:
        state = "UNKNOWN";
        break;
    case CPlayerInfo::PLAYERSTATE_PLAYING:
        state = "PLAYING";
        break;
    case CPlayerInfo::PLAYERSTATE_HASDIED:
        state = "DIED";
        break;
    case CPlayerInfo::PLAYERSTATE_HASBEENARRESTED:
        state = "ARRESTED";
        break;
    case CPlayerInfo::PLAYERSTATE_FAILEDMISSION:
        state = "FAILED MISSION";
        break;
    case CPlayerInfo::PLAYERSTATE_LEFTGAME:
        state = "LEFT GAME";
        break;
    case CPlayerInfo::PLAYERSTATE_RESPAWN:
        state = "RESPAWN";
        break;
    case CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE:
        state = "IN MP CUTSCENE";
        break;
    default:
        gnetAssertf(0, "Unexpected player state!");
    }

    log.WriteDataValue(name, state);
}


void LogTaskState(netLoggingInterface &log, s32 taskType, s32 taskState, const char *name)
{
	const char* stateName = TASKCLASSINFOMGR.GetTaskStateName( taskType, taskState );

	if (gnetVerifyf(stateName, "No state name defined for state %d, task %d (%s)", taskState, taskType, TASKCLASSINFOMGR.GetTaskName(taskType)))
	{
		log.WriteDataValue(name, stateName);
	}
}

EXTERN_PARSER_ENUM(CVehicleEnterExitFlags__Flags);

void LogVehicleEnterExitFlags(netLoggingInterface &log, const VehicleEnterExitFlags &iFlags, const char *name)
{
    const unsigned MAX_BUFFER_SIZE = 640;
    char buffer[MAX_BUFFER_SIZE] = "";

    const unsigned numBits = iFlags.BitSet().GetNumBits();

    for(unsigned index = 0; index < numBits; index++)
    {
        if(iFlags.BitSet().IsSet(index))
        {
            if(strlen(buffer))
            {
                safecat(buffer, ";", MAX_BUFFER_SIZE);
                buffer[MAX_BUFFER_SIZE-1] = '\0';
            }
            const char *flagName = PARSER_ENUM(CVehicleEnterExitFlags__Flags).m_Names[index];
            safecat(buffer, flagName, MAX_BUFFER_SIZE);
            buffer[MAX_BUFFER_SIZE-1] = '\0';
        }
    }

    log.WriteDataValue(name, buffer);
}

EXTERN_PARSER_ENUM(CCombatData__BehaviourFlags);

void LogCombatBehaviorFlags(netLoggingInterface &log, const CombatBehaviorFlags &iFlags, const char *name)
{
	const unsigned MAX_BUFFER_SIZE = 640;
	char buffer[MAX_BUFFER_SIZE] = "";

	const unsigned numBits = iFlags.BitSet().GetNumBits();

	for(unsigned index = 0; index < numBits; index++)
	{
		if(iFlags.BitSet().IsSet(index))
		{
			if(strlen(buffer))
			{
				safecat(buffer, ";", MAX_BUFFER_SIZE);
				buffer[MAX_BUFFER_SIZE-1] = '\0';
			}
			const char *flagName = PARSER_ENUM(CCombatData__BehaviourFlags).m_Names[index];
			safecat(buffer, flagName, MAX_BUFFER_SIZE);
			buffer[MAX_BUFFER_SIZE-1] = '\0';
		}
	}

	log.WriteDataValue(name, buffer);
}

void LogDoorSystemState(netLoggingInterface &log, const u32 state, const char *name)
{
	const char *stateStr = "UNKNOWN";

	switch(state)
	{
	case DOORSTATE_UNLOCKED:
		stateStr = "UNLOCKED";
		break;
	case DOORSTATE_LOCKED:
		stateStr = "LOCKED";
		break;
	case DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA:
		stateStr = "DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA";
		break;
	case DOORSTATE_FORCE_UNLOCKED_THIS_FRAME:
		stateStr = "DOORSTATE_FORCE_UNLOCKED_THIS_FRAME";
		break;
	case DOORSTATE_FORCE_LOCKED_THIS_FRAME:
		stateStr = "DOORSTATE_FORCE_LOCKED_THIS_FRAME";
		break;
	case DOORSTATE_FORCE_OPEN_THIS_FRAME:
		stateStr = "DOORSTATE_FORCE_OPEN_THIS_FRAME";
		break;
	case DOORSTATE_FORCE_CLOSED_THIS_FRAME:
		stateStr = "DOORSTATE_FORCE_CLOSED_THIS_FRAME";
		break;
	default:
		gnetAssertf(0, "Unexpected door state!");
	}

	log.WriteDataValue(name, stateStr);
}

void LogMigrationType(netLoggingInterface &log, const eMigrationType migrationType, const char *name)
{
	const char *migrateStr = "UNKNOWN";

	switch(migrationType)
	{
	case MIGRATE_PROXIMITY:
		migrateStr = "PROXIMITY";
		break;
	case MIGRATE_OUT_OF_SCOPE:
		migrateStr = "OUT_OF_SCOPE";
		break;
	case MIGRATE_SCRIPT:
		migrateStr = "SCRIPT";
		break;
	case MIGRATE_FORCED:
		migrateStr = "FORCED";
		break;
    case MIGRATE_FROZEN_PED:
        migrateStr = "FROZEN_PED";
		break;
	default:
		gnetAssertf(0, "Unexpected migration type!");
	}

	log.WriteDataValue(name, migrateStr);
}

void LogIncidentType(netLoggingInterface &log, const u32 type, const char *name)
{
	const char *incidentStr = "UNKNOWN";

	switch(type)
	{
	case CIncident::IT_Invalid:
		incidentStr = "INVALID";
		break;
	case CIncident::IT_Wanted:
		incidentStr = "WANTED";
		break;
	case CIncident::IT_Fire:
		incidentStr = "FIRE";
		break;
	case CIncident::IT_Injury:
		incidentStr = "INJURY";
		break;
	case CIncident::IT_Scripted:
		incidentStr = "SCRIPTED";
		break;
	case CIncident::IT_Arrest:
		incidentStr = "ARREST";
		break;
	default:
		gnetAssertf(0, "Unexpected incident type!");
	}

	log.WriteDataValue(name, incidentStr);
}
#endif //!__FINAL
