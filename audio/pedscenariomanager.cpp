//  
// audio/pedscenariomanager.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
 
#include "pedscenariomanager.h"
#include "pedaudioentity.h"
#include "streamslot.h"
#include "northaudioengine.h"
#include "frontendaudioentity.h"

audPedScenarioManager g_PedScenarioManager;
u32 audPedScenario::sm_PedScenarioUniqueID = 1;
 
AUDIO_PEDS_OPTIMISATIONS()

// ----------------------------------------------------------------
// PedScenario Constructor
// ----------------------------------------------------------------
audPedScenario::audPedScenario()
{
	Reset();
}

// ----------------------------------------------------------------
// PedScenario Reset
// ----------------------------------------------------------------
void audPedScenario::Reset()
{
	StopStream();
	m_NameHash = 0;
	m_PropHash = 0;
	m_UniqueID = 0;
	m_ScenarioSettings = NULL;
	m_Sound = NULL;
	m_StreamSlot = NULL;
	m_SlotIndex = 0;
	m_SoundHash = g_NullSoundHash;
	m_HasPlayed = false;
	m_Center = Vec3V(V_ZERO);	
}

// ----------------------------------------------------------------
// PedScenario Init
// ----------------------------------------------------------------
void audPedScenario::Init(u32 name, u32 prop, audPedAudioEntity* ped, PedScenarioAudioSettings* settings)
{
	m_ScenarioSettings = settings;

	if(m_ScenarioSettings)
	{
		m_NameHash = name;
		m_PropHash = prop;
		m_UniqueID = ++sm_PedScenarioUniqueID;
		m_SoundHash = m_ScenarioSettings->Sound;
		m_Center = ped->GetPosition();

		// Check for any props overriding the default sound
		for(u32 loop = 0; loop < m_ScenarioSettings->numPropOverrides; loop++)
		{
			if(m_ScenarioSettings->PropOverride[loop].PropName == prop)
			{
				m_SoundHash = m_ScenarioSettings->PropOverride[loop].Sound;
				break;
			}
		}

		m_Peds.PushAndGrow(ped);
	}
}

// ----------------------------------------------------------------
// PedScenario HasStreamStopped
// ----------------------------------------------------------------
bool audPedScenario::HasStreamStopped() const
{
	bool hasStopped = true;
	if(m_StreamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = m_StreamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}

	return hasStopped;
}

// ----------------------------------------------------------------
// Remove a ped from the given scenario
// ----------------------------------------------------------------
void audPedScenario::RemovePed(audPedAudioEntity* ped)
{
	s32 index = m_Peds.Find(ped);
	audAssertf(index >= 0, "Failed to find ped in registered list!");
	
	if(index >= 0)
	{
		m_Peds.Delete(index);
	}
}

// ----------------------------------------------------------------
// Check if the given prop shares the same sound that is already playing
// ----------------------------------------------------------------
bool audPedScenario::DoesPropShareExistingSound(u32 prop) const
{
	if(m_ScenarioSettings)
	{
		u32 soundHashForProp = m_ScenarioSettings->Sound;

		// Check for any props overriding the default sound
		for(u32 loop = 0; loop < m_ScenarioSettings->numPropOverrides; loop++)
		{
			if(m_ScenarioSettings->PropOverride[loop].PropName == prop)
			{
				soundHashForProp = m_ScenarioSettings->PropOverride[loop].Sound;
				break;
			}
		}

		return soundHashForProp == m_SoundHash;
	}

	return false;
}

// ----------------------------------------------------------------
// PedScenario IsActive
// ----------------------------------------------------------------
bool audPedScenario::IsActive() const
{ 	
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && m_ScenarioSettings)
	{
		return true;
	}
#endif	

	return m_Peds.GetCount() > 0;	
}

// ----------------------------------------------------------------
// PedScenario StopStream
// ----------------------------------------------------------------
void audPedScenario::StopStream()
{
	if(m_Sound)
	{
		m_Sound->StopAndForget();
	}

	if(m_StreamSlot)
	{
		m_StreamSlot->Free();
		m_StreamSlot = NULL;
	}
}

#if __BANK
// ----------------------------------------------------------------
// PedScenario DebugDraw
// ----------------------------------------------------------------
void audPedScenario::DebugDraw()
{
	if(IsActive())
	{
		if(m_Sound)
		{
			grcDebugDraw::Sphere(m_Sound->GetRequestedPosition(), 0.4f, Color32(0, 255, 0, 255), true);

			for(u32 loop = 0; loop < m_Peds.GetCount(); loop++)
			{
				grcDebugDraw::Line(m_Sound->GetRequestedPosition(), m_Peds[loop]->GetPosition(), Color32(0, 0, 255, 255), Color32(0, 0, 255, 255));
			}
		}
	}
}
#endif

// ----------------------------------------------------------------
// Create an environment group
// ----------------------------------------------------------------
naEnvironmentGroup* audPedScenario::CreateEnvironmentGroup()
{
	naEnvironmentGroup* environmentGroup = NULL;
	environmentGroup = naEnvironmentGroup::Allocate("PedScenario");

	if(environmentGroup)
	{
		environmentGroup->Init(NULL, 20.0f, 2000);
		environmentGroup->SetUsePortalOcclusion(true);
		environmentGroup->SetSourceEnvironmentUpdates(1000);
	}

	return environmentGroup;
}

// ----------------------------------------------------------------
// PedScenario Update
// ----------------------------------------------------------------
void audPedScenario::Update()
{
	if(IsActive())
	{
		if(m_SoundHash == g_NullSoundHash)
		{
			// For scenarios that rely on props, its possible that we get notified of the scenario starting before the prop has streamed in, so we
			// might get this far without a valid sound. This code just delays the scenario audio until the ped is holding a prop object.
			if(m_Peds.GetCount() > 0)
			{
				CPedWeaponManager* weaponMgr = m_Peds[0]->GetOwningPed()->GetWeaponManager();

				if(weaponMgr)
				{
					CObject* weaponObject = weaponMgr->GetEquippedWeaponObject();

					if(weaponObject && weaponObject->GetArchetype())
					{
						u32 propHash = weaponObject->GetArchetype()->GetModelNameHash();
						//audDisplayf("Found ped with %s", weaponObject->GetArchetype()->GetModelName());

						for(u32 loop = 0; loop < m_ScenarioSettings->numPropOverrides; loop++)
						{
							if(m_ScenarioSettings->PropOverride[loop].PropName == propHash)
							{
								m_SoundHash = m_ScenarioSettings->PropOverride[loop].Sound;
								break;
							}
						}
					}
				}
			}			

			if(m_SoundHash == g_NullSoundHash)
			{
				return;
			}
		}

		// Only one ped - just position the sound at their location
		if(m_Peds.GetCount() == 1)
		{
			m_Center = m_Peds[0]->GetPosition();
		}
		else if(m_Peds.GetCount() > 1)
		{
			// Multiple peds, sound is positioned at central point
			audAssert(m_Peds.GetCount() > 1);
			Vec3V totalPosition = Vec3V(V_ZERO);

			for(u32 loop = 0; loop < m_Peds.GetCount(); loop++)
			{
				totalPosition += m_Peds[loop]->GetPosition();
			}

			// Smoothly interpolate the sound position to cope with the fact that new peds may have joined the scenario
			Vec3V averagePosition = totalPosition/ScalarV((f32)m_Peds.GetCount());
			const ScalarV filterFactor = ScalarV(0.05f);
			m_Center = (averagePosition * filterFactor) + (m_Center * (ScalarV(V_ONE) - filterFactor));
		}

		bool isStreaming = AUD_GET_TRISTATE_VALUE(m_ScenarioSettings->Flags, FLAG_ID_PEDSCENARIOAUDIOSETTINGS_ISSTREAMING) == AUD_TRISTATE_TRUE;

		if(isStreaming)
		{
			if(!m_StreamSlot)
			{
				audStreamClientSettings settings;
				settings.priority = audStreamSlot::STREAM_PRIORITY_STATIC_AMBIENT_EMITTER;
				settings.stopCallback = &audPedScenarioManager::OnScenarioStreamStopCallback;
				settings.hasStoppedCallback = &audPedScenarioManager::HasScenarioStreamStoppedCallback;
				settings.userData = m_SlotIndex;
				m_StreamSlot = audStreamSlot::AllocateSlot(&settings);
			}

			if(m_StreamSlot)
			{
				if(!m_Sound)
				{
					audSoundInitParams initParams;

					if(m_Peds.GetCount() == 1 && AUD_GET_TRISTATE_VALUE(m_ScenarioSettings->Flags, FLAG_ID_PEDSCENARIOAUDIOSETTINGS_ALLOWSHAREDOWNERSHIP) != AUD_TRISTATE_TRUE)
					{
						initParams.EnvironmentGroup = m_Peds[0]->GetEnvironmentGroup(true);
					}
					else
					{
						initParams.EnvironmentGroup = CreateEnvironmentGroup();
					}
					
					if(initParams.EnvironmentGroup)
					{
						Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
						g_FrontendAudioEntity.CreateSound_PersistentReference(m_SoundHash, &m_Sound, &initParams);
						m_HasPlayed = false;
					}					
				}

				if(m_Sound)
				{
					if(!m_HasPlayed)
					{
						if(m_Sound->Prepare(m_StreamSlot->GetWaveSlot(), true) == AUD_PREPARED)
						{
							m_Sound->Play();
							m_HasPlayed = true;
						}
					}
				}
			}
		}
		else if(!m_Sound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = CreateEnvironmentGroup();
			g_FrontendAudioEntity.CreateAndPlaySound_Persistent(m_SoundHash, &m_Sound, &initParams);
		}

		if(m_Sound)
		{
			// Multiple/no peds? We don't want any doppler if the average position changes
			if(m_Peds.GetCount() != 1)
			{
				m_Sound->SetRequestedDopplerFactor(0.0f);
			}
			
			m_Sound->SetRequestedPosition(m_Center);
		}
	}
	else
	{
		StopStream();
	}
}

// ----------------------------------------------------------------
// PedScenario Manager Constructor
// ----------------------------------------------------------------
audPedScenarioManager::audPedScenarioManager()
{
	for(u32 loop = 0; loop < kMaxActiveScenarios; loop++)
	{
		m_ActiveScenarios[loop].SetSlotIndex(loop);
	}
}

// ----------------------------------------------------------------
// Start a new scenario
// ----------------------------------------------------------------
audPedScenario* audPedScenarioManager::StartScenario(audPedAudioEntity* ped, u32 name, u32 prop)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditorActive())
	{
		return NULL;
	}
#endif

	s32 firstFreeIndex = -1;
	u32 numInstances = 0;

	// Peds can share scenarios with one another if scenario sharing is enabled in the PedScenarioAudioSettings gameobject. This means that 
	// if multiple peds spawn near to each other and are playing the same scenario, we only play one sound instead of multiple copies.
	for(u32 loop = 0; loop < kMaxActiveScenarios; loop++)
	{
		if(m_ActiveScenarios[loop].IsActive())
		{
			// Check if we are already playing a scenario with the same name, and if the given prop would be playing the same sound
			if(m_ActiveScenarios[loop].GetName() == name && m_ActiveScenarios[loop].DoesPropShareExistingSound(prop))
			{
				// Check the ped doesn't already exist in the scenario
				if(!m_ActiveScenarios[loop].ContainsPed(ped))
				{
					// Is scenario sharing allowed?
					if(AUD_GET_TRISTATE_VALUE(m_ActiveScenarios[loop].GetSettings()->Flags, FLAG_ID_PEDSCENARIOAUDIOSETTINGS_ALLOWSHAREDOWNERSHIP) == AUD_TRISTATE_TRUE)
					{
						f32 maxRadius = m_ActiveScenarios[loop].GetSettings()->SharedOwnershipRadius;

						// Sharing is enabled, but is the ped within the minimum radius to share the scenario?
						if(DistSquared(ped->GetPosition(), m_ActiveScenarios[loop].GetCenter()).Getf() < (maxRadius * maxRadius))
						{
							// All good - register the ped
							m_ActiveScenarios[loop].AddPed(ped);
							return &m_ActiveScenarios[loop];
						}
					}
				}
				else
				{
					return &m_ActiveScenarios[loop];
				}

				numInstances++;
			}
		}
		else if(firstFreeIndex == -1)
		{
			firstFreeIndex = loop;
		}
	}

	// Ped could not be added to an existing scenario - try to create a new one
	if(firstFreeIndex >= 0)
	{
		PedScenarioAudioSettings* settings = audNorthAudioEngine::GetObject<PedScenarioAudioSettings>(name);

		if(settings && numInstances < settings->MaxInstances)
		{
			m_ActiveScenarios[firstFreeIndex].Init(name, prop, ped, settings);
			return &m_ActiveScenarios[firstFreeIndex];
		}
	}

	return NULL;
}

// ----------------------------------------------------------------
// audPedScenarioManager Update
// ----------------------------------------------------------------
void audPedScenarioManager::Update()
{
	for(u32 loop = 0; loop < kMaxActiveScenarios; loop++)
	{
		m_ActiveScenarios[loop].Update();
	}

	REPLAY_ONLY(UpdateReplayScenarioStates();)
}

// ----------------------------------------------------------------
// audPedScenarioManager OnScenarioStreamStopCallback
// ----------------------------------------------------------------
bool audPedScenarioManager::OnScenarioStreamStopCallback(u32 userData)
{
	audPedScenario* scenario = g_PedScenarioManager.GetPedScenario(userData);
	scenario->StopStream();
	return true;
}

// ----------------------------------------------------------------
// audPedScenarioManager HasScenarioStreamStoppedCallback
// ----------------------------------------------------------------
bool audPedScenarioManager::HasScenarioStreamStoppedCallback(u32 userData)
{
	audPedScenario* scenario = g_PedScenarioManager.GetPedScenario(userData);
	return scenario->HasStreamStopped();
}

#if GTA_REPLAY
// ----------------------------------------------------------------
// audPedScenarioManager AddReplayActiveScenario
// ----------------------------------------------------------------
void audPedScenarioManager::AddReplayActiveScenario(audReplayScenarioState replayScenario)
{
	m_ReplayActiveScenarios.PushAndGrow(replayScenario);
}

// ----------------------------------------------------------------
// audPedScenarioManager UpdateReplayScenarioStates
// ----------------------------------------------------------------
void audPedScenarioManager::UpdateReplayScenarioStates()
{
	if(!CReplayMgr::IsEditModeActive())
	{
		m_ReplayActiveScenarios.clear();

		for(u32 loop = 0; loop < m_ActiveScenarios.GetMaxCount(); loop++)
		{			
			if(m_ActiveScenarios[loop].IsActive() && m_ActiveScenarios[loop].IsSoundValid() && m_ActiveScenarios[loop].GetSoundHash() != g_NullSoundHash)
			{
				audReplayScenarioState replayScenarioState;
				replayScenarioState.scenarioName = m_ActiveScenarios[loop].GetName();
				replayScenarioState.scenarioID = m_ActiveScenarios[loop].GetUniqueID();
				replayScenarioState.soundHash = m_ActiveScenarios[loop].GetSoundHash();
				replayScenarioState.scenarioCenter.Store(VEC3V_TO_VECTOR3(m_ActiveScenarios[loop].GetCenter()));
				m_ReplayActiveScenarios.PushAndGrow(replayScenarioState);
			}
		}
	}
	else
	{		
		// Start any scenarios that need starting/have changed
		for(u32 replayScenarioIndex = 0; replayScenarioIndex < m_ReplayActiveScenarios.GetCount(); replayScenarioIndex++)
		{
			bool exists = false;
			s32 firstFreeIndex = -1;

			audReplayScenarioState replayScenarioState = m_ReplayActiveScenarios[replayScenarioIndex];

			for(u32 activeScenarioIndex = 0; activeScenarioIndex < m_ActiveScenarios.GetMaxCount(); activeScenarioIndex++)
			{					
				if(m_ActiveScenarios[activeScenarioIndex].IsActive() && 
				  m_ReplayActiveScenarios[replayScenarioIndex].scenarioID == m_ActiveScenarios[activeScenarioIndex].GetUniqueID())
				{
					firstFreeIndex = activeScenarioIndex;
					exists = true;
					break;
				}
				else if(firstFreeIndex == -1)
				{
					firstFreeIndex = activeScenarioIndex;
				}
			}

			if(!exists && firstFreeIndex >= 0)
			{
				PedScenarioAudioSettings* scenarioSettings = audNorthAudioEngine::GetObject<PedScenarioAudioSettings>(replayScenarioState.scenarioName);

				if(scenarioSettings)
				{
					m_ActiveScenarios[firstFreeIndex].SetScenarioSettings(scenarioSettings);
					m_ActiveScenarios[firstFreeIndex].SetUniqueID(replayScenarioState.scenarioID);
					m_ActiveScenarios[firstFreeIndex].SetSoundHash(replayScenarioState.soundHash);
					exists = true;
				}				
			}
			
			if(exists && firstFreeIndex >= 0)
			{
				Vector3 center;
				replayScenarioState.scenarioCenter.Load(center);
				m_ActiveScenarios[firstFreeIndex].SetCenter(VECTOR3_TO_VEC3V(center));
			}			
		}
	
		// Stop any scenarios that are no longer valid
		for(u32 activeScenarioIndex = 0; activeScenarioIndex < m_ActiveScenarios.GetMaxCount(); activeScenarioIndex++)
		{	
			bool exists = false;

			for(u32 replayScenarioIndex = 0; replayScenarioIndex < m_ReplayActiveScenarios.GetCount(); replayScenarioIndex++)
			{
				if(m_ReplayActiveScenarios[replayScenarioIndex].scenarioID == m_ActiveScenarios[activeScenarioIndex].GetUniqueID())
				{
					exists = true;
					break;
				}
			}

			if(!exists)
			{
				m_ActiveScenarios[activeScenarioIndex].Reset();
			}
		}
	}
}
#endif

#if __BANK
// ----------------------------------------------------------------
// audPedScenarioManager DebugDraw
// ----------------------------------------------------------------
void audPedScenarioManager::DebugDraw()
{
	for(u32 loop = 0; loop < kMaxActiveScenarios; loop++)
	{
		m_ActiveScenarios[loop].DebugDraw();
	}
}
#endif
