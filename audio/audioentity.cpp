// 
// audio/audioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audioentity.h"
#include "northaudioengine.h"
#include "audio/environment/environmentgroup.h"
#include "audioengine/engine.h"
#include "control/replay/replay.h"
#include "control/replay/ReplaySettings.h"
#include "control/replay/Audio/SoundPacket.h"
#include "system/bit.h"

AUDIO_OPTIMISATIONS()

atFixedArray<naDeferredSound, naAudioEntity::sm_MaxNumDeferredSounds> naAudioEntity::sm_DeferredSoundList;

u16 naAudioEntity::sm_BaseAnimLoopTimeout = 100;
sysCriticalSectionToken naAudioEntity::sm_EnvGroupCritSec;
namespace rage
{
	extern bool g_WarnOnMissingSounds;
}

naAudioEntity::naAudioEntity() : m_EnvironmentGroup(NULL), m_GroupRequests(0)
{
	
}

naAudioEntity::~naAudioEntity()
{
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Deleted an audio entity from the main thread while the audio update thread is running.");
}

void naAudioEntity::Init()
{
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Initialised an audio entity from the main thread while the audio update thread is running.");

	fwAudioEntity::Init();	
	m_AnimLoopTimeout = sm_BaseAnimLoopTimeout;
}

void naAudioEntity::Shutdown()
{
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Shutting down an audio entity from the main thread while the audio update thread is running.");
	fwAudioEntity::Shutdown();
}
void naAudioEntity::InitClass()
{
	sm_DeferredSoundList.Reset();
}

void naAudioEntity::CreateEnvironmentGroup(const char* debugIdentifier,u32 type )
{
	if(!m_EnvironmentGroup)
	{
		m_EnvironmentGroup = naEnvironmentGroup::Allocate(debugIdentifier);
	}
	AddEnvironmentGroupRequest(type);
}

void naAudioEntity::RemoveEnvironmentGroup(u32 type)
{
	RemoveEnvironmentGroupRequest(type);
	if(m_EnvironmentGroup)
	{
		if(!m_GroupRequests || type==naAudioEntity::SHUTDOWN)
		{
			m_EnvironmentGroup->SetEntity(NULL);
			m_EnvironmentGroup = NULL;
			m_GroupRequests = 0;
		}
	}
}

naEnvironmentGroup* naAudioEntity::GetEnvironmentGroup(bool UNUSED_PARAM(create)) const
{ 
	return m_EnvironmentGroup;
}

naEnvironmentGroup* naAudioEntity::GetEnvironmentGroup(bool UNUSED_PARAM(create))
{ 
	return m_EnvironmentGroup; 
}

void naAudioEntity::AddEnvironmentGroupRequest(u32 type)
{
	SYS_CS_SYNC(sm_EnvGroupCritSec);
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Can't add env group request from the main thread while the audio update thread is running.");
	if(m_EnvironmentGroup && (m_GroupRequests & type) == 0)
	{
		m_EnvironmentGroup->AddSoundReference();
	}
	m_GroupRequests |= type;
}

void naAudioEntity::RemoveEnvironmentGroupRequest(u32 type)
{
	SYS_CS_SYNC(sm_EnvGroupCritSec);
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Can't remove env group request from the main thread while the audio update thread is running.");
	if(m_EnvironmentGroup)
	{
		if(type == naAudioEntity::SHUTDOWN)
		{
			// Need to remove all request refs
			const u32 numRefs = CountOnBits(m_GroupRequests);
			for(u32 i = 0; i < numRefs; i++)
			{
				m_EnvironmentGroup->RemoveSoundReference();
			}
		}
		else if(m_GroupRequests & type)
		{
			m_EnvironmentGroup->RemoveSoundReference();
		}
	}
	m_GroupRequests &= ~type;
}

void naAudioEntity::ProcessAnimEvents()
{
	for(int i=0; i< AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(m_AnimEvents[i])
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.AllowLoad = false;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(m_AnimEvents[i], &initParams);
			m_AnimEvents[i] = 0;

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_AnimEvents[i], &initParams, GetOwningEntity()));
		}
	}


	if(m_AnimLoopWasStarted && !m_AnimLoopWasStopped)
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.AllowLoad = false;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound_Persistent(m_AnimLoopHash, &m_AnimLoop, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_AnimLoopHash, &initParams, m_AnimLoop, GetOwningEntity(), eNoGlobalSoundEntity);)
	}
	else if(m_AnimLoop && (m_AnimLoopWasStopped || g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > (m_AnimLoopTriggerTime + m_AnimLoopTimeout))) 
	{
		m_AnimLoop->StopAndForget();
		m_AnimLoopHash = 0;
	}

	m_AnimLoopWasStarted = false;
	m_AnimLoopWasStopped = false;
}

void naAudioEntity::HandleLoopingAnimEvent(const audAnimEvent & event)
{
	if(event.isStart)
	{
		if(!m_AnimLoop || event.hash == m_AnimLoopHash)
		{
			if(!m_AnimLoop)
			{
				m_AnimLoopWasStarted = true;
				m_AnimLoopHash = event.hash;
			}
			m_AnimLoopTriggerTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		}
		else
		{
			BANK_ONLY(audWarningf("Trying to start anim looping event with sound %s but we're already playing sound %s. Only one allowed at a time", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(event.hash), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_AnimLoopHash));)
		}
	}
	else
	{
		if(m_AnimLoop && naVerifyf(event.hash == m_AnimLoopHash, "Trying to stop anim looping event with sound %s but we're playing sound %s. Only one allowed at a time", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(event.hash), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_AnimLoopHash)))
		{
			m_AnimLoopWasStopped = true;
		}
	}
}

bool naAudioEntity::CreateDeferredSound(const char *soundName, const CEntity* entity, const audSoundInitParams *initParams /* = NULL */,  
										const bool useEnvGroup /* = false */, const bool useTracker /* = false */, const bool invalidateEntity /* = false */, const bool persistAcrossClears /* = false */)
{
	const bool success = CreateDeferredSound(atStringHash(soundName), entity, initParams, useEnvGroup, useTracker, invalidateEntity, persistAcrossClears);
#if __BANK
	if(g_WarnOnMissingSounds && !success)
	{
		audWarningf("Missing sound %s", soundName);
	}
#endif
	return success;
}

bool naAudioEntity::CreateDeferredSound(const u32 soundHash, const CEntity* entity, const audSoundInitParams *initParams  /* = NULL */,
										const bool useEnvGroup /* = false */, const bool useTracker /* = false */, const bool invalidateEntity /* = false */, const bool persistAcrossClears /* = false */)
{
	return CreateDeferredSound(SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(soundHash), entity, initParams, useEnvGroup, useTracker, invalidateEntity, persistAcrossClears);
}

bool naAudioEntity::CreateDeferredSound(const audMetadataRef metadataRef, const CEntity* entity, const audSoundInitParams *initParams /* = NULL */,
										const bool useEnvGroup /* = false */, const bool useTracker /* = false */, const bool invalidateEntity /* = false */, const bool persistAcrossClears /* = false */)
{
#if __ASSERT
	if(initParams)
	{
		Assert(initParams->VolumeCurveScale != 0.f);
		Assertf(initParams->Volume >= g_SilenceVolume*3.f, "Invalid requested volume: %f", initParams->Volume);
		Assertf(initParams->Volume <= 24.f,"Invalid requested volume: %f", initParams->Volume);
		Assertf(!initParams->EnvironmentGroup, "Cannot set environmentGroup on a deferred sound, it will be created from the CEntity");
		Assertf(!initParams->Tracker, "Cannot set tracker on a deferred sound, it will be created from the CEntity");
		if(useEnvGroup)
		{
			Assertf(entity, "Can't create an environmentGroup in a deferred sound without a CEntity");
		}
		if(useTracker)
		{
			Assertf(entity, "Can't create an audTracker in a deferred sound without a CEntity");
		}
	}
#endif

	if(metadataRef != g_NullSoundRef && metadataRef.IsValid() && 
		naVerifyf(!sm_DeferredSoundList.IsFull(), "Creating more than %d deferred sounds in 1 frame - sound will not be played", sm_MaxNumDeferredSounds))
	{
#if __BANK	
		audDisplayf("Triggering deferred sound %s (%u) from entity %s", SOUNDFACTORY.GetMetadataManager().GetObjectName(metadataRef), metadataRef.Get(), entity ? entity->GetModelName() : "(null)");
#endif

		naDeferredSound& deferredSound = sm_DeferredSoundList.Append();
		deferredSound.metadataRef = metadataRef;
		deferredSound.audioEntity = this;
		deferredSound.entity = entity;
		deferredSound.useEnvGroup = useEnvGroup;
		deferredSound.useTracker = useTracker;
		deferredSound.invalidateEntity = invalidateEntity;
		deferredSound.persistAcrossClears = persistAcrossClears;
		if(initParams)
		{
			deferredSound.initParams = *initParams;
		}
		else
		{
			audSoundInitParams defaultInitParams;
			deferredSound.initParams = defaultInitParams;
		}
		return true;
	}
	return false;
}

void naAudioEntity::ProcessDeferredSounds()
{
	const s32 timeStepMs = static_cast<s32>(fwTimer::GetTimeStepInMilliseconds());
	
	for(s32 i = sm_DeferredSoundList.GetCount() - 1; i >= 0; i--)
	{
		const bool needCEntity = sm_DeferredSoundList[i].useEnvGroup || sm_DeferredSoundList[i].useTracker;
		const bool isAudEntityDeleted = sm_DeferredSoundList[i].audioEntity == NULL || sm_DeletedEntities.Find(sm_DeferredSoundList[i].audioEntity) != -1;

#if __ASSERT
		if(isAudEntityDeleted && needCEntity && sm_DeferredSoundList[i].entity)
		{
			bool entityPendingDeletion = sm_DeferredSoundList[i].entity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD);

			if(sm_DeferredSoundList[i].entity->GetIsTypeVehicle())
			{
				const CVehicle* vehicle = static_cast<const CVehicle*>(sm_DeferredSoundList[i].entity.Get());
				entityPendingDeletion |= vehicle->GetIsInReusePool();
			}
						
			naAssertf(!entityPendingDeletion, "audEntity has been deleted or is in deleted list, but CEntity: %s has not in ProcessDeferredSounds.  Sound metadataRef: %s",
				sm_DeferredSoundList[i].entity ? sm_DeferredSoundList[i].entity->GetModelName() : "NULL entity", 
				audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_DeferredSoundList[i].metadataRef));
		}
#endif

		if(!isAudEntityDeleted && (!needCEntity || (needCEntity && sm_DeferredSoundList[i].entity)))
		{
			// Evaluate the first part of a long predelay at the game level, before we create any sounds
			if(sm_DeferredSoundList[i].initParams.Predelay > 100)
			{
				sm_DeferredSoundList[i].initParams.Predelay -= Min(timeStepMs, sm_DeferredSoundList[i].initParams.Predelay);
			}
			else
			{	
				if(sm_DeferredSoundList[i].useEnvGroup)
				{
					sm_DeferredSoundList[i].initParams.EnvironmentGroup = sm_DeferredSoundList[i].entity->GetAudioEnvironmentGroup();
				}
				if(sm_DeferredSoundList[i].useTracker)
				{
					sm_DeferredSoundList[i].initParams.Tracker = sm_DeferredSoundList[i].entity->GetPlaceableTracker();
				}

				audSound *sound;
				sm_DeferredSoundList[i].audioEntity->CreateSound_LocalReference(sm_DeferredSoundList[i].metadataRef, &sound, &sm_DeferredSoundList[i].initParams);
				if(sound)
				{	
					sound->PrepareAndPlay(const_cast<audWaveSlot*>(sm_DeferredSoundList[i].initParams.WaveSlot), 
						sm_DeferredSoundList[i].initParams.AllowLoad, 
						sm_DeferredSoundList[i].initParams.PrepareTimeLimit);
					if(sm_DeferredSoundList[i].invalidateEntity)
					{
						sound->InvalidateEntity();
					}

					if(sm_DeferredSoundList[i].persistAcrossClears)
					{
						sound->GetRequestedSettings()->SetShouldPersistOverClears(true);
					}
				}
				sm_DeferredSoundList.DeleteFast(i);
			}
		}
		else
		{
			sm_DeferredSoundList.DeleteFast(i);
		}
	}
}

#if GTA_REPLAY
void naAudioEntity::ClearDeferredSoundsForReplay()
{
	for(s32 i = sm_DeferredSoundList.GetCount() - 1; i >= 0; i--)
	{
		if(!sm_DeferredSoundList[i].persistAcrossClears)
		{
			sm_DeferredSoundList.Delete(i);
		}
	}
}
#endif //GTA_REPLAY
