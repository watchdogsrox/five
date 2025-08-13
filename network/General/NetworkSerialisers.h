#ifndef NETWORK_SERIALISERS_H
#define NETWORK_SERIALISERS_H

#include "task/system/TaskClassInfo.h"
#include "entity/archetypeManager.h"
#include "fwanimation/animdefines.h"
#include "fwanimation/animmanager.h"
#include "fwnet/netlog.h"
#include "fwnet/netlogginginterface.h"
#include "fwnet/netserialisers.h"
#include "fwnet/nettypes.h"
#include "data/bitbuffer.h"
#include "modelInfo/ModelInfo.h"
#include "peds\CombatData.h"
#include "vehicles\Metadata\VehicleEnterExitFlags.h"

typedef CVehicleEnterExitFlags::FlagsBitSet VehicleEnterExitFlags;
typedef CCombatData::BehaviourFlagsBitSet CombatBehaviorFlags;

static const unsigned int SIZEOF_WEAPON_TYPE = 9;  
static const unsigned int NO_WEAPON_TYPE = (1<<SIZEOF_WEAPON_TYPE)-1;  

//-------------------------------------------------------------------------------
//Dead strip out name
#if ENABLE_NETWORK_LOGGING
#define SERIALISE_GLOBAL_FLAGS(serialiser,globalflags,...)						SerialiseGlobalFlags(serialiser,globalflags, ##__VA_ARGS__);
#define SERIALISE_ANIMATION_CLIP(serialiser,clipsetid,clipid,...)				SerialiseAnimation(serialiser,clipsetid,clipid, ##__VA_ARGS__);
#define SERIALISE_ANIMATION_ANIM(serialiser,animslotindex,animhashkey,...)		SerialiseAnimation(serialiser,animslotindex,animhashkey, ##__VA_ARGS__);
#define SERIALISE_ANIM_BLEND(serialiser,blend,...)								SerialiseAnimBlend(serialiser,blend, ##__VA_ARGS__);
#define SERIALISE_MODELHASH(serialiser,modelhash,...)							SerialiseModelHash(serialiser,modelhash, ##__VA_ARGS__);
#define SERIALISE_WEAPON(serialiser,weaponhash,...)							    SerialiseWeapon(serialiser,weaponhash, ##__VA_ARGS__);
#define SERIALISE_VELOCITY(serialiser,packedvelx,packedvely,packedvelz,sizeofvelocity,...)	SerialiseVelocity(serialiser,packedvelx,packedvely,packedvelz,sizeofvelocity, ##__VA_ARGS__);
#define SERIALISE_TASKTYPE(serialiser,tasktype,...)								SerialiseTaskType(serialiser,tasktype, ##__VA_ARGS__);
#define SERIALISE_VEHICLE_TASKTYPE(serialiser,tasktype,...)						SerialiseVehicleTaskType(serialiser,tasktype, ##__VA_ARGS__);
#define SERIALISE_VEHICLE_ENTER_EXIT_FLAGS(serialiser,irunningflags,...)		SerialiseVehicleEnterExitFlags(serialiser,irunningflags, ##__VA_ARGS__);
#define SERIALISE_COMBAT_BEHAVIOR_FLAGS(serialiser,behaviorflags,...)			SerialiseCombatBehaviorFlags(serialiser,behaviorflags, ##__VA_ARGS__);
#define SERIALISE_PED_POP_TYPE(serialiser,poptype,...)							SerialisePedPopType(serialiser,poptype, ##__VA_ARGS__);
#define SERIALISE_OBJECT_CREATED_BY(serialiser,ownedby,...)						SerialiseObjectCreatedBy(serialiser,ownedby, ##__VA_ARGS__);
#define SERIALISE_VEHICLE_POP_TYPE(serialiser,poptype,...)						SerialiseVehiclePopType(serialiser,poptype, ##__VA_ARGS__);
#define SERIALISE_MOVE_BLEND_STATE(serialiser,moveblendtype,moveblendstate,...)	SerialiseMoveBlendState(serialiser,moveblendtype,moveblendstate, ##__VA_ARGS__);
#define SERIALISE_MOTION_GROUP(serialiser,motiongroupid,...)					SerialiseMotionGroup(serialiser,motiongroupid, ##__VA_ARGS__);
#define SERIALISE_RELATIONSHIP_GROUP(serialiser,relgroup,...)					SerialiseRelationshipGroup(serialiser,relgroup, ##__VA_ARGS__);
#define SERIALISE_DECISION_MAKER(serialiser,decisionmaker,...)					SerialiseDecisionMaker(serialiser,decisionmaker, ##__VA_ARGS__);
#define SERIALISE_WEAPON_TYPE(serialiser,weapontype,...)						SerialiseWeaponType(serialiser,weapontype, ##__VA_ARGS__);
#define SERIALISE_AMMO_TYPE(serialiser,ammotype,...)							SerialiseAmmoType(serialiser,ammotype, ##__VA_ARGS__);
#define SERIALISE_ARREST_STATE(serialiser,arreststate,...)						SerialiseArrestState(serialiser,arreststate, ##__VA_ARGS__);
#define SERIALISE_DEATH_STATE(serialiser,deathstate,...)						SerialiseDeathState(serialiser,deathstate, ##__VA_ARGS__);
#define SERIALISE_PLAYER_STATE(serialiser,playerstate,...)						SerialisePlayerState(serialiser,playerstate, ##__VA_ARGS__);
#define SERIALISE_TASK_STATE(serialiser,tasktype,taskstate,sizeofstate,...)		SerialiseTaskState(serialiser,tasktype,taskstate,sizeofstate, ##__VA_ARGS__);
#define SERIALISE_DOOR_SYSTEM_STATE(serialiser,state,...)						SerialiseDoorSystemState(serialiser,state, ##__VA_ARGS__);
#define SERIALISE_MIGRATION_TYPE(serialiser,migrationtype,...)					SerialiseMigrationType(serialiser,migrationtype, ##__VA_ARGS__);
#define SERIALISE_INCIDENT_TYPE(serialiser,type,...)							SerialiseIncidentType(serialiser,type, ##__VA_ARGS__);
#define SERIALISE_HASH(serialiser,hash,...)										SerialiseHash(serialiser,hash, ##__VA_ARGS__);
#define SERIALISE_PARTIALHASH(serialiser,partialhash,...)						SerialisePartialHash(serialiser,partialhash, ##__VA_ARGS__);
#define SERIALISE_ROAD_NODE_ADDRESS(serialiser,nodeAddress,...)					SerialiseRoadNodeAddress(serialiser,nodeAddress, ##__VA_ARGS__);
#else
#define SERIALISE_GLOBAL_FLAGS(serialiser,globalflags,...)						SerialiseGlobalFlags(serialiser,globalflags);
#define SERIALISE_ANIMATION_CLIP(serialiser,clipsetid,clipid,...)				SerialiseAnimation(serialiser,clipsetid,clipid);
#define SERIALISE_ANIMATION_ANIM(serialiser,animslotindex,animhashkey,...)		SerialiseAnimation(serialiser,animslotindex,animhashkey);
#define SERIALISE_ANIM_BLEND(serialiser,blend,...)								SerialiseAnimBlend(serialiser,blend);
#define SERIALISE_MODELHASH(serialiser,modelhash,...)							SerialiseModelHash(serialiser,modelhash);
#define SERIALISE_WEAPON(serialiser,weaponhash,...)							    SerialiseWeapon(serialiser,weaponhash);
#define SERIALISE_VELOCITY(serialiser,packedvelx,packedvely,packedvelz,sizeofvelocity,...)	SerialiseVelocity(serialiser,packedvelx,packedvely,packedvelz,sizeofvelocity);
#define SERIALISE_TASKTYPE(serialiser,tasktype,...)								SerialiseTaskType(serialiser,tasktype);
#define SERIALISE_VEHICLE_TASKTYPE(serialiser,tasktype,...)						SerialiseVehicleTaskType(serialiser,tasktype);
#define SERIALISE_VEHICLE_ENTER_EXIT_FLAGS(serialiser,irunningflags,...)		SerialiseVehicleEnterExitFlags(serialiser,irunningflags);
#define SERIALISE_COMBAT_BEHAVIOR_FLAGS(serialiser,behaviorflags,...)			SerialiseCombatBehaviorFlags(serialiser,behaviorflags);
#define SERIALISE_PED_POP_TYPE(serialiser,poptype,...)							SerialisePedPopType(serialiser,poptype);
#define SERIALISE_OBJECT_CREATED_BY(serialiser,ownedby,...)						SerialiseObjectCreatedBy(serialiser,ownedby);
#define SERIALISE_VEHICLE_POP_TYPE(serialiser,poptype,...)						SerialiseVehiclePopType(serialiser,poptype);
#define SERIALISE_MOVE_BLEND_STATE(serialiser,moveblendtype,moveblendstate,...)	SerialiseMoveBlendState(serialiser,moveblendtype,moveblendstate);
#define SERIALISE_MOTION_GROUP(serialiser,motiongroupid,...)					SerialiseMotionGroup(serialiser,motiongroupid);
#define SERIALISE_RELATIONSHIP_GROUP(serialiser,relgroup,...)					SerialiseRelationshipGroup(serialiser,relgroup);
#define SERIALISE_DECISION_MAKER(serialiser,decisionmaker,...)					SerialiseDecisionMaker(serialiser,decisionmaker);
#define SERIALISE_WEAPON_TYPE(serialiser,weapontype,...)						SerialiseWeaponType(serialiser,weapontype);
#define SERIALISE_AMMO_TYPE(serialiser,ammotype,...)							SerialiseAmmoType(serialiser,ammotype);
#define SERIALISE_ARREST_STATE(serialiser,arreststate,...)						SerialiseArrestState(serialiser,arreststate);
#define SERIALISE_DEATH_STATE(serialiser,deathstate,...)						SerialiseDeathState(serialiser,deathstate);
#define SERIALISE_PLAYER_STATE(serialiser,playerstate,...)						SerialisePlayerState(serialiser,playerstate);
#define SERIALISE_TASK_STATE(serialiser,tasktype,taskstate,sizeofstate,...)		SerialiseTaskState(serialiser,tasktype,taskstate,sizeofstate);
#define SERIALISE_DOOR_SYSTEM_STATE(serialiser,state,...)						SerialiseDoorSystemState(serialiser,state);
#define SERIALISE_MIGRATION_TYPE(serialiser,migrationtype,...)					SerialiseMigrationType(serialiser,migrationtype);
#define SERIALISE_INCIDENT_TYPE(serialiser,type,...)							SerialiseIncidentType(serialiser,type);
#define SERIALISE_HASH(serialiser,hash,...)										SerialiseHash(serialiser,hash);
#define SERIALISE_PARTIALHASH(serialiser,partialhash,...)						SerialisePartialHash(serialiser,partialhash);
#define SERIALISE_ROAD_NODE_ADDRESS(serialiser,nodeAddress,...)					SerialiseRoadNodeAddress(serialiser,nodeAddress);
#endif
//-------------------------------------------------------------------------------


void SerialiseGlobalFlags(CSyncDataBase& serialiser, u32 &globalFlags, const char *name = NULL);
void SerialiseAnimation(CSyncDataBase& serialiser, fwMvClipSetId &clipSetId, fwMvClipId &clipId, const char *name = NULL);
void SerialiseAnimation(CSyncDataBase& serialiser, s32 &animSlotIndex, u32 &animHashKey, const char *name = NULL);
void SerialiseAnimBlend(CSyncDataBase& serialiser, float& blend, const char *name = NULL);
void SerialiseModelHash(CSyncDataBase& serialiser, u32 &modelHash, const char *name = NULL);
void SerialiseWeapon(CSyncDataBase& serialiser, u32 &weaponHash, const char *name = NULL);
void SerialiseVelocity(CSyncDataBase& serialiser, s32 &packedVelX, s32 &packedVelY, s32 &packedVelZ, int sizeOfVelocity, const char *name = NULL);
void SerialiseTaskType(CSyncDataBase& serialiser, u32 &taskType, const char *name = NULL);
void SerialiseVehicleTaskType(CSyncDataBase& serialiser, u32 &taskType, const char *name = NULL);
void SerialiseVehicleEnterExitFlags(CSyncDataBase& serialiser, VehicleEnterExitFlags &iRunningFlags, const char *name = NULL);
void SerialiseCombatBehaviorFlags(CSyncDataBase& serialiser, CombatBehaviorFlags &behaviorFlags, const char* name = NULL);
void SerialisePedPopType(CSyncDataBase& serialiser, u32 &popType, const char *name = NULL);
void SerialiseObjectCreatedBy(CSyncDataBase& serialiser, u32 &ownedBy, const char *name = NULL);
void SerialiseVehiclePopType(CSyncDataBase& serialiser, u32 &popType, const char *name = NULL);
void SerialiseMoveBlendState(CSyncDataBase& serialiser, u16 &moveBlendType, u32 &moveBlendState, const char *name = NULL);
void SerialiseMotionGroup(CSyncDataBase& serialiser, fwMvClipSetId &motionGroupId, const char *name = NULL);
void SerialiseRelationshipGroup(CSyncDataBase& serialiser, u32 &relGroup, const char *name = NULL);
void SerialiseDecisionMaker(CSyncDataBase& serialiser, u32 &decisionMaker, const char *name = NULL);
void SerialiseWeaponType(CSyncDataBase& serialiser, u32 &weaponType, const char *name = NULL);
void SerialiseAmmoType(CSyncDataBase& serialiser, u32 &ammoType, const char *name = NULL);
void SerialiseArrestState(CSyncDataBase& serialiser, u32 &arrestState, const char *name = NULL);
void SerialiseDeathState(CSyncDataBase& serialiser, u32 &deathState, const char *name = NULL);
void SerialisePlayerState(CSyncDataBase& serialiser, u32 &playerState, const char *name = NULL);
void SerialiseTaskState(CSyncDataBase& serialiser, const s32 taskType, s32 &taskState, const u32 sizeOfState, const char *name = NULL);
void SerialiseDoorSystemState(CSyncDataBase& serialiser, u32& state, const char *name = NULL);
void SerialiseMigrationType(CSyncDataBase& serialiser, eMigrationType& migrationType, const char *name = NULL);
void SerialiseIncidentType(CSyncDataBase& serialiser, u32& type, const char *name = NULL);
void SerialiseHash(CSyncDataBase& serialiser, u32 &hash, const char *name = NULL);
void SerialisePartialHash(CSyncDataBase& serialiser, u32 &partialHash, const char *name = NULL);
void SerialiseRoadNodeAddress(CSyncDataBase& serialiser, s32 &roadNodeAddress, const char *name = NULL);

#if __FINAL
inline void LogGlobalFlags(netLoggingInterface &, u32, const char *)	{}
inline void LogPedPopType(netLoggingInterface &, u32, const char *)	{}
inline void LogObjectCreatedBy(netLoggingInterface &, u32, const char *)	{}
inline void LogVehiclePopType(netLoggingInterface &, u32, const char *)	{}
inline void LogMoveBlendState(netLoggingInterface &, u16, u32, const char *)	{}
inline void LogMotionGroup(netLoggingInterface &, fwMvClipSetId, const char *)	{}
inline void LogRelationshipGroup(netLoggingInterface &, u32, const char *)	{}
inline void LogDecisionMaker(netLoggingInterface &, u32, const char *)	{}
inline void LogWeaponType(netLoggingInterface &, u32, const char *)	{}
inline void LogAmmoType(netLoggingInterface &, u32, const char *)	{}
inline void LogArrestState(netLoggingInterface &, u32, const char *)	{}
inline void LogDeathState(netLoggingInterface &, u32, const char *)	{}
inline void LogPlayerState(netLoggingInterface &, u32, const char *)	{}
inline void LogVehicleEnterExitFlags(netLoggingInterface &, const VehicleEnterExitFlags &, const char *)	{}
inline void LogCombatBehaviorFlags(netLoggingInterface &, const CombatBehaviorFlags &, const char *)	{}
inline void LogDoorSystemState(netLoggingInterface &, const u32, const char *)	{}
inline void LogMigrationType(netLoggingInterface &, const eMigrationType, const char *)	{}
inline void LogIncidentType(netLoggingInterface &, const u32, const char *)	{}
inline void LogTaskState(netLoggingInterface &, s32 , s32 , const char *)	{}
#else
void LogGlobalFlags(netLoggingInterface &log, u32 globalFlags, const char *name);
void LogPedPopType(netLoggingInterface &log, u32 popType, const char *name);
void LogObjectCreatedBy(netLoggingInterface &log, u32 ownedBy, const char *name);
void LogVehiclePopType(netLoggingInterface &log, u32 popType, const char *name);
void LogMoveBlendState(netLoggingInterface &log, u16 moveBlendType, u32 moveBlendState, const char *name);
void LogMotionGroup(netLoggingInterface &log, fwMvClipSetId motionGroupId, const char *name);
void LogRelationshipGroup(netLoggingInterface &log, u32 relGroup, const char *name);
void LogDecisionMaker(netLoggingInterface &log, u32 decisionMaker, const char *name);
void LogWeaponType(netLoggingInterface &log, u32 weaponType, const char *name);
void LogAmmoType(netLoggingInterface &log, u32 ammoType, const char *name);
void LogArrestState(netLoggingInterface &log, u32 arrestState, const char *name);
void LogDeathState(netLoggingInterface &log, u32 deathState, const char *name);
void LogPlayerState(netLoggingInterface &log, u32 playerState, const char *name);
void LogVehicleEnterExitFlags(netLoggingInterface &log, const VehicleEnterExitFlags &iRunningFlags, const char *name);
void LogCombatBehaviorFlags(netLoggingInterface &log, const CombatBehaviorFlags &combatBehaviorFlags, const char *name);
void LogDoorSystemState(netLoggingInterface &log, const u32 state, const char *name);
void LogMigrationType(netLoggingInterface &log, const eMigrationType migrationType, const char *name);
void LogIncidentType(netLoggingInterface &log, const u32 type, const char *name);
void LogTaskState(netLoggingInterface &log, s32 taskType, s32 taskState, const char *name);
#endif // __FINAL

#endif // NETWORK_SERIALISERS_H
