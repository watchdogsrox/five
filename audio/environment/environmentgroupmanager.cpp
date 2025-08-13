// 
// audio/environment/environmentgroupmanager.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

// Game
#include "audio/environment/environmentgroupmanager.h"
#include "audio/northaudioengine.h"

// Rage
#include "audioengine/engine.h"
#include "profile/profiler.h"

f32 g_EnvGroupUpdateTimeLOD = 2000.0f;

#if __BANK
s32 g_DebugSelectedEnvironmentGroupIdx = -1;
bool g_DebugSelectedEnvironmentGroup = false;	// Allows you to select groups with the mouse, and will display enabled info for this group only
bool g_DebugAllEnvironmentGroups = false;
bool g_UpdateDebugEnvGroupEveryFrame = true;
#endif

PF_PAGE(environmentGroupManagerPage, "naEnvironmentGroupManager");
PF_GROUP(environmentGroupManager);
PF_LINK(environmentGroupManagerPage, environmentGroupManager);

PF_TIMER(UpdateEnvironmentGroups, environmentGroupManager);
PF_TIMER(UpdateEnvironmentGroup, environmentGroupManager);
PF_COUNTER(numGroupsActive, environmentGroupManager);
PF_COUNTER(numGroupsUpdated, environmentGroupManager);
PF_COUNTER(freeUpdates, environmentGroupManager);

AUDIO_ENVIRONMENT_OPTIMISATIONS()

void naEnvironmentGroupManager::Init()
{
	m_groupToUpdate = audNorthAudioEngine::GetAudioController() ? audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex() : 0xffff;
	m_DistToUpdateTimeScalarCurve.Init(ATSTRINGHASH("UPDATE_TIME_SCALAR_DIST", 0xAA8110BC));
	m_SpeedToUpdateTimeScalarCurve.Init(ATSTRINGHASH("UPDATE_TIME_SCALAR_SPEED", 0x620C243B));
}

void naEnvironmentGroupManager::Shutdown()
{
	m_groupToUpdate = 0xffff;
}

void naEnvironmentGroupManager::UpdateEnvironmentGroups(audEnvironmentGroupInterface** envGroupList, u32 maxGroups, u32 timeInMs)
{
#if __BANK
	audNorthAudioEngine::GetGtaEnvironment()->ResetPathServerRequestsCounter();

	if(g_DebugSelectedEnvironmentGroup && g_AudioEngine.GetEnvironment().AreSourceMetricsEnabled())
	{
		UpdateDebugSelectedEnvironmentGroup(envGroupList);
	}
#endif

	PF_FUNC(UpdateEnvironmentGroups);

	if(g_AudioEngine.GetEnvironment().AreSourceMetricsEnabled())
	{
		atFixedArray<u32, MAX_ENVIRONMENT_GROUPS> updatedGroups;
		s32 numGroupsToUpdate = (s32)maxGroups;

		// Update the player's or player car's envGroup each frame, so we always have the correct interior info
		// so we don't end up with wonky stuff when driving into tunnels and repositioning the sound.
		naEnvironmentGroup* playerEnvGroup = FindPlayerEnvironmentGroup();
		if(playerEnvGroup)
		{
			playerEnvGroup->SetShouldForceUpdate(true);
		}

		// Update any groups that we're forcing an update on (new groups, player group, interiorLoc changes).
		// Forced groups must be updated even if it goes beyond our max groups to update, 
		// otherwise we get jumps in frequency as they are smoothed down from the non-occluded value
		// to the actual environment value in subsequent frames (not because of the smoother in the environmentgroup,
		// but because of the m_FrequencySmoother on the environment sound).
		UpdateEnvironmentGroupsForced(timeInMs, playerEnvGroup, envGroupList, updatedGroups, numGroupsToUpdate);
	
		// Update the existing groups in order of priority, so closer and faster groups will be updated more often
		UpdateEnvironmentGroupsPriority(timeInMs, playerEnvGroup, envGroupList, updatedGroups, numGroupsToUpdate);
		
		// If we're not busy and still have numGroupsToUpdate, then just update the group regardless of it's distance
		// Update all envGroups that are linked as that's basically free
		UpdateEnvironmentGroupsNormal(timeInMs, envGroupList, updatedGroups, numGroupsToUpdate);
	}				

	m_DeletedEnvironmentGroups.Reset();

	// Kill the array's to get back the memory.
	audNorthAudioEngine::GetOcclusionManager()->FinishEnvironmentGroupListUpdate();
}

naEnvironmentGroup* naEnvironmentGroupManager::FindPlayerEnvironmentGroup() const
{
	naEnvironmentGroup* playerEnvGroup = NULL;
	const CPed* player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		playerEnvGroup = (naEnvironmentGroup*)player->GetAudioEnvironmentGroup(true);
		if(playerEnvGroup 
			&& playerEnvGroup->GetLinkedEnvironmentGroup() 
			&& !IsEnvironmentGroupInDeletedList(playerEnvGroup->GetLinkedEnvironmentGroup()))
		{
			playerEnvGroup = playerEnvGroup->GetLinkedEnvironmentGroup();
		}
	}
	return playerEnvGroup;
}

void naEnvironmentGroupManager::UpdateEnvironmentGroupsForced(const u32 timeInMs, const naEnvironmentGroup* playerEnvGroup, audEnvironmentGroupInterface** envGroupList, 
																	atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate)
{
	u32 index = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();
	while(index != 0xffff)
	{
		naEnvironmentGroup* envGroup = (naEnvironmentGroup*)envGroupList[index];
		if(envGroup)
		{
			PF_INCREMENT(numGroupsActive);

			// Need to make sure the linked group wasn't deleted between where it was set and the controller update
			if(envGroup->GetLinkedEnvironmentGroup() && IsEnvironmentGroupInDeletedList(envGroup->GetLinkedEnvironmentGroup()))
			{
				envGroup->SetLinkedEnvironmentGroup(NULL);
			}

#if __BANK
			envGroup->DrawDebug();
#endif

			// Also update any linked groups here as that's basically free
			if(envGroup->GetShouldForceUpdate() && !envGroup->GetLinkedEnvironmentGroup())
			{
				bool usedFreeUpdate = UpdateEnvironmentGroup(envGroupList, envGroup, updatedGroups, index, timeInMs, envGroup == playerEnvGroup);
				if(!usedFreeUpdate)
				{
					numGroupsToUpdate--;
				}
			}	
		}

		index = envGroupList[index]->GetNextIndex();
	}
}

void naEnvironmentGroupManager::UpdateEnvironmentGroupsPriority(const u32 timeInMs, const naEnvironmentGroup* playerEnvGroup, audEnvironmentGroupInterface** envGroupList, 
																atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate)
{
	if( m_groupToUpdate == 0xffff || !envGroupList[m_groupToUpdate])
	{
		m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();
	}

	u32 startIndex = m_groupToUpdate;

	const f32 listSpeed = g_AudioEngine.GetEnvironment().GetActualListenerSpeed();
	f32 listUpdateScalar = m_SpeedToUpdateTimeScalarCurve.CalculateValue(listSpeed);
	listUpdateScalar = Clamp(listUpdateScalar, 0.0f, 1.0f);

	while(numGroupsToUpdate > 0 && m_groupToUpdate != 0xffff)
	{
		naEnvironmentGroup* envGroup = (naEnvironmentGroup*)envGroupList[m_groupToUpdate];
		if(envGroup && !envGroup->GetLinkedEnvironmentGroup() && envGroup->GetLastUpdateTime() < timeInMs)
		{
			const u32 updateWaitTime = ComputeUpdateWaitTime(timeInMs, listUpdateScalar, envGroup);
			if(envGroup->GetLastUpdateTime() + updateWaitTime <= timeInMs)
			{
				bool usedFreeUpdate = UpdateEnvironmentGroup(envGroupList, envGroup, updatedGroups, m_groupToUpdate, timeInMs, envGroup == playerEnvGroup);
				if(!usedFreeUpdate)
				{
					numGroupsToUpdate--;
				}
			}
		}

		m_groupToUpdate = envGroupList[m_groupToUpdate]->GetNextIndex();

		if(m_groupToUpdate == 0xffff)
			m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();

		// Check to see if we've gone through the entire list, in which case we're done
		if(m_groupToUpdate == startIndex)
		{
			break;
		}
	}
}

void naEnvironmentGroupManager::UpdateEnvironmentGroupsNormal(const u32 timeInMs, audEnvironmentGroupInterface** envGroupList, 
																atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, s32& numGroupsToUpdate)
{
	if( m_groupToUpdate == 0xffff || !envGroupList[m_groupToUpdate])
	{
		m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();
	}

	u32 startIndex = m_groupToUpdate;
	while(m_groupToUpdate != 0xffff)
	{
		naEnvironmentGroup* envGroup = (naEnvironmentGroup*)envGroupList[m_groupToUpdate];
		if(envGroup
			&& (numGroupsToUpdate > 0 || envGroup->GetLinkedEnvironmentGroup()) 
			&& envGroup->GetLastUpdateTime() < timeInMs)
		{
			bool usedFreeUpdate = UpdateEnvironmentGroup(envGroupList, envGroup, updatedGroups, m_groupToUpdate, timeInMs, false);
			if(!usedFreeUpdate)
			{
				numGroupsToUpdate--;
			}
		}

		m_groupToUpdate = envGroupList[m_groupToUpdate]->GetNextIndex();

		if(m_groupToUpdate == 0xffff)
			m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();

		// Check to see if we've gone through the entire list, in which case we're done
		if(m_groupToUpdate == startIndex)
		{
			break;
		}
	}
}

bool naEnvironmentGroupManager::UpdateEnvironmentGroup(audEnvironmentGroupInterface** envGroupList, naEnvironmentGroup* envGroup, 
														atFixedArray<u32, MAX_ENVIRONMENT_GROUPS>& updatedGroups, u32 index, u32 timeInMs, const bool isPlayerGroup)
{
	PF_INCREMENT(numGroupsUpdated);
	PF_FUNC(UpdateEnvironmentGroup);

	bool usedFreeUpdate = false;

	// For small things like collisions where you get a bunch right next to each other, 
	// we want to share them because 2m isn't going to make much difference at all for our metrics
	// So go through the groups we've updated so far and see if there's one that we can use (assuming we're not already linked to another group)
	if(envGroup->GetUseCloseProximityGroupsForUpdates() && !envGroup->GetLinkedEnvironmentGroup())
	{
		for(s32 j = 0; j < updatedGroups.GetCount(); j++)
		{
			u32 updatedGroupIdx = updatedGroups[j];
			naEnvironmentGroup* updatedGroup = (naEnvironmentGroup*)envGroupList[updatedGroupIdx];
			if(envGroup->ShouldCloseProximityGroupBeUsedForUpdate(updatedGroup, timeInMs))
			{
				envGroup->SetLinkedEnvironmentGroup(updatedGroup);
				break;
			}
		}
	}

	envGroup->UpdateSourceEnvironment(timeInMs);
	envGroup->UpdateOcclusion(timeInMs, isPlayerGroup);
	envGroup->SetLastUpdateTime(timeInMs);
	envGroup->UpdateMixGroup();

	if(envGroup->GetLinkedEnvironmentGroup())
	{
		PF_INCREMENT(freeUpdates);
		usedFreeUpdate = true;
	}

	// We can't save linked groups between frames because the linked group may have been deleted
	envGroup->SetLinkedEnvironmentGroup(NULL);

	// We've updated the group, so unset the force update bool
	envGroup->SetShouldForceUpdate(false);

	// Cache our position so that we can use the velocity to determine priority
	envGroup->CachePositionLastUpdate();

	if(updatedGroups.GetAvailable())
	{
		updatedGroups.Push(index);
	}

	return usedFreeUpdate;
}

u32 naEnvironmentGroupManager::ComputeUpdateWaitTime(const u32 timeInMs, const f32 listUpdateScalar, const naEnvironmentGroup* envGroup)
{
	f32 updateTime = 0.0f;

	// Check to see if we can do a very slow update time if it's not playing any sounds,
	if(envGroup->GetNumberOfSoundReference() == 0)
	{
		updateTime = g_EnvGroupUpdateTimeLOD;

#if __BANK
		if(envGroup->GetIsDebugEnvironmentGroup() && g_DebugSelectedEnvironmentGroup)
		{
			char strDebug[255];
			formatf(strDebug, "No sounds, using LOD update - UpdateTime: %f", updateTime);
			grcDebugDraw::Text(envGroup->GetPosition(), Color_white, 0, envGroup->GetDebugTextPosY(), strDebug);
		}
#endif
	}
	else
	{
		updateTime = static_cast<f32>(envGroup->GetMaxUpdateTime());

		// Scale it down by distance and velocity
		const f32 dist = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition() - VECTOR3_TO_VEC3V(envGroup->GetPosition())).Getf();
		f32 distUpdateScalar = m_DistToUpdateTimeScalarCurve.CalculateValue(dist);
		distUpdateScalar = Clamp(distUpdateScalar, 0.0f, 1.0f);

		// Environment group velocity
		Vector3 envGroupVel = envGroup->GetPosition() - envGroup->GetPositionLastFrame();
		const u32 timeStep = timeInMs - envGroup->GetLastUpdateTime();
		timeStep != 0 ? envGroupVel.Scale(30.0f/((f32)timeStep)) : envGroupVel.Zero();
		const f32 envGroupSpeed = envGroupVel.Mag() * TIME.GetInvSeconds();
		f32 envGroupUpdateScalar = m_SpeedToUpdateTimeScalarCurve.CalculateValue(envGroupSpeed);
		envGroupUpdateScalar = Clamp(envGroupUpdateScalar, 0.0f, 1.0f);

		updateTime *= Lerp(envGroupUpdateScalar * listUpdateScalar, distUpdateScalar, 1.0f);

#if __BANK
		if(envGroup->GetIsDebugEnvironmentGroup() && g_DebugSelectedEnvironmentGroup)
		{
			char strDebug[255];
			formatf(strDebug, "Dist: %f DistWaitTimeScalar: %f", dist, distUpdateScalar);
			grcDebugDraw::Text(envGroup->GetPosition(), Color_white, 0, envGroup->GetDebugTextPosY(), strDebug);
			formatf(strDebug, "envGroupSpeed: %f envGroupUpdateScalar: %f", envGroupSpeed, envGroupUpdateScalar);
			grcDebugDraw::Text(envGroup->GetPosition(), Color_white, 0, envGroup->GetDebugTextPosY(), strDebug);
			formatf(strDebug, "listSpeed: %f listUpdateScalar: %f", g_AudioEngine.GetEnvironment().GetActualListenerSpeed(), listUpdateScalar);
			grcDebugDraw::Text(envGroup->GetPosition(), Color_white, 0, envGroup->GetDebugTextPosY(), strDebug);
			formatf(strDebug, "updateTime: %f", updateTime);
			grcDebugDraw::Text(envGroup->GetPosition(), Color_white, 0, envGroup->GetDebugTextPosY(), strDebug);
		}
#endif
	}

	naAssertf(updateTime >= 0.0f, "Invalid update time for naEnvironmentGroup: %f", updateTime);
	return static_cast<u32>(updateTime);
}

void naEnvironmentGroupManager::RegisterGameObjectMetadataUnloading(audEnvironmentGroupInterface** envGroupList, const audMetadataChunk& chunk)
{
	if( m_groupToUpdate == 0xffff || !envGroupList[m_groupToUpdate])
	{
		m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();
	}

	u32 startIndex = m_groupToUpdate;
	while(m_groupToUpdate != 0xffff)
	{
		naEnvironmentGroup* envGroup = (naEnvironmentGroup*)envGroupList[m_groupToUpdate];
		if(envGroup && (chunk.IsObjectInChunk(envGroup->GetInteriorSettings()) || chunk.IsObjectInChunk(envGroup->GetInteriorRoom())))
		{
			envGroup->ClearInteriorInfo();
		}

		m_groupToUpdate = envGroupList[m_groupToUpdate]->GetNextIndex();

		if(m_groupToUpdate == 0xffff)
			m_groupToUpdate = audNorthAudioEngine::GetAudioController()->GetEnvironmentGroupHeadIndex();

		// Check to see if we've gone through the entire list, in which case we're done
		if(m_groupToUpdate == startIndex)
		{
			break;
		}
	}
}

#if __BANK
void naEnvironmentGroupManager::UpdateDebugSelectedEnvironmentGroup(audEnvironmentGroupInterface** environmentGroupListPtr)
{
	bool checkMouseClickOnSelectEntity = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT);
	if(checkMouseClickOnSelectEntity)
	{
		Vector3 pos;
		Vector3 normal;
		void* pEntity;
		bool bHasPosition = CDebugScene::GetWorldPositionUnderMouse(pos, ArchetypeFlags::GTA_ALL_MAP_TYPES, &normal, &pEntity);
		if(bHasPosition)
		{	
			f32 closestEnvironmentGroupDist2 = LARGE_FLOAT;

			for(s32 i = 0; i < MAX_ENVIRONMENT_GROUPS; i++)
			{
				if(environmentGroupListPtr[i])
				{
					f32 currentEnvironmentGroupDist2 = (pos - ((naEnvironmentGroup*)environmentGroupListPtr[i])->GetPosition()).Mag2();
					if(currentEnvironmentGroupDist2 < closestEnvironmentGroupDist2)
					{
						closestEnvironmentGroupDist2 = currentEnvironmentGroupDist2;
						g_DebugSelectedEnvironmentGroupIdx = i;
					}
				}
			}
		}
	}

	s32 mouseWheel = ioMouse::GetDZ();
	if(mouseWheel)
	{
		g_DebugSelectedEnvironmentGroupIdx += mouseWheel;
		g_DebugSelectedEnvironmentGroupIdx = Clamp(g_DebugSelectedEnvironmentGroupIdx, 0, MAX_ENVIRONMENT_GROUPS);
	}

	for(s32 i = 0; i < MAX_ENVIRONMENT_GROUPS; i++)
	{
		if(environmentGroupListPtr[i])
		{
			if(i == g_DebugSelectedEnvironmentGroupIdx)
			{
				naEnvironmentGroup* environmentGroup = (naEnvironmentGroup*)environmentGroupListPtr[i];
				if(environmentGroup)
				{
					environmentGroup->SetAsDebugEnvironmentGroup(true);
					environmentGroup->SetShouldForceUpdate(g_UpdateDebugEnvGroupEveryFrame);

					grcDebugDraw::Sphere(environmentGroup->GetPosition(), 0.5f, Color_orange, true);

					char strDebug[256] = "";
					const CInteriorInst* intInst = environmentGroup->GetInteriorInst();
					if(intInst)
					{
						formatf(strDebug, "EnvironmentGroup Idx: %d Interior: %s Room: %s", g_DebugSelectedEnvironmentGroupIdx,
							intInst->GetBaseModelInfo()->GetModelName(), 
							intInst->GetRoomName(environmentGroup->GetRoomIdx()));
					}
					else
					{
						formatf(strDebug, "EnvironmentGroup Idx: %d Interior: Outside", g_DebugSelectedEnvironmentGroupIdx);
					}

					grcDebugDraw::Text(environmentGroup->GetPosition(), Color_white, 0, environmentGroup->GetDebugTextPosY(), strDebug);
				}
			}
			else
			{
				((naEnvironmentGroup*)environmentGroupListPtr[i])->SetAsDebugEnvironmentGroup(false);
			}
		}
	}
}

void naEnvironmentGroupManager::AddWidgets(bkBank& bank)
{
	bank.AddToggle("DebugSelectedEnvironmentGroup", &g_DebugSelectedEnvironmentGroup);
	bank.AddToggle("DebugAllEnvironmentGroups", &g_DebugAllEnvironmentGroups);
	bank.AddToggle("UpdateDebugEnvGroupEveryFrame", &g_UpdateDebugEnvGroupEveryFrame);
	bank.AddSlider("EnvGroupUpdateTimeLOD", &g_EnvGroupUpdateTimeLOD, 0.0f, 10000.0f, 1.0f);
}
#endif

