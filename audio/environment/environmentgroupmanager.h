//
// audio/environment/environmentgroupmanager.h
//
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//

#ifndef AUD_NAENVIRONMENTGROUPMANAGER_H
#define AUD_NAENVIRONMENTGROUPMANAGER_H

// Rage
#include "audioengine/controller.h"
#include "audioengine/environmentgroup.h"

class naEnvironmentGroup;

class naEnvironmentGroupManager : public audEnvironmentGroupManager
{

public:

	naEnvironmentGroupManager(){};
	~naEnvironmentGroupManager(){};

	void Init();
	void Shutdown();

	// Called from the audio controller, traverses the environmentgroup list and updates X amounts of groups
	void UpdateEnvironmentGroups(audEnvironmentGroupInterface** environmentGroupList, u32 maxGroups, u32 timeInMs);

	// Returns true if it was able to use an environment group within close proximity or a linked environment group
	// which is essentially a "free" update so we don't count that against our max to update per frame.
	bool UpdateEnvironmentGroup(audEnvironmentGroupInterface** environmentGroupList, naEnvironmentGroup* environmentGroup, 
								atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, u32 index, u32 timeInMs, const bool isPlayerGroup);

	// Clear out the environmentGroups InteriorSettings and InteriorRoom*'s referencing the chunk as they're cached and not updated each frame.
	virtual void RegisterGameObjectMetadataUnloading(audEnvironmentGroupInterface** environmentGroupList, const audMetadataChunk& chunk);

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif

private:

	naEnvironmentGroup* FindPlayerEnvironmentGroup() const;

	void UpdateEnvironmentGroupsForced(const u32 timeInMs, const naEnvironmentGroup* playerEnvGroup, audEnvironmentGroupInterface** envGroupList, 
										atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate);
	void UpdateEnvironmentGroupsPriority(const u32 timeInMs, const naEnvironmentGroup* playerEnvGroup, audEnvironmentGroupInterface** envGroupList, 
										atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate);
	void UpdateEnvironmentGroupsNormal(const u32 timeInMs, audEnvironmentGroupInterface** envGroupList, 
										atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate);

	u32 ComputeUpdateWaitTime(const u32 timeInMs, const f32 listUpdateScalar, const naEnvironmentGroup* envGroup);

#if __BANK
	void UpdateDebugSelectedEnvironmentGroup(audEnvironmentGroupInterface** environmentGroupListPtr);
#endif

private:

	audCurve m_DistToUpdateTimeScalarCurve;
	audCurve m_SpeedToUpdateTimeScalarCurve;
	u32 m_groupToUpdate;

};

#endif // AUD_NAENVIRONMENTGROUPMANAGER_H
