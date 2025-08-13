// 
// audio/radioslot.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "audiodefines.h"

#if NA_RADIO_ENABLED

#include "radioslot.h"
#include "streamslot.h"
#include "grcore/debugdraw.h"
#include "radioaudioentity.h"
#include "radiostation.h"
#include "northaudioengine.h"
#include "audio/environment/environment.h"
#include "audioengine/engine.h"
#include "vehicles/vehicle.h"
#include "Cutscene/CutSceneManagerNew.h"

#include "debugaudio.h"

#include "audiohardware/driverdefs.h"

AUDIO_MUSIC_OPTIMISATIONS()

enum audRadioSlotStates
{
	RADIO_SLOT_IDLE,
	RADIO_SLOT_STARTING,
	RADIO_SLOT_PLAYING,
	RADIO_SLOT_STOPPING,
	RADIO_SLOT_RETUNING,
	RADIO_SLOT_RETUNING_STOPPING_OLD,
	RADIO_SLOT_RETUNING_STARTING_NEW
};

audRadioStation *audRadioSlot::sm_RetuneStation = NULL;
audRadioSlot *audRadioSlot::sm_RadioSlots[g_MaxRadioSlots];
bank_float g_PlayerPositionedRadioLinVol = 4.f;
bank_float g_PlayerPositionedRadioSoftTopLinVol = 6.f;
f32 g_PositionedPlayerVehicleRadioReverbSmall = 1.0f;
f32 g_PositionedPlayerVehicleRadioReverbMedium = 0.37f;
f32 g_PositionedPlayerVehicleRadioReverbLarge = 0.0f;
bool g_PositionedPlayerVehicleRadioEnabled = false;

bank_float g_CutsceneRadioLeakage = 0.5f;
BANK_ONLY(extern bool g_DebugSimulateStreamSlotStarvation;)

u32 audRadioSlot::sm_SoundBucketId = ~0U;

void audRadioSlot::InitClass(void)
{
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		sm_RadioSlots[slotIndex] = rage_new audRadioSlot(slotIndex);
	}
	
	sm_SoundBucketId = audSound::ReserveBucket();
}

void audRadioSlot::ShutdownClass(void)
{
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		if(sm_RadioSlots[slotIndex])
		{
			delete sm_RadioSlots[slotIndex];
			sm_RadioSlots[slotIndex] = NULL;
		}
	}
}

audRadioSlot *audRadioSlot::GetSlot(u32 index)
{
	naCErrorf(index < g_MaxRadioSlots, "Index passed into GetSlot is out of bounds");
	return ((index < g_MaxRadioSlots) ? sm_RadioSlots[index] : NULL);
}

audRadioSlot *audRadioSlot::FindSlotByStation(const audRadioStation *station)
{
	audRadioSlot *slot;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		slot = sm_RadioSlots[slotIndex];
		if(slot && (slot->GetStation() == station))
		{
			return slot;
		}
	}

	return NULL;
}

audRadioSlot *audRadioSlot::FindSlotByEmitter(audRadioEmitter *emitter)
{
	audRadioSlot *slot;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		slot = sm_RadioSlots[slotIndex];
		if(slot && (slot->HasEmitter(emitter)))
		{
			return slot;
		}
	}

	return NULL;
}

audRadioSlot *audRadioSlot::FindFreeSlot(u32 priority)
{
	audRadioSlot *slot;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		slot = sm_RadioSlots[slotIndex];
		if(slot && slot->IsFree(priority == audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO))
		{
			//We have a free radio slot, so now lets see if we have a free stream slot.
			if(slot->HasStreamSlot(0) || slot->HasStreamSlot(1))
			{
				return slot;
			}
			else if(slot->AllocateStreamSlot(0, priority))
			{
				return slot;
			}
		}
	}

	return NULL;
}

audRadioSlot *audRadioSlot::FindSlotToOverride(u32 priority)
{
	//Find the slot with the lowest priority.
	u32 lowestPriority = priority;
	audRadioSlot *lowestPrioritySlot = NULL;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		audRadioSlot *slot = sm_RadioSlots[slotIndex];
		if(slot && 
			(slot->GetState() == RADIO_SLOT_IDLE || slot->GetState()  == RADIO_SLOT_PLAYING || slot->GetState() == RADIO_SLOT_RETUNING))
		{
			u32 slotPriority = slot->GetPriority();
			if(slotPriority < lowestPriority)
			{
				lowestPriority = slotPriority;
				lowestPrioritySlot = slot;
			}
		}
	}

	if(lowestPrioritySlot)
	{
		//We have found a slot we can override, so now lets make sure we have a stream slot.
		if(lowestPrioritySlot->HasStreamSlot(0) || lowestPrioritySlot->HasStreamSlot(1))
		{
			return lowestPrioritySlot;
		}
		else if(lowestPrioritySlot->AllocateStreamSlot(0, priority))
		{
			return lowestPrioritySlot;
		}
	}

	return NULL;
}

audRadioSlot *audRadioSlot::FindSlotWithLeastEmitters(void)
{
	u32 emitterIndex;
	u32 numEmittersPerSlot;
	u32 minEmittersPerSlot = g_NumRadioSlotEmitters + 1;
	audRadioSlot *minEmittersSlot = NULL;
	audRadioSlot *slot;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		slot = sm_RadioSlots[slotIndex];
		numEmittersPerSlot = 0;

		for(emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
		{
			if(slot && (slot->GetEmitter(emitterIndex) != NULL))
			{
				numEmittersPerSlot++;
			}
		}

		if(numEmittersPerSlot < minEmittersPerSlot)
		{
			minEmittersPerSlot = numEmittersPerSlot;
			minEmittersSlot = slot;
		}
	}

	return minEmittersSlot;
}

void audRadioSlot::UpdateSlots(u32 timeInMs)
{
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		sm_RadioSlots[slotIndex]->Update(timeInMs);
	}
}

void audRadioSlot::SkipForwardSlots(u32 timeToSkipMs)
{
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		sm_RadioSlots[slotIndex]->SkipForward(timeToSkipMs);
	}
}

void audRadioSlot::SetPositionedPlayerVehicleRadioEmitterEnabled(bool enabled)
{
	g_PositionedPlayerVehicleRadioEnabled = enabled;
}

s32 audRadioSlot::GetSlotIndex(audRadioSlot* slot)
{
	for (u32 i = 0; i < g_MaxRadioSlots; i++)
	{
		if (sm_RadioSlots[i] == slot)
		{
			return i;
		}
	}

	return -1;
}

bool audRadioSlot::RequestRadioStationForEmitter(const audRadioStation *station, audRadioEmitter *emitter, u32 priority, s32 requestedPCMChannel)
{
	BANK_ONLY(const bool isClubEmitter = emitter && emitter->IsClubEmitter());
	BANK_ONLY(if (isClubEmitter) { audDisplayf("[CLUB EMITTERS] Club radio emitter requesting slot for station %s", station ? station->GetName() : "NULL"); })

	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if(slot)
	{
		//We are already playing radio for this emitter.
		if(slot->GetStation() == station || (slot->IsRetuning() && slot->GetStationPendingRetune() == station))
		{
			//We are already playing the correct station.
			BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Emitter already playing station %s from slot %d", station ? station->GetName() : "NULL", GetSlotIndex(slot));})
			return true;
		}
		else
		{
			//We are playing a different station for this emitter, so remove it and allow it to be allocated the correct station below.
			BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Emitter already playing from slot %d, but using different station (%s)", GetSlotIndex(slot), slot->GetStation() ? slot->GetStation()->GetName() : "NULL");})
			slot->RemoveEmitter(emitter, false);
		}
	}

	bool isPlayerRadio = (priority == audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO);

	//Check if we're already playing this station.
	slot = FindSlotByStation(station);
	if(slot)
	{
		//Only add new emitters to radio slots that are playing (or about to play.)
		u32 state = slot->GetState();
		if((state == RADIO_SLOT_STARTING) || (state == RADIO_SLOT_PLAYING))
		{
			BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Slot %d already playing station - adding...", GetSlotIndex(slot));})

			if(slot->AddEmitter(emitter, isPlayerRadio, requestedPCMChannel, station))
			{
				BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Successfully added emitter to slot %d", GetSlotIndex(slot));})
				return true;
			}
		}

		//We're already playing the requested station, but we couldn't add this emitter.
		BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Failed to add emitter to slot %d", GetSlotIndex(slot));})
		return false;
	}

	BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] No slot found - searching for free slot...");})

	//Check if we have a free radio slot (that's been free for a while.)
	slot = FindFreeSlot(priority);
	if(slot)
	{
		BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Found free slot (%d), starting radio playback", GetSlotIndex(slot));})

		//We're not already playing the requested station, but we have a free slot.
		slot->AddEmitter(emitter, false, requestedPCMChannel, station);
		slot->StartRadioPlayback(station);

		return true;
	}
	else
	{
		BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] No free slots, searching for slot to override");})

		slot = FindSlotToOverride(priority);
		if(slot)
		{
			BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Found slot to override (%d)", GetSlotIndex(slot));})

			//Add this emitter to the slot and switch off the radios of the existing emitters.
			if(slot->OverrideEmitters(emitter, const_cast<audRadioStation*>(station), requestedPCMChannel))
			{
				BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Successfully overrode emitters in slot %d", GetSlotIndex(slot));})
				return true;
			}
		}
		/*else if(priority == audStreamSlot::STREAM_PRIORITY_ENTITY_EMITTER_NEAR)
		{
			//We're not already playing the requested station, and we don't have any free slots for this ambient vehicle emitter.

			//Try and allocate an emitter on a slot playing another station.
			// - Starting on a random slot, so we don't fill them in order.
			u32 startOffset = static_cast<u32>(audEngineUtil::GetRandomNumberInRange(0, g_MaxRadioSlots - 1));
			for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
			{
				slot = GetSlot((slotIndex + startOffset) % g_MaxRadioSlots);

				//Only add new emitters to radio slots that are playing (or about to play.)
				u32 state = slot->GetState();
				if((state == RADIO_SLOT_STARTING) || (state == RADIO_SLOT_PLAYING))
				{
					if(slot->AddEmitter(emitter, isPlayerRadio))
					{
						return slot->GetStationIndex();
					}
				}
			}
		}*/
	}

	BANK_ONLY(if (isClubEmitter) {audDisplayf("[CLUB EMITTERS] Radio station request failed!");})

	//We couldn't add this emitter.
	return false;
}

bool audRadioSlot::IsStationActive(const audRadioStation *station)
{
	return (FindSlotByStation(station) != NULL);
}

void audRadioSlot::FindAndStopEmitter(audRadioEmitter *emitter, bool clearStation)
{
	/*
	//Find this emitter in our radio slots and NULL it.
	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if(slot)
	{
		slot->RemoveEmitter(emitter);
	}
	*/

	u32 count = 0;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		audRadioSlot *slot = sm_RadioSlots[slotIndex];
		if(slot && (slot->HasEmitter(emitter)))
		{
			slot->RemoveEmitter(emitter, clearStation);
			count++;
		}
	}

	naAssertf(count <= 1, "Emitter registered with %u slots", count);
}

void audRadioSlot::SwapRadioEmitter(audRadioEmitter *oldEmitter, audRadioEmitter *newEmitter)
{
	u32 count = 0;
	for(u32 slotIndex=0; slotIndex<g_MaxRadioSlots; slotIndex++)
	{
		audRadioSlot *slot = sm_RadioSlots[slotIndex];
		if(slot && (slot->HasEmitter(oldEmitter)))
		{			
			slot->RemoveEmitter(oldEmitter, false);
			slot->AddEmitter(newEmitter);
			count++;
		}
	}

	naAssertf(count <= 1, "Emitter registered with %u slots", count);
}

bool audRadioSlot::IsEmitterRetuning(audRadioEmitter *emitter)
{
	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if(slot)
	{
		return slot->IsRetuning();
	}

	return false;
}

void audRadioSlot::FindAndRetuneEmitter(audRadioEmitter *emitter)
{
	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if(slot)
	{
		slot->RetuneEmitter(emitter);
	}
	else
	{
		//The radio is currently off for this emitter, so we need to add it to one of the radio slots.
		//Add the emitter to the slot with the least existing emitters and switch off the radios of those emitters.
		slot = FindSlotWithLeastEmitters();
		if(slot)
		{
			//Make sure we have a stream slot.
			if(!slot->HasStreamSlot(0) && !slot->HasStreamSlot(1))
			{
				slot->AllocateStreamSlot(0, audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO);
			}

			if(slot->HasStreamSlot(0) || slot->HasStreamSlot(1))
			{
				slot->OverrideEmitters(emitter);
			}
		}
	}
}

#if !__FINAL
void audRadioSlot::FindAndSkipForwardEmitter(audRadioEmitter *emitter, u32 timeToSkipMs)
{
	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if(slot)
	{
		slot->SkipForward(timeToSkipMs);
	}
}

void audRadioSlot::FindAndSkipForwardEmitterToTransition(audRadioEmitter *emitter, u32 timeToSkipMs)
{
	audRadioSlot *slot = FindSlotByEmitter(emitter);
	if (slot)
	{
		slot->SkipForwardToTransition(timeToSkipMs);
	}
}
#endif // !__FINAL

bool audRadioSlot::OnStopCallback(u32 userData)
{
	audRadioSlot *slot = audRadioSlot::GetSlot(userData);
	if(slot)
	{
		//We are losing our stream slot, so stop the radio slot.
		slot->Stop();
	}

	return true;
}

bool audRadioSlot::HasStoppedCallback(u32 userData)
{
	bool hasStopped = true;
	audRadioSlot *slot = audRadioSlot::GetSlot(userData);
	if(slot)
	{
		//Check if we have stopped streaming from this radio slot yet
		// NOTE: check for both starting states, since they block until their stream slot is active - this generates a dead lock
		hasStopped = (slot->GetState() == RADIO_SLOT_IDLE || slot->GetState() == RADIO_SLOT_RETUNING_STARTING_NEW || slot->GetState() == RADIO_SLOT_STARTING);
	}

	return hasStopped;
}

audRadioSlot::audRadioSlot(u32 slotIndex)
{
	Assign(m_SlotIndex, slotIndex);
	m_StreamSlots[0] = NULL;
	m_StreamSlots[1] = NULL;
	m_State = RADIO_SLOT_IDLE;
	m_Priority = audStreamSlot::STREAM_PRIORITY_IDLE;
	m_IdleStartTimeMs = 0;
	m_TimeToSkipForwardMs = 0;
	m_Station = NULL;
	m_RetuningEmitterIndex = g_NullRadioSlotEmitter;
	m_RetuningStation = NULL;
	m_ShouldSkipForward = false;
	m_WasUsingPositionedPlayerVehicleRadioReverb = false;

	m_CurrentStationIndex = 0;

	for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
	{
		ClearEmitter(emitterIndex);
	}
}

audRadioSlot::~audRadioSlot()
{
}

bool audRadioSlot::IsFree(bool UNUSED_PARAM(isRequestingPlayerRadio))
{
	//if(isRequestingPlayerRadio)
	{
		//Don't bother checking free time for player requests.
		return (m_State == RADIO_SLOT_IDLE);
	}
	/*else
	{
		return ((m_State == RADIO_SLOT_IDLE) && (g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2) >
			(m_IdleStartTimeMs + g_MinRadioSlotIdleTimeMs)));
	}*/
}

bool audRadioSlot::IsRetuning(void)
{
	return ((m_State == RADIO_SLOT_RETUNING) || (m_State == RADIO_SLOT_RETUNING_STOPPING_OLD) || (m_State == RADIO_SLOT_RETUNING_STARTING_NEW));
}

bool audRadioSlot::AllocateStreamSlot(u32 index, u32 priority)
{
	audStreamClientSettings settings;

	// If we haven't specified a priority, use the old one, otherwise update the recorded priority
	if(priority == audStreamSlot::NUM_STREAM_PRIORITIES)
	{
		priority = m_Priority;
	}
	else
	{
		m_Priority = priority;
	}

	Assign(settings.priority, priority);
	settings.stopCallback = &OnStopCallback;
	settings.hasStoppedCallback = &HasStoppedCallback;
	settings.userData = m_SlotIndex;

	if(!m_StreamSlots[index])
	{
		m_StreamSlots[index] = audStreamSlot::AllocateSlot(&settings);
	}

	return (m_StreamSlots[index] != NULL);
}

void audRadioSlot::Update(u32 timeInMs)
{
	bool hasEmitters = false;
	CVehicle *playerVehicle = NULL;
	
	const audRadioStation *playerRadioStation = g_RadioAudioEntity.GetPlayerRadioStation();

	CalculatePriority();	

	switch(m_State)
	{
		case RADIO_SLOT_STARTING:
			//Check if our radio station is streaming physically yet.
			if(m_Station)
			{
				//Start this station muted to prevent audibility prior to setting up correct emitter volumes.
				m_Station->MuteEmitters();

				naAssertf(m_StreamSlots[0] || m_StreamSlots[1], "Invalid stream slot in RADIO_SLOT_STARTING state during Update");

				// If we've just got the one slot, then duplicate it so that we use it regardless of which track is playing
				if(!m_StreamSlots[0] && m_StreamSlots[1])
				{
					m_StreamSlots[0] = m_StreamSlots[1];
				}
				else if(!m_StreamSlots[1] && m_StreamSlots[0])
				{
					m_StreamSlots[1] = m_StreamSlots[0];
				}

				if(m_Station->IsStreamingPhysically())
				{
					m_State = RADIO_SLOT_PLAYING;
				}
				else if((m_StreamSlots[0] && (m_StreamSlots[0]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE)) &&
						(m_StreamSlots[1] && (m_StreamSlots[1]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE)))
				{
					m_Station->SetPhysicalStreamingState(true, m_StreamSlots.GetElements(), (u8)sm_SoundBucketId);
				}
			}
			break;

		case RADIO_SLOT_PLAYING:
			if(m_Station)
			{
				if(m_Station->IsStreamingPhysically())
				{
					if(m_Station->IsRequestingOverlappedTrackPrepare())
					{
#if __BANK
						// Simulate not being able to allocate additional stream slots. The playback should starve, stop, then restart, just
						// without the crossfade between the two track
						if(!g_DebugSimulateStreamSlotStarvation)
#endif
						{
							// We need two unique stream slots if we're doing a crossfade
							if(m_StreamSlots[0] == m_StreamSlots[1] && !m_Station->GetNextTrack().IsPlaying())
							{
								m_StreamSlots[m_Station->GetNextTrackIndex()] = NULL;
							}

							AllocateStreamSlot(0);
							AllocateStreamSlot(1);
						}
					}
					else
					{
						// We don't currently need the slot for the next track, so get rid. Slot manager default HasStopped function
						// will verify that reference counts etc. are at zero
						if(m_Station->GetNextTrack().GetWaveSlot() == NULL)
						{
							u32 currentTrackIndex = m_Station->GetActiveTrackIndex();
							u32 nextTrackIndex = m_Station->GetNextTrackIndex();

							if(m_StreamSlots[nextTrackIndex])
							{
								if(m_StreamSlots[nextTrackIndex] != m_StreamSlots[currentTrackIndex])
								{
									m_StreamSlots[nextTrackIndex]->ClearActiveSettings();
									m_StreamSlots[nextTrackIndex]->StopAndFree();
									m_StreamSlots[nextTrackIndex] = NULL;
								}
								else
								{
									m_StreamSlots[nextTrackIndex] = NULL;
								}
							}
						}
					}

					m_Station->SetStreamSlots(m_StreamSlots.GetElements());
				}
				else
				{
					// catch the strange case where we're marked as playing but the station is still virtual - we need to jump back to starting
					// to kick off the phys streaming
					m_State = RADIO_SLOT_STARTING;
					break;
				}
			}
			/*if(m_Station && m_Station->IsStreamingPhysically() && m_Station->IsLocked())
			{
				// if this is no longer a valid station then stop playing it physically
				m_Station->SetPhysicalStreamingState(false);
				m_State = RADIO_SLOT_STOPPING;
				break;
			}*/

			//Check if we have any emitters and if one of them is the player's vehicle.
			playerVehicle = g_RadioAudioEntity.GetLastPlayerVehicle();
			for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
			{
				if(m_Emitters[emitterIndex] != NULL)
				{
					hasEmitters = true;

					//Make sure each emitter is assigned the active station (handling the player vehicle as a special-case.)
					m_Emitters[emitterIndex]->SetRadioStation(m_Station);
					if(m_Emitters[emitterIndex]->Equals(playerVehicle))
					{
						playerRadioStation = m_Station;
					}
				}
			}

			//If the player is fully in a vehicle and this is not the player's station we have to stop this slot so we
			//don't take up too much streaming bandwidth.
			if(hasEmitters &&
				(m_Station == playerRadioStation ||	playerRadioStation == NULL || !g_RadioAudioEntity.StopEmittersForPlayerVehicle()))
			{
#if !__WIN32PC
				naAssertf(m_StreamSlots[0] || m_StreamSlots[1], "No valid stream slots in radioslot update");
#endif

				UpdateEmitters();

				//TODO: Check if this station can realistically be heard before updating its history.
				if(m_Station)
				{
					m_Station->AddActiveTrackToHistory();
				}
			}
			else
			{
				//This slot is now free, so stop it.
				Stop();
			}
			break;

		case RADIO_SLOT_STOPPING:
			if(m_Station == NULL)
			{
				//The radio is currently switched off, so there's no station to stop physically streaming.
				m_IdleStartTimeMs = timeInMs;
				m_State = RADIO_SLOT_IDLE;
				FreeStreamSlot();
			}
			else if(m_Station)
			{
				//Check if our radio station is streaming virtually yet.
				if(m_Station->IsStreamingVirtually())
				{
					m_Station = NULL;
					m_IdleStartTimeMs = timeInMs;
					m_State = RADIO_SLOT_IDLE;
					FreeStreamSlot();
				}
				else
				{
					m_Station->SetPhysicalStreamingState(false);
				}
			}
			else
			{
				naErrorf("In Update; case RADIO_SLOT_STOPPING, Radio station is not off but doesn't have a Valid audRadioStation");
			}
			break;

		case RADIO_SLOT_RETUNING:
			if(!ActionRadioRetune(timeInMs))
			{
				//This station is retuning, so mute all radio sounds.
				if(m_Station)
				{
					m_Station->MuteEmitters();
				}
			}

			break;

		case RADIO_SLOT_RETUNING_STOPPING_OLD:
			if(!m_Station)
			{
				//The radio is currently switched off, so there's no station to stop physically streaming.
				m_Station = const_cast<audRadioStation*>(m_RetuningStation);
				if(m_Emitters[m_RetuningEmitterIndex])
				{
					m_Emitters[m_RetuningEmitterIndex]->SetRadioStation(m_RetuningStation);
				}
				m_RetuningEmitterIndex = g_NullRadioSlotEmitter;
				m_RetuningStation = NULL;
				m_State = RADIO_SLOT_RETUNING_STARTING_NEW;
			}
			else if(m_Station)
			{
				//Check if our radio station is streaming virtually yet.
				if(m_Station->IsStreamingVirtually())
				{
					m_Station = const_cast<audRadioStation*>(m_RetuningStation);
					if(m_Emitters[m_RetuningEmitterIndex])
					{
						m_Emitters[m_RetuningEmitterIndex]->SetRadioStation(m_RetuningStation);
					}
					m_RetuningEmitterIndex = g_NullRadioSlotEmitter;
					m_RetuningStation = NULL;

					if(m_ShouldSkipForward)
					{
						//Action the skip forward request by advancing the virtual play time of the new radio
						//station's active track.
						if(m_Station)
						{
							m_Station->SkipForward(m_TimeToSkipForwardMs);
						}

						m_ShouldSkipForward = false;
					}

					m_State = RADIO_SLOT_RETUNING_STARTING_NEW;
				}
				else
				{
					naAssertf(m_StreamSlots[0] || m_StreamSlots[1], "In Update; case RADIO_SLOT_RETUNING_STOPPING_OLD, station is not streaming virtually but has no valid stream slot");
					m_Station->SetPhysicalStreamingState(false);
				}
			}
			break;

		case RADIO_SLOT_RETUNING_STARTING_NEW:
			//Check if our radio station is streaming physically yet.
			if(!m_StreamSlots[0] && !m_StreamSlots[1])
			{
				naWarningf("RADIO_SLOT_RETUNING_STARTING_NEW with NULL m_StreamSlot: %p, %u", this, m_SlotIndex);
				m_State = RADIO_SLOT_STOPPING;
			}
			else if(m_Station)
			{
				if(m_Station->IsStreamingPhysically())
				{
					m_State = RADIO_SLOT_PLAYING;
				}
				else
				{	
					// If we've just got the one slot, then duplicate it so that we use it regardless of which track is playing
					if(!m_StreamSlots[0] && m_StreamSlots[1])
					{
						m_StreamSlots[0] = m_StreamSlots[1];
					}
					else if(!m_StreamSlots[1] && m_StreamSlots[0])
					{
						m_StreamSlots[1] = m_StreamSlots[0];
					}

					if(HasStreamSlots())
					{
						if(m_StreamSlots[0]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE &&
							m_StreamSlots[1]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE)
						{
							m_Station->SetPhysicalStreamingState(true, m_StreamSlots.GetElements(), (u8)sm_SoundBucketId);
						}
					}
				}
			}
			else
			{
				m_State = RADIO_SLOT_PLAYING;
			}
			break;

		case RADIO_SLOT_IDLE:
		default:
			break;
	}
}

void audRadioSlot::StartRadioPlayback(const audRadioStation *station)
{
	m_Station = const_cast<audRadioStation*>(station);
	m_State = RADIO_SLOT_STARTING;
	m_CurrentStationIndex = station->GetStationIndex();
}

bool audRadioSlot::ActionRadioRetune(u32 timeInMs)
{
	//Ensure that we don't retune too quickly.
	if(!g_RadioAudioEntity.IsTimeToRetune(timeInMs))
	{
		return false; //Still retuning.
	}

	const audRadioStation *newStation = m_RetuningStation ? m_RetuningStation : g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
	//Check if we're already playing this station in a slot.
	audRadioSlot *otherRadioSlot = audRadioSlot::FindSlotByStation(newStation);
	if(otherRadioSlot)
	{
		if(otherRadioSlot != this)
		{
			//We're already playing this station in another radio slot, so move the vehicle over.
			audRadioEmitter *emitter = m_Emitters[m_RetuningEmitterIndex];
			if(emitter)
			{
				otherRadioSlot->AddEmitter(emitter, true);
				emitter->SetRadioStation(newStation);

				//Remove the original emitter entry.
				m_Emitters[m_RetuningEmitterIndex] = NULL;
			}
		}
		//else if(otherRadioSlot == this)
		//	- We're already playing this station in the current host slot, so the retune is complete.

		g_RadioAudioEntity.ClearPendingPlayerRadioStationRetunes();
		
		//Return to playing state.
		m_State = RADIO_SLOT_PLAYING;
		m_RetuningEmitterIndex = g_NullRadioSlotEmitter;

		return true; //Done retuning.
	}

	//We're not already playing this station, so...

	//Retune this slot to the requested station, switching off all other emitters using this slot, to avoid retuning
	//them too.
	m_RetuningStation = newStation;
	for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
	{
		if((emitterIndex != m_RetuningEmitterIndex) && m_Emitters[emitterIndex])
		{
			//Switch the radio off for this other emitter.
			m_Emitters[emitterIndex]->SetRadioStation(NULL);
			ClearEmitter(emitterIndex);
		}
	}

	g_RadioAudioEntity.ClearPendingPlayerRadioStationRetunes();
	
	m_State = RADIO_SLOT_RETUNING_STOPPING_OLD;

	return false; //Still retuning.
}

bool audRadioSlot::AddEmitter(audRadioEmitter *emitter, bool shouldReplaceExisting, s32 requestedPCMChannel, const audRadioStation* station)
{
	if(!naVerifyf(emitter != NULL, "NULL radio emitter being added to slot"))
	{
		return false;
	}

	

	//Check if we already have this emitter.
	for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
	{
		if(m_Emitters[emitterIndex] && m_Emitters[emitterIndex]->Equals(emitter))
		{
			return true;
		}
	}

	//Check if we have a free emitter.
	u32 largestDistanceEmitterIndex = 0;
	f32 largestEmitterDistance = 0.0f;
	f32 distance;
	Vector3 distanceVector;
	bool foundValidReplacementCandidate = false;

	for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
	{
		const u32 numStationChannels = station ? station->GetNumChannels() : 2;
		
		if (requestedPCMChannel >= 0 && (emitterIndex % numStationChannels) != (u32)requestedPCMChannel)
		{
			continue;
		}

		if(m_Emitters[emitterIndex] == NULL)
		{
			//Use this free emitter.
			m_Emitters[emitterIndex] = emitter;
			return true;
		}
		else if(shouldReplaceExisting && m_Emitters[emitterIndex]->IsReplaceable())
		{
			//Check if this emitter is the farthest from the listener.
			Vector3 position;
			m_Emitters[emitterIndex]->GetPosition(position);
			distanceVector = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - position;

			distance = distanceVector.Mag();
			if(distance > largestEmitterDistance)
			{
				largestEmitterDistance = distance;
				largestDistanceEmitterIndex = emitterIndex;
				foundValidReplacementCandidate = true;
			}
		}
	}

	if(shouldReplaceExisting && foundValidReplacementCandidate)
	{
		//Replace the most distant emitter.
		m_Emitters[largestDistanceEmitterIndex]->SetRadioStation(NULL);
		m_Emitters[largestDistanceEmitterIndex] = emitter;
		
		return true;
	}

	return false;
}

void audRadioSlot::RemoveEmitter(audRadioEmitter *emitter, bool clearRadioStation)
{
	if(naVerifyf(emitter, "Null audRadioEmitter passed into RemoveEmitter"))
	{
		if(clearRadioStation)
		{
			emitter->SetRadioStation(NULL);
		}		

		u32 count = 0;
		for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
		{
			if(m_Emitters[emitterIndex] == emitter)
			{
				ClearEmitter(emitterIndex);
				count++;
			}
		}

		naAssertf(count <= 1, "emitter registered multiple times on one slot (%u)", count);		

		if(count > 0)
		{
			CalculatePriority();
		}
	}	
}

void audRadioSlot::ClearEmitter(s32 emitterIndex)
{
	m_Emitters[emitterIndex] = NULL;	
}

void audRadioSlot::CalculatePriority()
{
	const audRadioStation *playerRadioStation = g_RadioAudioEntity.IsPlayerRadioActive() ? g_RadioAudioEntity.GetPlayerRadioStation() : NULL;

	for(u32 loop = 0; loop < 2; loop++)
	{
		if(m_StreamSlots[loop])
		{
			//Update the priority of our stream slot (if we have one), based on the highest priority emitter we have
			u32 priority = audStreamSlot::STREAM_PRIORITY_IDLE;

			if(m_Station == playerRadioStation)
			{
				priority = audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO;
			}
			for(u32 i = 0; i < g_NumRadioSlotEmitters; i++)
			{
				if(GetEmitter(i) && GetEmitter(i)->GetPriority() > priority)
				{
					priority = GetEmitter(i)->GetPriority();
				}
			}

			m_Priority = priority;
			m_StreamSlots[loop]->SetPriority(priority);
		}
	}
}

void audRadioSlot::RetuneEmitter(audRadioEmitter *emitter)
{
	//Only request the retune if this slot is in a safe state.
	if((m_State == RADIO_SLOT_IDLE) || (m_State == RADIO_SLOT_PLAYING) ||
		(m_State == RADIO_SLOT_RETUNING) || (m_State == RADIO_SLOT_STARTING && m_Station && m_Station->IsFrozen()))
	{
		//Enter the retuning state.
		m_State = RADIO_SLOT_RETUNING;
		m_RetuningEmitterIndex = FindEmitterIndex(emitter);
	}
	//else - This slot is not currently in a safe state, so try again next update.
}

void audRadioSlot::SkipForward(u32 timeToSkipMs)
{
	//Only request the skip if this slot is playing a real station.
	if((m_State == RADIO_SLOT_PLAYING) && (m_Station != NULL))
	{
		//Enter the retuning state.
		m_RetuningEmitterIndex = 0; //This does not matter as we are not changing station.
		m_ShouldSkipForward = true;
		m_TimeToSkipForwardMs = timeToSkipMs;
		m_RetuningStation = m_Station;
		m_State = RADIO_SLOT_RETUNING_STOPPING_OLD;
	}
	//else - This slot is not currently in a safe state, so try again next update.
}

void audRadioSlot::SkipForwardToTransition(u32 timeToSkipMs)
{
	//Only request the skip if this slot is playing a real station.
	if ((m_State == RADIO_SLOT_PLAYING) && (m_Station != NULL))
	{
		s32 playTime = m_Station->GetCurrentTrack().GetPlayTime();
		s32 duration = m_Station->GetCurrentTrack().GetDuration();
		s32 timeRemaining = duration - playTime;
		timeRemaining -= timeToSkipMs;

		if (timeRemaining > 0)
		{
			//Enter the retuning state.
			m_RetuningEmitterIndex = 0; //This does not matter as we are not changing station.
			m_ShouldSkipForward = true;
			m_TimeToSkipForwardMs = timeRemaining;
			m_RetuningStation = m_Station;
			m_State = RADIO_SLOT_RETUNING_STOPPING_OLD;
		}
	}
	//else - This slot is not currently in a safe state, so try again next update.
}

bool audRadioSlot::OverrideEmitters(audRadioEmitter *emitter, audRadioStation *newStation, s32 requestedPCMChannel)
{
	naCErrorf(emitter, "Null audradioEmitter passed into OverrideEmitters");

	naAssertf(m_StreamSlots[0] || m_StreamSlots[1], "Null stream slot in OverrideEmitters");

	//Only override the existing emitters if this slot is in a safe state.
	if((m_State == RADIO_SLOT_IDLE) || (m_State == RADIO_SLOT_PLAYING) ||
		(m_State == RADIO_SLOT_RETUNING))
	{
		if(newStation)
		{
			newStation->MuteEmitters();
		}

		for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
		{
			if(m_Emitters[emitterIndex])
			{
#if __BANK
				if (m_Emitters[emitterIndex]->IsClubEmitter())
				{
					audDisplayf("[CLUB EMITTERS] Club emitter in slot %d is being overriden for station %s", GetSlotIndex(this), newStation ? newStation->GetName() : "NULL");
				}
#endif

				m_Emitters[emitterIndex]->SetRadioStation(NULL);
				ClearEmitter(emitterIndex);
			}
		}

		u32 emitterIndex = requestedPCMChannel == -1 ? 0 : requestedPCMChannel;
		m_Emitters[emitterIndex] = emitter;
		m_RetuningEmitterIndex = emitterIndex;

		if(emitter)
		{
			emitter->SetRadioStation(NULL);
		}

		if(m_Station)
		{			
			m_Station->SetPhysicalStreamingState(false);
			m_Station = NULL;
		}
		
		m_RetuningStation = newStation;
		
		//Enter the retuning state.
		m_State = RADIO_SLOT_RETUNING;

		return true;
	}

	return false;
}

void audRadioSlot::UpdateEmitters(void)
{
	if(m_Station)
	{
		u32 emitterIndex;
		f32 emitterVolumeFactor;
		f32 playerVehicleInsideFactor = g_RadioAudioEntity.GetPlayerVehicleInsideFactor();
		const audRadioStation *playerRadioStation = g_RadioAudioEntity.GetPlayerRadioStation();
		f32 stereoCutoff = kVoiceFilterLPFMaxCutoff;

		if(playerRadioStation == m_Station)
		{
			//This is the Player's station, so ramp the volume of the stereo emitter sound.
			emitterVolumeFactor = playerVehicleInsideFactor;
		}
		else
		{
			//This is not the Player's station, so mute the stereo emitter sound, unless
			//we find that one of our positioned emitters is the Player's last vehicle.
			emitterVolumeFactor = 0.0f;

			if(!g_RadioAudioEntity.IsMobilePhoneRadioActive())
			{
				CVehicle *lastPlayerVehicle = g_RadioAudioEntity.GetLastPlayerVehicle();
				if(lastPlayerVehicle != NULL)
				{
					for(emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
					{
						if(m_Emitters[emitterIndex] && m_Emitters[emitterIndex]->Equals(lastPlayerVehicle))
						{
							//This is the Player's last vehicle, so ramp the volume of the
							//stereo emitter sound.
							emitterVolumeFactor = playerVehicleInsideFactor;
							break;
						}
					}
				}
			}
		}

		if(g_RadioAudioEntity.GetLastPlayerVehicle())
		{
			// Boats (specifically, submersibles) can drown, but this shouldn't affect the radio
			f32 drowningFactor = (g_RadioAudioEntity.GetLastPlayerVehicle()->InheritsFromSubmarineCar() || g_RadioAudioEntity.GetLastPlayerVehicle()->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BOAT)? 0.0f : g_RadioAudioEntity.GetLastPlayerVehicle()->GetVehicleAudioEntity()->GetDrowningFactor();
			stereoCutoff -= (kVoiceFilterLPFMaxCutoff - 500.0f) * playerVehicleInsideFactor * drowningFactor;
		}
		
		m_Station->UpdateStereoEmitter(emitterVolumeFactor, stereoCutoff);

		bool isPlayerRadioActive = (playerRadioStation != NULL);

		bool shouldPlayFullRadio = true;
		f32 emittedVolume;
		Vector3 emitterPosition;
		naEnvironmentGroup *occlusionGroup;
		bool muteDry = false;
		bool usePositionedPlayerVehicleRadioReverb = false;
		u32 LPFCutoff = kVoiceFilterLPFMaxCutoff;
		u32 HPFCutoff = 0;
		f32 environmentalLoudness = 0.f;

		for(emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
		{
			audRadioEmitter::RadioEmitterType emitterType = audRadioEmitter::RADIO_EMITTER_TYPE_MAX;

			if(m_Emitters[emitterIndex] != NULL)
			{
				//This is an active emitter, so ramp the volume of the associated positioned emitter sound.
				if(isPlayerRadioActive)
				{
					//The Player's radio is active, so we forcibly ramp down the volume of the positioned emitter sounds.
					
					if(m_Emitters[emitterIndex]->IsPlayerVehicle())
					{
						// crank it nice and loud, based on how open this car is
						if(playerVehicleInsideFactor>0.97f)
						{
//							emitterVolumeFactor = playerVehicleInsideFactor * g_PlayerPositionedRadioLinVol * (1.f - audNorthAudioEngine::GetGtaEnvironment()->GetOutsideWorldOcclusion());
							emitterVolumeFactor = playerVehicleInsideFactor * g_PlayerPositionedRadioLinVol * (1.f - audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionIgnoringInteriors());
							CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
							if (playerVehicle && playerVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
							{
								emitterVolumeFactor = playerVehicle->GetVehicleAudioEntity()->GetOpenness(false) * g_PlayerPositionedRadioSoftTopLinVol;
							}

							if(!g_PositionedPlayerVehicleRadioEnabled && DYNAMICMIXER.GetActiveSceneFromHash(ATSTRINGHASH("INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x7EBA8171)) == NULL)
							{
								muteDry = true;
							}
							else
							{
								emitterVolumeFactor = 1.0f;
								usePositionedPlayerVehicleRadioReverb = true;
							}
							
							// reduce it for talk stations
							if (playerRadioStation && playerRadioStation->IsTalkStation())
							{
								emitterVolumeFactor *= 0.6f;
							}
							// and duck for scripted conversations
							emitterVolumeFactor *= audNorthAudioEngine::GetFrontendRadioVolumeLin();
							// clamp to -60dB to avoid dbz in the shadow sound stuff
							emitterVolumeFactor = Max(emitterVolumeFactor, 0.001f);
						}
						else
						{
							emitterVolumeFactor = 1.0f - playerVehicleInsideFactor;
							// work around for shadow sound issue: we want the player car to stay above silence to ensure that we dont end up with
							// no shadow sounds with >0 contributions when computing filter cutoffs
							// -60dB should be fine...
							emitterVolumeFactor = Max(emitterVolumeFactor, 0.001f);
						}
					}
					else
					{
						if(m_Emitters[emitterIndex]->ShouldMuteForFrontendRadio())
						{
							emitterVolumeFactor = 1.0f - playerVehicleInsideFactor;
						}
						else
						{
							emitterVolumeFactor = 1.f;
						}						
					}
				}
				else if(g_RadioAudioEntity.IsPlayerInVehicle())
				{
					//The Player's radio is inactive, so we forcibly ramp up the volume of the positioned emitter sounds
					//as we now can allow positioned radio emitters to be audible even when the Player is in a vehicle.
					emitterVolumeFactor = playerVehicleInsideFactor;
				}
				else
				{
					//The Player's radio is inactive, so we forcibly ramp up the volume of the positioned emitter sounds
					//as we now can allow positioned radio emitters to be audible even when the Player is in a vehicle.
					emitterVolumeFactor = 1.0f - playerVehicleInsideFactor;
				}

				//Extract the emitted volume offset for this emitter.
				emittedVolume = m_Emitters[emitterIndex]->GetEmittedVolume();
				emitterType = m_Emitters[emitterIndex]->GetEmitterType();

				m_Emitters[emitterIndex]->GetPosition(emitterPosition);
				occlusionGroup = m_Emitters[emitterIndex]->GetOcclusionGroup();
				LPFCutoff = m_Emitters[emitterIndex]->GetLPFCutoff();			
				HPFCutoff = m_Emitters[emitterIndex]->GetHPFCutoff();			
				environmentalLoudness = m_Emitters[emitterIndex]->GetEnvironmentalLoudness();

				if (occlusionGroup)
				{
					occlusionGroup->PopulateEnvironmentMetric(&m_OcclusionMetrics[emitterIndex]);
				}

				// need to set this after calling Populate()
				m_OcclusionMetrics[emitterIndex].SetJustReverb(muteDry);

				if(usePositionedPlayerVehicleRadioReverb)
				{
					m_OcclusionMetrics[emitterIndex].SetReverbSmall(g_PositionedPlayerVehicleRadioReverbSmall);
					m_OcclusionMetrics[emitterIndex].SetReverbMedium(g_PositionedPlayerVehicleRadioReverbMedium);
					m_OcclusionMetrics[emitterIndex].SetReverbLarge(g_PositionedPlayerVehicleRadioReverbLarge);
				}
				// Reset back to normal levels once we leave aircraft passenger mode
				else if(m_WasUsingPositionedPlayerVehicleRadioReverb)
				{
					m_OcclusionMetrics[emitterIndex].SetReverbSmall(0.0f);
					m_OcclusionMetrics[emitterIndex].SetReverbMedium(0.0f);
					m_OcclusionMetrics[emitterIndex].SetReverbLarge(0.0f);
				}
				
				if(!m_Emitters[emitterIndex]->ShouldPlayFullRadio())
				{
					shouldPlayFullRadio = false;
				}

				m_OcclusionMetrics[emitterIndex].SetRolloffFactor(m_Emitters[emitterIndex]->GetRolloffFactor());

				m_WasUsingPositionedPlayerVehicleRadioReverb = usePositionedPlayerVehicleRadioReverb;

			}
			else
			{
				emitterVolumeFactor = 0.0f;
				emitterPosition.Zero();
				emittedVolume = 0.0f;
				environmentalLoudness = 0.0f;
				m_OcclusionMetrics[emitterIndex].Reset();				
			}

			m_Station->UpdatePositionedEmitter(emitterIndex, emittedVolume, emitterVolumeFactor, emitterPosition, LPFCutoff,
				HPFCutoff, &m_OcclusionMetrics[emitterIndex], emitterType, environmentalLoudness, m_Emitters[emitterIndex]? m_Emitters[emitterIndex]->IsLastPlayerVehicle() : false);
			if (m_Station->HasTrackChanged() && m_Emitters[emitterIndex])
			{
				m_Emitters[emitterIndex]->NotifyTrackChanged();
			}
		} //for(u8 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)

		// Player radio should always play full content
		m_Station->SetShouldPlayFullRadio(shouldPlayFullRadio || (playerRadioStation == m_Station));
	}
}

s32 audRadioSlot::FindEmitterIndex(audRadioEmitter *emitter)
{
	for(s32 emitterIndex=0; emitterIndex<(s32)g_NumRadioSlotEmitters; emitterIndex++)
	{
		if(m_Emitters[emitterIndex] == emitter)
		{
			return emitterIndex;
		}
	}

	return -1;
}

bool audRadioSlot::HasEmitter(audRadioEmitter *emitter)
{
	return (FindEmitterIndex(emitter) >= 0);
}

void audRadioSlot::Stop(void)
{
	if(m_Station)
	{
		m_Station->MuteEmitters();
	}

	for(u32 emitterIndex=0; emitterIndex<g_NumRadioSlotEmitters; emitterIndex++)
	{
		if(m_Emitters[emitterIndex])
		{
			m_Emitters[emitterIndex]->SetRadioStation(NULL);
		}

		ClearEmitter(emitterIndex);
	}

	FreeStreamSlot();

	m_RetuningEmitterIndex = g_NullRadioSlotEmitter;
	m_RetuningStation = NULL;

	//Enter the stopping state.
	m_State = RADIO_SLOT_STOPPING;
}

void audRadioSlot::InvalidateStation()
{
	Stop();
	m_Station = NULL;
}

void audRadioSlot::FreeStreamSlot()
{
	for(u32 loop = 0; loop < 2; loop++)
	{
		if(m_StreamSlots[loop])
		{
			if(m_StreamSlots[loop]->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE)
			{
				m_StreamSlots[loop]->Free();
			}
			else
			{
				m_StreamSlots[loop]->ClearRequestedSettings();
			}
			m_StreamSlots[loop] = NULL;
		}
	}
}

#if RSG_BANK

void audRadioSlot::DrawDebug(audDebugDrawManager &drawMgr)
{
	for(u32 i = 0; i < g_MaxRadioSlots; i++)
	{
		sm_RadioSlots[i]->DrawSlotDebug(drawMgr);
	}
}

void audRadioSlot::DrawSlotDebug(audDebugDrawManager &drawMgr)
{
	const char *prio[audStreamSlot::NUM_STREAM_PRIORITIES] = 	
	{
		"PrioIdle",
		"PrioEntityEmitter_Far",
		"PrioEntityEmitter_Near",
		"PrioStaticEmitter",
		"PrioEntityEmitter_Network",
		"ScriptedVehicleRadio",
		"PrioFrontendMenu",
		"PrioScript",
		"PrioScriptEmitter",
		"PrioMusic",
		"PrioPlayerRadio",
		"PrioCutscene",
	};
	const char *states[] =
	{
		"Idle",
		"Starting",
		"Playing",
		"Stopping",
		"Retuning",
		"Retuning_StoppingOld",
		"Retuning_StartingNew"
	};
	s32 streamSlotIndex1 = -1;
	s32 streamSlotIndex2 = -1;

	for(u32 i = 0; i < g_NumStreamSlots; i++)
	{
		if(audStreamSlot::GetSlot(i) == m_StreamSlots[0])
		{
			streamSlotIndex1 = (s32)i;
		}
		
		if(audStreamSlot::GetSlot(i) == m_StreamSlots[1])
		{
			streamSlotIndex2 = (s32)i;
		}
	}

	const char *stationName = (m_Station == NULL?"None":m_Station->GetName());
	const char *retuningStationName = (m_RetuningStation == NULL?"None":m_RetuningStation->GetName());
	drawMgr.DrawLinef("RadioSlot %u state: %s, station: %s, retuningEmitter: %u, retuningStation: %s [stream slots %d/%d]", m_SlotIndex, states[m_State], stationName, m_RetuningEmitterIndex, retuningStationName, streamSlotIndex1, streamSlotIndex2);
	for(u32 i = 0; i < g_NumRadioSlotEmitters; i++)
	{
		if(m_Emitters[i])
		{
			Vector3 pos;
			m_Emitters[i]->GetPosition(pos);
			const char *prioString = prio[m_Emitters[i]->GetPriority()];
			Vector3 emitterPos;
			m_Emitters[i]->GetPosition(emitterPos);
			f32 dist = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - emitterPos).Mag();
			drawMgr.DrawLinef("%u: %s %.2fm (%.2f,%.2f,%.2f) [%s]", i, m_Emitters[i]->GetName(), dist, pos.x, pos.y, pos.z, prioString);
		}
	}
}
#endif // RSG_BANK

f32 audRadioSlot::ComputeDistanceSqToNearestEmitter() const
{
	f32 nearestDist2 = LARGE_FLOAT;
	for(u32 i = 0 ; i < g_NumRadioSlotEmitters; i++)
	{
		if(m_Emitters[i])
		{
			// Special case active 'fill space' emitters
			if(m_Emitters[i]->GetOcclusionGroup() && m_Emitters[i]->GetOcclusionGroup()->IsCurrentlyFillingListenerSpace())
			{
				return 0.f;
			}
						
			Vector3 emitterPos;
			m_Emitters[i]->GetPosition(emitterPos);
			f32 dist2 = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - emitterPos).Mag2();
			if(dist2 < nearestDist2)
			{
				nearestDist2 = dist2;
			}		
		}
	}
	return nearestDist2;
}

f32 audRadioSlot::ComputeDistanceSqToNearestEmitter(CInteriorInst* pIntInst, s32 roomIdx) const
{
	f32 nearestDist2 = LARGE_FLOAT;
	for (u32 i = 0; i < g_NumRadioSlotEmitters; i++)
	{
		if (m_Emitters[i])
		{
			naEnvironmentGroup* environmentGroup = m_Emitters[i]->GetOcclusionGroup();

			if (environmentGroup && environmentGroup->GetInteriorInst() == pIntInst && environmentGroup->GetRoomIdx() == roomIdx)
			{
				// Special case active 'fill space' emitters
				if (m_Emitters[i]->GetOcclusionGroup() && m_Emitters[i]->GetOcclusionGroup()->IsCurrentlyFillingListenerSpace())
				{
					return 0.f;
				}

				Vector3 emitterPos;
				m_Emitters[i]->GetPosition(emitterPos);
				f32 dist2 = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - emitterPos).Mag2();
				if (dist2 < nearestDist2)
				{
					nearestDist2 = dist2;
				}
			}			
		}
	}
	return nearestDist2;
}

#endif // NA_RADIO_ENABLED
